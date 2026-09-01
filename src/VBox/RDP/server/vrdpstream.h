/* $Id: vrdpstream.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vrdpstream_h
#define VRDP_INCLUDED_SRC_vrdpstream_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpctx.h"

/* The VRDPStream object is a stream for updates, which are generated
 * by the output thread:
 *   - orders;
 *   - bitmaps;
 *   - pointers.
 *
 * Updates are accumulated and are sent in a single network packet
 * when either enough data accumulated or a timer flush happens.
 *
 * The stream is the layer between VRDP (VRDPTP) and SECURE (VRDPPacket).
 *
 * The stream takes care of composition of network packets for both
 * rdp4 and rdp5 (without sec, mcs, iso) RDP packets.
 *
 * The stream is operated as directly accessible memory.
 *
 * VRDPTP is upper level that uses VRDP stream to store
 * RDP commands data.
 *
 * SECURE is lower level used for transmitting composed RDP data.
 * SECURE (and lower levels), actually VRDPPacket, should be able to
 * scatter data to be transmitted. Data is passed as array of (ptr,size) pairs.
 *
 *
 *
 */

class VRDPTP;

typedef enum _VRDPUpdateType
{
    /* This enum is used as an array index in the vrdpstream.cpp. */
    VRDPUpdate_Invalid = 0,
    VRDPUpdate_Order,
    VRDPUpdate_Bitmap,
    VRDPUpdate_Pointer,
    VRDPUpdate_Palette,
    VRDPUpdate_KeepAlive
} VRDPUpdateType;

class VRDPStream: public VRDPOutputCtx
{
    public:
        VRDPStream (VRDPTP *pvrdptp);
        virtual ~VRDPStream ();

        /* Describes a memory block in the stream. */
        typedef struct _MemoryBlock
        {
            /* The starting address. */
            uint8_t *          pu8DstStart;
            /* Size of the memory. */
            uint16_t           cbDst;
        } MemoryBlock;

        /* Allocates memory for an update. The caller may then access
         * the returned memory.
         *
         * @param enmType Type of RDP update.
         * @param cb      Required bytes. This is a preallocation estimated value.
         *                The block can be extended if necessary and at the end
         *                will be reduced to actual size of written data.
         *
         * @return Pointer to information structure about the allocated block.
         */
        const MemoryBlock *BeginBlock (VRDPUpdateType enmType, unsigned cb);

        /* Marks the block as completed.
         */
        void EndBlock (const MemoryBlock *pBlock, unsigned cbActual);

        /* Flushes accumulated blocks.
         */
        int Flush (void);

        /* Resets the stream to an empty state.
         */
        void ResetStream (void);

    private:
        typedef struct _Seq
        {
            VRDPUpdateType enmType;

            bool       fRDP5;

            uint8_t   *pu8Hdr;
            unsigned   cBlocks;
            uint8_t   *pu8Blocks;
            uint8_t   *pu8End;

            MemoryBlock block;

            DECLCALLBACKMEMBER(void, pfnComplete,(uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks));
        } Seq;

        Seq         m_aSeq[64];

        uint8_t     m_au8Buffer[65536];
        uint8_t    *m_pu8Dst;
        uint8_t    *m_pu8End;

        VRDPTP     *m_pvrdptp;

        int         m_iCurrentSeq;

        bool        m_fBlockInProcess;

        int pktWrite (Seq *pSeq, unsigned cbData);

        void seqBegin (VRDPUpdateType enmType, Seq *pSeq);
        void seqComplete (Seq *pSeq);
};

#endif /* !VRDP_INCLUDED_SRC_vrdpstream_h */
