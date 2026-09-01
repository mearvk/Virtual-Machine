/* $Id: VBoxEditElf-template.cpp.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBoxEditElf - Simple ELF binary file editor, templated code.
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


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if ELF_MODE == 32
# define ELFEDIT_NAME(name)    elfEditElf32##name
# define ELFEDIT_SUFF(name)    name##32
# define ELFEDIT_MID(pre,suff) pre##32##suff
# define FMT_ELF_ADDR           "%08RX32"
# define FMT_ELF_ADDR7          "%07RX32"
# define FMT_ELF_HALF           "%04RX16"
# define FMT_ELF_OFF            "%08RX32"
# define FMT_ELF_SIZE           "%08RX32"
# define FMT_ELF_SWORD          "%RI32"
# define FMT_ELF_WORD           "%08RX32"
# define FMT_ELF_XWORD          "%08RX32"
# define FMT_ELF_SXWORD         "%RI32"
# define Elf_Xword              Elf32_Word
# define Elf_Sxword             Elf32_Sword

#elif ELF_MODE == 64
# define ELFEDIT_NAME(name)    elfEditElf64##name
# define ELFEDIT_SUFF(name)    name##64
# define ELFEDIT_MID(pre,suff) pre##64##suff
# define FMT_ELF_ADDR           "%016RX64"
# define FMT_ELF_ADDR7          "%08RX64"
# define FMT_ELF_HALF           "%04RX16"
# define FMT_ELF_SHALF          "%RI16"
# define FMT_ELF_OFF            "%016RX64"
# define FMT_ELF_SIZE           "%016RX64"
# define FMT_ELF_SWORD          "%RI32"
# define FMT_ELF_WORD           "%08RX32"
# define FMT_ELF_XWORD          "%016RX64"
# define FMT_ELF_SXWORD         "%RI64"
# define Elf_Xword              Elf64_Xword
# define Elf_Sxword             Elf64_Sxword
#endif

#define Elf_Ehdr            ELFEDIT_MID(Elf,_Ehdr)
#define Elf_Phdr            ELFEDIT_MID(Elf,_Phdr)
#define Elf_Shdr            ELFEDIT_MID(Elf,_Shdr)
#define Elf_Sym             ELFEDIT_MID(Elf,_Sym)
#define Elf_Rel             ELFEDIT_MID(Elf,_Rel)
#define Elf_Rela            ELFEDIT_MID(Elf,_Rela)
#define Elf_Nhdr            ELFEDIT_MID(Elf,_Nhdr)
#define Elf_Dyn             ELFEDIT_MID(Elf,_Dyn)
#define Elf_Addr            ELFEDIT_MID(Elf,_Addr)
#define Elf_Half            ELFEDIT_MID(Elf,_Half)
#define Elf_Off             ELFEDIT_MID(Elf,_Off)
#define Elf_Size            ELFEDIT_MID(Elf,_Size)
#define Elf_Sword           ELFEDIT_MID(Elf,_Sword)
#define Elf_Word            ELFEDIT_MID(Elf,_Word)

#define ELF_R_SYM(info)     ELFEDIT_MID(ELF,_R_SYM)(info)
#define ELF_R_TYPE(info)    ELFEDIT_MID(ELF,_R_TYPE)(info)
#define ELF_R_INFO(sym, type) ELFEDIT_MID(ELF,_R_INFO)(sym, type)

#define ELF_ST_BIND(info)   ELFEDIT_MID(ELF,_ST_BIND)(info)
#define ELF_ST_TYPE(info)   ELFEDIT_MID(ELF,_ST_TYPE)(info)
#define ELF_ST_VISIBILITY(info)   ELFEDIT_MID(ELF,_ST_VISIBILITY)(info)


static RTEXITCODE ELFEDIT_NAME(DeleteRunpath)(RTFILE hFileElf, const char *pszInput)
{
    Elf_Ehdr Hdr;
    int rc = RTFileReadAt(hFileElf, 0, &Hdr, sizeof(Hdr), NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF header from '%s': %Rrc\n", pszInput, rc);

    if (    Hdr.e_ident[EI_MAG0] != ELFMAG0
        ||  Hdr.e_ident[EI_MAG1] != ELFMAG1
        ||  Hdr.e_ident[EI_MAG2] != ELFMAG2
        ||  Hdr.e_ident[EI_MAG3] != ELFMAG3)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF magic (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_CLASS] != ELFEDIT_SUFF(ELFCLASS))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF class (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_DATA] != ELFDATA2LSB)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF endian %x is unsupported", Hdr.e_ident[EI_DATA]);
    if (Hdr.e_version != EV_CURRENT)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF version %x is unsupported", Hdr.e_version);

    if (sizeof(Elf_Ehdr) != Hdr.e_ehsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_ehsize is %d expected %d!", Hdr.e_ehsize, sizeof(Elf_Ehdr));
    if (    sizeof(Elf_Phdr) != Hdr.e_phentsize
        &&  (   Hdr.e_phnum != 0
             || Hdr.e_type == ET_DYN
             || Hdr.e_type == ET_EXEC))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_phentsize is %d expected %d!", Hdr.e_phentsize, sizeof(Elf_Phdr));
    if (sizeof(Elf_Shdr) != Hdr.e_shentsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_shentsize is %d expected %d!", Hdr.e_shentsize, sizeof(Elf_Shdr));

    /* Find dynamic section. */
    Elf_Phdr Phdr; RT_ZERO(Phdr);
    bool fFound = false;
    for (uint32_t i = 0; i < Hdr.e_phnum; i++)
    {
        rc = RTFileReadAt(hFileElf, Hdr.e_phoff + i * sizeof(Phdr), &Phdr, sizeof(Phdr), NULL);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF program header header from '%s': %Rrc\n", pszInput, rc);

        if (Phdr.p_type == PT_DYNAMIC)
        {
            if (!Phdr.p_filesz)
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Dynmic section in '%s' is empty\n", pszInput);

            fFound = true;
            break;
        }
    }

    if (!fFound)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF binary '%s' doesn't contain dynamic section\n", pszInput);

    Elf_Dyn *paDynSh = (Elf_Dyn *)RTMemAllocZ(Phdr.p_filesz);
    if (!paDynSh)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes of memory for dynamic section of '%s'\n", Phdr.p_filesz, pszInput);

    rc = RTFileReadAt(hFileElf, Phdr.p_offset, paDynSh, Phdr.p_filesz, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF program header header from '%s': %Rrc\n", pszInput, rc);

    /* Remove all DT_RUNPATH entries and padd the remainder with DT_NULL. */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < Phdr.p_filesz / sizeof(Elf64_Dyn); i++)
    {
        paDynSh[idx] = paDynSh[i];
        if (paDynSh[i].d_tag != DT_RPATH && paDynSh[i].d_tag != DT_RUNPATH)
            idx++;
    }

    while (idx < Phdr.p_filesz / sizeof(Elf64_Dyn))
    {
        paDynSh[idx].d_tag = DT_NULL;
        paDynSh[idx].d_un.d_val = 0;
        idx++;
    }

    /* Write the result. */
    rc = RTFileWriteAt(hFileElf, Phdr.p_offset, paDynSh, Phdr.p_filesz, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write updated ELF dynamic section for '%s': %Rrc\n", pszInput, rc);

    RTMemFree(paDynSh);
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE ELFEDIT_NAME(ChangeRunpathEntry)(RTFILE hFileElf, const char *pszInput, Elf_Ehdr *pHdr, Elf_Xword offInStrTab, const char *pszRunpath)
{
    /* Read section headers to find the string table. */
    size_t const cbShdrs = pHdr->e_shnum * sizeof(Elf_Shdr);
    Elf_Shdr *paShdrs = (Elf_Shdr *)RTMemAlloc(cbShdrs);
    if (!paShdrs)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes of memory for section headers of '%s'\n", cbShdrs, pszInput);

    int rc = RTFileReadAt(hFileElf, pHdr->e_shoff, paShdrs, cbShdrs, NULL);
    if (RT_FAILURE(rc))
    {
        RTMemFree(paShdrs);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read %zu bytes of section headers from '%s': %Rrc\n", cbShdrs, pszInput, rc);
    }

    uint32_t idx;
    for (idx = 0; idx < pHdr->e_shnum; idx++)
    {
        if (paShdrs[idx].sh_type == SHT_STRTAB)
            break;
    }

    if (idx == pHdr->e_shnum)
    {
        RTMemFree(paShdrs);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF binary '%s' does not contain a string table\n", pszInput);
    }

    size_t const cbStrTab  = paShdrs[idx].sh_size;
    RTFOFF const offStrTab = paShdrs[idx].sh_offset;
    RTMemFree(paShdrs);

    if (offInStrTab >= cbStrTab)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table offset of runpath entry is out of bounds: got %#RX64, maximum is %zu\n", offInStrTab, cbStrTab - 1);

    /* Read the string table. */
    char *pbStrTab = (char *)RTMemAllocZ(cbStrTab + 1); /* Force a zero terminator. */
    if (!pbStrTab)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes of memory for string table of '%s'\n", cbStrTab + 1, pszInput);

    rc = RTFileReadAt(hFileElf, offStrTab, pbStrTab, cbStrTab, NULL);
    if (RT_FAILURE(rc))
    {
        RTMemFree(pbStrTab);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read %zu bytes of the string table from '%s': %Rrc\n", cbStrTab, pszInput, rc);
    }

    /* Calculate the maximum number of characters we can replace. */
    char *pbStr = &pbStrTab[offInStrTab];
    size_t cchMax = strlen(pbStr);
    while (   &pbStr[cchMax + 1] < &pbStrTab[cbStrTab]
           && pbStr[cchMax] == '\0')
        cchMax++;

    size_t const cchNewRunpath = strlen(pszRunpath);
    if (cchMax < cchNewRunpath)
    {
        RTMemFree(pbStrTab);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "New runpath '%s' is too long to overwrite current one, maximum length is: %zu\n", cchNewRunpath, cchMax);
    }

    memcpy(pbStr, pszRunpath, cchNewRunpath + 1);
    rc = RTFileWriteAt(hFileElf, offStrTab, pbStrTab, cbStrTab, NULL);
    RTMemFree(pbStrTab);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Writing altered string table failed: %Rrc\n", rc);

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE ELFEDIT_NAME(ChangeRunpath)(RTFILE hFileElf, const char *pszInput, const char *pszRunpath)
{
    /* Only support for 64-bit ELF files currently. */
    Elf_Ehdr Hdr;
    int rc = RTFileReadAt(hFileElf, 0, &Hdr, sizeof(Hdr), NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF header from '%s': %Rrc\n", pszInput, rc);

    if (    Hdr.e_ident[EI_MAG0] != ELFMAG0
        ||  Hdr.e_ident[EI_MAG1] != ELFMAG1
        ||  Hdr.e_ident[EI_MAG2] != ELFMAG2
        ||  Hdr.e_ident[EI_MAG3] != ELFMAG3)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF magic (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_CLASS] != ELFEDIT_SUFF(ELFCLASS))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF class (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_DATA] != ELFDATA2LSB)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF endian %x is unsupported", Hdr.e_ident[EI_DATA]);
    if (Hdr.e_version != EV_CURRENT)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF version %x is unsupported", Hdr.e_version);

    if (sizeof(Elf_Ehdr) != Hdr.e_ehsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_ehsize is %d expected %d!", Hdr.e_ehsize, sizeof(Elf_Ehdr));
    if (    sizeof(Elf_Phdr) != Hdr.e_phentsize
        &&  (   Hdr.e_phnum != 0
             || Hdr.e_type == ET_DYN
             || Hdr.e_type == ET_EXEC))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_phentsize is %d expected %d!", Hdr.e_phentsize, sizeof(Elf_Phdr));
    if (sizeof(Elf_Shdr) != Hdr.e_shentsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_shentsize is %d expected %d!", Hdr.e_shentsize, sizeof(Elf_Shdr));

    /* Find dynamic section. */
    Elf_Phdr Phdr; RT_ZERO(Phdr);
    bool fFound = false;
    for (uint32_t i = 0; i < Hdr.e_phnum; i++)
    {
        rc = RTFileReadAt(hFileElf, Hdr.e_phoff + i * sizeof(Phdr), &Phdr, sizeof(Phdr), NULL);
        if (RT_FAILURE(rc))
        {
            RTFileClose(hFileElf);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF program header header from '%s': %Rrc\n", pszInput, rc);
        }
        if (Phdr.p_type == PT_DYNAMIC)
        {
            if (!Phdr.p_filesz)
            {
                RTFileClose(hFileElf);
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Dynmic section in '%s' is empty\n", pszInput);
            }
            fFound = true;
            break;
        }
    }

    if (!fFound)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF binary '%s' doesn't contain dynamic section\n", pszInput);

    Elf_Dyn *paDynSh = (Elf_Dyn *)RTMemAllocZ(Phdr.p_filesz);
    if (!paDynSh)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes of memory for dynamic section of '%s'\n", Phdr.p_filesz, pszInput);

    rc = RTFileReadAt(hFileElf, Phdr.p_offset, paDynSh, Phdr.p_filesz, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF program header header from '%s': %Rrc\n", pszInput, rc);

    /* Look for the first DT_RUNPATH entry and rewrite it. */
    for (uint32_t i = 0; i < Phdr.p_filesz / sizeof(Elf_Dyn); i++)
    {
        if (   paDynSh[i].d_tag == DT_RPATH
            || paDynSh[i].d_tag == DT_RUNPATH)
        {
            RTEXITCODE rcExit = ELFEDIT_NAME(ChangeRunpathEntry)(hFileElf, pszInput, &Hdr, paDynSh[i].d_un.d_val, pszRunpath);
            RTMemFree(paDynSh);
            return rcExit;
        }
    }

    RTMemFree(paDynSh);
    return RTMsgErrorExit(RTEXITCODE_FAILURE, "No DT_RPATH or DT_RUNPATH entry found in '%s'\n", pszInput);
}


static RTEXITCODE ELFEDIT_NAME(Parse)(PELFEDITSTUBIMG pStubImg, RTFILE hFileElf)
{
    Elf_Ehdr Hdr;
    int rc = RTFileReadAt(hFileElf, 0, &Hdr, sizeof(Hdr), NULL);
    if (RT_FAILURE(rc))
    {
        RTFileClose(hFileElf);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF header: %Rrc\n", rc);
    }

    if (    Hdr.e_ident[EI_MAG0] != ELFMAG0
        ||  Hdr.e_ident[EI_MAG1] != ELFMAG1
        ||  Hdr.e_ident[EI_MAG2] != ELFMAG2
        ||  Hdr.e_ident[EI_MAG3] != ELFMAG3)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF magic (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_CLASS] != ELFEDIT_SUFF(ELFCLASS))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF class (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_DATA] != ELFDATA2LSB)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF endian %x is unsupported", Hdr.e_ident[EI_DATA]);
    if (Hdr.e_version != EV_CURRENT)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF version %x is unsupported", Hdr.e_version);

    if (sizeof(Elf_Ehdr) != Hdr.e_ehsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_ehsize is %d expected %d!", Hdr.e_ehsize, sizeof(Elf_Ehdr));
    if (    sizeof(Elf_Phdr) != Hdr.e_phentsize
        &&  (   Hdr.e_phnum != 0
             || Hdr.e_type == ET_DYN
             || Hdr.e_type == ET_EXEC))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_phentsize is %d expected %d!", Hdr.e_phentsize, sizeof(Elf_Phdr));
    if (sizeof(Elf_Shdr) != Hdr.e_shentsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_shentsize is %d expected %d!", Hdr.e_shentsize, sizeof(Elf_Shdr));

    switch (Hdr.e_machine)
    {
        case EM_AARCH64: pStubImg->enmArch = RTLDRARCH_ARM64; break;
        case EM_X86_64:  pStubImg->enmArch = RTLDRARCH_AMD64; break;
        case EM_386:
        case EM_486:     pStubImg->enmArch = RTLDRARCH_X86_32; break;
        default:
            RTMsgErrorExit(RTEXITCODE_FAILURE, "Machine type %#x is not supported!", Hdr.e_machine);
    }

    /* Find all the dynamic sections we need. */
    uint32_t idStrTab = UINT32_MAX;
    uint32_t idDynamic = UINT32_MAX;
    Elf_Shdr ShdrDyn; RT_ZERO(ShdrDyn);
    char *pachStrTab = NULL; size_t cbStrTab = 0;
    Elf_Sym *paDynSyms = NULL; size_t cbDynSyms = 0;
    uint16_t *pu16GnuVerSym = NULL; size_t cbGnuVerSym = 0;
    uint8_t *pbGnuVerDef = NULL; size_t cbGnuVerDef = 0;
    uint8_t *pbGnuVerNeed = NULL; size_t cbGnuVerNeed = 0;

    for (uint32_t i = 0; i < Hdr.e_shnum; i++)
    {
        Elf_Shdr Shdr; RT_ZERO(Shdr);
        rc = RTFileReadAt(hFileElf, Hdr.e_shoff + i * sizeof(Shdr), &Shdr, sizeof(Shdr), NULL /*pcbRead*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read section header at %RX64\n", Hdr.e_shoff + i * sizeof(Shdr));

        verbose("Section header %u:\n"
                "    sh_name:      %RU32\n"
                "    sh_type:      %RX32\n"
                "    sh_flags:     %#RX64\n"
                "    sh_addr:      %#RX64\n"
                "    sh_offset:    %RU64\n"
                "    sh_size:      %RU64\n"
                "    sh_link:      %RU16\n"
                "    sh_info:      %RU16\n"
                "    sh_addralign: %#RX64\n"
                "    sh_entsize:   %#RX64\n", i,
                Shdr.sh_name, Shdr.sh_type, Shdr.sh_flags,
                Shdr.sh_addr, Shdr.sh_offset, Shdr.sh_size,
                Shdr.sh_link, Shdr.sh_info, Shdr.sh_addralign,
                Shdr.sh_entsize);

        switch (Shdr.sh_type)
        {
            case SHT_DYNSYM:
            {
                cbDynSyms = Shdr.sh_size;
                paDynSyms = (Elf_Sym *)RTMemAllocZ(cbDynSyms);
                if (!paDynSyms)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .dynsym section\n", cbDynSyms);

                rc = RTFileReadAt(hFileElf, Shdr.sh_offset, paDynSyms, cbDynSyms, NULL /*pcbRead*/);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .dynsym section at %RX64\n", Shdr.sh_offset);

                /* It should link to the string table. */
                if (idStrTab == UINT32_MAX)
                    idStrTab = Shdr.sh_link;
                else if (idStrTab != Shdr.sh_link)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table index %u doesn't match %u\n", Shdr.sh_link, idStrTab);
                break;
            }
            case SHT_GNU_versym:
            {
                cbGnuVerSym = Shdr.sh_size;
                pu16GnuVerSym = (uint16_t *)RTMemAllocZ(cbGnuVerSym);
                if (!pu16GnuVerSym)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .gnu.version section\n", cbGnuVerSym);

                rc = RTFileReadAt(hFileElf, Shdr.sh_offset, pu16GnuVerSym, cbGnuVerSym, NULL /*pcbRead*/);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .dynsym section at %RX64\n", Shdr.sh_offset);

                /** @todo It should link to the .dynsym table. */
                break;
            }
            case SHT_GNU_verdef:
            {
                cbGnuVerDef = Shdr.sh_size;
                pbGnuVerDef = (uint8_t *)RTMemAllocZ(cbGnuVerDef);
                if (!pbGnuVerDef)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .gnu.version_d section\n", cbGnuVerDef);

                rc = RTFileReadAt(hFileElf, Shdr.sh_offset, pbGnuVerDef, cbGnuVerDef, NULL /*pcbRead*/);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .dynsym section at %RX64\n", Shdr.sh_offset);

                /* It should link to the string table. */
                if (idStrTab == UINT32_MAX)
                    idStrTab = Shdr.sh_link;
                else if (idStrTab != Shdr.sh_link)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table index %u doesn't match %u\n", Shdr.sh_link, idStrTab);
                break;
            }
            case SHT_GNU_verneed:
            {
                cbGnuVerNeed = Shdr.sh_size;
                pbGnuVerNeed = (uint8_t *)RTMemAllocZ(cbGnuVerNeed);
                if (!pbGnuVerNeed)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .gnu.version_r section\n", cbGnuVerNeed);

                rc = RTFileReadAt(hFileElf, Shdr.sh_offset, pbGnuVerNeed, cbGnuVerNeed, NULL /*pcbRead*/);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .gnu.version_r section at %RX64\n", Shdr.sh_offset);

                /* It should link to the string table. */
                if (idStrTab == UINT32_MAX)
                    idStrTab = Shdr.sh_link;
                else if (idStrTab != Shdr.sh_link)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table index %u doesn't match %u\n", Shdr.sh_link, idStrTab);
                break;
            }
            case SHT_DYNAMIC:
            {
                if (idDynamic != UINT32_MAX)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "More than one .dynamic section\n");

                idDynamic = i;
                ShdrDyn = Shdr;
                /* It should link to the string table. */
                if (idStrTab == UINT32_MAX)
                    idStrTab = Shdr.sh_link;
                else if (idStrTab != Shdr.sh_link)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table index %u doesn't match %u\n", Shdr.sh_link, idStrTab);
            }
            default:
                break; /* Ignored. */
        }
    }

    if (idStrTab == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table index not found\n");
    if (idDynamic == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, ".dynamic section index not found\n");

    if (pbGnuVerDef && !pu16GnuVerSym)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Versioned library misses .gnu.version\n");

    /* Read the string table section header. */
    Elf_Shdr Shdr; RT_ZERO(Shdr);
    rc = RTFileReadAt(hFileElf, Hdr.e_shoff + idStrTab * sizeof(Shdr), &Shdr, sizeof(Shdr), NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .strtab section header at %RX64: %Rrc\n",
                              Hdr.e_shoff + idStrTab * sizeof(Shdr), rc);

    cbStrTab = Shdr.sh_size;
    pachStrTab = (char *)RTMemAllocZ(cbStrTab + 1); /* Force termination */
    if (!pachStrTab)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .strtab section\n", cbStrTab);

    rc = RTFileReadAt(hFileElf, Shdr.sh_offset, pachStrTab, cbStrTab, NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .strtab section at %RX64\n", Shdr.sh_offset);

    /* Read the .dynamic section. */
    Elf_Dyn *paDynEnt = (Elf_Dyn *)RTMemAllocZ(ShdrDyn.sh_size);
    if (!paDynEnt)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .dynamic section\n", ShdrDyn.sh_size);

    rc = RTFileReadAt(hFileElf, ShdrDyn.sh_offset, paDynEnt, ShdrDyn.sh_size, NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .dynamic section at %RX64 from\n", ShdrDyn.sh_offset);

    size_t cVerDefNum = 0;

    /* Parse the dynamic section first to get some basics sorted. */
    for (uint32_t i = 0; i < ShdrDyn.sh_size / sizeof(*paDynEnt); i++)
    {
        switch (paDynEnt[i].d_tag)
        {
            case DT_SONAME:
            {
                pStubImg->pszName = elfEditImgAddString(pStubImg, &pachStrTab[paDynEnt[i].d_un.d_val]);
                if (!pStubImg->pszName)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                          "Failed to add '%s' to string table\n",
                                          &pachStrTab[paDynEnt[i].d_un.d_val]);

                break;
            }
            case DT_NEEDED:
            {
                const char *pszNeeded = elfEditImgAddString(pStubImg, &pachStrTab[paDynEnt[i].d_un.d_val]);
                if (!pszNeeded)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                          "Failed to add '%s' to string table\n",
                                          &pachStrTab[paDynEnt[i].d_un.d_val]);

                if (pStubImg->cNeeded == pStubImg->cNeededMax)
                {
                    const char **papszNeededNew = (const char **)RTMemRealloc(pStubImg->papszNeeded,
                                                                              (pStubImg->cNeededMax + 10) * sizeof(const char *));
                    if (!papszNeededNew)
                        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                              "Failed to allocate more entries for the needed array\n",
                                              &pachStrTab[paDynEnt[i].d_un.d_val]);
                    pStubImg->papszNeeded = papszNeededNew;
                    pStubImg->cNeededMax += 10;
                }

                pStubImg->papszNeeded[pStubImg->cNeeded] = pszNeeded;
                pStubImg->cNeeded++;
                break;
            }
            case DT_VERDEFNUM:
                cVerDefNum = paDynEnt[i].d_un.d_val;
                break;
            default:
                break;
        }
    }

    if (cVerDefNum)
    {
        pStubImg->paVersions = (PELFEDITSTUBVERSION)RTMemAllocZ((cVerDefNum + 1) * sizeof(*pStubImg->paVersions));
        if (!pStubImg->paVersions)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %u version entries\n", cVerDefNum);

        pStubImg->cVersions = cVerDefNum + 1;

        /* Process the verdef entries. */
        Elf_Verdef *pCur = (Elf_Verdef *)pbGnuVerDef;
        for (uint32_t i = 0; i < cVerDefNum; i++)
        {
            verbose("Verdef entry %u:\n"
                    "    vd_version:   %RU16\n"
                    "    vd_flags:     %RX16\n"
                    "    vd_ndx:       %RX16\n"
                    "    vd_cnt:       %RX16\n"
                    "    vd_hash:      %RX32\n"
                    "    vd_aux:       %RU32\n"
                    "    vd_next:      %RU32\n", i,
                    pCur->vd_version, pCur->vd_flags, pCur->vd_ndx,
                    pCur->vd_cnt, pCur->vd_hash, pCur->vd_aux, pCur->vd_next);

            if (pCur->vd_cnt > 2)
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Image has more than 2 auxiliary verdef entries (%u)\n", pCur->vd_cnt);

            pStubImg->paVersions[pCur->vd_ndx].idxVersion = pCur->vd_ndx;
            if (pCur->vd_aux != 0)
            {
                uint8_t idxVerdaux = 0;
                Elf_Verdaux *pCurVerdaux = (Elf_Verdaux *)((uint8_t *)pCur + pCur->vd_aux);
                for (;;)
                {
                    const char *pszVersion = elfEditImgAddString(pStubImg, &pachStrTab[pCurVerdaux->vda_name]);
                    if (!pszVersion)
                        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                              "Failed to add '%s' to string table\n",
                                              &pachStrTab[pCurVerdaux->vda_name]);

                    verbose("Verdaux entry %u:\n"
                            "    vda_name:     %RU32\n"
                            "    vda_next:     %RU32\n"
                            "        String:   %s (%RX32)\n", idxVerdaux,
                            pCurVerdaux->vda_name, pCurVerdaux->vda_next,
                            pszVersion, elfHash(pszVersion));

                    if (idxVerdaux == 0)
                        pStubImg->paVersions[pCur->vd_ndx].pszVersion = pszVersion;
                    else if (idxVerdaux == 1)
                        pStubImg->paVersions[pCur->vd_ndx].pszParent = pszVersion;

                    if (pCurVerdaux->vda_next == 0)
                        break;

                    idxVerdaux++;
                    pCurVerdaux = (Elf_Verdaux *)((uint8_t *)pCurVerdaux + pCurVerdaux->vda_next);
                }
            }

            if (pCur->vd_next == 0)
                break;

            pCur = (Elf_Verdef *)((uint8_t *)pCur + pCur->vd_next);
        }
    }

    /* Process exported symbols. */
    size_t const cSyms = cbDynSyms / sizeof(*paDynSyms);

    pStubImg->paSyms = (PELFEDITSTUBSYM)RTMemAllocZ(cSyms * sizeof(*pStubImg->paSyms));
    if (!pStubImg->paSyms)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %u entries for the symbol table\n", cSyms);

    uint32_t idxSym = 0;
    for (size_t i = 0; i < cSyms; i++)
    {
        PELFEDITSTUBSYM pSym = &pStubImg->paSyms[idxSym];

        uint8_t iStBind = ELF_ST_BIND(paDynSyms[i].st_info);
        uint8_t iStType = ELF_ST_TYPE(paDynSyms[i].st_info);
        uint8_t iVisibility = ELF_ST_VISIBILITY(paDynSyms[i].st_other);

        /* Ignore imports and local stuff. */
        if (iStBind == STB_LOCAL || paDynSyms[i].st_shndx == STN_UNDEF)
            continue;

        switch (iStType)
        {
            case STT_NOTYPE:    pSym->enmType = kElfEditSymType_NoType;     break;
            case STT_OBJECT:    pSym->enmType = kElfEditSymType_Object;     break;
            case STT_FUNC:      pSym->enmType = kElfEditSymType_Function;   break;
            case STT_TLS:       pSym->enmType = kElfEditSymType_Tls;        break;
            case STT_GNU_IFUNC: pSym->enmType = kElfEditSymType_GnuIndFunc; break;
            default:            RTMsgErrorExit(RTEXITCODE_FAILURE, "Unsupported symbol type %u\n", iStType);
        }

        switch (iVisibility)
        {
            case STV_DEFAULT:   pSym->enmVisibility = kElfEditSymVisbility_Default;   break;
            case STV_INTERNAL:  pSym->enmVisibility = kElfEditSymVisbility_Internal;  break;
            case STV_HIDDEN:    pSym->enmVisibility = kElfEditSymVisbility_Hidden;    break;
            case STV_PROTECTED: pSym->enmVisibility = kElfEditSymVisbility_Protected; break;
            default:            RTMsgErrorExit(RTEXITCODE_FAILURE, "Unsupported symbol visbility %u\n", iVisibility);
        }

        pSym->fWeak   = iStBind == STB_WEAK ? true : false;
        pSym->cbSym   = paDynSyms[i].st_size;
        pSym->pszName = elfEditImgAddString(pStubImg, &pachStrTab[paDynSyms[i].st_name]);
        if (!pSym->pszName)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to add '%s' to string table\n",
                                  &pachStrTab[paDynSyms[i].st_name]);

        if (pu16GnuVerSym && pu16GnuVerSym[i] > 1)
        {
            uint16_t const idxVersion = pu16GnuVerSym[i] & ~GNU_VERSYM_HIDDEN;
            pSym->fDefaultVersion = !RT_BOOL(pu16GnuVerSym[i] & GNU_VERSYM_HIDDEN);
            pSym->pVersion        = &pStubImg->paVersions[idxVersion];
        }

        idxSym++;
    }

    pStubImg->cSyms = idxSym;
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE ELFEDIT_NAME(GenerateStub)(PELFEDITSTUBIMG pStubImg, const char *pszOutput)
{
    /* Generate the string table first. */
    void *pvStrTab = NULL;
    size_t cbStrTab = 0;
    int rc = elfEditImgStrTabGenerate(pStubImg, &pvStrTab, &cbStrTab);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to generate string table for stub '%s': %Rrc\n", pszOutput, rc);

    /* Generate the version definitions. */
    void *pvVerdef = NULL;
    size_t cbVerdef = 0;
    uint16_t *pu16GnuVerSym = NULL;
    size_t cbGnuVerSym = 0;
    if (pStubImg->cVersions)
    {
        rc = elfEditImgVerdefGenerate(pStubImg, &pvVerdef, &cbVerdef);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to generate versions definition for stub '%s': %Rrc\n", pszOutput, rc);

        cbGnuVerSym   = pStubImg->cSyms * sizeof(uint16_t);
        pu16GnuVerSym = (uint16_t *)RTMemAllocZ(cbGnuVerSym);
        if (!pu16GnuVerSym)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .gnu.version table in '%s'\n", cbGnuVerSym, pszOutput);
    }

    /* Generate the symbol table now .*/
    size_t const cbSyms = pStubImg->cSyms * sizeof(Elf_Sym);
    Elf_Sym *paSyms = (Elf_Sym *)RTMemAllocZ(cbSyms);
    if (!paSyms)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for symbol table in '%s'\n", cbSyms, pszOutput);

    for (uint32_t i = 0; i < pStubImg->cSyms; i++)
    {
        PCELFEDITSTUBSYM pSym = &pStubImg->paSyms[i];
        Elf_Sym *pElfSym = &paSyms[i];

        uint8_t bBind = pSym->fWeak ? STB_WEAK : STB_GLOBAL;
        uint8_t bType = 0;
        switch (pSym->enmType)
        {
            case kElfEditSymType_Function:   bType = STT_FUNC;      break;
            case kElfEditSymType_Object:     bType = STT_OBJECT;    break;
            case kElfEditSymType_Tls:        bType = STT_TLS;       break;
            case kElfEditSymType_GnuIndFunc: bType = STT_GNU_IFUNC; break;
            case kElfEditSymType_NoType:     bType = STT_NOTYPE;    break;
            case kElfEditSymType_Invalid:
            default:
                AssertReleaseFailed();
        }

        uint8_t bVisibility = 0;
        switch (pSym->enmVisibility)
        {
            case kElfEditSymVisbility_Default:   bVisibility = STV_DEFAULT;   break;
            case kElfEditSymVisbility_Hidden:    bVisibility = STV_HIDDEN;    break;
            case kElfEditSymVisbility_Internal:  bVisibility = STV_INTERNAL;  break;
            case kElfEditSymVisbility_Protected: bVisibility = STV_PROTECTED; break;
            case kElfEditSymVisbility_Invalid:
            default:
                AssertReleaseFailed();
        }

        pElfSym->st_name  = elfEditImgGetStrIdxInStrTab(pStubImg, pSym->pszName);
        pElfSym->st_shndx = 1; /** @todo Generate a simple.text section containing only breakpoints. */
        pElfSym->st_value = 0;
        pElfSym->st_size  = (Elf_Xword)pSym->cbSym;
        pElfSym->st_info  = ELF64_ST_INFO(bBind, bType);
        pElfSym->st_other = ELF64_ST_OTHER(bVisibility);

        if (pu16GnuVerSym)
            pu16GnuVerSym[i] = pSym->pVersion->idxVersion | (pSym->fDefaultVersion ? 0 : GNU_VERSYM_HIDDEN);
    }

    /* Generate the .dynamic section. */
    size_t const cbDyn = (9 + pStubImg->cNeeded) * sizeof(Elf_Dyn);
    Elf_Dyn *paDyn = (Elf_Dyn *)RTMemAllocZ(cbDyn);
    if (!paDyn)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .dynamic section in '%s'\n", cbDyn, pszOutput);

    RTFILE hFileOut = NIL_RTFILE;
    rc = RTFileOpen(&hFileOut, pszOutput, RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create file '%s': %Rrc\n", pszOutput, rc);

    /* Program headers. */
    Elf_Phdr aPhdrs[4]; RT_ZERO(aPhdrs);
    Elf_Shdr aShdrs[8]; RT_ZERO(aShdrs);

    /* Fill in the header. */
    Elf_Ehdr Hdr; RT_ZERO(Hdr);
    Hdr.e_ident[EI_MAG0]       = ELFMAG0;
    Hdr.e_ident[EI_MAG1]       = ELFMAG1;
    Hdr.e_ident[EI_MAG2]       = ELFMAG2;
    Hdr.e_ident[EI_MAG3]       = ELFMAG3;
    Hdr.e_ident[EI_CLASS]      = ELFEDIT_SUFF(ELFCLASS);
    Hdr.e_ident[EI_DATA]       = ELFDATA2LSB;
    Hdr.e_ident[EI_VERSION]    = EV_CURRENT;
    Hdr.e_ident[EI_OSABI]      = ELFOSABI_LINUX;
    Hdr.e_ident[EI_ABIVERSION] = 0;

    Hdr.e_type = ET_DYN;
    switch (pStubImg->enmArch)
    {
        case RTLDRARCH_X86_32: Hdr.e_machine = EM_486;     break;
        case RTLDRARCH_AMD64:  Hdr.e_machine = EM_X86_64;  break;
        case RTLDRARCH_ARM64:  Hdr.e_machine = EM_AARCH64; break;
        default: AssertReleaseFailed(); break;
    }

    Hdr.e_version   = 1;
    Hdr.e_entry     = 0;
    Hdr.e_phoff     = sizeof(Hdr);
    Hdr.e_shoff     = Hdr.e_phoff + sizeof(aPhdrs);
    Hdr.e_flags     = 0;
    Hdr.e_ehsize    = sizeof(Hdr);
    Hdr.e_phentsize = sizeof(aPhdrs[0]);
    Hdr.e_phnum     = RT_ELEMENTS(aPhdrs);
    Hdr.e_shentsize = sizeof(aShdrs[0]);
    Hdr.e_shnum     = pvVerdef ? 7 : 5; /** @todo */
    Hdr.e_shstrndx  = pvVerdef ? 6 : 4; /** @todo */

    uint32_t idx = 1;
    /* NULL header */
    /* .dynsym */
    aShdrs[idx].sh_name      = 11;
    aShdrs[idx].sh_type      = SHT_DYNSYM;
    aShdrs[idx].sh_flags     = SHF_ALLOC;
    aShdrs[idx].sh_addr      = 0;
    aShdrs[idx].sh_offset    = Hdr.e_shoff + Hdr.e_shnum * sizeof(aShdrs[0]);
    aShdrs[idx].sh_size      = (Elf_Word)cbSyms;
    aShdrs[idx].sh_link      = 2; /* .dynstr */
    aShdrs[idx].sh_info      = 1;
    aShdrs[idx].sh_addralign = sizeof(Elf_Addr);
    aShdrs[idx].sh_entsize   = sizeof(*paSyms);
    uint32_t const idxDynSym = idx++;

    /* .dynstr */
    aShdrs[idx].sh_name      = 19;
    aShdrs[idx].sh_type      = SHT_STRTAB;
    aShdrs[idx].sh_flags     = SHF_ALLOC;
    aShdrs[idx].sh_addr      = 0;
    aShdrs[idx].sh_offset    = aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size;
    aShdrs[idx].sh_size      = (Elf_Word)cbStrTab;
    aShdrs[idx].sh_link      = 0;
    aShdrs[idx].sh_info      = 0;
    aShdrs[idx].sh_addralign = sizeof(uint8_t);
    aShdrs[idx].sh_entsize   = 0;
    uint32_t const idxDynStr = idx++;

    uint32_t idxGnuVerSym = 0;
    uint32_t idxGnuVerDef = 0;
    if (pvVerdef)
    {
        /* .gnu.version */
        aShdrs[idx].sh_name      = 27;
        aShdrs[idx].sh_type      = SHT_GNU_versym;
        aShdrs[idx].sh_flags     = SHF_ALLOC;
        aShdrs[idx].sh_addr      = RT_ALIGN_64(aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size, sizeof(uint16_t));
        aShdrs[idx].sh_offset    = RT_ALIGN_64(aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size, sizeof(uint16_t));
        aShdrs[idx].sh_size      = (Elf_Word)cbGnuVerSym;
        aShdrs[idx].sh_link      = 1; /* .dynsym */
        aShdrs[idx].sh_info      = 0;
        aShdrs[idx].sh_addralign = sizeof(uint16_t);
        aShdrs[idx].sh_entsize   = sizeof(uint16_t);
        idxGnuVerSym = idx++;

        /* .gnu.version_d */
        aShdrs[idx].sh_name      = 40;
        aShdrs[idx].sh_type      = SHT_GNU_verdef;
        aShdrs[idx].sh_flags     = SHF_ALLOC;
        aShdrs[idx].sh_addr      = RT_ALIGN_64(aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size, sizeof(Elf_Addr));
        aShdrs[idx].sh_offset    = RT_ALIGN_64(aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size, sizeof(Elf_Addr));
        aShdrs[idx].sh_size      = (Elf_Word)cbVerdef;
        aShdrs[idx].sh_link      = 2; /* .dynstr */
        aShdrs[idx].sh_info      = (Elf_Word)(pStubImg->cVersions - 1);
        aShdrs[idx].sh_addralign = sizeof(Elf_Addr);
        aShdrs[idx].sh_entsize   = 0;
        idxGnuVerDef = idx++;
    }

    /* Build the .dynamic section. */
    uint32_t idxDyn = 0;
    paDyn[idxDyn].d_tag        = DT_SONAME;
    paDyn[idxDyn++].d_un.d_val = elfEditImgGetStrIdxInStrTab(pStubImg, pStubImg->pszName);

    for (uint32_t i = 0; i < pStubImg->cNeeded; i++)
    {
        paDyn[idxDyn].d_tag        = DT_NEEDED;
        paDyn[idxDyn++].d_un.d_val = elfEditImgGetStrIdxInStrTab(pStubImg, pStubImg->papszNeeded[i]);
    }

    paDyn[idxDyn].d_tag        = DT_STRSZ;
    paDyn[idxDyn++].d_un.d_val = (Elf_Word)cbStrTab;
    paDyn[idxDyn].d_tag        = DT_SYMENT;
    paDyn[idxDyn++].d_un.d_val = sizeof(Elf_Sym);
    paDyn[idxDyn].d_tag        = DT_VERDEFNUM;
    paDyn[idxDyn++].d_un.d_val = (Elf_Word)(pStubImg->cVersions - 1);
    paDyn[idxDyn].d_tag        = DT_VERDEF;
    paDyn[idxDyn++].d_un.d_val = aShdrs[idxGnuVerDef].sh_offset;
    paDyn[idxDyn].d_tag        = DT_VERSYM;
    paDyn[idxDyn++].d_un.d_val = aShdrs[idxGnuVerSym].sh_offset;
    paDyn[idxDyn].d_tag        = DT_STRTAB;
    paDyn[idxDyn++].d_un.d_val = aShdrs[idxDynStr].sh_offset;
    paDyn[idxDyn].d_tag        = DT_SYMTAB;
    paDyn[idxDyn++].d_un.d_val = aShdrs[idxDynSym].sh_offset;
    paDyn[idxDyn].d_tag        = DT_NULL;
    paDyn[idxDyn++].d_un.d_val = 0;

    /* .dynamic */
    aShdrs[idx].sh_name      = 55;
    aShdrs[idx].sh_type      = SHT_DYNAMIC;
    aShdrs[idx].sh_flags     = SHF_ALLOC | SHF_WRITE;
    aShdrs[idx].sh_addr      = RT_ALIGN_64(aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size, sizeof(Elf_Addr));
    aShdrs[idx].sh_offset    = RT_ALIGN_64(aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size, sizeof(Elf_Addr));
    aShdrs[idx].sh_size      = (Elf_Word)cbDyn;
    aShdrs[idx].sh_link      = 2; /* .dynstr */
    aShdrs[idx].sh_info      = 0;
    aShdrs[idx].sh_addralign = sizeof(Elf_Addr);
    aShdrs[idx].sh_entsize   = sizeof(Elf_Dyn);

    /* Build the program headers. */
    aPhdrs[0].p_type    = PT_PHDR;
    aPhdrs[0].p_flags   = PF_R;
    aPhdrs[0].p_offset  = Hdr.e_phoff;
    aPhdrs[0].p_vaddr   = Hdr.e_phoff;
    aPhdrs[0].p_paddr   = Hdr.e_phoff;
    aPhdrs[0].p_filesz  = sizeof(aPhdrs);
    aPhdrs[0].p_memsz   = sizeof(aPhdrs);
    aPhdrs[0].p_align   = sizeof(Elf_Addr);

    aPhdrs[1].p_type    = PT_LOAD;
    aPhdrs[1].p_flags   = PF_R | PF_X;
    aPhdrs[1].p_offset  = 0;
    aPhdrs[1].p_vaddr   = 0;
    aPhdrs[1].p_paddr   = 0;
    aPhdrs[1].p_filesz  = aShdrs[idx].sh_offset;
    aPhdrs[1].p_memsz   = aShdrs[idx].sh_offset;
    aPhdrs[1].p_align   = 0x10000;

    aPhdrs[2].p_type    = PT_LOAD;
    aPhdrs[2].p_flags   = PF_R | PF_W;
    aPhdrs[2].p_offset  = aShdrs[idx].sh_offset;
    aPhdrs[2].p_vaddr   = aShdrs[idx].sh_offset;
    aPhdrs[2].p_paddr   = aShdrs[idx].sh_offset;
    aPhdrs[2].p_filesz  = aShdrs[idx].sh_size;
    aPhdrs[2].p_memsz   = aShdrs[idx].sh_size;
    aPhdrs[2].p_align   = 0x10000;

    aPhdrs[3].p_type    = PT_DYNAMIC;
    aPhdrs[3].p_flags   = PF_R | PF_W;
    aPhdrs[3].p_offset  = aShdrs[idx].sh_offset;
    aPhdrs[3].p_vaddr   = aShdrs[idx].sh_offset;
    aPhdrs[3].p_paddr   = aShdrs[idx].sh_offset;
    aPhdrs[3].p_filesz  = aShdrs[idx].sh_size;
    aPhdrs[3].p_memsz   = aShdrs[idx].sh_size;
    aPhdrs[3].p_align   = sizeof(uint64_t);

    idx++;

    /* .shstrtab */
    aShdrs[idx].sh_name      = 1;
    aShdrs[idx].sh_type      = SHT_STRTAB;
    aShdrs[idx].sh_flags     = SHF_ALLOC;
    aShdrs[idx].sh_addr      = 0;
    aShdrs[idx].sh_offset    = aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size;
    aShdrs[idx].sh_size      = sizeof(s_achShStrTab);
    aShdrs[idx].sh_link      = 0;
    aShdrs[idx].sh_info      = 0;
    aShdrs[idx].sh_addralign = sizeof(uint8_t);
    aShdrs[idx].sh_entsize   = 0;
    idx++;

    rc = RTFileWriteAt(hFileOut, 0, &Hdr, sizeof(Hdr), NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write ELF header to '%s': %Rrc\n", pszOutput, rc);

    rc = RTFileWriteAt(hFileOut, Hdr.e_phoff, &aPhdrs[0], sizeof(aPhdrs), NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write section headers to '%s': %Rrc\n", pszOutput, rc);

    rc = RTFileWriteAt(hFileOut, Hdr.e_shoff, &aShdrs[0], idx * sizeof(aShdrs[0]), NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write section headers to '%s': %Rrc\n", pszOutput, rc);

    idx = 1;
    rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, paSyms, cbSyms, NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .dynsym section to '%s': %Rrc\n", pszOutput, rc);

    rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, pvStrTab, cbStrTab, NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .dynstr section to '%s': %Rrc\n", pszOutput, rc);

    if (pvVerdef)
    {
        rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, pu16GnuVerSym, cbGnuVerSym, NULL /*pcbWritten*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .gnu.version section to '%s': %Rrc\n", pszOutput, rc);

        rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, pvVerdef, cbVerdef, NULL /*pcbWritten*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .gnu.version section to '%s': %Rrc\n", pszOutput, rc);
    }

    rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, paDyn, cbDyn, NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .dynamic section to '%s': %Rrc\n", pszOutput, rc);

    rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, s_achShStrTab, sizeof(s_achShStrTab), NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .shstrtab section to '%s': %Rrc\n", pszOutput, rc);

    RTFileClose(hFileOut);
    return RTEXITCODE_SUCCESS;
}

#undef ELFEDIT_NAME
#undef ELFEDIT_SUFF
#undef ELFEDIT_MID

#undef FMT_ELF_ADDR
#undef FMT_ELF_ADDR7
#undef FMT_ELF_HALF
#undef FMT_ELF_SHALF
#undef FMT_ELF_OFF
#undef FMT_ELF_SIZE
#undef FMT_ELF_SWORD
#undef FMT_ELF_WORD
#undef FMT_ELF_XWORD
#undef FMT_ELF_SXWORD

#undef Elf_Ehdr
#undef Elf_Phdr
#undef Elf_Shdr
#undef Elf_Sym
#undef Elf_Rel
#undef Elf_Rela
#undef Elf_Reloc
#undef Elf_Nhdr
#undef Elf_Dyn

#undef Elf_Addr
#undef Elf_Half
#undef Elf_Off
#undef Elf_Size
#undef Elf_Sword
#undef Elf_Word
#undef Elf_Xword
#undef Elf_Sxword

#undef ELF_R_SYM
#undef ELF_R_TYPE
#undef ELF_R_INFO

#undef ELF_ST_BIND
#undef ELF_ST_TYPE
#undef ELF_ST_VISIBILITY

