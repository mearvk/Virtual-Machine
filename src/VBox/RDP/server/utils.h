/* $Id: utils.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_utils_h
#define VRDP_INCLUDED_SRC_utils_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpdefs.h"

#ifdef LOG_ENABLED
void hexdump(unsigned char *p, unsigned int len);
#else
# define hexdump(a,b) do { } while (0)
#endif

void hexdumprel(unsigned char *p, unsigned int len);

#define BYTESPERPIXEL(_bpp) (((_bpp) + 7) / 8)

uint32_t getpixel (const uint8_t *pu8Src, uint32_t x, uint32_t bpp);

uint32_t getpixel0 (const uint8_t *pu8Src, uint32_t x);
uint32_t getpixel8 (const uint8_t *pu8Src, uint32_t x);
uint32_t getpixel16 (const uint8_t *pu8Src, uint32_t x);
uint32_t getpixel24 (const uint8_t *pu8Src, uint32_t x);
uint32_t getpixel32 (const uint8_t *pu8Src, uint32_t x);

typedef uint32_t FNGETPIXEL (const uint8_t *pu8Src, uint32_t x);
typedef FNGETPIXEL *PFNGETPIXEL;

inline PFNGETPIXEL GetPixelFn (unsigned cbPixel)
{
    switch (cbPixel)
    {
        case 4: return getpixel32;
        case 3: return getpixel24;
        case 2: return getpixel16;
        case 1: return getpixel8;
        default: AssertFailed (); return getpixel0;
    }
}

uint8_t * storepixel (uint8_t *pu8Dst, uint32_t pixel, uint32_t bpp);

uint8_t * storepixel0 (uint8_t *pu8Dst, uint32_t pixel);
uint8_t * storepixel8 (uint8_t *pu8Dst, uint32_t pixel);
uint8_t * storepixel16 (uint8_t *pu8Dst, uint32_t pixel);
uint8_t * storepixel24 (uint8_t *pu8Dst, uint32_t pixel);
uint8_t * storepixel32 (uint8_t *pu8Dst, uint32_t pixel);

typedef uint8_t * FNSTOREPIXEL (uint8_t *pu8Dst, uint32_t pixel);
typedef FNSTOREPIXEL *PFNSTOREPIXEL;

inline PFNSTOREPIXEL StorePixelFn (unsigned cbPixel)
{
    switch (cbPixel)
    {
        case 4: return storepixel32;
        case 3: return storepixel24;
        case 2: return storepixel16;
        case 1: return storepixel8;
        default: AssertFailed (); return storepixel0;
    }
}

extern uint8_t colorPalette256[256 * 3];

void ConvertColors (const uint8_t *src, unsigned bpp, unsigned width, uint8_t *dst, unsigned dstbpp, unsigned dstSize);

/* Memory leak detection. */
// #define MEMLEAK

#ifdef MEMLEAK
void *VRDPMemAllocDbg (size_t cb, bool fTmp, bool fZero, const char *pszCaller, int iLine);
void *VRDPMemReallocDbg (void *pv, size_t cb, const char *pszCaller, int iLine);
void VRDPMemFreeDbg (void *pv, bool fTmp);

void VRDPMemInit (void);
void VRDPMemUninit (void);

void VRDPMemDump (void);

#define VRDPMemAlloc(__cb)          VRDPMemAllocDbg   (__cb, false /* fTmp */, false /* fZero */, __FILE__,  __LINE__)
#define VRDPMemAllocZ(__cb)         VRDPMemAllocDbg   (__cb, false /* fTmp */, true /* fZero */,  __FILE__,  __LINE__)
#define VRDPMemTmpAlloc(__cb)       VRDPMemAllocDbg   (__cb, true /* fTmp */,  false /* fZero */, __FILE__,  __LINE__)
#define VRDPMemTmpAllocZ(__cb)      VRDPMemAllocDbg   (__cb, true /* fTmp */,  true /* fZero */,  __FILE__,  __LINE__)
#define VRDPMemRealloc(__pv, __cb)  VRDPMemReallocDbg (__pv, __cb, __FILE__,  __LINE__)
#define VRDPMemFree(__pv)           VRDPMemFreeDbg    (__pv, false /* fTmp */)
#define VRDPMemTmpFree(__pv)        VRDPMemFreeDbg    (__pv, true /* fTmp */)
#else
#define VRDPMemInit() do {} while (0)
#define VRDPMemUninit() do {} while (0)

#define VRDPMemDump() do {} while (0)

#define VRDPMemAlloc     RTMemAlloc
#define VRDPMemAllocZ    RTMemAllocZ
#define VRDPMemTmpAlloc  RTMemTmpAlloc
#define VRDPMemTmpAllocZ RTMemTmpAllocZ
#define VRDPMemRealloc   RTMemRealloc
#define VRDPMemFree      RTMemFree
#define VRDPMemTmpFree   RTMemTmpFree
#endif /* MEMLEAK */

class VRDPLock
{
    private:
        RTCRITSECT m_critsect;

