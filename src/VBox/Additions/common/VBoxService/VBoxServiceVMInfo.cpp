/* $Id: VBoxServiceVMInfo.cpp 111836 2025-11-21 08:37:59Z alexander.eichner@oracle.com $ */
/** @file
 * VBoxService - Virtual Machine Information for the Host.
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

/** @page pg_vgsvc_vminfo VBoxService - VM Information
 *
 * The VM Information subservice provides heaps of useful information about the
 * VM via guest properties.
 *
 * Guest properties is a limited database maintained by the HGCM GuestProperties
 * service in cooperation with the Main API (VBoxSVC).  Properties have a name
 * (ours are path-like), a string value, and a nanosecond timestamp (unix epoch
 * base).  The timestamp lets the user see how recent the information is.  As an
 * laternative to polling on changes, it is also possible to wait on changes via
 * the Main API or VBoxManage on the host side and VBoxControl in the guest.
 *
 * The namespace "/VirtualBox/" is reserved for value provided by VirtualBox.
 *
 * This subservice provides all the information under "/VirtualBox/GuestInfo/" (
 * except for (some stupid reson) "/VirtualBox/GuestInfo/OS/AutoLogonStatus"
 * which is written by the GINA and credential provider plugins on Windows) and
 * some bits under "/VirtualBox/GuestAdd/"
 *
 * The properties are either of a fixed or transient (changing/temporary) in
 * nature. The fixed properties are typically updated only when the subservice
 * starts and the transient ones are updated at an interval.
 *
 *
 * @section sec_vgsvc_vminfo_fixed      Fixed Properties
 *
 * To the fixed properties belong things like guest OS version, additions
 * version, additions installation path and component info.
 *
 *
 * @section sec_vgsvc_vminfo_transient  Transient Properties & Caching
 *
 * Transient properties provided by this subservice are for things are may
 * change over time, for instance network addresses, the number of logged in
 * users and such.  These properties will be delete (or in rare cases set to a
 * special value) when the subservice is shut down.  When the VM is reset or
 * powered off they will deleted.
 *
 * They are updated by the subservice every so often, with the interval given by
 * the --vminfo-interval or --interval options.
 *
 * While the information provided by these properties is volatile, it is
 * typically not changing at every update interval.  So, to avoid frequent
 * writing of the same info to the host, we use a 'cache' to track the
 * information we've already written to the host and how to deal with it when
 * the subservice shuts down.  (While the 'cache' can be used for fixed
 * properties, it is currently only used for transient ones and therefore it
 * defaults to the standard transient behaviour when adding new entries to it.)
 *
 *
 * @section sec_vgsvc_vminfo_beacons    Beacons
 *
 * As mentioned, the subservice does not write properties unless there are
 * changes. So, in order for the host side to know that information is up to
 * date despite an oldish timestamp we define a couple of values that are always
 * updated and can reliably used to figure how old the information actually is.
 *
 * For the networking part "/VirtualBox/GuestInfo/Net/Count" is the value to
 * watch out for.
 *
 * For the login part "/VirtualBox/GuestInfo/OS/LoggedInUsers" is the value to
 * watch, starting with VBox 7.2.6.
 *
 *
 * @section sec_vgsvc_vminfo_properties Property List
 *
 * @todo list the properties with some explanation.
 *
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifdef RT_OS_WINDOWS
# include <iprt/win/winsock2.h>
# include <iprt/win/iphlpapi.h>
# include <iprt/win/ws2tcpip.h>
# include <iprt/win/windows.h>
# include <Ntsecapi.h>
#else
# define __STDC_LIMIT_MACROS
# include <arpa/inet.h>
# include <errno.h>
# include <netinet/in.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <net/if.h>
# include <pwd.h> /* getpwuid */
# include <unistd.h>
# if !defined(RT_OS_OS2) && !defined(RT_OS_FREEBSD) && !defined(RT_OS_HAIKU)
#  include <utmpx.h> /** @todo FreeBSD 9 should have this. */
# endif
# ifdef RT_OS_OS2
#  include <net/if_dl.h>
# endif
# ifdef RT_OS_SOLARIS
#  include <sys/sockio.h>
#  include <net/if_arp.h>
# endif
# if defined(RT_OS_DARWIN) || defined(RT_OS_FREEBSD) || defined(RT_OS_NETBSD)
#  include <ifaddrs.h> /* getifaddrs, freeifaddrs */
#  include <net/if_dl.h> /* LLADDR */
#  include <netdb.h> /* getnameinfo */
# endif
# ifdef VBOX_WITH_DBUS
#  include <VBox/dbus.h>
#  ifndef DBUS_TYPE_VARIANT
#   define DBUS_TYPE_VARIANT    ((int) 'v')
#  endif
# endif
#endif

#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/assert.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestPropertySvc.h> /* For GUEST_PROP_MAX_VALUE_LEN and GUEST_PROP_MAX_NAME_LEN.  */
#include "VBoxServiceInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** This enables some code for monitoring /VirtualBox/HostInfo/VRDP/Active
 * and logging changes (i.e. VDE client attach and detach).
 *
 * It doesn't seem to much more purpose beyond loging here in VBoxService.
 *
 * @note 2025-11-11 bird: Disabled this, as it has little purpose here.
 *       VBoxTray & VBoxClient is where this stuff would be useful. */
#if 0 || defined(DOXYGEN_RUNNING)
# define WITH_VDE_CONNECTION_MONITORING
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Structure for gathering list of logged in users.
 */
typedef struct VBOXSERVICEVMINFOUSERLIST
{
    /** Number of users in the list (szUserList). */
    uint32_t    cUsersInList;
    /** The user list length. */
    uint32_t    cchUserList;
    /** Number of user names dropped due to list overflow. */
    uint32_t    cOverflowUsers;
    /** The user list. */
    char        szUserList[GUEST_PROP_MAX_VALUE_LEN];
} VBOXSERVICEVMINFOUSERLIST;
/** Pointer to a user list gathering structure. */
typedef VBOXSERVICEVMINFOUSERLIST *PVBOXSERVICEVMINFOUSERLIST;


#ifdef WITH_VDE_CONNECTION_MONITORING
/** Structure containing information about a location awarness
 *  client provided by the host. */
/** @todo Move this (and functions) into VbglR3. */
typedef struct VBOXSERVICELACLIENTINFO
{
    uint32_t    uID;
    char       *pszName;
    char       *pszLocation;
    char       *pszDomain;
    bool        fAttached;
    uint64_t    nsAttachedTS;
} VBOXSERVICELACLIENTINFO, *PVBOXSERVICELACLIENTINFO;
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The vminfo interval (milliseconds). */
static uint32_t                 g_cMsVMInfoInterval = 0;
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI          g_hVMInfoEvent = NIL_RTSEMEVENTMULTI;
/** The guest property service client session details. */
static VBGLGSTPROPCLIENT        g_VMInfoGuestPropSvcClient;
/** Number of currently logged in users in OS.
 * @note This for handling out-of-memory situations in what we hope is a
 *       reasonable manner... */
static uint32_t                 g_cVMInfoLoggedInUsers = 0;
/** The guest property cache. */
static VBOXSERVICEVEPROPCACHE   g_VMInfoPropCache;
static const char              *g_pszPropCacheValLoggedInUsersList = "/VirtualBox/GuestInfo/OS/LoggedInUsersList";
static const char              *g_pszPropCacheValLoggedInUsers     = "/VirtualBox/GuestInfo/OS/LoggedInUsers";
static const char              *g_pszPropCacheValNoLoggedInUsers   = "/VirtualBox/GuestInfo/OS/NoLoggedInUsers";
static const char              *g_pszPropCacheValNetCount          = "/VirtualBox/GuestInfo/Net/Count";
#ifdef RT_OS_WINDOWS
/** A guest user's guest property root key. */
static const char              *g_pszPropCacheKeyUser              = "/VirtualBox/GuestInfo/User";
#endif
/** The VM session ID. Changes whenever the VM is restored or reset. */
static uint64_t                 g_idVMInfoSession;
/** User idle threshold (in ms). This specifies the minimum time a user is considered
 *  as being idle and then will be reported to the host. Default is 5s. */
uint32_t                        g_cMsVMInfoUserIdleThreshold = 5 * 1000;

#ifdef WITH_VDE_CONNECTION_MONITORING
/** Property written by the host when the active VDE client changes.
 * @note it gets a bit fuzzy with multiple clients, ofc.  */
static const char              *g_pszLAActiveClient                = "/VirtualBox/HostInfo/VRDP/ActiveClient";
/** The last attached locartion awareness (LA) client timestamp. */
static uint64_t                 g_nsLAClientAttachedTS = 0;
/** The current LA client info.
 * @note This is not really used by anyone beyond logging, so pointless.  */
static VBOXSERVICELACLIENTINFO  g_LAClientInfo;
#endif


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/

#ifdef VBOX_WITH_DBUS
/** @name ConsoleKit defines (taken from 0.4.5).
 * @{ */
# define CK_NAME                "org.freedesktop.ConsoleKit"            /* unused */
# define CK_PATH                "/org/freedesktop/ConsoleKit"           /* unused */
# define CK_INTERFACE           "org.freedesktop.ConsoleKit"
# define CK_MANAGER_PATH        "/org/freedesktop/ConsoleKit/Manager"
# define CK_MANAGER_INTERFACE   "org.freedesktop.ConsoleKit.Manager"
# define CK_SEAT_INTERFACE      "org.freedesktop.ConsoleKit.Seat"       /* unused */
# define CK_SESSION_INTERFACE   "org.freedesktop.ConsoleKit.Session"
/** @} */
/** @name systemd-logind defines
 * @{ */
# define SYSTEMD_LOGIN_INTERFACE           "org.freedesktop.login1"
# define SYSTEMD_LOGIN_PATH                "/org/freedesktop/login1"
# define SYSTEMD_LOGIN_MANAGER_INTERFACE   "org.freedesktop.login1.Manager"
# define SYSTEMD_LOGIN_SESSION_INTERFACE   "org.freedesktop.login1.Session"
/** @} */
#endif



/**
 * Signals the service thread so that a refresh of VM-specific information (like
 * logged in users or interface addresses) can happen.
 *
 * @return  IPRT status code.
 */
int VGSvcVMInfoSignal(void)
{
    if (g_hVMInfoEvent != NIL_RTSEMEVENTMULTI)
        return RTSemEventMultiSignal(g_hVMInfoEvent);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnOption}
 */
