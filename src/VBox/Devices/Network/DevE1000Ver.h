/** $Id: DevE1000Ver.h 112643 2026-01-19 14:25:02Z aleksey.ilyushin@oracle.com $ */
/** @file
 * DevE1000Ver - Intel 82540EM Ethernet Controller saved state versions, Header.
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

#ifndef VBOX_INCLUDED_SRC_Network_DevE1000Ver_h
#define VBOX_INCLUDED_SRC_Network_DevE1000Ver_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** The current Saved state version. */
# define E1K_SAVEDSTATE_VERSION               6
/** Saved state version at the introduction of 82583V support. */
# define E1K_SAVEDSTATE_VERSION_82583V        5
/** Saved state version before the introduction of 82583V support. */
# define E1K_SAVEDSTATE_VERSION_PRE_82583V    4

#endif /* !VBOX_INCLUDED_SRC_Network_DevE1000Ver_h */

