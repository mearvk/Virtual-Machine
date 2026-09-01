/* $Id: utcinfo.cpp 113820 2026-04-12 21:02:07Z vitali.pelenjow@oracle.com $ */
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

#include "utcinfo.h"
#include "vrdpclient.h"
#include "vrdpserv.h"
#include <iprt/utf16.h>

#define VRDP_DEBUG_UTCINFO
#ifdef VRDP_DEBUG_UTCINFO
#  define UTCINFOLOG(a) do { VRDPLOG(("UTCINFO::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define UTCINFOLOG(a)
#endif /* VRDP_DEBUG_UTCINFO */

typedef struct DVCCLOSEPARM
{
    uint8_t u8Channel;
    int32_t rc;
} DVCCLOSEPARM;

/*
 * The UTCINFO channel.
 */
#define FN_UTCINFO_SERVER_VERSION_INFO     0x00
#define FN_UTCINFO_CLIENT_VERSION_INFO     0x01
#define FN_UTCINFO_SET_COMPRESSION_RATE    0x02
#define FN_UTCINFO_HOTDESKING_NOTIFICATION 0x03
#define FN_UTCINFO_MONITOR_LAYOUT          0x04
#define FN_UTCINFO_CLIENT_STREAM_SIZE_LIMIT 0x05
#define FN_UTCINFO_CLIENT_NAME             0x06
#define FN_UTCINFO_CLIENT_INFO             0x07

/* Types of capabilities. */
#define T_UTCINFO_CAPSTYPE_GENERAL 0x0001
#define T_UTCINFO_CAPSTYPE_MONITOR 0x0002

/* Capabilities: UTCINFO_CAPSET_GENERAL::u32Capabilities */
#define CAP_UTCINFO_DESKTOP_RESIZE_SUPPORTED 0x00000001

/* Client Location Info */
#define LA_UTCINFO_CLIENT_INFO_NAME      0
#define LA_UTCINFO_CLIENT_INFO_IPADDR    1
#define LA_UTCINFO_CLIENT_INFO_LOCATION  2
#define LA_UTCINFO_CLIENT_INFO_OTHERINFO 3
#define LA_UTCINFO_CLIENT_INFO_LAST      3

#if defined(VRDP_DEBUG_UTCINFO) && defined(LOG_ENABLED)
static const char * const g_apszClientInfoNames[] =
{
    "Name",
    "IPaddr",
    "Location",
    "OtherInfo"
};
#endif

#pragma pack(1)
typedef struct UTCINFO_HDR
{
    uint8_t  u8FunctionId;
    uint32_t u32Length;
} UTCINFO_HDR;

typedef struct UTCINFO_CAPSET_HDR
{
    uint16_t u16CapabilitySetType;
    uint16_t u16CapabilityLength;
} UTCINFO_CAPSET_HDR;

typedef struct UTCINFO_CAPSET_GENERAL
{
    UTCINFO_CAPSET_HDR Header;
    uint32_t u32Capabilities;
} UTCINFO_CAPSET_GENERAL;

/* UTCINFO_CAPSET_MONITOR::u8MonitorCapabilities */
#define CAP_UTCINFO_MONITOR_CHANGE_SUPPORTED 0x01

typedef struct UTCINFO_CAPSET_MONITOR
{
    UTCINFO_CAPSET_HDR Header;
    uint8_t  u8MonitorCapabilities;
    uint16_t u16MaxMonitorsSupported;
} UTCINFO_CAPSET_MONITOR;

typedef struct UTCINFO_SERVER_VERSION_INFO
{
    UTCINFO_HDR Header;
    uint8_t  u8Version;
    uint16_t u16NumberCapabilities;
    /* UTCINFO_CAPSET_* follow. */
} UTCINFO_SERVER_VERSION_INFO;

typedef struct UTCINFO_CLIENT_VERSION_INFO
{
    UTCINFO_HDR Header;
    uint8_t  u8Version;
    uint16_t u16NumberCapabilities;
    /* UTCINFO_CAPSET_* follow. */
} UTCINFO_CLIENT_VERSION_INFO;

