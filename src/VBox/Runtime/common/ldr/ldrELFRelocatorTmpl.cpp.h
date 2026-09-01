/* $Id: ldrELFRelocatorTmpl.cpp.h 112510 2026-01-13 15:24:18Z knut.osmundsen@oracle.com $ */
/** @file
 * IPRT - Binary Image Loader, Template for ELF Relocatable Images.
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


#ifdef ELF_RELOCATOR_IS_REL
# define Elf_Reloc                      Elf_Rel
# define ELF_PICK_ADDEND(rela, rel)     (rel)
#else
# define Elf_Reloc                      Elf_Rela
# define ELF_PICK_ADDEND(rela, rel)     (rela)
#endif



/**
 * Applies the fixups for a section in an executable image.
 *
 * @returns iprt status code.
 * @param   pModElf         The ELF loader module instance data.
 * @param   BaseAddr        The base address which the module is being fixedup to.
 * @param   pfnGetImport    The callback function to use to resolve imports (aka unresolved externals).
 * @param   pvUser          User argument to pass to the callback.
 * @param   SecAddr         The section address. This is the address the relocations are relative to.
 * @param   cbSec           The section size. The relocations must be inside this.
 * @param   pu8SecBaseR     Where we read section bits from.
 * @param   pu8SecBaseW     Where we write section bits to.
 * @param   pvRelocs        Pointer to where we read the relocations from.
 * @param   cbRelocs        Size of the relocations.
 */
static int ELF_RELOCATOR_NAME(RelocateSectionExecDyn)(PRTLDRMODELF pModElf, Elf_Addr BaseAddr,
                                                      PFNRTLDRIMPORT pfnGetImport, void *pvUser,
                                                      const Elf_Addr SecAddr, Elf_Size cbSec,
                                                      const uint8_t *pu8SecBaseR, uint8_t *pu8SecBaseW,
                                                      const void *pvRelocs, Elf_Size cbRelocs)
{
#ifndef ELF_RELOCATOR_IS_REL
    NOREF(pu8SecBaseR);
#endif

    /*
     * Iterate the relocations.
     * The relocations are stored in an array of Elf32_Rel records and covers the entire relocation section.
     */
#ifdef ELF_RELOCATOR_IS_REL
    const Elf_Shdr   *pShdr    = pModElf->paShdrs;
# if   ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_X86
    const Elf_Addr    offDelta = BaseAddr - pModElf->LinkAddress;
# endif
#endif
    const Elf_Reloc  *paRels   = (const Elf_Reloc *)pvRelocs;
    const unsigned    iRelMax  = (unsigned)(cbRelocs / sizeof(paRels[0]));
    AssertMsgReturn(iRelMax == cbRelocs / sizeof(paRels[0]), (FMT_ELF_SIZE "\n", cbRelocs / sizeof(paRels[0])),
                    VERR_IMAGE_TOO_BIG);
    for (unsigned iRel = 0; iRel < iRelMax; iRel++)
    {
        /*
         * Apply fixups not taking a symbol (will 'continue' rather than 'break').
         */
        AssertMsgReturn(paRels[iRel].r_offset < cbSec, (FMT_ELF_ADDR " " FMT_ELF_SIZE "\n", paRels[iRel].r_offset, cbSec),
                        VERR_LDRELF_INVALID_RELOCATION_OFFSET);
#ifdef ELF_RELOCATOR_IS_REL
        if (paRels[iRel].r_offset - pShdr->sh_addr /*rva*/ >= pShdr->sh_size)
            pShdr = RTLDRELF_NAME(RvaToSectionHeader)(pModElf, paRels[iRel].r_offset);
        static const Elf_Addr s_uZero = 0;
        const Elf_Addr *pAddrR = RT_LIKELY(pShdr->sh_type != SHT_NOBITS)                     /* Where to read the addend. */
                               ? (const Elf_Addr *)(pu8SecBaseR + paRels[iRel].r_offset - pShdr->sh_addr /*rva*/
                                                    + pShdr->sh_offset)
                               : &s_uZero;
#endif
        Elf_Addr       *pAddrW =       (Elf_Addr *)(pu8SecBaseW + paRels[iRel].r_offset);    /* Where to write the fixup. */
        switch (ELF_R_TYPE(paRels[iRel].r_info))
        {
            /*
             * Image relative (addend + base).
             */
#if   ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_X86
            case R_386_RELATIVE:
            {
                const Elf_Addr Value = *pAddrR + BaseAddr;
                *(uint32_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_386_RELATIVE Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value));
                AssertCompile(sizeof(Value) == sizeof(uint32_t));
                continue;
            }
#elif ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_AMD64
            case R_X86_64_RELATIVE:
            {
                const Elf_Addr Value = paRels[iRel].r_addend + BaseAddr;
                *(uint64_t *)pAddrW = (uint64_t)Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_X86_64_RELATIVE Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value));
                AssertCompile(sizeof(Value) == sizeof(uint64_t));
                continue;
            }
