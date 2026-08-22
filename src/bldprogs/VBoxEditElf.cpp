/* $Id: VBoxEditElf.cpp $ */
/** @file
 * VBoxEditElf - Simple ELF binary file editor.
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
#include <stdio.h>
#include <stdlib.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/json.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/types.h>
#include <iprt/formats/elf32.h>
#include <iprt/formats/elf64.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Symbol type.
 */
typedef enum ELFEDITSYMTYPE
{
    kElfEditSymType_Invalid = 0,
    kElfEditSymType_Function,
    kElfEditSymType_Object,
    kElfEditSymType_Tls,
    kElfEditSymType_GnuIndFunc,
    kElfEditSymType_NoType,
    kElfEditSymType_32Bit_Hack = 0x7fffffff
} ELFEDITSYMTYPE;


/**
 * Symbol visbility.
 */
typedef enum ELFEDITSYMVISIBILITY
{
    kElfEditSymVisbility_Invalid = 0,
    kElfEditSymVisbility_Default,
    kElfEditSymVisbility_Hidden,
    kElfEditSymVisbility_Internal,
    kElfEditSymVisbility_Protected,
    kElfEditSymVisbility_32Bit_Hack = 0x7fffffff
} ELFEDITSYMVISIBILITY;


/**
 * A symbol version.
 */
typedef struct ELFEDITSTUBVERSION
{
    /** The version name. */
    const char              *pszVersion;
    /** The parent version name, optional. */
    const char              *pszParent;
    /** The version index. */
    uint32_t                idxVersion;
} ELFEDITSTUBVERSION;
typedef ELFEDITSTUBVERSION *PELFEDITSTUBVERSION;
typedef const ELFEDITSTUBVERSION *PCELFEDITSTUBVERSION;


/**
 * A symbol.
 */
typedef struct ELFEDITSTUBSYM
{
    /** The name of the symbol. */
    const char              *pszName;
    /** The symbol type */
    ELFEDITSYMTYPE          enmType;
    /** The symbol visbility. */
    ELFEDITSYMVISIBILITY    enmVisibility;
    /** Size of the symbol in bytes. */
    size_t                  cbSym;
    /** Flag whether this is a weak symbol. */
    bool                    fWeak;
    /** The version assigned for this symbol, NULL if no versioning. */
    PCELFEDITSTUBVERSION    pVersion;
    /** Flag whether this is the default version of the symbol, only valid if versioning is set. */
    bool                    fDefaultVersion;
} ELFEDITSTUBSYM;
/** Pointer to a symbol. */
typedef ELFEDITSTUBSYM *PELFEDITSTUBSYM;
/** Pointer to a constant symbol. */
typedef const ELFEDITSTUBSYM *PCELFEDITSTUBSYM;


/**
 * Entry in the string table.
 */
typedef struct ELFEDITSTR
{
    RTSTRSPACECORE          StrCore;
    /** The offset in the final string table. */
    size_t                  offStrTab;
} ELFEDITSTR;
typedef ELFEDITSTR *PELFEDITSTR;
typedef const ELFEDITSTR *PCELFEDITSTR;


/**
 * Stub image state
 */
