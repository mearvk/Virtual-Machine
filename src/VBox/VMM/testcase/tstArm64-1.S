/* $Id: tstArm64-1.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * ARM64 instruction set testcase \#1.
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


/**
 * Main entry point for the VM run.
 */
BEGINPROC_EXPORTED  MainEntrypoint
        /*
         * Set CPACR_EL1.FPEN=11 to enable the advanced SIMD fun.
         */
        mrs     x0, CPACR_EL1
        orr     x0, x0, 0x00300000
        msr     CPACR_EL1, x0

        /*
         * Now, do the same as the user-mode executable.
         */
        b       NAME(ExeEntrypoint)
ENDPROC             MainEntrypoint


/**
 * Main entry point for the user-mode exectuable.
 */
.global NAME(main)
NAME(main):
BEGINPROC_EXPORTED  ExeEntrypoint
        stp     x19, x20, [sp, #-0x60]!
        stp     x21, x22, [sp, #0x10]
        stp     x23, x24, [sp, #0x20]
        stp     x25, x26, [sp, #0x30]
        stp     x27, x28, [sp, #0x40]
        stp     x29, x30, [sp, #0x50]       /* top */

        bl      NAME(ReInitWriteArea)
        /*bl      NAME(HandcodedTests1)*/
        bl      NAME(GeneratedTests)

        ldp     x29, x30, [sp, #0x50]
        ldp     x27, x28, [sp, #0x40]
        ldp     x25, x26, [sp, #0x30]
        ldp     x23, x24, [sp, #0x20]
        ldp     x21, x22, [sp, #0x10]
        ldp     x19, x20, [sp], #0x60
        movz    x0, #0
        ret
ENDPROC             ExeEntrypoint


/**
 * Where MainEntrypoint returns to when running in a VM (see IEMR3.cpp).
 */
BEGINPROC_EXPORTED  PowerOffVm
        /** @todo figure out how to shut down the VM! */
.loop:
        brk     #0xfffe
        b       .loop
ENDPROC             PowerOffVm


BEGINPROC_EXPORTED  HandcodedTests1
        /* 1. sub w/ zero. */
        movz    x0, #0xffff
        mov     x10, x0
        sub     x1, x0, xzr
        cmp     x1, x10
        b.eq    L1
        brk     #1
L1:

        ret
ENDPROC             HandcodedTests1



/*
 * Area used in the read testing.
 */
BEGINDATA
ALIGNDATA(64)
EXPORTEDNAME g_ReadArea
NAME(g_ReadArea_00):    .byte   0x00
NAME(g_ReadArea_01):    .byte   0x01
NAME(g_ReadArea_02):    .byte   0x02
NAME(g_ReadArea_03):    .byte   0x03
NAME(g_ReadArea_04):    .byte   0x04
NAME(g_ReadArea_05):    .byte   0x05
NAME(g_ReadArea_06):    .byte   0x06
NAME(g_ReadArea_07):    .byte   0x07
NAME(g_ReadArea_08):    .byte   0x08
NAME(g_ReadArea_09):    .byte   0x09
NAME(g_ReadArea_0a):    .byte   0x0a
NAME(g_ReadArea_0b):    .byte   0x0b
NAME(g_ReadArea_0c):    .byte   0x0c
NAME(g_ReadArea_0d):    .byte   0x0d
NAME(g_ReadArea_0e):    .byte   0x0e
NAME(g_ReadArea_0f):    .byte   0x0f
NAME(g_ReadArea_10):    .byte   0x10
NAME(g_ReadArea_11):    .byte   0x11
NAME(g_ReadArea_12):    .byte   0x12
NAME(g_ReadArea_13):    .byte   0x13
NAME(g_ReadArea_14):    .byte   0x14
NAME(g_ReadArea_15):    .byte   0x15
NAME(g_ReadArea_16):    .byte   0x16
NAME(g_ReadArea_17):    .byte   0x17
NAME(g_ReadArea_18):    .byte   0x18
NAME(g_ReadArea_19):    .byte   0x19
NAME(g_ReadArea_1a):    .byte   0x1a
NAME(g_ReadArea_1b):    .byte   0x1b
NAME(g_ReadArea_1c):    .byte   0x1c
NAME(g_ReadArea_1d):    .byte   0x1d
NAME(g_ReadArea_1e):    .byte   0x1e
NAME(g_ReadArea_1f):    .byte   0x1f
NAME(g_ReadArea_20):    .byte   0x20
NAME(g_ReadArea_21):    .byte   0x21
NAME(g_ReadArea_22):    .byte   0x22
NAME(g_ReadArea_23):    .byte   0x23
NAME(g_ReadArea_24):    .byte   0x24
NAME(g_ReadArea_25):    .byte   0x25
NAME(g_ReadArea_26):    .byte   0x26
NAME(g_ReadArea_27):    .byte   0x27
NAME(g_ReadArea_28):    .byte   0x28
NAME(g_ReadArea_29):    .byte   0x29
NAME(g_ReadArea_2a):    .byte   0x2a
NAME(g_ReadArea_2b):    .byte   0x2b
NAME(g_ReadArea_2c):    .byte   0x2c
NAME(g_ReadArea_2d):    .byte   0x2d
NAME(g_ReadArea_2e):    .byte   0x2e
NAME(g_ReadArea_2f):    .byte   0x2f
NAME(g_ReadArea_30):    .byte   0x30
NAME(g_ReadArea_31):    .byte   0x31
NAME(g_ReadArea_32):    .byte   0x32
NAME(g_ReadArea_33):    .byte   0x33
NAME(g_ReadArea_34):    .byte   0x34
NAME(g_ReadArea_35):    .byte   0x35
NAME(g_ReadArea_36):    .byte   0x36
NAME(g_ReadArea_37):    .byte   0x37
NAME(g_ReadArea_38):    .byte   0x38
NAME(g_ReadArea_39):    .byte   0x39
NAME(g_ReadArea_3a):    .byte   0x3a
NAME(g_ReadArea_3b):    .byte   0x3b
NAME(g_ReadArea_3c):    .byte   0x3c
NAME(g_ReadArea_3d):    .byte   0x3d
NAME(g_ReadArea_3e):    .byte   0x3e
NAME(g_ReadArea_3f):    .byte   0x3f

NAME(g_ReadArea_40):    .byte   0x40
NAME(g_ReadArea_41):    .byte   0x41
NAME(g_ReadArea_42):    .byte   0x42
NAME(g_ReadArea_43):    .byte   0x43
NAME(g_ReadArea_44):    .byte   0x44
NAME(g_ReadArea_45):    .byte   0x45
NAME(g_ReadArea_46):    .byte   0x46
NAME(g_ReadArea_47):    .byte   0x47
NAME(g_ReadArea_48):    .byte   0x48
NAME(g_ReadArea_49):    .byte   0x49
NAME(g_ReadArea_4a):    .byte   0x4a
NAME(g_ReadArea_4b):    .byte   0x4b
NAME(g_ReadArea_4c):    .byte   0x4c
NAME(g_ReadArea_4d):    .byte   0x4d
NAME(g_ReadArea_4e):    .byte   0x4e
NAME(g_ReadArea_4f):    .byte   0x4f
NAME(g_ReadArea_50):    .byte   0x50
NAME(g_ReadArea_51):    .byte   0x51
NAME(g_ReadArea_52):    .byte   0x52
NAME(g_ReadArea_53):    .byte   0x53
NAME(g_ReadArea_54):    .byte   0x54
NAME(g_ReadArea_55):    .byte   0x55
NAME(g_ReadArea_56):    .byte   0x56
NAME(g_ReadArea_57):    .byte   0x57
NAME(g_ReadArea_58):    .byte   0x58
NAME(g_ReadArea_59):    .byte   0x59
NAME(g_ReadArea_5a):    .byte   0x5a
NAME(g_ReadArea_5b):    .byte   0x5b
NAME(g_ReadArea_5c):    .byte   0x5c
NAME(g_ReadArea_5d):    .byte   0x5d
NAME(g_ReadArea_5e):    .byte   0x5e
NAME(g_ReadArea_5f):    .byte   0x5f
NAME(g_ReadArea_60):    .byte   0x60
NAME(g_ReadArea_61):    .byte   0x61
NAME(g_ReadArea_62):    .byte   0x62
NAME(g_ReadArea_63):    .byte   0x63
NAME(g_ReadArea_64):    .byte   0x64
NAME(g_ReadArea_65):    .byte   0x65
NAME(g_ReadArea_66):    .byte   0x66
NAME(g_ReadArea_67):    .byte   0x67
NAME(g_ReadArea_68):    .byte   0x68
NAME(g_ReadArea_69):    .byte   0x69
NAME(g_ReadArea_6a):    .byte   0x6a
NAME(g_ReadArea_6b):    .byte   0x6b
NAME(g_ReadArea_6c):    .byte   0x6c
NAME(g_ReadArea_6d):    .byte   0x6d
NAME(g_ReadArea_6e):    .byte   0x6e
NAME(g_ReadArea_6f):    .byte   0x6f
NAME(g_ReadArea_70):    .byte   0x70
NAME(g_ReadArea_71):    .byte   0x71
NAME(g_ReadArea_72):    .byte   0x72
NAME(g_ReadArea_73):    .byte   0x73
NAME(g_ReadArea_74):    .byte   0x74
NAME(g_ReadArea_75):    .byte   0x75
NAME(g_ReadArea_76):    .byte   0x76
NAME(g_ReadArea_77):    .byte   0x77
NAME(g_ReadArea_78):    .byte   0x78
NAME(g_ReadArea_79):    .byte   0x79
NAME(g_ReadArea_7a):    .byte   0x7a
NAME(g_ReadArea_7b):    .byte   0x7b
NAME(g_ReadArea_7c):    .byte   0x7c
NAME(g_ReadArea_7d):    .byte   0x7d
NAME(g_ReadArea_7e):    .byte   0x7e
NAME(g_ReadArea_7f):    .byte   0x7f

NAME(g_ReadArea_80):    .byte   0x80
NAME(g_ReadArea_81):    .byte   0x81
NAME(g_ReadArea_82):    .byte   0x82
NAME(g_ReadArea_83):    .byte   0x83
NAME(g_ReadArea_84):    .byte   0x84
NAME(g_ReadArea_85):    .byte   0x85
NAME(g_ReadArea_86):    .byte   0x86
NAME(g_ReadArea_87):    .byte   0x87
NAME(g_ReadArea_88):    .byte   0x88
NAME(g_ReadArea_89):    .byte   0x89
NAME(g_ReadArea_8a):    .byte   0x8a
NAME(g_ReadArea_8b):    .byte   0x8b
NAME(g_ReadArea_8c):    .byte   0x8c
NAME(g_ReadArea_8d):    .byte   0x8d
NAME(g_ReadArea_8e):    .byte   0x8e
NAME(g_ReadArea_8f):    .byte   0x8f
NAME(g_ReadArea_90):    .byte   0x90
NAME(g_ReadArea_91):    .byte   0x91
NAME(g_ReadArea_92):    .byte   0x92
NAME(g_ReadArea_93):    .byte   0x93
NAME(g_ReadArea_94):    .byte   0x94
NAME(g_ReadArea_95):    .byte   0x95
NAME(g_ReadArea_96):    .byte   0x96
NAME(g_ReadArea_97):    .byte   0x97
NAME(g_ReadArea_98):    .byte   0x98
NAME(g_ReadArea_99):    .byte   0x99
NAME(g_ReadArea_9a):    .byte   0x9a
NAME(g_ReadArea_9b):    .byte   0x9b
NAME(g_ReadArea_9c):    .byte   0x9c
NAME(g_ReadArea_9d):    .byte   0x9d
NAME(g_ReadArea_9e):    .byte   0x9e
NAME(g_ReadArea_9f):    .byte   0x9f
NAME(g_ReadArea_a0):    .byte   0xa0
NAME(g_ReadArea_a1):    .byte   0xa1
NAME(g_ReadArea_a2):    .byte   0xa2
NAME(g_ReadArea_a3):    .byte   0xa3
NAME(g_ReadArea_a4):    .byte   0xa4
NAME(g_ReadArea_a5):    .byte   0xa5
NAME(g_ReadArea_a6):    .byte   0xa6
NAME(g_ReadArea_a7):    .byte   0xa7
NAME(g_ReadArea_a8):    .byte   0xa8
NAME(g_ReadArea_a9):    .byte   0xa9
NAME(g_ReadArea_aa):    .byte   0xaa
NAME(g_ReadArea_ab):    .byte   0xab
NAME(g_ReadArea_ac):    .byte   0xac
NAME(g_ReadArea_ad):    .byte   0xad
NAME(g_ReadArea_ae):    .byte   0xae
NAME(g_ReadArea_af):    .byte   0xaf
NAME(g_ReadArea_b0):    .byte   0xb0
NAME(g_ReadArea_b1):    .byte   0xb1
NAME(g_ReadArea_b2):    .byte   0xb2
NAME(g_ReadArea_b3):    .byte   0xb3
NAME(g_ReadArea_b4):    .byte   0xb4
NAME(g_ReadArea_b5):    .byte   0xb5
NAME(g_ReadArea_b6):    .byte   0xb6
NAME(g_ReadArea_b7):    .byte   0xb7
NAME(g_ReadArea_b8):    .byte   0xb8
NAME(g_ReadArea_b9):    .byte   0xb9
NAME(g_ReadArea_ba):    .byte   0xba
NAME(g_ReadArea_bb):    .byte   0xbb
NAME(g_ReadArea_bc):    .byte   0xbc
NAME(g_ReadArea_bd):    .byte   0xbd
NAME(g_ReadArea_be):    .byte   0xbe
NAME(g_ReadArea_bf):    .byte   0xbf

NAME(g_ReadArea_c0):    .byte   0xc0
NAME(g_ReadArea_c1):    .byte   0xc1
NAME(g_ReadArea_c2):    .byte   0xc2
NAME(g_ReadArea_c3):    .byte   0xc3
NAME(g_ReadArea_c4):    .byte   0xc4
NAME(g_ReadArea_c5):    .byte   0xc5
NAME(g_ReadArea_c6):    .byte   0xc6
NAME(g_ReadArea_c7):    .byte   0xc7
NAME(g_ReadArea_c8):    .byte   0xc8
NAME(g_ReadArea_c9):    .byte   0xc9
NAME(g_ReadArea_ca):    .byte   0xca
NAME(g_ReadArea_cb):    .byte   0xcb
NAME(g_ReadArea_cc):    .byte   0xcc
NAME(g_ReadArea_cd):    .byte   0xcd
NAME(g_ReadArea_ce):    .byte   0xce
NAME(g_ReadArea_cf):    .byte   0xcf
NAME(g_ReadArea_d0):    .byte   0xd0
NAME(g_ReadArea_d1):    .byte   0xd1
NAME(g_ReadArea_d2):    .byte   0xd2
NAME(g_ReadArea_d3):    .byte   0xd3
NAME(g_ReadArea_d4):    .byte   0xd4
NAME(g_ReadArea_d5):    .byte   0xd5
NAME(g_ReadArea_d6):    .byte   0xd6
NAME(g_ReadArea_d7):    .byte   0xd7
NAME(g_ReadArea_d8):    .byte   0xd8
NAME(g_ReadArea_d9):    .byte   0xd9
NAME(g_ReadArea_da):    .byte   0xda
NAME(g_ReadArea_db):    .byte   0xdb
NAME(g_ReadArea_dc):    .byte   0xdc
NAME(g_ReadArea_dd):    .byte   0xdd
NAME(g_ReadArea_de):    .byte   0xde
NAME(g_ReadArea_df):    .byte   0xdf
NAME(g_ReadArea_e0):    .byte   0xe0
NAME(g_ReadArea_e1):    .byte   0xe1
NAME(g_ReadArea_e2):    .byte   0xe2
NAME(g_ReadArea_e3):    .byte   0xe3
NAME(g_ReadArea_e4):    .byte   0xe4
NAME(g_ReadArea_e5):    .byte   0xe5
NAME(g_ReadArea_e6):    .byte   0xe6
NAME(g_ReadArea_e7):    .byte   0xe7
NAME(g_ReadArea_e8):    .byte   0xe8
NAME(g_ReadArea_e9):    .byte   0xe9
NAME(g_ReadArea_ea):    .byte   0xea
NAME(g_ReadArea_eb):    .byte   0xeb
NAME(g_ReadArea_ec):    .byte   0xec
NAME(g_ReadArea_ed):    .byte   0xed
NAME(g_ReadArea_ee):    .byte   0xee
NAME(g_ReadArea_ef):    .byte   0xef
NAME(g_ReadArea_f0):    .byte   0xf0
NAME(g_ReadArea_f1):    .byte   0xf1
NAME(g_ReadArea_f2):    .byte   0xf2
NAME(g_ReadArea_f3):    .byte   0xf3
NAME(g_ReadArea_f4):    .byte   0xf4
NAME(g_ReadArea_f5):    .byte   0xf5
NAME(g_ReadArea_f6):    .byte   0xf6
NAME(g_ReadArea_f7):    .byte   0xf7
NAME(g_ReadArea_f8):    .byte   0xf8
NAME(g_ReadArea_f9):    .byte   0xf9
NAME(g_ReadArea_fa):    .byte   0xfa
NAME(g_ReadArea_fb):    .byte   0xfb
NAME(g_ReadArea_fc):    .byte   0xfc
NAME(g_ReadArea_fd):    .byte   0xfd
NAME(g_ReadArea_fe):    .byte   0xfe
NAME(g_ReadArea_ff):    .byte   0xff

BEGINDATA
ALIGNDATA(64)
EXPORTEDNAME g_WriteAreaPre
        .int    0xfeedface, 0xfeedface, 0xfeedface, 0xfeedface
        .int    0xfeedface, 0xfeedface, 0xfeedface, 0xfeedface
        .int    0xfeedface, 0xfeedface, 0xfeedface, 0xfeedface
        .int    0xfeedface, 0xfeedface, 0xfeedface, 0xfeedface
EXPORTEDNAME g_WriteArea
NAME(g_WriteArea_00):   .byte   0x00
NAME(g_WriteArea_01):   .byte   0x01
NAME(g_WriteArea_02):   .byte   0x02
NAME(g_WriteArea_03):   .byte   0x03
NAME(g_WriteArea_04):   .byte   0x04
NAME(g_WriteArea_05):   .byte   0x05
NAME(g_WriteArea_06):   .byte   0x06
NAME(g_WriteArea_07):   .byte   0x07
NAME(g_WriteArea_08):   .byte   0x08
NAME(g_WriteArea_09):   .byte   0x09
NAME(g_WriteArea_0a):   .byte   0x0a
NAME(g_WriteArea_0b):   .byte   0x0b
NAME(g_WriteArea_0c):   .byte   0x0c
NAME(g_WriteArea_0d):   .byte   0x0d
NAME(g_WriteArea_0e):   .byte   0x0e
NAME(g_WriteArea_0f):   .byte   0x0f
NAME(g_WriteArea_10):   .byte   0x10
NAME(g_WriteArea_11):   .byte   0x11
NAME(g_WriteArea_12):   .byte   0x12
NAME(g_WriteArea_13):   .byte   0x13
NAME(g_WriteArea_14):   .byte   0x14
NAME(g_WriteArea_15):   .byte   0x15
NAME(g_WriteArea_16):   .byte   0x16
NAME(g_WriteArea_17):   .byte   0x17
NAME(g_WriteArea_18):   .byte   0x18
NAME(g_WriteArea_19):   .byte   0x19
NAME(g_WriteArea_1a):   .byte   0x1a
NAME(g_WriteArea_1b):   .byte   0x1b
NAME(g_WriteArea_1c):   .byte   0x1c
NAME(g_WriteArea_1d):   .byte   0x1d
NAME(g_WriteArea_1e):   .byte   0x1e
NAME(g_WriteArea_1f):   .byte   0x1f
NAME(g_WriteArea_20):   .byte   0x20
NAME(g_WriteArea_21):   .byte   0x21
NAME(g_WriteArea_22):   .byte   0x22
NAME(g_WriteArea_23):   .byte   0x23
NAME(g_WriteArea_24):   .byte   0x24
NAME(g_WriteArea_25):   .byte   0x25
NAME(g_WriteArea_26):   .byte   0x26
NAME(g_WriteArea_27):   .byte   0x27
NAME(g_WriteArea_28):   .byte   0x28
NAME(g_WriteArea_29):   .byte   0x29
NAME(g_WriteArea_2a):   .byte   0x2a
NAME(g_WriteArea_2b):   .byte   0x2b
NAME(g_WriteArea_2c):   .byte   0x2c
NAME(g_WriteArea_2d):   .byte   0x2d
NAME(g_WriteArea_2e):   .byte   0x2e
NAME(g_WriteArea_2f):   .byte   0x2f
NAME(g_WriteArea_30):   .byte   0x30
NAME(g_WriteArea_31):   .byte   0x31
NAME(g_WriteArea_32):   .byte   0x32
NAME(g_WriteArea_33):   .byte   0x33
NAME(g_WriteArea_34):   .byte   0x34
NAME(g_WriteArea_35):   .byte   0x35
NAME(g_WriteArea_36):   .byte   0x36
NAME(g_WriteArea_37):   .byte   0x37
NAME(g_WriteArea_38):   .byte   0x38
NAME(g_WriteArea_39):   .byte   0x39
NAME(g_WriteArea_3a):   .byte   0x3a
NAME(g_WriteArea_3b):   .byte   0x3b
NAME(g_WriteArea_3c):   .byte   0x3c
NAME(g_WriteArea_3d):   .byte   0x3d
NAME(g_WriteArea_3e):   .byte   0x3e
NAME(g_WriteArea_3f):   .byte   0x3f

NAME(g_WriteArea_40):   .byte   0x40
NAME(g_WriteArea_41):   .byte   0x41
NAME(g_WriteArea_42):   .byte   0x42
NAME(g_WriteArea_43):   .byte   0x43
NAME(g_WriteArea_44):   .byte   0x44
NAME(g_WriteArea_45):   .byte   0x45
NAME(g_WriteArea_46):   .byte   0x46
NAME(g_WriteArea_47):   .byte   0x47
NAME(g_WriteArea_48):   .byte   0x48
NAME(g_WriteArea_49):   .byte   0x49
NAME(g_WriteArea_4a):   .byte   0x4a
NAME(g_WriteArea_4b):   .byte   0x4b
NAME(g_WriteArea_4c):   .byte   0x4c
NAME(g_WriteArea_4d):   .byte   0x4d
NAME(g_WriteArea_4e):   .byte   0x4e
NAME(g_WriteArea_4f):   .byte   0x4f
NAME(g_WriteArea_50):   .byte   0x50
NAME(g_WriteArea_51):   .byte   0x51
NAME(g_WriteArea_52):   .byte   0x52
NAME(g_WriteArea_53):   .byte   0x53
NAME(g_WriteArea_54):   .byte   0x54
NAME(g_WriteArea_55):   .byte   0x55
NAME(g_WriteArea_56):   .byte   0x56
NAME(g_WriteArea_57):   .byte   0x57
NAME(g_WriteArea_58):   .byte   0x58
NAME(g_WriteArea_59):   .byte   0x59
NAME(g_WriteArea_5a):   .byte   0x5a
NAME(g_WriteArea_5b):   .byte   0x5b
NAME(g_WriteArea_5c):   .byte   0x5c
NAME(g_WriteArea_5d):   .byte   0x5d
NAME(g_WriteArea_5e):   .byte   0x5e
NAME(g_WriteArea_5f):   .byte   0x5f
NAME(g_WriteArea_60):   .byte   0x60
NAME(g_WriteArea_61):   .byte   0x61
NAME(g_WriteArea_62):   .byte   0x62
NAME(g_WriteArea_63):   .byte   0x63
NAME(g_WriteArea_64):   .byte   0x64
NAME(g_WriteArea_65):   .byte   0x65
NAME(g_WriteArea_66):   .byte   0x66
NAME(g_WriteArea_67):   .byte   0x67
NAME(g_WriteArea_68):   .byte   0x68
NAME(g_WriteArea_69):   .byte   0x69
NAME(g_WriteArea_6a):   .byte   0x6a
NAME(g_WriteArea_6b):   .byte   0x6b
NAME(g_WriteArea_6c):   .byte   0x6c
NAME(g_WriteArea_6d):   .byte   0x6d
NAME(g_WriteArea_6e):   .byte   0x6e
NAME(g_WriteArea_6f):   .byte   0x6f
NAME(g_WriteArea_70):   .byte   0x70
NAME(g_WriteArea_71):   .byte   0x71
NAME(g_WriteArea_72):   .byte   0x72
NAME(g_WriteArea_73):   .byte   0x73
NAME(g_WriteArea_74):   .byte   0x74
NAME(g_WriteArea_75):   .byte   0x75
NAME(g_WriteArea_76):   .byte   0x76
NAME(g_WriteArea_77):   .byte   0x77
NAME(g_WriteArea_78):   .byte   0x78
NAME(g_WriteArea_79):   .byte   0x79
NAME(g_WriteArea_7a):   .byte   0x7a
NAME(g_WriteArea_7b):   .byte   0x7b
NAME(g_WriteArea_7c):   .byte   0x7c
NAME(g_WriteArea_7d):   .byte   0x7d
NAME(g_WriteArea_7e):   .byte   0x7e
NAME(g_WriteArea_7f):   .byte   0x7f

NAME(g_WriteArea_80):   .byte   0x80
NAME(g_WriteArea_81):   .byte   0x81
NAME(g_WriteArea_82):   .byte   0x82
NAME(g_WriteArea_83):   .byte   0x83
NAME(g_WriteArea_84):   .byte   0x84
NAME(g_WriteArea_85):   .byte   0x85
NAME(g_WriteArea_86):   .byte   0x86
NAME(g_WriteArea_87):   .byte   0x87
NAME(g_WriteArea_88):   .byte   0x88
NAME(g_WriteArea_89):   .byte   0x89
NAME(g_WriteArea_8a):   .byte   0x8a
NAME(g_WriteArea_8b):   .byte   0x8b
NAME(g_WriteArea_8c):   .byte   0x8c
NAME(g_WriteArea_8d):   .byte   0x8d
NAME(g_WriteArea_8e):   .byte   0x8e
NAME(g_WriteArea_8f):   .byte   0x8f
NAME(g_WriteArea_90):   .byte   0x90
NAME(g_WriteArea_91):   .byte   0x91
NAME(g_WriteArea_92):   .byte   0x92
NAME(g_WriteArea_93):   .byte   0x93
NAME(g_WriteArea_94):   .byte   0x94
NAME(g_WriteArea_95):   .byte   0x95
NAME(g_WriteArea_96):   .byte   0x96
NAME(g_WriteArea_97):   .byte   0x97
NAME(g_WriteArea_98):   .byte   0x98
NAME(g_WriteArea_99):   .byte   0x99
NAME(g_WriteArea_9a):   .byte   0x9a
NAME(g_WriteArea_9b):   .byte   0x9b
NAME(g_WriteArea_9c):   .byte   0x9c
NAME(g_WriteArea_9d):   .byte   0x9d
NAME(g_WriteArea_9e):   .byte   0x9e
NAME(g_WriteArea_9f):   .byte   0x9f
NAME(g_WriteArea_a0):   .byte   0xa0
NAME(g_WriteArea_a1):   .byte   0xa1
NAME(g_WriteArea_a2):   .byte   0xa2
NAME(g_WriteArea_a3):   .byte   0xa3
NAME(g_WriteArea_a4):   .byte   0xa4
NAME(g_WriteArea_a5):   .byte   0xa5
NAME(g_WriteArea_a6):   .byte   0xa6
NAME(g_WriteArea_a7):   .byte   0xa7
NAME(g_WriteArea_a8):   .byte   0xa8
NAME(g_WriteArea_a9):   .byte   0xa9
NAME(g_WriteArea_aa):   .byte   0xaa
NAME(g_WriteArea_ab):   .byte   0xab
NAME(g_WriteArea_ac):   .byte   0xac
NAME(g_WriteArea_ad):   .byte   0xad
NAME(g_WriteArea_ae):   .byte   0xae
NAME(g_WriteArea_af):   .byte   0xaf
NAME(g_WriteArea_b0):   .byte   0xb0
NAME(g_WriteArea_b1):   .byte   0xb1
NAME(g_WriteArea_b2):   .byte   0xb2
NAME(g_WriteArea_b3):   .byte   0xb3
NAME(g_WriteArea_b4):   .byte   0xb4
NAME(g_WriteArea_b5):   .byte   0xb5
NAME(g_WriteArea_b6):   .byte   0xb6
NAME(g_WriteArea_b7):   .byte   0xb7
NAME(g_WriteArea_b8):   .byte   0xb8
NAME(g_WriteArea_b9):   .byte   0xb9
NAME(g_WriteArea_ba):   .byte   0xba
NAME(g_WriteArea_bb):   .byte   0xbb
NAME(g_WriteArea_bc):   .byte   0xbc
NAME(g_WriteArea_bd):   .byte   0xbd
NAME(g_WriteArea_be):   .byte   0xbe
NAME(g_WriteArea_bf):   .byte   0xbf

NAME(g_WriteArea_c0):   .byte   0xc0
NAME(g_WriteArea_c1):   .byte   0xc1
NAME(g_WriteArea_c2):   .byte   0xc2
NAME(g_WriteArea_c3):   .byte   0xc3
NAME(g_WriteArea_c4):   .byte   0xc4
NAME(g_WriteArea_c5):   .byte   0xc5
NAME(g_WriteArea_c6):   .byte   0xc6
NAME(g_WriteArea_c7):   .byte   0xc7
NAME(g_WriteArea_c8):   .byte   0xc8
NAME(g_WriteArea_c9):   .byte   0xc9
NAME(g_WriteArea_ca):   .byte   0xca
NAME(g_WriteArea_cb):   .byte   0xcb
NAME(g_WriteArea_cc):   .byte   0xcc
NAME(g_WriteArea_cd):   .byte   0xcd
NAME(g_WriteArea_ce):   .byte   0xce
NAME(g_WriteArea_cf):   .byte   0xcf
NAME(g_WriteArea_d0):   .byte   0xd0
NAME(g_WriteArea_d1):   .byte   0xd1
NAME(g_WriteArea_d2):   .byte   0xd2
NAME(g_WriteArea_d3):   .byte   0xd3
NAME(g_WriteArea_d4):   .byte   0xd4
NAME(g_WriteArea_d5):   .byte   0xd5
NAME(g_WriteArea_d6):   .byte   0xd6
NAME(g_WriteArea_d7):   .byte   0xd7
NAME(g_WriteArea_d8):   .byte   0xd8
NAME(g_WriteArea_d9):   .byte   0xd9
NAME(g_WriteArea_da):   .byte   0xda
NAME(g_WriteArea_db):   .byte   0xdb
NAME(g_WriteArea_dc):   .byte   0xdc
NAME(g_WriteArea_dd):   .byte   0xdd
NAME(g_WriteArea_de):   .byte   0xde
NAME(g_WriteArea_df):   .byte   0xdf
NAME(g_WriteArea_e0):   .byte   0xe0
NAME(g_WriteArea_e1):   .byte   0xe1
NAME(g_WriteArea_e2):   .byte   0xe2
NAME(g_WriteArea_e3):   .byte   0xe3
NAME(g_WriteArea_e4):   .byte   0xe4
NAME(g_WriteArea_e5):   .byte   0xe5
NAME(g_WriteArea_e6):   .byte   0xe6
NAME(g_WriteArea_e7):   .byte   0xe7
NAME(g_WriteArea_e8):   .byte   0xe8
NAME(g_WriteArea_e9):   .byte   0xe9
NAME(g_WriteArea_ea):   .byte   0xea
NAME(g_WriteArea_eb):   .byte   0xeb
NAME(g_WriteArea_ec):   .byte   0xec
NAME(g_WriteArea_ed):   .byte   0xed
NAME(g_WriteArea_ee):   .byte   0xee
NAME(g_WriteArea_ef):   .byte   0xef
NAME(g_WriteArea_f0):   .byte   0xf0
NAME(g_WriteArea_f1):   .byte   0xf1
NAME(g_WriteArea_f2):   .byte   0xf2
NAME(g_WriteArea_f3):   .byte   0xf3
NAME(g_WriteArea_f4):   .byte   0xf4
NAME(g_WriteArea_f5):   .byte   0xf5
NAME(g_WriteArea_f6):   .byte   0xf6
NAME(g_WriteArea_f7):   .byte   0xf7
NAME(g_WriteArea_f8):   .byte   0xf8
NAME(g_WriteArea_f9):   .byte   0xf9
NAME(g_WriteArea_fa):   .byte   0xfa
NAME(g_WriteArea_fb):   .byte   0xfb
NAME(g_WriteArea_fc):   .byte   0xfc
NAME(g_WriteArea_fd):   .byte   0xfd
NAME(g_WriteArea_fe):   .byte   0xfe
NAME(g_WriteArea_ff):   .byte   0xff

EXPORTEDNAME g_WriteAreaPost
        .int    0x8badf00d, 0x8badf00d, 0x8badf00d, 0x8badf00d
        .int    0x8badf00d, 0x8badf00d, 0x8badf00d, 0x8badf00d
        .int    0x8badf00d, 0x8badf00d, 0x8badf00d, 0x8badf00d
        .int    0x8badf00d, 0x8badf00d, 0x8badf00d, 0x8badf00d

BEGINCODE


/**
 * This checks that the write area is pristine.
 *
 * Note! SP is busted. We can trash x0-x5 and NZCV, but nothing else here.
 */
BEGINPROC_EXPORTED  CheckWriteArea
        adrp    x3, PAGE(NAME(g_WriteAreaPre))
        add     x3, x3, PAGEOFF(NAME(g_WriteAreaPre))

        /*
         * Check the first 32 bytes of the pre-area (0xfeedface).
         */
        movz    x0, #0xface
        movk    x0, #0xfeed, LSL #16
        movk    x0, #0xface, LSL #32
        movk    x0, #0xfeed, LSL #48

        movz    x1, #4

.Lcheck_loop_pre:
        ldp     x4, x5, [x3], #16
        cmp     x4, x0
        b.eq    .Lcheck_loop_pre_t1
        brk     #0xffd0
.Lcheck_loop_pre_t1:
        cmp     x5, x0
        b.eq    .Lcheck_loop_pre_t2
        brk     #0xffd1
.Lcheck_loop_pre_t2:
        subs    x1, x1, #1
        b.ne    .Lcheck_loop_pre

        /*
         * The main body.
         */
        movz    x0, #0x0100
        movk    x0, #0x0302, LSL #16
        movk    x0, #0x0504, LSL #32
        movk    x0, #0x0706, LSL #48

        movz    x1, #0x0908
        movk    x1, #0x0b0a, LSL #16
        movk    x1, #0x0d0c, LSL #32
        movk    x1, #0x0f0e, LSL #48

        movz    x2, #0x1010
        movk    x2, #0x1010, LSL #16
        movk    x2, #0x1010, LSL #32
        movk    x2, #0x1010, LSL #48

.Lcheck_loop:
        ldp     x4, x5, [x3], #16
        cmp     x4, x0
        b.eq    .Lcheck_loop_t1
        brk     #0xffd4
.Lcheck_loop_t1:
        cmp     x5, x1
        b.eq    .Lcheck_loop_t2
        brk     #0xffd5
.Lcheck_loop_t2:
        add     x0, x0, x2
        add     x1, x1, x2
        tst     x0, #0xff
        b.ne    .Lcheck_loop

        /*
         * Check the post-area (0x8badf00d).
         */
        movz    x0, #0xf00d
        movk    x0, #0x8bad, LSL #16
        movk    x0, #0xf00d, LSL #32
        movk    x0, #0x8bad, LSL #48

        movz    x1, #4

.Lcheck_loop_post:
        ldp     x4, x5, [x3], #16
        cmp     x4, x0
        b.eq    .Lcheck_loop_post_t1
        brk     #0xffd8
.Lcheck_loop_post_t1:
        cmp     x5, x0
        b.eq    .Lcheck_loop_post_t2
        brk     #0xffd8
.Lcheck_loop_post_t2:
        subs    x1, x1, #1
        b.ne    .Lcheck_loop_post

        ret
ENDPROC             CheckWriteArea


BEGINPROC_EXPORTED  ReInitWriteArea
        movz    x0, #0x0100
        movk    x0, #0x0302, LSL #16
        movk    x0, #0x0504, LSL #32
        movk    x0, #0x0706, LSL #48

        movz    x1, #0x0908
        movk    x1, #0x0b0a, LSL #16
        movk    x1, #0x0d0c, LSL #32
        movk    x1, #0x0f0e, LSL #48

        movz    x2, #0x1010
        movk    x2, #0x1010, LSL #16
        movk    x2, #0x1010, LSL #32
        movk    x2, #0x1010, LSL #48

        adrp    x3, PAGE(NAME(g_WriteArea))
        add     x3, x3, PAGEOFF(NAME(g_WriteArea))

.Lreinit_loop:
        stp     x0, x1, [x3], #16
        add     x0, x0, x2
        add     x1, x1, x2
        tst     x0, #0xff
        b.ne    .Lreinit_loop

        ret
ENDPROC             ReInitWriteArea


#include "tstArm64-1-generated.S"

