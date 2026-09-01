/* $Id: IEMAllStub.cpp 112440 2026-01-13 09:05:15Z knut.osmundsen@oracle.com $ */
/** @file
 * IEM - Interpreted Execution Manager, dummy IEM stub functions.
 *
 * This is for use during porting to a new host as well as when IEM isn't
 * required for some reason.
 */

/*
 * Copyright (C) 2023-2025 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_IEM
#include <VBox/vmm/iem.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

VMM_INT_DECL(VBOXSTRICTRC) IEMExecForExits(PVMCPUCC pVCpu, uint32_t fWillExit, uint32_t cMaxInstructions,
                                           uint32_t cMaxInstructionsWithoutExits, PIEMEXECFOREXITSTATS pStats)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, fWillExit, cMaxInstructions, cMaxInstructionsWithoutExits);
    RT_ZERO(*pStats);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecLots(PVMCPUCC pVCpu, uint32_t cMaxInstructions, uint32_t cPollRate, uint32_t *pcInstructions)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, cMaxInstructions, cPollRate, pcInstructions);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOne(PVMCPUCC pVCpu)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOneWithPrefetchedByPC(PVMCPUCC pVCpu, uint64_t OpcodeBytesPC,
                                                        const void *pvOpcodeBytes, size_t cbOpcodeBytes)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, OpcodeBytesPC, pvOpcodeBytes, cbOpcodeBytes);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOneIgnoreLock(PVMCPUCC pVCpu)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOneBypass(PVMCPUCC pVCpu)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOneBypassWithPrefetchedByPC(PVMCPUCC pVCpu, uint64_t OpcodeBytesPC,
                                                              const void *pvOpcodeBytes, size_t cbOpcodeBytes)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, OpcodeBytesPC, pvOpcodeBytes, cbOpcodeBytes);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMInjectTrpmEvent(PVMCPUCC pVCpu)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(void) IEMTlbInvalidateAll(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
}


VMM_INT_DECL(void) IEMTlbInvalidateAllGlobal(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
}


VMM_INT_DECL(void) IEMTlbInvalidateAllPhysicalAllCpus(PVMCC pVM, VMCPUID idCpuCaller, IEMTLBPHYSFLUSHREASON enmReason)
{
    RT_NOREF(pVM, idCpuCaller, enmReason);
}


VMMR3_INT_DECL(VBOXSTRICTRC) IEMR3ProcessForceFlag(PVM pVM, PVMCPUCC pVCpu, VBOXSTRICTRC rcStrict)
{
    AssertReleaseFailed();
    RT_NOREF(pVM, pVCpu, rcStrict);
    return VERR_NOT_IMPLEMENTED;
}


#ifdef VBOX_VMM_TARGET_X86

VMM_INT_DECL(VBOXSTRICTRC)
IEMInjectTrap(PVMCPUCC pVCpu, uint8_t u8TrapNo, TRPMEVENT enmType, uint16_t uErrCode, RTGCPTR uCr2, uint8_t cbInstr)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, u8TrapNo, enmType, uErrCode, uCr2, cbInstr);
    return VERR_NOT_IMPLEMENTED;
}

VMM_INT_DECL(IEMXCPTRAISE) IEMEvaluateRecursiveXcpt(PVMCPUCC pVCpu, uint32_t fPrevFlags, uint8_t uPrevVector, uint32_t fCurFlags,
                                                    uint8_t uCurVector, PIEMXCPTRAISEINFO pXcptRaiseInfo)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, fPrevFlags, uPrevVector, fCurFlags, uCurVector, pXcptRaiseInfo);
    return IEMXCPTRAISE_INVALID;
}


VMM_INT_DECL(void) IEMTlbInvalidatePage(PVMCPUCC pVCpu, RTGCPTR GCPtr)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, GCPtr);
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedCpuid(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, cbInstr);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedClts(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedInvlpg(PVMCPUCC pVCpu,  uint8_t cbInstr, RTGCPTR GCPtrPage)
{
    RT_NOREF(pVCpu, cbInstr, GCPtrPage);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedLmsw(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t uValue, RTGCPTR GCPtrEffDst)
{
    RT_NOREF(pVCpu, cbInstr, uValue, GCPtrEffDst);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMonitor(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMwait(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovCRxWrite(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iCrReg, uint8_t iGReg)
{
    RT_NOREF(pVCpu, cbInstr, iCrReg, iGReg);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovCRxRead(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iGReg, uint8_t iCrReg)
{
    RT_NOREF(pVCpu, cbInstr, iGReg, iCrReg);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovDRxWrite(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iDrReg, uint8_t iGReg)
{
    RT_NOREF(pVCpu, cbInstr, iDrReg, iGReg);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedMovDRxRead(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iGReg, uint8_t iDrReg)
{
    RT_NOREF(pVCpu, cbInstr, iGReg, iDrReg);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedRdpmc(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedRdtsc(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedRdtscp(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedRdmsr(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedWrmsr(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecDecodedXsetbv(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    RT_NOREF(pVCpu, cbInstr);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecStringIoWrite(PVMCPUCC pVCpu, uint8_t cbValue, IEMMODE enmAddrMode,
                                                 bool fRepPrefix, uint8_t cbInstr, uint8_t iEffSeg, bool fIoChecked)
{
    RT_NOREF(pVCpu, cbValue, enmAddrMode, fRepPrefix, cbInstr, iEffSeg, fIoChecked);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC)  IEMExecStringIoRead(PVMCPUCC pVCpu, uint8_t cbValue, IEMMODE enmAddrMode,
                                                bool fRepPrefix, uint8_t cbInstr, bool fIoChecked)
{
    RT_NOREF(pVCpu, cbValue, enmAddrMode, fRepPrefix, cbInstr, fIoChecked);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}

#endif
