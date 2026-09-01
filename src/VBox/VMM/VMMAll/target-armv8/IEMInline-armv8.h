/* $Id: IEMInline-armv8.h 112435 2026-01-13 08:49:25Z knut.osmundsen@oracle.com $ */
/** @file
 * IEM - Interpreted Execution Manager - Inlined Functions, ARMv8 target.
 */

/*
 * Copyright (C) 2011-2025 Oracle and/or its affiliates.
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

#ifndef VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInline_armv8_h
#define VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInline_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/err.h>
#include <iprt/armv8.h>


/**
 * Figures out the current translation regime.
 *
 * This is necessary for proper TLB handling, since entries from different
 * translation regimes shall not intermix.
 *
 * @returns IEM_F_ARM_REGIME_MASK
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   fExecMode           The mode part of fExec.
 */
DECL_FORCE_INLINE(uint32_t) iemCalcExecTranslationRegime(PVMCPU pVCpu, uint32_t fExecMode)
{
#if 0 /** @todo EL3 */
    uint64_t const uScrEl3 = pVCpu->cpum.GstCtx.ScrEl3.u64;
#else
    uint64_t const uScrEl3 = RT_BIT_64(0) /*NS*/;
#endif
    uint64_t const uNseNs = (uScrEl3         & RT_BIT_64(0))
                          | ((uScrEl3 >> 62) & RT_BIT_64(1));
    switch (fExecMode & IEM_F_MODE_ARM_EL_MASK)
    {
        default:
        case 0:
            if (   (pVCpu->cpum.GstCtx.HcrEl2.u64 & (ARMV8_HCR_EL2_E2H | ARMV8_HCR_EL2_TGE))
                == (ARMV8_HCR_EL2_E2H | ARMV8_HCR_EL2_TGE)) /* Effective VM is 0 here. */
            {
                if (uNseNs == 1)
                    return IEM_ARM_REGIME_EL20_NOSEC << IEM_F_ARM_REGIME_SHIFT;
                if (uNseNs == 0)
                {
                    Assert(uScrEl3 & RT_BIT_64(18));
                    return IEM_ARM_REGIME_EL20_SEC << IEM_F_ARM_REGIME_SHIFT;
                }
                Assert(uNseNs == 3);
                return IEM_ARM_REGIME_EL20_REALM << IEM_F_ARM_REGIME_SHIFT;
            }
            RT_FALL_THRU();

        case 1 << IEM_F_MODE_ARM_EL_SHIFT:
            if (!(pVCpu->cpum.GstCtx.HcrEl2.u64 & ARMV8_HCR_EL2_VM))
            {
                if (uNseNs == 1)
                    return IEM_ARM_REGIME_EL10_NOSEC << IEM_F_ARM_REGIME_SHIFT;
                Assert(uNseNs == 0);
                return IEM_ARM_REGIME_EL10_SEC << IEM_F_ARM_REGIME_SHIFT;
            }
            if (uNseNs == 1)
                return IEM_ARM_REGIME_EL10_NOSEC_S2 << IEM_F_ARM_REGIME_SHIFT;
            if (uNseNs == 0)
                return IEM_ARM_REGIME_EL10_SEC_S2 << IEM_F_ARM_REGIME_SHIFT;
            Assert(uNseNs == 3);
            return IEM_ARM_REGIME_EL10_REALM_S2 << IEM_F_ARM_REGIME_SHIFT;

        case 2 << IEM_F_MODE_ARM_EL_SHIFT:
            if (pVCpu->cpum.GstCtx.HcrEl2.u64 & ARMV8_HCR_EL2_E2H)
            {
                if (uNseNs == 1)
                    return IEM_ARM_REGIME_EL20_NOSEC << IEM_F_ARM_REGIME_SHIFT;
                if (uNseNs == 0)
                {
                    Assert(uScrEl3 & RT_BIT_64(18));
                    return IEM_ARM_REGIME_EL20_SEC << IEM_F_ARM_REGIME_SHIFT;
                }
                Assert(uNseNs == 3);
                return IEM_ARM_REGIME_EL20_REALM << IEM_F_ARM_REGIME_SHIFT;
            }
            if (uNseNs == 1)
                return IEM_ARM_REGIME_EL2_NONSEC << IEM_F_ARM_REGIME_SHIFT;
            if (uNseNs == 0)
            {
                Assert(uScrEl3 & RT_BIT_64(18));
                return IEM_ARM_REGIME_EL2_SEC << IEM_F_ARM_REGIME_SHIFT;
            }
            Assert(uNseNs == 3);
            return  IEM_ARM_REGIME_EL2_REALM << IEM_F_ARM_REGIME_SHIFT;

        case 3 << IEM_F_MODE_ARM_EL_SHIFT:
            return IEM_ARM_REGIME_EL3 << IEM_F_ARM_REGIME_SHIFT;
    }
}


/**
 * Calculates the IEM_F_ARM_A & IEM_F_ARM_AA flags.
 *
 * @returns Mix of IEM_F_ARM_A, IEM_F_ARM_AA and zero.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   fExecMode           The mode part of fExec.
 */
DECL_FORCE_INLINE(uint32_t) iemCalcExecAcFlag(PVMCPUCC pVCpu, uint32_t fExecMode) RT_NOEXCEPT
{
    IEM_CTX_ASSERT(pVCpu, CPUMCTX_EXTRN_SCTLR_TCR_TTBR | CPUMCTX_EXTRN_SYSREG_EL2 | CPUMCTX_EXTRN_PSTATE);
    uint64_t const fSctlr =    IEM_F_MODE_ARM_GET_EL(fExecMode) == 1
                            || (   IEM_F_MODE_ARM_GET_EL(fExecMode) == 0
                                &&    (pVCpu->cpum.GstCtx.HcrEl2.u64 & (RT_BIT_64(34)/*E2H*/ | RT_BIT_64(27)/*TGE*/))
                                   !=                                  (RT_BIT_64(34)/*E2H*/ | RT_BIT_64(27)/*TGE*/) )
                          ? pVCpu->cpum.GstCtx.Sctlr.u64
                          : pVCpu->cpum.GstCtx.SctlrEl2.u64;
    /** @todo armv8: EL3 */
    AssertCompile(ARMV8_SCTLR_EL1_A   == ARMV8_SCTLR_EL2_A);
    AssertCompile(ARMV8_SCTLR_EL1_NAA == ARMV8_SCTLR_EL2_NAA);
    AssertCompile(ARMV8_SCTLR_EL1_SA  == ARMV8_SCTLR_EL2_SA);
    AssertCompile(ARMV8_SCTLR_EL1_SA0 == ARMV8_SCTLR_EL2_SA0);

    return ((fSctlr & ARMV8_SCTLR_EL1_A)   ? IEM_F_ARM_A : 0)
         | ((fSctlr & ARMV8_SCTLR_EL1_NAA) ? 0 : IEM_F_ARM_AA)
         | ((fSctlr & (IEM_F_MODE_ARM_GET_EL(fExecMode) == 0 ? ARMV8_SCTLR_EL1_SA0 : ARMV8_SCTLR_EL1_SA)) ? IEM_F_ARM_SA : 0);
}


#if 0 /* revisit when needed */
/**
 * Calculates the minimum page size.
 *
 * @returns IEM_F_ARM_S1_PAGE_MASK
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   fExecMode           The mode part of fExec.
 */
DECL_FORCE_INLINE(uint32_t) iemCalcExecMinPageSize(PVMCPUCC pVCpu, uint32_t fExecMode) RT_NOEXCEPT
{
    IEM_CTX_ASSERT(pVCpu, CPUMCTX_EXTRN_SCTLR_TCR_TTBR | CPUMCTX_EXTRN_PSTATE);
    uint64_t const fSCtlR = IEM_F_MODE_ARM_GET_EL(fExecMode) <= 1 ? pVCpu->cpum.GstCtx.Sctlr.u64
#if 1 /** @todo armv8: EL3 & check up EL2 logic. */
                          :                                         pVCpu->cpum.GstCtx.SctlrEl2.u64;
#else
                          : IEM_F_MODE_ARM_GET_EL(fExecMode) == 2 ? pVCpu->cpum.GstCtx.SctlrEl2.u64
                          :                                         pVCpu->cpum.GstCtx.SctlrEl3.u64;
#endif
    if (fSCtlR & ARMV8_SCTLR_EL1_M)
    {
        uint64_t const fTcr = IEM_F_MODE_ARM_GET_EL(fExecMode) <= 1 ? pVCpu->cpum.GstCtx.Tcr.u64
#if 1 /** @todo armv8: EL3 & check up EL2 logic. */
                            :                                         pVCpu->cpum.GstCtx.TcrEl2.u64;
#else
                        : IEM_F_MODE_ARM_GET_EL(fExecMode) == 2 ? pVCpu->cpum.GstCtx.TcrEl2.u64
                        :                                         pVCpu->cpum.GstCtx.TcrEl3.u64;
#endif
        switch (fTcr & ARMV8_TCR_EL1_AARCH64_TG0_MASK)
        {
            case ARMV8_TCR_EL1_AARCH64_TG0_4KB  << ARMV8_TCR_EL1_AARCH64_TG0_SHIFT:
                return IEM_F_ARM_S1_PAGE_4K;
            case ARMV8_TCR_EL1_AARCH64_TG0_16KB << ARMV8_TCR_EL1_AARCH64_TG0_SHIFT:
                return IEM_F_ARM_S1_PAGE_16K;
            case ARMV8_TCR_EL1_AARCH64_TG0_64KB << ARMV8_TCR_EL1_AARCH64_TG0_SHIFT:
                return IEM_F_ARM_S1_PAGE_64K;
            default:
                AssertFailed();
                return IEM_F_ARM_S1_PAGE_4K;
        }
    }
    /* MMU is not enabled, use 4KB TLB entries for now. */
    /** @todo check out 64KB for non-MMU mode. */
    return IEM_F_ARM_S1_PAGE_4K; /** @todo Do we need a NO_MMU flag? */
}
#endif /* revisit when needed */


/**
 * Calculates the IEM_F_MODE_XXX, IEM_F_ARM_A, IEM_F_ARM_AA and IEM_F_ARM_SP_IDX
 *
 * @returns IEM_F_MODE_XXX, IEM_F_ARM_A, IEM_F_ARM_AA, IEM_F_ARM_SP_IDX.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 */
