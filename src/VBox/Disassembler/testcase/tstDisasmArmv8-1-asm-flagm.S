/* $Id: tstDisasmArmv8-1-asm-flagm.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - FlagM.
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

BEGINPROC_HIDDEN TestProcA64FlagM

        .cpu generic+flagm

        rmif x0,  #0,  #0
        rmif xzr, #63, #0xf

        setf8 w0
        setf8 wzr

        setf16 w0
        setf16 wzr

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64FlagM_EndProc
