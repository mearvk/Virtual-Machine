/* $Id: ARM_Apple_M2_Max.h 113643 2026-03-30 10:31:43Z knut.osmundsen@oracle.com $ */
/** @file
 * CPU database entry "Apple M2 Max".
 * Generated at 2026-01-31T03:49:37Z by VBoxCpuReport v7.2.97r172494 on darwin.arm64.
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

#ifndef VBOX_CPUDB_ARM_Apple_M2_Max_h
#define VBOX_CPUDB_ARM_Apple_M2_Max_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/**
 * Common system register values for Apple M2 Max.
 */
static SUPARMSYSREGVAL const g_aCmnSysRegVals_ARM_Apple_M2_Max[] =
{
    { UINT64_C(0x0000000080000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 5), 0x1 }, /* MPIDR_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 6), 0x0 }, /* REVIDR_EL1 */
    { UINT64_C(0x1101000010110111), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 0), 0x0 }, /* ID_AA64PFR0_EL1 */
    { UINT64_C(0x0000000000000021), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 1), 0x0 }, /* ID_AA64PFR1_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 2), 0x0 }, /* ID_AA64PFR2_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 3), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 4), 0x0 }, /* ID_AA64ZFR0_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 5), 0x0 }, /* ID_AA64SMFR0_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 6), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 4, 7), 0x0 }, /* ID_AA64FPFR0_EL1 */
    { UINT64_C(0x0000000010305f09), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 0), 0x0 }, /* ID_AA64DFR0_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 1), 0x0 }, /* ID_AA64DFR1_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 2), 0x0 }, /* ID_AA64DFR2_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 3), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 4), 0x0 }, /* ID_AA64AFR0_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 5), 0x0 }, /* ID_AA64AFR1_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 6), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 5, 7), 0x0 },
    { UINT64_C(0x0221100110212120), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 0), 0x0 }, /* ID_AA64ISAR0_EL1 */
    { UINT64_C(0x0010111110211402), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 1), 0x0 }, /* ID_AA64ISAR1_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 2), 0x0 }, /* ID_AA64ISAR2_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 3), 0x0 }, /* ID_AA64ISAR3_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 4), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 5), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 6), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 6, 7), 0x0 },
    { UINT64_C(0x110012120f100003), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 0), 0x0 }, /* ID_AA64MMFR0_EL1 */
    { UINT64_C(0x0000001111212100), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 1), 0x0 }, /* ID_AA64MMFR1_EL1 */
    { UINT64_C(0x1201111102001011), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 2), 0x0 }, /* ID_AA64MMFR2_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 3), 0x0 }, /* ID_AA64MMFR3_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 4), 0x0 }, /* ID_AA64MMFR4_EL1 */
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 5), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 6), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 7, 7), 0x0 },
    { UINT64_C(0x0000000000000000), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 5, 3, 0), 0x0 }, /* ERRIDR_EL1 */
    { UINT64_C(0x0000000081000023), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 1, 0, 0, 1), 0x0 }, /* CLIDR_EL1 */
    { UINT64_C(0x000000006afd5797), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 1, 0, 0, 7), 0x0 }, /* AIDR_EL1 */
    { UINT64_C(0x000000008444c004), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 3, 0, 0, 1), 0x0 }, /* CTR_EL0 */
    { UINT64_C(0x0000000000000004), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 3, 0, 0, 7), 0x0 }, /* DCZID_EL0 */
    { UINT64_C(0x00000000016e3600), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 3,14, 0, 0), 0x0 }, /* CNTFRQ_EL0 */
};


/**
 * System register values for Apple M2 Max (Blizzard), variation #0.
 * 4 CPUs shares this variant: 0, 1, 2, 3
 */
static SUPARMSYSREGVAL const g_aVar0SysRegVals_ARM_Apple_M2_Max[] =
{
    { UINT64_C(0x00000000611f0380), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 0), 0x0 }, /* MIDR_EL1 */
};


/**
 * System register values for Apple M2 Max (Blizzard), variation #0.
 * 4 CPUs shares this variant: 0, 1, 2, 3
 */
static SUPARMCACHELEVEL const g_aVar0CacheEntries_ARM_Apple_M2_Max[] =
{
    { 0x00, {0,0,0}, 0, UINT64_C(0x0000700fe03a), UINT64_MAX }, /* CSSEL=L0-data cbLine=64  cSets=128   Asc=8  (64 KiB) */
    { 0x01, {0,0,0}, 0, UINT64_C(0x0000203fe01a), UINT64_MAX }, /* CSSEL=L0-inst cbLine=64  cSets=512   Asc=4  (128 KiB) */
    { 0x02, {0,0,0}, 0, UINT64_C(0x000070ffe07b), UINT64_MAX }, /* CSSEL=L1-unif cbLine=128 cSets=2048  Asc=16 (4 MiB) */
};


/**
 * System register values for Apple M2 Max (Avalanche), variation #1.
 * 8 CPUs shares this variant: 4, 5, 6, 7, 8, 9, 10, 11
 */
