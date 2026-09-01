/* $Id: bmpscale2.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2011-2025 Oracle and/or its affiliates.
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

#include <iprt/types.h>

#include "bmpscale.h"
#include "utils.h"
#include "vhstat.h"

/*
 * Subsampling with averaging.
 */

typedef int32_t FIXEDPOINT8;
#define INT_TO_FIXEDPOINT8(i) (FIXEDPOINT8)((i) << 8)
#define FIXEDPOINT8_TO_INT(v) (int)((v) >> 8)

#define PIXEL_GET_R(c) (((c) >> 16) & 0xFF)
#define PIXEL_GET_G(c) (((c) >> 8) & 0xFF)
#define PIXEL_GET_B(c) ((c) & 0xFF)

#define PIXEL_FROM_RGB(r, g, b) (((r) << 16) | ((g) << 8) | (b))

/* A destination pixel value is calculated as the average value of corresponding
 * source pixels in a square area. The size of the area should be equal
 * to the srcSize/dstSize, the nearest integer is used in the implementation.
 * For example if the source image is about 2 times larger than the destination,
 * then the area size must be 2x2.
 *
 * The nearest integer upper left corner position of the destination pixel in the
 * source image is calculated. After that the average value of source pixels in a square,
 * which is right and down from the upper left corner, is calculated.
 */

int BitmapDownscale32Avg2(uint8_t *pu8Dst,
                          uint32_t u32DstW, uint32_t u32DstH,
                          const uint8_t *pu8Src,
                          int32_t i32SrcDeltaLine,
                          uint32_t u32SrcW, uint32_t u32SrcH)
{
    const uint32_t u32AvgSize = 2; /** @todo BitmapDownscale32Sub3 */

    uint32_t x, y;

    /* To compute the average value of source pixels the sum must be divided by the number of pixels.
     * DIV is replaced by MUL 1/cPixels;
     */
    const FIXEDPOINT8 fpPixelsReciprocal = INT_TO_FIXEDPOINT8(1) / (u32AvgSize * u32AvgSize);

    FIXEDPOINT8 fpScaleW = INT_TO_FIXEDPOINT8(u32SrcW) / u32DstW;
    FIXEDPOINT8 fpScaleH = INT_TO_FIXEDPOINT8(u32SrcH) / u32DstH;

    /* Precompute upper left positions of dst pixels in src coords.
     * So pau32SrcY[yDst] == vertical coord of dst pixel in the src bitmap.
     */
    uint32_t *pau32SrcY = (uint32_t *)VRDPMemAlloc(u32DstH * sizeof(uint32_t));
    if (pau32SrcY == NULL)
    {
        return VERR_NO_MEMORY;
    }

    uint32_t *pau32SrcX = (uint32_t *)VRDPMemAlloc(u32DstW * sizeof(uint32_t));
    if (pau32SrcX == NULL)
    {
        VRDPMemFree(pau32SrcY);
        return VERR_NO_MEMORY;
    }

    for (y = 0; y < u32DstH; y++)
    {
        uint32_t u32 = FIXEDPOINT8_TO_INT(fpScaleH * y);
        pau32SrcY[y] = RT_MIN(u32, u32SrcH - u32AvgSize); /* Keep space for averaging. */
    }

    for (x = 0; x < u32DstW; x++)
    {
        uint32_t u32 = FIXEDPOINT8_TO_INT(fpScaleW * x);
        pau32SrcX[x] = RT_MIN(u32, u32SrcW - u32AvgSize); /* Keep space for averaging. */
    }

    uint32_t *pu32DstLine = (uint32_t *)pu8Dst; /* Pointer to 32bpp pixel line. */

    for (y = 0; y < u32DstH; y++, pu32DstLine += u32DstW)
    {
        uint32_t *pu32SrcLine = (uint32_t *)(pu8Src + pau32SrcY[y] * i32SrcDeltaLine);

        for (x = 0; x < u32DstW; x++)
        {
            /* Average pixels in the u32AvgSize*u32AvgSize area. */
            AssertCompile(u32AvgSize == 2);

            /* Two pixels in the current line. */
            uint32_t *pu32Pixel = pu32SrcLine + pau32SrcX[x];
            uint32_t u32Pixel = pu32Pixel[0];
            uint32_t u32R = PIXEL_GET_R(u32Pixel);
            uint32_t u32G = PIXEL_GET_G(u32Pixel);
            uint32_t u32B = PIXEL_GET_B(u32Pixel);

            u32Pixel = pu32Pixel[1];
            u32R += PIXEL_GET_R(u32Pixel);
            u32G += PIXEL_GET_G(u32Pixel);
            u32B += PIXEL_GET_B(u32Pixel);

            /* Next line. */
            pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);

            u32Pixel = pu32Pixel[0];
            u32R += PIXEL_GET_R(u32Pixel);
            u32G += PIXEL_GET_G(u32Pixel);
            u32B += PIXEL_GET_B(u32Pixel);

            u32Pixel = pu32Pixel[1];
            u32R += PIXEL_GET_R(u32Pixel);
            u32G += PIXEL_GET_G(u32Pixel);
            u32B += PIXEL_GET_B(u32Pixel);

            u32R = FIXEDPOINT8_TO_INT(u32R * fpPixelsReciprocal);
            u32G = FIXEDPOINT8_TO_INT(u32G * fpPixelsReciprocal);
            u32B = FIXEDPOINT8_TO_INT(u32B * fpPixelsReciprocal);

            *(pu32DstLine + x) = PIXEL_FROM_RGB(u32R, u32G, u32B);
        }
    }

    VRDPMemFree(pau32SrcY);
    VRDPMemFree(pau32SrcX);

    return VINF_SUCCESS;
}


