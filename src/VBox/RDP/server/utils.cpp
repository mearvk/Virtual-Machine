/* $Id: utils.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "vrdpdefs.h"

#include "utils.h"

#include <iprt/crc.h>
#include <iprt/env.h>
#include <iprt/time.h>

#ifdef LOG_ENABLED
void hexdump(unsigned char *p, unsigned int len)
{
    VRDPLOG(("\n%.*Rhxd\n", len, p));
}
#endif /* LOG_ENABLED */

void hexdumprel(unsigned char *p, unsigned int len)
{
    VRDPLOGREL(("\n%.*Rhxd\n", len, p));
}

#ifdef MEMLEAK

typedef struct VRDPMemCaller
{
    struct VRDPMemCaller *next;
    const char *pszCaller;
    int iLine;
    uint32_t u32Hash;
    int64_t cAllocs;
    int64_t cFrees;
    int64_t cbAllocMinSize;
    int64_t cbAllocMaxSize;
    int64_t cbAllocCurrent;
    int64_t cbAllocPeak;
} VRDPMemCaller;

typedef struct _VRDPMemBlock
{
    unsigned uSignature;
    struct _VRDPMemBlock *next;
    struct _VRDPMemBlock *prev;
    const char *pszCaller;
    int iLine;
    bool fTmp;
    uint32_t u32Status;
    size_t size;
    void *pv;
} VRDPMemBlock;

static VRDPMemBlock *gMemBlockListHead;
static RTCRITSECT g_critsect;
static uint64_t gcbAllocatedMax;
static uint64_t gcbAllocated;
static VRDPMemCaller *gMemCallerListHead;

#define VRDP_BLOCK_TO_PTR(__p) ((__p)? (void *) ( (uint8_t *)(__p) + sizeof (VRDPMemBlock) ) : NULL)
#define VRDP_PTR_TO_BLOCK(__p) ((__p)? (VRDPMemBlock *)((uint8_t *)(__p) - sizeof (VRDPMemBlock)): NULL)

#define VRDP_BLOCK_SIGNATURE 0xFEDCBA98

static void vrdpMemLock (void)
{
    int rc = RTCritSectEnter (&g_critsect);
    AssertRC(rc);
}

static void vrdpMemUnlock (void)
{
    RTCritSectLeave (&g_critsect);
}

static void vrdpMemCaller(VRDPMemBlock *pBlock, bool fAlloc)
{
    uint32_t u32Hash = RTCrc32Start();
    u32Hash = RTCrc32Process(u32Hash, pBlock->pszCaller, strlen (pBlock->pszCaller));
    u32Hash = RTCrc32Process(u32Hash, &pBlock->iLine, sizeof (pBlock->iLine));
    u32Hash = RTCrc32Finish(u32Hash);

    /* Search caller. */
    VRDPMemCaller *pCaller = gMemCallerListHead;

    while (pCaller)
    {
        if (   pCaller->u32Hash == u32Hash
            && pCaller->iLine == pBlock->iLine
            && RTStrCmp(pCaller->pszCaller, pBlock->pszCaller) == 0
           )
        {
            break;
        }
        pCaller = pCaller->next;
    }

    if (!pCaller)
    {
        pCaller = (VRDPMemCaller *)RTMemAlloc(sizeof (VRDPMemCaller));
        if (!pCaller)
        {
            AssertFailed();
            return;
        }

        pCaller->next = gMemCallerListHead;
        pCaller->pszCaller = pBlock->pszCaller;
        pCaller->iLine = pBlock->iLine;
        pCaller->u32Hash = u32Hash;
        pCaller->cAllocs = 0;
        pCaller->cFrees = 0;
        pCaller->cbAllocMinSize = 1024 * _1M;
        pCaller->cbAllocMaxSize = 0;
        pCaller->cbAllocCurrent = 0;
        pCaller->cbAllocPeak = 0;

        gMemCallerListHead = pCaller;
    }

    if (fAlloc)
    {
        pCaller->cAllocs++;

        if (pCaller->cbAllocMinSize > (int64_t)pBlock->size)
        {
            pCaller->cbAllocMinSize = pBlock->size;
        }

        if (pCaller->cbAllocMaxSize < (int64_t)pBlock->size)
        {
            pCaller->cbAllocMaxSize = pBlock->size;
        }

        pCaller->cbAllocCurrent += pBlock->size;
        if (pCaller->cbAllocPeak < pCaller->cbAllocCurrent)
        {
            pCaller->cbAllocPeak = pCaller->cbAllocCurrent;
        }
    }
    else
    {
        pCaller->cFrees++;
        pCaller->cbAllocCurrent -= pBlock->size;
    }
}

static void vrdpMemAppendBlock (VRDPMemBlock *pBlock)
{
    pBlock->next = gMemBlockListHead;
    pBlock->prev = NULL;

    if (gMemBlockListHead)
    {
        gMemBlockListHead->prev = pBlock;
    }
    gMemBlockListHead = pBlock;
}

static void vrdpMemExcludeBlock (VRDPMemBlock *pBlock)
{
    /* Assert that the block is in the list. */
    VRDPMemBlock *pIter = gMemBlockListHead;

    while (pIter && pIter != pBlock)
    {
        pIter = pIter->next;
    }

    Assert (pIter == pBlock);

    /* Exclude the block from list. */
    if (pBlock->next)
    {
        pBlock->next->prev = pBlock->prev;
    }
    else
    {
        /* do nothing */
    }

    if (pBlock->prev)
    {
        pBlock->prev->next = pBlock->next;
    }
    else
    {
        gMemBlockListHead = pBlock->next;
    }

    pBlock->next = NULL;
    pBlock->prev = NULL;
}

void *VRDPMemAllocDbg (size_t cb, bool fTmp, bool fZero, const char *pszCaller, int iLine)
{
    // LogFlowFunc(("cb = %d, fTmp = %d, fZero = %d, pszCaller = %s, iLine = %d\n",
    //              cb, fTmp, fZero, pszCaller, iLine));

    vrdpMemLock ();

    size_t cbAlloc = cb + sizeof (VRDPMemBlock);

    VRDPMemBlock *pBlock = (VRDPMemBlock *)RTMemAlloc (cbAlloc);

    if (pBlock)
    {
        if (fZero)
        {
            memset (pBlock, 0, cbAlloc);
        }

        pBlock->pszCaller  = pszCaller;
        pBlock->iLine      = iLine;
        pBlock->size       = cb;
        pBlock->fTmp       = fTmp;
        pBlock->uSignature = VRDP_BLOCK_SIGNATURE;
        pBlock->pv         = VRDP_BLOCK_TO_PTR(pBlock);

        vrdpMemAppendBlock (pBlock);
        gcbAllocated += pBlock->size;
        if (gcbAllocatedMax < gcbAllocated)
        {
            gcbAllocatedMax = gcbAllocated;
        }
        vrdpMemCaller(pBlock, true);
    }

    vrdpMemUnlock ();

    return VRDP_BLOCK_TO_PTR(pBlock);
}

