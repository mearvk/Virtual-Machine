/* $Id: HostWebcam-darwin.mm 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * DrvHostWebcam - Mac host webcam backend. Uses AVFoundation framework.
 */

/*
 * Copyright (C) 2013-2025 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "DrvHostWebcam.h"
#include "JPEGEnc.h"

#import <AVFoundation/AVFoundation.h>
#import <AVFoundation/AVMediaFormat.h>
#import <Foundation/NSException.h>

#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>

#ifdef AVFTEST
# include <stdio.h>
#endif


#if MAC_OS_X_VERSION_MIN_REQUIRED < 101400

/* HACK ALERT! It's there in the 10.13 SDK, but only for iOS 7.0+. Deploying CPP trickery to shut up warnings/errors. */
# if MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
#  define AVAuthorizationStatus                 OurAVAuthorizationStatus
#  define AVAuthorizationStatusNotDetermined    OurAVAuthorizationStatusNotDetermined
#  define AVAuthorizationStatusRestricted       OurAVAuthorizationStatusRestricted
#  define AVAuthorizationStatusDenied           OurAVAuthorizationStatusDenied
#  define AVAuthorizationStatusAuthorized       OurAVAuthorizationStatusAuthorized
# endif

/**
 * The authorization status enum.
 *
 * Starting macOS 10.14 we need to request permissions in order to use the webcam
 * but as we build against an older SDK where this is not available we have to duplicate
 * AVAuthorizationStatus and do everything dynmically during runtime, sigh...
 */
typedef enum AVAuthorizationStatus: NSInteger
{
    AVAuthorizationStatusNotDetermined = 0,
    AVAuthorizationStatusRestricted    = 1,
    AVAuthorizationStatusDenied        = 2,
    AVAuthorizationStatusAuthorized    = 3,
} AVAuthorizationStatus;
#endif

@interface HWCCaptureDelegate: NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
    PDRVHOSTWEBCAM mpThis;
    HWCJPEG *mpEnc;
}

-(int)initInstance:(PDRVHOSTWEBCAM)pThis;
-(void)captureOutput:(AVCaptureOutput *)captureOutput
       didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection;
@end


/* The host specific data. */
struct HOSTWEBCAM
{
    VRDEVIDEOINCTRL_VS_SETUP lastSetup;

    char *pszDeviceUniqueID;
    char *pszDeviceName;

    AVCaptureDevice          *avcDevice;
    HWCCaptureDelegate       *captureDelegate;
    AVCaptureDeviceInput     *avcDeviceInput;
    AVCaptureSession         *avcSession;
    AVCaptureVideoDataOutput *avcVideoDataOutput;

    struct {
        PPDMTHREAD pThreadMonitor;
    } monitor;
};


/*
 * Helpers
 */
static int hwcGetCaptureDevice(const char *pszDeviceUniqueID, AVCaptureDevice **pDevice)
{
    NSAutoreleasePool *localpool = [[NSAutoreleasePool alloc] init];

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 101500
    RT_GCC_NO_WARN_DEPRECATED_BEGIN /* AVCaptureDeviceTypeExternalUnknown is deprecated since 15.0, was renamed to AVCaptureDeviceTypeExternal. */
    AVCaptureDeviceDiscoverySession *DiscoverySession = [AVCaptureDeviceDiscoverySession
                                                         discoverySessionWithDeviceTypes:@[ /* only types available on macOS: */
                                                             AVCaptureDeviceTypeBuiltInWideAngleCamera,
                                                             AVCaptureDeviceTypeExternalUnknown]
                                                         mediaType:AVMediaTypeVideo
                                                         position:AVCaptureDevicePositionUnspecified];
    RT_GCC_NO_WARN_DEPRECATED_END
    NSArray<AVCaptureDevice *> *devices = DiscoverySession.devices;
#else /* < 10.15 */
    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#endif

    /* Search for a device with the pszDeviceUniqueID. */
    AVCaptureDevice *device = 0;
    for (AVCaptureDevice *iter in devices)
    {
        if (RTStrICmp(pszDeviceUniqueID, iter.uniqueID.UTF8String) == 0)
        {
            device = iter;
            [device retain];
            break;
        }
    }

    [localpool drain];

    int rc;
    if (device)
    {
        *pDevice = device;
        rc = VINF_SUCCESS;
    }
    else
    {
        rc = VERR_NOT_FOUND;
    }
    UWLOGFLEAVERC(rc);
    return rc;
}

static void hwcDumpFormatDescription(const char *pszMsg, CMFormatDescriptionRef desc)
{
    if (!UWLOGENABLED())
    {
        return;
    }

    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(desc);
    FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(desc);
    char szFourcc[5];
    szFourcc[0] = char(fourcc >> 24);
    szFourcc[1] = char(fourcc >> 16);
    szFourcc[2] = char(fourcc >> 8);
    szFourcc[3] = char(fourcc);
    szFourcc[4] = 0;

    UWLOG(("%s: %dx%d %s\n",
           pszMsg, dimensions.width, dimensions.height, szFourcc));
}


