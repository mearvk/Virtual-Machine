/* $Id: vrdpimage.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include <VBox/RemoteDesktop/VRDEImage.h>

#include "vrdpdefs.h"
#include "vrdpserv.h"
#include "vrdpimage.h"
#include "videostream.h"
#include "bmpscale.h"

/*
 * Implementation of different image putput modes.
 */
class VRDPImageBitmap: public VRDPImage
{
    private:

    public:

       VRDPImageBitmap (VRDPServer *pServer, VRDEImageInterfaceData *pImageInterface, void *pvUser, uint32_t u32ScreenId);
       virtual ~VRDPImageBitmap ();

       virtual int ImageCreate (uint32_t fu32Flags,
                                const RTRECT *pRect,
                                const void *pvFormat,
                                uint32_t cbFormat,
                                uint32_t *pfu32CompletionFlags);

       virtual int ImageRegionSet (uint32_t cRects,
                                   const RTRECT *paRects);

       virtual int ImageGeometrySet (const RTRECT *pRect);

       virtual void ImageUpdate (int32_t i32TargetX,
                                 int32_t i32TargetY,
                                 uint32_t u32TargetW,
                                 uint32_t u32TargetH,
                                 const void *pvImageData,
                                 uint32_t cbImageData);
};

class VRDPImageMJPEG: public VRDPImage
{
    private:

        uint32_t m_u32DirectStreamId;

        static DECLCALLBACK(int) imageStreamCallback(void *pvContext,
                                                     uint32_t u32Id,
                                                     void *pvData,
                                                     uint32_t cbData);

    public:

       VRDPImageMJPEG (VRDPServer *pServer, VRDEImageInterfaceData *pImageInterface, void *pvUser, uint32_t u32ScreenId);
       virtual ~VRDPImageMJPEG ();

       virtual int ImageCreate (uint32_t fu32Flags,
                                const RTRECT *pRect,
                                const void *pvFormat,
                                uint32_t cbFormat,
                                uint32_t *pfu32CompletionFlags);

       virtual int ImageRegionSet (uint32_t cRects,
                                   const RTRECT *paRects);

       virtual int ImageGeometrySet (const RTRECT *pRect);

       virtual void ImageUpdate (int32_t i32TargetX,
                                 int32_t i32TargetY,
                                 uint32_t u32TargetW,
                                 uint32_t u32TargetH,
                                 const void *pvImageData,
                                 uint32_t cbImageData);
};


/*
 * External wrappers for the VRDPServer Image API.
 */

static DECLCALLBACK(int) VRDEImageHandleCreate (HVRDESERVER hServer,
                                                HVRDEIMAGE *phImage,
                                                void *pvUser,
                                                uint32_t u32ScreenId,
                                                uint32_t fu32Flags,
                                                const RTRECT *pRect,
                                                const char *pszFormatId,
                                                const void *pvFormat,
                                                uint32_t cbFormat,
                                                uint32_t *pfu32CompletionFlags)
{
    int rc = VINF_SUCCESS;

    VRDPServer *pServer = (VRDPServer *)hServer;

    VRDPAPILOG(("%p\n", pServer));

    if (pServer)
    {
        VRDPImage *pImage = NULL;

        rc = pServer->ImageHandleCreate (&pImage, pvUser, u32ScreenId, fu32Flags, pRect, pszFormatId, pvFormat, cbFormat, pfu32CompletionFlags);

        if (RT_SUCCESS(rc))
        {
            *phImage = (HVRDEIMAGE)pImage;
        }
    }

    return rc;
}

static DECLCALLBACK(void) VRDEImageHandleClose (HVRDEIMAGE hImage)
{
    VRDPImage *pImage = (VRDPImage *)hImage;

    VRDPAPILOG(("%p\n", pImage));

    if (pImage)
    {
        VRDPImage::ImageDelete (pImage);
    }

    return;
}

static DECLCALLBACK(int) VRDEImageRegionSet (HVRDEIMAGE hImage,
                                             uint32_t cRects,
                                             const RTRECT *paRects)
{
    int rc = VINF_SUCCESS;

    VRDPImage *pImage = (VRDPImage *)hImage;

    VRDPAPILOG(("%p\n", pImage));

    if (pImage)
    {
        rc = pImage->ImageRegionSet (cRects, paRects);
    }

    return rc;
}

