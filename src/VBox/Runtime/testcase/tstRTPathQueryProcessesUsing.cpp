/* $Id: tstRTPathQueryProcessesUsing.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * IPRT Testcase - RTPathQueryProcessesUsing testcase
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
#include <iprt/path.h>

#include <iprt/file.h>
#include <iprt/errcore.h>
#include <iprt/message.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>


static void basicTest(void)
{
    RTTestISub("basics");

    /*
     * Open the executable and check that we can see that we've done so.
     */
    const char * const pszExePath = RTProcExecutablePath();
    RTFILE             hFile      = NIL_RTFILE;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile, pszExePath, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE), VINF_SUCCESS);

    RTPROCESS aPids[512];
    uint32_t  cPids;

    for (uint32_t iVar = 0; iVar < 3; iVar++)
    {
        cPids = RT_ELEMENTS(aPids);
        int rc = RTPathQueryProcessesUsing(pszExePath, iVar < 1 ? RTPATH_QUERY_PROC_F_SKIP_MAPPINGS : 0, &cPids, &aPids[0]);
        if (rc != VERR_NOT_SUPPORTED)
        {
            RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
            if (RT_SUCCESS(rc))
            {
                RTTESTI_CHECK(cPids < RT_ELEMENTS(aPids));
                RTPROCESS const pidSelf = RTProcSelf();
                uint32_t        cFound  = 0;
                for (uint32_t idx = 0; idx < cPids; idx++)
                    if (aPids[idx] == pidSelf)
                        cFound++;
                if (cFound == 0)
                    RTTestIFailed("iVar=%u", iVar);
            }
        }
        if (iVar == 1)
        {
            RTFileClose(hFile);
            hFile = NIL_RTFILE;
        }
    }
}


int main(int argc, char **argv)
{
    RTTEST      hTest;
    RTEXITCODE  rcExit = RTTestInitExAndCreate(argc, &argv, 0, "tstRTPathQueryProcessesUsing", &hTest);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        RTTestBanner(hTest);

        /*
         * If arguments given, do custom testing.
         */
        if (argc > 1)
        {
            uint32_t fFlags = 0;
            for (int i = 1; i < argc; i++)
            {
                if (   argv[i][0] == '-'
                    && argv[i][1] == 's')
                    fFlags = RTPATH_QUERY_PROC_F_DIR_INCLUDE_SUB_OBJ;
                else
                {
                    RTPROCESS aPids[512];
                    uint32_t cProcs = RT_ELEMENTS(aPids);
                    int rc = RTPathQueryProcessesUsing(argv[i], fFlags, &cProcs, &aPids[0]);
                    if (RT_SUCCESS(rc))
                    {
                        RTPrintf("%s%s:\n", argv[i], rc == VINF_BUFFER_OVERFLOW ? " (truncated)" : "");
                        for (uint32_t idx = 0; idx < (rc == VINF_BUFFER_OVERFLOW ? RT_ELEMENTS(aPids) : cProcs); idx++)
                        {
                            char szName[RTPATH_MAX];
                            rc = RTProcQueryExecutablePath(aPids[idx], szName, sizeof(szName), NULL);
                            if (RT_SUCCESS(rc))
                            {
                                RTPrintf("   pid %RU32 - %s\n", aPids[idx], szName);

                                size_t cbNeeded = 0;
                                RTTESTI_CHECK_RC(RTProcQueryExecutablePath(aPids[idx], NULL, 0, &cbNeeded), VERR_BUFFER_OVERFLOW);
                                RTTESTI_CHECK(cbNeeded == strlen(szName) + 1);
                            }
                            else
                                RTPrintf("   pid %RU32 (RTProcQueryExecutablePath -> %Rrc)\n", aPids[idx], rc);
                        }
                    }
                    else
                        RTPrintf("RTPathQueryProcessesUsing(%s,,,) ->  %Rrc\n", argv[i], rc);
                }
            }
        }
        /*
         * Otherwise, automatic testing.
         */
        else
        {
            basicTest();
        }

        rcExit = RTTestSummaryAndDestroy(hTest);
    }
    return rcExit;
}