DECL_FORCE_INLINE(uint32_t) iemCalcExecModeAndSpIdxAndAcFlagsAndS1PgSize(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    IEM_CTX_ASSERT(pVCpu, CPUMCTX_EXTRN_PSTATE);

    /** @todo we probably need to take the security state into the mode as
     *        well... */

    /* EL, SP idx, aarch64, aarch32, t32. */
    AssertCompile(ARMV8_SPSR_EL2_AARCH64_EL == IEM_F_MODE_ARM_EL_MASK);
    AssertCompile(ARMV8_SPSR_EL2_AARCH64_M4 == IEM_F_MODE_ARM_32BIT);
    AssertCompile(ARMV8_SPSR_EL2_AARCH64_T  == IEM_F_MODE_ARM_T32);
    uint32_t fExec = (uint32_t)pVCpu->cpum.GstCtx.fPState
                   & (uint32_t)(ARMV8_SPSR_EL2_AARCH64_EL | ARMV8_SPSR_EL2_AARCH64_M4 | ARMV8_SPSR_EL2_AARCH64_T);
    if (!(fExec & ARMV8_SPSR_EL2_AARCH64_M4))
    {
        Assert(!(fExec & ARMV8_SPSR_EL2_AARCH64_T)); /* aarch64 */
        if (pVCpu->cpum.GstCtx.fPState & ARMV8_SPSR_EL2_AARCH64_SP)
            fExec |= IEM_F_MODE_ARM_GET_EL(fExec); /* SP index = ELx */
    }
    else
    {
        fExec &= ARMV8_SPSR_EL2_AARCH64_M4 | ARMV8_SPSR_EL2_AARCH64_T;
        switch (pVCpu->cpum.GstCtx.fPState & ARMV8_SPSR_EL2_AARCH64_M)
        {
            case 0x0: /* User */
                //fExec |= 0 << IEM_F_MODE_ARM_EL_SHIFT;
                break;
            case 0x1: /* FIQ */
            case 0x2: /* IRQ */
            case 0x3: /* Supervisor */
            case 0x7: /* Abort */
            case 0xb: /* Undefined */
            case 0xf: /* System */
                fExec |= 1 << IEM_F_MODE_ARM_EL_SHIFT;
                break;
            case 0xa: /* Hypervisor */
                fExec |= 2 << IEM_F_MODE_ARM_EL_SHIFT;
                break;
            case 0x4: case 0x5: case 0x6:
            case 0x8: case 0x9:
            case 0xc: case 0xd: case 0xe:
                AssertFailed();
                break;
        }

#if 0 /** @todo SP index for aarch32: We don't have SPSEL. */
        if (pVCpu->cpum.GstCtx.SpSel.u32 & 1)
            fExec |= IEM_F_MODE_ARM_GET_EL(fExec); /* SP index = ELx */
#endif
    }

    /* The translation regime. */
    fExec |= iemCalcExecTranslationRegime(pVCpu, fExec);

    /* Alignment checks: */
    fExec |= iemCalcExecAcFlag(pVCpu, fExec);

#if 0 /* revisit when needed */
    /* Page size. */
    fExec |= iemCalcExecMinPageSize(pVCpu, fExec);
#endif
    return fExec;
}

#ifdef VBOX_INCLUDED_vmm_dbgf_h /* VM::dbgf.ro.cEnabledHwBreakpoints is only accessible if VBox/vmm/dbgf.h is included. */

/**
 * Calculates IEM_F_BRK_PENDING_XXX (IEM_F_PENDING_BRK_MASK) and IEM_F_ARM_SOFTWARE_STEP flags.
 *
 * @returns IEM_F_BRK_PENDING_XXX + IEM_F_ARM_SOFTWARE_STEP.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 */
DECL_FORCE_INLINE(uint32_t) iemCalcExecDbgFlags(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    IEM_CTX_ASSERT(pVCpu, CPUMCTX_EXTRN_PSTATE);
    /** @todo The debug state is a bit complicated at first. It appears the
     *        MDSCR_EL1.SS flag is the one to set/disable together with anything
     *        masking debug exceptions.  The PSTATE.SS flag is just to indicate
     *        whether we're raising a debug exception before the current
     *        instruction (SS=1) or the next one (SS=0, set to 1 upon instruction
     *        retirement).  More on the exact boundrary and priority stuff needs
     *        to be considered here... */
    if (RT_LIKELY(   !(pVCpu->cpum.GstCtx.Mdscr.u64 & ARMV8_MDSCR_EL1_AARCH64_SS)
                  && pVCpu->CTX_SUFF(pVM)->dbgf.ro.cEnabledHwBreakpoints == 0))
        return 0;
    return iemCalcExecDbgFlagsSlow(pVCpu);
}


DECL_FORCE_INLINE(uint32_t) iemCalcExecFlags(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    return iemCalcExecModeAndSpIdxAndAcFlagsAndS1PgSize(pVCpu)
         | iemCalcExecDbgFlags(pVCpu)
         ;
}


/**
 * Re-calculates the IEM_F_MODE_XXX, IEM_F_ARM_A, IEM_F_ARM_AA and
 * IEM_F_ARM_SP_IDX parts of IEMCPU::fExec.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 */
DECL_FORCE_INLINE(void) iemRecalcExecModeAndSpIdxAndAcFlags(PVMCPUCC pVCpu)
{
    ICORE(pVCpu).fExec = (ICORE(pVCpu).fExec & ~(IEM_F_MODE_MASK | IEM_F_ARM_A | IEM_F_ARM_AA))
                       | iemCalcExecModeAndSpIdxAndAcFlagsAndS1PgSize(pVCpu);
}


/**
 * Re-calculates IEM_F_BRK_PENDING_XXX (IEM_F_PENDING_BRK_MASK) and
 * IEM_F_ARM_SOFTWARE_STEP part of IEMCPU::fExec.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 */
DECL_FORCE_INLINE(void) iemRecalcExecDbgFlags(PVMCPUCC pVCpu)
{
    ICORE(pVCpu).fExec = (ICORE(pVCpu).fExec & ~(IEM_F_PENDING_BRK_MASK | IEM_F_ARM_SOFTWARE_STEP))
                       | iemCalcExecDbgFlags(pVCpu);
}

#endif /* VBOX_INCLUDED_vmm_dbgf_h */


/** @name   Register Access.
 * @{
 */


/**
 * Fetches the value of a 8-bit general purpose register.
 *
 * @returns The register value.
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 * @param   fSp     Set if register 31 is SP, otherwise it's zero.
 */
DECL_FORCE_INLINE(uint8_t) iemGRegFetchU8(PVMCPUCC pVCpu, uint8_t iReg, bool fSp = false) RT_NOEXCEPT
{
    Assert(iReg < 32);
    return iReg < 31 ? (uint8_t)pVCpu->cpum.GstCtx.aGRegs[iReg].w
          : fSp      ? (uint8_t)pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u32
          : UINT8_C(0);
}


/**
 * Fetches the value of a 16-bit general purpose register.
 *
 * @returns The register value.
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 * @param   fSp     Set if register 31 is SP, otherwise it's zero.
 */
DECL_FORCE_INLINE(uint16_t) iemGRegFetchU16(PVMCPUCC pVCpu, uint8_t iReg, bool fSp = false) RT_NOEXCEPT
{
    Assert(iReg < 32);
    return iReg < 31 ? (uint16_t)pVCpu->cpum.GstCtx.aGRegs[iReg].w
          : fSp      ? (uint16_t)pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u32
          : UINT16_C(0);
}


/**
 * Fetches the value of a 32-bit general purpose register.
 *
 * @returns The register value.
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 * @param   fSp     Set if register 31 is SP, otherwise it's zero.
 */
DECL_FORCE_INLINE(uint32_t) iemGRegFetchU32(PVMCPUCC pVCpu, uint8_t iReg, bool fSp = false) RT_NOEXCEPT
{
    Assert(iReg < 32);
    return iReg < 31 ? pVCpu->cpum.GstCtx.aGRegs[iReg].w
          : fSp      ? pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u32
          : UINT32_C(0);
}


/**
 * Fetches the value of a 64-bit general purpose register.
 *
 * @returns The register value.
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 * @param   fSp     Set if register 31 is SP, otherwise it's zero.
 */
DECL_FORCE_INLINE(uint64_t) iemGRegFetchU64(PVMCPUCC pVCpu, uint8_t iReg, bool fSp = false) RT_NOEXCEPT
{
    Assert(iReg < 32);
    return iReg < 31 ? pVCpu->cpum.GstCtx.aGRegs[iReg].x
          : fSp      ? pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u64
          : UINT64_C(0);
}


/**
 * Fetches the value of a 64-bit general purpose register, with SP alignment
 * checking.
 *
 * @returns The register value.
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 */
DECL_FORCE_INLINE_THROW(uint64_t) iemGRegFetchU64WithSpAlignCheck(PVMCPUCC pVCpu, uint8_t iReg)
{
    Assert(iReg < 32);
    uint64_t uRet;
    if (iReg < 31)
        uRet = pVCpu->cpum.GstCtx.aGRegs[iReg].x;
    else
    {
        uRet = pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u64;
        if (RT_LIKELY(!(uRet & 15) || !(ICORE(pVCpu).fExec & IEM_F_ARM_SA)))
        { /* likely */ }
        else
            iemRaiseSpAlignmentCheck(pVCpu);
    }
    return uRet;
}


/**
 * Stores a 8-bit value to a general purpose register, zeroing extending it to
 * the full register width.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 * @param   uValue  The value to store.
 * @param   fSp     Set if register 31 is SP, otherwise it's zero.
 */
DECL_FORCE_INLINE(void) iemGRegStoreU8(PVMCPUCC pVCpu, uint8_t iReg, uint8_t uValue, bool fSp = false) RT_NOEXCEPT
{
    Assert(iReg < 32);
    if (iReg < 31)
        pVCpu->cpum.GstCtx.aGRegs[iReg].x = uValue;
    else if (fSp)
        pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u64 = uValue;
}


/**
 * Stores a 16-bit value to a general purpose register, zeroing extending it to
 * the full register width.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 * @param   uValue  The value to store.
 * @param   fSp     Set if register 31 is SP, otherwise it's zero.
 */
DECL_FORCE_INLINE(void) iemGRegStoreU16(PVMCPUCC pVCpu, uint8_t iReg, uint16_t uValue, bool fSp = false) RT_NOEXCEPT
{
    Assert(iReg < 32);
    if (iReg < 31)
        pVCpu->cpum.GstCtx.aGRegs[iReg].x = uValue;
    else if (fSp)
        pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u64 = uValue;
}


/**
 * Stores a 32-bit value to a general purpose register, zeroing extending it to
 * the full register width.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 * @param   uValue  The value to store.
 * @param   fSp     Set if register 31 is SP, otherwise it's zero.
 */
DECL_FORCE_INLINE(void) iemGRegStoreU32(PVMCPUCC pVCpu, uint8_t iReg, uint32_t uValue, bool fSp = false) RT_NOEXCEPT
{
    Assert(iReg < 32);
    if (iReg < 31)
        pVCpu->cpum.GstCtx.aGRegs[iReg].x = uValue;
    else if (fSp)
        pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u64 = uValue;
}


