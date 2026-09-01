/* $Id: jpegtest.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2006-2025 Oracle and/or its affiliates.
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

#include "jpegtest.h"

static void bmpTest(const char *pszBMP)
{
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;
    uint32_t u32BitsPerPixel = 0;

    uint8_t *pu8Bitmap = bmpRead(pszBMP, &u32Width, &u32Height, &u32BitsPerPixel);

    RTPrintf ("%dx%d:\n", u32Width, u32Height);

    VEJPEG *pEncoder;
    int rc = VEJPEGInit(&pEncoder);
    RTPrintf("VEJPEGInit %Rrc\n", rc);
    if (RT_SUCCESS(rc))
    {
        uint8_t *pu8JPEG;
        uint32_t cbJPEG;

        int iJPEGQuality;
        for (iJPEGQuality = 70; iJPEGQuality <= 70; iJPEGQuality += 10)
        {
            /* Get size. */
            rc = VEJPEGEncodeFrameBGR0(pEncoder, iJPEGQuality,
                                       &pu8JPEG,
                                       &cbJPEG,
                                       pu8Bitmap,
                                       u32Width, u32Height);
            if (RT_FAILURE (rc))
            {
                RTPrintf("videoEncoderEncodeFrame %Rrc\n", rc);
                break;
            }

            jpgWrite ("testResult.jpg", pu8JPEG, cbJPEG);

            RTMemFree(pu8JPEG);

            uint64_t u64TimeStart = RTTimeNanoTS();

            int iIterations = 1000;
            int r;
            for (r = 0; r < iIterations; r++)
            {
                rc = VEJPEGEncodeFrameBGR0(pEncoder, iJPEGQuality,
                                           &pu8JPEG,
                                           &cbJPEG,
                                           pu8Bitmap,
                                           u32Width, u32Height);

                if (RT_FAILURE (rc))
                {
                    RTPrintf("videoEncoderEncodeFrame %Rrc\n", rc);
                    break;
                }

                RTMemFree(pu8JPEG);
            }

            uint64_t u64TimeEnd = RTTimeNanoTS();

            uint64_t u64AverageTime = (u64TimeEnd - u64TimeStart) / iIterations / 1000; /* nano -> micro */

            RTPrintf("%3d %d %llu\n", iJPEGQuality, cbJPEG, u64AverageTime);
        }

        VEJPEGDestroy(pEncoder);
    }

    bmpFree(pu8Bitmap);
}

static void bmpCompressTest(VEJPEG *pEncoder, uint8_t *pu8Bitmap, uint32_t u32Width, uint32_t u32Height)
{
    uint8_t *pu8JPEG;
    uint32_t cbJPEG;

    int iJPEGQuality;
    for (iJPEGQuality = 10; iJPEGQuality <= 100; iJPEGQuality += 10)
    {
        /* Get size. */
        int rc = VEJPEGEncodeFrameBGR0(pEncoder, iJPEGQuality,
                                       &pu8JPEG,
                                       &cbJPEG,
                                       pu8Bitmap,
                                       u32Width, u32Height);
        if (RT_FAILURE (rc))
        {
            RTPrintf("VEJPEGEncodeFrame %Rrc\n", rc);
            break;
        }

        RTMemFree(pu8JPEG);

        uint64_t u64TimeStart = RTTimeNanoTS();

        int r;
        for (r = 0; r < 100; r++)
        {
            rc = VEJPEGEncodeFrameBGR0(pEncoder, iJPEGQuality,
                                       &pu8JPEG,
                                       &cbJPEG,
                                       pu8Bitmap,
                                       u32Width, u32Height);

            if (RT_FAILURE (rc))
            {
                RTPrintf("VEJPEGEncodeFrame %Rrc\n", rc);
                break;
            }

            RTMemFree(pu8JPEG);
        }

        uint64_t u64TimeEnd = RTTimeNanoTS();

        uint64_t u64AverageTime = (u64TimeEnd - u64TimeStart) / 100 / 1000; /* nano -> micro */

        RTPrintf("%3d %d %llu\n", iJPEGQuality, cbJPEG, u64AverageTime);
    }
}

