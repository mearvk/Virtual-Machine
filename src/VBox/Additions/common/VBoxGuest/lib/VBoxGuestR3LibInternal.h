/* $Id: VBoxGuestR3LibInternal.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBoxGuestR3Lib - Ring-3 support library for the guest additions, Internal header.
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

#ifndef GA_INCLUDED_SRC_common_VBoxGuest_lib_VBoxGuestR3LibInternal_h
#define GA_INCLUDED_SRC_common_VBoxGuest_lib_VBoxGuestR3LibInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/VBoxGuestLibHGCMInline.h>

RT_C_DECLS_BEGIN

int     vbglR3DoIOCtl(uintptr_t uFunction, PVBGLREQHDR pReq, size_t cbReq);
int     vbglR3DoIOCtlRaw(uintptr_t uFunction, PVBGLREQHDR pReq, size_t cbReq);
int     vbglR3GRAlloc(VMMDevRequestHeader **ppReq, size_t cb, VMMDevRequestType enmReqType);
int     vbglR3GRPerform(VMMDevRequestHeader *pReq);
void    vbglR3GRFree(VMMDevRequestHeader *pReq);


RT_C_DECLS_END

#endif /* !GA_INCLUDED_SRC_common_VBoxGuest_lib_VBoxGuestR3LibInternal_h */

