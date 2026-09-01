/* $Id: bmpscale.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * Bitmap downscaling.
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
 *
 * ----------------------------------------------------------------------------
 *
 * Based on gdImageCopyResampled from libgd.
 * Original copyright notice follows:

     Portions copyright 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
     Pierre-Alain Joye (pierre@libgd.org).

     Permission has been granted to copy, distribute and modify gd in
     any context without fee, including a commercial application,
     provided that this notice is present in user-accessible supporting
     documentation.

     This does not affect your ownership of the derived work itself, and
     the intent is to assure proper credit for the authors of gd, not to
     interfere with your productive use of gd. If you have questions,
     ask. "Derived works" includes all programs that utilize the
     library. Credit must be given in user-accessible documentation.

     This software is provided "AS IS." The copyright holders disclaim
     all warranties, either express or implied, including but not
     limited to implied warranties of merchantability and fitness for a
     particular purpose, with respect to this code and accompanying
     documentation.
 */

#include <iprt/types.h>

#include "bmpscale.h"

/* This define disables code which is not needed for downscaling. */
#define VRDP_BMP_DOWNSCALE_ONLY

typedef uint8_t *gdImagePtr;

DECLINLINE(void) gdImageSetPixel (gdImagePtr im, int x, int y, int color, int w)
{
    *(int32_t *)(im + y * w * 4 + x * 4) = color;
}

#define gdTrueColorGetRed(c) (((c) & 0xFF0000) >> 16)
#define gdTrueColorGetGreen(c) (((c) & 0x00FF00) >> 8)
#define gdTrueColorGetBlue(c) ((c) & 0x0000FF)

/* Fast interger implementation for 32 bpp bitmap downscaling in VRDP server.
 * Use fixed point values * 16.
 */
typedef int32_t FIXEDPOINT;
#define INT_TO_FIXEDPOINT(i) (FIXEDPOINT)((i) << 4)
#define FIXEDPOINT_TO_INT(v) (int)((v) >> 4)
#define FIXEDPOINT_FLOOR(v) ((v) & ~0xF)
#define FIXEDPOINT_FRACTION(v) ((v) & 0xF)

/* The only difference between BitmapDownscale32 and BitmapDownscale is the
 * method to obtain source 32bpp RGB pixel.
 *
 * Differences are marked with "Color depth specific code" comments.
 */

/* Optimized for 32 bit source. */
void BitmapDownscale32 (uint8_t *dst,
                        int dstW, int dstH,
                        const uint8_t *src,
                        int iDeltaLine,
                        int srcW, int srcH)
{
    int x, y;

    for (y = 0; y < dstH; y++)
    {
        FIXEDPOINT sy1 = INT_TO_FIXEDPOINT(y * srcH) / dstH;
        FIXEDPOINT sy2 = INT_TO_FIXEDPOINT((y + 1) * srcH) / dstH;

        for (x = 0; x < dstW; x++)
        {
            FIXEDPOINT red = 0, green = 0, blue = 0;

            FIXEDPOINT sx1 = INT_TO_FIXEDPOINT(x * srcW) / dstW;
            FIXEDPOINT sx2 = INT_TO_FIXEDPOINT((x + 1) * srcW) / dstW;

            FIXEDPOINT spixels = (sx2 - sx1) * (sy2 - sy1);

            FIXEDPOINT sy = sy1;

            do
            {
                FIXEDPOINT yportion;
                if (FIXEDPOINT_FLOOR (sy) == FIXEDPOINT_FLOOR (sy1))
                {
                    yportion = INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(sy);
#ifndef VRDP_BMP_DOWNSCALE_ONLY
                    /* sy2 - sy1 always > 1.0 for downscaling. yportion always <= 1.0 */
                    if (yportion > sy2 - sy1)
                    {
                        yportion = sy2 - sy1;
                    }
#endif /* !VRDP_BMP_DOWNSCALE_ONLY */
                    sy = FIXEDPOINT_FLOOR (sy);
                }
                else if (sy == FIXEDPOINT_FLOOR (sy2))
                {
                    yportion = FIXEDPOINT_FRACTION(sy2);
                }
                else
                {
                    yportion = INT_TO_FIXEDPOINT(1);
                }

                const uint8_t *pu8SrcLine = src + iDeltaLine * FIXEDPOINT_TO_INT(sy);
                FIXEDPOINT sx = sx1;
                do
                {
                    FIXEDPOINT xportion;
                    FIXEDPOINT pcontribution;
                    int p;
                    if (FIXEDPOINT_FLOOR (sx) == FIXEDPOINT_FLOOR (sx1))
                    {
                        xportion = INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(sx);
#ifndef VRDP_BMP_DOWNSCALE_ONLY
                        if (xportion > sx2 - sx1)
                        {
                            xportion = sx2 - sx1;
                        }
#endif /* !VRDP_BMP_DOWNSCALE_ONLY */
                        pcontribution = xportion * yportion;
                        sx = FIXEDPOINT_FLOOR (sx);
                    }
                    else if (sx == FIXEDPOINT_FLOOR (sx2))
                    {
                        xportion = FIXEDPOINT_FRACTION(sx2);
                        pcontribution = xportion * yportion;
                    }
                    else
                    {
                        xportion = INT_TO_FIXEDPOINT(1);
                        pcontribution = xportion * yportion;
                    }
                    /* Color depth specific code begin */
                    p = *(uint32_t *)(pu8SrcLine + FIXEDPOINT_TO_INT(sx) * 4);
                    /* Color depth specific code end */
                    red += gdTrueColorGetRed (p) * pcontribution;
                    green += gdTrueColorGetGreen (p) * pcontribution;
                    blue += gdTrueColorGetBlue (p) * pcontribution;

                    sx += INT_TO_FIXEDPOINT(1);
                } while (sx < sx2);

                sy += INT_TO_FIXEDPOINT(1);
            } while (sy < sy2);

            if (spixels != 0)
            {
                red /= spixels;
                green /= spixels;
                blue /= spixels;
            }
            /* Clamping to allow for rounding errors above */
            if (red > 255)
            {
                red = 255;
            }
            if (green > 255)
            {
                green = 255;
            }
            if (blue > 255)
            {
                blue = 255;
            }
            gdImageSetPixel (dst,
                             x, y,
                             ( ((int) red) << 16) + (((int) green) << 8) + ((int) blue),
                             dstW);
        }
    }
}