static DECLCALLBACK(RTEXITCODE) vbsvcVMInfoOption(int iShort, PCRTGETOPTUNION pValueUnion, bool fCmdLine)
{
    switch (iShort)
    {
        case kVGSvcOptVminfoInterval:
            return VGSvcOptUInt32(&g_cMsVMInfoInterval, pValueUnion, 50, UINT32_MAX - 1,  "ms", "interval",
                                  "VM info", fCmdLine);

        case kVGSvcOptVminfoUserIdleThreshold:
            return VGSvcOptUInt32(&g_cMsVMInfoUserIdleThreshold, pValueUnion, 1, UINT32_MAX - 1, "ms", "user idle threshold",
                                  "time sync", fCmdLine);

        default:
            return VGSvcDefaultOption(iShort, pValueUnion, fCmdLine);
    }
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnInit}
 */
static DECLCALLBACK(int) vbsvcVMInfoInit(void)
{
    /*
     * Init global data.
     */
    /* Get the session ID. The status code is ignored as this information is
       not available with VBox < 3.2.10. */
    VbglR3QuerySessionId(&g_idVMInfoSession);

#ifdef WITH_VDE_CONNECTION_MONITORING
    /* Initialize the LA client object (paranoia). */
    RT_ZERO(g_LAClientInfo);
#endif

    /*
     * If not specified, find the right interval default.
     *
     * On Windows (XP++ at least) we'll get notified when interfaces changes
     * and when users log on and off.  So, we don't really need a high frequency
     * here to keep the relevant data up to date.
     */
    if (!g_cMsVMInfoInterval)
        g_cMsVMInfoInterval = g_cSecDefaultInterval * 1000;
#ifdef RT_OS_WINDOWS
    if (!g_cMsVMInfoInterval && RTSystemGetNtVersion() >= RTSYSTEM_MAKE_NT_VERSION(5, 1, 0))
        g_cMsVMInfoInterval = RT_MS_15SEC;
#endif
    if (!g_cMsVMInfoInterval)
        g_cMsVMInfoInterval = RT_MS_10SEC;

    /*
     * Create the event semaphore the thread will be waiting on, then try
     * connect to the guest property service.
     */
    int rc = RTSemEventMultiCreate(&g_hVMInfoEvent);
    AssertRCReturn(rc, rc);

    rc = VbglGuestPropConnect(&g_VMInfoGuestPropSvcClient);
    if (RT_SUCCESS(rc))
    {
        VGSvcVerbose(3, "Property Service Client ID: %#x\n", g_VMInfoGuestPropSvcClient.idClient);

        rc = VGSvcPropCacheInit(&g_VMInfoPropCache, &g_VMInfoGuestPropSvcClient);
        if (RT_SUCCESS(rc))
        {
            /*
             * Declare the beacons.
             *
             * We ignore errors here, though, we probably shouldn't as the only
             * error is running out of memory or process corruption.
             */
            int rc2 = VGSvcPropCacheDeclareEntry(&g_VMInfoPropCache, g_pszPropCacheValLoggedInUsers,
                                                 VGSVCPROPCACHE_FLAGS_TMP_DEL_TRANSRESET | VGSVCPROPCACHE_FLAGS_ALWAYS_UPDATE);
            AssertLogRelRC(rc2);

            rc2 = VGSvcPropCacheDeclareEntry(&g_VMInfoPropCache, g_pszPropCacheValNetCount,
                                             VGSVCPROPCACHE_FLAGS_TMP_DEL_TRANSRESET | VGSVCPROPCACHE_FLAGS_ALWAYS_UPDATE);
            AssertLogRelRC(rc2);

#ifdef RT_OS_WINDOWS
            VGSvcVMInfoWinInit();
#endif
            return VINF_SUCCESS;
        }

        VGSvcError("Failed to init guest property cache: %Rrc\n", rc);
        VbglGuestPropDisconnect(&g_VMInfoGuestPropSvcClient);
    }
    /* If the service was not found, we disable this service without
       causing VBoxService to fail. */
    else if (rc == VERR_HGCM_SERVICE_NOT_FOUND) /* Host service is not available. */
    {
        VGSvcVerbose(0, "Guest property service is not available, disabling the service\n");
        rc = VERR_SERVICE_DISABLED;
    }
    else
        VGSvcError("Failed to connect to the guest property service! Error: %Rrc\n", rc);

    RTSemEventMultiDestroy(g_hVMInfoEvent);
    g_hVMInfoEvent = NIL_RTSEMEVENTMULTI;
    return rc;
}

#ifdef WITH_VDE_CONNECTION_MONITORING

/**
 * Retrieves a specifiy client LA property.
 *
 * @return  IPRT status code.
 * @param   idLAClient              LA client ID to retrieve property for.
 * @param   pszProperty             Property (without path) to retrieve.
 * @param   ppszValue               Where to store value of property.
 * @param   puTimestamp             Timestamp of property to retrieve. Optional.
 */
static int vgsvcGetLAClientValue(uint32_t idLAClient, const char *pszProperty, char **ppszValue, uint64_t *puTimestamp)
{
    AssertReturn(idLAClient, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszProperty, VERR_INVALID_POINTER);

    int  rc;
    char szFullPropNm[GUEST_PROP_MAX_NAME_LEN];
    if (RTStrPrintf2(szFullPropNm, sizeof(szFullPropNm),
                     "/VirtualBox/HostInfo/VRDP/Client/%RU32/%s", idLAClient, pszProperty) > 0)
        rc = VGSvcReadHostProp(&g_VMInfoGuestPropSvcClient, szFullPropNm, true /*fReadOnly*/,
                               ppszValue, NULL /*fFlags*/, puTimestamp);
    else
        rc = VERR_FILENAME_TOO_LONG;
    return rc;
}


/**
 * Retrieves LA client information. On success the returned structure will have allocated
 * objects which need to be free'd with vboxServiceFreeLAClientInfo.
 *
 * @return  IPRT status code.
 * @param   uClientID               Client ID to retrieve information for.
 * @param   pClient                 Pointer where to store the client information.
 */
static int vgsvcGetLAClientInfo(uint32_t uClientID, PVBOXSERVICELACLIENTINFO pClient)
{
    AssertReturn(uClientID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);

    int rc = vgsvcGetLAClientValue(uClientID, "Name", &pClient->pszName,
                                         NULL /* Timestamp */);
    if (RT_SUCCESS(rc))
    {
        char *pszAttach;
        rc = vgsvcGetLAClientValue(uClientID, "Attach", &pszAttach, &pClient->nsAttachedTS);
        if (RT_SUCCESS(rc))
        {
            AssertPtr(pszAttach);
            pClient->fAttached = RTStrICmp(pszAttach, "1") == 0;

            RTStrFree(pszAttach);
        }
    }
    if (RT_SUCCESS(rc))
        rc = vgsvcGetLAClientValue(uClientID, "Location", &pClient->pszLocation, NULL /* Timestamp */);
    if (RT_SUCCESS(rc))
        rc = vgsvcGetLAClientValue(uClientID, "Domain", &pClient->pszDomain, NULL /* Timestamp */);
    if (RT_SUCCESS(rc))
        pClient->uID = uClientID;

    return rc;
}


/**
 * Frees all allocated LA client information of a structure.
 *
 * @param   pClient                 Pointer to client information structure to free.
 */
static void vgsvcFreeLAClientInfo(PVBOXSERVICELACLIENTINFO pClient)
{
    if (pClient)
    {
        if (pClient->pszName)
        {
            RTStrFree(pClient->pszName);
            pClient->pszName = NULL;
        }
        if (pClient->pszLocation)
        {
            RTStrFree(pClient->pszLocation);
            pClient->pszLocation = NULL;
        }
        if (pClient->pszDomain)
        {
            RTStrFree(pClient->pszDomain);
            pClient->pszDomain = NULL;
        }
    }
}


/**
 * Check for location awareness (just logging things, so mostly pointless).
 *
 * This was added in VBox 4.1.
 *
 * @returns Whether we should wait (true) or hurry up and refresh info (false).
 */
static bool vbsvcDoVdeConnectionChangePolling(void)
{
    bool fWait = true;

    /** @todo Use VbglGuestPropWait in a separate thread? Polling is a bit
     *        pointless... */
    /* Check for new connection. */
    char *pszLAClientID = NULL;
    int rc = VGSvcReadHostProp(&g_VMInfoGuestPropSvcClient, g_pszLAActiveClient, true /*fReadOnly*/,
                               &pszLAClientID, NULL /*ppszFlags*/, NULL /*puTimestamp*/);
    if (RT_SUCCESS(rc))
    {
        AssertPtr(pszLAClientID);
        if (strcmp(pszLAClientID, "0") != 0) /* Is a client connected? */
        {
            uint32_t idLAClient = RTStrToInt32(pszLAClientID);

            /* Peek at "Attach" value to figure out if hotdesking happened. */
            char    *pszAttach            = NULL;
            uint64_t nsLAClientAttachedTS = 0;
            rc = vgsvcGetLAClientValue(idLAClient, "Attach", &pszAttach, &nsLAClientAttachedTS);
            if (   RT_SUCCESS(rc)
                && (   !g_nsLAClientAttachedTS
                    || g_nsLAClientAttachedTS != nsLAClientAttachedTS))
            {
                vgsvcFreeLAClientInfo(&g_LAClientInfo);

                /* Note: There is a race between setting the guest properties by the host and getting them by
                 *       the guest. */
                rc = vgsvcGetLAClientInfo(idLAClient, &g_LAClientInfo);
                if (RT_SUCCESS(rc))
                {
                    VGSvcVerbose(1, "VRDP: Hotdesk client %s with ID=%RU32, Name=%s, Domain=%s\n",
                                 /* If g_nsLAClientAttachedTS is 0 this means there already was an active
                                  * hotdesk session when VBoxService started. */
                                 !g_nsLAClientAttachedTS ? "already active"
                                 : g_LAClientInfo.fAttached ? "connected" : "disconnected",
                                 idLAClient, g_LAClientInfo.pszName, g_LAClientInfo.pszDomain);

                    g_nsLAClientAttachedTS = g_LAClientInfo.nsAttachedTS;

                    /* Don't wait for event semaphore below anymore because we now know that the client
                     * changed. This means we need to iterate all VM information again immediately. */
                    fWait = false;
                }
                else
                {
                    static uint64_t s_cBitchedAboutLAClientInfo = 0;
                    if (s_cBitchedAboutLAClientInfo++ < 10)
                        VGSvcError("Error getting active location awareness client info, rc=%Rrc\n", rc);
                }
            }
            else if (RT_FAILURE(rc))
                 VGSvcError("Error getting attached value of location awareness client %RU32, rc=%Rrc\n", idLAClient, rc);
            if (pszAttach)
                RTStrFree(pszAttach);
        }
        else
        {
            VGSvcVerbose(1, "VRDP: UTTSC disconnected from VRDP server\n");
            vgsvcFreeLAClientInfo(&g_LAClientInfo);
        }

        RTStrFree(pszLAClientID);
    }
    else
    {
        static uint64_t s_cBitchedAboutLAClient = 0;
        if (rc != VERR_NOT_FOUND && s_cBitchedAboutLAClient++ < 3)
            VGSvcError("VRDP: Querying connected location awareness client failed with rc=%Rrc\n", rc);
    }

    VGSvcVerbose(3, "VRDP: Handling location awareness done\n");
    return fWait;
}

