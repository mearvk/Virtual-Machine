/* $Id: VBoxServiceVMInfo-win.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBoxService - Virtual Machine Information for the Host, Windows specifics.
 */

/*
 * Copyright (C) 2009-2025 Oracle and/or its affiliates.
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
#define WIN32_LEAN_AND_MEAN  /* Must avoid dragging in old winsock.h. */
#include <iprt/nt/nt-and-windows.h>
#include <wtsapi32.h>        /* For WTS* calls. */
#include <sddl.h>            /* For ConvertSidToStringSidW. */
#define UNICODE_STRING  NTSEC_UNICODE_STRING
#define PUNICODE_STRING PNTSEC_UNICODE_STRING
#define STRING          NTSEC_STRING
#define PSTRING         PNTSEC_STRING
#include <Ntsecapi.h>        /* Needed for process security information. */
#undef  UNICODE_STRING
#undef  PUNICODE_STRING
#undef  STRING
#undef  PSTRING
#include <iprt/win/winsock2.h>
#include <iprt/win/iphlpapi.h>

#include <iprt/assert.h>
#include <iprt/ldr.h>
#include <iprt/localipc.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/utf16.h>

#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestPropertySvc.h> /* GUEST_PROP_MAX_NAME_LEN */
#include "VBoxServiceInternal.h"
#include "../../win/VBoxTray/VBoxTrayMsg.h" /* For IPC. */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/* advapi32.dll: */
typedef BOOL (WINAPI *PFNCONVERTSIDTOSTRINGSIDW)(PSID, LPWSTR *);

/* advapi32.dll: */
static PFNCONVERTSIDTOSTRINGSIDW g_pfnConvertSidToStringSidW = NULL;

/** Structure for storing the looked up user information. */
typedef struct VBOXSERVICEVMINFOUSER
{
    WCHAR wszUser[MAX_PATH];
    WCHAR wszAuthenticationPackage[MAX_PATH];
    WCHAR wszLogonDomain[MAX_PATH];
    /** Number of assigned user processes.
     * @note This is only accurate for logging level 3 and higher.  */
    ULONG cInteractiveProcesses;
    /** Last (highest) session ID. This is needed for distinguishing old
     * session process counts from new (current) session ones. */
    ULONG ulLastSession;
} VBOXSERVICEVMINFOUSER, *PVBOXSERVICEVMINFOUSER;

/** Structure for process information lookup. */
typedef struct VBOXSERVICEVMINFOPROC
{
    /** The PID. */
    DWORD id;
    /** The session ID (if available). */
    ULONG idSession;
    /** Pointer to the process name (can be NULL). */
    PUNICODE_STRING pUniStrName;
    /** The SID. */
    PSID pSid;
#if 0 /* unused */
    /** The LUID. */
    LUID luid;
#endif
} VBOXSERVICEVMINFOPROC, *PVBOXSERVICEVMINFOPROC;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static uint32_t vgsvcVMInfoWinCountSessionProcesses(PLUID pSession, PVBOXSERVICEVMINFOPROC const paProcs, DWORD cProcs);
static bool vgsvcVMInfoWinIsLoggedInWithUserInfoReturned(PVBOXSERVICEVMINFOUSER a_pUserInfo, PLUID a_pSession);
static int  vgsvcVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppProc, DWORD *pdwCount);
static void vgsvcVMInfoWinProcessesFree(DWORD cProcs, PVBOXSERVICEVMINFOPROC paProcs);
static int  vgsvcVMInfoWinWriteLastInput(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** @todo r=bird: Is this temporary? (bad orig code quality) If not, docs++, plase. */
static uint32_t g_uDebugIter = 0;
/** Whether to skip the logged-in user detection over RDP or not.
 *  See notes in this section why we might want to skip this. */
static bool     g_fSkipRdpDetection = false;

static RTONCE                                   g_vgsvcWinVmInitOnce = RTONCE_INITIALIZER;

/** @name Secur32.dll imports are dynamically resolved because of NT4.
 * @{ */
static decltype(LsaGetLogonSessionData)        *g_pfnLsaGetLogonSessionData = NULL;
static decltype(LsaEnumerateLogonSessions)     *g_pfnLsaEnumerateLogonSessions = NULL;
static decltype(LsaFreeReturnBuffer)           *g_pfnLsaFreeReturnBuffer = NULL;
/** @} */

/** @name WtsApi32.dll imports are dynamically resolved because of NT4.
 * @{ */
static decltype(WTSFreeMemory)                 *g_pfnWTSFreeMemory = NULL;
static decltype(WTSQuerySessionInformationA)   *g_pfnWTSQuerySessionInformationA = NULL;
/** @} */

/** @name Iphlpapi.dll imports are dynamically resolved to be on the safe side.
 * @{ */
static decltype(CancelIPChangeNotify)          *g_pfnCancelIPChangeNotify = NULL;
static decltype(NotifyAddrChange)              *g_pfnNotifyAddrChange = NULL;
/** @} */


/** S-1-5-4 (leaked). */
static PSID                                     g_pSidInteractive = NULL;
/** S-1-2-0 (leaked). */
static PSID                                     g_pSidLocal = NULL;

/** Indicates whether RTNT_SYSTEM_PROCESS_INFORMATION::SessionId is valid. */
static bool                                     g_fHasProcInfoSessionId;


/**
 * An RTOnce callback function.
 */
static DECLCALLBACK(int) vgsvcWinVmInfoInitOnce(void *pvIgnored)
{
    RT_NOREF1(pvIgnored);
    g_fHasProcInfoSessionId = RTSystemGetNtVersion() >= RTSYSTEM_MAKE_NT_VERSION(5, 0, 0); /* Windows 2000 */

    /* SECUR32 */
    RTLDRMOD hLdrMod;
    int rc = RTLdrLoadSystem("secur32.dll", true /*fNoUnload*/, &hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hLdrMod, "LsaGetLogonSessionData", (void **)&g_pfnLsaGetLogonSessionData);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "LsaEnumerateLogonSessions", (void **)&g_pfnLsaEnumerateLogonSessions);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "LsaFreeReturnBuffer", (void **)&g_pfnLsaFreeReturnBuffer);
        AssertRC(rc);
        RTLdrClose(hLdrMod);
    }
    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(1, "Secur32.dll APIs are not available (%Rrc)\n", rc);
        g_pfnLsaGetLogonSessionData = NULL;
        g_pfnLsaEnumerateLogonSessions = NULL;
        g_pfnLsaFreeReturnBuffer = NULL;
        Assert(RTSystemGetNtVersion() < RTSYSTEM_MAKE_NT_VERSION(5, 0, 0));
    }

    /* WTSAPI32 */
    rc = RTLdrLoadSystem("wtsapi32.dll", true /*fNoUnload*/, &hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hLdrMod, "WTSFreeMemory", (void **)&g_pfnWTSFreeMemory);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "WTSQuerySessionInformationA", (void **)&g_pfnWTSQuerySessionInformationA);
        AssertRC(rc);
        RTLdrClose(hLdrMod);
    }
    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(1, "WtsApi32.dll APIs are not available (%Rrc)\n", rc);
        g_pfnWTSFreeMemory = NULL;
        g_pfnWTSQuerySessionInformationA = NULL;
        Assert(RTSystemGetNtVersion() < RTSYSTEM_MAKE_NT_VERSION(5, 0, 0));
    }

    /* advapi32: */
    rc = RTLdrLoadSystem("advapi32.dll", true /*fNoUnload*/, &hLdrMod);
    if (RT_SUCCESS(rc))
        RTLdrGetSymbol(hLdrMod, "ConvertSidToStringSidW", (void **)&g_pfnConvertSidToStringSidW);


    /* IPHLPAPI: */
    rc = RTLdrLoadSystem("iphlpapi.dll", true /*fNoUnload*/, &hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hLdrMod, "CancelIPChangeNotify", (void **)&g_pfnCancelIPChangeNotify);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "NotifyAddrChange", (void **)&g_pfnNotifyAddrChange);
        AssertRC(rc);
        RTLdrClose(hLdrMod);
    }
    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(1, "iphlpapi.dll notification APIs are not available (%Rrc)\n", rc);
        g_pfnCancelIPChangeNotify = NULL;
        g_pfnNotifyAddrChange = NULL;
        Assert(RTSystemGetNtVersion() < RTSYSTEM_MAKE_NT_VERSION(5, 1, 0)); /* XP */
    }

    /*
     * Initialize the SIDs we need.
     */
    SID_IDENTIFIER_AUTHORITY SidAuthNT = SECURITY_NT_AUTHORITY;
    AssertStmt(AllocateAndInitializeSid(&SidAuthNT, 1, 4, 0, 0, 0, 0, 0, 0, 0, &g_pSidInteractive), g_pSidInteractive = NULL);

    SID_IDENTIFIER_AUTHORITY SidAuthLocal = SECURITY_LOCAL_SID_AUTHORITY;
    AssertStmt(AllocateAndInitializeSid(&SidAuthLocal, 1, 0, 0, 0, 0, 0, 0, 0, 0, &g_pSidLocal), g_pSidLocal = NULL);

    return VINF_SUCCESS;
}


static bool vgsvcVMInfoSession0Separation(void)
{
    return RTSystemGetNtVersion() >= RTSYSTEM_MAKE_NT_VERSION(6, 0, 0); /* Vista */
}


/**
 * Fills in more data for a process.
 *
 * @returns VBox status code.
 * @param   hToken      The token to query information from.
 * @param   enmClass    The kind of token information to get and add to pProc.
 * @param   pProc       The process structure to fill data into.
 */
