/* $Id: jpegraw.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

static void measureRGB(VEJPEG *pEncoder, int iJPEGQuality,
                       uint8_t *pu8Bitmap, uint32_t u32Width, uint32_t u32Height,
                       int iIterations)
{
    int rc;

    uint8_t *pu8JPEG;
    uint32_t cbJPEG = 0;

    /* Test RGB input data. */

    /* Compress twice to get to the stable running conditions. */
    int r;
    for (r = 0; r < 2; r++)
    {
        rc = VEJPEGEncodeFrameBGR0(pEncoder, iJPEGQuality,
                                   &pu8JPEG,
                                   &cbJPEG,
                                   pu8Bitmap,
                                   u32Width, u32Height);

        if (RT_FAILURE (rc))
        {
            RTPrintf("VEJPEGEncodeFrameBGR0 %Rrc\n", rc);
            break;
        }

        RTMemFree(pu8JPEG);
    }

    /* Measure time. */
    uint64_t u64TimeStart = RTTimeNanoTS();

    for (r = 0; r < iIterations; r++)
    {
        rc = VEJPEGEncodeFrameBGR0(pEncoder, iJPEGQuality,
                                   &pu8JPEG,
                                   &cbJPEG,
                                   pu8Bitmap,
                                   u32Width, u32Height);

        if (RT_FAILURE (rc))
        {
            RTPrintf("VEJPEGEncodeFrameBGR0 %Rrc\n", rc);
            break;
        }

        RTMemFree(pu8JPEG);
    }

    uint64_t u64AverageTime = (RTTimeNanoTS() - u64TimeStart) / iIterations / 1000; /* nano -> micro */

    RTPrintf("RGB %3d %d bytes %llu mcs\n", iJPEGQuality, cbJPEG, u64AverageTime);

    /* Save resulting image. */
    rc = VEJPEGEncodeFrameBGR0(pEncoder, iJPEGQuality,
                               &pu8JPEG,
                               &cbJPEG,
                               pu8Bitmap,
                               u32Width, u32Height);
    if (RT_FAILURE (rc))
    {
        RTPrintf("VEJPEGEncodeFrameBGR0 %Rrc\n", rc);
    }
    else
    {
        jpgWrite ("testResult.jpg", pu8JPEG, cbJPEG);

        RTMemFree(pu8JPEG);
    }
}

static void measureRaw(VEJPEG *pEncoder, int iJPEGQuality,
                       uint8_t *pu8YUVRaw, uint32_t cbYUVRaw,
                       uint32_t u32Width, uint32_t u32Height,
                       int iIterations)
{
    int rc;

    uint8_t *pu8JPEG;
    uint32_t cbJPEG = 0;

    /* Test Raw input data. */

    /* Compress twice to get to the stable running conditions. */
    int r;
    for (r = 0; r < 2; r++)
    {
        rc = VEJPEGEncodeFrameYUVRaw(pEncoder, iJPEGQuality,
                                     &pu8JPEG, &cbJPEG,
                                     pu8YUVRaw, cbYUVRaw,
                                     u32Width, u32Height);

        if (RT_FAILURE (rc))
        {
            RTPrintf("VEJPEGEncodeFrameYUVRaw %Rrc\n", rc);
            break;
        }

        RTMemFree(pu8JPEG);
    }

    /* Measure time. */
    uint64_t u64TimeStart = RTTimeNanoTS();

    for (r = 0; r < iIterations; r++)
    {
        rc = VEJPEGEncodeFrameYUVRaw(pEncoder, iJPEGQuality,
                                     &pu8JPEG, &cbJPEG,
                                     pu8YUVRaw, cbYUVRaw,
                                     u32Width, u32Height);

        if (RT_FAILURE (rc))
        {
            RTPrintf("VEJPEGEncodeFrameYUVRaw %Rrc\n", rc);
            break;
        }

        RTMemFree(pu8JPEG);
    }

    uint64_t u64AverageTime = (RTTimeNanoTS() - u64TimeStart) / iIterations / 1000; /* nano -> micro */

    RTPrintf("RAW %3d %d bytes %llu mcs\n", iJPEGQuality, cbJPEG, u64AverageTime);

    /* Save resulting image. */
    rc = VEJPEGEncodeFrameYUVRaw(pEncoder, iJPEGQuality,
                                 &pu8JPEG, &cbJPEG,
                                 pu8YUVRaw, cbYUVRaw,
                                 u32Width, u32Height);

    if (RT_FAILURE (rc))
    {
        RTPrintf("VEJPEGEncodeFrameYUVRaw %Rrc\n", rc);
    }
    else
    {
        jpgWrite ("testResultRaw.jpg", pu8JPEG, cbJPEG);

        RTMemFree(pu8JPEG);
    }
}

