/* $Id: usb.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol - Remote USB.
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

#include "vrdpserv.h"
#include "utils.h"

/*
 * USB VRDP channel implementation.
 */
static int appUSB (const VRDECALLBACKS_4 *pCallbacks,
                   void *pvCallback,
                   void *pvIntercept,
                   uint32_t u32ClientId,
                   uint8_t u8Code,
                   const void *pvRet,
                   uint32_t cbRet)
{
    int rc = VERR_NOT_SUPPORTED;

    if (pCallbacks && pCallbacks->VRDECallbackUSB)
    {
        rc = pCallbacks->VRDECallbackUSB (pvCallback, pvIntercept, u32ClientId, u8Code, pvRet, cbRet);
    }

    return rc;
}


typedef struct _USBTRANSACTION USBTRANSACTION;

void VRDPServer::ProcessUSBSend (uint32_t u32ClientId, const void *pvParm, uint32_t cbParm)
{
    USBLOG(("USB: ProcessUSBSend: client %d\n", u32ClientId));

    PostOutput (VRDP_OUTPUT_USB, u32ClientId, pvParm, cbParm);

    RaiseOutputEvent ();
}

VRDPChannelUSB::VRDPChannelUSB (VRDPTP *pvrdptp)
    :
    VRDPChannel (),
    m_pvrdptp (pvrdptp)
{
    initMembers ();
}

void VRDPChannelUSB::initMembers (void)
{
    m_channelOptions = 0;
    m_channelId = 0;

    m_fOperational = false;
    m_fSetup = false;

    m_pvCallbackUser = NULL;
}

VRDPChannelUSB::~VRDPChannelUSB ()
{
    Close ();
}

/// @todo -> VRDPChannel
int VRDPChannelUSB::Open (uint32_t options, uint16_t id)
{
    int rc = VINF_SUCCESS;

    m_channelOptions = options;
    m_channelId = id;

    InitOutgoingChannelId (m_channelId);

    return rc;
}

void VRDPChannelUSB::Setup (void)
{
    USBLOG(("USB: Setup\n"));

    if (m_channelId != 0)
    {
        int rc = m_pvrdptp->Desktop ()->InterceptChannel (m_pvrdptp->Client (), VRDE_CLIENT_INTERCEPT_USB, &m_pvCallbackUser);

        if (RT_SUCCESS (rc))
        {
            m_fSetup = true;
        }
    }

    return;
}

void VRDPChannelUSB::Close (void)
{
    initMembers ();
}


void VRDPChannelUSB::Start (void)
{
    USBLOG(("USB: Start\n"));

    if (m_fSetup && !m_fOperational)
    {
        m_fOperational = true;
    }
}

/** @thread output */
int VRDPChannelUSB::Send (void *pvParm, uint32_t cbParm)
{
    int rc = VINF_SUCCESS;

    USBLOG(("USB: Send %d\n", m_fOperational));

    if (!m_fOperational)
    {
        return rc;
    }

    USBLOG(("USB: VRDPChannelUSB::SendOutput: pvParm = %p, cbParm = %d, code %d\n", pvParm, cbParm, pvParm? *(uint8_t *)pvParm: 0));

    /* A Reset is not necessary because the contexts automatically resets after sending a packet. */
    Assert (IsReset ());

    /* Send the USB request. */

    uint8_t code = *(uint8_t *)pvParm;

    VRDPLOGRELIO(("USB_R send %d:\n%.*Rhxd\n", cbParm, cbParm, pvParm));

    if (code == VRDE_USB_REQ_QUEUE_URB)
    {
        VRDE_USB_REQ_QUEUE_URB_PARM *pParm = (VRDE_USB_REQ_QUEUE_URB_PARM *)pvParm;

        VRDPBUFFER aBuffers[2];

        aBuffers[0].pv = pParm;
        aBuffers[0].cb = sizeof (*pParm) - sizeof (pParm->data);

        aBuffers[1].pv = pParm->data;
        aBuffers[1].cb = pParm->datalen;

        VRDPLOGRELIO(("USB_R send URB %d:\n%.*Rhxd\n", pParm->datalen, pParm->datalen, pParm->data));

        rc = m_pvrdptp->SendToChannel (this /* as OutputCtx */, m_channelId, m_channelOptions, RT_ELEMENTS (aBuffers), aBuffers, true);
    }
    else
    {
        VRDPBUFFER aBuffers[1];

        aBuffers[0].pv = pvParm;
        aBuffers[0].cb = cbParm;

        rc = m_pvrdptp->SendToChannel (this /* as OutputCtx */, m_channelId, m_channelOptions, RT_ELEMENTS (aBuffers), aBuffers, true);
    }

    return rc;
}

int VRDPChannelUSB::ProcessChannelInput (const uint8_t *pu8Input, uint32_t u32Length)
{
    int rc = VINF_SUCCESS;

    if (!m_fOperational)
    {
        return rc;
    }

    VRDPLOGRELIO(("USB_R recv %d:\n%.*Rhxd\n", u32Length, u32Length, pu8Input));

    if (u32Length < sizeof (VRDEUSBPKTHDR)) /* expecting len and code */
    {
        USBLOG(("not enough data %d\n", u32Length));
        return rc;
    }

    VRDEUSBPKTHDR *pPktHdr = (VRDEUSBPKTHDR *)pu8Input;

    if (u32Length < pPktHdr->length + sizeof (pPktHdr->length))
    {
        USBLOG(("not enough data len = %d, bytesleft = %d\n", pPktHdr->length, u32Length - sizeof (pPktHdr->length)));
        return rc;
    }

    pu8Input += sizeof (VRDEUSBPKTHDR);
    u32Length -= sizeof (VRDEUSBPKTHDR);

    USBLOG(("USB: VRDPChannelUSB::ProcessChannelInput: len = %d, code = %d\n", pPktHdr->length, pPktHdr->code));

    appUSB (m_pvrdptp->Client ()->Server ()->ApplicationCallbacks (),
            m_pvrdptp->Client ()->Server ()->ApplicationCallbackPointer (),
            m_pvCallbackUser,
            m_pvrdptp->Client ()->Id(),
            pPktHdr->code,
            u32Length > 0? pu8Input: NULL,
            u32Length);

    return rc;
}