static int vgsvcVMInfoWinProcessesGetTokenInfo(HANDLE hToken, TOKEN_INFORMATION_CLASS enmClass, PVBOXSERVICEVMINFOPROC pProc)
{
    AssertPtr(pProc);

    /*
     * Query the data.
     */
    DWORD cbTokenInfo;
    switch (enmClass)
    {
#if 0 /* unused */
        case TokenStatistics:
            cbTokenInfo = sizeof(TOKEN_STATISTICS);
            break;
#endif
        case TokenUser:
            cbTokenInfo = 0;
            AssertReturn(!GetTokenInformation(hToken, enmClass, NULL, 0, &cbTokenInfo), VERR_INTERNAL_ERROR_2);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                break;
            return GetLastError() ? RTErrConvertFromWin32(GetLastError()) : VERR_INTERNAL_ERROR_3;

        default:
            AssertLogRelFailedReturn(VERR_NOT_IMPLEMENTED);
    }

    void *pvTokenInfo = RTMemAllocZ(cbTokenInfo);
    AssertReturn(pvTokenInfo, VERR_NO_MEMORY);

    DWORD dwRetLength = 0;
    if (!GetTokenInformation(hToken, enmClass, pvTokenInfo, cbTokenInfo, &dwRetLength))
        return GetLastError() ? RTErrConvertFromWin32(GetLastError()) : VERR_INTERNAL_ERROR_4;

    /*
     * Process the data.
     */
    int rc = VINF_SUCCESS;
    switch (enmClass)
    {
#if 0 /* unused */
        case TokenStatistics:
        {
            PTOKEN_STATISTICS const pStats = (PTOKEN_STATISTICS)pvTokenInfo;
            memcpy(&pProc->luid, &pStats->AuthenticationId, sizeof(LUID));
            /** @todo Add more information of TOKEN_STATISTICS as needed. */
            break;
        }
#endif
        case TokenUser:
        {
            PTOKEN_USER const pUser     = (PTOKEN_USER)pvTokenInfo;
            DWORD const       cbUserSid = GetLengthSid(pUser->User.Sid);
            AssertBreakStmt(cbUserSid, rc = VERR_NO_DATA);
            pProc->pSid = (PSID)RTMemAllocZ(cbUserSid);
            AssertBreakStmt(pProc->pSid, rc = VERR_NO_MEMORY);

            if (CopySid(cbUserSid, pProc->pSid, pUser->User.Sid))
            {
                if (IsValidSid(pProc->pSid))
                    break;
                AssertMsgFailed(("cbUserSid=%u\n%.*Rhxd\n", cbUserSid, cbUserSid, pProc->pSid));
                rc = VERR_INVALID_NAME;
            }
            else
                rc = GetLastError() ? RTErrConvertFromWin32(GetLastError()) : VERR_INTERNAL_ERROR_5;
            RTMemFree(pProc->pSid);
            pProc->pSid = NULL;
            break;
        }

        default:
            AssertMsgFailed(("Unhandled token information class\n"));
            break;
    }

    /*
     * Clean up.
     */
    RTMemFree(pvTokenInfo);
    return rc;
}


/**
 * Worker for vgsvcVMInfoWinTokenQueryInteractive.
 */
static bool vgsvcVMInfoWinTokenQueryInteractiveWorker(TOKEN_GROUPS const *pGroups)
{
    for (DWORD i = 0; i < pGroups->GroupCount; i++)
        if (   (pGroups->Groups[i].Attributes & SE_GROUP_LOGON_ID)
            || (g_pSidInteractive && EqualSid(pGroups->Groups[i].Sid, g_pSidInteractive))
            || (g_pSidLocal       && EqualSid(pGroups->Groups[i].Sid, g_pSidLocal)) )
            return true;
    return false;
}


/**
 * Determins if the token is for an interactive process.
 *
 * Specialized code for this as it's the filtering criteria and best be as
 * efficient as we can get it.
 *
 * @returns VBox status code.
 * @param   hToken          The token to query information from.
 * @param   pid             The PID we're querying it for (error reporting).
 * @param   pfInteractive   Where to return the indicator.
 */
static int vgsvcVMInfoWinTokenQueryInteractive(HANDLE hToken, DWORD pid, bool *pfInteractive)
{
    /* Try with a stack buffer first. */
    uint8_t         abBuffer[_1K];
    PTOKEN_GROUPS   pGroups     = (PTOKEN_GROUPS)&abBuffer[0];
    DWORD           cbTokenInfo = sizeof(abBuffer);
    if (GetTokenInformation(hToken, TokenGroups, pGroups, cbTokenInfo, &cbTokenInfo))
    {
        *pfInteractive = vgsvcVMInfoWinTokenQueryInteractiveWorker(pGroups);
        return VINF_SUCCESS;
    }

    DWORD dwErr = GetLastError();
    if (dwErr == ERROR_INSUFFICIENT_BUFFER)
    {
        /* Okay, need a larger buffer off the heap. */
        pGroups = (PTOKEN_GROUPS)RTMemTmpAlloc(cbTokenInfo);
        if (pGroups)
        {
            AssertReturn(pGroups, VERR_NO_TMP_MEMORY);
            if (GetTokenInformation(hToken, TokenGroups, pGroups, cbTokenInfo, &cbTokenInfo))
            {
                *pfInteractive = vgsvcVMInfoWinTokenQueryInteractiveWorker(pGroups);
                RTMemFree(pGroups);
                return VINF_SUCCESS;
            }
            dwErr = GetLastError();
            RTMemTmpFree(pGroups);
        }
        else
            dwErr = ERROR_OUTOFMEMORY;
    }
    int const rc = dwErr ? RTErrConvertFromWin32(dwErr) : VERR_INTERNAL_ERROR_3;
    if (g_cVerbosity)
        VGSvcError("Get token class 'groups' for process %u failed: dwErr=%u (rc=%Rrc)\n", pid, dwErr, rc);
    *pfInteractive = false;
    return rc;
}


/**
 * Enumerate all the processes in the system and get the logon user IDs for
 * them.
 *
 * @returns VBox status code.
 * @param   ppaProcs    Where to return the process snapshot.  This must be
 *                      freed by calling vgsvcVMInfoWinProcessesFree.
 *
 * @param   pcProcs     Where to store the returned process count.
 * @param   ppvExtra    Where to return extra memory that needs freeing.
 */
static int vgsvcVMInfoWinEnumerateInteractiveProcesses(PVBOXSERVICEVMINFOPROC *ppaProcs, PDWORD pcProcs, void **ppvExtra)
{
    AssertPtr(ppaProcs);
    AssertPtr(pcProcs);
    AssertPtr(ppvExtra);

    /*
     * Query the information via the NT API.
     */
    static ULONG    s_cbPrev = _16K;
    ULONG           cbBuf    = s_cbPrev;
    ULONG           cbNeeded = 0;
    uint8_t        *pbBuf    = (uint8_t *)RTMemTmpAlloc(cbBuf);
    AssertReturn(pbBuf, VERR_NO_TMP_MEMORY);
    NTSTATUS rcNt = NtQuerySystemInformation(SystemProcessInformation, pbBuf, cbBuf, &cbNeeded);
    if (NT_SUCCESS(rcNt))
        s_cbPrev = RT_ALIGN_32(cbNeeded + _8K, _16K);
    else
    {
        while (rcNt == STATUS_INFO_LENGTH_MISMATCH)
        {
            RTMemTmpFree(pbBuf);
            cbBuf = RT_ALIGN_32(cbNeeded + _8K, _16K);
            pbBuf = (uint8_t *)RTMemTmpAlloc(cbBuf);
            AssertReturn(pbBuf, VERR_NO_TMP_MEMORY);

            rcNt = NtQuerySystemInformation(SystemProcessInformation, pbBuf, cbBuf, &cbNeeded);
        }
        s_cbPrev = cbBuf;
        if (NT_SUCCESS(rcNt))
        { /* likely */ }
        else
        {
            RTMemTmpFree(pbBuf);
            return RTErrConvertFromNtStatus(rcNt);
        }
    }

    /*
     * Destill the data into paProcs.
     */
    static const uint32_t   s_cbMinProcInfoEntry = RT_UOFFSETOF(RTNT_SYSTEM_PROCESS_INFORMATION, IoCounters);
    static uint32_t         s_cPrevProcesses     = 64;
    uint32_t                cAllocated           = s_cPrevProcesses;
    uint32_t                cProcesses           = 0;
    PVBOXSERVICEVMINFOPROC  paProcesses          = (PVBOXSERVICEVMINFOPROC)RTMemAlloc(sizeof(paProcesses[0]) * cAllocated);
    AssertReturnStmt(paProcesses, RTMemTmpFree(pbBuf), VERR_NO_MEMORY);
    if (RT_LIKELY(cbNeeded > s_cbMinProcInfoEntry)) /* paranoia */
        for (size_t offBuf = 0; offBuf <= cbNeeded - s_cbMinProcInfoEntry;)
        {
            PRTNT_SYSTEM_PROCESS_INFORMATION const pProcInfo = (PRTNT_SYSTEM_PROCESS_INFORMATION)&pbBuf[offBuf];
            DWORD const pid = (DWORD)(uintptr_t)pProcInfo->UniqueProcessId;

            /** @todo Filter on session ID if we can. */

            HANDLE const hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE /*bInheritHandle*/, pid);
            if (hProcess)
            {
                HANDLE hToken = NULL;
                if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
                {
                    /* Check if it is an interactive process that we ought to return. */
                    bool fInteractive = false;
                    int rc2 = vgsvcVMInfoWinTokenQueryInteractive(hToken, pid, &fInteractive);
                    if (RT_SUCCESS(rc2) && fInteractive)
                    {
                        if (cProcesses >= cAllocated)
                        {
                            cAllocated += RT_MIN(cAllocated, 1024);
                            void * const pvNew = RTMemRealloc(paProcesses, sizeof(paProcesses[0]) * cAllocated);
                            if (pvNew)
                                paProcesses = (PVBOXSERVICEVMINFOPROC)pvNew;
                            else
                            {
                                RTMemFree(paProcesses);
                                RTMemFree(pbBuf);
                                CloseHandle(hToken);
                                CloseHandle(hProcess);
                                return VERR_NO_MEMORY;
                            }
                        }

                        paProcesses[cProcesses].id          = pid;
                        paProcesses[cProcesses].idSession   = g_fHasProcInfoSessionId ? pProcInfo->SessionId : 0;
                        paProcesses[cProcesses].pUniStrName =    pProcInfo->ProcessName.Length
                                                              && pProcInfo->ProcessName.Buffer
                                                            ? &pProcInfo->ProcessName : NULL;
                        paProcesses[cProcesses].pSid        = NULL;


                        /** @todo Ignore processes we can't get the user for? */
                        rc2 = vgsvcVMInfoWinProcessesGetTokenInfo(hToken, TokenUser, &paProcesses[cProcesses]);
                        /** @todo On NT 6.2+, the user SID is often available in an
                         *        SYSTEM_PROCESS_INFORMATION_EXTENSION item after the thread
                         *        information. (See Geoff Chappell's site) */
                        if (RT_FAILURE(rc2) && g_cVerbosity)
                            VGSvcError("Get token class 'groups' for process %u failed: %Rrc\n", pid, rc2);

#if 0 /* unused */
                        rc2 = vgsvcVMInfoWinProcessesGetTokenInfo(hToken, TokenStatistics, &paProcesses[cProcesses]);
                        if (RT_FAILURE(rc2) && g_cVerbosity)
                            VGSvcError("Get token class 'statistics' for process %u failed: %Rrc\n", pid, rc2);
#endif

                        cProcesses++;
                    }
                    CloseHandle(hToken);
                }
                else if (g_cVerbosity)
                    VGSvcError("Unable to open token for PID %u: GetLastError=%u\n", pid, GetLastError());
                CloseHandle(hProcess);
            }
            else if (g_cVerbosity)
                VGSvcError("Unable to open PID %u: GetLastError=%u\n", pid, GetLastError());

            /* Advance. */
            uint32_t offNext = pProcInfo->NextEntryOffset;
            if (offNext >= s_cbMinProcInfoEntry)
                offBuf += offNext;
            else
                break;
        }

    /* Return */
    *ppaProcs = paProcesses;
    *pcProcs  = cProcesses;
    *ppvExtra = pbBuf;
    return VINF_SUCCESS;
}


