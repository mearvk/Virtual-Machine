/* $Id: IEMMc-armv8.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * IEM - Interpreted Execution Manager - IEM_MC_XXX, ARMv8 target.
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

#ifndef VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMMc_armv8_h
#define VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMMc_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/** @name   "Microcode" macros, ARMv8 specifics and overrides.
 * @{
 */

 /** Set 64-bit PC to uAddr (local) and PSTATE.BTYPE to a_uBType (constant). */
#define IEM_MC_BRANCH_TO_WITH_BTYPE_AND_FINISH(a_uNewPc, a_uBType); \
    return iemRegPcA64BranchToAndFinishClearingFlags((pVCpu), (a_uNewPc), (a_uBType))

#define IEM_MC_A64_STORE_NZCV_CONST(a_uNzcv) \
    Assert(((a_uNzcv) & ~(uint64_t)UINT32_C(0xf0000000)) == 0); \
    pVCpu->cpum.GstCtx.fPState = (pVCpu->cpum.GstCtx.fPState & ~(uint64_t)UINT32_C(0xf0000000)) | (a_uNzcv)

#define IEM_MC_FETCH_GREG_SP_U32(a_u32Dst, a_iGReg)     (a_u32Dst) = iemGRegFetchU32(pVCpu, (a_iGReg), true /*fSp*/)
#define IEM_MC_FETCH_GREG_SP_U64(a_u64Dst, a_iGReg)     (a_u64Dst) = iemGRegFetchU64(pVCpu, (a_iGReg), true /*fSp*/)
#define IEM_MC_FETCH_GREG_SP_CHECK_ALIGN_U64(a_u64Dst, a_iGReg) \
    (a_u64Dst) = iemGRegFetchU64WithSpAlignCheck(pVCpu, (a_iGReg))

#define IEM_MC_STORE_GREG_SP_U32(a_iGReg, a_u32Value)   iemGRegStoreU32(pVCpu, (a_iGReg), (a_u32Value), true /*fSp*/)
#define IEM_MC_STORE_GREG_SP_U64(a_iGReg, a_u64Value)   iemGRegStoreU64(pVCpu, (a_iGReg), (a_u64Value), true /*fSp*/)

/** Fetches and extends the register value according to the extended SUB/ADD
 *  option and shift values. */
#define IEM_MC_FETCH_AND_SHIFT_GREG_U32(a_u32Dst, a_iGReg, a_u2Shift, a_cShiftCount) do { \
        (a_u32Dst) = iemGRegFetchU32(pVCpu, (a_iGReg), false /*fSp*/); \
        switch (a_u2Shift) \
        { \
            case 0: (a_u32Dst) <<= (a_cShiftCount);                              break; /* LSL */ \
            case 1: (a_u32Dst) >>= (a_cShiftCount);                              break; /* LSR */ \
            case 2: (a_u32Dst) = (int32_t)(a_u32Dst) >> (a_cShiftCount);         break; /* ASR */ \
            case 3: (a_u32Dst) = ASMRotateRightU32((a_u32Dst), (a_cShiftCount)); break; /* ROR */ \
        } \
    } while (0)

/** Fetches and extends the register value according to the extended SUB/ADD
 *  option and shift values. */
#define IEM_MC_FETCH_AND_SHIFT_GREG_U64(a_u64Dst, a_iGReg, a_u2Shift, a_cShiftCount) do { \
        (a_u64Dst) = iemGRegFetchU64(pVCpu, (a_iGReg), false /*fSp*/); \
        switch (a_u2Shift) \
        { \
            case 0: (a_u64Dst) <<= (a_cShiftCount);                              break; /* LSL */ \
            case 1: (a_u64Dst) >>= (a_cShiftCount);                              break; /* LSR */ \
            case 2: (a_u64Dst) = (int64_t)(a_u64Dst) >> (a_cShiftCount);         break; /* ASR */ \
            case 3: (a_u64Dst) = ASMRotateRightU64((a_u64Dst), (a_cShiftCount)); break; /* ROR */ \
        } \
    } while (0)

