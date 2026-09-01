/* $Id: ARM_ARM_Cortex_X925_A725.h 113643 2026-03-30 10:31:43Z knut.osmundsen@oracle.com $ */
/** @file
 * CPU database entry "ARM_Cortex_X925_A725".
 * Generated at 2026-01-27T20:46:29Z by VBoxCpuReport v7.2.97r172412 on linux.arm64.
 */

/*
 * Copyright (C) 2013-2025 Oracle and/or its affiliates.
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

#ifndef VBOX_CPUDB_ARM_ARM_Cortex_X925_A725_h
#define VBOX_CPUDB_ARM_ARM_Cortex_X925_A725_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/**
 * Common system register values for ARM_Cortex_X925_A725.
 */
static SUPARMSYSREGVAL const g_aCmnSysRegVals_ARM_ARM_Cortex_X925_A725[] =
{
    { UINT64_C(0x0000000081000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 5), 0x1 }, /* MPIDR_EL1 */
    { UINT64_C(0x1201111123111111), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 0), 0x0 }, /* ID_AA64PFR0_EL1 */
    { UINT64_C(0x0000000000010121), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 1), 0x0 }, /* ID_AA64PFR1_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 2), 0x0 }, /* ID_AA64PFR2_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 3), 0x0 },
    { UINT64_C(0x0000110100110021), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 4), 0x0 }, /* ID_AA64ZFR0_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 5), 0x0 }, /* ID_AA64SMFR0_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 6), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 7), 0x0 }, /* ID_AA64FPFR0_EL1 */
    { UINT64_C(0x100f11f310305719), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 0), 0x0 }, /* ID_AA64DFR0_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 1), 0x0 }, /* ID_AA64DFR1_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 2), 0x0 }, /* ID_AA64DFR2_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 3), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 4), 0x0 }, /* ID_AA64AFR0_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 5), 0x0 }, /* ID_AA64AFR1_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 6), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 7), 0x0 },
    { UINT64_C(0x0221111110212120), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 0), 0x0 }, /* ID_AA64ISAR0_EL1 */
    { UINT64_C(0x0111111100211002), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 1), 0x0 }, /* ID_AA64ISAR1_EL1 */
    { UINT64_C(0x0000000001005102), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 2), 0x0 }, /* ID_AA64ISAR2_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 3), 0x0 }, /* ID_AA64ISAR3_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 4), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 5), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 6), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 7), 0x0 },
    { UINT64_C(0x2100022200101122), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 0), 0x0 }, /* ID_AA64MMFR0_EL1 */
    { UINT64_C(0x1001111010312122), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 1), 0x0 }, /* ID_AA64MMFR1_EL1 */
    { UINT64_C(0x1221011110101011), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 2), 0x0 }, /* ID_AA64MMFR2_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 3), 0x0 }, /* ID_AA64MMFR3_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 4), 0x0 }, /* ID_AA64MMFR4_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 5), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 6), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 7), 0x0 },
    { UINT64_C(0x0000000000000002), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 5, 3, 0), 0x0 }, /* ERRIDR_EL1 */
    { UINT64_C(0x0000000000000026), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 9,10, 7), 0x0 }, /* PMBIDR_EL1 */
    { UINT64_C(0x0000000000000036), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 9,11, 7), 0x0 }, /* TRBIDR_EL1 */
    { UINT64_C(0x240000010006003f), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0,10, 4, 4), 0x0 }, /* MPAMIDR_EL1 */
    { UINT64_C(0x00000000c3000123), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 1, 0, 0, 1), 0x0 }, /* CLIDR_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 1, 0, 0, 7), 0x0 }, /* AIDR_EL1 */
    { UINT64_C(0x000000009444c004), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 3, 0, 0, 1), 0x0 }, /* CTR_EL0 */
    { UINT64_C(0x0000000000000004), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 3, 0, 0, 7), 0x0 }, /* DCZID_EL0 */
    { UINT64_C(0x000000003b9aca00), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 3,14, 0, 0), 0x0 }, /* CNTFRQ_EL0 */
};


/**
 * System register values for ARM Cortex-A725 (Chaberton), variation #0.
 * 10 CPUs shares this variant: 0, 1, 2, 3, 4, 10, 11, 12, 13, 14
 */
static SUPARMSYSREGVAL const g_aVar0SysRegVals_ARM_ARM_Cortex_X925_A725[] =
{
    { UINT64_C(0x00000000410fd871), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 0), 0x0 }, /* MIDR_EL1 */
    { UINT64_C(0x0000000000000001), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 6), 0x0 }, /* REVIDR_EL1 */
    { UINT64_C(0x0000000000026457), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 9, 9, 7), 0x0 }, /* PMSIDR_EL1 */
    { UINT64_C(0x0000000000000005), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 9,14, 6), 0x0 }, /* PMMIR_EL1 */
};


