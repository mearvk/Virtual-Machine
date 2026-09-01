/* $Id: tstPGM-1.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * PGM unit tests.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM_PHYS
#define VBOX_WITHOUT_PAGING_BIT_FIELDS /* 64-bit bitfields are just asking for trouble. See @bugref{9841} and others. */
#include <VBox/vmm/pgm.h>
#include "PGMInternal.h"
#include <VBox/vmm/vmcc.h>
#include "PGMInline.h"

#include <iprt/assert.h>

#include "tstVMMUnitTests-1.h"

/** GCC complains if we memcpy the array. */
static void CopyRamRangeLookupArray(PGMRAMRANGELOOKUPENTRY *paDst, PGMRAMRANGELOOKUPENTRY const *paSrc, uint32_t cToCopy)
{
    for (uint32_t i = 0; i < cToCopy; i++)
    {
        paDst[i].GCPhysFirstAndId = paSrc[i].GCPhysFirstAndId;
        paDst[i].GCPhysLast       = paSrc[i].GCPhysLast;
    }
}


typedef DECLCALLBACKTYPE(PPGMRAMRANGE, FNPGMGETRANGE,(PVMCC pVM, RTGCPHYS GCPhys));
template<FNPGMGETRANGE fnPgmGetRange, bool a_fAbove>
void testPhysGetRange(PVM pVM)
{
    /*
     * Walk aRamRangeLookup and check various guest phys addresses for each entry.
     */
    RTTESTI_CHECK(pVM->pgm.s.RamRangeUnion.cLookupEntries > 2);
    uint64_t const         u64SavedRangeRangeUnion = pVM->pgm.s.RamRangeUnion.u64Combined;
    uint32_t const         cSavedLookupEntries     = pVM->pgm.s.RamRangeUnion.cLookupEntries;
    PGMRAMRANGELOOKUPENTRY aSavedRamRangeLookup[PGM_MAX_RAM_RANGES];
    CopyRamRangeLookupArray(aSavedRamRangeLookup, pVM->pgm.s.aRamRangeLookup, cSavedLookupEntries);

    for (uint32_t iTableVar = 0; iTableVar < 4; iTableVar++)
    {
        RTGCPHYS GCPhysLastPrev = NIL_RTGCPHYS;
        for (uint32_t idx = 0, cRanges = pVM->pgm.s.RamRangeUnion.cLookupEntries; idx < cRanges; idx++)
        {
            RTGCPHYS const GCPhysFirst = PGMRAMRANGELOOKUPENTRY_GET_FIRST(pVM->pgm.s.aRamRangeLookup[idx]);
            RTGCPHYS const GCPhysLast  = pVM->pgm.s.aRamRangeLookup[idx].GCPhysLast;

            /* Direct hit at start. */
            PPGMRAMRANGE pRange = fnPgmGetRange(pVM, GCPhysFirst);
            RTTESTI_CHECK(pRange && pRange->GCPhys == GCPhysFirst);

            /* Direct hit at end. */
            pRange = fnPgmGetRange(pVM, GCPhysLast);
            RTTESTI_CHECK(pRange && pRange->GCPhys == GCPhysFirst);

            /* Direct hit inside. */
            pRange = fnPgmGetRange(pVM, GCPhysFirst + (GCPhysLast - GCPhysFirst) / 4 * 3);
            RTTESTI_CHECK(pRange && pRange->GCPhys == GCPhysFirst);

            /* If we got a gap between this range and the previous, check how that works out. */
            if (GCPhysFirst > GCPhysLastPrev + 1)
            {
                pRange = fnPgmGetRange(pVM, GCPhysFirst - 1);
                RTTESTI_CHECK(a_fAbove ? pRange && pRange->GCPhys == GCPhysFirst : !pRange);

                pRange = fnPgmGetRange(pVM, GCPhysLastPrev + 1);
                RTTESTI_CHECK(a_fAbove ? pRange && pRange->GCPhys == GCPhysFirst : !pRange);
            }

            GCPhysLastPrev = GCPhysLast;
        }

        if (GCPhysLastPrev != NIL_RTGCPHYS)
        {
            PPGMRAMRANGE pRange = fnPgmGetRange(pVM, GCPhysLastPrev + 1);
            RTTESTI_CHECK(!pRange);

            pRange = fnPgmGetRange(pVM, NIL_RTGCPHYS);
            RTTESTI_CHECK(!pRange);
        }

        if (iTableVar == 0)
        {
            /* Variation #1: Reduce the table to two entries, first and last. */
            pVM->pgm.s.aRamRangeLookup[0].GCPhysFirstAndId = aSavedRamRangeLookup[0].GCPhysFirstAndId;
            pVM->pgm.s.aRamRangeLookup[0].GCPhysLast       = aSavedRamRangeLookup[0].GCPhysLast;
            pVM->pgm.s.aRamRangeLookup[1].GCPhysFirstAndId = aSavedRamRangeLookup[cSavedLookupEntries - 1].GCPhysFirstAndId;
            pVM->pgm.s.aRamRangeLookup[1].GCPhysLast       = aSavedRamRangeLookup[cSavedLookupEntries - 1].GCPhysLast;
            pVM->pgm.s.RamRangeUnion.cLookupEntries = 2;
        }
        else
        {
            /* Introduce gaps in the table by removing every 2nd entry, ASSUMING there
               is at least one entry in the table. */
            RTTESTI_CHECK(pVM->pgm.s.RamRangeUnion.cLookupEntries > 1 || iTableVar > 1);
            if (pVM->pgm.s.RamRangeUnion.cLookupEntries <= 1)
                break;
            uint32_t idxDst = 1;
            for (uint32_t idxSrc = iTableVar + 1; idxSrc < cSavedLookupEntries; idxSrc += iTableVar + 1)
            {
                pVM->pgm.s.aRamRangeLookup[idxDst].GCPhysFirstAndId = aSavedRamRangeLookup[idxSrc].GCPhysFirstAndId;
                pVM->pgm.s.aRamRangeLookup[idxDst].GCPhysLast       = aSavedRamRangeLookup[idxSrc].GCPhysLast;
                idxDst++;
            }
            pVM->pgm.s.RamRangeUnion.cLookupEntries = idxDst;
        }
    }

    CopyRamRangeLookupArray(pVM->pgm.s.aRamRangeLookup, aSavedRamRangeLookup, cSavedLookupEntries);
    pVM->pgm.s.RamRangeUnion.u64Combined = u64SavedRangeRangeUnion;
}

