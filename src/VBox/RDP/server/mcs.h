/* $Id: mcs.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_mcs_h
#define VRDP_INCLUDED_SRC_mcs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "iso.h"

#define MCS_GLOBAL_CHANNEL      1003
#define MCS_USERCHANNEL_BASE    1001

class MCSTP
{
    private:
        ISOTP m_isotp;
        uint16_t m_mcsuserid;

        enum VRDP_MCS_Status {
           VRDP_MCS_Status_RecvConnectInitial,
           VRDP_MCS_Status_MCS
        };

        enum VRDP_MCS_Status m_enmStatus;

    public:
        MCSTP ();
        ~MCSTP ();

        void Reset (void);
        int Send (uint8_t u8Version, VRDPCtx *pCtx);
        int Recv (VRDPInputCtx *pInputCtx);
        void Disconnect (VRDPInputCtx *pInputCtx);
        int SendConnectResponse (VRDPInputCtx *pInputCtx, uint8_t u8Result, int cChannels);
};

#endif /* !VRDP_INCLUDED_SRC_mcs_h */
