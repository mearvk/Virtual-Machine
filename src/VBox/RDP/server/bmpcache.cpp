/* $Id: bmpcache.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol Server - Bitmap Cache.
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

#include "utils.h"
#include "shadowbuffer.h"
#include "bmpcache.h"
#include "bmpcomp.h"

#include <iprt/heap.h>
#include <iprt/list.h>
#include <iprt/system.h>

#include <iprt/formats/bmp.h>

// #define VRDP_LOG_BITMAPS

#ifdef VRDP_LOG_BITMAPS
#include <stdio.h>


void logbmp (const VRDEDATABITS *pBitsHdr, const uint8_t *pu8Bits)
{
    static int sIdx = 0;

    LogRel(("logbmp: cb = %d\n", pBitsHdr->cbPixel));

    if (   pBitsHdr->cbPixel != 2
        && pBitsHdr->cbPixel != 3
        && pBitsHdr->cbPixel != 4)
    {
        return;
    }

    char szFilename[256];

    sprintf (szFilename, "bmp\\bc%04d.bmp", sIdx++);

    FILE *f = fopen (szFilename, "wb");

    LogRel(("logbmp: %p, %s\n", f, szFilename));

    if (f)
    {
        BMPFILEHDR fileHdr;
        RT_ZERO(fileHdr);
        fileHdr.uType      = BMP_HDR_MAGIC;
        fileHdr.cbFileSize = sizeof (BMPFILEHDR) + sizeof (BMPWIN3XINFOHDR) + pBitsHdr->cb;
        fileHdr.offBits    = sizeof (BMPFILEHDR) + sizeof (BMPWIN3XINFOHDR);

        BMPWIN3XINFOHDR coreHdr;
        RT_ZERO(coreHdr);
        coreHdr.cbSize      = sizeof (coreHdr);
        coreHdr.uWidth      = pBitsHdr->cWidth;
        coreHdr.uHeight     = -pBitsHdr->cHeight;
        coreHdr.cPlanes     = 1;
        coreHdr.cBits       = pBitsHdr->cbPixel * 8;
        coreHdr.cbSizeImage = pBitsHdr->cb;

        fwrite (&fileHdr, 1, sizeof (fileHdr), f);
        fwrite (&coreHdr, 1, sizeof (coreHdr), f);
        fwrite (pu8Bits, 1, pBitsHdr->cb, f);

        fclose (f);
    }
}
#endif /* VRDP_LOG_BITMAPS */

/*
 * VRDP bitmap cache.
 *
 * The VRDP server maintains single cache for all clients.
 * The cache is always active, regardless of client connections.
 *
 * Incoming bitmaps have a hash value assigned by the caller
 * (guest additions).
 *
 * When bitmaps are placed to the cache, they are tiled.
 * Each tile is addressed as { HASH, X, Y }. X, Y are
 * tile coordinates.
 *
 * Every client maintains mapping from { HASH, X, Y }
 * to RDP { ID, IDX }.
 *
 * There are 3 RDP cache IDs. They contain bitmaps up to specific size:
 *
 * Cache id      Maximum size in pixels (w*h) Maximum IDX
 *        0      0x100                        0x78
 *        1      0x400                        0x78
 *        2      0x1000                       0x151
 *
 * The latter corresponds to maximum tile size of 64x64.
 *
 * When bitmap is cached, it is compressed into tiles
 * using the VRDPCompressedBitmap object.
 *
 * The bitmap cache is executed on the OUTPUT thread.
 *
 */

/*
 * Cached bitmap entry.
 */
typedef struct _BMPCACHEENTRY
{
    /* The MRU list next. */
    struct _BMPCACHEENTRY *mruNext;
    /* The MRU list prev. */
    struct _BMPCACHEENTRY *mruPrev;

    /* Hash */
    VRDEBITMAPHASH hash;

    /* Size of uncompressed bitmap. */
    uint32_t cbBitmapCompressed;

    /* The compressed bitmap tiles. */
    VRDPBitmapCompressed bmp;

    _BMPCACHEENTRY(const VRDEBITMAPHASH *pHash)
        :
        mruNext (NULL),
        mruPrev (NULL),
        cbBitmapCompressed (0),
        bmp ()
    {
        memcpy (hash, pHash, sizeof (VRDEBITMAPHASH));
    }

    ~_BMPCACHEENTRY() {};
} BMPCACHEENTRY;