/*
 * Frame capture.
 */

static int hwcCaptureStop(PDRVHOSTWEBCAM pThis)
{
    NSAutoreleasePool* localpool = [[NSAutoreleasePool alloc] init];

    int rc = VINF_SUCCESS;
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    if (pHostWebcam->avcSession)
    {
        [pHostWebcam->avcSession stopRunning];
        [pHostWebcam->avcSession release];
        pHostWebcam->avcSession = 0;
    }

    if (pHostWebcam->avcDeviceInput)
    {
        [pHostWebcam->avcDeviceInput release];
        pHostWebcam->avcDeviceInput = 0;
    }

    if (pHostWebcam->avcVideoDataOutput)
    {
        [pHostWebcam->avcVideoDataOutput release];
        pHostWebcam->avcVideoDataOutput = 0;
    }

    if (pHostWebcam->avcDevice)
    {
        [pHostWebcam->avcDevice release];
        pHostWebcam->avcDevice = 0;
    }

    if (pHostWebcam->captureDelegate)
    {
        [pHostWebcam->captureDelegate release];
        pHostWebcam->captureDelegate = 0;
    }

    [localpool drain];
    return rc;
}


/**
 * Requests camera permissions for Mojave and onwards.
 *
 * @returns VBox status code.
 */
static int hwcCapturePermissionRequest(void)
{
    __block RTSEMEVENT hEvt = NIL_RTSEMEVENT;
    __block int rc = RTSemEventCreate(&hEvt);
    if (RT_SUCCESS(rc))
    {
        /* Perform auth request. */
        [AVCaptureDevice performSelector: @selector(requestAccessForMediaType: completionHandler:) withObject: (id)AVMediaTypeVideo withObject: (id)^(BOOL granted) {
            if (!granted) {
                LogRel(("Webcam: Access denied!\n"));
                rc = VERR_ACCESS_DENIED;
            }
            RTSemEventSignal(hEvt);
        }];

        rc = RTSemEventWait(hEvt, 10 * RT_MS_1SEC);
        RTSemEventDestroy(hEvt);
    }

    return rc;
}


/**
 * Checks permission for capturing devices on Mojave and onwards.
 *
 * @returns VBox status code.
 */
static int hwcCapturePermissionCheck(void)
{
    int rc = VINF_SUCCESS;

    /*
     * Because we build with an older SDK where the authorization APIs are not available
     * (introduced with Mojave 10.14) we have to resort to resolving the APIs dynamically.
     */
    if (NSFoundationVersionNumber >= 10.14)
    {
        LogRel(("Webcam: macOS 10.14+ detected, checking camera permissions\n"));

        if ([AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)])
        {
            AVAuthorizationStatus enmAuthSts = (AVAuthorizationStatus)(NSInteger)[AVCaptureDevice performSelector: @selector(authorizationStatusForMediaType:) withObject: (id)AVMediaTypeVideo];
            if (enmAuthSts == AVAuthorizationStatusNotDetermined)
                rc = hwcCapturePermissionRequest();
            else if (   enmAuthSts == AVAuthorizationStatusRestricted
                     || enmAuthSts == AVAuthorizationStatusDenied)
            {
                LogRel(("Webcam: Access denied!\n"));
                rc = VERR_ACCESS_DENIED;
            }
        }
    }

    return rc;
}


