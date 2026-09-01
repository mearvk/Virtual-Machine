/** @file
 * JPEG compression of various input formats.
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

#ifndef VBOX_INCLUDED_SRC_Video_JPEGEnc_h
#define VBOX_INCLUDED_SRC_Video_JPEGEnc_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

/* Host WebCam JPEG encoder. */
typedef struct HWCJPEG HWCJPEG;

int HWCJPEGInit(HWCJPEG **ppEncoder);
void HWCJPEGDestroy(HWCJPEG *pEncoder);
int HWCJPEGEncodeFrameBGR0(HWCJPEG *pEncoder,
                          int iQuality,
                          uint8_t **ppu8Encoded,
                          uint32_t *pcbEncoded,
                          const uint8_t *pu8Src,
                          uint32_t cbSrc,
                          uint32_t cWidth,
                          uint32_t cHeight);
int HWCJPEGEncodeFrameRaw_2vuy_yuvs(HWCJPEG *pEncoder,
                                    int iQuality,
                                    uint8_t **ppu8Encoded,
                                    uint32_t *pcbEncoded,
                                    const uint8_t *pu8Src,
                                    uint32_t cbSrc,
                                    uint32_t cWidth,
                                    uint32_t cHeight,
                                    bool fYUVS);
int HWCJPEGEncodeFrameRaw_I420(HWCJPEG *pEncoder,
                               int iQuality,
                               uint8_t **ppu8Encoded,
                               uint32_t *pcbEncoded,
                               const uint8_t *pu8Src,
                               uint32_t cbSrc,
                               uint32_t cWidth,
                               uint32_t cHeight);

#endif /* !VBOX_INCLUDED_SRC_Video_JPEGEnc_h */
