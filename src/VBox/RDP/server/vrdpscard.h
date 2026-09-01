/* $Id: vrdpscard.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vrdpscard_h
#define VRDP_INCLUDED_SRC_vrdpscard_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/RemoteDesktop/VRDESCard.h>

#include "vrdp.h"


class VRDPServer;

typedef struct VRDPSCARDDEVICE VRDPSCARDDEVICE;

typedef struct VRDPSCARDNDRCTX VRDPSCARDNDRCTX;

typedef bool FNSCARDDECODE(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult);
typedef FNSCARDDECODE *PFNSCARDDECODE;

typedef bool FNSCARDENCODE(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms);
typedef FNSCARDENCODE *PFNSCARDENCODE;

class VRDPSCard
{
    private:

        VRDPServer *m_pServer;

        bool m_fShutdown;

        struct
        {
            VRDESCARDCALLBACKS callbacks;
            void *pvContext;
        } m_interface;

        VRDPLock *m_pLock;

        /* List of SCARD RDPDR devices. Devices are referenced. */
        RTLISTANCHOR m_ListSCardDevices;

        int scInitialize(void);
        void scShutdown(void);

        VRDPSCARDDEVICE *scDeviceFind(uint32_t u32ClientId, uint32_t u32DeviceId);
        VRDPSCARDDEVICE *scDeviceFindByContext(const VRDESCARDCONTEXT *pContext);
        VRDPSCARDDEVICE *scDeviceAdd(uint32_t u32ClientId, uint32_t u32DeviceId);
        void scDeviceRemove(VRDPSCARDDEVICE *pDev);
        VRDPSCARDDEVICE *scDeviceFirst(void);
        VRDPSCARDDEVICE *scDeviceNext(VRDPSCARDDEVICE *pDev);

        int scDecode(const uint8_t *pu8Buffer, uint32_t cbBuffer,  uint32_t u32ReturnedOutputLength,
                     PFNSCARDDECODE pfnDecode, void *pvResult, uint32_t cbResult);
        int scEncode(uint8_t **ppu8Buffer, uint32_t *pcbBuffer, uint32_t cbEncodedEstimate,
                     PFNSCARDENCODE pfnEncode, const void *pvParms, uint32_t cbParms);

        int scEncodeIOCTL(uint8_t **ppu8Buffer, uint32_t *pcbBuffer,
                          uint32_t u32Code, const void *pvParms, uint32_t cbParms);
        int scSubmitIOCTL(uint32_t u32Code, VRDPSCARDDEVICE *pDev,
                          const void *parms, uint32_t cbParms,
                          void *pvUser, uint32_t u32VRDEFunction);

        int scProcessIOResponse(VRDPSCARDDEVICE *pDev, RDPDRIOCTL *pIO);

        int scCallbackNotify(uint32_t u32Id, void *pvData, uint32_t cbData);
        int scCallbackResponse(int rcRequest, void *pvUser, uint32_t u32VRDEFunction,
                               void *pvData, uint32_t cbData);

    public:

        VRDPSCard(VRDPServer *pServer);
        virtual ~VRDPSCard();

        int GetInterfaceSCard(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks, void *pvContext);

        int SCardInitialize(void);
        void SCardShutdown(void);

        int SCardRequest(void *pvUser, uint32_t u32Function, const void *pvData, uint32_t cbData);

        int SCardAttach(uint32_t u32ClientId, uint32_t u32DeviceId);
        int SCardDetach(uint32_t u32ClientId, uint32_t u32DeviceId);
        int SCardIOCompletion(uint32_t u32ClientId, uint32_t u32DeviceId,
                              uint32_t u32IoStatus, RDPDRIOCTL *pIO);
};

#endif /* !VRDP_INCLUDED_SRC_vrdpscard_h */
