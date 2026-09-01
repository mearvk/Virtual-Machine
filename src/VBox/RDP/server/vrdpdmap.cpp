/* $Id: vrdpdmap.cpp 113981 2026-04-23 07:50:11Z andreas.loeffler@oracle.com $ */
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

#include "vrdp.h"
#include "shadowbuffer.h"

class VRDPClientDesktopMapIdentical: public VRDPClientDesktopMap
{
    private:
        unsigned m_uScreenId;

    public:
        VRDPClientDesktopMapIdentical(VRDPTP *pTP, unsigned uScreenId);
        virtual ~VRDPClientDesktopMapIdentical();

        virtual void OnResolutionChanged(void);
        virtual void CalculateClientRect(RGNRECT *pRect);
        virtual void QueryClientMonitorRect (unsigned uScreenId, RGNRECT *pRect);
        virtual bool IsScreenMatched (unsigned uScreenId);
        virtual int DesktopMode(void);

        virtual void Client2Client (unsigned uScreenId, const RGNRECT *pRectClient, RGNRECT *pRectResult);
        virtual void Client2Screen (unsigned uScreenId, const RGNRECT *pRectClient, RGNRECT *pRectResult);
        virtual void Client2ScreenPoint(unsigned *puScreenId, int16_t *pi16x, int16_t *pi16y);

        virtual void Screen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult);

        virtual void Screen2ClientPoint (unsigned uScreenId, int16_t *pi16x, int16_t *pi16y);

        virtual bool ClipScreen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult);
};

class VRDPClientDesktopMapSingleMon: public VRDPClientDesktopMap
{
    private:
        unsigned m_uScreenId;

        int m_xScreenOrigin;
        int m_yScreenOrigin;

        RGNRECT m_rectShadowBuffer;

    public:
        VRDPClientDesktopMapSingleMon(VRDPTP *pTP, unsigned uScreenId);
        virtual ~VRDPClientDesktopMapSingleMon();

        virtual void OnResolutionChanged(void);
        virtual void CalculateClientRect(RGNRECT *pRect);
        virtual void QueryClientMonitorRect (unsigned uScreenId, RGNRECT *pRect);
        virtual bool IsScreenMatched (unsigned uScreenId);
        virtual int DesktopMode(void);

        virtual void Client2Client (unsigned uScreenId, const RGNRECT *pRectClient, RGNRECT *pRectResult);
        virtual void Client2Screen (unsigned uScreenId, const RGNRECT *pRectClient, RGNRECT *pRectResult);
        virtual void Client2ScreenPoint(unsigned *puScreenId, int16_t *pi16x, int16_t *pi16y);

        virtual void Screen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult);

        virtual void Screen2ClientPoint (unsigned uScreenId, int16_t *pi16x, int16_t *pi16y);

        virtual bool ClipScreen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult);
};

class VRDPClientDesktopMapMultiMon: public VRDPClientDesktopMap
{
    private:
        uint32_t m_cMonitors;
        RGNRECT *m_paRects;

        uint32_t m_cShadowBuffers;
        RGNRECT *m_paRectsShadowBuffer;
        int32_t *m_paScreenOriginX;
        int32_t *m_paScreenOriginY;

    public:
        VRDPClientDesktopMapMultiMon(VRDPTP *pTP);
        virtual ~VRDPClientDesktopMapMultiMon();

        virtual void OnResolutionChanged(void);
        virtual void CalculateClientRect(RGNRECT *pRect);
        virtual void QueryClientMonitorRect (unsigned uScreenId, RGNRECT *pRect);
        virtual bool IsScreenMatched (unsigned uScreenId);
        virtual int DesktopMode(void);

        virtual void Client2Client (unsigned uScreenId, const RGNRECT *pRectClient, RGNRECT *pRectResult);
        virtual void Client2Screen (unsigned uScreenId, const RGNRECT *pRectClient, RGNRECT *pRectResult);
        virtual void Client2ScreenPoint(unsigned *puScreenId, int16_t *pi16x, int16_t *pi16y);

        virtual void Screen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult);

        virtual void Screen2ClientPoint (unsigned uScreenId, int16_t *pi16x, int16_t *pi16y);

        virtual bool ClipScreen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult);
};