typedef struct BCHEAPBLOCK
{
    RTLISTNODE NodeHeapBlock;
    uint32_t u32ScreenAccessKey;
    uint32_t u32ScreenId;
    int32_t i32Op;
    int32_t i32Keep;
    uint32_t cb;
} BCHEAPBLOCK;

/* Heap handle info. */
typedef struct BCHEAPHANDLEINTERNAL
{
    uint32_t u32Offset;
    uint32_t u32ScreenAccessKey;
    uint32_t u32ScreenId;
    uint32_t u32Reserved;
} BCHEAPHANDLEINTERNAL;
AssertCompileSize(BCHEAPHANDLEINTERNAL, sizeof (BCHEAPHANDLE));


/*
 * The bitmap cache.
 *
 * Heap is used by both APP and OUTPUT threads and protected by a critical section.
 * Bitmap cache entries are accessed only by the OUTPUT thread and do not require a lock.
 *
 */
typedef struct _BMPCACHE
{
    /* Bitmap heap lock. */
    VRDPLock *pBitmapHeapLock;

    /* Preallocated memory block for heap, where bitmaps are stored. */
    uint8_t *pu8BitmapHeap;
    uint32_t cbBitmapHeap;

    /* Heap for the pu8BitmapHeap memory block. */
    RTHEAPSIMPLE hHeap;

    /* The cached bitmaps MRU list.
     * Used for fast search of cached bitmaps, which were recently accessed.
     */
    BMPCACHEENTRY *pHeadMRU;
    BMPCACHEENTRY *pTailMRU;

    /* Total size of compressed cached bitmaps. */
    uint32_t cbBitmaps;

    /* Maximum allowed size of compressed bitmaps. */
    uint32_t cbBitmapsLimit;

    /* Current "generation" of bitmaps for each screen in the cache. */
    volatile uint32_t au32ScreenAccessKeys[64];

    /** List of BCHEAPBLOCK nodes. */
    RTLISTANCHOR ListHeapBlocks;
    bool fResetHeap;
} BMPCACHE;



static int bcHeapLock(BMPCACHE *pbc)
{
    return VRDPLock::Lock(pbc->pBitmapHeapLock);
}

static void bcHeapUnlock(BMPCACHE *pbc)
{
    VRDPLock::Unlock(pbc->pBitmapHeapLock);
}

static uint8_t *bcResolveBitmapHandle(PBMPCACHE pbc, const BCHEAPHANDLEINTERNAL *ph)
{
    uint32_t u32ScreenId = ph->u32ScreenId;
    uint32_t u32Offset = ph->u32Offset;
    uint32_t u32ScreenAccessKey = ph->u32ScreenAccessKey;

    if (u32ScreenId >= RT_ELEMENTS(pbc->au32ScreenAccessKeys))
    {
        BCLOG(("BC: wrong screen id in a handle %d\n", u32ScreenId));
        return NULL;
    }

    uint32_t u32Tmp = ASMAtomicReadU32(&pbc->au32ScreenAccessKeys[u32ScreenId]);
    if (u32Tmp != u32ScreenAccessKey)
    {
        BCLOG(("BC: obsolete handle %d/%d cache %d\n", u32ScreenAccessKey, u32Offset, u32Tmp));
        return NULL;
    }

    if (!(0 < u32Offset && u32Offset + sizeof (BCHEAPBLOCK) + sizeof (VRDEDATABITS) < pbc->cbBitmapHeap))
    {
        AssertMsgFailed(("0x%x, 0x%llx", u32Offset, pbc->cbBitmapHeap));
        return NULL;
    }

    return pbc->pu8BitmapHeap + u32Offset;
}