#define CHECK_GET_PAGE_NOT_FOUND(fnPgmGetPage, pVM, a_GCPhys) do { \
        PPGMPAGE     pPageR; \
        PPGMRAMRANGE pRamR; \
        int rcResult = fnPgmGetPage(pVM, a_GCPhys, &pPageR, &pRamR); \
        if (rcResult != VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS) \
            RTTestIFailed("line %u/iTableVar=%u: GCPhys=%RGp -> %Rrc, expected VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS!", \
                          __LINE__, iTableVar, a_GCPhys, rcResult); \
        if (pPageR != NULL) \
            RTTestIFailed("line %u/iTableVar=%u: GCPhys=%RGp -> pPage=%p", __LINE__, iTableVar, a_GCPhys, pPageR); \
        if (pRamR != NULL) \
            RTTestIFailed("line %u/iTableVar=%u: GCPhys=%RGp -> pRam=%p %RGp LB %RGp", \
                          __LINE__, iTableVar, a_GCPhys, pRamR, pRamR->GCPhys, pRamR->cb); \
    } while (0)

typedef DECLCALLBACKTYPE(int, FNPGMGETPAGE,(PVMCC pVM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRam));
template<FNPGMGETPAGE fnPgmGetPage, bool const a_fInvalidRangeTlb = false>
void testPhysGetPage(PVM pVM)
{
    /*
     * Walk aRamRangeLookup and check various guest phys addresses for each entry.
     */
    RTTESTI_CHECK(pVM->pgm.s.RamRangeUnion.cLookupEntries > 2);
    uint64_t const         u64SavedRangeRangeUnion = pVM->pgm.s.RamRangeUnion.u64Combined;
    uint32_t const         cSavedLookupEntries     = pVM->pgm.s.RamRangeUnion.cLookupEntries;
    PGMRAMRANGELOOKUPENTRY aSavedRamRangeLookup[PGM_MAX_RAM_RANGES];
    CopyRamRangeLookupArray(aSavedRamRangeLookup, pVM->pgm.s.aRamRangeLookup, cSavedLookupEntries);

    for (uint32_t iTableVar = 0; iTableVar < 4; iTableVar++)
    {
        if (a_fInvalidRangeTlb)
            pgmPhysInvalidRamRangeTlbs(pVM); /* Necessary for pgmPhysGetPageAndRangeExSlowLockless only. */

        RTGCPHYS GCPhysLastPrev = NIL_RTGCPHYS;
        for (uint32_t idx = 0, cRanges = pVM->pgm.s.RamRangeUnion.cLookupEntries; idx < cRanges; idx++)
        {
            RTGCPHYS const GCPhysFirst = PGMRAMRANGELOOKUPENTRY_GET_FIRST(pVM->pgm.s.aRamRangeLookup[idx]);
            RTGCPHYS const GCPhysLast  = pVM->pgm.s.aRamRangeLookup[idx].GCPhysLast;

            /* Direct hit at start. */
            PPGMPAGE     pPage;
            PPGMRAMRANGE pRam;
            int rc = fnPgmGetPage(pVM, GCPhysFirst, &pPage, &pRam);
            RTTESTI_CHECK(rc == VINF_SUCCESS && pPage && pRam && pRam->GCPhys == GCPhysFirst);

            /* Direct hit at end. */
            rc = fnPgmGetPage(pVM, GCPhysLast, &pPage, &pRam);
            RTTESTI_CHECK(rc == VINF_SUCCESS && pPage && pRam && pRam->GCPhys == GCPhysFirst);

            /* Direct hit inside. */
            rc = fnPgmGetPage(pVM, GCPhysFirst + (GCPhysLast - GCPhysFirst) / 4 * 3, &pPage, &pRam);
            RTTESTI_CHECK(rc == VINF_SUCCESS && pPage && pRam && pRam->GCPhys == GCPhysFirst);

            /* If we got a gap between this range and the previous, check how that works out. */
            if (GCPhysFirst > GCPhysLastPrev + 1)
            {
                CHECK_GET_PAGE_NOT_FOUND(fnPgmGetPage, pVM, GCPhysFirst - 1);
                CHECK_GET_PAGE_NOT_FOUND(fnPgmGetPage, pVM, GCPhysLastPrev + 1);
            }

            GCPhysLastPrev = GCPhysLast;
        }

        if (GCPhysLastPrev != NIL_RTGCPHYS)
        {
            CHECK_GET_PAGE_NOT_FOUND(fnPgmGetPage, pVM, GCPhysLastPrev + 1);
            CHECK_GET_PAGE_NOT_FOUND(fnPgmGetPage, pVM, NIL_RTGCPHYS);
        }

        if (iTableVar == 0)
        {
            /* Variation #1: Reduce the table to two entries, first and last. */
            pVM->pgm.s.aRamRangeLookup[0].GCPhysFirstAndId = aSavedRamRangeLookup[0].GCPhysFirstAndId;
            pVM->pgm.s.aRamRangeLookup[0].GCPhysLast       = aSavedRamRangeLookup[0].GCPhysLast;
            pVM->pgm.s.aRamRangeLookup[1].GCPhysFirstAndId = aSavedRamRangeLookup[cSavedLookupEntries - 1].GCPhysFirstAndId;
            pVM->pgm.s.aRamRangeLookup[1].GCPhysLast       = aSavedRamRangeLookup[cSavedLookupEntries - 1].GCPhysLast;
            pVM->pgm.s.RamRangeUnion.cLookupEntries = 2;
        }
        else
        {
            /* Introduce gaps in the table by removing every 2nd entry, ASSUMING there
               is at least one entry in the table. */
            RTTESTI_CHECK(pVM->pgm.s.RamRangeUnion.cLookupEntries > 1 || iTableVar > 1);
            if (pVM->pgm.s.RamRangeUnion.cLookupEntries <= 1)
                break;
            uint32_t idxDst = 1;
            for (uint32_t idxSrc = iTableVar + 1; idxSrc < cSavedLookupEntries; idxSrc += iTableVar + 1)
            {
                pVM->pgm.s.aRamRangeLookup[idxDst].GCPhysFirstAndId = aSavedRamRangeLookup[idxSrc].GCPhysFirstAndId;
                pVM->pgm.s.aRamRangeLookup[idxDst].GCPhysLast       = aSavedRamRangeLookup[idxSrc].GCPhysLast;
                idxDst++;
            }
            pVM->pgm.s.RamRangeUnion.cLookupEntries = idxDst;
        }
    }

    CopyRamRangeLookupArray(pVM->pgm.s.aRamRangeLookup, aSavedRamRangeLookup, cSavedLookupEntries);
    pVM->pgm.s.RamRangeUnion.u64Combined = u64SavedRangeRangeUnion;
}