static DECLCALLBACK(int) VRDEImageGeometrySet (HVRDEIMAGE hImage,
                                               const RTRECT *pRect)
{
    int rc = VINF_SUCCESS;

    VRDPImage *pImage = (VRDPImage *)hImage;

    VRDPAPILOG(("%p\n", pImage));

    if (pImage)
    {
        rc = pImage->ImageGeometrySet (pRect);
    }

    return rc;
}

static DECLCALLBACK(int) VRDEImagePropertySet (HVRDEIMAGE hImage,
                                               const char *pszName,
                                               const char *pszValue)
{
    int rc = VINF_SUCCESS;

    VRDPImage *pImage = (VRDPImage *)hImage;

    VRDPAPILOG(("%p\n", pImage));

    if (pImage)
    {
        rc = pImage->ImagePropertySet (pszName, pszValue);
    }

    return rc;
}

static DECLCALLBACK(int) VRDEImagePropertyQuery (HVRDEIMAGE hImage,
                                                 const char *pszName,
                                                 char *pszValue,
                                                 uint32_t cbValueIn,
                                                 uint32_t *pcbValueOut)
{
    int rc = VINF_SUCCESS;

    VRDPImage *pImage = (VRDPImage *)hImage;

    VRDPAPILOG(("%p\n", pImage));

    if (pImage)
    {
        rc = pImage->ImagePropertyQuery (pszName, pszValue, cbValueIn, pcbValueOut);
    }

    return rc;
}

static DECLCALLBACK(void) VRDEImageUpdate (HVRDEIMAGE hImage,
                                           int32_t i32TargetX,
                                           int32_t i32TargetY,
                                           uint32_t u32TargetW,
                                           uint32_t u32TargetH,
                                           const void *pvImageData,
                                           uint32_t cbImageData)
{
    VRDPImage *pImage = (VRDPImage *)hImage;

    VRDPAPILOG(("%p\n", pImage));

    if (pImage)
    {
        pImage->ImageUpdate (i32TargetX, i32TargetY, u32TargetW, u32TargetH, pvImageData, cbImageData);
    }

    return;
}

static VRDEIMAGEINTERFACE gImageInterface =
{
    { 1, sizeof (VRDEIMAGEINTERFACE) },

    VRDEImageHandleCreate,
    VRDEImageHandleClose,
    VRDEImageRegionSet,
    VRDEImageGeometrySet,
    VRDEImagePropertySet,
    VRDEImagePropertyQuery,
    VRDEImageUpdate
};

/* Helper to get the interface method table, called from VRDPServer::GetInterface */
int vrdpGetInterfaceImage(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks)
{
    int rc = VINF_SUCCESS;

    if (   pInterface->u64Version == 1
        && pInterface->u64Size == sizeof (VRDEIMAGEINTERFACE)
        && pCallbacks->u64Version == 1
        && pCallbacks->u64Size == sizeof (VRDEIMAGECALLBACKS))
    {
        /* Interface version 1 is requested. */
        VRDEIMAGEINTERFACE *p = (VRDEIMAGEINTERFACE *)pInterface;

        *p = gImageInterface;
    }
    else
    {
        rc = VERR_VERSION_MISMATCH;
    }

    return rc;
}

/*
 * VRDPServer management helpers.
 */

int VRDPServer::ImageHandleCreate (VRDPImage **ppImage,
                                   void *pvUser,
                                   uint32_t u32Screen,
                                   uint32_t fu32Flags,
                                   const RTRECT *pRect,
                                   const char *pszFormatId,
                                   const void *pvFormat,
                                   uint32_t cbFormat,
                                   uint32_t *pfu32CompletionFlags)
{
    int rc = VINF_SUCCESS;
    VRDPImage *pImage = NULL;

    if (RTStrICmp(pszFormatId, VRDE_IMAGE_FMT_ID_BITMAP_BGRA8) == 0)
    {
        if (fu32Flags & (VRDE_IMAGE_F_CREATE_CONTENT_3D | VRDE_IMAGE_F_CREATE_WINDOW))
        {
            pImage = new VRDPImageMJPEG(this, &m_imageInterface, pvUser, u32Screen);
        }
        else
        {
            pImage = new VRDPImageBitmap(this, &m_imageInterface, pvUser, u32Screen);
        }

        if (RT_SUCCESS(rc))
        {
            if (pImage)
            {
                rc = pImage->ImageCreate(fu32Flags, pRect, pvFormat, cbFormat, pfu32CompletionFlags);
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }
        }
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        *ppImage = pImage;
    }
    else
    {
        if (pImage)
        {
            delete pImage;
        }
    }

    return rc;
}


