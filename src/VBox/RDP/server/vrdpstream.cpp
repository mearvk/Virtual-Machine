/* $Id: vrdpstream.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "vrdp.h"
#include "vrdpstream.h"

// #define VRDP_SEPARATE_PACKETS

#define VRDP_STREAM_RDP_MAX_HDR_SIZE (64)

/*
 * Form RDP packet stream. At the begin of the buffer leave space for SEC, MCS, ISO.
 * The VRDPPacket will use the memory.
 */

VRDPStream::VRDPStream (VRDPTP *pvrdptp)
    :
    m_pu8Dst (&m_au8Buffer[VRDP_LOW_PROTOCOL_RESERVE_SIZE]),
    m_pu8End (&m_au8Buffer[sizeof (m_au8Buffer)]),
    m_pvrdptp (pvrdptp),
    m_iCurrentSeq (-1),
    m_fBlockInProcess (false)
{
}

VRDPStream::~VRDPStream ()
{
}

void VRDPStream::ResetStream (void)
{
    m_pu8Dst          = &m_au8Buffer[VRDP_LOW_PROTOCOL_RESERVE_SIZE];
    m_iCurrentSeq     = -1;
    m_fBlockInProcess = false;
}

const VRDPStream::MemoryBlock *VRDPStream::BeginBlock (VRDPUpdateType enmType, unsigned cb)
{
    STREAMLOG(("type %d, cb %d, inprocess %d, current %d, bufferleft %d\n",
               enmType, cb, m_fBlockInProcess, m_iCurrentSeq, m_pu8End - m_pu8Dst));

    if (m_fBlockInProcess)
    {
        AssertFailed ();
        return NULL;
    }

    Seq *pSeq = NULL;

    bool fNewSeq;

    /* Check if there is enough space in the accumulation buffer. */
    if (   m_iCurrentSeq >= (int)(RT_ELEMENTS (m_aSeq) - 1)
        || m_pu8Dst + cb + VRDP_STREAM_RDP_MAX_HDR_SIZE >= m_pu8End)
    {
        int rc = Flush ();

        if (RT_FAILURE (rc))
        {
            return NULL;
        }

        Assert (m_iCurrentSeq == -1);

        fNewSeq = true;
    }
    else
    {
        fNewSeq = m_iCurrentSeq < 0;
    }

    /* Check whether a new sequence must be started. */
    if (!fNewSeq)
    {
        pSeq = &m_aSeq[m_iCurrentSeq];

        fNewSeq =    (enmType != pSeq->enmType)
                  || (enmType == VRDPUpdate_KeepAlive)
                  || (enmType == VRDPUpdate_Pointer)
                  || (enmType == VRDPUpdate_Palette)
                  || (VRDP_LOW_PROTOCOL_RESERVE_SIZE + (m_pu8Dst - pSeq->pu8Hdr) + cb > VRDP_NETWORK_PACKET_SIZE);
    }

#ifdef VRDP_SEPARATE_PACKETS
    fNewSeq = true;
#endif /* VRDP_SEPARATE_PACKETS */

    if (fNewSeq)
    {
        if (pSeq)
        {
            /* Complete the previous seq. */
            seqComplete (pSeq);

            m_iCurrentSeq++;
        }
        else
        {
            /* Start first seq. */
            Assert (m_pu8Dst == &m_au8Buffer[VRDP_LOW_PROTOCOL_RESERVE_SIZE]);
            m_iCurrentSeq = 0;
        }

        pSeq = &m_aSeq[m_iCurrentSeq];

        seqBegin (enmType, pSeq);
    }

    m_fBlockInProcess = true;

    pSeq->block.pu8DstStart  = m_pu8Dst;
    pSeq->block.cbDst        = cb;

    Assert (m_pu8Dst == pSeq->block.pu8DstStart);
    Assert (pSeq->block.pu8DstStart + cb <= &m_au8Buffer[sizeof (m_au8Buffer)]);

    return &pSeq->block;
}

