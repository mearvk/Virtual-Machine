/* $Id: VBoxServiceInternal.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBoxService - Guest Additions Services.
 */

/*
 * Copyright (C) 2007-2025 Oracle and/or its affiliates.
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

#ifndef GA_INCLUDED_SRC_common_VBoxService_VBoxServiceInternal_h
#define GA_INCLUDED_SRC_common_VBoxService_VBoxServiceInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
#endif

#include <iprt/list.h>
#include <iprt/critsect.h>
#include <iprt/path.h> /* RTPATH_MAX */
#include <iprt/stdarg.h>
#include <iprt/getopt.h>

#include <VBox/VBoxGuestLib.h>
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/VBoxGuestLibGuestProp.h>
# include <VBox/HostServices/GuestControlSvc.h>
#endif


#if !defined(RT_OS_WINDOWS) || defined(DOXYGEN_RUNNING)
/** Special argv[1] value that indicates that argv is UTF-8.
 * This causes RTR3Init to be called with RTR3INIT_FLAGS_UTF8_ARGV and helps
 * work around potential issues caused by a user's locale config not being
 * UTF-8.  See @bugref{10153}.
 *
 * @note We don't need this on windows and it would be harmful to enable it
 *       as the argc/argv vs __argc/__argv comparison would fail and we would
 *       not use the unicode command line to create a UTF-8 argv.  Since the
 *       original argv is ANSI, it may be missing codepoints not present in
 *       the ANSI code page of the process. */
# define VBOXSERVICE_ARG1_UTF8_ARGV         "--utf8-argv"
#endif
/** RTProcCreateEx flags corresponding to VBOXSERVICE_ARG1_UTF8_ARGV. */
#ifdef VBOXSERVICE_ARG1_UTF8_ARGV
# define VBOXSERVICE_PROC_F_UTF8_ARGV       RTPROC_FLAGS_UTF8_ARGV
#else
# define VBOXSERVICE_PROC_F_UTF8_ARGV       0
#endif


/**
 * A service descriptor.
 */
typedef struct
{
    /** The short service name. */
    const char     *pszName;
    /** The longer service name. */
    const char     *pszDescription;
    /** The usage options stuff for the --help screen. */
    const char     *pszUsage;
    /** The option descriptions for the --help screen. */
    const char     *pszOptions;
    /** Array of option descriptors for the service. */
    PCRTGETOPTDEF   paOptions;
    /** Number of options in paOptions. */
    size_t          cOptions;

    /**
     * Called before parsing arguments.
     * @returns VBox status code.
     */
    DECLCALLBACKMEMBER(int, pfnPreInit,(void));

    /**
     * Handles an sub-service option.
     *
     * @returns RTEXITCODE_SUCCESS on success, RTEXITCODE_SYNTAX or
     *          RTEXITCODE_FAILURE on error.
     *
     * @param   iShort      The short option (from VBOXSERVICE::paOptions).
     * @param   pValueUnion Where to find the option value (if any).
     * @param   fCmdLine    Set if the option came from the command line,
     *                      clear if it came from the host (guest properties).
     */
    DECLCALLBACKMEMBER(RTEXITCODE, pfnOption,(int iShort, PCRTGETOPTUNION pValueUnion, bool fCmdLine));

    /**
     * Called before parsing arguments.
     * @returns VBox status code.
     */
    DECLCALLBACKMEMBER(int, pfnInit,(void));

    /** Called from the worker thread.
     *
     * @returns VBox status code.
     * @retval  VINF_SUCCESS if exitting because *pfShutdown was set.
     * @param   pfShutdown      Pointer to a per service termination flag to check
     *                          before and after blocking.
     */
    DECLCALLBACKMEMBER(int, pfnWorker,(bool volatile *pfShutdown));

    /**
     * Stops a service.
     */
    DECLCALLBACKMEMBER(void, pfnStop,(void));

    /**
     * Does termination cleanups.
     *
     * @remarks This may be called even if pfnInit hasn't been called!
     */
    DECLCALLBACKMEMBER(void, pfnTerm,(void));
} VBOXSERVICE;
/** Pointer to a VBOXSERVICE. */
typedef VBOXSERVICE *PVBOXSERVICE;
/** Pointer to a const VBOXSERVICE. */
typedef VBOXSERVICE const *PCVBOXSERVICE;