static void bcHeapFreeObsoleteBlocks(BMPCACHE *pbc)
{
    BCHEAPBLOCK *pBlock = RTListGetFirst(&pbc->ListHeapBlocks,
                                         BCHEAPBLOCK,
                                         NodeHeapBlock);
    while (pBlock)
    {
        BCHEAPBLOCK *pNext = RTListNodeIsLast(&pbc->ListHeapBlocks, &pBlock->NodeHeapBlock)?
                                 NULL:
                                 RTListNodeGetNext(&pBlock->NodeHeapBlock,
                                                   BCHEAPBLOCK, NodeHeapBlock);

        if (pBlock->u32ScreenId < RT_ELEMENTS(pbc->au32ScreenAccessKeys))
        {
            uint32_t u32Tmp = ASMAtomicReadU32(&pbc->au32ScreenAccessKeys[pBlock->u32ScreenId]);

            if (   u32Tmp != pBlock->u32ScreenAccessKey
                && pBlock->i32Keep <= 0)
            {
                RTListNodeRemove(&pBlock->NodeHeapBlock);
                RTHeapSimpleFree(pbc->hHeap, pBlock);
            }
        }
        else
        {
            AssertFailed();
        }

        pBlock = pNext;
    }
}

/*
 * Allocate a cached bitmap entry.
 */
static BMPCACHEENTRY *bceAlloc (const VRDEBITMAPHASH *pHash)
{
    BMPCACHEENTRY *pbce = new BMPCACHEENTRY(pHash);

    if (!pbce)
    {
        BCLOG(("Could not allocate cache entry!!!\n"));
    }

    return pbce;
}

/*
 * Deallocate a cached bitmap entry.
 */
static void bceFree (const BMPCACHEENTRY *pbce)
{
    if (pbce)
    {
        delete pbce;
    }
}

/*
 * Move an entry to the head of the MRU list.
 */
static void bcMoveToMRUHead (BMPCACHE *pbc, BMPCACHEENTRY *pbce)
{
    if (pbce->mruPrev)
    {
        /* The entry is not yet in the head. Exclude from list. */
        pbce->mruPrev->mruNext = pbce->mruNext;

        if (pbce->mruNext)
        {
            pbce->mruNext->mruPrev = pbce->mruPrev;
        }
        else
        {
            pbc->pTailMRU = pbce->mruPrev;
        }

        /* Insert the entry at the head of MRU list. */
        pbce->mruPrev = NULL;
        pbce->mruNext = pbc->pHeadMRU;

        /* Since we are working with an existing not head entry, we must have a head. */
        Assert (pbc->pHeadMRU);

        pbc->pHeadMRU->mruPrev = pbce;
        pbc->pHeadMRU = pbce;
    }
}

/*
 * Search the bitmap list for given hash value.
 */
static BMPCACHEENTRY *bcSearchHash (PBMPCACHE pbc, const VRDEBITMAPHASH *pHash)
{
    BCLOG(("searching %08X %08X %08X %08X\n",
               *(uint32_t *)&(*pHash)[0],
               *(uint32_t *)&(*pHash)[4],
               *(uint32_t *)&(*pHash)[8],
               *(uint32_t *)&(*pHash)[12]
               ));

    BMPCACHEENTRY *pbce = pbc->pHeadMRU;

    while (pbce)
    {
        if (memcmp (pbce->hash, pHash, sizeof (VRDEBITMAPHASH)) == 0)
        {
            break;
        }

        pbce = pbce->mruNext;
    }

    BCLOG(("found %p\n", pbce));

    return pbce;
}

/*
 * Deletes an entry from the cache.
 */
static void bcDeleteEntry (BMPCACHE *pbc, const BMPCACHEENTRY *pbce)
{
    if (pbce->mruPrev || pbce->mruNext || (pbce == pbc->pHeadMRU && pbce == pbc->pTailMRU))
    {
        /* Exclude from the MRU list. */
        if (pbce->mruPrev)
        {
            pbce->mruPrev->mruNext = pbce->mruNext;
        }
        else
        {
            pbc->pHeadMRU = pbce->mruNext;
        }

        if (pbce->mruNext)
        {
            pbce->mruNext->mruPrev = pbce->mruPrev;
        }
        else
        {
            pbc->pTailMRU = pbce->mruPrev;
        }
    }
    else
    {
        BCLOG(("Deleting an entry that is not in the MRU list!!!\n"));
    }

    /* Deallocate memory. */
    bceFree (pbce);

    return;
}