/** Fetches and extends the register value according to the extended SUB/ADD
 *  option and shift values. */
#define IEM_MC_FETCH_AND_EXTEND_GREG_U32(a_u32Dst, a_iGReg, a_u3ExtendOption, a_cShiftLeft) do { \
        (a_u32Dst) = iemGRegFetchU32(pVCpu, (a_iGReg), false /*fSp*/); \
        switch (a_u3ExtendOption) \
        { \
            case 2: /* nothing to do */                         break; /* UXTW/LSL */ \
            case 0: (a_u32Dst) = (uint8_t)(a_u32Dst);           break; /* UXTB */ \
            case 1: (a_u32Dst) = (uint16_t)(a_u32Dst);          break; /* UXTH */ \
            case 3: /* nothing to do */                         break; /* UXTX */ \
            case 4: (a_u32Dst) = (int32_t)(int8_t)(a_u32Dst);   break; /* SXTB */ \
            case 5: (a_u32Dst) = (int32_t)(int16_t)(a_u32Dst);  break; /* SXTH */ \
            case 6: /* nothing to do */                         break; /* SXTW */ \
            case 7: /* nothing to do */                         break; /* SXTX */ \
        } \
        (a_u32Dst) <<= (a_cShiftLeft); \
    } while (0)

/** Fetches and extends the register value according to the extended SUB/ADD
 *  option and shift values. */
#define IEM_MC_FETCH_AND_EXTEND_GREG_U64(a_u64Dst, a_iGReg, a_u3ExtendOption, a_cShiftLeft) do { \
        (a_u64Dst) = iemGRegFetchU64(pVCpu, (a_iGReg), false /*fSp*/); \
        switch (a_u3ExtendOption) \
        { \
            case 3: /* nothing to do */                         break; /* UXTX/LSL */ \
            case 0: (a_u64Dst) = (uint8_t)(a_u64Dst);           break; /* UXTB */ \
            case 1: (a_u64Dst) = (uint16_t)(a_u64Dst);          break; /* UXTH */ \
            case 2: (a_u64Dst) = (uint32_t)(a_u64Dst);          break; /* UXTW */ \
            case 4: (a_u64Dst) = (int64_t)(int8_t)(a_u64Dst);   break; /* SXTB */ \
            case 5: (a_u64Dst) = (int64_t)(int16_t)(a_u64Dst);  break; /* SXTH */ \
            case 6: (a_u64Dst) = (int64_t)(int32_t)(a_u64Dst);  break; /* SXTW */ \
            case 7: /* nothing to do */                         break; /* SXTX */ \
        } \
        (a_u64Dst) <<= (a_cShiftLeft); \
    } while (0)


/** Fetched PC (for PC relative addressing). */
#define IEM_MC_FETCH_PC_U64(a_GCPtrMem)  (a_GCPtrMem) = pVCpu->cpum.GstCtx.Pc.u64

/** Adds a constant offset to an address (64-bit), applying checked
 *  pointer arithmetic at the current EL. */
#define IEM_MC_ADD_CONST_U64_TO_ADDR(a_EffAddr, a_u64Const) do { \
        /*uint64_t const OldEffAddr = (a_EffAddr);*/ \
        (a_EffAddr) += (uint64_t)(a_u64Const); \
        AssertReturn(!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fCpa2, VERR_IEM_ASPECT_NOT_IMPLEMENTED); /** @todo CPA2 */ \
    } while (0)

/** Adds an offset from a local to an address (64-bit), applying checked
 *  pointer arithmetic at the current EL. */
#define IEM_MC_ADD_LOCAL_U64_TO_ADDR(a_EffAddr, a_u64Local) do { \
        /*uint64_t const OldEffAddr = (a_EffAddr);*/ \
        (a_EffAddr) += a_u64Local; \
        AssertReturn(!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fCpa2, VERR_IEM_ASPECT_NOT_IMPLEMENTED); /** @todo CPA2 */ \
    } while (0)