/**
 * Frees the process structures returned by
 * vgsvcVMInfoWinEnumerateInteractiveProcesses() before.
 *
 * @param   cProcs      Number of processes in paProcs.
 * @param   paProcs     The process array.
 * @param   pvExtra     The extra memory returned with paProcs (where process
 *                      name strings live).
 */
static void vgsvcVMInfoWinProcessesFree(DWORD cProcs, PVBOXSERVICEVMINFOPROC paProcs, void *pvExtra)
{
    for (DWORD i = 0; i < cProcs; i++)
        if (paProcs[i].pSid)
        {
            RTMemFree(paProcs[i].pSid);
            paProcs[i].pSid = NULL;
        }
    RTMemFree(paProcs);
    RTMemTmpFree(pvExtra);
}


/**
 * Determines whether the specified session has interactive processes on the
 * system.
 *
 * @returns Number of processes found for a specified session.
 * @param   paProcs             The snapshot of the interactive processes.
 * @param   cProcs              The number of processes in the snaphot.
 * @param   pSessionData        The data for the session we're looking for. We
 *                              use the SID and Session members.
 */
static uint32_t vgsvcVMInfoWinCountSessionProcesses(PVBOXSERVICEVMINFOPROC const paProcs, DWORD cProcs,
                                                    PSECURITY_LOGON_SESSION_DATA pSessionData)
{
    AssertPtrReturn(pSessionData, 0);
    Assert(IsValidSid(pSessionData->Sid));

    /*
     * Even if a user seems to be logged in, it could be a stale/orphaned logon
     * session. So check if we have some processes bound to it by comparing the
     * session <-> process LUIDs.
     *
     * 2025-11-09 bird: We don't actually compare LUID, we compare user SID, the
     *                  purposes is the same though.
     */
    uint32_t cProcessesFound = 0;
    for (DWORD i = 0; i < cProcs; i++)
    {
        PSID pProcSID = paProcs[i].pSid;
        if (   pProcSID
            && IsValidSid(pProcSID))
            if (EqualSid(pSessionData->Sid, paProcs[i].pSid))
            {
                cProcessesFound++;
                if (g_cVerbosity < 3) /* This must match the logging statements using cInteractiveProcesses. */
                    break;
                if (g_cVerbosity >= 4)
                    VGSvcVerbose(4, "Session %RU32: PID=%u SessionID=%u: %.*ls\n",
                                 pSessionData->Session, paProcs[i].id, paProcs[i].idSession,
                                 paProcs[i].pUniStrName ? paProcs[i].pUniStrName->Length / sizeof(RTUTF16) : 0,
                                 paProcs[i].pUniStrName ? paProcs[i].pUniStrName->Buffer : NULL);
            }
    }

    return cProcessesFound;
}


/**
 * Save and noisy string copy.
 *
 * @param   pwszDst             Destination buffer.
 * @param   cbDst               Size in bytes - not WCHAR count!
 * @param   pSrc                Source string.
 * @param   pszWhat             What this is. For the log.
 */
static void vgsvcVMInfoWinSafeCopy(PWCHAR pwszDst, size_t cbDst, LSA_UNICODE_STRING const *pSrc, const char *pszWhat)
{
    Assert(RT_ALIGN(cbDst, sizeof(WCHAR)) == cbDst);

    size_t cbCopy = pSrc->Length;
    if (cbCopy + sizeof(WCHAR) > cbDst)
    {
        VGSvcVerbose(0, "%s is too long - %u bytes, buffer %u bytes! It will be truncated.\n", pszWhat, cbCopy, cbDst);
        cbCopy = cbDst - sizeof(WCHAR);
    }
    if (cbCopy)
        memcpy(pwszDst, pSrc->Buffer, cbCopy);
    pwszDst[cbCopy / sizeof(WCHAR)] = '\0';
}


/**
 * Detects whether a user is logged on and gets user info.
 *
 * @returns true if logged in, false if not (or error).
 * @param   pSession            The session to check.
 * @param   pUserInfo           Where to return the user information.
 * @param   ppSessionData       Where to return pointer to the session data.
 */