/* Actually compresses and puts bitmap in the cache. */
static bool bcCacheBitmap (PBMPCACHE pbc, const VRDEBITMAPHASH *pHash, const VRDEDATABITS *pBitsHdr, const uint8_t *pu8Bits)
{
    BCLOG(("%dx%d cbPixel %d\n",
            pBitsHdr->cWidth, pBitsHdr->cHeight, pBitsHdr->cbPixel));

    /* Verify whether the hash value is already in the cache. */
    BMPCACHEENTRY *pbce = bcSearchHash (pbc, pHash);

    if (pbce)
    {
        /* That means that the display driver from a secondary monitor
         * has reported the same bitmap or that the same bitmaps are reported
         * after a reboot.
         * tell the caller that the bitmap was successfully cached.
         */
        BCLOG(("Cache entry already in the cache.\n"));
        return true;
    }

    /* Allocate memory for the cache entry. */
    pbce = bceAlloc (pHash);

    if (!pbce)
    {
        return false;
    }

    /* Compress the bitmap bits. */
    pbce->bmp.Compress (pu8Bits, pBitsHdr->cWidth * pBitsHdr->cbPixel,
                        pBitsHdr->cWidth, pBitsHdr->cHeight,
                        pBitsHdr->cbPixel * 8, VRDP_F_BMPCOMP_TILE);

    pbce->cbBitmapCompressed = pbce->bmp.CompressedSize();

    BCLOG(("BCLIMIT: add %d/%d/%d\n", pbce->cbBitmapCompressed, pbc->cbBitmaps, pbc->cbBitmapsLimit));

    if (pbce->cbBitmapCompressed >= pbc->cbBitmapsLimit)
    {
        BCLOG(("BCLIMIT: bitmap too large: %d >= %d\n", pbce->cbBitmapCompressed, pbc->cbBitmapsLimit));
        bceFree(pbce);
        return false;
    }

    /* Delete old entries. */
    while (pbce->cbBitmapCompressed + pbc->cbBitmaps > pbc->cbBitmapsLimit && pbc->pTailMRU)
    {
        Assert(pbc->cbBitmaps >= pbc->pTailMRU->cbBitmapCompressed);
        BCLOG(("BCLIMIT: removing tail %d/%d\n", pbc->pTailMRU->cbBitmapCompressed, pbc->cbBitmaps));
        pbc->cbBitmaps -= pbc->pTailMRU->cbBitmapCompressed;
        bcDeleteEntry (pbc, pbc->pTailMRU);
    }

    if (pbce->cbBitmapCompressed + pbc->cbBitmaps > pbc->cbBitmapsLimit)
    {
        AssertFailed(); /* Should not happen. */
        bceFree(pbce);
        return false;
    }

    /* Insert the entry as the head in the MRU list. */
    pbc->cbBitmaps += pbce->cbBitmapCompressed;

    pbce->mruPrev = NULL;
    pbce->mruNext = pbc->pHeadMRU;

    if (pbc->pHeadMRU)
    {
        pbc->pHeadMRU->mruPrev = pbce;
    }
    else
    {
        pbc->pTailMRU = pbce;
    }

    pbc->pHeadMRU = pbce;

    BCLOG(("inserted %08X %08X %08X %08X as %p\n",
               *(uint32_t *)&(*pHash)[0],
               *(uint32_t *)&(*pHash)[4],
               *(uint32_t *)&(*pHash)[8],
               *(uint32_t *)&(*pHash)[12],
               pbce
               ));

    return true;
}

/*
 * Puts the bitmap into the cache.
 */