/** Prepares for using the FPU state.
 * Ensures that we can use the host FPU in the current context (RC+R0.
 * Ensures the guest FPU state in the CPUMCTX is up to date. */
#define IEM_MC_PREPARE_FPU_USAGE()              iemFpuPrepareUsage(pVCpu)
/** Actualizes the guest FPU state so it can be accessed read-only fashion. */
#define IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ()   iemFpuActualizeStateForRead(pVCpu)
/** Actualizes the guest FPU state so it can be accessed and modified. */
#define IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE() iemFpuActualizeStateForChange(pVCpu)

/** @todo AArch64.CheckFPAdvSIMDEnabled */
#define IEM_MC_A64_CHECK_FP_AND_ADV_SIMD_ENABLED() ((void)0)

#define IEM_MC_FETCH_FREG_HI_U64(a_u64Dst, a_iFpReg)    (a_u64Dst) = iemFRegFetchHiU64(pVCpu, (a_iFpReg))
#define IEM_MC_FETCH_FREG_HI_S64(a_i64Dst, a_iFpReg)    (a_i64Dst) = (int64_t)iemFRegFetchHiU64(pVCpu, (a_iFpReg))
#define IEM_MC_STORE_FREG_HI_U64(a_iFpReg, a_u64Value)  iemFRegStoreHiU64(pVCpu, (a_iFpReg), (a_u64Value))
#define IEM_MC_STORE_FREG_HI_S64(a_iFpReg, a_i64Value)  iemFRegStoreHiU64(pVCpu, (a_iFpReg), (a_i64Value))

#define IEM_MC_FETCH_MEM_FLAT_U32_PAIR(a_u32Value1, a_u32Value2, a_GCPtrMem) \
    (a_u32Value1) = iemMemFlatFetchDataPairU32Jmp(pVCpu, (a_GCPtrMem), &(a_u32Value2))
#define IEM_MC_FETCH_MEM_FLAT_U32_PAIR_SX_U64(a_u64Value1, a_u64Value2, a_GCPtrMem) \
    uint32_t       u32LdTmp2 = 0; \
    uint32_t const u32LdTmp1 = iemMemFlatFetchDataPairU32Jmp(pVCpu, (a_GCPtrMem), &u32LdTmp2); \
    (a_u64Value1) = (int64_t)(int32_t)u32LdTmp1; \
    (a_u64Value2) = (int64_t)(int32_t)u32LdTmp2
#define IEM_MC_FETCH_MEM_FLAT_U64_PAIR(a_u64Value1, a_u64Value2, a_GCPtrMem) \
    (a_u64Value1) = iemMemFlatFetchDataPairU64Jmp(pVCpu, (a_GCPtrMem), &(a_u64Value2))
#define IEM_MC_FETCH_MEM_FLAT_U128_PAIR(a_u128Value1, a_u128Value2, a_GCPtrMem) \
    iemMemFlatFetchDataPairU128Jmp(pVCpu, (a_GCPtrMem), &(a_u128Value1), &(a_u128Value2))

#define IEM_MC_STORE_MEM_FLAT_U32_PAIR(a_GCPtrMem, a_u32Value1, a_u32Value2) \
    iemMemFlatStoreDataPairU32Jmp(pVCpu, (a_GCPtrMem), (a_u32Value1), (a_u32Value2))
#define IEM_MC_STORE_MEM_FLAT_U64_PAIR(a_GCPtrMem, a_u64Value1, a_u64Value2) \
    iemMemFlatStoreDataPairU64Jmp(pVCpu, (a_GCPtrMem), (a_u64Value1), (a_u64Value2))
#define IEM_MC_STORE_MEM_FLAT_U128_PAIR(a_GCPtrMem, a_u128Value1, a_u128Value2) \
    iemMemFlatStoreDataPairU128Jmp(pVCpu, (a_GCPtrMem), &(a_u128Value1), &(a_u128Value2))



