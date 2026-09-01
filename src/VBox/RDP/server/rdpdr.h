/* $Id: rdpdr.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_rdpdr_h
#define VRDP_INCLUDED_SRC_rdpdr_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "utils.h"

#include <VBox/RemoteDesktop/VRDE.h>

#include <iprt/list.h>

#define RDPDR_STATUS_SUCCESS          UINT32_C(0x00000000)
#define RDPDR_STATUS_UNSUCCESSFUL     UINT32_C(0xC0000001)
#define RDPDR_STATUS_NO_MEMORY        UINT32_C(0xC0000017)
#define RDPDR_STATUS_ACCESS_DENIED    UINT32_C(0xC0000022)
#define RDPDR_STATUS_BUFFER_TOO_SMALL UINT32_C(0xC0000023)
#define RDPDR_STATUS_NOT_SUPPORTED    UINT32_C(0xC00000BB)

/* RDPDR uses reference counted packets which are asynchronously processed by the client.
 * The called, for example smartcard, creates a packet and submits it to RDPDR, when RDPDR
 * receives response, it updates the packet and invokes a callback.
 */
typedef struct RDPDRPKTHDR RDPDRPKTHDR;
typedef void FNRDPDRPKTDELETE(RDPDRPKTHDR *pPkt);
typedef FNRDPDRPKTDELETE *PFNRDPDRPKTDELETE;

/* RDPDRPKTHDR::pktType */
#define RDPDR_OUT_RAW   0
#define RDPDR_OUT_IOCTL 1

#pragma pack(1)
struct RDPDRPKTHDR
{
    uint32_t pktType;
    uint32_t pktSize;
    int32_t volatile cRefs;
    uint32_t u32DeviceId;           /* Target RDPDR device. */
    PFNRDPDRPKTDELETE pfnPktDelete; /* Deallocation routine, called when cRefs goes to 0. */
    /* pktSize - sizeof(RDPDRPKTHDR) bytes follow. */
};
#pragma pack()

inline void RDPDRPktInit(RDPDRPKTHDR *pHdr,
                         uint32_t pktType,
                         uint32_t pktSize,
                         PFNRDPDRPKTDELETE pfn,
                         uint32_t u32DeviceId)
{
    pHdr->pktType = pktType;
    pHdr->pktSize = pktSize;
    pHdr->cRefs = 0;
    pHdr->u32DeviceId = u32DeviceId;
    pHdr->pfnPktDelete = pfn;
}

inline void *RDPDRPktDataPtr(RDPDRPKTHDR *pHdr)
{
    return (uint8_t *)pHdr + sizeof(RDPDRPKTHDR);
}

inline uint32_t RDPDRPktDataSize(RDPDRPKTHDR *pHdr)
{
    return pHdr->pktSize - sizeof(RDPDRPKTHDR);
}

inline int32_t RDPDRPktAddRef(RDPDRPKTHDR *pHdr)
{
    int c = ASMAtomicIncS32(&pHdr->cRefs);
#ifdef DEBUG_sunlover
    LogRel(("RDPDRPKTHDR %p -> %d\n", pHdr, c));
#endif
    return c;
}

inline void RDPDRPktRelease(RDPDRPKTHDR *pHdr)
{
    int32_t c = ASMAtomicDecS32(&pHdr->cRefs);
#ifdef DEBUG_sunlover
    LogRel(("RDPDRPKTHDR %p -> %d\n", pHdr, c));
#endif
    Assert(c >= 0);
    if (c == 0)
    {
#ifdef DEBUG_sunlover
        LogRel(("RDPDRPKTHDR %p deleted\n", pHdr));
#endif
        if (pHdr->pfnPktDelete)
        {
            pHdr->pfnPktDelete(pHdr);
        }
        VRDPMemFree(pHdr);
    }
}

/* Used for PostOutput. */
typedef struct RDPDROUT
{
    RDPDRPKTHDR *pPktHdr;
} RDPDROUT;

#pragma pack(1)
typedef struct RDPDRIOCTL
{
    RDPDRPKTHDR hdr;

    void *pvCtx;                       /* The callers context. */

    uint32_t u32Function;              /* IOCTL function. */

    uint32_t u32InputBufferLength;     /* Length of data in input buffer. */

    uint8_t *pu8InputBuffer;           /* Pointer to the input buffer with encoded ioctl parameters. */
    uint32_t u32InputBufferSize;       /* Size of the input buffer. */

    uint32_t u32RequestedOutputLength; /* Requested length of data in output buffer. */
    uint32_t u32ReturnedOutputLength;  /* Length of data in output buffer. */

    const uint8_t *pu8OutputBuffer;    /* Pointer to the output buffer returned by the client. */
    uint32_t u32OutputBufferSize;      /* Size of the output buffer. */

    /* The callers data may follow. */

} RDPDRIOCTL;
#pragma pack()

#endif /* !VRDP_INCLUDED_SRC_rdpdr_h */
