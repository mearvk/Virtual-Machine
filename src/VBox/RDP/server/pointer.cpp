/* $Id: pointer.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

/* RDP can cache mouse pointers on client and send an cache index
 * rather than the pointer bitmap.
 *
 * To use this feature we have to cache pointers on server as well,
 * so we will be able to compare new pointer to those which were
 * already sent to a client and then either send cache index to client
 * or entire pointer with a new cache index.
 *
 * The server side caching must be performed for at least one pointer,
 * even if client was not yet connected. On a client connect VRDP will
 * send the current pointer to the client.
 *
 * The server saves all pointers in a limited cache.
 *
 * Each client maintains a MRU list of pointers actually sent
 * to the client. Upon receiving a pointer update, the client
 * looks whether the given server cache index was already
 * sent to the client.
 *
 * The client provides mapping from the server side cache indexes
 * to the client side cache indexes. 0 client side cache index
 * is used for sending uncacheable pointers.
 *
 */

#include "vrdpserv.h"
#include "utils.h"

static DECLCALLBACK(void) VRDEMousePtr(HVRDESERVER hServer,
                                      const VRDEMOUSEPTRDATA *pPointer)
{
    VRDPServer *pServer = (VRDPServer *)hServer;

    VRDPAPILOG(("%p\n", pServer));

    if (pServer)
    {
        pServer->ProcessPointer(pPointer);
    }
}

static VRDEMOUSEPTRINTERFACE gMousePtrInterface =
{
    { 1, sizeof (VRDEMOUSEPTRINTERFACE) },

    VRDEMousePtr
};

/* Helper to get the interface method table, called from VRDPServer::GetInterface */
int vrdpGetInterfaceMousePtr(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks)
{
    NOREF(pCallbacks);

    int rc = VINF_SUCCESS;

    if (   pInterface->u64Version == 1
        && pInterface->u64Size == sizeof (VRDEMOUSEPTRINTERFACE))
    {
        /* Interface version 1 is requested. */
        VRDEMOUSEPTRINTERFACE *p = (VRDEMOUSEPTRINTERFACE *)pInterface;

        *p = gMousePtrInterface;
    }
    else
    {
        rc = VERR_VERSION_MISMATCH;
    }

    return rc;
}

#ifdef DEBUG_sunlover
#define LOGDUMPPTR Log
void dumpPointer(const uint8_t *pu8Shape, uint32_t width, uint32_t height, bool fXorMaskRGB32)
{
    unsigned i;

    if (fXorMaskRGB32)
    {
        uint32_t *pu32Xor = (uint32_t*)pu8Shape;

        for (i = 0; i < height; i++)
        {
            unsigned j;
            LOGDUMPPTR(("%p: ", pu32Xor));
            for (j = 0; j < width; j++)
            {
                LOGDUMPPTR(("%08X", *pu32Xor++));
            }
            LOGDUMPPTR(("\n"));
        }
    }
    else
    {
        const uint8_t *pu8And = pu8Shape;

        for (i = 0; i < height; i++)
        {
            unsigned j;
            LOGDUMPPTR(("%p: ", pu8And));
            for (j = 0; j < (width + 7) / 8; j++)
            {
                unsigned k;
                for (k = 0; k < 8; k++)
                {
                    LOGDUMPPTR(("%d", ((*pu8And) & (1 << (7 - k)))? 1: 0));
                }

                pu8And++;
            }
            LOGDUMPPTR(("\n"));
        }

        /* RDP 24 bit RGB mask. */
        uint8_t *pu8Xor = (uint8_t*)(pu8Shape + ((((width + 7) / 8) * height + 3) & ~3));
        for (i = 0; i < height; i++)
        {
            unsigned j;
            LOGDUMPPTR(("%p: ", pu8Xor));
            for (j = 0; j < width; j++)
            {
                LOGDUMPPTR(("%02X%02X%02X%02X", pu8Xor[3], pu8Xor[2], pu8Xor[1], pu8Xor[0]));
                pu8Xor += 4;
            }
            LOGDUMPPTR(("\n"));
        }
    }
}
#endif /* DEBUG_sunlover */

/*
 *
 * VRDPServer methods
 *
 */

static void pcGenServerId (VRDPPOINTERCACHE *pPointerCache, VRDPPOINTERCACHEELEM *pCacheElem, uint8_t u8Index)
{
    uint32_t u32ServerHandle = (++pPointerCache->u32ServerHandleSrc) & 0xFFFFFFU;

    if (u32ServerHandle == VRDP_POINTER_SERVER_ID_NULL)
    {
        u32ServerHandle = (++pPointerCache->u32ServerHandleSrc) & 0xFFFFFFU;
    }

    pCacheElem->u32ServerId = pcMakeServerId (u32ServerHandle, u8Index);
}

VRDEMOUSEPTRDATA *VRDPServer::PointerCacheLock (uint32_t u32ServerId)
{
    if (RT_FAILURE(Enter()))
    {
        return NULL;
    }

    uint8_t u8Index = pcDecodeServerIndex (u32ServerId);

    if (   u8Index < RT_ELEMENTS(m_pointerCache.aPointers)
        && u32ServerId == m_pointerCache.aPointers[u8Index].u32ServerId)
    {
        return m_pointerCache.aPointers[u8Index].pPointer;
    }

    Exit ();

    return NULL;
}

void VRDPServer::PointerCacheUnlock (uint32_t u32ServerId)
{
    NOREF (u32ServerId);
    Exit ();
}

uint32_t VRDPServer::PointerCacheQueryLatest (void)
{
    uint32_t u32ServerId = VRDP_POINTER_SERVER_ID_NULL;

    if (RT_FAILURE(Enter()))
    {
        return u32ServerId;
    }

    if (m_pointerCache.u8LatestPointer != VRDP_POINTER_IDX_NULL)
    {
        Assert (m_pointerCache.u8LatestPointer < RT_ELEMENTS(m_pointerCache.aPointers));

        u32ServerId = m_pointerCache.aPointers[m_pointerCache.u8LatestPointer].u32ServerId;
    }

    Exit ();

    return u32ServerId;
}

/*
 * Cache the pointer on the server and inform all client that
 * they must update the pointer shape on the client.
 * @thread VM
 */