#define IEM_MC_A64_ADDS_U32(a_uSum, a_uAddend1, a_uAddend2, a_fCarry) \
    (a_uSum) = iemMcA64Adds<uint32_t, 32, a_fCarry>(a_uAddend1, a_uAddend2, &pVCpu->cpum.GstCtx.fPState)
#define IEM_MC_A64_ADDS_U64(a_uSum, a_uAddend1, a_uAddend2, a_fCarry) \
    (a_uSum) = iemMcA64Adds<uint64_t, 64, a_fCarry>(a_uAddend1, a_uAddend2, &pVCpu->cpum.GstCtx.fPState)

/** Helper that implements IEM_MC_A64_ADDS_U32 & IEM_MC_A64_ADDS_U64.
 * @todo not use if the a_fCarry approach is the best...  */
template<typename a_Type, unsigned const a_cBits, unsigned const a_fCarry>
DECL_FORCE_INLINE(a_Type) iemMcA64Adds(a_Type uAddend1, a_Type uAddend2, uint64_t *pfPState)
{
    a_Type const   uSum      = uAddend1 + uAddend2 + a_fCarry;
    uint64_t const fNewFlags = ((uSum >> (a_cBits - 1 - ARMV8_SPSR_EL2_AARCH64_N_BIT)) & ARMV8_SPSR_EL2_AARCH64_N)
                             | (uSum             ? 0 : ARMV8_SPSR_EL2_AARCH64_Z)
                             | ((!a_fCarry ? uSum >= uAddend2 : uSum > uAddend2) ? 0 : ARMV8_SPSR_EL2_AARCH64_C)
                             | (   (((~(uAddend1 ^ uAddend2) & (uSum ^ uAddend1)) >> (a_cBits - 1)) & 1)
                                << ARMV8_SPSR_EL2_AARCH64_V_BIT);
    *pfPState = (*pfPState & ~ARMV8_SPSR_EL2_AARCH64_NZCV) | fNewFlags;
    return uSum;
}


#define IEM_MC_A64_SUBS_U32(a_uDifference, a_uMinuend, a_uSubtrahend, a_fCarry) \
    (a_uDifference) = iemMcA64Subs<uint32_t, 32, a_fCarry>(a_uMinuend, a_uSubtrahend, &pVCpu->cpum.GstCtx.fPState)
#define IEM_MC_A64_SUBS_U64(a_uDifference, a_uMinuend, a_uSubtrahend, a_fCarry) \
    (a_uDifference) = iemMcA64Subs<uint64_t, 64, a_fCarry>(a_uMinuend, a_uSubtrahend, &pVCpu->cpum.GstCtx.fPState)

/** Helper that implements IEM_MC_A64_SUBS_U32 & IEM_MC_A64_SUBS_U64. */
template<typename a_Type, unsigned const a_cBits, unsigned const a_fCarry>
DECL_FORCE_INLINE(a_Type) iemMcA64Subs(a_Type uMinuend, a_Type uSubtrahend, uint64_t *pfPState)
{
    a_Type const   uDiff     = uMinuend - uSubtrahend - (a_fCarry ? 0U : 1U);
    uint64_t const fNewFlags = ((uDiff >> (a_cBits - 1 - ARMV8_SPSR_EL2_AARCH64_N_BIT)) & ARMV8_SPSR_EL2_AARCH64_N)
                             | (uDiff                  ? 0 : ARMV8_SPSR_EL2_AARCH64_Z)
                             | ((a_fCarry ? uMinuend < uSubtrahend : uMinuend <= uSubtrahend) ? 0 : ARMV8_SPSR_EL2_AARCH64_C) /* inverted for subtractions */
                             | (   ((((uMinuend ^ uSubtrahend) & (uDiff ^ uMinuend)) >> (a_cBits - 1)) & 1)
                                << ARMV8_SPSR_EL2_AARCH64_V_BIT);
    *pfPState = (*pfPState & ~ARMV8_SPSR_EL2_AARCH64_NZCV) | fNewFlags;
    return uDiff;
}