/**
 * Stores a 64-bit value to a general purpose register.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   iReg    The register.
 * @param   uValue  The value to store.
 * @param   fSp     Set if register 31 is SP, otherwise it's zero.
 */
DECL_FORCE_INLINE(void) iemGRegStoreU64(PVMCPUCC pVCpu, uint8_t iReg, uint64_t uValue, bool fSp = false) RT_NOEXCEPT
{
    Assert(iReg < 32);
    if (iReg < 31)
        pVCpu->cpum.GstCtx.aGRegs[iReg].x = uValue;
    else if (fSp)
        pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u64 = uValue;
}


/**
 * Get the address of the top of the stack.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 */
DECL_FORCE_INLINE(RTGCPTR) iemRegGetSp(PCVMCPU pVCpu) RT_NOEXCEPT
{
    return pVCpu->cpum.GstCtx.aSpReg[IEM_F_ARM_GET_SP_IDX(ICORE(pVCpu).fExec)].u64;
}


/**
 * Updates the PC to point to the next instruction.
 *
 * This is the generic version used by code that isn't mode specific.  Code that
 * is only used in aarch64, aarch32 and t32 should call the specific versions
 * below.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   f32Bit  Set if it's a 32-bit wide instruction, clear if 16-bit (T32
 *                  mode only).
 * @see     iemRegPcA64Inc, iemRegPcA32Inc, iemRegPcT32Inc
 */
DECL_FORCE_INLINE(void) iemRegPcInc(PVMCPUCC pVCpu, bool f32Bit = true) RT_NOEXCEPT
{
    if (!(ICORE(pVCpu).fExec & IEM_F_MODE_ARM_32BIT))
    {
        Assert(f32Bit);
        pVCpu->cpum.GstCtx.Pc.u64 = pVCpu->cpum.GstCtx.Pc.u64 + 4;
    }
    else
    {
        Assert(f32Bit || (ICORE(pVCpu).fExec & IEM_F_MODE_ARM_T32));
        pVCpu->cpum.GstCtx.Pc.u64 = pVCpu->cpum.GstCtx.Pc.u32 + (f32Bit ? 4 : 2);
    }
}


/**
 * Updates the PC to point to the next instruction in aarch64 mode.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 */
DECL_FORCE_INLINE(void) iemRegPcA64Inc(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    pVCpu->cpum.GstCtx.Pc.u64 += 4;
}


/**
 * Updates the PC to point to the next instruction in aarch32 mode.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 */
DECL_FORCE_INLINE(void) iemRegPcA32Inc(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    pVCpu->cpum.GstCtx.Pc.u64 = pVCpu->cpum.GstCtx.Pc.u32 + 4;
}


/**
 * Updates the PC to point to the next instruction in T32 mode.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 * @param   f32Bit  Set if it's a 32-bit wide instruction.
 */
DECL_FORCE_INLINE(void) iemRegPcT32Inc(PVMCPUCC pVCpu, bool f32Bit = false) RT_NOEXCEPT
{
    pVCpu->cpum.GstCtx.Pc.u64 = pVCpu->cpum.GstCtx.Pc.u32 + (!f32Bit ? 2 : 4);
}


/**
 * Adds a signed values to PC.
 *
 * This is the generic version used by code that isn't mode specific.  Code that
 * is only used in aarch64, aarch32 and t32 should call the specific versions
 * below.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   offAddend   What to add to PC.
 * @see     iemRegPcA64Add, iemRegPcA32Add, iemRegPcT32Add
 */
DECL_FORCE_INLINE(void) iemRegPcAdd(PVMCPUCC pVCpu, int32_t offAddend) RT_NOEXCEPT
{
    if (!(ICORE(pVCpu).fExec & IEM_F_MODE_ARM_32BIT))
        pVCpu->cpum.GstCtx.Pc.u64 = pVCpu->cpum.GstCtx.Pc.u64 + offAddend;
    else
        pVCpu->cpum.GstCtx.Pc.u64 = (uint32_t)(pVCpu->cpum.GstCtx.Pc.u32 + offAddend);
}


/**
 * Updates the PC to point to the next instruction in aarch64 mode.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   offAddend   What to add to PC.
 */
DECL_FORCE_INLINE(void) iemRegPcA64Add(PVMCPUCC pVCpu, int32_t offAddend) RT_NOEXCEPT
{
    pVCpu->cpum.GstCtx.Pc.u64 = pVCpu->cpum.GstCtx.Pc.u64 + offAddend;
}


/**
 * Updates the PC to point to the next instruction in aarch32 mode.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   offAddend   What to add to PC.
 */
DECL_FORCE_INLINE(void) iemRegPcA32Add(PVMCPUCC pVCpu, int32_t offAddend) RT_NOEXCEPT
{
    pVCpu->cpum.GstCtx.Pc.u64 = (uint32_t)(pVCpu->cpum.GstCtx.Pc.u32 + offAddend);
}


/**
 * Updates the PC to point to the next instruction in T32 mode.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   offAddend   What to add to PC.
 */
DECL_FORCE_INLINE(void) iemRegPcT32Add(PVMCPUCC pVCpu, int32_t offAddend) RT_NOEXCEPT
{
    pVCpu->cpum.GstCtx.Pc.u64 = (uint32_t)(pVCpu->cpum.GstCtx.Pc.u32 + offAddend);
}


/**
 * Gets the exeception level that debug exceptions are routed to.
 *
 * @returns Exception level.
 * @param   pVCpu   The cross context virtual CPU structure of the calling thread.
 */
DECL_FORCE_INLINE(uint8_t) iemGetDebugExceptionLevel(PVMCPUCC pVCpu)
{
    /** @todo EL3 */
    if (   (pVCpu->cpum.GstCtx.MdcrEl2.u64 & ARMV8_MDCR_EL2_TDE)
        || (pVCpu->cpum.GstCtx.HcrEl2.u64  & ARMV8_HCR_EL2_TGE))
        return 2;
    return 1;
}


/**
 * Called to handle software step when retiring an instruction.
 *
 * This is only called when IEM_F_ARM_SOFTWARE_STEP is set.
 */
static VBOXSTRICTRC iemFinishInstructionWithSoftwareStep(PVMCPUCC pVCpu, int rcNormal) RT_NOEXCEPT
{
    /*
     * The ARMV8_MDSCR_EL1_AARCH64_SS flag must be set.  Any instruction
     * modifying it will recalculate fExec, so we shouldn't get here.
     */
    Assert(pVCpu->cpum.GstCtx.Mdscr.u64 & ARMV8_MDSCR_EL1_AARCH64_SS);

    /*
     * Clear the PSTATE.SS.
     */
    pVCpu->cpum.GstCtx.fPState &= ~ARMV8_SPSR_EL2_AARCH64_SS;
    /** @todo guess IEM_F_ARM_SOFTWARE_STEP shouldn't be cleared till
     *        ARMV8_MDSCR_EL1_AARCH64_SS is cleared... Re-check that against
     *        docs and what guests actually does. */

    /*
     * Check if we can raise the exception.
     */
    /** @todo The documentation (D2.3.1) seems to indicate that PSTATE.D=1 won't
     *        mask software step debug exceptions when bCurEl is less than bDebugEl.
     *        mask SS.  But I find that hard to believe... */
    if (!(pVCpu->cpum.GstCtx.fPState & ARMV8_SPSR_EL2_AARCH64_D))
    {
        uint8_t const bDebugEl = iemGetDebugExceptionLevel(pVCpu);
        uint8_t const bCurEl   = IEM_F_MODE_ARM_GET_EL(ICORE(pVCpu).fExec);
        if (   bCurEl < bDebugEl
            || (   bCurEl == bDebugEl
                && (pVCpu->cpum.GstCtx.Mdscr.u64 & ARMV8_MDSCR_EL1_AARCH64_KDE)) )
        {
            LogFlowFunc(("Guest debug exception/software step at %016llx\n", pVCpu->cpum.GstCtx.Pc.u64));
            /** @todo iemRaiseDebugException(pVCpu, xxx);   */
            AssertFailedReturn(VERR_IEM_ASPECT_NOT_IMPLEMENTED);
        }
    }
    return rcNormal;
}


/**
 * Deals with PSTATE.SS as necessary, maybe raising debug exception.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegFinishClearingFlags(PVMCPUCC pVCpu, int rcNormal) RT_NOEXCEPT
{
    /*
     * We assume that most of the time nothing actually needs doing here.
     */
    if (RT_LIKELY(!(ICORE(pVCpu).fExec & IEM_F_ARM_SOFTWARE_STEP)))
        return rcNormal;
    return iemFinishInstructionWithSoftwareStep(pVCpu, rcNormal);
}


/**
 * Updates the PC to point to the next instruction and deals with PSTATE.SS.
 *
 * This is the generic version used by code that isn't mode specific.  Code that
 * is only used in aarch64, aarch32 and t32 should call the specific versions
 * below.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   f32Bit              Set if it's a 32-bit wide instruction (default).
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegPcIncAndFinishClearingFlagsEx(PVMCPUCC pVCpu, bool f32Bit = true, int rcNormal = VINF_SUCCESS) RT_NOEXCEPT
{
    iemRegPcInc(pVCpu, f32Bit);
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Updates the PC to point to the next AArch64 instruction (32-bit) for mode and
 * deals with PSTATE.SS.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegPcA64IncAndFinishingClearingFlags(PVMCPUCC pVCpu, int rcNormal = VINF_SUCCESS) RT_NOEXCEPT
{
    iemRegPcA64Inc(pVCpu);
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Updates the PC to point to the next AArch32 instruction (32-bit) for mode and
 * deals with PSTATE.SS.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegPcA32IncAndFinishingClearingFlags(PVMCPUCC pVCpu, int rcNormal = VINF_SUCCESS) RT_NOEXCEPT
{
    iemRegPcA32Inc(pVCpu);
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Updates the PC to point to the next thumb instruction (16-bit or 32-bit) for
 * mode and deals with PSTATE.SS.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   f32Bit              Set if it's a 32-bit wide instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegPcT32IncAndFinishingClearingFlags(PVMCPUCC pVCpu, bool f32Bit = false,
                                                                        int rcNormal = VINF_SUCCESS) RT_NOEXCEPT
{
    iemRegPcT32Inc(pVCpu, f32Bit);
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Tail method for a finish function that does't clear flags nor raises any
 * debug exceptions.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegFinishNoFlags(PVMCPUCC pVCpu, int rcNormal) RT_NOEXCEPT
{
    Assert(!(pVCpu->cpum.GstCtx.Mdscr.u64 & ARMV8_MDSCR_EL1_AARCH64_SS));
    Assert(!(ICORE(pVCpu).fExec & IEM_F_ARM_SOFTWARE_STEP));

    RT_NOREF(pVCpu);
    return rcNormal;
}


/**
 * Updates the PC to point to the next AArch64 instruction (32-bit) for mode,
 * skipping PSTATE.SS as it's assumed to be zero or otherwise left alone.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegPcA64IncAndFinishingNoFlags(PVMCPUCC pVCpu, int rcNormal) RT_NOEXCEPT
{
    iemRegPcA64Inc(pVCpu);
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Updates the PC to point to the next AArch32 instruction (32-bit) for mode,
 * skipping PSTATE.SS as it's assumed to be zero or otherwise left alone.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegPcA32IncAndFinishingNoFlags(PVMCPUCC pVCpu, int rcNormal) RT_NOEXCEPT
{
    iemRegPcA32Inc(pVCpu);
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Updates the PC to point to the next thumb instruction (16-bit or 32-bit) for
 * mode, skipping PSTATE.SS as it's assumed to be zero or otherwise left alone.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   f32Bit              Set if it's a 32-bit wide instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegPcT32IncAndFinishingNoFlags(PVMCPUCC pVCpu, bool f32Bit, int rcNormal) RT_NOEXCEPT
{
    iemRegPcT32Inc(pVCpu, f32Bit);
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


#if 0 /** @todo go over this later */