void VRDPServer::ProcessPointer (const VRDEMOUSEPTRDATA *pPointer)
{
    if (RT_FAILURE(Enter()))
    {
        return;
    }

#ifdef DEBUG_sunlover
    dumpPointer((uint8_t *)pPointer + sizeof (VRDEMOUSEPTRDATA),
                 pPointer->u16Width,
                 pPointer->u16Height,
                 pPointer->u16MaskLen == 0);
#endif /* DEBUG_sunlover */

    SERVERLOG(("POINTERCACHE: Entry: u8Pointers: %d, u8LatestPointer %d, head %d, tail %d\n",
                m_pointerCache.u8Pointers, m_pointerCache.u8LatestPointer,
                m_pointerCache.u8MRUHead, m_pointerCache.u8MRUTail));

    VRDPPOINTERCACHEELEM *pCacheElem = NULL;

    /* Size of the entire pointer. */
    uint32_t cbPointer = sizeof (VRDEMOUSEPTRDATA) + pPointer->u16MaskLen + pPointer->u32DataLen;

    /*
     * Lookup the pointer in local cache.
     */
    uint8_t u8Index = m_pointerCache.u8MRUHead;

    while (u8Index != VRDP_POINTER_IDX_NULL)
    {
        pCacheElem = &m_pointerCache.aPointers[u8Index];

        VRDEMOUSEPTRDATA *pCachedPointer = pCacheElem->pPointer;

        if (   pPointer->u16HotX    == pCachedPointer->u16HotX
            && pPointer->u16HotY    == pCachedPointer->u16HotY
            && pPointer->u16Width   == pCachedPointer->u16Width
            && pPointer->u16Height  == pCachedPointer->u16Height
            && pPointer->u16MaskLen == pCachedPointer->u16MaskLen
            && pPointer->u32DataLen == pCachedPointer->u32DataLen
            && memcmp ((uint8_t *)pPointer       + sizeof (VRDEMOUSEPTRDATA),
                       (uint8_t *)pCachedPointer + sizeof (VRDEMOUSEPTRDATA),
                       pPointer->u16MaskLen + pPointer->u32DataLen) == 0)
        {
            SERVERLOG(("POINTERCACHE: found pointer %d, cache addr %p\n", u8Index, pCachedPointer));
            break;
        }

        u8Index = pCacheElem->u8MRUNext;
    }

    if (u8Index == VRDP_POINTER_IDX_NULL)
    {
        /*
         * Insert the new pointer to the cache.
         */
        SERVERLOG(("POINTERCACHE: new pointer.\n"));

        VRDEMOUSEPTRDATA *pNewPointer = (VRDEMOUSEPTRDATA *)VRDPMemAlloc (cbPointer);

        if (pNewPointer == NULL)
        {
            /* Indicate an error. */
            u8Index = VRDP_POINTER_IDX_NULL;
        }
        else
        {
            memcpy (pNewPointer, pPointer, cbPointer);

            if (m_pointerCache.u8Pointers < RT_ELEMENTS(m_pointerCache.aPointers))
            {
                /* There is a place for the new pointer. Search an empty slot.
                 * Linear search since there are not a lot of slots and that
                 * is simpler than maintaining a free slot list.
                 */
                pCacheElem = &m_pointerCache.aPointers[0];

                for (u8Index = 0; u8Index < RT_ELEMENTS(m_pointerCache.aPointers); u8Index++, pCacheElem++)
                {
                    if (pCacheElem->fUsed == false)
                    {
                        break;
                    }
                }

                if (u8Index == RT_ELEMENTS(m_pointerCache.aPointers))
                {
                    /* State inconsistency: number of slots indicate that there are
                     * free slots but all slots are in use.
                     */
                    AssertFailed ();
                    u8Index = VRDP_POINTER_IDX_NULL;
                }
                else
                {
                    /* It will be now more pointers in the cache. */
                    m_pointerCache.u8Pointers++;
                }
            }
            else
            {
                /* Replace an existing pointer at the tile of MRU list. */
                SERVERLOG(("POINTERCACHE: not enough slots in cache. Replacing existing slot.\n"));

                /* The cache must have more than 1 element. Do not support the case of 1 element cache. */
                Assert (m_pointerCache.u8MRUHead != m_pointerCache.u8MRUTail);
                Assert (m_pointerCache.u8MRUTail != VRDP_POINTER_IDX_NULL);
                Assert (m_pointerCache.u8MRUHead < RT_ELEMENTS(m_pointerCache.aPointers));
                Assert (m_pointerCache.u8MRUTail < RT_ELEMENTS(m_pointerCache.aPointers));

                /* Exclude the tile from the list. */
                u8Index = m_pointerCache.u8MRUTail;
                pCacheElem = &m_pointerCache.aPointers[u8Index];

                Assert (pCacheElem->u8MRUPrev != VRDP_POINTER_IDX_NULL);

                m_pointerCache.u8MRUTail = pCacheElem->u8MRUPrev;
                m_pointerCache.aPointers[m_pointerCache.u8MRUTail].u8MRUNext = VRDP_POINTER_IDX_NULL;
            }

            if (u8Index == VRDP_POINTER_IDX_NULL)
            {
                VRDPMemFree (pNewPointer);
            }
            else
            {
                /* Initialize the slot as the head of the MRU list. */
                pCacheElem->fUsed = true;

                pCacheElem->u8MRUNext = m_pointerCache.u8MRUHead;
                pCacheElem->u8MRUPrev = VRDP_POINTER_IDX_NULL;

                if (pCacheElem->pPointer != NULL)
                {
                    VRDPMemFree (pCacheElem->pPointer);
                }

                pCacheElem->pPointer = pNewPointer;

                pcGenServerId (&m_pointerCache, pCacheElem, u8Index);

                if (m_pointerCache.u8MRUHead == VRDP_POINTER_IDX_NULL)
                {
                    /* First element in the cache. */
                    m_pointerCache.u8MRUTail = u8Index;
                }
                else
                {
                    m_pointerCache.aPointers[m_pointerCache.u8MRUHead].u8MRUPrev = u8Index;
                }

                m_pointerCache.u8MRUHead = u8Index;

                m_pointerCache.u8LatestPointer = u8Index;
            }
        }
    }
    else
    {
        /* Move the pointer at the head of MRU list. */
        if (u8Index != m_pointerCache.u8MRUHead)
        {
            pCacheElem = &m_pointerCache.aPointers[u8Index];

            if (u8Index == m_pointerCache.u8MRUTail)
            {
                /* tail->head */

                /* Adjust the tail. */
                m_pointerCache.u8MRUTail = pCacheElem->u8MRUPrev;
                m_pointerCache.aPointers[m_pointerCache.u8MRUTail].u8MRUNext = VRDP_POINTER_IDX_NULL;
            }
            else
            {
                /* middle->head */

                /* Adjust the prev and next. */
                Assert (   pCacheElem->u8MRUPrev != VRDP_POINTER_IDX_NULL
                        && pCacheElem->u8MRUPrev < RT_ELEMENTS (m_pointerCache.aPointers));
                Assert (   pCacheElem->u8MRUNext != VRDP_POINTER_IDX_NULL
                        && pCacheElem->u8MRUNext < RT_ELEMENTS (m_pointerCache.aPointers));

                m_pointerCache.aPointers[pCacheElem->u8MRUPrev].u8MRUNext = pCacheElem->u8MRUNext;
                m_pointerCache.aPointers[pCacheElem->u8MRUNext].u8MRUPrev = pCacheElem->u8MRUPrev;
            }

            /* Insert as head. */
            pCacheElem->u8MRUNext = m_pointerCache.u8MRUHead;
            pCacheElem->u8MRUPrev = VRDP_POINTER_IDX_NULL;

            m_pointerCache.aPointers[m_pointerCache.u8MRUHead].u8MRUPrev = u8Index;
            m_pointerCache.u8MRUHead = u8Index;
        }

        m_pointerCache.u8LatestPointer = u8Index;
    }

    Exit ();

    if (u8Index != VRDP_POINTER_IDX_NULL)
    {
        Assert (pCacheElem);

        PostOutput (VRDP_OUTPUT_CACHED_POINTER, 0, &pCacheElem->u32ServerId, sizeof (pCacheElem->u32ServerId));
    }
    else
    {
        /* That actually should never happened. Something serious
         * happened if the pointer has not been cached on the server.
         * Most likely memory allocation has failed.
         */
        SERVERLOG(("Failed to cache a pointer!!!\n"));
        AssertFailed ();
    }
}


