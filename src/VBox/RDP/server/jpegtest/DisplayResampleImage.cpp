/* $Id: DisplayResampleImage.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * Image resampling code, used for snapshot thumbnails.
 */

/*
 * Copyright (C) 2009-2025 Oracle and/or its affiliates.
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

/*
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

/*
 *
 * @todo Simplify: Offsets of images are 0,0 => no dstX, dstY, srcX, srcY;
 *             Screenshot has no alpha channel => no processing of alpha byte.
 */

#include <iprt/types.h>

/* 2.0.10: cast instead of floor() yields 35% performance improvement.
    Thanks to John Buckman. */

#define floor2(exp) ((long) exp)
/*#define floor2(exp) floor(exp)*/

typedef uint8_t *gdImagePtr;

DECLINLINE(int) gdImageGetTrueColorPixel (gdImagePtr im, int x, int y, int w)
{
    return *(int32_t *)(im + y * w * 4 + x * 4);
}

DECLINLINE(void) gdImageSetPixel (gdImagePtr im, int x, int y, int color, int w)
{
    *(int32_t *)(im + y * w * 4 + x * 4) = color;
}

#define gdAlphaMax 127
#define gdAlphaOpaque 0
#define gdAlphaTransparent 127
#define gdRedMax 255
#define gdGreenMax 255
#define gdBlueMax 255
#define gdTrueColorGetAlpha(c) (((c) & 0x7F000000) >> 24)
#define gdTrueColorGetRed(c) (((c) & 0xFF0000) >> 16)
#define gdTrueColorGetGreen(c) (((c) & 0x00FF00) >> 8)
#define gdTrueColorGetBlue(c) ((c) & 0x0000FF)
#define gdTrueColorAlpha(r, g, b, a) (((a) << 24) + \
    ((r) << 16) + \
    ((g) << 8) + \
    (b))

void gdImageCopyResampled (uint8_t *dst,
              uint8_t *src,
              int dstX, int dstY,
              int srcX, int srcY,
              int dstW, int dstH, int srcW, int srcH)
{
    int x, y;
    double sy1, sy2, sx1, sx2;
    for (y = dstY; (y < dstY + dstH); y++)
    {
        sy1 = ((double) y - (double) dstY) * (double) srcH / (double) dstH;
        sy2 = ((double) (y + 1) - (double) dstY) * (double) srcH / (double) dstH;

        for (x = dstX; (x < dstX + dstW); x++)
        {
            double sx, sy;
            double spixels = 0;
            double red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
            sx1 = ((double) x - (double) dstX) * (double) srcW / dstW;
            sx2 = ((double) (x + 1) - (double) dstX) * (double) srcW / dstW;

            sy = sy1;

            do
            {
                double yportion;
                if (floor2 (sy) == floor2 (sy1))
                  {
                    yportion = 1.0 - (sy - floor2 (sy));
                    if (yportion > sy2 - sy1)
                      {
                        yportion = sy2 - sy1;
                      }
                    sy = floor2 (sy);
                  }
                else if (sy == floor2 (sy2))
                  {
                    yportion = sy2 - floor2 (sy2);
                  }
                else
                  {
                    yportion = 1.0;
                  }
                sx = sx1;
                do
                  {
                    double xportion;
                    double pcontribution;
                    int p;
                    if (floor2 (sx) == floor2 (sx1))
                      {
                        xportion = 1.0 - (sx - floor2 (sx));
                        if (xportion > sx2 - sx1)
                          {
                            xportion = sx2 - sx1;
                          }
                        sx = floor2 (sx);
                      }
                    else if (sx == floor2 (sx2))
                      {
                        xportion = sx2 - floor2 (sx2);
                      }
                    else
                      {
                        xportion = 1.0;
                      }
                    pcontribution = xportion * yportion;
                    /* 2.08: previously srcX and srcY were ignored.
                       Andrew Pattison */
                    p = gdImageGetTrueColorPixel (src,
                                                  (int) sx + srcX,
                                                  (int) sy + srcY, srcW);
                    red += gdTrueColorGetRed (p) * pcontribution;
                    green += gdTrueColorGetGreen (p) * pcontribution;
                    blue += gdTrueColorGetBlue (p) * pcontribution;
                    alpha += gdTrueColorGetAlpha (p) * pcontribution;
                    spixels += xportion * yportion;
                    sx += 1.0;
                  }
                while (sx < sx2);
                sy += 1.0;
              }
            while (sy < sy2);
            if (spixels != 0.0)
              {
                red /= spixels;
                green /= spixels;
                blue /= spixels;
                alpha /= spixels;
              }
            /* Clamping to allow for rounding errors above */
            if (red > 255.0)
              {
                red = 255.0;
              }
            if (green > 255.0)
              {
                green = 255.0;
              }
            if (blue > 255.0)
              {
                blue = 255.0;
              }
            if (alpha > gdAlphaMax)
              {
                alpha = gdAlphaMax;
              }
            gdImageSetPixel (dst,
                             x, y,
                             gdTrueColorAlpha ((int) red,
                                               (int) green,
                                               (int) blue, (int) alpha), dstW);
          }
      }
}