static bool vgsvcVMInfoWinIsLoggedInWithUserInfoReturned(PLUID pSession, PVBOXSERVICEVMINFOUSER pUserInfo,
                                                         PSECURITY_LOGON_SESSION_DATA *ppSessionData)
{
    *ppSessionData = NULL;
    if (!pSession)
        return false;
    if (   !g_pfnLsaGetLogonSessionData
        || !g_pfnLsaNtStatusToWinError)
        return false;

    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    NTSTATUS rcNt = g_pfnLsaGetLogonSessionData(pSession, &pSessionData);
    if (rcNt != STATUS_SUCCESS)
    {
        ULONG ulError = g_pfnLsaNtStatusToWinError(rcNt);
        switch (ulError)
        {
            case ERROR_NOT_ENOUGH_MEMORY:
                /* If we don't have enough memory it's hard to judge whether the specified user
                 * is logged in or not, so just assume he/she's not. */
                VGSvcVerbose(3, "Not enough memory to retrieve logon session data!\n");
                break;

            case ERROR_NO_SUCH_LOGON_SESSION:
                /* Skip session data which is not valid anymore because it may have been
                 * already terminated. */
                break;

            default:
                VGSvcError("LsaGetLogonSessionData failed with error %u (rcNt=%#x)\n", ulError, rcNt);
                break;
        }
        if (pSessionData)
            g_pfnLsaFreeReturnBuffer(pSessionData);
        return false;
    }
    if (!pSessionData)
    {
        VGSvcError("Invalid logon session data!\n");
        return false;
    }

    VGSvcVerbose(3, "Session data: Name=%ls, SessionID=%RU32, LogonID=%d,%u, LogonType=%u\n",
                 pSessionData->UserName.Buffer, pSessionData->Session,
                 pSessionData->LogonId.HighPart, pSessionData->LogonId.LowPart, pSessionData->LogonType);

    if (vgsvcVMInfoSession0Separation())
    {
        /* Starting at Windows Vista user sessions begin with session 1, so
         * ignore (stale) session 0 users. */
        if (   pSessionData->Session == 0
            /* Also check the logon time. */
            || pSessionData->LogonTime.QuadPart == 0)
        {
            g_pfnLsaFreeReturnBuffer(pSessionData);
            return false;
        }
    }

    /*
     * Only handle users which can login interactively or logged in
     * remotely over native RDP.
     */
    bool fFoundUser = false;
    if (   IsValidSid(pSessionData->Sid)
        && (   (SECURITY_LOGON_TYPE)pSessionData->LogonType == Interactive
            || (SECURITY_LOGON_TYPE)pSessionData->LogonType == RemoteInteractive
            /* Note: We also need CachedInteractive in case Windows cached the credentials
             *       or just wants to reuse them! */
            || (SECURITY_LOGON_TYPE)pSessionData->LogonType == CachedInteractive))
    {
        VGSvcVerbose(3, "Session LogonType=%u is supported -- looking up SID + type ...\n", pSessionData->LogonType);

        /*
         * Copy out relevant data.
         */
        vgsvcVMInfoWinSafeCopy(pUserInfo->wszUser, sizeof(pUserInfo->wszUser), &pSessionData->UserName, "User name");
        vgsvcVMInfoWinSafeCopy(pUserInfo->wszAuthenticationPackage, sizeof(pUserInfo->wszAuthenticationPackage),
                               &pSessionData->AuthenticationPackage, "Authentication pkg name");
        vgsvcVMInfoWinSafeCopy(pUserInfo->wszLogonDomain, sizeof(pUserInfo->wszLogonDomain),
                               &pSessionData->LogonDomain, "Logon domain name");

        TCHAR           szOwnerName[MAX_PATH]   = { 0 };
        DWORD           dwOwnerNameSize         = sizeof(szOwnerName);
        TCHAR           szDomainName[MAX_PATH]  = { 0 };
        DWORD           dwDomainNameSize        = sizeof(szDomainName);
        SID_NAME_USE    enmOwnerType            = SidTypeInvalid;
        if (!LookupAccountSid(NULL,
                              pSessionData->Sid,
                              szOwnerName,
                              &dwOwnerNameSize,
                              szDomainName,
                              &dwDomainNameSize,
                              &enmOwnerType))
        {
            /*
             * If a network time-out prevents the function from finding the name or
             * if a SID that does not have a corresponding account name (such as a
             * logon SID that identifies a logon session), we get ERROR_NONE_MAPPED
             * here that we just skip.
             */
            DWORD dwErr = GetLastError();
            if (dwErr != ERROR_NONE_MAPPED)
                VGSvcError("Failed looking up account info for user=%ls, error=$ld!\n", pUserInfo->wszUser, dwErr);
        }
        else
        {
            if (enmOwnerType == SidTypeUser) /* Only recognize users; we don't care about the rest! */
            {
                VGSvcVerbose(3, "Account User=%ls, Session=%u, LogonID=%d,%u, AuthPkg=%ls, Domain=%ls\n",
                             pUserInfo->wszUser, pSessionData->Session, pSessionData->LogonId.HighPart,
                             pSessionData->LogonId.LowPart, pUserInfo->wszAuthenticationPackage, pUserInfo->wszLogonDomain);

                /* KB970910 (check http://support.microsoft.com/kb/970910 on archive.org)
                 * indicates that WTSQuerySessionInformation may leak memory and return the
                 * wrong status code for WTSApplicationName and WTSInitialProgram queries.
                 *
                 * The system must be low on resources, and presumably some internal operation
                 * must fail because of this, triggering an error handling path that forgets
                 * to free memory and set last error.
                 *
                 * bird 2022-08-26: However, we do not query either of those info items.  We
                 * query WTSConnectState, which is a rather simple affair.  So, I've
                 * re-enabled the code for all systems that includes the API.
                 */
                if (!g_fSkipRdpDetection)
                {
                    /* Skip if we don't have the WTS API. */
                    if (!g_pfnWTSQuerySessionInformationA)
                        g_fSkipRdpDetection = true;
#if 0 /* bird: see above */
                    /* Skip RDP detection on Windows 2000 and older.
                       For Windows 2000 however we don't have any hotfixes, so just skip the
                       RDP detection in any case. */
                    else if (RTSystemGetNtVersion() < RTSYSTEM_MAKE_NT_VERSION(5, 1, 0)) /* older than XP */
                        g_fSkipRdpDetection = true;
#endif
                    if (g_fSkipRdpDetection)
                        VGSvcVerbose(0, "Detection of logged-in users via RDP is disabled\n");
                }

                if (!g_fSkipRdpDetection)
                {
                    Assert(g_pfnWTSQuerySessionInformationA);
                    Assert(g_pfnWTSFreeMemory);

                    /* Detect RDP sessions as well. */
                    LPTSTR  pBuffer = NULL;
                    DWORD   cbRet   = 0;
                    int     iState  = -1;
                    if (g_pfnWTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE,
                                                         pSessionData->Session,
                                                         WTSConnectState,
                                                         &pBuffer,
                                                         &cbRet))
                    {
                        if (cbRet)
                            iState = *pBuffer;
                        VGSvcVerbose(3, "Account User=%ls, WTSConnectState=%d (%u)\n", pUserInfo->wszUser, iState, cbRet);
                        if (    iState == WTSActive           /* User logged on to WinStation. */
                             || iState == WTSShadow           /* Shadowing another WinStation. */
                             || iState == WTSDisconnected)    /* WinStation logged on without client. */
                        {
                            /** @todo On Vista and W2K, always "old" user name are still
                             *        there. Filter out the old one! */
                            VGSvcVerbose(3, "Account User=%ls using TCS/RDP, state=%d \n", pUserInfo->wszUser, iState);
                            fFoundUser = true;
                        }
                        if (pBuffer)
                            g_pfnWTSFreeMemory(pBuffer);
                    }
                    else
                    {
                        DWORD dwLastErr = GetLastError();
                        switch (dwLastErr)
                        {
                            /*
                             * Terminal services don't run (for example in W2K,
                             * nothing to worry about ...).  ... or is on the Vista
                             * fast user switching page!
                             */
                            case ERROR_CTX_WINSTATION_NOT_FOUND:
                                VGSvcVerbose(3, "No WinStation found for user=%ls\n", pUserInfo->wszUser);
                                break;

                            default:
                                VGSvcVerbose(3, "Cannot query WTS connection state for user=%ls, error=%u\n",
                                             pUserInfo->wszUser, dwLastErr);
                                break;
                        }

                        fFoundUser = true;
                    }
                }
            }
            else
                VGSvcVerbose(3, "SID owner type=%d not handled, skipping\n", enmOwnerType);
        }

        VGSvcVerbose(3, "Account User=%ls %s logged in\n", pUserInfo->wszUser, fFoundUser ? "is" : "is not");
    }

    if (fFoundUser)
    {
        pUserInfo->ulLastSession = pSessionData->Session;
        *ppSessionData = pSessionData;
    }
    else
        g_pfnLsaFreeReturnBuffer(pSessionData);

    return fFoundUser;
}


/**
 * Destroys an allocated SID.
 *
 * @param   pSid                SID to dsetroy. The pointer will be invalid on return.
 */
static void vgsvcVMInfoWinUserSidDestroy(PSID pSid)
{
    RTMemFree(pSid);
    pSid = NULL;
}


/**
 * Looks up and returns a SID for a given user.
 *
 * @returns VBox status code.
 * @param   pszUser             User to look up a SID for.
 * @param   ppSid               Where to return the allocated SID.
 *                              Must be destroyed with vgsvcVMInfoWinUserSidDestroy().
 */
