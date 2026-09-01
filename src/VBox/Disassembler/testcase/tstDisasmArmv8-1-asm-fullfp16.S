/* $Id: tstDisasmArmv8-1-asm-fullfp16.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

BEGINPROC_HIDDEN TestProcA64FullFp16

#ifndef RT_OS_DARWIN
        .cpu generic+fp16 /* Our macOS toolchain doesn't know about fp16 but includes support by default */
#endif

        fmadd  h0, h1, h3, h31
        fmsub  h0, h1, h3, h31
        fnmadd h0, h1, h3, h31
        fnmsub h0, h1, h3, h31

        fcsel  h0, h1, h2, eq
        fmul   h0, h1, h2
        fdiv   h0, h1, h2
        fadd   h0, h1, h2
        fsub   h0, h1, h2
        fmax   h0, h1, h2
        fmin   h0, h1, h2
        fmaxnm h0, h1, h2
        fminnm h0, h1, h2
        fnmul  h0, h1, h2

        fccmp  h0, h1, #0,  eq
        fccmp  h0, h1, #15, ne

        fccmpe h0, h1, #0,  eq
        fccmpe h0, h1, #15, ne

        fcmp h0, h1
        /*fcmp h0, #0.0 */

        fcmpe h0, h1
        /*fcmpe h0, #0.0 */

        fmov  h0, h1
        fabs  h0, h1
        fneg  h0, h1
        fsqrt h0, h1

        fcvt s0, h1
        fcvt d0, h1
        fcvt h0, s1
        fcvt h0, d1

        fcvtzu x0, h0, #1
        fcvtzu x0, h0, #31
        fcvtzu x0, h0, #63
        fcvtzu w0, h0, #1
        fcvtzu w0, h0, #32

        fcvtzs x0, h0, #1
        fcvtzs x0, h0, #31
        fcvtzs x0, h0, #63
        fcvtzs w0, h0, #1
        fcvtzs w0, h0, #32

        frintn h0, h1
        frintp h0, h1
        frintm h0, h1
        frintz h0, h1
        frinta h0, h1
        frintx h0, h1
        frinti h0, h1

        ucvtf  h0, x0, #1
        ucvtf  h0, x0, #31
        ucvtf  h0, x0, #63
        ucvtf  h0, w0, #1
        ucvtf  h0, w0, #32

        scvtf  h0, x0, #1
        scvtf  h0, x0, #31
        scvtf  h0, x0, #63
        scvtf  h0, w0, #1
        scvtf  h0, w0, #32

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64FullFp16_EndProc
