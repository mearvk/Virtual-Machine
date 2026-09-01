/* $Id: bmpscale.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2010-2025 Oracle and/or its affiliates.
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

#ifndef VRDP_INCLUDED_SRC_bmpscale_h
#define VRDP_INCLUDED_SRC_bmpscale_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/*
 * Generic bitmap scaling interface.
 */
typedef struct BMPSCALECTX BMPSCALECTX;

typedef struct BMPSCALE
{
    BMPSCALECTX *pCtx;
    DECLCALLBACKMEMBER(int, pfnScale,(BMPSCALECTX *pCtx,
                                      uint8_t *pu8Dst,
                                      const uint8_t *pu8Src,
                                      int32_t i32SrcDeltaLine));
    DECLCALLBACKMEMBER(void, pfnDestroy,(BMPSCALECTX *pCtx));
} BMPSCALE;

#define BMPSCALE_AUTO       0
#define BMPSCALE_AREAMAP    1
#define BMPSCALE_SUBSAMPLE2 2
#define BMPSCALE_AREAMAPOPT 3

/* Initialize a scaling method suitable for given src and dst. */
int BmpScaleInit(BMPSCALE *pScaler,
                 uint32_t u32DstW, uint32_t u32DstH,
                 uint32_t u32SrcW, uint32_t u32SrcH,
                 int iMethod);

/*
 * Old interface.
 */

typedef uint32_t FNGETTRUECOLORPIXEL(const uint8_t *pu8SrcLine, int x);
typedef FNGETTRUECOLORPIXEL *PFNGETTRUECOLORPIXEL;

/* Area mapping, generic color depth. */
void BitmapDownscale (uint8_t *dst,
                      int dstW, int dstH,
                      PFNGETTRUECOLORPIXEL pfnGetTrueColorPixel,
                      const uint8_t *src,
                      int iDeltaLine,
                      int srcW, int srcH);

/* Area mapping, 32bpp color depth. */
void BitmapDownscale32 (uint8_t *dst,
                        int dstW, int dstH,
                        const uint8_t *src,
                        int iDeltaLine,
                        int srcW, int srcH);

/* Subsampling with averaging over 2x2 area, 32bpp color depth. */
int BitmapDownscale32Avg2 (uint8_t *pu8Dst,
                           uint32_t u32DstW, uint32_t u32DstH,
                           const uint8_t *pu8Src,
                           int32_t i32SrcDeltaLine,
                           uint32_t u32SrcW, uint32_t u32SrcH);

#endif /* !VRDP_INCLUDED_SRC_bmpscale_h */