static int vgsvcVMInfoWinUserSidLookup(const char *pszUser, PSID *ppSid)
{
    RTUTF16 *pwszUser = NULL;
    size_t cwUser = 0;
    int rc = RTStrToUtf16Ex(pszUser, RTSTR_MAX, &pwszUser, 0, &cwUser);
    AssertRCReturn(rc, rc);

    PSID pSid = NULL;
    DWORD cbSid = 0;
    DWORD cbDomain = 0;
    SID_NAME_USE enmSidUse = SidTypeUser;
    if (!LookupAccountNameW(NULL, pwszUser, pSid, &cbSid, NULL, &cbDomain, &enmSidUse))
    {
        DWORD const dwErr = GetLastError();
        if (dwErr == ERROR_INSUFFICIENT_BUFFER)
        {
            pSid = (PSID)RTMemAllocZ(cbSid);
            if (pSid)
            {
                PRTUTF16 pwszDomain = (PRTUTF16)RTMemAllocZ(cbDomain * sizeof(RTUTF16));
                if (pwszDomain)
                {
                    if (LookupAccountNameW(NULL, pwszUser, pSid, &cbSid, pwszDomain, &cbDomain, &enmSidUse))
                    {
                        if (IsValidSid(pSid))
                            *ppSid = pSid;
                        else
                            rc = VERR_INVALID_PARAMETER;
                    }
                    else
                        rc = RTErrConvertFromWin32(GetLastError());
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
            rc = RTErrConvertFromWin32(dwErr);
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());

    if (RT_FAILURE(rc))
        vgsvcVMInfoWinUserSidDestroy(pSid);

    return rc;
}


/**
 * Fallback function in case writing the user name failed within vgsvcVMInfoWinUserUpdateF().
 *
 * This uses the following approach:
 *   - only use the user name as part of the property name from now on
 *   - write the domain name into a separate "Domain" property
 *   - write the (full) SID into a  separate "SID" property
 *
 * @returns VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the final property name length exceeds the maximum supported length.
 * @param   pCache                  Pointer to guest property cache to update user in.
 * @param   pszUser                 Name of guest user to update.
 * @param   pszDomain               Domain of guest user to update. Optional.
 * @param   pwszSid                 The user's SID as a string. Might be NULL if not supported (NT4).
 * @param   pszKey                  Key name of guest property to update.
 * @param   pszValueFormat          Guest property value to set. Pass NULL for deleting
 *                                  the property.
 * @param   va                      Variable arguments.
 */
static int vgsvcVMInfoWinUserUpdateFallbackV(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                                             WCHAR *pwszSid, const char *pszKey, const char *pszValueFormat, va_list va)
{
    int rc = VGSvcVMInfoUpdateUser(pCache, pszUser, NULL /* pszDomain */, "Domain", pszDomain);
    if (pwszSid && RT_SUCCESS(rc))
        rc = VGSvcVMInfoUpdateUserF(pCache, pszUser, NULL /* pszDomain */, "SID", "%ls", pwszSid);

    /* Last but no least, write the actual guest property value we initially were called for.
     * We always do this, no matter of what the outcome from above was. */
    int rc2 = VGSvcVMInfoUpdateUserV(pCache, pszUser, NULL /* pszDomain */, pszKey, pszValueFormat, va);
    if (RT_SUCCESS(rc))
        rc2 = rc;

    return rc;
}


/**
 * Wrapper function for VGSvcVMInfoUpdateUserF() that deals with too long guest property names.
 *
 * @return  VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the final property name length exceeds the maximum supported length.
 * @param   pCache                  Pointer to guest property cache to update user in.
 * @param   pszUser                 Name of guest user to update.
 * @param   pszDomain               Domain of guest user to update. Optional.
 * @param   pszKey                  Key name of guest property to update.
 * @param   pszValueFormat          Guest property value to set. Pass NULL for deleting
 *                                  the property.
 * @param   ...                     Variable arguments.
 */
static int vgsvcVMInfoWinUserUpdateF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                                     const char *pszKey, const char *pszValueFormat, ...)
{
    va_list va;
    va_start(va, pszValueFormat);

    /* First, try to write stuff as we always did, to not break older VBox versions. */
    int rc = VGSvcVMInfoUpdateUserV(pCache, pszUser, pszDomain, pszKey, pszValueFormat, va);
    if (rc == VERR_BUFFER_OVERFLOW)
    {
        /*
         * If the constructed property name was too long, we have to be a little more creative here:
         *
         *   - only use the user name as part of the property name from now on
         *   - write the domain name into a separate "Domain" property
         *   - write the (full) SID into a  separate "SID" property
         */
        PSID pSid;
        rc = vgsvcVMInfoWinUserSidLookup(pszUser, &pSid); /** @todo Shall we cache this? */
        if (RT_SUCCESS(rc))
        {
            WCHAR *pwszSid = NULL;
            if (g_pfnConvertSidToStringSidW)
                g_pfnConvertSidToStringSidW(pSid, &pwszSid); /** @todo Ditto. */

            rc = vgsvcVMInfoWinUserUpdateFallbackV(pCache, pszUser, pszDomain, pwszSid, pszKey, pszValueFormat, va);
            if (RT_FAILURE(rc))
            {
                /*
                 * If using the sole user name as a property name still is too long or something else failed,
                 * at least try to look up the user's RID (relative identifier). Note that the RID always is bound to the
                 * to the authority that issued the SID.
                 */
                int const cSubAuth = *GetSidSubAuthorityCount(pSid);
                if (cSubAuth > 1)
                {
                    DWORD const dwUserRid = *GetSidSubAuthority(pSid, cSubAuth - 1);
                    char  szUserRid[16 + 1];
                    if (RTStrPrintf2(szUserRid, sizeof(szUserRid), "%u", dwUserRid) > 0)
                    {
                        rc = vgsvcVMInfoWinUserUpdateFallbackV(pCache, szUserRid, pszDomain, pwszSid, pszKey, pszValueFormat, va);
                        /* Also write the resolved user name into a dedicated key,
                         * so that it's easier to look it up for the host. */
                        if (RT_SUCCESS(rc))
                            rc = VGSvcVMInfoUpdateUser(pCache, szUserRid, NULL /* pszDomain */, "User", pszUser);
                    }
                    else
                        rc = VERR_BUFFER_OVERFLOW;
                }
                /* else not much else we can do then. */
            }

            if (pwszSid)
            {
                LocalFree(pwszSid);
                pwszSid = NULL;
            }

            vgsvcVMInfoWinUserSidDestroy(pSid);
        }
        else
            VGSvcError("Looking up SID for user '%s' (domain '%s') failed with %Rrc\n", pszUser, pszDomain, rc);
    }
    va_end(va);
    return rc;
}


static int vgsvcVMInfoWinWriteLastInput(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    AssertPtrReturn(pszUser, VERR_INVALID_POINTER);
    /* pszDomain is optional. */

    char szPipeName[512 + sizeof(VBOXTRAY_IPC_PIPE_PREFIX)];
    memcpy(szPipeName, VBOXTRAY_IPC_PIPE_PREFIX, sizeof(VBOXTRAY_IPC_PIPE_PREFIX));
    int rc = RTStrCat(szPipeName, sizeof(szPipeName), pszUser);
    if (RT_SUCCESS(rc))
    {
        bool fReportToHost = false;
        VBoxGuestUserState userState = VBoxGuestUserState_Unknown;

        RTLOCALIPCSESSION hSession;
        rc = RTLocalIpcSessionConnect(&hSession, szPipeName, RTLOCALIPC_FLAGS_NATIVE_NAME);
        if (RT_SUCCESS(rc))
        {
            VBOXTRAYIPCHEADER ipcHdr =
            {
                /* .uMagic      = */ VBOXTRAY_IPC_HDR_MAGIC,
                /* .uVersion    = */ VBOXTRAY_IPC_HDR_VERSION,
                /* .enmMsgType  = */ VBOXTRAYIPCMSGTYPE_USER_LAST_INPUT,
                /* .cbPayload   = */ 0 /* No payload */
            };

            rc = RTLocalIpcSessionWrite(hSession, &ipcHdr, sizeof(ipcHdr));
            if (RT_SUCCESS(rc))
            {
                VBOXTRAYIPCREPLY_USER_LAST_INPUT_T ipcReply;
                rc = RTLocalIpcSessionRead(hSession, &ipcReply, sizeof(ipcReply), NULL /* Exact read */);
                if (   RT_SUCCESS(rc)
                    /* If uLastInput is set to UINT32_MAX VBoxTray was not able to retrieve the
                     * user's last input time. This might happen when running on Windows NT4 or older. */
                    && ipcReply.cSecSinceLastInput != UINT32_MAX)
                {
                    userState = ipcReply.cSecSinceLastInput * 1000 < g_cMsVMInfoUserIdleThreshold
                              ? VBoxGuestUserState_InUse
                              : VBoxGuestUserState_Idle;

                    rc = vgsvcVMInfoWinUserUpdateF(pCache, pszUser, pszDomain, "UsageState",
                                                   userState == VBoxGuestUserState_InUse ? "InUse" : "Idle");
                    /*
                     * Note: vboxServiceUserUpdateF can return VINF_NO_CHANGE in case there wasn't anything
                     *       to update. So only report the user's status to host when we really got something
                     *       new.
                     */
                    fReportToHost = rc == VINF_SUCCESS;
                    VGSvcVerbose(4, "User '%s' (domain '%s') is idle for %RU32, fReportToHost=%RTbool\n",
                                 pszUser, pszDomain ? pszDomain : "<None>", ipcReply.cSecSinceLastInput, fReportToHost);

#if 0 /* Do we want to write the idle time as well? */
                        /* Also write the user's current idle time, if there is any. */
                        if (userState == VBoxGuestUserState_Idle)
                            rc = vgsvcVMInfoWinUserUpdateF(pCache, pszUser, pszDomain, "IdleTimeMs", "%RU32",
                                                           ipcReply.cSecSinceLastInput);
                        else
                            rc = vgsvcVMInfoWinUserUpdateF(pCache, pszUser, pszDomain, "IdleTimeMs",
                                                           NULL /* Delete property */);
                        if (RT_SUCCESS(rc))
#endif
                }
#ifdef DEBUG
                else if (RT_SUCCESS(rc) && ipcReply.cSecSinceLastInput == UINT32_MAX)
                    VGSvcVerbose(4, "Last input for user '%s' is not supported, skipping\n", pszUser, rc);
#endif
            }
#ifdef DEBUG
            VGSvcVerbose(4, "Getting last input for user '%s' ended with rc=%Rrc\n", pszUser, rc);
#endif
            int rc2 = RTLocalIpcSessionClose(hSession);
            if (RT_SUCCESS(rc) && RT_FAILURE(rc2))
                rc = rc2; \
        }
        else
        {
            switch (rc)
            {
                case VERR_FILE_NOT_FOUND:
                {
                    /* No VBoxTray (or too old version which does not support IPC) running
                       for the given user. Not much we can do then. */
                    VGSvcVerbose(4, "VBoxTray for user '%s' not running (anymore), no last input available\n", pszUser);

                    /* Overwrite rc from above. */
                    rc = vgsvcVMInfoWinUserUpdateF(pCache, pszUser, pszDomain, "UsageState", "Idle");

                    fReportToHost = rc == VINF_SUCCESS;
                    if (fReportToHost)
                        userState = VBoxGuestUserState_Idle;
                    break;
                }

                default:
                    VGSvcError("Error querying last input for user '%s', rc=%Rrc\n", pszUser, rc);
                    break;
            }
        }

        if (fReportToHost)
        {
            Assert(userState != VBoxGuestUserState_Unknown);
            int rc2 = VbglR3GuestUserReportState(pszUser, pszDomain, userState, NULL /* No details */, 0);
            if (RT_FAILURE(rc2))
                VGSvcError("Error reporting usage state %d for user '%s' to host, rc=%Rrc\n", userState, pszUser, rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }

    return rc;
}


/**
 * Retrieves the currently logged in users and stores their names along with the
 * user count.
 *
 * @returns VBox status code.
 * @param   pUserGatherer   Pointer to the user gatherer state that we pass to
 *                          VGSvcVMInfoAddUserToList().
 * @param   pCache          Property cache to use for storing some of the lookup
 *                          data in between calls.
 */
int VGSvcVMInfoWinQueryUserListAndUpdateInfo(struct VBOXSERVICEVMINFOUSERLIST *pUserGatherer, PVBOXSERVICEVEPROPCACHE pCache)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
#if 1
# define VGSVC_VMINFO_WIN_QUERY_USER_LIST_DEBUG
    char szDebugPath[GUEST_PROP_MAX_NAME_LEN];
#endif

    /** @todo why don't we do this during sub-service init?   */
    int rc = RTOnce(&g_vgsvcWinVmInitOnce, vgsvcWinVmInfoInitOnce, NULL);
    if (RT_FAILURE(rc))
        return rc;
    if (!g_pfnLsaEnumerateLogonSessions || !g_pfnLsaNtStatusToWinError)
        return VERR_NOT_SUPPORTED;

    /*
     * Snapshot the logon sessions.
     *
     * This function can report stale or orphaned interactive logon sessions
     * of already logged off users (especially in Windows 2000).
     */
    PLUID    paSessions = NULL;
    ULONG    cSessions  = 0;
    NTSTATUS rcNt = g_pfnLsaEnumerateLogonSessions(&cSessions, &paSessions);
    if (rcNt != STATUS_SUCCESS)
    {
        ULONG const uError = g_pfnLsaNtStatusToWinError(rcNt);
        switch (uError)
        {
            case ERROR_NOT_ENOUGH_MEMORY:
                VGSvcError("Not enough memory to enumerate logon sessions!\n");
                rc = VERR_NO_MEMORY;
                break;

            case ERROR_SHUTDOWN_IN_PROGRESS:
                /* If we're about to shutdown when we were in the middle of enumerating the logon
                 * sessions, skip the error to not confuse the user with an unnecessary log message. */
                VGSvcVerbose(3, "Shutdown in progress ...\n");
                rc = VINF_SUCCESS;
                break;

            default:
                VGSvcError("LsaEnumerate failed with error %RU32 (rcNt=%#x)\n", uError, rcNt);
                rc = RTErrConvertFromWin32(uError);
                break;
        }
        if (paSessions)
            g_pfnLsaFreeReturnBuffer(paSessions);
        return rc;
    }
    VGSvcVerbose(3, "Found %u sessions\n", cSessions);

    /*
     * Snapshot the interactive processes in the system (that we can get info from).
     */
    PVBOXSERVICEVMINFOPROC  paProcs = NULL;
    DWORD                   cProcs  = 0;
    void                   *pvExtra = NULL;
    rc = vgsvcVMInfoWinEnumerateInteractiveProcesses(&paProcs, &cProcs, &pvExtra);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate an array for gather unique user info that we'll be destilling
         * from the logon sessions and process snapshot.
         */
        PVBOXSERVICEVMINFOUSER paUserInfo = (PVBOXSERVICEVMINFOUSER)RTMemTmpAllocZ(cSessions * sizeof(VBOXSERVICEVMINFOUSER));
        if (paUserInfo)
        {
            ULONG cUniqueUsers = 0;

            /*
             * Iterate thru the login sessions, popuplating paUserInfo with unique entries.
             *
             * Note: The cSessions loop variable does *not* correlate with
             *       the Windows session ID!
             */
            for (ULONG iSession = 0; iSession < cSessions; iSession++)
            {
                VGSvcVerbose(3, "iSession=%RU32 (of %RU32)\n", iSession, cSessions);

                /* Get user information. */
                PVBOXSERVICEVMINFOUSER const pUserSession = &paUserInfo[cUniqueUsers];
                PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
                if (vgsvcVMInfoWinIsLoggedInWithUserInfoReturned(&paSessions[iSession], pUserSession, &pSessionData))
                {
                    VGSvcVerbose(4, "Handling user=%ls, domain=%ls, package=%ls, session=%RU32\n", pUserSession->wszUser,
                                 pUserSession->wszLogonDomain, pUserSession->wszAuthenticationPackage, pUserSession->ulLastSession);

                    /* Count the interactive processes in the session. */
                    pUserSession->cInteractiveProcesses = vgsvcVMInfoWinCountSessionProcesses(paProcs, cProcs, pSessionData);
#ifdef VGSVC_VMINFO_WIN_QUERY_USER_LIST_DEBUG
                    if (g_cVerbosity > 3)
                    {
                        RTStrPrintf(szDebugPath, sizeof(szDebugPath),
                                    "/VirtualBox/GuestInfo/Debug/LSA/Session/%RU32", pUserSession->ulLastSession);
                        VGSvcWritePropF(pCache->pClient, szDebugPath, "#%RU32: cSessionProcs=%RU32 (of %RU32 procs total)",
                                        g_uDebugIter, pUserSession->cInteractiveProcesses, cProcs);
                    }
#endif
                    g_pfnLsaFreeReturnBuffer(pSessionData);

                    /*
                     * Check if the user of this session is already in the paUserInfo array.
                     */
                    ULONG iUserInfo;
                    for (iUserInfo = 0; iUserInfo < cUniqueUsers; iUserInfo++)
                    {
                        PVBOXSERVICEVMINFOUSER const pCurUser = &paUserInfo[iUserInfo];
                        if (   !RTUtf16Cmp(pUserSession->wszUser, pCurUser->wszUser)
                            && !RTUtf16Cmp(pUserSession->wszLogonDomain, pCurUser->wszLogonDomain)
                            && !RTUtf16Cmp(pUserSession->wszAuthenticationPackage, pCurUser->wszAuthenticationPackage))
                        {
                            /** @todo r=bird: What if a user has two session, and it's the latter one
                             *        that is stale?  We'll hide the first one that still active with the
                             *        current approach... */

                            /*  Only respect the highest session for the current user. */
                            if (pUserSession->ulLastSession > pCurUser->ulLastSession)
                            {
                                VGSvcVerbose(4, "Updating user=%ls to %u processes (last used session: %RU32)\n",
                                             pCurUser->wszUser, pUserSession->cInteractiveProcesses, pUserSession->ulLastSession);

                                if (!pUserSession->cInteractiveProcesses)
                                    VGSvcVerbose(3, "Stale session for user=%ls detected! Processes: %RU32 -> 0, Session: %RU32 -> %RU32\n",
                                                 pCurUser->wszUser, pCurUser->cInteractiveProcesses, pCurUser->ulLastSession,
                                                 pUserSession->ulLastSession);

                                pCurUser->cInteractiveProcesses = pUserSession->cInteractiveProcesses;
                                pCurUser->ulLastSession         = pUserSession->ulLastSession;
                            }
                            /* There can be multiple session objects using the same session ID for the
                               current user -- so when we got the same session again just add the found
                               processes to it. */
                            else if (pCurUser->ulLastSession == pUserSession->ulLastSession)
                            {
                                VGSvcVerbose(4, "Updating processes for user=%ls (old procs=%RU32, new procs=%RU32, session=%RU32)\n",
                                             pCurUser->wszUser, pCurUser->cInteractiveProcesses,
                                             pUserSession->cInteractiveProcesses, pCurUser->ulLastSession);
                                pCurUser->cInteractiveProcesses = pUserSession->cInteractiveProcesses;
                            }
                            break;
                        }
                    }

                    /*
                     * If we got thru the array, it's a new unique user which we should add.
                     *
                     * Since pUserSession already points to the next array entry, there
                     * isn't much to do here other than updating the interactive process count.
                     */
                    if (iUserInfo >= cUniqueUsers)
                    {
                        VGSvcVerbose(4, "Adding new user=%ls (session=%RU32) with %RU32 processes\n",
                                     pUserSession->wszUser, pUserSession->ulLastSession, pUserSession->cInteractiveProcesses);
                        cUniqueUsers++;
                        Assert(cUniqueUsers <= cSessions);
                    }
                }
            }

            vgsvcVMInfoWinProcessesFree(cProcs, paProcs, pvExtra); /* (free it early so we got more heap for string conversion) */

#ifdef VGSVC_VMINFO_WIN_QUERY_USER_LIST_DEBUG
            if (g_cVerbosity > 3)
                VGSvcWritePropF(pCache->pClient, "/VirtualBox/GuestInfo/Debug/LSA",
                                "#%RU32: cSessions=%RU32, cProcs=%RU32, cUniqueUsers=%RU32",
                                g_uDebugIter, cSessions, cProcs, cUniqueUsers);
#endif
            VGSvcVerbose(3, "Found %u unique logged-in user%s\n", cUniqueUsers, cUniqueUsers == 1 ? "" : "s");

            /*
             * Publish the unique user information that we've destilled above.
             */
            for (ULONG i = 0; i < cUniqueUsers; i++)
            {
#ifdef VGSVC_VMINFO_WIN_QUERY_USER_LIST_DEBUG
                if (g_cVerbosity > 3)
                {
                    RTStrPrintf(szDebugPath, sizeof(szDebugPath), "/VirtualBox/GuestInfo/Debug/LSA/User/%RU32", i);
                    VGSvcWritePropF(pCache->pClient, szDebugPath, "#%RU32: szName=%ls, sessionID=%RU32, cProcs=%RU32",
                                    g_uDebugIter, paUserInfo[i].wszUser, paUserInfo[i].ulLastSession, paUserInfo[i].cInteractiveProcesses);
                }
#endif
                if (paUserInfo[i].cInteractiveProcesses > 0) /* (non-stale sessions only) */
                {
                    VGSvcVerbose(3, "User '%ls' has %RU32 interactive processes (session=%RU32)\n",
                                 paUserInfo[i].wszUser, paUserInfo[i].cInteractiveProcesses, paUserInfo[i].ulLastSession);

                    char *pszUser = NULL;
                    rc = RTUtf16ToUtf8(paUserInfo[i].wszUser, &pszUser);
                    if (RT_SUCCESS(rc))
                    {
                        VGSvcVMInfoAddUserToList(pUserGatherer, pszUser, "win", false /*fCheckUnique*/);

                        char *pszDomain = NULL;
                        if (paUserInfo[i].wszLogonDomain)
                            rc = RTUtf16ToUtf8(paUserInfo[i].wszLogonDomain, &pszDomain);
                        if (RT_SUCCESS(rc))
                        {
                            rc = vgsvcVMInfoWinWriteLastInput(pCache, pszUser, pszDomain);
                            RTStrFree(pszDomain);
                        }
                        RTStrFree(pszUser);
                    }
                    else
                        VGSvcVMInfoAddUserToList(pUserGatherer, "<conv-error>", "win", false /*fCheckUnique*/);
                    AssertRCBreak(rc); /** @todo is this sensible behaviour? */
                }
            }

            RTMemTmpFree(paUserInfo);
        }
        else
        {
            vgsvcVMInfoWinProcessesFree(cProcs, paProcs, pvExtra);
            VGSvcError("Not enough memory to store unique users!\n");
            rc = VERR_NO_MEMORY;
        }
    }
    else if (rc == VERR_NO_MEMORY)
        VGSvcError("Not enough memory to enumerate processes\n");
    else
        VGSvcError("Failed to enumerate processes: rc=%Rrc\n", rc);
    if (paSessions)
        g_pfnLsaFreeReturnBuffer(paSessions);

#ifdef VGSVC_VMINFO_WIN_QUERY_USER_LIST_DEBUG
    g_uDebugIter++;
#endif
    return rc;
}



/*********************************************************************************************************************************
*   Populate /VirtualBox/GuestAdd/Components/                                                                                    *
*********************************************************************************************************************************/

/**
 * Helper for vgsvcVMInfoWinGetFileVersion and attempts to read and parse
 * FileVersion.
 *
 * @returns Success indicator.
 */
static bool vgsvcVMInfoWinGetFileVersionOwn(LPSTR pVerData, uint32_t *puMajor, uint32_t *puMinor,
                                            uint32_t *puBuildNumber, uint32_t *puRevisionNumber)
{
    UINT    cchStrValue = 0;
    LPTSTR  pStrValue   = NULL;
    if (!VerQueryValueA(pVerData, "\\StringFileInfo\\040904b0\\FileVersion", (LPVOID *)&pStrValue, &cchStrValue))
        return false;

    char *pszNext = pStrValue;
    int rc = RTStrToUInt32Ex(pszNext, &pszNext, 0, puMajor);
    AssertReturn(rc == VWRN_TRAILING_CHARS, false);
    AssertReturn(*pszNext == '.', false);

    rc = RTStrToUInt32Ex(pszNext + 1, &pszNext, 0, puMinor);
    AssertReturn(rc == VWRN_TRAILING_CHARS, false);
    AssertReturn(*pszNext == '.', false);

    rc = RTStrToUInt32Ex(pszNext + 1, &pszNext, 0, puBuildNumber);
    AssertReturn(rc == VWRN_TRAILING_CHARS, false);
    AssertReturn(*pszNext == '.', false);

    rc = RTStrToUInt32Ex(pszNext + 1, &pszNext, 0, puRevisionNumber);
    AssertReturn(rc == VINF_SUCCESS || rc == VWRN_TRAILING_CHARS /*??*/, false);

    return true;
}


/**
 * Gets version number and revision from the VS_FIXEDFILEINFO table of the given
 * file, if found and present.
 *
 * @returns VBox status code.  Will always set the return variables to a value,
 *           regardless of status code.
 * @param   pszFilename         ASCII & ANSI & UTF-8 compliant name.
 * @param   puMajor             Where to return the major version number.
 * @param   puMinor             Where to return the minor version number.
 * @param   puBuildNumber       Where to return the build number.
 * @param   puRevisionNumber    Where to return the revision number.
 */
static int vgsvcVMInfoWinGetFileVersion(const char *pszFilename, uint32_t *puMajor, uint32_t *puMinor,
                                        uint32_t *puBuildNumber, uint32_t *puRevisionNumber)
{
    int rc;

    *puMajor = *puMinor = *puBuildNumber = *puRevisionNumber = 0;

    /*
     * Get the file version info.
     */
    DWORD dwHandleIgnored;
    DWORD cbVerData = GetFileVersionInfoSizeA(pszFilename, &dwHandleIgnored);
    if (cbVerData)
    {
        LPTSTR pVerData = (LPTSTR)RTMemTmpAllocZ(cbVerData);
        if (pVerData)
        {
            if (GetFileVersionInfoA(pszFilename, dwHandleIgnored, cbVerData, pVerData))
            {
                /*
                 * Try query and parse the FileVersion string our selves first
                 * since this will give us the correct revision number when
                 * it goes beyond the range of an uint16_t / WORD.
                 */
                if (vgsvcVMInfoWinGetFileVersionOwn(pVerData, puMajor, puMinor, puBuildNumber, puRevisionNumber))
                    rc = VINF_SUCCESS;
                else
                {
                    /* Fall back on VS_FIXEDFILEINFO */
                    UINT                 cbFileInfoIgnored = 0;
                    VS_FIXEDFILEINFO    *pFileInfo = NULL;
                    if (VerQueryValue(pVerData, "\\", (LPVOID *)&pFileInfo, &cbFileInfoIgnored))
                    {
                        *puMajor          = HIWORD(pFileInfo->dwFileVersionMS);
                        *puMinor          = LOWORD(pFileInfo->dwFileVersionMS);
                        *puBuildNumber    = HIWORD(pFileInfo->dwFileVersionLS);
                        *puRevisionNumber = LOWORD(pFileInfo->dwFileVersionLS);
                        rc = VINF_SUCCESS;
                    }
                    else
                    {
                        rc = RTErrConvertFromWin32(GetLastError());
                        VGSvcVerbose(3, "No file version value for file '%s' available! (%d / rc=%Rrc)\n",
                                     pszFilename,  GetLastError(), rc);
                    }
                }
            }
            else
            {
                rc = RTErrConvertFromWin32(GetLastError());
                VGSvcVerbose(0, "GetFileVersionInfo(%s) -> %u / %Rrc\n", pszFilename, GetLastError(), rc);
            }

            RTMemTmpFree(pVerData);
        }
        else
        {
            VGSvcVerbose(0, "Failed to allocate %u byte for file version info for '%s'\n", cbVerData, pszFilename);
            rc = VERR_NO_TMP_MEMORY;
        }
    }
    else
    {
        rc = RTErrConvertFromWin32(GetLastError());
        VGSvcVerbose(3, "GetFileVersionInfoSize(%s) -> %u / %Rrc\n", pszFilename, GetLastError(), rc);
    }
    return rc;
}


/**
 * Called by vgsvcVMInfoWriteFixedProperties() to popuplate the
 * "/VirtualBox/GuestAdd/Components/" area with file versions.
 */
int VGSvcVMInfoWinWriteComponentVersions(PVBGLGSTPROPCLIENT pClient)
{
    /*
     * Gather the directories we need.
     *
     * The buffers are sized so that there will always be sufficent space
     * to append the filename, so all we have to do it to append the filename
     * before querying the information.
     *
     * ASSUME: szSysDir and szWinDir and derivatives are always ASCII compatible.
     */
#define IDX_DIR_SYSTEM32    0
#define IDX_DIR_DRIVERS     1
#ifdef RT_ARCH_AMD64
# define IDX_DIR_WOW64      2
#endif
    struct { char *pszDir; size_t cchDir; } aDirs[3] = { { NULL, 0 }, { NULL, 0 }, { NULL, 0 } };

    char szSysDir[MAX_PATH + GUEST_PROP_MAX_NAME_LEN] = {0};
    UINT const cchSysDir = GetSystemDirectory(szSysDir, MAX_PATH);
    aDirs[IDX_DIR_SYSTEM32].pszDir = szSysDir;
    aDirs[IDX_DIR_SYSTEM32].cchDir = cchSysDir;
    Assert(strlen(aDirs[IDX_DIR_SYSTEM32].pszDir) == cchSysDir);

    char szDriversDir[MAX_PATH + GUEST_PROP_MAX_NAME_LEN + 8] = {0};
    memcpy(szDriversDir, szSysDir, cchSysDir);
    memcpy(&szDriversDir[cchSysDir], "\\drivers", sizeof("\\drivers"));
    aDirs[IDX_DIR_DRIVERS].pszDir  = szDriversDir;
    aDirs[IDX_DIR_DRIVERS].cchDir  = cchSysDir + sizeof("\\drivers") - 1;
    Assert(strlen(aDirs[IDX_DIR_DRIVERS].pszDir) == aDirs[IDX_DIR_DRIVERS].cchDir);

#ifdef RT_ARCH_AMD64
    char szWow64Dir[MAX_PATH + GUEST_PROP_MAX_NAME_LEN + 12] = {0};
    UINT const cchWinDir = GetWindowsDirectory(szWow64Dir, MAX_PATH);
    memcpy(&szWow64Dir[cchWinDir], "\\SysWow64", sizeof("\\SysWow64"));
    aDirs[IDX_DIR_WOW64].pszDir    = szWow64Dir;
    aDirs[IDX_DIR_WOW64].cchDir    = cchWinDir + sizeof("\\SysWow64") - 1;
    Assert(strlen(aDirs[IDX_DIR_WOW64].pszDir) == aDirs[IDX_DIR_WOW64].cchDir);
#endif

    /*
     * The file information table.
     * Note! The filename must be less than 30 chars long!
     */
    static struct { const char *pszFilename; uint32_t cchFilename; uint32_t idxDir; } const s_aVBoxFiles[] =
    {
        /* \windows\system32: */
        { RT_STR_TUPLE("VBoxControl.exe"),      IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxService.exe"),      IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxTray.exe"),         IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxHook.dll"),         IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxMRXNP.dll"),        IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxGINA.dll"),         IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxCredProv.dll"),     IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxDisp.dll"),         IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxDispD3D.dll"),      IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxDX.dll"),           IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxGL.dll"),           IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxNine.dll"),         IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxSVGA.dll"),         IDX_DIR_SYSTEM32 },
        { RT_STR_TUPLE("VBoxOGL.dll"),          IDX_DIR_SYSTEM32 }, /* obsolete */
        /* \windows\system32\drivers: */
        { RT_STR_TUPLE("VBoxGuest.sys"),        IDX_DIR_DRIVERS },
        { RT_STR_TUPLE("VBoxSF.sys"),           IDX_DIR_DRIVERS },
        { RT_STR_TUPLE("VBoxMouse.sys"),        IDX_DIR_DRIVERS },
        { RT_STR_TUPLE("VBoxVideo.sys"),        IDX_DIR_DRIVERS },
        { RT_STR_TUPLE("VBoxWddm.sys"),         IDX_DIR_DRIVERS },
        { RT_STR_TUPLE("VBoxMouseNT.sys"),      IDX_DIR_DRIVERS },  /* obsolete */