#endif /* WITH_VDE_CONNECTION_MONITORING */
#ifdef RT_OS_WINDOWS

/**
 * Updates a per-guest user guest property inside the given property cache.
 *
 * @return  VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the final property name length exceeds the maximum supported length.
 * @param   pCache                  Pointer to guest property cache to update user in.
 * @param   pszUser                 Name of guest user to update.
 * @param   pszDomain               Domain of guest user to update. Optional.
 * @param   pszKey                  Key name of guest property to update.
 * @param   pszValue                Guest property value to set. Pass NULL for
 *                                  deleting the property.
 */
int VGSvcVMInfoUpdateUser(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                          const char *pszKey, const char *pszValue)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    Assert(pCache == &g_VMInfoPropCache);
    AssertPtrReturn(pszUser, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszDomain, VERR_INVALID_POINTER);
    AssertPtrReturn(pszKey, VERR_INVALID_POINTER);

    /*
     * We limit guest property names to 64 characters (see GUEST_PROP_MAX_NAME_LEN,
     * including terminator). So, we need to make sure the stuff we want to write
     * as a value fits into that space. See bugref{10575}.
     */
    char szName[GUEST_PROP_MAX_NAME_LEN];
    AssertCompile(GUEST_PROP_MAX_NAME_LEN == 64); /* Can we improve stuff once we (ever) raise this limit? */

    /* Try to write things the legacy way first.
       bird: Confused as to what 'legacy' refers to here, but it's possibly the pszDomain == NULL situation. */
    ssize_t const cchName = pszDomain
                          ? RTStrPrintf2(szName, sizeof(szName), "%s/%s@%s/%s", g_pszPropCacheKeyUser, pszUser, pszDomain, pszKey)
                          : RTStrPrintf2(szName, sizeof(szName), "%s/%s/%s",    g_pszPropCacheKeyUser, pszUser, pszKey);

    /* Did we exceed the length limit? Tell the caller to try again with some more sane values. */
    if (cchName < 0)
        return VERR_BUFFER_OVERFLOW;

    /*
     * Set the property.
     */
    return VGSvcPropCacheUpdate(pCache, szName, pszValue);
}


/**
 * Updates a per-guest user guest property inside the given property cache.
 *
 * @return  VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the final property name length exceeds the maximum supported length.
 * @retval  VERR_INVALID_PARAMETER if the value is too long.
 * @param   pCache                  Pointer to guest property cache to update user in.
 * @param   pszUser                 Name of guest user to update.
 * @param   pszDomain               Domain of guest user to update. Optional.
 * @param   pszKey                  Key name of guest property to update.
 * @param   pszValueFormat          Guest property value (format string) to set.
 *                                  Pass NULL for deleting the property.
 * @param   va                      Format arguments.
 */
int VGSvcVMInfoUpdateUserV(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                           const char *pszKey, const char *pszValueFormat, va_list va)
{
    int rc;
    if (pszValueFormat)
    {
        AssertPtrReturn(pszValueFormat, VERR_INVALID_POINTER);

        char    szValue[GUEST_PROP_MAX_VALUE_LEN];
        ssize_t cchValue = RTStrPrintf2V(szValue, sizeof(szValue), pszValueFormat, va);
        if (cchValue >= 0)
            rc = VGSvcVMInfoUpdateUser(pCache, pszUser, pszDomain, pszKey, szValue);
        else
            rc = VERR_INVALID_PARAMETER;
    }
    else
        rc = VGSvcVMInfoUpdateUser(pCache, pszUser, pszDomain, pszKey, NULL);
    return rc;
}


/**
 * Updates a per-guest user guest property inside the given property cache.
 *
 * @return  VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the final property name length exceeds the maximum supported length.
 * @retval  VERR_INVALID_PARAMETER if the value is too long.
 * @param   pCache                  Pointer to guest property cache to update user in.
 * @param   pszUser                 Name of guest user to update.
 * @param   pszDomain               Domain of guest user to update. Optional.
 * @param   pszKey                  Key name of guest property to update.
 * @param   pszValueFormat          Guest property value (format string) to set.
 *                                  Pass NULL for deleting the property.
 * @param   ...                     Format arguments.
 */
int VGSvcVMInfoUpdateUserF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszUser, const char *pszDomain,
                           const char *pszKey, const char *pszValueFormat, ...)
{
    va_list va;
    va_start(va, pszValueFormat);
    int rc = VGSvcVMInfoUpdateUserV(pCache, pszUser, pszDomain, pszKey, pszValueFormat, va);
    va_end(va);
    return rc;
}

#endif /* RT_OS_WINDOWS */

/**
 * Writes the properties that won't change while the service is running.
 *
 * Errors are ignored.
 */
static void vgsvcVMInfoWriteFixedProperties(void)
{
    /*
     * First get OS information that won't change.
     */
    char szInfo[GUEST_PROP_MAX_VALUE_LEN];
    int rc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szInfo, sizeof(szInfo));
    if (RT_FAILURE(rc) && rc != VERR_BUFFER_OVERFLOW) szInfo[0] = '\0';
    VGSvcWriteProp(&g_VMInfoGuestPropSvcClient, "/VirtualBox/GuestInfo/OS/Product", szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szInfo, sizeof(szInfo));
    if (RT_FAILURE(rc) && rc != VERR_BUFFER_OVERFLOW) szInfo[0] = '\0';
    VGSvcWriteProp(&g_VMInfoGuestPropSvcClient, "/VirtualBox/GuestInfo/OS/Release", szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szInfo, sizeof(szInfo));
    if (RT_FAILURE(rc) && rc != VERR_BUFFER_OVERFLOW) szInfo[0] = '\0';
    VGSvcWriteProp(&g_VMInfoGuestPropSvcClient, "/VirtualBox/GuestInfo/OS/Version", szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szInfo, sizeof(szInfo));
    if (RT_FAILURE(rc) && rc != VERR_BUFFER_OVERFLOW) szInfo[0] = '\0';
    VGSvcWriteProp(&g_VMInfoGuestPropSvcClient, "/VirtualBox/GuestInfo/OS/ServicePack", szInfo);

    /*
     * Retrieve version information about Guest Additions and installed files (components).
     */
    char *pszAddVer;
    char *pszAddVerExt;
    char *pszAddRev;
    rc = VbglR3QueryAdditionsVersion(&pszAddVer, &pszAddVerExt, &pszAddRev);
    VGSvcWriteProp(&g_VMInfoGuestPropSvcClient, "/VirtualBox/GuestAdd/Version",
                   RT_SUCCESS(rc) ? pszAddVer    : VBOX_VERSION_STRING_RAW);
    VGSvcWriteProp(&g_VMInfoGuestPropSvcClient, "/VirtualBox/GuestAdd/VersionExt",
                   RT_SUCCESS(rc) ? pszAddVerExt : VBOX_VERSION_STRING);
    VGSvcWriteProp(&g_VMInfoGuestPropSvcClient, "/VirtualBox/GuestAdd/Revision",
                   RT_SUCCESS(rc) ? pszAddRev    : RT_XSTR(VBOX_SVN_REV));
    if (RT_SUCCESS(rc))
    {
        RTStrFree(pszAddVer);
        RTStrFree(pszAddVerExt);
        RTStrFree(pszAddRev);
    }

#ifdef RT_OS_WINDOWS
    /*
     * Do windows specific properties.
     */
    char *pszInstDir = NULL;
    rc = VbglR3QueryAdditionsInstallationPath(&pszInstDir);
    VGSvcWriteProp(&g_VMInfoGuestPropSvcClient, "/VirtualBox/GuestAdd/InstallDir", RT_SUCCESS(rc) ? pszInstDir : "");
    if (RT_SUCCESS(rc))
        RTStrFree(pszInstDir);

    VGSvcVMInfoWinWriteComponentVersions(&g_VMInfoGuestPropSvcClient);
#endif
}


/**
 * Adds a user to the list we're gathering for setting:
 *  - "/VirtualBox/GuestInfo/OS/LoggedInUsers"
 *  - "/VirtualBox/GuestInfo/OS/LoggedInUsersList"
 *  - "/VirtualBox/GuestInfo/OS/NoLoggedInUsers"
 */
void VGSvcVMInfoAddUserToList(PVBOXSERVICEVMINFOUSERLIST pUserGatherer, const char *pszName,
                              const char *pszSource, bool fCheckUnique)
{
    size_t const cchName = strlen(pszName);

    /*
     * Check if it's already in the list.
     */
    if (fCheckUnique && pUserGatherer->cchUserList > 0)
    {
        const char *pszHit  = strstr(pUserGatherer->szUserList, pszName);
        while (pszHit)
        {
            if (pszHit == pUserGatherer->szUserList || pszHit[-1] == ',')
                if (pszHit[cchName] == '\0' || pszHit[cchName] == ',')
                {
                    VGSvcVerbose(5, "LoggedInUsers: User '%s' from '%s' is already in the list\n", pszName, pszSource);
                    return;
                }
            pszHit = strstr(pszHit + 1, pszName);
        }
    }

    /*
     * Append it to the list, if there is room.
     */
    size_t off = pUserGatherer->cchUserList;
    if (off + 1 + cchName < sizeof(pUserGatherer->szUserList))
    {
        if (off)
            pUserGatherer->szUserList[off++] = ',';
        memcpy(&pUserGatherer->szUserList[off], pszName, cchName);
        off += cchName;
        pUserGatherer->szUserList[off] = '\0';
        pUserGatherer->cchUserList     = (uint32_t)off;
        pUserGatherer->cUsersInList   += 1;
        VGSvcVerbose(4, "LoggedInUsers: Added user '%s' from '%s' to list (size = %zu, count = %u)\n",
                     pszName, pszSource, off, pUserGatherer->cUsersInList);
    }
    else
    {
        pUserGatherer->cOverflowUsers++;
        VGSvcVerbose(5, "LoggedInUsers: Overflow! User '%s' from '%s' does not fit in the list\n", pszName, pszSource);
    }
}