/*
 *
 * VRDPClient methods
 *
 */

void VRDPClient::pointerCacheSetup (void)
{
    uint8_t u8PointerCacheSize = m_vrdptp.PointerCacheSize ();

    SERVERLOG(("pointer cache size was %d, must be %d\n", m_PointerCache.u8Pointers, u8PointerCacheSize));

    if (m_PointerCache.u8Pointers == u8PointerCacheSize)
    {
        /* Nothing changed. */
        return;
    }

    /* Just recreate the cache. */
    pointerCacheDestroy ();

    m_PointerCache.u8Pointers = u8PointerCacheSize;

    m_PointerCache.paPointers = (VRDPCLIENTPOINTERCACHEELEM *)VRDPMemAlloc (m_PointerCache.u8Pointers * sizeof (VRDPCLIENTPOINTERCACHEELEM));

    if (!m_PointerCache.paPointers)
    {
        SERVERLOG(("No memory for pointer cache. Cache will not be working!!!\n"));
        pointerCacheDestroy ();
        return;
    }

    m_PointerCache.u8MRUHead = VRDP_POINTER_IDX_NULL;
    m_PointerCache.u8MRUTail = VRDP_POINTER_IDX_NULL;

    uint8_t u8Index;

    VRDPCLIENTPOINTERCACHEELEM *pCacheElem = m_PointerCache.paPointers;

    for (u8Index = 0; u8Index < m_PointerCache.u8Pointers; u8Index++, pCacheElem++)
    {
        pCacheElem->u32ServerId = VRDP_POINTER_SERVER_ID_NULL;
        pCacheElem->u16RemoteId = VRDP_POINTER_CLIENT_ID_NULL;
        pCacheElem->u8MRUNext = VRDP_POINTER_IDX_NULL;
        pCacheElem->u8MRUPrev = VRDP_POINTER_IDX_NULL;
    }
}

void VRDPClient::pointerCacheDestroy (void)
{
    if (m_PointerCache.paPointers)
    {
        VRDPMemFree (m_PointerCache.paPointers);
    }

    m_PointerCache.init ();
}