#elif ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_ARM64
            case R_AARCH64_RELATIVE:
            {
                const Elf_Addr Value = BaseAddr + ELF_PICK_ADDEND(paRels[iRel].r_addend, *pAddrR);
                *(uint64_t *)pAddrW = (uint64_t)Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_AARCH64_RELATIVE Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value));
                AssertCompile(sizeof(Value) == sizeof(uint64_t));
                continue;
            }
#else
# error "port me"
#endif

            /*
             * R_XXX_NONE.
             */
            case ELF_RELOCATOR_R_XXX_NONE:
                continue;
        }

        /*
         * Validate and find the symbol, resolve undefined ones.
         */
        const Elf_Sym  *pSym = NULL; /* shut up gcc */
        Elf_Addr        SymValue = 0; /* shut up gcc-4 */
        int rc = RTLDRELF_NAME(SymbolExecDyn)(pModElf, BaseAddr, pfnGetImport, pvUser, ELF_R_SYM(paRels[iRel].r_info),
                                              &pSym, &SymValue);
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Apply the fixup.
         */
        switch (ELF_R_TYPE(paRels[iRel].r_info))
        {
#if   ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_X86
            /*
             * GOT/PLT.
             */
            case R_386_GLOB_DAT:
            {
                *(uint32_t *)pAddrW = (uint32_t)SymValue;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_386_GLOB_DAT Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, SymValue));
                AssertCompile(sizeof(SymValue) == sizeof(uint32_t));
                break;
            }

            case R_386_JMP_SLOT:
            {
                *(uint32_t *)pAddrW = (uint32_t)SymValue;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_386_JMP_SLOT Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, SymValue));
                AssertCompile(sizeof(SymValue) == sizeof(uint32_t));
                break;
            }

            /*
             * Absolute addressing.
             */
            case R_386_32:
            {
                Elf_Addr Value;
                if (pSym->st_shndx < pModElf->Ehdr.e_shnum)
                    Value = *pAddrR + offDelta;         /* Simplified. */
                else if (pSym->st_shndx == SHN_ABS)
                    continue;                           /* Internal fixup, no need to apply it. */
                else if (pSym->st_shndx == SHN_UNDEF)
                    Value = SymValue + *pAddrR;
                else
                    AssertFailedReturn(VERR_LDR_GENERAL_FAILURE); /** @todo SHN_COMMON */
                *(uint32_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_386_32   Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value));
                break;
            }

            /*
             * PC relative addressing.
             */
            case R_386_PC32:
            {
                Elf_Addr Value;
                if (pSym->st_shndx < pModElf->Ehdr.e_shnum)
                    continue;                           /* Internal fixup, no need to apply it. */
                else if (pSym->st_shndx == SHN_ABS)
                    Value = *pAddrR + offDelta;         /* Simplified. */
                else if (pSym->st_shndx == SHN_UNDEF)
                {
                    const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                    Value = SymValue + *(uint32_t *)pAddrR - SourceAddr;
                    *(uint32_t *)pAddrW = Value;
                }
                else
                    AssertFailedReturn(VERR_LDR_GENERAL_FAILURE); /** @todo SHN_COMMON */
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_386_PC32 Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value));
                break;
            }