#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2) && !defined(RT_OS_HAIKU) && !defined(RT_OS_FREEBSD)
/**
 * Worker for vgsvcVMInfoWriteUsers that uses the utmpx.h interface to gather
 * users that are logged in.
 */
static void vgsvcVMInfoAddUsersFromUTmpX(PVBOXSERVICEVMINFOUSERLIST pUserGatherer)
{
    setutxent();
    utmpx *ut_user;
    while ((ut_user = getutxent()) != NULL)
    {
# ifdef RT_OS_DARWIN /* No ut_user->ut_session on Darwin */
        VGSvcVerbose(4, "Found entry '%s' (type: %d, PID: %RU32)\n", ut_user->ut_user, ut_user->ut_type, ut_user->ut_pid);
# else
        VGSvcVerbose(4, "Found entry '%s' (type: %d, PID: %RU32, session: %RU32)\n",
                     ut_user->ut_user, ut_user->ut_type, ut_user->ut_pid, ut_user->ut_session);
# endif

        /* Make sure we don't add user names which are not part of type USER_PROCESS. */
        if (ut_user->ut_type == USER_PROCESS) /* Regular user process. */
            VGSvcVMInfoAddUserToList(pUserGatherer, ut_user->ut_user, "utmpx", true /*fCheckUnique*/);
    }
    endutxent(); /* Close utmpx file. */

}
#endif

#if defined(VBOX_WITH_DBUS) && defined(RT_OS_LINUX) /* Not yet for Solaris or FreeBSD. */

/** @name Simple wrappers to work around compiler-specific va_list madness.
 * @{
 */
static dbus_bool_t vboxService_dbus_message_get_args(DBusMessage *message, DBusError *error, int first_arg_type, ...)
{
    va_list va;
    va_start(va, first_arg_type);
    dbus_bool_t ret = dbus_message_get_args_valist(message, error, first_arg_type, va);
    va_end(va);
    return ret;
}

static dbus_bool_t vboxService_dbus_message_append_args(DBusMessage *message, int first_arg_type, ...)
{
    va_list va;
    va_start(va, first_arg_type);
    dbus_bool_t ret = dbus_message_append_args_valist(message, first_arg_type, va);
    va_end(va);
    return ret;
}
/** @} */


/**
 * Wrapper to dig values out of dbus replies, which are contained in
 * a 'variant' and must be iterated into twice.
 *
 * Returns true if it thinks it got a value; false if not.
 *
 * This does various error checking so the caller can skip it:
 *   - whether a DBusError is set
 *   - whether the DBusMessage is valid
 *   - whether we actually got a 'variant'
 *   - whether we got the type the caller's looking for
 */
static bool vboxService_dbus_unpack_variant_reply(DBusError *error, DBusMessage *pReply, char pType, void *pValue)
{
    if (dbus_error_is_set(error))
    {
        VGSvcError("dbus_unpack_variant_reply: dbus returned error '%s'\n", error->message);
        dbus_error_free(error);
    }
    else if (pReply)
    {
        DBusMessageIter iterMsg;
        int             iterType;
        dbus_message_iter_init(pReply, &iterMsg);
        iterType = dbus_message_iter_get_arg_type(&iterMsg);
        if (iterType == DBUS_TYPE_VARIANT)
        {
            DBusMessageIter iterValueMsg;
            int             iterValueType;
            dbus_message_iter_recurse(&iterMsg, &iterValueMsg);
            iterValueType = dbus_message_iter_get_arg_type(&iterValueMsg);
            if (iterValueType == pType)
            {
                dbus_message_iter_get_basic(&iterValueMsg, pValue);
                return true;
            }
        }
    }
    return false;
}

/**
 * Wrapper to NULL out the DBusMessage pointer while discarding it.
 *
 * DBus API is multi-threaded and can have multiple concurrent accessors.
 * Our use here is single-threaded and can never have multiple accessors.
 */
static void vboxService_dbus_message_discard(DBusMessage **ppMsg)
{
    if (ppMsg && *ppMsg)
    {
        /** @todo any clean-ish way to verify DBus internal refcount == 1 here? */
        dbus_message_unref(*ppMsg);
        *ppMsg = NULL;
    }
}


/**
 * Worker for vgsvcVMInfoWriteUsers that adds to the user list from the
 * systemd session manager and ConsoleKit.
 *
 * @todo this is rather horrible stuff and should be split up into 3 separate
 *       functions (connect to dbus, systemd and ConsoleKit).
 */