void VRDPStream::EndBlock (const VRDPStream::MemoryBlock *pBlock, unsigned cbActual)
{
    RT_NOREF1(pBlock);
    STREAMLOG(("cbActual %d, inprocess %d, current %d, bufferleft %d\n",
               cbActual, m_fBlockInProcess, m_iCurrentSeq, m_pu8End - m_pu8Dst));

    Assert (m_fBlockInProcess);
    Assert (pBlock->cbDst >= cbActual);

    if (cbActual > 0)
    {
#ifdef VRDP_DEBUG_STREAM
        VRDPLOG(("Block content %d bytes:\n%.*Rhxd\n", cbActual, cbActual > 0x20? 0x20: cbActual, pBlock->pu8DstStart));
#endif /* VRDP_DEBUG_STREAM */

        Seq *pSeq = &m_aSeq[m_iCurrentSeq];

        pSeq->cBlocks++;

        m_pu8Dst += cbActual;
    }

    Assert (m_pu8Dst <= m_pu8End);

    m_fBlockInProcess = false;

    return;
}

int VRDPStream::Flush (void)
{
    /* The flush is performed either by the BeginBlock or called
     * externally.
     */
    Assert (!m_fBlockInProcess);

    if (m_iCurrentSeq < 0)
    {
        return VINF_SUCCESS;
    }

    STREAMLOG(("inprocess %d, current %d, bufferleft %d\n",
               m_fBlockInProcess, m_iCurrentSeq, m_pu8End - m_pu8Dst));

    int rc = VINF_SUCCESS;

    /* Current seq must be completed. */
    seqComplete (&m_aSeq[m_iCurrentSeq]);

    /*
     * Scan all seqs [0;m_iCurrentSeq].
     */
    int iSeq;

    /* Current seq entry. */
    Seq *pSeq = &m_aSeq[0];

    /* Size of network data, sum of accumulated sequences sizes. */
    uint32_t cbData = 0;

    /* First seq of the currently accumulated. */
    Seq *pFirstSeq = pSeq;

    for (iSeq = 0; iSeq <= m_iCurrentSeq; iSeq++)
    {
        /* Size of the current sequence. */
        uint32_t cbSeq = pSeq->pu8End - pSeq->pu8Hdr;

        bool fPacketNotFit = (VRDP_LOW_PROTOCOL_RESERVE_SIZE + cbData + cbSeq > VRDP_NETWORK_PACKET_SIZE);

#ifdef VRDP_SEPARATE_PACKETS
        fPacketNotFit = true;
#endif /* VRDP_SEPARATE_PACKETS */
/// @todo separate slow path packets
        if (   fPacketNotFit
            || pSeq->fRDP5 != pFirstSeq->fRDP5)
        {
            STREAMLOG(("fPacketNotFit %d cbData = %d, pSeq->fRDP5 %d, pFirstSeq->fRDP5 %d\n", fPacketNotFit, cbData, pSeq->fRDP5, pFirstSeq->fRDP5));

            if (RT_SUCCESS (rc))
            {
                if (cbData > 0)
                {
                    /* There were accumulated data. */
                    rc = pktWrite (pFirstSeq, cbData);

                    /* Current sequence will be the first of new accumulated.  */
                    pFirstSeq = pSeq;
                    cbData = cbSeq;

                    pSeq++;
                }
                else
                {
                    /* This means that the current sequence must be sent. */
                    rc = pktWrite (pSeq, cbSeq);

                    /* The next sequence will be the first of new accumulated.  */
                    pSeq++;

                    pFirstSeq = pSeq;
                    cbData = 0;
                }
            }

        }
        else
        {
            /* This sequence fit to the network packet. */
            cbData += cbSeq;
            pSeq++;
        }
    }

    if (cbData)
    {
        if (RT_SUCCESS (rc))
        {
            STREAMLOG(("rest of data cbData = %d, pFirstSeq->fRDP5 %d\n", cbData, pFirstSeq->fRDP5));
            rc = pktWrite (pFirstSeq, cbData);
        }
    }

    ResetStream ();

#ifdef VRDP_DEBUG_STREAM
    memset (m_au8Buffer, 0xEE, sizeof (m_au8Buffer));
#endif /* VRDP_DEBUG_STREAM */

    return rc;
}

