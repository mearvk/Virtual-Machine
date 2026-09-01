/* $Id: tsmf.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2012-2025 Oracle and/or its affiliates.
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

#ifndef VRDP_INCLUDED_SRC_tsmf_h
#define VRDP_INCLUDED_SRC_tsmf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpdefs.h"

#include <VBox/RemoteDesktop/VRDETSMF.h>

#define VRDP_OUTPUT_TSMFRAW   70

class VRDPClient;
class TSMFRaw;

typedef struct TSMFRAWCHANNEL
{
    TSMFRaw *pTSMFRaw;
    uint32_t volatile u32Status;
    uint32_t u32Index;
    uint32_t u32ChannelId;
    uint8_t u8DVCChannelId;
    void *pvContext;
} TSMFRAWCHANNEL;

typedef struct TSMFRAWDATA TSMFRAWDATA;

typedef struct TSMFRAWCHANNELCTX TSMFRAWCHANNELCTX;

/*
 * Per client TSMF channel manager.
 */
class TSMFRaw
{
    private:
        VRDPClient *m_pClient;

        TSMFRAWCHANNEL m_aChannels[64]; /* Arbitrary number, enough for testing. */

        int registerChannel(uint32_t *pu32ChannelId, void *pvContext);
        int closeChannel(uint32_t u32ChannelId);
        TSMFRAWCHANNEL *findChannel(uint32_t u32ChannelId);

        static void dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData);
        void processInput(TSMFRAWCHANNEL *pInstance, uint32_t u32Event, const void *pvData, uint32_t cbData);

        void destroy(void);

    public:
        TSMFRaw(VRDPClient *pClient);
        ~TSMFRaw();

        void TSMFRawOutput(void *pvData, uint32_t cbData);
};

/*
 * Server wide TSMF manager.
 */
class VRDPTSMF
{
    private:

        VRDPServer *m_pServer;

        bool m_fShutdown;

        struct
        {
            VRDETSMFCALLBACKS callbacks;
            void *pvContext;
        } m_interface;

        VRDPLock *m_pLock;

        uint32_t volatile m_u32HandleSrc;

        /* The currently active DVC client, which will be used for TSMF. 0 if not client. */
        uint32_t m_u32ClientId;

        /* List of channels. Channels are referenced. TSMFRAWCHANNELCTX */
        RTLISTANCHOR m_ListChannels;

        int tsmfChannelCtxCreate(TSMFRAWCHANNELCTX **ppContext, uint32_t u32ClientId, void *pvChannel, uint32_t u32Flags);
        int tsmfChannelCtxFind(TSMFRAWCHANNELCTX **ppContext, uint32_t u32ChannelHandle);
        void tsmfChannelCtxDelete(TSMFRAWCHANNELCTX *pContext);

        /* Interface callback. */
        void tsmfCbNotify(uint32_t u32Notification, void *pvChannel,
                          const void *pvParm, uint32_t cbParm);

    public:

        VRDPTSMF(VRDPServer *pServer);
        virtual ~VRDPTSMF();

        int GetInterfaceTSMF(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks, void *pvContext);

        int TSMFInitialize(void);
        void TSMFShutdown(void);

        /* Interface entry points. */
        int TSMFChannelCreate(void *pvChannel, uint32_t u32Flags);
        int TSMFChannelClose(uint32_t u32ChannelHandle);
        int TSMFChannelSend(uint32_t u32ChannelHandle, const void *pvData, uint32_t cbData);

        /* Clients events. */
        void TSMFClientConnectedDVC(uint32_t u32ClientId);
        void TSMFClientDisconnectedDVC(uint32_t u32ClientId);

        int TSMFOnCreate(uint32_t u32ClientId, TSMFRAWCHANNELCTX *pChannel,
                         uint32_t u32ChannelId, int rcCreate);
        int TSMFOnData(uint32_t u32ClientId, TSMFRAWCHANNELCTX *pChannel,
                       const void *pvData, uint32_t cbData);
        int TSMFOnClose(uint32_t u32ClientId, TSMFRAWCHANNELCTX *pChannel);
};

#endif /* !VRDP_INCLUDED_SRC_tsmf_h */