static void vgsvcVMInfoDBusAddToUserList(PVBOXSERVICEVMINFOUSERLIST pUserGatherer)
{
    DBusError dbErr;
    DBusConnection *pConnection = NULL;
    int rc2 = RTDBusLoadLib();
    bool fHaveLibDbus = false;
    if (RT_SUCCESS(rc2))
    {
        /* Handle desktop sessions using systemd-logind. */
        VGSvcVerbose(4, "Checking systemd-logind sessions ...\n");
        fHaveLibDbus = true;
        dbus_error_init(&dbErr);
        pConnection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbErr);
    }

    if (   pConnection
        && !dbus_error_is_set(&dbErr))
    {
/** @todo is there some Less Horrible Way(tm) to access dbus? */
        /* Get all available sessions. */
        /* like `busctl call org.freedesktop.login1 /org/freedesktop/login1 org.freedesktop.login1.Manager ListSessions` */
        DBusMessage *pMsgSessions = dbus_message_new_method_call(SYSTEMD_LOGIN_INTERFACE,
                                                                 SYSTEMD_LOGIN_PATH,
                                                                 SYSTEMD_LOGIN_MANAGER_INTERFACE,
                                                                 "ListSessions");
        if (   pMsgSessions
            && dbus_message_get_type(pMsgSessions) == DBUS_MESSAGE_TYPE_METHOD_CALL)
        {
            DBusMessage *pReplySessions = dbus_connection_send_with_reply_and_block(pConnection,
                                                                                    pMsgSessions, 30 * 1000 /* 30s timeout */,
                                                                                    &dbErr);
            if (   pReplySessions
                && !dbus_error_is_set(&dbErr))
            {
                /* dbus_message_new_method_call() returns a DBusMessage, which we must iterate to get the returned value */
                DBusMessageIter messageIterMsg;
                int             messageIterType;
                dbus_message_iter_init(pReplySessions, &messageIterMsg);
                while ((messageIterType = dbus_message_iter_get_arg_type (&messageIterMsg)) != DBUS_TYPE_INVALID)
                {
                    if (messageIterType == DBUS_TYPE_ARRAY)
                    {
                        /* "ListSessions" returns an array, which we must iterate to get the array elements */
                        DBusMessageIter arrayIterMsg;
                        int             arrayIterType;
                        dbus_message_iter_recurse(&messageIterMsg, &arrayIterMsg);
                        while ((arrayIterType = dbus_message_iter_get_arg_type (&arrayIterMsg)) != DBUS_TYPE_INVALID)
                        {
                            if (arrayIterType == DBUS_TYPE_STRUCT)
                            {
                                /* The array elements are structs, which we must iterate to get the struct elements */
                                DBusMessageIter structIterMsg;
                                int             structIterType;
                                dbus_message_iter_recurse(&arrayIterMsg, &structIterMsg);
                                while ((structIterType = dbus_message_iter_get_arg_type (&structIterMsg)) != DBUS_TYPE_INVALID)
                                {
                                    if (structIterType == DBUS_TYPE_OBJECT_PATH)
                                    {
                                        /* We are interested only in the "object path" struct element */
                                        const char *objectPath;
                                        dbus_message_iter_get_basic(&structIterMsg, &objectPath);
                                        const char *pInterface = SYSTEMD_LOGIN_SESSION_INTERFACE;
                                        /* Create and send a new dbus query asking for that session's details */
                                        DBusMessage *pMsgSession = dbus_message_new_method_call(SYSTEMD_LOGIN_INTERFACE,
                                                                                                objectPath,
                                                                                                "org.freedesktop.DBus.Properties",
                                                                                                "Get");
                                        if (   pMsgSession
                                            && dbus_message_get_type(pMsgSession) == DBUS_MESSAGE_TYPE_METHOD_CALL)
                                            {
                                            const char *pPropertyActive = "Active";
                                            vboxService_dbus_message_append_args(pMsgSession,
                                                                                 DBUS_TYPE_STRING, &pInterface,
                                                                                 DBUS_TYPE_STRING, &pPropertyActive,
                                                                                 DBUS_TYPE_INVALID, 0);
                                            /* like `busctl get-property org.freedesktop.login1 %s org.freedesktop.login1.Session Active` %(objectPath) */
                                            DBusMessage *pReplySession = dbus_connection_send_with_reply_and_block(
                                                                             pConnection,
                                                                             pMsgSession,
                                                                             -1,
                                                                             &dbErr);
                                            int sessionPropertyActiveValue;
                                            if (   vboxService_dbus_unpack_variant_reply(
                                                       &dbErr,
                                                       pReplySession,
                                                       DBUS_TYPE_BOOLEAN,
                                                       &sessionPropertyActiveValue)
                                                && sessionPropertyActiveValue)
                                                {
                                                DBusMessage *pMsgSession2 = dbus_message_new_method_call(SYSTEMD_LOGIN_INTERFACE,
                                                                                                         objectPath,
                                                                                                         "org.freedesktop.DBus.Properties",
                                                                                                         "Get");
                                                const char *pPropertyName = "Name";
                                                if (   pMsgSession2
                                                    && dbus_message_get_type(pMsgSession2) == DBUS_MESSAGE_TYPE_METHOD_CALL)
                                                    {
                                                    vboxService_dbus_message_append_args(pMsgSession2,
                                                                                         DBUS_TYPE_STRING, &pInterface,
                                                                                         DBUS_TYPE_STRING, &pPropertyName,
                                                                                         DBUS_TYPE_INVALID, 0);
                                                    /* like `busctl get-property org.freedesktop.login1 %s org.freedesktop.login1.Session Name` %(objectPath) */
                                                    DBusMessage *pReplyName = dbus_connection_send_with_reply_and_block(
                                                                                 pConnection,
                                                                                 pMsgSession2,
                                                                                 -1,
                                                                                 &dbErr);
                                                    const char *sessionPropertyNameValue;
                                                    if (   vboxService_dbus_unpack_variant_reply(
                                                               &dbErr,
                                                               pReplyName,
                                                               DBUS_TYPE_STRING,
                                                               &sessionPropertyNameValue)
                                                        && sessionPropertyNameValue)
                                                        VGSvcVMInfoAddUserToList(pUserGatherer, sessionPropertyNameValue,
                                                                                 "systemd-logind", true /*fCheckUnique*/);
                                                    vboxService_dbus_message_discard(&pReplyName);
                                                }
                                                vboxService_dbus_message_discard(&pMsgSession2);
                                            }
                                            vboxService_dbus_message_discard(&pReplySession);
                                        }
                                        vboxService_dbus_message_discard(&pMsgSession);
                                    }
                                    dbus_message_iter_next (&structIterMsg);
                                }
                            }
                            dbus_message_iter_next (&arrayIterMsg);
                        }
                    }
                    dbus_message_iter_next (&messageIterMsg);
                }
                vboxService_dbus_message_discard(&pReplySessions);
            }
        }
        else
        {
            static int s_iBitchedAboutSystemdLogind = 0;
            if (s_iBitchedAboutSystemdLogind < 3)
            {
                s_iBitchedAboutSystemdLogind++;
                VGSvcError("Unable to invoke systemd-logind (%d/3) -- maybe not installed / used? Error: %s\n",
                           s_iBitchedAboutSystemdLogind,
                           dbus_error_is_set(&dbErr) ? dbErr.message : "No error information available");
            }
        }

        vboxService_dbus_message_discard(&pMsgSessions);
        if (dbus_error_is_set(&dbErr))
            dbus_error_free(&dbErr);
    }
    if (RT_SUCCESS(rc2)) /* rc2 is from RTDBusLoadLib() way above, sigh. */
    {
        /* Handle desktop sessions using ConsoleKit. */
        VGSvcVerbose(4, "Checking ConsoleKit sessions ...\n");
        fHaveLibDbus = true;
        dbus_error_init(&dbErr);
        /** @todo should this be dbus_connection_open() (and below, dbus_connection_unref())? */
        pConnection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbErr);
    }

    if (   pConnection
        && !dbus_error_is_set(&dbErr))
    {
        /* Get all available sessions. */
        DBusMessage *pMsgSessions = dbus_message_new_method_call(CK_INTERFACE,
                                                                 CK_MANAGER_PATH,
                                                                 CK_MANAGER_INTERFACE,
                                                                 "GetSessions");
        if (   pMsgSessions
            && dbus_message_get_type(pMsgSessions) == DBUS_MESSAGE_TYPE_METHOD_CALL)
        {
            DBusMessage *pReplySessions = dbus_connection_send_with_reply_and_block(pConnection,
                                                                                    pMsgSessions, 30 * 1000 /* 30s timeout */,
                                                                                    &dbErr);
            if (   pReplySessions
                && !dbus_error_is_set(&dbErr))
            {
                char **ppszSessions;
                int cSessions;
                if (   dbus_message_get_type(pMsgSessions) == DBUS_MESSAGE_TYPE_METHOD_CALL
                    && vboxService_dbus_message_get_args(pReplySessions, &dbErr, DBUS_TYPE_ARRAY,
                                                         DBUS_TYPE_OBJECT_PATH, &ppszSessions, &cSessions,
                                                         DBUS_TYPE_INVALID /* Termination */))
                {
                    VGSvcVerbose(4, "ConsoleKit: retrieved %RU16 session(s)\n", cSessions);

                    char **ppszCurSession = ppszSessions;
                    for (ppszCurSession; ppszCurSession && *ppszCurSession; ppszCurSession++)
                    {
                        VGSvcVerbose(4, "ConsoleKit: processing session '%s' ...\n", *ppszCurSession);

                        /* Only respect active sessions .*/
                        bool fActive = false;
                        DBusMessage *pMsgSessionActive = dbus_message_new_method_call(CK_INTERFACE,
                                                                                      *ppszCurSession,
                                                                                      CK_SESSION_INTERFACE,
                                                                                      "IsActive");
                        if (   pMsgSessionActive
                            && dbus_message_get_type(pMsgSessionActive) == DBUS_MESSAGE_TYPE_METHOD_CALL)
                        {
                            DBusMessage *pReplySessionActive = dbus_connection_send_with_reply_and_block(pConnection,
                                                                                                         pMsgSessionActive,
                                                                                                         30 * 1000 /*sec*/,
                                                                                                         &dbErr);
                            if (   pReplySessionActive
                                && !dbus_error_is_set(&dbErr))
                            {
                                DBusMessageIter itMsg;
                                if (   dbus_message_iter_init(pReplySessionActive, &itMsg)
                                    && dbus_message_iter_get_arg_type(&itMsg) == DBUS_TYPE_BOOLEAN)
                                {
                                    /* Get uid from message. */
                                    int val;
                                    dbus_message_iter_get_basic(&itMsg, &val);
                                    fActive = val >= 1;
                                }

                            }
                            /** @todo clean up if &dbErr */
                            vboxService_dbus_message_discard(&pReplySessionActive);

                            vboxService_dbus_message_discard(&pMsgSessionActive);
                        }

                        VGSvcVerbose(4, "ConsoleKit: session '%s' is %s\n",
                                           *ppszCurSession, fActive ? "active" : "not active");

                        /* *ppszCurSession now contains the object path
                         * (e.g. "/org/freedesktop/ConsoleKit/Session1"). */
                        DBusMessage *pMsgUnixUser = dbus_message_new_method_call(CK_INTERFACE,
                                                                                 *ppszCurSession,
                                                                                 CK_SESSION_INTERFACE,
                                                                                 "GetUnixUser");
                        if (   fActive
                            && pMsgUnixUser
                            && dbus_message_get_type(pMsgUnixUser) == DBUS_MESSAGE_TYPE_METHOD_CALL)
                        {
                            DBusMessage *pReplyUnixUser = dbus_connection_send_with_reply_and_block(pConnection,
                                                                                                    pMsgUnixUser,
                                                                                                    30 * 1000 /* 30s timeout */,
                                                                                                    &dbErr);
                            if (   pReplyUnixUser
                                && !dbus_error_is_set(&dbErr))
                            {
                                DBusMessageIter itMsg;
                                if (   dbus_message_iter_init(pReplyUnixUser, &itMsg)
                                    && dbus_message_iter_get_arg_type(&itMsg) == DBUS_TYPE_UINT32)
                                {
                                    /* Get uid from message. */
                                    uint32_t uid;
                                    dbus_message_iter_get_basic(&itMsg, &uid);

                                    /* Look up user name (realname) from uid. */
                                    setpwent();
                                    struct passwd *ppwEntry = getpwuid(uid);
                                    if (   ppwEntry
                                        && ppwEntry->pw_name)
                                    {
                                            VGSvcVerbose(4, "ConsoleKit: session '%s' -> %s (uid: %RU32)\n",
                                                         *ppszCurSession, ppwEntry->pw_name, uid);
                                            VGSvcVMInfoAddUserToList(pUserGatherer, ppwEntry->pw_name,
                                                                     "ConsoleKit", true /*fCheckUnique*/);
                                    }
                                    else
                                        VGSvcError("ConsoleKit: unable to lookup user name for uid=%RU32\n", uid);
                                }
                                else
                                    AssertMsgFailed(("ConsoleKit: GetUnixUser returned a wrong argument type\n"));
                            }
                            /** @todo clean up if &dbErr */

                            vboxService_dbus_message_discard(&pReplyUnixUser);
                        }
                        else if (fActive) /* don't bitch about inactive users */
                        {
                            static int s_iBitchedAboutConsoleKit = 0;
                            if (s_iBitchedAboutConsoleKit < 1)
                            {
                                s_iBitchedAboutConsoleKit++;
                                VGSvcError("ConsoleKit: unable to retrieve user for session '%s' (msg type=%d): %s\n",
                                           *ppszCurSession, dbus_message_get_type(pMsgUnixUser),
                                           dbus_error_is_set(&dbErr) ? dbErr.message : "No error information available");
                            }
                        }

                        vboxService_dbus_message_discard(&pMsgUnixUser);
                    }

                    dbus_free_string_array(ppszSessions);
                }
                else
                    VGSvcError("ConsoleKit: unable to retrieve session parameters (msg type=%d): %s\n",
                               dbus_message_get_type(pMsgSessions),
                               dbus_error_is_set(&dbErr) ? dbErr.message : "No error information available");
                vboxService_dbus_message_discard(&pReplySessions);
            }
        }
        else
        {
            static int s_iBitchedAboutConsoleKit = 0;
            if (s_iBitchedAboutConsoleKit < 3)
            {
                s_iBitchedAboutConsoleKit++;
                VGSvcError("Unable to invoke ConsoleKit (%d/3) -- maybe not installed / used? Error: %s\n",
                           s_iBitchedAboutConsoleKit,
                           dbus_error_is_set(&dbErr) ? dbErr.message : "No error information available");
            }
        }

        vboxService_dbus_message_discard(&pMsgSessions);
    }
    else
    {
        static int s_iBitchedAboutDBus = 0;
        if (s_iBitchedAboutDBus < 3)
        {
            s_iBitchedAboutDBus++;
            VGSvcError("Unable to connect to system D-Bus (%d/3): %s\n", s_iBitchedAboutDBus,
                       fHaveLibDbus && dbus_error_is_set(&dbErr) ? dbErr.message : "D-Bus not installed");
        }
    }

    if (   fHaveLibDbus
        && dbus_error_is_set(&dbErr))
        dbus_error_free(&dbErr);
}

#endif /* VBOX_WITH_DBUS && RT_OS_LINUX */


/**
 * Provide information about active users.
 */