static int hwcCaptureStart(PDRVHOSTWEBCAM pThis)
{
    NSAutoreleasePool* localpool = [[NSAutoreleasePool alloc] init];

    int rc = VINF_SUCCESS;
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    NSError* error = 0;
    do /* break loop */
    {
        rc = hwcCapturePermissionCheck();
        if (RT_FAILURE(rc)) break;

        rc = hwcGetCaptureDevice(pHostWebcam->pszDeviceUniqueID, &pHostWebcam->avcDevice);
        if (RT_FAILURE(rc)) break;

        /** @todo proper error handling. */
        pHostWebcam->captureDelegate = [[HWCCaptureDelegate alloc] init];
        if (!pHostWebcam->captureDelegate)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = [pHostWebcam->captureDelegate initInstance:pThis];
        if (RT_FAILURE(rc)) break;

        pHostWebcam->avcDeviceInput = [[AVCaptureDeviceInput alloc] initWithDevice:pHostWebcam->avcDevice error:&error];
        if (!pHostWebcam->avcDeviceInput)
        {
            UWLOG(("DeviceInput failed\n"));
            if (error)
            {
                NSString *desc = [error localizedDescription];
                if (desc)
                {
                     UWLOG(("DeviceInput: [%s]\n", desc.UTF8String));
                }
            }
            rc = VERR_NO_MEMORY;
            break;
        }

        pHostWebcam->avcSession = [[AVCaptureSession alloc] init];
        if (!pHostWebcam->avcSession)
        {
            UWLOG(("Session failed\n"));
            rc = VERR_NO_MEMORY;
            break;
        }

        pHostWebcam->avcVideoDataOutput = [[AVCaptureVideoDataOutput alloc] init];
        if (!pHostWebcam->avcVideoDataOutput)
        {
            UWLOG(("VideoDataOutput failed\n"));
            rc = VERR_NO_MEMORY;
            break;
        }

        dispatch_queue_t dq = dispatch_queue_create("VirtualBox.HWCQueue", NULL);
        [pHostWebcam->avcVideoDataOutput setSampleBufferDelegate:pHostWebcam->captureDelegate queue:dq];
        dispatch_release(dq);

        /* Need '2vuy', which is default for built-in MacBook camera. */
        NSDictionary *videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithUnsignedInt:kCVPixelFormatType_422YpCbCr8], (id)kCVPixelBufferPixelFormatTypeKey,
            nil];
        pHostWebcam->avcVideoDataOutput.videoSettings = videoSettings;
        pHostWebcam->avcVideoDataOutput.alwaysDiscardsLateVideoFrames = YES;

        /*
         * Select device format.
         */
        for (AVCaptureDeviceFormat *format in pHostWebcam->avcDevice.formats)
        {
            if ([format.mediaType compare:AVMediaTypeVideo] == NSOrderedSame)
            {
                CMFormatDescriptionRef desc = format.formatDescription;
                CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(desc);
                /* FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(desc); - unused */

                if (   dimensions.width == pHostWebcam->lastSetup.u16Width
                    && dimensions.height == pHostWebcam->lastSetup.u16Height)
                {
                    UWLOG(("Found device format\n"));
                    NSError* error2 = 0;
                    [pHostWebcam->avcDevice lockForConfiguration:&error2];
                    pHostWebcam->avcDevice.activeFormat = format;
                    [pHostWebcam->avcDevice unlockForConfiguration];
                    break;
                }
            }
        }

        [pHostWebcam->avcSession addInput:pHostWebcam->avcDeviceInput];
        [pHostWebcam->avcSession addOutput:pHostWebcam->avcVideoDataOutput];

        /* Must call lockForConfiguration: before calling the AVCaptureSession::startRunning,
         * or the session's preset will override the selected active format on the capture device.
         */
        NSError* error3 = 0;
        [pHostWebcam->avcDevice lockForConfiguration:&error3];
        /* #todo receive AVCaptureSessionRuntimeErrorNotification. */
        [pHostWebcam->avcSession startRunning];
        [pHostWebcam->avcDevice unlockForConfiguration];

        UWLOG(("Session running\n"));
        hwcDumpFormatDescription("Start", pHostWebcam->avcDevice.activeFormat.formatDescription);
    } while(0);

    [localpool drain];

    if (RT_FAILURE(rc))
    {
        hwcCaptureStop(pThis);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}


@implementation HWCCaptureDelegate

-(id)init
{
    [super init];
    mpThis = 0;
    mpEnc = 0;
    return self;
}

-(void)dealloc
{
    UWLOG(("\n"));
    HWCJPEGDestroy(mpEnc);
    [super dealloc];
}

-(int)initInstance:(PDRVHOSTWEBCAM)pThis
{
    mpThis = pThis;
    int rc = HWCJPEGInit(&mpEnc);
    UWLOG(("%Rrc\n", rc));
    return rc;
}

-(void)captureOutput:(AVCaptureOutput *)captureOutput
       didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    RT_NOREF(captureOutput, connection);
    CMFormatDescriptionRef desc = CMSampleBufferGetFormatDescription(sampleBuffer);
    hwcDumpFormatDescription("Sample", desc);

    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    CVPixelBufferRef pixelBuffer = CVBufferRetain(imageBuffer);
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);

    uint8_t *pu8Data = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
    size_t cbData = CVPixelBufferGetDataSize(pixelBuffer);
    size_t cWidth = CVPixelBufferGetWidth(pixelBuffer);
    size_t cHeight = CVPixelBufferGetHeight(pixelBuffer);

    UWLOG(("%dx%d %p %d\n", cWidth, cHeight, pu8Data, cbData));

    if (mpEnc)
    {
        uint8_t *pu8Encoded = 0;
        uint32_t cbEncoded = 0;
        int rc = HWCJPEGEncodeFrameRaw_2vuy_yuvs(mpEnc, 75,
                                                 &pu8Encoded, &cbEncoded,
                                                 pu8Data, cbData, (uint32_t)cWidth, (uint32_t) cHeight, false);
        if (RT_SUCCESS(rc))
        {
            if (mpThis && mpThis->pIWebcamUp)
            {
                VRDEVIDEOINPAYLOADHDR hdr;
                hdr.u8HeaderLength      = sizeof(VRDEVIDEOINPAYLOADHDR);
                hdr.u8HeaderInfo        = VRDE_VIDEOIN_PAYLOAD_F_EOF;
                hdr.u32PresentationTime = 0;
                hdr.u32SourceTimeClock  = 0;
                hdr.u16Reserved         = 0;
                mpThis->pIWebcamUp->pfnFrame(mpThis->pIWebcamUp,
                                             1,
                                             &hdr,
                                             sizeof(hdr),
                                             pu8Encoded,
                                             cbEncoded);
            }
#ifdef AVFTEST
            FILE *f = fopen("avftest.jpg", "wb");
            if (f)
            {
                fwrite(pu8Encoded, 1, cbEncoded, f);
                fclose(f);
            }
#endif
            RTMemFree(pu8Encoded);
        }
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    CVBufferRelease(pixelBuffer);
}