void VRDPClient::pointerCacheOutput (uint32_t u32ServerId)
{
    /* Check whether the pointer was already sent to the client. */
    if (u32ServerId == VRDP_POINTER_SERVER_ID_NULL)
    {
        u32ServerId = m_pServer->PointerCacheQueryLatest ();

        if (u32ServerId == VRDP_POINTER_SERVER_ID_NULL)
        {
            SERVERLOG(("No pointer to be sent\n"));
            return;
        }
    }

    /* Check whether the client has the mouse pointer cache. */
    if (!m_PointerCache.u8Pointers)
    {
        return;
    }

    VRDPCLIENTPOINTERCACHEELEM *pCacheElem = NULL;

    uint8_t u8Index = m_PointerCache.u8MRUHead;

    while (u8Index != VRDP_POINTER_IDX_NULL)
    {
        pCacheElem = &m_PointerCache.paPointers[u8Index];

        if (u32ServerId == pCacheElem->u32ServerId)
        {
            break;
        }

        u8Index = pCacheElem->u8MRUNext;
    }

    if (u8Index != VRDP_POINTER_IDX_NULL)
    {
        /* The client already has an image of the pointer. */
        SERVERLOG(("Sending remote id %d\n", pCacheElem->u16RemoteId));

        /* Move the entry to the head of the MRU list. */
        if (pCacheElem->u8MRUPrev != VRDP_POINTER_IDX_NULL)
        {
            Assert (u8Index != m_PointerCache.u8MRUHead);

            /* The entry is not yet in the head. Exclude from list. */
            m_PointerCache.paPointers[pCacheElem->u8MRUPrev].u8MRUNext = pCacheElem->u8MRUNext;

            if (pCacheElem->u8MRUNext != VRDP_POINTER_IDX_NULL)
            {
                m_PointerCache.paPointers[pCacheElem->u8MRUNext].u8MRUPrev = pCacheElem->u8MRUPrev;
            }
            else
            {
                Assert (u8Index == m_PointerCache.u8MRUTail);
                m_PointerCache.u8MRUTail = pCacheElem->u8MRUPrev;
            }

            /* Insert the entry at the head of MRU list. */
            pCacheElem->u8MRUNext = m_PointerCache.u8MRUHead;
            pCacheElem->u8MRUPrev = VRDP_POINTER_IDX_NULL;

            if (m_PointerCache.u8MRUHead == VRDP_POINTER_IDX_NULL)
            {
                /* The only element in the cache. */
                m_PointerCache.u8MRUTail = u8Index;
            }
            else
            {
                m_PointerCache.paPointers[m_PointerCache.u8MRUHead].u8MRUPrev = u8Index;
            }

            m_PointerCache.u8MRUHead = u8Index;
        }
        else
        {
            /* This must be the head of the MRU list then. */
            Assert (u8Index == m_PointerCache.u8MRUHead);
        }

        m_vrdptp.OutputPointerCached (Stream (), pCacheElem->u16RemoteId);
    }
    else
    {
        /* The client does not yet have the image. */
        SERVERLOG(("Sending color pointer\n"));

        /* Find a free slot in the cache for the new pointer. */
        pCacheElem = m_PointerCache.paPointers;

        for (u8Index = 0; u8Index < m_PointerCache.u8Pointers; u8Index++, pCacheElem++)
        {
            if (pCacheElem->u16RemoteId == VRDP_POINTER_CLIENT_ID_NULL)
            {
                break;
            }
        }

        if (u8Index < m_PointerCache.u8Pointers)
        {
            /* There is still a place in the cache. */
            /* Do nothing. */
        }
        else
        {
            /* Reuse the tile of MRU list. Exclude it from the list. */
            u8Index = m_PointerCache.u8MRUTail;
            pCacheElem = &m_PointerCache.paPointers[u8Index];

            if (m_PointerCache.u8MRUTail == m_PointerCache.u8MRUHead)
            {
                /* A case of cache for 1 pointer only. */
                m_PointerCache.u8MRUTail = m_PointerCache.u8MRUHead = VRDP_POINTER_IDX_NULL;
            }
            else
            {
                m_PointerCache.u8MRUTail = pCacheElem->u8MRUPrev;
                m_PointerCache.paPointers[m_PointerCache.u8MRUTail].u8MRUNext = VRDP_POINTER_IDX_NULL;
            }
        }

        /* Initialize the slot as the head of the MRU list. */
        pCacheElem->u32ServerId = u32ServerId;
        pCacheElem->u16RemoteId = u8Index;
        pCacheElem->u8MRUNext   = m_PointerCache.u8MRUHead;
        pCacheElem->u8MRUPrev   = VRDP_POINTER_IDX_NULL;

        if (m_PointerCache.u8MRUHead == VRDP_POINTER_IDX_NULL)
        {
            /* First element in the cache. */
            m_PointerCache.u8MRUTail = u8Index;
        }
        else
        {
            m_PointerCache.paPointers[m_PointerCache.u8MRUHead].u8MRUPrev = u8Index;
        }

        m_PointerCache.u8MRUHead = u8Index;

        SERVERLOG(("Allocated remote index %d\n", u8Index));

        m_vrdptp.OutputPointerColor (m_pServer, Stream (), u32ServerId, pCacheElem->u16RemoteId);
    }
}

static VRDEMOUSEPTRDATA *transformColorPointer (VRDEMOUSEPTRDATA *pPointerOrig)
{
    /** @todo implement */
    return pPointerOrig;
}

static void transformFreeColorPointer (VRDEMOUSEPTRDATA *pPointerOrig, VRDEMOUSEPTRDATA *pPointer)
{
   /** @todo implement */
   NOREF(pPointer);
   NOREF(pPointerOrig);
}


/*
 *
 * VRDPTP methods
 *
 */

#define TS_PTRMSGTYPE_SYSTEM   0x0001
#define TS_PTRMSGTYPE_POSITION 0x0003
#define TS_PTRMSGTYPE_COLOR    0x0006
#define TS_PTRMSGTYPE_CACHED   0x0007
#define TS_PTRMSGTYPE_POINTER  0x0008

#pragma pack(1)
typedef struct RDPPointerHdr
{
    uint16_t u16Type;                  /* Type of the pointer PDU: TS_PTRMSGTYPE_*. */
    uint16_t u16Pad;                   /* Padding. */
} RDPPointerHdr;

typedef struct RDPPointerAttr /* TS_COLORPOINTERATTRIBUTE */
{
    uint16_t u16CacheIndex;

    uint16_t u16HotSpotX;
    uint16_t u16HotSpotY;

    uint16_t u16Width;
    uint16_t u16Height;

    uint16_t u16LengthAndMask;
    uint16_t u16LengthXorMask;
} RDPPointerAttr;

typedef struct RDPPointerColor
{
    RDPPointerHdr hdr;

    RDPPointerAttr attr;

    /* 24bpp RGB values of XOR (color) mask. Total attr.u16LengthXorMask bytes. */

    /* 1bpp AND mask. Total attr.u16LengthAndMask bytes. */

    /* uint8_t u8Pad; */
} RDPPointerColor;

typedef struct RDPPointerColorNew /* TS_POINTERATTRIBUTE */
{
    RDPPointerHdr hdr;

    uint16_t u16XorBpp;

    RDPPointerAttr attr;

    /* u16XorBpp RGB values of XOR (color) mask. Total attr.u16LengthXorMask bytes. */

    /* 1bpp AND mask. Total attr.u16LengthAndMask bytes. */

    /* uint8_t u8Pad; */
} RDPPointerColorNew;

typedef struct RDPPointerCached
{
    RDPPointerHdr hdr;

    uint16_t u16CacheIndex;
} RDPPointerCached;
#pragma pack(1)