/*
 * Base VRDPImage interface.
 *
 * This is a base for different image output methods: bitmap, JPEG, etc.
 */

VRDPImage::VRDPImage (VRDPServer *pServer, VRDEImageInterfaceData *pImageInterface, void *pvUser, uint32_t u32ScreenId)
    :
    m_pImageInterface (pImageInterface),
    m_pServer (pServer),
    m_pvUser (pvUser),
    m_u32ScreenId (u32ScreenId),
    m_cbId (0)
{
    memset(&m_szId, 0, sizeof(m_szId));
}

/* virtual */ VRDPImage::~VRDPImage ()
{
}

/* static */ void VRDPImage::ImageDelete (VRDPImage *pImage)
{
    delete pImage;
}

int VRDPImage::ImageCreate (uint32_t fu32Flags,
                            const RTRECT *pRect,
                            const void *pvFormat,
                            uint32_t cbFormat,
                            uint32_t *pfu32CompletionFlags)
{
    NOREF(fu32Flags);
    NOREF(pRect);
    NOREF(pvFormat);
    NOREF(cbFormat);
    NOREF(pfu32CompletionFlags);

    RTUUID uuid;
    int rc = RTUuidCreate(&uuid);

    if (RT_SUCCESS(rc))
    {
        rc = RTUuidToStr(&uuid, m_szId, sizeof(m_szId));

        if (RT_SUCCESS(rc))
        {
            m_cbId = (uint32_t)(strlen(m_szId) + 1);
        }
    }

    return rc;
}

int VRDPImage::ImagePropertySet (const char *pszName,
                                 const char *pszValue)
{
    int rc = VINF_SUCCESS;

    rc = VERR_NOT_SUPPORTED; /** @todo */
    RT_NOREF2(pszName, pszValue);

    return rc;
}

int VRDPImage::ImagePropertyQuery (const char *pszName,
                                   char *pszValue,
                                   uint32_t cbValueIn,
                                   uint32_t *pcbValueOut)
{
    int rc = VINF_SUCCESS;

    if (RTStrCmp(pszName, "ID") == 0)
    {
        if (m_cbId <= cbValueIn)
        {
            memcpy(pszValue, m_szId, m_cbId);
        }
        else
        {
            rc = VERR_BUFFER_OVERFLOW;
        }
        *pcbValueOut = m_cbId;
    }
    else
    {
        rc = VERR_NOT_SUPPORTED; /** @todo */
    }

    return rc;
}

int VRDPImage::CallbackNotify(uint32_t u32Id,
                              void *pvData,
                              uint32_t cbData)
{
    if (m_pImageInterface && m_pImageInterface->callbacks.VRDEImageCbNotify)
    {
        return m_pImageInterface->callbacks.VRDEImageCbNotify(m_pImageInterface->pvContext,
                                                              m_pvUser,
                                                              (HVRDEIMAGE)this,
                                                              u32Id,
                                                              pvData,
                                                              cbData);
    }

    return VERR_NOT_SUPPORTED;
}


/*
 * Regular RDP bitmap updates.
 */
VRDPImageBitmap::VRDPImageBitmap (VRDPServer *pServer, VRDEImageInterfaceData *pImageInterface, void *pvUser, uint32_t u32ScreenId)
    :
    VRDPImage(pServer, pImageInterface, pvUser, u32ScreenId)
{
}

/* virtual */ VRDPImageBitmap::~VRDPImageBitmap ()
{
}