@end


/*
 * Device description helpers.
 */

#define HWC_MAX_INTERVALS 6

#pragma pack(1)
typedef struct HWCFRAMEDESC
{
    VRDEVIDEOINFRAMEDESC frame;
    uint16_t cbIntervals;
    uint32_t au32Intervals[HWC_MAX_INTERVALS];
} HWCFRAMEDESC;

typedef struct HWCDEVICEEXT
{
    VRDEVIDEOINDEVICEEXT ext;
    VRDEVIDEOINDEVICEFIELDHDR hdrProduct;
    char achProduct[128];
    VRDEVIDEOINDEVICEFIELDHDR hdrSerial;
    char achSerial[64];
} HWCDEVICEEXT;

typedef struct HWCDEVICEDESC
{
    VRDEVIDEOINDEVICEDESC desc;
    HWCDEVICEEXT ext;
    VRDEVIDEOINFORMATDESC format;
    HWCFRAMEDESC aFrames[1];
} HWCDEVICEDESC;
#pragma pack()

static int hwcFrameDescFind(HWCDEVICEDESC *pDesc, int cFrames, uint16_t u16Width, uint16_t u16Height)
{
    int iIndex;
    for (iIndex = 0; iIndex < cFrames; iIndex++)
    {
        VRDEVIDEOINFRAMEDESC *p = &pDesc->aFrames[iIndex].frame;
        if (p->u16Width == u16Width && p->u16Height == u16Height)
        {
            return iIndex;
        }
    }
    return -1;
}

#if 0 /* Disabled code because format.videoSupportedFrameRateRanges is not reliable. */
static uint32_t hwcFrameIntervalCombine(uint32_t *pau32Combined, uint32_t cCombined, const uint32_t *pau32Intervals, uint32_t cIntervals)
{
    uint32_t i;
    for (i = 0; i < cIntervals; ++i)
    {
        const uint32_t u32Interval = pau32Intervals[i];
        bool fSkip = false;
        uint32_t j;
        for (j = 0; j < cCombined; ++j)
        {
            if (u32Interval == pau32Combined[j])
            {
                /* Already exists in pau32Combined array. */
                fSkip = true;
                break;
            }

            if (u32Interval < pau32Combined[j])
            {
                /* Should be placed at [j] in pau32Combined array. */
                break;
            }
        }

        if (!fSkip)
        {
            if (j < cCombined)
            {
                memmove(&pau32Combined[j + 1], &pau32Combined[j], sizeof(uint32_t) * (cCombined - j));
            }
            pau32Combined[cCombined++] = u32Interval;
        }
    }

    return cCombined;
}
#endif /* Disabled code because format.videoSupportedFrameRateRanges is not reliable. */

static uint32_t sau32Intervals[HWC_MAX_INTERVALS] =
{            /* FPS */
    333333,  /* 30  */
    400000,  /* 25  */
    500000,  /* 20  */
    666666,  /* 15  */
    1000000, /* 10  */
    2000000  /* 5   */
};