static int vgsvcVMInfoWriteUsers(void)
{
#ifdef RT_OS_WINDOWS
    /*
     * Mark the properties under "/VirtualBox/GuestInfo/User" as potentially stale, so
     * the ones we don't update will get deleted (by the main loop).
     */
    VGSvcPropCacheMarkNotUpdatedByPath(&g_VMInfoPropCache, g_pszPropCacheKeyUser);
#endif

    /*
     * Initialize the user gatherer structure.
     */
    VBOXSERVICEVMINFOUSERLIST UserGatherer;
    UserGatherer.cUsersInList   = 0;
    UserGatherer.cchUserList    = 0;
    UserGatherer.cOverflowUsers = 0;
    UserGatherer.szUserList[0]  = '\0';

    /*
     * Get the number of logged in users and their names (comma separated list).
     */
#ifdef RT_OS_WINDOWS
    /* We're passing &g_VMInfoPropCache to this function, however, it's only
       ever used to call back into VGSvcVMInfoUpdateUserF and VGSvcVMInfoUpdateUserV (which
       doesn't technically need them). */
    int rc = VGSvcVMInfoWinQueryUserListAndUpdateInfo(&UserGatherer, &g_VMInfoPropCache);

#elif defined(RT_OS_FREEBSD)
    /** @todo FreeBSD: Port logged on user info retrieval.
     *                 However, FreeBSD 9 supports utmpx, so we could use the code
     *                 block below (?). */
    int rc = VERR_NOT_IMPLEMENTED;

#elif defined(RT_OS_HAIKU)
    /** @todo Haiku: Port logged on user info retrieval. */
    int rc = VERR_NOT_IMPLEMENTED;

#elif defined(RT_OS_OS2)
    /** @todo OS/2: Port logged on (LAN/local/whatever) user info retrieval. */
    int rc = VERR_NOT_IMPLEMENTED;

#else
    int rc = VINF_SUCCESS;

    /* Gather using setutxent & getutxent to source the utmp file. */
    vgsvcVMInfoAddUsersFromUTmpX(&UserGatherer);

# if defined(VBOX_WITH_DBUS) && defined(RT_OS_LINUX) /* Not yet for Solaris/FreeBSD. */
    /* Gather using various DBus interface. */
    vgsvcVMInfoDBusAddToUserList(&UserGatherer);
# endif

#endif /* !RT_OS_WINDOWS && !RT_OS_FREEBSD && !RT_OS_HAIKU && !RT_OS_OS2 */

    if (RT_SUCCESS(rc))
        g_cVMInfoLoggedInUsers = UserGatherer.cUsersInList;
    /* If the enumeration code ran out of memory, preserve the count to try avoid
       confusing 3rd party tools which uses the count to take pause/save/whatever
       action on the VM. */
    else if (rc == VERR_NO_MEMORY || rc == VERR_NO_TMP_MEMORY || rc == VERR_NO_STR_MEMORY || rc == VERR_NO_PAGE_MEMORY)
    {
        static int s_iVMInfoBitchedOOM = 0;
        if (s_iVMInfoBitchedOOM++ < 3)
            VGSvcVerbose(0, "Warning: Not enough memory available to enumerate users! Keeping old value (%RU32)\n",
                         g_cVMInfoLoggedInUsers);
        UserGatherer.cUsersInList = g_cVMInfoLoggedInUsers;
    }
    /* else: For all other errors, just display what we've got... */

    VGSvcVerbose(4, "rc=%Rrc: cUsersInList=%RU32 szUserList=%s\n", rc, UserGatherer.cUsersInList, UserGatherer.szUserList);
    Assert(RT_FAILURE(rc) || UserGatherer.cUsersInList == 0 || UserGatherer.szUserList[0]);
    AssertMsg(!UserGatherer.szUserList[0] || UserGatherer.cUsersInList,
              ("szUserList contains users whereas cUsersInList is 0: %s\n", UserGatherer.szUserList));

    /*
     * Update the properties.
     */
    rc = VGSvcPropCacheUpdate(&g_VMInfoPropCache, g_pszPropCacheValLoggedInUsersList, UserGatherer.szUserList);
    if (RT_FAILURE(rc))
        VGSvcError("Error writing logged in users list, rc=%Rrc\n", rc);

    rc = VGSvcPropCacheUpdate(&g_VMInfoPropCache, g_pszPropCacheValNoLoggedInUsers,
                              UserGatherer.cUsersInList == 0 ? "true" : "false");
    if (RT_FAILURE(rc))
        VGSvcError("Error writing no logged in users, rc=%Rrc\n", rc);

    /* (This is the operation which return code counts and must be returned.) */
    rc = VGSvcPropCacheUpdateF(&g_VMInfoPropCache, g_pszPropCacheValLoggedInUsers, "%RU32", UserGatherer.cUsersInList);
    if (RT_FAILURE(rc))
        VGSvcError("Error writing logged in users count (beacon), rc=%Rrc\n", rc);

    VGSvcVerbose(4, "Writing users returned with rc=%Rrc\n", rc);
    return rc;
}


/**
 * Provide information about the guest network.
 */
static int vgsvcVMInfoWriteNetwork(void)
{
    uint32_t    cIfsReported = 0;
    char        szPropPath[GUEST_PROP_MAX_VALUE_LEN];

    /*
     * Mark the properties under "/VirtualBox/GuestInfo/Net/" as potentially stale, so
     * if an interface is removed, we'll delete any items related to it.
     */
    VGSvcPropCacheMarkNotUpdatedByPath(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/Net/");

#ifdef RT_OS_WINDOWS
    /*
     * Check that the APIs we need are present.
     */
    if (   !g_pfnWSAIoctl
        || !g_pfnWSASocketA
        || !g_pfnWSAGetLastError
        || !g_pfninet_ntoa
        || !g_pfnclosesocket)
        return VINF_SUCCESS;

    /*
     * Query the IP adapter info first, if we have the API.
     */
    IP_ADAPTER_INFO *pAdpInfo  = NULL;
    if (g_pfnGetAdaptersInfo)
    {
        ULONG cbAdpInfo = RT_MAX(sizeof(IP_ADAPTER_INFO) * 2, _2K);
        pAdpInfo  = (IP_ADAPTER_INFO *)RTMemAllocZ(cbAdpInfo);
        if (!pAdpInfo)
        {
            VGSvcError("VMInfo/Network: Failed to allocate two IP_ADAPTER_INFO structures\n");
            return VERR_NO_MEMORY;
        }

        DWORD dwRet = g_pfnGetAdaptersInfo(pAdpInfo, &cbAdpInfo);
        if (dwRet == ERROR_BUFFER_OVERFLOW)
        {
            IP_ADAPTER_INFO *pAdpInfoNew = (IP_ADAPTER_INFO*)RTMemRealloc(pAdpInfo, cbAdpInfo);
            if (pAdpInfoNew)
            {
                pAdpInfo = pAdpInfoNew;
                RT_BZERO(pAdpInfo, cbAdpInfo);
                dwRet = g_pfnGetAdaptersInfo(pAdpInfo, &cbAdpInfo);
            }
        }
        if (dwRet != NO_ERROR)
        {
            RTMemFree(pAdpInfo);
            pAdpInfo  = NULL;
            if (dwRet == ERROR_NO_DATA)
                /* If no network adapters available / present in the
                   system we pretend success to not bail out too early. */
                VGSvcVerbose(3, "VMInfo/Network: No network adapters present according to GetAdaptersInfo.\n");
            else
            {
                VGSvcError("VMInfo/Network: Failed to get adapter info: Error %d\n", dwRet);
                return RTErrConvertFromWin32(dwRet);
            }
        }
    }

    /*
     * Ask the TCP/IP stack for an interface list.
     */
    SOCKET sd = g_pfnWSASocketA(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
    if (sd == SOCKET_ERROR) /* Socket invalid. */
    {
        int const wsaErr = g_pfnWSAGetLastError();
        RTMemFree(pAdpInfo);

        /* Don't complain/bail out with an error if network stack is not up; can happen
         * on NT4 due to start up when not connected shares dialogs pop up. */
        if (wsaErr == WSAENETDOWN)
        {
            VGSvcVerbose(0, "VMInfo/Network: Network is not up yet.\n");
            return VINF_SUCCESS;
        }
        VGSvcError("VMInfo/Network: Failed to get a socket: Error %d\n", wsaErr);
        return RTErrConvertFromWin32(wsaErr);
    }

    INTERFACE_INFO  aInterfaces[20] = {{0}};
    DWORD           cbReturned      = 0;
# ifdef RT_ARCH_X86
    /* Workaround for uninitialized variable used in memcpy in GetTcpipInterfaceList
       (NT4SP1 at least).  It seems to be happy enough with garbages, no failure
       returns so far, so we just need to prevent it from crashing by filling the
       stack with valid pointer values prior to the API call. */
    _asm
    {
        mov     edx, edi
        lea     eax, aInterfaces
        mov     [esp - 0x1000], eax
        mov     [esp - 0x2000], eax
        mov     ecx, 0x2000/4 - 1
        cld
        lea     edi, [esp - 0x2000]
        rep stosd
        mov     edi, edx
    }
# endif
    int rc = g_pfnWSAIoctl(sd,
                           SIO_GET_INTERFACE_LIST,
                           NULL,                /* pvInBuffer */
                           0,                   /* cbInBuffer */
                           &aInterfaces[0],     /* pvOutBuffer */
                           sizeof(aInterfaces), /* cbOutBuffer */
                           &cbReturned,
                           NULL,                /* pOverlapped */
                           NULL);               /* pCompletionRoutine */
    if (rc == SOCKET_ERROR)
    {
        VGSvcError("VMInfo/Network: Failed to WSAIoctl() on socket: Error: %d\n", g_pfnWSAGetLastError());
        RTMemFree(pAdpInfo);
        g_pfnclosesocket(sd);
        return RTErrConvertFromWin32(g_pfnWSAGetLastError());
    }
    g_pfnclosesocket(sd);
    int cIfacesSystem = cbReturned / sizeof(INTERFACE_INFO);

    /*
     * Iterate the inteface list we got back from the TCP/IP,
     * using the pAdpInfo list to supply the MAC address.
     */
    /** @todo Use GetAdaptersInfo() and GetAdapterAddresses (IPv4 + IPv6) for more information. */
    for (int i = 0; i < cIfacesSystem; ++i)
    {
        if (aInterfaces[i].iiFlags & IFF_LOOPBACK) /* Skip loopback device. */
            continue;
        sockaddr_in *pAddress = &aInterfaces[i].iiAddress.AddressIn;
        char szIp[32];
        RTStrPrintf(szIp, sizeof(szIp), "%s", g_pfninet_ntoa(pAddress->sin_addr)); /* (Keep a copy for the MAC lookup below.) */
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/V4/IP", cIfsReported);
        VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, szIp);

        pAddress = &aInterfaces[i].iiBroadcastAddress.AddressIn;
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/V4/Broadcast", cIfsReported);
        VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, g_pfninet_ntoa(pAddress->sin_addr));

        pAddress = (sockaddr_in *)&aInterfaces[i].iiNetmask;
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/V4/Netmask", cIfsReported);
        VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, g_pfninet_ntoa(pAddress->sin_addr));

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/Status", cIfsReported);
        VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, aInterfaces[i].iiFlags & IFF_UP ? "Up" : "Down");

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/MAC", cIfsReported);
        IP_ADAPTER_INFO *pAdp;
        for (pAdp = pAdpInfo; pAdp; pAdp = pAdp->Next)
            if (!strcmp(pAdp->IpAddressList.IpAddress.String, szIp))
                break;
        if (pAdp)
            VGSvcPropCacheUpdateF(&g_VMInfoPropCache, szPropPath, "%02X%02X%02X%02X%02X%02X",
                                  pAdp->Address[0], pAdp->Address[1], pAdp->Address[2],
                                  pAdp->Address[3], pAdp->Address[4], pAdp->Address[5]);
        else
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, NULL);

        cIfsReported++;
    }

    RTMemFree(pAdpInfo);

