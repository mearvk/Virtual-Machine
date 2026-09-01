/* $Id: tstDisasmArmv8-1-asm-frint3264.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - Frint3264.
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

BEGINPROC_HIDDEN TestProcA64Frint3264

        .arch armv8.5-a /* No specific flag to enable FEAT_FRINTTS */

        frint32z s0, s1
        frint32z d0, d1

        frint32x s0, s1
        frint32x d0, d1

        frint64z s0, s1
        frint64z d0, d1

        frint64x s0, s1
        frint64x d0, d1

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64Frint3264_EndProc