int VRDPImageBitmap::ImageCreate (uint32_t fu32Flags,
                                  const RTRECT *pRect,
                                  const void *pvFormat,
                                  uint32_t cbFormat,
                                  uint32_t *pfu32CompletionFlags)
{
    int rc = VRDPImage::ImageCreate (fu32Flags,
                                     pRect,
                                     pvFormat,
                                     cbFormat,
                                     pfu32CompletionFlags);
    if (RT_FAILURE(rc))
    {
        return rc;
    }

    /* Currently it is a bitmap, which does not use any format specific data. */
    NOREF(pvFormat);
    NOREF(cbFormat);

    /** @todo process the flags. */
    NOREF(fu32Flags);

    /* Nothing to do anymore. */
    *pfu32CompletionFlags = VRDE_IMAGE_F_COMPLETE_DEFAULT;

    return rc;
}

int VRDPImageBitmap::ImageRegionSet (uint32_t cRects,
                                     const RTRECT *paRects)
{
    int rc = VINF_SUCCESS;

    rc = VERR_NOT_SUPPORTED; /** @todo */
    RT_NOREF2(cRects, paRects);

    return rc;
}

int VRDPImageBitmap::ImageGeometrySet (const RTRECT *pRect)
{
    RT_NOREF1(pRect);
    int rc = VINF_SUCCESS;

    /* Bitmap updates do not care about the actual area. */

    return rc;
}

void VRDPImageBitmap::ImageUpdate (int32_t i32TargetX,
                                   int32_t i32TargetY,
                                   uint32_t u32TargetW,
                                   uint32_t u32TargetH,
                                   const void *pvImageData,
                                   uint32_t cbImageData)
{
    if (cbImageData != sizeof(VRDEIMAGEBITMAP))
    {
        AssertFailed();
        return;
    }

    VRDEIMAGEBITMAP *p = (VRDEIMAGEBITMAP *)pvImageData;

    bool fVideoDetection = false;

    /* Bitmap updates do not support scaling. */
    uint32_t w = RT_MIN(u32TargetW, p->cWidth);
    uint32_t h = RT_MIN(u32TargetH, p->cHeight);

    shadowBufferBitmapUpdateEx (ScreenId(),
                                i32TargetX, i32TargetY,
                                w, h,
                                (uint8_t *)p->pvScanLine0,
                                p->iScanDelta,
                                fVideoDetection);
}


/*
 * Video redirection using MJPEG.
 *
 * It uses the video redirection channel to create a video player window
 * together with a clipping region to transfer 3D content to the
 * client for demo purposes.
 *
 * Usually 3D is a separate window (or fullscreen), so in principle a player window
 * can be setup for each 3D. The window is currently recreated after each change
 * of position and size.
 *
 */
VRDPImageMJPEG::VRDPImageMJPEG (VRDPServer *pServer, VRDEImageInterfaceData *pImageInterface, void *pvUser, uint32_t u32ScreenId)
    :
    VRDPImage(pServer, pImageInterface, pvUser, u32ScreenId)
{
    m_u32DirectStreamId = 0;
}

/* virtual */ VRDPImageMJPEG::~VRDPImageMJPEG ()
{
    if (m_u32DirectStreamId)
    {
        videoHandlerDirectStreamStop(m_u32DirectStreamId);
        m_u32DirectStreamId = 0;
    }
}

/* static */ DECLCALLBACK(int) VRDPImageMJPEG::imageStreamCallback(void *pvContext,
                                                                   uint32_t u32Id,
                                                                   void *pvData,
                                                                   uint32_t cbData)
{
    VRDPImageMJPEG *pThis = (VRDPImageMJPEG *)pvContext;

    Log(("GEOM: imageStreamCallback\n"));

    Assert(cbData == sizeof (uint32_t));

    uint32_t u32StreamId = *(uint32_t *)pvData;

    if (u32StreamId != pThis->m_u32DirectStreamId)
    {
        return VERR_NOT_SUPPORTED;
    }

    return pThis->CallbackNotify(u32Id, pvData, cbData);
}