#ifdef RT_ARCH_AMD64
        /* \windows\wow64: */
        { RT_STR_TUPLE("VBoxMRXNP-x86.dll"),    IDX_DIR_WOW64 },
        { RT_STR_TUPLE("VBoxDispD3D-x86.dll"),  IDX_DIR_WOW64 },
        { RT_STR_TUPLE("VBoxDX-x86.dll"),       IDX_DIR_WOW64 },
        { RT_STR_TUPLE("VBoxGL-x86.dll"),       IDX_DIR_WOW64 },
        { RT_STR_TUPLE("VBoxNine-x86.dll"),     IDX_DIR_WOW64 },
        { RT_STR_TUPLE("VBoxSVGA-x86.dll"),     IDX_DIR_WOW64 },
        { RT_STR_TUPLE("VBoxOGL-x86.dll"),      IDX_DIR_WOW64 },    /* obsolete*/
#endif
    };

    /* Preparet the base property name. */
    static char const s_szPropPrefix[] = "/VirtualBox/GuestAdd/Components/";
    char              szPropPath[GUEST_PROP_MAX_NAME_LEN];
    AssertCompile(sizeof("VBoxDispD3D-x86.dll") + sizeof(s_szPropPrefix) + 8 < sizeof(szPropPath));
    memcpy(szPropPath, s_szPropPrefix, sizeof(s_szPropPrefix));
    char * const      pszPropName      = &szPropPath[sizeof(s_szPropPrefix) - 1];
    size_t const      cbMaxPropName    = sizeof(szPropPath) - sizeof(s_szPropPrefix) + 1;

    /* Loop thru the file table and publish the info we find. */
    for (unsigned i = 0; i < RT_ELEMENTS(s_aVBoxFiles); i++)
    {
        AssertRelease(s_aVBoxFiles[i].cchFilename < cbMaxPropName);
        memcpy(pszPropName, s_aVBoxFiles[i].pszFilename, s_aVBoxFiles[i].cchFilename + 1);

        char * const pszPath = aDirs[s_aVBoxFiles[i].idxDir].pszDir;
        size_t const cchDir  = aDirs[s_aVBoxFiles[i].idxDir].cchDir;
        pszPath[cchDir] = '\\';
        memcpy(&pszPath[cchDir + 1], s_aVBoxFiles[i].pszFilename, s_aVBoxFiles[i].cchFilename + 1);
        Assert(strlen(pszPath) == cchDir + 1 + s_aVBoxFiles[i].cchFilename);

        uint32_t uMajor, uMinor, uBuild, uRev;
        int rc = vgsvcVMInfoWinGetFileVersion(pszPath, &uMajor, &uMinor, &uBuild, &uRev);
        if (RT_SUCCESS(rc))
        {
            char szVer[128];
            RTStrPrintf(szVer, sizeof(szVer), "%u.%u.%ur%u", uMajor, uMinor, uBuild, uRev);
            VGSvcWriteProp(pClient, szPropPath, szVer);
        }
        else
            VGSvcWriteProp(pClient, szPropPath, NULL);
    }

    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Interface IP change monitoring.                                                                                              *
