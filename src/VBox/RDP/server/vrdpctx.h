/* $Id: vrdpctx.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vrdpctx_h
#define VRDP_INCLUDED_SRC_vrdpctx_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "transport.h"
#include "vrdpdata.h"

#define VRDP_CONTEXT_VM 1
#define VRDP_CONTEXT_OUTPUT 2


/* Packet length should fit in 2 bytes encoding to make rdesktop and other clients
 * happy. 0x8000 | length with high bits = 10xx X X X
 */
#define VRDP_NETWORK_PACKET_SIZE (0x3fffU)

#define VRDP_NETWORK_PACKET_SIZE_ALLOC (0x4000U)

/* Number of bytes reserved for lower protocol (sec, mcs, iso). */
#define VRDP_LOW_PROTOCOL_RESERVE_SIZE (64U)

#define VRDP_CTX_VER_NULL 0

class VRDPCtx
{
    public:
        VRDPCtx ();
        virtual ~VRDPCtx ();

        virtual void DumpWrite (void) = 0;

        virtual int SendPacket (void) = 0;

        virtual uint16_t SizeOfWrite (void) = 0;

        virtual uint8_t *AppendProtocolHeader (size_t cb) = 0;

        void InitOutgoingChannelId (uint16_t u16OutgoingChannelId)
        {
            /* The method can be called only once. */
            Assert (m_u16OutgoingChannelId == 0);
            m_u16OutgoingChannelId = u16OutgoingChannelId;
        }

        uint16_t QueryOutgoingChannelId (void)
        {
            /* The id must be initialized. */
            Assert (m_u16OutgoingChannelId != 0);
            return m_u16OutgoingChannelId;
        }

        void SetTransport (VRDPTransport *pTransport, VRDPTRANSPORTID id);

        VRDPTransport *Transport (void) { return m_pTransport; };
        VRDPTRANSPORTID TransportId (void) { return m_id; };

        void SetData(VRDPData *pData) { m_pData = pData; }
        VRDPData *Data(void) { Assert(m_pData); return m_pData; }

    private:
        /* The channel id that is used for outgoing packets.
         * It is set once by the 'InitOutgoingChannelId' and can not be changed
         * later.
         */
        uint16_t m_u16OutgoingChannelId;

        VRDPTransport  *m_pTransport;
        VRDPTRANSPORTID m_id;

        VRDPData *m_pData;
};

class VRDPInputCtx : public VRDPCtx
{
    public:
        VRDPInputCtx ();
        virtual ~VRDPInputCtx ();

        void StoreIncomingChannelId (uint16_t u16IncomingChannelId)
        {
            Assert (m_u16IncomingChannelId == 0);
            m_u16IncomingChannelId = u16IncomingChannelId;
        }

        uint16_t QueryIncomingChannelId (void)
        {
            /* Verify that the channel id has been assigned. */
            Assert (m_u16IncomingChannelId != 0);
            return m_u16IncomingChannelId;
        }

        void DumpRead (void);
        void DumpReadRel (void);

        uint16_t BytesToRead (void)
        {
            return (uint16_t)(m_pu8ToRecv - m_pu8ToRead);
        }

        /* Set the packet at initial state, ready to fetch the ISO header. */
        void Reset (void);

        bool IsReset (void);

        /* Returns the ISO packet version, if a packet is in the buffer. */
        int PacketVersion (void)
        {
            if (m_pu8ToRecv > &m_au8DataRecv[0])
            {
                /* The packet version is the first byte of the packet. */
                return m_au8DataRecv[0];
            }

            return VRDP_CTX_VER_NULL;
        }

        /* Fetch the data that is available in the transport. */
        int Recv (void);

        bool IsReadOK (int cbNeeded)
        {
            return (cbNeeded >= 0) && (cbNeeded <= BytesToRead());
        }

        const uint8_t *Read (int cbNeeded)
        {
            if (IsReadOK (cbNeeded))
            {
                uint8_t *pu8 = m_pu8ToRead;
                m_pu8ToRead += cbNeeded;
                return pu8;
            }

            return NULL;
        }

        virtual void DumpWrite (void);
        void DumpWriteRel (void);

        virtual int SendPacket (void);

        virtual uint16_t SizeOfWrite (void);

        virtual uint8_t *AppendProtocolHeader (size_t cb);

    private:
        bool        m_fISOHdrFetched;

        uint8_t    *m_pu8ToRecv;
        unsigned    m_cbToRecv;

        uint8_t    *m_pu8ToRead;

        uint8_t     m_au8DataRecv[VRDP_NETWORK_PACKET_SIZE_ALLOC];

        uint8_t    *m_pu8LowerProtocolStart;
        uint8_t     m_au8DataSend[VRDP_NETWORK_PACKET_SIZE_ALLOC];

        /* The MCS channel id of the last received packet. Since the Input context
         * receives data for all channels, this is a variable value.
         * It is set by MCS level, which uses 'StoreIncomingChannelId' method.
         * The value can be queried by 'QueryIncomingChannelId' method.
         */
        uint16_t m_u16IncomingChannelId;
};

class VRDPOutputCtx : public VRDPCtx
{
    public:
        VRDPOutputCtx ();
        virtual ~VRDPOutputCtx ();

        void Reset (void);

        bool IsReset (void);

        void SetOutputBuffer (const uint8_t *pu8DataStart, uint8_t *pu8BufferBegin, uint16_t cbData);

        virtual void DumpWrite (void);

        virtual int SendPacket (void);

        virtual uint16_t SizeOfWrite (void);

        virtual uint8_t *AppendProtocolHeader (size_t cb);

    private:

        uint8_t *m_pu8BufferBegin;
        const uint8_t *m_pu8DataStart;

        uint16_t m_cbData;

        uint8_t *m_pu8LowerProtocolStart;
};

#endif /* !VRDP_INCLUDED_SRC_vrdpctx_h */
