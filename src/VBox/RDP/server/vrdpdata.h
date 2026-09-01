/* $Id: vrdpdata.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vrdpdata_h
#define VRDP_INCLUDED_SRC_vrdpdata_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/*
 * Interface for storing the RDP connection properties.
 * It is used by all protocol levels (transport, ISO, MCS, SEC, RDP).
 * For example ISO saves there the information about RDP Negotiation Response:
 * if it was received and requestedProtocols.
 *
 */

#define PROTOCOL_RDP    0x00000000
#define PROTOCOL_SSL    0x00000001
#define PROTOCOL_HYBRID 0x00000002

/* Return values for RDP_NEG_FAILURE */
#define SSL_REQUIRED_BY_SERVER 0x00000001
#define SSL_NOT_ALLOWED_BY_SERVER 0x00000002
#define SSL_CERT_NOT_ON_SERVER 0x00000003
#define INCONSISTENT_FLAGS 0x00000004
#define HYBRID_REQUIRED_BY_SERVER 0x00000005
#define SSL_WITH_USER_AUTH_REQUIRED_BY_SERVER 0x00000006

/* Caps flags for TS_UD_CS_CORE::u16EarlyCapabilityFlags. */
#define RNS_UD_CS_SUPPORT_ERRINFO_PDU 0x0001
#define RNS_UD_CS_WANT_32BPP_SESSION 0x0002
#define RNS_UD_CS_SUPPORT_STATUSINFO_PDU 0x0004
#define RNS_UD_CS_STRONG_ASYMMETRIC_KEYS 0x0008
#define RNS_UD_CS_VALID_CONNECTION_TYPE 0x0020
#define RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU 0x0040

#define TS_MONITOR_PRIMARY 0x00000001

/* Maximum number of monitors supported by RDP in multimonitor mode. */
#define RDP_MAX_SCREENS 16

typedef struct TS_MONITOR_DEF
{
    /* All coords are inclusive, that is for a 800x600: i32Left = 0, i32Right = 799. */
    int32_t  i32Left;
    int32_t  i32Top;
    int32_t  i32Right;
    int32_t  i32Bottom;
    uint32_t u32Flags; /* TS_MONITOR_* */
} TS_MONITOR_DEF;

#define DRAW_ALLOW_DYNAMIC_COLOR_FIDELITY 0x02
#define DRAW_ALLOW_COLOR_SUBSAMPLING 0x04
#define DRAW_ALLOW_SKIP_ALPHA 0x08

/* TS_ORDER_CAPABILITYSET::orderFlags */
#define TS_NEGOTIATEORDERSUPPORT   0x0002
#define TS_ZEROBOUNDSDELTASSUPPORT 0x0008
#define TS_COLORINDEXSUPPORT       0x0020
#define TS_SOLIDPATTERNBRUSHONLY   0x0040
#define TS_ORDERFLAGS_EXTRA_FLAGS  0x0080

/* TS_ORDER_CAPABILITYSET::orderSupportExFlags */
#define TS_ORDERFLAGS_EX_CACHE_BITMAP_REV3_SUPPORT   0x0002
#define TS_ORDERFLAGS_EX_ALTSEC_FRAME_MARKER_SUPPORT 0x0004

class VRDPTP;

class VRDPData
{
    public:

        VRDPData (VRDPTP *pTP);
        ~VRDPData ();

        VRDPTP *TP(void) { return m_pTP; }

        bool RDPNegotiationRequest(void);
        void RDPNegotiationRequest(bool fSet);

        uint32_t RDPNegotiationRequestedProtocols(void);
        void RDPNegotiationRequestedProtocols(uint32_t u32Value);

        uint32_t RDPNegotiationSelectedProtocol(void);
        void RDPNegotiationSelectedProtocol(uint32_t u32Value);

        uint16_t EarlyCapabilityFlags(void);
        void EarlyCapabilityFlags(uint16_t u16Value);

        uint16_t DesktopWidth(void);
        void DesktopWidth(uint16_t u16Value);

        uint16_t DesktopHeight(void);
        void DesktopHeight(uint16_t u16Value);

        bool MonitorLayout(void);
        void MonitorLayout(bool fSet);

        uint32_t MonitorCount(void);
        void MonitorCount(uint32_t u32Value);

        void MonitorDef(uint32_t index, const TS_MONITOR_DEF *pMonitor);
        const TS_MONITOR_DEF *MonitorDef(uint32_t index);

        bool EnhancedSecurity(void);
        void EnhancedSecurity(bool fSet);

        const uint8_t *ClientDigProductId(void);
        void ClientDigProductId(const uint8_t *pValue);

        bool NewMousePointers(void);
        void NewMousePointers(bool fSet);

        bool LargeMousePointers(void);
        void LargeMousePointers(bool fSet);

        uint16_t OrderFlags(void);
        void OrderFlags(uint16_t u16Value);

        uint16_t OrderSupportExFlags(void);
        void OrderSupportExFlags(uint16_t u16Value);

    private:

        VRDPTP *m_pTP;

        bool m_fRDPNegotiationRequest;
        uint32_t m_u32RDPNegotiationRequestedProtocols;
        uint32_t m_u32RDPNegotiationSelectedProtocol;

        uint16_t m_u16EarlyCapabilityFlags;

        uint16_t m_u16DesktopWidth;
        uint16_t m_u16DesktopHeight;

        bool m_fMonitorLayout;
        uint32_t m_u32MonitorCount;
        struct {
            bool fSet;
            TS_MONITOR_DEF monitor;
        } m_aMonitors[RDP_MAX_SCREENS];

        bool m_fEnhancedSecurity;

        bool m_fNewMousePointers;
        bool m_fLargeMousePointers;

        uint8_t m_au8ClientDigProductId[64];

        uint16_t m_u16OrderFlags;
        uint16_t m_u16OrderSupportExFlags;
};

#endif /* !VRDP_INCLUDED_SRC_vrdpdata_h */
