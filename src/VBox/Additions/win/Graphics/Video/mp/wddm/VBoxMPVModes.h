/* $Id: VBoxMPVModes.h 113109 2026-02-20 17:03:12Z vitali.pelenjow@oracle.com $ */
/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2014-2025 Oracle and/or its affiliates.
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVModes_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVModes_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* A temporary development define. Allows to set a preferred video mode for each display
 * and resize displays from VBoxService when the guest is in the login screen
 * (VBoxTray is not running yet).
 */
#define VBOXWDDM_NEW_VIDPN

//#include "../../common/VBoxVideoTools.h"

#include "VBoxMPSa.h"

#define _CR_TYPECAST(_Type, _pVal) ((_Type*)((void*)(_pVal)))

DECLINLINE(uint64_t) vboxRSize2U64(RTRECTSIZE size) { return *_CR_TYPECAST(uint64_t, &(size)); }
DECLINLINE(RTRECTSIZE) vboxU642RSize2(uint64_t size) { return *_CR_TYPECAST(RTRECTSIZE, &(size)); }

#define CR_RSIZE2U64 vboxRSize2U64
#define CR_U642RSIZE vboxU642RSize2

int VBoxWddmVModesInit(PVBOXMP_DEVEXT pExt);
void VBoxWddmVModesCleanup();
const CR_SORTARRAY* VBoxWddmVModesGet(PVBOXMP_DEVEXT pExt, uint32_t u32Target);
#ifdef VBOXWDDM_NEW_VIDPN
void VBoxWddmVModesQueryPreferred(PVBOXMP_DEVEXT pExt, uint32_t u32Target, RTRECTSIZE *pResolution);
#endif
int VBoxWddmVModesRemove(PVBOXMP_DEVEXT pExt, uint32_t u32Target, const RTRECTSIZE *pResolution);
int VBoxWddmVModesAdd(PVBOXMP_DEVEXT pExt, uint32_t u32Target, const RTRECTSIZE *pResolution, BOOLEAN fTrancient, BOOLEAN fPreferred);

NTSTATUS VBoxWddmChildStatusReportReconnected(PVBOXMP_DEVEXT pDevExt, uint32_t iChild);
NTSTATUS VBoxWddmChildStatusConnect(PVBOXMP_DEVEXT pDevExt, uint32_t iChild, BOOLEAN fConnect);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVModes_h */
