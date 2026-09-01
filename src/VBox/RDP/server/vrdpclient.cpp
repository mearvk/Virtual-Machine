/* $Id: vrdpclient.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "vrdpclient.h"
#include "vrdpserv.h"
#include "bmpcomp.h"

#include <iprt/crc.h>

#define VRDP_CLIENT_DISCONNECT 0x1
#define VRDP_CLIENT_RECONNECT  0x2

static void orderDataDelete(VRDPCLIENTORDERDATA *p);

VRDPClient::VRDPClient (VRDPServer *pServer, uint32_t u32ClientId)
    :
    m_pServer (pServer),
    m_vrdptp (pServer, this),
    m_fConnected (false),
    m_Stream (&m_vrdptp),
    m_TransportId (NIL_VRDPTRANSPORTID),
    m_u32ClientId (u32ClientId),
    m_fDemandActiveInProcess (true),
    m_u32PendingResize (0),
    m_fu32Intercepted (0),
    m_u32EventFlags (0),
    m_fu32SupportedOrders (0),
    m_TSMFRaw (this),
    m_ai (this),
    m_UTCINFO (this),
    m_VideoIn(this),
    m_Input(this)
{
    SERVERLOG(("Client created %p, id %d\n", this, u32ClientId));
    Assert (m_pServer->IsInputThread ());

    /** @todo initialization function, called in the output thread. */
    memset (&m_TextFragmentCache, 0, sizeof (m_TextFragmentCache));

    m_fVideoChannelActive = false;
    m_pVideoChannelNull = new VideoChannelNull(this, videoChannelEvent, this);
    VideoChannelTSMFCreate(&m_pVideoChannelTSMF, this, videoChannelEvent, this);
    VideoChannelSunFlshCreate(&m_pVideoChannelSunFlsh, this, videoChannelEvent, this);
    m_pVideoChannel = m_pVideoChannelNull;
    m_achLastVideoChannelName[0] = 0;

    RT_ZERO(m_redraw);
    RT_ZERO(m_orderData);

    m_fDisableDisplay = false;
    m_fDisableInput = false;
    m_fDisableAudio = false;
    m_fDisableUSB = false;
    m_fDisableClipboard = false;
    m_fDisableRDPDR = false;

    m_InputCtx.SetData(m_vrdptp.Data());
    m_Stream.SetData(m_vrdptp.Data());
#ifdef VRDP_BMP_STAT
    VRDPBmpStatReset();
#endif /* VRDP_BMP_STAT */
}

VRDPClient::~VRDPClient ()
{
#ifdef VRDP_BMP_STAT
    VRDPBmpStatLogRel();
#endif /* VRDP_BMP_STAT */

    SERVERLOG(("Client deleted %p, id %d\n", this, m_u32ClientId));
    orderDataDelete(&m_orderData);

    delete m_pVideoChannelNull;
    delete m_pVideoChannelSunFlsh;
    delete m_pVideoChannelTSMF;
}

/*
 * VM thread. Called when the RDP connectio must be dropped.
 */
void VRDPClient::NotifyDisconnect(bool fReconnect)
{
    SERVERLOG(("fReconnect %d\n", fReconnect));

    ASMAtomicOrU32(&m_u32EventFlags, fReconnect?
                                        (VRDP_CLIENT_DISCONNECT | VRDP_CLIENT_RECONNECT):
                                        VRDP_CLIENT_DISCONNECT);
}

/*
 * INPUT thread. Called when a new transport connection is established.
 */
int VRDPClient::Start (VRDPTransport *pTransport, VRDPTRANSPORTID id)
{
    Assert (m_pServer->IsInputThread ());

    /* Remember the transport id of the client. */
    m_TransportId = id;

    /* Set the state variables to start the RDP connection establishing. */
    m_InputCtx.SetTransport (pTransport, id);
    Assert (m_InputCtx.IsReset ());

    m_Stream.SetTransport (pTransport, id);
    m_Stream.InitOutgoingChannelId (MCS_GLOBAL_CHANNEL);
    Assert(m_Stream.IsReset ());

    m_vrdptp.DVC()->SetTransport (pTransport, id);
    m_vrdptp.SunFlsh()->SetTransport (pTransport, id);
    m_vrdptp.USB()->SetTransport (pTransport, id);
    m_vrdptp.Audio()->SetTransport (pTransport, id);
    m_vrdptp.Clipboard()->SetTransport (pTransport, id);
    m_vrdptp.RDPDR()->SetTransport (pTransport, id);

    /* The channel id is set by vrdptp, when channels are registered. */

    return VINF_SUCCESS;
}

/*
 * INPUT thread. Called when RDP connection has been established.
 */
void VRDPClient::NotifyConnected (void)
{
    m_fConnected = true;

    m_pServer->ClientConnect (this);
}

static int disableStatus (VRDPClientUseStatus *pStatus, int iTimeout)
{
    int iMillisec = 0;
    for (;;)
    {
        bool fStatusChanged = pStatus->ClientUseStateFree2Disabled ();

        if (fStatusChanged || pStatus->ClientUseStateCheckDisabled ())
        {
            SERVERLOG(("ThreadContextStatus disabled after %d ms\n", iMillisec));
            break;
        }

        VRDPThreadSleep (1);
        iMillisec++;

        if (iMillisec > iTimeout)
        {
            /* Something is wrong. But still return. */
            AssertFailed ();
            VRDPLOGREL(("DisableStatus timed out %d!!!\n", iTimeout));
            return VERR_TIMEOUT;
        }
    }

    return VINF_SUCCESS;
}

int VRDPClient::ThreadContextDisableBoth (void)
{
    VRDP_ASSERT_CONTEXT_INPUT(Server ());

    SERVERLOG(("%p id %d\n", this, m_u32ClientId));

    int rc = disableStatus (StatusVM (), 10000);
    if (RT_SUCCESS(rc))
    {
        rc = disableStatus (StatusOutput (), 10000);
    }
    return rc;
}

int VRDPClient::NotifyDelete(void)
{
    VRDP_ASSERT_CONTEXT_INPUT(Server ());

    SERVERLOG(("NotifyDelete %p id %d\n", this, m_u32ClientId));

    int rc = ThreadContextDisableBoth ();

    /* Do cleanup. */

    TP()->Reset();

    pointerCacheDestroy ();

    if (m_fConnected)
    {
        m_pServer->ClientDisconnect (this);
        m_fConnected = false;
    }
    if (m_fVideoChannelActive)
    {
        VIDEOLOG(("Notification about client disconnect\n"));
        m_fVideoChannelActive = false;
        VHCONTEXT *pCtx = Server()->VideoHandler();
        Assert(pCtx);
        if (pCtx)
        {
            videoHandlerNotifyClientDisconnect(pCtx);
        }
    }
    m_pVideoChannel = m_pVideoChannelNull;

    return rc;
}

void VRDPClient::setupMultiMonitor(void)
{
    /* Check if the client works in multimonitor mode. */
    if (IsMultiMonitor())
    {
        m_InputCtx.Reset();
        TP()->SendMonitorLayout (&m_InputCtx);
    }
}

/*
 * INPUT thread. Called when a transport connection has an event for the client.
 */
