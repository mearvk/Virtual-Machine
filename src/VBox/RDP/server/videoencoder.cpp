/* $Id: videoencoder.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VirtualBox - JPEG encoder with support for "Raw input" and BGR0.
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

#include "videoencoder.h"
#include "utils.h"

#include <stdio.h> /* jpeglib.h needs FILE */
#include <setjmp.h>

#define JPEG_INTERNAL_OPTIONS /* Reveal RGB_PIXELSIZE, etc */
#include <jpeglib.h>
#include <jerror.h>

#ifndef JCS_EXTENSIONS
/* VRDP uses libjpeg configured for BGR0 input data. */
AssertCompile(RGB_RED == 2);
AssertCompile(RGB_GREEN == 1);
AssertCompile(RGB_BLUE == 0);
AssertCompile(RGB_PIXELSIZE == 4);
#endif

AssertCompile(BITS_IN_JSAMPLE == 8);

struct ve_error_mgr
{
    struct jpeg_error_mgr pub; /* "public" fields */
    jmp_buf setjmp_buffer;     /* for return to the caller */
};

struct ve_destination_mgr
{
    struct jpeg_destination_mgr pub;
    uint8_t *pu8DestBuffer;
    size_t cbAllocated;
};

struct VEJPEG
{
    struct jpeg_compress_struct cinfo;
    struct ve_destination_mgr dest;
    ve_error_mgr jerr;
};

METHODDEF(void) ve_error_exit(j_common_ptr cinfo)
{
    struct ve_error_mgr *err = (struct ve_error_mgr *)cinfo->err;

    static int scLogCnt = 0;
    if (scLogCnt < 1000)
    {
         scLogCnt++;

         char achBuffer[JMSG_LENGTH_MAX + 1];
         if (cinfo->err->format_message)
         {
             cinfo->err->format_message(cinfo, achBuffer);
         }
         else
         {
             RTStrCopy(achBuffer, sizeof(achBuffer), "format_message is NULL");
         }

         VRDPLOGREL(("libjpeg failure: %d: [%s]\n", cinfo->err->msg_code, achBuffer));
    }

    /* Return control to the setjmp point */
    longjmp(err->setjmp_buffer, 1);
}

METHODDEF(void) ve_output_message(j_common_ptr cinfo)
{
    NOREF(cinfo); /* Do nothing. */
}

METHODDEF(void) ve_init_destination(j_compress_ptr cinfo)
{
    NOREF(cinfo); /* Do nothing. */
}

METHODDEF(boolean) ve_empty_output_buffer(j_compress_ptr cinfo)
{
    /* More space in the output buffer is required. */
    struct ve_destination_mgr *dest = (struct ve_destination_mgr *)cinfo->dest;

    /* Try to allocate new buffer with double size */
    size_t cbNewAllocated = dest->cbAllocated * 2;
    uint8_t *pu8NewDestBuffer = (uint8_t *)VRDPMemRealloc(dest->pu8DestBuffer, cbNewAllocated);

    if (pu8NewDestBuffer == NULL)
    {
        ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 1000);
    }

    size_t cbBuffer = dest->cbAllocated - dest->pub.free_in_buffer;
    dest->pub.next_output_byte = pu8NewDestBuffer + cbBuffer;
    dest->pub.free_in_buffer = cbNewAllocated - cbBuffer;

    dest->pu8DestBuffer = pu8NewDestBuffer;
    dest->cbAllocated = cbNewAllocated;

    return TRUE;
}

METHODDEF(void) ve_term_destination(j_compress_ptr cinfo)
{
    NOREF(cinfo); /* Do nothing. */
}


void VEJPEGDestroy(VEJPEG *pEncoder)
{
    if (pEncoder)
    {
        if (setjmp(pEncoder->jerr.setjmp_buffer))
        {
            /* Do nothing */ ;
        }
        else
        {
            jpeg_destroy_compress(&pEncoder->cinfo);
            VRDPMemFree(pEncoder);
        }
    }
}