/*
 * Area mapping downscaling.
 */
typedef struct MAPCTX
{
    uint32_t u32DstW;
    uint32_t u32DstH;

    uint32_t u32SrcW;
    uint32_t u32SrcH;
} MAPCTX;

static DECLCALLBACK(int) mapScale(BMPSCALECTX *pCtx,
                                  uint8_t *pu8Dst,
                                  const uint8_t *pu8Src,
                                  int32_t i32SrcDeltaLine)
{
    MAPCTX *p = (MAPCTX *)pCtx;

    BitmapDownscale32(pu8Dst,
                      p->u32DstW, p->u32DstH,
                      pu8Src,
                      i32SrcDeltaLine,
                      p->u32SrcW, p->u32SrcH);

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) mapDestroy(BMPSCALECTX *pCtx)
{
    MAPCTX *p = (MAPCTX *)pCtx;

    if (p)
    {
        VRDPMemFree(p);
    }
}

static int mapInit(BMPSCALECTX **ppCtx,
                   uint32_t u32DstW, uint32_t u32DstH,
                   uint32_t u32SrcW, uint32_t u32SrcH)
{
    MAPCTX *p = (MAPCTX *)VRDPMemAllocZ(sizeof(MAPCTX));

    if (p == NULL)
    {
        return VERR_NO_MEMORY;
    }

    int rc = VINF_SUCCESS;

    p->u32DstW = u32DstW;
    p->u32DstH = u32DstH;
    p->u32SrcW = u32SrcW;
    p->u32SrcH = u32SrcH;

    if (RT_SUCCESS(rc))
    {
        *ppCtx = (BMPSCALECTX *)p;
    }
    else
    {
        mapDestroy((BMPSCALECTX *)p);
    }

    return rc;
}

/*
 * Optimized area mapping downscaling for 1 > dstW/srcW > 0.5 and 1 > dstH/srcH > 0.5.
 */

typedef uint32_t FIXEDPOINT;
#define INT_TO_FIXEDPOINT(i) (FIXEDPOINT)((i) << 4)
#define FIXEDPOINT_TO_INT(v) (int)((v) >> 4)
#define FIXEDPOINT_FLOOR(v) ((v) & ~0xF)
#define FIXEDPOINT_FRACTION(v) ((v) & 0xF)

#define CHECK_SRC() do { Assert((uint8_t *)pu32Pixel < pu8Src + i32SrcDeltaLine * p->u32SrcH); } while (0)