int VRDPClient::ProcessIncoming(uint32_t fu32Events)
{
    VRDP_ASSERT_CONTEXT_INPUT(Server());

    int rc = VINF_SUCCESS;

    uint32_t u32EventFlags = ASMAtomicXchgU32(&m_u32EventFlags, 0);

    if (u32EventFlags & VRDP_CLIENT_RECONNECT)
    {
        SERVERLOG(("VRDP_CLIENT_RECONNECT\n"));

        /* Send the reconnect packet. */
        m_InputCtx.Reset();
        m_vrdptp.Reconnect(&m_InputCtx);
    }

    if (u32EventFlags & VRDP_CLIENT_DISCONNECT)
    {
        SERVERLOG(("VRDP_CLIENT_DISCONNECT\n"));

        /* The client must not be accessed anymore. */
        ThreadContextDisableBoth();

        /* Let the lower protocol to disconnect properly. */
        m_InputCtx.Reset();
        m_vrdptp.Disconnect(&m_InputCtx, exDiscReasonAPIInitiatedDisconnect);

        /* Disconnect the client */
        return VINF_VRDP_OPERATION_COMPLETED;
    }

    if (fu32Events & VRDP_TRANSPORT_DATA_AVAILABLE)
    {
        /* The ISO/RDP5 packet is in the m_InputCtx. Parse it.
         * Always process the input packet, because the state
         * of levels (SEC especially) must follow the one of the client.
         */
        rc = m_vrdptp.Recv (&m_InputCtx);
        SERVERLOG(("m_vrdptp.Recv rc = %Rrc\n", rc));

        if (RT_FAILURE (rc))
        {
            VRDPLOGREL(("Failed to process incoming RDP packet: %Rrc!!!\n", rc));
            m_InputCtx.DumpReadRel ();
            return rc;
        }

        if (rc == VINF_VRDP_OPERATION_COMPLETED)
        {
            SERVERLOG(("Server closes the connection.\n"));
            return rc;
        }
    }

#ifdef DEBUG_sunlover
    static uint64_t u64Start = 0;
    if (u64Start == 0)
    {
        u64Start = VRDPTimeMilliTS();
    }
    else if (VRDPTimeMilliTS() - u64Start > 30000)
    {
        // m_UTCINFO.testMonitor();
        // m_UTCINFO.testHotdesking();
        // m_UTCINFO.testLimits();
        // m_UTCINFO.testNameChange();
        u64Start = VRDPTimeMilliTS();
    }
#endif
    /* Check if no resize is in process and a resize is pending. */
    if (   !m_fDemandActiveInProcess
        && ASMAtomicCmpXchgU32(&m_u32PendingResize, 0 /* new */, 1 /* old */))
    {
        RESIZELOG(("initiating resize.\n"));

        if (IsClientResizable ())
        {
            m_fDemandActiveInProcess = true;

            /* Notify about a possible client resolution change. */
            m_vrdptp.DesktopMap()->Reinitialize();

            RESIZELOG(("sending demand active.\n"));
            m_InputCtx.Reset ();
            m_vrdptp.SendDemandActive (&m_InputCtx);
            RESIZELOG(("demand active sent.\n"));
        }
        else
        {
            /* There is no demand active seq. */

            /* Inform the VRDP level that the shadow buffer size changed. */
            m_vrdptp.DesktopMap()->OnResolutionChanged();

            AddRedraw (NULL, false, 0);

            /* The client state must be set to Free for VM and Output. */
            bool fStatusChanged = m_StatusVM.ClientUseStateDisabled2Free ();
            Assert(fStatusChanged);
            fStatusChanged = m_StatusOutput.ClientUseStateDisabled2Free ();
            Assert(fStatusChanged);
            NOREF(fStatusChanged);

            /* Restart direct video streams. */
            m_pServer->PostOutputEvent(VRDP_OUTPUT_VIDEO_STREAM_RESTART, 0, NULL, 0);
        }
    }

    if (rc == VWRN_VRDP_PDU_NOT_SUPPORTED)
    {
        /* Ignore the PDU. */
        return VINF_SUCCESS;
    }

    if (rc == VINF_VRDP_OUTPUT_ENABLE)
    {
       /* The client has finished initial RDP initialization or a demand active sequence.
        * It is time to enable output updates:
        *   - reset the output stream for the client,
        *     and therefore let the stream start with a full
        *     screen update;
        *   - set the proper client state (updates enabled)
        *     so the output thread could pickup the client
        *     and inject updates.
        * The input thread
        *  (note for later) Later when the output must be disabled,
        *  the client state is set to "OutputDisableRequested" and
        *  the output event is posted. Then the input thread waits
        *
        */

        SERVERLOG(("VINF_VRDP_OUTPUT_ENABLE\n"));

        m_fu32SupportedOrders = TP()->GetSupportedOrders ();
        SERVERLOG(("m_fu32SupportedOrders: 0x%08X\n", m_fu32SupportedOrders));

        /* Delete obsolete updates. */
        m_Stream.ResetStream ();

        setupMultiMonitor();

        /* Inform the VRDP level that the shadow buffer size changed. */
        m_vrdptp.DesktopMap()->OnResolutionChanged();

        bitmapCacheSetup ();

        fontCacheSetup ();
        brushCacheSetup ();

        pointerCacheSetup ();

        m_vrdptp.Audio ()->Start ();
        m_vrdptp.USB ()->Start ();
        m_vrdptp.Clipboard ()->Start ();

        m_pVideoChannelNull->VideoChannelSetup ();

        m_pVideoChannelSunFlsh->VideoChannelSetup ();
        m_vrdptp.SunFlsh ()->RegisterCallback(VRDPClient::sunFlsh, this);
        m_vrdptp.SunFlsh ()->Start ();

        m_pVideoChannelTSMF->VideoChannelSetup ();

        m_UTCINFO.RegisterChannel();
        m_VideoIn.RegisterChannel();
        m_Input.RegisterChannel();
        m_vrdptp.DVC ()->Start ();

        m_vrdptp.RDPDR ()->Start ();

        /* The Output context resets automatically after sending a packet. */
        Assert (m_Stream.IsReset ());

        RESIZELOG(("marking demand active as completed.\n"));

        m_fDemandActiveInProcess = false;

        /* The client state must be set to Free for VM and Output. */
        bool fStatusChanged = m_StatusVM.ClientUseStateDisabled2Free ();
        Assert(fStatusChanged);
        fStatusChanged = m_StatusOutput.ClientUseStateDisabled2Free ();
        Assert(fStatusChanged);
        NOREF(fStatusChanged);

        /* Now updates can be posted. All updates now should go to the output thread.
         * A palette, a full screen update and current mouse pointer shape must be send to the client.
         */

        /* For 256 color client send a palette. */
        if (m_vrdptp.QueryClientBpp () == 8)
        {
            m_pServer->PostOutput (VRDP_OUTPUT_PALETTE_8_BPP, Id (), NULL, 0);
        }

        /* And the current pointer update. */
        uint32_t u32ServerId = VRDP_POINTER_SERVER_ID_NULL;
        m_pServer->PostOutput (VRDP_OUTPUT_CACHED_POINTER, Id (), &u32ServerId, sizeof (u32ServerId));

        /* Post to output thread the fullscreen update. */
        AddRedraw (NULL, false, 0);

        /* Restart direct video streams. */
        m_pServer->PostOutputEvent(VRDP_OUTPUT_VIDEO_STREAM_RESTART, 0, NULL, 0);

        /* From now the client mostly is used by output thread and the m_Stream can not be used. */
        return VINF_SUCCESS;
    }

    Assert(rc == VINF_SUCCESS);

    return VINF_SUCCESS;
}

/*
 * VM/OUTPUT thread. Changes the clients 'Status' to 'free'.
 */
void VRDPClient::ThreadContextRelease (int iContext)
{
    VRDPClientUseStatus *pStatus = NULL;

    switch (iContext)
    {
        case VRDP_CONTEXT_VM: pStatus = StatusVM (); break;
        case VRDP_CONTEXT_OUTPUT: pStatus = StatusOutput (); break;
        default: break;
    }

    if (!pStatus)
    {
        AssertReleaseFailed ();
    }
    else
    {
        if (!pStatus->ClientUseStateUse2Free ())
        {
            VRDPLOGREL(("Failed to release the client. Status 0x%x, Context 0x%x!!!\n", pStatus->Status (), iContext));
            AssertReleaseFailed ();
        }
    }

    return;
}

/** @thread OUTPUT */
void VRDPClient::NotifyResizeAndThreadContextDisableBoth (void)
{
    VRDP_ASSERT_CONTEXT_OUTPUT(Server ());

    /* The client is in use by OUTPUT context here. Chnage the state to Disabled. */
    bool fStatusChanged = StatusOutput()->ClientUseStateUse2Disabled ();
    Assert(fStatusChanged);

    /* Also disable the VM context, the context uses client only for very short periods,
     * so, use active wait.
     */
    int iMillisec = 0;
    for (;;)
    {
        fStatusChanged = StatusVM()->ClientUseStateFree2Disabled ();

        if (fStatusChanged || StatusVM()->ClientUseStateCheckDisabled ())
        {
            SERVERLOG(("On resize ThreadContextStatus disabled after %d ms\n", iMillisec));
            break;
        }

        VRDPThreadSleep (1);
        iMillisec++;

        if (iMillisec > 1000)
        {
            /* Something is seriously wrong. */
            AssertFailed ();
        }
    }

    /* Inform the INPUT thread that the client is ready to be resized.
     * Atomically set the variable.
     */
    ASMAtomicXchgU32(&m_u32PendingResize, 1);
}

bool VRDPClient::IsClientResizable (void)
{
    return !m_vrdptp.IsClientResolution ();
}

int VRDPClient::OutputBitmap2 (unsigned uScreenId, VRDPBitmapCompressed *pBmp, uint16_t x, uint16_t y)
{
    if (m_fDisableDisplay)
    {
        return VERR_NOT_SUPPORTED;
    }

    return m_vrdptp.OutputBitmap2 (uScreenId, Stream (), pBmp, x, y);
}