/*
 * The base VRDPClientDesktopMap implementation.
 */

VRDPClientDesktopMap::VRDPClientDesktopMap(VRDPTP *pTP)
    :
    m_pTP(pTP)
{
    RT_ZERO(m_rectClient);
}

/* virtual */ VRDPClientDesktopMap::~VRDPClientDesktopMap()
{
}

int VRDPClientDesktopMap::initializeDesktopMap(void)
{
    CalculateClientRect(&m_rectClient);
    return VINF_SUCCESS;
}

/* static */ int VRDPClientDesktopMap::CreateDesktopMap(VRDPClientDesktopMap **ppMap, VRDPTP *pTP, unsigned uScreenId)
{
    int rc = VINF_SUCCESS;

    /* Select appropriate mode from client data and server config. */
    int iMode = VRDP_CLIENT_DESKTOP_MAP_SINGLE_MONITOR;

    if (   (pTP->Data()->EarlyCapabilityFlags() & RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU) != 0
        && pTP->Data()->MonitorCount() > 1)
    {
        iMode = VRDP_CLIENT_DESKTOP_MAP_MULTI_MONITOR;
    }
    else if (!pTP->IsClientResolution())
    {
        iMode = VRDP_CLIENT_DESKTOP_MAP_IDENTICAL;
    }

    VRDPClientDesktopMap *pMap = NULL;

    if (iMode == VRDP_CLIENT_DESKTOP_MAP_IDENTICAL)
    {
        pMap = new VRDPClientDesktopMapIdentical(pTP, uScreenId);
    }
    else if (iMode == VRDP_CLIENT_DESKTOP_MAP_SINGLE_MONITOR)
    {
        pMap = new VRDPClientDesktopMapSingleMon(pTP, uScreenId);
    }
    else if (iMode == VRDP_CLIENT_DESKTOP_MAP_MULTI_MONITOR)
    {
        pMap = new VRDPClientDesktopMapMultiMon(pTP);
    }
    else
    {
        rc = VERR_INVALID_PARAMETER;
    }

    if (RT_SUCCESS(rc))
    {
        if (pMap != NULL)
        {
            rc = pMap->initializeDesktopMap();
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }
    }

    if (RT_SUCCESS(rc))
    {
        *ppMap = pMap;
    }
    else
    {
        if (pMap)
        {
             delete pMap;
             pMap = NULL;
        }
    }

    return rc;
}

void VRDPClientDesktopMap::Reinitialize (void)
{
    initializeDesktopMap();
}

void VRDPClientDesktopMap::QueryClientRect (RGNRECT *pClient)
{
    /* Return the resolution of the entire client's desktop. */
    pClient->x = 0;
    pClient->y = 0;
    pClient->w = m_rectClient.w;
    pClient->h = m_rectClient.h;
}


/*
 * Identical mapping, the client always has the same size as the shadowbuffer.
 */

VRDPClientDesktopMapIdentical::VRDPClientDesktopMapIdentical(VRDPTP *pTP, unsigned uScreenId)
    :
    VRDPClientDesktopMap(pTP),
    m_uScreenId (uScreenId)
{
}

/* virtual */ VRDPClientDesktopMapIdentical::~VRDPClientDesktopMapIdentical()
{
}

/* virtual */ void VRDPClientDesktopMapIdentical::OnResolutionChanged(void)
{
   /* Do nothing. */
}

/* virtual */ void VRDPClientDesktopMapIdentical::CalculateClientRect(RGNRECT *pRect)
{
    VRDPData *pData = TP()->Data();

    /* The client can be resized. Use the shadow buffer resolution. */
    RGNRECT rect;
    shadowBufferQueryRect (m_uScreenId, &rect);

    /* Only change the client's supplied resolution if the shadow buffer is initialized. */
    if (rect.w == 0)
    {
        rect.w = pData->DesktopWidth();
    }
    if (rect.h == 0)
    {
        rect.h = pData->DesktopHeight();
    }

    rect.x = 0;
    rect.y = 0;

    *pRect = rect;
}

