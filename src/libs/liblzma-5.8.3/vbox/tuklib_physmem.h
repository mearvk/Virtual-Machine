/* $Id: tuklib_physmem.h 113768 2026-04-08 18:23:12Z alexander.rudnev@oracle.com $ */
/** @file
 * tuklib_physmem.h - Memory information.
 */

/*
 * Copyright (C) 2023-2026 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_SRC_vbox_tuklib_physmem_h
#define VBOX_INCLUDED_SRC_vbox_tuklib_physmem_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "sysdefs.h"
RT_C_DECLS_BEGIN

DECL_FORCE_INLINE(uint64_t) tuklib_physmem(void)
{
    uint64_t cbMemAvail = 0;
    int rc = RTSystemQueryAvailableRam(&cbMemAvail);
    if (RT_FAILURE(rc))
        cbMemAvail = 0;
    return cbMemAvail;
}

RT_C_DECLS_END
#endif /* !VBOX_INCLUDED_SRC_vbox_tuklib_physmem_h */