int VRDPImageMJPEG::ImageCreate (uint32_t fu32Flags,
                                 const RTRECT *pRect,
                                 const void *pvFormat,
                                 uint32_t cbFormat,
                                 uint32_t *pfu32CompletionFlags)
{
    int rc = VRDPImage::ImageCreate (fu32Flags,
                                     pRect,
                                     pvFormat,
                                     cbFormat,
                                     pfu32CompletionFlags);
    if (RT_FAILURE(rc))
    {
        return rc;
    }

    /* Currently it is a bitmap, which does not use any format specific data. */
    NOREF(pvFormat);
    NOREF(cbFormat);

    AssertReturn((fu32Flags & VRDE_IMAGE_F_CREATE_WINDOW) != 0, VERR_INVALID_PARAMETER);

    RGNRECT rect;

    rect.x = pRect->xLeft;
    rect.y = pRect->yTop;
    rect.w = pRect->xRight - pRect->xLeft;
    rect.h = pRect->yBottom - pRect->yTop;

    VHSTREAMCALLBACKDATA ctx;
    ctx.VHStreamNotify = imageStreamCallback;
    ctx.pvContext = this;

    m_u32DirectStreamId = 0;
    bool fAccepted = videoHandlerDirectStreamStart(&m_u32DirectStreamId, &rect, ScreenId(), &ctx);

    if (!fAccepted)
    {
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        /* The video redirection channel is being established. */
        *pfu32CompletionFlags = VRDE_IMAGE_F_COMPLETE_ASYNC;
    }

    return rc;
}

int VRDPImageMJPEG::ImageRegionSet (uint32_t cRects,
                                    const RTRECT *paRects)
{
    int rc = VINF_SUCCESS;

    if (cRects != 0)
    {
        videoHandlerDirectRegion(m_u32DirectStreamId,
                                 cRects,
                                 paRects);
    }

    return rc;
}

int VRDPImageMJPEG::ImageGeometrySet(const RTRECT *pRect)
{
    return videoHandlerDirectGeometry(m_u32DirectStreamId, pRect);
}

typedef struct BITMAPREADCTX
{
    VRDPImageMJPEG *pThis;
    VRDEIMAGEBITMAP *pBitmap;
} BITMAPREADCTX;

static DECLCALLBACK(int) imageBitmapRead (void *pvCtx,
                                          uint8_t *pu8Buffer,
                                          uint32_t cbBuffer)
{
    RT_NOREF1(cbBuffer);
    BITMAPREADCTX *pCtx = (BITMAPREADCTX *)pvCtx;
    VRDEIMAGEBITMAP *pBitmap = pCtx->pBitmap;

    /* Copy bitmap data to the supplied buffer. */

    if (RT_LIKELY(pBitmap->iScanDelta == (int)(pBitmap->cWidth * 4)))
    {
        memcpy(pu8Buffer, pBitmap->pvScanLine0, pBitmap->iScanDelta * pBitmap->cHeight);
    }
    else
    {
        const uint8_t *s = (uint8_t *)pBitmap->pvScanLine0;
        uint8_t *d = pu8Buffer;

        unsigned y;
        for (y = 0; y < pBitmap->cHeight; y++)
        {
            memcpy(d, s, pBitmap->cWidth * 4);

            s += pBitmap->iScanDelta;
            d += pBitmap->cWidth * 4;
        }
    }

    return VINF_SUCCESS;
}

/** @todo i32Target* are necessary to put the image within the area.
 *       This used for scaling and for partial updates.
 */
void VRDPImageMJPEG::ImageUpdate (int32_t i32TargetX,
                                  int32_t i32TargetY,
                                  uint32_t u32TargetW,
                                  uint32_t u32TargetH,
                                  const void *pvImageData,
                                  uint32_t cbImageData)
{
    if (cbImageData != sizeof(VRDEIMAGEBITMAP))
    {
        AssertFailed();
        return;
    }

    VRDEIMAGEBITMAP *p = (VRDEIMAGEBITMAP *)pvImageData;

    BITMAPREADCTX ctx;

    ctx.pThis = this;
    ctx.pBitmap = p;

    RGNRECT rectTarget;
    rectTarget.x = i32TargetX;
    rectTarget.y = i32TargetY;
    rectTarget.w = u32TargetW;
    rectTarget.h = u32TargetH;

    videoHandlerDirectFrame(m_u32DirectStreamId,
                            &rectTarget,
                            imageBitmapRead,
                            &ctx);
}