#elif defined(RT_OS_HAIKU)
    /** @todo Haiku: implement network info. retreival */
    return VERR_NOT_IMPLEMENTED;

#elif defined(RT_OS_DARWIN) || defined(RT_OS_FREEBSD) || defined(RT_OS_NETBSD)
    struct ifaddrs *pIfHead = NULL;

    /* Get all available interfaces */
    int rc = getifaddrs(&pIfHead);
    if (rc < 0)
    {
        rc = RTErrConvertFromErrno(errno);
        VGSvcError("VMInfo/Network: Failed to get all interfaces: Error %Rrc\n");
        return rc;
    }

    /* Loop through all interfaces and set the data. */
    for (struct ifaddrs *pIfCurr = pIfHead; pIfCurr; pIfCurr = pIfCurr->ifa_next)
    {
        /*
         * Only AF_INET and no loopback interfaces
         */
        /** @todo IPv6 interfaces */
        if (   pIfCurr->ifa_addr->sa_family == AF_INET
            && !(pIfCurr->ifa_flags & IFF_LOOPBACK))
        {
            char szInetAddr[NI_MAXHOST];

            memset(szInetAddr, 0, NI_MAXHOST);
            getnameinfo(pIfCurr->ifa_addr, sizeof(struct sockaddr_in),
                        szInetAddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/V4/IP", cIfsReported);
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, szInetAddr);

            memset(szInetAddr, 0, NI_MAXHOST);
            getnameinfo(pIfCurr->ifa_broadaddr, sizeof(struct sockaddr_in),
                        szInetAddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/V4/Broadcast", cIfsReported);
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, szInetAddr);

            memset(szInetAddr, 0, NI_MAXHOST);
            getnameinfo(pIfCurr->ifa_netmask, sizeof(struct sockaddr_in),
                        szInetAddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/V4/Netmask", cIfsReported);
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, szInetAddr);

            /* Search for the AF_LINK interface of the current AF_INET one and get the mac. */
            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/MAC", cIfsReported);
            uint8_t const *pu8Mac = NULL;
            for (struct ifaddrs *pIfLinkCurr = pIfHead; pIfLinkCurr; pIfLinkCurr = pIfLinkCurr->ifa_next)
                if (   pIfLinkCurr->ifa_addr->sa_family == AF_LINK
                    && !strcmp(pIfCurr->ifa_name, pIfLinkCurr->ifa_name))
                {
                    struct sockaddr_dl *pLinkAddress = (struct sockaddr_dl *)pIfLinkCurr->ifa_addr;
                    AssertPtr(pLinkAddress);
                    pu8Mac = (uint8_t const *)LLADDR(pLinkAddress);
                    AssertPtr(pu8Mac);
                    break;
                }
            if (pu8Mac)
                VGSvcPropCacheUpdateF(&g_VMInfoPropCache, szPropPath, "%02X%02X%02X%02X%02X%02X",
                                      pu8Mac[0], pu8Mac[1], pu8Mac[2], pu8Mac[3],  pu8Mac[4], pu8Mac[5]);
            else
                VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, NULL);

            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/Status", cIfsReported);
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, pIfCurr->ifa_flags & IFF_UP ? "Up" : "Down");

# ifdef RT_OS_FREEBSD /** @todo Check the other guests. */
            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32/Name", cIfsReported);
            int rc2 = RTStrValidateEncoding(pIfCurr->ifa_name);
            if (RT_SUCCESS(rc2))
                VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", pIfCurr->ifa_name);
            else
                VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, NULL);
# endif

            cIfsReported++;
        }
    }

    /* Free allocated resources. */
    freeifaddrs(pIfHead);

#else /* !RT_OS_WINDOWS && !RT_OS_FREEBSD */
    /*
     * Use SIOCGIFCONF to get a list of interface/protocol configurations.
     *
     * See "UNIX Network Programming Volume 1" by W. R. Stevens, section 17.6
     * for details on this ioctl.
     */
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        int rc = RTErrConvertFromErrno(errno);
        VGSvcError("VMInfo/Network: Failed to get a socket: Error %Rrc\n", rc);
        return rc;
    }

    /* Call SIOCGIFCONF with the right sized buffer (remember the size). */
    static int      s_cbBuf = 256; // 1024
    int             cbBuf   = s_cbBuf;
    char           *pchBuf;
    struct ifconf   IfConf;
    int rc = VINF_SUCCESS;
    for (;;)
    {
        pchBuf = (char *)RTMemTmpAllocZ(cbBuf);
        if (!pchBuf)
        {
            rc = VERR_NO_TMP_MEMORY;
            break;
        }

        IfConf.ifc_len = cbBuf;
        IfConf.ifc_buf = pchBuf;
        if (ioctl(sd, SIOCGIFCONF, &IfConf) >= 0)
        {
            /* Hard to anticipate how space an address might possibly take, so
               making some generous assumptions here to avoid performing the
               query twice with different buffer sizes. */
            if (IfConf.ifc_len + 128 < cbBuf)
                break;
        }
        else if (errno != EOVERFLOW)
        {
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        /* grow the buffer */
        s_cbBuf = cbBuf *= 2;
        RTMemFree(pchBuf);
    }
    if (RT_FAILURE(rc))
    {
        close(sd);
        RTMemTmpFree(pchBuf);
        VGSvcError("VMInfo/Network: Error doing SIOCGIFCONF (cbBuf=%d): %Rrc\n", cbBuf, rc);
        return rc;
    }

    /*
     * Iterate the interface/protocol configurations.
     *
     * Note! The current code naively assumes one IPv4 address per interface.
     *       This means that guest assigning more than one address to an
     *       interface will get multiple entries for one physical interface.
     */
# ifdef RT_OS_OS2
    struct ifreq   *pPrevLinkAddr = NULL;
# endif
    struct ifreq   *pCur   = IfConf.ifc_req;
    size_t          cbLeft = IfConf.ifc_len;
    while (cbLeft >= sizeof(*pCur))
    {
# if defined(RT_OS_SOLARIS) || defined(RT_OS_LINUX)
        /* These two do not provide the sa_len member but only support address
         * families which do not need extra bytes on the end. */
#  define SA_LEN(pAddr) sizeof(struct sockaddr)
# elif !defined(SA_LEN)
#  define SA_LEN(pAddr) (pAddr)->sa_len
# endif
        /* Figure the size of the current request. */
        size_t cbCur = RT_UOFFSETOF(struct ifreq, ifr_addr)
                     + SA_LEN(&pCur->ifr_addr);
        cbCur = RT_MAX(cbCur, sizeof(struct ifreq));
# if defined(RT_OS_SOLARIS) || defined(RT_OS_LINUX)
        Assert(pCur->ifr_addr.sa_family == AF_INET);
# endif
        AssertBreak(cbCur <= cbLeft);

# ifdef RT_OS_OS2
        /* On OS/2 we get the MAC address in the AF_LINK that the BSD 4.4 stack
           emits.  We boldly ASSUME these always comes first. */
        if (   pCur->ifr_addr.sa_family == AF_LINK
            && ((struct sockaddr_dl *)&pCur->ifr_addr)->sdl_alen == 6)
            pPrevLinkAddr = pCur;
# endif

        /* Skip it if it's not the kind of address we're looking for. */
        struct ifreq IfReqTmp;
        bool         fIfUp = false;
        bool         fSkip = false;
        if (pCur->ifr_addr.sa_family != AF_INET)
            fSkip = true;
        else
        {
            /* Get the interface flags so we can detect loopback and check if it's up. */
            IfReqTmp = *pCur;
            if (ioctl(sd, SIOCGIFFLAGS, &IfReqTmp) < 0)
            {
                rc = RTErrConvertFromErrno(errno);
                VGSvcError("VMInfo/Network: Failed to ioctl(SIOCGIFFLAGS,%s) on socket: Error %Rrc\n", pCur->ifr_name, rc);
                break;
            }
            fIfUp = !!(IfReqTmp.ifr_flags & IFF_UP);
            if (IfReqTmp.ifr_flags & IFF_LOOPBACK) /* Skip the loopback device. */
                fSkip = true;
        }
        if (!fSkip)
        {
            size_t offSubProp = RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%RU32", cIfsReported);

            sockaddr_in *pAddress = (sockaddr_in *)&pCur->ifr_addr;
            strcpy(&szPropPath[offSubProp], "/V4/IP");
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, inet_ntoa(pAddress->sin_addr));

            /* Get the broadcast address. */
            IfReqTmp = *pCur;
            if (ioctl(sd, SIOCGIFBRDADDR, &IfReqTmp) < 0)
            {
                rc = RTErrConvertFromErrno(errno);
                VGSvcError("VMInfo/Network: Failed to ioctl(SIOCGIFBRDADDR) on socket: Error %Rrc\n", rc);
                break;
            }
            pAddress = (sockaddr_in *)&IfReqTmp.ifr_broadaddr;
            strcpy(&szPropPath[offSubProp], "/V4/Broadcast");
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, inet_ntoa(pAddress->sin_addr));

            /* Get the net mask. */
            IfReqTmp = *pCur;
            if (ioctl(sd, SIOCGIFNETMASK, &IfReqTmp) < 0)
            {
                rc = RTErrConvertFromErrno(errno);
                VGSvcError("VMInfo/Network: Failed to ioctl(SIOCGIFNETMASK) on socket: Error %Rrc\n", rc);
                break;
            }
# if defined(RT_OS_OS2) || defined(RT_OS_SOLARIS)
            pAddress = (sockaddr_in *)&IfReqTmp.ifr_addr;
# else
            pAddress = (sockaddr_in *)&IfReqTmp.ifr_netmask;
# endif
            strcpy(&szPropPath[offSubProp], "/V4/Netmask");
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, inet_ntoa(pAddress->sin_addr));

