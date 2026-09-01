/* $Id: vrdpvideoin.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vrdpvideoin_h
#define VRDP_INCLUDED_SRC_vrdpvideoin_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VRDE_VIDEOIN_WITH_VRDEINTERFACE
#include <VBox/RemoteDesktop/VRDEVideoIn.h>

#include "vrdp.h"
#include "vrdpclient.h"

class VRDPServer;
typedef struct VRDPOUTPUTGENERICHDR VRDPOUTPUTGENERICHDR;

typedef struct VIDEOINCHANNEL VIDEOINCHANNEL;
typedef struct VRDPVIDEOINDEVICE VRDPVIDEOINDEVICE;

/*
 * Per client VIDEOIN channel instance.
 */
class VideoInClient
{
    private:
        VRDPClient *m_pClient;

        uint8_t m_u8DVCChannelId;

        static void dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData);
        void processInput(uint32_t u32Event, const void *pvData, uint32_t cbData);

        static DECLCALLBACK(void) processOutput(VRDPClient *pClient, const VRDPOUTPUTGENERICHDR *pHdr);
        static DECLCALLBACK(void) processCloseChannel(VRDPClient *pClient, const VRDPOUTPUTGENERICHDR *pHdr);

    public:
        VideoInClient(VRDPClient *pClient);
        ~VideoInClient();

        int RegisterChannel(void); /* Registers the VIDEOIN channel with DVC. */
        void CloseChannel(void); /* Closes the VIDEOIN channel. */

        int SendData(const void *pvData, uint32_t cbData);

        VRDPClient *Client(void) { return m_pClient; }
};

class VRDPVideoIn
{
    private:

        VRDPServer *m_pServer;

        bool m_fShutdown;

        struct
        {
            VRDEVIDEOINCALLBACKS callbacks;
            void *pvContext;
        } m_interface;

        VRDPLock *m_pLock;

        /* List of VIDEOIN channels. */
        RTLISTANCHOR m_listChannels;
        /* List of VIDEOIN devices. */
        RTLISTANCHOR m_listDevices;

        struct
        {
            uint32_t volatile u32IdSrc;
            uint32_t cIds;
            RTLISTANCHOR ListFree;
            RTLISTANCHOR ListUsed;
        } m_IOCompletion;

        int viInitialize(void);
        void viShutdown(void);

        int viChannelAdd(VIDEOINCHANNEL *pChannel);
        void viChannelRemove(VIDEOINCHANNEL *pChannel);
        VIDEOINCHANNEL *viChannelFind(uint32_t u32ClientId);

        VRDPVIDEOINDEVICE *viDeviceAdd(VIDEOINCHANNEL *pChannel, uint32_t u32DeviceId);
        void viDeviceRemove(VRDPVIDEOINDEVICE *pDev);
        VRDPVIDEOINDEVICE *viDeviceFind(uint32_t u32ClientId, uint32_t u32DeviceId);
        VRDPVIDEOINDEVICE *viDeviceFirst(void);
        VRDPVIDEOINDEVICE *viDeviceNext(VRDPVIDEOINDEVICE *pDev);

        int viCallbackNotify(uint32_t u32NotifyEvent, const void *pvData, uint32_t cbData);
        int viCallbackDeviceDesc(int rcRequest, void *pvDeviceCtx, void *pvUser, const VRDEVIDEOINDEVICEDESC *pDevice, uint32_t cbDevice);
        int viCallbackControl(int rcRequest, void *pvDeviceCtx, void *pvUser, const VRDEVIDEOINCTRLHDR *pControl, uint32_t cbControl);
        int viCallbackFrame(int rcRequest, void *pvDeviceCtx, const VRDEVIDEOINPAYLOADHDR *pFrame, uint32_t cbFrame);

        int viStatusToRC(uint16_t u16Status);

        int viOnNotify(VideoInClient *pClientChannel, const VRDEVIDEOINMSGHDR *pHdr, uint32_t cbMsg);
        int viOnDeviceDesc(VideoInClient *pClientChannel, const VRDEVIDEOINMSGHDR *pHdr, uint32_t cbMsg);
        int viOnControl(VideoInClient *pClientChannel, const VRDEVIDEOINMSGHDR *pHdr, uint32_t cbMsg);
        int viOnControlNotify(VideoInClient *pClientChannel, const VRDEVIDEOINMSGHDR *pHdr, uint32_t cbMsg);
        int viOnFrame(VideoInClient *pClientChannel, const VRDEVIDEOINMSGHDR *pHdr, uint32_t cbMsg);
        int viOnControlCommon(VideoInClient *pClientChannel, const VRDEVIDEOINMSGHDR *pHdr, uint32_t cbMsg, void *pvUser);

        int viOnDeviceAttach(VideoInClient *pClientChannel, uint32_t u32DeviceId);
        int viOnDeviceDetach(VideoInClient *pClientChannel, uint32_t u32DeviceId);
        int viOnNegotiate(VideoInClient *pClientChannel);

        int viDeviceDetach(VideoInClient *pClientChannel, VRDPVIDEOINDEVICE *pDev);

        int createIO(uint32_t *pu32CompletionId, void *pvCtx);
        int fetchIO(uint32_t u32CompletionId, void **ppvCtx);

        static DECLCALLBACK(void) processSubmitMsg(VRDPClient *pClient, const VRDPOUTPUTGENERICHDR *pHdr);
        int viSubmitMsg(uint16_t u16FunctionId, VRDPVIDEOINDEVICE *pDev, const void *pvParms, uint32_t cbParms, void *pvUser);

        int viSendNegotiate(VideoInClient *pClientChannel);
        void viChannelCleanup(VIDEOINCHANNEL *pChannel);

    public:

        VRDPVideoIn(VRDPServer *pServer);
        virtual ~VRDPVideoIn();

        int GetInterfaceVideoIn(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks, void *pvContext);

        int VideoInInitialize(void);
        void VideoInShutdown(void);

        /* The interface methods. */
        int VideoInDeviceAttach(const VRDEVIDEOINDEVICEHANDLE *pDeviceHandle, void *pvDeviceCtx);
        int VideoInDeviceDetach(const VRDEVIDEOINDEVICEHANDLE *pDeviceHandle);
        int VideoInGetDeviceDesc(void *pvUser, const VRDEVIDEOINDEVICEHANDLE *pDeviceHandle);
        int VideoInControl(void *pvUser, const VRDEVIDEOINDEVICEHANDLE *pDeviceHandle,
                           const VRDEVIDEOINCTRLHDR *pReq, uint32_t cbReq);

        /* Methods for the VRDP server components. */
        int VideoInOnCreate(VideoInClient *pClientChannel);
        int VideoInOnData(VideoInClient *pClientChannel,
                          const void *pvData, uint32_t cbData);
        int VideoInOnClose(VideoInClient *pClientChannel);
};

#endif /* !VRDP_INCLUDED_SRC_vrdpvideoin_h */
