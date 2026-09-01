/* $Id: videodetector.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
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

#ifndef VRDP_INCLUDED_SRC_videodetector_h
#define VRDP_INCLUDED_SRC_videodetector_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpserv.h"

DECLHIDDEN(int) queryPropertyInt64Def(VRDPServer *pServer,
                                      const char *pszProperty,
                                      int64_t *pi64Val,
                                      int64_t  i64Def);
DECLHIDDEN(int) queryPropertyBoolDef(VRDPServer *pServer,
                                     const char *pszProperty,
                                     bool *pfVal,
                                     bool fDef);
DECLHIDDEN(uint32_t) idCreate(void);

/* In videohandler.cpp actually */
DECLHIDDEN(void) vhSourceComputeScaledSize(VHCONTEXT *pCtx, RGNRECT *pRectScaled, const RGNRECT *pRectOriginal, bool fDirect);


#endif /* !VRDP_INCLUDED_SRC_videodetector_h */
