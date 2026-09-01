/* $Id: jpegtest.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_jpegtest_jpegtest_h
#define VRDP_INCLUDED_SRC_jpegtest_jpegtest_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "../vrdpdefs.h"
#include "../videoencoder.h"
#include "../bmpscale.h"

#include <stdio.h>
#include <string.h>

#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/time.h>


void bmpWrite (const char *pszBMP, const uint8_t *pu8Bits, int w, int h);
uint8_t *bmpRead (const char *pszBMP, uint32_t *pu32Width, uint32_t *pu32Height, uint32_t *pu32BitsPerPixel);
void bmpFree(uint8_t *pu8Bitmap);
void jpgWrite (const char *pszName, uint8_t *pu8JPEG, uint32_t cbJPEG);


void rawTest(const char *pszBMP);

#endif /* !VRDP_INCLUDED_SRC_jpegtest_jpegtest_h */