bool BCCacheBitmap (PBMPCACHE pbc, const BCHEAPHANDLE *pHandle, unsigned uScreenId)
{
    /* The memory block has been allocated on the APP thread but will not be touched by the APP thread anymore.
     * So this OUTPUT thread can access the block without a lock.
     */
    const BCHEAPHANDLEINTERNAL *ph = (BCHEAPHANDLEINTERNAL *)pHandle;

    BCLOG(("BCLIMIT: received handle = %d/%d/0x%x\n", ph->u32ScreenId, ph->u32ScreenAccessKey, ph->u32Offset));

    uint32_t u32Offset = ph->u32Offset;
    uint32_t u32ScreenAccessKey = ph->u32ScreenAccessKey;

    if (uScreenId != ph->u32ScreenId)
    {
        AssertMsgFailed(("BCLIMIT: screen id mismatch %d/%d\n", uScreenId, ph->u32ScreenId));
        return false;
    }

    if (ph->u32ScreenId >= RT_ELEMENTS(pbc->au32ScreenAccessKeys))
    {
        AssertMsgFailed(("BCLIMIT: wrong screen id %d\n", ph->u32ScreenId));
        return false;
    }

    uint32_t u32Tmp = ASMAtomicReadU32(&pbc->au32ScreenAccessKeys[ph->u32ScreenId]);
    if (u32Tmp != u32ScreenAccessKey)
    {
        BCLOG(("BCLIMIT: obsolete handle %d/0x%d currently %d\n", u32ScreenAccessKey, u32Offset, u32Tmp));
        return false;
    }

    if (!(0 < u32Offset && u32Offset + sizeof (BCHEAPBLOCK) + sizeof (VRDEBITMAPHASH) + sizeof (VRDEDATABITS) < pbc->cbBitmapHeap))
    {
        AssertMsgFailed(("0x%x, 0x%llx", u32Offset, pbc->cbBitmapHeap));
        return false;
    }

    uint8_t *pu8BitmapBlock = pbc->pu8BitmapHeap + u32Offset;

    uint8_t *pu8Src = pu8BitmapBlock;

    BCHEAPBLOCK *pBlock = (BCHEAPBLOCK *)pu8Src;
    pu8Src += sizeof (BCHEAPBLOCK);
    Assert(pBlock->u32ScreenAccessKey == u32ScreenAccessKey);

    VRDEBITMAPHASH *pHash = (VRDEBITMAPHASH *)pu8Src;
    pu8Src += sizeof (VRDEBITMAPHASH);

    VRDEDATABITS *pBitsHdr = (VRDEDATABITS *)pu8Src;
    pu8Src += sizeof (VRDEDATABITS);

    uint8_t *pu8Bits = pu8Src;

    if (!(u32Offset + sizeof (BCHEAPBLOCK) + sizeof (VRDEBITMAPHASH) + sizeof (VRDEDATABITS) + pBitsHdr->cb <= pbc->cbBitmapHeap))
    {
        AssertMsgFailed(("0x%x, 0x%llx", u32Offset, pbc->cbBitmapHeap));
        return false;
    }

    /* The bitmap bits are in the framebuffer coordinates.
     * The entire bitmap must be rotated before putting to the cache.
     * Since the shadow buffer hold the transformation information,
     * it transforms the bitmap.
     */
    VRDEDATABITS transBitsHdr;
    const uint8_t *pu8TransBits = NULL;
    bool fSuccess = shadowBufferTransformDataBits(ph->u32ScreenId, &transBitsHdr, &pu8TransBits, pBitsHdr, pu8Bits);

    if (fSuccess)
    {
        fSuccess = bcCacheBitmap (pbc, pHash, &transBitsHdr, pu8TransBits);

        shadowBufferFreeTransformDataBits (ph->u32ScreenId, &transBitsHdr, pu8TransBits, pBitsHdr, pu8Bits);
    }

    /* Free the heap block under lock. */
    int rc = bcHeapLock(pbc);

    if (RT_SUCCESS(rc))
    {
        if (pBlock->NodeHeapBlock.pPrev)
        {
            RTListNodeRemove(&pBlock->NodeHeapBlock);
            RTHeapSimpleFree(pbc->hHeap, pu8BitmapBlock);
            BCLOG(("BCLIMIT: dealloc block %d bytes free, offset 0x%x.\n", RTHeapSimpleGetFreeSize(pbc->hHeap), u32Offset));
        }
        else
        {
            AssertFailed();
        }

        /* Heap maintenance, */
        if (pbc->fResetHeap)
        {
            pbc->fResetHeap = false;

            /* Empty the heap. */
            bcHeapFreeObsoleteBlocks(pbc);

            BCLOG(("BCLIMIT: ResetHeap %d bytes free\n", RTHeapSimpleGetFreeSize(pbc->hHeap)));
        }

        bcHeapUnlock(pbc);
    }

    if (RT_FAILURE(rc))
    {
        return false;
    }

    return fSuccess;
}