typedef struct UTCINFO_SET_COMPRESSION_RATE
{
    UTCINFO_HDR Header;
    uint8_t  au8PresentationId[16];
    uint32_t u32StreamId;
    uint8_t  u8CompressionRate;
} UTCINFO_SET_COMPRESSION_RATE;

/* UTCINFO_HOTDESKING_NOTIFICATION::u8EventType */
#define CLIENT_ATTACHED 0x00
#define CLIENT_DETACHED 0x01

typedef struct UTCINFO_HOTDESKING_NOTIFICATION
{
    UTCINFO_HDR Header;
    uint8_t  u8EventType;
} UTCINFO_HOTDESKING_NOTIFICATION;

typedef struct UTCINFO_MONITOR_LAYOUT
{
    UTCINFO_HDR Header;
    uint16_t  u16DesktopWidth;
    uint16_t  u16DesktopHeight;
    uint16_t  u16MonitorCount;
    /* TS_MONITOR_DEF structures follow. */
} UTCINFO_MONITOR_LAYOUT;

typedef struct UTCINFO_CLIENT_STREAM_SIZE_LIMIT
{
    UTCINFO_HDR Header;
    uint32_t u32ClientMaxArea;
    uint16_t u16ClientMinArea;
    uint16_t u16ClientMinWidth;
    uint16_t u16ClientMinHeight;
} UTCINFO_CLIENT_STREAM_SIZE_LIMIT;

typedef struct UTCINFO_CLIENT_NAME
{
    UTCINFO_HDR Header;
    uint16_t u16ClientNameLength;
    uint16_t au16ClientName[1]; /* With trailing nul. */
} UTCINFO_CLIENT_NAME;

typedef struct UTCINFO_CLIENT_INFO
{
    uint16_t u16ClientLocationInfoLength;
    uint16_t au16ClientLocationInfo[1]; /* With trailing nul. */
} UTCINFO_CLIENT_INFO;

#pragma pack()


UTCINFOClient::UTCINFOClient (VRDPClient *pClient)
    :
    m_pClient (pClient),
    m_u8ChannelId (0),
    m_u8ClientVersion (0)
{
    RT_ZERO(m_clientCaps);
}

UTCINFOClient::~UTCINFOClient ()
{
}

/** @todo base class DVCChannel for dvcInput and send context? */

/** @thread INPUT */
/* static */ void UTCINFOClient::dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    UTCINFOClient *pThis = (UTCINFOClient *)pvCtx;

    pThis->processInput(u32Event, pvData, cbData);
}

typedef struct SENDCONTEXT
{
    VRDPClient *pClient;
    bool       fOutputThread;
    uint8_t    u8ChannelId;
} SENDCONTEXT;

static void initSendContext(SENDCONTEXT *pSend,
                            bool fOutputThread,
                            VRDPClient *pClient,
                            uint8_t u8ChannelId)
{
    pSend->fOutputThread = fOutputThread;
    pSend->pClient       = pClient;
    pSend->u8ChannelId   = u8ChannelId;
}

static void sendPDU(SENDCONTEXT *pSend,
                    void *pvData,
                    uint32_t cbData)
{
    if (pSend->fOutputThread)
    {
        pSend->pClient->TP()->DVC()->SendData (pSend->u8ChannelId,
                                               pvData,
                                               cbData);
    }
    else
    {
        pSend->pClient->Server()->PostOutput (VRDP_OUTPUT_UTCINFO_DATA,
                                              pSend->pClient->Id (),
                                              pvData,
                                              cbData);
    }
}

/* Generic helper for closing a DVC channel from any context. */
void UTCINFOClient::DVCClose (int rc)
{
    DVCCLOSEPARM parm;

    parm.u8Channel = m_u8ChannelId;
    parm.rc = rc;

    m_pClient->Server()->PostOutput (VRDP_OUTPUT_DVC_CLOSE, m_pClient->Id (), &parm, sizeof (parm));
    m_pClient->Server()->RaiseOutputEvent ();
}


