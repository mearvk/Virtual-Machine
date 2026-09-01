/* $Id: vrdpdata.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "vrdp.h"

VRDPData::VRDPData (VRDPTP *pTP)
    :
    m_pTP(pTP),
    m_fRDPNegotiationRequest(false),
    m_u32RDPNegotiationRequestedProtocols(0),
    m_u32RDPNegotiationSelectedProtocol(0),
    m_u16EarlyCapabilityFlags(0),
    m_u16DesktopWidth(0),
    m_u16DesktopHeight(0),
    m_fMonitorLayout(false),
    m_u32MonitorCount(0),
    m_fEnhancedSecurity(false),
    m_fNewMousePointers(false),
    m_fLargeMousePointers(false),
    m_u16OrderFlags(0),
    m_u16OrderSupportExFlags(0)
{
    memset(m_aMonitors, 0, sizeof(m_aMonitors));
    memset(m_au8ClientDigProductId, 0, sizeof(m_au8ClientDigProductId));
}

VRDPData::~VRDPData ()
{
}

bool VRDPData::RDPNegotiationRequest(void)
{
    return m_fRDPNegotiationRequest;
}

void VRDPData::RDPNegotiationRequest(bool fSet)
{
    m_fRDPNegotiationRequest = fSet;
}

uint32_t VRDPData::RDPNegotiationRequestedProtocols(void)
{
    return m_u32RDPNegotiationRequestedProtocols;
}

void VRDPData::RDPNegotiationRequestedProtocols(uint32_t u32Value)
{
    m_u32RDPNegotiationRequestedProtocols = u32Value;
}

uint32_t VRDPData::RDPNegotiationSelectedProtocol(void)
{
    return m_u32RDPNegotiationSelectedProtocol;
}

void VRDPData::RDPNegotiationSelectedProtocol(uint32_t u32Value)
{
    m_u32RDPNegotiationSelectedProtocol = u32Value;
}

#define DEF_VRDPDATA_PROPERTY(name, type, prefix) \
    type VRDPData::name(void)                     \
    {                                             \
        return m_##prefix##name;                  \
    }                                             \
                                                  \
    void VRDPData::name(type prefix##Value)       \
    {                                             \
        m_##prefix##name = prefix##Value;         \
    }

#define DEF_VRDPDATA_PROPERTY_BOOL(name) DEF_VRDPDATA_PROPERTY(name, bool, f)
#define DEF_VRDPDATA_PROPERTY_U16(name) DEF_VRDPDATA_PROPERTY(name, uint16_t, u16)
#define DEF_VRDPDATA_PROPERTY_U32(name) DEF_VRDPDATA_PROPERTY(name, uint32_t, u32)

DEF_VRDPDATA_PROPERTY_U16(EarlyCapabilityFlags)

DEF_VRDPDATA_PROPERTY_U16(DesktopWidth)
DEF_VRDPDATA_PROPERTY_U16(DesktopHeight)

DEF_VRDPDATA_PROPERTY_BOOL(MonitorLayout)

uint32_t VRDPData::MonitorCount(void)
{
    return m_u32MonitorCount;
}

void VRDPData::MonitorCount(uint32_t u32Value)
{
    if (u32Value > RT_ELEMENTS(m_aMonitors))
    {
        u32Value = RT_ELEMENTS(m_aMonitors);
    }

    m_u32MonitorCount = u32Value;
}

void VRDPData::MonitorDef(uint32_t index, const TS_MONITOR_DEF *pMonitor)
{
    if (index >= m_u32MonitorCount)
    {
        AssertFailed();
        return;
    }

    m_aMonitors[index].fSet = true;
    m_aMonitors[index].monitor = *pMonitor;
}

const TS_MONITOR_DEF *VRDPData::MonitorDef(uint32_t index)
{
    if (   index >= m_u32MonitorCount
        || !m_aMonitors[index].fSet)
    {
        AssertFailed();
        return NULL;
    }

    return &m_aMonitors[index].monitor;
}

DEF_VRDPDATA_PROPERTY_BOOL(EnhancedSecurity)

const uint8_t *VRDPData::ClientDigProductId(void)
{
    return &m_au8ClientDigProductId[0];
}

void VRDPData::ClientDigProductId(const uint8_t *pValue)
{
    memcpy(m_au8ClientDigProductId, pValue, 64);
}

DEF_VRDPDATA_PROPERTY_BOOL(NewMousePointers)
DEF_VRDPDATA_PROPERTY_BOOL(LargeMousePointers)

DEF_VRDPDATA_PROPERTY_U16(OrderFlags)
DEF_VRDPDATA_PROPERTY_U16(OrderSupportExFlags)