/**
 * Adds a 8-bit signed jump offset to RIP from 64-bit code.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   enmEffOpSize        Effective operand size.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegRip64RelativeJumpS8AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                                             IEMMODE enmEffOpSize, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));
    Assert(enmEffOpSize == IEMMODE_64BIT || enmEffOpSize == IEMMODE_16BIT);

    uint64_t uNewRip = pVCpu->cpum.GstCtx.rip + cbInstr + (int64_t)offNextInstr;
    if (enmEffOpSize == IEMMODE_16BIT)
        uNewRip &= UINT16_MAX;

    if (RT_LIKELY(IEM_IS_CANONICAL(uNewRip)))
        pVCpu->cpum.GstCtx.rip = uNewRip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to RIP from 64-bit code when the caller is
 * sure it stays within the same page.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   enmEffOpSize        Effective operand size.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRip64RelativeJumpS8IntraPgAndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                    IEMMODE enmEffOpSize, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));
    Assert(enmEffOpSize == IEMMODE_64BIT); RT_NOREF(enmEffOpSize);

    uint64_t const uNewRip = pVCpu->cpum.GstCtx.rip + cbInstr + (int64_t)offNextInstr;
    Assert((pVCpu->cpum.GstCtx.rip >> GUEST_PAGE_SHIFT) == (uNewRip >> GUEST_PAGE_SHIFT));
    pVCpu->cpum.GstCtx.rip = uNewRip;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to EIP, on 386 or later from 16-bit or 32-bit
 * code (never 64-bit).
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   enmEffOpSize        Effective operand size.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS8AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                                             IEMMODE enmEffOpSize, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);

    uint32_t uNewEip = pVCpu->cpum.GstCtx.eip + cbInstr + (int32_t)offNextInstr;
    if (enmEffOpSize == IEMMODE_16BIT)
        uNewEip &= UINT16_MAX;
    if (RT_LIKELY(uNewEip <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewEip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to EIP, on 386 or later from FLAT 32-bit code
 * (never 64-bit).
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   enmEffOpSize        Effective operand size.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
 iemRegEip32RelativeJumpS8FlatAndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                  IEMMODE enmEffOpSize, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);

    uint32_t uNewEip = pVCpu->cpum.GstCtx.eip + cbInstr + (int32_t)offNextInstr;
    if (enmEffOpSize == IEMMODE_16BIT)
        uNewEip &= UINT16_MAX;
    pVCpu->cpum.GstCtx.rip = uNewEip;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to IP, on a pre-386 CPU.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegIp16RelativeJumpS8AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                            int8_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));

    uint16_t const uNewIp = pVCpu->cpum.GstCtx.ip + cbInstr + (int16_t)offNextInstr;
    if (RT_LIKELY(uNewIp <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewIp;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to RIP from 64-bit code, no checking or
 * clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   enmEffOpSize        Effective operand size.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegRip64RelativeJumpS8AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                                          IEMMODE enmEffOpSize, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));
    Assert(enmEffOpSize == IEMMODE_64BIT || enmEffOpSize == IEMMODE_16BIT);

    uint64_t uNewRip = pVCpu->cpum.GstCtx.rip + cbInstr + (int64_t)offNextInstr;
    if (enmEffOpSize == IEMMODE_16BIT)
        uNewRip &= UINT16_MAX;

    if (RT_LIKELY(IEM_IS_CANONICAL(uNewRip)))
        pVCpu->cpum.GstCtx.rip = uNewRip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to RIP from 64-bit code when caller is sure
 * it stays within the same page, no checking or clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   enmEffOpSize        Effective operand size.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRip64RelativeJumpS8IntraPgAndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                 IEMMODE enmEffOpSize, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));
    Assert(enmEffOpSize == IEMMODE_64BIT); RT_NOREF(enmEffOpSize);

    uint64_t const uNewRip = pVCpu->cpum.GstCtx.rip + cbInstr + (int64_t)offNextInstr;
    Assert((pVCpu->cpum.GstCtx.rip >> GUEST_PAGE_SHIFT) == (uNewRip >> GUEST_PAGE_SHIFT));
    pVCpu->cpum.GstCtx.rip = uNewRip;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to EIP, on 386 or later from 16-bit or 32-bit
 * code (never 64-bit), no checking or clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   enmEffOpSize        Effective operand size.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS8AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                                                          IEMMODE enmEffOpSize, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);

    uint32_t uNewEip = pVCpu->cpum.GstCtx.eip + cbInstr + (int32_t)offNextInstr;
    if (enmEffOpSize == IEMMODE_16BIT)
        uNewEip &= UINT16_MAX;
    if (RT_LIKELY(uNewEip <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewEip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to EIP, on 386 or later from flat 32-bit code
 * (never 64-bit), no checking or clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   enmEffOpSize        Effective operand size.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegEip32RelativeJumpS8FlatAndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, int8_t offNextInstr,
                                              IEMMODE enmEffOpSize, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);

    uint32_t uNewEip = pVCpu->cpum.GstCtx.eip + cbInstr + (int32_t)offNextInstr;
    if (enmEffOpSize == IEMMODE_16BIT)
        uNewEip &= UINT16_MAX;
    pVCpu->cpum.GstCtx.rip = uNewEip;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 8-bit signed jump offset to IP, on a pre-386 CPU, no checking or
 * clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegIp16RelativeJumpS8AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                         int8_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));

    uint16_t const uNewIp = pVCpu->cpum.GstCtx.ip + cbInstr + (int16_t)offNextInstr;
    if (RT_LIKELY(uNewIp <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewIp;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 16-bit signed jump offset to RIP from 64-bit code.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegRip64RelativeJumpS16AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                              int16_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));

    pVCpu->cpum.GstCtx.rip = (uint16_t)(pVCpu->cpum.GstCtx.ip + cbInstr + offNextInstr);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 16-bit signed jump offset to EIP from 16-bit or 32-bit code.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 *
 * @note    This is also used by 16-bit code in pre-386 mode, as the code is
 *          identical.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS16AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                              int16_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));

    uint16_t const uNewIp = pVCpu->cpum.GstCtx.ip + cbInstr + offNextInstr;
    if (RT_LIKELY(uNewIp <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewIp;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 16-bit signed jump offset to EIP from FLAT 32-bit code.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 *
 * @note    This is also used by 16-bit code in pre-386 mode, as the code is
 *          identical.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS16FlatAndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                                  int16_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));

    uint16_t const uNewIp = pVCpu->cpum.GstCtx.ip + cbInstr + offNextInstr;
    pVCpu->cpum.GstCtx.rip = uNewIp;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 16-bit signed jump offset to RIP from 64-bit code, no checking or
 * clearing of flags.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegRip64RelativeJumpS16AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                           int16_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));

    pVCpu->cpum.GstCtx.rip = (uint16_t)(pVCpu->cpum.GstCtx.ip + cbInstr + offNextInstr);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 16-bit signed jump offset to EIP from 16-bit or 32-bit code,
 * no checking or clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 *
 * @note    This is also used by 16-bit code in pre-386 mode, as the code is
 *          identical.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS16AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                           int16_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));

    uint16_t const uNewIp = pVCpu->cpum.GstCtx.ip + cbInstr + offNextInstr;
    if (RT_LIKELY(uNewIp <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewIp;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 16-bit signed jump offset to EIP from FLAT 32-bit code, no checking or
 * clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 *
 * @note    This is also used by 16-bit code in pre-386 mode, as the code is
 *          identical.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS16FlatAndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                               int16_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));

    uint16_t const uNewIp = pVCpu->cpum.GstCtx.ip + cbInstr + offNextInstr;
    pVCpu->cpum.GstCtx.rip = uNewIp;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}

#endif


/**
 * Adds a 32-bit signed jump offset to PC from 64-bit code.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegPcA64RelativeJumpS32AndFinishClearingFlags(PVMCPUCC pVCpu, int32_t offNextInstr, int rcNormal = VINF_SUCCESS) RT_NOEXCEPT
{
    /** @todo set branch type / tracing. */
    iemRegPcA64Add(pVCpu, offNextInstr);
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


#if 0 /** @todo go over this later */
/**
 * Adds a 32-bit signed jump offset to PC from 64-bit code.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * We ASSUME that the effective operand size is 64-bit here, as 16-bit is the
 * only alternative for relative jumps in 64-bit code and that is already
 * handled in the decoder stage.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegRip64RelativeJumpS32AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                              int32_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));

    uint64_t const uNewRip = pVCpu->cpum.GstCtx.rip + cbInstr + (int64_t)offNextInstr;
    if (RT_LIKELY(IEM_IS_CANONICAL(uNewRip)))
        pVCpu->cpum.GstCtx.rip = uNewRip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 32-bit signed jump offset to RIP from 64-bit code when the caller is
 * sure the target is in the same page.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * We ASSUME that the effective operand size is 64-bit here, as 16-bit is the
 * only alternative for relative jumps in 64-bit code and that is already
 * handled in the decoder stage.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRip64RelativeJumpS32IntraPgAndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                     int32_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));

    uint64_t const uNewRip = pVCpu->cpum.GstCtx.rip + cbInstr + (int64_t)offNextInstr;
    Assert((pVCpu->cpum.GstCtx.rip >> GUEST_PAGE_SHIFT) == (uNewRip >> GUEST_PAGE_SHIFT));
    pVCpu->cpum.GstCtx.rip = uNewRip;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 32-bit signed jump offset to RIP from 64-bit code.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * We ASSUME that the effective operand size is 32-bit here, as 16-bit is the
 * only alternative for relative jumps in 32-bit code and that is already
 * handled in the decoder stage.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS32AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                              int32_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    Assert(pVCpu->cpum.GstCtx.rip <= UINT32_MAX);

    uint32_t const uNewEip = pVCpu->cpum.GstCtx.eip + cbInstr + offNextInstr;
    if (RT_LIKELY(uNewEip <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewEip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Adds a 32-bit signed jump offset to RIP from FLAT 32-bit code.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * We ASSUME that the effective operand size is 32-bit here, as 16-bit is the
 * only alternative for relative jumps in 32-bit code and that is already
 * handled in the decoder stage.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS32FlatAndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                                  int32_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    Assert(pVCpu->cpum.GstCtx.rip <= UINT32_MAX);

    uint32_t const uNewEip = pVCpu->cpum.GstCtx.eip + cbInstr + offNextInstr;
    pVCpu->cpum.GstCtx.rip = uNewEip;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif

    /*
     * Clear RF and finish the instruction (maybe raise #DB).
     */
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}