int VRDPStream::pktWrite (Seq *pSeq, unsigned cbData)
{
    STREAMLOG(("cbData = %d\n", cbData));

    Assert (cbData > 0);
    Assert (cbData <= VRDP_NETWORK_PACKET_SIZE - VRDP_LOW_PROTOCOL_RESERVE_SIZE);
    Assert (pSeq->pu8Hdr - VRDP_LOW_PROTOCOL_RESERVE_SIZE >= &m_au8Buffer[0]);
    Assert (pSeq->pu8Hdr + cbData <= &m_au8Buffer[sizeof (m_au8Buffer)]);

    SetOutputBuffer (pSeq->pu8Hdr, pSeq->pu8Hdr - VRDP_LOW_PROTOCOL_RESERVE_SIZE, cbData);

#ifdef VRDP_DEBUG_STREAM
    DumpWrite ();
#endif /* VRDP_DEBUG_STREAM */

    int rc = m_pvrdptp->Send (pSeq->fRDP5, this);

#ifdef VRDP_SEPARATE_PACKETS
    VRDPThreadSleep (50);
#endif /* VRDP_SEPARATE_PACKETS */

    return rc;
}

#pragma pack(1)
typedef struct _RDPPDUHdr
{
    uint16_t u16PDULength;             /* Length of entire PDU. */
    uint16_t u16Type;                  /* Type of PDU. */
    uint16_t u16UserID;                /* MCS user id. */
} RDPPDUHdr;

typedef struct _RDPDataHdr
{
    RDPPDUHdr hdr;

    uint32_t u32ShareID;               /* Share id. */
    uint8_t  u8Pad;                    /* Pad. */
    uint8_t  u8StreamID;               /* Stream id. */

    uint16_t cbPacket;                 /* In rdesktop this is length of packet after the data pdu header.
                                        * But MS server sets this to the length of entire DATA PDU.
                                        * That is equal to the length of RDP packet 'length + 12 + 4'.
                                        */

    uint8_t  u8Type;                   /* Type of the data PDU. */

    uint8_t  ctype;                    /* Compression type. */
    uint16_t clen;                     /* Compression length. */
} RDPDataHdr;

typedef struct _RDPUpdateHdr
{
    RDPDataHdr hdr;

    uint16_t u16Type;                  /* Type of the update. */
} RDPUpdateHdr;

typedef struct _RDPOrderHdr
{
    RDPUpdateHdr hdr;

    uint16_t u16Pad1;                  /* Pad. */
    uint16_t cOrders;                  /* Number of orders. */
    uint16_t u16Pad2;                  /* Pad. */
} RDPOrderHdr;

typedef struct _RDPBitmapHdr
{
    RDPUpdateHdr hdr;

    uint16_t cBitmaps;                 /* Number of bitmaps. */
} RDPBitmapHdr;

typedef struct _RDPPointerHdr
{
    RDPDataHdr hdr;
} RDPPointerHdr;

typedef struct _RDPPaletteHdr
{
    RDPUpdateHdr hdr;
} RDPPaletteHdr;

typedef struct _RDPKeepAliveHdr
{
    RDPDataHdr hdr;
} RDPKeepAliveHdr;


/* RDP5 */
typedef struct _RDP5PDUHdr
{
    uint8_t  u8Type;                   /* Type of RDP5 PDU. */
    uint16_t u16Length;                /* Length of the following PDU data. */
} RDP5PDUHdr;

typedef struct _RDP5OrderHdr
{
    RDP5PDUHdr hdr;

    uint16_t cOrders;                  /* Number of orders. */
} RDP5OrderHdr;