/* Default call-backs for services which do not need special behaviour. */
DECLCALLBACK(int)           VGSvcDefaultPreInit(void);
DECLCALLBACK(RTEXITCODE)    VGSvcDefaultOption(int iShort, PCRTGETOPTUNION pValueUnion, bool fCmdLine);
DECLCALLBACK(int)           VGSvcDefaultInit(void);
DECLCALLBACK(void)          VGSvcDefaultTerm(void);

/** Option short numbers. */
typedef enum VBSVCOPT
{
    kVGSvcOptFirst = 1000,

    kVGSvcOptVminfoInterval,
    kVGSvcOptVminfoUserIdleThreshold,

    kVGSvcOptGstCtrlInterval,
#ifdef DEBUG
    kVGSvcOptGstCtrlDumpStdOut,
    kVGSvcOptGstCtrlDumpStdErr,
#endif

    kVGSvcOptTimeSyncInterval,
    kVGSvcOptTimeSyncMinAdjust,
    kVGSvcOptTimeSyncLatencyFactor,
    kVGSvcOptTimeSyncMaxLatency,
    kVGSvcOptTimeSyncSetThreshold,
    kVGSvcOptTimeSyncSetStart,
    kVGSvcOptTimeSyncNoSetStart,
    kVGSvcOptTimeSyncSetOnRestore,
    kVGSvcOptTimeSyncNoSetOnRestore,
    kVGSvcOptTimeSyncVerbosity,

    kVGSvcOptGlobalDaemonzied,
    /** The options following this one, are handled in the first cmdline pass
     *  and ignored later in vgsvcProcessCmdLineOptions(). */
    kVGSvcOptGlobalStartFirstPassOnly,
    kVGSvcOptGlobalHostOptsFirst  = kVGSvcOptGlobalStartFirstPassOnly,
    kVGSvcOptGlobalHostOptsLast,
    kVGSvcOptGlobalNoHostOpts,
    /* These next three options each have a range of 64 and must come last. */
    kVGSvcOptGlobalOnlyService,
    kVGSvcOptGlobalDisableService = kVGSvcOptGlobalOnlyService    + 64,
    kVGSvcOptGlobalEnableService  = kVGSvcOptGlobalDisableService + 64,
    kVGSvcOptEnd                  = kVGSvcOptGlobalEnableService  + 64
} VBSVCOPT;


/** The service name.
 * @note Used on windows to name the service as well as the global mutex. */
#define VBOXSERVICE_NAME            "VBoxService"

#ifdef RT_OS_WINDOWS
/** The friendly service name. */
# define VBOXSERVICE_FRIENDLY_NAME  "VirtualBox Guest Additions Service"
/** The service description (only W2K+ atm) */
# define VBOXSERVICE_DESCRIPTION    "Manages VM runtime information, time synchronization, guest control execution and miscellaneous utilities for guest operating systems."
/** The following constant may be defined by including NtStatus.h. */
# define STATUS_SUCCESS             ((NTSTATUS)0x00000000L)
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_GUEST_PROPS
/**
 * A guest property cache.
 */
typedef struct VBOXSERVICEVEPROPCACHE
{
    /** The guest property client session info. */
    PVBGLGSTPROPCLIENT  pClient;
    /** Head in a list of VBOXSERVICEVEPROPCACHEENTRY nodes. */
    RTLISTANCHOR        NodeHead;
    /** Critical section for thread-safe use. */
    RTCRITSECT          CritSect;
} VBOXSERVICEVEPROPCACHE;
/** Pointer to a guest property cache. */
typedef VBOXSERVICEVEPROPCACHE *PVBOXSERVICEVEPROPCACHE;

/**
 * An entry in the property cache (VBOXSERVICEVEPROPCACHE).
 */
