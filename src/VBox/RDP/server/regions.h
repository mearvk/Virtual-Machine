/* $Id: regions.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VRDP_INCLUDED_SRC_regions_h
#define VRDP_INCLUDED_SRC_regions_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpdefs.h"

struct _REGIONCTX;
typedef struct _REGIONCTX REGIONCTX;

struct _REGION;
typedef struct _REGION REGION;

typedef struct _RGNRECT
{
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
} RGNRECT;

REGIONCTX *rgnCtxCreate (void);
void rgnCtxRelease (REGIONCTX *pctx);

bool rgnIntersectRects (RGNRECT *prectResult,
                        const RGNRECT *prect1,
                        const RGNRECT *prect2);
void rgnMergeRects (RGNRECT *prectResult,
                    const RGNRECT *prect1,
                    const RGNRECT *prect2);

bool rgnIsRectEmpty (const RGNRECT *prect);
bool rgnIsRectWithin (const RGNRECT *rect, const RGNRECT *rectTest);

bool rgnIsEmpty (const REGION *prgn);

// #define RGNLEAK

#ifdef RGNLEAK
REGION *rgnCreateEmptyDbg (REGIONCTX *pctx, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t uniq, const char *pszCaller, int iLine);
#define rgnCreateEmpty(__pctx, __x, __y, __w, __h, __uniq) rgnCreateEmptyDbg (__pctx, __x, __y, __w, __h, __uniq, __FILE__,  __LINE__)
#else
REGION *rgnCreateEmpty (REGIONCTX *pctx, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t uniq);
#endif /* RGNLEAK */

void rgnDelete (REGION *prgn);

void rgnInvert (REGION *prgn);

void rgnCut (REGION *prgnSect, const REGION *prgn, const RGNRECT *prectCut);

void rgnAddRect (REGION *prgn, const RGNRECT *prect);
void rgnAdd (REGION *prgn, REGION *padd);

void rgnEnumRect (REGION *prgn);
RGNRECT *rgnNextRect (REGION *prgn);

void rgnUpdateRectWidth (RGNRECT *prect, int32_t x, uint32_t w);

void rgnRemoveEmptyBricks (REGION *prgn);
void rgnMergeAdjacentRows (REGION *prgn);

uint32_t rgnGetUniq (REGION *prgn);

void rgnReset (REGION *prgn, uint32_t uniq);

#endif /* !VRDP_INCLUDED_SRC_regions_h */