        VRDPLock();
        ~VRDPLock();

    public:

        static int  Create(const char *pszLockName, VRDPLock **ppLock);
        static void Delete(VRDPLock **ppLock);
        static bool IsInitialized(VRDPLock *pLock);
        static bool IsOwner(VRDPLock *pLock);
        static int  Lock(VRDPLock *pLock);
        static void Unlock(VRDPLock *pLock);
};


/*
 * Port list string enumeration.
 */
typedef struct PORTENUMCONTEXT
{
    const char *pszNextRange;
    bool fMainPortRange;
    bool fMainPortRangeReported;
    uint16_t u16MainPort;
    uint16_t u16LastPort;
    uint16_t u16RangeStart;
    uint16_t u16RangeEnd;
    bool fDecrement;
} PORTENUMCONTEXT;

void portEnumStart(PORTENUMCONTEXT *pCtx, const char *pszPortRange, uint16_t u16MainPort);
uint16_t portEnumNext(PORTENUMCONTEXT *pCtx);


/*
 * Reference counted structures.
 *
 * VRDPPKT must be the first field of a structure.
 */
typedef struct VRDPPKT VRDPPKT;

typedef void FNVRDPPKTDELETE(VRDPPKT *pPkt);
typedef FNVRDPPKTDELETE *PFNVRDPPKTDELETE;

#pragma pack(1)
struct VRDPPKT
{
    int32_t volatile cRefs;
    uint32_t pktType;
    uint32_t pktSize;
    uint32_t u32Align1;
    PFNVRDPPKTDELETE pfnPktDelete; /* Deallocation routine, called when cRefs goes to 0. */
    void *pvAlloc;                 /* Memory to be deallocated, can be != &cRefs if the structure is aligned. */
#if ARCH_BITS == 32
    uint64_t u64Align2;
#endif
    /* pktSize - sizeof(VRDPPKT) bytes follow. */
};
#pragma pack()
AssertCompileSize(VRDPPKT, 32);

DECLINLINE(void) VRDPPktInit(VRDPPKT *pPkt,
                             uint32_t pktType,
                             uint32_t pktSize,
                             PFNVRDPPKTDELETE pfn,
                             void *pvAlloc)
{
    pPkt->cRefs = 0;
    pPkt->pktType = pktType;
    pPkt->pktSize = pktSize;
    pPkt->u32Align1 = 0;
    pPkt->pfnPktDelete = pfn;
    pPkt->pvAlloc = pvAlloc? pvAlloc: pPkt;
}

DECLINLINE(VRDPPKT *) VRDPPktAlloc(uint32_t pktType,
                                   uint32_t pktSize,
                                   PFNVRDPPKTDELETE pfn,
                                   uint32_t uAlign)
{
    /* Allocate aligned packet. 'uAlign' must be a power of 2.
     * The memory buffer must be large enough to include an aligned address for packet
     * That is extra (uAlign - 1) bytes must be allocated.
     */
    uint32_t cbAlloc = (uAlign - 1) + pktSize;
    void *pvAlloc = VRDPMemAllocZ(cbAlloc);
    if (!pvAlloc)
    {
        return NULL;
    }

    VRDPPKT *pPkt = RT_ALIGN_PT(pvAlloc, uAlign, VRDPPKT *);
#ifdef DEBUG_sunlover
    LogRel(("VRDPPKT %p => %p\n", pvAlloc, pPkt));
#endif
    VRDPPktInit(pPkt, pktType, pktSize, pfn, pvAlloc);
    return pPkt;
}

DECLINLINE(void *) VRDPPktDataPtr(VRDPPKT *pPkt)
{
    return (uint8_t *)pPkt + sizeof(VRDPPKT);
}

DECLINLINE(uint32_t) VRDPPktDataSize(VRDPPKT *pPkt)
{
    return pPkt->pktSize - sizeof(VRDPPKT);
}

DECLINLINE(int32_t) VRDPPktAddRef(VRDPPKT *pPkt)
{
    int c = ASMAtomicIncS32(&pPkt->cRefs);
    return c;
}

DECLINLINE(void) VRDPPktRelease(VRDPPKT *pPkt)
{
    int32_t c = ASMAtomicDecS32(&pPkt->cRefs);
    Assert(c >= 0);
    if (c == 0)
    {
#ifdef DEBUG_sunlover
        LogRel(("VRDPPKT %p deleted\n", pPkt));
#endif
        if (pPkt->pfnPktDelete)
        {
            pPkt->pfnPktDelete(pPkt);
        }
        VRDPMemFree(pPkt->pvAlloc);
    }
}

uint64_t VRDPTimeMilliTS(void);
uint64_t VRDPTimeNanoTS(void);
void VRDPQueryCurrentTimeMilli(volatile int64_t *pi64);
void VRDPThreadSleep(uint32_t cMillies);
bool VRDPEnvExist(const char *pszVar);

#endif /* !VRDP_INCLUDED_SRC_utils_h */
