/* $Id: IEMCpuCoreTmpl.h 112438 2026-01-13 09:00:11Z knut.osmundsen@oracle.com $ */
/** @file
 * IEM - Core data structure template.
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


/**
 * The core per-CPU IEM state.
 */
typedef struct IEMCPUCORE
{
    /** Info status code that needs to be propagated to the IEM caller.
     * This cannot be passed internally, as it would complicate all success
     * checks within the interpreter making the code larger and almost impossible
     * to get right.  Instead, we'll store status codes to pass on here.  Each
     * source of these codes will perform appropriate sanity checks. */
    int32_t                 rcPassUp;                                                                       /* 0x00 */
    /** Execution flag, IEM_F_XXX. */
    uint32_t                fExec;                                                                          /* 0x04 */

    /** @name Decoder state.
     * @{ */
#ifdef IEMCPUCORE_WITH_CODE_TLB
    /** The offset of the next instruction byte. */
    uint32_t                offInstrNextByte;                                                               /* 0x08 */
# if defined(VBOX_VMM_TARGET_X86) || defined(DOXYGEN_RUNNING)
    /** X86: The number of bytes available at pbInstrBuf for the current
     * instruction. This takes the max opcode length into account so that doesn't
     * need to be checked separately. */
    uint32_t                cbInstrBuf;                                                                /* x86: 0x0c */
# else
    /** The number of bytes available at pbInstrBuf in total (for IEMExecLots).
     * @note Set to zero when the code TLB is flushed to trigger TLB reload. */
    uint32_t                cbInstrBufTotal;                                                          /* !x86: 0x0c */
# endif
    /** Pointer to the page containing PC, user specified buffer or abOpcode.
     * This can be NULL if the page isn't mappable for some reason, in which
     * case we'll do fallback stuff.
     *
     * If we're executing an instruction from a user specified buffer,
     * IEMExecOneWithPrefetchedByPC and friends, this is not necessarily a page
     * aligned pointer but pointer to the user data.
     *
     * @x86 For instructions crossing pages, this will start on the first page and
     * be advanced to the next page by the time we've decoded the instruction.  This
     * therefore precludes stuff like <tt>pbInstrBuf[offInstrNextByte + cbInstrBuf - cbCurInstr]</tt>
     */
    uint8_t const          *pbInstrBuf;                                                                     /* 0x10 */
# if ARCH_BITS == 32
    uint32_t                uInstrBufHigh; /** The high dword of the host context pbInstrBuf member. */
# endif
    /** The program counter corresponding to pbInstrBuf.
     * This is set to a non-canonical address when we need to invalidate it. */
    uint64_t                uInstrBufPc;                                                                    /* 0x18 */
    /** The guest physical address corresponding to pbInstrBuf. */
    RTGCPHYS                GCPhysInstrBuf;                                                                 /* 0x20 */
# if defined(VBOX_VMM_TARGET_X86) || defined(DOXYGEN_RUNNING)
    /** X86: The number of bytes available at pbInstrBuf in total (for IEMExecLots).
     * This takes the CS segment limit into account.
     * @note Set to zero when the code TLB is flushed to trigger TLB reload. */
    uint16_t                cbInstrBufTotal;                                                           /* x86: 0x28 */
    /** X86: Offset into pbInstrBuf of the first byte of the current instruction.
     * Can be negative to efficiently handle cross page instructions. */
    int16_t                 offCurInstrStart;                                                          /* x86: 0x2a */
# endif

# if (!defined(IEM_WITH_OPAQUE_DECODER_STATE) && defined(VBOX_VMM_TARGET_X86)) || defined(DOXYGEN_RUNNING)
    /** X86: The prefix mask (IEM_OP_PRF_XXX). */
    uint32_t                fPrefixes;                                                                 /* x86: 0x2c */
    /** X86: The extra REX ModR/M register field bit (REX.R << 3). */
    uint8_t                 uRexReg;                                                                   /* x86: 0x30 */
    /** X86: The extra REX ModR/M r/m field, SIB base and opcode reg bit
     * (REX.B << 3). */
    uint8_t                 uRexB;                                                                     /* x86: 0x31 */
    /** X86: The extra REX SIB index field bit (REX.X << 3). */
    uint8_t                 uRexIndex;                                                                 /* x86: 0x32 */

    /** X86: The effective segment register (X86_SREG_XXX). */
    uint8_t                 iEffSeg;                                                                   /* x86: 0x33 */

    /** X86: The offset of the ModR/M byte relative to the start of the instruction. */
    uint8_t                 offModRm;                                                                  /* x86: 0x34 */

#  ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    /** X86: The current offset into abOpcode. */
    uint8_t                 offOpcode;                                                                 /* x86: 0x35 */
#  else
    uint8_t                 bUnused;                                                                   /* x86: 0x35 */
#  endif
# else  /* IEM_WITH_OPAQUE_DECODER_STATE || !X86 */
#  ifdef VBOX_VMM_TARGET_X86
    uint8_t                 abOpaqueDecoderPart1[0x36 - 0x2c];
#  endif
# endif /* IEM_WITH_OPAQUE_DECODER_STATE || !X86 */

#else  /* !IEMCPUCORE_WITH_CODE_TLB */
#  ifndef IEM_WITH_OPAQUE_DECODER_STATE
    /** The size of what has currently been fetched into abOpcode. */
    uint8_t                 cbOpcode;                                                                       /*       0x08 */
    /** The current offset into abOpcode. */
    uint8_t                 offOpcode;                                                                      /*       0x09 */

#   ifdef VBOX_VMM_TARGET_X86
    /** X86: The offset of the ModR/M byte relative to the start of the
     *  instruction. */
    uint8_t                 offModRm;                                                                  /* x86:       0x0a */

    /** X86: The effective segment register (X86_SREG_XXX). */
    uint8_t                 iEffSeg;                                                                   /* x86:       0x0b */

    /** X86: The prefix mask (IEM_OP_PRF_XXX). */
    uint32_t                fPrefixes;                                                                 /* x86:       0x0c */
    /** X86: The extra REX ModR/M register field bit (REX.R << 3). */
    uint8_t                 uRexReg;                                                                   /* x86:       0x10 */
    /** X86: The extra REX ModR/M r/m field, SIB base and opcode reg bit
     * (REX.B << 3). */
    uint8_t                 uRexB;                                                                     /* x86:       0x11 */
    /** X86: The extra REX SIB index field bit (REX.X << 3). */
    uint8_t                 uRexIndex;                                                                 /* x86:       0x12 */
#   endif
# else  /* IEM_WITH_OPAQUE_DECODER_STATE */
#   ifdef VBOX_VMM_TARGET_X86
    uint8_t                 abOpaqueDecoderPart1[0x13 - 0x08];
#   else
    uint8_t                 abOpaqueDecoderPart1[2];
#   endif
# endif /* IEM_WITH_OPAQUE_DECODER_STATE */
#endif /* !IEMCPUCORE_WITH_CODE_TLB */

#if  (!defined(IEM_WITH_OPAQUE_DECODER_STATE) && (defined(VBOX_VMM_TARGET_X86) || !defined(IEMCPUCORE_WITH_CODE_TLB))) \
  || defined(DOXGYEN_RUNNING)
# ifdef VBOX_VMM_TARGET_X86
    /** X86: The effective operand mode. */
    IEMMODE                 enmEffOpSize;                                                              /* x86: 0x36, 0x13 */
    /** X86: The default addressing mode. */
    IEMMODE                 enmDefAddrMode;                                                            /* x86: 0x37, 0x14 */
    /** X86: The effective addressing mode. */
    IEMMODE                 enmEffAddrMode;                                                            /* x86: 0x38, 0x15 */
    /** X86: The default operand mode. */
    IEMMODE                 enmDefOpSize;                                                              /* x86: 0x39, 0x16 */

    /** X86: Prefix index (VEX.pp) for two byte and three byte tables. */
    uint8_t                 idxPrefix;                                                                 /* x86: 0x3a, 0x17 */
    /** X86: 3rd VEX/EVEX/XOP register.
     * Please use IEM_GET_EFFECTIVE_VVVV to access.  */
    uint8_t                 uVex3rdReg;                                                                /* x86: 0x3b, 0x18 */
    /** X86: The VEX/EVEX/XOP length field. */
    uint8_t                 uVexLength;                                                                /* x86: 0x3c, 0x19 */
    /** X86: Additional EVEX stuff. */
    uint8_t                 fEvexStuff;                                                                /* x86: 0x3d, 0x1a */

#  ifndef IEMCPUCORE_WITH_CODE_TLB
    /** Explicit alignment padding. */
    uint8_t                 abAlignment2a[1];                                                          /* x86:       0x1b */
#  endif
    /** X86: The FPU opcode (FOP). */
    uint16_t                uFpuOpcode;                                                                /* x86: 0x3e, 0x1c */
#  ifndef IEMCPUCORE_WITH_CODE_TLB
    /** Opcode buffer alignment padding. */
    uint8_t                 abAlignment2b[2];                                                          /* x86:       0x1e */
#  endif
# else  /* !VBOX_VMM_TARGET_X86 */
    /** Opcode buffer alignment padding. */
    uint8_t                 abAlignment2b[2];                                                         /* !x86:       0x0a */
# endif /* !VBOX_VMM_TARGET_X86 */

    /** The opcode bytes. */
# ifdef VBOX_VMM_TARGET_X86
    uint8_t                 abOpcode[15];                                                              /* x86: 0x40, 0x20 */
# else
    union
    {
        uint8_t             abOpcode[  28];                                                           /* !x86:       0x0c(..0x28) */
        uint16_t            au16Opcode[14];
        uint32_t            au32Opcode[ 7];
    };
# endif
# ifdef VBOX_VMM_TARGET_X86
    /** X86: Explicit alignment padding. */
#  ifdef IEMCPUCORE_WITH_CODE_TLB
    //uint8_t                 abAlignment2c[0x4f - 0x4f];                                              /* x86: 0x4f */
#  else
    uint8_t                 abAlignment2c[0x4f - 0x2f];                                                /* x86:       0x2f */
#  endif
# elif !defined(VBOX_VMM_TARGET_ARMV8)
    uint8_t                 abAlignment2c[0x4f - 0x28];                                         /* !x86: !arm:       0x28 */
# endif

#else  /* IEM_WITH_OPAQUE_DECODER_STATE || (!x86 && TLB) */
# ifdef IEMCPUCORE_WITH_CODE_TLB
#  ifdef VBOX_VMM_TARGET_X86
    uint8_t                 abOpaqueDecoderPart2[0x4f - 0x36];
#  elif !defined(VBOX_VMM_TARGET_ARMV8)
    uint8_t                 abOpaqueDecoderPart2[0x4f - 0x28];
#  endif
# else
#  ifdef VBOX_VMM_TARGET_X86
    uint8_t                 abOpaqueDecoderPart2[0x4f - 0x13];
#  elif defined(VBOX_VMM_TARGET_ARMV8)
    uint8_t                 abOpaqueDecoderPart2[0x28 - 0x0a];
#  else
    uint8_t                 abOpaqueDecoderPart2[0x4f - 0x09];
#  endif
# endif
#endif /* IEM_WITH_OPAQUE_DECODER_STATE */
    /** @} */

#  ifdef VBOX_VMM_TARGET_ARMV8
    RTGCPTR                 GCPtrEffT0SzAndMask;                                                       /* arm: 0x28 */
    RTGCPTR                 GCPtrEffT1SzOrMask;                                                        /* arm: 0x30 */
    uint8_t                 abPadding3[0x4f - 0x38];                                                   /* arm: 0x38 */
#  endif

    /** The number of active guest memory mappings. */
    uint8_t                 cActiveMappings;                                                                /* 0x4f, 0x4f */

    /** Records for tracking guest memory mappings. */
    struct
    {
        /** The address of the mapped bytes. */
        R3R0PTRTYPE(void *) pv;
        /** The access flags (IEM_ACCESS_XXX).
         * IEM_ACCESS_INVALID if the entry is unused. */
        uint32_t            fAccess;
        uint32_t            u32Alignment4; /**< Alignment padding. */
    } aMemMappings[IEM_MAX_MEM_MAPPINGS];                                           /* arm: 0x50 LB 0x20  x86: 0x50 LB 0x30 */

    /** Locking records for the mapped memory. */
    union
    {
        PGMPAGEMAPLOCK      Lock;
        uint64_t            au64Padding[2];
    } aMemMappingLocks[IEM_MAX_MEM_MAPPINGS];                                       /* arm: 0x70 LB 0x20  x86: 0x80 LB 0x30 */

    /** Bounce buffer info.
     * This runs in parallel to aMemMappings. */
    struct
    {
        /** The physical address of the first byte. */
        RTGCPHYS            GCPhysFirst;
        /** The physical address of the second page. */
        RTGCPHYS            GCPhysSecond;
        /** The number of bytes in the first page. */
        uint16_t            cbFirst;
        /** The number of bytes in the second page. */
        uint16_t            cbSecond;
        /** Whether it's unassigned memory. */
        bool                fUnassigned;
        /** Explicit alignment padding. */
        bool                afAlignment5[3];
    } aMemBbMappings[IEM_MAX_MEM_MAPPINGS];                                         /* arm: 0x90 LB 0x30  x86: 0xb0 LB 0x48 */

    /** The flags of the current exception / interrupt.
     * @note X86 specific? */
    uint32_t                fCurXcpt;                                               /* arm: 0xc0          x86: 0xf8 */
    /** The current exception / interrupt.
     *@note X86 specific? */
    uint8_t                 uCurXcpt;                                               /* arm: 0xc4          x86: 0xfc */
    /** Exception / interrupt recursion depth.
     *@note X86 specific? */
    int8_t                  cXcptRecursions;                                        /* arm: 0xc5          x86: 0xfb */

    /** The next unused mapping index.
     * @todo try find room for this up with cActiveMappings. */
    uint8_t                 iNextMapping;                                           /* arm: 0xc6          x86: 0xfd */
    uint8_t                 abAlignment7[IEM_MAX_MEM_MAPPINGS == 3 ? 1 : 0x39];

    /** Bounce buffer storage.
     * This runs in parallel to aMemMappings and aMemBbMappings. */
    struct
    {
        uint8_t             ab[IEM_BOUNCE_BUFFER_SIZE];
    } aBounceBuffers[IEM_MAX_MEM_MAPPINGS];                                        /* arm: 0x100 LB 0x80  x86: 0x100 LB 0x600 */


    /** Pointer set jump buffer - ring-3 context. */
    R3PTRTYPE(jmp_buf *)    pJmpBufR3;
    /** Pointer set jump buffer - ring-0 context. */
    R0PTRTYPE(jmp_buf *)    pJmpBufR0;

    /** @todo Should move this near @a fCurXcpt later. */
    /** The CR2 for the current exception / interrupt. */
    uint64_t                uCurXcptCr2;
    /** The error code for the current exception / interrupt. */
    uint32_t                uCurXcptErr;

    /** The number of potential exits. */
    uint32_t                cPotentialExits;

    /** @name Target CPU information.
     * @{ */
#if IEM_CFG_TARGET_CPU == IEMTARGETCPU_DYNAMIC
    /** The target CPU. */
    uint8_t                 uTargetCpu;
#else
    uint8_t                 bTargetCpuPadding;
#endif
    /** For selecting assembly works matching the target CPU EFLAGS behaviour, see
     * IEMTARGETCPU_EFL_BEHAVIOR_XXX for values, with the 1st entry for when no
     * native host support and the 2nd for when there is.
     *
     * The two values are typically indexed by a g_CpumHostFeatures bit.
     *
     * This is for instance used for the BSF & BSR instructions where AMD and
     * Intel CPUs produce different EFLAGS. */
    uint8_t                 aidxTargetCpuEflFlavour[2];

    /** The CPU vendor. */
    CPUMCPUVENDOR           enmCpuVendor;
    /** @} */
} IEMCPUCORE;


#undef IEMCPUCORE
#undef IEMCPUCORE_IN_RING0
#undef IEMCPUCORE_IN_RING3
#undef IEMCPUCORE_WITH_CODE_TLB

