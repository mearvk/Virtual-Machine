/* $Id: HostWebcam-v4l2.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * DrvHostWebcam - Video for Linux (v4l2) webcam backend.
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

#include <iprt/asm-math.h>
#include <iprt/crc.h>
#include <iprt/mem.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>

#include <VBox/err.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/stat.h>

#if defined(RT_OS_SOLARIS)
# include <stropts.h>
# include <sys/videodev2.h>
#else
# include <sys/ioctl.h>
# include "HostWebcam-linux.h"
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* According to V4L2 API spec: 4.1. Video Capture Interface */
#define HWC_V4L2_MAX_DEVICES 64

/* Frame interval used as a fallback. In 100 nanoseconds unit. 50ms == 20FPS. */
/* If you change the fallback FPS, check and update UserManual->Known Issues section for Solaris. */
#define HWC_DEFAULT_INTERVAL (50 * 1000 * 10)

#define HWC_MAX_FRAMES    256
#define HWC_MAX_INTERVALS 6


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct HWCMMAPBUFFER
{
    void *pv;
    uint32_t cb;
} HWCMMAPBUFFER;

typedef struct HWCFRAMEFORMAT
{
    uint32_t u32PixelFormat;
    uint16_t u16Width;
    uint16_t u16Height;
} HWCFRAMEFORMAT;

/* The host specific data. */
struct HOSTWEBCAM
{
    VRDEVIDEOINCTRL_VS_SETUP lastSetup;

    char *pszDevicePath;
    char *pszDeviceName;
    char *pszSerial;

    int hDevice;                   /* Handle during video streaming. */
    uint32_t pixelformat;          /* The selected format. */
    HWCJPEG *pEnc;
    bool fBuffersRequested;        /* V4L2 buffers requested. */
    HWCMMAPBUFFER *paBuffers;      /* mmap'ed buffers. */
    uint32_t cBuffers;             /* Number of valid entries in paBuffers. */

    bool volatile fStreaming;      /* Streaming has been enabled. */

    /** Event semaphore synchronising the stream starting and stopping between
     * the USB webcam thread and our monitor thread. */
    RTSEMEVENT                  hEvtCapture;

    struct
    {
        PPDMTHREAD pThreadMonitor;
        RTPIPE hPipeRead;
        RTPIPE hPipeWrite;
    } monitor;

    int cFrameFormats;
    HWCFRAMEFORMAT aFrameFormats[HWC_MAX_FRAMES];
};


/*
 * Helpers.
 */

static int hwcDetached(PDRVHOSTWEBCAM pThis)
{
    pThis->pIWebcamUp->pfnDetached(pThis->pIWebcamUp, 1);
    return VINF_SUCCESS;
}

static int hwcOpen(const char *pszPath, int *pHandle)
{
    int rc = VINF_SUCCESS;

    struct stat st;
    int ret = stat(pszPath, &st);
    if (   ret == 0
        && S_ISCHR(st.st_mode))
    {
        /* V4L device must be opened with these flags. */
        int h = open(pszPath, O_RDWR | O_NONBLOCK);
        if (h != -1)
        {
            *pHandle = h;
        }
        else
        {
            rc = VERR_OPEN_FAILED;
        }
    }
    else
    {
        rc = VERR_NOT_FOUND;
    }

    return rc;
}

static void hwcClose(int handle)
{
    if (handle != -1)
    {
        close(handle);
    }
}

static int hwcIoctl(int handle, unsigned fn, void *pv)
{
    int ret;
    do
    {
        ret = ioctl(handle, (int)fn, pv);
    } while (ret == -1 && errno == EINTR);

#ifdef RT_OS_SOLARIS
    if (ret == 0)
        return VINF_SUCCESS;
    Assert(ret == -1 && errno > 0);
    return RTErrConvertFromErrno((unsigned)errno);
#else
    return ret == 0 ? VINF_SUCCESS : VERR_FILE_IO_ERROR;
#endif
}

