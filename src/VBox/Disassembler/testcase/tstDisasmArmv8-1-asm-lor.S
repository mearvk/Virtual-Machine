/* $Id: tstDisasmArmv8-1-asm-lor.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - LOR.
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

#include <iprt/asmdefs-arm.h>

BEGINPROC_HIDDEN TestProcA64Lor

#ifndef RT_OS_DARWIN
        .cpu generic+lor /* Our macOS toolchain doesn't know about lor but includes support by default */
#endif

        ldlarb w0,  [x1]
        ldlarb wzr, [sp]

        ldlarh w0,  [x1]
        ldlarh wzr, [sp]

        ldlar w0,  [x1]
        ldlar wzr, [sp]

        ldlar x0,  [x1]
        ldlar xzr, [sp]

        stllrb w0,  [x1]
        stllrb wzr, [sp]

        stllrh w0,  [x1]
        stllrh wzr, [sp]

        stllr w0,  [x1]
        stllr wzr, [sp]

        stllr x0,  [x1]
        stllr xzr, [sp]

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64Lor_EndProc
