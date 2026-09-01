/* $Id: tstDisasmArmv8-1-tests.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Collection of testcases for the ARMv8 disassembler.
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

#ifndef VBOX_INCLUDED_SRC_testcase_tstDisasmArmv8_1_tests_h
#define VBOX_INCLUDED_SRC_testcase_tstDisasmArmv8_1_tests_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

RT_C_DECLS_BEGIN

extern const unsigned char g_abtstDisasmArmv8_1[];
extern const unsigned g_cbtstDisasmArmv8_1;

extern const unsigned char g_abtstDisasmArmv8_1_Simd[];
extern const unsigned g_cbtstDisasmArmv8_1_Simd;

extern const unsigned char g_abtstDisasmArmv8_1_PAuth[];
extern const unsigned g_cbtstDisasmArmv8_1_PAuth;

extern const unsigned char g_abtstDisasmArmv8_1_FlagM[];
extern const unsigned g_cbtstDisasmArmv8_1_FlagM;

extern const unsigned char g_abtstDisasmArmv8_1_FullFp16[];
extern const unsigned g_cbtstDisasmArmv8_1_FullFp16;

extern const unsigned char g_abtstDisasmArmv8_1_Lor[];
extern const unsigned g_cbtstDisasmArmv8_1_Lor;

extern const unsigned char g_abtstDisasmArmv8_1_Lse[];
extern const unsigned g_cbtstDisasmArmv8_1_Lse;

extern const unsigned char g_abtstDisasmArmv8_1_Crc[];
extern const unsigned g_cbtstDisasmArmv8_1_Crc;

extern const unsigned char g_abtstDisasmArmv8_1_Frint3264[];
extern const unsigned g_cbtstDisasmArmv8_1_Frint3264;

extern const unsigned char g_abtstDisasmArmv8_1_Rcpc[];
extern const unsigned g_cbtstDisasmArmv8_1_Rcpc;

extern const unsigned char g_abtstDisasmArmv8_1_The[];
extern const unsigned g_cbtstDisasmArmv8_1_The;

extern const unsigned char g_abtstDisasmArmv8_1_Mte[];
extern const unsigned g_cbtstDisasmArmv8_1_Mte;

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_testcase_tstDisasmArmv8_1_tests_h */