void *VRDPMemReallocDbg (void *pv, size_t cb, const char *pszCaller, int iLine)
{
    // LogFlowFunc(("pv = %p, cb = %d, pszCaller = %s, iLine = %d\n",
    //              pv, cb, pszCaller, iLine));

    vrdpMemLock ();

    VRDPMemBlock *pBlock = VRDP_PTR_TO_BLOCK(pv);

    if (pBlock)
    {
        size_t cbAlloc = cb + sizeof (VRDPMemBlock);

        Assert(pBlock->uSignature == VRDP_BLOCK_SIGNATURE);
        Assert(!pBlock->fTmp); /* Tmp blocks are not to be reallocated. */
        Assert(pBlock->pv == pv);

        vrdpMemExcludeBlock (pBlock);

        gcbAllocated -= pBlock->size;
        vrdpMemCaller(pBlock, false);

        pBlock = (VRDPMemBlock *)RTMemRealloc (pBlock, cbAlloc);

        pBlock->pszCaller = pszCaller;
        pBlock->iLine     = iLine;
        pBlock->size      = cb;

        vrdpMemAppendBlock (pBlock);

        gcbAllocated += pBlock->size;
        if (gcbAllocatedMax < gcbAllocated)
        {
            gcbAllocatedMax = gcbAllocated;
        }
        vrdpMemCaller(pBlock, true);

        pv = VRDP_BLOCK_TO_PTR(pBlock);

        pBlock->pv = pv;
    }
    else
    {
        pv = VRDPMemAllocDbg (cb, false /* fTmp */, false /* fZero */, pszCaller, iLine);
    }

    vrdpMemUnlock ();

    return pv;
}

void VRDPMemFreeDbg (void *pv, bool fTmp)
{
    NOREF(fTmp);
    // LogFlowFunc(("pv = %d, fTmp = %d\n",
    //              pv, fTmp));

    vrdpMemLock ();

    VRDPMemBlock *pBlock = VRDP_PTR_TO_BLOCK(pv);

    if (pBlock)
    {
        Assert(pBlock->uSignature == VRDP_BLOCK_SIGNATURE);
        Assert(pBlock->fTmp == fTmp);
        Assert(pBlock->pv == pv);

        vrdpMemExcludeBlock (pBlock);

        gcbAllocated -= pBlock->size;
        vrdpMemCaller(pBlock, false);

        /* Mark the block as freed. */
        pBlock->u32Status = 0xDEA110C;

        RTMemFree (pBlock);
    }

    vrdpMemUnlock ();
}

static int gcRefsMem = 0;

void VRDPMemInit (void)
{
    if (++gcRefsMem == 1)
    {
        int rc = RTCritSectInit (&g_critsect);
        AssertRC(rc);
        gMemBlockListHead = NULL;
    }
}

void VRDPMemUninit (void)
{
    VRDPMemDump();

    if (--gcRefsMem == 0)
    {
        gMemBlockListHead = NULL;

        VRDPMemCaller *pCaller = gMemCallerListHead;

        gMemCallerListHead = NULL;

        while (pCaller)
        {
            VRDPMemCaller *pNext = pCaller->next;

            RTMemFree(pCaller);

            pCaller = pNext;
        }

        if (RTCritSectIsInitialized (&g_critsect))
        {
            RTCritSectDelete (&g_critsect);
        }
    }
}

void VRDPMemDump (void)
{
    VRDPMemBlock *pBlock = gMemBlockListHead;

    int c = 0;
    size_t size = 0;
    while (pBlock)
    {
        LogRel(("VRDPMEM: %p 0x%8X bytes %d %s@%d\n", pBlock, pBlock->size, pBlock->fTmp, pBlock->pszCaller, pBlock->iLine));
        c++;
        size += pBlock->size;
        pBlock = pBlock->next;
    }

    LogRel(("VRDPMEM: Callers:\n"));
    VRDPMemCaller *pCaller = gMemCallerListHead;

    while (pCaller)
    {
        LogRel(("VRDPMEM: peak %8lldKB(%4lldKB), s %8lld/%8lldB, a/d %4lld/%4lld, %s@%d\n",
                pCaller->cbAllocPeak / 1024,
                pCaller->cbAllocCurrent / 1024,
                pCaller->cbAllocMinSize,
                pCaller->cbAllocMaxSize,
                pCaller->cAllocs,
                pCaller->cAllocs - pCaller->cFrees,
                pCaller->pszCaller,
                pCaller->iLine
              ));

        pCaller = pCaller->next;
    }

    LogRel(("VRDPMEM: %d blocks, total %dKb in use.\n", c, size / 1024));
    LogRel(("VRDPMEM: max alloc %lldKb.\n", gcbAllocatedMax / 1024));
}

#endif /* MEMLEAK */


VRDPLock::VRDPLock()
{
    RT_ZERO(m_critsect);
}

VRDPLock::~VRDPLock()
{
}