/**
 * Adds a 32-bit signed jump offset to RIP from 64-bit code, no checking or
 * clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * We ASSUME that the effective operand size is 64-bit here, as 16-bit is the
 * only alternative for relative jumps in 64-bit code and that is already
 * handled in the decoder stage.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegRip64RelativeJumpS32AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                           int32_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));

    uint64_t const uNewRip = pVCpu->cpum.GstCtx.rip + cbInstr + (int64_t)offNextInstr;
    if (RT_LIKELY(IEM_IS_CANONICAL(uNewRip)))
        pVCpu->cpum.GstCtx.rip = uNewRip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 32-bit signed jump offset to RIP from 64-bit code when the caller is
 * sure it stays within the same page, no checking or clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * We ASSUME that the effective operand size is 64-bit here, as 16-bit is the
 * only alternative for relative jumps in 64-bit code and that is already
 * handled in the decoder stage.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRip64RelativeJumpS32IntraPgAndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, int32_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));

    uint64_t const uNewRip = pVCpu->cpum.GstCtx.rip + cbInstr + (int64_t)offNextInstr;
    Assert((pVCpu->cpum.GstCtx.rip >> GUEST_PAGE_SHIFT) == (uNewRip >> GUEST_PAGE_SHIFT));
    pVCpu->cpum.GstCtx.rip = uNewRip;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 32-bit signed jump offset to RIP from 32-bit code, no checking or
 * clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * We ASSUME that the effective operand size is 32-bit here, as 16-bit is the
 * only alternative for relative jumps in 32-bit code and that is already
 * handled in the decoder stage.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS32AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                           int32_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    Assert(pVCpu->cpum.GstCtx.rip <= UINT32_MAX);

    uint32_t const uNewEip = pVCpu->cpum.GstCtx.eip + cbInstr + offNextInstr;
    if (RT_LIKELY(uNewEip <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewEip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Adds a 32-bit signed jump offset to RIP from FLAT 32-bit code, no checking or
 * clearing of flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * We ASSUME that the effective operand size is 32-bit here, as 16-bit is the
 * only alternative for relative jumps in 32-bit code and that is already
 * handled in the decoder stage.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             Instruction size.
 * @param   offNextInstr        The offset of the next instruction.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemRegEip32RelativeJumpS32FlatAndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr,
                                                                               int32_t offNextInstr, int rcNormal) RT_NOEXCEPT
{
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    Assert(pVCpu->cpum.GstCtx.rip <= UINT32_MAX);

    uint32_t const uNewEip = pVCpu->cpum.GstCtx.eip + cbInstr + offNextInstr;
    pVCpu->cpum.GstCtx.rip = uNewEip;

#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, rcNormal);
}


/**
 * Extended version of iemFinishInstructionWithFlagsSet that goes with
 * iemRegAddToRipAndFinishingClearingRfEx.
 *
 * See iemFinishInstructionWithFlagsSet() for details.
 */
static VBOXSTRICTRC iemFinishInstructionWithTfSet(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    /*
     * Raise a #DB.
     */
    IEM_CTX_IMPORT_RET(pVCpu, CPUMCTX_EXTRN_DR6);
    pVCpu->cpum.GstCtx.dr[6] &= ~X86_DR6_B_MASK;
    pVCpu->cpum.GstCtx.dr[6] |= X86_DR6_BS
                             | (   (pVCpu->cpum.GstCtx.eflags.uBoth & CPUMCTX_DBG_HIT_DRX_MASK_NONSILENT)
                                >> CPUMCTX_DBG_HIT_DRX_SHIFT);
    /** @todo Do we set all pending \#DB events, or just one? */
    LogFlowFunc(("Guest #DB fired at %04X:%016llX: DR6=%08X, RFLAGS=%16RX64 (popf)\n",
                 pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, (unsigned)pVCpu->cpum.GstCtx.dr[6],
                 pVCpu->cpum.GstCtx.rflags.uBoth));
    pVCpu->cpum.GstCtx.eflags.uBoth &= ~(X86_EFL_RF | CPUMCTX_INHIBIT_SHADOW | CPUMCTX_DBG_HIT_DRX_MASK | CPUMCTX_DBG_DBGF_MASK);
    return iemRaiseDebugException(pVCpu);
}


/**
 * Extended version of iemRegAddToRipAndFinishingClearingRF for use by POPF and
 * others potentially updating EFLAGS.TF.
 *
 * The single step event must be generated using the TF value at the start of
 * the instruction, not the new value set by it.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The number of bytes to add.
 * @param   fEflOld             The EFLAGS at the start of the instruction
 *                              execution.
 */
DECLINLINE(VBOXSTRICTRC) iemRegAddToRipAndFinishingClearingRfEx(PVMCPUCC pVCpu, uint8_t cbInstr, uint32_t fEflOld) RT_NOEXCEPT
{
    iemRegAddToRip(pVCpu, cbInstr);
    if (!(fEflOld & X86_EFL_TF))
    {
        /* Specialized iemRegFinishClearingFlags edition here that doesn't check X86_EFL_TF. */
        AssertCompile(CPUMCTX_INHIBIT_SHADOW < UINT32_MAX);
        if (RT_LIKELY(!(  pVCpu->cpum.GstCtx.eflags.uBoth
                        & (X86_EFL_RF | CPUMCTX_INHIBIT_SHADOW | CPUMCTX_DBG_HIT_DRX_MASK | CPUMCTX_DBG_DBGF_MASK)) ))
            return VINF_SUCCESS;
        return iemFinishInstructionWithFlagsSet<0 /*a_fTF*/>(pVCpu, VINF_SUCCESS); /* TF=0, so ignore it.  */
    }
    return iemFinishInstructionWithTfSet(pVCpu);
}


#ifndef IEM_WITH_OPAQUE_DECODER_STATE
/**
 * Updates the RIP/EIP/IP to point to the next instruction and clears EFLAGS.RF.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECLINLINE(VBOXSTRICTRC) iemRegUpdateRipAndFinishClearingRF(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    return iemRegAddToRipAndFinishingClearingRF(pVCpu, IEM_GET_INSTR_LEN(pVCpu));
}
#endif


#ifdef IEM_WITH_CODE_TLB_IN_CUR_CTX

/**
 * Performs a near jump to the specified address, no checking or clearing of
 * flags
 *
 * May raise a \#GP(0) if the new IP outside the code segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   uNewIp              The new IP value.
 */