/* Generic for any source depths. */
void BitmapDownscale (uint8_t *dst,
              int dstW, int dstH,
                      PFNGETTRUECOLORPIXEL pfnGetTrueColorPixel,
              const uint8_t *src,
                      int iDeltaLine,
              int srcW, int srcH)
{
    int x, y;

    for (y = 0; y < dstH; y++)
    {
        FIXEDPOINT sy1 = INT_TO_FIXEDPOINT(y * srcH) / dstH;
        FIXEDPOINT sy2 = INT_TO_FIXEDPOINT((y + 1) * srcH) / dstH;

        for (x = 0; x < dstW; x++)
        {
            FIXEDPOINT red = 0, green = 0, blue = 0;

            FIXEDPOINT sx1 = INT_TO_FIXEDPOINT(x * srcW) / dstW;
            FIXEDPOINT sx2 = INT_TO_FIXEDPOINT((x + 1) * srcW) / dstW;

            FIXEDPOINT spixels = (sx2 - sx1) * (sy2 - sy1);

            FIXEDPOINT sy = sy1;

            do
            {
                FIXEDPOINT yportion;
                if (FIXEDPOINT_FLOOR (sy) == FIXEDPOINT_FLOOR (sy1))
                {
                    yportion = INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(sy);
#ifndef VRDP_BMP_DOWNSCALE_ONLY
                    if (yportion > sy2 - sy1)
                    {
                        yportion = sy2 - sy1;
                    }
#endif /* !VRDP_BMP_DOWNSCALE_ONLY */
                    sy = FIXEDPOINT_FLOOR (sy);
                }
                else if (sy == FIXEDPOINT_FLOOR (sy2))
                {
                    yportion = FIXEDPOINT_FRACTION(sy2);
                }
                else
                {
                    yportion = INT_TO_FIXEDPOINT(1);
                }

                const uint8_t *pu8SrcLine = src + iDeltaLine * FIXEDPOINT_TO_INT(sy);
                FIXEDPOINT sx = sx1;
                do
                {
                    FIXEDPOINT xportion;
                    FIXEDPOINT pcontribution;
                    int p;
                    if (FIXEDPOINT_FLOOR (sx) == FIXEDPOINT_FLOOR (sx1))
                    {
                        xportion = INT_TO_FIXEDPOINT(1) - FIXEDPOINT_FRACTION(sx);
#ifndef VRDP_BMP_DOWNSCALE_ONLY
                        if (xportion > sx2 - sx1)
                        {
                            xportion = sx2 - sx1;
                        }
#endif /* !VRDP_BMP_DOWNSCALE_ONLY */
                        pcontribution = xportion * yportion;
                        sx = FIXEDPOINT_FLOOR (sx);
                    }
                    else if (sx == FIXEDPOINT_FLOOR (sx2))
                    {
                        xportion = FIXEDPOINT_FRACTION(sx2);
                        pcontribution = xportion * yportion;
                    }
                    else
                    {
                        xportion = INT_TO_FIXEDPOINT(1);
                        pcontribution = xportion * yportion;
                    }
                    /* Color depth specific code begin */
                    p = pfnGetTrueColorPixel (pu8SrcLine, FIXEDPOINT_TO_INT(sx));
                    /* Color depth specific code end */
                    red += gdTrueColorGetRed (p) * pcontribution;
                    green += gdTrueColorGetGreen (p) * pcontribution;
                    blue += gdTrueColorGetBlue (p) * pcontribution;

                    sx += INT_TO_FIXEDPOINT(1);
                } while (sx < sx2);

                sy += INT_TO_FIXEDPOINT(1);
            } while (sy < sy2);

            if (spixels != 0)
            {
                red /= spixels;
                green /= spixels;
                blue /= spixels;
            }
            /* Clamping to allow for rounding errors above */
            if (red > 255)
            {
                red = 255;
            }
            if (green > 255)
            {
                green = 255;
            }
            if (blue > 255)
            {
                blue = 255;
            }
            gdImageSetPixel (dst,
                             x, y,
                             ( ((int) red) << 16) + (((int) green) << 8) + ((int) blue),
                             dstW);
        }
    }
}