static DECLCALLBACK(int) mapOptScale(BMPSCALECTX *pCtx,
                                     uint8_t *pu8Dst,
                                     const uint8_t *pu8Src,
                                     int32_t i32SrcDeltaLine)
{
    MAPCTX *p = (MAPCTX *)pCtx;

    /*
     * Optimized for scale factors from < 1 to > 0.5. That is the source bitmap is
     * up two 2 times larger than the destination and the destination pixel
     * is built from maximum 2 or 3 source pixels.
     *
     * Because of that it is possible to unroll internal loops.
     */
    uint32_t *pu32DstPixel = (uint32_t *)pu8Dst;

    FIXEDPOINT fpAreaSquare;

    unsigned x, y;

    FIXEDPOINT fpSrcY2 = 0;

    for (y = 0; y < p->u32DstH; y++)
    {
        FIXEDPOINT fpSrcY1 = fpSrcY2;
        fpSrcY2 = INT_TO_FIXEDPOINT((y + 1) * p->u32SrcH) / p->u32DstH;

        FIXEDPOINT fpDY = fpSrcY2 - fpSrcY1;

        const uint8_t *pu8SrcLine = pu8Src + i32SrcDeltaLine * FIXEDPOINT_TO_INT(fpSrcY1);

        /* Check how the dst pixel is located relative to src. */
        if (FIXEDPOINT_FRACTION(fpSrcY2) == 0)
        {
            /* The dst pixel ends at the fpSrcY2 pixel: takes full fpSrcY2 and part of fpSrcY1. */
            FIXEDPOINT fpSrcX2 = 0;

            for (x = 0; x < p->u32DstW; x++)
            {
                FIXEDPOINT fpAreaR, fpAreaG, fpAreaB;

                FIXEDPOINT fpSrcX1 = fpSrcX2;
                fpSrcX2 = INT_TO_FIXEDPOINT((x + 1) * p->u32SrcW) / p->u32DstW;

                fpAreaSquare = (fpSrcX2 - fpSrcX1) * fpDY;

                /* All branches start with this pixel. */
                uint32_t *pu32Pixel = (uint32_t *)pu8SrcLine + FIXEDPOINT_TO_INT(fpSrcX1);
                CHECK_SRC();
                uint32_t u32Pixel = pu32Pixel[0];

                if (FIXEDPOINT_FRACTION(fpSrcX2) == 0)
                {
                    if (FIXEDPOINT_FRACTION(fpSrcX1) != 0)
                    {
                        /* The dst pixel ends at the fpSrcX2 pixel: takes part of fpSrcX1 and full fpSrcX2. */

                        /* First line: part of fpSrcY1. */
                        uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        u32Pixel = pu32Pixel[1];
                        CHECK_SRC();
                        u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                        fpAreaR = u32R;
                        fpAreaG = u32G;
                        fpAreaB = u32B;

                        if (FIXEDPOINT_FRACTION(fpSrcY1) != 0)
                        {
                            /* Second line: full fpSrcY2. */
                            pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                            CHECK_SRC();

                            u32Pixel = pu32Pixel[0];
                            u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                            u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                            u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                            u32Pixel = pu32Pixel[1];
                            u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                            u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                            u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                            /* Take y contribution of fpSrcY2 into account. */
                            u32R *= INT_TO_FIXEDPOINT(1);
                            u32G *= INT_TO_FIXEDPOINT(1);
                            u32B *= INT_TO_FIXEDPOINT(1);

                            fpAreaR += u32R;
                            fpAreaG += u32G;
                            fpAreaB += u32B;
                        }
                    }
                    else
                    {
                        /* The dst pixel ends at the fpSrcX2 pixel and start at fpSrcX1: takes full fpSrcX1. */

                        /* First line: part of fpSrcY1. */
                        uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                        fpAreaR = u32R;
                        fpAreaG = u32G;
                        fpAreaB = u32B;

                        if (FIXEDPOINT_FRACTION(fpSrcY1) != 0)
                        {
                            /* Second line: full fpSrcY2. */
                            pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                            CHECK_SRC();

                            u32Pixel = pu32Pixel[0];
                            u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                            u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                            u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                            /* Take y contribution of fpSrcY2 into account. */
                            u32R *= INT_TO_FIXEDPOINT(1);
                            u32G *= INT_TO_FIXEDPOINT(1);
                            u32B *= INT_TO_FIXEDPOINT(1);

                            fpAreaR += u32R;
                            fpAreaG += u32G;
                            fpAreaB += u32B;
                        }
                    }
                }
                else if (FIXEDPOINT_TO_INT(fpSrcX1) + 1 == FIXEDPOINT_TO_INT(fpSrcX2))
                {
                    /* The dst pixel takes part of fpSrcX1 and part of fpSrcX2. */

                    /* First line: part of fpSrcY1. */
                    uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                    fpAreaR = u32R;
                    fpAreaG = u32G;
                    fpAreaB = u32B;

                    if (FIXEDPOINT_FRACTION(fpSrcY1) != 0)
                    {
                        /* Second line: full fpSrcY2. */
                        pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                        CHECK_SRC();

                        u32Pixel = pu32Pixel[0];
                        u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        u32Pixel = pu32Pixel[1];
                        u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                        u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                        u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                        /* Take y contribution of fpSrcY2 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1);
                        u32G *= INT_TO_FIXEDPOINT(1);
                        u32B *= INT_TO_FIXEDPOINT(1);

                        fpAreaR += u32R;
                        fpAreaG += u32G;
                        fpAreaB += u32B;
                    }
                }
                else
                {
                    /* The dst pixel takes part of fpSrcX1, full fpSrcX1 + 1 and part of fpSrcX1 + 2 == fpSrcX2. */

                    /* First line: part of fpSrcY1. */
                    uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                    u32Pixel = pu32Pixel[2];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                    fpAreaR = u32R;
                    fpAreaG = u32G;
                    fpAreaB = u32B;

                    if (FIXEDPOINT_FRACTION(fpSrcY1) != 0)
                    {
                        /* Second line: full fpSrcY2. */
                        pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                        CHECK_SRC();

                        u32Pixel = pu32Pixel[0];
                        u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        u32Pixel = pu32Pixel[1];
                        u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                        u32Pixel = pu32Pixel[2];
                        u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                        u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                        u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                        /* Take y contribution of fpSrcY2 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1);
                        u32G *= INT_TO_FIXEDPOINT(1);
                        u32B *= INT_TO_FIXEDPOINT(1);

                        fpAreaR += u32R;
                        fpAreaG += u32G;
                        fpAreaB += u32B;
                    }
                }

                fpAreaR /= fpAreaSquare;
                fpAreaG /= fpAreaSquare;
                fpAreaB /= fpAreaSquare;

                *pu32DstPixel++ = PIXEL_FROM_RGB(fpAreaR, fpAreaG, fpAreaB);
            }
        }
        else if (FIXEDPOINT_TO_INT(fpSrcY1) + 1 == FIXEDPOINT_TO_INT(fpSrcY2))
        {
            /* The dst pixel takes part of fpSrcY1 and part of fpSrcY2. */
            FIXEDPOINT fpSrcX2 = 0;

            for (x = 0; x < p->u32DstW; x++)
            {
                FIXEDPOINT fpAreaR, fpAreaG, fpAreaB;

                FIXEDPOINT fpSrcX1 = fpSrcX2;
                fpSrcX2 = INT_TO_FIXEDPOINT((x + 1) * p->u32SrcW) / p->u32DstW;

                fpAreaSquare = (fpSrcX2 - fpSrcX1) * fpDY;

                /* All branches start with this pixel. */
                uint32_t *pu32Pixel = (uint32_t *)pu8SrcLine + FIXEDPOINT_TO_INT(fpSrcX1);
                CHECK_SRC();
                uint32_t u32Pixel = pu32Pixel[0];

                if (FIXEDPOINT_FRACTION(fpSrcX2) == 0)
                {
                    if (FIXEDPOINT_FRACTION(fpSrcX1) != 0)
                    {
                        /* The dst pixel ends at the fpSrcX2 pixel: takes part of fpSrcX1 and full fpSrcX2. */

                        /* First line: part of fpSrcY1. */
                        uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        u32Pixel = pu32Pixel[1];
                        u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                        fpAreaR = u32R;
                        fpAreaG = u32G;
                        fpAreaB = u32B;

                        /* Second line: part of fpSrcY2. */
                        pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                        CHECK_SRC();

                        u32Pixel = pu32Pixel[0];
                        u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        u32Pixel = pu32Pixel[1];
                        u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= FIXEDPOINT_FRACTION(fpSrcY2);
                        u32G *= FIXEDPOINT_FRACTION(fpSrcY2);
                        u32B *= FIXEDPOINT_FRACTION(fpSrcY2);

                        fpAreaR += u32R;
                        fpAreaG += u32G;
                        fpAreaB += u32B;
                    }
                    else
                    {
                        /* The dst pixel ends at the fpSrcX2 pixel and start at fpSrcX1: takes full fpSrcX1. */

                        /* First line: part of fpSrcY1. */
                        uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                        fpAreaR = u32R;
                        fpAreaG = u32G;
                        fpAreaB = u32B;

                        /* Second line: part of fpSrcY2. */
                        pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                        CHECK_SRC();

                        u32Pixel = pu32Pixel[0];
                        u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= FIXEDPOINT_FRACTION(fpSrcY2);
                        u32G *= FIXEDPOINT_FRACTION(fpSrcY2);
                        u32B *= FIXEDPOINT_FRACTION(fpSrcY2);

                        fpAreaR += u32R;
                        fpAreaG += u32G;
                        fpAreaB += u32B;
                    }
                }
                else if (FIXEDPOINT_TO_INT(fpSrcX1) + 1 == FIXEDPOINT_TO_INT(fpSrcX2))
                {
                    /* The dst pixel takes part of fpSrcX1 and part of fpSrcX2. */

                    /* First line: part of fpSrcY1. */
                    uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                    fpAreaR = u32R;
                    fpAreaG = u32G;
                    fpAreaB = u32B;

                    /* Second line: part of fpSrcY2. */
                    pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                    CHECK_SRC();

                    u32Pixel = pu32Pixel[0];
                    u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= FIXEDPOINT_FRACTION(fpSrcY2);
                    u32G *= FIXEDPOINT_FRACTION(fpSrcY2);
                    u32B *= FIXEDPOINT_FRACTION(fpSrcY2);

                    fpAreaR += u32R;
                    fpAreaG += u32G;
                    fpAreaB += u32B;
                }
                else
                {
                    /* The dst pixel takes part of fpSrcX1, full fpSrcX1 + 1 and part of fpSrcX1 + 2 == fpSrcX2. */

                    /* First line: part of fpSrcY1. */
                    uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                    u32Pixel = pu32Pixel[2];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                    fpAreaR = u32R;
                    fpAreaG = u32G;
                    fpAreaB = u32B;

                    /* Second line: part of fpSrcY2. */
                    pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                    CHECK_SRC();

                    u32Pixel = pu32Pixel[0];
                    u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                    u32Pixel = pu32Pixel[2];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= FIXEDPOINT_FRACTION(fpSrcY2);
                    u32G *= FIXEDPOINT_FRACTION(fpSrcY2);
                    u32B *= FIXEDPOINT_FRACTION(fpSrcY2);

                    fpAreaR += u32R;
                    fpAreaG += u32G;
                    fpAreaB += u32B;
                }

                fpAreaR /= fpAreaSquare;
                fpAreaG /= fpAreaSquare;
                fpAreaB /= fpAreaSquare;

                *pu32DstPixel++ = PIXEL_FROM_RGB(fpAreaR, fpAreaG, fpAreaB);
            }
        }
        else
        {
            /* The dst pixel takes part of fpSrcY1, full fpSrcY1 + 1 and part of sy + 2 == fpSrcY2. */
            FIXEDPOINT fpSrcX2 = 0;

            for (x = 0; x < p->u32DstW; x++)
            {
                FIXEDPOINT fpAreaR, fpAreaG, fpAreaB;

                FIXEDPOINT fpSrcX1 = fpSrcX2;
                fpSrcX2 = INT_TO_FIXEDPOINT((x + 1) * p->u32SrcW) / p->u32DstW;

                fpAreaSquare = (fpSrcX2 - fpSrcX1) * fpDY;

                /* All branches start with this pixel. */
                uint32_t *pu32Pixel = (uint32_t *)pu8SrcLine + FIXEDPOINT_TO_INT(fpSrcX1);
                CHECK_SRC();
                uint32_t u32Pixel = pu32Pixel[0];

                if (FIXEDPOINT_FRACTION(fpSrcX2) == 0)
                {
                    if (FIXEDPOINT_FRACTION(fpSrcX1) != 0)
                    {
                        /* The dst pixel ends at the fpSrcX2 pixel: takes part of fpSrcX1 and full fpSrcX2. */

                        /* First line: part of fpSrcY1. */
                        uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        u32Pixel = pu32Pixel[1];
                        u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                        fpAreaR = u32R;
                        fpAreaG = u32G;
                        fpAreaB = u32B;

                        /* Second line: full fpSrcY1 + 1. */
                        pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                        CHECK_SRC();

                        u32Pixel = pu32Pixel[0];
                        u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        u32Pixel = pu32Pixel[1];
                        u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                        /* Take y contribution of fpSrcY1 + 1 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1);
                        u32G *= INT_TO_FIXEDPOINT(1);
                        u32B *= INT_TO_FIXEDPOINT(1);

                        fpAreaR += u32R;
                        fpAreaG += u32G;
                        fpAreaB += u32B;

                        /* Third line: part of fpSrcY2. */
                        pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                        CHECK_SRC();

                        u32Pixel = pu32Pixel[0];
                        u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        u32Pixel = pu32Pixel[1];
                        u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                        u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= FIXEDPOINT_FRACTION(fpSrcY2);
                        u32G *= FIXEDPOINT_FRACTION(fpSrcY2);
                        u32B *= FIXEDPOINT_FRACTION(fpSrcY2);

                        fpAreaR += u32R;
                        fpAreaG += u32G;
                        fpAreaB += u32B;
                    }
                    else
                    {
                        /* The dst pixel ends at the fpSrcX2 pixel and start at fpSrcX1 pixel: takes full fpSrcX1. */

                        /* First line: part of fpSrcY1. */
                        uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                        u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                        fpAreaR = u32R;
                        fpAreaG = u32G;
                        fpAreaB = u32B;

                        /* Second line: full fpSrcY1 + 1. */
                        pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                        CHECK_SRC();

                        u32Pixel = pu32Pixel[0];
                        u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        /* Take y contribution of fpSrcY1 + 1 into account. */
                        u32R *= INT_TO_FIXEDPOINT(1);
                        u32G *= INT_TO_FIXEDPOINT(1);
                        u32B *= INT_TO_FIXEDPOINT(1);

                        fpAreaR += u32R;
                        fpAreaG += u32G;
                        fpAreaB += u32B;

                        /* Third line: part of fpSrcY2. */
                        pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                        CHECK_SRC();

                        u32Pixel = pu32Pixel[0];
                        u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                        u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                        /* Take y contribution of fpSrcY1 into account. */
                        u32R *= FIXEDPOINT_FRACTION(fpSrcY2);
                        u32G *= FIXEDPOINT_FRACTION(fpSrcY2);
                        u32B *= FIXEDPOINT_FRACTION(fpSrcY2);

                        fpAreaR += u32R;
                        fpAreaG += u32G;
                        fpAreaB += u32B;
                    }
                }
                else if (FIXEDPOINT_TO_INT(fpSrcX1) + 1 == FIXEDPOINT_TO_INT(fpSrcX2))
                {
                    /* The dst pixel takes part of fpSrcX1 and part of fpSrcX2. */

                    /* First line: part of fpSrcY1. */
                    uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                    fpAreaR = u32R;
                    fpAreaG = u32G;
                    fpAreaB = u32B;

                    /* Second line: full fpSrcY1 + 1. */
                    pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                    CHECK_SRC();

                    u32Pixel = pu32Pixel[0];
                    u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 + 1 into account. */
                    u32R *= INT_TO_FIXEDPOINT(1);
                    u32G *= INT_TO_FIXEDPOINT(1);
                    u32B *= INT_TO_FIXEDPOINT(1);

                    fpAreaR += u32R;
                    fpAreaG += u32G;
                    fpAreaB += u32B;

                    /* Third line: part of fpSrcY2. */
                    pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                    CHECK_SRC();

                    u32Pixel = pu32Pixel[0];
                    u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= FIXEDPOINT_FRACTION(fpSrcY2);
                    u32G *= FIXEDPOINT_FRACTION(fpSrcY2);
                    u32B *= FIXEDPOINT_FRACTION(fpSrcY2);

                    fpAreaR += u32R;
                    fpAreaG += u32G;
                    fpAreaB += u32B;
                }
                else
                {
                    /* The dst pixel takes part of fpSrcX1, full fpSrcX1 + 1 and part of fpSrcX1 + 2 == fpSrcX2. */

                    /* First line: part of fpSrcY1. */
                    uint32_t u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    uint32_t u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                    u32Pixel = pu32Pixel[2];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32G *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);
                    u32B *= INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcY1);

                    fpAreaR = u32R;
                    fpAreaG = u32G;
                    fpAreaB = u32B;

                    /* Second line: full fpSrcY1 + 1. */
                    pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                    CHECK_SRC();

                    u32Pixel = pu32Pixel[0];
                    u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                    u32Pixel = pu32Pixel[2];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 + 1 into account. */
                    u32R *= INT_TO_FIXEDPOINT(1);
                    u32G *= INT_TO_FIXEDPOINT(1);
                    u32B *= INT_TO_FIXEDPOINT(1);

                    fpAreaR += u32R;
                    fpAreaG += u32G;
                    fpAreaB += u32B;

                    /* Third line: part of fpSrcY2. */
                    pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);
                    CHECK_SRC();

                    u32Pixel = pu32Pixel[0];
                    u32R = PIXEL_GET_R(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32G = PIXEL_GET_G(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));
                    u32B = PIXEL_GET_B(u32Pixel) * (INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(fpSrcX1));

                    u32Pixel = pu32Pixel[1];
                    u32R += PIXEL_GET_R(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32G += PIXEL_GET_G(u32Pixel) * INT_TO_FIXEDPOINT(1);
                    u32B += PIXEL_GET_B(u32Pixel) * INT_TO_FIXEDPOINT(1);

                    u32Pixel = pu32Pixel[2];
                    u32R += PIXEL_GET_R(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32G += PIXEL_GET_G(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);
                    u32B += PIXEL_GET_B(u32Pixel) * FIXEDPOINT_FRACTION(fpSrcX2);

                    /* Take y contribution of fpSrcY1 into account. */
                    u32R *= FIXEDPOINT_FRACTION(fpSrcY2);
                    u32G *= FIXEDPOINT_FRACTION(fpSrcY2);
                    u32B *= FIXEDPOINT_FRACTION(fpSrcY2);

                    fpAreaR += u32R;
                    fpAreaG += u32G;
                    fpAreaB += u32B;
                }

                fpAreaR /= fpAreaSquare;
                fpAreaG /= fpAreaSquare;
                fpAreaB /= fpAreaSquare;

                *pu32DstPixel++ = PIXEL_FROM_RGB(fpAreaR, fpAreaG, fpAreaB);
            }
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) mapOptDestroy(BMPSCALECTX *pCtx)
{
    MAPCTX *p = (MAPCTX *)pCtx;

    if (p)
    {
        VRDPMemFree(p);
    }
}

static int mapOptInit(BMPSCALECTX **ppCtx,
                      uint32_t u32DstW, uint32_t u32DstH,
                      uint32_t u32SrcW, uint32_t u32SrcH)
{
    FIXEDPOINT8 fpScaleW = INT_TO_FIXEDPOINT8(u32SrcW) / u32DstW;
    FIXEDPOINT8 fpScaleH = INT_TO_FIXEDPOINT8(u32SrcH) / u32DstH;

    if (   fpScaleW >= INT_TO_FIXEDPOINT8(20) / 10
        || fpScaleW <= INT_TO_FIXEDPOINT8(10) / 10
        || fpScaleH >= INT_TO_FIXEDPOINT8(20) / 10
        || fpScaleH <= INT_TO_FIXEDPOINT8(10) / 10
       )
    {
        return VERR_INVALID_PARAMETER;
    }

    MAPCTX *p = (MAPCTX *)VRDPMemAllocZ(sizeof(MAPCTX));

    if (p == NULL)
    {
        return VERR_NO_MEMORY;
    }

    int rc = VINF_SUCCESS;

    p->u32DstW = u32DstW;
    p->u32DstH = u32DstH;
    p->u32SrcW = u32SrcW;
    p->u32SrcH = u32SrcH;

    if (RT_SUCCESS(rc))
    {
        *ppCtx = (BMPSCALECTX *)p;
    }
    else
    {
        mapDestroy((BMPSCALECTX *)p);
    }

    return rc;
}

/*
 * Subsampling downscaling.
 */
typedef struct AVGCTX
{
    uint32_t u32DstW;
    uint32_t u32DstH;

    uint32_t u32SrcW;
    uint32_t u32SrcH;

    uint32_t *pau32SrcY;
    uint32_t *pau32SrcX;
} AVGCTX;

static DECLCALLBACK(int) avg2Scale(BMPSCALECTX *pCtx,
                                   uint8_t *pu8Dst,
                                   const uint8_t *pu8Src,
                                   int32_t i32SrcDeltaLine)
{
    AVGCTX *p = (AVGCTX *)pCtx;

    const uint32_t u32AvgSize = 2; /** @todo BitmapDownscale32Sub3 */

    uint32_t x, y;

    /* To compute the average value of source pixels the sum must be divided by the number of pixels.
     * DIV is replaced by MUL 1/cPixels;
     */
    const FIXEDPOINT8 fpPixelsReciprocal = INT_TO_FIXEDPOINT8(1) / (u32AvgSize * u32AvgSize);

    uint32_t *pu32DstLine = (uint32_t *)pu8Dst; /* Pointer to 32bpp pixel line. */

    for (y = 0; y < p->u32DstH; y++, pu32DstLine += p->u32DstW)
    {
        uint32_t *pu32SrcLine = (uint32_t *)(pu8Src + p->pau32SrcY[y] * i32SrcDeltaLine);

        for (x = 0; x < p->u32DstW; x++)
        {
            /* Average pixels in the u32AvgSize*u32AvgSize area. */
            AssertCompile(u32AvgSize == 2);

            /* Two pixels in the current line. */
            uint32_t *pu32Pixel = pu32SrcLine + p->pau32SrcX[x];
            uint32_t u32Pixel = pu32Pixel[0];
            uint32_t u32R = PIXEL_GET_R(u32Pixel);
            uint32_t u32G = PIXEL_GET_G(u32Pixel);
            uint32_t u32B = PIXEL_GET_B(u32Pixel);

            u32Pixel = pu32Pixel[1];
            u32R += PIXEL_GET_R(u32Pixel);
            u32G += PIXEL_GET_G(u32Pixel);
            u32B += PIXEL_GET_B(u32Pixel);

            /* Next line. */
            pu32Pixel = (uint32_t *)((uint8_t *)pu32Pixel + i32SrcDeltaLine);

            u32Pixel = pu32Pixel[0];
            u32R += PIXEL_GET_R(u32Pixel);
            u32G += PIXEL_GET_G(u32Pixel);
            u32B += PIXEL_GET_B(u32Pixel);

            u32Pixel = pu32Pixel[1];
            u32R += PIXEL_GET_R(u32Pixel);
            u32G += PIXEL_GET_G(u32Pixel);
            u32B += PIXEL_GET_B(u32Pixel);

            u32R = FIXEDPOINT8_TO_INT(u32R * fpPixelsReciprocal);
            u32G = FIXEDPOINT8_TO_INT(u32G * fpPixelsReciprocal);
            u32B = FIXEDPOINT8_TO_INT(u32B * fpPixelsReciprocal);

            *(pu32DstLine + x) = PIXEL_FROM_RGB(u32R, u32G, u32B);
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) avg2Destroy(BMPSCALECTX *pCtx)
{
    AVGCTX *p = (AVGCTX *)pCtx;

    if (p)
    {
        VRDPMemFree(p->pau32SrcY);
        VRDPMemFree(p->pau32SrcX);
        VRDPMemFree(p);
    }
}

static int avg2Init(BMPSCALECTX **ppCtx,
                    uint32_t u32DstW, uint32_t u32DstH,
                    uint32_t u32SrcW, uint32_t u32SrcH)
{
    const uint32_t u32AvgSize = 2;

    AVGCTX *p = (AVGCTX *)VRDPMemAllocZ(sizeof(AVGCTX));

    if (p == NULL)
    {
        return VERR_NO_MEMORY;
    }

    int rc = VINF_SUCCESS;

    p->pau32SrcY = (uint32_t *)VRDPMemAlloc(u32DstH * sizeof(uint32_t));

    if (p->pau32SrcY != NULL)
    {
        p->pau32SrcX = (uint32_t *)VRDPMemAlloc(u32DstW * sizeof(uint32_t));

        if (p->pau32SrcX != NULL)
        {
            FIXEDPOINT8 fpScaleW = INT_TO_FIXEDPOINT8(u32SrcW) / u32DstW;
            FIXEDPOINT8 fpScaleH = INT_TO_FIXEDPOINT8(u32SrcH) / u32DstH;

            /* Precompute upper left positions of dst pixels in src coords.
             * So pau32SrcY[yDst] == vertical coord of dst pixel in the src bitmap.
             */
            uint32_t i;
            for (i = 0; i < u32DstH; i++)
            {
                uint32_t u32 = FIXEDPOINT8_TO_INT(fpScaleH * i);
                p->pau32SrcY[i] = RT_MIN(u32, u32SrcH - u32AvgSize); /* Keep space for averaging. */
            }

            for (i = 0; i < u32DstW; i++)
            {
                uint32_t u32 = FIXEDPOINT8_TO_INT(fpScaleW * i);
                p->pau32SrcX[i] = RT_MIN(u32, u32SrcW - u32AvgSize); /* Keep space for averaging. */
            }

            p->u32DstW = u32DstW;
            p->u32DstH = u32DstH;
            p->u32SrcW = u32SrcW;
            p->u32SrcH = u32SrcH;
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *ppCtx = (BMPSCALECTX *)p;
    }
    else
    {
        avg2Destroy((BMPSCALECTX *)p);
    }

    return rc;
}

/* Initialize a scaling method suitable for given src and dst. */
int BmpScaleInit(BMPSCALE *pScaler,
                 uint32_t u32DstW, uint32_t u32DstH,
                 uint32_t u32SrcW, uint32_t u32SrcH,
                 int iMethod)
{
    int rc = VINF_SUCCESS;

    if (iMethod == BMPSCALE_AUTO)
    {
        FIXEDPOINT8 fpScaleW = INT_TO_FIXEDPOINT8(u32SrcW) / u32DstW;
        FIXEDPOINT8 fpScaleH = INT_TO_FIXEDPOINT8(u32SrcH) / u32DstH;

        FIXEDPOINT8 fpScaleMax = RT_MAX(fpScaleW, fpScaleH);

        if (fpScaleMax >= INT_TO_FIXEDPOINT8(13) / 10)
        {
            /* If src is 1.3 times larger than dst. Use faster averaging method. */
            /** @todo BitmapDownscale32Sub3 for fpScaleMax > 2.3 */
            iMethod = BMPSCALE_SUBSAMPLE2;
        }
        else if (   fpScaleW < INT_TO_FIXEDPOINT8(20) / 10
                 && fpScaleW > INT_TO_FIXEDPOINT8(10) / 10
                 && fpScaleH < INT_TO_FIXEDPOINT8(20) / 10
                 && fpScaleH > INT_TO_FIXEDPOINT8(10) / 10
                )
        {
            iMethod = BMPSCALE_AREAMAPOPT;
        }
        else
        {
            iMethod = BMPSCALE_AREAMAP;
        }
    }

    if (iMethod == BMPSCALE_SUBSAMPLE2)
    {
        rc = avg2Init(&pScaler->pCtx, u32DstW, u32DstH, u32SrcW, u32SrcH);

        if (RT_SUCCESS(rc))
        {
            pScaler->pfnScale = avg2Scale;
            pScaler->pfnDestroy = avg2Destroy;
        }
    }
    else if (iMethod == BMPSCALE_AREAMAPOPT)
    {
        /* Use area mapping. */
        rc = mapOptInit(&pScaler->pCtx, u32DstW, u32DstH, u32SrcW, u32SrcH);

        if (RT_SUCCESS(rc))
        {
            pScaler->pfnScale = mapOptScale;
            pScaler->pfnDestroy = mapOptDestroy;
        }
    }
    else if (iMethod == BMPSCALE_AREAMAP)
    {
        /* Use area mapping. */
        rc = mapInit(&pScaler->pCtx, u32DstW, u32DstH, u32SrcW, u32SrcH);

        if (RT_SUCCESS(rc))
        {
            pScaler->pfnScale = mapScale;
            pScaler->pfnDestroy = mapDestroy;
        }
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        VHSTATLOG(("VHSTAT: downscale %d: %dx%d -> %dx%d\n", iMethod, u32SrcW, u32SrcH, u32DstW, u32DstH));
    }
    else
    {
        VHSTATLOG(("VHSTAT: downscale %d: %dx%d -> %dx%d failed %Rrc\n", iMethod, u32SrcW, u32SrcH, u32DstW, u32DstH, rc));
    }

    if (RT_FAILURE(rc))
    {
        iMethod = BMPSCALE_AREAMAP;

        /* Try to fall back to AREAMAP. */
        rc = mapInit(&pScaler->pCtx, u32DstW, u32DstH, u32SrcW, u32SrcH);

        if (RT_SUCCESS(rc))
        {
            pScaler->pfnScale = mapScale;
            pScaler->pfnDestroy = mapDestroy;

            VHSTATLOG(("VHSTAT: downscale fallback to AREAMAP.\n"));
        }
    }

    return rc;
}