static void findPointerArea(const uint8_t *pu8AndMask,
                            const uint8_t *pu8XorMask,
                            uint16_t width, uint16_t height,
                            uint16_t *pxStart, uint16_t *pyStart,
                            uint16_t *pxEnd, uint16_t *pyEnd)
{
    unsigned y;
    unsigned x;

    const uint16_t CoordNull = UINT16_MAX;

    bool fFirstFound;

    uint16_t xStartAnd = CoordNull;
    uint16_t yStartAnd = CoordNull;
    uint16_t xEndAnd = CoordNull;
    uint16_t yEndAnd = CoordNull;

    if (pu8AndMask)
    {
        /*
         * Find the first and last not empty line of the AND mask. First assign to special value.
         */
        const uint8_t *pu8And = pu8AndMask;
        const uint32_t cbAndRow = (width + 7) / 8;
        const uint8_t maskLastByte = (uint8_t)( 0xFF << (cbAndRow * 8 - width) );

        Assert(cbAndRow > 0);

        fFirstFound = false;
        for (y = 0; y < height; y++, pu8And += cbAndRow)
        {
            /* For each complete byte in the row. */
            for (x = 0; x < cbAndRow - 1; x++)
            {
                if (pu8And[x] != 0xFF)
                {
                    /* Not empty line. */
                    if (fFirstFound)
                    {
                        yEndAnd = y;
                    }
                    else
                    {
                        fFirstFound = true;
                        yStartAnd = y;
                        yEndAnd = y;
                    }
                    break;
                }
            }

            /* Last byte. */
            if ((pu8And[cbAndRow - 1] & maskLastByte) != maskLastByte)
            {
                /* Not empty line. */
                if (fFirstFound)
                {
                    yEndAnd = y;
                }
                else
                {
                    fFirstFound = true;
                    yStartAnd = y;
                    yEndAnd = y;
                }
            }
        }

        /*
         * Find the first and last not empty column of the AND mask.
         */

        /* For all bit columns. */
        fFirstFound = false;
        for (x = 0; x < width; x++)
        {
            pu8And = pu8AndMask + x/8;     /* Currently checking byte. */
            uint8_t mask = 1 << (7 - x%8); /* Currently checking bit in the byte. */

            for (y = 0; y < height; y++, pu8And += cbAndRow)
            {
                if ((*pu8And & mask) == 0)
                {
                    /* Not empty column. */
                    if (fFirstFound)
                    {
                        xEndAnd = x;
                    }
                    else
                    {
                        fFirstFound = true;
                        xStartAnd = x;
                        xEndAnd = x;
                    }
                    break;
                }
            }
        }
    }

    /*
     * XOR mask.
     */
    const uint32_t *pu32XorStart = (uint32_t *)pu8XorMask;
    const uint32_t *pu32Xor;

    /* First set to a special value, which means that the XOR mask is empty. */
    uint16_t xStartXor = CoordNull;
    uint16_t yStartXor = CoordNull;
    uint16_t xEndXor = CoordNull;
    uint16_t yEndXor = CoordNull;

    /*
     * Find the first and last not empty lines in the XOR mask.
     */
    pu32Xor = pu32XorStart;
    fFirstFound = false;
    for (y = 0; y < height; y++, pu32Xor += width)
    {
        for (x = 0; x < width; x++)
        {
            if (pu32Xor[x] != 0)
            {
                /* Not empty line. */
                if (fFirstFound)
                {
                    yEndXor = y;
                }
                else
                {
                    fFirstFound = true;
                    yStartXor = y;
                    yEndXor = y;
                }
                break;
            }
        }
    }

    /*
     * Find the first and last not empty column of the XOR mask.
     */
    fFirstFound = false;
    for (x = 0; x < width; x++)
    {
        pu32Xor = pu32XorStart + x; /* Currently checking dword. */

        for (y = 0; y < height; y++, pu32Xor += width)
        {
            if (*pu32Xor != 0)
            {
                /* Not empty column. */
                if (fFirstFound)
                {
                    xEndXor = x;
                }
                else
                {
                    fFirstFound = true;
                    xStartXor = x;
                    xEndXor = x;
                }
                break;
            }
        }
    }

    VRDPTPLOG(("findPointerArea: xor %u,%u %u,%u; and %u,%u %u,%u\n",
               xStartXor, yStartXor, xEndXor, yEndXor,
               xStartAnd, yStartAnd, xEndAnd, yEndAnd));

    if (   xStartAnd == CoordNull
        || yStartAnd == CoordNull)
    {
        /* AND mask was empty. */
        if (   xStartXor == CoordNull
            || yStartXor == CoordNull)
        {
            /* XorMask was empty */
            *pxStart = 0;
            *pyStart = 0;
            *pxEnd = 1; /* Exclusive */
            *pyEnd = 1; /* Exclusive */
        }
        else
        {
            *pxStart = xStartXor;
            *pyStart = yStartXor;
            *pxEnd = xEndXor + 1; /* Inclusive->exclusive */
            *pyEnd = yEndXor + 1; /* Inclusive->exclusive */
        }
    }
    else
    {
        /* AND mask was not empty. */
        if (   xStartXor == CoordNull
            || yStartXor == CoordNull)
        {
            /* XorMask was empty */
            *pxStart = xStartAnd;
            *pyStart = yStartAnd;
            *pxEnd = xEndAnd + 1; /* Inclusive->exclusive */
            *pyEnd = yEndAnd + 1; /* Inclusive->exclusive */
        }
        else
        {
            /* Both AND and XOR masks. */
            *pxStart = RT_MIN(xStartXor, xStartAnd);
            *pyStart = RT_MIN(yStartXor, yStartAnd);
            *pxEnd = RT_MAX(xEndXor, xEndAnd) + 1; /* Inclusive->exclusive */
            *pyEnd = RT_MAX(yEndXor, yEndAnd) + 1; /* Inclusive->exclusive */
        }
    }
}

/* Generate an AND mask for alpha pointers.
 */
static void mousePointerGenerateANDMask(uint8_t *pu8DstAndMask, int cbDstAndMask,
                                        const uint8_t *pu8SrcAlpha, int w, int h)
{
    memset(pu8DstAndMask, 0xFF, cbDstAndMask);

    int y;
    for (y = 0; y < h; y++)
    {
        uint8_t bitmask = 0x80;

        int x;
        for (x = 0; x < w; x++, bitmask >>= 1)
        {
            if (bitmask == 0)
            {
                bitmask = 0x80;
            }

            /* Whether alpha channel value is not transparent enough for the pixel to be seen. */
            if (pu8SrcAlpha[x * 4 + 3] > 0x7f)
            {
                pu8DstAndMask[x / 8] &= ~bitmask;
            }
        }

        /* Point to next source and dest scans. */
        pu8SrcAlpha += w * 4;
        pu8DstAndMask += (w + 7) / 8;
    }
}

static bool vrdpIsPointer1BPP(const uint8_t *pu8XorMask,
                              uint16_t u16SrcWidth,
                              uint16_t u16SrcHeight,
                              uint16_t u16StartX,
                              uint16_t u16StartY,
                              uint16_t u16Width,
                              uint16_t u16Height)
{
    RT_NOREF1(u16SrcHeight);
    /* Check if the not alpha pointer only have 0 and 0xFFFFFF pixels. */
    const uint8_t *pu8Src;
    uint16_t x, y;

    uint16_t cbSrcXorLine = u16SrcWidth * 4;

    pu8Src = pu8XorMask + u16StartY * cbSrcXorLine;

    for (y = 0; y < u16Height ; y++)
    {
        for (x = 0; x < u16Width; x++)
        {
            uint32_t u32Pixel = *(uint32_t *)&pu8Src[4 * (x + u16StartX)];
            u32Pixel &= 0xFFFFFF;
            if (u32Pixel != 0 && u32Pixel != 0xFFFFFF)
            {
                return false;
            }
        }

        pu8Src += cbSrcXorLine;
    }

    return true;
}