*********************************************************************************************************************************/
static bool volatile    g_fVGSvcVMInfoWinIpChangeShutdown = false;
static RTTHREAD         g_hVGSvcVMInfoWinIpChangeThread   = NIL_RTTHREAD;
static HANDLE           g_hVGSvcVMInfoWinIpChangeEvent    = NULL;
static OVERLAPPED       g_VGSvcVMInfoWinIpChangeOverlapped;

/**
 * Initialize the windows specific stuff.
 *
 * Called by vgsvcVMInfoInit().
 */
void VGSvcVMInfoWinInit(void)
{
    RTOnce(&g_vgsvcWinVmInitOnce, vgsvcWinVmInfoInitOnce, NULL);

    if (g_pfnCancelIPChangeNotify && g_pfnNotifyAddrChange)
        g_hVGSvcVMInfoWinIpChangeEvent = CreateEventW(NULL /*pSecAttribs*/,  FALSE /*fManualReset*/,
                                                      FALSE /*fInitialState*/, NULL /*pwszName*/);
    else
        g_hVGSvcVMInfoWinIpChangeEvent = NULL;
}


/**
 * @callback_method_impl{FNRTTHREAD,
 *      Thread waiting for network interface change notifications.}
 *
 * This thread will signal the main vminfo service thread to update the
 * properties after a change notification comes in.  For paranoid reasons,
 * it adds a slight (0.5s) delay before doing so, which is one of the reasons
 * why we're using a separate thread for this. (Another reason is to keep the
 * main service code as similar as possible across guest OSes and not do ugly
 * windows specific waiting logic.)
 */