typedef struct ELFEDITSTUBIMG
{
    /** The architecture of the image. */
    RTLDRARCH           enmArch;
    /** The string space. */
    RTSTRSPACE          StrSpace;
    /** Current length of the string table. */
    size_t              cbStrTab;
    /** The image name. */
    const char          *pszName;
    /** Array of neded libraries this image depends on. */
    const char          **papszNeeded;
    /** Number of entries in the needed libraries array. */
    size_t              cNeeded;
    /** Maximum number of entries the needed libraries array can hold. */
    size_t              cNeededMax;
    /** The symbol table. */
    PELFEDITSTUBSYM     paSyms;
    /** Number of entries in the symbol table. */
    size_t              cSyms;
    /** The version table. */
    PELFEDITSTUBVERSION paVersions;
    /** Number of entries in the version table. */
    size_t              cVersions;
} ELFEDITSTUBIMG;
/** Pointer to a stub image state. */
typedef ELFEDITSTUBIMG *PELFEDITSTUBIMG;
/** Pointer to a const stub image state. */
typedef const ELFEDITSTUBIMG *PCELFEDITSTUBIMG;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** @name Options
 * @{ */
static enum
{
    kVBoxEditElfAction_Nothing,
    kVBoxEditElfAction_DeleteRunpath,
    kVBoxEditElfAction_ChangeRunpath,
    kVBoxEditElfAction_CreateLinkerStub,
    kVBoxEditElfAction_CreateJsonStub,
    kVBoxEditElfAction_CreateLinkerStubFromJson
}                 g_enmAction = kVBoxEditElfAction_Nothing;
static const char *g_pszInput = NULL;
/** Verbosity level. */
static int        g_cVerbosity = 0;
/** New runpath. */
static const char *g_pszRunpath = NULL;
/** The output path for the stub library. */
static const char *g_pszLinkerStub = NULL;
/** @} */

static const char s_achShStrTab[] = "\0.shstrtab\0.dynsym\0.dynstr\0.gnu.version\0.gnu.version_d\0.dynamic";


static void verbose(const char *pszFmt, ...)
{
    if (g_cVerbosity == 0)
        return;

    va_list Args;
    va_start(Args, pszFmt);
    RTMsgInfoV(pszFmt, Args);
    va_end(Args);
}


static const char *elfEditImgAddString(PELFEDITSTUBIMG pStubImg, const char *pszString)
{
    PELFEDITSTR pStr = (PELFEDITSTR)RTStrSpaceGet(&pStubImg->StrSpace, pszString);
    if (pStr)
        return pStr->StrCore.pszString;

    /* Allocate and insert. */
    size_t const cchStr = strlen(pszString);
    pStr = (PELFEDITSTR)RTMemAlloc(sizeof(*pStr) + cchStr + 1);
    if (!pStr)
        return NULL;

    pStr->StrCore.cchString = cchStr;
    pStr->StrCore.pszString = (char *)memcpy(pStr + 1, pszString, cchStr + 1);
    pStr->offStrTab         = 0;
    if (!RTStrSpaceInsert(&pStubImg->StrSpace, &pStr->StrCore))
        AssertFailed();
    pStubImg->cbStrTab     += cchStr + 1; /* Include zero terminator. */

    return pStr->StrCore.pszString;
}


static uint32_t elfEditImgGetStrIdxInStrTab(PELFEDITSTUBIMG pStubImg, const char *pszString)
{
    PELFEDITSTR pStr = (PELFEDITSTR)RTStrSpaceGet(&pStubImg->StrSpace, pszString);
    AssertRelease(pStr && pStr->offStrTab > 0);

    return (uint32_t)pStr->offStrTab;
}


typedef struct ELFEDITSTRTABCTX
{
    /* Current offset. */
    size_t          off;
    /** Pointer to the base of the string table. */
    char           *pachStrTab;
} ELFEDITSTRTABCTX, *PELFEDITSTRTABCTX;


static DECLCALLBACK(int) elfEditImgStrTabWorker(PRTSTRSPACECORE pStr, void *pvUser)
{
    PELFEDITSTRTABCTX pCtx = (PELFEDITSTRTABCTX)pvUser;
    PELFEDITSTR pElfStr    = (PELFEDITSTR)pStr;

    memcpy(&pCtx->pachStrTab[pCtx->off], pStr->pszString, pStr->cchString + 1);
    pElfStr->offStrTab = pCtx->off;
    pCtx->off += pStr->cchString + 1;
    return VINF_SUCCESS;
}


static int elfEditImgStrTabGenerate(PELFEDITSTUBIMG pStubImg, void **ppvStrTab, size_t *pcbStrTab)
{
    /* Go over all strings and generate the offsets. */
    char *pachStrTab = (char *)RTMemAllocZ(pStubImg->cbStrTab + 1); /* First entry containing an empty string. */
    if (!pachStrTab)
        return VERR_NO_MEMORY;

    ELFEDITSTRTABCTX Ctx;

    Ctx.off        = 1;
    Ctx.pachStrTab = pachStrTab;
    pachStrTab[0] = '\0';
    RTStrSpaceEnumerate(&pStubImg->StrSpace, elfEditImgStrTabWorker, &Ctx);
    AssertRelease(Ctx.off == pStubImg->cbStrTab + 1);
    *ppvStrTab = pachStrTab;
    *pcbStrTab = pStubImg->cbStrTab + 1;
    return VINF_SUCCESS;
}


static int elfEditImgVerdefGenerate(PELFEDITSTUBIMG pStubImg, void **ppvVerdef, size_t *pcbVerdef)
{
    /* Allocate maximum possible amount. */
    uint8_t *pbVerdef = (uint8_t *)RTMemAllocZ(pStubImg->cVersions * (sizeof(Elf_Verdef) + 2 * sizeof(Elf_Verdaux))); /* 2 Parents maximum supported right now. */
    if (!pbVerdef)
        return VERR_NO_MEMORY;

    uint8_t *pbCur = pbVerdef;
    size_t offVerdef = 0;

    AssertRelease(!pStubImg->paVersions[0].pszVersion);
    for (uint32_t i = 1; i < pStubImg->cVersions; i++)
    {
        PCELFEDITSTUBVERSION pVersion = &pStubImg->paVersions[i];
        Elf_Verdef *pVerdef = (Elf_Verdef *)pbCur;

        AssertRelease(pVersion->pszVersion);

        pVerdef->vd_version = 1;
        pVerdef->vd_flags   = 0; /** @todo 1 for BASE */
        pVerdef->vd_ndx     = pVersion->idxVersion;
        pVerdef->vd_cnt     = pVersion->pszParent ? 2 : 1;
        pVerdef->vd_hash    = elfHash(pVersion->pszVersion);
        pVerdef->vd_aux     = sizeof(*pVerdef);
        pVerdef->vd_next    =   i == pStubImg->cVersions - 1
                              ? 0 /* No further entry if this is the last version being processed. */
                              : pVerdef->vd_aux + pVerdef->vd_cnt * sizeof(Elf_Verdaux);

        offVerdef += sizeof(*pVerdef);
        pbCur     += sizeof(*pVerdef);

        Elf_Verdaux *pVerdaux = (Elf_Verdaux *)pbCur;
        pVerdaux->vda_name = elfEditImgGetStrIdxInStrTab(pStubImg, pVersion->pszVersion);
        pVerdaux->vda_next = pVersion->pszParent ? sizeof(*pVerdaux) : 0; /* No parent, jsut a single entry. */

        offVerdef += sizeof(*pVerdaux);
        pbCur     += sizeof(*pVerdaux);

        if (pVersion->pszParent)
        {
            pVerdaux = (Elf_Verdaux *)pbCur;
            pVerdaux->vda_name = elfEditImgGetStrIdxInStrTab(pStubImg, pVersion->pszParent);
            pVerdaux->vda_next = 0; /* Last entry. */

            offVerdef += sizeof(*pVerdaux);
            pbCur     += sizeof(*pVerdaux);
        }
    }

    *ppvVerdef = pbVerdef;
    *pcbVerdef = offVerdef;
    return VINF_SUCCESS;
}


/* Select ELF mode and include the template. */
#define ELF_MODE            32
#include "VBoxEditElf-template.cpp.h"
#undef ELF_MODE


#define ELF_MODE            64
#include "VBoxEditElf-template.cpp.h"
#undef ELF_MODE

static RTEXITCODE elfEditSetup(const char *pszInput, bool fReadonly, PRTFILE phFileElf, bool *pf32Bit)
{
    uint64_t fOpenFlags = RTFILE_O_OPEN | RTFILE_O_DENY_NONE;
    if (fReadonly)
        fOpenFlags |= RTFILE_O_READ;
    else
        fOpenFlags |= RTFILE_O_READWRITE;

    RTFILE hFileElf = NIL_RTFILE;
    int rc = RTFileOpen(&hFileElf, pszInput, fOpenFlags);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Filed to open file '%s': %Rrc\n", pszInput, rc);

    /*
     * Read the ident to decide if this is 32-bit or 64-bit
     * and worth dealing with.
     */
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    uint8_t e_ident[EI_NIDENT];
    rc = RTFileReadAt(hFileElf, 0, &e_ident, sizeof(e_ident), NULL);
    if (RT_SUCCESS(rc))
    {
        if (    e_ident[EI_MAG0] == ELFMAG0
            &&  e_ident[EI_MAG1] == ELFMAG1
            &&  e_ident[EI_MAG2] == ELFMAG2
            &&  e_ident[EI_MAG3] == ELFMAG3
            &&  (   e_ident[EI_CLASS] == ELFCLASS32
                 || e_ident[EI_CLASS] == ELFCLASS64)
           )
        {
            if (e_ident[EI_DATA] == ELFDATA2LSB)
            {
                *pf32Bit   = e_ident[EI_CLASS] == ELFCLASS32 ? true : false;
                *phFileElf = hFileElf;
                return RTEXITCODE_SUCCESS;
            }
            else
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "%s: ELF endian %x is unsupported", pszInput, e_ident[EI_DATA]);
        }
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "%s: Unsupported/invalid ident %.*Rhxs", pszInput, sizeof(e_ident), e_ident);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF header from '%s': %Rrc\n", pszInput, rc);

    RTFileClose(hFileElf);
    return rcExit;
}