#define IEM_MC_A64_ANDS_U32(a_uDst, a_fMask) (a_uDst) = iemMcA64Ands<uint32_t>(a_uDst, a_fMask, &pVCpu->cpum.GstCtx.fPState)
#define IEM_MC_A64_ANDS_U64(a_uDst, a_fMask) (a_uDst) = iemMcA64Ands<uint64_t>(a_uDst, a_fMask, &pVCpu->cpum.GstCtx.fPState)

/** Helper that implements IEM_MC_A64_ANDS_U32 and IEM_MC_A64_ANDS_U64. */
template<typename a_Type>
DECL_FORCE_INLINE(a_Type) iemMcA64Ands(a_Type uValue, a_Type fMask, uint64_t *pfPState)
{
    uValue &= fMask;
    AssertCompile(sizeof(a_Type) * 8 - 1 >= ARMV8_SPSR_EL2_AARCH64_N_BIT);
    uint64_t const fNewFlags = ((uValue >> ((sizeof(a_Type) * 8 - 1) - ARMV8_SPSR_EL2_AARCH64_N_BIT)) & ARMV8_SPSR_EL2_AARCH64_N)
                             | (uValue ? 0 : ARMV8_SPSR_EL2_AARCH64_Z);
    *pfPState = (*pfPState & ~ARMV8_SPSR_EL2_AARCH64_NZCV) | fNewFlags;
    return uValue;
}


#define IEM_MC_A64_RBIT_LOCAL_U32(a_uValue)             (a_uValue) = iemMcA64RBitU32(a_uValue)
#define IEM_MC_A64_RBIT_LOCAL_U64(a_uValue)             (a_uValue) = iemMcA64RBitU64(a_uValue)

/** Helper that implements IEM_MC_A64_RBIT_LOCAL_U32 */
DECLINLINE(uint32_t) iemMcA64RBitU32(uint32_t uValue)
{
    /** @todo use the rbit instruction on arm64  */
    uValue = ((uValue & UINT32_C(0xaaaaaaaa)) >> 1) | ((uValue & UINT32_C(0x55555555)) << 1);
    uValue = ((uValue & UINT32_C(0xcccccccc)) >> 2) | ((uValue & UINT32_C(0x33333333)) << 2);
    uValue = ((uValue & UINT32_C(0xf0f0f0f0)) >> 4) | ((uValue & UINT32_C(0x0f0f0f0f)) << 4);
    //uValue = ((uValue & UINT32_C(0xff00ff00)) >> 8) | ((uValue & UINT32_C(0xff00ff00)) << 8);
    //return (uValue >> 16) | (uValue << 16);
    return ASMByteSwapU32(uValue);
}

/** Helper that implements IEM_MC_A64_RBIT_LOCAL_U64 */
DECLINLINE(uint64_t) iemMcA64RBitU64(uint64_t uValue)
{
    /** @todo use the rbit instruction on arm64  */
    uValue = ((uValue & UINT64_C(0xaaaaaaaaaaaaaaaa)) >>  1) | ((uValue & UINT64_C(0x5555555555555555)) <<  1);
    uValue = ((uValue & UINT64_C(0xcccccccccccccccc)) >>  2) | ((uValue & UINT64_C(0x3333333333333333)) <<  2);
    uValue = ((uValue & UINT64_C(0xf0f0f0f0f0f0f0f0)) >>  4) | ((uValue & UINT64_C(0x0f0f0f0f0f0f0f0f)) <<  4);
    //uValue = ((uValue & UINT64_C(0xff00ff00ff00ff00)) >>  8) | ((uValue & UINT64_C(0xff00ff00ff00ff00)) <<  8);
    //uValue = ((uValue & UINT64_C(0xffff0000ffff0000)) >> 16) | ((uValue & UINT64_C(0x0000ffff0000ffff)) << 16);
    //return (uValue >> 32) | (uValue << 32);
    return ASMByteSwapU64(uValue);
}