/**
 * System register values for ARM Cortex-A725 (Chaberton), variation #0.
 * 10 CPUs shares this variant: 0, 1, 2, 3, 4, 10, 11, 12, 13, 14
 */
static SUPARMCACHELEVEL const g_aVar0CacheEntries_ARM_ARM_Cortex_X925_A725[] =
{
    { 0x00, {0,0,0}, 0, UINT64_C(0x00ff0000001a), UINT64_MAX }, /* CSSEL=L0-data cbLine=64 cSets=256   Asc=4  (64 KiB) */
    { 0x01, {0,0,0}, 0, UINT64_C(0x00ff0000001a), UINT64_MAX }, /* CSSEL=L0-inst cbLine=64 cSets=256   Asc=4  (64 KiB) */
    { 0x02, {0,0,0}, 0, UINT64_C(0x03ff0000003a), UINT64_MAX }, /* CSSEL=L1-unif cbLine=64 cSets=1024  Asc=8  (512 KiB) */
    { 0x04, {0,0,0}, 0, UINT64_C(0x1fff0000007a), UINT64_MAX }, /* CSSEL=L2-unif cbLine=64 cSets=8192  Asc=16 (8 MiB) */
};


/**
 * System register values for ARM Cortex-X925 (Blackhawk), variation #1.
 * 10 CPUs shares this variant: 5, 6, 7, 8, 9, 15, 16, 17, 18, 19
 */
static SUPARMSYSREGVAL const g_aVar1SysRegVals_ARM_ARM_Cortex_X925_A725[] =
{
    { UINT64_C(0x00000000410fd851), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 0), 0x0 }, /* MIDR_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 6), 0x0 }, /* REVIDR_EL1 */
    { UINT64_C(0x0000000021110131), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 1, 0), 0x0 }, /* ID_PFR0_EL1 */
    { UINT64_C(0x0000000030010000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 1, 1), 0x0 }, /* ID_PFR1_EL1 */
    { UINT64_C(0x0000000017011099), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 1, 2), 0x0 }, /* ID_DFR0_EL1 */
    { UINT64_C(0x0000000010201105), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 1, 4), 0x0 }, /* ID_MMFR0_EL1 */
    { UINT64_C(0x0000000040000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 1, 5), 0x0 }, /* ID_MMFR1_EL1 */
    { UINT64_C(0x0000000001260000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 1, 6), 0x0 }, /* ID_MMFR2_EL1 */
    { UINT64_C(0x0000000002122211), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 1, 7), 0x0 }, /* ID_MMFR3_EL1 */
    { UINT64_C(0x0000000002101110), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 2, 0), 0x0 }, /* ID_ISAR0_EL1 */
    { UINT64_C(0x0000000013112111), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 2, 1), 0x0 }, /* ID_ISAR1_EL1 */
    { UINT64_C(0x0000000021232042), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 2, 2), 0x0 }, /* ID_ISAR2_EL1 */
    { UINT64_C(0x0000000001112131), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 2, 3), 0x0 }, /* ID_ISAR3_EL1 */
    { UINT64_C(0x0000000000010142), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 2, 4), 0x0 }, /* ID_ISAR4_EL1 */
    { UINT64_C(0x0000000011011121), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 2, 5), 0x0 }, /* ID_ISAR5_EL1 */
    { UINT64_C(0x0000000001021110), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 2, 6), 0x0 }, /* ID_MMFR4_EL1 */
    { UINT64_C(0x0000000001111111), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 2, 7), 0x0 }, /* ID_ISAR6_EL1 */
    { UINT64_C(0x0000000010110222), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 3, 0), 0x0 }, /* MVFR0_EL1 */
    { UINT64_C(0x0000000013211111), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 3, 1), 0x0 }, /* MVFR1_EL1 */
    { UINT64_C(0x0000000000000043), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 3, 2), 0x0 }, /* MVFR2_EL1 */
    { UINT64_C(0x0000000000000011), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 3, 4), 0x0 }, /* ID_PFR2_EL1 */
    { UINT64_C(0x0000000000036457), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 9, 9, 7), 0x0 }, /* PMSIDR_EL1 */
    { UINT64_C(0x000000000000000a), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 9,14, 6), 0x0 }, /* PMMIR_EL1 */
};


/**
 * System register values for ARM Cortex-X925 (Blackhawk), variation #1.
 * 10 CPUs shares this variant: 5, 6, 7, 8, 9, 15, 16, 17, 18, 19
 */