/* static */ int VRDPLock::Create(const char *pszLockName, VRDPLock **ppLock)
{
    int rc;

    VRDPLock *pLock = new VRDPLock();
    if (pLock)
    {
        RTLOCKVALCLASS hLockValClass;
#ifdef RT_LOCK_STRICT_ORDER
        hLockValClass = RTLockValidatorClassCreateUnique(RT_SRC_POS, "%s", pszLockName);
#else
        hLockValClass = NIL_RTLOCKVALCLASS;
        RT_NOREF(pszLockName);
#endif
        rc = RTCritSectInitEx(&pLock->m_critsect, 0, hLockValClass,
                              RTLOCKVAL_SUB_CLASS_NONE, "VRDPCritSect");
        if (RT_FAILURE(rc))
        {
            AssertRC(rc);
            delete pLock;
            pLock = NULL;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    *ppLock = pLock;
    return rc;
}

/* static */ void VRDPLock::Delete(VRDPLock **ppLock)
{
    AssertPtrReturnVoid(ppLock);
    AssertPtrReturnVoid(*ppLock);

    VRDPLock *pLock = *ppLock;
    *ppLock = NULL;

    if (RTCritSectIsInitialized(&pLock->m_critsect))
    {
        RTCritSectDelete(&pLock->m_critsect);
    }

    delete pLock;
}

/* static */ bool VRDPLock::IsInitialized(VRDPLock *pLock)
{
    AssertPtrReturn(pLock, false);

    return RTCritSectIsInitialized(&pLock->m_critsect);
}

/* static */ bool VRDPLock::IsOwner(VRDPLock *pLock)
{
    AssertPtrReturn(pLock, false);

    return RTCritSectIsOwner(&pLock->m_critsect);
}

/* static */ int VRDPLock::Lock(VRDPLock *pLock)
{
    AssertPtrReturn(pLock, VERR_INVALID_PARAMETER);

    return RTCritSectEnter(&pLock->m_critsect);
}

/* static */ void VRDPLock::Unlock(VRDPLock *pLock)
{
    AssertPtrReturnVoid(pLock);

    RTCritSectLeave(&pLock->m_critsect);
}

/*
 * Pixel read/write routines.
 */
uint32_t getpixel (const uint8_t *pu8Src, uint32_t x, uint32_t bpp)
{
    uint32_t u32Pixel;

    switch (bpp)
    {
        case 32:
        {
            u32Pixel = getpixel32 (pu8Src, x);
        } break;
        case 24:
        {
            u32Pixel = getpixel24 (pu8Src, x);
        } break;
        case 16:
        {
            u32Pixel = getpixel16 (pu8Src, x);
        } break;
        case 15:
        {
            u32Pixel = getpixel16 (pu8Src, x);
        } break;
        case 8:
        {
            u32Pixel = getpixel8 (pu8Src, x);
        } break;
        default:
        {
            u32Pixel = 0;
        }
    }

    return u32Pixel;
}

uint32_t getpixel0 (const uint8_t *pu8Src, uint32_t x)
{
    NOREF(pu8Src);
    NOREF(x);
    return 0;
}

uint32_t getpixel8 (const uint8_t *pu8Src, uint32_t x)
{
    return pu8Src[x];
}

uint32_t getpixel16 (const uint8_t *pu8Src, uint32_t x)
{
    return ((uint16_t *)pu8Src)[x];
}

uint32_t getpixel24 (const uint8_t *pu8Src, uint32_t x)
{
    pu8Src += x * 3;

    uint32_t u32Pixel = *(uint16_t *)pu8Src;

    pu8Src += 2;

    u32Pixel |= (*pu8Src) << 16;

    return u32Pixel;
}

uint32_t getpixel32 (const uint8_t *pu8Src, uint32_t x)
{
    return ((uint32_t *)pu8Src)[x];
}

uint8_t * storepixel (uint8_t *pu8Dst, uint32_t pixel, uint32_t bpp)
{
    switch (bpp)
    {
        case 32:
        {
            pu8Dst = storepixel32 (pu8Dst, pixel);
        } break;
        case 24:
        {
            pu8Dst = storepixel24 (pu8Dst, pixel);
        } break;
        case 16:
        {
            pu8Dst = storepixel16 (pu8Dst, pixel);
        } break;
        case 15:
        {
            pu8Dst = storepixel16 (pu8Dst, pixel);
        } break;
        case 8:
        {
            pu8Dst = storepixel8 (pu8Dst, pixel);
        } break;
    }

    return pu8Dst;
}

uint8_t * storepixel0 (uint8_t *pu8Dst, uint32_t pixel)
{
    NOREF(pixel);
    return pu8Dst;
}

uint8_t * storepixel8 (uint8_t *pu8Dst, uint32_t pixel)
{
    *pu8Dst = (uint8_t)pixel;
    return pu8Dst + 1;
}

uint8_t * storepixel16 (uint8_t *pu8Dst, uint32_t pixel)
{
    *(uint16_t *)pu8Dst = (uint16_t)pixel;
    return pu8Dst + 2;
}

uint8_t * storepixel24 (uint8_t *pu8Dst, uint32_t pixel)
{
    *(uint16_t *)pu8Dst = (uint16_t)pixel;
    pu8Dst += 2;

    *pu8Dst = (uint8_t)(pixel >> 16);
    return pu8Dst + 1;
}

uint8_t * storepixel32 (uint8_t *pu8Dst, uint32_t pixel)
{
    *(uint32_t *)pu8Dst = pixel;
    return pu8Dst + 4;
}


/*
 * RGB color depth convertion routines.
 */

/*
 * VRDP uses fixed 256 color palette.
 *
 * First 16 entries are mapped to "standard" 16 colors of VGA.
 * Remaining entries represent RGB values with 6 levels for G component
 * and 5 levels for R and B components:
 *
 * index(G, R, B) = 16 + 6 ^ 2 * (7 * G / 256) + 6 * (6 * R / 256) + (6 * B / 256), where 0 <= R, G, B <= 255.
 *
 * maximum index will be then 16 + 6 ^ 2 * 6 + 6 * 5 + 5 = 267 which is greater than allowed 255.
 * We have to exclude at least 12 colors from these indexes.
 *
 * 16 colors which can be mapped to standard ones are excluded from table.
 *
 * Index in the color palette is rgb2index[g * 36 + r * 6 + b],
 * where 0 <= g <= 6, 0 <= r,b <= 5.
 */

static uint8_t rgb2index[268 - 16] =
{
      0, // 16 (0, 0, 0)
     16, // 17 (0, 0, 1)
     17, // 18 (0, 0, 2)
      1, // 19 (0, 0, 3)
     18, // 20 (0, 0, 4)
      9, // 21 (0, 0, 5)
     19, // 22 (1, 0, 0)
     20, // 23 (1, 0, 1)
     21, // 24 (1, 0, 2)
     22, // 25 (1, 0, 3)
     23, // 26 (1, 0, 4)
     24, // 27 (1, 0, 5)
     25, // 28 (2, 0, 0)
     26, // 29 (2, 0, 1)
     27, // 30 (2, 0, 2)
     28, // 31 (2, 0, 3)
     29, // 32 (2, 0, 4)
     30, // 33 (2, 0, 5)
      4, // 34 (3, 0, 0)
     31, // 35 (3, 0, 1)
     32, // 36 (3, 0, 2)
      5, // 37 (3, 0, 3)
     33, // 38 (3, 0, 4)
     34, // 39 (3, 0, 5)
     35, // 40 (4, 0, 0)
     36, // 41 (4, 0, 1)
     37, // 42 (4, 0, 2)
     38, // 43 (4, 0, 3)
     39, // 44 (4, 0, 4)
     40, // 45 (4, 0, 5)
     12, // 46 (5, 0, 0)
     41, // 47 (5, 0, 1)
     42, // 48 (5, 0, 2)
     43, // 49 (5, 0, 3)
     44, // 50 (5, 0, 4)
     13, // 51 (5, 0, 5)
     45, // 52 (0, 1, 0)
     46, // 53 (0, 1, 1)
     47, // 54 (0, 1, 2)
     48, // 55 (0, 1, 3)
     49, // 56 (0, 1, 4)
     50, // 57 (0, 1, 5)
     51, // 58 (1, 1, 0)
     52, // 59 (1, 1, 1)
     53, // 60 (1, 1, 2)
     54, // 61 (1, 1, 3)
     55, // 62 (1, 1, 4)
     56, // 63 (1, 1, 5)
     57, // 64 (2, 1, 0)
     58, // 65 (2, 1, 1)
     59, // 66 (2, 1, 2)
     60, // 67 (2, 1, 3)
     61, // 68 (2, 1, 4)
     62, // 69 (2, 1, 5)
     63, // 70 (3, 1, 0)
     64, // 71 (3, 1, 1)
     65, // 72 (3, 1, 2)
     66, // 73 (3, 1, 3)
     67, // 74 (3, 1, 4)
     68, // 75 (3, 1, 5)
     69, // 76 (4, 1, 0)
     70, // 77 (4, 1, 1)
     71, // 78 (4, 1, 2)
     72, // 79 (4, 1, 3)
     73, // 80 (4, 1, 4)
     74, // 81 (4, 1, 5)
     75, // 82 (5, 1, 0)
     76, // 83 (5, 1, 1)
     77, // 84 (5, 1, 2)
     78, // 85 (5, 1, 3)
     79, // 86 (5, 1, 4)
     80, // 87 (5, 1, 5)
     81, // 88 (0, 2, 0)
     82, // 89 (0, 2, 1)
     83, // 90 (0, 2, 2)
     84, // 91 (0, 2, 3)
     85, // 92 (0, 2, 4)
     86, // 93 (0, 2, 5)
     87, // 94 (1, 2, 0)
     88, // 95 (1, 2, 1)
     89, // 96 (1, 2, 2)
     90, // 97 (1, 2, 3)
     91, // 98 (1, 2, 4)
     92, // 99 (1, 2, 5)
     93, // 100 (2, 2, 0)
     94, // 101 (2, 2, 1)
     95, // 102 (2, 2, 2)
     96, // 103 (2, 2, 3)
     97, // 104 (2, 2, 4)
     98, // 105 (2, 2, 5)
     99, // 106 (3, 2, 0)
    100, // 107 (3, 2, 1)
    101, // 108 (3, 2, 2)
    102, // 109 (3, 2, 3)
    103, // 110 (3, 2, 4)
    104, // 111 (3, 2, 5)
    105, // 112 (4, 2, 0)
    106, // 113 (4, 2, 1)
    107, // 114 (4, 2, 2)
    108, // 115 (4, 2, 3)
    109, // 116 (4, 2, 4)
    110, // 117 (4, 2, 5)
    111, // 118 (5, 2, 0)
    112, // 119 (5, 2, 1)
    113, // 120 (5, 2, 2)
    114, // 121 (5, 2, 3)
    115, // 122 (5, 2, 4)
    116, // 123 (5, 2, 5)
      2, // 124 (0, 3, 0)
    117, // 125 (0, 3, 1)
    118, // 126 (0, 3, 2)
      3, // 127 (0, 3, 3)
    119, // 128 (0, 3, 4)
    120, // 129 (0, 3, 5)
    121, // 130 (1, 3, 0)
    122, // 131 (1, 3, 1)
    123, // 132 (1, 3, 2)
    124, // 133 (1, 3, 3)
    125, // 134 (1, 3, 4)
    126, // 135 (1, 3, 5)
    127, // 136 (2, 3, 0)
    128, // 137 (2, 3, 1)
    129, // 138 (2, 3, 2)
    130, // 139 (2, 3, 3)
    131, // 140 (2, 3, 4)
    132, // 141 (2, 3, 5)
      6, // 142 (3, 3, 0)
    133, // 143 (3, 3, 1)
    134, // 144 (3, 3, 2)
      7, // 145 (3, 3, 3)
    135, // 146 (3, 3, 4)
    136, // 147 (3, 3, 5)
    137, // 148 (4, 3, 0)
    138, // 149 (4, 3, 1)
    139, // 150 (4, 3, 2)
    140, // 151 (4, 3, 3)
    141, // 152 (4, 3, 4)
    142, // 153 (4, 3, 5)
    143, // 154 (5, 3, 0)
    144, // 155 (5, 3, 1)
    145, // 156 (5, 3, 2)
    146, // 157 (5, 3, 3)
    147, // 158 (5, 3, 4)
    148, // 159 (5, 3, 5)
    149, // 160 (0, 4, 0)
    150, // 161 (0, 4, 1)
    151, // 162 (0, 4, 2)
    152, // 163 (0, 4, 3)
    153, // 164 (0, 4, 4)
    154, // 165 (0, 4, 5)
    155, // 166 (1, 4, 0)
    156, // 167 (1, 4, 1)
    157, // 168 (1, 4, 2)
    158, // 169 (1, 4, 3)
    159, // 170 (1, 4, 4)
    160, // 171 (1, 4, 5)
    161, // 172 (2, 4, 0)
    162, // 173 (2, 4, 1)
    163, // 174 (2, 4, 2)
    164, // 175 (2, 4, 3)
    165, // 176 (2, 4, 4)
    166, // 177 (2, 4, 5)
    167, // 178 (3, 4, 0)
    168, // 179 (3, 4, 1)
    169, // 180 (3, 4, 2)
    170, // 181 (3, 4, 3)
    171, // 182 (3, 4, 4)
    172, // 183 (3, 4, 5)
    173, // 184 (4, 4, 0)
    174, // 185 (4, 4, 1)
    175, // 186 (4, 4, 2)
    176, // 187 (4, 4, 3)
    177, // 188 (4, 4, 4)
    178, // 189 (4, 4, 5)
    179, // 190 (5, 4, 0)
    180, // 191 (5, 4, 1)
    181, // 192 (5, 4, 2)
    182, // 193 (5, 4, 3)
    183, // 194 (5, 4, 4)
    184, // 195 (5, 4, 5)
    185, // 196 (0, 5, 0)
    186, // 197 (0, 5, 1)
    187, // 198 (0, 5, 2)
    188, // 199 (0, 5, 3)
    189, // 200 (0, 5, 4)
    190, // 201 (0, 5, 5)
    191, // 202 (1, 5, 0)
    192, // 203 (1, 5, 1)
    193, // 204 (1, 5, 2)
    194, // 205 (1, 5, 3)
    195, // 206 (1, 5, 4)
    196, // 207 (1, 5, 5)
    197, // 208 (2, 5, 0)
    198, // 209 (2, 5, 1)
    199, // 210 (2, 5, 2)
    200, // 211 (2, 5, 3)
    201, // 212 (2, 5, 4)
    202, // 213 (2, 5, 5)
    203, // 214 (3, 5, 0)
    204, // 215 (3, 5, 1)
    205, // 216 (3, 5, 2)
    206, // 217 (3, 5, 3)
    207, // 218 (3, 5, 4)
    208, // 219 (3, 5, 5)
    209, // 220 (4, 5, 0)
    210, // 221 (4, 5, 1)
    211, // 222 (4, 5, 2)
    212, // 223 (4, 5, 3)
      8, // 224 (4, 5, 4)
    213, // 225 (4, 5, 5)
    214, // 226 (5, 5, 0)
    215, // 227 (5, 5, 1)
    216, // 228 (5, 5, 2)
    217, // 229 (5, 5, 3)
    218, // 230 (5, 5, 4)
    219, // 231 (5, 5, 5)
     10, // 232 (0, 6, 0)
    220, // 233 (0, 6, 1)
    221, // 234 (0, 6, 2)
    222, // 235 (0, 6, 3)
    223, // 236 (0, 6, 4)
     11, // 237 (0, 6, 5)
    224, // 238 (1, 6, 0)
    225, // 239 (1, 6, 1)
    226, // 240 (1, 6, 2)
    227, // 241 (1, 6, 3)
    228, // 242 (1, 6, 4)
    229, // 243 (1, 6, 5)
    230, // 244 (2, 6, 0)
    231, // 245 (2, 6, 1)
    232, // 246 (2, 6, 2)
    233, // 247 (2, 6, 3)
    234, // 248 (2, 6, 4)
    235, // 249 (2, 6, 5)
    236, // 250 (3, 6, 0)
    237, // 251 (3, 6, 1)
    238, // 252 (3, 6, 2)
    239, // 253 (3, 6, 3)
    240, // 254 (3, 6, 4)
    241, // 255 (3, 6, 5)
    242, // 256 (4, 6, 0)
    243, // 257 (4, 6, 1)
    244, // 258 (4, 6, 2)
    245, // 259 (4, 6, 3)
    246, // 260 (4, 6, 4)
    247, // 261 (4, 6, 5)
     14, // 262 (5, 6, 0)
    248, // 263 (5, 6, 1)
    249, // 264 (5, 6, 2)
    250, // 265 (5, 6, 3)
    251, // 266 (5, 6, 4)
     15, // 267 (5, 6, 5)
};

/* 256 color palette */
uint8_t colorPalette256[256 * 3] =
{
    // R     G     B
    0x00, 0x00, 0x00, //
    0x00, 0x00, 0x80, //
    0x00, 0x80, 0x00, //
    0x00, 0x80, 0x80, //

    0x80, 0x00, 0x00, //
    0x80, 0x00, 0x80, //
    0x80, 0x80, 0x00, //
    0x80, 0x80, 0x80, //

    0xCC, 0xCC, 0xCC, //
    0x00, 0x00, 0xFF, //
    0x00, 0xFF, 0x00, //
    0x00, 0xFF, 0xFF, //

    0xFF, 0x00, 0x00, //
    0xFF, 0x00, 0xFF, //
    0xFF, 0xFF, 0x00, //
    0xFF, 0xFF, 0xFF, //

    0x00, 0x00, 0x33, // (0, 0, 1)
    0x00, 0x00, 0x66, // (0, 0, 2)
    0x00, 0x00, 0xCC, // (0, 0, 4)
    0x33, 0x00, 0x00, // (1, 0, 0)
    0x33, 0x00, 0x33, // (1, 0, 1)
    0x33, 0x00, 0x66, // (1, 0, 2)
    0x33, 0x00, 0x99, // (1, 0, 3)
    0x33, 0x00, 0xCC, // (1, 0, 4)
    0x33, 0x00, 0xFF, // (1, 0, 5)
    0x66, 0x00, 0x00, // (2, 0, 0)
    0x66, 0x00, 0x33, // (2, 0, 1)
    0x66, 0x00, 0x66, // (2, 0, 2)
    0x66, 0x00, 0x99, // (2, 0, 3)
    0x66, 0x00, 0xCC, // (2, 0, 4)
    0x66, 0x00, 0xFF, // (2, 0, 5)
    0x99, 0x00, 0x33, // (3, 0, 1)
    0x99, 0x00, 0x66, // (3, 0, 2)
    0x99, 0x00, 0xCC, // (3, 0, 4)
    0x99, 0x00, 0xFF, // (3, 0, 5)
    0xCC, 0x00, 0x00, // (4, 0, 0)
    0xCC, 0x00, 0x33, // (4, 0, 1)
    0xCC, 0x00, 0x66, // (4, 0, 2)
    0xCC, 0x00, 0x99, // (4, 0, 3)
    0xCC, 0x00, 0xCC, // (4, 0, 4)
    0xCC, 0x00, 0xFF, // (4, 0, 5)
    0xFF, 0x00, 0x33, // (5, 0, 1)
    0xFF, 0x00, 0x66, // (5, 0, 2)
    0xFF, 0x00, 0x99, // (5, 0, 3)
    0xFF, 0x00, 0xCC, // (5, 0, 4)
    0x00, 0x2A, 0x00, // (0, 1, 0)
    0x00, 0x2A, 0x33, // (0, 1, 1)
    0x00, 0x2A, 0x66, // (0, 1, 2)
    0x00, 0x2A, 0x99, // (0, 1, 3)
    0x00, 0x2A, 0xCC, // (0, 1, 4)
    0x00, 0x2A, 0xFF, // (0, 1, 5)
    0x33, 0x2A, 0x00, // (1, 1, 0)
    0x33, 0x2A, 0x33, // (1, 1, 1)
    0x33, 0x2A, 0x66, // (1, 1, 2)
    0x33, 0x2A, 0x99, // (1, 1, 3)
    0x33, 0x2A, 0xCC, // (1, 1, 4)
    0x33, 0x2A, 0xFF, // (1, 1, 5)
    0x66, 0x2A, 0x00, // (2, 1, 0)
    0x66, 0x2A, 0x33, // (2, 1, 1)
    0x66, 0x2A, 0x66, // (2, 1, 2)
    0x66, 0x2A, 0x99, // (2, 1, 3)
    0x66, 0x2A, 0xCC, // (2, 1, 4)
    0x66, 0x2A, 0xFF, // (2, 1, 5)
    0x99, 0x2A, 0x00, // (3, 1, 0)
    0x99, 0x2A, 0x33, // (3, 1, 1)
    0x99, 0x2A, 0x66, // (3, 1, 2)
    0x99, 0x2A, 0x99, // (3, 1, 3)
    0x99, 0x2A, 0xCC, // (3, 1, 4)
    0x99, 0x2A, 0xFF, // (3, 1, 5)
    0xCC, 0x2A, 0x00, // (4, 1, 0)
    0xCC, 0x2A, 0x33, // (4, 1, 1)
    0xCC, 0x2A, 0x66, // (4, 1, 2)
    0xCC, 0x2A, 0x99, // (4, 1, 3)
    0xCC, 0x2A, 0xCC, // (4, 1, 4)
    0xCC, 0x2A, 0xFF, // (4, 1, 5)
    0xFF, 0x2A, 0x00, // (5, 1, 0)
    0xFF, 0x2A, 0x33, // (5, 1, 1)
    0xFF, 0x2A, 0x66, // (5, 1, 2)
    0xFF, 0x2A, 0x99, // (5, 1, 3)
    0xFF, 0x2A, 0xCC, // (5, 1, 4)
    0xFF, 0x2A, 0xFF, // (5, 1, 5)
    0x00, 0x55, 0x00, // (0, 2, 0)
    0x00, 0x55, 0x33, // (0, 2, 1)
    0x00, 0x55, 0x66, // (0, 2, 2)
    0x00, 0x55, 0x99, // (0, 2, 3)
    0x00, 0x55, 0xCC, // (0, 2, 4)
    0x00, 0x55, 0xFF, // (0, 2, 5)
    0x33, 0x55, 0x00, // (1, 2, 0)
    0x33, 0x55, 0x33, // (1, 2, 1)
    0x33, 0x55, 0x66, // (1, 2, 2)
    0x33, 0x55, 0x99, // (1, 2, 3)
    0x33, 0x55, 0xCC, // (1, 2, 4)
    0x33, 0x55, 0xFF, // (1, 2, 5)
    0x66, 0x55, 0x00, // (2, 2, 0)
    0x66, 0x55, 0x33, // (2, 2, 1)
    0x66, 0x55, 0x66, // (2, 2, 2)
    0x66, 0x55, 0x99, // (2, 2, 3)
    0x66, 0x55, 0xCC, // (2, 2, 4)
    0x66, 0x55, 0xFF, // (2, 2, 5)
    0x99, 0x55, 0x00, // (3, 2, 0)
    0x99, 0x55, 0x33, // (3, 2, 1)
    0x99, 0x55, 0x66, // (3, 2, 2)
    0x99, 0x55, 0x99, // (3, 2, 3)
    0x99, 0x55, 0xCC, // (3, 2, 4)
    0x99, 0x55, 0xFF, // (3, 2, 5)
    0xCC, 0x55, 0x00, // (4, 2, 0)
    0xCC, 0x55, 0x33, // (4, 2, 1)
    0xCC, 0x55, 0x66, // (4, 2, 2)
    0xCC, 0x55, 0x99, // (4, 2, 3)
    0xCC, 0x55, 0xCC, // (4, 2, 4)
    0xCC, 0x55, 0xFF, // (4, 2, 5)
    0xFF, 0x55, 0x00, // (5, 2, 0)
    0xFF, 0x55, 0x33, // (5, 2, 1)
    0xFF, 0x55, 0x66, // (5, 2, 2)
    0xFF, 0x55, 0x99, // (5, 2, 3)
    0xFF, 0x55, 0xCC, // (5, 2, 4)
    0xFF, 0x55, 0xFF, // (5, 2, 5)
    0x00, 0x7F, 0x33, // (0, 3, 1)
    0x00, 0x7F, 0x66, // (0, 3, 2)
    0x00, 0x7F, 0xCC, // (0, 3, 4)
    0x00, 0x7F, 0xFF, // (0, 3, 5)
    0x33, 0x7F, 0x00, // (1, 3, 0)
    0x33, 0x7F, 0x33, // (1, 3, 1)
    0x33, 0x7F, 0x66, // (1, 3, 2)
    0x33, 0x7F, 0x99, // (1, 3, 3)
    0x33, 0x7F, 0xCC, // (1, 3, 4)
    0x33, 0x7F, 0xFF, // (1, 3, 5)
    0x66, 0x7F, 0x00, // (2, 3, 0)
    0x66, 0x7F, 0x33, // (2, 3, 1)
    0x66, 0x7F, 0x66, // (2, 3, 2)
    0x66, 0x7F, 0x99, // (2, 3, 3)
    0x66, 0x7F, 0xCC, // (2, 3, 4)
    0x66, 0x7F, 0xFF, // (2, 3, 5)
    0x99, 0x7F, 0x33, // (3, 3, 1)
    0x99, 0x7F, 0x66, // (3, 3, 2)
    0x99, 0x7F, 0xCC, // (3, 3, 4)
    0x99, 0x7F, 0xFF, // (3, 3, 5)
    0xCC, 0x7F, 0x00, // (4, 3, 0)
    0xCC, 0x7F, 0x33, // (4, 3, 1)
    0xCC, 0x7F, 0x66, // (4, 3, 2)
    0xCC, 0x7F, 0x99, // (4, 3, 3)
    0xCC, 0x7F, 0xCC, // (4, 3, 4)
    0xCC, 0x7F, 0xFF, // (4, 3, 5)
    0xFF, 0x7F, 0x00, // (5, 3, 0)
    0xFF, 0x7F, 0x33, // (5, 3, 1)
    0xFF, 0x7F, 0x66, // (5, 3, 2)
    0xFF, 0x7F, 0x99, // (5, 3, 3)
    0xFF, 0x7F, 0xCC, // (5, 3, 4)
    0xFF, 0x7F, 0xFF, // (5, 3, 5)
    0x00, 0xAA, 0x00, // (0, 4, 0)
    0x00, 0xAA, 0x33, // (0, 4, 1)
    0x00, 0xAA, 0x66, // (0, 4, 2)
    0x00, 0xAA, 0x99, // (0, 4, 3)
    0x00, 0xAA, 0xCC, // (0, 4, 4)
    0x00, 0xAA, 0xFF, // (0, 4, 5)
    0x33, 0xAA, 0x00, // (1, 4, 0)
    0x33, 0xAA, 0x33, // (1, 4, 1)
    0x33, 0xAA, 0x66, // (1, 4, 2)
    0x33, 0xAA, 0x99, // (1, 4, 3)
    0x33, 0xAA, 0xCC, // (1, 4, 4)
    0x33, 0xAA, 0xFF, // (1, 4, 5)
    0x66, 0xAA, 0x00, // (2, 4, 0)
    0x66, 0xAA, 0x33, // (2, 4, 1)
    0x66, 0xAA, 0x66, // (2, 4, 2)
    0x66, 0xAA, 0x99, // (2, 4, 3)
    0x66, 0xAA, 0xCC, // (2, 4, 4)
    0x66, 0xAA, 0xFF, // (2, 4, 5)
    0x99, 0xAA, 0x00, // (3, 4, 0)
    0x99, 0xAA, 0x33, // (3, 4, 1)
    0x99, 0xAA, 0x66, // (3, 4, 2)
    0x99, 0xAA, 0x99, // (3, 4, 3)
    0x99, 0xAA, 0xCC, // (3, 4, 4)
    0x99, 0xAA, 0xFF, // (3, 4, 5)
    0xCC, 0xAA, 0x00, // (4, 4, 0)
    0xCC, 0xAA, 0x33, // (4, 4, 1)
    0xCC, 0xAA, 0x66, // (4, 4, 2)
    0xCC, 0xAA, 0x99, // (4, 4, 3)
    0xCC, 0xAA, 0xCC, // (4, 4, 4)
    0xCC, 0xAA, 0xFF, // (4, 4, 5)
    0xFF, 0xAA, 0x00, // (5, 4, 0)
    0xFF, 0xAA, 0x33, // (5, 4, 1)
    0xFF, 0xAA, 0x66, // (5, 4, 2)
    0xFF, 0xAA, 0x99, // (5, 4, 3)
    0xFF, 0xAA, 0xCC, // (5, 4, 4)
    0xFF, 0xAA, 0xFF, // (5, 4, 5)
    0x00, 0xD4, 0x00, // (0, 5, 0)
    0x00, 0xD4, 0x33, // (0, 5, 1)
    0x00, 0xD4, 0x66, // (0, 5, 2)
    0x00, 0xD4, 0x99, // (0, 5, 3)
    0x00, 0xD4, 0xCC, // (0, 5, 4)
    0x00, 0xD4, 0xFF, // (0, 5, 5)
    0x33, 0xD4, 0x00, // (1, 5, 0)
    0x33, 0xD4, 0x33, // (1, 5, 1)
    0x33, 0xD4, 0x66, // (1, 5, 2)
    0x33, 0xD4, 0x99, // (1, 5, 3)
    0x33, 0xD4, 0xCC, // (1, 5, 4)
    0x33, 0xD4, 0xFF, // (1, 5, 5)
    0x66, 0xD4, 0x00, // (2, 5, 0)
    0x66, 0xD4, 0x33, // (2, 5, 1)
    0x66, 0xD4, 0x66, // (2, 5, 2)
    0x66, 0xD4, 0x99, // (2, 5, 3)
    0x66, 0xD4, 0xCC, // (2, 5, 4)
    0x66, 0xD4, 0xFF, // (2, 5, 5)
    0x99, 0xD4, 0x00, // (3, 5, 0)
    0x99, 0xD4, 0x33, // (3, 5, 1)
    0x99, 0xD4, 0x66, // (3, 5, 2)
    0x99, 0xD4, 0x99, // (3, 5, 3)
    0x99, 0xD4, 0xCC, // (3, 5, 4)
    0x99, 0xD4, 0xFF, // (3, 5, 5)
    0xCC, 0xD4, 0x00, // (4, 5, 0)
    0xCC, 0xD4, 0x33, // (4, 5, 1)
    0xCC, 0xD4, 0x66, // (4, 5, 2)
    0xCC, 0xD4, 0x99, // (4, 5, 3)
    0xCC, 0xD4, 0xFF, // (4, 5, 5)
    0xFF, 0xD4, 0x00, // (5, 5, 0)
    0xFF, 0xD4, 0x33, // (5, 5, 1)
    0xFF, 0xD4, 0x66, // (5, 5, 2)
    0xFF, 0xD4, 0x99, // (5, 5, 3)
    0xFF, 0xD4, 0xCC, // (5, 5, 4)
    0xFF, 0xD4, 0xFF, // (5, 5, 5)
    0x00, 0xFF, 0x33, // (0, 6, 1)
    0x00, 0xFF, 0x66, // (0, 6, 2)
    0x00, 0xFF, 0x99, // (0, 6, 3)
    0x00, 0xFF, 0xCC, // (0, 6, 4)
    0x33, 0xFF, 0x00, // (1, 6, 0)
    0x33, 0xFF, 0x33, // (1, 6, 1)
    0x33, 0xFF, 0x66, // (1, 6, 2)
    0x33, 0xFF, 0x99, // (1, 6, 3)
    0x33, 0xFF, 0xCC, // (1, 6, 4)
    0x33, 0xFF, 0xFF, // (1, 6, 5)
    0x66, 0xFF, 0x00, // (2, 6, 0)
    0x66, 0xFF, 0x33, // (2, 6, 1)
    0x66, 0xFF, 0x66, // (2, 6, 2)
    0x66, 0xFF, 0x99, // (2, 6, 3)
    0x66, 0xFF, 0xCC, // (2, 6, 4)
    0x66, 0xFF, 0xFF, // (2, 6, 5)
    0x99, 0xFF, 0x00, // (3, 6, 0)
    0x99, 0xFF, 0x33, // (3, 6, 1)
    0x99, 0xFF, 0x66, // (3, 6, 2)
    0x99, 0xFF, 0x99, // (3, 6, 3)
    0x99, 0xFF, 0xCC, // (3, 6, 4)
    0x99, 0xFF, 0xFF, // (3, 6, 5)
    0xCC, 0xFF, 0x00, // (4, 6, 0)
    0xCC, 0xFF, 0x33, // (4, 6, 1)
    0xCC, 0xFF, 0x66, // (4, 6, 2)
    0xCC, 0xFF, 0x99, // (4, 6, 3)
    0xCC, 0xFF, 0xCC, // (4, 6, 4)
    0xCC, 0xFF, 0xFF, // (4, 6, 5)
    0xFF, 0xFF, 0x33, // (5, 6, 1)
    0xFF, 0xFF, 0x66, // (5, 6, 2)
    0xFF, 0xFF, 0x99, // (5, 6, 3)
    0xFF, 0xFF, 0xCC, // (5, 6, 4)
    0x00, 0x00, 0x00, //
    0x00, 0x00, 0x00, //
    0x00, 0x00, 0x00, //
    0x00, 0x00, 0x00, //
};

void ConvertColors (const uint8_t *pu8Src,
                    unsigned cBitsPerPixelSrc,
                    unsigned cPixels,
                    uint8_t *pu8Dst,
                    unsigned cBitsPerPixelDst,
                    unsigned cbDst)
{
    unsigned x;
    unsigned pixel;
    unsigned pixelDst;

    unsigned r;
    unsigned g;
    unsigned b;

    /* Note: Important cases 32 -> 24, 32 -> 16, 16 -> 24 are further optimized. */

    switch (cBitsPerPixelSrc)
    {
        case 32:
        {
            switch (cBitsPerPixelDst)
            {
                case 32:
                {
                    memcpy (pu8Dst, pu8Src, cPixels * 32 / 8);
                } break;
                case 24:
                {
                    while (cPixels--)
                    {
                        pixel = *(uint32_t *)pu8Src;
                        pu8Src += 4;

                        pu8Dst = storepixel24 (pu8Dst, pixel);
                    }
                } break;
                case 16:
                {
                    while (cPixels--)
                    {
                        pixel = *(uint32_t *)pu8Src;
                        pu8Src += 4;

                        /* rrrrr ggg ggg bbbbb */
                        pixelDst  = (pixel >> 8) & 0xf800;
                        pixelDst += (pixel >> 5) & 0x07e0;
                        pixelDst += (pixel >> 3) & 0x001f;

                        pu8Dst = storepixel16 (pu8Dst, pixelDst);
                    }
                } break;
                case 15:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel32 (pu8Src, x);

                        r = (uint8_t)(pixel >> 16);
                        g = (uint8_t)(pixel >> 8);
                        b = (uint8_t)(pixel);

                        pixel = ((r >> 3) << 10) + ((g >> 3) << 5) + (b >> 3);

                        pu8Dst = storepixel16 (pu8Dst, pixel);
                    }
                } break;
                case 8:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel32 (pu8Src, x);

                        r = (uint8_t)(pixel >> 16);
                        g = (uint8_t)(pixel >> 8);
                        b = (uint8_t)(pixel);

                        r = (r * 6)/256;
                        g = (g * 7)/256;
                        b = (b * 6)/256;

                        pixel = rgb2index[g * 36 + r * 6 + b];

                        pu8Dst = storepixel8 (pu8Dst, pixel);
                    }
                } break;
                default:
                {
                    memset (pu8Dst, 0, cbDst);
                } break;
            }
        } break;
        case 24:
        {
            switch (cBitsPerPixelDst)
            {
                case 32:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel24 (pu8Src, x);
                        pu8Dst = storepixel32 (pu8Dst, pixel);
                    }
                } break;
                case 24:
                {
                    memcpy (pu8Dst, pu8Src, cPixels * 24 / 8);
                } break;
                case 16:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel24 (pu8Src, x);

                        r = (uint8_t)(pixel >> 16);
                        g = (uint8_t)(pixel >> 8);
                        b = (uint8_t)(pixel);

                        pixel = ((r >> 3) << 11) + ((g >> 2) << 5) + (b >> 3);

                        pu8Dst = storepixel16 (pu8Dst, pixel);
                    }
                } break;
                case 15:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel24 (pu8Src, x);

                        r = (uint8_t)(pixel >> 16);
                        g = (uint8_t)(pixel >> 8);
                        b = (uint8_t)(pixel);

                        pixel = ((r >> 3) << 10) + ((g >> 3) << 5) + (b >> 3);

                        pu8Dst = storepixel16 (pu8Dst, pixel);
                    }
                } break;
                case 8:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel24 (pu8Src, x);

                        r = (uint8_t)(pixel >> 16);
                        g = (uint8_t)(pixel >> 8);
                        b = (uint8_t)(pixel);

                        r = (r * 6)/256;
                        g = (g * 7)/256;
                        b = (b * 6)/256;

                        pixel = rgb2index[g * 36 + r * 6 + b];

                        pu8Dst = storepixel8 (pu8Dst, pixel);
                    }
                } break;
                default:
                {
                    memset (pu8Dst, 0, cbDst);
                } break;
            }
        } break;
        case 16:
        {
            switch (cBitsPerPixelDst)
            {
                case 32:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel16 (pu8Src, x);

                        r = (pixel >> 11) & 0x1F;
                        g = (pixel >> 5) & 0x3F;
                        b = (pixel) & 0x1F;

                        pixel = ((r << 3) << 16) + ((g << 2) << 8) + (b << 3);

                        pu8Dst = storepixel32 (pu8Dst, pixel);
                    }
                } break;
                case 24:
                {
                    while (cPixels--)
                    {
                        pixel = *(uint16_t *)pu8Src;
                        pu8Src += 2;

                        /* rrrrr ggg ggg bbbbb -> rrrrr000 gggggg00 bbbbb000 */
                        pixelDst  = (pixel << 8) & 0xf80000;
                        pixelDst += (pixel << 5) & 0x00fc00;
                        pixelDst += (pixel << 3) & 0x0000f8;

                        pu8Dst = storepixel24 (pu8Dst, pixelDst);
                    }
                } break;
                case 16:
                {
                    memcpy (pu8Dst, pu8Src, cPixels * 16 / 8);
                } break;
                case 15:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel16 (pu8Src, x);

                        r = (pixel >> 11) & 0x1F;
                        g = (pixel >> 5) & 0x3F;
                        b = (pixel) & 0x1F;

                        pixel = (r << 10) + ((g >> 1) << 5) + (b);

                        pu8Dst = storepixel16 (pu8Dst, pixel);
                    }
                } break;
                case 8:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel16 (pu8Src, x);

                        r = (pixel >> 11) & 0x1F;
                        g = (pixel >> 5) & 0x3F;
                        b = (pixel) & 0x1F;

                        r = (r * 6)/0x20;
                        g = (g * 7)/0x40;
                        b = (b * 6)/0x20;

                        pixel = rgb2index[g * 36 + r * 6 + b];

                        pu8Dst = storepixel8 (pu8Dst, pixel);
                    }
                } break;
                default:
                {
                    memset (pu8Dst, 0, cbDst);
                } break;
            }
        } break;
        case 15:
        {
            switch (cBitsPerPixelDst)
            {
                case 32:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel16 (pu8Src, x);

                        r = (pixel >> 10) & 0x1F;
                        g = (pixel >> 5) & 0x1F;
                        b = (pixel) & 0x1F;

                        pixel = ((r << 3) << 16) + ((g << 2) << 8) + (b << 3);

                        pu8Dst = storepixel32 (pu8Dst, pixel);
                    }
                } break;
                case 24:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel16 (pu8Src, x);

                        r = (pixel >> 10) & 0x1F;
                        g = (pixel >> 5) & 0x1F;
                        b = (pixel) & 0x1F;

                        pixel = ((r << 3) << 16) + ((g << 2) << 8) + (b << 3);

                        pu8Dst = storepixel24 (pu8Dst, pixel);
                    }
                } break;
                case 16:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel16 (pu8Src, x);

                        r = (pixel >> 10) & 0x1F;
                        g = (pixel >> 5) & 0x1F;
                        b = (pixel) & 0x1F;

                        pixel = (r << 11) + (g << 6) + (b);

                        pu8Dst = storepixel16 (pu8Dst, pixel);
                    }
                } break;
                case 15:
                {
                    memcpy (pu8Dst, pu8Src, cPixels * 16 / 8);
                } break;
                case 8:
                {
                    for (x = 0; x < cPixels; x++)
                    {
                        pixel = getpixel16 (pu8Src, x);

                        r = (pixel >> 10) & 0x1F;
                        g = (pixel >> 5) & 0x1F;
                        b = (pixel) & 0x1F;

                        r = (r * 6)/0x20;
                        g = (g * 7)/0x20;
                        b = (b * 6)/0x20;

                        pixel = rgb2index[g * 36 + r * 6 + b];

                        pu8Dst = storepixel8 (pu8Dst, pixel);
                    }
                } break;
                default:
                {
                    memset (pu8Dst, 0, cbDst);
                } break;
            }
        } break;
        default:
        {
            memset (pu8Dst, 0, cbDst);
        } break;
    }
}

