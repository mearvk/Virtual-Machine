/* $Id: jpegbmp.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include <iprt/formats/bmp.h>

void bmpWrite (const char *pszBMP, const uint8_t *pu8Bits, int w, int h)
{
    int cbBitmap = w * h * 4;

    FILE *f = fopen (pszBMP, "wb");

    if (f)
    {
        BMPFILEHDR fileHdr;
        RT_ZERO(fileHdr);
        fileHdr.uType      = BMP_HDR_MAGIC;
        fileHdr.cbFileSize = sizeof (BMPFILEHDR) + sizeof (BMPWIN3XINFOHDR) + cbBitmap;
        fileHdr.offBits    = sizeof (BMPFILEHDR) + sizeof (BMPWIN3XINFOHDR);

        BMPWIN3XINFOHDR coreHdr;
        RT_ZERO(coreHdr);
        coreHdr.cbSize      = sizeof (coreHdr);
        coreHdr.uWidth      = w;
        coreHdr.uHeight     = h;
        coreHdr.cPlanes     = 1;
        coreHdr.cBits       = 32;
        coreHdr.cbSizeImage = cbBitmap;

        fwrite (&fileHdr, 1, sizeof (fileHdr), f);
        fwrite (&coreHdr, 1, sizeof (coreHdr), f);
        fwrite (pu8Bits, 1, cbBitmap, f);

        fclose (f);
    }
}

uint8_t *bmpRead (const char *pszBMP, uint32_t *pu32Width, uint32_t *pu32Height, uint32_t *pu32BitsPerPixel)
{
    uint8_t *pu8Bitmap = NULL;

    FILE *f = fopen (pszBMP, "rb");

    if (f)
    {
        BMPFILEHDR fileHdr;

        fread (&fileHdr, 1, sizeof (fileHdr), f);

//        fileHdr.Type      = BMP_HDR_MAGIC;
//        fileHdr.FileSize  = sizeof(BMPFILEHDR) + sizeof(BMPWIN3XCOREHDR) + pBitsHdr->cb;
//        fileHdr.Reserved1 = 0;
//        fileHdr.Reserved2 = 0;
//        fileHdr.Offset    = sizeof(BMPFILEHDR) + sizeof(BMPWIN3XCOREHDR);

        printf("BITMAPFILEHEADER: type '%c%c', size %d, offset %d\n",
               (fileHdr.uType >> 8) & 0xFF, fileHdr.uType & 0xFF, fileHdr.cbFileSize, fileHdr.offBits);

        BMPWIN3XINFOHDR coreHdr;

//        coreHdr.cbSize         = sizeof(bi);
//        coreHdr.uWidth         = pBitsHdr->cWidth;
//        coreHdr.uHeight        = -pBitsHdr->cHeight;
//        coreHdr.cPlanes        = 1;
//        coreHdr.cBits          = pBitsHdr->cbPixel * 8;
//        coreHdr.enmCompression = 0;
//        coreHdr.cbSizeImage    = pBitsHdr->cb;
//        coreHdr.uXPelsPerMeter = 0;
//        coreHdr.uYPelsPerMeter = 0;
//        coreHdr.cClrUsed       = 0;
//        coreHdr.cClrImportant  = 0;

        fread (&coreHdr, 1, sizeof (coreHdr), f);

        printf("BITMAPINFOHEADER: size %d, %dx%d planes %d, bits %d, image size %d, ppm %dx%d, clr %d/%d\n",
               coreHdr.cbSize, coreHdr.uWidth, coreHdr.uHeight, coreHdr.cPlanes, coreHdr.cBits, coreHdr.cbSizeImage,
               coreHdr.uXPelsPerMeter, coreHdr.uYPelsPerMeter, coreHdr.cClrUsed, coreHdr.cClrImportant);

        if (coreHdr.cPlanes == 1 && (coreHdr.cBits == 24 || coreHdr.cBits == 32))
        {
            uint32_t u32SizeImage = coreHdr.uWidth * coreHdr.uHeight * 4;

            pu8Bitmap = (uint8_t *)RTMemAlloc(u32SizeImage);

            if (pu8Bitmap)
            {
                fseek (f, fileHdr.offBits, SEEK_SET);
                if (coreHdr.cBits == 32)
                {
                    fread (pu8Bitmap, 1, u32SizeImage, f);
                }
                else
                {
                    size_t i;
                    uint8_t *pu8 = pu8Bitmap;
                    for (i = 0; i < coreHdr.uWidth * coreHdr.uHeight; i++)
                    {
                        fread (pu8, 1, 3, f);
                        pu8 += 3;
                        *pu8++ = 0;
                    }
                }

                *pu32Width = coreHdr.uWidth;
                *pu32Height = coreHdr.uHeight;
                *pu32BitsPerPixel = 32;
            }
        }

        fclose (f);
    }

    return pu8Bitmap;
}

void bmpFree(uint8_t *pu8Bitmap)
{
    RTMemFree(pu8Bitmap);
}

void jpgWrite (const char *pszName, uint8_t *pu8JPEG, uint32_t cbJPEG)
{
    FILE *f = fopen (pszName, "wb");

    if (f)
    {
        fwrite (pu8JPEG, 1, cbJPEG, f);

        fclose (f);
    }
}
