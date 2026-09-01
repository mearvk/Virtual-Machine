/* $Id: utcinfo.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_utcinfo_h
#define VRDP_INCLUDED_SRC_utcinfo_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpdefs.h"

#include <VBox/RemoteDesktop/VRDE.h>

#define VRDP_OUTPUT_UTCINFO_DATA 50
#define VRDP_OUTPUT_DVC_CLOSE 51

#define UTCINFO_SERVER_VERSION 2

class VRDPClient;

class UTCINFOClient
{
    private:
        VRDPClient *m_pClient;

        uint8_t m_u8ChannelId;

        uint8_t m_u8ClientVersion;

        struct ClientCaps
        {
            struct
            {
                uint32_t u32Capabilities;
            } general;
            struct
            {
                uint8_t u8MonitorCapabilities;
                uint16_t u16MaxMonitorsSupported;
            } monitor;
        } m_clientCaps;

        static void dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData);
        void processInput(uint32_t u32Event, const void *pvData, uint32_t cbData);

        void postOutput (int iEvent, const void *pvData, uint32_t cbData, bool fRaiseEvent);

        int processClientVersionInfo(const void *pvData, uint32_t cbData);
        int processMonitorLayout(const void *pvData, uint32_t cbData);

    public:
        UTCINFOClient(VRDPClient *pClient);
        ~UTCINFOClient();

        int RegisterChannel(void);
        void DVCClose (int rc);
        void SendData(void *pvData, uint32_t cbData);
#ifdef DEBUG_sunlover
        void testMonitor(void);
        void testHotdesking(void);
        void testLimits(void);
        void testNameChange(void);
#endif
};

#endif /* !VRDP_INCLUDED_SRC_utcinfo_h */
