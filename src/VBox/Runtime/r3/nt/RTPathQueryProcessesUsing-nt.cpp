/* $Id: RTPathQueryProcessesUsing-nt.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * IPRT - RTPathQueryProcessesUsing, Native NT.
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
#define LOG_GROUP RTLOGGROUP_FILE
#include <iprt/nt/nt-and-windows.h>
#include "internal-r3-nt.h"

#include <iprt/path.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/utf16.h>
#include "internal/fs.h"
#include "internal/path.h"


/**
 * Resolves the given path (usually something like C:\... to the final NT path).
 *
 * @returns IPRT status code.
 * @param   pszPath     The path to resolve.
 * @param   pNtName     Where to store the NT name, free with RTNtPathFree();
 */
static int rtNtPathResolveFinal(const char *pszPath, struct _UNICODE_STRING *pNtName)
{
    /*
     * RTNtPathFromWinUtf8() will usually convert something like C:\Windows to \??\C:\Windows.
     * while we need the real NT path later on.
     * So after the first conversion we have to open the object and get the final path from the
     * handle.
     */
    HANDLE         hRootDir;
    UNICODE_STRING NtName;
    int rcRet = RTNtPathFromWinUtf8(&NtName, &hRootDir, pszPath);
    if (RT_FAILURE(rcRet))
        return rcRet;

    HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    OBJECT_ATTRIBUTES   ObjAttr;

    /* Try open the path. */
    InitializeObjectAttributes(&ObjAttr, &NtName, OBJ_CASE_INSENSITIVE, hRootDir, NULL);
    NTSTATUS rcNt = NtCreateFile(&hFile,
                                 FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                                 &ObjAttr,
                                 &Ios,
                                 NULL /*pcbFile*/,
                                 FILE_ATTRIBUTE_NORMAL,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 FILE_OPEN,
                                 FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT,
                                 NULL /*pvEaBuffer*/,
                                 0 /*cbEa*/);
    if (NT_SUCCESS(rcNt))
    {
        rcRet = RTNtPathFromHandle(pNtName, hFile, 0);
        NtClose(hFile);
        RTNtPathFree(&NtName, &hRootDir);
        return rcRet;
    }

    /* Failed to open, so return the name from RTNtPathFromWinUtf8. */
    *pNtName = NtName;
    return VINF_SUCCESS;
}


DECLINLINE(bool) rtNtPathMatchName(UNICODE_STRING const *pThisNtName, uint32_t fFlags, UNICODE_STRING const *pNtName)
{
    if (  fFlags & RTPATH_QUERY_PROC_F_DIR_INCLUDE_SUB_OBJ
        ? pNtName->Length <= pThisNtName->Length
        : pNtName->Length == pThisNtName->Length)
        return RTUtf16NICmp(pNtName->Buffer, pThisNtName->Buffer, pNtName->Length / sizeof(RTUTF16)) == 0;
    return false;
}


static bool rtNtPathScanProcessMappings(HANDLE hProcess, uint32_t fFlags, UNICODE_STRING const *pNtName)
{
    uintptr_t   uPrevBase = ~(uintptr_t)0;
    uintptr_t   uPtrWhere = 0;
    for (uint32_t i = 0; i < 16384; i++)
    {
        SIZE_T                      cbActual = 0;
        MEMORY_BASIC_INFORMATION    MemInfo  = { 0, 0, 0, 0, 0, 0, 0 };
        NTSTATUS rcNt = NtQueryVirtualMemory(hProcess,
                                             (void const *)uPtrWhere,
                                             MemoryBasicInformation,
                                             &MemInfo,
                                             sizeof(MemInfo),
                                             &cbActual);
        if (!NT_SUCCESS(rcNt))
            break;

        /*
         * Image mapping?
         */
        if (    (uintptr_t)MemInfo.BaseAddress != uPrevBase
            && (MemInfo.Type & (SEC_IMAGE | SEC_PROTECTED_IMAGE)) )
        {
            struct
            {
                /** The full unicode name. */
                UNICODE_STRING  UniStr;
                /** Buffer space. */
                WCHAR           awcBuffer[1024];
            } ImageName;
            cbActual = 0;
            rcNt = NtQueryVirtualMemory(hProcess, (void const *)uPtrWhere, MemorySectionName,
                                        &ImageName, sizeof(ImageName) - sizeof(WCHAR), &cbActual);
            if (NT_SUCCESS(rcNt))
            {
                if (rtNtPathMatchName(&ImageName.UniStr, fFlags, pNtName))
                    return true;
            }
            else
                AssertMsgFailed(("rcNt=%#x uPtrWhere=%p\n", rcNt, uPtrWhere));
            uPrevBase = (uintptr_t)MemInfo.BaseAddress;
        }

        /*
         * Advance (a bit paranoid).
         */
        uintptr_t const uPtrNext = uPtrWhere + MemInfo.RegionSize;
        if (uPtrNext > uPtrWhere)
            uPtrWhere = uPtrNext;
        else
            break;
    }

    return false;
}


