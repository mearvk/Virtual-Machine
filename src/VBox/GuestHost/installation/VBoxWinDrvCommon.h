/* $Id: VBoxWinDrvCommon.h 111786 2025-11-18 10:33:09Z andreas.loeffler@oracle.com $ */
/** @file
 * VBoxWinDrvCommon - Common Windows driver functions.
 */

/*
 * Copyright (C) 2024-2025 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_SRC_installation_VBoxWinDrvCommon_h
#define VBOX_INCLUDED_SRC_installation_VBoxWinDrvCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/windows.h>
#include <iprt/win/setupapi.h>

#include <iprt/utf16.h>

#include <VBox/GuestHost/VBoxWinDrvDefs.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Function pointer for a general try INF section callback. */
typedef int (*PFNVBOXWINDRVINST_TRYINFSECTION_CALLBACK)(HINF hInf, PCRTUTF16 pwszSection, void *pvCtx);


/* newdev.dll: */
typedef BOOL(WINAPI* PFNDIINSTALLDRIVERW) (HWND hwndParent, LPCWSTR InfPath, DWORD Flags, PBOOL NeedReboot);
typedef BOOL(WINAPI* PFNDIUNINSTALLDRIVERW) (HWND hwndParent, LPCWSTR InfPath, DWORD Flags, PBOOL NeedReboot);
typedef BOOL(WINAPI* PFNUPDATEDRIVERFORPLUGANDPLAYDEVICESW) (HWND hwndParent, LPCWSTR HardwareId, LPCWSTR FullInfPath, DWORD InstallFlags, PBOOL bRebootRequired);
/* setupapi.dll: */
typedef VOID(WINAPI* PFNINSTALLHINFSECTIONW) (HWND Window, HINSTANCE ModuleHandle, PCWSTR CommandLine, INT ShowCommand);
typedef BOOL(WINAPI* PFNSETUPCOPYOEMINFW) (PCWSTR SourceInfFileName, PCWSTR OEMSourceMediaLocation, DWORD OEMSourceMediaType, DWORD CopyStyle, PWSTR DestinationInfFileName, DWORD DestinationInfFileNameSize, PDWORD RequiredSize, PWSTR DestinationInfFileNameComponent);
typedef HINF(WINAPI* PFNSETUPOPENINFFILEW) (PCWSTR FileName, PCWSTR InfClass, DWORD InfStyle, PUINT ErrorLine);
typedef VOID(WINAPI* PFNSETUPCLOSEINFFILE) (HINF InfHandle);
typedef BOOL(WINAPI* PFNSETUPDIGETINFCLASSW) (PCWSTR, LPGUID, PWSTR, DWORD, PDWORD);
typedef BOOL(WINAPI* PFNSETUPUNINSTALLOEMINFW) (PCWSTR InfFileName, DWORD Flags, PVOID Reserved);
typedef BOOL(WINAPI *PFNSETUPSETNONINTERACTIVEMODE) (BOOL NonInteractiveFlag);
/* advapi32.dll: */
typedef BOOL(WINAPI *PFNQUERYSERVICESTATUSEX) (SC_HANDLE, SC_STATUS_TYPE, LPBYTE, DWORD, LPDWORD);

extern PFNDIINSTALLDRIVERW                    g_pfnDiInstallDriverW;
extern PFNDIUNINSTALLDRIVERW                  g_pfnDiUninstallDriverW;
extern PFNUPDATEDRIVERFORPLUGANDPLAYDEVICESW  g_pfnUpdateDriverForPlugAndPlayDevicesW;

extern PFNINSTALLHINFSECTIONW                 g_pfnInstallHinfSectionW;
extern PFNSETUPCOPYOEMINFW                    g_pfnSetupCopyOEMInf;
extern PFNSETUPOPENINFFILEW                   g_pfnSetupOpenInfFileW;
extern PFNSETUPCLOSEINFFILE                   g_pfnSetupCloseInfFile;
extern PFNSETUPDIGETINFCLASSW                 g_pfnSetupDiGetINFClassW;
extern PFNSETUPUNINSTALLOEMINFW               g_pfnSetupUninstallOEMInfW;
extern PFNSETUPSETNONINTERACTIVEMODE          g_pfnSetupSetNonInteractiveMode;