static SUPARMCACHELEVEL const g_aVar1CacheEntries_ARM_ARM_Cortex_X925_A725[] =
{
    { 0x00, {0,0,0}, 0, UINT64_C(0x00ff0000001a), UINT64_MAX }, /* CSSEL=L0-data cbLine=64 cSets=256   Asc=4  (64 KiB) */
    { 0x01, {0,0,0}, 0, UINT64_C(0x00ff0000001a), UINT64_MAX }, /* CSSEL=L0-inst cbLine=64 cSets=256   Asc=4  (64 KiB) */
    { 0x02, {0,0,0}, 0, UINT64_C(0x0fff0000003a), UINT64_MAX }, /* CSSEL=L1-unif cbLine=64 cSets=4096  Asc=8  (2 MiB) */
    { 0x04, {0,0,0}, 0, UINT64_C(0x1fff0000007a), UINT64_MAX }, /* CSSEL=L2-unif cbLine=64 cSets=8192  Asc=16 (8 MiB) */
};


/**
 * Database entry for ARM_Cortex_X925_A725.
 */
static CPUMDBENTRYARM const g_Entry_ARM_ARM_Cortex_X925_A725 =
{
    {
        /*.pszName      = */ "ARM_Cortex_X925_A725",
        /*.pszFullName  = */ "ARM_Cortex_X925_A725",
        /*.enmVendor    = */ CPUMCPUVENDOR_ARM,
        /*.enmMicroarch = */ kCpumMicroarch_Arm_Chaberton,
        /*.fFlags       = */ 0,
        /*.enmEntryType = */ CPUMDBENTRYTYPE_ARM,
    },
    /*.paSysRegCmnVals  = */ NULL_ALONE(g_aCmnSysRegVals_ARM_ARM_Cortex_X925_A725),
    /*.cSysRegCmnVals   = */ ZERO_ALONE(RT_ELEMENTS(g_aCmnSysRegVals_ARM_ARM_Cortex_X925_A725)),
    /*.cVariants        = */ 2,
    /*.aVariants        = */
    {
        /*.Variants[0] = */
        {
            /*.pszName      = */ "ARM Cortex-A725 (Chaberton)",
            /*.Midr         = */
            {
                /*Midr.s = */
                {
                    /*.u4Revision    = */ 0x1,
                    /*.u12PartNum    = */ 0xd87,
                    /*.u4Arch        = */ 0xf,
                    /*.u4Variant     = */ 0x0,
                    /*.u4Implementer = */ 0x41,
                }
            },
            /*.enmCoreType  = */ kCpumCoreType_Efficiency,
            /*.cSysRegVals    = */ ZERO_ALONE(RT_ELEMENTS(g_aVar0SysRegVals_ARM_ARM_Cortex_X925_A725)),
            /*.paSysRegVals   = */ NULL_ALONE(g_aVar0SysRegVals_ARM_ARM_Cortex_X925_A725),
            /*.uUnused        = */ 0,
            /*.cCacheEntries  = */ ZERO_ALONE(RT_ELEMENTS(g_aVar0CacheEntries_ARM_ARM_Cortex_X925_A725)),
            /*.paCacheEntries = */ NULL_ALONE(g_aVar0CacheEntries_ARM_ARM_Cortex_X925_A725)
            /* info: CLIDR_EL1  = 0xc3000123 - L1=i&d L2=unified L3=unified */
            /* info: CTR_EL0    = 0x9444c004 */
            /* info: DCZID_EL0: = 0x4 */
        },
        /*.Variants[1] = */
        {
            /*.pszName      = */ "ARM Cortex-X925 (Blackhawk)",
            /*.Midr         = */
            {
                /*Midr.s = */
                {
                    /*.u4Revision    = */ 0x1,
                    /*.u12PartNum    = */ 0xd85,
                    /*.u4Arch        = */ 0xf,
                    /*.u4Variant     = */ 0x0,
                    /*.u4Implementer = */ 0x41,
                }
            },
            /*.enmCoreType  = */ kCpumCoreType_Performance,
            /*.cSysRegVals    = */ ZERO_ALONE(RT_ELEMENTS(g_aVar1SysRegVals_ARM_ARM_Cortex_X925_A725)),
            /*.paSysRegVals   = */ NULL_ALONE(g_aVar1SysRegVals_ARM_ARM_Cortex_X925_A725),
            /*.uUnused        = */ 0,
            /*.cCacheEntries  = */ ZERO_ALONE(RT_ELEMENTS(g_aVar1CacheEntries_ARM_ARM_Cortex_X925_A725)),
            /*.paCacheEntries = */ NULL_ALONE(g_aVar1CacheEntries_ARM_ARM_Cortex_X925_A725)
            /* info: CLIDR_EL1  = 0xc3000123 - L1=i&d L2=unified L3=unified */
            /* info: CTR_EL0    = 0x9444c004 */
            /* info: DCZID_EL0: = 0x4 */
        },
    }
};

#endif /* !VBOX_CPUDB_ARM_ARM_Cortex_X925_A725_h */