/* virtual */ void VRDPClientDesktopMapIdentical::QueryClientMonitorRect (unsigned uScreenId, RGNRECT *pRect)
{
    /*VRDPData *pData = TP()->Data();*/

    if (!IsScreenMatched(uScreenId))
    {
        pRect->x = 0;
        pRect->y = 0;
        pRect->w = 0;
        pRect->h = 0;
        return;
    }

    QueryClientRect(pRect);
}

/* virtual */ bool VRDPClientDesktopMapIdentical::IsScreenMatched (unsigned uScreenId)
{
    return uScreenId == m_uScreenId;
}

/* virtual */ int VRDPClientDesktopMapIdentical::DesktopMode(void)
{
    return VRDP_CLIENT_DESKTOP_MAP_IDENTICAL;
}

/* virtual */ void VRDPClientDesktopMapIdentical::Client2Client (unsigned uScreenId,
                                                                 const RGNRECT *pRectClient,
                                                                 RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    *pRectResult = *pRectClient;
}

/* virtual */ void VRDPClientDesktopMapIdentical::Client2Screen (unsigned uScreenId,
                                                                 const RGNRECT *pRectClient,
                                                                 RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    *pRectResult = *pRectClient;
}

/* virtual */ void VRDPClientDesktopMapIdentical::Client2ScreenPoint(unsigned *puScreenId, int16_t *pi16x, int16_t *pi16y)
{
    RT_NOREF2(pi16x, pi16y);
    *puScreenId = m_uScreenId;
    /* No translation necessary. */
}

/* virtual */ void VRDPClientDesktopMapIdentical::Screen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    *pRectResult = *pRectScreen;
}

/* virtual */ void VRDPClientDesktopMapIdentical::Screen2ClientPoint (unsigned uScreenId, int16_t *pi16x, int16_t *pi16y)
{
    RT_NOREF3(uScreenId, pi16x, pi16y);
    /* No translation necessary. */
}

/* virtual */ bool VRDPClientDesktopMapIdentical::ClipScreen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        return false;
    }

    RGNRECT rectClient;
    QueryClientRect (&rectClient);

    if (!rgnIntersectRects(&rectClient, &rectClient, pRectScreen))
    {
        return false;
    }

    *pRectResult = rectClient;
    return true;
}


/*
 * Single monitor. The client size is fixed and the smaller shadow buffer is centered.
 */

VRDPClientDesktopMapSingleMon::VRDPClientDesktopMapSingleMon(VRDPTP *pTP, unsigned uScreenId)
    :
    VRDPClientDesktopMap(pTP),
    m_uScreenId (uScreenId),
    m_xScreenOrigin (0),
    m_yScreenOrigin (0)
{
    RT_ZERO(m_rectShadowBuffer);
}

/* virtual */ VRDPClientDesktopMapSingleMon::~VRDPClientDesktopMapSingleMon()
{
}

/* virtual */ void VRDPClientDesktopMapSingleMon::OnResolutionChanged(void)
{
    shadowBufferQueryRect (m_uScreenId, &m_rectShadowBuffer);

    if (m_rectShadowBuffer.w >= GetClientWidth())
    {
        m_xScreenOrigin = 0;
    }
    else
    {
        m_xScreenOrigin = (GetClientWidth() - m_rectShadowBuffer.w) / 2;
    }

    if (m_rectShadowBuffer.h >= GetClientHeight())
    {
        m_yScreenOrigin = 0;
    }
    else
    {
        m_yScreenOrigin = (GetClientHeight() - m_rectShadowBuffer.h) / 2;
    }

    m_rectShadowBuffer.x += m_xScreenOrigin;
    m_rectShadowBuffer.y += m_yScreenOrigin;

    /* Tell the guest to adjust the resolution to fit the client. */
    if (   m_rectShadowBuffer.w != GetClientWidth()
        || m_rectShadowBuffer.h != GetClientHeight())
    {
        TP()->Desktop()->NotifyClientResolution (GetClientWidth(), GetClientHeight(), m_uScreenId);
    }
}

/* virtual */ void VRDPClientDesktopMapSingleMon::CalculateClientRect(RGNRECT *pRect)
{
    VRDPData *pData = TP()->Data();

    /* The client can not change resolution. Use what the client has reported. */
    pRect->x = 0;
    pRect->y = 0;
    pRect->w = pData->DesktopWidth();
    pRect->h = pData->DesktopHeight();
}

