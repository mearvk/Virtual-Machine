/* $Id: tstDisasmArmv8-1-asm-crc.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - CRC.
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

BEGINPROC_HIDDEN TestProcA64Crc

        .cpu generic+crc

        crc32b w0, w1, wzr
        crc32w w0, w1, wzr
        crc32h w0, w1, wzr

        crc32cb w0, w1, wzr
        crc32cw w0, w1, wzr
        crc32ch w0, w1, wzr

        crc32x w0, w1, x3
        crc32x w0, w1, xzr

        crc32cx w0, w1, x3
        crc32cx w0, w1, xzr

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64Crc_EndProc
