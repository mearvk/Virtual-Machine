/* $Id: videoencoder.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_videoencoder_h
#define VRDP_INCLUDED_SRC_videoencoder_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpdefs.h"

typedef struct VEJPEG VEJPEG;

int VEJPEGInit(VEJPEG **ppEncoder);
void VEJPEGDestroy(VEJPEG *pEncoder);
int VEJPEGEncodeFrameBGR0(VEJPEG *pEncoder,
                          int iQuality,
                          uint8_t **ppu8Encoded,
                          uint32_t *pcbEncoded,
                          const uint8_t *pu8Bitmap,
                          uint32_t cWidth,
                          uint32_t cHeight);
int VEJPEGEncodeFrameYUVRaw(VEJPEG *pEncoder,
                            int iQuality,
                            uint8_t **ppu8Encoded,
                            uint32_t *pcbEncoded,
                            const uint8_t *pu8YUVRaw,
                            uint32_t cbYUVRaw,
                            uint32_t cWidth,
                            uint32_t cHeight);

#endif /* !VRDP_INCLUDED_SRC_videoencoder_h */