static RTEXITCODE deleteRunpath(const char *pszInput)
{
    RTFILE hFileElf = NIL_RTFILE;
    bool f32Bit = false;
    RTEXITCODE rcExit = elfEditSetup(pszInput, false /*fReadonly*/, &hFileElf, &f32Bit);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        if (f32Bit)
            rcExit = elfEditElf32DeleteRunpath(hFileElf, pszInput);
        else
            rcExit = elfEditElf64DeleteRunpath(hFileElf, pszInput);
        RTFileClose(hFileElf);
    }

    return rcExit;
}


static RTEXITCODE changeRunpath(const char *pszInput, const char *pszRunpath)
{
    RTFILE hFileElf = NIL_RTFILE;
    bool f32Bit = false;
    RTEXITCODE rcExit = elfEditSetup(pszInput, false /*fReadonly*/, &hFileElf, &f32Bit);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        if (f32Bit)
            rcExit = elfEditElf32ChangeRunpath(hFileElf, pszInput, pszRunpath);
        else
            rcExit = elfEditElf64ChangeRunpath(hFileElf, pszInput, pszRunpath);
        RTFileClose(hFileElf);
    }

    return rcExit;
}


static RTEXITCODE elfEditStubLoadFromJsonNeeded(PELFEDITSTUBIMG pStubImg, RTJSONVAL hJsonNeeded, const char *pszInput)
{
    if (RTJsonValueGetType(hJsonNeeded) != RTJSONVALTYPE_ARRAY)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "'Needed' in JSON stub '%s' is not an array\n", pszInput);

    uint32_t cNeeded = 0;
    int rc = RTJsonValueQueryArraySize(hJsonNeeded, &cNeeded);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query number of items in 'Needed' array in JSON stub '%s': %Rrc\n", pszInput, rc);

    pStubImg->papszNeeded = (const char **)RTMemAllocZ(cNeeded * sizeof(const char *));
    if (!pStubImg->papszNeeded)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %u entries for the 'Needed' array\n", cNeeded);
    pStubImg->cNeeded = cNeeded;

    RTJSONIT hJsonIt = NIL_RTJSONIT;
    rc = RTJsonIteratorBeginArray(hJsonNeeded, &hJsonIt);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to process 'Needed' array in JSON stub '%s': %Rrc\n", pszInput, rc);

    for (uint32_t i = 0; i < cNeeded; i++)
    {
        RTJSONVAL hJsonValNeeded = NIL_RTJSONVAL;

        rc = RTJsonIteratorQueryValue(hJsonIt, &hJsonValNeeded, NULL /*ppszName*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to get entry %u in 'Needed' array in JSON stub '%s': %Rrc\n",
                                  i, pszInput, rc);

        const char *pszNeeded = RTJsonValueGetString(hJsonValNeeded);
        if (!pszNeeded)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Entry %u in 'Needed' array in JSON stub '%s' is not a string\n",
                                  i, pszInput);

        pStubImg->papszNeeded[i] = elfEditImgAddString(pStubImg, pszNeeded);
        if (!pStubImg->papszNeeded[i])
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to clone entry %u in 'Needed' array in JSON stub '%s'\n",
                                  i, pszInput);

        RTJsonValueRelease(hJsonValNeeded);

        rc = RTJsonIteratorNext(hJsonIt);
        if (rc == VERR_JSON_ITERATOR_END)
        {
            Assert(i == cNeeded - 1);
            break;
        }
    }

    RTJsonIteratorFree(hJsonIt);
    return RTEXITCODE_SUCCESS;
}


static int elfEditStubLoadFromJsonAddToStrSpace(PELFEDITSTUBIMG pStubImg, RTJSONVAL hJsonObj, const char *pszMember, const char **ppsz)
{
    RTJSONVAL hJsonVal = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hJsonObj, pszMember, &hJsonVal);
    if (RT_FAILURE(rc))
        return rc;

    const char *pszVal = RTJsonValueGetString(hJsonVal);
    if (!pszVal)
        return VERR_JSON_VALUE_INVALID_TYPE;

    *ppsz = elfEditImgAddString(pStubImg, pszVal);
    RTJsonValueRelease(hJsonVal);
    return *ppsz ? VINF_SUCCESS : VERR_NO_MEMORY;
}


static RTEXITCODE elfEditStubLoadFromJsonVersions(PELFEDITSTUBIMG pStubImg, RTJSONVAL hJsonVersions, const char *pszInput)
{
    if (RTJsonValueGetType(hJsonVersions) != RTJSONVALTYPE_ARRAY)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "'Versions' in JSON stub '%s' is not an array\n", pszInput);

    uint32_t cVersions = 0;
    int rc = RTJsonValueQueryArraySize(hJsonVersions, &cVersions);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query number of items in 'Versions' array in JSON stub '%s': %Rrc\n", pszInput, rc);

    pStubImg->paVersions = (PELFEDITSTUBVERSION)RTMemAllocZ(cVersions * sizeof(*pStubImg->paVersions));
    if (!pStubImg->papszNeeded)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %u entries for the 'Needed' array\n", cVersions);
    pStubImg->cVersions = cVersions;

    RTJSONIT hJsonIt = NIL_RTJSONIT;
    rc = RTJsonIteratorBeginArray(hJsonVersions, &hJsonIt);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to process 'Versions' array in JSON stub '%s': %Rrc\n", pszInput, rc);

    for (uint32_t i = 0; i < cVersions; i++)
    {
        PELFEDITSTUBVERSION pVersion = &pStubImg->paVersions[i];
        RTJSONVAL hJsonVal = NIL_RTJSONVAL;

        rc = RTJsonIteratorQueryValue(hJsonIt, &hJsonVal, NULL /*ppszName*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to get entry %u in 'Versions' array in JSON stub '%s': %Rrc\n",
                                  i, pszInput, rc);

        RTJSONVALTYPE enmType = RTJsonValueGetType(hJsonVal);
        if (   enmType != RTJSONVALTYPE_NULL
            && enmType != RTJSONVALTYPE_OBJECT)
        {
            RTJsonValueRelease(hJsonVal);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Entry %u in 'Versions' array in JSON stub '%s' is not an object or null\n",
                                  i, pszInput);
        }

        pVersion->idxVersion = i;
        if (enmType == RTJSONVALTYPE_NULL)
        {
            pVersion->pszVersion = NULL;
            pVersion->pszParent  = NULL;
        }
        else
        {
            rc = elfEditStubLoadFromJsonAddToStrSpace(pStubImg, hJsonVal, "Version", &pVersion->pszVersion);
            if (RT_FAILURE(rc))
            {
                RTJsonValueRelease(hJsonVal);
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Version' field in entry %u in 'Versions' array in JSON stub '%s': %Rrc\n",
                                      i, pszInput, rc);
            }

            rc = elfEditStubLoadFromJsonAddToStrSpace(pStubImg, hJsonVal, "Parent", &pVersion->pszParent);
            if (RT_FAILURE(rc) && rc != VERR_NOT_FOUND)
            {
                RTJsonValueRelease(hJsonVal);
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Parent' field in entry %u in 'Versions' array in JSON stub '%s': %Rrc\n",
                                      i, pszInput, rc);
            }
        }

        RTJsonValueRelease(hJsonVal);

        rc = RTJsonIteratorNext(hJsonIt);
        if (rc == VERR_JSON_ITERATOR_END)
        {
            Assert(i == cVersions - 1);
            break;
        }
    }

    RTJsonIteratorFree(hJsonIt);
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE elfEditStubLoadFromJsonSymType(RTJSONVAL hJsonObj, ELFEDITSYMTYPE *penmType)
{
    RTJSONVAL hJsonVal = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hJsonObj, "Type", &hJsonVal);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Type' for symbol in 'Symbols' array in JSON stub: %Rrc\n", rc);

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    const char *psz = RTJsonValueGetString(hJsonVal);
    if (psz)
    {
        if (!RTStrCmp(psz, "Function"))
            *penmType = kElfEditSymType_Function;
        else if (!RTStrCmp(psz, "Object"))
            *penmType = kElfEditSymType_Object;
        else if (!RTStrCmp(psz, "Tls"))
            *penmType = kElfEditSymType_Tls;
        else if (!RTStrCmp(psz, "GnuIFunc"))
            *penmType = kElfEditSymType_GnuIndFunc;
        else if (!RTStrCmp(psz, "NoType"))
            *penmType = kElfEditSymType_NoType;
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "'%s' is not a valid type for 'Type' for symbol in 'Symbols' array in JSON stub\n", psz);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "'Type' for symbol in 'Symbols' array in JSON stub is not a string\n");

    RTJsonValueRelease(hJsonVal);
    return rcExit;
}


static RTEXITCODE elfEditStubLoadFromJsonSymVisibility(RTJSONVAL hJsonObj, ELFEDITSYMVISIBILITY *penmVisibility)
{
    RTJSONVAL hJsonVal = NIL_RTJSONVAL;
    int rc = RTJsonValueQueryByName(hJsonObj, "Visibility", &hJsonVal);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Visibility' for symbol in 'Symbols' array in JSON stub: %Rrc\n", rc);

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    const char *psz = RTJsonValueGetString(hJsonVal);
    if (psz)
    {
        if (!RTStrCmp(psz, "Default"))
            *penmVisibility = kElfEditSymVisbility_Default;
        else if (!RTStrCmp(psz, "Hidden"))
            *penmVisibility = kElfEditSymVisbility_Hidden;
        else if (!RTStrCmp(psz, "Internal"))
            *penmVisibility = kElfEditSymVisbility_Internal;
        else if (!RTStrCmp(psz, "Protected"))
            *penmVisibility = kElfEditSymVisbility_Protected;
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "'%s' is not a valid visbility for 'Visibility' for symbol in 'Symbols' array in JSON stub\n", psz);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "'Visibility' for symbol in 'Symbols' array in JSON stub is not a string\n");

    RTJsonValueRelease(hJsonVal);
    return rcExit;
}


static RTEXITCODE elfEditStubLoadFromJsonSymbols(PELFEDITSTUBIMG pStubImg, RTJSONVAL hJsonSyms, const char *pszInput)
{
    if (RTJsonValueGetType(hJsonSyms) != RTJSONVALTYPE_ARRAY)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "'Symbols' in JSON stub '%s' is not an array\n", pszInput);

    uint32_t cSyms = 0;
    int rc = RTJsonValueQueryArraySize(hJsonSyms, &cSyms);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query number of items in 'Symbols' array in JSON stub '%s': %Rrc\n", pszInput, rc);

    pStubImg->paSyms = (PELFEDITSTUBSYM)RTMemAllocZ(cSyms * sizeof(*pStubImg->paSyms));
    if (!pStubImg->paSyms)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %u entries for the symbol table\n", cSyms);
    pStubImg->cSyms = cSyms;

    RTJSONIT hJsonIt = NIL_RTJSONIT;
    rc = RTJsonIteratorBeginArray(hJsonSyms, &hJsonIt);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to process 'Symbols' array in JSON stub '%s': %Rrc\n", pszInput, rc);

    for (uint32_t i = 0; i < cSyms; i++)
    {
        PELFEDITSTUBSYM pSym = &pStubImg->paSyms[i];
        RTJSONVAL hJsonVal = NIL_RTJSONVAL;

        rc = RTJsonIteratorQueryValue(hJsonIt, &hJsonVal, NULL /*ppszName*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to get entry %u in 'Versions' array in JSON stub '%s': %Rrc\n",
                                  i, pszInput, rc);

        RTJSONVALTYPE enmType = RTJsonValueGetType(hJsonVal);
        if (enmType != RTJSONVALTYPE_OBJECT)
        {
            RTJsonValueRelease(hJsonVal);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Entry %u in 'Versions' array in JSON stub '%s' is not an object or null\n",
                                  i, pszInput);
        }

        rc = elfEditStubLoadFromJsonAddToStrSpace(pStubImg, hJsonVal, "Name", &pSym->pszName);
        if (RT_FAILURE(rc))
        {
            RTJsonValueRelease(hJsonVal);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Name' field in entry %u in 'Symbols' array in JSON stub '%s': %Rrc\n",
                                  i, pszInput, rc);
        }

        RTEXITCODE rcExit = elfEditStubLoadFromJsonSymType(hJsonVal, &pSym->enmType);
        if (rcExit != RTEXITCODE_SUCCESS)
        {
            RTJsonValueRelease(hJsonVal);
            return rcExit;
        }

        rcExit = elfEditStubLoadFromJsonSymVisibility(hJsonVal, &pSym->enmVisibility);
        if (rcExit != RTEXITCODE_SUCCESS)
        {
            RTJsonValueRelease(hJsonVal);
            return rcExit;
        }

        int64_t cbSym = 0;
        rc = RTJsonValueQueryIntegerByName(hJsonVal, "Size", &cbSym);
        if (RT_FAILURE(rc))
        {
            RTJsonValueRelease(hJsonVal);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Size' field in entry %u in 'Symbols' array in JSON stub '%s': %Rrc\n",
                                  i, pszInput, rc);
        }
        pSym->cbSym = (size_t)cbSym;

        rc = RTJsonValueQueryBooleanByName(hJsonVal, "Weak", &pSym->fWeak);
        if (RT_FAILURE(rc))
        {
            RTJsonValueRelease(hJsonVal);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Weak' field in entry %u in 'Symbols' array in JSON stub '%s': %Rrc\n",
                                  i, pszInput, rc);
        }

        int64_t idxVersion = 0;
        rc = RTJsonValueQueryIntegerByName(hJsonVal, "Version", &idxVersion);
        if (RT_FAILURE(rc))
        {
            RTJsonValueRelease(hJsonVal);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Version' field in entry %u in 'Symbols' array in JSON stub '%s': %Rrc\n",
                                  i, pszInput, rc);
        }
        if (idxVersion < 0 || idxVersion >= (int64_t)pStubImg->cVersions)
        {
            RTJsonValueRelease(hJsonVal);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "'Version' %RI64 in entry %u in 'Symbols' array in JSON stub '%s' is out of bounds [0, %u[\n",
                                  idxVersion, i, pszInput, pStubImg->cVersions);
        }
        pSym->pVersion = &pStubImg->paVersions[idxVersion];

        rc = RTJsonValueQueryBooleanByName(hJsonVal, "Default", &pSym->fDefaultVersion);
        if (RT_FAILURE(rc))
        {
            RTJsonValueRelease(hJsonVal);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Default' field in entry %u in 'Symbols' array in JSON stub '%s': %Rrc\n",
                                  i, pszInput, rc);
        }

        RTJsonValueRelease(hJsonVal);

        rc = RTJsonIteratorNext(hJsonIt);
        if (rc == VERR_JSON_ITERATOR_END)
        {
            Assert(i == cSyms - 1);
            break;
        }
    }

    RTJsonIteratorFree(hJsonIt);
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE elfEditStubLoadFromJson(PELFEDITSTUBIMG pStubImg, const char *pszInput)
{
    RTERRINFOSTATIC ErrInfo;
    RTErrInfoInitStatic(&ErrInfo);

    RTJSONVAL hJsonRoot = NIL_RTJSONVAL;
    int rc = RTJsonParseFromFile(&hJsonRoot, RTJSON_PARSE_F_JSON5, pszInput, &ErrInfo.Core);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to load JSON stub '%s': %Rrc (%s)\n", pszInput, rc, ErrInfo.Core.pszMsg);

    char *pszTmp;
    rc = RTJsonValueQueryStringByName(hJsonRoot, "Target", &pszTmp);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Target' from JSON stub '%s': %Rrc\n", pszInput, rc);

    if (!RTStrICmp(pszTmp, "arm64"))
        pStubImg->enmArch = RTLDRARCH_ARM64;
    else if (!RTStrICmp(pszTmp, "amd64"))
        pStubImg->enmArch = RTLDRARCH_AMD64;
    else if (!RTStrICmp(pszTmp, "x86"))
        pStubImg->enmArch = RTLDRARCH_X86_32;
    else
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "'%s' is not a valid target in JSON stub '%s'\n", pszTmp, pszInput);

    RTStrFree(pszTmp);

    rc = RTJsonValueQueryStringByName(hJsonRoot, "SoName", &pszTmp);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'SoName' from JSON stub '%s': %Rrc\n", pszInput, rc);

    pStubImg->pszName = elfEditImgAddString(pStubImg, pszTmp);
    if (!pStubImg->pszName)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to add '%s' to string table\n", pszTmp);
    RTStrFree(pszTmp);

    RTJSONVAL hJsonArr = NIL_RTJSONVAL;
    rc = RTJsonValueQueryByName(hJsonRoot, "Needed", &hJsonArr);
    if (RT_FAILURE(rc) && rc != VERR_NOT_FOUND)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Needed' from JSON stub '%s': %Rrc\n", pszInput, rc);

    RTEXITCODE rcExit = elfEditStubLoadFromJsonNeeded(pStubImg, hJsonArr, pszInput);
    RTJsonValueRelease(hJsonArr);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    rc = RTJsonValueQueryByName(hJsonRoot, "Versions", &hJsonArr);
    if (RT_FAILURE(rc) && rc != VERR_NOT_FOUND)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Versions' from JSON stub '%s': %Rrc\n", pszInput, rc);

    rcExit = elfEditStubLoadFromJsonVersions(pStubImg, hJsonArr, pszInput);
    RTJsonValueRelease(hJsonArr);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    rc = RTJsonValueQueryByName(hJsonRoot, "Symbols", &hJsonArr);
    if (RT_FAILURE(rc) && rc != VERR_NOT_FOUND)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to query 'Symbols' from JSON stub '%s': %Rrc\n", pszInput, rc);

    rcExit = elfEditStubLoadFromJsonSymbols(pStubImg, hJsonArr, pszInput);
    RTJsonValueRelease(hJsonArr);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    RTJsonValueRelease(hJsonRoot);
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE elfEditStubDumpToJson(PELFEDITSTUBIMG pStubImg, const char *pszOutput)
{
    PRTSTREAM pStrm = NULL;
    int rc = RTStrmOpen(pszOutput, "wt", &pStrm);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create file '%s': %Rrc\n", pszOutput, rc);

    rc = RTStrmPutStr(pStrm, "{\n");

    const char *pszTarget = NULL;
    switch (pStubImg->enmArch)
    {
        case RTLDRARCH_ARM64:  pszTarget = "arm64"; break;
        case RTLDRARCH_AMD64:  pszTarget = "amd64"; break;
        case RTLDRARCH_X86_32: pszTarget = "x86";   break;
        default: RTMsgErrorExit(RTEXITCODE_FAILURE,
                                "Unsupported architecture %u\n", pStubImg->enmArch);
    }

    rc |= RTStrmPrintf(pStrm, "    Target: '%s',\n", pszTarget);
    rc |= RTStrmPrintf(pStrm, "    SoName: '%s',\n", pStubImg->pszName);

    if (pStubImg->papszNeeded)
    {
        /* Find needed libraries. */
        rc |= RTStrmPutStr(pStrm, "    Needed:\n    [\n");
        for (uint32_t i = 0; i < pStubImg->cNeeded; i++)
            rc |= RTStrmPrintf(pStrm, "        '%s',\n", pStubImg->papszNeeded[i]);
        rc |= RTStrmPutStr(pStrm, "    ],\n");
    }

    /* Dump versions */
    if (pStubImg->paVersions)
    {
        rc |= RTStrmPutStr(pStrm, "    Versions:\n    [\n");
        for (uint32_t i = 0; i < pStubImg->cVersions; i++)
        {
            PCELFEDITSTUBVERSION pVersion = &pStubImg->paVersions[i];
            Assert(i == pVersion->idxVersion);

            if (!pVersion->pszVersion)
                rc |= RTStrmPutStr(pStrm, "        null,\n");
            else
            {
                rc |= RTStrmPrintf(pStrm, "        { Version: '%s'", pVersion->pszVersion);
                if (pVersion->pszParent)
                    rc |= RTStrmPrintf(pStrm, ", Parent: '%s'", pVersion->pszParent);
                rc |= RTStrmPutStr(pStrm, " },\n");
            }
        }
        rc |= RTStrmPutStr(pStrm, "    ],\n");
    }

    /* Dump symbols. */
    rc |= RTStrmPutStr(pStrm, "    Symbols:\n    [\n");

    for (uint32_t i = 0; i < pStubImg->cSyms; i++)
    {
        PELFEDITSTUBSYM pSym = &pStubImg->paSyms[i];
        const char *pszType = NULL;
        switch (pSym->enmType)
        {
            case kElfEditSymType_NoType:     pszType = "NoType";      break;
            case kElfEditSymType_Object:     pszType = "Object";      break;
            case kElfEditSymType_Function:   pszType = "Function";    break;
            case kElfEditSymType_Tls:        pszType = "Tls";         break;
            case kElfEditSymType_GnuIndFunc: pszType = "GnuIFunc";    break;
            default:                         pszType = "UNSUPPORTED"; break;
        }

        const char *pszVisibilty = NULL;
        switch (pSym->enmVisibility)
        {
            case kElfEditSymVisbility_Default:   pszVisibilty = "Default";     break;
            case kElfEditSymVisbility_Internal:  pszVisibilty = "Internal";    break;
            case kElfEditSymVisbility_Hidden:    pszVisibilty = "Hidden";      break;
            case kElfEditSymVisbility_Protected: pszVisibilty = "Protected";   break;
            default:                             pszVisibilty = "UNSUPPORTED"; break;
        }

        RTStrmPrintf(pStrm,
                     "        { Name: '%s', Type: '%s', Visibility: '%s', Weak: %RTbool, Size: %RU64",
                     pSym->pszName, pszType, pszVisibilty, pSym->fWeak, pSym->cbSym);

        if (pSym->pVersion)
        {
            RTStrmPrintf(pStrm, ", Version: %RU16, Default: %RTbool",
                         pSym->pVersion->idxVersion, pSym->fDefaultVersion);
        }

        rc |= RTStrmPutStr(pStrm, " },\n");
    }
    rc |= RTStrmPutStr(pStrm, "    ],\n");

    rc |= RTStrmPutStr(pStrm, "}\n");
    rc |= RTStrmClose(pStrm);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write file '%s': %Rrc\n", pszOutput, rc);

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE elfEditCreateStub(const char *pszInput, bool fJsonInput, const char *pszOutput, bool fJsonOutput)
{
    ELFEDITSTUBIMG StubImg; RT_ZERO(StubImg);

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    if (!fJsonInput)
    {
        RTFILE hFileElf = NIL_RTFILE;
        bool f32Bit = false;
        rcExit = elfEditSetup(pszInput, true /*fReadonly*/, &hFileElf, &f32Bit);
        if (rcExit == RTEXITCODE_SUCCESS)
        {
            if (f32Bit)
                rcExit = elfEditElf32Parse(&StubImg, hFileElf);
            else
                rcExit = elfEditElf64Parse(&StubImg, hFileElf);
            RTFileClose(hFileElf);
        }
    }
    else
        rcExit = elfEditStubLoadFromJson(&StubImg, pszInput);

    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    if (fJsonOutput)
        rcExit = elfEditStubDumpToJson(&StubImg, pszOutput);
    else
    {
        if (StubImg.enmArch == RTLDRARCH_X86_32)
            rcExit = elfEditElf32GenerateStub(&StubImg, pszOutput);
        else
            rcExit = elfEditElf64GenerateStub(&StubImg, pszOutput);
    }

    return rcExit;
}


/**
 * Display usage
 *
 * @returns success if stdout, syntax error if stderr.
 */
static RTEXITCODE usage(FILE *pOut, const char *argv0)
{
    fprintf(pOut,
            "usage: %s --input <input binary> [options and operations]\n"
            "\n"
            "Operations and Options (processed in place):\n"
            "  --verbose                                        Noisier.\n"
            "  --quiet                                          Quiet execution.\n"
            "  --delete-runpath                                 Deletes all DT_RUNPATH entries.\n"
            "  --change-runpath <new runpath>                   Changes the first DT_RUNPATH entry to the new one.\n"
            "  --create-stub-library <path/to/stub>             Creates a stub library used for linking.\n"
            "  --create-stub-json <path/to/stub>                Creates a stub JSON file which can be edited and turned into a linker stub.\n"
            "  --create-linker-stub-from-json <path/to/stub>    Creates an ELF shared object linker stub from JSON file.\n"
            , argv0);
    return pOut == stdout ? RTEXITCODE_SUCCESS : RTEXITCODE_SYNTAX;
}


/**
 * Parses the arguments.
 */
static RTEXITCODE parseArguments(int argc,  char **argv)
{
    /*
     * Option config.
     */
    static RTGETOPTDEF const s_aOpts[] =
    {
        { "--input",                            'i', RTGETOPT_REQ_STRING  },
        { "--verbose",                          'v', RTGETOPT_REQ_NOTHING },
        { "--delete-runpath",                   'd', RTGETOPT_REQ_NOTHING },
        { "--change-runpath",                   'c', RTGETOPT_REQ_STRING  },
        { "--create-stub-library",              's', RTGETOPT_REQ_STRING  },
        { "--create-stub-json",                 'j', RTGETOPT_REQ_STRING  },
        { "--create-linker-stub-from-json",     'l', RTGETOPT_REQ_STRING}
    };

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    int rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, RTEXITCODE_FAILURE);

    /*
     * Process the options.
     */
    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'h':
                return usage(stdout, argv[0]);

            case 'i':
                if (g_pszInput)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Input file is already set to '%s'", g_pszInput);
                g_pszInput = ValueUnion.psz;
                break;

            case 'v':
                g_cVerbosity++;
                break;

            case 'd':
                g_enmAction = kVBoxEditElfAction_DeleteRunpath;
                break;

            case 'c':
                g_enmAction = kVBoxEditElfAction_ChangeRunpath;
                g_pszRunpath = ValueUnion.psz;
                break;

            case 's':
                g_enmAction = kVBoxEditElfAction_CreateLinkerStub;
                g_pszLinkerStub = ValueUnion.psz;
                break;

            case 'j':
                g_enmAction = kVBoxEditElfAction_CreateJsonStub;
                g_pszLinkerStub = ValueUnion.psz;
                break;

            case 'l':
                g_enmAction = kVBoxEditElfAction_CreateLinkerStubFromJson;
                g_pszLinkerStub = ValueUnion.psz;
                break;

            case 'V':
            {
                /* The following is assuming that svn does it's job here. */
                static const char s_szRev[] = "$Revision: 170441 $";
                const char *psz = RTStrStripL(strchr(s_szRev, ' '));
                RTPrintf("r%.*s\n", strchr(psz, ' ') - psz, psz);
                return RTEXITCODE_SUCCESS;
            }

            /*
             * Errors and bugs.
             */
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Check that we've got all we need.
     */
    if (g_enmAction == kVBoxEditElfAction_Nothing)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No action specified (--delete-runpath, --change-runpath or --create-stub-library)");
    if (!g_pszInput)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No input file specified (--input)");

    return RTEXITCODE_SUCCESS;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return 1;

    RTEXITCODE rcExit = parseArguments(argc, argv);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Take action.
         */
        if (g_enmAction == kVBoxEditElfAction_DeleteRunpath)
            rcExit = deleteRunpath(g_pszInput);
        else if (g_enmAction == kVBoxEditElfAction_ChangeRunpath)
            rcExit = changeRunpath(g_pszInput, g_pszRunpath);
        else if (g_enmAction == kVBoxEditElfAction_CreateLinkerStub)
            rcExit = elfEditCreateStub(g_pszInput, false, g_pszLinkerStub, false);
        else if (g_enmAction == kVBoxEditElfAction_CreateJsonStub)
            rcExit = elfEditCreateStub(g_pszInput, false, g_pszLinkerStub, true);
        else if (g_enmAction == kVBoxEditElfAction_CreateLinkerStubFromJson)
            rcExit = elfEditCreateStub(g_pszInput, true, g_pszLinkerStub, false);
    }

    return rcExit;
}