/* A comma-separated list of ports or range of ports to bind to. A range of ports is specified
 * through a dash (-) between two port numbers.
 */
void portEnumStart(PORTENUMCONTEXT *pCtx, const char *pszPortRange, uint16_t u16MainPort)
{
    pCtx->pszNextRange = pszPortRange;

    pCtx->fMainPortRange = (pszPortRange == NULL || *pszPortRange == 0);
    pCtx->fMainPortRangeReported = false;
    pCtx->u16MainPort = u16MainPort;

    pCtx->u16LastPort = 0;

    pCtx->u16RangeStart = 0;
    pCtx->u16RangeEnd   = 0;

    pCtx->fDecrement = false;
}

static int portParseNumber(uint16_t *pu16Port, const char *pszStart, const char *pszEnd)
{
    /* Gets a string of digits, converts to 16 bit port number.
     * Note: pszStart <= pszEnd is expected, the string contains
     *       only digits and pszEnd points to the char after last
     *       digit.
     */
    int cch = pszEnd - pszStart;
    if (cch > 0 && cch <= 5) /* Port is up to 5 decimal digits. */
    {
        unsigned uPort = 0;
        while (pszStart != pszEnd)
        {
            uPort = uPort * 10 + *pszStart - '0';
            pszStart++;
        }

        if (uPort != 0 && uPort < 0x10000)
        {
            *pu16Port = (uint16_t)uPort;
            return VINF_SUCCESS;
        }
    }

    return VERR_INVALID_PARAMETER;
}