void UTCINFOClient::postOutput (int iEvent, const void *pvData, uint32_t cbData, bool fRaiseEvent)
{
    m_pClient->Server()->PostOutput (iEvent, m_pClient->Id (), pvData, cbData);

    if (fRaiseEvent)
    {
        m_pClient->Server()->RaiseOutputEvent ();
    }
}

/** @thread INPUT */
int UTCINFOClient::processClientVersionInfo(const void *pvData, uint32_t cbData)
{
    if (cbData < sizeof(UTCINFO_CLIENT_VERSION_INFO))
    {
        UTCINFOLOG(("cbData %d < UTCINFO_CLIENT_VERSION_INFO!!!\n",
                    cbData));
        return VERR_IO_BAD_LENGTH;
    }

    const UTCINFO_CLIENT_VERSION_INFO *pPDU = (UTCINFO_CLIENT_VERSION_INFO *)pvData;

    UTCINFOLOG(("UTCINFO_CLIENT_VERSION_INFO: 0x%x, ncaps 0x%x\n",
                pPDU->u8Version, pPDU->u16NumberCapabilities));

    if (pPDU->u8Version > UTCINFO_SERVER_VERSION)
    {
        /** @todo error reporting in UTCINFO? */
        DVCClose (VERR_NOT_SUPPORTED);
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    const uint8_t *pu8Cap = (uint8_t *)pPDU + sizeof(UTCINFO_CLIENT_VERSION_INFO);
    uint32_t cbCaps = cbData - sizeof(UTCINFO_CLIENT_VERSION_INFO);

    int i;
    for (i = 0; i < pPDU->u16NumberCapabilities; i++)
    {
        UTCINFO_CAPSET_HDR *pCapHdr = (UTCINFO_CAPSET_HDR *)pu8Cap;

        if (cbCaps < sizeof(UTCINFO_CAPSET_HDR))
        {
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        cbCaps -= sizeof(UTCINFO_CAPSET_HDR);

        if (cbCaps < pCapHdr->u16CapabilityLength)
        {
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        cbCaps -= pCapHdr->u16CapabilityLength;

        if (pCapHdr->u16CapabilitySetType == T_UTCINFO_CAPSTYPE_GENERAL)
        {
            const UTCINFO_CAPSET_GENERAL *pCap = (UTCINFO_CAPSET_GENERAL *)pCapHdr;
            m_clientCaps.general.u32Capabilities = pCap->u32Capabilities;
        }
        else if (pCapHdr->u16CapabilitySetType == T_UTCINFO_CAPSTYPE_MONITOR)
        {
            const UTCINFO_CAPSET_MONITOR *pCap = (UTCINFO_CAPSET_MONITOR *)pCapHdr;
            m_clientCaps.monitor.u8MonitorCapabilities = pCap->u8MonitorCapabilities;
            m_clientCaps.monitor.u16MaxMonitorsSupported = pCap->u16MaxMonitorsSupported;
        }
        else
        {
            /* Ignore unknown. */
            VRDPLOGREL(("UTCINFO: ignored client capability set %d\n",
                        pCapHdr->u16CapabilitySetType));
        }

        pu8Cap += pCapHdr->u16CapabilityLength + sizeof(UTCINFO_CAPSET_HDR);
    }

    if (RT_SUCCESS(rc))
    {
        m_u8ClientVersion = pPDU->u8Version;

        VRDPLOGREL(("UTCINFO client version %d, general capabilities 0x%08X\n",
                    pPDU->u8Version, m_clientCaps.general.u32Capabilities));
    }
    else
    {
        RT_ZERO(m_clientCaps);
    }

    return rc;
}

/** @thread INPUT */
int UTCINFOClient::processMonitorLayout(const void *pvData, uint32_t cbData)
{
    if (cbData < sizeof(UTCINFO_MONITOR_LAYOUT))
    {
        UTCINFOLOG(("cbData < UTCINFO_MONITOR_LAYOUT!!!\n"));
        return VERR_IO_BAD_LENGTH;
    }

    const UTCINFO_MONITOR_LAYOUT *pPDU = (UTCINFO_MONITOR_LAYOUT *)pvData;

    UTCINFOLOG(("UTCINFO_MONITOR_LAYOUT: %dx%d %d\n",
                pPDU->u16DesktopWidth, pPDU->u16DesktopHeight, pPDU->u16MonitorCount));

    if (cbData < sizeof(UTCINFO_MONITOR_LAYOUT) + pPDU->u16MonitorCount * sizeof(TS_MONITOR_DEF))
    {
        UTCINFOLOG(("cbData < UTCINFO_MONITOR_LAYOUT!!!\n"));
        return VERR_IO_BAD_LENGTH;
    }

    int rc = VINF_SUCCESS;

    VRDPData *pData = m_pClient->TP()->Data();

    /** @todo assuming that number of monitors has not changed. */
    uint32_t u32CurrentMonitorCount = 1;

    if (pData->MonitorLayout())
    {
        u32CurrentMonitorCount = pData->MonitorCount();
    }

    if (u32CurrentMonitorCount == pPDU->u16MonitorCount)
    {
        pData->DesktopWidth(pPDU->u16DesktopWidth);
        pData->DesktopHeight(pPDU->u16DesktopHeight);

        if (pData->MonitorLayout())
        {
            pData->MonitorLayout(true);
            pData->MonitorCount(pPDU->u16MonitorCount);

            const TS_MONITOR_DEF *pMonitor = (const TS_MONITOR_DEF *)&pPDU[1];

            uint32_t i;
            for (i = 0; i < pPDU->u16MonitorCount; i++, pMonitor++)
            {
                UTCINFOLOG(("UTCINFO_MONITOR_LAYOUT: [%02d] %d-%d, %d-%d, flags 0x%08X\n",
                           i,
                           pMonitor->i32Left,
                           pMonitor->i32Right,
                           pMonitor->i32Top,
                           pMonitor->i32Bottom,
                           pMonitor->u32Flags));

                pData->MonitorDef(i, pMonitor);
            }
        }
        else if (pPDU->u16MonitorCount > 0)
        {
            /* Single monitor. */
            const TS_MONITOR_DEF *pMonitor = (const TS_MONITOR_DEF *)&pPDU[1];
            /* Do nothing for now. */
            RT_NOREF_PV(pMonitor);
        }

        m_pClient->TP()->DesktopMap()->Reinitialize();
        m_pClient->TP()->DesktopMap()->OnResolutionChanged();
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

/** @thread INPUT */
void UTCINFOClient::processInput(uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    UTCINFOLOG(("u32Event %d, pvData %p, cbData %d\n", u32Event, pvData, cbData));

    int rc = VINF_SUCCESS;

    SENDCONTEXT sendContext;
    bool fOutputThread = false;

    initSendContext(&sendContext, fOutputThread, m_pClient, m_u8ChannelId);

    switch (u32Event)
    {
        case DVC_EVENT_CREATE:
        {
            /* Client accepted the channel. Server must send the version PDU. */
#pragma pack(1)
            struct VRDP_UTCINFO_SERVER_VERSION_INFO
            {
                UTCINFO_SERVER_VERSION_INFO info;
                UTCINFO_CAPSET_GENERAL capsGeneral;
                UTCINFO_CAPSET_MONITOR capsMonitor;
            } pdu;
#pragma pack()

            pdu.info.Header.u8FunctionId = FN_UTCINFO_SERVER_VERSION_INFO;
            pdu.info.Header.u32Length    = sizeof(pdu);

            pdu.info.u8Version             = 2;
            pdu.info.u16NumberCapabilities = 2;

            pdu.capsGeneral.Header.u16CapabilitySetType = T_UTCINFO_CAPSTYPE_GENERAL;
            pdu.capsGeneral.Header.u16CapabilityLength  = sizeof(UTCINFO_CAPSET_GENERAL) - sizeof(UTCINFO_CAPSET_HDR);
            pdu.capsGeneral.u32Capabilities = CAP_UTCINFO_DESKTOP_RESIZE_SUPPORTED;

            pdu.capsMonitor.Header.u16CapabilitySetType = T_UTCINFO_CAPSTYPE_MONITOR;
            pdu.capsMonitor.Header.u16CapabilityLength  = sizeof(UTCINFO_CAPSET_MONITOR) - sizeof(UTCINFO_CAPSET_HDR);
            pdu.capsMonitor.u8MonitorCapabilities = 0;
            /* The screen count is set when the server starts, so always valid here. */
            pdu.capsMonitor.u16MaxMonitorsSupported = shadowBufferQueryScreenCount();

            sendPDU (&sendContext, &pdu, sizeof (pdu));

            VRDPLOGREL(("UTCINFO enabled\n"));
        } break;

        case DVC_EVENT_DATA:
        {
#ifdef DEBUG_sunlover
            UTCINFOLOG(("UTCINFO dvc data:\n"));
            hexdump((uint8_t *)pvData, cbData);
#endif
            if (cbData < sizeof(UTCINFO_HDR))
            {
                UTCINFOLOG(("cbData < UTCINFO_HDR!!!\n"));
                break;
            }

            const UTCINFO_HDR *pHdr = (UTCINFO_HDR *)pvData;

            UTCINFOLOG(("received u8FunctionId %u, length %u\n", pHdr->u8FunctionId, pHdr->u32Length));

            if (cbData < pHdr->u32Length)
            {
                UTCINFOLOG(("cbData < pHdr->u32Length!!!\n"));
                break;
            }

            switch (pHdr->u8FunctionId)
            {

                case FN_UTCINFO_CLIENT_VERSION_INFO:
                {
                    rc = processClientVersionInfo(pvData, cbData);
                } break;

                case FN_UTCINFO_SET_COMPRESSION_RATE:
                {
                    if (cbData < sizeof(UTCINFO_SET_COMPRESSION_RATE))
                    {
                        UTCINFOLOG(("cbData < UTCINFO_SET_COMPRESSION_RATE!!!\n"));
                        break;
                    }

                    const UTCINFO_SET_COMPRESSION_RATE *pPDU = (UTCINFO_SET_COMPRESSION_RATE *)pvData;

                    UTCINFOLOG(("UTCINFO_SET_COMPRESSION_RATE: id=%#x rate=%#x\n", pPDU->u32StreamId, pPDU->u8CompressionRate));

                    postOutput(VRDP_OUTPUT_VIDEO_COMPRESSION_RATE,
                               &pPDU->u8CompressionRate,
                               sizeof (pPDU->u8CompressionRate),
                               false);
                } break;

                case FN_UTCINFO_HOTDESKING_NOTIFICATION:
                {
                    if (cbData < sizeof(UTCINFO_HOTDESKING_NOTIFICATION))
                    {
                        UTCINFOLOG(("cbData < UTCINFO_HOTDESKING_NOTIFICATION!!!\n"));
                        break;
                    }

                    const UTCINFO_HOTDESKING_NOTIFICATION *pPDU = (UTCINFO_HOTDESKING_NOTIFICATION *)pvData;

                    UTCINFOLOG(("UTCINFO_HOTDESKING_NOTIFICATION: 0x%x\n", pPDU->u8EventType));

                    if (pPDU->u8EventType == CLIENT_DETACHED)
                    {
                        postOutput(VRDP_OUTPUT_VIDEO_STREAM_DISABLE,
                                   NULL,
                                   0,
                                   true);
                        m_pClient->Server()->ClientAttach(m_pClient, false);
                    }
                    else if (pPDU->u8EventType == CLIENT_ATTACHED)
                    {
                        postOutput(VRDP_OUTPUT_VIDEO_STREAM_ENABLE,
                                   NULL,
                                   0,
                                   true);
                        m_pClient->Server()->ClientAttach(m_pClient, true);
                    }
                } break;

                case FN_UTCINFO_MONITOR_LAYOUT:
                {
                    rc = processMonitorLayout(pvData, cbData);
                } break;

                case FN_UTCINFO_CLIENT_STREAM_SIZE_LIMIT:
                {
                    if (cbData < sizeof(UTCINFO_CLIENT_STREAM_SIZE_LIMIT))
                    {
                        UTCINFOLOG(("cbData < UTCINFO_CLIENT_STREAM_SIZE_LIMIT!!!\n"));
                        break;
                    }

                    const UTCINFO_CLIENT_STREAM_SIZE_LIMIT *pPDU = (UTCINFO_CLIENT_STREAM_SIZE_LIMIT *)pvData;

                    UTCINFOLOG(("UTCINFO_CLIENT_STREAM_SIZE_LIMIT: %d, %d, %dx%d\n",
                                pPDU->u32ClientMaxArea, pPDU->u16ClientMinArea, pPDU->u16ClientMinWidth, pPDU->u16ClientMinHeight));

                    VHEVENTSIZELIMIT data;
                    data.u32ClientMaxArea   = pPDU->u32ClientMaxArea;
                    data.u16ClientMinArea   = pPDU->u16ClientMinArea;
                    data.u16ClientMinWidth  = pPDU->u16ClientMinWidth;
                    data.u16ClientMinHeight = pPDU->u16ClientMinHeight;

                    postOutput(VRDP_OUTPUT_VIDEO_STREAM_SIZE_LIMIT,
                               &data,
                               sizeof(data),
                               true);
                } break;

                case FN_UTCINFO_CLIENT_NAME:
                {
                    if (cbData < sizeof(UTCINFO_CLIENT_NAME))
                    {
                        UTCINFOLOG(("cbData < UTCINFO_CLIENT_NAME!!!\n"));
                        break;
                    }

                    const UTCINFO_CLIENT_NAME *pPDU = (UTCINFO_CLIENT_NAME *)pvData;

                    UTCINFOLOG(("UTCINFO_CLIENT_NAME: %d bytes\n",
                                pPDU->u16ClientNameLength));

                    if (cbData < RT_UOFFSETOF(UTCINFO_CLIENT_NAME, au16ClientName) + pPDU->u16ClientNameLength)
                    {
                        UTCINFOLOG(("cbData < UTCINFO_CLIENT_NAME + name!!!\n"));
                        break;
                    }

                    /* Verify the name:
                     * at least 2 bytes for nul terminator, length is a multiple of uint16_t, last char is nul.
                     */
                    if (   pPDU->u16ClientNameLength < 2
                        || pPDU->u16ClientNameLength % 2 != 0
                        || pPDU->au16ClientName[pPDU->u16ClientNameLength / 2 - 1] != 0)
                    {
                        break;
                    }

                    /* Convert to UTF8. */
                    char *pszClientName = NULL;
                    AssertCompile(sizeof(pPDU->au16ClientName[0]) == sizeof(RTUTF16));
                    rc = RTUtf16ToUtf8((PCRTUTF16)pPDU->au16ClientName, &pszClientName);

                    if (RT_SUCCESS(rc))
                    {
                        m_pClient->TP()->setClientName(pszClientName);
                        m_pClient->Server()->ClientLocationInfo(m_pClient,
                                                                pszClientName,
                                                                NULL,
                                                                NULL,
                                                                NULL);
                        RTStrFree(pszClientName);
                    }
                } break;

                case FN_UTCINFO_CLIENT_INFO:
                {
                    uint32_t cbLeft = cbData;

                    if (cbLeft < sizeof(UTCINFO_HDR) + sizeof(UTCINFO_CLIENT_INFO) * (LA_UTCINFO_CLIENT_INFO_LAST + 1))
                    {
                        UTCINFOLOG(("cbData < LA minimum expected length!!!\n"));
                        break;
                    }

                    const uint8_t *pu8Info = (const uint8_t *)pvData;

                    pu8Info += sizeof(UTCINFO_HDR);
                    cbLeft -= sizeof(UTCINFO_HDR);

                    char *apszClientLocationInfo[LA_UTCINFO_CLIENT_INFO_LAST + 1];
                    memset(apszClientLocationInfo, 0, sizeof(apszClientLocationInfo));

                    unsigned int idx;
                    for (idx = 0; idx <= LA_UTCINFO_CLIENT_INFO_LAST; idx++)
                    {
                        uint32_t cbInfo = RT_UOFFSETOF(UTCINFO_CLIENT_INFO, au16ClientLocationInfo);

                        if (cbLeft < cbInfo)
                        {
                            UTCINFOLOG(("cbLeft < UTCINFO_CLIENT_INFO[%s]!!!\n", g_apszClientInfoNames[idx]));
                            rc = VERR_IO_BAD_LENGTH;
                            break;
                        }

                        const UTCINFO_CLIENT_INFO *pClientLocationInfo = (UTCINFO_CLIENT_INFO *)pu8Info;

                        UTCINFOLOG(("UTCINFO_CLIENT_INFO [%s]: %d bytes\n",
                                    g_apszClientInfoNames[idx],
                                    pClientLocationInfo->u16ClientLocationInfoLength));

                        cbInfo += pClientLocationInfo->u16ClientLocationInfoLength;

                        if (cbLeft < cbInfo)
                        {
                            UTCINFOLOG(("cbLeft < UTCINFO_CLIENT_INFO + Info[%s]!!!\n", g_apszClientInfoNames[idx]));
                            rc = VERR_IO_BAD_LENGTH;
                            break;
                        }

                        /* Verify the client location info:
                         * at least 2 bytes for nul terminator, length is a multiple of uint16_t, last char is nul.
                         */
                        if (   pClientLocationInfo->u16ClientLocationInfoLength < 2
                            || pClientLocationInfo->u16ClientLocationInfoLength % 2 != 0
                            || pClientLocationInfo->au16ClientLocationInfo[pClientLocationInfo->u16ClientLocationInfoLength / 2 - 1] != 0)
                        {
                            UTCINFOLOG(("UTCINFO::ClientInfo failed at %s verification\n", g_apszClientInfoNames[idx]));
                            rc = VERR_IO_BAD_LENGTH;
                            break;
                        }

                        /* Convert to UTF8. */
                        AssertCompile(sizeof(pClientLocationInfo->au16ClientLocationInfo[0]) == sizeof(RTUTF16));
                        rc = RTUtf16ToUtf8((PCRTUTF16)pClientLocationInfo->au16ClientLocationInfo, &apszClientLocationInfo[idx]);

                        if (RT_FAILURE(rc))
                        {
                            UTCINFOLOG(("UTCINFO::ClientInfo failed at %s UTF8 conversion %Rrc\n",
                                        g_apszClientInfoNames[idx], rc));
                            break;
                        }

                        UTCINFOLOG(("UTCINFO::ClientInfo [%s] = [%s]\n",
                                    g_apszClientInfoNames[idx], apszClientLocationInfo[idx]));

                        cbLeft -= cbInfo;
                        pu8Info += cbInfo;

                    }

                    if (RT_SUCCESS(rc))
                    {
                        m_pClient->TP()->setClientName(apszClientLocationInfo[LA_UTCINFO_CLIENT_INFO_NAME]);
                        m_pClient->Server()->ClientLocationInfo(m_pClient,
                                                                apszClientLocationInfo[LA_UTCINFO_CLIENT_INFO_NAME],
                                                                apszClientLocationInfo[LA_UTCINFO_CLIENT_INFO_IPADDR],
                                                                apszClientLocationInfo[LA_UTCINFO_CLIENT_INFO_LOCATION],
                                                                apszClientLocationInfo[LA_UTCINFO_CLIENT_INFO_OTHERINFO]);
                    }

                    for (idx = 0; idx <= LA_UTCINFO_CLIENT_INFO_LAST; idx++)
                    {
                        if (apszClientLocationInfo[idx])
                        {
                            RTStrFree(apszClientLocationInfo[idx]);
                        }
                    }

                    break;
                }

                default:
                {
                    UTCINFOLOG(("wrong MessageId!!!\n"));
                    rc = VERR_INVALID_FUNCTION;
                } break;
            }
        } break;

        case DVC_EVENT_CLOSE:
        {
        } break;

        default:
            AssertFailed();
    }

    if (RT_FAILURE(rc))
    {
        VRDPLOGRELLIMIT(32, ("VRDP: UTCINFO: input %Rrc!!!\n", rc));
    }
}


int UTCINFOClient::RegisterChannel(void)
{
    int rc = m_pClient->TP()->DVC ()->RegisterChannel("UTCINFO",
                                                      dvcInput,
                                                      this,
                                                      &m_u8ChannelId);

    return rc;
}

/** @thread OUTPUT */
void UTCINFOClient::SendData(void *pvData, uint32_t cbData)
{
    m_pClient->TP()->DVC()->SendData (m_u8ChannelId, pvData, cbData);
}


#ifdef DEBUG_sunlover
void UTCINFOClient::testMonitor(void)
{
#pragma pack(1)
    struct
    {
        UTCINFO_MONITOR_LAYOUT l;
        TS_MONITOR_DEF d[2];
    } m;
#pragma pack()

    m.l.Header.u8FunctionId = FN_UTCINFO_MONITOR_LAYOUT;
    m.l.Header.u32Length = sizeof(m);

    m.l.u16DesktopWidth = 800 + 800;
    m.l.u16DesktopHeight = 600;
    m.l.u16MonitorCount = 2;

    m.d[0].i32Left = 0;
    m.d[0].i32Top = 0;
    m.d[0].i32Right = 799;
    m.d[0].i32Bottom = 599;
    m.d[0].u32Flags = TS_MONITOR_PRIMARY;

    m.d[1].i32Left = 800;
    m.d[1].i32Top = 0;
    m.d[1].i32Right = 800 + 799;
    m.d[1].i32Bottom = 599;
    m.d[1].u32Flags = 0;


    processMonitorLayout(&m, sizeof (m));
}

void UTCINFOClient::testHotdesking(void)
{
    static int i = 0;
    if (i == 0)
    {
        postOutput(VRDP_OUTPUT_VIDEO_STREAM_DISABLE,
                   NULL,
                   0,
                   true);
        m_pClient->Server()->ClientAttach(m_pClient, false);
        i = 1;
    }
    else if (i == 1)
    {
        postOutput(VRDP_OUTPUT_VIDEO_STREAM_ENABLE,
                   NULL,
                   0,
                   true);
        m_pClient->Server()->ClientAttach(m_pClient, true);
        i = 2;
    }
    else if (i == 2)
    {
        shadowBufferVideoDetectorCmd(VD_CMD_RESET);
        shadowBufferVideoDetectorCmd(VD_CMD_RESET);
        shadowBufferVideoDetectorCmd(VD_CMD_RESET);
        shadowBufferVideoDetectorCmd(VD_CMD_RESET);
        i = 0;
    }
}

void UTCINFOClient::testLimits(void)
{
    static int i = 0;

    VHEVENTSIZELIMIT data;

    data.u16ClientMinArea   = 0;
    data.u16ClientMinWidth  = 0;
    data.u16ClientMinHeight = 0;

    if (i == 0)
    {
        data.u32ClientMaxArea = 32*32;
    }
    else if (i == 1)
    {
        data.u32ClientMaxArea = 100*100;
    }
    else
    {
        data.u32ClientMaxArea = 1000*1000;
    }

    i = (i + 1) % 3;

    postOutput(VRDP_OUTPUT_VIDEO_STREAM_SIZE_LIMIT,
               &data,
               sizeof(data),
               true);
}

void UTCINFOClient::testNameChange(void)
{
    static int i = 0;

    const char *pu8ClientName;

    if (i == 0)
    {
        pu8ClientName = "clientA";
    }
    else if (i == 1)
    {
        pu8ClientName = "clientB";
    }
    else
    {
        pu8ClientName = "clientC";
    }

    i = (i + 1) % 3;

    m_pClient->Server()->ClientLocationInfo(m_pClient,
                                            pu8ClientName,
                                            NULL,
                                            NULL,
                                            NULL);
}
#endif /* DEBUG_sunlover */
