/* $Id: tstLdrObjR0.cpp 112518 2026-01-13 15:36:59Z knut.osmundsen@oracle.com $ */
/** @file
 * IPRT - RTLdr test object.
 *
 * We use precompiled versions of this object for testing all the loaders.
 *
 * This is not supposed to be pretty or usable code, just something which
 * make life difficult for the loader.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */



/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifndef IN_RING0
# error "not IN_RING0!"
#endif
#include <VBox/dis.h>
#include <iprt/crc.h>
#include <iprt/log.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const char szStr1[] = "some readonly string";
static char szStr2[6000] = "some read/write string";
static char achBss[8192];

/** Absolute data fixups. */
struct CLANG11WEIRDNOTHROW { PFNRT pfn; } g_ManualR0Deps[] =
{
    { (PFNRT)RTCrc32 },
    { NULL }
};


#ifdef VBOX_SOME_IMPORT_FUNCTION
extern "C" DECLIMPORT(int) SomeImportFunction(void);
#endif


extern "C" DECLEXPORT(int) Entrypoint(void)
{
    strcpy(achBss, szStr2);
    memcpy(achBss, szStr1, sizeof(szStr1));
    memcpy(achBss, (void *)(uintptr_t)&Entrypoint, 32);
#ifdef VBOX_SOME_IMPORT_FUNCTION
    memcpy(achBss, (void *)(uintptr_t)&SomeImportFunction, 32);
    return SomeImportFunction();
#else
    return 0;
#endif
}


extern "C" DECLEXPORT(uint32_t) SomeExportFunction1(void *pvBuf)
{
    NOREF(pvBuf);
    return achBss[0] + achBss[RT_ELEMENTS(achBss) - 1];
}


extern "C" DECLEXPORT(char *) SomeExportFunction2(void *pvBuf)
{
    NOREF(pvBuf);
    return (char *)memcpy(achBss, szStr1, sizeof(szStr1));
}


extern "C" DECLEXPORT(char *) SomeExportFunction3(void *pvBuf)
{
    NOREF(pvBuf);
    return (char *)memcpy(achBss, szStr2, strlen(szStr2));
}


extern "C" DECLEXPORT(void *) SomeExportFunction4(void)
{
    static unsigned cb;
    DISSTATE Dis;

    memset(&Dis, 0, sizeof(Dis));

    DISInstr((void *)(uintptr_t)SomeExportFunction3, DISCPUMODE_32BIT, &Dis, &cb);
    return (void *)(uintptr_t)&SomeExportFunction1;
}


extern "C" DECLEXPORT(uintptr_t) SomeExportFunction5(void)
{
    return (uintptr_t)SomeExportFunction3(NULL) + (uintptr_t)SomeExportFunction2(NULL)
         + (uintptr_t)SomeExportFunction1(NULL) + (uintptr_t)&SomeExportFunction4;
}


/**
 * 2nd test function.
 */
extern "C" DECLEXPORT(int) Test2(void)
{
    /*
     * Do a similar RTLogCreateEx calls we do in VMMR0.cpp.
     * Had trouble with this on linux.arm64.
     */
    PRTLOGGER                 pLogger        = NULL;
    static const char * const s_apszGroups[] = { "all", "whatever" };
    static char               s_achBuf[4096];
    RTLOGBUFFERDESC           aBufDescs[4];
    uint32_t const            cbBuf          = sizeof(s_achBuf) / RT_ELEMENTS(aBufDescs);
    for (size_t i = 0; i < RT_ELEMENTS(aBufDescs); i++)
    {
        aBufDescs[i].u32Magic    = RTLOGBUFFERDESC_MAGIC;
        aBufDescs[i].uReserved   = 0;
        aBufDescs[i].cbBuf       = cbBuf;
        aBufDescs[i].offBuf      = 0;
        aBufDescs[i].pchBuf      = s_achBuf + i * cbBuf;
        aBufDescs[i].pAux        = NULL;
    }

    int rc = RTLogCreateEx(&pLogger, "VBOX_LDR_TEST_LOG", RTLOG_F_NO_LOCKING | RTLOGFLAGS_BUFFERED,
                           "all", RT_ELEMENTS(s_apszGroups), s_apszGroups, UINT32_MAX,
                           RT_ELEMENTS(aBufDescs), aBufDescs, RTLOGDEST_DUMMY,
                           NULL /*pfnPhase*/, 0 /*cHistory*/, 0 /*cbHistoryFileMax*/, 0 /*cSecsHistoryTimeSlot*/,
                           NULL /*pOutputIf*/, NULL /*pvOutputIfUser*/,
                           NULL /*pErrInfo*/, NULL /*pszFilenameFmt*/);
    if (RT_SUCCESS(rc))
        rc = RTLogDestroy(pLogger);
    return rc;
}