static int vrdpMakeMousePointer1(RDPPointerAttr **ppAttr,
                                 size_t *pcbAttr,
                                 uint16_t u16DstWidth,
                                 uint16_t u16DstHeight,
                                 const uint8_t *pu8AndMask,
                                 const uint8_t *pu8XorMask,
                                 uint16_t u16SrcWidth,
                                 uint16_t u16SrcHeight,
                                 uint16_t u16StartX,
                                 uint16_t u16StartY,
                                 uint16_t u16Width,
                                 uint16_t u16Height)
{
    int rc = VINF_SUCCESS;

    NOREF(u16SrcHeight);

    /* Calculate the size of pointer attributes. */
    uint16_t cbDstXorLine = (u16DstWidth + 7) / 8;
    cbDstXorLine = (cbDstXorLine + 1) & ~1; /* 2 bytes align. */

    uint16_t cbDstAndLine = (u16DstWidth + 7) / 8;
    cbDstAndLine = (cbDstAndLine + 1) & ~1; /* 2 bytes align. */

    uint16_t cbDstXorMask = cbDstXorLine * u16DstHeight;
    uint16_t cbDstAndMask = cbDstAndLine * u16DstHeight;

    size_t cbAttr = sizeof(RDPPointerAttr) + cbDstXorMask + cbDstAndMask + 1;

    VRDPTPLOG(("Pointer dst: %dx%d, xorline %d, andline %d\n",
               u16DstWidth, u16DstHeight, cbDstXorLine, cbDstAndLine));

    RDPPointerAttr *pAttr = (RDPPointerAttr *)VRDPMemAlloc(cbAttr);
    if (pAttr)
    {
        pAttr->u16CacheIndex = 0;

        pAttr->u16HotSpotX = 0;
        pAttr->u16HotSpotY = 0;

        pAttr->u16Width = u16DstWidth;
        pAttr->u16Height = u16DstHeight;

        pAttr->u16LengthAndMask = cbDstAndMask;
        pAttr->u16LengthXorMask = cbDstXorMask;

        uint8_t *pu8DstXorMask = (uint8_t*)pAttr + sizeof(RDPPointerAttr);
        uint8_t *pu8DstAndMask = pu8DstXorMask + cbDstXorMask;
        uint8_t *pu8DstPad = pu8DstAndMask + cbDstAndMask;

        memset(pu8DstXorMask, 0x00, cbDstXorMask);
        memset(pu8DstAndMask, 0xFF, cbDstAndMask);
        pu8DstPad[0] = 0;

        uint16_t cbSrcXorLine = u16SrcWidth * 4;
        uint16_t cbSrcAndLine = (u16SrcWidth + 7) / 8;

        const uint8_t *pu8Src;
        uint8_t *pu8Dst;
        uint16_t x, y;

        /* Copy XOR mask */
        pu8Src = pu8XorMask + u16StartY * cbSrcXorLine;
        pu8Dst = pu8DstXorMask;

        for (y = 0; y < u16Height ; y++)
        {
            for (x = 0; x < u16Width; x++)
            {
                uint32_t u32Pixel = *(uint32_t *)&pu8Src[4 * (x + u16StartX)];
                if (u32Pixel)
                {
                    uint32_t byteIndex = x / 8;
                    uint32_t bitIndex = x % 8;

                    pu8Dst[byteIndex] |= (1 << (7 - bitIndex));
                }
            }

            pu8Src += cbSrcXorLine;
            pu8Dst += cbDstXorLine;
        }

        /* Copy AND mask. */
        pu8Src = pu8AndMask + u16StartY * cbSrcAndLine;
        pu8Dst = pu8DstAndMask;

        for (y = 0; y < u16Height; y++)
        {
            for (x = 0; x < u16Width; x++)
            {
                uint32_t byteIndex = (x + u16StartX) / 8;
                uint32_t bitIndex = (x + u16StartX) % 8;

                bool bit = (pu8Src[byteIndex] & (1 << (7 - bitIndex))) != 0;

                if (!bit)
                {
                    byteIndex = x / 8;
                    bitIndex = x % 8;

                    pu8Dst[byteIndex] &= ~(1 << (7 - bitIndex));
                }
            }

            pu8Src += cbSrcAndLine;
            pu8Dst += cbDstAndLine;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *ppAttr = pAttr;
        *pcbAttr = cbAttr;
    }
    else
    {
        if (pAttr)
        {
            VRDPMemFree(pAttr);
        }
    }

    return rc;
}

static int vrdpMakeMousePointerRGB(uint16_t cbDstBytesPerPixel,
                                   RDPPointerAttr **ppAttr,
                                   size_t *pcbAttr,
                                   uint16_t u16DstWidth,
                                   uint16_t u16DstHeight,
                                   const uint8_t *pu8AndMask,
                                   const uint8_t *pu8XorMask,
                                   uint16_t u16SrcWidth,
                                   uint16_t u16SrcHeight,
                                   uint16_t u16StartX,
                                   uint16_t u16StartY,
                                   uint16_t u16Width,
                                   uint16_t u16Height)
{
    VRDPTPLOG(("Pointer RGB: cbPixel %d\n",
               cbDstBytesPerPixel));

    if (   cbDstBytesPerPixel != 2
        && cbDstBytesPerPixel != 3
        && cbDstBytesPerPixel != 4)
    {
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    NOREF(u16SrcHeight);

    /* Calculate the size of pointer attributes. */
    uint16_t cbDstXorLine = u16DstWidth * cbDstBytesPerPixel;
    cbDstXorLine = (cbDstXorLine + 1) & ~1; /* 2 bytes align. */

    uint16_t cbDstAndLine = (u16DstWidth + 7) / 8;
    cbDstAndLine = (cbDstAndLine + 1) & ~1; /* 2 bytes align. */

    uint16_t cbDstXorMask = cbDstXorLine * u16DstHeight;
    uint16_t cbDstAndMask = cbDstAndLine * u16DstHeight;

    size_t cbAttr = sizeof(RDPPointerAttr) + cbDstXorMask + cbDstAndMask + 1;

    VRDPTPLOG(("Pointer dst: cbPixel %d, %dx%d, xorline %d, andline %d\n",
               cbDstBytesPerPixel, u16DstWidth, u16DstHeight, cbDstXorLine, cbDstAndLine));

    RDPPointerAttr *pAttr = (RDPPointerAttr *)VRDPMemAlloc(cbAttr);
    if (pAttr)
    {
        pAttr->u16CacheIndex = 0;

        pAttr->u16HotSpotX = 0;
        pAttr->u16HotSpotY = 0;

        pAttr->u16Width = u16DstWidth;
        pAttr->u16Height = u16DstHeight;

        pAttr->u16LengthAndMask = cbDstAndMask;
        pAttr->u16LengthXorMask = cbDstXorMask;

        uint8_t *pu8DstXorMask = (uint8_t*)pAttr + sizeof(RDPPointerAttr);
        uint8_t *pu8DstAndMask = pu8DstXorMask + cbDstXorMask;
        uint8_t *pu8DstPad = pu8DstAndMask + cbDstAndMask;

        memset(pu8DstXorMask, 0x00, cbDstXorMask);
        memset(pu8DstAndMask, 0xFF, cbDstAndMask);
        pu8DstPad[0] = 0;

        uint16_t cbSrcXorLine = u16SrcWidth * 4;
        uint16_t cbSrcAndLine = (u16SrcWidth + 7) / 8;

        const uint8_t *pu8Src;
        uint8_t *pu8Dst;
        uint16_t x, y;

        /* Copy XOR mask */
        pu8Src = pu8XorMask + u16StartY * cbSrcXorLine;
        pu8Dst = pu8DstXorMask + (u16DstHeight - 1) * cbDstXorLine;

        for (y = 0; y < u16Height ; y++)
        {
            if (cbDstBytesPerPixel == 2)
            {
                for (x = 0; x < u16Width; x++)
                {
                    uint32_t u32SrcPixel = *(uint32_t *)&pu8Src[4 * (x + u16StartX)];

                    /* rrrrr ggg ggg bbbbb */
                    uint8_t r = (uint8_t)(u32SrcPixel >> 16);
                    uint8_t g = (uint8_t)(u32SrcPixel >> 8);
                    uint8_t b = (uint8_t)(u32SrcPixel);

                    uint16_t u16DstPixel =   (((uint16_t)r >> 3) << 11)
                                           + (((uint16_t)g >> 2) << 5)
                                           +  ((uint16_t)b >> 3);

                    *(uint16_t *)&pu8Dst[x * 2] = u16DstPixel;
                }
            }
            else if (cbDstBytesPerPixel == 3)
            {
                for (x = 0; x < u16Width; x++)
                {
                    memcpy(pu8Dst + x * 3, &pu8Src[4 * (x + u16StartX)], 3);
                }
            }
            else if (cbDstBytesPerPixel == 4)
            {
                memcpy(pu8Dst, &pu8Src[4 * u16StartX], u16Width * 4);
            }

            pu8Src += cbSrcXorLine;
            pu8Dst -= cbDstXorLine;
        }

        /* Copy AND mask. */
        pu8Src = pu8AndMask + u16StartY * cbSrcAndLine;
        pu8Dst = pu8DstAndMask + (u16DstHeight - 1) * cbDstAndLine;

        for (y = 0; y < u16Height; y++)
        {
            for (x = 0; x < u16Width; x++)
            {
                uint32_t byteIndex = (x + u16StartX) / 8;
                uint32_t bitIndex = (x + u16StartX) % 8;

                bool bit = (pu8Src[byteIndex] & (1 << (7 - bitIndex))) != 0;

                if (!bit)
                {
                    byteIndex = x / 8;
                    bitIndex = x % 8;

                    pu8Dst[byteIndex] &= ~(1 << (7 - bitIndex));
                }
            }

            pu8Src += cbSrcAndLine;
            pu8Dst -= cbDstAndLine;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *ppAttr = pAttr;
        *pcbAttr = cbAttr;
    }
    else
    {
        if (pAttr)
        {
            VRDPMemFree(pAttr);
        }
    }

    return rc;
}

void VRDPTP::OutputPointerColor(VRDPServer *pServer,
                                VRDPStream *pStream,
                                uint32_t u32ServerId,
                                uint16_t u16RemoteId)
{
    /* Prepare a pointer update according to the client capabilities.
     */

    VRDEMOUSEPTRDATA *pPointerOrig = pServer->PointerCacheLock (u32ServerId);

    if (!pPointerOrig)
    {
        VRDPTPLOG(("Could not lock the pointer 0x%08X\n", u32ServerId));
        return;
    }

    VRDEMOUSEPTRDATA *pPointer = transformColorPointer (pPointerOrig);

    int rc;

    uint16_t u16MaxWidth;
    uint16_t u16MaxHeight;
    if (Data()->LargeMousePointers())
    {
        u16MaxWidth = 96;
        u16MaxHeight = 96;
    }
    else
    {
        u16MaxWidth = 32;
        u16MaxHeight = 32;
    }

    uint8_t *pu8AndMask = (uint8_t *)pPointer + sizeof(VRDEMOUSEPTRDATA);
    uint8_t *pu8XorMask = pu8AndMask + pPointer->u16MaskLen;
    uint32_t cbAndMask = pPointer->u16MaskLen;
    /* uint32_t cbXorMask = pPointer->u32DataLen; - unused */

    bool fAlpha = (cbAndMask == 0);

    if (fAlpha)
    {
        pu8AndMask = NULL;
    }

    /* Find the pointer area which will be actually used:
     * skip empty rows and lines from top left and
     * take maximum supported width and height into account.
     */
    uint16_t u16StartX = 0;
    uint16_t u16StartY = 0;
    uint16_t u16EndX = 0;
    uint16_t u16EndY = 0;
    uint16_t u16Width = pPointer->u16Width;
    uint16_t u16Height = pPointer->u16Height;

    VRDPTPLOG(("Pointer: %dx%d cbAnd %d, cbXor %d\n",
               u16Width, u16Height, pPointer->u16MaskLen, pPointer->u32DataLen));

    findPointerArea(pu8AndMask, pu8XorMask, u16Width, u16Height, &u16StartX, &u16StartY, &u16EndX, &u16EndY);

    /* Must not skip the hot spot. */
    u16StartX = RT_MIN(u16StartX, pPointer->u16HotX);
    u16StartY = RT_MIN(u16StartY, pPointer->u16HotY);

    u16Width = u16EndX - u16StartX;
    u16Height = u16EndY - u16StartY;

    u16Width = RT_MIN(u16Width, u16MaxWidth);
    u16Height = RT_MIN(u16Height, u16MaxHeight);

    /* The actual pointer area is u16StartX, u16StartY u16Width x u16Height */
    VRDPTPLOG(("Pointer area: %d,%d %dx%d (%d,%d)\n",
               u16StartX, u16StartY, u16Width, u16Height, u16EndX, u16EndY));

    /* Make at least 32x32 pointers to workaround problems with some clients. */
    uint16_t u16DstWidth = RT_MAX(u16Width, 32);
    uint16_t u16DstHeight = RT_MAX(u16Height, 32);

    RDPPointerAttr *pAttr = NULL;
    size_t cbAttr = 0;
    uint16_t u16NewPointerBPP = 0;

    bool fNewPointers = Data()->NewMousePointers();

    if (fAlpha)
    {
        /* Generate the AND mask anyway, it is needed for clients which do not support new pointers.
         * Also some client do not work without the mask even with 32BPP new pointers.
         */
        cbAndMask = (((pPointer->u16Width + 7) / 8) * pPointer->u16Height + 3) & ~3;

        pu8AndMask = (uint8_t *)alloca(cbAndMask);

        mousePointerGenerateANDMask(pu8AndMask, cbAndMask,
                                    pu8XorMask,
                                    pPointer->u16Width, pPointer->u16Height);

        VRDPTPLOG(("Pointer: AND mask generated\n"));
    }

    if (fNewPointers)
    {
        VRDPTPLOG(("Pointer: new, fAlpha %d\n", fAlpha));

        if (fAlpha)
        {
            rc = vrdpMakeMousePointerRGB(4, &pAttr, &cbAttr, u16DstWidth, u16DstHeight,
                                         pu8AndMask, pu8XorMask, pPointer->u16Width, pPointer->u16Height,
                                         u16StartX, u16StartY, u16Width, u16Height);
            u16NewPointerBPP = 32;
        }
        else
        {
            /* If possible, make black and white pointer. */
            if (vrdpIsPointer1BPP(pu8XorMask, pPointer->u16Width, pPointer->u16Height,
                                  u16StartX, u16StartY, u16Width, u16Height))
            {
                rc = vrdpMakeMousePointer1(&pAttr, &cbAttr, u16DstWidth, u16DstHeight,
                                           pu8AndMask, pu8XorMask, pPointer->u16Width, pPointer->u16Height,
                                           u16StartX, u16StartY, u16Width, u16Height);
                u16NewPointerBPP = 1;
            }
            else
            {
                /* Send the color pointer as 16BPP because some client do not support 24BPP. */
                rc = vrdpMakeMousePointerRGB(2, &pAttr, &cbAttr, u16DstWidth, u16DstHeight,
                                             pu8AndMask, pu8XorMask, pPointer->u16Width, pPointer->u16Height,
                                             u16StartX, u16StartY, u16Width, u16Height);
                u16NewPointerBPP = 16;
            }
        }
    }
    else
    {
        /* Can use only 24 BPP pointers with AND and XOR masks. */
        VRDPTPLOG(("Pointer: old 24\n"));
        rc = vrdpMakeMousePointerRGB(3, &pAttr, &cbAttr, u16DstWidth, u16DstHeight,
                                    pu8AndMask, pu8XorMask, pPointer->u16Width, pPointer->u16Height,
                                    u16StartX, u16StartY, u16Width, u16Height);
    }

    if (RT_SUCCESS(rc))
    {
        VRDPTPLOG(("Pointer: CacheIndex id%d\n", u16RemoteId));
        pAttr->u16CacheIndex = u16RemoteId;
        pAttr->u16HotSpotX   = pPointer->u16HotX - u16StartX;
        pAttr->u16HotSpotY   = pPointer->u16HotY - u16StartY;
    }

    transformFreeColorPointer (pPointerOrig, pPointer);
    pServer->PointerCacheUnlock (u32ServerId);

    if (RT_FAILURE(rc))
    {
        return;
    }

    uint32_t cbUpdate = fNewPointers?
                            sizeof(RDPPointerColorNew):
                            sizeof(RDPPointerColor);
    cbUpdate += (uint32_t)(cbAttr - sizeof(RDPPointerAttr)); /* Masks and padding. */

    const VRDPStream::MemoryBlock *pBlock;

    /* Reserve enough space for an pointer update with complete information. */
    pBlock = pStream->BeginBlock(VRDPUpdate_Pointer, cbUpdate);

    if (pBlock != NULL)
    {
        if (fNewPointers)
        {
            RDPPointerColorNew *pRDPPointerColorNew = (RDPPointerColorNew *)pBlock->pu8DstStart;

            pRDPPointerColorNew->hdr.u16Type = TS_PTRMSGTYPE_POINTER;
            pRDPPointerColorNew->hdr.u16Pad  = 0;

            pRDPPointerColorNew->u16XorBpp   = u16NewPointerBPP;

            memcpy(&pRDPPointerColorNew->attr, pAttr, cbAttr);
        }
        else
        {
            RDPPointerColor *pRDPPointerColor = (RDPPointerColor *)pBlock->pu8DstStart;

            pRDPPointerColor->hdr.u16Type = TS_PTRMSGTYPE_COLOR;
            pRDPPointerColor->hdr.u16Pad  = 0;

            memcpy(&pRDPPointerColor->attr, pAttr, cbAttr);
        }

        /* Complete the block. Pass how many bytes were actually written. */
        pStream->EndBlock(pBlock, cbUpdate);
    }

    VRDPMemFree(pAttr);

    return;
}

void VRDPTP::OutputPointerCached (VRDPStream *pStream, uint16_t u16RemoteId)
{
    const VRDPStream::MemoryBlock *pBlock;

    /* Reserve enough space for an pointer update with complete information. */
    pBlock = pStream->BeginBlock (VRDPUpdate_Pointer, sizeof (RDPPointerCached));

    if (pBlock == NULL)
    {
        return;
    }

    VRDPTPLOG(("Pointer: cached id%d\n", u16RemoteId));

    RDPPointerCached *pRDPPointerCached = (RDPPointerCached *)pBlock->pu8DstStart;

    pRDPPointerCached->hdr.u16Type     = TS_PTRMSGTYPE_CACHED;
    pRDPPointerCached->hdr.u16Pad      = 0;
    pRDPPointerCached->u16CacheIndex = u16RemoteId;

    /* Complete the block. Pass how many bytes were actually written. */
    pStream->EndBlock (pBlock, sizeof (RDPPointerCached));
}