static bool rtNtPathScanProcessHandles(HANDLE hProcess, uint32_t fFlags, UNICODE_STRING const *pNtName,
                                       ULONG_PTR cHandles, SYSTEM_HANDLE_ENTRY_INFO_EX const *paHandles)
{
    for (ULONG_PTR i = 0; i < cHandles; i++)
    {
        /*
         * Duplicate the handle into our process, get the path handle if it's a file
         * and check if it matches the request.
         */
        HANDLE hDup;
        HANDLE hProcSelf = NtCurrentProcess();
        NTSTATUS rcNt = NtDuplicateObject(hProcess, paHandles[i].HandleValue, hProcSelf, &hDup, SYNCHRONIZE, 0, 0);
        if (NT_SUCCESS(rcNt))
        {
            /* Continue if this is a disk file object as RTNtPathFromHandle (calling NtQueryObject()) might hang for anything else. */
#if 0 /** @todo This also triggers for named pipes, causing RTNtPathFromHandle() to get stuck again, investigate what GetFileType() is doing. */
            union
            {
                OBJECT_TYPE_INFORMATION TypeInfo;
                uint8_t                 ab[128];
            } ObjTypeInfo;
            rcNt = NtQueryObject(hDup, ObjectTypeInformation,
                                 &ObjTypeInfo, sizeof(ObjTypeInfo), NULL);
            if (   NT_SUCCESS(rcNt)
                && ObjTypeInfo.TypeInfo.TypeName.Length == sizeof(L"File") - sizeof(wchar_t)
                && memcmp(ObjTypeInfo.TypeInfo.TypeName.Buffer, L"File", sizeof(L"File") - sizeof(wchar_t)) == 0)
#else
            if (GetFileType(hDup) == FILE_TYPE_DISK)
#endif
            {
                UNICODE_STRING NtNameDup; RT_ZERO(NtNameDup);
                int rc2 = RTNtPathFromHandle(&NtNameDup, hDup, 0);
                if (RT_SUCCESS(rc2))
                {
                    bool const fMatch = rtNtPathMatchName(&NtNameDup, fFlags, pNtName);
                    RTNtPathFree(&NtNameDup, NULL);
                    if (fMatch)
                    {
                        NtClose(hDup);
                        return true;
                    }
                }
            }

            NtClose(hDup);
        }
    }
    return false;
}



