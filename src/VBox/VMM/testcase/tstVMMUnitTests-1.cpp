/* $Id: tstVMMUnitTests-1.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VMM internal unit tests.
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
#include <VBox/vmm/vmm.h>

#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/test.h>

#include "tstVMMUnitTests-1.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
RTTEST g_hTest;



int main(int argc, char **argv)
{
    /*
     * We run the VMM in driverless mode to avoid needing to hardened the testcase.
     */
    RTEXITCODE rcExit;
    int rc = RTR3InitExe(argc, &argv, SUPR3INIT_F_DRIVERLESS << RTR3INIT_FLAGS_SUPLIB_SHIFT);
    if (RT_SUCCESS(rc))
    {
        rc = RTTestCreate("tstVMMUnitTests-1", &g_hTest);
        if (RT_SUCCESS(rc))
        {
            RTTestBanner(g_hTest);

            /*
             * Create a test VM to play with.
             */
            PVM  pVM;
            PUVM pUVM;
            RTTESTI_CHECK_RC_OK(rc = VMR3Create(1 /*cCpus*/, NULL, VMCREATE_F_DRIVERLESS, NULL, NULL, NULL, NULL, &pVM, &pUVM));
            if (RT_SUCCESS(rc))
            {
                testPGM(pVM);

                /*
                 * Clean up.
                 */
                RTTESTI_CHECK_RC_OK(VMR3PowerOff(pUVM));
                RTTESTI_CHECK_RC_OK(VMR3Destroy(pUVM));
                VMR3ReleaseUVM(pUVM);
            }

            rcExit = RTTestSummaryAndDestroy(g_hTest);
        }
        else
            rcExit = RTMsgErrorExitFailure("RTTestCreate failed: %Rrc", rc);
    }
    else
        rcExit = RTMsgInitFailure(rc);
    return rcExit;
}