uint16_t portEnumNext(PORTENUMCONTEXT *pCtx)
{
    if (pCtx->u16LastPort == pCtx->u16RangeEnd)
    {
        /* Current range ends. */
        if (pCtx->fMainPortRange)
        {
            if (pCtx->fMainPortRangeReported == false)
            {
                /* Use the port from config once. */
                pCtx->fMainPortRangeReported = true;

                pCtx->u16RangeStart = pCtx->u16MainPort;
                pCtx->u16RangeEnd   = pCtx->u16MainPort;

                pCtx->u16LastPort = pCtx->u16RangeStart;
            }
            else
            {
                /* It was the single m_u16Port range. End of enumeration. */
                pCtx->u16LastPort = 0;
            }
        }
        else
        {
            /* Get next range. */
            if (pCtx->pszNextRange)
            {
                for (;;)
                {
                    if (*pCtx->pszNextRange == 0)
                    {
                        /* No more ports in the range. End of enumeration. */
                        pCtx->u16LastPort = 0;
                        break;
                    }

                    /* The range string has more data. */
                    const char *pszStart = pCtx->pszNextRange;
                    const char *pszDash = NULL;
                    const char *pszEnd = pszStart;

                    bool fRange = true; /* Think it is a valid range. */

                    while (*pszEnd && *pszEnd != ',')
                    {
                        if (*pszEnd == '-')
                        {
                            if (pszDash == NULL)
                            {
                                pszDash = pszEnd;
                            }
                            else
                            {
                                fRange = false; /* 2 or more dash chars. */
                            }
                        }
                        else if (*pszEnd < '0' || *pszEnd > '9')
                        {
                            fRange = false;
                        }
                        pszEnd++;
                    }

                    /* Update the next range pointer. */
                    pCtx->pszNextRange = pszEnd;
                    if (*pszEnd == ',')
                    {
                        pCtx->pszNextRange++;
                    }

                    if (!fRange)
                    {
                        continue;
                    }

                    /* A probably valid range. Verify and parse it. */
                    if (pszDash)
                    {
                        /* A range. */
                        uint16_t u16PortStart;
                        int rc = portParseNumber(&u16PortStart, pszStart, pszDash);
                        if (RT_SUCCESS(rc))
                        {
                            uint16_t u16PortEnd;
                            rc = portParseNumber(&u16PortEnd, pszDash + 1, pszEnd);
                            if (RT_SUCCESS(rc))
                            {
                                /* Got the range. */
                                pCtx->fDecrement = u16PortStart > u16PortEnd;

                                pCtx->u16RangeStart = u16PortStart;
                                pCtx->u16RangeEnd = u16PortEnd;

                                pCtx->u16LastPort = pCtx->u16RangeStart;

                                break;
                            }
                        }
                    }
                    else
                    {
                        /* Single port. */
                        uint16_t u16Port;
                        int rc = portParseNumber(&u16Port, pszStart, pszEnd);
                        if (RT_SUCCESS(rc))
                        {
                            /* Got the single port. */
                            pCtx->u16RangeStart = u16Port;
                            pCtx->u16RangeEnd = u16Port;

                            pCtx->u16LastPort = pCtx->u16RangeStart;
                            break;
                        }
                    }
                    /* Continue. */
                }
            }

        }
    }
    else
    {
        if (pCtx->fDecrement)
        {
            pCtx->u16LastPort--;
        }
        else
        {
            pCtx->u16LastPort++;
        }
    }

    return pCtx->u16LastPort;
}