/*
 * Wrappers for use with the testPhysGetPage template.
 */
static DECLCALLBACK(int)
testPgmPhysGetPageAndRangeExSlowLockless(PVMCC pVM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRam)
{
    return pgmPhysGetPageAndRangeExSlowLockless(pVM, pVM->apCpusR3[0], GCPhys,
                                                (PGMPAGE volatile **)ppPage, (PGMRAMRANGE volatile **)ppRam);
}

static DECLCALLBACK(int)
testPgmPhysGetPageExSlow(PVMCC pVM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRam)
{
    *ppRam = NULL;
    int rc = pgmPhysGetPageExSlow(pVM, GCPhys, ppPage);
    if (RT_SUCCESS(rc))
    {
        PPGMPAGE pPage = *ppPage;
        for (uint32_t idx = 0, idxLast = RT_MIN(pVM->pgm.s.idRamRangeMax, RT_ELEMENTS(pVM->pgm.s.apRamRanges) - 1U);
             idx <= idxLast; idx++)
        {
            PPGMRAMRANGE const pRamRange = pVM->pgm.s.apRamRanges[idx];
            if (pRamRange && (uintptr_t)(pPage - &pRamRange->aPages[0]) < (pRamRange->cb >> GUEST_PAGE_SHIFT))
            {
                *ppRam = pRamRange;
                return rc;
            }
        }
        RTTestIFailed("Unable to find ram range for pPage=%p (GCPhys=%RGp)!", pPage, GCPhys);
    }
    return rc;
}