int VRDPClient::OutputOrder (unsigned uScreenId, int32_t i32Op, void *pvOrder, uint32_t cbOrder)
{
    if (m_fDisableDisplay)
    {
        return VERR_NOT_SUPPORTED;
    }

    if (   i32Op == VRDE_ORDER_PATBLTBRUSH
        && m_vrdptp.BrushSupportLevel() >= RDP_CAP_BRUSH_COLOR_8x8)
    {
        /* The client supports the brush cache. Generate a
         * BRUSH_CACHE secondary order and then a VRDE_ORDER_PATBLTBRUSH
         * which will use cache index.
         *
         * This is also a workaround for Windows7 RDP client bug.
         */
        VRDEORDERPATBLTBRUSH *pOrder = (VRDEORDERPATBLTBRUSH *)pvOrder;
        int iCache = -1;

        /* The cache is small. Use simple linear search. */
        BRUSHCACHEELEM *pElem;
        unsigned i;

        for (i = 0, pElem = &m_BrushCache.aBrushes[0]; i < RT_ELEMENTS(m_BrushCache.aBrushes); i++, pElem++)
        {
            if (   pElem->fCached
                && memcmp(pOrder->pattern, pElem->au8Pattern, sizeof (pElem->au8Pattern)) == 0)
            {
                iCache = i;
                TESTLOG(("BRUSHCACHE: found %d\n", iCache));
                break;
            }
        }

        if (iCache == -1)
        {
            /* Not yet cached. Send to client. */
            bool  fCached = m_vrdptp.OutputBrushCache (Stream (),
                                                       (uint8_t)m_BrushCache.iLastBrush,
                                                       8, /* width */
                                                       8, /* height */
                                                       pOrder->pattern,
                                                       sizeof (pOrder->pattern));
            if (fCached)
            {
                iCache = m_BrushCache.iLastBrush;
                m_BrushCache.iLastBrush = (m_BrushCache.iLastBrush + 1) % RT_ELEMENTS(m_BrushCache.aBrushes);

                TESTLOG(("BRUSHCACHE: cached %d\n", iCache));

                /* Pattern was sent. Save the pattern in the cache. */
                pElem = &m_BrushCache.aBrushes[iCache];

                memcpy(pElem->au8Pattern, pOrder->pattern, sizeof (pElem->au8Pattern));
                pElem->fCached = true;
            }
        }

        if (iCache != -1)
        {
            /* The pattern is on the client. Generate a cached order. */
            VRDEORDERPATBLTBRUSH order = *pOrder;

            order.pattern[0] = (uint8_t)iCache;

            return processOutputOrder (uScreenId, VRDP_ORDER_INTERNALPATBLTCACHED, &order, sizeof (order));
        }

        /* Fall back to generic output. */
    }

    return processOutputOrder (uScreenId, i32Op, pvOrder, cbOrder);
}

/*
 * Process an output update for the client.
 * Note that the function does not free any data related to the update.
 * The data is freed by the OutputQueue code when update is deleted.
 * @thread OUTPUT
 */