/**
 * Gets the current millisecond timestamp.
 *
 * @returns millisecond timestamp.
 */
uint64_t VRDPTimeMilliTS(void)
{
    return RTTimeMilliTS();
}

/**
 * Gets the current nanosecond timestamp.
 *
 * @returns nanosecond timestamp.
 */
uint64_t VRDPTimeNanoTS(void)
{
    return RTTimeNanoTS();
}

/**
 * Gets the time as milliseconds relative to the unix epoch.
 *
 * @param   pi64   The pointer to the returned time in milliseconds.
 */
void VRDPQueryCurrentTimeMilli(volatile int64_t *pi64)
{
    RTTIMESPEC ts;
    RTTimeNow(&ts);
    ASMAtomicXchgS64(pi64, RTTimeSpecGetMilli(&ts));
}

/**
 * Millisecond granular sleep function.
 *
 * @param   cMillies    Number of milliseconds to sleep.
 */
void VRDPThreadSleep(uint32_t cMillies)
{
    RTThreadSleep((RTMSINTERVAL)cMillies);
}

/**
 * Checks if an environment variable exists in the default environment block.
 *
 * @returns Whether the variable exists.
 *
 * @param   pszVar      The environment variable name.
 */
bool VRDPEnvExist(const char *pszVar)
{
    return RTEnvExist(pszVar);
}