DECLINLINE(VBOXSTRICTRC) iemRegRipJumpU16AndFinishNoFlags(PVMCPUCC pVCpu, uint16_t uNewIp) RT_NOEXCEPT
{
    if (RT_LIKELY(   uNewIp <= pVCpu->cpum.GstCtx.cs.u32Limit
                  || IEM_IS_64BIT_CODE(pVCpu) /* no limit checks in 64-bit mode */))
        pVCpu->cpum.GstCtx.rip = uNewIp;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Performs a near jump to the specified address, no checking or clearing of
 * flags
 *
 * May raise a \#GP(0) if the new RIP is outside the code segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   uNewEip             The new EIP value.
 */
DECLINLINE(VBOXSTRICTRC) iemRegRipJumpU32AndFinishNoFlags(PVMCPUCC pVCpu, uint32_t uNewEip) RT_NOEXCEPT
{
    Assert(pVCpu->cpum.GstCtx.rip <= UINT32_MAX);
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    if (RT_LIKELY(uNewEip <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewEip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Performs a near jump to the specified address, no checking or clearing of
 * flags.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   uNewRip             The new RIP value.
 */
DECLINLINE(VBOXSTRICTRC) iemRegRipJumpU64AndFinishNoFlags(PVMCPUCC pVCpu, uint64_t uNewRip) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));
    if (RT_LIKELY(IEM_IS_CANONICAL(uNewRip)))
        pVCpu->cpum.GstCtx.rip = uNewRip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}

#endif /* IEM_WITH_CODE_TLB_IN_CUR_CTX */

/**
 * Performs a near jump to the specified address.
 *
 * May raise a \#GP(0) if the new IP outside the code segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   uNewIp              The new IP value.
 * @param   cbInstr             The instruction length, for flushing in the non-TLB case.
 */
DECLINLINE(VBOXSTRICTRC) iemRegRipJumpU16AndFinishClearingRF(PVMCPUCC pVCpu, uint16_t uNewIp, uint8_t cbInstr) RT_NOEXCEPT
{
    if (RT_LIKELY(   uNewIp <= pVCpu->cpum.GstCtx.cs.u32Limit
                  || IEM_IS_64BIT_CODE(pVCpu) /* no limit checks in 64-bit mode */))
        pVCpu->cpum.GstCtx.rip = uNewIp;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#else
    RT_NOREF_PV(cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Performs a near jump to the specified address.
 *
 * May raise a \#GP(0) if the new RIP is outside the code segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   uNewEip             The new EIP value.
 * @param   cbInstr             The instruction length, for flushing in the non-TLB case.
 */
DECLINLINE(VBOXSTRICTRC) iemRegRipJumpU32AndFinishClearingRF(PVMCPUCC pVCpu, uint32_t uNewEip, uint8_t cbInstr) RT_NOEXCEPT
{
    Assert(pVCpu->cpum.GstCtx.rip <= UINT32_MAX);
    Assert(!IEM_IS_64BIT_CODE(pVCpu));
    if (RT_LIKELY(uNewEip <= pVCpu->cpum.GstCtx.cs.u32Limit))
        pVCpu->cpum.GstCtx.rip = uNewEip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#else
    RT_NOREF_PV(cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Performs a near jump to the specified address.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   uNewRip             The new RIP value.
 * @param   cbInstr             The instruction length, for flushing in the non-TLB case.
 */
DECLINLINE(VBOXSTRICTRC) iemRegRipJumpU64AndFinishClearingRF(PVMCPUCC pVCpu, uint64_t uNewRip, uint8_t cbInstr) RT_NOEXCEPT
{
    Assert(IEM_IS_64BIT_CODE(pVCpu));
    if (RT_LIKELY(IEM_IS_CANONICAL(uNewRip)))
        pVCpu->cpum.GstCtx.rip = uNewRip;
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#else
    RT_NOREF_PV(cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}

#endif



/**
 * Implements a 32-bit relative call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   offNextInstr        The 32-bit displacement.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegPcA64RelativeCallS32AndFinishClearingFlags(PVMCPUCC pVCpu, int32_t offNextInstr, int rcNormal = VINF_SUCCESS) RT_NOEXCEPT
{
    /** @todo tracing. */
    pVCpu->cpum.GstCtx.aGRegs[ARMV8_A64_REG_LR].x = pVCpu->cpum.GstCtx.Pc.u64 + 4;
    iemRegPcA64Add(pVCpu, offNextInstr);
    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}


/**
 * Implements a 64-bit indirect call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   uNewPc              The new PC value.
 * @param   uBType              The new PSTATE.BTYPE value.
 * @param   rcNormal            VINF_SUCCESS to continue TB.
 *                              VINF_IEM_REEXEC_BREAK to force TB exit when
 *                              taking the wrong conditional branch.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegPcA64BranchToAndFinishClearingFlags(PVMCPUCC pVCpu, uint64_t uNewPc, uint8_t uBType,
                                          int rcNormal = VINF_SUCCESS) RT_NOEXCEPT
{
    Assert(uBType < 4);
    /** @todo tracing. */

    pVCpu->cpum.GstCtx.Pc.u64 = uNewPc;
    if (pVCpu->pVMR3->cpum.ro.GuestFeatures.fBti)
        pVCpu->cpum.GstCtx.fPState = (pVCpu->cpum.GstCtx.fPState & ~ARMV8_SPSR_EL2_AARCH64_BTYPE)
                                   | ((uint64_t)uBType << ARMV8_SPSR_EL2_AARCH64_BTYPE_SHIFT);

    return iemRegFinishClearingFlags(pVCpu, rcNormal);
}

#if 0 /* later */


/**
 * Implements a 16-bit relative call, no checking or clearing of
 * flags.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   offDisp             The 16-bit displacement.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRipRelativeCallS16AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, int16_t offDisp) RT_NOEXCEPT
{
    uint16_t const uOldIp = pVCpu->cpum.GstCtx.ip + cbInstr;
    uint16_t const uNewIp = uOldIp + offDisp;
    if (   uNewIp <= pVCpu->cpum.GstCtx.cs.u32Limit
        || IEM_IS_64BIT_CODE(pVCpu) /* no CS limit checks in 64-bit mode */)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pVCpu, uOldIp);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewIp;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements a 16-bit relative call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   offDisp             The 16-bit displacement.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRipRelativeCallS16AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int16_t offDisp) RT_NOEXCEPT
{
    uint16_t const uOldIp = pVCpu->cpum.GstCtx.ip + cbInstr;
    uint16_t const uNewIp = uOldIp + offDisp;
    if (   uNewIp <= pVCpu->cpum.GstCtx.cs.u32Limit
        || IEM_IS_64BIT_CODE(pVCpu) /* no CS limit checks in 64-bit mode */)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pVCpu, uOldIp);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewIp;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements a 32-bit relative call, no checking or clearing of flags.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   offDisp             The 32-bit displacement.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegEip32RelativeCallS32AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, int32_t offDisp) RT_NOEXCEPT
{
    Assert(pVCpu->cpum.GstCtx.rip <= UINT32_MAX); Assert(!IEM_IS_64BIT_CODE(pVCpu));

    uint32_t const uOldRip = pVCpu->cpum.GstCtx.eip + cbInstr;
    uint32_t const uNewRip = uOldRip + offDisp;
    if (uNewRip <= pVCpu->cpum.GstCtx.cs.u32Limit)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU32(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements a 32-bit relative call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   offDisp             The 32-bit displacement.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegEip32RelativeCallS32AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int32_t offDisp) RT_NOEXCEPT
{
    Assert(pVCpu->cpum.GstCtx.rip <= UINT32_MAX); Assert(!IEM_IS_64BIT_CODE(pVCpu));

    uint32_t const uOldRip = pVCpu->cpum.GstCtx.eip + cbInstr;
    uint32_t const uNewRip = uOldRip + offDisp;
    if (uNewRip <= pVCpu->cpum.GstCtx.cs.u32Limit)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU32(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements a 64-bit relative call, no checking or clearing of flags.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   offDisp             The 64-bit displacement.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRip64RelativeCallS64AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, int64_t offDisp) RT_NOEXCEPT
{
    uint64_t const uOldRip = pVCpu->cpum.GstCtx.rip + cbInstr;
    uint64_t const uNewRip = uOldRip + (int64_t)offDisp;
    if (IEM_IS_CANONICAL(uNewRip))
    { /* likely */ }
    else
        return iemRaiseNotCanonical(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU64(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements a 64-bit relative call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   offDisp             The 64-bit displacement.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRip64RelativeCallS64AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, int64_t offDisp) RT_NOEXCEPT
{
    uint64_t const uOldRip = pVCpu->cpum.GstCtx.rip + cbInstr;
    uint64_t const uNewRip = uOldRip + (int64_t)offDisp;
    if (IEM_IS_CANONICAL(uNewRip))
    { /* likely */ }
    else
        return iemRaiseNotCanonical(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU64(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements an 16-bit indirect call, no checking or clearing of
 * flags.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   uNewRip             The new RIP value.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegIp16IndirectCallU16AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t uNewRip) RT_NOEXCEPT
{
    uint16_t const uOldRip = pVCpu->cpum.GstCtx.ip + cbInstr;
    if (uNewRip <= pVCpu->cpum.GstCtx.cs.u32Limit)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements an 16-bit indirect call, no checking or clearing of
 * flags.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   uNewRip             The new RIP value.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegEip32IndirectCallU16AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t uNewRip) RT_NOEXCEPT
{
    uint16_t const uOldRip = pVCpu->cpum.GstCtx.ip + cbInstr;
    if (uNewRip <= pVCpu->cpum.GstCtx.cs.u32Limit)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements an 16-bit indirect call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   uNewRip             The new RIP value.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegIp16IndirectCallU16AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t uNewRip) RT_NOEXCEPT
{
    uint16_t const uOldRip = pVCpu->cpum.GstCtx.ip + cbInstr;
    if (uNewRip <= pVCpu->cpum.GstCtx.cs.u32Limit)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements an 16-bit indirect call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   uNewRip             The new RIP value.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegEip32IndirectCallU16AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t uNewRip) RT_NOEXCEPT
{
    uint16_t const uOldRip = pVCpu->cpum.GstCtx.ip + cbInstr;
    if (uNewRip <= pVCpu->cpum.GstCtx.cs.u32Limit)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements an 32-bit indirect call, no checking or clearing of
 * flags.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   uNewRip             The new RIP value.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegEip32IndirectCallU32AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, uint32_t uNewRip) RT_NOEXCEPT
{
    uint32_t const uOldRip = pVCpu->cpum.GstCtx.eip + cbInstr;
    if (uNewRip <= pVCpu->cpum.GstCtx.cs.u32Limit)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU32(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements an 32-bit indirect call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   uNewRip             The new RIP value.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegEip32IndirectCallU32AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, uint32_t uNewRip) RT_NOEXCEPT
{
    uint32_t const uOldRip = pVCpu->cpum.GstCtx.eip + cbInstr;
    if (uNewRip <= pVCpu->cpum.GstCtx.cs.u32Limit)
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU32(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements an 64-bit indirect call, no checking or clearing of
 * flags.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   uNewRip             The new RIP value.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRip64IndirectCallU64AndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, uint64_t uNewRip) RT_NOEXCEPT
{
    uint64_t const uOldRip = pVCpu->cpum.GstCtx.rip + cbInstr;
    if (IEM_IS_CANONICAL(uNewRip))
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU64(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements an 64-bit indirect call.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbInstr             The instruction length.
 * @param   uNewRip             The new RIP value.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRip64IndirectCallU64AndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, uint64_t uNewRip) RT_NOEXCEPT
{
    uint64_t const uOldRip = pVCpu->cpum.GstCtx.rip + cbInstr;
    if (IEM_IS_CANONICAL(uNewRip))
    { /* likely */ }
    else
        return iemRaiseGeneralProtectionFault0(pVCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU64(pVCpu, uOldRip);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
        return rcStrict;

    pVCpu->cpum.GstCtx.rip = uNewRip;
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
}



/**
 * Adds to the stack pointer.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbToAdd             The number of bytes to add (8-bit!).
 */
DECLINLINE(void) iemRegAddToRsp(PVMCPUCC pVCpu, uint8_t cbToAdd) RT_NOEXCEPT
{
    if (IEM_IS_64BIT_CODE(pVCpu))
        pVCpu->cpum.GstCtx.rsp += cbToAdd;
    else if (pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
        pVCpu->cpum.GstCtx.esp += cbToAdd;
    else
        pVCpu->cpum.GstCtx.sp  += cbToAdd;
}


/**
 * Subtracts from the stack pointer.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbToSub             The number of bytes to subtract (8-bit!).
 */
DECLINLINE(void) iemRegSubFromRsp(PVMCPUCC pVCpu, uint8_t cbToSub) RT_NOEXCEPT
{
    if (IEM_IS_64BIT_CODE(pVCpu))
        pVCpu->cpum.GstCtx.rsp -= cbToSub;
    else if (pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
        pVCpu->cpum.GstCtx.esp -= cbToSub;
    else
        pVCpu->cpum.GstCtx.sp  -= cbToSub;
}


/**
 * Adds to the temporary stack pointer.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pTmpRsp             The temporary SP/ESP/RSP to update.
 * @param   cbToAdd             The number of bytes to add (16-bit).
 */
DECLINLINE(void) iemRegAddToRspEx(PCVMCPU pVCpu, PRTUINT64U pTmpRsp, uint16_t cbToAdd) RT_NOEXCEPT
{
    if (IEM_IS_64BIT_CODE(pVCpu))
        pTmpRsp->u           += cbToAdd;
    else if (pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
        pTmpRsp->DWords.dw0  += cbToAdd;
    else
        pTmpRsp->Words.w0    += cbToAdd;
}


/**
 * Subtracts from the temporary stack pointer.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pTmpRsp             The temporary SP/ESP/RSP to update.
 * @param   cbToSub             The number of bytes to subtract.
 * @remarks The @a cbToSub argument *MUST* be 16-bit, iemCImpl_enter is
 *          expecting that.
 */
DECLINLINE(void) iemRegSubFromRspEx(PCVMCPU pVCpu, PRTUINT64U pTmpRsp, uint16_t cbToSub) RT_NOEXCEPT
{
    if (IEM_IS_64BIT_CODE(pVCpu))
        pTmpRsp->u          -= cbToSub;
    else if (pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
        pTmpRsp->DWords.dw0 -= cbToSub;
    else
        pTmpRsp->Words.w0   -= cbToSub;
}


/**
 * Calculates the effective stack address for a push of the specified size as
 * well as the new RSP value (upper bits may be masked).
 *
 * @returns Effective stack addressf for the push.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbItem              The size of the stack item to pop.
 * @param   puNewRsp            Where to return the new RSP value.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPush(PCVMCPU pVCpu, uint8_t cbItem, uint64_t *puNewRsp) RT_NOEXCEPT
{
    RTUINT64U   uTmpRsp;
    RTGCPTR     GCPtrTop;
    uTmpRsp.u = pVCpu->cpum.GstCtx.rsp;

    if (IEM_IS_64BIT_CODE(pVCpu))
        GCPtrTop = uTmpRsp.u            -= cbItem;
    else if (pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
        GCPtrTop = uTmpRsp.DWords.dw0   -= cbItem;
    else
        GCPtrTop = uTmpRsp.Words.w0     -= cbItem;
    *puNewRsp = uTmpRsp.u;
    return GCPtrTop;
}


/**
 * Gets the current stack pointer and calculates the value after a pop of the
 * specified size.
 *
 * @returns Current stack pointer.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   cbItem              The size of the stack item to pop.
 * @param   puNewRsp            Where to return the new RSP value.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPop(PCVMCPU pVCpu, uint8_t cbItem, uint64_t *puNewRsp) RT_NOEXCEPT
{
    RTUINT64U   uTmpRsp;
    RTGCPTR     GCPtrTop;
    uTmpRsp.u = pVCpu->cpum.GstCtx.rsp;

    if (IEM_IS_64BIT_CODE(pVCpu))
    {
        GCPtrTop = uTmpRsp.u;
        uTmpRsp.u += cbItem;
    }
    else if (pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
    {
        GCPtrTop = uTmpRsp.DWords.dw0;
        uTmpRsp.DWords.dw0 += cbItem;
    }
    else
    {
        GCPtrTop = uTmpRsp.Words.w0;
        uTmpRsp.Words.w0 += cbItem;
    }
    *puNewRsp = uTmpRsp.u;
    return GCPtrTop;
}


/**
 * Calculates the effective stack address for a push of the specified size as
 * well as the new temporary RSP value (upper bits may be masked).
 *
 * @returns Effective stack addressf for the push.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pTmpRsp             The temporary stack pointer.  This is updated.
 * @param   cbItem              The size of the stack item to pop.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPushEx(PCVMCPU pVCpu, PRTUINT64U pTmpRsp, uint8_t cbItem) RT_NOEXCEPT
{
    RTGCPTR GCPtrTop;

    if (IEM_IS_64BIT_CODE(pVCpu))
        GCPtrTop = pTmpRsp->u          -= cbItem;
    else if (pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
        GCPtrTop = pTmpRsp->DWords.dw0 -= cbItem;
    else
        GCPtrTop = pTmpRsp->Words.w0   -= cbItem;
    return GCPtrTop;
}


/**
 * Gets the effective stack address for a pop of the specified size and
 * calculates and updates the temporary RSP.
 *
 * @returns Current stack pointer.
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 * @param   pTmpRsp             The temporary stack pointer.  This is updated.
 * @param   cbItem              The size of the stack item to pop.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPopEx(PCVMCPU pVCpu, PRTUINT64U pTmpRsp, uint8_t cbItem) RT_NOEXCEPT
{
    RTGCPTR GCPtrTop;
    if (IEM_IS_64BIT_CODE(pVCpu))
    {
        GCPtrTop = pTmpRsp->u;
        pTmpRsp->u          += cbItem;
    }
    else if (pVCpu->cpum.GstCtx.ss.Attr.n.u1DefBig)
    {
        GCPtrTop = pTmpRsp->DWords.dw0;
        pTmpRsp->DWords.dw0 += cbItem;
    }
    else
    {
        GCPtrTop = pTmpRsp->Words.w0;
        pTmpRsp->Words.w0   += cbItem;
    }
    return GCPtrTop;
}


/** Common body for iemRegRipNearReturnAndFinishClearingRF()
 * and iemRegRipNearReturnAndFinishNoFlags(). */
template<bool a_fWithFlags>
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRipNearReturnCommon(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t cbPop, IEMMODE enmEffOpSize) RT_NOEXCEPT
{
    /* Fetch the new RIP from the stack. */
    VBOXSTRICTRC    rcStrict;
    RTUINT64U       NewRip;
    RTUINT64U       NewRsp;
    NewRsp.u = pVCpu->cpum.GstCtx.rsp;
    switch (enmEffOpSize)
    {
        case IEMMODE_16BIT:
            NewRip.u = 0;
            rcStrict = iemMemStackPopU16Ex(pVCpu, &NewRip.Words.w0, &NewRsp);
            break;
        case IEMMODE_32BIT:
            NewRip.u = 0;
            rcStrict = iemMemStackPopU32Ex(pVCpu, &NewRip.DWords.dw0, &NewRsp);
            break;
        case IEMMODE_64BIT:
            rcStrict = iemMemStackPopU64Ex(pVCpu, &NewRip.u, &NewRsp);
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Check the new ew RIP before loading it. */
    /** @todo Should test this as the intel+amd pseudo code doesn't mention half
     *        of it.  The canonical test is performed here and for call. */
    if (enmEffOpSize != IEMMODE_64BIT)
    {
        if (RT_LIKELY(NewRip.DWords.dw0 <= pVCpu->cpum.GstCtx.cs.u32Limit))
        { /* likely */ }
        else
        {
            Log(("retn newrip=%llx - out of bounds (%x) -> #GP\n", NewRip.u, pVCpu->cpum.GstCtx.cs.u32Limit));
            return iemRaiseSelectorBounds(pVCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        }
    }
    else
    {
        if (RT_LIKELY(IEM_IS_CANONICAL(NewRip.u)))
        { /* likely */ }
        else
        {
            Log(("retn newrip=%llx - not canonical -> #GP\n", NewRip.u));
            return iemRaiseNotCanonical(pVCpu);
        }
    }

    /* Apply cbPop */
    if (cbPop)
        iemRegAddToRspEx(pVCpu, &NewRsp, cbPop);

    /* Commit it. */
    pVCpu->cpum.GstCtx.rip = NewRip.u;
    pVCpu->cpum.GstCtx.rsp = NewRsp.u;

    /* Flush the prefetch buffer. */
#ifndef IEM_WITH_CODE_TLB_IN_CUR_CTX
    iemOpcodeFlushLight(pVCpu, cbInstr);
#endif
    RT_NOREF(cbInstr);


    if (a_fWithFlags)
        return iemRegFinishClearingFlags(pVCpu, VINF_SUCCESS);
    return iemRegFinishNoFlags(pVCpu, VINF_SUCCESS);
}


/**
 * Implements retn and retn imm16.
 *
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling thread.
 * @param   cbInstr         The current instruction length.
 * @param   enmEffOpSize    The effective operand size.  This is constant.
 * @param   cbPop           The amount of arguments to pop from the stack
 *                          (bytes).  This can be constant (zero).
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRipNearReturnAndFinishClearingRF(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t cbPop, IEMMODE enmEffOpSize) RT_NOEXCEPT
{
    return iemRegRipNearReturnCommon<true /*a_fWithFlags*/>(pVCpu, cbInstr, cbPop, enmEffOpSize);
}


/**
 * Implements retn and retn imm16, no checking or clearing of
 * flags.
 *
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling thread.
 * @param   cbInstr         The current instruction length.
 * @param   enmEffOpSize    The effective operand size.  This is constant.
 * @param   cbPop           The amount of arguments to pop from the stack
 *                          (bytes).  This can be constant (zero).
 */
DECL_FORCE_INLINE(VBOXSTRICTRC)
iemRegRipNearReturnAndFinishNoFlags(PVMCPUCC pVCpu, uint8_t cbInstr, uint16_t cbPop, IEMMODE enmEffOpSize) RT_NOEXCEPT
{
    return iemRegRipNearReturnCommon<false /*a_fWithFlags*/>(pVCpu, cbInstr, cbPop, enmEffOpSize);
}

#endif /* go over this later */


#if 1
/* Mappings for A64 execution: */
# define iemRegPcIncAndFinishClearingFlags                  iemRegPcA64IncAndFinishingClearingFlags
# define iemRegPcRelativeJumpS32AndFinishClearingFlags      iemRegPcA64RelativeJumpS32AndFinishClearingFlags
# define iemRegPcRelativeCallS32AndFinishClearingFlags      iemRegPcA64RelativeCallS32AndFinishClearingFlags

#elif 0
/* Mappings for A32 execution: */
# define iemRegPcIncAndFinishClearingFlags                  iemRegPcA32IncAndFinishingClearingFlags

#elif 0
/* Mappings for T32 execution: */
# define iemRegPcIncAndFinishClearingFlags                  iemRegPcT32IncAndFinishingClearingFlags

#endif

/** @}  */



/** @name   FPU access and helpers.
 *
 * @{
 */


/**
 * Hook for preparing to use the host FPU.
 *
 * This is necessary in ring-0 and raw-mode context (nop in ring-3).
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECLINLINE(void) iemFpuPrepareUsage(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    IEM_CTX_IMPORT_NORET(pVCpu, CPUMCTX_EXTRN_V0_V31 | CPUMCTX_EXTRN_FPCR | CPUMCTX_EXTRN_FPSR);
#ifdef LOG_ENABLED
    pVCpu->iem.s.cLogFpuCountdown = RT_MAX(pVCpu->iem.s.cLogFpuCountdown, 4);
#endif
}


/**
 * Hook for actualizing the guest FPU state before the interpreter reads it.
 *
 * This is necessary in ring-0 and raw-mode context (nop in ring-3).
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECLINLINE(void) iemFpuActualizeStateForRead(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    IEM_CTX_IMPORT_NORET(pVCpu, CPUMCTX_EXTRN_V0_V31 | CPUMCTX_EXTRN_FPCR | CPUMCTX_EXTRN_FPSR);
#ifdef LOG_ENABLED
    pVCpu->iem.s.cLogFpuCountdown = RT_MAX(pVCpu->iem.s.cLogFpuCountdown, 2);
#endif
}


/**
 * Hook for actualizing the guest FPU state before the interpreter changes it.
 *
 * This is necessary in ring-0 and raw-mode context (nop in ring-3).
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling thread.
 */
DECLINLINE(void) iemFpuActualizeStateForChange(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    IEM_CTX_IMPORT_NORET(pVCpu, CPUMCTX_EXTRN_V0_V31 | CPUMCTX_EXTRN_FPCR | CPUMCTX_EXTRN_FPSR);
#ifdef LOG_ENABLED
    pVCpu->iem.s.cLogFpuCountdown = RT_MAX(pVCpu->iem.s.cLogFpuCountdown, 3);
#endif
}


/**
 * Fetches a register from the extension (floating point, SIMD) register bank
 * as 128-bit unsigned int.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 * @param   pu128Value  Where to store the value.
 */
DECL_FORCE_INLINE(void) iemFRegFetchU128(PVMCPUCC pVCpu, uint8_t iReg, PRTUINT128U pu128Value) RT_NOEXCEPT
{
    Assert(iReg < 32);
    *pu128Value = pVCpu->cpum.GstCtx.aVRegs[iReg].v;
}


/**
 * Fetches a register from the extension (floating point, SIMD) register bank
 * as 64-bit unsigned int.
 *
 * @returns The register value.
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 */
DECL_FORCE_INLINE(uint64_t) iemFRegFetchU64(PVMCPUCC pVCpu, uint8_t iReg) RT_NOEXCEPT
{
    Assert(iReg < 32);
    return pVCpu->cpum.GstCtx.aVRegs[iReg].v.QWords.qw0;
}


/**
 * Fetches the high 64-bits from a register in the extension (floating point,
 * SIMD) register bank as 64-bit unsigned int.
 *
 * @returns The register value.
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 */
DECL_FORCE_INLINE(uint64_t) iemFRegFetchHiU64(PVMCPUCC pVCpu, uint8_t iReg) RT_NOEXCEPT
{
    Assert(iReg < 32);
    return pVCpu->cpum.GstCtx.aVRegs[iReg].v.QWords.qw1;
}


/**
 * Fetches a register from the extension (floating point, SIMD) register bank
 * as 32-bit unsigned int.
 *
 * @returns The register value.
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 */
DECL_FORCE_INLINE(uint32_t) iemFRegFetchU32(PVMCPUCC pVCpu, uint8_t iReg) RT_NOEXCEPT
{
    Assert(iReg < 32);
    return pVCpu->cpum.GstCtx.aVRegs[iReg].v.DWords.dw0;
}


/**
 * Fetches a register from the extension (floating point, SIMD) register bank
 * as 16-bit unsigned int.
 *
 * @returns The register value.
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 */
DECL_FORCE_INLINE(uint16_t) iemFRegFetchU16(PVMCPUCC pVCpu, uint8_t iReg) RT_NOEXCEPT
{
    Assert(iReg < 32);
    return pVCpu->cpum.GstCtx.aVRegs[iReg].v.Words.w0;
}


/**
 * Fetches a register from the extension (floating point, SIMD) register bank
 * as 8-bit unsigned int.
 *
 * @returns The register value.
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 */
DECL_FORCE_INLINE(uint8_t) iemFRegFetchU8(PVMCPUCC pVCpu, uint8_t iReg) RT_NOEXCEPT
{
    Assert(iReg < 32);
#ifdef RT_BIG_ENDIAN
    return pVCpu->cpum.GstCtx.aVRegs[iReg].v.au8[7];
#else
    return pVCpu->cpum.GstCtx.aVRegs[iReg].v.au8[0];
#endif
}


/**
 * Store a 128-bit unsigned value in a register from the extension (floating
 * point, SIMD) register bank.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 * @param   pu128Value  The value to store.
 */
DECL_FORCE_INLINE(void) iemFRegStoreU128(PVMCPUCC pVCpu, uint8_t iReg, PCRTUINT128U pu128Value) RT_NOEXCEPT
{
    Assert(iReg < 32);
    pVCpu->cpum.GstCtx.aVRegs[iReg].v = *pu128Value;
}


/**
 * Store a 64-bit unsigned value in a register from the extension (floating
 * point, SIMD) register bank.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 * @param   u64Value    The value to store.
 * @note    Clears the upper bits.
 */
DECL_FORCE_INLINE(void) iemFRegStoreU64(PVMCPUCC pVCpu, uint8_t iReg, uint64_t u64Value) RT_NOEXCEPT
{
    Assert(iReg < 32);
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Lo = u64Value;
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Hi = 0;
}


/**
 * Store a 64-bit unsigned value in a register from the extension (floating
 * point, SIMD) register bank.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 * @param   u64Value    The value to store.
 * @note    Preserves the lower bits.
 */
DECL_FORCE_INLINE(void) iemFRegStoreHiU64(PVMCPUCC pVCpu, uint8_t iReg, uint64_t u64Value) RT_NOEXCEPT
{
    Assert(iReg < 32);
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Hi = u64Value;
}


/**
 * Store a 32-bit unsigned value in a register from the extension (floating
 * point, SIMD) register bank.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 * @param   u32Value    The value to store.
 * @note    Clears the upper bits.
 */
DECL_FORCE_INLINE(void) iemFRegStoreU32(PVMCPUCC pVCpu, uint8_t iReg, uint32_t u32Value) RT_NOEXCEPT
{
    Assert(iReg < 32);
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Lo = u32Value;
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Hi = 0;
}


/**
 * Store a 16-bit unsigned value in a register from the extension (floating
 * point, SIMD) register bank.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 * @param   u16Value    The value to store.
 * @note    Clears the upper bits.
 */
DECL_FORCE_INLINE(void) iemFRegStoreU16(PVMCPUCC pVCpu, uint8_t iReg, uint16_t u16Value) RT_NOEXCEPT
{
    Assert(iReg < 32);
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Lo = u16Value;
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Hi = 0;
}


/**
 * Store a 8-bit unsigned value in a register from the extension (floating
 * point, SIMD) register bank.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   iReg        The register.
 * @param   u8Value     The value to store.
 * @note    Clears the upper bits.
 */
DECL_FORCE_INLINE(void) iemFRegStoreU8(PVMCPUCC pVCpu, uint8_t iReg, uint8_t u8Value) RT_NOEXCEPT
{
    Assert(iReg < 32);
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Lo = u8Value;
    pVCpu->cpum.GstCtx.aVRegs[iReg].v.s.Hi = 0;
}

/** @}  */



/** @name Misc stuff
 * @{ */

/**
 * Gets the effective NV2:NV1:NV values from HCR_EL2.
 *
 * Helper used by the generated MRS code.
 */
DECLINLINE(uint32_t) iemGetEffHcrEl2NVx(PVMCPU pVCpu, const CPUMFEATURESARMV8 * const pGstFeats)
{
    if (   pGstFeats->fNv
        && pGstFeats->fEl2) /** @todo EL3 */
    {
        uint64_t fHcrEl2 = pVCpu->cpum.GstCtx.HcrEl2.u32;
        if (!pGstFeats->fE2H0)
            fHcrEl2 &= ~ARMV8_HCR_EL2_NV1;
        if (fHcrEl2 & ARMV8_HCR_EL2_NV)
        {
            if (!pGstFeats->fNv2)
                fHcrEl2 &= ~ARMV8_HCR_EL2_NV2;
            AssertCompile(ARMV8_HCR_EL2_NV_BIT + 1 == ARMV8_HCR_EL2_NV1_BIT);
            AssertCompile(ARMV8_HCR_EL2_NV_BIT + 3 == ARMV8_HCR_EL2_NV2_BIT);
            return ((fHcrEl2 >> ARMV8_HCR_EL2_NV_BIT) & 3)
                 | ((fHcrEl2 >> (ARMV8_HCR_EL2_NV2_BIT - 2)) & 4);
        }
        Assert(!(fHcrEl2 & ARMV8_HCR_EL2_NV1)); /* unpredictable, we pick 0 */
    }
    return 0;
}


/**
 * Gets the effective ACTLRMASK_EL1 value.
 */
DECLINLINE(uint64_t) iemGetEffActlrMaskEl1(PVMCPU pVCpu, const CPUMFEATURESARMV8 * const pGstFeats)
{
    /** @todo implement ACTLRMASK_EL1 & FEAT_SRMASK */
    Assert(!pGstFeats->fSrMask);
    RT_NOREF(pVCpu, pGstFeats);
    return 0;
}


/**
 * Gets the effective CPACRMASK_EL1 value.
 */
DECLINLINE(uint64_t) iemGetEffCpacrMaskEl1(PVMCPU pVCpu, const CPUMFEATURESARMV8 * const pGstFeats)
{
    /** @todo implement CPACRMASK_EL1 & FEAT_SRMASK */
    Assert(!pGstFeats->fSrMask);
    RT_NOREF(pVCpu, pGstFeats);
    return 0;
}


/**
 * Gets the effective CPTRMASK_EL2 value.
 */
DECLINLINE(uint64_t) iemGetEffCptrMaskEl2(PVMCPU pVCpu, const CPUMFEATURESARMV8 * const pGstFeats)
{
    /** @todo implement CPTRMASK_EL2 & FEAT_SRMASK */
    Assert(!pGstFeats->fSrMask);
    RT_NOREF(pVCpu, pGstFeats);
    return 0;
}


/**
 * Gets the effective SCTLRMASK_EL1 value.
 */
DECLINLINE(uint64_t) iemGetEffSctlrMaskEl1(PVMCPU pVCpu, const CPUMFEATURESARMV8 * const pGstFeats)
{
    /** @todo implement SCTLRMASK_EL1 & FEAT_SRMASK */
    Assert(!pGstFeats->fSrMask);
    RT_NOREF(pVCpu, pGstFeats);
    return 0;
}


/**
 * Gets the effective SCTLRMASK_EL2 value.
 */
DECLINLINE(uint64_t) iemGetEffSctlrMaskEl2(PVMCPU pVCpu, const CPUMFEATURESARMV8 * const pGstFeats)
{
    /** @todo implement SCTLRMASK_EL2 & FEAT_SRMASK */
    Assert(!pGstFeats->fSrMask);
    RT_NOREF(pVCpu, pGstFeats);
    return 0;
}


/**
 * Gets GCSCR_EL1/2/3.EXLOCKEN corresponding to PSTATE.EL.
 */
DECLINLINE(bool) iemGetCurrentExlockEn(PVMCPU pVCpu, const CPUMFEATURESARMV8 * const pGstFeats)
{
    /** @todo implement FEAT_GCS */
    Assert(!pGstFeats->fGcs);
    RT_NOREF(pVCpu, pGstFeats);
    return false;
}

/** @} */

#endif /* !VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInline_armv8_h */