#elif ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_AMD64
            /*
             * GOT/PLT.
             */
            case R_X86_64_GLOB_DAT:
            {
                *(uint64_t *)pAddrW = (uint64_t)SymValue;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_X86_64_GLOB_DAT Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, SymValue));
                AssertCompile(sizeof(SymValue) == sizeof(uint64_t));
                break;
            }

            case R_X86_64_JMP_SLOT:
            {
                *(uint64_t *)pAddrW = (uint64_t)SymValue;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_X86_64_JMP_SLOT Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, SymValue));
                AssertCompile(sizeof(SymValue) == sizeof(uint64_t));
                break;
            }

            /*
             * Absolute addressing.
             */
            case R_X86_64_64:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(uint64_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_X86_64_64   Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value, SymValue));
                break;
            }

            /*
             * Truncated 32-bit value (zero-extendedable to the 64-bit value).
             */
            case R_X86_64_32:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(uint32_t *)pAddrW = (uint32_t)Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_X86_64_32   Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(uint32_t *)pAddrW == SymValue, ("Value=" FMT_ELF_ADDR "\n", SymValue),
                                VERR_SYMBOL_VALUE_TOO_BIG);
                break;
            }

            /*
             * Truncated 32-bit value (sign-extendedable to the 64-bit value).
             */
            case R_X86_64_32S:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_X86_64_32S  Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG); /** @todo check the sign-extending here. */
                break;
            }

            /*
             * PC relative addressing.
             */
            case R_X86_64_PC32:
            {
                const Elf_Addr SourceAddr = SecAddr  + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value      = SymValue + paRels[iRel].r_addend - SourceAddr;
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_X86_64_PC32 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, paRels[iRel].r_offset, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG); /** @todo check the sign-extending here. */
                break;
            }

#elif ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_ARM64
            /*
             * GOT/PLT.
             */
            case R_AARCH64_GLOB_DAT:
            {
                const Elf_Addr Value = (uint64_t)SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *pAddrR);
                *(uint64_t *)pAddrW = (uint64_t)Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_AARCH64_GLOB_DAT Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value));
                AssertCompile(sizeof(SymValue) == sizeof(uint64_t));
                break;
            }

            case R_AARCH64_JUMP_SLOT:
            {
                const Elf_Addr Value = (uint64_t)SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *pAddrR);
                *(uint64_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR "/" FMT_ELF_ADDR7 ": R_AARCH64_JMP_SLOT Value=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, paRels[iRel].r_offset, Value));
                AssertCompile(sizeof(SymValue) == sizeof(uint64_t));
                break;
            }

            case R_AARCH64_ABS64:
            {
                const Elf_Addr Value = SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *pAddrR);
                *pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_AARCH64_ABS64 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                break;
            }

#else
# error "port me"
#endif
            default:
                AssertMsgFailed(("Unknown relocation type: %d (iRel=%d iRelMax=%d)\n",
                                 ELF_R_TYPE(paRels[iRel].r_info), iRel, iRelMax));
                return VERR_LDRELF_RELOCATION_NOT_SUPPORTED;
        }
    }

#if ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_ARM64
    RT_NOREF(SecAddr, cbSec);
#endif
    return VINF_SUCCESS;
}