RTR3DECL(int) RTPathQueryProcessesUsing(const char *pszPath, uint32_t fFlags, uint32_t *pcProcesses, PRTPROCESS paPids)
{
    AssertReturn(!(fFlags & ~RTPATH_QUERY_PROC_F_VALID_MASK), VERR_INVALID_FLAGS);
    uint32_t const cMaxProcesses = *pcProcesses;

    /*
     * Convert the input path.
     */
    UNICODE_STRING NtName;
    int rcRet = rtNtPathResolveFinal(pszPath, &NtName);
    if (RT_FAILURE(rcRet))
        return rcRet;

    /*
     * Take a snapshot of all the handles in the system.
     */
    uint32_t cbBuf = _256K;
    uint8_t *pbBuf = (uint8_t *)RTMemAlloc(cbBuf);
    if (!pbBuf)
    {
        RTNtPathFree(&NtName, NULL);
        return VERR_NO_MEMORY;
    }

    ULONG cbNeeded = cbBuf;
    NTSTATUS rcNt = NtQuerySystemInformation(SystemExtendedHandleInformation, pbBuf, cbBuf, &cbNeeded);
    if (!NT_SUCCESS(rcNt))
    {
        while (   rcNt == STATUS_INFO_LENGTH_MISMATCH
               && cbNeeded > cbBuf
               && cbBuf <= _32M)
        {
            cbBuf = RT_ALIGN_32(cbNeeded + _4K, _64K);
            RTMemFree(pbBuf);
            pbBuf = (uint8_t *)RTMemAlloc(cbBuf);
            if (!pbBuf)
            {
                RTNtPathFree(&NtName, NULL);
                return VERR_NO_MEMORY;
            }
            rcNt = NtQuerySystemInformation(SystemExtendedHandleInformation, pbBuf, cbBuf, &cbNeeded);
        }
        if (!NT_SUCCESS(rcNt))
        {
            RTMemFree(pbBuf);
            RTNtPathFree(&NtName, NULL);
            return RTErrConvertFromNtStatus(rcNt);
        }
    }

    /*
     * Examine the snapshot.
     *
     * Since the exe and dll images are typically not part of the listing, we
     * also scan the process memory to locate image sections.  ASSUMING that
     * the handles are chunked by PID (which they kind of have to be, since
     * the kernel have to examine each process' handle table in turn), the
     * outer loop will do blocks of handles belonging to the same process.
     */
    rcRet = VINF_SUCCESS;

    uint32_t                            cProcesses = 0;
    HANDLE const                        idProcess  = fFlags & RTPATH_QUERY_PROC_F_EXCLUDE_SELF
                                                   ? RTNtCurrentTeb()->ClientId.UniqueProcess : INVALID_HANDLE_VALUE;
    SYSTEM_HANDLE_INFORMATION_EX const *pInfo      = (SYSTEM_HANDLE_INFORMATION_EX const *)pbBuf;
    ULONG_PTR                           i          = pInfo->NumberOfHandles;
    AssertRelease(RT_UOFFSETOF_DYN(SYSTEM_HANDLE_INFORMATION_EX, Handles[i]) == cbNeeded);
    while (i-- > 0)
    {
        /*
         * Determin how many handles we've got for this process.
         */
        ULONG_PTR const iLast        = i;
        HANDLE const    idCurProcess = pInfo->Handles[i].UniqueProcessId;
        while (i > 0 && pInfo->Handles[i - 1].UniqueProcessId == idCurProcess)
            i--;

        if (idCurProcess != idProcess)
        {
            /*
             * Open the process.
             *
             * Try with full PROCESS_QUERY_INFORMATION first since we probably
             * need that for the VA enumeration, then fall back on
             * PROCESS_QUERY_LIMITED_INFORMATION if that fails.
             */
            CLIENT_ID  ClientId;
            ClientId.UniqueProcess = idCurProcess;
            ClientId.UniqueThread  = NULL;

            OBJECT_ATTRIBUTES ObjAttrs;
            InitializeObjectAttributes(&ObjAttrs, NULL, OBJ_CASE_INSENSITIVE, NULL, NULL);

            HANDLE hProcess = NULL;
            rcNt = NtOpenProcess(&hProcess, PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, &ObjAttrs, &ClientId);
            if (!NT_SUCCESS(rcNt))
                rcNt = NtOpenProcess(&hProcess, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_DUP_HANDLE, &ObjAttrs, &ClientId);
            if (NT_SUCCESS(rcNt))
            {
                /*
                 * Check if there is a matching handle or if the virtual address
                 * space has a matching image mapping.
                 */
                if (   rtNtPathScanProcessHandles(hProcess, fFlags, &NtName, iLast + 1 - i, &pInfo->Handles[i])
                    || (   !(fFlags & RTPATH_QUERY_PROC_F_SKIP_MAPPINGS)
                        && rtNtPathScanProcessMappings(hProcess, fFlags, &NtName)))
                {
                    if (cProcesses < cMaxProcesses)
                        paPids[cProcesses] = (RTPROCESS)(uintptr_t)idCurProcess;
                    else
                        rcRet = VINF_BUFFER_OVERFLOW;
                    cProcesses++;
                }
                NtClose(hProcess);
            }
        }
    }

    /*
     * Cleanup handle scanning.
     */
    RTMemFree(pbBuf);
    RTNtPathFree(&NtName, NULL);

    *pcProcesses = cProcesses;
    return rcRet;
}

