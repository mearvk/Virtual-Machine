/* $Id: vrdpctx.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */
/*
 * Copyright (C) 2004-2025 Oracle and/or its affiliates.
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

#include "vrdpctx.h"

#include "mcs.h"

#ifdef VRDP_LOG_ALL_PACKETS
#define DUMPWRITEREL do { DumpWriteRel (); } while (0)
#define DUMPREADREL do { DumpReadRel (); } while (0)
#else
#define DUMPWRITEREL do { } while (0)
#define DUMPREADREL do { } while (0)
#endif /* VRDP_LOG_ALL_PACKETS */

#define VRDP_ISO_HDR_LEN 4

VRDPCtx::VRDPCtx ()
    :
    m_u16OutgoingChannelId (0),
    m_pTransport (0),
    m_id (NIL_VRDPTRANSPORTID),
    m_pData (NULL)
{
}

VRDPCtx::~VRDPCtx ()
{
}

void VRDPCtx::SetTransport (VRDPTransport *pTransport, VRDPTRANSPORTID id)
{
    m_pTransport = pTransport;
    m_id = id;
}

VRDPInputCtx::VRDPInputCtx ()
{
    Reset ();
    InitOutgoingChannelId (MCS_GLOBAL_CHANNEL);
}

VRDPInputCtx::~VRDPInputCtx ()
{
}

void VRDPInputCtx::DumpRead (void)
{
    uint16_t u16 = BytesToRead ();

    VRDPLOG(("InputCtx:DumpRead %d bytes\n", u16));

    if (u16)
    {
        VRDPLOG(("%.*Rhxd\n", u16, m_pu8ToRead));
    }
}


void VRDPInputCtx::DumpReadRel (void)
{
    uint16_t u16 = (uint16_t)(m_pu8ToRecv - &m_au8DataRecv[0]);

    if (u16)
    {
        VRDPLOGREL(("The RDP packet content (read):\n\n%.*Rhxd\n\n", u16, m_au8DataRecv));
    }
}


void VRDPInputCtx::Reset (void)
{
    SERVERLOG(("InputCtx:Reset\n"));

    m_fISOHdrFetched        = false;

    m_pu8ToRecv             = &m_au8DataRecv[0];
    m_cbToRecv              = VRDP_ISO_HDR_LEN;

    m_pu8ToRead             = &m_au8DataRecv[0];

    m_pu8LowerProtocolStart = &m_au8DataSend[sizeof (m_au8DataSend)];

    /* Reset the channel id, upon reception of a new packet it will be set. */
    m_u16IncomingChannelId  = 0;
}

bool VRDPInputCtx::IsReset (void)
{
    return    m_fISOHdrFetched        == false
           && m_pu8ToRecv             == &m_au8DataRecv[0]
           && m_cbToRecv              == VRDP_ISO_HDR_LEN
           && m_pu8ToRead             == &m_au8DataRecv[0]
           && m_pu8LowerProtocolStart == &m_au8DataSend[sizeof (m_au8DataSend)]
           && m_u16IncomingChannelId  == 0;

}

int VRDPInputCtx::Recv (void)
{
    Assert (m_cbToRecv > 0);
    AssertMsg ((uintptr_t)(m_pu8ToRecv - &m_au8DataRecv[0]) + m_cbToRecv <= sizeof (m_au8DataRecv),
               ("%p torecv %u rcvd %x\n", m_pu8ToRecv, m_cbToRecv, m_pu8ToRecv - &m_au8DataRecv[0]));

    unsigned cbRecv = 0;

    int rc = Transport()->Recv (TransportId (), m_pu8ToRecv, m_cbToRecv, &cbRecv);
    TESTLOG(("m_pu8ToRecv %p, m_cbToRecv %u, cbRecv %u, rc %Rrc\n", m_pu8ToRecv, m_cbToRecv, cbRecv, rc));

    if (   RT_FAILURE(rc)
        || rc == VINF_VRDP_OPERATION_COMPLETED)
    {
        /* The Recv method returns VINF_VRDP_OPERATION_COMPLETED if 0 bytes were read (== connection closed). */
        SERVERLOG(("VRDP: connection closed: rc = %Rrc\n", rc));
        return rc;
    }

    Assert (cbRecv > 0);
    Assert (cbRecv <= m_cbToRecv);

    m_pu8ToRecv += cbRecv;
    m_cbToRecv -= cbRecv;

    if (m_cbToRecv == 0)
    {
        if (!m_fISOHdrFetched)
        {
            /* The ISO hdr has been fetched. */
            m_fISOHdrFetched = true;

            Assert (m_pu8ToRecv - &m_au8DataRecv[0] == VRDP_ISO_HDR_LEN);

            /* Length of the packet. */
            unsigned uLength;

            /* Parse the ISO HDR. */
            if (m_au8DataRecv[0] == 3)
            {
                /* ISO version 3: byte version; byte reserved; uint16_be totalLength. */
                uLength = (m_au8DataRecv[2] << 8) + m_au8DataRecv[3];
                SERVERLOG(("uLength = %d\n", uLength));
            }
            else
            {
                /* RDP5 modification. byte flags; 2 bytes field length of remaining + 4. */

                SERVERLOG(("Non standard ISO version: 0x%02X\n", m_au8DataRecv[0]));

                uLength = m_au8DataRecv[1];

                if (uLength & 0x80)
                {
                    uLength &= ~0x80;
                    uLength <<= 8;
                    uLength += m_au8DataRecv[2];
                }
            }

            if (uLength > sizeof (m_au8DataRecv))
            {
                VRDPLOGREL(("Network packet too big 0x%04X. Closing connection.\n", uLength));
                return VERR_NOT_SUPPORTED;
            }

            if (uLength <= VRDP_ISO_HDR_LEN)
            {
                VRDPLOGREL(("Network packet length is incorrect 0x%04X. Closing connection.\n", uLength));
                return VERR_NOT_SUPPORTED;
            }

            m_cbToRecv = uLength - VRDP_ISO_HDR_LEN; /* The first 4 bytes are already fetched. */
                                                     /* It is also OK for the RDP5 packets. */
        }
        else
        {
            /* Signal to the caller that the packet is there and need to be processed. */
           DUMPREADREL;
           rc = VINF_VRDP_PROCESS_PDU;
        }
    }

    return rc;
}

