/* $Id: tstDisasmArmv8-1-asm-pauth.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - PAuth.
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

BEGINPROC_HIDDEN TestProcA64PAuth

        .cpu generic+pauth

        pacia1716
        pacib1716
        autia1716
        autib1716
        paciaz
        paciasp
        pacibz
        pacibsp
        autiaz
        autiasp
        autibz
        autibsp

        retaa
        retab

        braaz x15
        brabz xzr

        blraaz x15
        blrabz xzr

        eretaa
        eretab

        braa xzr, sp
        braa x0, x1
        brab xzr, sp
        brab x0, x1
        blraa xzr, sp
        blraa x0, x1
        blrab xzr, sp
        blrab x0, x1

        /* FEAT_PAuth memory loads */

        ldraa x0, [x1]
        ldraa x0, [x1, #-1024]
        ldraa x0, [x1, #-4096]
        ldraa x0, [x1, #4088]

        ldraa xzr, [sp]
        ldraa xzr, [sp, #-1024]
        ldraa xzr, [sp, #-4096]
        ldraa xzr, [sp, #4088]

        ldraa x0, [x1, #0]!
        ldraa x0, [x1, #-1024]!
        ldraa x0, [x1, #-4096]!
        ldraa x0, [x1, #4088]!

        ldraa xzr, [sp, #0]!
        ldraa xzr, [sp, #-1024]!
        ldraa xzr, [sp, #-4096]!
        ldraa xzr, [sp, #4088]!


        ldrab x0, [x1]
        ldrab x0, [x1, #-1024]
        ldrab x0, [x1, #-4096]
        ldrab x0, [x1, #4088]

        ldrab xzr, [sp]
        ldrab xzr, [sp, #-1024]
        ldrab xzr, [sp, #-4096]
        ldrab xzr, [sp, #4088]

        ldrab x0, [x1, #0]!
        ldrab x0, [x1, #-1024]!
        ldrab x0, [x1, #-4096]!
        ldrab x0, [x1, #4088]!

        ldrab xzr, [sp, #0]!
        ldrab xzr, [sp, #-1024]!
        ldrab xzr, [sp, #-4096]!
        ldrab xzr, [sp, #4088]!

        pacga  xzr, x0, x1
        pacga  x28, x0, sp

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64PAuth_EndProc