# if defined(RT_OS_SOLARIS)
            /*
             * "ifreq" is obsolete on Solaris. We use the recommended "lifreq".
             * We might fail if the interface has not been assigned an IP address.
             * That doesn't matter; as long as it's plumbed we can pick it up.
             * But, if it has not acquired an IP address we cannot obtain it's MAC
             * address this way, so we just use all zeros there.
             */
            RTMAC           IfMac;
            struct lifreq   IfReq;
            RT_ZERO(IfReq);
            AssertCompile(sizeof(IfReq.lifr_name) >= sizeof(pCur->ifr_name));
            strncpy(IfReq.lifr_name, pCur->ifr_name, sizeof(IfReq.lifr_name));
            if (ioctl(sd, SIOCGLIFADDR, &IfReq) >= 0)
            {
                struct arpreq ArpReq;
                RT_ZERO(ArpReq);
                memcpy(&ArpReq.arp_pa, &IfReq.lifr_addr, sizeof(struct sockaddr_in));

                if (ioctl(sd, SIOCGARP, &ArpReq) >= 0)
                    memcpy(&IfMac, ArpReq.arp_ha.sa_data, sizeof(IfMac));
                else
                {
                    rc = RTErrConvertFromErrno(errno);
                    VGSvcError("VMInfo/Network: failed to ioctl(SIOCGARP) on socket: Error %Rrc\n", rc);
                    break;
                }
            }
            else
            {
                VGSvcVerbose(2, "VMInfo/Network: Interface '%s' has no assigned IP address, skipping ...\n", pCur->ifr_name);
                continue;
            }

# elif defined(RT_OS_OS2)
            RTMAC   IfMac;
            if (   pPrevLinkAddr
                && strncmp(pCur->ifr_name, pPrevLinkAddr->ifr_name, sizeof(pCur->ifr_name)) == 0)
            {
                struct sockaddr_dl *pDlAddr = (struct sockaddr_dl *)&pPrevLinkAddr->ifr_addr;
                IfMac = *(PRTMAC)&pDlAddr->sdl_data[pDlAddr->sdl_nlen];
            }
            else
                RT_ZERO(IfMac);

# else
            if (ioctl(sd, SIOCGIFHWADDR, &IfReqTmp) < 0)
            {
                rc = RTErrConvertFromErrno(errno);
                VGSvcError("VMInfo/Network: Failed to ioctl(SIOCGIFHWADDR) on socket: Error %Rrc\n", rc);
                break;
            }
            RTMAC IfMac = *(PRTMAC)&IfReqTmp.ifr_hwaddr.sa_data[0];
# endif

            strcpy(&szPropPath[offSubProp], "/MAC");
            VGSvcPropCacheUpdateF(&g_VMInfoPropCache, szPropPath, "%02X%02X%02X%02X%02X%02X",
                                  IfMac.au8[0], IfMac.au8[1], IfMac.au8[2], IfMac.au8[3], IfMac.au8[4], IfMac.au8[5]);

            strcpy(&szPropPath[offSubProp], "/Status");
            VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, fIfUp ? "Up" : "Down");

            /* The name. */
            strcpy(&szPropPath[offSubProp], "/Name");
            int rc2 = RTStrValidateEncodingEx(pCur->ifr_name, sizeof(pCur->ifr_name), 0);
            if (RT_SUCCESS(rc2))
                VGSvcPropCacheUpdateF(&g_VMInfoPropCache, szPropPath, "%.*s", sizeof(pCur->ifr_name), pCur->ifr_name);
            else
                VGSvcPropCacheUpdate(&g_VMInfoPropCache, szPropPath, NULL);

            cIfsReported++;
        }

        /*
         * Next interface/protocol configuration.
         */
        pCur = (struct ifreq *)((uintptr_t)pCur + cbCur);
        cbLeft -= cbCur;
    }

    RTMemTmpFree(pchBuf);
    close(sd);
    if (RT_FAILURE(rc))
        VGSvcError("VMInfo/Network: Network enumeration for interface %RU32 failed with error %Rrc\n", cIfsReported, rc);

#endif /* !RT_OS_WINDOWS */

    /*
     * This property is a beacon which is _always_ written, even if the network configuration
     * does not change. If this property is missing, the host assumes that all other GuestInfo
     * properties are no longer valid.
     */
    VGSvcPropCacheUpdateF(&g_VMInfoPropCache, g_pszPropCacheValNetCount, "%RU32", cIfsReported);

    /* Don't fail here; just report everything we got. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnWorker}
 */
static DECLCALLBACK(int) vbsvcVMInfoWorker(bool volatile *pfShutdown)
{
    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

#ifdef RT_OS_WINDOWS
    /* Required for network information (must be called per thread). */
    if (g_pfnWSAStartup)
    {
        WSADATA wsaData;
        RT_ZERO(wsaData);
        if (g_pfnWSAStartup(MAKEWORD(2, 2), &wsaData))
            VGSvcError("VMInfo/Network: WSAStartup failed! Error: %Rrc\n", RTErrConvertFromWin32(g_pfnWSAGetLastError()));
    }

    /* Windows: Start network change monitoring thread, if we can. */
    VGSvcVMInfoWinWorkerStarting(pfShutdown);
#endif

    /*
     * Cleanup old properties before we start.
     */
    const char *apszPat[] =
    {
        "/VirtualBox/GuestInfo/Net/*",
        "/VirtualBox/GuestInfo/User/*",
        "/VirtualBox/GuestInfo/Debug/*",
        "/VirtualBox/GuestAdd/Components/*",
    };
    VbglGuestPropDelSet(&g_VMInfoGuestPropSvcClient, &apszPat[0], RT_ELEMENTS(apszPat));

    /*
     * Write the fixed properties first.
     */
    vgsvcVMInfoWriteFixedProperties();

    /*
     * Now enter the loop retrieving runtime data continuously.
     */
    int rc = VINF_SUCCESS;
    for (;;)
    {
        /*
         * Refresh the data.
         */
        vgsvcVMInfoWriteUsers();
        vgsvcVMInfoWriteNetwork();

        /* delete stale entries. */
        VGSvcPropCachedDeleteNotUpdated(&g_VMInfoPropCache);

        /*
         * Do VDE connection logging, if enabled.
         */
#ifndef WITH_VDE_CONNECTION_MONITORING
        bool const fWait = true;
#else
        bool const fWait = vbsvcDoVdeConnectionChangePolling();
#endif

        /*
         * Flush all properties if we were restored.
         */
        uint64_t idNewSession = g_idVMInfoSession;
        VbglR3QuerySessionId(&idNewSession);
        if (idNewSession != g_idVMInfoSession)
        {
            VGSvcVerbose(3, "The VM session ID changed, flushing all properties\n");
            vgsvcVMInfoWriteFixedProperties();
            VGSvcPropCacheFlush(&g_VMInfoPropCache);
            g_idVMInfoSession = idNewSession;
        }

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = VERR_TIMEOUT;
        if (fWait)
            rc2 = RTSemEventMultiWait(g_hVMInfoEvent, g_cMsVMInfoInterval);
        if (*pfShutdown)
            break;
        if (rc2 == VERR_TIMEOUT)
        { /* likely */ }
        else if (RT_SUCCESS(rc2))
        {
            /* Reset event semaphore if it got triggered. */
            rc2 = RTSemEventMultiReset(g_hVMInfoEvent);
            if (RT_FAILURE(rc2))
                VGSvcError("RTSemEventMultiReset failed; rc2=%Rrc\n", rc2);
        }
        else
        {
            VGSvcError("RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

#ifdef RT_OS_WINDOWS
    VGSvcVMInfoWinWorkerStopping();
    if (g_pfnWSACleanup)
        g_pfnWSACleanup();
#endif

    return rc;
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnStop}
 */
static DECLCALLBACK(void) vbsvcVMInfoStop(void)
{
    RTSemEventMultiSignal(g_hVMInfoEvent);
#ifdef RT_OS_WINDOWS
    VGSvcVMInfoWinStop();
#endif
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnTerm}
 */
static DECLCALLBACK(void) vbsvcVMInfoTerm(void)
{
    if (g_hVMInfoEvent != NIL_RTSEMEVENTMULTI)
    {
#ifdef RT_OS_WINDOWS
        VGSvcVMInfoWinTerm();
#endif

        /* Destroy property cache (will delete or reset temporary values) */
        VGSvcPropCacheTerm(&g_VMInfoPropCache);

        /* Disconnect from guest properties service. */
        VbglGuestPropDisconnect(&g_VMInfoGuestPropSvcClient);

#ifdef WITH_VDE_CONNECTION_MONITORING
        /* Destroy LA client info. */
        vgsvcFreeLAClientInfo(&g_LAClientInfo);
#endif

        RTSemEventMultiDestroy(g_hVMInfoEvent);
        g_hVMInfoEvent = NIL_RTSEMEVENTMULTI;
    }
}


/** Options. */
static const RTGETOPTDEF g_aVMInfoOptions[] =
{
    { "--vminfo-interval",              kVGSvcOptVminfoInterval,            RTGETOPT_REQ_UINT32 },
    { "--vminfo-user-idle-threshold",   kVGSvcOptVminfoUserIdleThreshold,   RTGETOPT_REQ_UINT32 },
};


/**
 * The 'vminfo' service description.
 */
VBOXSERVICE g_VMInfo =
{
    /* pszName. */
    "vminfo",
    /* pszDescription. */
    "Virtual Machine Information",
    /* pszUsage. */
    "           [--vminfo-interval <ms>] [--vminfo-user-idle-threshold <ms>]"
    ,
    /* pszOptions. */
    "    --vminfo-interval       Specifies the interval at which to retrieve the\n"
    "                            VM information. The default is 10000 ms.\n"
    "    --vminfo-user-idle-threshold <ms>\n"
    "                            Specifies the user idle threshold (in ms) for\n"
    "                            considering a guest user as being idle. The default\n"
    "                            is 5000 (5 seconds).\n"
    ,
    /* paOptions, cOptions. */
    g_aVMInfoOptions,
    RT_ELEMENTS(g_aVMInfoOptions),
    /* methods */
    VGSvcDefaultPreInit,
    vbsvcVMInfoOption,
    vbsvcVMInfoInit,
    vbsvcVMInfoWorker,
    vbsvcVMInfoStop,
    vbsvcVMInfoTerm
};