typedef struct _RDP5BitmapHdr
{
    RDP5PDUHdr hdr;

    uint16_t u16PartLength;            /* Seems to be 1. */
    uint16_t cBitmaps;                 /* Number of bitmaps. */
} RDP5BitmapHdr;

#pragma pack()

static void seqCompletePDU (RDPPDUHdr *pHdr, uint16_t u16Type, uint16_t cb)
{
    pHdr->u16PDULength = cb + 6;
    pHdr->u16Type      = VRDP_MAKE_PDU_TYPE(u16Type);
    pHdr->u16UserID    = 0;
}

static void seqCompleteData (RDPDataHdr *pHdr, uint8_t u8Type, uint16_t cb)
{
    pHdr->u32ShareID = 0;
    pHdr->u8Pad      = 0;
    pHdr->u8StreamID = 0;

    pHdr->cbPacket = cb;

    pHdr->u8Type = u8Type;

    pHdr->ctype = 0;
    pHdr->clen  = 0;

    seqCompletePDU (&pHdr->hdr, RDP_PDU_DATA, cb + 12);
}

static void seqCompleteUpdate (RDPUpdateHdr *pHdr, uint16_t u16Type, uint16_t cb)
{
    pHdr->u16Type = u16Type;

    seqCompleteData (&pHdr->hdr, RDP_DATA_PDU_UPDATE, cb + 2);
}

static DECLCALLBACK(void) seqCompleteOrder (uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks)
{
    RDPOrderHdr *pHdr = (RDPOrderHdr *)pu8Hdr;

    pHdr->u16Pad1 = 0;
    pHdr->cOrders = cBlocks;
    pHdr->u16Pad2 = 0;

    seqCompleteUpdate (&pHdr->hdr, RDP_UPDATE_ORDERS, cb + 6);
}

static DECLCALLBACK(void) seqCompleteBitmap (uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks)
{
    RDPBitmapHdr *pHdr = (RDPBitmapHdr *)pu8Hdr;

    pHdr->cBitmaps = cBlocks;

    seqCompleteUpdate (&pHdr->hdr, RDP_UPDATE_BITMAP, cb + 2);
}

static DECLCALLBACK(void) seqCompletePointer (uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks)
{
    Assert (cBlocks == 1); RT_NOREF1(cBlocks);

    RDPPointerHdr *pHdr = (RDPPointerHdr *)pu8Hdr;

    seqCompleteData (&pHdr->hdr, RDP_DATA_PDU_POINTER, cb);
}

static DECLCALLBACK(void) seqCompletePalette (uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks)
{
    Assert (cBlocks == 1); RT_NOREF1(cBlocks);

    RDPPaletteHdr *pHdr = (RDPPaletteHdr *)pu8Hdr;

    seqCompleteUpdate (&pHdr->hdr, RDP_UPDATE_PALETTE, cb);
}

static DECLCALLBACK(void) seqCompleteKeepAlive (uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks)
{
    Assert (cBlocks == 1); RT_NOREF1(cBlocks);

    RDPKeepAliveHdr *pHdr = (RDPKeepAliveHdr *)pu8Hdr;

    seqCompleteData (&pHdr->hdr, RDP_DATA_PDU_SYNCHRONISE, cb);
}

#define RDP5_UPDATE_ORDERS 0
#define RDP5_UPDATE_BITMAP 1

static void seqCompletePDURDP5 (RDP5PDUHdr *pHdr, uint8_t u8Type, uint16_t cb)
{
    pHdr->u8Type    = u8Type;
    pHdr->u16Length = cb;
}

static DECLCALLBACK(void) seqCompleteOrderRDP5 (uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks)
{
    RDP5OrderHdr *pHdr = (RDP5OrderHdr *)pu8Hdr;

    pHdr->cOrders = cBlocks;

    seqCompletePDURDP5 (&pHdr->hdr, RDP5_UPDATE_ORDERS, cb + 2);
}