/* virtual */ void VRDPClientDesktopMapSingleMon::QueryClientMonitorRect (unsigned uScreenId, RGNRECT *pRect)
{
    /*VRDPData *pData = TP()->Data();*/

    if (!IsScreenMatched(uScreenId))
    {
        pRect->x = 0;
        pRect->y = 0;
        pRect->w = 0;
        pRect->h = 0;
        return;
    }

    QueryClientRect(pRect);
}

/* virtual */ bool VRDPClientDesktopMapSingleMon::IsScreenMatched (unsigned uScreenId)
{
    return uScreenId == m_uScreenId;
}

/* virtual */ int VRDPClientDesktopMapSingleMon::DesktopMode(void)
{
    return VRDP_CLIENT_DESKTOP_MAP_SINGLE_MONITOR;
}

/* virtual */ void VRDPClientDesktopMapSingleMon::Client2Client (unsigned uScreenId,
                                                                 const RGNRECT *pRectClient,
                                                                 RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    *pRectResult = *pRectClient;
}

/* virtual */ void VRDPClientDesktopMapSingleMon::Client2Screen (unsigned uScreenId,
                                                                 const RGNRECT *pRectClient,
                                                                 RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    rgnIntersectRects(pRectResult, pRectClient, &m_rectShadowBuffer);

    pRectResult->x -= m_xScreenOrigin;
    pRectResult->y -= m_yScreenOrigin;
}

/* virtual */ void VRDPClientDesktopMapSingleMon::Client2ScreenPoint(unsigned *puScreenId, int16_t *pi16x, int16_t *pi16y)
{
    *puScreenId = m_uScreenId;
    *pi16x -= (int16_t)m_xScreenOrigin;
    *pi16y -= (int16_t)m_yScreenOrigin;
}

/* virtual */ void VRDPClientDesktopMapSingleMon::Screen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    pRectResult->x = pRectScreen->x + m_xScreenOrigin;
    pRectResult->y = pRectScreen->y + m_yScreenOrigin;
    pRectResult->w = pRectScreen->w;
    pRectResult->h = pRectScreen->h;
}

/* virtual */ void VRDPClientDesktopMapSingleMon::Screen2ClientPoint (unsigned uScreenId, int16_t *pi16x, int16_t *pi16y)
{
    Assert(IsScreenMatched(uScreenId));
    NOREF(uScreenId);

    *pi16x += (int16_t)m_xScreenOrigin;
    *pi16y += (int16_t)m_yScreenOrigin;
}

/* virtual */ bool VRDPClientDesktopMapSingleMon::ClipScreen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        return false;
    }

    RGNRECT rectClient;
    Screen2Client (uScreenId, pRectScreen, &rectClient);

    if (!rgnIntersectRects(&rectClient, &rectClient, &m_rectShadowBuffer))
    {
        return false;
    }

    *pRectResult = rectClient;
    return true;
}


/*
 * TS multimonitor mode. Multiple shadow buffers map to the single large client desktop.
 */

VRDPClientDesktopMapMultiMon::VRDPClientDesktopMapMultiMon(VRDPTP *pTP)
    :
    VRDPClientDesktopMap(pTP),
    m_cMonitors(0),
    m_paRects(NULL),
    m_cShadowBuffers(0),
    m_paRectsShadowBuffer(NULL),
    m_paScreenOriginX(NULL),
    m_paScreenOriginY(NULL)
{
}

/* virtual */ VRDPClientDesktopMapMultiMon::~VRDPClientDesktopMapMultiMon()
{
    VRDPMemFree(m_paRectsShadowBuffer);
    m_paRectsShadowBuffer = NULL;

    VRDPMemFree(m_paScreenOriginX);
    m_paScreenOriginX = NULL;

    VRDPMemFree(m_paScreenOriginY);
    m_paScreenOriginY = NULL;
}