/**
 * Applies the fixups for a sections.
 *
 * @returns iprt status code.
 * @param   pModElf         The ELF loader module instance data.
 * @param   BaseAddr        The base address which the module is being fixedup to.
 * @param   pfnGetImport    The callback function to use to resolve imports (aka unresolved externals).
 * @param   pvUser          User argument to pass to the callback.
 * @param   SecAddr         The section address. This is the address the relocations are relative to.
 * @param   cbSec           The section size. The relocations must be inside this.
 * @param   pu8SecBaseR     Where we read section bits from.
 * @param   pu8SecBaseW     Where we write section bits to.
 * @param   pvRelocs        Pointer to where we read the relocations from.
 * @param   cbRelocs        Size of the relocations.
 */
static int ELF_RELOCATOR_NAME(RelocateSectionRel)(PRTLDRMODELF pModElf, Elf_Addr BaseAddr,
                                                  PFNRTLDRIMPORT pfnGetImport, void *pvUser,
                                                  const Elf_Addr SecAddr, Elf_Size cbSec,
                                                  const uint8_t *pu8SecBaseR, uint8_t *pu8SecBaseW,
                                                  const void *pvRelocs, Elf_Size cbRelocs)
{
#ifndef ELF_RELOCATOR_IS_REL
    NOREF(pu8SecBaseR);
#endif

    /*
     * Iterate the relocations.
     * The relocations are stored in an array of Elf32_Rel records and covers the entire relocation section.
     */
    const Elf_Reloc  *paRels = (const Elf_Reloc *)pvRelocs;
    const unsigned   iRelMax = (unsigned)(cbRelocs / sizeof(paRels[0]));
    AssertMsgReturn(iRelMax == cbRelocs / sizeof(paRels[0]), (FMT_ELF_SIZE "\n", cbRelocs / sizeof(paRels[0])), VERR_IMAGE_TOO_BIG);
    for (unsigned iRel = 0; iRel < iRelMax; iRel++)
    {
        /*
         * Skip R_XXX_NONE entries early to avoid confusion in the symbol
         * getter code.
         */
        if (ELF_R_TYPE(paRels[iRel].r_info) == ELF_RELOCATOR_R_XXX_NONE)
            continue;

        /*
         * Get the symbol.
         */
        const Elf_Sym  *pSym = NULL; /* shut up gcc */
        Elf_Addr        SymValue = 0; /* shut up gcc-4 */
        int rc = RTLDRELF_NAME(Symbol)(pModElf, BaseAddr, pfnGetImport, pvUser, ELF_R_SYM(paRels[iRel].r_info), &pSym, &SymValue);
        if (RT_FAILURE(rc))
            return rc;

        Log3(("rtldrELF: " FMT_ELF_ADDR " %02x %06x - " FMT_ELF_ADDR " %3d %02x %s\n",
              paRels[iRel].r_offset, ELF_R_TYPE(paRels[iRel].r_info), (unsigned)ELF_R_SYM(paRels[iRel].r_info),
              SymValue, (unsigned)pSym->st_shndx, pSym->st_info, ELF_STR(pModElf, pSym->st_name)));

        /*
         * Apply the fixup.
         */
        AssertMsgReturn(paRels[iRel].r_offset < cbSec, (FMT_ELF_ADDR " " FMT_ELF_SIZE "\n", paRels[iRel].r_offset, cbSec), VERR_LDRELF_INVALID_RELOCATION_OFFSET);
#ifdef ELF_RELOCATOR_IS_REL
        const Elf_Addr *pAddrR = (const Elf_Addr *)(pu8SecBaseR + paRels[iRel].r_offset);    /* Where to read the addend. */
#endif
        Elf_Addr       *pAddrW =       (Elf_Addr *)(pu8SecBaseW + paRels[iRel].r_offset);    /* Where to write the fixup. */
        switch (ELF_R_TYPE(paRels[iRel].r_info))
        {
#if   ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_X86
            /*
             * Absolute addressing.
             */
            case R_386_32:
            {
                const Elf_Addr Value = SymValue + *pAddrR;
                *(uint32_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_386_32   Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                break;
            }

            /*
             * PC relative addressing.
             */
            case R_386_PC32:
            {
                const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value = SymValue + *(uint32_t *)pAddrR - SourceAddr;
                *(uint32_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_386_PC32 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, Value, SymValue));
                break;
            }

            /* ignore */
            case R_386_NONE:
                break;

#elif ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_AMD64

            /*
             * Absolute addressing
             */
            case R_X86_64_64:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(uint64_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_X86_64_64   Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                break;
            }

            /*
             * Truncated 32-bit value (zero-extendedable to the 64-bit value).
             */
            case R_X86_64_32:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(uint32_t *)pAddrW = (uint32_t)Value;
                Log4((FMT_ELF_ADDR": R_X86_64_32   Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(uint32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                break;
            }

            /*
             * Truncated 32-bit value (sign-extendedable to the 64-bit value).
             */
            case R_X86_64_32S:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR": R_X86_64_32S  Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG); /** @todo check the sign-extending here. */
                break;
            }

            /*
             * PC relative addressing.
             */
            case R_X86_64_PC32:
            case R_X86_64_PLT32: /* binutils commit 451875b4f976a527395e9303224c7881b65e12ed feature/regression. */
            {
                const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend - SourceAddr;
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR": R_X86_64_PC32 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG); /** @todo check the sign-extending here. */
                break;
            }

            /* ignore */
            case R_X86_64_NONE:
                break;

#elif ELF_RELOCATOR_ARCH_VAL == RT_ARCH_VAL_ARM64

            /*
             * Data relocations.
             */
            case R_AARCH64_ABS64:
            {
                const Elf_Addr Value = SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *pAddrR);
                *pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_AARCH64_ABS64 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                break;
            }

            case R_AARCH64_ABS32:
            {
                const Elf_Addr Value = SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *(int32_t *)pAddrR);
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR": R_AARCH64_ABS32 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                break;
            }

            case R_AARCH64_ABS16:
            {
                const Elf_Addr Value = SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *(int16_t *)pAddrR);
                *(int16_t *)pAddrW = (int16_t)Value;
                Log4((FMT_ELF_ADDR": R_AARCH64_ABS16 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int16_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                break;
            }

            case R_AARCH64_PREL64:
            {
                const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value = SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *pAddrR) - SourceAddr;
                *pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_AARCH64_PREL64 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, Value, SymValue));
                break;
            }

            case R_AARCH64_PREL32:
            {
                const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value = SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *(int32_t *)pAddrR) - SourceAddr;
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR": R_AARCH64_PREL32 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                break;
            }

            case R_AARCH64_PREL16:
            {
                const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value = SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *(int16_t *)pAddrR) - SourceAddr;
                *(int16_t *)pAddrW = (int16_t)Value;
                Log4((FMT_ELF_ADDR": R_AARCH64_PREL16 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int16_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                break;
            }

            case R_AARCH64_PLT32:
            {
                const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value = SymValue + ELF_PICK_ADDEND(paRels[iRel].r_addend, *(int32_t *)pAddrR) - SourceAddr;
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR": R_AARCH64_PLT32 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                break;
            }

            /* ignore */
            case R_AARCH64_NONE:
                break;

#else
# error "port me"
#endif

            default:
                AssertMsgFailed(("Unknown relocation type: %d (iRel=%d iRelMax=%d)\n",
                                 ELF_R_TYPE(paRels[iRel].r_info), iRel, iRelMax));
                return VERR_LDRELF_RELOCATION_NOT_SUPPORTED;
        }
    }

    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Cleanup Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#undef ELF_RELOCATOR_ARCH_VAL
#undef ELF_RELOCATOR_R_XXX_NONE
#undef ELF_RELOCATOR_IS_REL
#undef ELF_RELOCATOR_IS_RELA
#undef ELF_RELOCATOR_NAME
#undef ELF_PICK_ADDEND
#undef Elf_Reloc