static SUPARMSYSREGVAL const g_aVar1SysRegVals_ARM_Apple_M2_Max[] =
{
    { UINT64_C(0x00000000611f0390), ARMV8_AARCH64_SYSREG_ID_CREATE(3, 0, 0, 0, 0), 0x0 }, /* MIDR_EL1 */
};


/**
 * System register values for Apple M2 Max (Avalanche), variation #1.
 * 8 CPUs shares this variant: 4, 5, 6, 7, 8, 9, 10, 11
 */
static SUPARMCACHELEVEL const g_aVar1CacheEntries_ARM_Apple_M2_Max[] =
{
    { 0x00, {0,0,0}, 0, UINT64_C(0x0000701fe03a), UINT64_MAX }, /* CSSEL=L0-data cbLine=64  cSets=256   Asc=8  (128 KiB) */
    { 0x01, {0,0,0}, 0, UINT64_C(0x0000203fe02a), UINT64_MAX }, /* CSSEL=L0-inst cbLine=64  cSets=512   Asc=6  (192 KiB) */
    { 0x02, {0,0,0}, 0, UINT64_C(0x000073ffe07b), UINT64_MAX }, /* CSSEL=L1-unif cbLine=128 cSets=8192  Asc=16 (16 MiB) */
};


/**
 * Database entry for Apple M2 Max.
 */
static CPUMDBENTRYARM const g_Entry_ARM_Apple_M2_Max =
{
    {
        /*.pszName      = */ "Apple M2 Max",
        /*.pszFullName  = */ "Apple M2 Max",
        /*.enmVendor    = */ CPUMCPUVENDOR_APPLE,
        /*.enmMicroarch = */ kCpumMicroarch_Apple_M2,
        /*.fFlags       = */ 0,
        /*.enmEntryType = */ CPUMDBENTRYTYPE_ARM,
    },
    /*.paSysRegCmnVals  = */ NULL_ALONE(g_aCmnSysRegVals_ARM_Apple_M2_Max),
    /*.cSysRegCmnVals   = */ ZERO_ALONE(RT_ELEMENTS(g_aCmnSysRegVals_ARM_Apple_M2_Max)),
    /*.cVariants        = */ 2,
    /*.aVariants        = */
    {
        /*.Variants[0] = */
        {
            /*.pszName      = */ "Apple M2 Max (Blizzard)",
            /*.Midr         = */
            {
                /*Midr.s = */
                {
                    /*.u4Revision    = */ 0x0,
                    /*.u12PartNum    = */ 0x038,
                    /*.u4Arch        = */ 0xf,
                    /*.u4Variant     = */ 0x1,
                    /*.u4Implementer = */ 0x61,
                }
            },
            /*.enmCoreType    = */ kCpumCoreType_Efficiency,
            /*.cSysRegVals    = */ ZERO_ALONE(RT_ELEMENTS(g_aVar0SysRegVals_ARM_Apple_M2_Max)),
            /*.paSysRegVals   = */ NULL_ALONE(g_aVar0SysRegVals_ARM_Apple_M2_Max),
            /*.uUnused        = */ 0,
            /*.cCacheEntries  = */ ZERO_ALONE(RT_ELEMENTS(g_aVar0CacheEntries_ARM_Apple_M2_Max)),
            /*.paCacheEntries = */ NULL_ALONE(g_aVar0CacheEntries_ARM_Apple_M2_Max)
            /* info: CLIDR_EL1  = 0x81000023 - L1=i&d L2=unified */
            /* info: CTR_EL0    = 0x8444c004 */
            /* info: DCZID_EL0: = 0x4 */
        },
        /*.Variants[1] = */
        {
            /*.pszName      = */ "Apple M2 Max (Avalanche)",
            /*.Midr         = */
            {
                /*Midr.s = */
                {
                    /*.u4Revision    = */ 0x0,
                    /*.u12PartNum    = */ 0x039,
                    /*.u4Arch        = */ 0xf,
                    /*.u4Variant     = */ 0x1,
                    /*.u4Implementer = */ 0x61,
                }
            },
            /*.enmCoreType    = */ kCpumCoreType_Performance,
            /*.cSysRegVals    = */ ZERO_ALONE(RT_ELEMENTS(g_aVar1SysRegVals_ARM_Apple_M2_Max)),
            /*.paSysRegVals   = */ NULL_ALONE(g_aVar1SysRegVals_ARM_Apple_M2_Max),
            /*.uUnused        = */ 0,
            /*.cCacheEntries  = */ ZERO_ALONE(RT_ELEMENTS(g_aVar1CacheEntries_ARM_Apple_M2_Max)),
            /*.paCacheEntries = */ NULL_ALONE(g_aVar1CacheEntries_ARM_Apple_M2_Max)
            /* info: CLIDR_EL1  = 0x81000023 - L1=i&d L2=unified */
            /* info: CTR_EL0    = 0x8444c004 */
            /* info: DCZID_EL0: = 0x4 */
        },
    }
};

#endif /* !VBOX_CPUDB_ARM_Apple_M2_Max_h */