/* virtual */ void VRDPClientDesktopMapMultiMon::OnResolutionChanged(void)
{
    unsigned cShadowBuffers = shadowBufferQueryScreenCount();

    if (cShadowBuffers == 0)
    {
        return;
    }

    if (cShadowBuffers > m_cMonitors)
    {
        cShadowBuffers = m_cMonitors;
    }

    RGNRECT *paRectsShadowBuffer = (RGNRECT *)VRDPMemAlloc(sizeof (RGNRECT) * cShadowBuffers);
    int32_t *paScreenOriginX = (int32_t *)VRDPMemAlloc(sizeof (int32_t) * cShadowBuffers);
    int32_t *paScreenOriginY = (int32_t *)VRDPMemAlloc(sizeof (int32_t) * cShadowBuffers);

    unsigned uScreenId;

    for (uScreenId = 0; uScreenId < cShadowBuffers; uScreenId++)
    {
        shadowBufferQueryRect (uScreenId, &paRectsShadowBuffer[uScreenId]);

        if (paRectsShadowBuffer[uScreenId].w >= m_paRects[uScreenId].w)
        {
            paScreenOriginX[uScreenId] = 0;
        }
        else
        {
            paScreenOriginX[uScreenId] = (m_paRects[uScreenId].w - paRectsShadowBuffer[uScreenId].w) / 2;
        }

        paScreenOriginX[uScreenId] += m_paRects[uScreenId].x;

        if (paRectsShadowBuffer[uScreenId].h >= m_paRects[uScreenId].h)
        {
            paScreenOriginY[uScreenId] = 0;
        }
        else
        {
            paScreenOriginY[uScreenId] = (m_paRects[uScreenId].h - paRectsShadowBuffer[uScreenId].h) / 2;
        }

        paScreenOriginY[uScreenId] += m_paRects[uScreenId].y;

        paRectsShadowBuffer[uScreenId].x += paScreenOriginX[uScreenId];
        paRectsShadowBuffer[uScreenId].y += paScreenOriginY[uScreenId];

        /* Tell the guest to adjust the resolution to fit the client. */
        if (   paRectsShadowBuffer[uScreenId].w != m_paRects[uScreenId].w
            || paRectsShadowBuffer[uScreenId].h != m_paRects[uScreenId].h)
        {
            /** @todo also pass the position of the monitor. Needs a new SetVideoModeHint. */
            TP()->Desktop()->NotifyClientResolution (m_paRects[uScreenId].w, m_paRects[uScreenId].h, uScreenId);
        }
    }

    VRDPMemFree(m_paRectsShadowBuffer);
    m_paRectsShadowBuffer = paRectsShadowBuffer;

    VRDPMemFree(m_paScreenOriginX);
    m_paScreenOriginX = paScreenOriginX;

    VRDPMemFree(m_paScreenOriginY);
    m_paScreenOriginY = paScreenOriginY;

    m_cShadowBuffers = cShadowBuffers;
}

/* virtual */ void VRDPClientDesktopMapMultiMon::CalculateClientRect(RGNRECT *pRect)
{
    VRDPData *pData = TP()->Data();

    /* Prepare the RGNRECT array of monitor positions.
     * @todo In this array the element i correspond to the i'th shadow buffer.
     */
    uint32_t cMonitors = pData->MonitorCount();
    if (cMonitors == 0)
    {
        AssertFailed();
        pRect->x = 0;
        pRect->y = 0;
        pRect->w = 0;
        pRect->h = 0;
        return;
    }

    uint32_t cbRects = cMonitors * sizeof(RGNRECT);
    RGNRECT *paRects = (RGNRECT *)VRDPMemAlloc(cbRects);

    unsigned i;
    for (i = 0; i < cMonitors; i++)
    {
        const TS_MONITOR_DEF *pMonitor = pData->MonitorDef(i);

        if (pMonitor)
        {
            paRects[i].x = pMonitor->i32Left;
            paRects[i].y = pMonitor->i32Top;
            paRects[i].w = pMonitor->i32Right - pMonitor->i32Left + 1;
            paRects[i].h = pMonitor->i32Bottom - pMonitor->i32Top + 1;
        }
    }

    RGNRECT rectDesktop = paRects[0];
    for (i = 1; i < cMonitors; i++)
    {
        rgnMergeRects(&rectDesktop, &rectDesktop, &paRects[i]);
    }

    m_paRects = paRects;
    m_cMonitors = cMonitors;

    *pRect = rectDesktop;
}