static void hwcFrameDescTryAdd(HWCDEVICEDESC *pDesc, int *pcFrames, AVCaptureDeviceFormat *format)
{
    NSAutoreleasePool* localpool = [[NSAutoreleasePool alloc] init];

    CMFormatDescriptionRef desc = format.formatDescription;
    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(desc);
    FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(desc);

    /* Check for duplicates. */
    int iExistingFrame = hwcFrameDescFind(pDesc, *pcFrames,
                                          (uint16_t)dimensions.width,
                                          (uint16_t)dimensions.height);

    int iFrame = iExistingFrame == -1? *pcFrames: iExistingFrame;

    UWLOG(("%dx%d iExisting %d 0x%RX32(%c%c%c%c)%s\n",
           dimensions.width, dimensions.height,
           iExistingFrame,
           fourcc, RT_BYTE1(fourcc), RT_BYTE2(fourcc), RT_BYTE3(fourcc), RT_BYTE4(fourcc),
           fourcc == 'dmb1'? " MJPG": ""));

    VRDEVIDEOINFRAMEDESC *p = &pDesc->aFrames[iFrame].frame;

    /* u8FrameFlags is temporarily used to indicate that the frame description was from MJPG format. */
    if (iExistingFrame != -1 && p->u8FrameFlags != 0)
    {
        /* Do not alter the frame anymore. */
        [localpool drain];
        return;
    }

    if (iExistingFrame == -1)
    {
        p->cbFrame      = sizeof(HWCFRAMEDESC);
        p->u8FrameId    = (uint8_t)(iFrame + 1);
        p->u8FrameFlags = 0;
        p->u16Width     = dimensions.width;
        p->u16Height    = dimensions.height;

        pDesc->aFrames[iFrame].cbIntervals = sizeof(pDesc->aFrames[iFrame].au32Intervals);
    }

    if (fourcc == 'dmb1')
    {
        /* Mark the frame as complete. */
        p->u8FrameFlags =   VRDE_VIDEOIN_F_FRM_DISCRETE_INTERVALS
                          | VRDE_VIDEOIN_F_FRM_SIZE_OF_FIELDS;
    }

    /* Init intervals if this is a first frame of this dimensions. */
    if (iExistingFrame == -1)
    {
        /* Min/Max values will be updated later. */
        p->u32MinFrameInterval = 0xFFFFFFFF;
        p->u32MaxFrameInterval = 0;

        /* Initialize with some default intervals because the OS does not provide them in format.videoSupportedFrameRateRanges.  */
        p->u32NumFrameIntervals = HWC_MAX_INTERVALS;
        memcpy(pDesc->aFrames[iFrame].au32Intervals, sau32Intervals, sizeof(uint32_t) * HWC_MAX_INTERVALS);
    }

#if 0 /* Disabled code because format.videoSupportedFrameRateRanges is not reliable. */
    /* Add intervals to the au32Intervals array if new intervals extend the available range.
     * I.e. create a new array of intervals from au32Intervals and format.videoSupportedFrameRateRanges.
     */
    UWLOG(("%d intervals\n", [format.videoSupportedFrameRateRanges count]));

    /* First create a new local array of intervals. */
    uint32_t au32Intervals[HWC_MAX_INTERVALS];
    uint32_t iDst = 0;
    for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges)
    {
        if (iDst >= HWC_MAX_INTERVALS)
        {
            break;
        }

        /* in 100ns units */
        CMTime cmtime = range.maxFrameDuration;
        au32Intervals[iDst] = (cmtime.value * 1000 * 1000 * 10) / cmtime.timescale;
        UWLOG(("  %d\n", au32Intervals[iDst]));
        ++iDst;
    }

    /* iDst is the number of frame intervals in the local au32Intervals array.
     * Merge au32Intervals and pDesc->aFrames[iFrame].au32Intervals.
     * First create a new array from 2 of them, shortest interval first and remove duplicates.
     */
    uint32_t au32Combined[2 * HWC_MAX_INTERVALS];
    uint32_t cCombined = 0;
    cCombined = hwcFrameIntervalCombine(au32Combined, cCombined, au32Intervals, iDst);
    cCombined = hwcFrameIntervalCombine(au32Combined, cCombined, pDesc->aFrames[iFrame].au32Intervals, p->u32NumFrameIntervals);

    /* Copy au32Combined back to pDesc->aFrames[iFrame].au32Intervals. */
    cCombined = RT_MIN(cCombined, HWC_MAX_INTERVALS); /* Just drop longer intervals. */
    p->u32NumFrameIntervals = cCombined;
    memcpy(pDesc->aFrames[iFrame].au32Intervals, au32Combined, sizeof(uint32_t) * cCombined);
#endif /* Disabled code because format.videoSupportedFrameRateRanges is not reliable. */

    /* Update min/max */
    uint32_t i;
    for (i = 0; i < p->u32NumFrameIntervals; ++i)
    {
        const uint32_t u32Interval = pDesc->aFrames[iFrame].au32Intervals[i];
        p->u32MinFrameInterval = RT_MIN(p->u32MinFrameInterval, u32Interval);
        p->u32MaxFrameInterval = RT_MAX(p->u32MaxFrameInterval, u32Interval);
    }

    /* It was a new frame. */
    if (iExistingFrame == -1)
    {
        *pcFrames = iFrame + 1;
    }

    [localpool drain];
}