static DECLCALLBACK(int)
testPgmPhysGetPageSlow(PVMCC pVM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRam)
{
    *ppRam = NULL;
    PPGMPAGE pPage = *ppPage = pgmPhysGetPageSlow(pVM, GCPhys);
    if (pPage)
    {
        for (uint32_t idx = 0, idxLast = RT_MIN(pVM->pgm.s.idRamRangeMax, RT_ELEMENTS(pVM->pgm.s.apRamRanges) - 1U);
             idx <= idxLast; idx++)
        {
            PPGMRAMRANGE const pRamRange = pVM->pgm.s.apRamRanges[idx];
            if (pRamRange && (uintptr_t)(pPage - &pRamRange->aPages[0]) < (pRamRange->cb >> GUEST_PAGE_SHIFT))
            {
                *ppRam = pRamRange;
                return VINF_SUCCESS;
            }
        }
        RTTestIFailed("Unable to find ram range for pPage=%p (GCPhys=%RGp)!", pPage, GCPhys);
        return VINF_SUCCESS;
    }
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/** Wrap the PGMR3PhysRegisterRam call as passing 64-bit int values is
 *  in theory problematic on 32-bit hosts (like we support those). */
static DECLCALLBACK(int) AddTopRamRange(PVM pVM)
{
    return PGMR3PhysRegisterRam(pVM, RTGCPHYS_MAX - _64K + 1, _32K, "At the very top");
}


void testPGM(PVM pVM)
{
    /*
     * Do the tests.
     */
    RTTestISub("pgmPhysGetRangeSlow (1st round)");
    testPhysGetRange<pgmPhysGetRangeSlow, false>(pVM);
    RTTestISub("pgmPhysGetRangeAtOrAboveSlow (1st round)");
    testPhysGetRange<pgmPhysGetRangeAtOrAboveSlow, true>(pVM);
    RTTestISub("pgmPhysGetPageAndRangeExSlow (1st round)");
    testPhysGetPage<pgmPhysGetPageAndRangeExSlow>(pVM);
    RTTestISub("pgmPhysGetPageSlow (1st round)");
    testPhysGetPage<testPgmPhysGetPageSlow>(pVM);
    RTTestISub("pgmPhysGetPageExSlow (1st round)");
    testPhysGetPage<testPgmPhysGetPageExSlow>(pVM);
    RTTestISub("pgmPhysGetPageAndRangeExSlowLockless (1st round)");
    testPhysGetPage<testPgmPhysGetPageAndRangeExSlowLockless, true>(pVM);

    /* Insert a RAM range near the top of the memory to make sure we test the
       whole guest physical address range. */
    RTTESTI_CHECK_RC_OK(VMR3ReqCallWait(pVM, 0, (PFNRT)AddTopRamRange, 1, pVM));

    RTTestISub("pgmPhysGetRangeSlow (2nd round)");
    testPhysGetRange<pgmPhysGetRangeSlow, false>(pVM);
    RTTestISub("pgmPhysGetRangeAtOrAboveSlow (2nd round)");
    testPhysGetRange<pgmPhysGetRangeAtOrAboveSlow, true>(pVM);
    RTTestISub("pgmPhysGetPageAndRangeExSlow (2nd round)");
    testPhysGetPage<pgmPhysGetPageAndRangeExSlow>(pVM);
    RTTestISub("pgmPhysGetPageSlow (2nd round)");
    testPhysGetPage<testPgmPhysGetPageSlow>(pVM);
    RTTestISub("pgmPhysGetPageExSlow (2nd round)");
    testPhysGetPage<testPgmPhysGetPageExSlow>(pVM);
    RTTestISub("pgmPhysGetPageAndRangeExSlowLockless (2nd round)");
    testPhysGetPage<testPgmPhysGetPageAndRangeExSlowLockless, true>(pVM);
}