static DECLCALLBACK(int) vgsvcVMInfoWinIfIpChangeThread(RTTHREAD ThreadSelf, void *pvUser)
{
    VGSvcVerbose(3, "Starting...\n");
    bool volatile *pfShutdown = (bool volatile *)pvUser;
    RT_NOREF(ThreadSelf);

    /* Required for network information (must be called per thread). */
    if (g_pfnWSAStartup)
    {
        WSADATA wsaData;
        RT_ZERO(wsaData);
        if (g_pfnWSAStartup(MAKEWORD(2, 2), &wsaData))
            VGSvcError("VMInfo/Win/IfIpChange: WSAStartup failed! Error: %Rrc\n", RTErrConvertFromWin32(g_pfnWSAGetLastError()));
    }

    /*
     * Loop till we're told to shut down.
     */
    bool fIsPending = false;
    while (!*pfShutdown && !g_fVGSvcVMInfoWinIpChangeShutdown)
    {
        /*
         * Queue the notification request (it's just an async I/O control call).
         */
        if (!fIsPending)
        {
            RT_ZERO(g_VGSvcVMInfoWinIpChangeOverlapped);
            g_VGSvcVMInfoWinIpChangeOverlapped.hEvent = g_hVGSvcVMInfoWinIpChangeEvent;
            HANDLE      hIgnored = NULL;
            DWORD const rc       = g_pfnNotifyAddrChange(&hIgnored, &g_VGSvcVMInfoWinIpChangeOverlapped);
            if (rc == ERROR_IO_PENDING)
            {
                VGSvcVerbose(4, "Giving g_pfnNotifyAddrChange returns ERROR_IO_PENDING as expected.\n");
                fIsPending = true;
            }
            else if (rc == NO_ERROR)
            {
                VGSvcVerbose(4, "Giving g_pfnNotifyAddrChange returns NO_ERROR\n");
                SetEvent(g_hVGSvcVMInfoWinIpChangeEvent); /* paranoia */
            }
            else if (rc != NO_ERROR || rc != ERROR_CANCELLED)
            {
                VGSvcVerbose(1, "Giving up because g_pfnNotifyAddrChange failed: %u (%#x)\n", rc, rc);
                break;
            }

            if (*pfShutdown || g_fVGSvcVMInfoWinIpChangeShutdown)
                break;
        }

        /*
         * Wait for it to complete.
         */
        DWORD const rcWait = WaitForSingleObjectEx(g_hVGSvcVMInfoWinIpChangeEvent, INFINITE, TRUE /*bAlertable*/);
        VGSvcVerbose(5, "WaitForSingleObjectEx returns: %#x\n", rcWait);
        if (*pfShutdown || g_fVGSvcVMInfoWinIpChangeShutdown)
            break;
        if (rcWait == WAIT_OBJECT_0)
        {
            /*
             * The async NotifyAddrChange call completed, so notify the main service
             * thread so it can refresh the interfaces.
             *
             * Because we're a little paranoid, we delay half a second before doing it.
             * This has two reasons, first avoiding spinning at 100% CPU should this
             * code go nuts, and second to let the change settle a little.
             */
            fIsPending = false;
            if (   WaitForSingleObjectEx(g_hVGSvcVMInfoWinIpChangeEvent, 500, TRUE /*bAlertable*/) != WAIT_TIMEOUT
                && !g_fVGSvcVMInfoWinIpChangeShutdown
                && !*pfShutdown)
                RTThreadSleep(500);
            if (*pfShutdown || g_fVGSvcVMInfoWinIpChangeShutdown)
                break;
            VGSvcVerbose(3, "Signalling vminfo thread...\n");
            VGSvcVMInfoSignal();
        }
        else if (   rcWait != WAIT_IO_COMPLETION
                 && rcWait != WAIT_TIMEOUT /* impossible */)
        {
            VGSvcError("WaitForSingleObjectEx failed: %u (%#x), last error %u\n", rcWait, rcWait, GetLastError());
            break;
        }
    }

    /*
     * Cleanup.
     */
    if (fIsPending)
    {
        if (!g_pfnCancelIPChangeNotify(&g_VGSvcVMInfoWinIpChangeOverlapped))
            VGSvcError("CancelIPChangeNotify failed!\n");
    }

    VGSvcVerbose(3, "Terminating.\n");
    return VINF_SUCCESS;
}


/**
 * The worker thread is starting.
 *
 * Called by vgsvcVMInfoWorker().
 */
void VGSvcVMInfoWinWorkerStarting(bool volatile *pfShutdown)
{
    /*
     * If all the preconditions are present, start the waiter thread.
     */
    g_fVGSvcVMInfoWinIpChangeShutdown = false;
    if (   g_pfnCancelIPChangeNotify
        && g_pfnNotifyAddrChange
        && g_hVGSvcVMInfoWinIpChangeEvent)
    {
        int rc = RTThreadCreate(&g_hVGSvcVMInfoWinIpChangeThread, vgsvcVMInfoWinIfIpChangeThread, (void *)pfShutdown,
                                0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "if-wait");
        if (RT_FAILURE(rc))
        {
            g_hVGSvcVMInfoWinIpChangeThread = NIL_RTTHREAD;
            VGSvcError("RTThreadCreate failed: %Rrc\n", rc);
        }
    }
}


/**
 * The worker thread is stopping.
 *
 * Called by vgsvcVMInfoWorker().
 */
void VGSvcVMInfoWinWorkerStopping()
{
    if (g_hVGSvcVMInfoWinIpChangeThread != NIL_RTTHREAD)
    {
        /* Make the thread quit. */
        ASMAtomicWriteBool(&g_fVGSvcVMInfoWinIpChangeShutdown, true);
        SetEvent(g_hVGSvcVMInfoWinIpChangeEvent);

        /* Wait for it to do so. */
        int rc = RTThreadWait(g_hVGSvcVMInfoWinIpChangeThread, RT_MS_10SEC, NULL);
        if (RT_SUCCESS(rc))
            g_hVGSvcVMInfoWinIpChangeThread = NIL_RTTHREAD;
    }
}


/**
 * Signal that the worker thread should stop.
 *
 * Called by vgsvcVMInfoStop().
 */
void VGSvcVMInfoWinStop(void)
{
    if (g_hVGSvcVMInfoWinIpChangeThread != NIL_RTTHREAD)
    {
        ASMAtomicWriteBool(&g_fVGSvcVMInfoWinIpChangeShutdown, true);
        SetEvent(g_hVGSvcVMInfoWinIpChangeEvent);
    }
}


/**
 * Terminate - cleanup stuff.
 *
 * Called by vgsvcVMInfoTerm().
 */
void VGSvcVMInfoWinTerm(void)
{
    if (g_hVGSvcVMInfoWinIpChangeThread != NIL_RTTHREAD)
        VGSvcVMInfoWinWorkerStopping();
}

