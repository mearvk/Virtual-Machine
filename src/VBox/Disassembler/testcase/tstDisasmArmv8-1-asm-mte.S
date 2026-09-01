/* $Id: tstDisasmArmv8-1-asm-mte.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - MTE.
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

BEGINPROC_HIDDEN TestProcA64Mte

        .cpu generic+memtag

        /* Add/subtract immediate with tags */
        addg x0, x1, #0, #0
        addg sp, sp, #1008, #0x0f

        subg x0, x1, #0, #0
        subg sp, sp, #1008, #0x0f

        subps x0, x1, x3
        subps x0, sp, sp

        subp  x0, x1, x3
        subp  x0, sp, sp

        gmi   x0, x1, x3
        gmi   x0, sp, x3

        irg   x0, x1, x3
        irg   x0, x1, xzr
        irg   sp, sp, x3
        irg   sp, sp, xzr

        /*
         * Load/Store memory tags
         */

        stg x0, [x0]
        stg x0, [x0, #-4096]
        stg x0, [x0, #4080]
        stg sp, [sp]
        stg sp, [sp, #-4096]
        stg sp, [sp, #4080]

        stg x0, [x0], #0
        stg x0, [x0], #-4096
        stg x0, [x0], #4080
        stg sp, [sp], #0
        stg sp, [sp], #-4096
        stg sp, [sp], #4080

        stg x0, [x0, #0]!
        stg x0, [x0, #-4096]!
        stg x0, [x0, #4080]!
        stg sp, [sp, #0]!
        stg sp, [sp, #-4096]!
        stg sp, [sp, #4080]!

        stzgm x1, [x0]
        stzgm xzr, [sp]

        ldg x0, [x0]
        ldg x0, [x0, #-4096]
        ldg x0, [x0, #4080]
        ldg xzr, [sp]
        ldg xzr, [sp, #-4096]
        ldg xzr, [sp, #4080]

        stzg x0, [x0]
        stzg x0, [x0, #-4096]
        stzg x0, [x0, #4080]
        stzg sp, [sp]
        stzg sp, [sp, #-4096]
        stzg sp, [sp, #4080]

        stzg x0, [x0], #0
        stzg x0, [x0], #-4096
        stzg x0, [x0], #4080
        stzg sp, [sp], #0
        stzg sp, [sp], #-4096
        stzg sp, [sp], #4080

        stzg x0, [x0, #0]!
        stzg x0, [x0, #-4096]!
        stzg x0, [x0, #4080]!
        stzg sp, [sp, #0]!
        stzg sp, [sp, #-4096]!
        stzg sp, [sp, #4080]!

        stgm x1, [x0]
        stgm xzr, [sp]

        st2g x0, [x0]
        st2g x0, [x0, #-4096]
        st2g x0, [x0, #4080]
        st2g sp, [sp]
        st2g sp, [sp, #-4096]
        st2g sp, [sp, #4080]

        st2g x0, [x0], #0
        st2g x0, [x0], #-4096
        st2g x0, [x0], #4080
        st2g sp, [sp], #0
        st2g sp, [sp], #-4096
        st2g sp, [sp], #4080

        st2g x0, [x0, #0]!
        st2g x0, [x0, #-4096]!
        st2g x0, [x0, #4080]!
        st2g sp, [sp, #0]!
        st2g sp, [sp, #-4096]!
        st2g sp, [sp, #4080]!

        ldgm x1, [x0]
        ldgm xzr, [sp]

        stz2g x0, [x0]
        stz2g x0, [x0, #-4096]
        stz2g x0, [x0, #4080]
        stz2g sp, [sp]
        stz2g sp, [sp, #-4096]
        stz2g sp, [sp, #4080]

        stz2g x0, [x0], #0
        stz2g x0, [x0], #-4096
        stz2g x0, [x0], #4080
        stz2g sp, [sp], #0
        stz2g sp, [sp], #-4096
        stz2g sp, [sp], #4080

        stz2g x0, [x0, #0]!
        stz2g x0, [x0, #-4096]!
        stz2g x0, [x0, #4080]!
        stz2g sp, [sp, #0]!
        stz2g sp, [sp, #-4096]!
        stz2g sp, [sp, #4080]!

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64Mte_EndProc