void VRDPInputCtx::DumpWrite (void)
{
    uint16_t u16 = SizeOfWrite ();

    VRDPLOG(("InputCtx:DumpWrite(transport id %d) %d bytes\n", TransportId (), u16));

    if (u16)
    {
        VRDPLOG(("%.*Rhxd\n", u16, m_pu8LowerProtocolStart));
    }
}

void VRDPInputCtx::DumpWriteRel (void)
{
    uint16_t u16 = SizeOfWrite ();

    if (u16)
    {
        VRDPLOGREL(("The RDP packet content (write):\n\n%.*Rhxd\n\n", u16, m_pu8LowerProtocolStart));
    }
}

int VRDPInputCtx::SendPacket (void)
{
    SERVERLOG(("InputCtx:SendPacket\n"));

    Assert (QueryOutgoingChannelId () == MCS_GLOBAL_CHANNEL);

    DUMPWRITEREL;

    int rc = Transport()->Send (TransportId(), m_pu8LowerProtocolStart, SizeOfWrite ());

    /* Automatically reset the context. */
    Reset ();

    return rc;
}

uint16_t VRDPInputCtx::SizeOfWrite (void)
{
    return (uint16_t)(&m_au8DataSend[sizeof (m_au8DataSend)] - m_pu8LowerProtocolStart);
}

uint8_t *VRDPInputCtx::AppendProtocolHeader (size_t cb)
{
    Assert ((unsigned)(m_pu8LowerProtocolStart - &m_au8DataSend[0]) >= cb);

    m_pu8LowerProtocolStart -= cb;

    return m_pu8LowerProtocolStart;
}



VRDPOutputCtx::VRDPOutputCtx ()
{
    Reset ();
}

VRDPOutputCtx::~VRDPOutputCtx ()
{
}

void VRDPOutputCtx::Reset (void)
{
    m_pu8BufferBegin        = NULL;
    m_pu8DataStart          = NULL;
    m_cbData                = 0;
    m_pu8LowerProtocolStart = NULL;
}

bool VRDPOutputCtx::IsReset (void)
{
    return    m_pu8BufferBegin        == NULL
           && m_pu8DataStart          == NULL
           && m_cbData                == 0
           && m_pu8LowerProtocolStart == NULL;
}

void VRDPOutputCtx::SetOutputBuffer (const uint8_t *pu8DataStart, uint8_t *pu8BufferBegin, uint16_t cbData)
{
    m_pu8BufferBegin = pu8BufferBegin;
    m_pu8DataStart   = pu8DataStart;
    m_cbData         = cbData;

    m_pu8LowerProtocolStart = (uint8_t *)m_pu8DataStart;
}

void VRDPOutputCtx::DumpWrite (void)
{
    uint16_t u16 = SizeOfWrite ();
    NOREF(u16);

    VRDPLOG(("OutputCtx:DumpWrite(transport id %d) %d bytes\n", TransportId (), u16));

#ifdef DEBUG_sunlover
    if (u16)
    {
        VRDPLOG(("%.*Rhxd\n", u16, m_pu8LowerProtocolStart));
    }
#endif /* DEBUG_sunlover */
}

int VRDPOutputCtx::SendPacket (void)
{
    VRDPLOG(("OutputCtx:SendPacket\n"));

    int rc = Transport()->Send (TransportId (), m_pu8LowerProtocolStart, SizeOfWrite ());

    VRDPLOG(("OutputCtx:SendPacket rc = %Rrc\n", rc));

    /* Automatically reset the context. */
    Reset ();

    return rc;
}

uint16_t VRDPOutputCtx::SizeOfWrite (void)
{
    Assert (m_pu8DataStart);

    return (uint16_t)(m_cbData + (m_pu8DataStart - m_pu8LowerProtocolStart));
}

uint8_t *VRDPOutputCtx::AppendProtocolHeader (size_t cb)
{
    if ((size_t)(m_pu8LowerProtocolStart - m_pu8BufferBegin) < cb)
    {
        AssertFailed ();
        return NULL;
    }

    m_pu8LowerProtocolStart -= cb;

    return m_pu8LowerProtocolStart;
}