#define IEM_MC_A64_REV16_LOCAL_U32(a_uValue) \
    (a_uValue) = RT_MAKE_U32(ASMByteSwapU16(a_uValue), ASMByteSwapU16((a_uValue) >> 16))

#define IEM_MC_A64_REV16_LOCAL_U64(a_uValue) \
    (a_uValue) = RT_MAKE_U64_FROM_U16(ASMByteSwapU16(a_uValue), ASMByteSwapU16((a_uValue) >> 16), \
                                      ASMByteSwapU16((a_uValue) >> 32), ASMByteSwapU16((a_uValue) >> 48))

#define IEM_MC_A64_REV32_LOCAL_U64(a_uValue) \
    (a_uValue) = RT_MAKE_U64(ASMByteSwapU32(a_uValue), ASMByteSwapU32((a_uValue) >> 32))

#define IEM_MC_A64_CLZ_LOCAL_U32(a_uValue) \
    (a_uValue) = 32 - ASMBitLastSetU64(a_uValue)

#define IEM_MC_A64_CLZ_LOCAL_U64(a_uValue) \
    (a_uValue) = 64 - ASMBitLastSetU64(a_uValue)


#define IEM_MC_A64_CLS_LOCAL_U32(a_uValue)              (a_uValue) = iemMcA64ClsU32(a_uValue)
#define IEM_MC_A64_CLS_LOCAL_U64(a_uValue)              (a_uValue) = iemMcA64ClsU64(a_uValue)

/** Helper that implements IEM_MC_A64_CLS_LOCAL_U32. */
DECLINLINE(uint32_t) iemMcA64ClsU32(uint32_t uValue)
{
    if (uValue & RT_BIT_32(31))
        uValue = ~uValue;
    return 31 - ASMBitLastSetU64(uValue);
}

/** Helper that implements IEM_MC_A64_CLS_LOCAL_U64. */
DECLINLINE(uint64_t) iemMcA64ClsU64(uint64_t uValue)
{
    if (uValue & RT_BIT_64(63))
        uValue = ~uValue;
    return 63 - ASMBitLastSetU64(uValue);
}



/*
 * Synchronizing primitives and memory barriers.
 * Treat these as all being in the outer domain for now.
 */
#if defined(RT_ARCH_ARM64)
# if defined(_MSC_VER)
#  define IEM_MC_A64_DSB_READS()    __dsb(_ARM64_BARRIER_OSHLD)
#  define IEM_MC_A64_DSB_WRITES()   __dsb(_ARM64_BARRIER_OSHST)
#  define IEM_MC_A64_DSB_ALL()      __dsb(_ARM64_BARRIER_OSH)

#  define IEM_MC_A64_DMB_READS()    __dmb(_ARM64_BARRIER_OSHLD)
#  define IEM_MC_A64_DMB_WRITES()   __dmb(_ARM64_BARRIER_OSHST)
#  define IEM_MC_A64_DMB_ALL()      __dmb(_ARM64_BARRIER_OSH)
# else
#  define IEM_MC_A64_DSB_READS()    __asm__ __volatile__("dsb oshld")
#  define IEM_MC_A64_DSB_WRITES()   __asm__ __volatile__("dsb oshst")
#  define IEM_MC_A64_DSB_ALL()      __asm__ __volatile__("dsb osh")

#  define IEM_MC_A64_DMB_READS()    __asm__ __volatile__("dmb oshld")
#  define IEM_MC_A64_DMB_WRITES()   __asm__ __volatile__("dmb oshst")
#  define IEM_MC_A64_DMB_ALL()      __asm__ __volatile__("dmb osh")
# endif
#else
# define IEM_MC_A64_DSB_READS()     ASMReadFence()
# define IEM_MC_A64_DSB_WRITES()    ASMWriteFence()
# define IEM_MC_A64_DSB_ALL()       ASMMemoryFence()

