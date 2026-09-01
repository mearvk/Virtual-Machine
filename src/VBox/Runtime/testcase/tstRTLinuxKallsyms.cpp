/* $Id: tstRTLinuxKallsyms.cpp 113553 2026-03-25 00:26:55Z knut.osmundsen@oracle.com $ */
/** @file
 * IPRT Testcase - /proc/kallsyms parsing for Linux kernel modules.
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
#include <iprt/dbg.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
const char *g_pszTestKallsyms = "/proc/kallsyms";


static void dotest(void)
{
    RTDBGKRNLINFO hKrnlInfo;
    RTTESTI_CHECK_RC_RETV(RTR0DbgKrnlInfoOpen(&hKrnlInfo, 0), VINF_SUCCESS);
    static struct { const char *pszSym, *pszMod; } const s_aSyms[] =
    {
        { "kernel_read",               NULL  },
        { "init_mm",                   NULL  },
        { "hid_bus_type",              "hid" },         // optional
        { "kvm_enable_virtualization", "kvm" },         // optional
        { "nvme_auth_hmac_name",       "nvme_auth" },   // optional
        { "fuse_direct_io",            "fuse" },        // optional
        { "fuse_init",                 "fuse" },        // optional
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_aSyms); i++)
    {
        void *pvValue = NULL;
        int rc = RTR0DbgKrnlInfoQuerySymbol(hKrnlInfo, s_aSyms[i].pszMod, s_aSyms[i].pszSym, &pvValue);
        if (s_aSyms[i].pszMod)
            RTTestIPrintf(RTTESTLVL_ALWAYS, "%Rrc %p %s!%s\n", rc, pvValue, s_aSyms[i].pszMod, s_aSyms[i].pszSym);
        else
            RTTestIPrintf(RTTESTLVL_ALWAYS, "%Rrc %p %s\n", rc, pvValue, s_aSyms[i].pszSym);
        if (!s_aSyms[i].pszMod)
            RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
        if (RT_SUCCESS(rc))
            RTTESTI_CHECK_RC(RTR0DbgKrnlInfoQuerySymbol(hKrnlInfo, s_aSyms[i].pszMod, s_aSyms[i].pszSym, NULL), rc);
    }

    RTTESTI_CHECK_RC(RTR0DbgKrnlInfoQuerySymbol(hKrnlInfo, NULL, "no_such_symbol_name_really", NULL), VERR_SYMBOL_NOT_FOUND);
    RTTESTI_CHECK(RTR0DbgKrnlInfoRelease(hKrnlInfo) == 0);
    RTTESTI_CHECK(RTR0DbgKrnlInfoRelease(NIL_RTDBGKRNLINFO) == 0);
}


int main(int argc, char **argv)
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTLinuxKallsyms", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /* Optional kallsyms path as argument. */
    if (argc == 2)
        g_pszTestKallsyms = argv[1];
#ifndef RT_OS_LINUX
    else
        return RTTestSkipAndDestroy(hTest, "not on linux");
#endif

    dotest();

    return RTTestSummaryAndDestroy(hTest);
}

