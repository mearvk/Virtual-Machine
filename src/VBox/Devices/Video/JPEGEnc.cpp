/** @file
 * VirtualBox - JPEG encoder helpers with support for "Raw input" and BGR0.
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

#include "JPEGEnc.h"

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <stdio.h> /* jpeglib.h needs FILE */
#include <setjmp.h>

#define JPEG_INTERNAL_OPTIONS /* Reveal RGB_PIXELSIZE, etc */
#include <jpeglib.h>
#include <jerror.h>

#ifndef JCS_EXTENSIONS
/* Use libjpeg configured for BGR0 input data. */
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

struct HWCJPEG
{
    struct jpeg_compress_struct cinfo;
    struct ve_destination_mgr dest;
    ve_error_mgr jerr;
};

METHODDEF(void) ve_error_exit(j_common_ptr cinfo)
{
    struct ve_error_mgr *err = (struct ve_error_mgr *)cinfo->err;

    static int s_cLogCnt = 0;
    if (s_cLogCnt < 1000)
    {
         s_cLogCnt++;

         char szBuffer[JMSG_LENGTH_MAX + 1];
         if (cinfo->err->format_message)
         {
             cinfo->err->format_message(cinfo, szBuffer);
         }
         else
         {
             RTStrPrintf(szBuffer, sizeof(szBuffer), "format_message is NULL");
         }

         LogRel(("libjpeg failure: %d: [%s]\n", cinfo->err->msg_code, szBuffer));
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
    uint8_t *pu8NewDestBuffer = (uint8_t *)RTMemRealloc(dest->pu8DestBuffer, cbNewAllocated);

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


void HWCJPEGDestroy(HWCJPEG *pEncoder)
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
            RTMemFree(pEncoder);
        }
    }
}

int HWCJPEGInit(HWCJPEG **ppEncoder)
{
    HWCJPEG *pEncoder = (HWCJPEG *)RTMemAllocZ(sizeof(HWCJPEG));
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
        HWCJPEGDestroy(pEncoder);
    }

    return rc;
}

