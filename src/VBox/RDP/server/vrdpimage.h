/* $Id: vrdpimage.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vrdpimage_h
#define VRDP_INCLUDED_SRC_vrdpimage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdp.h"
#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/RemoteDesktop/VRDEImage.h>

#include "shadowbuffer.h"

#include "vrdpclient.h"
#include "bmpcache.h"
#include "bmpcomp.h"
extern "C" {
#include "mixeng/mixeng.h"
}

#include "videostream.h"

class VRDPServer;

typedef struct VRDEImageInterfaceData
{
    VRDEIMAGECALLBACKS callbacks;
    void *pvContext;
} VRDEImageInterfaceData;

class VRDPImage
{
    private:

       VRDEImageInterfaceData *m_pImageInterface;

       VRDPServer *m_pServer;
       void *m_pvUser;

       uint32_t m_u32ScreenId;

       char m_szId[64];
       uint32_t m_cbId;

    public:

       VRDPImage (VRDPServer *pServer, VRDEImageInterfaceData *pImageInterface, void *pvUser, uint32_t u32ScreenId);
       virtual ~VRDPImage ();

       static void ImageDelete (VRDPImage *pImage);

       uint32_t ScreenId(void) { return m_u32ScreenId; }

       int CallbackNotify(uint32_t u32Id,
                          void *pvData,
                          uint32_t cbData);

       int ImagePropertySet (const char *pszName,
                             const char *pszValue);
       int ImagePropertyQuery (const char *pszName,
                               char *pszValue,
                               uint32_t cbValueIn,
                               uint32_t *pcbValueOut);

       virtual int ImageCreate (uint32_t fu32Flags,
                                const RTRECT *pRect,
                                const void *pvFormat,
                                uint32_t cbFormat,
                                uint32_t *pfu32CompletionFlags);

       virtual int ImageRegionSet (uint32_t cRects,
                                   const RTRECT *paRects) = 0;
       virtual int ImageGeometrySet (const RTRECT *pRect) = 0;
       virtual void ImageUpdate (int32_t i32TargetX,
                                 int32_t i32TargetY,
                                 uint32_t u32TargetW,
                                 uint32_t u32TargetH,
                                 const void *pvImageData,
                                 uint32_t cbImageData) = 0;
};

#endif /* !VRDP_INCLUDED_SRC_vrdpimage_h */