void BCDeleteBitmap (PBMPCACHE pbc, const VRDEBITMAPHASH *pHash)
{
    BCLOG(("removed %08X %08X %08X %08X\n",
               *(uint32_t *)&(*pHash)[0],
               *(uint32_t *)&(*pHash)[4],
               *(uint32_t *)&(*pHash)[8],
               *(uint32_t *)&(*pHash)[12]
               ));

    BMPCACHEENTRY *pbce = bcSearchHash (pbc, pHash);

    if (pbce)
    {
        pbc->cbBitmaps -= pbce->cbBitmapCompressed;
        bcDeleteEntry (pbc, pbce);
    }
    else
    {
        BCLOG(("No bitmap to delete!!!\n"));
    }
}

PBMPCACHEENTRY BCFindBitmap (PBMPCACHE pbc, const VRDEBITMAPHASH *pHash)
{
    BMPCACHEENTRY *pbce = bcSearchHash (pbc, pHash);

    if (pbce)
    {
        bcMoveToMRUHead (pbc, pbce);
        BCLOG(("found %08X %08X %08X %08X as %p\n",
               *(uint32_t *)&(*pHash)[0],
               *(uint32_t *)&(*pHash)[4],
               *(uint32_t *)&(*pHash)[8],
               *(uint32_t *)&(*pHash)[12],
               pbce
               ));
    }

    return pbce;
}

VRDPBitmapCompressed *BCQueryBitmapCompressed (PBMPCACHEENTRY pbce)
{
    return &pbce->bmp;
}

/* Store the bitmap in the intermediate heap.
 *
 * If heap is full, this will fail.
 * Bitmaps from the heap will be consumed by OUTPUT thread.
 */