void VRDPClient::Output (OutputUpdate *pUpdate)
{
    Assert (pUpdate);

    switch (pUpdate->iCode)
    {
        case VRDP_OUTPUT_KEEP_ALIVE:
        {
            /* Transport detected a period of client's inactivity. */
            SERVERLOG(("VRDP_OUTPUT_KEEP_ALIVE\n"));
            m_vrdptp.OutputKeepAlive (Stream ());
        } break;

        case VRDP_OUTPUT_HIDE_POINTER:
        {
            SERVERLOG(("VRDP_OUTPUT_HIDE_POINTER\n"));
        } break;

        case VRDP_OUTPUT_CACHED_POINTER:
        {
            uint32_t *pu32ServerId = (uint32_t *)pUpdate->pvData;

            SERVERLOG(("VRDP_OUTPUT_CACHED_POINTER: 0x%08X\n", *pu32ServerId));

            pointerCacheOutput (*pu32ServerId);
        } break;

        case VRDP_OUTPUT_SET_VOLUME:
        {
            VRDPSetVolumeStruct *pSetVolume = (VRDPSetVolumeStruct *)pUpdate->pvData;

            m_vrdptp.Audio()->SendSetVolume (pSetVolume->left, pSetVolume->right);
        } break;

        case VRDP_OUTPUT_AUDIO_COMPLETION:
        {
            uint8_t *pu8PacketIdx = (uint8_t *)pUpdate->pvData;

            AUDIOLOG(("VRDP_OUTPUT_AUDIO_COMPLETION: 0x%02X\n", *pu8PacketIdx));

            m_vrdptp.Audio()->Completion (*pu8PacketIdx);
        } break;

        case VRDP_OUTPUT_USB:
        {
            m_vrdptp.USB()->Send (pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_CLIPBOARD_DATA:
        {
            /* Process the host request. */
            m_vrdptp.Clipboard()->SendData (pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_CLIPBOARD_PACKET:
        {
            /* Process the request from INPUT thread. */
            m_vrdptp.Clipboard()->SendPacket (pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_PALETTE_8_BPP:
        {
            m_vrdptp.OutputPalette (Stream ());
        } break;

        case VRDP_OUTPUT_DVC:
        {
            SERVERLOG(("VRDP_OUTPUT_DVC\n"));
            m_vrdptp.DVC()->ProcessOutput(pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_TSMFRAW:
        {
            /** @todo output updates must use callback pointers. */
            SERVERLOG(("VRDP_OUTPUT_TSMFRAW\n"));
            m_TSMFRaw.TSMFRawOutput(pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_UTCINFO_DATA:
        {
            SERVERLOG(("VRDP_OUTPUT_UTCINFO_DATA\n"));
            m_UTCINFO.SendData(pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_AUDIOINPUT_DATA:
        {
            SERVERLOG(("VRDP_OUTPUT_AUDIOINPUT_DATA\n"));
            m_ai.OutputAudioInputData(pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_AUDIOINPUT_OPEN:
        {
            SERVERLOG(("VRDP_OUTPUT_AUDIOINPUT_OPEN\n"));
            m_ai.OutputAudioInputOpen(pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_AUDIOINPUT_CLOSE:
        {
            SERVERLOG(("VRDP_OUTPUT_AUDIOINPUT_CLOSE\n"));
            m_ai.OutputAudioInputClose(pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_RDPDR:
        {
            /* Process the request from INPUT thread. */
            m_vrdptp.RDPDR()->ProcessOutput (pUpdate->pvData, pUpdate->cbData);
        } break;

        case VRDP_OUTPUT_GENERIC:
        {
            /* Process the request from other threads. */
            VRDPOUTPUTGENERICHDR *pHdr = (VRDPOUTPUTGENERICHDR *)pUpdate->pvData;

            if (pHdr->pfnCallback)
            {
                pHdr->pfnCallback(this, pHdr);
            }
        } break;

        default:
        {
            AssertFailed ();
        }
    }
}


void VRDPClient::fontCacheSetup (void)
{
    memset (&m_FontCache, 0, sizeof (m_FontCache));
}

void VRDPClient::brushCacheSetup (void)
{
    memset (&m_BrushCache, 0, sizeof (m_BrushCache));
}

void VRDPClient::bitmapCacheSetup (void)
{
    uint16_t u16CacheId;

    for (u16CacheId = 0; u16CacheId < 3; u16CacheId++)
    {
        m_BitmapCache.au16MRUHead[u16CacheId] = VRDP_CLIENT_BC_IDX_NULL;
        m_BitmapCache.au16MRUTail[u16CacheId] = VRDP_CLIENT_BC_IDX_NULL;
    }

    uint16_t u16Idx;
    VRDPCLIENTBITMAPCACHEELEM *pCacheElem;

    for (u16Idx = 0, pCacheElem = &m_BitmapCache.aCache0[0];
                 u16Idx < RT_ELEMENTS(m_BitmapCache.aCache0); u16Idx++, pCacheElem++)
    {
        pCacheElem->u16MRUNext = VRDP_CLIENT_BC_IDX_NULL;
        pCacheElem->u16MRUPrev = VRDP_CLIENT_BC_IDX_NULL;
        pCacheElem->u16RemoteId = VRDP_CLIENT_BC_IDX_NULL;
    }

    for (u16Idx = 0, pCacheElem = &m_BitmapCache.aCache1[0];
                 u16Idx < RT_ELEMENTS(m_BitmapCache.aCache1); u16Idx++, pCacheElem++)
    {
        pCacheElem->u16MRUNext = VRDP_CLIENT_BC_IDX_NULL;
        pCacheElem->u16MRUPrev = VRDP_CLIENT_BC_IDX_NULL;
        pCacheElem->u16RemoteId = VRDP_CLIENT_BC_IDX_NULL;
    }

    for (u16Idx = 0, pCacheElem = &m_BitmapCache.aCache2[0];
                 u16Idx < RT_ELEMENTS(m_BitmapCache.aCache2); u16Idx++, pCacheElem++)
    {
        pCacheElem->u16MRUNext = VRDP_CLIENT_BC_IDX_NULL;
        pCacheElem->u16MRUPrev = VRDP_CLIENT_BC_IDX_NULL;
        pCacheElem->u16RemoteId = VRDP_CLIENT_BC_IDX_NULL;
    }
}

bool VRDPClient::OutputMemblt (unsigned uScreenId,
                               VRDPBitmapCompressed *pBmp,
                               uint16_t u16TileX,
                               uint16_t u16TileY,
                               uint16_t u16DstX,
                               uint16_t u16DstY,
                               uint16_t u16SrcX,
                               uint16_t u16SrcY,
                               uint16_t u16SrcW,
                               uint16_t u16SrcH,
                               uint8_t u8ROP,
                               const RGNRECT *pRectTile,
                               uint8_t u8TileSizeClass)
{
    RT_NOREF1(pRectTile);
    SERVERLOG(("OutputMemblt: tile %d.%d, dst %d,%d, src %d,%d %dx%d, rect tile %d,%d %dx%d, class %d\n",
               u16TileX,
               u16TileY,
               u16DstX,
               u16DstY,
               u16SrcX,
               u16SrcY,
               u16SrcW,
               u16SrcH,
               pRectTile->x, pRectTile->y, pRectTile->w, pRectTile->h,
               u8TileSizeClass
             ));

    if (m_fDisableDisplay)
    {
        return false;
    }

    /* Find out whether the tile {*pHash, u16TileX, u16TileY} is already
     * in the client's cache. If not, cache it.
     */

    uint16_t u16CacheId = u8TileSizeClass;
    uint16_t u16Idx;

    bool fCached = false;

    VRDPBitmapTileHash tileHash;

    if (!pBmp->QueryTileHash (u16TileX, u16TileY, &tileHash))
    {
        return false;
    }

    VRDPCLIENTBITMAPCACHEELEM *paCache = NULL;
    uint16_t cCache = 0;

    switch (u16CacheId)
    {
        case 0: paCache = &m_BitmapCache.aCache0[0]; cCache = RT_ELEMENTS(m_BitmapCache.aCache0); break;
        case 1: paCache = &m_BitmapCache.aCache1[0]; cCache = RT_ELEMENTS(m_BitmapCache.aCache1); break;
        case 2: paCache = &m_BitmapCache.aCache2[0]; cCache = RT_ELEMENTS(m_BitmapCache.aCache2); break;

        default: AssertFailed ();
    }

    VRDPCLIENTBITMAPCACHEELEM *pCacheElem = NULL;

    u16Idx = m_BitmapCache.au16MRUHead[u16CacheId];

    while (u16Idx != VRDP_CLIENT_BC_IDX_NULL)
    {
        pCacheElem = &paCache[u16Idx];

        if (IsBitmapHashEqual (&tileHash, &pCacheElem->tileHash))
        {
            break;
        }

        u16Idx = pCacheElem->u16MRUNext;
    }

    if (u16Idx != VRDP_CLIENT_BC_IDX_NULL)
    {
        /* The client already has the bitmap cached. */
        SERVERLOG(("Sending remote id %d, %d\n", u16CacheId, u16Idx));
        Assert (u16Idx == pCacheElem->u16RemoteId);

        if (pCacheElem->u16MRUPrev != VRDP_CLIENT_BC_IDX_NULL)
        {
            Assert (u16Idx != m_BitmapCache.au16MRUHead[u16CacheId]);

            /* The entry is not yet in the head. Exclude from list. */
            paCache[pCacheElem->u16MRUPrev].u16MRUNext = pCacheElem->u16MRUNext;

            if (pCacheElem->u16MRUNext != VRDP_CLIENT_BC_IDX_NULL)
            {
                paCache[pCacheElem->u16MRUNext].u16MRUPrev = pCacheElem->u16MRUPrev;
            }
            else
            {
                Assert (u16Idx == m_BitmapCache.au16MRUTail[u16CacheId]);
                m_BitmapCache.au16MRUTail[u16CacheId] = pCacheElem->u16MRUPrev;
            }

            /* Insert the entry at the head of MRU list. */
            pCacheElem->u16MRUNext = m_BitmapCache.au16MRUHead[u16CacheId];
            pCacheElem->u16MRUPrev = VRDP_CLIENT_BC_IDX_NULL;

            Assert (m_BitmapCache.au16MRUHead[u16CacheId] != VRDP_CLIENT_BC_IDX_NULL);

            paCache[m_BitmapCache.au16MRUHead[u16CacheId]].u16MRUPrev = u16Idx;

            m_BitmapCache.au16MRUHead[u16CacheId] = u16Idx;

        }
        else
        {
            /* This must be the head of the MRU list then. */
            Assert (u16Idx == m_BitmapCache.au16MRUHead[u16CacheId]);
        }

        fCached = true;
    }
    else
    {
        /* The client does not yet have the bitmap in the cache. */
        SERVERLOG(("Caching bitmap cache id %d\n", u16CacheId));

        /* Find a free slot in the cache for the new pointer. */
        pCacheElem = &paCache[0];

        for (u16Idx = 0; u16Idx < cCache; u16Idx++, pCacheElem++)
        {
            if (pCacheElem->u16RemoteId == VRDP_CLIENT_BC_IDX_NULL)
            {
                break;
            }
        }

        if (u16Idx < cCache)
        {
            /* There is still a place in the cache. */
            /* Do nothing. */
            SERVERLOG(("There is a free slot in the cache.\n"));
        }
        else
        {
            /* Reuse the tail of the MRU list. Exclude it from the list. */
            u16Idx = m_BitmapCache.au16MRUTail[u16CacheId];
            pCacheElem = &paCache[u16Idx];

            SERVERLOG(("Reusing the tail %d.\n", u16Idx));

            Assert (u16Idx != m_BitmapCache.au16MRUHead[u16CacheId]);

            /* Exclude the current tail. */
            m_BitmapCache.au16MRUTail[u16CacheId] = pCacheElem->u16MRUPrev;
            /* Adjust the new tail. */
            paCache[pCacheElem->u16MRUPrev].u16MRUNext = VRDP_CLIENT_BC_IDX_NULL;

            /* Clear the just excluded cache element. */
            Assert (pCacheElem->u16MRUNext == VRDP_CLIENT_BC_IDX_NULL); /* Tail is being reused. */
            pCacheElem->u16MRUPrev = VRDP_CLIENT_BC_IDX_NULL;
            pCacheElem->u16RemoteId = VRDP_CLIENT_BC_IDX_NULL;
        }

        SERVERLOG(("Allocated remote bitmap index %d, %d\n", u16CacheId, u16Idx));

        fCached = m_vrdptp.OutputBitmapCache (Stream (),
                                              pBmp,
                                              u16TileX,
                                              u16TileY,
                                              u16CacheId,
                                              u16Idx);
        if (fCached)
        {
           /* Successfully generated the order.
            * Initialize the slot as the head of the MRU list.
            */
           pCacheElem->tileHash = tileHash;
           pCacheElem->u16TileX = u16TileX;
           pCacheElem->u16TileY = u16TileY;

           pCacheElem->u16RemoteId = u16Idx;

           pCacheElem->u16MRUNext  = m_BitmapCache.au16MRUHead[u16CacheId];
           pCacheElem->u16MRUPrev  = VRDP_CLIENT_BC_IDX_NULL;

           if (m_BitmapCache.au16MRUHead[u16CacheId] == VRDP_CLIENT_BC_IDX_NULL)
           {
               /* First element in the cache. */
               m_BitmapCache.au16MRUTail[u16CacheId] = u16Idx;
           }
           else
           {
               paCache[m_BitmapCache.au16MRUHead[u16CacheId]].u16MRUPrev = u16Idx;
           }

           m_BitmapCache.au16MRUHead[u16CacheId] = u16Idx;
        }
    }

    SERVERLOG(("fCached = %d\n", fCached));

    if (fCached)
    {
        VRDPORDERINTERNALMEMBLT order;

        order.u16CacheId = u16CacheId;
        order.u16Idx     = u16Idx;
        order.u16DstX    = u16DstX;
        order.u16DstY    = u16DstY;
        order.u16Width   = u16SrcW;
        order.u16Height  = u16SrcH;
        order.u16SrcX    = u16SrcX;
        order.u16SrcY    = u16SrcY;
        order.u8ROP      = u8ROP;

        int rc = processOutputOrder (uScreenId, VRDP_ORDER_INTERNALMEMBLT, &order, sizeof(order));

        fCached = RT_SUCCESS(rc);
    }

    return fCached;
}

bool VRDPClient::IsOrderSupported (int iOrder)
{
    if (iOrder < 0)
    {
        return true; /* Internal orders always supported. */
    }

    if (iOrder >= 32)
    {
        TESTLOG(("Order fallback: %d, invalid index\n", iOrder));
        return false;
    }

    bool fSupported = (m_fu32SupportedOrders & (1UL << iOrder)) != 0;

    if (!fSupported)
    {
        TESTLOG(("Order fallback: %d\n", iOrder));
    }

    return fSupported;
}

void VRDPClient::AddRedraw (const RGNRECT *pRect, bool fShadowBuffer, unsigned uScreenId)
{
    /* The redraw rectangle is either in clients coords (fShadowBuffer == false)
     * or in the guest coords (fShadowBuffer == true).
     *
     * uScreenId is valid only when fShadowBuffer == true.
     */
    int rc = m_pServer->Enter ();

    if (RT_SUCCESS (rc))
    {
        if (fShadowBuffer)
        {
            /* Add a shadow buffer redraw. */
            Assert(pRect);

            if (m_redraw.cRectsShadowBuffer < (int)RT_ELEMENTS(m_redraw.aRectsShadowBuffer))
            {
                TESTLOG(("REDRAW: Adding shadow buffer rect at %d\n", m_redraw.cRectsShadowBuffer));

                VRDPRedrawShadowBuffer *pRedrawShadowBuffer = &m_redraw.aRectsShadowBuffer[m_redraw.cRectsShadowBuffer];

                pRedrawShadowBuffer->rectRedraw = *pRect;
                pRedrawShadowBuffer->uScreenId = uScreenId;

                m_redraw.cRectsShadowBuffer++;
            }
            else
            {
                /** @todo A separate accumulation rectangle for each screen for this case.
                 */

                /* Merge the new rect with the last rect in the array.
                 * This is a rare case, so can be not efficient.
                 */
                TESTLOG(("REDRAW: Too many shadow buffer redraw requests, merging new!!!\n"));

                Assert (m_redraw.cRectsShadowBuffer == RT_ELEMENTS(m_redraw.aRectsShadowBuffer));

                VRDPRedrawShadowBuffer *pRedrawShadowBuffer = &m_redraw.aRectsShadowBuffer[m_redraw.cRectsShadowBuffer - 1];

                rgnMergeRects(&pRedrawShadowBuffer->rectRedraw, &pRedrawShadowBuffer->rectRedraw, pRect);
            }
        }
        else if (pRect == NULL)
        {
            /* Fullscreen redraw. */
            TESTLOG(("REDRAW: Adding fullscreen\n"));
            m_redraw.fFullscreen = true;
        }
        else
        {
            /* Client redraw. */
            if (m_redraw.cRectsClient < (int)RT_ELEMENTS(m_redraw.aRectsClient))
            {
                TESTLOG(("REDRAW: Adding client rect at %d\n", m_redraw.cRectsClient));

                VRDPRedrawClient *pRedrawClient = &m_redraw.aRectsClient[m_redraw.cRectsClient];

                pRedrawClient->rectRedraw = *pRect;

                m_redraw.cRectsClient++;
            }
            else
            {
                /* Merge the new rect with the last rect in the array.
                 * This is a rare case, so can be not efficient.
                 */
                TESTLOG(("REDRAW: Too many client redraw requests, merging new!!!\n"));

                Assert (m_redraw.cRectsClient == RT_ELEMENTS (m_redraw.aRectsClient));

                VRDPRedrawClient *pRedrawClient = &m_redraw.aRectsClient[m_redraw.cRectsClient - 1];

                rgnMergeRects(&pRedrawClient->rectRedraw, &pRedrawClient->rectRedraw, pRect);
            }
        }

        m_pServer->Exit ();
    }
}

int VRDPClient::QueryRedraw (VRDPRedrawInfo *pRedraw)
{
    Assert (pRedraw);

    int rc = m_pServer->Enter ();

    if (RT_SUCCESS (rc))
    {
        Assert (m_redraw.cRectsClient <= (int)RT_ELEMENTS(m_redraw.aRectsClient));
        Assert (m_redraw.cRectsShadowBuffer <= (int)RT_ELEMENTS(m_redraw.aRectsShadowBuffer));

        /* Copy redraw information to the called */
        *pRedraw = m_redraw;

        m_redraw.cRectsShadowBuffer = 0;
        m_redraw.cRectsClient = 0;
        m_redraw.fFullscreen = false;

        m_pServer->Exit ();
    }

    return rc;
}

uint8_t VRDPClient::textFragmentCache (uint8_t *pu8Text, uint8_t *pcbText, uint8_t u8CharInc, int16_t i16Delta, uint8_t u8FontHandle)
{
    uint8_t u8Fragment = 0xFF; /* The invalid fragment code. */

    /* The fragment cache will
     * either
     *   add 3 bytes of the fragment index, if the fragment is not yet cached,
     * or
     *   replace the au8Text with the 3 bytes of the cached fragment.
     *
     * Note: pu8Text does have at least 3 available trailing bytes.
     *
     * Current implementation is simple:
     * There is no MRU list, fragment cache is considered a ring buffer.
     * New Fragments are added to the head of the buffer, and overwrite
     * existing fragments. This can be improved later.
     */
    uint8_t cbText = *pcbText;

    if (cbText <= 3)
    {
        /* The fragment is too short to be cached. */
        return u8Fragment;
    }

    TESTLOG(("fragment cache length %d\n", cbText));

    uint64_t u64CRC = RTCrc64 (pu8Text, cbText);

    unsigned i;
    for (i = 0; i < RT_ELEMENTS(m_TextFragmentCache.aElems); i++)
    {
        if (   m_TextFragmentCache.aElems[i].u64CRC == u64CRC
            && m_TextFragmentCache.aElems[i].u8Length == cbText
            && m_TextFragmentCache.aElems[i].u8FontHandle == u8FontHandle)
        {
            break;
        }
    }

    if (i == RT_ELEMENTS(m_TextFragmentCache.aElems))
    {
        /* The fragment is not yet in the cache. */
        TESTLOG(("fragment cache %d: new element, length %d\n", m_TextFragmentCache.u8FreeElem, cbText));

        /* Add it to the cache. */
        TEXTFRAGMENTELEM *pElem = &m_TextFragmentCache.aElems[m_TextFragmentCache.u8FreeElem];

        pElem->u8Index      = m_TextFragmentCache.u8FreeElem;
        pElem->u8Length     = cbText;
        pElem->u64CRC       = u64CRC;
        pElem->u8FontHandle = u8FontHandle;

        /* Advance the ring buffer pointer. */
        m_TextFragmentCache.u8FreeElem = (m_TextFragmentCache.u8FreeElem + 1) % RT_ELEMENTS(m_TextFragmentCache.aElems);

        /* Add the fragment description to the text. */
        pu8Text[cbText++] = 0xFF;
        pu8Text[cbText++] = pElem->u8Index;
        pu8Text[cbText++] = pElem->u8Length;

        /* Only return the fragment index if it was added to the cache. */
        u8Fragment = pElem->u8Index;
    }
    else
    {
        /* Found fragment in the cache. */
        TEXTFRAGMENTELEM *pElem = &m_TextFragmentCache.aElems[i];

        TESTLOG(("fragment cache %d: found, length %d\n", i, pElem->u8Length));

        cbText = 0;
        pu8Text[cbText++] = 0xFE;
        pu8Text[cbText++] = pElem->u8Index;

        Assert (pElem->u8Index == i);

        if (u8CharInc == 0)
        {
           /* Add the delta bytes. */
           if (0 <= i16Delta && i16Delta <= 0x7F)
           {
               pu8Text[cbText++] = (uint8_t)i16Delta;
           }
           else
           {
               pu8Text[cbText++] = 0x80;
               pu8Text[cbText++] = (uint8_t)i16Delta;
               pu8Text[cbText++] = (uint8_t)(i16Delta >> 8);
           }
        }
    }

    *pcbText = cbText;

    return u8Fragment;
}

void VRDPClient::textFragmentRemove (uint8_t u8Fragment)
{
    if (u8Fragment >= RT_ELEMENTS(m_TextFragmentCache.aElems))
    {
        /* Nothing to remove, the input parameter indicates that the cache must not be changed. */
        return;
    }

    /* Clear the corresponding cache element. */
    TEXTFRAGMENTELEM *pElem = &m_TextFragmentCache.aElems[u8Fragment];

    pElem->u8Index = 0;
    pElem->u8Length = 0;
    pElem->u64CRC = 0;
    pElem->u8FontHandle = 0;
}

void VRDPClient::OutputText2 (unsigned uScreenId, const TCFONTTEXT2 *pFontText2)
{
    int i;
    int rc;

    VRDPORDERINTERNALTEXT2 order; /* Used both for FONTCACHE and TEXT2 orders. */

    /*
     * First, generate required FONTCACHE order.
     *
     * Check which glyphs have to be resent to the client.
     */

    TCCLIENTFONT *pFont = &m_FontCache.aFonts[pFontText2->index];

    if (pFont->u32Uniq != pFontText2->u32Uniq)
    {
        /* Reset the cache. */
        memset (pFont->aGlyphs, 0, sizeof (pFont->aGlyphs));

        pFont->u32Uniq = pFontText2->u32Uniq;
    }

    uint8_t *pu8Text = &order.au8Text[0];

    for (i = 0; i < pFontText2->cGlyphs; i++)
    {
        uint8_t u8Index = pFontText2->aGlyphs[i].pCachedGlyph->u8Index;

        Assert(u8Index < RT_ELEMENTS(pFont->aGlyphs));

        if (!pFont->aGlyphs[u8Index].fCached)
        {
            pFont->aGlyphs[u8Index].fCached = 1;
            *pu8Text++ = i;
        }
    }

    if (pu8Text != &order.au8Text[0])
    {
        if (!m_vrdptp.OutputFontCache (Stream (), pFontText2, order.au8Text, pu8Text - &order.au8Text[0]))
        {
            /* Mark entire cache as invalid. */
            pFont->u32Uniq = 0;
            return;
        }
    }

    TESTLOG(("Generating glyph sequences: BM_BASE = 0x%02X, u8CharInc = %d\n",
              pFontText2->u8Flags & VRDP_TEXT2_CHAR_INC_EQUAL_BM_BASE,
              pFontText2->u8CharInc));

    /*
     * Initialize order fields which will be not changed.
     */
    order.u8RDPFontHandle = pFontText2->u8RDPFontHandle;
    order.u8Flags         = pFontText2->u8Flags;
    order.u8CharInc       = pFontText2->u8CharInc;
    order.rgbFG           = pFontText2->rgbFG;
    order.rgbBG           = pFontText2->rgbBG;
    /* And fields which might remain unchanged partially. */
    order.bkground        = pFontText2->bkground;
    order.opaque          = pFontText2->opaque;

    /*
     * Generate the string of glyph indexes.
     */
    if (   (pFontText2->u8Flags & VRDP_TEXT2_CHAR_INC_EQUAL_BM_BASE) == 0
        && pFontText2->u8CharInc == 0)
    {
        /* Not monospaced font. The original string may require a few TEXT2 orders.
         *
         * Calculate and encode the next positions (x for horizontal, y for vertical) of glyphs.
         *
         * The string of glyphs is divided to substrings.
         *
         */

        /* Since it is possible that 2 or more internal orders will be emitted,
         * issue the sequence start internal order.
         */
        processOutputOrder (uScreenId, VRDP_ORDER_INTERNALSEQBEGIN, NULL, 0);

        /* How many bytes of data the au8Text contains. */
        uint8_t cbText = 0;

        /* Origin of the first glyph in the substring. Used as the substring origin. */
        int xFirstGlyphStart = pFontText2->origin.x;
        int yFirstGlyphStart = pFontText2->origin.y;

        /* Where the first glyph in the substring ends. To divide the bkground/opaque rectangles. */
        int xFirstGlyphBitmapEnd = pFontText2->origin.x;
        int yFirstGlyphBitmapEnd = pFontText2->origin.y;

        /* Where the last glyph of the previous substring starts. To divide the bkground/opaque rectangles. */
        int xPrevSubstringGlyphStart = pFontText2->origin.x;
        int yPrevSubstringGlyphStart = pFontText2->origin.y;

        /* Where the last glyph bitmap of the previous substring ends. To divide the bkground/opaque rectangles. */
        int xPrevSubstringGlyphBitmapEnd = pFontText2->origin.x;
        int yPrevSubstringGlyphBitmapEnd = pFontText2->origin.y;

        /* Origin of last processed glyph. Used for calculation of delta to the next glyph. */
        int xLastGlyphStart = pFontText2->origin.x;
        int yLastGlyphStart = pFontText2->origin.y;

        /* Where the last glyph bitmap ends. To divide the bkground/opaque rectangles. */
        int xLastGlyphBitmapEnd = pFontText2->origin.x;
        int yLastGlyphBitmapEnd = pFontText2->origin.y;

        const bool fHorizontal = (pFontText2->u8Flags & VRDP_TEXT2_VERTICAL) == 0;

        /* Indicates a new substring start. */
        bool fSubstringStart = false;

        /* How many substrings have been generated. */
        int cSubstrings = 0;

        i = 0;

        for (;;) /* Note: i is incremented the first line of the loop. */
        {
            const TCGLYPH *pGlyph = &pFontText2->aGlyphs[i++]; /* Increment i here to be able to check for
                                                                * the end of string as i == pFontText2->cGlyphs.
                                                                */

            bool fEndOfGlyphs = (i == pFontText2->cGlyphs);

            uint8_t u8Glyph = pGlyph->pCachedGlyph->u8Index;

            if (fSubstringStart)
            {
                /* This is the first glyph in the substring. */
                xFirstGlyphStart = pGlyph->x;
                yFirstGlyphStart = pGlyph->y;

                xFirstGlyphBitmapEnd = pGlyph->x + pGlyph->pCachedGlyph->w + pGlyph->pCachedGlyph->xOrigin;
                yFirstGlyphBitmapEnd = pGlyph->y + pGlyph->pCachedGlyph->h + pGlyph->pCachedGlyph->yOrigin;

                /* Make the first calculated delta = 0. */
                xLastGlyphStart = pGlyph->x;
                yLastGlyphStart = pGlyph->y;

                fSubstringStart = false;
            }

            /*
             * Store the glyph index.
             */
            order.au8Text[cbText++] = u8Glyph;

            /*
             * Compute the delta to the previous glyph.
             */
            int iDelta;

            if (fHorizontal)
            {
                iDelta = pGlyph->x - xLastGlyphStart;
            }
            else
            {
                iDelta = pGlyph->y - yLastGlyphStart;
            }

            /*
             * Store the delta.
             */
            if (0 <= iDelta && iDelta <= 0x7f)
            {
                order.au8Text[cbText++] = (uint8_t)iDelta;
            }
            else
            {
                order.au8Text[cbText++] = 0x80;
                order.au8Text[cbText++] = (uint8_t)iDelta;
                order.au8Text[cbText++] = (uint8_t)(iDelta >> 8);
            }

            /*
             * Update the glyph positions.
             */
            xLastGlyphStart = pGlyph->x;
            yLastGlyphStart = pGlyph->y;

            xLastGlyphBitmapEnd = pGlyph->x + pGlyph->pCachedGlyph->w + pGlyph->pCachedGlyph->xOrigin;
            yLastGlyphBitmapEnd = pGlyph->y + pGlyph->pCachedGlyph->h + pGlyph->pCachedGlyph->yOrigin;

            TESTLOG(("[%d] d %d, [%d..%d] \n", i, iDelta, xLastGlyphStart, xLastGlyphBitmapEnd));

            /*
             * Send the order if enough data has been accumulated.
             */
            if (cbText >= 0xf0 || fEndOfGlyphs)
            {
                /* Setup the order rectangles. */
                order.origin.x = xFirstGlyphStart;
                order.origin.y = yFirstGlyphStart;

                /*clip==bkground*/
                /*box==opaque*/

                if (fHorizontal)
                {
                    /* Horizontal: bottom and top fields remain unchanged. */
                    if (xFirstGlyphStart <= xLastGlyphStart)
                    {
                        /* Left to right text. */
                        if (cSubstrings == 0)
                        {
                            /* Left edge is not changed for the first substring. */
                            order.bkground.left = pFontText2->bkground.left;
                            order.opaque.left   = pFontText2->opaque.left;
                        }
                        else
                        {
                            order.bkground.left = RT_MIN(xFirstGlyphStart, xPrevSubstringGlyphBitmapEnd);

                            if (pFontText2->opaque.top != pFontText2->opaque.bottom)
                            {
                                /* Only if the opaque rectangle is not empty. */
                                order.opaque.left = xPrevSubstringGlyphBitmapEnd;
                            }
                        }

                        if (fEndOfGlyphs)
                        {
                            /* Right edge is not changed for the last substring. */
                            order.bkground.right = pFontText2->bkground.right;
                            order.opaque.right = pFontText2->opaque.right;
                        }
                        else
                        {
                            order.bkground.right = xLastGlyphBitmapEnd;
                            if (pFontText2->opaque.top != pFontText2->opaque.bottom)
                            {
                                /* Only if the opaque rectangle is not empty. */
                                order.opaque.right = xLastGlyphBitmapEnd;
                            }
                        }
                    }
                    else
                    {
                        /* Right to left text. */
                        if (cSubstrings == 0)
                        {
                            /* Right edge is not changed for the first substring. */
                            order.bkground.right = pFontText2->bkground.right;
                            order.opaque.right   = pFontText2->opaque.right;
                        }
                        else
                        {
                            order.bkground.right = RT_MAX(xFirstGlyphBitmapEnd, xPrevSubstringGlyphStart);

                            if (pFontText2->opaque.top != pFontText2->opaque.bottom)
                            {
                                /* Only if the opaque rectangle is not empty. */
                                order.opaque.right = xPrevSubstringGlyphStart;
                            }
                        }

                        if (fEndOfGlyphs)
                        {
                            /* Left edge is not changed for the last substring. */
                            order.bkground.left = pFontText2->bkground.left;
                            order.opaque.left = pFontText2->opaque.left;
                        }
                        else
                        {
                            order.bkground.left = xLastGlyphStart;
                            if (pFontText2->opaque.top != pFontText2->opaque.bottom)
                            {
                                /* Only if the opaque rectangle is not empty. */
                                order.opaque.left = xLastGlyphStart;
                            }
                        }
                    }
                }
                else
                {
                    /* Vertical: left and right fields remain unchanged. */
                    AssertMsgFailed (("Unimplemented.\n"));
                }

                /* Apply the fragment cache. */
                uint8_t u8TextFragment = textFragmentCache (order.au8Text, &cbText, pFontText2->u8CharInc, 0, order.u8RDPFontHandle);

                order.u8TextLength = cbText;
                rc = processOutputOrder (uScreenId, VRDP_ORDER_INTERNALTEXT2, &order, sizeof(order));

                if (rc != VINF_SUCCESS) /* Includes VWRN_INVALID_PARAMETER. */
                {
                    /* The order was not generated. Update the fragment cache. */
                    textFragmentRemove (u8TextFragment);
                }

                cbText = 0;

                xPrevSubstringGlyphStart = xLastGlyphStart;
                yPrevSubstringGlyphStart = yLastGlyphStart;

                xPrevSubstringGlyphBitmapEnd = xLastGlyphBitmapEnd;
                yPrevSubstringGlyphBitmapEnd = yLastGlyphBitmapEnd;

                fSubstringStart = true;
                cSubstrings++;

                if (fEndOfGlyphs)
                {
                    TESTLOG(("TEXT2: substrings %d\n", cSubstrings));
                    break;
                }
            }
        }

        /* Sequence of internal orders has been completed. */
        processOutputOrder (uScreenId, VRDP_ORDER_INTERNALSEQEND, NULL, 0);

        Assert (cbText == 0);
    }
    else
    {
        /* Monospaced font. Glyph indexes are enough. */
        for (i = 0; i < pFontText2->cGlyphs; i++)
        {
            order.au8Text[i] = pFontText2->aGlyphs[i].pCachedGlyph->u8Index;
        }

        /* Apply the fragment cache. */
        uint8_t cbText = (uint8_t)pFontText2->cGlyphs;
        uint8_t u8TextFragment = textFragmentCache (order.au8Text, &cbText, pFontText2->u8CharInc, 0, order.u8RDPFontHandle);

        order.bkground     = pFontText2->bkground;
        order.opaque       = pFontText2->opaque;
        order.origin       = pFontText2->origin;
        order.u8TextLength = cbText;

        rc = processOutputOrder (uScreenId, VRDP_ORDER_INTERNALTEXT2, &order, sizeof(order));

        if (rc != VINF_SUCCESS) /* Includes VWRN_INVALID_PARAMETER. */
        {
            /* The order was not generated. Update the fragment cache. */
            textFragmentRemove (u8TextFragment);
        }
    }
}

void VRDPClient::Redirect(const char *pszServer, const char *pszUser, const char *pszDomain, const char *pszPassword, uint32_t u32SessionId, const char *pszCookie)
{
    uint32_t u32Flags = 0;

    m_vrdptp.Redirect(&m_InputCtx,
                      u32Flags,
                      pszServer,
                      pszUser,
                      pszDomain,
                      pszPassword,
                      u32SessionId,
                      pszCookie);

    /* "[The client] MUST terminate the current connection to the server that transmitted the redirection PDU"
     * Therefore it is not necessary to close the connection. However set the flag to close the connection anyway,
     * just to be sure that connection is closed.
     */
    ASMAtomicOrU32(&m_u32EventFlags, VRDP_CLIENT_DISCONNECT);
}

static int queryFeature(VRDPServer *pServer,
                        VRDEFEATURE *pFeature,
                        uint32_t cbBuffer,
                        const char *pszName,
                        uint32_t u32ClientId)
{
    /* First query via properties interface. */
    int rc = RTStrCopy(pFeature->achInfo, cbBuffer - RT_OFFSETOF(VRDEFEATURE, achInfo), "Property/");

    if (RT_SUCCESS(rc))
    {
        rc = RTStrCopy(&pFeature->achInfo[9], cbBuffer - RT_OFFSETOF(VRDEFEATURE, achInfo) - 9, pszName);
    }

    if (RT_SUCCESS(rc))
    {
        uint32_t cbOut = 0;

        pFeature->u32ClientId = u32ClientId;

        rc = appProperty(pServer->ApplicationCallbacks (), pServer->ApplicationCallbackPointer (),
                         VRDE_QP_FEATURE,
                         &pFeature, cbBuffer, &cbOut);
        if (   RT_FAILURE(rc)
            || pFeature->achInfo[0] == 0)
        {
            /* No value. Try legacy names. */
            rc = RTStrCopy(pFeature->achInfo, cbBuffer - RT_OFFSETOF(VRDEFEATURE, achInfo), pszName);

            if (RT_SUCCESS(rc))
            {
                pFeature->u32ClientId = u32ClientId;

                rc = appProperty(pServer->ApplicationCallbacks (), pServer->ApplicationCallbackPointer (),
                                 VRDE_QP_FEATURE,
                                 &pFeature, cbBuffer, &cbOut);
            }
        }
    }

    SERVERLOG(("VRDE_QP_FEATURE[%s] rc %Rrc\n", pszName, rc));

    return rc;
}

static void queryFeatureBoolean(bool *pfValue,
                                VRDPServer *pServer,
                                VRDEFEATURE *pFeature,
                                uint32_t cbBuffer,
                                const char *pszName,
                                uint32_t u32ClientId)
{
    int rc = queryFeature(pServer,
                          pFeature,
                          cbBuffer,
                          pszName,
                          u32ClientId);
    if (RT_SUCCESS(rc))
    {
        if (RTStrCmp(pFeature->achInfo, "1") == 0)
        {
            VRDPLOGREL(("feature: %s\n", pszName));
            *pfValue = true;
        }
    }
}

void VRDPClient::QueryClientFeatures(void)
{
    uint8_t au8Buffer[4096];

    VRDEFEATURE *pFeature = (VRDEFEATURE *)&au8Buffer[0];

    queryFeatureBoolean(&m_fDisableDisplay, Server(), pFeature, sizeof (au8Buffer),
                        "Client/DisableDisplay", m_u32ClientId);

    queryFeatureBoolean(&m_fDisableInput, Server(), pFeature, sizeof (au8Buffer),
                        "Client/DisableInput", m_u32ClientId);

    queryFeatureBoolean(&m_fDisableAudio, Server(), pFeature, sizeof (au8Buffer),
                        "Client/DisableAudio", m_u32ClientId);

    queryFeatureBoolean(&m_fDisableUSB, Server(), pFeature, sizeof (au8Buffer),
                        "Client/DisableUSB", m_u32ClientId);

    queryFeatureBoolean(&m_fDisableClipboard, Server(), pFeature, sizeof (au8Buffer),
                        "Client/DisableClipboard", m_u32ClientId);

    queryFeatureBoolean(&m_fDisableRDPDR, Server(), pFeature, sizeof (au8Buffer),
                        "Property/Client/DisableRDPDR", m_u32ClientId);
}

void VRDPClient::SetupChannels(void)
{
    if (!m_fDisableAudio)
    {
        m_vrdptp.Audio()->Setup ();
    }

    if (!m_fDisableUSB)
    {
        m_vrdptp.USB()->Setup ();
    }

    if (!m_fDisableClipboard)
    {
        m_vrdptp.Clipboard()->Setup ();
    }

    m_vrdptp.DVC()->Setup ();

    /* Only enable SunFlsh if TMSF is not available. */
    if (m_vrdptp.DVC()->Id () == 0)
    {
        m_vrdptp.SunFlsh()->Setup ();
    }

    if (!m_fDisableRDPDR)
    {
        m_vrdptp.RDPDR()->Setup ();
    }
}

static void orderSequenceDelete(VRDPCLIENTORDERSCREENDATA *pScreen)
{
    VRDPSAVEDINTERNALORDER *pIter = pScreen->sequence.pSavedInternalOrders;

    pScreen->sequence.pSavedInternalOrders = NULL;
    pScreen->sequence.fSavedSequence = false;

    while (pIter)
    {
        VRDPSAVEDINTERNALORDER *pNext = pIter->next;

        VRDPMemFree(pIter);

        pIter = pNext;
    }
}

static void orderSequenceReverse(VRDPCLIENTORDERSCREENDATA *pScreen)
{
    /* Reverse the saved internal orders list. */
    VRDPSAVEDINTERNALORDER *pIter = pScreen->sequence.pSavedInternalOrders;

    pScreen->sequence.pSavedInternalOrders = NULL;

    while (pIter)
    {
        VRDPSAVEDINTERNALORDER *pNext = pIter->next;

        pIter->next = pScreen->sequence.pSavedInternalOrders;
        pScreen->sequence.pSavedInternalOrders = pIter;

        pIter = pNext;
    }
}

static void orderDataDelete(VRDPCLIENTORDERDATA *p)
{
    unsigned i;
    for (i = 0; i < RT_ELEMENTS(p->aScreens); i++)
    {
        orderSequenceDelete(&p->aScreens[i]);
        VRDPMemFree(p->aScreens[i].order.pu8SavedOrder);
    }

    memset(p, 0, sizeof(VRDPCLIENTORDERDATA));
}

int VRDPClient::processOutputOrder (unsigned uScreenId, int32_t i32Op, void *pvOrder, uint32_t cbOrder)
{
    /* Internal wrapper which calls VRDPTP::OutputOrder and takes case of
     * VRDE_ORDER_BOUNDS and VRDE_ORDER_REPEAT in multimonitor mode.
     *
     * Graphics orders may arrive as a sequence of:
     *   - VRDE_ORDER_BOUNDS;
     *   - an order;
     *   - one or more VRDE_ORDER_REPEAT.
     * The order itself may be splitted to:
     *   - VRDP_ORDER_INTERNALSEQBEGIN;
     *   - one or more internal orders;
     *   - VRDP_ORDER_INTERNALSEQEND.
     *
     * In multiscreen mode orders for one screen may be interleaved with orders for another screen.
     *
     * Sequences for one screen must not be affected by orders for other screens.
     *
     * So following must be done before sending orders to the VRDPTP level:
     *   * VRDE_ORDER_BOUNDS must be saved per screen and must be sent immediately before
     *     the actual order for the same screen;
     *   * the last processed order must be saved per screen to resend it before
     *     a VRDE_ORDER_REPEAT, if there was an order for another screen between
     *     the order and the VRDE_ORDER_REPEAT;
     *   * INTERNALSEQBEGIN/END must be saved as well to be able to resend them.
     */

    if (uScreenId >= RDP_MAX_SCREENS)
    {
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;

    VRDPCLIENTORDERDATA *p = &m_orderData;
    VRDPCLIENTORDERSCREENDATA *pScreen = &p->aScreens[uScreenId];

    if (i32Op == VRDE_ORDER_BOUNDS)
    {
        /* Save bounds for this particular screen, do not forward it to the VRDPTP yet. */
        VRDEORDERBOUNDS *pOrder = (VRDEORDERBOUNDS *)pvOrder;

        SERVERLOG(("VRDPClient::processOutputOrder: [%u] VRDE_ORDER_BOUNDS %d,%d %d,%d\n",
                   uScreenId, pOrder->pt1.x, pOrder->pt1.y, pOrder->pt2.x, pOrder->pt2.y));

        Assert(!pScreen->bounds.fBounds);
        Assert(!pScreen->sequence.fSequenceInProcess);

        pScreen->bounds.fBounds = true;
        pScreen->bounds.savedBounds = *pOrder;
    }
    else if (i32Op == VRDE_ORDER_REPEAT)
    {
        VRDEORDERREPEAT *pOrder = (VRDEORDERREPEAT *)pvOrder;

        SERVERLOG(("VRDPClient::processOutputOrder: [%u] VRDE_ORDER_REPEAT %d,%d %d,%d\n",
                   uScreenId, pOrder->bounds.pt1.x, pOrder->bounds.pt1.y, pOrder->bounds.pt2.x, pOrder->bounds.pt2.y));

        Assert(!pScreen->sequence.fSequenceInProcess);

        /* Check if the REPEAT corresponds to the last order. */
        if (p->uLastScreenId == uScreenId)
        {
            /* It is OK to just forward the REPEAT because the screen has not changed. */
            rc = m_vrdptp.OutputOrder(uScreenId, Stream (), i32Op, pvOrder);
        }
        else
        {
            SERVERLOG(("VRDPClient::processOutputOrder: [%u] p->uLastScreenId %d\n",
                       uScreenId, p->uLastScreenId));

            /* Actually repeat the last order which was directed to this screen.
             * To do so, replace the REPEAT with a VRDE_ORDER_BOUNDS/order sequence.
             */
            VRDEORDERBOUNDS order;
            order.pt1.x = pOrder->bounds.pt1.x;
            order.pt1.y = pOrder->bounds.pt1.y;
            order.pt2.x = pOrder->bounds.pt2.x;
            order.pt2.y = pOrder->bounds.pt2.y;

            rc = m_vrdptp.OutputOrder(uScreenId, Stream (), VRDE_ORDER_BOUNDS, &order);

            if (RT_SUCCESS(rc))
            {
                /* Remember the screen id of the order. */
                p->uLastScreenId = uScreenId;

                if (RT_UNLIKELY(pScreen->sequence.fSavedSequence))
                {
                    /* If the last order is a saved sequence, then replay it. */
                    SERVERLOG(("VRDPClient::processOutputOrder: [%u] replaying saved sequence\n",
                               uScreenId));

                    Assert(!pScreen->sequence.fSequenceInProcess);

                    m_vrdptp.OutputOrder(uScreenId, Stream (), VRDP_ORDER_INTERNALSEQBEGIN, NULL);

                    VRDPSAVEDINTERNALORDER *pIter = pScreen->sequence.pSavedInternalOrders;

                    while (pIter)
                    {
                        m_vrdptp.OutputOrder(uScreenId, Stream (), pIter->i32Op, &pIter[1]);

                        pIter = pIter->next;
                    }

                    m_vrdptp.OutputOrder(uScreenId, Stream (), VRDP_ORDER_INTERNALSEQEND, NULL);
                }
                else
                {
                    /* Last order for this screen is a normal order. */
                    SERVERLOG(("VRDPClient::processOutputOrder: [%u] resending order\n",
                               uScreenId));

                    rc = m_vrdptp.OutputOrder(uScreenId, Stream (),
                                              pScreen->order.i32Op,
                                              pScreen->order.pvOrder);
                }
            }
        }
    }
    else if (i32Op == VRDP_ORDER_INTERNALSEQBEGIN)
    {
        SERVERLOG(("VRDPClient::processOutputOrder: [%u] VRDP_ORDER_INTERNALSEQBEGIN\n",
                   uScreenId));

        /* The INTERNALSEQ is a rare thing and is more complex than the normal orders.
         * Therefore the sequence is processed in a different code branch to keep
         * normal orders code path as fast as possible.
         */

        /* Clear old sequence. */
        orderSequenceDelete(pScreen);

        /* If bounds was saved, then apply bounds before sending the sequence. */
        if (pScreen->bounds.fBounds)
        {
            SERVERLOG(("VRDPClient::processOutputOrder: [%u] sending saved bounds\n",
                       uScreenId));

            pScreen->bounds.fBounds = false;

            rc = m_vrdptp.OutputOrder(uScreenId, Stream (), VRDE_ORDER_BOUNDS, &pScreen->bounds.savedBounds);
        }

        if (RT_SUCCESS(rc))
        {
            Assert(!pScreen->sequence.fSequenceInProcess);
            pScreen->sequence.fSequenceInProcess = true;

            /* Remember the screen id of the order. */
            p->uLastScreenId = uScreenId;

            /* Forward the order to the VRDPTP. */
            rc = m_vrdptp.OutputOrder(uScreenId, Stream (), i32Op, pvOrder);
        }
    }
    else if (i32Op == VRDP_ORDER_INTERNALSEQEND)
    {
        SERVERLOG(("VRDPClient::processOutputOrder: [%u] VRDP_ORDER_INTERNALSEQEND\n",
                   uScreenId));

        Assert(pScreen->sequence.fSequenceInProcess);
        Assert(!pScreen->sequence.fSavedSequence);

        /* Prepare the sequence for replaying. */
        orderSequenceReverse(pScreen);

        pScreen->sequence.fSequenceInProcess = false;
        pScreen->sequence.fSavedSequence = true;

        /* Remember the screen id of the order. */
        p->uLastScreenId = uScreenId;

        /* Forward the order to the VRDPTP. */
        rc = m_vrdptp.OutputOrder(uScreenId, Stream (), i32Op, pvOrder);
    }
    else
    {
        if (RT_UNLIKELY(pScreen->sequence.fSequenceInProcess))
        {
            /* Save this one order of a sequence. */
            SERVERLOG(("VRDPClient::processOutputOrder: [%u] saving sequence\n",
                       uScreenId));

            size_t cbAllocated = sizeof(VRDPSAVEDINTERNALORDER) + cbOrder;
            VRDPSAVEDINTERNALORDER *pSavedOrder = (VRDPSAVEDINTERNALORDER *)VRDPMemAlloc(cbAllocated);

            if (pSavedOrder)
            {
                pSavedOrder->i32Op = i32Op;
                pSavedOrder->cbInternalOrder = cbOrder;
                if (cbOrder != 0)
                {
                    memcpy (&pSavedOrder[1], pvOrder, cbOrder);
                }

                pSavedOrder->next = pScreen->sequence.pSavedInternalOrders;
                pScreen->sequence.pSavedInternalOrders = pSavedOrder;
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }
        }
        else
        {
            if (RT_UNLIKELY(pScreen->sequence.fSavedSequence))
            {
                /* A new order which is not a REPEAT and not the SEQEND arrived and
                 * no sequence is being accumulated. It overrides the saved sequence.
                 */
                orderSequenceDelete(pScreen);
            }

            /* Other orders. If bounds was saved, then apply bounds before sending the order. */
            if (pScreen->bounds.fBounds)
            {
                SERVERLOG(("VRDPClient::processOutputOrder: [%u] sending saved bounds\n",
                           uScreenId));

                pScreen->bounds.fBounds = false;

                rc = m_vrdptp.OutputOrder(uScreenId, Stream (), VRDE_ORDER_BOUNDS, &pScreen->bounds.savedBounds);
            }

            if (RT_SUCCESS(rc))
            {
                /* Save the order. */
                SERVERLOG(("VRDPClient::processOutputOrder: [%u] saving order\n",
                           uScreenId));

                void *pvSavedOrder = NULL;

                if (cbOrder <= sizeof(pScreen->order.au8SavedOrder))
                {
                    /* Use the preallocated buffer. */
                    pvSavedOrder = &pScreen->order.au8SavedOrder[0];
                }
                else
                {
                    /* Dynamically allocate a buffer for the order. */
                    if (cbOrder <= pScreen->order.cbAllocated)
                    {
                        /* Already have a big enough buffer. */
                        Assert(pScreen->order.pu8SavedOrder);
                        pvSavedOrder = pScreen->order.pu8SavedOrder;
                    }
                    else
                    {
                        SERVERLOG(("VRDPClient::processOutputOrder: [%u] allocating %d bytes for order %d\n",
                                   uScreenId, cbOrder, i32Op));

                        void *pv = VRDPMemAlloc(cbOrder);

                        if (pv)
                        {
                            VRDPMemFree(pScreen->order.pu8SavedOrder);

                            pScreen->order.pu8SavedOrder = (uint8_t *)pv;
                            pScreen->order.cbAllocated = cbOrder;

                            pvSavedOrder = pScreen->order.pu8SavedOrder;
                        }
                        else
                        {
                            rc = VERR_NO_MEMORY;
                        }
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    pScreen->order.i32Op = i32Op;
                    pScreen->order.cbOrder = cbOrder;
                    if (cbOrder > 0)
                    {
                        Assert(pvSavedOrder != NULL);
                        memcpy(pvSavedOrder, pvOrder, cbOrder);
                        pScreen->order.pvOrder = pvSavedOrder;
                    }
                    else
                    {
                        pScreen->order.pvOrder = NULL;
                    }
                }
            }
        }

        if (RT_SUCCESS(rc))
        {
            /* Remember the screen id of the order. */
            p->uLastScreenId = uScreenId;

            /* Forward the order to the VRDPTP. */
            rc = m_vrdptp.OutputOrder(uScreenId, Stream (), i32Op, pvOrder);
        }
    }

    return rc;
}