static void hwcResetCropping(int handle)
{
    struct v4l2_cropcap cropcap;
    RT_ZERO(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int rc = hwcIoctl(handle, VIDIOC_CROPCAP, &cropcap);
    if (RT_SUCCESS(rc))
    {
        struct v4l2_crop crop;
        RT_ZERO(crop);
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;
        hwcIoctl(handle, VIDIOC_S_CROP, &crop);
    }
}

static bool hwcIsSupportedFormat(uint32_t pixelformat)
{
    return (   pixelformat == v4l2_fourcc('M','J','P','G')
            || pixelformat == V4L2_PIX_FMT_YUYV
            || pixelformat == V4L2_PIX_FMT_UYVY
            || pixelformat == V4L2_PIX_FMT_YUV420);
}

static void hwcInitFormat(struct v4l2_format *pfmtdst, const struct v4l2_format *pfmtsrc,
                          uint32_t cWidth, uint32_t cHeight, uint32_t preferredFormat)
{
    *pfmtdst = *pfmtsrc;

    if (cWidth)
    {
        pfmtdst->fmt.pix.width = cWidth;
    }
    if (cHeight)
    {
        pfmtdst->fmt.pix.height = cHeight;
    }

    /* Try to set the preferred format. */
    pfmtdst->fmt.pix.pixelformat = preferredFormat;
    pfmtdst->fmt.pix.field = V4L2_FIELD_NONE; /* Need progressive. */
}

static int hwcSetStreamFormat(int handle, uint32_t cWidth, uint32_t cHeight, uint32_t u32FrameInterval,
                              uint32_t preferredFormat, uint32_t *pSelectedFormat)
{
    UWLOG(("%ux%u interval %u\n", cWidth, cHeight, u32FrameInterval));

    struct v4l2_format fmt;
    RT_ZERO(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int rc = hwcIoctl(handle, VIDIOC_G_FMT, &fmt);
    if (RT_SUCCESS(rc))
    {
        UWLOG(("Current format: %dx%d 0x%08X(%c%c%c%c) field %d bpl %d size 0x%08X clr %d priv 0x%08X\n",
               fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat,
               RT_BYTE1(fmt.fmt.pix.pixelformat), RT_BYTE2(fmt.fmt.pix.pixelformat),
               RT_BYTE3(fmt.fmt.pix.pixelformat), RT_BYTE4(fmt.fmt.pix.pixelformat),
               fmt.fmt.pix.field, fmt.fmt.pix.bytesperline, fmt.fmt.pix.sizeimage,
               fmt.fmt.pix.colorspace, fmt.fmt.pix.priv));

        /* Make a copy which will be used for VIDIOC_S_FMT calls. */
        struct v4l2_format fmtset;
        hwcInitFormat(&fmtset, &fmt, cWidth, cHeight, preferredFormat);
        rc = hwcIoctl(handle, VIDIOC_S_FMT, &fmtset);
        UWLOG(("SetFormat preferred 0x%08X(%c%c%c%c) %Rrc\n",
               preferredFormat,
               RT_BYTE1(preferredFormat), RT_BYTE2(preferredFormat),
               RT_BYTE3(preferredFormat), RT_BYTE4(preferredFormat),
               rc));

        if (RT_FAILURE(rc))
        {
            /* If webcam format is not one of supported, then try to set a supported format. */
            if (hwcIsSupportedFormat(fmt.fmt.pix.pixelformat))
            {
                /* Set the supported webcam format. */
                hwcInitFormat(&fmtset, &fmt, cWidth, cHeight, fmt.fmt.pix.pixelformat);
                rc = hwcIoctl(handle, VIDIOC_S_FMT, &fmtset);
                UWLOG(("SetFormat supported %Rrc\n", rc));
            }
            else
            {
                /* Try to set one of supported formats. */
                hwcInitFormat(&fmtset, &fmt, cWidth, cHeight, V4L2_PIX_FMT_YUYV);
                rc = hwcIoctl(handle, VIDIOC_S_FMT, &fmtset);
                UWLOG(("SetFormat YUYV %Rrc\n", rc));

                if (RT_FAILURE(rc))
                {
                    hwcInitFormat(&fmtset, &fmt, cWidth, cHeight, V4L2_PIX_FMT_YUV420);
                    rc = hwcIoctl(handle, VIDIOC_S_FMT, &fmtset);
                    UWLOG(("SetFormat YUV420 %Rrc\n", rc));
                }

                if (RT_FAILURE(rc))
                {
                    hwcInitFormat(&fmtset, &fmt, cWidth, cHeight, V4L2_PIX_FMT_UYVY);
                    rc = hwcIoctl(handle, VIDIOC_S_FMT, &fmtset);
                    UWLOG(("SetFormat UYVY %Rrc\n", rc));
                }
            }
        }

        if (RT_SUCCESS(rc))
        {
            if (u32FrameInterval)
            {
                struct v4l2_streamparm streamparm;
                RT_ZERO(streamparm);
                streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                int rc2 = hwcIoctl(handle, VIDIOC_G_PARM, &streamparm);
                if (RT_SUCCESS(rc2))
                {
                    streamparm.parm.capture.timeperframe.numerator = u32FrameInterval;
                    streamparm.parm.capture.timeperframe.denominator = 1000 * 1000 * 10;
                    hwcIoctl(handle, VIDIOC_S_PARM, &streamparm);
                }
                /* Ignore error. */
            }

            *pSelectedFormat = fmtset.fmt.pix.pixelformat;
        }
    }

    return rc;
}

static int hwcSetupBuffers(HOSTWEBCAM *pHostWebcam, uint32_t cBuffers)
{
    struct v4l2_requestbuffers req;
    RT_ZERO(req);
    req.count = cBuffers;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    int rc = hwcIoctl(pHostWebcam->hDevice, VIDIOC_REQBUFS, &req);
    if (RT_SUCCESS(rc))
    {
        pHostWebcam->fBuffersRequested = true;
        if (!req.count)
        {
            rc = VERR_NO_MEMORY;
        }
        else
        {
            /* Map buffers. */
            pHostWebcam->cBuffers = 0;
            pHostWebcam->paBuffers = (HWCMMAPBUFFER *)RTMemAllocZ(req.count * sizeof(HWCMMAPBUFFER));
            if (pHostWebcam->paBuffers)
            {
                uint32_t i;
                for (i = 0; i < req.count; i++)
                {
                    pHostWebcam->paBuffers[i].pv = MAP_FAILED;
                }

                for (i = 0; i < req.count; i++)
                {
                    struct v4l2_buffer buf;
                    RT_ZERO(buf);
                    buf.type = req.type;
                    buf.memory = V4L2_MEMORY_MMAP;
                    buf.index = i;

                    rc = hwcIoctl(pHostWebcam->hDevice, VIDIOC_QUERYBUF, &buf);
                    if (RT_FAILURE(rc))
                    {
                        break;
                    }

                    /* mmap parameters by V4L2 API spec: 3.2. Streaming I/O (Memory Mapping) */
                    pHostWebcam->paBuffers[i].cb = buf.length;
                    pHostWebcam->paBuffers[i].pv = mmap(NULL, buf.length,
                                                        PROT_READ | PROT_WRITE,
                                                        MAP_SHARED,
                                                        pHostWebcam->hDevice, buf.m.offset);

                    if (pHostWebcam->paBuffers[i].pv == MAP_FAILED)
                    {
                        rc = VERR_NOT_SUPPORTED;
                        break;
                    }

                    pHostWebcam->cBuffers++;
                }
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }
        }
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static void hwcSample(PDRVHOSTWEBCAM pThis, void *pvData, uint32_t cbData)
{
    UWLOGF(("%p cbActual %d\n%.*Rhxd\n",
             pvData, cbData, cbData <= 32? cbData: 32, pvData));

    if (pThis && pThis->pIWebcamUp)
    {
        int rc;
        uint8_t *pu8Encoded;
        uint32_t cbEncoded;

        void *pvFrame = NULL;
        uint32_t cbFrame = 0;

        switch (pThis->pHostWebcam->pixelformat)
        {
            default:
            case v4l2_fourcc('M','J','P','G'):
            {
                pvFrame = pvData;
                cbFrame = cbData;
            } break;

            case V4L2_PIX_FMT_YUYV:
            {
                pu8Encoded = 0;
                cbEncoded = 0;

                rc = HWCJPEGEncodeFrameRaw_2vuy_yuvs(pThis->pHostWebcam->pEnc, 75,
                                                     &pu8Encoded, &cbEncoded,
                                                     (uint8_t *)pvData, cbData,
                                                     pThis->pHostWebcam->lastSetup.u16Width,
                                                     pThis->pHostWebcam->lastSetup.u16Height,
                                                     true);
                if (RT_SUCCESS(rc))
                {
                    pvFrame = pu8Encoded;
                    cbFrame = cbEncoded;
                }
            } break;

            case V4L2_PIX_FMT_UYVY:
            {
                pu8Encoded = 0;
                cbEncoded = 0;

                rc = HWCJPEGEncodeFrameRaw_2vuy_yuvs(pThis->pHostWebcam->pEnc, 75,
                                                     &pu8Encoded, &cbEncoded,
                                                     (uint8_t *)pvData, cbData,
                                                     pThis->pHostWebcam->lastSetup.u16Width,
                                                     pThis->pHostWebcam->lastSetup.u16Height,
                                                     false);
                if (RT_SUCCESS(rc))
                {
                    pvFrame = pu8Encoded;
                    cbFrame = cbEncoded;
                }
            } break;

            case V4L2_PIX_FMT_YUV420:
            {
                pu8Encoded = 0;
                cbEncoded = 0;

                rc = HWCJPEGEncodeFrameRaw_I420(pThis->pHostWebcam->pEnc, 75,
                                                &pu8Encoded, &cbEncoded,
                                                (uint8_t *)pvData, cbData,
                                                pThis->pHostWebcam->lastSetup.u16Width,
                                                pThis->pHostWebcam->lastSetup.u16Height);
                if (RT_SUCCESS(rc))
                {
                    pvFrame = pu8Encoded;
                    cbFrame = cbEncoded;
                }
            } break;
        }

        if (pvFrame)
        {
            VRDEVIDEOINPAYLOADHDR hdr;
            hdr.u8HeaderLength      = sizeof(VRDEVIDEOINPAYLOADHDR);
            hdr.u8HeaderInfo        = VRDE_VIDEOIN_PAYLOAD_F_EOF;
            hdr.u32PresentationTime = 0;
            hdr.u32SourceTimeClock  = 0;
            hdr.u16Reserved         = 0;
            pThis->pIWebcamUp->pfnFrame(pThis->pIWebcamUp,
                                        1,
                                        &hdr,
                                        sizeof(hdr),
                                        pvFrame,
                                        cbFrame);
            if (pvFrame != pvData)
            {
                RTMemFree(pvFrame);
            }
        }
    }
}

static uint32_t hwcFrameFormatFind(HOSTWEBCAM *pHostWebcam, uint16_t u16Width, uint16_t u16Height)
{
    int iIndex;
    for (iIndex = 0; iIndex < pHostWebcam->cFrameFormats; ++iIndex)
    {
        HWCFRAMEFORMAT *p = &pHostWebcam->aFrameFormats[iIndex];
        if (p->u16Width == u16Width && p->u16Height == u16Height)
        {
            return p->u32PixelFormat;
        }
    }
    return V4L2_PIX_FMT_YUYV;
}


/*
 * Frame capture.
 */

static int hwcCaptureStop(PDRVHOSTWEBCAM pThis)
{
    int rc = VINF_SUCCESS;
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    if (pHostWebcam->fStreaming)
    {
        ASMAtomicXchgBool(&pHostWebcam->fStreaming, false);

        /* Tell the monitor thread to stop. */
        if (pHostWebcam->monitor.hPipeWrite != NIL_RTPIPE)
        {
            size_t cbWritten = 0;
            char c = 's';
            rc = RTPipeWrite(pHostWebcam->monitor.hPipeWrite, &c, sizeof(c), &cbWritten);
        }

        /* Wait for the monitor thread to stop. */
        rc = RTSemEventWait(pHostWebcam->hEvtCapture, RT_MS_10SEC);
        AssertRC(rc);

        enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        hwcIoctl(pHostWebcam->hDevice, VIDIOC_STREAMOFF, &buf_type);
    }

    if (pHostWebcam->paBuffers)
    {
        uint32_t i;
        for (i = 0; i < pHostWebcam->cBuffers; i++)
        {
            if (pHostWebcam->paBuffers[i].pv != MAP_FAILED)
            {
                munmap(pHostWebcam->paBuffers[i].pv, pHostWebcam->paBuffers[i].cb);
            }
        }
        RTMemFree(pHostWebcam->paBuffers);
        pHostWebcam->paBuffers = NULL;
    }

    if (pHostWebcam->fBuffersRequested)
    {
        struct v4l2_requestbuffers req;
        RT_ZERO(req);
        req.count = 0; /* Free buffers. */
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        hwcIoctl(pHostWebcam->hDevice, VIDIOC_REQBUFS, &req);

        pHostWebcam->fBuffersRequested = false;
    }

    if (pHostWebcam->pEnc)
    {
        HWCJPEGDestroy(pHostWebcam->pEnc);
        pHostWebcam->pEnc = NULL;
    }

    if (pHostWebcam->hDevice != -1)
    {
        hwcClose(pHostWebcam->hDevice);
        pHostWebcam->hDevice = -1;
    }

    return rc;
}

static int hwcCaptureStart(PDRVHOSTWEBCAM pThis)
{
    int rc = VINF_SUCCESS;
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    do /* break loop */
    {
        /* If device is not yet opened. */
        if (pHostWebcam->hDevice == -1)
        {
            rc = hwcOpen(pHostWebcam->pszDevicePath, &pHostWebcam->hDevice);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_NOT_FOUND)
                {
                    hwcDetached(pThis);
                }
                break;
            }
        }

        /* Reset cropping as recommented in V4L2 API spec: 4.1.3. Image Format Negotiation. */
        hwcResetCropping(pHostWebcam->hDevice);

        /* Select device format. */
        uint32_t u32PreferredFormat = hwcFrameFormatFind(pHostWebcam,
                                                         pHostWebcam->lastSetup.u16Width,
                                                         pHostWebcam->lastSetup.u16Height);
        rc = hwcSetStreamFormat(pHostWebcam->hDevice,
                                pHostWebcam->lastSetup.u16Width,
                                pHostWebcam->lastSetup.u16Height,
                                pHostWebcam->lastSetup.u32FrameInterval,
                                u32PreferredFormat,
                                &pHostWebcam->pixelformat);
        if (RT_FAILURE(rc)) break;

        if (pHostWebcam->pixelformat != v4l2_fourcc('M','J','P','G'))
        {
            rc = HWCJPEGInit(&pHostWebcam->pEnc);
            if (RT_FAILURE(rc)) break;
        }

        rc = hwcSetupBuffers(pHostWebcam, 3);
        if (RT_FAILURE(rc)) break;

        /* Enqueue buffers. */
        uint32_t i;
        for (i = 0; i < pHostWebcam->cBuffers; i++)
        {
            struct v4l2_buffer buf;
            RT_ZERO(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            rc = hwcIoctl(pHostWebcam->hDevice, VIDIOC_QBUF, &buf);
            if (RT_FAILURE(rc)) break;
        }
        if (RT_FAILURE(rc)) break;

        /* Start streaming. */
        enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        rc = hwcIoctl(pHostWebcam->hDevice, VIDIOC_STREAMON, &buf_type);
        if (RT_FAILURE(rc)) break;

        pHostWebcam->fStreaming = true;

        /* The monitor thread will read frames. */
        if (pHostWebcam->monitor.hPipeWrite != NIL_RTPIPE)
        {
            size_t cbWritten = 0;
            char c = 's';
            rc = RTPipeWrite(pHostWebcam->monitor.hPipeWrite, &c, sizeof(c), &cbWritten);
        }

        UWLOG(("Running\n"));
    } while(0);

    if (RT_FAILURE(rc))
    {
        hwcCaptureStop(pThis);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}


/*
 * Device removal monitor and capture worker thread.
 */

static DECLCALLBACK(int) hwcThreadMonitorWorker(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    UWLOG(("#%d: state %d\n", pDrvIns->iInstance, pThread->enmState));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
    {
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    PDRVHOSTWEBCAM pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTWEBCAM);
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;
    bool fStreaming = false;

    while(pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
#ifdef RT_OS_SOLARIS
        /* Sync the streaming flag and signal the waiter if it changed from true to false. */
        bool fStreamingNow = ASMAtomicReadBool(&pHostWebcam->fStreaming);
        if (   fStreaming
            && !fStreamingNow)
        {
            int rc2 = RTSemEventSignal(pHostWebcam->hEvtCapture);
            AssertRC(rc2);
        }

        fStreaming = fStreamingNow;

        /* Solaris usbvc doesn't support polling or select. */
        if (!fStreaming)
        {
            RTThreadSleep(300);
            continue;
        }
        else
        {
            struct v4l2_buffer buf;
            RT_ZERO(buf);
            buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            int rc2 = hwcIoctl(pHostWebcam->hDevice, VIDIOC_DQBUF, &buf);
            switch (rc2)
            {
                case VINF_SUCCESS:
                {
                    if (buf.index >= pHostWebcam->cBuffers)
                    {
                        UWLOG(("VIDIOC_DQBUF: invalid index %d\n", buf.index));
                        RTThreadSleep(10);
                        continue;
                    }

                    hwcSample(pThis, pHostWebcam->paBuffers[buf.index].pv, buf.bytesused);

                    rc2 = hwcIoctl(pHostWebcam->hDevice, VIDIOC_QBUF, &buf);
                    if (RT_FAILURE(rc2))
                    {
                        UWLOG(("VIDIOC_QBUF failed: errno %d\n", errno));
                        RTThreadSleep(10);
                        continue;
                    }
                    break;
                }

                case VERR_TRY_AGAIN:
                {
                    UWLOG(("Host webcam VIDIOC_DQBUF sleeping\n"));
                    RTThreadSleep(20);
                    continue;
                }

                case VERR_INVALID_POINTER:    /* EFAULT: When in non-blocking mode. */
                case VERR_INTERRUPTED:        /* EINTR:  When in blocking mode. */
                {
                    UWLOG(("Host webcam VIDIOC_DQBUF device disconnected: rc2=%Rrc errno=%d\n", rc2, errno));
                    hwcDetached(pThis);
                    continue;
                }

                default:
                {
                    UWLOG(("Host webcam VIDIOC_DQBUF failed: %Rrc\n", rc2));
                    RTThreadSleep(100);
                    continue;
                }
            }
        }
#else
        /* Poll the control pipe and the device handle. */
        struct pollfd fds[2];
        RT_ZERO(fds);

        int nfds = 1;

        fds[0].fd = RTPipeToNative(pHostWebcam->monitor.hPipeRead);
        fds[0].events = POLLIN | POLLPRI;
        fds[0].revents = 0;

        if (pHostWebcam->fStreaming)
        {
            /* 'poll' returns immediately with the POLLERR flag if VIDIOC_QBUF
             * or VIDIOC_STREAMON were not called yet.
             * Therefore poll the device handle only if streaming was enabled.
             */
            fds[1].fd = pHostWebcam->hDevice;
            fds[1].events = POLLIN | POLLPRI;
            fds[1].revents = 0;
            nfds = 2;
        }

        int ret = poll(fds, nfds, -1);

        UWLOGF(("poll(nfds %d) %d, revents[0] 0x%08X, revents[1] 0x%08X, errno %d\n",
                nfds, ret, fds[0].revents, fds[1].revents, errno));

        if (ret == 0)
        {
            /* Timed out. */
            continue;
        }

        if (ret == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            RTThreadSleep(10);
            continue;
        }

        if (fds[0].revents)
        {
            /* Some message. */
            size_t cbRead = 0;
            char c = 0;
            int rc2 = RTPipeRead(pHostWebcam->monitor.hPipeRead, &c, sizeof(c), &cbRead);
            UWLOG(("Event [%c] cbRead %d %Rrc\n", c, cbRead, rc2)); NOREF(rc2);

            /* Sync the streaming flag and signal the waiter if it changed from true to false. */
            bool fStreamingNow = ASMAtomicReadBool(&pHostWebcam->fStreaming);
            if (   fStreaming
                && !fStreamingNow)
            {
                rc2 = RTSemEventSignal(pHostWebcam->hEvtCapture);
                AssertRC(rc2);
            }

            fStreaming = fStreamingNow;

            continue;
        }

        if (nfds > 1)
        {
            /* Also polled the webcam handle. */
            if (fds[1].revents & POLLHUP)
            {
                /* According to poll spec this means that the device has been disconnected. */
                UWLOG(("POLLHUP\n"));
                hwcDetached(pThis);
                continue;
            }

            if (fds[1].revents & POLLERR)
            {
                /* 'poll' sets the POLLERR flag if VIDIOC_QBUF or VIDIOC_STREAMON were not called yet. */
                UWLOG(("POLLERR\n"));
                RTThreadSleep(10);
                continue;
            }

            if (fds[1].revents & POLLNVAL)
            {
                UWLOG(("POLLNVAL\n"));
                RTThreadSleep(100);
                continue;
            }

            if (fStreaming)
            {
                /* Should have a frame. */
                struct v4l2_buffer buf;
                RT_ZERO(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                int rc2 = hwcIoctl(pHostWebcam->hDevice, VIDIOC_DQBUF, &buf);
                if (RT_FAILURE(rc2))
                {
                    UWLOG(("VIDIOC_DQBUF failed: errno %d\n", errno));
                    RTThreadSleep(10);
                    continue;
                }

                /* Do not log buf.input field because it was dropped and replaced with reserved2
                 * field in recent videodev2.h versions.
                 */
                UWLOGF(("VIDIOC_DQBUF: index %d, type %d, bytesused %d, flags 0x%08X field %d\n"
                        "              timecode %d, seq %d, mem %d, m.off %d, l %d, timestamp %d\n",
                        buf.index, buf.type, buf.bytesused, buf.flags, buf.field,
                        buf.timecode.type, buf.sequence, buf.memory, buf.m.offset,
                        buf.length, buf.timestamp.tv_sec));

                if (buf.index >= pHostWebcam->cBuffers)
                {
                    UWLOG(("VIDIOC_DQBUF: invalid index %d\n",
                           buf.index));
                    RTThreadSleep(10);
                    continue;
                }

                hwcSample(pThis, pHostWebcam->paBuffers[buf.index].pv, buf.bytesused);

                rc2 = hwcIoctl(pHostWebcam->hDevice, VIDIOC_QBUF, &buf);
                if (RT_FAILURE(rc2))
                {
                    UWLOG(("VIDIOC_QBUF failed: errno %d\n", errno));
                    RTThreadSleep(10);
                    continue;
                }
            }
        }
#endif
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static DECLCALLBACK(int) hwcThreadMonitorWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    RT_NOREF1(pThread);
    PDRVHOSTWEBCAM pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTWEBCAM);
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    UWLOG(("#%d\n", pDrvIns->iInstance));
    int rc = VINF_SUCCESS;
    if (pHostWebcam->monitor.hPipeWrite != NIL_RTPIPE)
    {
        size_t cbWritten = 0;
        char c = 'w';
        rc = RTPipeWrite(pHostWebcam->monitor.hPipeWrite, &c, sizeof(c), &cbWritten);
    }

    return rc;
}

static int hwcMonitorStart(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis)
{
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    pHostWebcam->hEvtCapture = NIL_RTSEMEVENT;

    int rc = RTSemEventCreate(&pHostWebcam->hEvtCapture);
    if (RT_SUCCESS(rc))
    {
        rc = RTPipeCreate(&pHostWebcam->monitor.hPipeRead, &pHostWebcam->monitor.hPipeWrite, 0);
        if (RT_SUCCESS(rc))
        {
            rc = PDMDrvHlpThreadCreate(pDrvIns, &pHostWebcam->monitor.pThreadMonitor,
                                       NULL, hwcThreadMonitorWorker, hwcThreadMonitorWakeup,
                                       0, RTTHREADTYPE_IO, "HWCMONITOR");
            if (RT_SUCCESS(rc))
            {
                PDMDrvHlpThreadResume(pDrvIns, pHostWebcam->monitor.pThreadMonitor);
                return VINF_SUCCESS;
            }

            RTPipeClose(pHostWebcam->monitor.hPipeRead);
            pHostWebcam->monitor.hPipeRead = NIL_RTPIPE;
            RTPipeClose(pHostWebcam->monitor.hPipeWrite);
            pHostWebcam->monitor.hPipeWrite = NIL_RTPIPE;
        }

        RTSemEventDestroy(pHostWebcam->hEvtCapture);
        pHostWebcam->hEvtCapture = NIL_RTSEMEVENT;
    }

    return rc;
}

static void hwcMonitorStop(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis)
{
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;
    if (pHostWebcam->monitor.pThreadMonitor)
    {
        int rcThread = VINF_SUCCESS;
        PDMDrvHlpThreadDestroy(pDrvIns, pHostWebcam->monitor.pThreadMonitor, &rcThread);

        RTSemEventDestroy(pHostWebcam->hEvtCapture);
        pHostWebcam->hEvtCapture = NIL_RTSEMEVENT;

        pHostWebcam->monitor.pThreadMonitor = NULL;
        RTPipeClose(pHostWebcam->monitor.hPipeRead);
        pHostWebcam->monitor.hPipeRead = NIL_RTPIPE;
        RTPipeClose(pHostWebcam->monitor.hPipeWrite);
        pHostWebcam->monitor.hPipeWrite = NIL_RTPIPE;
    }
}


/*
 * Device description helpers.
 */

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

static void hwcFrameDescTryAdd(HOSTWEBCAM *pHostWebcam, int handle, HWCDEVICEDESC *pDesc, int *pcFrames,
                               struct v4l2_fmtdesc *pFmt, uint32_t cWidth, uint32_t cHeight)
{
    /* Every resolution is added to the pDesc->aFrames array and to the pHostWebcam->aFrameFormats.
     *
     * If a frame size is available in two or more formats (pFmt->pixelformat),
     * then MJPG format has highest priority for setting the frame properties,
     * for example supported intervals.
     *
     * HWCFRAMEFORMAT::u32PixelFormat contains preferred format for a frame resolution.
     * The format will be used in hwcSetStreamFormat as the first.
     * This should avoid conversion in V4L2, for example when a frame is not available
     * in MJPG. Old code always tried MJPG first and V4L2 provided JPG, which was not
     * understood by some guests.
     */
    const bool fMJPG = pFmt->pixelformat == v4l2_fourcc('M','J','P','G');

    /* Check for duplicates. */
    int iExistingFrame = hwcFrameDescFind(pDesc, *pcFrames,
                                          (uint16_t)cWidth,
                                          (uint16_t)cHeight);

    int iFrame = iExistingFrame == -1? *pcFrames: iExistingFrame;

    UWLOG(("%dx%d iExisting %d%s\n",
           cWidth, cHeight,
           iExistingFrame, fMJPG? " MJPG": ""));

    VRDEVIDEOINFRAMEDESC *p = &pDesc->aFrames[iFrame].frame;

    /* u8FrameFlags is temporarily used to indicate that the frame description was from MJPG format. */
    if (iExistingFrame != -1 && p->u8FrameFlags != 0)
    {
        /* Do not alter the frame anymore. */
        return;
    }

    if (iExistingFrame == -1)
    {
        /* First time init. */
        p->cbFrame      = sizeof(HWCFRAMEDESC);
        p->u8FrameId    = (uint8_t)(iFrame + 1);
        p->u8FrameFlags = 0;
        p->u16Width     = cWidth;
        p->u16Height    = cHeight;

        pDesc->aFrames[iFrame].cbIntervals = sizeof(pDesc->aFrames[iFrame].au32Intervals);

        /* Remember the format to be used with this frame size. If the format is not supported,
         * then use the uncompressed format to make sure that HWCJPEG is used. */
        pHostWebcam->aFrameFormats[iFrame].u32PixelFormat = hwcIsSupportedFormat(pFmt->pixelformat)?
                                                                pFmt->pixelformat:
                                                                V4L2_PIX_FMT_YUYV;
        pHostWebcam->aFrameFormats[iFrame].u16Width = cWidth;
        pHostWebcam->aFrameFormats[iFrame].u16Height = cHeight;
        pHostWebcam->cFrameFormats = iFrame + 1;
    }

    /* If this is a new frame or it is in MJPG format, then update the frame properties. */
    if (iExistingFrame != -1 && !fMJPG)
    {
        /* No need to update anything. */
        return;
    }

    if (fMJPG)
    {
        /* Mark the frame as complete. */
        p->u8FrameFlags =   VRDE_VIDEOIN_F_FRM_DISCRETE_INTERVALS
                          | VRDE_VIDEOIN_F_FRM_SIZE_OF_FIELDS;
        pHostWebcam->aFrameFormats[iFrame].u32PixelFormat = pFmt->pixelformat; /* MJPG has highest priority. */
    }

    p->u32MinFrameInterval = 0xFFFFFFFF;
    p->u32MaxFrameInterval = 0;
    RT_ZERO(pDesc->aFrames[iFrame].au32Intervals);

    int iDst = 0;

#ifdef VIDIOC_ENUM_FRAMEINTERVALS
    struct v4l2_frmivalenum frmival;
    uint32_t u32IntervalIndex = 0;
    for(;; u32IntervalIndex++)
    {
        if (iDst >= HWC_MAX_INTERVALS)
        {
            break;
        }

        uint32_t u32Interval = 0; /* In 100 nanoseconds unit. */

        RT_ZERO(frmival);
        frmival.index = u32IntervalIndex;
        frmival.pixel_format = pFmt->pixelformat;
        frmival.width = cWidth;
        frmival.height = cHeight;

        int rc = hwcIoctl(handle, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);
        if (RT_FAILURE(rc))
        {
            if (u32IntervalIndex > 0)
            {
                break; /* No more intervals. */
            }

            /* Apparently VIDIOC_ENUM_FRAMEINTERVALS is not supported, add default. */
            u32Interval = HWC_DEFAULT_INTERVAL;
        }
        else
        {
            if (frmival.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
#ifdef RT_OS_SOLARIS
                u32Interval = ASMDivU64ByU32RetU32((uint64_t)frmival.u.discrete.numerator * UINT64_C(10000000),
                                                   frmival.u.discrete.denominator);
#else
                u32Interval = ASMDivU64ByU32RetU32((uint64_t)frmival.discrete.numerator * UINT64_C(10000000),
                                                   frmival.discrete.denominator);
#endif

            }
            else if (   frmival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS
                     || frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE)
            {
                UWLOG(("Not discrete frame interval\n"));
                /* Add min frame interval. */
#ifdef RT_OS_SOLARIS
                u32Interval = ASMDivU64ByU32RetU32((uint64_t)frmival.u.stepwise.min.numerator * UINT64_C(10000000),
                                                   frmival.u.stepwise.min.denominator);
#else
                u32Interval = ASMDivU64ByU32RetU32((uint64_t)frmival.stepwise.min.numerator * UINT64_C(10000000),
                                                   frmival.stepwise.min.denominator);
#endif

                rc = VERR_NOT_SUPPORTED; /* Stop the loop after adding the interval. */
            }
            else
            {
                UWLOG(("Unsupported frame interval\n"));
                u32Interval = HWC_DEFAULT_INTERVAL;
                rc = VERR_NOT_SUPPORTED; /* Stop the loop after adding the interval. */
            }
        }

        /* Add interval in any case, because some failure paths above add an interval too. */
        pDesc->aFrames[iFrame].au32Intervals[iDst] = u32Interval;
        p->u32MinFrameInterval = RT_MIN(p->u32MinFrameInterval, u32Interval);
        p->u32MaxFrameInterval = RT_MAX(p->u32MaxFrameInterval, u32Interval);
        iDst++;

        if (RT_FAILURE(rc))
        {
            break;
        }
    }
#endif

    if (iDst == 0)
    {
        /* No intervals, maybe VIDIOC_ENUM_FRAMEINTERVALS does not work, add default. */
        pDesc->aFrames[iFrame].au32Intervals[iDst] = HWC_DEFAULT_INTERVAL;
        p->u32MinFrameInterval = pDesc->aFrames[iFrame].au32Intervals[iDst];
        p->u32MaxFrameInterval = p->u32MinFrameInterval;
        iDst++;
    }

    p->u32NumFrameIntervals = iDst;

    if (UWLOGENABLED())
    {
        int i;
        UWLOG(("intervals %d:", iDst));
        for (i = 0; i < iDst; i++)
        {
            UWLOG_NO_FN((" %u", pDesc->aFrames[iFrame].au32Intervals[i]));
        }
        UWLOG_NO_FN(("\n"));
    }

    /* It was a new frame. */
    if (iExistingFrame == -1)
    {
        *pcFrames = iFrame + 1;
    }
}

static int hwcBuildDeviceDesc(PDRVHOSTWEBCAM pThis,
                              int handle,
                              VRDEVIDEOINDEVICEDESC **ppDeviceDesc,
                              uint32_t *pcbDeviceDesc)
{
    int rc = VINF_SUCCESS;
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    /* Preallocate a buffer for frames. */
    uint32_t cbDesc = sizeof(HWCDEVICEDESC) + sizeof(HWCFRAMEDESC) * (HWC_MAX_FRAMES - 1);
    HWCDEVICEDESC *pDesc = (HWCDEVICEDESC *)RTMemAllocZ(cbDesc);
    if (pDesc)
    {
        pHostWebcam->cFrameFormats = 0;
        RT_ZERO(pHostWebcam->aFrameFormats);

        int cFrames = 0;

        /* Enumerate all formats and add frame sizes from supported formats. */
        struct v4l2_fmtdesc fmt;
        uint32_t u32FormatIndex = 0;
        for(; cFrames < HWC_MAX_FRAMES; u32FormatIndex++)
        {
            RT_ZERO(fmt);
            fmt.index = u32FormatIndex;
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            rc = hwcIoctl(handle, VIDIOC_ENUM_FMT, &fmt);
            if (RT_FAILURE(rc))
            {
                break;
            }

            UWLOG(("format: flags 0x%08X '%s' 0x%08X(%c%c%c%c)\n",
                   fmt.flags, fmt.description, fmt.pixelformat,
                   RT_BYTE1(fmt.pixelformat), RT_BYTE2(fmt.pixelformat),
                   RT_BYTE3(fmt.pixelformat), RT_BYTE4(fmt.pixelformat)));

            /* Check that this is one of supported formats. */
            if (hwcIsSupportedFormat(fmt.pixelformat))
            {
                UWLOG(("Supported format\n"));

                /* Enumerate frame sizes. */
#ifdef VIDIOC_ENUM_FRAMESIZES
                struct v4l2_frmsizeenum frmsize;
                uint32_t u32FrameIndex = 0;
                for(;; u32FrameIndex++)
                {
                    RT_ZERO(frmsize);
                    frmsize.index = u32FrameIndex;
                    frmsize.pixel_format = fmt.pixelformat;

                    rc = hwcIoctl(handle, VIDIOC_ENUM_FRAMESIZES, &frmsize);
                    if (RT_FAILURE(rc))
                    {
                        if (u32FrameIndex == 0)
                        {
                            /* Apparently VIDIOC_ENUM_FRAMESIZES is not supported, add 640x480. */
                            hwcFrameDescTryAdd(pHostWebcam, handle, pDesc, &cFrames, &fmt, 640, 480);
                        }
                        break;
                    }

                    if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
                    {
#ifdef RT_OS_SOLARIS
                        hwcFrameDescTryAdd(pHostWebcam, handle, pDesc, &cFrames, &fmt,
                                           frmsize.u.discrete.width, frmsize.u.discrete.height);
#else
                        hwcFrameDescTryAdd(pHostWebcam, handle, pDesc, &cFrames, &fmt,
                                           frmsize.discrete.width, frmsize.discrete.height);
#endif
                    }
                    else
                    {
                        UWLOG(("Not discrete frame size, skipping\n"));
                        break;
                    }
                }
#else
                /* VIDIOC_ENUM_FRAMESIZES is not supported, add 640x480. */
                hwcFrameDescTryAdd(pHostWebcam, handle, pDesc, &cFrames, &fmt, 640, 480);
#endif
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

            if (pThis->pHostWebcam->pszSerial)
            {
                pDesc->ext.ext.fu32Fields |= VRDE_VIDEOIN_F_DEV_EXT_SERIAL;
                pDesc->ext.hdrSerial.cbField =   sizeof(VRDEVIDEOINDEVICEFIELDHDR)
                                               + sizeof(pDesc->ext.achSerial);
                RTStrCopy(pDesc->ext.achSerial, sizeof(pDesc->ext.achSerial),
                          pThis->pHostWebcam->pszSerial);
            }

            pDesc->format.cbFormat      = sizeof(VRDEVIDEOINFORMATDESC);
            pDesc->format.u8FormatId    = 1;
            pDesc->format.u8FormatType  = VRDE_VIDEOIN_FORMAT_MJPEG;
            pDesc->format.u8FormatFlags = 0;
            pDesc->format.u8NumFrames   = (uint8_t)cFrames;
            pDesc->format.u16Reserved   = 0;

            rc = VINF_SUCCESS;
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

    UWLOGFLEAVERC(rc);
    return rc;
}


/*
 * Device discovery helper functions.
 */

static void hwcClearDeviceInfo(PDRVHOSTWEBCAM pThis)
{
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;
    RTStrFree(pHostWebcam->pszDevicePath);
    pHostWebcam->pszDevicePath = NULL;
    RTStrFree(pHostWebcam->pszDeviceName);
    pHostWebcam->pszDeviceName = NULL;
    RTStrFree(pHostWebcam->pszSerial);
    pHostWebcam->pszSerial = NULL;
}

static int hwcStoreDeviceInfo(PDRVHOSTWEBCAM pThis, const char *pszPath,
                              const char *pszName, const char *pszSerial)
{
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    pHostWebcam->pszDevicePath = RTStrDup(pszPath);
    pHostWebcam->pszDeviceName = RTStrDup(pszName);
    pHostWebcam->pszSerial = RTStrDup(pszSerial);

    if (   !pHostWebcam->pszDevicePath
        || !pHostWebcam->pszDeviceName
        || !pHostWebcam->pszSerial)
    {
        hwcClearDeviceInfo(pThis);
        return VERR_NO_MEMORY;
    }

    UWLOG(("Device: %s, %s, %s\n",
           pHostWebcam->pszDevicePath, pHostWebcam->pszDeviceName, pHostWebcam->pszSerial));
    return VINF_SUCCESS;
}

#if defined(RT_OS_LINUX)
static bool hwcLinuxVideoCaptureDevice(struct v4l2_capability_3_4_0 *pCaps)
{
    if (pCaps)
    {
        /* Starting from kernel version 4.12.0, V4L2_CAP_META_CAPTURE was introduced.
         * We filter out devices that report this capability because they do not provide
         * actual image data. The way to exclude such devices is to look into @device_caps
         * field instead of @capabilities. Field @device_caps was introduced in
         * kernel 3.1.0. We detect if we can use @device_caps in runtime in order to
         * support build on older platforms. */
        uint32_t caps = (pCaps->version >= VBOX_WITH_V4L2_CAP_META_CAPTURE) ?
            pCaps->device_caps : pCaps->capabilities;

        return ((caps & V4L2_CAP_VIDEO_CAPTURE) != 0
            && (caps & V4L2_CAP_STREAMING) != 0);
    }

    return false;
}
#endif

static int hwcFindDevice(PDRVHOSTWEBCAM pThis)
{
    int rc = VINF_SUCCESS;

    /* Find the webcam and save the path, name and serial. */
    hwcClearDeviceInfo(pThis);

    UWLOG(("idx %d, path [%s]\n",
           pThis->i32DeviceIndex, pThis->pszDevicePath));

#if defined(RT_OS_LINUX)
    struct v4l2_capability_3_4_0 caps;
#else
    struct v4l2_capability caps;
#endif
    RT_ZERO(caps);

    char *pszPath = NULL;
    char *pszSerial = NULL;

    if (pThis->i32DeviceIndex == -1)
    {
        /* Device with the specified pThis->pszDevicePath. */
        pszPath = pThis->pszDevicePath;
    }
    else if (pThis->i32DeviceIndex >= 0)
    {
        /* Treat index 0 and 1 the same way. */
        int iDevice = pThis->i32DeviceIndex == 0? 1: pThis->i32DeviceIndex;
        RTStrAPrintf(&pszPath, "/dev/video%d", iDevice - 1); /* First device is /dev/video0 */
    }

    if (pszPath)
    {
        int handle = -1;
        rc = hwcOpen(pszPath, &handle);
        if (RT_SUCCESS(rc))
        {
            rc = hwcIoctl(handle, VIDIOC_QUERYCAP, &caps);
            if (RT_SUCCESS(rc))
            {
                if (
#if defined(RT_OS_LINUX)
                    /* On Linux host make sure that actual device has required
                     * capabilities. Filter out meta devices (V4L2_CAP_META_CAPTURE). */
                    hwcLinuxVideoCaptureDevice(&caps))
#else
                       (caps.capabilities & V4L2_CAP_VIDEO_CAPTURE) != 0
                    && (caps.capabilities & V4L2_CAP_STREAMING) != 0)
#endif
                {
                    /* Get an unique id for pszSerial. Use the card name for now. */
                    uint64_t u64 = RTCrc64(&caps.card[0], strlen((char *)caps.card));
                    RTStrAPrintf(&pszSerial, "%RX64", u64);
                    if (!pszSerial)
                    {
                        rc = VERR_NO_MEMORY;
                    }
                }
                else
                {
                    rc = VERR_NOT_FOUND;
                }
            }

            hwcClose(handle);
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        hwcStoreDeviceInfo(pThis, pszPath, (char *)caps.card, pszSerial);
    }

    if (pThis->i32DeviceIndex != -1)
    {
        RTStrFree(pszPath);
    }

    return RT_SUCCESS(rc)? rc: VERR_NOT_FOUND; /* Caller expects it if device can't be used. */
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
    int handle = -1;
    int rc = hwcOpen(pThis->pHostWebcam->pszDevicePath, &handle);
    if (RT_SUCCESS(rc))
    {
        rc = hwcBuildDeviceDesc(pThis, handle, ppDeviceDesc, pcbDeviceDesc);

        hwcClose(handle);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

int hostWebcamConstruct(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis, PCFGMNODE pCfg)
{
    /* One time initialization of the backend. */
    NOREF(pCfg);

    pThis->pHostWebcam = (HOSTWEBCAM *)RTMemAllocZ(sizeof(HOSTWEBCAM));
    AssertReturn(pThis->pHostWebcam,  VERR_NO_MEMORY);

    pThis->pHostWebcam->hDevice = -1;
    pThis->pHostWebcam->monitor.hPipeRead = NIL_RTPIPE;
    pThis->pHostWebcam->monitor.hPipeWrite = NIL_RTPIPE;

    int rc = hwcFindDevice(pThis);
    if (RT_SUCCESS(rc))
        rc = hwcMonitorStart(pDrvIns, pThis);

    return rc;
}

void hostWebcamDestruct(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis)
{
    if (pThis->pHostWebcam)
    {
        hwcMonitorStop(pDrvIns, pThis);

        if (pThis->pHostWebcam->hDevice != -1)
        {
            hwcClose(pThis->pHostWebcam->hDevice);
            pThis->pHostWebcam->hDevice = -1;
        }

        /* Destruction of the backend. */
        hwcClearDeviceInfo(pThis);

        RTMemFree(pThis->pHostWebcam);
        pThis->pHostWebcam = NULL;
    }
}