int BCStore(BCHEAPHANDLE *pHandle,
            PBMPCACHE pbc,
            int32_t i32Op, const void *pvData, size_t cbData,
            const VRDEDATABITS *pBitsHdr, const uint8_t *pu8Bits,
            uint32_t u32ScreenAccessKey, uint32_t u32ScreenId)
{
    BCLOG(("%dx%d cbPixel %d\n",
            pBitsHdr->cWidth, pBitsHdr->cHeight, pBitsHdr->cbPixel));

    if (!pbc)
    {
        return VERR_INVALID_PARAMETER;
    }

    if (u32ScreenId >= RT_ELEMENTS(pbc->au32ScreenAccessKeys))
    {
        return VERR_NOT_SUPPORTED;
    }

    int rc = bcHeapLock(pbc);

    if (RT_SUCCESS(rc))
    {
        /* Check if heap should be cleared from obsoleted bitmaps. */
        uint32_t u32Tmp = ASMAtomicReadU32(&pbc->au32ScreenAccessKeys[u32ScreenId]);
        if (u32Tmp != u32ScreenAccessKey)
        {
            BCLOG(("BCLIMIT: access key changed for screen %d: %d -> %d\n", u32ScreenId, u32Tmp, u32ScreenAccessKey));
            ASMAtomicWriteU32(&pbc->au32ScreenAccessKeys[u32ScreenId], u32ScreenAccessKey);
            pbc->fResetHeap = true;
        }

        size_t cbAlloc = sizeof (BCHEAPBLOCK) + cbData + sizeof (VRDEDATABITS) + pBitsHdr->cb;

        uint8_t *pu8 = (uint8_t *)RTHeapSimpleAlloc(pbc->hHeap, cbAlloc, 0);

        if (pu8)
        {
            uint8_t *pu8Dst = pu8;

            BCHEAPBLOCK *pBlock = (BCHEAPBLOCK *)pu8Dst;
            pBlock->u32ScreenId = u32ScreenId;
            pBlock->u32ScreenAccessKey = u32ScreenAccessKey;
            pBlock->i32Op = i32Op;
            pBlock->i32Keep = 0;
            pBlock->cb = (uint32_t)(cbAlloc - sizeof(BCHEAPBLOCK));
            pu8Dst += sizeof (BCHEAPBLOCK);

            memcpy(pu8Dst, pvData, cbData);
            pu8Dst += cbData;

            memcpy(pu8Dst, pBitsHdr, sizeof (VRDEDATABITS));
            pu8Dst += sizeof (VRDEDATABITS);

            memcpy(pu8Dst, pu8Bits, pBitsHdr->cb);

            uint32_t u32Offset = (uint32_t)((uintptr_t)pu8 - (uintptr_t)pbc->pu8BitmapHeap);

            RTListAppend(&pbc->ListHeapBlocks, &pBlock->NodeHeapBlock);

            BCHEAPHANDLEINTERNAL *ph = (BCHEAPHANDLEINTERNAL *)pHandle;
            ph->u32Offset = u32Offset;
            ph->u32ScreenAccessKey = u32ScreenAccessKey;
            ph->u32ScreenId = u32ScreenId;
            ph->u32Reserved = 0;
            BCLOG(("BCLIMIT: allocated handle = %d/%d/0x%x\n", ph->u32ScreenId, ph->u32ScreenAccessKey, ph->u32Offset));
        }
        else
        {
            BCLOG(("BCLIMIT: alloc failed %d/%d\n", cbAlloc, RTHeapSimpleGetFreeSize(pbc->hHeap)));
            rc = VERR_NO_MEMORY;
        }

        bcHeapUnlock(pbc);
    }

    return rc;
}


void *BCBitmapHeapBlockQuery(PBMPCACHE pbc, const BCHEAPHANDLE *pHandle, int32_t i32Op, uint32_t *pcbBlock)
{
    if (!pbc)
    {
        return NULL;
    }

    const BCHEAPHANDLEINTERNAL *ph = (BCHEAPHANDLEINTERNAL *)pHandle;
    BCLOG(("handle = %d/%d/0x%x\n", ph->u32ScreenId, ph->u32ScreenAccessKey, ph->u32Offset));

    void *pvData = NULL;

    int rc = bcHeapLock(pbc);

    if (RT_SUCCESS(rc))
    {
        uint8_t *pu8BitmapBlock = bcResolveBitmapHandle(pbc, ph);

        if (pu8BitmapBlock)
        {
            BCHEAPBLOCK *pBlock = (BCHEAPBLOCK *)pu8BitmapBlock;

            if (i32Op == pBlock->i32Op)
            {
                Assert(0 <= pBlock->i32Keep && pBlock->i32Keep < 8);
                pBlock->i32Keep++;

                BCLOG(("i32Keep %d\n", pBlock->i32Keep));
                pvData = pu8BitmapBlock + sizeof (BCHEAPBLOCK);
                if (pcbBlock)
                {
                    *pcbBlock = pBlock->cb;
                }
            }
            else
            {
                AssertMsgFailed(("wrong op requested %d, block is %d\n", i32Op, pBlock->i32Op));
            }
        }
        else
        {
            AssertFailed();
        }

        bcHeapUnlock(pbc);
    }

    return pvData;
}