int HWCJPEGEncodeFrameBGR0(HWCJPEG *pEncoder,
                          int iQuality,
                          uint8_t **ppu8Encoded,
                          uint32_t *pcbEncoded,
                          const uint8_t *pu8Src,
                          uint32_t cbSrc,
                          uint32_t cWidth,
                          uint32_t cHeight)
{
    /* The SIMD routines in jpeg library can not use quality > 95,
     * because of an integer overflow.
     */
    iQuality = RT_CLAMP(iQuality, 1, 95);


    const uint32_t cbLine = cWidth * 4; /* 32bpp BRG0 bitmap. */

    const uint32_t cbData = cHeight * cbLine;

    if (cbSrc < cbData)
    {
#ifdef DEBUG_sunlover
        AssertMsgFailed(("cbSrc %d, cbData %d\n", cbSrc, cbData));
#endif
        return VERR_INVALID_PARAMETER;
    }

    pEncoder->dest.cbAllocated = cbLine * cHeight / 8; /* Expect compressed size 1/8 of original. */
    pEncoder->dest.pu8DestBuffer = (uint8_t *)RTMemAlloc(pEncoder->dest.cbAllocated);

    int rc;
    if (pEncoder->dest.pu8DestBuffer)
    {
        JSAMPROW *row_pointer = (JSAMPROW *)RTMemAlloc(sizeof(JSAMPROW) * cHeight);
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
                    row_pointer[i] = (JSAMPROW)&pu8Src[i * cbLine];
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

                rc = VINF_SUCCESS; /* Attempt at shutting up -Wclobbered warning. */
            }

            RTMemFree(row_pointer);
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }

        if (RT_FAILURE(rc))
        {
            RTMemFree(pEncoder->dest.pu8DestBuffer);
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

typedef struct HWCJPEGCOMPONENT
{
    const uint8_t *pu8Samples;
    uint32_t cBytesPerRow;
    uint32_t cRows;

    uint32_t cRequiredRows;
} HWCJPEGCOMPONENT;

/*
 * @param pu8Src    Raw data. Cb, Y0, Cr, Y1. Or Y0, Cb, Y1, Cr. Unaligned.
 * @param cbSrc     Size of raw data.
 * @param cWidth    Width of the original image.
 * @param cHeight   Height of the original image.
 */
int HWCJPEGEncodeFrameRaw_2vuy_yuvs(HWCJPEG *pEncoder,
                                    int volatile iQuality, /* gcc 10 w/ asan claims potential long-jmp clobbering issue w/o volatile */
                                    uint8_t **ppu8Encoded,
                                    uint32_t *pcbEncoded,
                                    const uint8_t *pu8Src,
                                    uint32_t cbSrc,
                                    uint32_t cWidth,
                                    uint32_t cHeight,
                                    bool fYUVS)
{
    AssertReturn(cWidth > 0 && cWidth < JPEG_MAX_DIMENSION, VERR_INVALID_PARAMETER);
    AssertReturn(cHeight > 0 && cHeight < JPEG_MAX_DIMENSION, VERR_INVALID_PARAMETER);

    /* The SIMD routines in jpeg library can not use quality > 95,
     * because of an integer overflow.
     */
    iQuality = RT_CLAMP(iQuality, 1, 95);

    int rc = VINF_SUCCESS;

    const uint32_t cbData = cWidth * cHeight + cWidth /* /2 * 2 */ * cHeight;

    if (cbSrc < cbData)
    {
#ifdef DEBUG_sunlover
        AssertMsgFailed(("cbSrc %d, cbData %d\n", cbSrc, cbData));
#endif
        return VERR_INVALID_PARAMETER;
    }

    /* Aligned by MCU. */
    uint32_t cWidthAligned = RT_ALIGN_32(cWidth, 2 * DCTSIZE);

    /* Initialize the components description.
     * All components are interleaved, so use one structure to describe them all.
     */
    HWCJPEGCOMPONENT component;
    component.pu8Samples    = pu8Src;
    component.cBytesPerRow  = cWidth * 4 / 2;
    component.cRows         = cHeight;
    component.cRequiredRows = 0;

    /* Allocate temporary buffer for one MCU row (height=DCTSIZE, width = cWidthAligned).
     * Y followed by Cb, followed by Cr.
     * size is DCTSIZE * (alignW + alignW/2 + alignW/2).
     */
    uint32_t cbMCU = DCTSIZE * 2 * cWidthAligned;
    uint8_t *pu8MCU = (uint8_t *)RTMemAllocZ(cbMCU);
    if (!pu8MCU)
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        pEncoder->dest.cbAllocated = cbData / 8; /* Expect compressed size 1/8 of original. */
        pEncoder->dest.pu8DestBuffer = (uint8_t *)RTMemAlloc(pEncoder->dest.cbAllocated);
        if (!pEncoder->dest.pu8DestBuffer)
        {
            rc = VERR_NO_MEMORY;
        }
    }

    if (RT_SUCCESS(rc))
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
            pcinfo->image_width       = (JDIMENSION)cWidth;
            pcinfo->image_height      = (JDIMENSION)cHeight;
            pcinfo->input_components  = 3;
            pcinfo->in_color_space    = JCS_YCbCr;
            pcinfo->data_precision    = 8;

            jpeg_set_defaults(pcinfo);

            jpeg_set_quality(pcinfo, iQuality, TRUE /* limit to baseline-JPEG values */);

            pcinfo->dct_method = JDCT_FASTEST;

            /* Adjust parameters for "raw data" input. */
            pcinfo->raw_data_in = TRUE;
#if JPEG_LIB_VERSION >= 70
            pcinfo->do_fancy_downsampling = FALSE;
#endif
            pcinfo->comp_info[0].h_samp_factor = 2;
            pcinfo->comp_info[0].v_samp_factor = 1;
            pcinfo->comp_info[1].h_samp_factor = 1;
            pcinfo->comp_info[1].v_samp_factor = 1;
            pcinfo->comp_info[2].h_samp_factor = 1;
            pcinfo->comp_info[2].v_samp_factor = 1;

            jpeg_start_compress(pcinfo, TRUE);

            /* Now cinfo is completely initialized.
             * Make sure that it is ready for raw compression.
             * Since there is no vertical subsampling, the height is the same for all components.
             */
            component.cRequiredRows = pcinfo->comp_info[0].height_in_blocks * DCTSIZE;

            Assert(   pcinfo->comp_info[0].height_in_blocks == pcinfo->comp_info[1].height_in_blocks
                   && pcinfo->comp_info[1].height_in_blocks == pcinfo->comp_info[2].height_in_blocks);

            /* Prepare JSAMPIMAGE struct for every DCTSIZE lines,
             * because jpeg_write_raw_data processes as many lines (one MCU) at a time.
             */
            JSAMPROW Y[DCTSIZE];
            JSAMPROW U[DCTSIZE];
            JSAMPROW V[DCTSIZE];

            JSAMPARRAY data[3] = { Y, U, V }; /* Input parameter for jpeg_write_raw_data. */

            uint8_t *pu8Y = pu8MCU;
            uint8_t *pu8U = pu8Y + DCTSIZE * cWidthAligned;
            uint8_t *pu8V = pu8U + DCTSIZE * cWidthAligned / 2;

            /* Init row pointers. */
            unsigned i;
            for (i = 0; i < DCTSIZE; i++)
            {
                Y[i] = pu8Y + i * cWidthAligned;
                U[i] = pu8U + i * cWidthAligned / 2;
                V[i] = pu8V + i * cWidthAligned / 2;
            }

            uint32_t iLoop = 0;

            /* Scanlines are counted in not downsampled component. */
            bool fOkay = true; /* -Wclobbered workaround */
            while (pcinfo->next_scanline < pcinfo->image_height)
            {
                /* Copy source data to the temporary buffer. */
                JDIMENSION cScanlines = pcinfo->next_scanline;
                uint32_t cRowsLeft = component.cRows - cScanlines;

                const JSAMPLE *pSample = &component.pu8Samples[cScanlines * component.cBytesPerRow];

                uint32_t iRow;
                for (iRow = 0; iRow < DCTSIZE; iRow++)
                {
                    uint8_t *pu8DstY = data[0][iRow];
                    uint8_t *pu8DstU = data[1][iRow];
                    uint8_t *pu8DstV = data[2][iRow];

                    if (iRow < cRowsLeft)
                    {
                        uint32_t iSample; /* Sample is 2 pixels in 4 bytes. */
                        if (fYUVS)
                        {
                            for (iSample = 0; iSample < cWidth / 2; iSample++)
                            {
                                *pu8DstY++ = pSample[0];
                                *pu8DstY++ = pSample[2];
                                *pu8DstU++ = pSample[1];
                                *pu8DstV++ = pSample[3];
                                pSample += 4;
                            }
                        }
                        else
                        {
                            for (iSample = 0; iSample < cWidth / 2; iSample++)
                            {
                                *pu8DstY++ = pSample[1];
                                *pu8DstY++ = pSample[3];
                                *pu8DstU++ = pSample[0];
                                *pu8DstV++ = pSample[2];
                                pSample += 4;
                            }
                        }
                    }
                    else
                    {
                        memset(pu8DstY, 0, cWidthAligned);
                        memset(pu8DstU, 0, cWidthAligned / 2);
                        memset(pu8DstV, 0, cWidthAligned / 2);
                    }
                }

                jpeg_write_raw_data(pcinfo, data, DCTSIZE);

                if (++iLoop > component.cRequiredRows)
                {
                    /* Avoid infinite loop. It can't be more than number of rows,
                     * because every call must process DCTSIZE rows.
                     */
                    fOkay = false;
                    break;
                }
            }

            jpeg_finish_compress(pcinfo);

            *ppu8Encoded = pEncoder->dest.pu8DestBuffer;
            *pcbEncoded = (uint32_t)(pEncoder->dest.cbAllocated - pEncoder->dest.pub.free_in_buffer);

            rc = fOkay ? VINF_SUCCESS : VERR_INTERRUPTED; /* Attempt at shutting up -Wclobbered warning. */
        }
    }

    RTMemFree(pu8MCU);

    if (RT_FAILURE(rc))
    {
        RTMemFree(pEncoder->dest.pu8DestBuffer);
    }

    pEncoder->dest.cbAllocated = 0;
    pEncoder->dest.pu8DestBuffer = NULL;

    return rc;
}