typedef struct VBOXSERVICEVEPROPCACHEENTRY
{
    /** Node to successor.
     * @todo r=bird: This is not really the node to the successor, but
     *       rather the OUR node in the list.  If it helps, remember that
     *       its a doubly linked list. */
    RTLISTNODE  NodeSucc;
    /** Name (and full path) of guest property. */
    char       *pszName;
    /** The last value stored (for reference). */
    char       *pszValue;
    /** Reset value to write if property is temporary.  If NULL, it will be
     *  deleted. */
    char       *pszValueReset;
    /** Flags. */
    uint32_t    fFlags;
    /** Used to delete stale entries under /VirtualBox/GuestInfo/User/.
     * This is set by calling VGSvcPropCacheMarkNotUpdatedByPath() and cleared
     * automatically when any of the update functions are called. */
    bool        fNotUpdated;
} VBOXSERVICEVEPROPCACHEENTRY;
/** Pointer to a cached guest property. */
typedef VBOXSERVICEVEPROPCACHEENTRY *PVBOXSERVICEVEPROPCACHEENTRY;
/* forward decl. */
struct VBOXSERVICEVMINFOUSERLIST;

/** @name VGSVCPROPCACHE_FLAG_XXX - Guest Property Cache Flags.
 * @{ */
/** Indicates wheter a guest property is temporary and either should
 *  - a) get a "reset" value assigned (via VBoxServicePropCacheUpdateEntry)
 *       as soon as the property cache gets destroyed, or
 *  - b) get deleted when no reset value is specified.
 */
# define VGSVCPROPCACHE_FLAGS_TEMPORARY             RT_BIT_32(0)
/** Indicates whether a property every time needs to be updated, regardless
 *  if its real value changed or not. */
# define VGSVCPROPCACHE_FLAGS_ALWAYS_UPDATE         RT_BIT_32(1)
/** The guest property gets deleted when the VM gets shutdown, rebooted, reset,
 *  or powered off.  Maps directly to the guest property TRANSRESET flag. */
# define VGSVCPROPCACHE_FLAGS_TRANSIENT             RT_BIT_32(2)

/** Same as VGSVCPROPCACHE_FLAGS_TEMPORARY|VGSVCPROPCACHE_FLAGS_TEMPORARY to
 *  convey intention at reset. */
# define VGSVCPROPCACHE_FLAGS_TMP_DEL_TRANSRESET    (VGSVCPROPCACHE_FLAGS_TEMPORARY | VGSVCPROPCACHE_FLAGS_TRANSIENT)
/** @}  */
#endif /* VBOX_WITH_GUEST_PROPS */


/** ID cache entry. */
typedef struct VGSVCUIDENTRY
{
    /** The identifier name. */
    uint32_t    id;
    /** Set if UID, clear if GID. */
    bool        fIsUid;
    /** The name. */
    char        szName[128 - 4 - 1];
} VGSVCUIDENTRY;
typedef VGSVCUIDENTRY *PVGSVCUIDENTRY;


/** ID cache. */
typedef struct VGSVCIDCACHE
{
    /** Number of valid cache entries. */
    uint32_t                cEntries;
    /** The next entry to replace. */
    uint32_t                iNextReplace;
    /** The cache entries. */
    VGSVCUIDENTRY           aEntries[16];
} VGSVCIDCACHE;
typedef VGSVCIDCACHE *PVGSVCIDCACHE;


RT_C_DECLS_BEGIN

extern char        *g_pszProgName;
extern unsigned     g_cVerbosity;
extern char         g_szLogFile[RTPATH_MAX + 128];
extern uint32_t     g_cSecDefaultInterval;
extern VBOXSERVICE  g_TimeSync;
#ifdef VBOX_WITH_VBOXSERVICE_CLIPBOARD
extern VBOXSERVICE  g_Clipboard;
#endif
extern VBOXSERVICE  g_Control;
extern VBOXSERVICE  g_VMInfo;
extern VBOXSERVICE  g_CpuHotPlug;
#ifdef VBOX_WITH_VBOXSERVICE_MANAGEMENT
extern VBOXSERVICE  g_MemBalloon;
extern VBOXSERVICE  g_VMStatistics;
#endif
#ifdef VBOX_WITH_VBOXSERVICE_PAGE_SHARING
extern VBOXSERVICE  g_PageSharing;
#endif
#ifdef VBOX_WITH_SHARED_FOLDERS
extern VBOXSERVICE  g_AutoMount;
#endif

#ifdef RT_OS_WINDOWS
extern VBOXSERVICE g_DisplayConfig;
#endif

#ifdef DEBUG
extern RTCRITSECT   g_csLog; /* For guest process stdout dumping. */
#endif

