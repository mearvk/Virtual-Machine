/* $Id: VBoxR0-wrapped.lds 114002 2026-04-23 23:07:16Z knut.osmundsen@oracle.com $ */
/** @file
 * Linker script augmentations for wrapped .r0.
 */

/*
 * Copyright (C) 2021-2025 Oracle and/or its affiliates.
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

/*
 * The --force-group-allocation option needs a little assistance.
 *
 * We also wish to combine the unlikely and cold text sections
 * with the other text sections to simplify text section tracking
 * in SUPDrv.
 *
 * Finally, all (important) sections should and vboxr0mod_section_start
 * and vboxr0mod_section_end symbols so we can more correctly calculate
 * the module span (for probe validation and such).
 */
SECTIONS
{
    .text :
    {
        vboxr0mod_text_start = .;
        *(.text.hot)
        *(.text .text._*)
        *(.text.unlikely)
        *(.text.cold)
        vboxr0mod_text_end   = .;
    }

    .rodata :
    {
        vboxr0mod_rodata_start = .;
        *(.rodata .rodata._*)
        vboxr0mod_rodata_end = .;
    }
    .rodata.str8 :
    {
        vboxr0mod_rodata_str8_start = .;
        *(.rodata.str8 .rodata.str8.*)
        vboxr0mod_rodata_str8_end = .;
    }
    .rodata.str1 :
    {
        vboxr0mod_rodata_str1_start = .;
        *(.rodata.str1 .rodata.str1.*)
        vboxr0mod_rodata_str1_end = .;
    }
    .rodata.cst2 :
    {
        vboxr0mod_rodata_cst2_start = .;
        *(.rodata.cst2 .rodata.cst2.*)
        vboxr0mod_rodata_cst2_end = .;
    }
}

/*
 * Combine the VBoxTpG sections into .data.
 */
SECTIONS
{
    .data :
    {
        vboxr0mod_data_start = .;
        *(.VTGData)
        *(.VTGPrLc.Begin)
        *(.VTGPrLc)
        *(.VTGPrLc.End)
        *(.data)
        vboxr0mod_data_end = .;
    }

    .bss :
    {
        vboxr0mod_bss_start = .;
        *(.bss)
        vboxr0mod_bss_end = .;
    }
}

