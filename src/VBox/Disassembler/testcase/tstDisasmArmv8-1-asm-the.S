/* $Id: tstDisasmArmv8-1-asm-the.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - THE.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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

BEGINPROC_HIDDEN TestProcA64The

        .cpu generic+the

        rcwclr x1,  x2,  [x0]
        rcwclr xzr, xzr, [sp]

        rcwswp x1,  x2,  [x0]
        rcwswp xzr, xzr, [sp]

        rcwset x1,  x2,  [x0]
        rcwset xzr, xzr, [sp]

        rcwclrl x1,  x2,  [x0]
        rcwclrl xzr, xzr, [sp]

        rcwswpl x1,  x2,  [x0]
        rcwswpl xzr, xzr, [sp]

        rcwsetl x1,  x2,  [x0]
        rcwsetl xzr, xzr, [sp]

        rcwclra x1,  x2,  [x0]
        rcwclra xzr, xzr, [sp]

        rcwswpa x1,  x2,  [x0]
        rcwswpa xzr, xzr, [sp]

        rcwseta x1,  x2,  [x0]
        rcwseta xzr, xzr, [sp]

        rcwclral x1,  x2,  [x0]
        rcwclral xzr, xzr, [sp]

        rcwswpal x1,  x2,  [x0]
        rcwswpal xzr, xzr, [sp]

        rcwsetal x1,  x2,  [x0]
        rcwsetal xzr, xzr, [sp]

        rcwsclr x1,  x2,  [x0]
        rcwsclr xzr, xzr, [sp]

        rcwsswp x1,  x2,  [x0]
        rcwsswp xzr, xzr, [sp]

        rcwsset x1,  x2,  [x0]
        rcwsset xzr, xzr, [sp]

        rcwsclrl x1,  x2,  [x0]
        rcwsclrl xzr, xzr, [sp]

        rcwsswpl x1,  x2,  [x0]
        rcwsswpl xzr, xzr, [sp]

        rcwssetl x1,  x2,  [x0]
        rcwssetl xzr, xzr, [sp]

        rcwsclra x1,  x2,  [x0]
        rcwsclra xzr, xzr, [sp]

        rcwsswpa x1,  x2,  [x0]
        rcwsswpa xzr, xzr, [sp]

        rcwsseta x1,  x2,  [x0]
        rcwsseta xzr, xzr, [sp]

        rcwsclral x1,  x2,  [x0]
        rcwsclral xzr, xzr, [sp]

        rcwsswpal x1,  x2,  [x0]
        rcwsswpal xzr, xzr, [sp]

        rcwssetal x1,  x2,  [x0]
        rcwssetal xzr, xzr, [sp]

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64The_EndProc