static void blackCompressTest(const char *pszBMP)
{
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;
    uint32_t u32BitsPerPixel = 0;

    uint8_t *pu8OriginalBitmap = bmpRead(pszBMP, &u32Width, &u32Height, &u32BitsPerPixel);

    RTPrintf ("%dx%d:\n", u32Width, u32Height);

    VEJPEG *pEncoder;
    int rc = VEJPEGInit(&pEncoder);
    RTPrintf("VEJPEGInit %Rrc\n", rc);
    if (RT_SUCCESS(rc))
    {
        /* Test original size. */
        bmpCompressTest(pEncoder, pu8OriginalBitmap, u32Width, u32Height);

        /* Make a larger bitmap with black surroundings. */
        uint32_t u32SizeLargeImage = 1920 * 1200 * 4;

        uint8_t *pu8LargeBitmap = (uint8_t *)RTMemAllocZ(u32SizeLargeImage);

        unsigned y;
        uint8_t *pu8Src = pu8OriginalBitmap;
        uint8_t *pu8Dst = pu8LargeBitmap + (1920 * 4) * (1200 - u32Height);
        for (y = 0; y < u32Height; y++)
        {
            memcpy(pu8Dst, pu8Src, u32Width * 4);
            pu8Src += u32Width * 4;
            pu8Dst += 1920 * 4;
        }

        bmpWrite ("bmplarge.bmp", pu8LargeBitmap, 1920, 1200);

        bmpCompressTest(pEncoder, pu8LargeBitmap, 1920, 1200);

        VEJPEGDestroy(pEncoder);

        RTMemFree(pu8LargeBitmap);
    }

    bmpFree(pu8OriginalBitmap);
}

void gdImageCopyResampled (uint8_t *dst,
                           uint8_t *src,
                           int dstX, int dstY,
                           int srcX, int srcY,
                           int dstW, int dstH, int srcW, int srcH);

typedef struct CTX
{
    uint8_t *src;
    int srcW;
} CTX;

static uint32_t getTrueColorPixel(const uint8_t *pu8SrcLine, int x)
{
//return 0x808080;
    return *(int32_t *)(pu8SrcLine + x * 4);
}

extern int BitmapDownscale32MapOpt(uint8_t *pu8Dst,
                            uint32_t u32DstW, uint32_t u32DstH,
                            const uint8_t *pu8Src,
                            int32_t i32SrcDeltaLine,
                            uint32_t u32SrcW, uint32_t u32SrcH);