static uint8_t *rawCreate(uint32_t *pcbYUVRaw, const uint8_t *pu8Bitmap,
                          uint32_t u32Width, uint32_t u32Height)
{
    /* Convert color space to YUV. */
    uint32_t cPixels = u32Width * u32Height;
    uint8_t *pu8YUV = (uint8_t *)RTMemAlloc(cPixels * 3);

    const uint8_t *pu8Src = pu8Bitmap;
    uint8_t *pu8DstY = pu8YUV;
    uint8_t *pu8DstU = pu8DstY + cPixels;
    uint8_t *pu8DstV = pu8DstU + cPixels;

    uint32_t i;
    for (i = 0; i < cPixels; i++)
    {
        uint8_t R = pu8Src[2];
        uint8_t G = pu8Src[1];
        uint8_t B = pu8Src[0];

        uint8_t Y = (uint8_t)(0.29900 * R + 0.58700 * G + 0.11400 * B);
        uint8_t U = (uint8_t)(-0.16874 * R - 0.33126 * G + 0.50000 * B + 128);
        uint8_t V = (uint8_t)(0.50000 * R - 0.41869 * G - 0.08131 * B + 128);

        *pu8DstY++ = Y;
        *pu8DstU++ = U;
        *pu8DstV++ = V;
        pu8Src += 4;
    }

    /* Downsample and expand scanlines. */
    uint32_t cWidthAligned = RT_ALIGN_32(u32Width, 8);
    uint32_t cScanlinesY = u32Height;
    uint32_t cScanlinesUV = (cScanlinesY + 1) / 2; /* Include the last scanline if height is not a multiple of 2. */
    uint32_t cbYUVRaw = cWidthAligned * cScanlinesY + cWidthAligned /* /2 * 2 */ * cScanlinesUV;

    RTPrintf("cWidthAligned %d, cScanlinesY %d, cScanlinesUV %d\n",
             cWidthAligned, cScanlinesY, cScanlinesUV);

    uint8_t *pu8YUVRaw = (uint8_t *)RTMemAllocZ(cbYUVRaw);

    uint8_t *pu8SrcY = pu8YUV;
    uint8_t *pu8SrcU = pu8SrcY + cPixels;
    uint8_t *pu8SrcV = pu8SrcU + cPixels;

    pu8DstY = pu8YUVRaw;
    pu8DstU = pu8DstY + cPixels;
    pu8DstV = pu8DstU + cPixels / 4;

    uint32_t y;
    uint32_t x;

    /* Y */
    for (y = 0; y < cScanlinesY; y++)
    {
        for (x = 0; x < cWidthAligned; x++)
        {
            *pu8DstY = x < u32Width? *pu8SrcY++: pu8DstY[-1];
            pu8DstY++;
        }
    }

    /* U */
    for (y = 0; y < cScanlinesY; y += 2)
    {
        uint8_t *pu8SrcULine0 = pu8SrcU;
        uint8_t *pu8SrcULine1 = y == cScanlinesY - 1?
                                    pu8SrcULine0:
                                    &pu8SrcU[u32Width];

        for (x = 0; x < cWidthAligned; x += 2)
        {
            if (x < u32Width)
            {
                uint32_t U00 = *pu8SrcULine0++;
                uint32_t U01 = *pu8SrcULine1++;
                uint32_t U10 = *pu8SrcULine0++;
                uint32_t U11 = *pu8SrcULine1++;

                *pu8DstU = (uint8_t)((U00 + U01 + U10 + U11) / 4);
            }
            else
            {
                *pu8DstU = pu8DstU[-1];
            }

            pu8DstU++;
        }

        pu8SrcU += 2 * u32Width;
    }

    /* V */
    for (y = 0; y < cScanlinesY; y += 2)
    {
        uint8_t *pu8SrcVLine0 = pu8SrcV;
        uint8_t *pu8SrcVLine1 = y == cScanlinesY - 1?
                                    pu8SrcVLine0:
                                    &pu8SrcV[u32Width];

        for (x = 0; x < cWidthAligned; x += 2)
        {
            if (x < u32Width)
            {
                uint32_t V00 = *pu8SrcVLine0++;
                uint32_t V01 = *pu8SrcVLine1++;
                uint32_t V10 = *pu8SrcVLine0++;
                uint32_t V11 = *pu8SrcVLine1++;

                *pu8DstV = (uint8_t)((V00 + V01 + V10 + V11) / 4);
            }
            else
            {
                *pu8DstV = pu8DstV[-1];
            }

            pu8DstV++;
        }

        pu8SrcV += 2 * u32Width;
    }

    RTMemFree(pu8YUV);

    *pcbYUVRaw = cbYUVRaw;
    return pu8YUVRaw;
}

#if 0
With zero U, V
    BITMAPINFOHEADER: size 40, 1680x1093 planes 1, bits 24, image size 0, ppm 3780x3780, clr 0/0
    cWidthAligned 1680, cScanlinesY 1093, cScanlinesUV 547
    RAW  70 85070 bytes 12370 mcs
    RGB  70 88533 bytes 16380 mcs

    Without the set_color_space call.
    RAW  70 85070 bytes 12309 mcs
    RGB  70 88533 bytes 16271 mcs

All components
    RAW  70 89056 bytes 12620 mcs
    RGB  70 88533 bytes 16302 mcs
#endif

void rawTest(const char *pszBMP)
{
    uint32_t u32Width = 0;
    uint32_t u32Height = 0;
    uint32_t u32BitsPerPixel = 0;

    uint8_t *pu8Bitmap = bmpRead(pszBMP, &u32Width, &u32Height, &u32BitsPerPixel);

    RTPrintf("%dx%d\n", u32Width, u32Height);

    VEJPEG *pEncoder;
    int rc = VEJPEGInit(&pEncoder);
    RTPrintf("VEJPEGInit %Rrc\n", rc);
    if (RT_SUCCESS(rc))
    {
        uint8_t *pu8YUVRaw;
        uint32_t cbYUVRaw;

        pu8YUVRaw = rawCreate(&cbYUVRaw, pu8Bitmap, u32Width, u32Height);

        int iJPEGQuality;
        for (iJPEGQuality = 70; iJPEGQuality <= 70; iJPEGQuality += 10)
        {
            const int iIterations = 1000;

            if (pu8YUVRaw)
            {
                measureRaw(pEncoder, iJPEGQuality, pu8YUVRaw, cbYUVRaw,
                           u32Width, u32Height, iIterations);
            }

            measureRGB(pEncoder, iJPEGQuality, pu8Bitmap, u32Width, u32Height, iIterations);
        }

        RTMemFree(pu8YUVRaw);

        VEJPEGDestroy(pEncoder);
    }

    bmpFree(pu8Bitmap);
}