#if 0
/* Fast interger implementation for 32 bpp bitmap downscaling in VRDP server.
 * Use scaled values * 16.
 */
typedef int FNGETTRUECOLORPIXEL(void *pvCtx, int x, int y);
typedef FNGETTRUECOLORPIXEL *PFNGETTRUECOLORPIXEL;

#include <iprt/stream.h>

typedef int32_t IDVAL;
#define INT_TO_VAL(i) (IDVAL)((i) << 4)
#define VAL_TO_INT(v) (int)((v) >> 4)
#define ID_FLOOR(v) ((v) & ~0xF)
#define ID_FRACTION(v) ((v) & 0xF)



void ImageDownsample (uint8_t *dst, // 8
                      PFNGETTRUECOLORPIXEL pfnGetTrueColorPixel, // c
              void *pvCtx, // 10
              int srcX, int srcY, // 14, 18
              int dstW, int dstH, // 1c, 20
              int srcW, int srcH) // 24, 2c
{
    int x, y;

    for (y = 0; y < dstH; y++)
    {
        IDVAL sy1 = INT_TO_VAL(y * srcH) / dstH;
        IDVAL sy2 = INT_TO_VAL((y + 1) * srcH) / dstH;

        for (x = 0; x < dstW; x++)
        {
            IDVAL spixels = 0;
            IDVAL red = 0, green = 0, blue = 0;

            IDVAL sx1 = INT_TO_VAL(x * srcW) / dstW;
            IDVAL sx2 = INT_TO_VAL((x + 1) * srcW) / dstW;

            IDVAL sy = sy1;

            do
            {
                IDVAL yportion;
                if (ID_FLOOR (sy) == ID_FLOOR (sy1))
                {
                    yportion = INT_TO_VAL(1) - ID_FRACTION(sy);
                    if (yportion > sy2 - sy1)
                    {
                        yportion = sy2 - sy1;
                    }
                    sy = ID_FLOOR (sy);
                }
                else if (sy == ID_FLOOR (sy2))
                {
                    yportion = ID_FRACTION(sy2);
                }
                else
                {
                    yportion = INT_TO_VAL(1);
                }
                IDVAL sx = sx1;
                do
                {
                    IDVAL xportion;
                    IDVAL pcontribution;
                    int p;
                    if (ID_FLOOR (sx) == ID_FLOOR (sx1))
                    {
                        xportion = INT_TO_VAL(1) - ID_FRACTION(sx);
                        if (xportion > sx2 - sx1)
                        {
                            xportion = sx2 - sx1;
                        }
                        sx = ID_FLOOR (sx);
                    }
                    else if (sx == ID_FLOOR (sx2))
                    {
                        xportion = ID_FRACTION(sx2);
                    }
                    else
                    {
                        xportion = INT_TO_VAL(1);
                    }
                    pcontribution = xportion * yportion;
                    p = pfnGetTrueColorPixel (pvCtx,
                                              VAL_TO_INT(sx) + srcX,
                                              VAL_TO_INT(sy) + srcY);
                    red += gdTrueColorGetRed (p) * pcontribution;
                    green += gdTrueColorGetGreen (p) * pcontribution;
                    blue += gdTrueColorGetBlue (p) * pcontribution;
                    spixels += xportion * yportion;

                    sx += INT_TO_VAL(1);
                } while (sx < sx2);

                sy += INT_TO_VAL(1);
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
#endif