/*
 * @param pu8Src    1 byte per sample Y, followed by downsampled with scale factor 2 U and V.
 *                  If cHeight is not a multiple of 2, then the last scanline of U and V is not
 *                  downsampled vertically.
 * @param cbSrc     Size of data.
 * @param cWidth    Width of the image.
 * @param cHeight   Height of the image, must be multiple of 2.
 */
int HWCJPEGEncodeFrameRaw_I420(HWCJPEG *pEncoder,
                               int volatile iQuality, /* gcc 10 w/ asan claims potential long-jmp clobbering issue w/o volatile */
                               uint8_t **ppu8Encoded,
                               uint32_t *pcbEncoded,
                               const uint8_t *pu8Src,
                               uint32_t cbSrc,
                               uint32_t cWidth,
                               uint32_t cHeight)
{
    AssertReturn(cWidth > 0 && cWidth < JPEG_MAX_DIMENSION, VERR_INVALID_PARAMETER);
    AssertReturn(cHeight > 0 && cHeight < JPEG_MAX_DIMENSION, VERR_INVALID_PARAMETER);
    AssertReturn((cHeight & 1) == 0, VERR_INVALID_PARAMETER);

    /* The SIMD routines in jpeg library can not use quality > 95,
     * because of an integer overflow.
     */
    iQuality = RT_CLAMP(iQuality, 1, 95);

    int rc = VINF_SUCCESS;

    const uint32_t cbData = cWidth * cHeight + cWidth /* /2 * 2 */ * cHeight / 2;

    if (cbSrc < cbData)
    {
#ifdef DEBUG_sunlover
        AssertMsgFailed(("cbSrc %d, cbData %d\n", cbSrc, cbData));
#endif
        return VERR_INVALID_PARAMETER;
    }

    /* Aligned by MCU. */
    const uint32_t cWidthAligned = RT_ALIGN_32(cWidth, 2 * DCTSIZE);
    /*const uint32_t cHeightAligned = RT_ALIGN_32(cHeight, 2 * DCTSIZE); - unused */

    /* Initialize the components description. */
    HWCJPEGCOMPONENT aComponents[3];

    /* Y */
    aComponents[0].pu8Samples     = pu8Src;
    aComponents[0].cBytesPerRow   = cWidth;
    aComponents[0].cRows          = cHeight;
    aComponents[0].cRequiredRows  = 0;

    /* U */
    aComponents[1].pu8Samples     = aComponents[0].pu8Samples +
                                    aComponents[0].cBytesPerRow * aComponents[0].cRows;
    aComponents[1].cBytesPerRow   = cWidth / 2;
    aComponents[1].cRows          = cHeight / 2;
    aComponents[1].cRequiredRows  = 0;

    /* V */
    aComponents[2].pu8Samples     = aComponents[1].pu8Samples +
                                    aComponents[1].cBytesPerRow * aComponents[1].cRows;
    aComponents[2].cBytesPerRow   = cWidth / 2;
    aComponents[2].cRows          = cHeight / 2;
    aComponents[2].cRequiredRows  = 0;

    /* Allocate temporary buffer for one MCU row (height=2 * DCTSIZE, width = cWidthAligned).
     * Y followed by Cb, followed by Cr.
     * size is 2 * DCTSIZE * (alignW + alignW/2 + alignW/2).
     */
    uint32_t cbMCU = 2 * DCTSIZE * 2 * cWidthAligned;
    uint8_t *pu8MCU = (uint8_t *)RTMemAllocZ(cbMCU);
    if (!pu8MCU)
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        pEncoder->dest.cbAllocated = cbData / 8; /* Expect compressed size 1/8 of original. */
        pEncoder->dest.pu8DestBuffer = (uint8_t *)RTMemAlloc(pEncoder->dest.cbAllocated);
        if (!pEncoder->dest.pu8DestBuffer)
        {
            rc = VERR_NO_MEMORY;
        }
    }

    if (RT_SUCCESS(rc))
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
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(aComponents); i++)
            {
                aComponents[i].cRequiredRows = pcinfo->comp_info[i].height_in_blocks * DCTSIZE;
            }

            Assert(   aComponents[0].cRequiredRows == 2 * aComponents[1].cRequiredRows
                   && aComponents[1].cRequiredRows == aComponents[2].cRequiredRows);

            /* Prepare JSAMPIMAGE struct for every 2 * DCTSIZE Y lines,
             * because jpeg_write_raw_data processes as many lines at a time.
             */
            JSAMPROW Y[2 * DCTSIZE];
            JSAMPROW U[DCTSIZE];
            JSAMPROW V[DCTSIZE];

            JSAMPARRAY data[3] = { Y, U, V }; /* Input parameter for jpeg_write_raw_data. */

            uint8_t *pu8Y = pu8MCU;
            uint8_t *pu8U = pu8Y + 2 * DCTSIZE * cWidthAligned;
            uint8_t *pu8V = pu8U + DCTSIZE * cWidthAligned / 2;

            /* Init row pointers. */
            for (i = 0; i < 2 * DCTSIZE; i++)
            {
                Y[i] = pu8Y + i * cWidthAligned;
            }

            for (i = 0; i < DCTSIZE; i++)
            {
                U[i] = pu8U + i * cWidthAligned / 2;
                V[i] = pu8V + i * cWidthAligned / 2;
            }

            uint32_t iLoop = 0;

            /* Scanlines are counted in not downsampled component. */
            bool fOkay = true; /* -Wclobbered workaround */
            while (pcinfo->next_scanline < pcinfo->image_height)
            {
                JDIMENSION cScanlines;
                uint32_t iComp;
                for (iComp = 0; iComp < RT_ELEMENTS(aComponents); iComp++)
                {
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

                    /* Copy source data to the temporary buffer. */
                    uint32_t cRowsLeft = aComponents[iComp].cRows - cScanlines;

                    const JSAMPLE *pSample = &aComponents[iComp].pu8Samples[cScanlines * aComponents[iComp].cBytesPerRow];

                    uint32_t iRow;
                    for (iRow = 0; iRow < cRows; iRow++)
                    {
                        uint8_t *pu8Dst = data[iComp][iRow];

                        if (iRow < cRowsLeft)
                        {
                            memcpy(pu8Dst, pSample, aComponents[iComp].cBytesPerRow);
                            pSample += aComponents[iComp].cBytesPerRow;
                        }
                        else
                        {
                            memset(pu8Dst, 0, aComponents[iComp].cBytesPerRow);
                        }
                    }
                }

                jpeg_write_raw_data(pcinfo, data, 2 * DCTSIZE);

                if (++iLoop > aComponents[0].cRequiredRows)
                {
                    /* Avoid infinite loop. It can't be more than number of rows,
                     * because every call must process 2*DCTSIZE rows.
                     */
                    fOkay = false;
                    break;
                }
            }

            jpeg_finish_compress(pcinfo);

            *ppu8Encoded = pEncoder->dest.pu8DestBuffer;
            *pcbEncoded = (uint32_t)(pEncoder->dest.cbAllocated - pEncoder->dest.pub.free_in_buffer);

            rc = fOkay ? VINF_SUCCESS : VERR_INTERRUPTED; /* Attempt at shutting up -Wclobbered warning. */
        }
    }

    RTMemFree(pu8MCU);

    if (RT_FAILURE(rc))
    {
        RTMemFree(pEncoder->dest.pu8DestBuffer);
    }

    pEncoder->dest.cbAllocated = 0;
    pEncoder->dest.pu8DestBuffer = NULL;

    return rc;
}