/* virtual */ void VRDPClientDesktopMapMultiMon::QueryClientMonitorRect (unsigned uScreenId, RGNRECT *pRect)
{
    if (uScreenId >= m_cMonitors)
    {
        pRect->x = 0;
        pRect->y = 0;
        pRect->w = 0;
        pRect->h = 0;
        return;
    }

    *pRect = m_paRects[uScreenId];
}

/* virtual */ bool VRDPClientDesktopMapMultiMon::IsScreenMatched (unsigned uScreenId)
{
    /* Accept almost all. */
    if (uScreenId >= m_cMonitors)
    {
        return false;
    }

    return true;
}

/* virtual */ int VRDPClientDesktopMapMultiMon::DesktopMode(void)
{
    return VRDP_CLIENT_DESKTOP_MAP_MULTI_MONITOR;
}

/* virtual */ void VRDPClientDesktopMapMultiMon::Client2Client (unsigned uScreenId,
                                                                const RGNRECT *pRectClient,
                                                                RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    rgnIntersectRects(pRectResult, pRectClient, &m_paRects[uScreenId]);
}

/* virtual */ void VRDPClientDesktopMapMultiMon::Client2Screen (unsigned uScreenId,
                                                                const RGNRECT *pRectClient,
                                                                RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    rgnIntersectRects(pRectResult, pRectClient, &m_paRectsShadowBuffer[uScreenId]);

    pRectResult->x -= m_paScreenOriginX[uScreenId];
    pRectResult->y -= m_paScreenOriginY[uScreenId];
}

/* virtual */ void VRDPClientDesktopMapMultiMon::Client2ScreenPoint(unsigned *puScreenId, int16_t *pi16x, int16_t *pi16y)
{
    /* Find the screen which contains the point. */
    unsigned uScreenId;

    for (uScreenId = 0; uScreenId < m_cShadowBuffers; uScreenId++)
    {
        if (   (m_paRects[uScreenId].x <= *pi16x && *pi16x < m_paRects[uScreenId].x + (int)m_paRects[uScreenId].w)
            && (m_paRects[uScreenId].y <= *pi16y && *pi16y < m_paRects[uScreenId].y + (int)m_paRects[uScreenId].h))
        {
            break;
        }
    }

    if (uScreenId == m_cShadowBuffers)
    {
        return;
    }

    *puScreenId = uScreenId;
    *pi16x -= (int16_t)m_paScreenOriginX[uScreenId];
    *pi16y -= (int16_t)m_paScreenOriginY[uScreenId];
}

/* virtual */ void VRDPClientDesktopMapMultiMon::Screen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        pRectResult->x = 0;
        pRectResult->y = 0;
        pRectResult->w = 0;
        pRectResult->h = 0;
        return;
    }

    pRectResult->x = pRectScreen->x + m_paScreenOriginX[uScreenId];
    pRectResult->y = pRectScreen->y + m_paScreenOriginY[uScreenId];
    pRectResult->w = pRectScreen->w;
    pRectResult->h = pRectScreen->h;
}

/* virtual */ void VRDPClientDesktopMapMultiMon::Screen2ClientPoint (unsigned uScreenId, int16_t *pi16x, int16_t *pi16y)
{
    Assert(IsScreenMatched(uScreenId));
    NOREF(uScreenId);

    *pi16x += (int16_t)m_paScreenOriginX[uScreenId];
    *pi16y += (int16_t)m_paScreenOriginY[uScreenId];
}

/* virtual */ bool VRDPClientDesktopMapMultiMon::ClipScreen2Client (unsigned uScreenId, const RGNRECT *pRectScreen, RGNRECT *pRectResult)
{
    if (!IsScreenMatched(uScreenId))
    {
        return false;
    }

    RGNRECT rectClient;
    Screen2Client (uScreenId, pRectScreen, &rectClient);

    if (!rgnIntersectRects(&rectClient, &rectClient, &m_paRectsShadowBuffer[uScreenId]))
    {
        return false;
    }

    *pRectResult = rectClient;
    return true;
}