static void scaleTest(const char *pszBMP)
{
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;
    uint32_t u32BitsPerPixel = 0;

    uint8_t *pu8Bitmap = bmpRead(pszBMP, &u32Width, &u32Height, &u32BitsPerPixel);

    RTPrintf ("%dx%d:\n", u32Width, u32Height);

    /* Allocate enough for a full bitmap to test various downscale values. */
    uint8_t *pu8Dest = (uint8_t *)RTMemAlloc(u32Width * u32Height * 4);

    if (pu8Dest)
    {
        CTX ctx;
        ctx.src = pu8Bitmap;
        ctx.srcW = u32Width;
        BMPSCALE s;

//        uint32_t u32DstWidth = u32Width / 2; // 636;
//        uint32_t u32DstHeight = u32Height / 2; // 357;
        uint32_t u32DstWidth = 636;
        uint32_t u32DstHeight = 357;

        uint64_t u64TimeStart = RTTimeNanoTS();

        int r;
//        for (r = 0; r < 100; r++)
        {
            gdImageCopyResampled (pu8Dest,
                                  pu8Bitmap,
                                  0, 0,
                                  0, 0,
                                  u32DstWidth, u32DstHeight,
                                  u32Width, u32Height);
        }

        uint64_t u64TimeEnd = RTTimeNanoTS();

        uint64_t u64AverageTime = (u64TimeEnd - u64TimeStart) / 100 / 1000; /* nano -> micro */

        RTPrintf("Floating point scale %llu\n", u64AverageTime);

        u64TimeStart = RTTimeNanoTS();

//        for (r = 0; r < 100; r++)
        {
            BitmapDownscale (pu8Dest,
                                  u32DstWidth, u32DstHeight,
                                  getTrueColorPixel, pu8Bitmap, u32Width  * 4,
                                  u32Width, u32Height);
        }

        u64TimeEnd = RTTimeNanoTS();

        u64AverageTime = (u64TimeEnd - u64TimeStart) / 100 / 1000; /* nano -> micro */

        RTPrintf("Scale fast generic %llu\n", u64AverageTime);

        u64TimeStart = RTTimeNanoTS();

//        for (r = 0; r < 1000; r++)
        {
            BitmapDownscale32 (pu8Dest,
                                  u32DstWidth, u32DstHeight,
                                  pu8Bitmap, u32Width  * 4,
                                  u32Width, u32Height);
        }

        u64TimeEnd = RTTimeNanoTS();

        u64AverageTime = (u64TimeEnd - u64TimeStart) / 1000 / 1000; /* nano -> micro */

        RTPrintf("Scale fast 32bpp %llu\n", u64AverageTime);


        /* Subsampling. */
        u64TimeStart = RTTimeNanoTS();

//        for (r = 0; r < 1000; r++)
        {
            BitmapDownscale32Avg2(pu8Dest,
                                  u32DstWidth, u32DstHeight,
                                  pu8Bitmap,
                                  u32Width * 4,
                                  u32Width, u32Height);
        }

        u64TimeEnd = RTTimeNanoTS();

        u64AverageTime = (u64TimeEnd - u64TimeStart) / 1000 / 1000; /* nano -> micro */

        RTPrintf("Scale subsample %llu mcs\n", u64AverageTime);


        /* Subsampling with context. */
        u64TimeStart = RTTimeNanoTS();

        BmpScaleInit(&s, u32DstWidth, u32DstHeight, u32Width, u32Height,
                     BMPSCALE_AREAMAP);

//        for (r = 0; r < 1000; r++)
        {
            s.pfnScale(s.pCtx, pu8Dest, pu8Bitmap, u32Width * 4);
        }

        s.pfnDestroy(s.pCtx);

        u64TimeEnd = RTTimeNanoTS();

        u64AverageTime = (u64TimeEnd - u64TimeStart) / 1000 / 1000; /* nano -> micro */

        RTPrintf("Scale subsample %llu mcs\n", u64AverageTime);

        /* Area map optimized. */
        u64TimeStart = RTTimeNanoTS();

        for (r = 0; r < 1000; r++)
        {
            BitmapDownscale32MapOpt(pu8Dest,
                                    u32DstWidth, u32DstHeight,
                                    pu8Bitmap,
                                    u32Width * 4,
                                    u32Width, u32Height);
        }

        u64TimeEnd = RTTimeNanoTS();

        u64AverageTime = (u64TimeEnd - u64TimeStart) / 1000 / 1000; /* nano -> micro */

        RTPrintf("Scale MapOpt %llu mcs\n", u64AverageTime);

        /* Downscaling quality. */
        u32DstWidth = (10 * u32Width) / 10;
        u32DstHeight = (10 * u32Height) / 10;

        gdImageCopyResampled (pu8Dest,
                              pu8Bitmap,
                              0, 0,
                              0, 0,
                              u32DstWidth, u32DstHeight,
                              u32Width, u32Height);

        bmpWrite ("scaled_float.bmp", pu8Dest, u32DstWidth, u32DstHeight);

        BitmapDownscale32 (pu8Dest,
                              u32DstWidth, u32DstHeight,
                              pu8Bitmap, u32Width  * 4,
                              u32Width, u32Height);

        bmpWrite ("scaled.bmp", pu8Dest, u32DstWidth, u32DstHeight);

        BitmapDownscale32Avg2(pu8Dest,
                              u32DstWidth, u32DstHeight,
                              pu8Bitmap,
                              u32Width * 4,
                              u32Width, u32Height);

        bmpWrite ("scaled_l.bmp", pu8Dest, u32DstWidth, u32DstHeight);

        BmpScaleInit(&s, u32DstWidth, u32DstHeight, u32Width, u32Height,
                     BMPSCALE_AREAMAP);

        s.pfnScale(s.pCtx, pu8Dest, pu8Bitmap, u32Width * 4);

        s.pfnDestroy(s.pCtx);

        bmpWrite ("scaled_lg.bmp", pu8Dest, u32DstWidth, u32DstHeight);

        BitmapDownscale32MapOpt(pu8Dest,
                                u32DstWidth, u32DstHeight,
                                pu8Bitmap,
                                u32Width * 4,
                                u32Width, u32Height);

        bmpWrite ("scaled_mo.bmp", pu8Dest, u32DstWidth, u32DstHeight);

        RTMemFree(pu8Dest);
    }

    bmpFree(pu8Bitmap);
}

int main (void)
{
    RTR3InitExeNoArguments(0);

    int rc = RTThreadSetAffinityToCpu(0);
    if (RT_FAILURE(rc))
    {
        RTPrintf ("Set affinity error %Rrc\n", rc);
    }
    rc = RTThreadSetType(RTThreadSelf(), RTTHREADTYPE_TIMER);
    if (RT_FAILURE(rc))
    {
        RTPrintf ("Set type error %Rrc\n", rc);
    }

    RTPrintf ("JPEG Test.\n");

//    bmpTest ("jpegtest.bmp");
//    bmpTest ("jpegtest632x344.bmp");
//    bmpTest ("jpegtest2.bmp");
//    bmpTest ("jpegtest4.bmp");
//    scaleTest ("blue.bmp");
    rawTest ("jpegtest4.bmp");
//    scaleTest ("jpegtest2.bmp");
//    scaleTest ("bmplarge.bmp");
//    blackCompressTest ("jpegtest.bmp");

    return 0;
}