extern RTEXITCODE               VGSvcSyntax(bool fCmdLine, const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(2, 3);
extern RTEXITCODE               VGSvcError(const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(1, 2);
extern void                     VGSvcVerbose(unsigned iLevel, const char *pszFormat, ...)  RT_IPRT_FORMAT_ATTR(2, 3);
extern int                      VGSvcLogCreate(const char *pszLogFile);
extern void                     VGSvcLogV(const char *pszFormat, va_list va) RT_IPRT_FORMAT_ATTR(1, 0);
extern void                     VGSvcLogDestroy(void);
extern RTEXITCODE               VGSvcOptUInt32(uint32_t *puValue, PCRTGETOPTUNION pValueUnion, uint32_t uMin, uint32_t uMax,
                                               const char *pszUnit, const char *pszDesc, const char *pszService, bool fCmdLine);

/* Exposing the following bits because of windows: */
extern int                      VGSvcStartServices(void);
extern int                      VGSvcStopServices(void);
extern void                     VGSvcMainWait(void);
extern int                      VGSvcReportStatus(VBoxGuestFacilityStatus enmStatus);
#ifdef RT_OS_WINDOWS
extern void                     VGSvcWinResolveApis(void);
extern RTEXITCODE               VGSvcWinInstall(void);
extern RTEXITCODE               VGSvcWinUninstall(void);
extern RTEXITCODE               VGSvcWinEnterCtrlDispatcher(void);
extern void                     VGSvcWinSetStopPendingStatus(uint32_t uCheckPoint);
# ifdef TH32CS_SNAPHEAPLIST
extern decltype(CreateToolhelp32Snapshot)      *g_pfnCreateToolhelp32Snapshot;
extern decltype(Process32First)                *g_pfnProcess32First;
extern decltype(Process32Next)                 *g_pfnProcess32Next;
extern decltype(Module32First)                 *g_pfnModule32First;
extern decltype(Module32Next)                  *g_pfnModule32Next;
# endif
extern decltype(GetSystemTimeAdjustment)       *g_pfnGetSystemTimeAdjustment;
extern decltype(SetSystemTimeAdjustment)       *g_pfnSetSystemTimeAdjustment;
# ifdef IPRT_INCLUDED_nt_nt_h
extern decltype(ZwQuerySystemInformation)      *g_pfnZwQuerySystemInformation;
# endif
extern ULONG (WINAPI *g_pfnGetAdaptersInfo)(struct _IP_ADAPTER_INFO *, PULONG);
#ifdef WINSOCK_VERSION
extern decltype(WSAStartup)                    *g_pfnWSAStartup;
extern decltype(WSACleanup)                    *g_pfnWSACleanup;
extern decltype(WSASocketA)                    *g_pfnWSASocketA;
extern decltype(WSAIoctl)                      *g_pfnWSAIoctl;
extern decltype(WSAGetLastError)               *g_pfnWSAGetLastError;
extern decltype(closesocket)                   *g_pfnclosesocket;
extern decltype(inet_ntoa)                     *g_pfninet_ntoa;
# endif /* WINSOCK_VERSION */

# ifdef SE_INTERACTIVE_LOGON_NAME
extern decltype(LsaNtStatusToWinError)         *g_pfnLsaNtStatusToWinError;
# endif
#endif /* RT_OS_WINDOWS */

/* VBoxServiceVMInfo.cpp & VBoxServiceVMInfo-win.cpp */
extern int                      VGSvcVMInfoSignal(void);
#ifdef VBOX_WITH_GUEST_PROPS
extern void                     VGSvcVMInfoAddUserToList(struct VBOXSERVICEVMINFOUSERLIST *pUserGatherer,
                                                         const char *pszName, const char *pszSource, bool fCheckUnique);
# ifdef RT_OS_WINDOWS
extern int                      VGSvcVMInfoUpdateUser(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                                                      const char *pszKey, const char *pszValue);
extern int                      VGSvcVMInfoUpdateUserF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                                                       const char *pszKey, const char *pszValueFormat, ...);
extern int                      VGSvcVMInfoUpdateUserV(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                                                       const char *pszKey, const char *pszValueFormat, va_list va);
extern int                      VGSvcVMInfoWinQueryUserListAndUpdateInfo(struct VBOXSERVICEVMINFOUSERLIST *pUserGatherer,
                                                                         PVBOXSERVICEVEPROPCACHE pCache);
extern int                      VGSvcVMInfoWinWriteComponentVersions(PVBGLGSTPROPCLIENT pClient);
extern void                     VGSvcVMInfoWinInit(void);
extern void                     VGSvcVMInfoWinWorkerStarting(bool volatile *pfShutdown);
extern void                     VGSvcVMInfoWinWorkerStopping(void);
extern void                     VGSvcVMInfoWinStop(void);
extern void                     VGSvcVMInfoWinTerm(void);
extern uint32_t                                 g_cMsVMInfoUserIdleThreshold;
# endif

/* VBoxServicePropCache.cpp */
extern int                      VGSvcPropCacheInit(PVBOXSERVICEVEPROPCACHE pCache, PVBGLGSTPROPCLIENT pClient);
extern void                     VGSvcPropCacheTerm(PVBOXSERVICEVEPROPCACHE pCache);
extern int                      VGSvcPropCacheDeclareEntry(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t fFlags,
                                                           const char *pszValueReset = NULL);
extern int                      VGSvcPropCacheUpdate(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValue);
extern int                      VGSvcPropCacheUpdateF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName,
                                                      const char *pszValueFormat, ...) RT_IPRT_FORMAT_ATTR(3, 4);