extern PFNQUERYSERVICESTATUSEX                g_pfnQueryServiceStatusEx;


int VBoxWinDrvInfOpenEx(PCRTUTF16 pwszInfFile, PRTUTF16 pwszClassName, HINF *phInf);
int VBoxWinDrvInfOpen(PCRTUTF16 pwszInfFile, HINF *phInf);
int VBoxWinDrvInfOpenUtf8(const char *pszInfFile, HINF *phInf);
int VBoxWinDrvInfClose(HINF hInf);
PRTUTF16 VBoxWinDrvInfGetPathFromId(unsigned idDir, PCRTUTF16 pwszSubDir);
VBOXWINDRVINFTYPE VBoxWinDrvInfGetTypeEx(HINF hInf, PRTUTF16 *ppwszSection);
VBOXWINDRVINFTYPE VBoxWinDrvInfGetType(HINF hInf);
int VBoxWinDrvInfQueryCopyFiles(HINF hInf, PRTUTF16 pwszSection, PVBOXWINDRVINFLIST *ppCopyFiles);
int VBoxWinDrvInfQueryFirstModel(HINF hInf, PCRTUTF16 pwszSection, PRTUTF16 *ppwszModel);
int VBoxWinDrvInfQueryFirstPnPId(HINF hInf, PRTUTF16 pwszModel, PRTUTF16 *ppwszPnPId);
int VBoxWinDrvInfQueryKeyValue(PINFCONTEXT pCtx, DWORD iValue, PRTUTF16 *ppwszValue, PDWORD pcwcValue);
int VBoxWinDrvInfQueryModelEx(HINF hInf, PCRTUTF16 pwszSection, unsigned uIndex, PRTUTF16 *ppwszValue, PDWORD pcwcValue);
int VBoxWinDrvInfQueryModel(HINF hInf, PCRTUTF16 pwszSection, unsigned uIndex, PRTUTF16 *ppwszValue, PDWORD pcwcValue);
int VBoxWinDrvInfQueryModelSection(HINF hInf, PCRTUTF16 pwszModel, PRTUTF16 *ppwszSection);
int VBoxWinDrvInfQueryParms(HINF hInf, PVBOXWINDRVINFPARMS pParms, bool fForce);
int VBoxWinDrvInfQuerySectionKeyByIndex(HINF hInf, PCRTUTF16 pwszSection, PRTUTF16 *ppwszValue, PDWORD pcwcValue);
int VBoxWinDrvInfQuerySectionVerEx(HINF hInf, UINT uIndex, PVBOXWINDRVINFSECVERSION pVer);
int VBoxWinDrvInfQuerySectionVer(HINF hInf, PVBOXWINDRVINFSECVERSION pVer);
bool VBoxWinDrvInfSectionExists(HINF hInf, PCRTUTF16 pwszSection);
int VBoxWinDrvInfTrySection(HINF hInf, PCRTUTF16 pwszSection, PCRTUTF16 pwszSuffix, PFNVBOXWINDRVINST_TRYINFSECTION_CALLBACK pfnCallback, void *pvCtx);

PVBOXWINDRVINFLIST VBoxWinDrvInfListCreate(VBOXWINDRVINFLISTENTRY_T enmType);
int VBoxWinDrvInfListInit(PVBOXWINDRVINFLIST pInfList, VBOXWINDRVINFLISTENTRY_T enmType);
void VBoxWinDrvInfListDestroy(PVBOXWINDRVINFLIST pInfList);
PVBOXWINDRVINFLIST VBoxWinDrvInfListDup(PVBOXWINDRVINFLIST pInfList);

const char *VBoxWinDrvSetupApiErrToStr(const DWORD dwErr);
const char *VBoxWinDrvWinErrToStr(const DWORD dwErr);
int VBoxWinDrvInstErrorFromWin32(unsigned uNativeCode);

int VBoxWinDrvRegQueryDWORDW(HKEY hKey, LPCWSTR pwszName, DWORD *pdwValue);
int VBoxWinDrvRegQueryDWORD(HKEY hKey, const char *pszName, DWORD *pdwValue);

#endif /* !VBOX_INCLUDED_SRC_installation_VBoxWinDrvCommon_h */