int VEJPEGInit(VEJPEG **ppEncoder)
{
    VEJPEG *pEncoder = (VEJPEG *)VRDPMemAllocZ(sizeof(VEJPEG));
    if (!pEncoder)
    {
         return VERR_NO_MEMORY;
    }

    int rc;

    pEncoder->cinfo.err = jpeg_std_error(&pEncoder->jerr.pub);
    pEncoder->jerr.pub.error_exit = ve_error_exit;

    /* Prevent stderr output. */
    pEncoder->jerr.pub.output_message = ve_output_message;

    if (setjmp(pEncoder->jerr.setjmp_buffer))
    {
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        jpeg_create_compress(&pEncoder->cinfo);
        pEncoder->cinfo.dest = &pEncoder->dest.pub;
        pEncoder->dest.pub.init_destination    = ve_init_destination;
        pEncoder->dest.pub.empty_output_buffer = ve_empty_output_buffer;
        pEncoder->dest.pub.term_destination    = ve_term_destination;
        rc = VINF_SUCCESS;
    }

    if (RT_SUCCESS(rc))
    {
       *ppEncoder = pEncoder;
    }
    else
    {
        AssertPtr(pEncoder);
        VEJPEGDestroy(pEncoder);
    }

    return rc;
}

int VEJPEGEncodeFrameBGR0(VEJPEG *pEncoder,
                          int iQuality,
                          uint8_t **ppu8Encoded,
                          uint32_t *pcbEncoded,
                          const uint8_t *pu8Bitmap,
                          uint32_t cWidth,
                          uint32_t cHeight)
{
    /* The SIMD routines in jpeg library can not use quality > 95,
     * because of an integer overflow.
     */
    iQuality = RT_CLAMP(iQuality, 1, 95);

    int rc;

    uint32_t cbLine = cWidth * 4; /* 32bpp BRG0 bitmap. */

    pEncoder->dest.cbAllocated = cbLine * cHeight / 8; /* Expect compressed size 1/8 of original. */
    pEncoder->dest.pu8DestBuffer = (uint8_t *)VRDPMemAlloc(pEncoder->dest.cbAllocated);

    if (pEncoder->dest.pu8DestBuffer)
    {
        JSAMPROW *row_pointer = (JSAMPROW *)VRDPMemAlloc(sizeof(JSAMPROW) * cHeight);
        if (row_pointer)
        {
            pEncoder->dest.pub.next_output_byte = pEncoder->dest.pu8DestBuffer;
            pEncoder->dest.pub.free_in_buffer = pEncoder->dest.cbAllocated;

            pEncoder->cinfo.image_width      = cWidth;
            pEncoder->cinfo.image_height     = cHeight;
            pEncoder->cinfo.input_components = 4;
#ifdef JCS_EXTENSIONS
            pEncoder->cinfo.in_color_space   = JCS_EXT_BGRX;
#else
            pEncoder->cinfo.in_color_space   = JCS_RGB;
#endif
            pEncoder->cinfo.data_precision   = 8;

            if (setjmp(pEncoder->jerr.setjmp_buffer))
            {
                rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                jpeg_set_defaults(&pEncoder->cinfo);

                jpeg_set_quality(&pEncoder->cinfo, iQuality, TRUE /* limit to baseline-JPEG values */);

                pEncoder->cinfo.dct_method = JDCT_FASTEST;

                uint32_t i;
                for (i = 0; i < cHeight; i++)
                {
                    row_pointer[i] = (JSAMPROW)&pu8Bitmap[i * cbLine];
                }

                jpeg_start_compress(&pEncoder->cinfo, TRUE);

                while (pEncoder->cinfo.next_scanline < pEncoder->cinfo.image_height)
                {
                    jpeg_write_scanlines(&pEncoder->cinfo, &row_pointer[pEncoder->cinfo.next_scanline],
                                         pEncoder->cinfo.image_height - pEncoder->cinfo.next_scanline);
                }

                jpeg_finish_compress(&pEncoder->cinfo);

                *ppu8Encoded = pEncoder->dest.pu8DestBuffer;
                *pcbEncoded = (uint32_t)(pEncoder->dest.cbAllocated - pEncoder->dest.pub.free_in_buffer);
                rc = VINF_SUCCESS;
            }

            VRDPMemFree(row_pointer);
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }

        if (RT_FAILURE(rc))
        {
            VRDPMemFree(pEncoder->dest.pu8DestBuffer);
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    pEncoder->dest.cbAllocated = 0;
    pEncoder->dest.pu8DestBuffer = NULL;

    return rc;
}

typedef struct VEJPEGCOMPONENT
{
    const uint8_t *pu8Samples;
    uint32_t cSamplesPerRow;
    uint32_t cRows;

    uint32_t cRequiredRows;
} VEJPEGCOMPONENT;

/*
 * @param pu8YUVRaw Raw data. 1 byte per sample 8 pixel aligned Y, followed by downsampled
 *                  with scale factor 2 U and V.
 *                  If cHeight is not a multiple of 2, then the last scanline of U and V is not
 *                  downsampled vertically.
 * @param cbYUVRaw  Size of raw data.
 * @param cWidth    Width of the original image (not including alignment).
 * @param cHeight   Height of the original image.
 */
int VEJPEGEncodeFrameYUVRaw(VEJPEG *pEncoder,
                            int volatile iQuality, /* gcc 10 w/ asan claims potential long-jmp clobbering issue w/o volatile */
                            uint8_t **ppu8Encoded,
                            uint32_t *pcbEncoded,
                            const uint8_t *pu8YUVRaw,
                            uint32_t cbYUVRaw,
                            uint32_t cWidth,
                            uint32_t cHeight)
{
    AssertReturn(cWidth > 0 && cWidth < JPEG_MAX_DIMENSION, VERR_INVALID_PARAMETER);
    AssertReturn(cHeight > 0 && cHeight < JPEG_MAX_DIMENSION, VERR_INVALID_PARAMETER);

    /* The SIMD routines in jpeg library can not use quality > 95,
     * because of an integer overflow.
     */
    iQuality = RT_CLAMP(iQuality, 1, 95);

    /* Initialize the components description. */
    VEJPEGCOMPONENT aComponents[3];

    { /* This scoping was an attempt at shutting up -Wclobbered on cWidthAligned with gcc 4.4.7. Didn't work, so volatile var.  */
#ifdef __GNUC__
        volatile
#endif
        const uint32_t cWidthAligned = (cWidth + 7) & ~0x7;
        const uint32_t cScanlinesY = cHeight;
        const uint32_t cScanlinesUV = (cScanlinesY + 1) / 2; /* Include the last scanline if height is not a multiple of 2. */
        const uint32_t cbYUV = cWidthAligned * cScanlinesY + cWidthAligned /* /2 * 2 */ * cScanlinesUV;

        AssertMsgReturn(cbYUVRaw >= cbYUV,
                        ("cbYUVRaw %d, cbYUV %d\n", cbYUVRaw, cbYUV),
                        VERR_INVALID_PARAMETER);

        /* Y */
        aComponents[0].pu8Samples     = pu8YUVRaw;
        aComponents[0].cSamplesPerRow = cWidthAligned;
        aComponents[0].cRows          = cScanlinesY;
        aComponents[0].cRequiredRows  = 0;

        /* U */
        aComponents[1].pu8Samples     = aComponents[0].pu8Samples +
                                        aComponents[0].cSamplesPerRow * aComponents[0].cRows;
        aComponents[1].cSamplesPerRow = cWidthAligned / 2;
        aComponents[1].cRows          = cScanlinesUV;
        aComponents[1].cRequiredRows  = 0;

        /* V */
        aComponents[2].pu8Samples     = aComponents[1].pu8Samples +
                                        aComponents[1].cSamplesPerRow * aComponents[1].cRows;
        aComponents[2].cSamplesPerRow = cWidthAligned / 2;
        aComponents[2].cRows          = cScanlinesUV;
        aComponents[2].cRequiredRows  = 0;

        pEncoder->dest.cbAllocated = cbYUV / 8; /* Expect compressed size 1/8 of original. */
    }
    pEncoder->dest.pu8DestBuffer = (uint8_t *)VRDPMemAlloc(pEncoder->dest.cbAllocated);

    int rc;
    if (RT_LIKELY(pEncoder->dest.pu8DestBuffer))
    {
        /* Initialize the compressor */
        if (setjmp(pEncoder->jerr.setjmp_buffer))
        {
            rc = VERR_NOT_SUPPORTED;
        }
        else
        {
            pEncoder->dest.pub.next_output_byte = pEncoder->dest.pu8DestBuffer;
            pEncoder->dest.pub.free_in_buffer = pEncoder->dest.cbAllocated;

            jpeg_compress_struct *pcinfo = &pEncoder->cinfo;

            /* Initialize parameters for jpeg_set_defaults. */
            pcinfo->image_width      = (JDIMENSION)cWidth;
            pcinfo->image_height     = (JDIMENSION)cHeight;
            pcinfo->input_components = 3;
            pcinfo->in_color_space   = JCS_YCbCr;
            pcinfo->data_precision   = 8;

            jpeg_set_defaults(pcinfo);

            jpeg_set_quality(pcinfo, iQuality, TRUE /* limit to baseline-JPEG values */);

            pcinfo->dct_method = JDCT_FASTEST;

            /* Adjust parameters for "raw data" input. */
            pcinfo->raw_data_in = TRUE;
#if JPEG_LIB_VERSION >= 70
            pcinfo->do_fancy_downsampling = FALSE;
#endif
            pcinfo->comp_info[0].h_samp_factor = 2;
            pcinfo->comp_info[0].v_samp_factor = 2;
            pcinfo->comp_info[1].h_samp_factor = 1;
            pcinfo->comp_info[1].v_samp_factor = 1;
            pcinfo->comp_info[2].h_samp_factor = 1;
            pcinfo->comp_info[2].v_samp_factor = 1;

            jpeg_start_compress(pcinfo, TRUE);

            /* Now cinfo is completely initialized.
             * Make sure that it is ready for raw compression.
             */
            rc = VINF_SUCCESS;
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(aComponents); i++)
            {
                VEJPEGCOMPONENT *p = &aComponents[i];

                uint32_t cRequiredSamplesPerRow = pcinfo->comp_info[i].width_in_blocks * DCTSIZE;

                /* Input data must have enough samples in a scanline. */
                if (cRequiredSamplesPerRow > p->cSamplesPerRow)
                {
                    AssertFailed();
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                p->cRequiredRows = pcinfo->comp_info[i].height_in_blocks * DCTSIZE;

                /* Y needs MCU alignment. Other components only need alignment to DCTSIZE,
                 * because they are downsampled by 2.
                 */
                if (i == 0)
                {
                    /* Round up to be a multiple of libjpeg MCU = max_samp * DCTSIZE = 2 * DCTSIZE. */
                    p->cRequiredRows = RT_ALIGN_32(p->cRequiredRows, 2 * DCTSIZE);
                }
            }

            Assert(   aComponents[0].cRequiredRows == 2 * aComponents[1].cRequiredRows
                   && aComponents[1].cRequiredRows == aComponents[2].cRequiredRows);

            if (RT_SUCCESS(rc))
            {
                /* Prepare JSAMPIMAGE struct for every 2 * DCTSIZE Y lines,
                 * because jpeg_write_raw_data processes as many lines at a time.
                 */
                JSAMPROW Y[2 * DCTSIZE];
                JSAMPROW U[DCTSIZE];
                JSAMPROW V[DCTSIZE];

                JSAMPARRAY data[3] = { Y, U, V }; /* Input parameter for jpeg_write_raw_data. */

                uint32_t iLoop = 0;

                /* Scanlines are counted in not downsampled component. */
                while (pcinfo->next_scanline < pcinfo->image_height)
                {
                    JDIMENSION cScanlines;

                    /* Assign row pointers for each component 'data'. */
                    uint32_t iComp;
                    for (iComp = 0; iComp < RT_ELEMENTS(aComponents); iComp++)
                    {
                        JSAMPROW *paComponentRows = data[iComp];
                        VEJPEGCOMPONENT *p = &aComponents[iComp];

                        uint32_t cRows;
                        if (iComp == 0)
                        {
                            cRows = 2 * DCTSIZE;
                            cScanlines = pcinfo->next_scanline;
                        }
                        else
                        {
                            cRows = DCTSIZE;
                            cScanlines = pcinfo->next_scanline / 2;
                        }

                        uint32_t cRowsLeft = p->cRows - cScanlines;
                        JSAMPROW pFirstRow = (JSAMPROW)&p->pu8Samples[cScanlines * p->cSamplesPerRow];

                        uint32_t iRow;
                        for (iRow = 0; iRow < cRows; iRow++)
                        {
                            paComponentRows[iRow] = iRow < cRowsLeft?
                                                        &pFirstRow[iRow * p->cSamplesPerRow]:
                                                        paComponentRows[iRow - 1];
                        }
                    }

                    cScanlines = jpeg_write_raw_data(pcinfo, data, 2 * DCTSIZE);

                    if (++iLoop > aComponents[0].cRequiredRows)
                    {
                        /* Avoid infinite loop. It can't be more than number of rows,
                         * because every call must process 2*DCTSIZE rows.
                         */
                        rc = VERR_INTERRUPTED;
                        break;
                    }
                }

                jpeg_finish_compress(pcinfo);

                *ppu8Encoded = pEncoder->dest.pu8DestBuffer;
                *pcbEncoded = (uint32_t)(pEncoder->dest.cbAllocated - pEncoder->dest.pub.free_in_buffer);
            }
        }

        if (RT_FAILURE(rc))
        {
            VRDPMemFree(pEncoder->dest.pu8DestBuffer);
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    pEncoder->dest.cbAllocated = 0;
    pEncoder->dest.pu8DestBuffer = NULL;

    return rc;
}
