/* $Id: iso.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_iso_h
#define VRDP_INCLUDED_SRC_iso_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpctx.h"

class ISOTP
{
    private:
        enum {
            TPDU_DR = 0x80,  /* Disconnect Request. */
            TPDU_CC = 0xD0,  /* Connection Confirm */
            TPDU_CR = 0xE0,  /* Connection Request */
            TPDU_DT = 0xF0   /* Data */
        };

        enum VRDP_ISO_Status {
           VRDP_ISO_Status_CR,
           VRDP_ISO_Status_DT
        };

        enum VRDP_ISO_Status m_enmStatus;

        int isoProcessCR (VRDPInputCtx *pInputCtx);

    public:
        ISOTP ();
        ~ISOTP ();

        void Reset (void);

        int Recv (VRDPInputCtx *pInputCtx);
        int Send (uint8_t u8Version, VRDPCtx *pCtx);
};

#endif /* !VRDP_INCLUDED_SRC_iso_h */