extern int                      VGSvcPropCacheUpdateEx(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValue,
                                                       uint32_t fFlags, const char *pszValueReset);
extern int                      VGSvcPropCacheUpdateExF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t fFlags,
                                                        const char *pszValueReset, const char *pszValueFormat, ...) RT_IPRT_FORMAT_ATTR(5, 6);
extern int                      VGSvcPropCacheUpdateTdtr(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValue);
extern int                      VGSvcPropCacheUpdateTdtrF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName,
                                                          const char *pszValueFormat, ...) RT_IPRT_FORMAT_ATTR(3, 4);
extern int                      VGSvcPropCacheMarkNotUpdatedByPath(PVBOXSERVICEVEPROPCACHE pCache, const char *pszPath);
extern int                      VGSvcPropCachedDeleteNotUpdated(PVBOXSERVICEVEPROPCACHE pCache);
extern int                      VGSvcPropCacheFlush(PVBOXSERVICEVEPROPCACHE pCache);
#endif /* VBOX_WITH_GUEST_PROPS */

/* VBoxServiceBalloon.cpp */
#ifdef VBOX_WITH_MEMBALLOON
extern uint32_t                 VGSvcBalloonQueryPages(uint32_t cbPage);
#endif

/* VBoxServicePageSharing.cpp */
#if defined(VBOX_WITH_VBOXSERVICE_PAGE_SHARING)
extern RTEXITCODE               VGSvcPageSharingWorkerChild(void);
#endif

/* VBoxServiceUtils.cpp */
#ifdef VBOX_WITH_GUEST_PROPS
extern int                      VGSvcReadProp(PVBGLGSTPROPCLIENT pGuestPropClient, const char *pszPropName,
                                              char **ppszValue, char **ppszFlags, uint64_t *puTimestamp);
extern int                      VGSvcReadPropUInt32(PVBGLGSTPROPCLIENT pGuestPropClient, const char *pszPropName,
                                                    uint32_t *pu32, uint32_t u32Min, uint32_t u32Max);
extern int                      VGSvcReadHostProp(PVBGLGSTPROPCLIENT pGuestPropClient, const char *pszPropName, bool fReadOnly,
                                                  char **ppszValue, char **ppszFlags, uint64_t *puTimestamp);
extern int                      VGSvcWriteProp(PVBGLGSTPROPCLIENT pGuestPropClient, const char *pszName, const char *pszValue);
extern int                      VGSvcWritePropF(PVBGLGSTPROPCLIENT pGuestPropClient, const char *pszName,
                                                const char *pszValueFormat, ...) RT_IPRT_FORMAT_ATTR(3, 4);
#endif
extern const char              *VGSvcIdCacheGetUidName(PVGSVCIDCACHE pIdCache, RTUID uid, const char *pszEntry,
                                                       const char *pszRelativeTo);
extern const char              *VGSvcIdCacheGetGidName(PVGSVCIDCACHE pIdCache, RTGID gid, const char *pszEntry,
                                                       const char *pszRelativeTo);

RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_common_VBoxService_VBoxServiceInternal_h */

