/* $Id: elf-arm64.h $ */
/** @file
 * IPRT - ELF definitions for ARM64 / AArch64.
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


#ifndef IPRT_INCLUDED_formats_elf_arm64_h
#define IPRT_INCLUDED_formats_elf_arm64_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


#define R_AARCH64_NONE                  0

/** @name Static data relocation
 * @{  */
#define R_AARCH64_ABS64                 257
#define R_AARCH64_ABS32                 258
#define R_AARCH64_ABS16                 259
#define R_AARCH64_PREL64                260
#define R_AARCH64_PREL32                261
#define R_AARCH64_PREL16                262
#define R_AARCH64_PLT32                 314
/** @} */

/** @name Static data relocation
 * @{  */
#define R_AARCH64_TSTBR14               279
#define R_AARCH64_CONDBR19              280
#define R_AARCH64_JUMP26                282
#define R_AARCH64_CALL26                283
/** @}   */

/** @name Dynamic relocations
 * @{  */
#define R_AARCH64_COPY                  1024
#define R_AARCH64_GLOB_DAT              1025
#define R_AARCH64_JUMP_SLOT             1026
#define R_AARCH64_RELATIVE              1027
#define R_AARCH64_TLS_IMPDEF1           1028
#define R_AARCH64_TLS_IMPDEF2           1029
#define R_AARCH64_TLS_DTPMOD            R_AARCH64_TLS_IMPDEF1  /* Linux mapping */
#define R_AARCH64_TLS_DTPREL            R_AARCH64_TLS_IMPDEF2  /* Linux mapping */
#define R_AARCH64_TLS_TPREL             1030
#define R_AARCH64_TLSDESC               1031
#define R_AARCH64_IRELATIVE             1032
/** @} */

#endif /* !IPRT_INCLUDED_formats_elf_arm64_h */