static int hwcBuildDeviceDesc(PDRVHOSTWEBCAM pThis,
                              AVCaptureDevice *device,
                              VRDEVIDEOINDEVICEDESC **ppDeviceDesc,
                              uint32_t *pcbDeviceDesc)
{
    NSAutoreleasePool* localpool = [[NSAutoreleasePool alloc] init];

    int rc = VINF_SUCCESS;

    /** @todo Either support uncompressed formats ('2vuy') or compress to JPEGs.
     * For uncompressed formats:
     * If webcam supports MJPEG, then get only MJPEG frames, otherwise an uncompressed format.
     * Formats priority: MJPEG, '2vuy', 'yuvs'.
     * 2vuy: Cb, Y0, Cr, Y1 (used by built-in FaceTime HD Camera).
     * yuvs: Y0, Cb, Y1, Cr (used by Logitech and other webcams).
     */

    NSArray *formats = [device formats];
    int iCount = [formats count];

    uint32_t cbDesc = sizeof(HWCDEVICEDESC) + sizeof(HWCFRAMEDESC) * (iCount - 1);
    HWCDEVICEDESC *pDesc = (HWCDEVICEDESC *)RTMemAllocZ(cbDesc);
    if (pDesc)
    {
        int cFrames = 0;

        for (AVCaptureDeviceFormat *format in formats)
        {
            if ([format.mediaType compare:AVMediaTypeVideo] == NSOrderedSame)
            {
                hwcFrameDescTryAdd(pDesc, &cFrames, format);
            }
        }

        if (cFrames > 0)
        {
            /* Set proper frame flags. The field was used temporarily by the above code. */
            int iIndex;
            for (iIndex = 0; iIndex < cFrames; iIndex++)
            {
                VRDEVIDEOINFRAMEDESC *p = &pDesc->aFrames[iIndex].frame;
                p->u8FrameFlags =   VRDE_VIDEOIN_F_FRM_DISCRETE_INTERVALS
                                  | VRDE_VIDEOIN_F_FRM_SIZE_OF_FIELDS;
            }

            cbDesc = sizeof(HWCDEVICEDESC) + sizeof(HWCFRAMEDESC) * (cFrames - 1);

            pDesc->desc.u16ObjectiveFocalLengthMin = 0;
            pDesc->desc.u16ObjectiveFocalLengthMax = 0;
            pDesc->desc.u16OcularFocalLength       = 0;
            pDesc->desc.u16MaxMultiplier           = 0;
            pDesc->desc.fu32CameraControls         = 0;
            pDesc->desc.fu32ProcessingControls     = VRDE_VIDEOIN_F_PU_CTRL_BRIGHTNESS;
            pDesc->desc.fu8DeviceCaps              = 0;
            pDesc->desc.u8NumFormats               = 1;
            pDesc->desc.cbExt                      = sizeof(HWCDEVICEEXT);

            pDesc->ext.ext.fu32Fields = 0;
            if (pThis->pHostWebcam->pszDeviceName)
            {
                pDesc->ext.ext.fu32Fields |= VRDE_VIDEOIN_F_DEV_EXT_NAME;
                pDesc->ext.hdrProduct.cbField =   sizeof(VRDEVIDEOINDEVICEFIELDHDR)
                                                + sizeof(pDesc->ext.achProduct);
                RTStrCopy(pDesc->ext.achProduct, sizeof(pDesc->ext.achProduct),
                          pThis->pHostWebcam->pszDeviceName);
            }

            if (pThis->pHostWebcam->pszDeviceUniqueID)
            {
                pDesc->ext.ext.fu32Fields |= VRDE_VIDEOIN_F_DEV_EXT_SERIAL;
                pDesc->ext.hdrSerial.cbField =   sizeof(VRDEVIDEOINDEVICEFIELDHDR)
                                               + sizeof(pDesc->ext.achSerial);
                RTStrCopy(pDesc->ext.achSerial, sizeof(pDesc->ext.achSerial),
                          pThis->pHostWebcam->pszDeviceUniqueID);
            }

            pDesc->format.cbFormat      = sizeof(VRDEVIDEOINFORMATDESC);
            pDesc->format.u8FormatId    = 1;
            pDesc->format.u8FormatType  = VRDE_VIDEOIN_FORMAT_MJPEG;
            pDesc->format.u8FormatFlags = 0;
            pDesc->format.u8NumFrames   = (uint8_t)cFrames;
            pDesc->format.u16Reserved   = 0;
        }
        else
        {
            rc = VERR_NOT_SUPPORTED;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *ppDeviceDesc = &pDesc->desc;
        *pcbDeviceDesc = cbDesc;
    }

    [localpool drain];
    UWLOGFLEAVERC(rc);
    return rc;
}


/*
 * Device discovery helper functions.
 */
static int hwcStoreDeviceInfo(PDRVHOSTWEBCAM pThis, AVCaptureDevice *device)
{
    pThis->pHostWebcam->pszDeviceUniqueID = RTStrDup(device.uniqueID.UTF8String);
    pThis->pHostWebcam->pszDeviceName = RTStrDup(device.localizedName.UTF8String);

    if (   !pThis->pHostWebcam->pszDeviceUniqueID
        || !pThis->pHostWebcam->pszDeviceName)
    {
        RTStrFree(pThis->pHostWebcam->pszDeviceUniqueID);
        pThis->pHostWebcam->pszDeviceUniqueID = NULL;
        RTStrFree(pThis->pHostWebcam->pszDeviceName);
        pThis->pHostWebcam->pszDeviceName = NULL;
        return VERR_NO_MEMORY;
    }

    UWLOG(("Device: %s, %s\n",
           pThis->pHostWebcam->pszDeviceUniqueID, pThis->pHostWebcam->pszDeviceName));
    return VINF_SUCCESS;
}

static int hwcFindDevice(PDRVHOSTWEBCAM pThis)
{
    NSAutoreleasePool* localpool = [[NSAutoreleasePool alloc] init];

    /* Find the webcam and save the uniqueID property to pszDeviceUniqueID. */
    RTStrFree(pThis->pHostWebcam->pszDeviceUniqueID);
    pThis->pHostWebcam->pszDeviceUniqueID = NULL;
    RTStrFree(pThis->pHostWebcam->pszDeviceName);
    pThis->pHostWebcam->pszDeviceName = NULL;

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 101500
    RT_GCC_NO_WARN_DEPRECATED_BEGIN /* AVCaptureDeviceTypeExternalUnknown is deprecated since 15.0, was renamed to AVCaptureDeviceTypeExternal. */
    AVCaptureDeviceDiscoverySession *DiscoverySession = [AVCaptureDeviceDiscoverySession
                                                         discoverySessionWithDeviceTypes:@[ /* only types available on macOS: */
                                                             AVCaptureDeviceTypeBuiltInWideAngleCamera,
                                                             AVCaptureDeviceTypeExternalUnknown]
                                                         mediaType:AVMediaTypeVideo
                                                         position:AVCaptureDevicePositionUnspecified];
    RT_GCC_NO_WARN_DEPRECATED_END
    NSArray<AVCaptureDevice *> *devices = DiscoverySession.devices;
#else /* < 10.15 */
    NSArray* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#endif

    UWLOG(("idx %d, path [%s], %d devices\n",
           pThis->i32DeviceIndex, pThis->pszDevicePath, [devices count]));

    const char *pszUniqueID = pThis->pszDevicePath;

    int rc;
    uint16_t u16VID = 0;
    uint16_t u16PID = 0;
    if (pThis->i32DeviceIndex == -1)
    {
        /* Deal with special ".{uuid}VVVVPPPP:address" path. */
        if (   pThis->pszDevicePath[0] == '.'
            && pThis->pszDevicePath[1] == '{')
        {
            /* Extract vendor id and product id. */
            const char *p = strchr(pThis->pszDevicePath, '}');
            if (p)
            {
                p++;
                char *pszNext = NULL;
                uint32_t u32 = 0;
                rc = RTStrToUInt32Ex(p, &pszNext, 16, &u32);
                if (rc == VINF_SUCCESS || rc == VWRN_TRAILING_CHARS)
                {
                    if (*pszNext == ':')
                    {
                        pszNext++;

                        u16VID = (uint16_t)((u32 >> 16) & 0xFFFF);
                        u16PID = (uint16_t)(u32 & 0xFFFF);

                        UWLOG(("vid %04x, pid %04x address [%s]\n", u16VID, u16PID, pszNext));

                        /* Also look for "l=" part of the address string. */
                        p = strstr(pszNext, "l=");
                        if (p)
                        {
                            p += 2;

                            /* Construct unique id as l + vendor + product. */
                            size_t cb = strlen(p);
                            const char *pszEnd = strchr(p, ';');
                            if (pszEnd)
                            {
                                cb = pszEnd - p;
                            }
                            UWLOG(("l %d [%s]\n", cb, p));
                            char *psz = NULL;
                            rc = RTStrAPrintf(&psz, "%.*s%04x%04x", cb, p, u16VID, u16PID);
                            if (RT_SUCCESS(rc))
                            {
                                pszUniqueID = psz;
                            }
                        }
                        else
                        {
                            /** @todo look for VID PID. */
                        }
                    }
                }
            }
        }
    }

    AVCaptureDevice *device;
    if (pThis->i32DeviceIndex == -1)
    {
        /* Search for a device with the pszUniqueID uniqueID. */
        UWLOG(("looking for %s\n", pszUniqueID));
        for (device in devices)
        {
            if (RTStrICmp(pszUniqueID, device.uniqueID.UTF8String) == 0)
            {
                break;
            }
        }
    }
    else if (pThis->i32DeviceIndex == 0)
    {
        device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    }
    else
    {
        if ((unsigned)pThis->i32DeviceIndex <= [devices count])
        {
            int i = pThis->i32DeviceIndex - 1;
            device = [devices objectAtIndex:i];
        }
        else
        {
            device = 0;
        }
    }

    if (device)
    {
        hwcStoreDeviceInfo(pThis, device);
    }

    if (pszUniqueID != pThis->pszDevicePath)
    {
        RTStrFree((char *)pszUniqueID);
    }

    [localpool drain];
    return pThis->pHostWebcam->pszDeviceUniqueID? VINF_SUCCESS: VERR_NOT_FOUND;
}


/*
 * HostWebcam entry points.
 */
int hostWebcamControl(PDRVHOSTWEBCAM pThis,
                      void *pvUser,
                      uint64_t u64DeviceId,
                      const VRDEVIDEOINCTRLHDR *pHdr,
                      uint32_t cbCtrl)
{
    int rc = VINF_SUCCESS;

    switch (pHdr->u16ControlSelector)
    {
        case VRDE_VIDEOIN_CTRLSEL_VS_SETUP:
        {
            UWLOGF(("VRDE_VIDEOIN_CTRLSEL_VS_SETUP\n"));
            VRDEVIDEOINCTRL_VS_SETUP *pSetup = (VRDEVIDEOINCTRL_VS_SETUP *)pHdr;
            pThis->pHostWebcam->lastSetup = *pSetup;
            if (pThis->pIWebcamUp)
            {
                pThis->pIWebcamUp->pfnControl(pThis->pIWebcamUp,
                                              true, /* fResponse */
                                              pvUser,
                                              u64DeviceId,
                                              pHdr,
                                              cbCtrl);
            }
        } break;

        case VRDE_VIDEOIN_CTRLSEL_VS_ON:
        {
            UWLOGF(("VRDE_VIDEOIN_CTRLSEL_VS_ON\n"));

            VRDEVIDEOINCTRL_VS_ON rsp;
            rsp.hdr = *pHdr;

            rc = hwcCaptureStart(pThis);
            if (RT_FAILURE(rc))
            {
                rsp.hdr.u16ParmSize = 0;
                rsp.hdr.u8Flags = VRDE_VIDEOIN_CTRLHDR_F_FAIL;
                rsp.hdr.u8Status = VRDE_VIDEOIN_CTRLHDR_STATUS_NOTREADY;
            }

            if (pThis->pIWebcamUp)
            {
                pThis->pIWebcamUp->pfnControl(pThis->pIWebcamUp,
                                              true, /* fResponse */
                                              pvUser,
                                              u64DeviceId,
                                              &rsp.hdr,
                                              sizeof(rsp));
            }
        } break;

        case VRDE_VIDEOIN_CTRLSEL_VS_OFF:
        {
            UWLOGF(("VRDE_VIDEOIN_CTRLSEL_VS_OFF\n"));

            VRDEVIDEOINCTRL_VS_OFF rsp;
            rsp.hdr = *pHdr;

            rc = hwcCaptureStop(pThis);
            if (RT_FAILURE(rc))
            {
                rsp.hdr.u16ParmSize = 0;
                rsp.hdr.u8Flags = VRDE_VIDEOIN_CTRLHDR_F_FAIL;
                rsp.hdr.u8Status = VRDE_VIDEOIN_CTRLHDR_STATUS_NOTREADY;
            }

            if (pThis->pIWebcamUp)
            {
                pThis->pIWebcamUp->pfnControl(pThis->pIWebcamUp,
                                              true, /* fResponse */
                                              pvUser,
                                              u64DeviceId,
                                              &rsp.hdr,
                                              sizeof(rsp));
            }
        } break;

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

int hostWebcamQueryDeviceDesc(PDRVHOSTWEBCAM pThis,
                              VRDEVIDEOINDEVICEDESC **ppDeviceDesc,
                              uint32_t *pcbDeviceDesc)
{
    AVCaptureDevice *device = 0;
    int rc = hwcGetCaptureDevice(pThis->pHostWebcam->pszDeviceUniqueID, &device);
    if (RT_SUCCESS(rc))
    {
        rc = hwcBuildDeviceDesc(pThis, device, ppDeviceDesc, pcbDeviceDesc);

        [device release];
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

int hostWebcamConstruct(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis, PCFGMNODE pCfg)
{
    /* One time initialization of the backend. */
    RT_NOREF(pDrvIns, pCfg);

    pThis->pHostWebcam = (HOSTWEBCAM *)RTMemAllocZ(sizeof(HOSTWEBCAM));
    AssertReturn(pThis->pHostWebcam, VERR_NO_MEMORY);
    int rc = hwcFindDevice(pThis);
    if (RT_SUCCESS(rc))
    {
        /// @todo later hwcMonitorStart(pDrvIns, pThis);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

void hostWebcamDestruct(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis)
{
    RT_NOREF(pDrvIns);
    /// @todo later hwcMonitorStop(pDrvIns, pThis);

    if (pThis->pHostWebcam)
    {
        RTStrFree(pThis->pHostWebcam->pszDeviceUniqueID);
        RTStrFree(pThis->pHostWebcam->pszDeviceName);

        RTMemFree(pThis->pHostWebcam);
        pThis->pHostWebcam = NULL;
    }
}

#ifdef AVFTEST
void avfList(void)
{
    NSAutoreleasePool* localpool = [[NSAutoreleasePool alloc] init];

    NSArray* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    printf("AV Foundation Video Input Devices: %lu\n", [devices count]);

    for (AVCaptureDevice *device in devices)
    {
        printf("Name: %s, %s, %s\n",
               device.localizedName.UTF8String,
               device.modelID.UTF8String,
               device.uniqueID.UTF8String);

        for (AVCaptureDeviceFormat *format in [device formats])
        {
            CMFormatDescriptionRef desc = format.formatDescription;
            CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(desc);
            FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(desc);
            char szFourcc[5];
            szFourcc[0] = char(fourcc >> 24);
            szFourcc[1] = char(fourcc >> 16);
            szFourcc[2] = char(fourcc >> 8);
            szFourcc[3] = char(fourcc);
            szFourcc[4] = 0;

            printf("  Format: %s %dx%d %s",
                   format.mediaType.UTF8String,
                   dimensions.width, dimensions.height,
                   szFourcc);

            for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges)
            {
                printf(" %f", range.maxFrameRate);
            }

            printf("\n");
        }
    }

    [localpool drain];
    return;
}
#endif