# define IEM_MC_A64_DMB_READS()     ASMReadFence()
# define IEM_MC_A64_DMB_WRITES()    ASMWriteFence()
# define IEM_MC_A64_DMB_ALL()       ASMMemoryFence()
#endif

/** Instruction boundrary is a NOP in the interpreter for now. */
#define IEM_MC_A64_ISB()            ((void)0)


/*
 * Advanced SIMD.
 */

/** Element worker for SQADD - set FPSR.QC=1 on overflow. */
#define IEM_MC_SQADD_2LOCS_ELEM_S64(a_cBits, a_iResultAndLeftElem, a_iRightElem) \
    (a_iResultAndLeftElem) = iemMcA64SQAddElem<a_cBits, INT##a_cBits##_MAX, INT##a_cBits##_MIN>((a_iResultAndLeftElem), \
                                                                                                (a_iRightElem), \
                                                                                                &pVCpu->cpum.GstCtx.fpsr)

/** Helper that implements IEM_MC_SQADD_2LOCS_ELEM_S64. */
template<unsigned const a_cBits, int64_t const a_iMax, int64_t const a_iMin>
DECL_FORCE_INLINE(int64_t) iemMcA64SQAddElem(int64_t iAddend1, int64_t iAddend2, uint64_t *pfFpSr)
{
    int64_t iSum = iAddend1 + iAddend2;

    if (!((~((uint64_t)iAddend1 ^ (uint64_t)iAddend2) & ((uint64_t)iSum ^ (uint64_t)iAddend1)) >> (a_cBits - 1)))
        return iSum;

    /* Some kind of overflow. Since setting QC is cummulative and nothing else
       will abort the instruction, there isn't strictly a problem doing it
       before we've calculated everything... */
    *pfFpSr |= RT_BIT_32(27); /* QC */
    if RT_CONSTEXPR_IF(a_cBits < 64)
        iSum = iSum < 0 ? a_iMin : a_iMax;
    else
        iSum = iSum >= 0 ? a_iMin : a_iMax;
    return iSum;
}


/** Element worker for SQSUB - set FPSR.QC=1 on overflow. */
#define IEM_MC_SQSUB_2LOCS_ELEM_S64(a_cBits, a_iResultAndLeftElem, a_iRightElem) \
    (a_iResultAndLeftElem) = iemMcA64SQSubElem<a_cBits, INT##a_cBits##_MAX, INT##a_cBits##_MIN>((a_iResultAndLeftElem), \
                                                                                                (a_iRightElem), \
                                                                                                &pVCpu->cpum.GstCtx.fpsr)

/** Helper that implements IEM_MC_A64_SUBS_U32 & IEM_MC_A64_SUBS_U64. */
template<unsigned const a_cBits, int64_t const a_iMax, int64_t const a_iMin>
DECL_FORCE_INLINE(int64_t) iemMcA64SQSubElem(int64_t iMinuend, int64_t iSubtrahend, uint64_t *pfFpSr)
{
    int64_t iSum = iMinuend - iSubtrahend;

    if (!((((uint64_t)iMinuend ^ (uint64_t)iSubtrahend) & ((uint64_t)iSum ^ (uint64_t)iMinuend)) >> (a_cBits - 1)))
        return iSum;

    /* Some kind of overflow. Since setting QC is cummulative and nothing else
       will abort the instruction, there isn't strictly a problem doing it
       before we've calculated everything... */
    *pfFpSr |= RT_BIT_32(27); /* QC */
    if RT_CONSTEXPR_IF(a_cBits < 64)
        iSum = iSum < 0 ? a_iMin : a_iMax;
    else
        iSum = iSum >= 0 ? a_iMin : a_iMax;
    return iSum;
}


/** @}  */

#endif /* !VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMMc_armv8_h */