void BCBitmapHeapBlockRelease(PBMPCACHE pbc, const BCHEAPHANDLE *pHandle)
{
    if (!pbc)
    {
        return;
    }

    const BCHEAPHANDLEINTERNAL *ph = (BCHEAPHANDLEINTERNAL *)pHandle;
    BCLOG(("handle = %d/%d/0x%x\n", ph->u32ScreenId, ph->u32ScreenAccessKey, ph->u32Offset));

    int rc = bcHeapLock(pbc);

    if (RT_SUCCESS(rc))
    {
        uint8_t *pu8BitmapBlock = bcResolveBitmapHandle(pbc, ph);

        if (pu8BitmapBlock)
        {
            BCHEAPBLOCK *pBlock = (BCHEAPBLOCK *)pu8BitmapBlock;

            pBlock->i32Keep--;
            BCLOG(("i32Keep %d\n", pBlock->i32Keep));
            Assert(0 <= pBlock->i32Keep && pBlock->i32Keep < 8);
        }
        else
        {
            AssertFailed();
        }

        bcHeapUnlock(pbc);
    }
}

void BCBitmapHeapBlockFree(PBMPCACHE pbc, const BCHEAPHANDLE *pHandle)
{
    if (!pbc)
    {
        return;
    }

    const BCHEAPHANDLEINTERNAL *ph = (BCHEAPHANDLEINTERNAL *)pHandle;
    BCLOG(("handle = %d/%d/0x%x\n", ph->u32ScreenId, ph->u32ScreenAccessKey, ph->u32Offset));

    int rc = bcHeapLock(pbc);

    if (RT_SUCCESS(rc))
    {
        uint8_t *pu8BitmapBlock = bcResolveBitmapHandle(pbc, ph);

        if (pu8BitmapBlock)
        {
            BCHEAPBLOCK *pBlock = (BCHEAPBLOCK *)pu8BitmapBlock;

            BCLOG(("i32Keep %d\n", pBlock->i32Keep));
            if (pBlock->i32Keep <= 0)
            {
                RTListNodeRemove(&pBlock->NodeHeapBlock);
                RTHeapSimpleFree(pbc->hHeap, pu8BitmapBlock);
            }
        }
        else
        {
            AssertFailed();
        }

        bcHeapUnlock(pbc);
    }
}

/*
 * Allocate and initialize a new cache.
 */
int BCCreate (PBMPCACHE *ppbc, size_t cbCache)
{
    if (cbCache > 256 * _1M)
    {
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;

    BMPCACHE *pbc = (BMPCACHE *)VRDPMemAllocZ (sizeof (BMPCACHE));

    if (pbc)
    {
        rc = VRDPLock::Create("BMPCache", &pbc->pBitmapHeapLock);

        if (RT_SUCCESS(rc))
        {
            RTListInit(&pbc->ListHeapBlocks);

            if (cbCache)
            {
                /* Half memory will be used for intermediate heap.
                 * Other half for compressed bitmap data.
                 */
                pbc->cbBitmapsLimit = (uint32_t)cbCache / 2;

                size_t const cbPage = RTSystemGetPageSize();
                size_t cbHeap = RT_ALIGN_Z(pbc->cbBitmapsLimit, cbPage);

                pbc->pu8BitmapHeap = (uint8_t *)VRDPMemAlloc(cbHeap);

                if (pbc->pu8BitmapHeap)
                {
                    pbc->cbBitmapHeap = (uint32_t)cbHeap;

                    rc = RTHeapSimpleInit (&pbc->hHeap, pbc->pu8BitmapHeap, pbc->cbBitmapHeap);
                }
                else
                {
                    rc = VERR_NO_MEMORY;
                }
            }
            else
            {
                pbc->hHeap = NIL_RTHEAPSIMPLE;
            }
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *ppbc = pbc;
    }
    else
    {
        BCDelete(pbc);
    }

    return rc;
}

/*
 * Deallocate cache.
 */
void BCDelete (PBMPCACHE pbc)
{
    if (pbc)
    {
        /* Delete all entries. */
        while (pbc->pHeadMRU)
        {
            pbc->cbBitmaps -= pbc->pHeadMRU->cbBitmapCompressed;
            bcDeleteEntry (pbc, pbc->pHeadMRU);
        }

        if (pbc->pu8BitmapHeap)
        {
            VRDPMemFree(pbc->pu8BitmapHeap);
            pbc->pu8BitmapHeap = NULL;
        }

        VRDPLock::Delete(&pbc->pBitmapHeapLock);

        VRDPMemFree (pbc);
    }
}