static DECLCALLBACK(void) seqCompleteBitmapRDP5 (uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks)
{
    RDP5BitmapHdr *pHdr = (RDP5BitmapHdr *)pu8Hdr;

    pHdr->u16PartLength = 1; /* Seems to must be 1. */
    pHdr->cBitmaps = cBlocks;

    seqCompletePDURDP5 (&pHdr->hdr, RDP5_UPDATE_BITMAP, cb + 4);
}

typedef struct _SeqInfo
{
    DECLCALLBACKMEMBER(void, pfnComplete,(uint8_t *pu8Hdr, uint16_t cb, uint16_t cBlocks));
    int cbHdr;
} SeqInfo;

static SeqInfo seqInfo[] =
{
    { NULL,                 0                        }, // VRDPUpdate_Invalid
    { seqCompleteOrder,     sizeof (RDPOrderHdr)     }, // VRDPUpdate_Order
    { seqCompleteBitmap,    sizeof (RDPBitmapHdr)    }, // VRDPUpdate_Bitmap
    { seqCompletePointer,   sizeof (RDPPointerHdr)   }, // VRDPUpdate_Pointer
    { seqCompletePalette,   sizeof (RDPPaletteHdr)   }, // VRDPUpdate_Palette
    { seqCompleteKeepAlive, sizeof (RDPKeepAliveHdr) }  // VRDPUpdate_KeepAlive
};

static SeqInfo seqInfoRDP5[] =
{
    { NULL,                     0                      }, // VRDPUpdate_Invalid
    { seqCompleteOrderRDP5,     sizeof (RDP5OrderHdr)  }, // VRDPUpdate_Order
    { seqCompleteBitmapRDP5,    sizeof (RDP5BitmapHdr) }, // VRDPUpdate_Bitmap
    { NULL,                     0                      }, // VRDPUpdate_Pointer
    { NULL,                     0                      }, // VRDPUpdate_Palette
    { NULL,                     0                      }  // VRDPUpdate_KeepAlive
};

void VRDPStream::seqBegin (VRDPUpdateType enmType, Seq *pSeq)
{
    bool fRDP5 = m_pvrdptp->AreRDP5PacketsEnabled ();

    if ((unsigned)enmType >= RT_ELEMENTS (seqInfo))
    {
        AssertFailed ();
        enmType = VRDPUpdate_Invalid;
    }

    SeqInfo *pSeqInfo;

    if (fRDP5)
    {
        pSeqInfo = &seqInfoRDP5[(int)enmType];

        if (!pSeqInfo->pfnComplete)
        {
            /* Unsupported RDP5 entry. Fallback to the RDP. */
            pSeqInfo = &seqInfo[(int)enmType];
            fRDP5 = false;
        }
    }
    else
    {
        pSeqInfo = &seqInfo[(int)enmType];
    }

    pSeq->enmType   = enmType;

    pSeq->fRDP5     = fRDP5;

    pSeq->pu8Hdr    = m_pu8Dst;
    pSeq->cBlocks   = 0;
    pSeq->pu8Blocks = pSeq->pu8Hdr;

    pSeq->pfnComplete = pSeqInfo->pfnComplete;
    m_pu8Dst += pSeqInfo->cbHdr;

    pSeq->pu8Blocks = m_pu8Dst;
    pSeq->pu8End = m_pu8Dst;
}

void VRDPStream::seqComplete (Seq *pSeq)
{
    if (   pSeq->cBlocks == 0
        || pSeq->enmType == VRDPUpdate_Invalid
        || !pSeq->pfnComplete)
    {
        STREAMLOG(("Cancelling seq %d!!!\n", m_iCurrentSeq));

        /* The sequence has no blocks or is invalid and need to be cancelled. */
        m_pu8Dst = pSeq->pu8Hdr;
        m_iCurrentSeq--;
    }
    else
    {
        pSeq->pfnComplete (pSeq->pu8Hdr, (uint16_t)(m_pu8Dst - pSeq->pu8Blocks), pSeq->cBlocks);
        pSeq->pfnComplete = NULL;
    }

    pSeq->pu8End = m_pu8Dst;
}
