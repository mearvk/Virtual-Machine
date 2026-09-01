/* $Id: vrdpinput.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
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

#ifndef VRDP_INCLUDED_SRC_vrdpinput_h
#define VRDP_INCLUDED_SRC_vrdpinput_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/RemoteDesktop/VRDEInput.h>

#include "vrdp.h"
#include "vrdpclient.h"

class VRDPServer;
typedef struct VRDPOUTPUTGENERICHDR VRDPOUTPUTGENERICHDR;

typedef struct INPUTCHANNEL INPUTCHANNEL;

/*
 * Per client channel instance.
 */
class InputClient
{
    private:
        VRDPClient *m_pClient;

        uint8_t m_u8DVCChannelId;

        static void dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData);
        void processInput(uint32_t u32Event, const void *pvData, uint32_t cbData);

        static DECLCALLBACK(void) processOutput(VRDPClient *pClient, const VRDPOUTPUTGENERICHDR *pHdr);
        static DECLCALLBACK(void) processCloseChannel(VRDPClient *pClient, const VRDPOUTPUTGENERICHDR *pHdr);

    public:
        InputClient(VRDPClient *pClient);
        ~InputClient();

        int RegisterChannel(void); /* Registers the channel with DVC. */
        void CloseChannel(void); /* Closes the channel. */

        int SendData(const void *pvData, uint32_t cbData);

        VRDPClient *Client(void) { return m_pClient; }
};

class VRDPInput
{
    private:

        VRDPServer *m_pServer;

        bool m_fShutdown;

        struct
        {
            VRDEINPUTCALLBACKS callbacks;
            void *pvContext;
        } m_interface;

        VRDPLock *m_pLock;

        /* List of INPUT channels. */
        RTLISTANCHOR m_listChannels;

        int inInitialize(void);
        void inShutdown(void);

        int inChannelAdd(INPUTCHANNEL *pChannel);
        void inChannelRemove(INPUTCHANNEL *pChannel);
        INPUTCHANNEL *inChannelFind(uint32_t u32ClientId);

        int inCallbackSetup(int rcRequest, const void *pvResult, uint32_t cbResult);
        int inCallbackEvent(const void *pvEvent, uint32_t cbEvent);

        int inOnTouch(InputClient *pClientChannel, const VRDEINPUTHEADER *pHdr, uint32_t cbMsg);
        int inOnDismiss(InputClient *pClientChannel, const VRDEINPUTHEADER *pHdr, uint32_t cbMsg);

        int inSendReady(InputClient *pClientChannel);

        void inChannelCleanup(INPUTCHANNEL *pChannel);

    public:

        VRDPInput(VRDPServer *pServer);
        virtual ~VRDPInput();

        int GetInterfaceInput(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks, void *pvContext);

        int InputInitialize(void);
        void InputShutdown(void);

        /* The interface methods. */
        void InputSetup(uint32_t u32Method, uint32_t fu32Flags, const void *pvSetup, uint32_t cbSetup);

        /* Methods for the VRDP server components. */
        int InputOnCreate(InputClient *pClientChannel);
        int InputOnData(InputClient *pClientChannel,
                        const void *pvData, uint32_t cbData);
        int InputOnClose(InputClient *pClientChannel);
};

#endif /* !VRDP_INCLUDED_SRC_vrdpinput_h */
