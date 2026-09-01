/* $Id: stack-except-eh4-vcc.cpp 113916 2026-04-16 21:00:28Z knut.osmundsen@oracle.com $ */
/** @file
 * IPRT - Visual C++ Compiler - Stack Checking, __GSHandlerCheck_SEH.
 */

/*
 * Copyright (C) 2022-2026 Oracle and/or its affiliates.
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
#include "internal/nocrt.h"

#include "except-vcc.h"


#if !defined(RT_ARCH_AMD64)
# error "This file is for AMD64 (and probably ARM, but needs porting)"
#endif



/**
 * Check the stack cookie before maybe calling the C++ exception handler.
 *
 * This is to prevent attackers from bypassing stack cookie checking by
 * triggering an exception.
 *
 * The signature is different from __GSHandlerCheck_SEH and the compiler
 * structures are also different (HandlerData).
 *
 * @returns Exception disposition.
 * @param   pXcptRec    The exception record.
 * @param   pvEstFrame  Establisher frame address.
 * @param   pCpuCtx     The CPU context for the exception.
 * @param   pDispCtx    Dispatcher context.
 */
extern "C" __declspec(guard(suppress))
EXCEPTION_DISPOSITION __GSHandlerCheck_EH4(PEXCEPTION_RECORD pXcptRec, PVOID pvEstFrame,
                                           PCONTEXT pCpuCtx, PDISPATCHER_CONTEXT pDispCtx)
{
    /*
     * The HandlerData points to a 32-bit image relative offset to the
     * FuncInfoHeader and following data used by __CxxFrameHandler4.  After
     * this 32-bit offset comes the GS handler data.
     */
    PCGS_HANDLER_DATA  pHandlerData = (PCGS_HANDLER_DATA)&((PULONG)pDispCtx->HandlerData)[1];

    /*
     * Do the cookie checking.
     */
    IPRT_GS_HANDLER_CHECK_BODY(pHandlerData, pvEstFrame);

    /*
     * Now call the handler if the GS handler data indicates that we ought to.
     */
    if (IPRT_GS_HANDLER_HAS_HANDLER(pHandlerData, pXcptRec))
        return __CxxFrameHandler4(pXcptRec, pvEstFrame, pCpuCtx, pDispCtx);

    return ExceptionContinueSearch;
}

