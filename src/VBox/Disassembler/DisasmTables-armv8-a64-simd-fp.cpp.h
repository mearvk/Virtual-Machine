/* $Id: DisasmTables-armv8-a64-simd-fp.cpp.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - SIMD & FP.
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


DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdThreeDifferent_U0)
    /** @todo    */
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          0,  5, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdThreeDifferent_U0)
  /*opcode=0000*/ INVALID_OPCODE, /** @todo */
  /*opcode=0001*/ INVALID_OPCODE, /** @todo */
  /*opcode=0010*/ INVALID_OPCODE, /** @todo */
  /*opcode=0011*/ INVALID_OPCODE, /** @todo */
  /*opcode=0100*/ INVALID_OPCODE, /** @todo */
  /*opcode=0101*/ INVALID_OPCODE, /** @todo */
  /*opcode=0110*/ INVALID_OPCODE, /** @todo */
  /*opcode=0111*/ INVALID_OPCODE, /** @todo */
  /*opcode=1000*/ INVALID_OPCODE, /** @todo */
  /*opcode=1001*/ INVALID_OPCODE, /** @todo */
  /*opcode=1010*/ INVALID_OPCODE, /** @todo */
  /*opcode=1011*/ INVALID_OPCODE, /** @todo */
  /*opcode=1100*/ INVALID_OPCODE, /** @todo */
  /*opcode=1101*/ INVALID_OPCODE, /** @todo */
  /*opcode=1110*/ INVALID_OPCODE, /** @todo */
  /*opcode=1111*/ INVALID_OPCODE, /** @todo */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdThreeDifferent_U0, 0x00000000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeLookup, UINT32_C(0x0000f000), 12);

DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdThreeDifferent_U1)
    /** @todo    */
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          0,  5, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdThreeDifferent_U1)
  /*opcode=0000*/ INVALID_OPCODE, /** @todo */
  /*opcode=0001*/ INVALID_OPCODE, /** @todo */
  /*opcode=0010*/ INVALID_OPCODE, /** @todo */
  /*opcode=0011*/ INVALID_OPCODE, /** @todo */
  /*opcode=0100*/ INVALID_OPCODE, /** @todo */
  /*opcode=0101*/ INVALID_OPCODE, /** @todo */
  /*opcode=0110*/ INVALID_OPCODE, /** @todo */
  /*opcode=0111*/ INVALID_OPCODE, /** @todo */
  /*opcode=1000*/ INVALID_OPCODE, /** @todo */
  /*opcode=1001*/ INVALID_OPCODE, /** @todo */
  /*opcode=1010*/ INVALID_OPCODE, /** @todo */
  /*opcode=1011*/ INVALID_OPCODE, /** @todo */
  /*opcode=1100*/ INVALID_OPCODE, /** @todo */
  /*opcode=1101*/ INVALID_OPCODE, /** @todo */
  /*opcode=1110*/ INVALID_OPCODE, /** @todo */
  /*opcode=1111*/ INVALID_OPCODE, /** @todo */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdThreeDifferent_U1, 0x00000000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeLookup, UINT32_C(0x0000f000), 12);

/*
 * C4.1.95.22 - Data Processing - Advanced SIMD across lanes
 *
 * U0: op0=0x00 op1=0x op2=x11x op3=xxxxxxxx0; select on op3[1].
 * U1: op0=0x10 op1=0x op2=x11x op3=xxxxxxxx0; select on op3[1].
 *
 * The F* instructions forces us to select on the size mask ask well as the opcode.
 */

DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdAcrossLanes_U0)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegElemSize, 22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,           30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSizeHSD, 22,  2, 0),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,   0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcAdvSimdAcrossLanes_U0_BHS)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegElemSize, 22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,           30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize,    22,  2, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,   0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcAdvSimdAcrossLanes_U0_Half)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegTypeFixed, kDisOpParamArmV8VecRegType_4H,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,           30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpTypeFixed,    kDisArmv8InstrFpType_Half,       0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdAcrossLanes_U0)
  /*size=00/opcode=00000*/ INVALID_OPCODE,
  /*size=00/opcode=00001*/ INVALID_OPCODE,
  /*size=00/opcode=00010*/ INVALID_OPCODE,
  /*size=00/opcode=00011*/ DIS_ARMV8_OP(           0x00000000, "saddlv",    OP_ARMV8_A64_SADDLV),
  /*size=00/opcode=00100*/ INVALID_OPCODE,
  /*size=00/opcode=00101*/ INVALID_OPCODE,
  /*size=00/opcode=00110*/ INVALID_OPCODE,
  /*size=00/opcode=00111*/ INVALID_OPCODE,
  /*size=00/opcode=01000*/ INVALID_OPCODE,
  /*size=00/opcode=01001*/ INVALID_OPCODE,
  /*size=00/opcode=01010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "smaxv",     OP_ARMV8_A64_SMAXV,     DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=00/opcode=01011*/ INVALID_OPCODE,
  /*size=00/opcode=01100*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fmaxnmv",   OP_ARMV8_A64_FMAXNMV,   DataProcAdvSimdAcrossLanes_U0_Half),
  /*size=00/opcode=01101*/ INVALID_OPCODE,
  /*size=00/opcode=01110*/ INVALID_OPCODE,
  /*size=00/opcode=01111*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fmaxv",     OP_ARMV8_A64_FMAXV,     DataProcAdvSimdAcrossLanes_U0_Half),
  /*size=00/opcode=10000*/ INVALID_OPCODE,
  /*size=00/opcode=10001*/ INVALID_OPCODE,
  /*size=00/opcode=10010*/ INVALID_OPCODE,
  /*size=00/opcode=10011*/ INVALID_OPCODE,
  /*size=00/opcode=10100*/ INVALID_OPCODE,
  /*size=00/opcode=10101*/ INVALID_OPCODE,
  /*size=00/opcode=10110*/ INVALID_OPCODE,
  /*size=00/opcode=10111*/ INVALID_OPCODE,
  /*size=00/opcode=11000*/ INVALID_OPCODE,
  /*size=00/opcode=11001*/ INVALID_OPCODE,
  /*size=00/opcode=11010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "sminv",     OP_ARMV8_A64_SMINV,     DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=00/opcode=11011*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "addv",      OP_ARMV8_A64_ADDV,      DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=00/opcode=11100*/ INVALID_OPCODE,
  /*size=00/opcode=11101*/ INVALID_OPCODE,
  /*size=00/opcode=11110*/ INVALID_OPCODE,
  /*size=00/opcode=11111*/ INVALID_OPCODE,
  /*size=01/opcode=00000*/ INVALID_OPCODE,
  /*size=01/opcode=00001*/ INVALID_OPCODE,
  /*size=01/opcode=00010*/ INVALID_OPCODE,
  /*size=01/opcode=00011*/ DIS_ARMV8_OP(           0x00000000, "saddlv",    OP_ARMV8_A64_SADDLV),
  /*size=01/opcode=00100*/ INVALID_OPCODE,
  /*size=01/opcode=00101*/ INVALID_OPCODE,
  /*size=01/opcode=00110*/ INVALID_OPCODE,
  /*size=01/opcode=00111*/ INVALID_OPCODE,
  /*size=01/opcode=01000*/ INVALID_OPCODE,
  /*size=01/opcode=01001*/ INVALID_OPCODE,
  /*size=01/opcode=01010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "smaxv",     OP_ARMV8_A64_SMAXV,     DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=01/opcode=01011*/ INVALID_OPCODE,
  /*size=01/opcode=01100*/ INVALID_OPCODE,
  /*size=01/opcode=01101*/ INVALID_OPCODE,
  /*size=01/opcode=01110*/ INVALID_OPCODE,
  /*size=01/opcode=01111*/ INVALID_OPCODE,
  /*size=01/opcode=10000*/ INVALID_OPCODE,
  /*size=01/opcode=10001*/ INVALID_OPCODE,
  /*size=01/opcode=10010*/ INVALID_OPCODE,
  /*size=01/opcode=10011*/ INVALID_OPCODE,
  /*size=01/opcode=10100*/ INVALID_OPCODE,
  /*size=01/opcode=10101*/ INVALID_OPCODE,
  /*size=01/opcode=10110*/ INVALID_OPCODE,
  /*size=01/opcode=10111*/ INVALID_OPCODE,
  /*size=01/opcode=11000*/ INVALID_OPCODE,
  /*size=01/opcode=11001*/ INVALID_OPCODE,
  /*size=01/opcode=11010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "sminv",     OP_ARMV8_A64_SMINV,     DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=01/opcode=11011*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "addv",      OP_ARMV8_A64_ADDV,      DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=01/opcode=11100*/ INVALID_OPCODE,
  /*size=01/opcode=11101*/ INVALID_OPCODE,
  /*size=01/opcode=11110*/ INVALID_OPCODE,
  /*size=01/opcode=11111*/ INVALID_OPCODE,
  /*size=10/opcode=00000*/ INVALID_OPCODE,
  /*size=10/opcode=00001*/ INVALID_OPCODE,
  /*size=10/opcode=00010*/ INVALID_OPCODE,
  /*size=10/opcode=00011*/ DIS_ARMV8_OP(           0x00000000, "saddlv",    OP_ARMV8_A64_SADDLV),
  /*size=10/opcode=00100*/ INVALID_OPCODE,
  /*size=10/opcode=00101*/ INVALID_OPCODE,
  /*size=10/opcode=00110*/ INVALID_OPCODE,
  /*size=10/opcode=00111*/ INVALID_OPCODE,
  /*size=10/opcode=01000*/ INVALID_OPCODE,
  /*size=10/opcode=01001*/ INVALID_OPCODE,
  /*size=10/opcode=01010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "smaxv",     OP_ARMV8_A64_SMAXV,     DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=10/opcode=01011*/ INVALID_OPCODE,
  /*size=10/opcode=01100*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fminnmv",   OP_ARMV8_A64_FMINNMV,   DataProcAdvSimdAcrossLanes_U0_Half),
  /*size=10/opcode=01101*/ INVALID_OPCODE,
  /*size=10/opcode=01110*/ INVALID_OPCODE,
  /*size=10/opcode=01111*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fminv",     OP_ARMV8_A64_FMINV,     DataProcAdvSimdAcrossLanes_U0_Half),
  /*size=10/opcode=10000*/ INVALID_OPCODE,
  /*size=10/opcode=10001*/ INVALID_OPCODE,
  /*size=10/opcode=10010*/ INVALID_OPCODE,
  /*size=10/opcode=10011*/ INVALID_OPCODE,
  /*size=10/opcode=10100*/ INVALID_OPCODE,
  /*size=10/opcode=10101*/ INVALID_OPCODE,
  /*size=10/opcode=10110*/ INVALID_OPCODE,
  /*size=10/opcode=10111*/ INVALID_OPCODE,
  /*size=10/opcode=11000*/ INVALID_OPCODE,
  /*size=10/opcode=11001*/ INVALID_OPCODE,
  /*size=10/opcode=11010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "sminv",     OP_ARMV8_A64_SMINV,     DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=10/opcode=11011*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "addv",      OP_ARMV8_A64_ADDV,      DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=10/opcode=11100*/ INVALID_OPCODE,
  /*size=10/opcode=11101*/ INVALID_OPCODE,
  /*size=10/opcode=11110*/ INVALID_OPCODE,
  /*size=10/opcode=11111*/ INVALID_OPCODE,
  /*size=11/opcode=00000*/ INVALID_OPCODE,
  /*size=11/opcode=00001*/ INVALID_OPCODE,
  /*size=11/opcode=00010*/ INVALID_OPCODE,
  /*size=11/opcode=00011*/ DIS_ARMV8_OP(           0x00000000, "saddlv",    OP_ARMV8_A64_SADDLV),
  /*size=11/opcode=00100*/ INVALID_OPCODE,
  /*size=11/opcode=00101*/ INVALID_OPCODE,
  /*size=11/opcode=00110*/ INVALID_OPCODE,
  /*size=11/opcode=00111*/ INVALID_OPCODE,
  /*size=11/opcode=01000*/ INVALID_OPCODE,
  /*size=11/opcode=01001*/ INVALID_OPCODE,
  /*size=11/opcode=01010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "smaxv",     OP_ARMV8_A64_SMAXV,     DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=11/opcode=01011*/ INVALID_OPCODE,
  /*size=11/opcode=01100*/ INVALID_OPCODE,
  /*size=11/opcode=01101*/ INVALID_OPCODE,
  /*size=11/opcode=01110*/ INVALID_OPCODE,
  /*size=11/opcode=01111*/ INVALID_OPCODE,
  /*size=11/opcode=10000*/ INVALID_OPCODE,
  /*size=11/opcode=10001*/ INVALID_OPCODE,
  /*size=11/opcode=10010*/ INVALID_OPCODE,
  /*size=11/opcode=10011*/ INVALID_OPCODE,
  /*size=11/opcode=10100*/ INVALID_OPCODE,
  /*size=11/opcode=10101*/ INVALID_OPCODE,
  /*size=11/opcode=10110*/ INVALID_OPCODE,
  /*size=11/opcode=10111*/ INVALID_OPCODE,
  /*size=11/opcode=11000*/ INVALID_OPCODE,
  /*size=11/opcode=11001*/ INVALID_OPCODE,
  /*size=11/opcode=11010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "sminv",     OP_ARMV8_A64_SMINV,     DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=11/opcode=11011*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "addv",      OP_ARMV8_A64_ADDV,      DataProcAdvSimdAcrossLanes_U0_BHS),
  /*size=11/opcode=11100*/ INVALID_OPCODE,
  /*size=11/opcode=11101*/ INVALID_OPCODE,
  /*size=11/opcode=11110*/ INVALID_OPCODE,
  /*size=11/opcode=11111*/ INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdAcrossLanes_U0, 0x00000000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate, UINT32_C(0x00c1f000), 12);

DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdAcrossLanes_U1)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegElemSize, 22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,           30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSizeHSD, 22,  2, 0),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,   0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcAdvSimdAcrossLanes_U1_BHS)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegElemSize, 22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,           30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize,    22,  2, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,   0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcAdvSimdAcrossLanes_U1_Single)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegTypeFixed, kDisOpParamArmV8VecRegType_4H,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,           30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpTypeFixed,    kDisArmv8InstrFpType_Single,     0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdAcrossLanes_U1)
  /*size=00/opcode=00000*/ INVALID_OPCODE,
  /*size=00/opcode=00001*/ INVALID_OPCODE,
  /*size=00/opcode=00010*/ INVALID_OPCODE,
  /*size=00/opcode=00011*/ DIS_ARMV8_OP(           0x00000000, "uaddlv",    OP_ARMV8_A64_UADDLV),
  /*size=00/opcode=00100*/ INVALID_OPCODE,
  /*size=00/opcode=00101*/ INVALID_OPCODE,
  /*size=00/opcode=00110*/ INVALID_OPCODE,
  /*size=00/opcode=00111*/ INVALID_OPCODE,
  /*size=00/opcode=01000*/ INVALID_OPCODE,
  /*size=00/opcode=01001*/ INVALID_OPCODE,
  /*size=00/opcode=01010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "umaxv",     OP_ARMV8_A64_UMAXV,     DataProcAdvSimdAcrossLanes_U1_BHS),
  /*size=00/opcode=01011*/ INVALID_OPCODE,
  /*size=00/opcode=01100*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fmaxnmv",   OP_ARMV8_A64_FMAXNMV,   DataProcAdvSimdAcrossLanes_U1_Single),
  /*size=00/opcode=01101*/ INVALID_OPCODE,
  /*size=00/opcode=01110*/ INVALID_OPCODE,
  /*size=00/opcode=01111*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fmaxv",     OP_ARMV8_A64_FMAXV,     DataProcAdvSimdAcrossLanes_U1_Single),
  /*size=00/opcode=10000*/ INVALID_OPCODE,
  /*size=00/opcode=10001*/ INVALID_OPCODE,
  /*size=00/opcode=10010*/ INVALID_OPCODE,
  /*size=00/opcode=10011*/ INVALID_OPCODE,
  /*size=00/opcode=10100*/ INVALID_OPCODE,
  /*size=00/opcode=10101*/ INVALID_OPCODE,
  /*size=00/opcode=10110*/ INVALID_OPCODE,
  /*size=00/opcode=10111*/ INVALID_OPCODE,
  /*size=00/opcode=11000*/ INVALID_OPCODE,
  /*size=00/opcode=11001*/ INVALID_OPCODE,
  /*size=00/opcode=11010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "uminv",     OP_ARMV8_A64_UMINV,     DataProcAdvSimdAcrossLanes_U1_BHS),
  /*size=00/opcode=11011*/ INVALID_OPCODE,
  /*size=00/opcode=11100*/ INVALID_OPCODE,
  /*size=00/opcode=11101*/ INVALID_OPCODE,
  /*size=00/opcode=11110*/ INVALID_OPCODE,
  /*size=00/opcode=11111*/ INVALID_OPCODE,
  /*size=01/opcode=00000*/ INVALID_OPCODE,
  /*size=01/opcode=00001*/ INVALID_OPCODE,
  /*size=01/opcode=00010*/ INVALID_OPCODE,
  /*size=01/opcode=00011*/ DIS_ARMV8_OP(           0x00000000, "uaddlv",    OP_ARMV8_A64_UADDLV),
  /*size=01/opcode=00100*/ INVALID_OPCODE,
  /*size=01/opcode=00101*/ INVALID_OPCODE,
  /*size=01/opcode=00110*/ INVALID_OPCODE,
  /*size=01/opcode=00111*/ INVALID_OPCODE,
  /*size=01/opcode=01000*/ INVALID_OPCODE,
  /*size=01/opcode=01001*/ INVALID_OPCODE,
  /*size=01/opcode=01010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "umaxv",     OP_ARMV8_A64_UMAXV,     DataProcAdvSimdAcrossLanes_U1_BHS),
  /*size=01/opcode=01011*/ INVALID_OPCODE,
  /*size=01/opcode=01100*/ INVALID_OPCODE,
  /*size=01/opcode=01101*/ INVALID_OPCODE,
  /*size=01/opcode=01110*/ INVALID_OPCODE,
  /*size=01/opcode=01111*/ INVALID_OPCODE,
  /*size=01/opcode=10000*/ INVALID_OPCODE,
  /*size=01/opcode=10001*/ INVALID_OPCODE,
  /*size=01/opcode=10010*/ INVALID_OPCODE,
  /*size=01/opcode=10011*/ INVALID_OPCODE,
  /*size=01/opcode=10100*/ INVALID_OPCODE,
  /*size=01/opcode=10101*/ INVALID_OPCODE,
  /*size=01/opcode=10110*/ INVALID_OPCODE,
  /*size=01/opcode=10111*/ INVALID_OPCODE,
  /*size=01/opcode=11000*/ INVALID_OPCODE,
  /*size=01/opcode=11001*/ INVALID_OPCODE,
  /*size=01/opcode=11010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "uminv",     OP_ARMV8_A64_UMINV,     DataProcAdvSimdAcrossLanes_U1_BHS),
  /*size=01/opcode=11011*/ INVALID_OPCODE,
  /*size=01/opcode=11100*/ INVALID_OPCODE,
  /*size=01/opcode=11101*/ INVALID_OPCODE,
  /*size=01/opcode=11110*/ INVALID_OPCODE,
  /*size=01/opcode=11111*/ INVALID_OPCODE,
  /*size=10/opcode=00000*/ INVALID_OPCODE,
  /*size=10/opcode=00001*/ INVALID_OPCODE,
  /*size=10/opcode=00010*/ INVALID_OPCODE,
  /*size=10/opcode=00011*/ DIS_ARMV8_OP(           0x00000000, "uaddlv",    OP_ARMV8_A64_UADDLV),
  /*size=10/opcode=00100*/ INVALID_OPCODE,
  /*size=10/opcode=00101*/ INVALID_OPCODE,
  /*size=10/opcode=00110*/ INVALID_OPCODE,
  /*size=10/opcode=00111*/ INVALID_OPCODE,
  /*size=10/opcode=01000*/ INVALID_OPCODE,
  /*size=10/opcode=01001*/ INVALID_OPCODE,
  /*size=10/opcode=01010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "umaxv",     OP_ARMV8_A64_UMAXV,     DataProcAdvSimdAcrossLanes_U1_BHS),
  /*size=10/opcode=01011*/ INVALID_OPCODE,
  /*size=10/opcode=01100*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fminnmv",   OP_ARMV8_A64_FMINNMV,   DataProcAdvSimdAcrossLanes_U1_Single),
  /*size=10/opcode=01101*/ INVALID_OPCODE,
  /*size=10/opcode=01110*/ INVALID_OPCODE,
  /*size=10/opcode=01111*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fminv",     OP_ARMV8_A64_FMINV,     DataProcAdvSimdAcrossLanes_U1_Single),
  /*size=10/opcode=10000*/ INVALID_OPCODE,
  /*size=10/opcode=10001*/ INVALID_OPCODE,
  /*size=10/opcode=10010*/ INVALID_OPCODE,
  /*size=10/opcode=10011*/ INVALID_OPCODE,
  /*size=10/opcode=10100*/ INVALID_OPCODE,
  /*size=10/opcode=10101*/ INVALID_OPCODE,
  /*size=10/opcode=10110*/ INVALID_OPCODE,
  /*size=10/opcode=10111*/ INVALID_OPCODE,
  /*size=10/opcode=11000*/ INVALID_OPCODE,
  /*size=10/opcode=11001*/ INVALID_OPCODE,
  /*size=10/opcode=11010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "uminv",     OP_ARMV8_A64_UMINV,     DataProcAdvSimdAcrossLanes_U1_BHS),
  /*size=10/opcode=11011*/ INVALID_OPCODE,
  /*size=10/opcode=11100*/ INVALID_OPCODE,
  /*size=10/opcode=11101*/ INVALID_OPCODE,
  /*size=10/opcode=11110*/ INVALID_OPCODE,
  /*size=10/opcode=11111*/ INVALID_OPCODE,
  /*size=11/opcode=00000*/ INVALID_OPCODE,
  /*size=11/opcode=00001*/ INVALID_OPCODE,
  /*size=11/opcode=00010*/ INVALID_OPCODE,
  /*size=11/opcode=00011*/ DIS_ARMV8_OP(           0x00000000, "uaddlv",    OP_ARMV8_A64_UADDLV),
  /*size=11/opcode=00100*/ INVALID_OPCODE,
  /*size=11/opcode=00101*/ INVALID_OPCODE,
  /*size=11/opcode=00110*/ INVALID_OPCODE,
  /*size=11/opcode=00111*/ INVALID_OPCODE,
  /*size=11/opcode=01000*/ INVALID_OPCODE,
  /*size=11/opcode=01001*/ INVALID_OPCODE,
  /*size=11/opcode=01010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "umaxv",     OP_ARMV8_A64_UMAXV,     DataProcAdvSimdAcrossLanes_U1_BHS),
  /*size=11/opcode=01011*/ INVALID_OPCODE,
  /*size=11/opcode=01100*/ INVALID_OPCODE,
  /*size=11/opcode=01101*/ INVALID_OPCODE,
  /*size=11/opcode=01110*/ INVALID_OPCODE,
  /*size=11/opcode=01111*/ INVALID_OPCODE,
  /*size=11/opcode=10000*/ INVALID_OPCODE,
  /*size=11/opcode=10001*/ INVALID_OPCODE,
  /*size=11/opcode=10010*/ INVALID_OPCODE,
  /*size=11/opcode=10011*/ INVALID_OPCODE,
  /*size=11/opcode=10100*/ INVALID_OPCODE,
  /*size=11/opcode=10101*/ INVALID_OPCODE,
  /*size=11/opcode=10110*/ INVALID_OPCODE,
  /*size=11/opcode=10111*/ INVALID_OPCODE,
  /*size=11/opcode=11000*/ INVALID_OPCODE,
  /*size=11/opcode=11001*/ INVALID_OPCODE,
  /*size=11/opcode=11010*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "uminv",     OP_ARMV8_A64_UMINV,     DataProcAdvSimdAcrossLanes_U1_BHS),
  /*size=11/opcode=11011*/ INVALID_OPCODE,
  /*size=11/opcode=11100*/ INVALID_OPCODE,
  /*size=11/opcode=11101*/ INVALID_OPCODE,
  /*size=11/opcode=11110*/ INVALID_OPCODE,
  /*size=11/opcode=11111*/ INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdAcrossLanes_U1, 0x00000000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate, UINT32_C(0x00c1f000), 12);


/*
 * C4.1.95 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * op0=0xx0 op1=0x op2=x11x op3=xxxxxxx10; select on op3[8:7].
 *
 * Note! Adv SIMD accross lanes has op3=00xxxxx10, with 01xxxxx10 and 1xxxxxx10
 *       being unallocated in the 2025-06 spec edition.
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcAdvSimdAcrossLanesOrUndefined_U0)
  /*op3=00xxxxx10*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanes_U0),
  /*op3=01xxxxx10*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
  /*op3=10xxxxx10*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
  /*op3=11xxxxx10*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcAdvSimdAcrossLanesOrUndefined_U0, UINT32_C(0x00060000), 17);


DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcAdvSimdAcrossLanesOrUndefined_U1)
  /*op3=00xxxxx10*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanes_U1),
  /*op3=01xxxxx10*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
  /*op3=10xxxxx10*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
  /*op3=11xxxxx10*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcAdvSimdAcrossLanesOrUndefined_U1, UINT32_C(0x00060000), 17);


/*
 * C4.1.95 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * op0=0xx0 op1=0x op2=x11x op3=xxxxxxxx0; select on op3[1].
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0)
  DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeDifferent_U0),
  DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrUndefined_U0),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0, 11);

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1)
  DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeDifferent_U1),
  DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrUndefined_U1),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1, 11);


/*
 * C4.1.95 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * op0 = 0xx0 (bit 28 & 31 are fixed at 0) while op3 = xxxxxxxx0 (bit 10 fixed
 * at 0). (Bits 27:25 are fixed to 111).
 *
 * Selecting on op0[2:1]+op1+op2. This has to be split over several tables, though
 * since MAPs can't do disjointed mask (collate).
 */

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFp_b10is0_op0is0000)
  /*0x00:op0=0000/op1=00/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x01:op0=0000/op1=00/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x02:op0=0000/op1=00/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x03:op0=0000/op1=00/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x04:op0=0000/op1=00/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x05:op0=0000/op1=00/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x06:op0=0000/op1=00/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0),
  /*0x07:op0=0000/op1=00/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x08:op0=0000/op1=00/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x09:op0=0000/op1=00/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0a:op0=0000/op1=00/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0b:op0=0000/op1=00/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0c:op0=0000/op1=00/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0d:op0=0000/op1=00/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0e:op0=0000/op1=00/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0),
  /*0x0f:op0=0000/op1=00/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x10:op0=0000/op1=01/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x11:op0=0000/op1=01/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x12:op0=0000/op1=01/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x13:op0=0000/op1=01/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x14:op0=0000/op1=01/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x15:op0=0000/op1=01/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x16:op0=0000/op1=01/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0),
  /*0x17:op0=0000/op1=01/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x18:op0=0000/op1=01/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x19:op0=0000/op1=01/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1a:op0=0000/op1=01/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1b:op0=0000/op1=01/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1c:op0=0000/op1=01/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1d:op0=0000/op1=01/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1e:op0=0000/op1=01/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0),
  /*0x1f:op0=0000/op1=01/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x20:op0=0000/op1=10/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x21:op0=0000/op1=10/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x22:op0=0000/op1=10/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x23:op0=0000/op1=10/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x24:op0=0000/op1=10/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x25:op0=0000/op1=10/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x26:op0=0000/op1=10/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x27:op0=0000/op1=10/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x28:op0=0000/op1=10/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x29:op0=0000/op1=10/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2a:op0=0000/op1=10/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2b:op0=0000/op1=10/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2c:op0=0000/op1=10/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2d:op0=0000/op1=10/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2e:op0=0000/op1=10/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2f:op0=0000/op1=10/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x30:op0=0000/op1=11/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x31:op0=0000/op1=11/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x32:op0=0000/op1=11/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x33:op0=0000/op1=11/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x34:op0=0000/op1=11/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x35:op0=0000/op1=11/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x36:op0=0000/op1=11/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x37:op0=0000/op1=11/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x38:op0=0000/op1=11/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x39:op0=0000/op1=11/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3a:op0=0000/op1=11/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3b:op0=0000/op1=11/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3c:op0=0000/op1=11/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3d:op0=0000/op1=11/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3e:op0=0000/op1=11/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3f:op0=0000/op1=11/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFp_b10is0_op0is0000, UINT32_C(0x01f80000), 19);

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFp_b10is0_op0is0010)
  /*0x00:op0=0010/op1=00/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x01:op0=0010/op1=00/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x02:op0=0010/op1=00/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x03:op0=0010/op1=00/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x04:op0=0010/op1=00/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x05:op0=0010/op1=00/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x06:op0=0010/op1=00/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1),
  /*0x07:op0=0010/op1=00/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x08:op0=0010/op1=00/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x09:op0=0010/op1=00/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0a:op0=0010/op1=00/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0b:op0=0010/op1=00/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0c:op0=0010/op1=00/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0d:op0=0010/op1=00/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0e:op0=0010/op1=00/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1),
  /*0x0f:op0=0010/op1=00/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x10:op0=0010/op1=01/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x11:op0=0010/op1=01/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x12:op0=0010/op1=01/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x13:op0=0010/op1=01/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x14:op0=0010/op1=01/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x15:op0=0010/op1=01/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x16:op0=0010/op1=01/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1),
  /*0x17:op0=0010/op1=01/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x18:op0=0010/op1=01/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x19:op0=0010/op1=01/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1a:op0=0010/op1=01/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1b:op0=0010/op1=01/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1c:op0=0010/op1=01/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1d:op0=0010/op1=01/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1e:op0=0010/op1=01/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1),
  /*0x1f:op0=0010/op1=01/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x20:op0=0010/op1=10/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x21:op0=0010/op1=10/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x22:op0=0010/op1=10/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x23:op0=0010/op1=10/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x24:op0=0010/op1=10/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x25:op0=0010/op1=10/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x26:op0=0010/op1=10/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x27:op0=0010/op1=10/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x28:op0=0010/op1=10/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x29:op0=0010/op1=10/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2a:op0=0010/op1=10/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2b:op0=0010/op1=10/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2c:op0=0010/op1=10/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2d:op0=0010/op1=10/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2e:op0=0010/op1=10/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2f:op0=0010/op1=10/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x30:op0=0010/op1=11/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x31:op0=0010/op1=11/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x32:op0=0010/op1=11/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x33:op0=0010/op1=11/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x34:op0=0010/op1=11/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x35:op0=0010/op1=11/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x36:op0=0010/op1=11/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x37:op0=0010/op1=11/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x38:op0=0010/op1=11/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x39:op0=0010/op1=11/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3a:op0=0010/op1=11/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3b:op0=0010/op1=11/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3c:op0=0010/op1=11/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3d:op0=0010/op1=11/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3e:op0=0010/op1=11/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3f:op0=0010/op1=11/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFp_b10is0_op0is0010, UINT32_C(0x01f80000), 19);

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFp_b10is0_op0is0100)
  /*0x00:op0=0100/op1=00/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x01:op0=0100/op1=00/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x02:op0=0100/op1=00/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x03:op0=0100/op1=00/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x04:op0=0100/op1=00/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x05:op0=0100/op1=00/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x06:op0=0100/op1=00/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0),
  /*0x07:op0=0100/op1=00/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x08:op0=0100/op1=00/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x09:op0=0100/op1=00/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0a:op0=0100/op1=00/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0b:op0=0100/op1=00/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0c:op0=0100/op1=00/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0d:op0=0100/op1=00/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0e:op0=0100/op1=00/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0),
  /*0x0f:op0=0100/op1=00/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x10:op0=0100/op1=01/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x11:op0=0100/op1=01/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x12:op0=0100/op1=01/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x13:op0=0100/op1=01/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x14:op0=0100/op1=01/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x15:op0=0100/op1=01/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x16:op0=0100/op1=01/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0),
  /*0x17:op0=0100/op1=01/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x18:op0=0100/op1=01/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x19:op0=0100/op1=01/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1a:op0=0100/op1=01/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1b:op0=0100/op1=01/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1c:op0=0100/op1=01/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1d:op0=0100/op1=01/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1e:op0=0100/op1=01/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U0),
  /*0x1f:op0=0100/op1=01/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x20:op0=0100/op1=10/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x21:op0=0100/op1=10/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x22:op0=0100/op1=10/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x23:op0=0100/op1=10/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x24:op0=0100/op1=10/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x25:op0=0100/op1=10/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x26:op0=0100/op1=10/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x27:op0=0100/op1=10/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x28:op0=0100/op1=10/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x29:op0=0100/op1=10/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2a:op0=0100/op1=10/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2b:op0=0100/op1=10/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2c:op0=0100/op1=10/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2d:op0=0100/op1=10/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2e:op0=0100/op1=10/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2f:op0=0100/op1=10/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x30:op0=0100/op1=11/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x31:op0=0100/op1=11/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x32:op0=0100/op1=11/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x33:op0=0100/op1=11/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x34:op0=0100/op1=11/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x35:op0=0100/op1=11/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x36:op0=0100/op1=11/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x37:op0=0100/op1=11/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x38:op0=0100/op1=11/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x39:op0=0100/op1=11/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3a:op0=0100/op1=11/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3b:op0=0100/op1=11/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3c:op0=0100/op1=11/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3d:op0=0100/op1=11/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3e:op0=0100/op1=11/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3f:op0=0100/op1=11/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFp_b10is0_op0is0100, UINT32_C(0x01f80000), 19);
//(0x6e31aab5>>29)&3 = 0x3 (3)
//(0x6e31aab5>>19)&0x3f = 0x6 (6) = 0x3 (3)


DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFp_b10is0_op0is0110)
  /*0x00:op0=0110/op1=00/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x01:op0=0110/op1=00/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x02:op0=0110/op1=00/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x03:op0=0110/op1=00/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x04:op0=0110/op1=00/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x05:op0=0110/op1=00/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x06:op0=0110/op1=00/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1),
  /*0x07:op0=0110/op1=00/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x08:op0=0110/op1=00/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x09:op0=0110/op1=00/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0a:op0=0110/op1=00/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0b:op0=0110/op1=00/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0c:op0=0110/op1=00/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0d:op0=0110/op1=00/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0e:op0=0110/op1=00/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1),
  /*0x0f:op0=0110/op1=00/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x10:op0=0110/op1=01/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x11:op0=0110/op1=01/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x12:op0=0110/op1=01/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x13:op0=0110/op1=01/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x14:op0=0110/op1=01/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x15:op0=0110/op1=01/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x16:op0=0110/op1=01/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1),
  /*0x17:op0=0110/op1=01/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x18:op0=0110/op1=01/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x19:op0=0110/op1=01/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1a:op0=0110/op1=01/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1b:op0=0110/op1=01/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1c:op0=0110/op1=01/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1d:op0=0110/op1=01/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1e:op0=0110/op1=01/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdAcrossLanesOrThreeDifferent_U1),
  /*0x1f:op0=0110/op1=01/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x20:op0=0110/op1=10/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x21:op0=0110/op1=10/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x22:op0=0110/op1=10/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x23:op0=0110/op1=10/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x24:op0=0110/op1=10/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x25:op0=0110/op1=10/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x26:op0=0110/op1=10/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x27:op0=0110/op1=10/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x28:op0=0110/op1=10/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x29:op0=0110/op1=10/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2a:op0=0110/op1=10/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2b:op0=0110/op1=10/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2c:op0=0110/op1=10/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2d:op0=0110/op1=10/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2e:op0=0110/op1=10/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2f:op0=0110/op1=10/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x30:op0=0110/op1=11/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x31:op0=0110/op1=11/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x32:op0=0110/op1=11/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x33:op0=0110/op1=11/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x34:op0=0110/op1=11/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x35:op0=0110/op1=11/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x36:op0=0110/op1=11/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x37:op0=0110/op1=11/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x38:op0=0110/op1=11/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x39:op0=0110/op1=11/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3a:op0=0110/op1=11/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3b:op0=0110/op1=11/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3c:op0=0110/op1=11/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3d:op0=0110/op1=11/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3e:op0=0110/op1=11/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3f:op0=0110/op1=11/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFp_b10is0_op0is0110, UINT32_C(0x01f80000), 19);

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_0_Bit31_0_b10is0)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFp_b10is0_op0is0000),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFp_b10is0_op0is0010),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFp_b10is0_op0is0100),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFp_b10is0_op0is0110),
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFpBit28_0_Bit31_0_b10is0, UINT32_C(0x60000000), 29);

/*
 * C4.1.95.24 - Data Processing - Advanced SIMD three same, U=0
 *
 * Fixed bits:
 *      - bit 31:     0     (op0=0xx0)
 *      - bit 28:     0     (op0=0xx0,U=0)
 *      - bits 27-24: 1110  (111,op1=0x)
 *      - bits 21:    1     (op2=x1xx)
 *      - bits 10:    1     (op3=xxxxxxxx1)
 *
 * Indexing size (bits 22 & 23) and opcode (bits 11 thru 15):
 *      - bits 0-4 = opcode; 0x00..0x1f
 *      - bits 5-6 = size;   0x00..0x60
 *      - total 7 bits;      0x00..0x7f
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdThreeSame_U0)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegElemSize, 22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,           30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,         16,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcAdvSimdThreeSame_U0_8B)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegTypeFixed, kDisOpParamArmV8VecRegType_8B, 0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,            30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,         16,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdThreeSame_U0)
  /* 0x00:size=00/opcode=00000*/    DIS_ARMV8_OP(           0, "shadd",       OP_ARMV8_A64_SHADD),
  /* 0x01:size=00/opcode=00001*/    DIS_ARMV8_OP(           0, "sqadd",       OP_ARMV8_A64_SQADD),
  /* 0x02:size=00/opcode=00010*/    DIS_ARMV8_OP(           0, "srhadd",      OP_ARMV8_A64_SRHADD),
  /* 0x03:size=00/opcode=00011*/    INVALID_OPCODE,
  /* 0x04:size=00/opcode=00100*/    DIS_ARMV8_OP(           0, "shsub",       OP_ARMV8_A64_SHSUB),
  /* 0x05:size=00/opcode=00101*/    DIS_ARMV8_OP(           0, "sqsub",       OP_ARMV8_A64_SQSUB),
  /* 0x06:size=00/opcode=00110*/    DIS_ARMV8_OP(           0, "cmgt",        OP_ARMV8_A64_CMGT),
  /* 0x07:size=00/opcode=00111*/    DIS_ARMV8_OP(           0, "cmge",        OP_ARMV8_A64_CMGE),
  /* 0x08:size=00/opcode=01000*/    DIS_ARMV8_OP(           0, "sshl",        OP_ARMV8_A64_SSHL),
  /* 0x09:size=00/opcode=01001*/    DIS_ARMV8_OP(           0, "sqshl",       OP_ARMV8_A64_SQSHL),
  /* 0x0a:size=00/opcode=01010*/    DIS_ARMV8_OP(           0, "srshl",       OP_ARMV8_A64_SRSHL),
  /* 0x0b:size=00/opcode=01011*/    DIS_ARMV8_OP(           0, "sqrshl",      OP_ARMV8_A64_SQRSHL),
  /* 0x0c:size=00/opcode=01100*/    DIS_ARMV8_OP(           0, "smax",        OP_ARMV8_A64_SMAX),
  /* 0x0d:size=00/opcode=01101*/    DIS_ARMV8_OP(           0, "smin",        OP_ARMV8_A64_SMIN),
  /* 0x0e:size=00/opcode=01110*/    DIS_ARMV8_OP(           0, "sabd",        OP_ARMV8_A64_SABD),
  /* 0x0f:size=00/opcode=01111*/    DIS_ARMV8_OP(           0, "saba",        OP_ARMV8_A64_SABA),
  /* 0x10:size=00/opcode=10000*/    DIS_ARMV8_OP(           0, "add",         OP_ARMV8_A64_ADD),
  /* 0x11:size=00/opcode=10001*/    DIS_ARMV8_OP(           0, "cmtst",       OP_ARMV8_A64_CMTST),
  /* 0x12:size=00/opcode=10010*/    DIS_ARMV8_OP(           0, "mla",         OP_ARMV8_A64_MLA),
  /* 0x13:size=00/opcode=10011*/    DIS_ARMV8_OP(           0, "mul",         OP_ARMV8_A64_MUL),
  /* 0x14:size=00/opcode=10100*/    DIS_ARMV8_OP(           0, "smaxp",       OP_ARMV8_A64_SMAXP),
  /* 0x15:size=00/opcode=10101*/    DIS_ARMV8_OP(           0, "sminp",       OP_ARMV8_A64_SMINP),
  /* 0x16:size=00/opcode=10110*/    DIS_ARMV8_OP(           0, "sqdmulh",     OP_ARMV8_A64_SQDMULH),
  /* 0x17:size=00/opcode=10111*/    DIS_ARMV8_OP(           0, "addp",        OP_ARMV8_A64_ADDP),
  /* 0x18:size=00/opcode=11000*/    DIS_ARMV8_OP(           0, "fmaxnm",      OP_ARMV8_A64_FMAXNM),
  /* 0x19:size=00/opcode=11001*/    DIS_ARMV8_OP(           0, "fmla",        OP_ARMV8_A64_FMLA),
  /* 0x1a:size=00/opcode=11010*/    DIS_ARMV8_OP(           0, "fadd",        OP_ARMV8_A64_FADD),
  /* 0x1b:size=00/opcode=11011*/    DIS_ARMV8_OP(           0, "fmulx",       OP_ARMV8_A64_FMULX),
  /* 0x1c:size=00/opcode=11100*/    DIS_ARMV8_OP(           0, "fcmeq",       OP_ARMV8_A64_FCMEQ),
  /* 0x1d:size=00/opcode=11101*/    INVALID_OPCODE,
  /* 0x1e:size=00/opcode=11110*/    DIS_ARMV8_OP(           0, "fmax",        OP_ARMV8_A64_FMAX),
  /* 0x1f:size=00/opcode=11111*/    DIS_ARMV8_OP(           0, "frecps",      OP_ARMV8_A64_FRECPS),
  /* 0x20:size=01/opcode=00000*/    DIS_ARMV8_OP(           0, "shadd",       OP_ARMV8_A64_SHADD),
  /* 0x21:size=01/opcode=00001*/    DIS_ARMV8_OP(           0, "sqadd",       OP_ARMV8_A64_SQADD),
  /* 0x22:size=01/opcode=00010*/    DIS_ARMV8_OP(           0, "srhadd",      OP_ARMV8_A64_SRHADD),
  /* 0x23:size=01/opcode=00011*/    INVALID_OPCODE,
  /* 0x24:size=01/opcode=00100*/    DIS_ARMV8_OP(           0, "shsub",       OP_ARMV8_A64_SHSUB),
  /* 0x25:size=01/opcode=00101*/    DIS_ARMV8_OP(           0, "sqsub",       OP_ARMV8_A64_SQSUB),
  /* 0x26:size=01/opcode=00110*/    DIS_ARMV8_OP(           0, "cmgt",        OP_ARMV8_A64_CMGT),
  /* 0x27:size=01/opcode=00111*/    DIS_ARMV8_OP(           0, "cmge",        OP_ARMV8_A64_CMGE),
  /* 0x28:size=01/opcode=01000*/    DIS_ARMV8_OP(           0, "sshl",        OP_ARMV8_A64_SSHL),
  /* 0x29:size=01/opcode=01001*/    DIS_ARMV8_OP(           0, "sqshl",       OP_ARMV8_A64_SQSHL),
  /* 0x2a:size=01/opcode=01010*/    DIS_ARMV8_OP(           0, "srshl",       OP_ARMV8_A64_SRSHL),
  /* 0x2b:size=01/opcode=01011*/    DIS_ARMV8_OP(           0, "sqrshl",      OP_ARMV8_A64_SQRSHL),
  /* 0x2c:size=01/opcode=01100*/    DIS_ARMV8_OP(           0, "smax",        OP_ARMV8_A64_SMAX),
  /* 0x2d:size=01/opcode=01101*/    DIS_ARMV8_OP(           0, "smin",        OP_ARMV8_A64_SMIN),
  /* 0x2e:size=01/opcode=01110*/    DIS_ARMV8_OP(           0, "sabd",        OP_ARMV8_A64_SABD),
  /* 0x2f:size=01/opcode=01111*/    DIS_ARMV8_OP(           0, "saba",        OP_ARMV8_A64_SABA),
  /* 0x30:size=01/opcode=10000*/    DIS_ARMV8_OP(           0, "add",         OP_ARMV8_A64_ADD),
  /* 0x31:size=01/opcode=10001*/    DIS_ARMV8_OP(           0, "cmtst",       OP_ARMV8_A64_CMTST),
  /* 0x32:size=01/opcode=10010*/    DIS_ARMV8_OP(           0, "mla",         OP_ARMV8_A64_MLA),
  /* 0x33:size=01/opcode=10011*/    DIS_ARMV8_OP(           0, "mul",         OP_ARMV8_A64_MUL),
  /* 0x34:size=01/opcode=10100*/    DIS_ARMV8_OP(           0, "smaxp",       OP_ARMV8_A64_SMAXP),
  /* 0x35:size=01/opcode=10101*/    DIS_ARMV8_OP(           0, "sminp",       OP_ARMV8_A64_SMINP),
  /* 0x36:size=01/opcode=10110*/    DIS_ARMV8_OP(           0, "sqdmulh",     OP_ARMV8_A64_SQDMULH),
  /* 0x37:size=01/opcode=10111*/    DIS_ARMV8_OP(           0, "addp",        OP_ARMV8_A64_ADDP),
  /* 0x38:size=01/opcode=11000*/    DIS_ARMV8_OP(           0, "fmaxnm",      OP_ARMV8_A64_FMAXNM),
  /* 0x39:size=01/opcode=11001*/    DIS_ARMV8_OP(           0, "fmla",        OP_ARMV8_A64_FMLA),
  /* 0x3a:size=01/opcode=11010*/    DIS_ARMV8_OP(           0, "fadd",        OP_ARMV8_A64_FADD),
  /* 0x3b:size=01/opcode=11011*/    DIS_ARMV8_OP(           0, "fmulx",       OP_ARMV8_A64_FMULX),
  /* 0x3c:size=01/opcode=11100*/    DIS_ARMV8_OP(           0, "fcmeq",       OP_ARMV8_A64_FCMEQ),
  /* 0x3d:size=01/opcode=11101*/    DIS_ARMV8_OP(           0, "fmlal",       OP_ARMV8_A64_FMLAL),
  /* 0x3e:size=01/opcode=11110*/    DIS_ARMV8_OP(           0, "fmax",        OP_ARMV8_A64_FMAX),
  /* 0x3f:size=01/opcode=11111*/    DIS_ARMV8_OP(           0, "frecps",      OP_ARMV8_A64_FRECPS),
  /* 0x40:size=10/opcode=00000*/    DIS_ARMV8_OP(           0, "shadd",       OP_ARMV8_A64_SHADD),
  /* 0x41:size=10/opcode=00001*/    DIS_ARMV8_OP(           0, "sqadd",       OP_ARMV8_A64_SQADD),
  /* 0x42:size=10/opcode=00010*/    DIS_ARMV8_OP(           0, "srhadd",      OP_ARMV8_A64_SRHADD),
  /* 0x43:size=10/opcode=00011*/    DIS_ARMV8_OP_ALT_DECODE(0, "orr",         OP_ARMV8_A64_ORR, DataProcAdvSimdThreeSame_U0_8B),
  /* 0x44:size=10/opcode=00100*/    DIS_ARMV8_OP(           0, "shsub",       OP_ARMV8_A64_SHSUB),
  /* 0x45:size=10/opcode=00101*/    DIS_ARMV8_OP(           0, "sqsub",       OP_ARMV8_A64_SQSUB),
  /* 0x46:size=10/opcode=00110*/    DIS_ARMV8_OP(           0, "cmgt",        OP_ARMV8_A64_CMGT),
  /* 0x47:size=10/opcode=00111*/    DIS_ARMV8_OP(           0, "cmge",        OP_ARMV8_A64_CMGE),
  /* 0x48:size=10/opcode=01000*/    DIS_ARMV8_OP(           0, "sshl",        OP_ARMV8_A64_SSHL),
  /* 0x49:size=10/opcode=01001*/    DIS_ARMV8_OP(           0, "sqshl",       OP_ARMV8_A64_SQSHL),
  /* 0x4a:size=10/opcode=01010*/    DIS_ARMV8_OP(           0, "srshl",       OP_ARMV8_A64_SRSHL),
  /* 0x4b:size=10/opcode=01011*/    DIS_ARMV8_OP(           0, "sqrshl",      OP_ARMV8_A64_SQRSHL),
  /* 0x4c:size=10/opcode=01100*/    DIS_ARMV8_OP(           0, "smax",        OP_ARMV8_A64_SMAX),
  /* 0x4d:size=10/opcode=01101*/    DIS_ARMV8_OP(           0, "smin",        OP_ARMV8_A64_SMIN),
  /* 0x4e:size=10/opcode=01110*/    DIS_ARMV8_OP(           0, "sabd",        OP_ARMV8_A64_SABD),
  /* 0x4f:size=10/opcode=01111*/    DIS_ARMV8_OP(           0, "saba",        OP_ARMV8_A64_SABA),
  /* 0x50:size=10/opcode=10000*/    DIS_ARMV8_OP(           0, "add",         OP_ARMV8_A64_ADD),
  /* 0x51:size=10/opcode=10001*/    DIS_ARMV8_OP(           0, "cmtst",       OP_ARMV8_A64_CMTST),
  /* 0x52:size=10/opcode=10010*/    DIS_ARMV8_OP(           0, "mla",         OP_ARMV8_A64_MLA),
  /* 0x53:size=10/opcode=10011*/    DIS_ARMV8_OP(           0, "mul",         OP_ARMV8_A64_MUL),
  /* 0x54:size=10/opcode=10100*/    DIS_ARMV8_OP(           0, "smaxp",       OP_ARMV8_A64_SMAXP),
  /* 0x55:size=10/opcode=10101*/    DIS_ARMV8_OP(           0, "sminp",       OP_ARMV8_A64_SMINP),
  /* 0x56:size=10/opcode=10110*/    DIS_ARMV8_OP(           0, "sqdmulh",     OP_ARMV8_A64_SQDMULH),
  /* 0x57:size=10/opcode=10111*/    DIS_ARMV8_OP(           0, "addp",        OP_ARMV8_A64_ADDP),
  /* 0x58:size=10/opcode=11000*/    DIS_ARMV8_OP(           0, "fminnm",      OP_ARMV8_A64_FMINNM),
  /* 0x59:size=10/opcode=11001*/    DIS_ARMV8_OP(           0, "fmls",        OP_ARMV8_A64_FMLS),
  /* 0x5a:size=10/opcode=11010*/    DIS_ARMV8_OP(           0, "fsub",        OP_ARMV8_A64_FSUB),
  /* 0x5b:size=10/opcode=11011*/    DIS_ARMV8_OP(           0, "famax",       OP_ARMV8_A64_FAMAX),
  /* 0x5c:size=10/opcode=11100*/    INVALID_OPCODE,
  /* 0x5d:size=10/opcode=11101*/    DIS_ARMV8_OP(           0, "fmlsl",       OP_ARMV8_A64_FMLSL),
  /* 0x5e:size=10/opcode=11110*/    DIS_ARMV8_OP(           0, "fmin",        OP_ARMV8_A64_FMIN),
  /* 0x5f:size=10/opcode=11111*/    DIS_ARMV8_OP(           0, "frsqrts",     OP_ARMV8_A64_FRSQRTS),
  /* 0x60:size=11/opcode=00000*/    DIS_ARMV8_OP(           0, "shadd",       OP_ARMV8_A64_SHADD),
  /* 0x61:size=11/opcode=00001*/    DIS_ARMV8_OP(           0, "sqadd",       OP_ARMV8_A64_SQADD),
  /* 0x62:size=11/opcode=00010*/    DIS_ARMV8_OP(           0, "srhadd",      OP_ARMV8_A64_SRHADD),
  /* 0x63:size=11/opcode=00011*/    DIS_ARMV8_OP(           0, "orn",         OP_ARMV8_A64_ORN),
  /* 0x64:size=11/opcode=00100*/    DIS_ARMV8_OP(           0, "shsub",       OP_ARMV8_A64_SHSUB),
  /* 0x65:size=11/opcode=00101*/    DIS_ARMV8_OP(           0, "sqsub",       OP_ARMV8_A64_SQSUB),
  /* 0x66:size=11/opcode=00110*/    DIS_ARMV8_OP(           0, "cmgt",        OP_ARMV8_A64_CMGT),
  /* 0x67:size=11/opcode=00111*/    DIS_ARMV8_OP(           0, "cmge",        OP_ARMV8_A64_CMGE),
  /* 0x68:size=11/opcode=01000*/    DIS_ARMV8_OP(           0, "sshl",        OP_ARMV8_A64_SSHL),
  /* 0x69:size=11/opcode=01001*/    DIS_ARMV8_OP(           0, "sqshl",       OP_ARMV8_A64_SQSHL),
  /* 0x6a:size=11/opcode=01010*/    DIS_ARMV8_OP(           0, "srshl",       OP_ARMV8_A64_SRSHL),
  /* 0x6b:size=11/opcode=01011*/    DIS_ARMV8_OP(           0, "sqrshl",      OP_ARMV8_A64_SQRSHL),
  /* 0x6c:size=11/opcode=01100*/    DIS_ARMV8_OP(           0, "smax",        OP_ARMV8_A64_SMAX),
  /* 0x6d:size=11/opcode=01101*/    DIS_ARMV8_OP(           0, "smin",        OP_ARMV8_A64_SMIN),
  /* 0x6e:size=11/opcode=01110*/    DIS_ARMV8_OP(           0, "sabd",        OP_ARMV8_A64_SABD),
  /* 0x6f:size=11/opcode=01111*/    DIS_ARMV8_OP(           0, "saba",        OP_ARMV8_A64_SABA),
  /* 0x70:size=11/opcode=10000*/    DIS_ARMV8_OP(           0, "add",         OP_ARMV8_A64_ADD),
  /* 0x71:size=11/opcode=10001*/    DIS_ARMV8_OP(           0, "cmtst",       OP_ARMV8_A64_CMTST),
  /* 0x72:size=11/opcode=10010*/    DIS_ARMV8_OP(           0, "mla",         OP_ARMV8_A64_MLA),
  /* 0x73:size=11/opcode=10011*/    DIS_ARMV8_OP(           0, "mul",         OP_ARMV8_A64_MUL),
  /* 0x74:size=11/opcode=10100*/    DIS_ARMV8_OP(           0, "smaxp",       OP_ARMV8_A64_SMAXP),
  /* 0x75:size=11/opcode=10101*/    DIS_ARMV8_OP(           0, "sminp",       OP_ARMV8_A64_SMINP),
  /* 0x76:size=11/opcode=10110*/    DIS_ARMV8_OP(           0, "sqdmulh",     OP_ARMV8_A64_SQDMULH),
  /* 0x77:size=11/opcode=10111*/    DIS_ARMV8_OP(           0, "addp",        OP_ARMV8_A64_ADDP),
  /* 0x78:size=11/opcode=11000*/    DIS_ARMV8_OP(           0, "fminnm",      OP_ARMV8_A64_FMINNM),
  /* 0x79:size=11/opcode=11001*/    DIS_ARMV8_OP(           0, "fmls",        OP_ARMV8_A64_FMLS),
  /* 0x7a:size=11/opcode=11010*/    DIS_ARMV8_OP(           0, "fsub",        OP_ARMV8_A64_FSUB),
  /* 0x7b:size=11/opcode=11011*/    DIS_ARMV8_OP(           0, "famax",       OP_ARMV8_A64_FAMAX),
  /* 0x7c:size=11/opcode=11100*/    INVALID_OPCODE,
  /* 0x7d:size=11/opcode=11101*/    INVALID_OPCODE,
  /* 0x7e:size=11/opcode=11110*/    DIS_ARMV8_OP(           0, "fmin",        OP_ARMV8_A64_FMIN),
  /* 0x7f:size=11/opcode=11111*/    DIS_ARMV8_OP(           0, "frsqrts",     OP_ARMV8_A64_FRSQRTS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdThreeSame_U0, 0x00000000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate, UINT32_C(0x00c0f800), 11);


/*
 * C4.1.95.24 - Data Processing - Advanced SIMD three same, U=1
 *
 * Fixed bits:
 *      - bit 31:     0     (op0=0xx0)
 *      - bit 28:     1     (op0=0xx0,U=1)
 *      - bits 27-24: 1110  (111,op1=0x)
 *      - bits 21:    1     (op2=x1xx)
 *      - bits 10:    1     (op3=xxxxxxxx1)
 *
 * Indexing size (bits 22 & 23) and opcode (bits 11 thru 15):
 *      - bits 0-4 = opcode; 0x00..0x1f
 *      - bits 5-6 = size;   0x00..0x60
 *      - total 7 bits;      0x00..0x7f
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdThreeSame_U1)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegElemSize, 22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,           30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,         16,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdThreeSame_U1)
  /*0x00:size=00/opcode=00000*/     DIS_ARMV8_OP(0x00000000, "uhadd",       OP_ARMV8_A64_UHADD),
  /*0x01:size=00/opcode=00001*/     DIS_ARMV8_OP(0x00000000, "uqadd",       OP_ARMV8_A64_UQADD),
  /*0x02:size=00/opcode=00010*/     DIS_ARMV8_OP(0x00000000, "urhadd",      OP_ARMV8_A64_URHADD),
  /*0x03:size=00/opcode=00011*/     DIS_ARMV8_OP(0x00000000, "eor",         OP_ARMV8_A64_EOR),
  /*0x04:size=00/opcode=00100*/     DIS_ARMV8_OP(0x00000000, "uhsub",       OP_ARMV8_A64_UHSUB),
  /*0x05:size=00/opcode=00101*/     DIS_ARMV8_OP(0x00000000, "uqsub",       OP_ARMV8_A64_UQSUB),
  /*0x06:size=00/opcode=00110*/     DIS_ARMV8_OP(0x00000000, "cmhi",        OP_ARMV8_A64_CMHI),
  /*0x07:size=00/opcode=00111*/     DIS_ARMV8_OP(0x00000000, "cmhs",        OP_ARMV8_A64_CMHS),
  /*0x08:size=00/opcode=01000*/     DIS_ARMV8_OP(0x00000000, "ushl",        OP_ARMV8_A64_USHL),
  /*0x09:size=00/opcode=01001*/     DIS_ARMV8_OP(0x00000000, "uqshl",       OP_ARMV8_A64_UQSHL),
  /*0x0a:size=00/opcode=01010*/     DIS_ARMV8_OP(0x00000000, "urshl",       OP_ARMV8_A64_URSHL),
  /*0x0b:size=00/opcode=01011*/     DIS_ARMV8_OP(0x00000000, "uqrshl",      OP_ARMV8_A64_UQRSHL),
  /*0x0c:size=00/opcode=01100*/     DIS_ARMV8_OP(0x00000000, "umax",        OP_ARMV8_A64_UMAX),
  /*0x0d:size=00/opcode=01101*/     DIS_ARMV8_OP(0x00000000, "umin",        OP_ARMV8_A64_UMIN),
  /*0x0e:size=00/opcode=01110*/     DIS_ARMV8_OP(0x00000000, "uadd",        OP_ARMV8_A64_UADD),
  /*0x0f:size=00/opcode=01111*/     DIS_ARMV8_OP(0x00000000, "uaba",        OP_ARMV8_A64_UABA),
  /*0x10:size=00/opcode=10000*/     DIS_ARMV8_OP(0x00000000, "sub",         OP_ARMV8_A64_SUB),
  /*0x11:size=00/opcode=10001*/     DIS_ARMV8_OP(0x00000000, "cmeq",        OP_ARMV8_A64_CMEQ),
  /*0x12:size=00/opcode=10010*/     DIS_ARMV8_OP(0x00000000, "mls",         OP_ARMV8_A64_MLS),
  /*0x13:size=00/opcode=10011*/     DIS_ARMV8_OP(0x00000000, "pmul",        OP_ARMV8_A64_PMUL),
  /*0x14:size=00/opcode=10100*/     DIS_ARMV8_OP(0x00000000, "umaxp",       OP_ARMV8_A64_UMAXP),
  /*0x15:size=00/opcode=10101*/     DIS_ARMV8_OP(0x00000000, "uminp",       OP_ARMV8_A64_UMINP),
  /*0x16:size=00/opcode=10110*/     DIS_ARMV8_OP(0x00000000, "sqrdmulh",    OP_ARMV8_A64_SQRDMULH),
  /*0x17:size=00/opcode=10111*/     INVALID_OPCODE,
  /*0x18:size=00/opcode=11000*/     DIS_ARMV8_OP(0x00000000, "fmaxnmp",     OP_ARMV8_A64_FMAXNMP),
  /*0x19:size=00/opcode=11001*/     DIS_ARMV8_OP(0x00000000, "fmlal2",      OP_ARMV8_A64_FMLAL2),
  /*0x1a:size=00/opcode=11010*/     DIS_ARMV8_OP(0x00000000, "faddp",       OP_ARMV8_A64_FADDP),
  /*0x1b:size=00/opcode=11011*/     DIS_ARMV8_OP(0x00000000, "fmul",        OP_ARMV8_A64_FMUL),
  /*0x1c:size=00/opcode=11100*/     DIS_ARMV8_OP(0x00000000, "fcmge",       OP_ARMV8_A64_FCMGE),
  /*0x1d:size=00/opcode=11101*/     DIS_ARMV8_OP(0x00000000, "facge",       OP_ARMV8_A64_FACGE),
  /*0x1e:size=00/opcode=11110*/     DIS_ARMV8_OP(0x00000000, "fmaxp",       OP_ARMV8_A64_FMAXP),
  /*0x1f:size=00/opcode=11111*/     DIS_ARMV8_OP(0x00000000, "fdiv",        OP_ARMV8_A64_FDIV),
  /*0x20:size=01/opcode=00000*/     DIS_ARMV8_OP(0x00000000, "uhadd",       OP_ARMV8_A64_UHADD),
  /*0x21:size=01/opcode=00001*/     DIS_ARMV8_OP(0x00000000, "uqadd",       OP_ARMV8_A64_UQADD),
  /*0x22:size=01/opcode=00010*/     DIS_ARMV8_OP(0x00000000, "urhadd",      OP_ARMV8_A64_URHADD),
  /*0x23:size=01/opcode=00011*/     DIS_ARMV8_OP(0x00000000, "bsl",         OP_ARMV8_A64_BSL),
  /*0x24:size=01/opcode=00100*/     DIS_ARMV8_OP(0x00000000, "uhsub",       OP_ARMV8_A64_UHSUB),
  /*0x25:size=01/opcode=00101*/     DIS_ARMV8_OP(0x00000000, "uqsub",       OP_ARMV8_A64_UQSUB),
  /*0x26:size=01/opcode=00110*/     DIS_ARMV8_OP(0x00000000, "cmhi",        OP_ARMV8_A64_CMHI),
  /*0x27:size=01/opcode=00111*/     DIS_ARMV8_OP(0x00000000, "cmhs",        OP_ARMV8_A64_CMHS),
  /*0x28:size=01/opcode=01000*/     DIS_ARMV8_OP(0x00000000, "ushl",        OP_ARMV8_A64_USHL),
  /*0x29:size=01/opcode=01001*/     DIS_ARMV8_OP(0x00000000, "uqshl",       OP_ARMV8_A64_UQSHL),
  /*0x2a:size=01/opcode=01010*/     DIS_ARMV8_OP(0x00000000, "urshl",       OP_ARMV8_A64_URSHL),
  /*0x2b:size=01/opcode=01011*/     DIS_ARMV8_OP(0x00000000, "uqrshl",      OP_ARMV8_A64_UQRSHL),
  /*0x2c:size=01/opcode=01100*/     DIS_ARMV8_OP(0x00000000, "umax",        OP_ARMV8_A64_UMAX),
  /*0x2d:size=01/opcode=01101*/     DIS_ARMV8_OP(0x00000000, "umin",        OP_ARMV8_A64_UMIN),
  /*0x2e:size=01/opcode=01110*/     DIS_ARMV8_OP(0x00000000, "uadd",        OP_ARMV8_A64_UADD),
  /*0x2f:size=01/opcode=01111*/     DIS_ARMV8_OP(0x00000000, "uaba",        OP_ARMV8_A64_UABA),
  /*0x30:size=01/opcode=10000*/     DIS_ARMV8_OP(0x00000000, "sub",         OP_ARMV8_A64_SUB),
  /*0x31:size=01/opcode=10001*/     DIS_ARMV8_OP(0x00000000, "cmeq",        OP_ARMV8_A64_CMEQ),
  /*0x32:size=01/opcode=10010*/     DIS_ARMV8_OP(0x00000000, "mls",         OP_ARMV8_A64_MLS),
  /*0x33:size=01/opcode=10011*/     DIS_ARMV8_OP(0x00000000, "pmul",        OP_ARMV8_A64_PMUL),
  /*0x34:size=01/opcode=10100*/     DIS_ARMV8_OP(0x00000000, "umaxp",       OP_ARMV8_A64_UMAXP),
  /*0x35:size=01/opcode=10101*/     DIS_ARMV8_OP(0x00000000, "uminp",       OP_ARMV8_A64_UMINP),
  /*0x36:size=01/opcode=10110*/     DIS_ARMV8_OP(0x00000000, "sqrdmulh",    OP_ARMV8_A64_SQRDMULH),
  /*0x37:size=01/opcode=10111*/     INVALID_OPCODE,
  /*0x38:size=01/opcode=11000*/     DIS_ARMV8_OP(0x00000000, "fmaxnmp",     OP_ARMV8_A64_FMAXNMP),
  /*0x39:size=01/opcode=11001*/     INVALID_OPCODE,
  /*0x3a:size=01/opcode=11010*/     DIS_ARMV8_OP(0x00000000, "faddp",       OP_ARMV8_A64_FADDP),
  /*0x3b:size=01/opcode=11011*/     DIS_ARMV8_OP(0x00000000, "fmul",        OP_ARMV8_A64_FMUL),
  /*0x3c:size=01/opcode=11100*/     DIS_ARMV8_OP(0x00000000, "fcmge",       OP_ARMV8_A64_FCMGE),
  /*0x3d:size=01/opcode=11101*/     DIS_ARMV8_OP(0x00000000, "facge",       OP_ARMV8_A64_FACGE),
  /*0x3e:size=01/opcode=11110*/     DIS_ARMV8_OP(0x00000000, "fmaxp",       OP_ARMV8_A64_FMAXP),
  /*0x3f:size=01/opcode=11111*/     DIS_ARMV8_OP(0x00000000, "fdiv",        OP_ARMV8_A64_FDIV),
  /*0x40:size=10/opcode=00000*/     DIS_ARMV8_OP(0x00000000, "uhadd",       OP_ARMV8_A64_UHADD),
  /*0x41:size=10/opcode=00001*/     DIS_ARMV8_OP(0x00000000, "uqadd",       OP_ARMV8_A64_UQADD),
  /*0x42:size=10/opcode=00010*/     DIS_ARMV8_OP(0x00000000, "urhadd",      OP_ARMV8_A64_URHADD),
  /*0x43:size=10/opcode=00011*/     DIS_ARMV8_OP(0x00000000, "bit",         OP_ARMV8_A64_BIT),
  /*0x44:size=10/opcode=00100*/     DIS_ARMV8_OP(0x00000000, "uhsub",       OP_ARMV8_A64_UHSUB),
  /*0x45:size=10/opcode=00101*/     DIS_ARMV8_OP(0x00000000, "uqsub",       OP_ARMV8_A64_UQSUB),
  /*0x46:size=10/opcode=00110*/     DIS_ARMV8_OP(0x00000000, "cmhi",        OP_ARMV8_A64_CMHI),
  /*0x47:size=10/opcode=00111*/     DIS_ARMV8_OP(0x00000000, "cmhs",        OP_ARMV8_A64_CMHS),
  /*0x48:size=10/opcode=01000*/     DIS_ARMV8_OP(0x00000000, "ushl",        OP_ARMV8_A64_USHL),
  /*0x49:size=10/opcode=01001*/     DIS_ARMV8_OP(0x00000000, "uqshl",       OP_ARMV8_A64_UQSHL),
  /*0x4a:size=10/opcode=01010*/     DIS_ARMV8_OP(0x00000000, "urshl",       OP_ARMV8_A64_URSHL),
  /*0x4b:size=10/opcode=01011*/     DIS_ARMV8_OP(0x00000000, "uqrshl",      OP_ARMV8_A64_UQRSHL),
  /*0x4c:size=10/opcode=01100*/     DIS_ARMV8_OP(0x00000000, "umax",        OP_ARMV8_A64_UMAX),
  /*0x4d:size=10/opcode=01101*/     DIS_ARMV8_OP(0x00000000, "umin",        OP_ARMV8_A64_UMIN),
  /*0x4e:size=10/opcode=01110*/     DIS_ARMV8_OP(0x00000000, "uadd",        OP_ARMV8_A64_UADD),
  /*0x4f:size=10/opcode=01111*/     DIS_ARMV8_OP(0x00000000, "uaba",        OP_ARMV8_A64_UABA),
  /*0x50:size=10/opcode=10000*/     DIS_ARMV8_OP(0x00000000, "sub",         OP_ARMV8_A64_SUB),
  /*0x51:size=10/opcode=10001*/     DIS_ARMV8_OP(0x00000000, "cmeq",        OP_ARMV8_A64_CMEQ),
  /*0x52:size=10/opcode=10010*/     DIS_ARMV8_OP(0x00000000, "mls",         OP_ARMV8_A64_MLS),
  /*0x53:size=10/opcode=10011*/     DIS_ARMV8_OP(0x00000000, "pmul",        OP_ARMV8_A64_PMUL),
  /*0x54:size=10/opcode=10100*/     DIS_ARMV8_OP(0x00000000, "umaxp",       OP_ARMV8_A64_UMAXP),
  /*0x55:size=10/opcode=10101*/     DIS_ARMV8_OP(0x00000000, "uminp",       OP_ARMV8_A64_UMINP),
  /*0x56:size=10/opcode=10110*/     DIS_ARMV8_OP(0x00000000, "sqrdmulh",    OP_ARMV8_A64_SQRDMULH),
  /*0x57:size=10/opcode=10111*/     INVALID_OPCODE,
  /*0x58:size=10/opcode=11000*/     DIS_ARMV8_OP(0x00000000, "fminnmp",     OP_ARMV8_A64_FMINNMP),
  /*0x59:size=10/opcode=11001*/     DIS_ARMV8_OP(0x00000000, "fmlsl2",      OP_ARMV8_A64_FMLSL2),
  /*0x5a:size=10/opcode=11010*/     DIS_ARMV8_OP(0x00000000, "fabd",        OP_ARMV8_A64_FABD),
  /*0x5b:size=10/opcode=11011*/     DIS_ARMV8_OP(0x00000000, "famin",       OP_ARMV8_A64_FAMIN),
  /*0x5c:size=10/opcode=11100*/     DIS_ARMV8_OP(0x00000000, "fcmgt",       OP_ARMV8_A64_FCMGT),
  /*0x5d:size=10/opcode=11101*/     DIS_ARMV8_OP(0x00000000, "facgt",       OP_ARMV8_A64_FACGT),
  /*0x5e:size=10/opcode=11110*/     DIS_ARMV8_OP(0x00000000, "fminp",       OP_ARMV8_A64_FMINP),
  /*0x5f:size=10/opcode=11111*/     DIS_ARMV8_OP(0x00000000, "fscale",      OP_ARMV8_A64_FSCALE),
  /*0x60:size=11/opcode=00000*/     DIS_ARMV8_OP(0x00000000, "uhadd",       OP_ARMV8_A64_UHADD),
  /*0x61:size=11/opcode=00001*/     DIS_ARMV8_OP(0x00000000, "uqadd",       OP_ARMV8_A64_UQADD),
  /*0x62:size=11/opcode=00010*/     DIS_ARMV8_OP(0x00000000, "urhadd",      OP_ARMV8_A64_URHADD),
  /*0x63:size=11/opcode=00011*/     DIS_ARMV8_OP(0x00000000, "bif",         OP_ARMV8_A64_BIF),
  /*0x64:size=11/opcode=00100*/     DIS_ARMV8_OP(0x00000000, "uhsub",       OP_ARMV8_A64_UHSUB),
  /*0x65:size=11/opcode=00101*/     DIS_ARMV8_OP(0x00000000, "uqsub",       OP_ARMV8_A64_UQSUB),
  /*0x66:size=11/opcode=00110*/     DIS_ARMV8_OP(0x00000000, "cmhi",        OP_ARMV8_A64_CMHI),
  /*0x67:size=11/opcode=00111*/     DIS_ARMV8_OP(0x00000000, "cmhs",        OP_ARMV8_A64_CMHS),
  /*0x68:size=11/opcode=01000*/     DIS_ARMV8_OP(0x00000000, "ushl",        OP_ARMV8_A64_USHL),
  /*0x69:size=11/opcode=01001*/     DIS_ARMV8_OP(0x00000000, "uqshl",       OP_ARMV8_A64_UQSHL),
  /*0x6a:size=11/opcode=01010*/     DIS_ARMV8_OP(0x00000000, "urshl",       OP_ARMV8_A64_URSHL),
  /*0x6b:size=11/opcode=01011*/     DIS_ARMV8_OP(0x00000000, "uqrshl",      OP_ARMV8_A64_UQRSHL),
  /*0x6c:size=11/opcode=01100*/     DIS_ARMV8_OP(0x00000000, "umax",        OP_ARMV8_A64_UMAX),
  /*0x6d:size=11/opcode=01101*/     DIS_ARMV8_OP(0x00000000, "umin",        OP_ARMV8_A64_UMIN),
  /*0x6e:size=11/opcode=01110*/     DIS_ARMV8_OP(0x00000000, "uadd",        OP_ARMV8_A64_UADD),
  /*0x6f:size=11/opcode=01111*/     DIS_ARMV8_OP(0x00000000, "uaba",        OP_ARMV8_A64_UABA),
  /*0x70:size=11/opcode=10000*/     DIS_ARMV8_OP(0x00000000, "sub",         OP_ARMV8_A64_SUB),
  /*0x71:size=11/opcode=10001*/     DIS_ARMV8_OP(0x00000000, "cmeq",        OP_ARMV8_A64_CMEQ),
  /*0x72:size=11/opcode=10010*/     DIS_ARMV8_OP(0x00000000, "mls",         OP_ARMV8_A64_MLS),
  /*0x73:size=11/opcode=10011*/     DIS_ARMV8_OP(0x00000000, "pmul",        OP_ARMV8_A64_PMUL),
  /*0x74:size=11/opcode=10100*/     DIS_ARMV8_OP(0x00000000, "umaxp",       OP_ARMV8_A64_UMAXP),
  /*0x75:size=11/opcode=10101*/     DIS_ARMV8_OP(0x00000000, "uminp",       OP_ARMV8_A64_UMINP),
  /*0x76:size=11/opcode=10110*/     DIS_ARMV8_OP(0x00000000, "sqrdmulh",    OP_ARMV8_A64_SQRDMULH),
  /*0x77:size=11/opcode=10111*/     INVALID_OPCODE,
  /*0x78:size=11/opcode=11000*/     DIS_ARMV8_OP(0x00000000, "fminnmp",     OP_ARMV8_A64_FMINNMP),
  /*0x79:size=11/opcode=11001*/     INVALID_OPCODE,
  /*0x7a:size=11/opcode=11010*/     DIS_ARMV8_OP(0x00000000, "fabd",        OP_ARMV8_A64_FABD),
  /*0x7b:size=11/opcode=11011*/     DIS_ARMV8_OP(0x00000000, "famin",       OP_ARMV8_A64_FAMIN),
  /*0x7c:size=11/opcode=11100*/     DIS_ARMV8_OP(0x00000000, "fcmgt",       OP_ARMV8_A64_FCMGT),
  /*0x7d:size=11/opcode=11101*/     DIS_ARMV8_OP(0x00000000, "facgt",       OP_ARMV8_A64_FACGT),
  /*0x7e:size=11/opcode=11110*/     DIS_ARMV8_OP(0x00000000, "fminp",       OP_ARMV8_A64_FMINP),
  /*0x7f:size=11/opcode=11111*/     DIS_ARMV8_OP(0x00000000, "fscale",      OP_ARMV8_A64_FSCALE),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdThreeSame_U1, 0x00000000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate, UINT32_C(0x00c0f800), 11);


/*
 * C4.1.95.25 - Data Processing - Advanced SIMD modified immediate
 *
 * Indexing size (bits 22 & 23) and opcode (bits 11 thru 15):
 *      - bits 0-4 = opcode; 0x00..0x1f
 *      - bits 5-6 = size;   0x00..0x60
 *      - total 7 bits;      0x00..0x7f
 */

DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdModifiedImm_Op0)
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmLo,     5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmHi,    16,  3, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdCMode8,   12,  4, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,            30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,           0,  5, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcAdvSimdModifiedImm_Op0_FpSingle)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegTypeFixed, kDisOpParamArmV8VecRegType_2S, 0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,            30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmLoFp,   5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmHiFp,  16,  3, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcAdvSimdModifiedImm_Op0_FpHalf)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegTypeFixed, kDisOpParamArmV8VecRegType_4H, 0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,            30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmLoFp,   5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmHiFp,  16,  3, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdModifiedImm_Op0)
  /*0x00/cmode=0000/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x01/cmode=0000/o2=1*/  INVALID_OPCODE,
  /*0x02/cmode=0001/o2=0*/  DIS_ARMV8_OP(           0, "orr",  OP_ARMV8_A64_ORR),
  /*0x03/cmode=0001/o2=1*/  INVALID_OPCODE,
  /*0x04/cmode=0010/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x05/cmode=0010/o2=1*/  INVALID_OPCODE,
  /*0x06/cmode=0011/o2=0*/  DIS_ARMV8_OP(           0, "orr",  OP_ARMV8_A64_ORR),
  /*0x07/cmode=0011/o2=1*/  INVALID_OPCODE,
  /*0x08/cmode=0100/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x09/cmode=0100/o2=1*/  INVALID_OPCODE,
  /*0x0a/cmode=0101/o2=0*/  DIS_ARMV8_OP(           0, "orr",  OP_ARMV8_A64_ORR),
  /*0x0b/cmode=0101/o2=1*/  INVALID_OPCODE,
  /*0x0c/cmode=0110/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x0d/cmode=0110/o2=1*/  INVALID_OPCODE,
  /*0x0e/cmode=0111/o2=0*/  DIS_ARMV8_OP(           0, "orr",  OP_ARMV8_A64_ORR),
  /*0x0f/cmode=0111/o2=1*/  INVALID_OPCODE,
  /*0x10/cmode=1000/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x11/cmode=1000/o2=1*/  INVALID_OPCODE,
  /*0x12/cmode=1001/o2=0*/  DIS_ARMV8_OP(           0, "orr",  OP_ARMV8_A64_ORR),
  /*0x13/cmode=1001/o2=1*/  INVALID_OPCODE,
  /*0x14/cmode=1010/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x15/cmode=1010/o2=1*/  INVALID_OPCODE,
  /*0x16/cmode=1011/o2=0*/  DIS_ARMV8_OP(           0, "orr",  OP_ARMV8_A64_ORR),
  /*0x17*cmode=1011/o2=1*/  INVALID_OPCODE,
  /*0x18*cmode=1100/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x19*cmode=1100/o2=1*/  INVALID_OPCODE,
  /*0x1a*cmode=1101/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x1b*cmode=1101/o2=1*/  INVALID_OPCODE,
  /*0x1c*cmode=1110/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x1d*cmode=1110/o2=1*/  INVALID_OPCODE,
  /*0x1e*cmode=1111/o2=0*/  DIS_ARMV8_OP_ALT_DECODE(0, "fmov", OP_ARMV8_A64_FMOV, DataProcAdvSimdModifiedImm_Op0_FpSingle),
  /*0x1f*cmode=1111/o2=1*/  DIS_ARMV8_OP_ALT_DECODE(0, "fmov", OP_ARMV8_A64_FMOV, DataProcAdvSimdModifiedImm_Op0_FpHalf),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdModifiedImm_Op0, 0 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop, UINT32_C(0x0000f800), 11);


DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdModifiedImm_Op1_Q0)
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmLo,     5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmHi,    16,  3, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdCMode64,  12,  4, 1 /*idxParam*/),
    //DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,            30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,           0,  5, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdModifiedImm_Op1_Q0)
  /*0x00/cmode=0000/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x01/cmode=0000/o2=1*/  INVALID_OPCODE,
  /*0x02/cmode=0001/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x03/cmode=0001/o2=1*/  INVALID_OPCODE,
  /*0x04/cmode=0010/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x05/cmode=0010/o2=1*/  INVALID_OPCODE,
  /*0x06/cmode=0011/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x07/cmode=0011/o2=1*/  INVALID_OPCODE,
  /*0x08/cmode=0100/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x09/cmode=0100/o2=1*/  INVALID_OPCODE,
  /*0x0a/cmode=0101/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x0b/cmode=0101/o2=1*/  INVALID_OPCODE,
  /*0x0c/cmode=0110/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x0d/cmode=0110/o2=1*/  INVALID_OPCODE,
  /*0x0e/cmode=0111/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x0f/cmode=0111/o2=1*/  INVALID_OPCODE,
  /*0x10/cmode=1000/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x11/cmode=1000/o2=1*/  INVALID_OPCODE,
  /*0x12/cmode=1001/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x13/cmode=1001/o2=1*/  INVALID_OPCODE,
  /*0x14/cmode=1010/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x15/cmode=1010/o2=1*/  INVALID_OPCODE,
  /*0x16/cmode=1011/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x17*cmode=1011/o2=1*/  INVALID_OPCODE,
  /*0x18*cmode=1100/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x19*cmode=1100/o2=1*/  INVALID_OPCODE,
  /*0x1a*cmode=1101/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x1b*cmode=1101/o2=1*/  INVALID_OPCODE,
  /*0x1c*cmode=1110/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x1d*cmode=1110/o2=1*/  INVALID_OPCODE,
  /*0x1e*cmode=1111/o2=0*/  INVALID_OPCODE,
  /*0x1f*cmode=1111/o2=1*/  INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdModifiedImm_Op1_Q0, 0 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop, UINT32_C(0x0000f800), 11);


DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcAdvSimdModifiedImm_Op1_Q1)
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmLo,     5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmHi,    16,  3, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdCMode64,  12,  4, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecQ,            30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,           0,  5, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcAdvSimdModifiedImm_Op1_Q1_FpDouble)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegTypeFixed, kDisOpParamArmV8VecRegType_2D, 0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmLoFp,   5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAdvSimdImmHiFp,  16,  3, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcAdvSimdModifiedImm_Op1_Q1)
  /*0x00/cmode=0000/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x01/cmode=0000/o2=1*/  INVALID_OPCODE,
  /*0x02/cmode=0001/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x03/cmode=0001/o2=1*/  INVALID_OPCODE,
  /*0x04/cmode=0010/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x05/cmode=0010/o2=1*/  INVALID_OPCODE,
  /*0x06/cmode=0011/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x07/cmode=0011/o2=1*/  INVALID_OPCODE,
  /*0x08/cmode=0100/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x09/cmode=0100/o2=1*/  INVALID_OPCODE,
  /*0x0a/cmode=0101/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x0b/cmode=0101/o2=1*/  INVALID_OPCODE,
  /*0x0c/cmode=0110/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x0d/cmode=0110/o2=1*/  INVALID_OPCODE,
  /*0x0e/cmode=0111/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x0f/cmode=0111/o2=1*/  INVALID_OPCODE,
  /*0x10/cmode=1000/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x11/cmode=1000/o2=1*/  INVALID_OPCODE,
  /*0x12/cmode=1001/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x13/cmode=1001/o2=1*/  INVALID_OPCODE,
  /*0x14/cmode=1010/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x15/cmode=1010/o2=1*/  INVALID_OPCODE,
  /*0x16/cmode=1011/o2=0*/  DIS_ARMV8_OP(           0, "bic",  OP_ARMV8_A64_BIC),
  /*0x17*cmode=1011/o2=1*/  INVALID_OPCODE,
  /*0x18*cmode=1100/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x19*cmode=1100/o2=1*/  INVALID_OPCODE,
  /*0x1a*cmode=1101/o2=0*/  DIS_ARMV8_OP(           0, "mvni", OP_ARMV8_A64_MVNI),
  /*0x1b*cmode=1101/o2=1*/  INVALID_OPCODE,
  /*0x1c*cmode=1110/o2=0*/  DIS_ARMV8_OP(           0, "movi", OP_ARMV8_A64_MOVI),
  /*0x1d*cmode=1110/o2=1*/  INVALID_OPCODE,
  /*0x1e*cmode=1111/o2=0*/  DIS_ARMV8_OP_ALT_DECODE(0, "fmov", OP_ARMV8_A64_FMOV, DataProcAdvSimdModifiedImm_Op1_Q1_FpDouble),
  /*0x1f*cmode=1111/o2=1*/  INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcAdvSimdModifiedImm_Op1_Q1, 0 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop, UINT32_C(0x0000f800), 11);


/*
 * C4.1.95 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * op0 = 0xx0 (bit 28 & 31 are fixed at 0) while op3 = xxxxxxxx1 (bit 10 fixed
 * at 1). (Bits 27:25 are fixed to 111).
 *
 * Selecting on op0[2:1]+op1+op2. This has to be split over several tables, though
 * since MAPs can't do disjointed mask (collate).
 */

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFp_b10is1_op0is0000)
  /*0x00:op0=0000/op1=00/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x01:op0=0000/op1=00/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x02:op0=0000/op1=00/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x03:op0=0000/op1=00/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x04:op0=0000/op1=00/op2=0100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x05:op0=0000/op1=00/op2=0101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x06:op0=0000/op1=00/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x07:op0=0000/op1=00/op2=0111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x08:op0=0000/op1=00/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x09:op0=0000/op1=00/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0a:op0=0000/op1=00/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0b:op0=0000/op1=00/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0c:op0=0000/op1=00/op2=1100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x0d:op0=0000/op1=00/op2=1101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x0e:op0=0000/op1=00/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x0f:op0=0000/op1=00/op2=1111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x10:op0=0000/op1=01/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x11:op0=0000/op1=01/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x12:op0=0000/op1=01/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x13:op0=0000/op1=01/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x14:op0=0000/op1=01/op2=0100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x15:op0=0000/op1=01/op2=0101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x16:op0=0000/op1=01/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x17:op0=0000/op1=01/op2=0111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x18:op0=0000/op1=01/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x19:op0=0000/op1=01/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1a:op0=0000/op1=01/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1b:op0=0000/op1=01/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1c:op0=0000/op1=01/op2=1100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x1d:op0=0000/op1=01/op2=1101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x1e:op0=0000/op1=01/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x1f:op0=0000/op1=01/op2=1111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x20:op0=0000/op1=10/op2=0000*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdModifiedImm_Op0),
  /*0x21:op0=0000/op1=10/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x22:op0=0000/op1=10/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x23:op0=0000/op1=10/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x24:op0=0000/op1=10/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x25:op0=0000/op1=10/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x26:op0=0000/op1=10/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x27:op0=0000/op1=10/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x28:op0=0000/op1=10/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x29:op0=0000/op1=10/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2a:op0=0000/op1=10/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2b:op0=0000/op1=10/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2c:op0=0000/op1=10/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2d:op0=0000/op1=10/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2e:op0=0000/op1=10/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2f:op0=0000/op1=10/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x30:op0=0000/op1=11/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x31:op0=0000/op1=11/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x32:op0=0000/op1=11/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x33:op0=0000/op1=11/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x34:op0=0000/op1=11/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x35:op0=0000/op1=11/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x36:op0=0000/op1=11/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x37:op0=0000/op1=11/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x38:op0=0000/op1=11/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x39:op0=0000/op1=11/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3a:op0=0000/op1=11/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3b:op0=0000/op1=11/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3c:op0=0000/op1=11/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3d:op0=0000/op1=11/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3e:op0=0000/op1=11/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3f:op0=0000/op1=11/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFp_b10is1_op0is0000, UINT32_C(0x01f80000), 19);

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFp_b10is1_op0is0010)
  /*0x00:op0=0010/op1=00/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x01:op0=0010/op1=00/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x02:op0=0010/op1=00/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x03:op0=0010/op1=00/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x04:op0=0010/op1=00/op2=0100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x05:op0=0010/op1=00/op2=0101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x06:op0=0010/op1=00/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x07:op0=0010/op1=00/op2=0111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x08:op0=0010/op1=00/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x09:op0=0010/op1=00/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0a:op0=0010/op1=00/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0b:op0=0010/op1=00/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0c:op0=0010/op1=00/op2=1100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x0d:op0=0010/op1=00/op2=1101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x0e:op0=0010/op1=00/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x0f:op0=0010/op1=00/op2=1111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x10:op0=0010/op1=01/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x11:op0=0010/op1=01/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x12:op0=0010/op1=01/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x13:op0=0010/op1=01/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x14:op0=0010/op1=01/op2=0100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x15:op0=0010/op1=01/op2=0101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x16:op0=0010/op1=01/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x17:op0=0010/op1=01/op2=0111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x18:op0=0010/op1=01/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x19:op0=0010/op1=01/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1a:op0=0010/op1=01/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1b:op0=0010/op1=01/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1c:op0=0010/op1=01/op2=1100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x1d:op0=0010/op1=01/op2=1101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x1e:op0=0010/op1=01/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x1f:op0=0010/op1=01/op2=1111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x20:op0=0010/op1=10/op2=0000*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdModifiedImm_Op1_Q0),
  /*0x21:op0=0010/op1=10/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x22:op0=0010/op1=10/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x23:op0=0010/op1=10/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x24:op0=0010/op1=10/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x25:op0=0010/op1=10/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x26:op0=0010/op1=10/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x27:op0=0010/op1=10/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x28:op0=0010/op1=10/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x29:op0=0010/op1=10/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2a:op0=0010/op1=10/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2b:op0=0010/op1=10/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2c:op0=0010/op1=10/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2d:op0=0010/op1=10/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2e:op0=0010/op1=10/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2f:op0=0010/op1=10/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x30:op0=0010/op1=11/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x31:op0=0010/op1=11/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x32:op0=0010/op1=11/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x33:op0=0010/op1=11/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x34:op0=0010/op1=11/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x35:op0=0010/op1=11/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x36:op0=0010/op1=11/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x37:op0=0010/op1=11/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x38:op0=0010/op1=11/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x39:op0=0010/op1=11/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3a:op0=0010/op1=11/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3b:op0=0010/op1=11/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3c:op0=0010/op1=11/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3d:op0=0010/op1=11/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3e:op0=0010/op1=11/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3f:op0=0010/op1=11/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFp_b10is1_op0is0010, UINT32_C(0x01f80000), 19);

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFp_b10is1_op0is0100)
  /*0x00:op0=0100/op1=00/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x01:op0=0100/op1=00/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x02:op0=0100/op1=00/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x03:op0=0100/op1=00/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x04:op0=0100/op1=00/op2=0100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x05:op0=0100/op1=00/op2=0101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x06:op0=0100/op1=00/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x07:op0=0100/op1=00/op2=0111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x08:op0=0100/op1=00/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x09:op0=0100/op1=00/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0a:op0=0100/op1=00/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0b:op0=0100/op1=00/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0c:op0=0100/op1=00/op2=1100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x0d:op0=0100/op1=00/op2=1101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x0e:op0=0100/op1=00/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x0f:op0=0100/op1=00/op2=1111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x10:op0=0100/op1=01/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x11:op0=0100/op1=01/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x12:op0=0100/op1=01/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x13:op0=0100/op1=01/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x14:op0=0100/op1=01/op2=0100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x15:op0=0100/op1=01/op2=0101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x16:op0=0100/op1=01/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x17:op0=0100/op1=01/op2=0111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x18:op0=0100/op1=01/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x19:op0=0100/op1=01/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1a:op0=0100/op1=01/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1b:op0=0100/op1=01/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1c:op0=0100/op1=01/op2=1100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x1d:op0=0100/op1=01/op2=1101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x1e:op0=0100/op1=01/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x1f:op0=0100/op1=01/op2=1111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U0),
  /*0x20:op0=0100/op1=10/op2=0000*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdModifiedImm_Op0),
  /*0x21:op0=0100/op1=10/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x22:op0=0100/op1=10/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x23:op0=0100/op1=10/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x24:op0=0100/op1=10/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x25:op0=0100/op1=10/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x26:op0=0100/op1=10/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x27:op0=0100/op1=10/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x28:op0=0100/op1=10/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x29:op0=0100/op1=10/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2a:op0=0100/op1=10/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2b:op0=0100/op1=10/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2c:op0=0100/op1=10/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2d:op0=0100/op1=10/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2e:op0=0100/op1=10/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2f:op0=0100/op1=10/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x30:op0=0100/op1=11/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x31:op0=0100/op1=11/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x32:op0=0100/op1=11/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x33:op0=0100/op1=11/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x34:op0=0100/op1=11/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x35:op0=0100/op1=11/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x36:op0=0100/op1=11/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x37:op0=0100/op1=11/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x38:op0=0100/op1=11/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x39:op0=0100/op1=11/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3a:op0=0100/op1=11/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3b:op0=0100/op1=11/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3c:op0=0100/op1=11/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3d:op0=0100/op1=11/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3e:op0=0100/op1=11/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3f:op0=0100/op1=11/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFp_b10is1_op0is0100, UINT32_C(0x01f80000), 19);

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFp_b10is1_op0is0110)
  /*0x00:op0=0110/op1=00/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x01:op0=0110/op1=00/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x02:op0=0110/op1=00/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x03:op0=0110/op1=00/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x04:op0=0110/op1=00/op2=0100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x05:op0=0110/op1=00/op2=0101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x06:op0=0110/op1=00/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x07:op0=0110/op1=00/op2=0111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x08:op0=0110/op1=00/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x09:op0=0110/op1=00/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0a:op0=0110/op1=00/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0b:op0=0110/op1=00/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x0c:op0=0110/op1=00/op2=1100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x0d:op0=0110/op1=00/op2=1101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x0e:op0=0110/op1=00/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x0f:op0=0110/op1=00/op2=1111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x10:op0=0110/op1=01/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x11:op0=0110/op1=01/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x12:op0=0110/op1=01/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x13:op0=0110/op1=01/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x14:op0=0110/op1=01/op2=0100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x15:op0=0110/op1=01/op2=0101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x16:op0=0110/op1=01/op2=0110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x17:op0=0110/op1=01/op2=0111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x18:op0=0110/op1=01/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x19:op0=0110/op1=01/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1a:op0=0110/op1=01/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1b:op0=0110/op1=01/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x1c:op0=0110/op1=01/op2=1100*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x1d:op0=0110/op1=01/op2=1101*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x1e:op0=0110/op1=01/op2=1110*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x1f:op0=0110/op1=01/op2=1111*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdThreeSame_U1),
  /*0x20:op0=0110/op1=10/op2=0000*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcAdvSimdModifiedImm_Op1_Q1),
  /*0x21:op0=0110/op1=10/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x22:op0=0110/op1=10/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x23:op0=0110/op1=10/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x24:op0=0110/op1=10/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x25:op0=0110/op1=10/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x26:op0=0110/op1=10/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x27:op0=0110/op1=10/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x28:op0=0110/op1=10/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x29:op0=0110/op1=10/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2a:op0=0110/op1=10/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2b:op0=0110/op1=10/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2c:op0=0110/op1=10/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2d:op0=0110/op1=10/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2e:op0=0110/op1=10/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x2f:op0=0110/op1=10/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x30:op0=0110/op1=11/op2=0000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x31:op0=0110/op1=11/op2=0001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x32:op0=0110/op1=11/op2=0010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x33:op0=0110/op1=11/op2=0011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x34:op0=0110/op1=11/op2=0100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x35:op0=0110/op1=11/op2=0101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x36:op0=0110/op1=11/op2=0110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x37:op0=0110/op1=11/op2=0111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x38:op0=0110/op1=11/op2=1000*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x39:op0=0110/op1=11/op2=1001*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3a:op0=0110/op1=11/op2=1010*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3b:op0=0110/op1=11/op2=1011*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3c:op0=0110/op1=11/op2=1100*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3d:op0=0110/op1=11/op2=1101*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3e:op0=0110/op1=11/op2=1110*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
  /*0x3f:op0=0110/op1=11/op2=1111*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFp_b10is1_op0is0110, UINT32_C(0x01f80000), 19);

DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_0_Bit31_0_b10is1)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFp_b10is1_op0is0000),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFp_b10is1_op0is0010),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFp_b10is1_op0is0100),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFp_b10is1_op0is0110),
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFpBit28_0_Bit31_0_b10is1, UINT32_C(0x60000000), 29);


/*
 * C4.1.95 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) and bit 31 (op0<3>) are already fixed at 0 at this point.
 * (Bits 27:25 are fixed to 111).
 *
 * Differentiate further based on the op3<0> field.
 * Splitting this up because the decoding would get insane otherwise with tables doing cross referencing...
 *
 *     Bit  10
 *     +-------------------------------------------
 *           0 Advanced SIMD table lookup/permute/extract/copy/three same (FP16)/two-register miscellaneous (FP16)/ three-register extension
 *                           two-register miscellaneous/across lanes/three different/three same/modified immediate/shift by immediate/vector x indexed element/
 *             Cryptographic AES
 *           1 Cryptographic three-register, imm2/three register SHA 512/four-register/two-register SHA 512
 *             XAR
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_0_31_0)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_0_Bit31_0_b10is0),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_0_Bit31_0_b10is1),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcSimdFpBit28_0_31_0, 10);



/*
 * C4.1.96 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) is already fixed at 0 at this point.
 *
 * Differentiate further based on the op0<3> field.
 * Splitting this up because the decoding would get insane otherwise with tables doing cross referencing...
 *
 *     Bit  31
 *     +-------------------------------------------
 *           0 Advanced SIMD table lookup/permute/extract/copy/three same (FP16)/two-register miscellaneous (FP16)/ three-register extension
 *                           two-register miscellaneous/across lanes/three different/three same/modified immediate/shift by immediate/vector x indexed element/
 *             Cryptographic AES
 *           1 Cryptographic three-register, imm2/three register SHA 512/four-register/two-register SHA 512
 *             XAR
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_0)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_0_31_0),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                     /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcSimdFpBit28_0, 31);



/*
 * SCVTF/UCVTF.
 *
 * Note: The opcode is selected based on the <opcode> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpFixedPConvGpr2FpReg)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSf,             31,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpScale,        10,  6, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpFixedPConvGpr2FpReg)
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x1e020000, "scvtf",           OP_ARMV8_A64_SCVTF),
    DIS_ARMV8_OP(0x1e030000, "ucvtf",           OP_ARMV8_A64_UCVTF),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpFixedPConvGpr2FpReg, 0x7f3f0000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(16) | RT_BIT_32(17) | RT_BIT_32(18), 16);


/*
 * FCVTZS/FCVTZU.
 *
 * Note: The opcode is selected based on the <opcode> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpFixedPConvFpReg2Gpr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSf,             31,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpScale,        10,  6, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpFixedPConvFpReg2Gpr)
    DIS_ARMV8_OP(0x1e180000, "fcvtzs",          OP_ARMV8_A64_FCVTZS),
    DIS_ARMV8_OP(0x1e190000, "fcvtzu",          OP_ARMV8_A64_FCVTZU),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpFixedPConvFpReg2Gpr, 0x7f3f0000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(16) | RT_BIT_32(17) | RT_BIT_32(18), 16);


/*
 * C4.1.96.32 - Conversion between floating-point and fixed-point
 *
 * Bit 28 (op0<0>) is already fixed at 1 at this point.
 * Bit 30 (op0<2>) is already fixed at 0 at this point.
 * Bit 24 (op1<1>) is already fixed at 0 at this point.
 * Bit 21 (op2<2>) is already fixed at 0 at this point.
 *
 * Differentiate further based on the rmode field.
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcFpFixedPConv)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpFixedPConvGpr2FpReg),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpFixedPConvFpReg2Gpr),
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcFpFixedPConv, RT_BIT_32(19) | RT_BIT_32(20), 19);


/*
 * C4.1.95.33 - Conversion between floating-point and integer.
 *
 * FCVTNS/FCVTNU/SCVTF/UCVTF/FCVTAS/FCVTAU/FMOV/FCVTPS/FCVTPU/FCVTMS/FCVTMU/FCVTZS/FCVTZU.
 *
 * The opcode is selected based on the <rmode>:<opcode> fields.
 *
 * To complicate matters, fmode=10 is used to encode FMOV to/from the upper
 * half of 128-bit registers.  So, it has to be sorted out separately to avoid
 * including fmod in the selector mask.
 */
/* bits 30:29 = 00, bit 28 = 1, bit 24 = 0, bits 23:22 != 10, bit 21 = 1, bits 15:10 = 000000 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpConvIntNormal)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSf,             31,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcFpConvIntNormal_ToFp)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSf,             31,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpConvIntNormal)
    /*rmode=00/opcode=000:*/ DIS_ARMV8_OP(           0x00000000, "fcvtns",      OP_ARMV8_A64_FCVTNS),
    /*rmode=00/opcode=001:*/ DIS_ARMV8_OP(           0x00000000, "fcvtnu",      OP_ARMV8_A64_FCVTNU),
    /*rmode=00/opcode=010:*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "scvtf",       OP_ARMV8_A64_SCVTF,     DataProcFpConvIntNormal_ToFp),
    /*rmode=00/opcode=011:*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "ucvtf",       OP_ARMV8_A64_UCVTF,     DataProcFpConvIntNormal_ToFp),
    /*rmode=00/opcode=100:*/ DIS_ARMV8_OP(           0x00000000, "fcvtas",      OP_ARMV8_A64_FCVTAS),
    /*rmode=00/opcode=101:*/ DIS_ARMV8_OP(           0x00000000, "fcvtau",      OP_ARMV8_A64_FCVTAU),
    /*rmode=00/opcode=110:*/ DIS_ARMV8_OP(           0x00000000, "fmov",        OP_ARMV8_A64_FMOV),
    /*rmode=00/opcode=111:*/ DIS_ARMV8_OP_ALT_DECODE(0x00000000, "fmov",        OP_ARMV8_A64_FMOV,      DataProcFpConvIntNormal_ToFp),
    /*rmode=01/opcode=000:*/ DIS_ARMV8_OP(           0x00000000, "fcvtps",      OP_ARMV8_A64_FCVTPS),
    /*rmode=01/opcode=001:*/ DIS_ARMV8_OP(           0x00000000, "fcvtpu",      OP_ARMV8_A64_FCVTPU),
    /*rmode=01/opcode=010:*/ INVALID_OPCODE,
    /*rmode=01/opcode=011:*/ INVALID_OPCODE,
    /*rmode=01/opcode=100:*/ INVALID_OPCODE,
    /*rmode=01/opcode=101:*/ INVALID_OPCODE,
    /*rmode=01/opcode=110:*/ INVALID_OPCODE,
    /*rmode=01/opcode=111:*/ INVALID_OPCODE,
    /*rmode=10/opcode=000:*/ DIS_ARMV8_OP(           0x00000000, "fcvtms",      OP_ARMV8_A64_FCVTMS),
    /*rmode=10/opcode=001:*/ DIS_ARMV8_OP(           0x00000000, "fcvtmu",      OP_ARMV8_A64_FCVTMU),
    /*rmode=10/opcode=010:*/ INVALID_OPCODE,
    /*rmode=10/opcode=011:*/ INVALID_OPCODE,
    /*rmode=10/opcode=100:*/ INVALID_OPCODE,
    /*rmode=10/opcode=101:*/ INVALID_OPCODE,
    /*rmode=10/opcode=110:*/ INVALID_OPCODE,
    /*rmode=10/opcode=111:*/ INVALID_OPCODE,
    /*rmode=11/opcode=000:*/ INVALID_OPCODE,
    /*rmode=11/opcode=001:*/ INVALID_OPCODE,
    /*rmode=11/opcode=010:*/ INVALID_OPCODE,
    /*rmode=11/opcode=011:*/ INVALID_OPCODE,
    /*rmode=11/opcode=100:*/ INVALID_OPCODE,
    /*rmode=11/opcode=101:*/ INVALID_OPCODE,
    /*rmode=11/opcode=110:*/ INVALID_OPCODE,
    /*rmode=11/opcode=111:*/ INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpConvIntNormal, 0 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(16) | RT_BIT_32(17) | RT_BIT_32(18) | RT_BIT_32(19) | RT_BIT_32(20), 16);

/* bits 30:29 = 00, bit 28 = 1, bit 24 = 0, bit 23 = 1, bit 22 = 0, bit 21 = 1, bits 15:10 = 000000 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpConvIntTop128)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,         0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegTypeFixed, kDisOpParamArmV8VecRegType_D_Elem1, 0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(DataProcFpConvIntTop128_ToFp)
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecRegTypeFixed, kDisOpParamArmV8VecRegType_D_Elem1, 0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseVecReg,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,         5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpConvIntTop128)
    /*opcode=xx0:*/ DIS_ARMV8_OP(           0x9eae0000, "fmov",       OP_ARMV8_A64_FMOV),
    /*opcode=xx1:*/ DIS_ARMV8_OP_ALT_DECODE(0x9eaf0000, "fmov",       OP_ARMV8_A64_FMOV,     DataProcFpConvIntTop128_ToFp),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpConvIntTop128, 0xfffffc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop, RT_BIT_32(16), 16);

/* bits 30:29 = 00, bit 28 = 1, bit 24 = 0, bit 21 = 1, bits 15:10 = 000000 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcFpConvIntByFMode)
    /*fmode=00:*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpConvIntNormal),
    /*fmode=01:*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpConvIntNormal),
    /*fmode=10:*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpConvIntTop128),
    /*fmode=11:*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpConvIntNormal),
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcFpConvIntByFMode, 0x00c00000, 22);

/* bit 30 = 0, bit 28 = 1, bit 24 = 0, bit 21 = 1, bits 15:10 = 000000 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcFpConvInt)
    /*S=0:*/ DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpConvIntByFMode),
    /*S=1:*/ DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcFpConvInt, 29);



/*
 * FCSEL.
 *
 * Note: The opcode is selected based on the <opcode> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpCondSelect)
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,          16,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseCond,           12,  4, 3 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpCondSelect)
    DIS_ARMV8_OP(0x1e200c00, "fcsel",           OP_ARMV8_A64_FCSEL),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpCondSelect, 0xff200c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(29), 29);


/*
 * FMUL/FDIV/FADD/FSUB/FMAX/FMIN/FMAXNM/FMINNM.
 *
 * Note: The opcode is selected based on the <opcode> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpDataProc2Src)
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,          16,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpDataProc2Src)
    DIS_ARMV8_OP(0x1e200800, "fmul",            OP_ARMV8_A64_FMUL),
    DIS_ARMV8_OP(0x1e201800, "fdiv",            OP_ARMV8_A64_FDIV),
    DIS_ARMV8_OP(0x1e202800, "fadd",            OP_ARMV8_A64_FADD),
    DIS_ARMV8_OP(0x1e203800, "fsub",            OP_ARMV8_A64_FSUB),
    DIS_ARMV8_OP(0x1e204800, "fmax",            OP_ARMV8_A64_FMAX),
    DIS_ARMV8_OP(0x1e205800, "fmin",            OP_ARMV8_A64_FMIN),
    DIS_ARMV8_OP(0x1e206800, "fmaxnm",          OP_ARMV8_A64_FMAXNM),
    DIS_ARMV8_OP(0x1e207800, "fminnm",          OP_ARMV8_A64_FMINNM),
    DIS_ARMV8_OP(0x1e208800, "fnmul",           OP_ARMV8_A64_FNMUL),
    /* Rest of the 4 bit block is invalid */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpDataProc2Src, 0xff20fc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(12) | RT_BIT_32(13) | RT_BIT_32(14) | RT_BIT_32(15), 12);


/*
 * C4.1.96.34 - Floating-point data-processing (1 source).
 *
 * FMOV/FABS/FNEG/FSQRT/FCVT/FRINTN/FRINTP/FRINTM/FRINTZ/FRINA/FRINTX/FRINTI/FRINT32Z/FRINT32X/FRINT64Z/FRINT64X.
 *
 * Note: The opcode is selected based on the <opcode> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpDataProc1Src)
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpFixupFCvt,     0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpDataProc1Src)
    DIS_ARMV8_OP(0x1e204000, "fmov",            OP_ARMV8_A64_FMOV),
    DIS_ARMV8_OP(0x1e20c000, "fabs",            OP_ARMV8_A64_FABS),
    DIS_ARMV8_OP(0x1e214000, "fneg",            OP_ARMV8_A64_FNEG),
    DIS_ARMV8_OP(0x1e21c000, "fsqrt",           OP_ARMV8_A64_FSQRT),
    DIS_ARMV8_OP(0x1e224000, "fcvt",            OP_ARMV8_A64_FCVT),
    DIS_ARMV8_OP(0x1e22c000, "fcvt",            OP_ARMV8_A64_FCVT),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x1e23c000, "fcvt",            OP_ARMV8_A64_FCVT),
    DIS_ARMV8_OP(0x1e244000, "frintn",          OP_ARMV8_A64_FRINTN),
    DIS_ARMV8_OP(0x1e24c000, "frintp",          OP_ARMV8_A64_FRINTP),
    DIS_ARMV8_OP(0x1e254000, "frintm",          OP_ARMV8_A64_FRINTM),
    DIS_ARMV8_OP(0x1e25c000, "frintz",          OP_ARMV8_A64_FRINTZ),
    DIS_ARMV8_OP(0x1e264000, "frinta",          OP_ARMV8_A64_FRINTA),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x1e274000, "frintx",          OP_ARMV8_A64_FRINTX),
    DIS_ARMV8_OP(0x1e27c000, "frinti",          OP_ARMV8_A64_FRINTI),
    DIS_ARMV8_OP(0x1e284000, "frint32z",        OP_ARMV8_A64_FRINT32Z),
    DIS_ARMV8_OP(0x1e28c000, "frint32x",        OP_ARMV8_A64_FRINT32X),
    DIS_ARMV8_OP(0x1e294000, "frint64z",        OP_ARMV8_A64_FRINT64Z),
    DIS_ARMV8_OP(0x1e29c000, "frint64x",        OP_ARMV8_A64_FRINT64X),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpDataProc1Src, 0xff3ffc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(15) | RT_BIT_32(16) | RT_BIT_32(17) | RT_BIT_32(18) | RT_BIT_32(19) | RT_BIT_32(20), 15);


/*
 * C4.1.96.35 - Floating-point compare.
 *
 * FCMP/FCMPE.
 *
 * Note: The opcode is selected based on the op2<3:4> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpCmpReg)
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,          16,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpCmpReg)
    DIS_ARMV8_OP(0x1e202000, "fcmp",            OP_ARMV8_A64_FCMP),
    DIS_ARMV8_OP(0x1e202010, "fcmpe",           OP_ARMV8_A64_FCMPE),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpCmpReg, 0xff20fc1f /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(4), 4);


/*
 * C4.1.96.35 - Floating-point compare.
 *
 * FCMP/FCMPE.
 *
 * Note: The opcode is selected based on the op2<3:4> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpCmpZero)
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmZero,         0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpCmpZero)
    DIS_ARMV8_OP(0x1e202008, "fcmp",            OP_ARMV8_A64_FCMP),
    DIS_ARMV8_OP(0x1e202018, "fcmpe",           OP_ARMV8_A64_FCMPE),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpCmpZero, 0xff20fc1f /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(4), 4);


/*
 * Floating Point compare, differentiate between register and zero variant.
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcFpCmp)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpCmpReg),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpCmpZero),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcFpCmp, 3);


/*
 * C4.1.96.36 - Floating-point immediate.
 *
 * FMOV.
 *
 * Note: The opcode is selected based on the <op> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpImm)
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,            13,  8, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpImm)
    DIS_ARMV8_OP(0x1e201000, "fmov",            OP_ARMV8_A64_FMOV),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpImm, 0xff201fe0 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(5) | RT_BIT_32(6) | RT_BIT_32(7) | RT_BIT_32(8) | RT_BIT_32(9), 5);


/*
 * C4.1.96.37 - Floating-point conditional compare.
 *
 * FCCMP/FCCMPE.
 *
 * Note: The opcode is selected based on the <op> field.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpCondCmp)
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,          16,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,             0,  4, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseCond,           12,  4, 3 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpCondCmp)
    DIS_ARMV8_OP(0x1e200400, "fccmp",           OP_ARMV8_A64_FCCMP),
    DIS_ARMV8_OP(0x1e200410, "fccmpe",          OP_ARMV8_A64_FCCMPE),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpCondCmp, 0xff200c10 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(4), 4);


/*
 * C4.1.96 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) is already fixed at 1 at this point.
 * Bit 30 (op0<2>) is already fixed at 0 at this point.
 * Bit 24 (op1<1>) is already fixed at 0 at this point.
 * Bit 21 (op2<2>) is already fixed at 1 at this point.
 * Bit 11 (op3<1>) is already fixed at 0 at this point.
 * Bit 10 (op3<0>) is already fixed at 0 at this point.
 *
 * Differentiate further based on the op3<5:2> field.
 *
 *     Bit  15 14 13 12
 *     +-------------------------------------------
 *           0  0  0  0 Conversion between FP and integer
 *           0  0  0  1 FP immediate
 *           0  0  1  0 FP compare
 *           0  0  1  1 FP immediate
 *           0  1  0  0 FP data-processing (1 source)
 *           0  1  0  1 FP immediate
 *           0  1  1  0 FP compare
 *           0  1  1  1 FP immediate
 *           1  0  0  0 UNDEFINED
 *           1  0  0  1 FP immediate
 *           1  0  1  0 FP compare
 *           1  0  1  1 FP immediate
 *           1  1  0  0 FP data-processing (1 source)
 *           1  1  0  1 FP immediate
 *           1  1  1  0 FP compare
 *           1  1  1  1 FP immediate
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_1_30_0_24_0_21_1_11_0_10_0)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpConvInt),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpCmp),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpDataProc1Src),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpCmp),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpImm),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpCmp),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpDataProc1Src),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpCmp),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpImm),
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFpBit28_1_30_0_24_0_21_1_11_0_10_0,
                                RT_BIT_32(12) | RT_BIT_32(13) | RT_BIT_32(14) | RT_BIT_32(15), 12);


/*
 * C4.1.96 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) is already fixed at 1 at this point.
 * Bit 30 (op0<2>) is already fixed at 0 at this point.
 * Bit 24 (op1<1>) is already fixed at 0 at this point.
 * Bit 21 (op2<2>) is already fixed at 1 at this point.
 *
 * Differentiate further based on the op3<1:0> field.
 *
 *     Bit  11 10
 *     +-------------------------------------------
 *           0  0 Conversion between FP and integer / FP data-processing (1 source) / compare / immediate
 *           0  1 FP conditional compare
 *           1  0 FP data processing (2 source)
 *           1  1 FP conditional select
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_1_30_0_24_0_21_1)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_1_30_0_24_0_21_1_11_0_10_0),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpCondCmp),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpDataProc2Src),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpCondSelect),
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcSimdFpBit28_1_30_0_24_0_21_1, RT_BIT_32(10) | RT_BIT_32(11), 10);


/*
 * C4.1.96 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) is already fixed at 1 at this point.
 * Bit 30 (op0<2>) is already fixed at 0 at this point.
 * Bit 24 (op1<1>) is already fixed at 0 at this point.
 *
 * Differentiate further based on the op2<2> field.
 *
 *     Bit  21
 *     +-------------------------------------------
 *           0 Conversion between FP and fixed-point
 *           1 Conversion between FP and integer/FP data-processing (1 source) /
 *             compare / immediate / conditional compare / data-processing (2 source) / conditional select
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_1_30_0_24_0)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpFixedPConv),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_1_30_0_24_0_21_1),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcSimdFpBit28_1_30_0_24_0, 21);


/*
 * FMADD/FMSUB/FNMADD/FNMSUB.
 *
 * Note: The o1,o0 bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcFpDataProc3Src)
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpType,         22,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,          16,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseFpReg,          10,  5, 3 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcFpDataProc3Src)
    DIS_ARMV8_OP(0x1f000000, "fmadd",           OP_ARMV8_A64_FMADD),
    DIS_ARMV8_OP(0x1f008000, "fmsub",           OP_ARMV8_A64_FMSUB),
    DIS_ARMV8_OP(0x1f200000, "fnmadd",          OP_ARMV8_A64_FNMADD),
    DIS_ARMV8_OP(0x1f208000, "fnmsub",          OP_ARMV8_A64_FNMSUB),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcFpDataProc3Src, 0xff208000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(15) | RT_BIT_32(21), 15);


/*
 * C4.1.96 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) is already fixed at 1 at this point.
 * Bit 30 (op0<2>) is already fixed at 0 at this point.
 *
 * Differentiate further based on the op1<1> field.
 *
 *     Bit  24
 *     +-------------------------------------------
 *           0 Conversion between FP and fixed-point/Conversion between FP and integer/
 *             FP data-processing (1 source) / compare / immediate / conditional compare / data-processing (2 source) /
 *             conditional select
 *           1 FP data-processing (3 source)
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_1_30_0)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_1_30_0_24_0),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcFpDataProc3Src),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcSimdFpBit28_1_30_0, 24);


/*
 * C4.1.96.12 - Data Processing - Advanced SIMD scalar shift by immediate
 *
 * FMADD/FMSUB/FNMADD/FNMSUB.
 *
 * Note: The U,opcode bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DataProcSimdScalarShiftByImm)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize64,   0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,   0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,   5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmHImmB,       16,  7, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DataProcSimdScalarShiftByImm)
    DIS_ARMV8_OP(0x5f000400, "sshr",            OP_ARMV8_A64_SSHR),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x5f001400, "ssra",            OP_ARMV8_A64_SSRA),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x5f002400, "srshr",           OP_ARMV8_A64_SRSHR),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x5f003400, "srsra",           OP_ARMV8_A64_SRSRA),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
#if 0 /** @todo */
    DIS_ARMV8_OP(0x5f005400, "shl",             OP_ARMV8_A64_SHL),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x5f007400, "sqshl",           OP_ARMV8_A64_SQSHL),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x5f009400, "sqshrn",          OP_ARMV8_A64_SQSHRN),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x5f009c00, "sqrshrn",         OP_ARMV8_A64_SQRSHRN),
#endif
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(DataProcSimdScalarShiftByImm, 0xff80fc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                         /* opcode */  RT_BIT_32(11) | RT_BIT_32(12) | RT_BIT_32(13) | RT_BIT_32(14) | RT_BIT_32(15)
                         /* U */     | RT_BIT_32(29), 11);


/*
 * C4.1.96 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) is already fixed at 1 at this point.
 * Bit 30 (op0<2>) is already fixed at 1 at this point.
 * Bit 10 (op3<0>) is already fixed at 1 at this point.
 *
 * Differentiate further based on the op1<1> field.
 *
 *     Bit  24
 *     +-------------------------------------------
 *           0 Advanced SIMD scalar copy / scalar three same FP16 / scalar three same /
 *             scalar three same extra
 *           1 Advanced SIMD scalar shift by immediate
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_1_30_1_10_1)
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY, //DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_1_30_1_10_0),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdScalarShiftByImm),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcSimdFpBit28_1_30_1_10_1, 24);


/*
 * C4.1.96 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) is already fixed at 1 at this point.
 * Bit 30 (op0<2>) is already fixed at 1 at this point.
 *
 * Differentiate further based on the op3<0> field.
 *
 *     Bit  10
 *     +-------------------------------------------
 *           0 Cryptographic three-register SHA / two-register SHA
 *             Advanced SIMD scalar two-register miscellaneous FP16 / scalar two-register miscellaneous / scalar pairwise /
 *             scalar three different / scalar x indexed element
 *           1 Advanced SIMD scalar copy / scalar three same FP16 / scalar three same /
 *             scalar shift by immediate / scalar three same extra
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_1_30_1)
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY, //DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_1_30_1_10_0),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_1_30_1_10_1),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcSimdFpBit28_1_30_1, 10);


/*
 * C4.1.96 - Data Processing - Scalar Floating-Point and Advanced SIMD
 *
 * Bit 28 (op0<0>) is already fixed at 1 at this point.
 *
 * Differentiate further based on the op0<2> field.
 *
 *     Bit  30
 *     +-------------------------------------------
 *           0 Conversion between FP and fixed-point/Conversion between FP and integer/
 *             FP data-processing (1 source) / compare / immediate / conditional compare / data-processing (2 source) /
 *             conditional select / data-processing (3 source)
 *           1 Cryptographic three-register SHA / two-register SHA
 *             Advanced SIMD scalar two-register miscellaneous FP16 / scalar two-register miscellaneous / scalar pairwise /
 *             scalar three different / scalar x indexed element / scalar copy / scalar three same FP16 / scalar three same /
 *             scalar shift by immediate / scalar three same extra
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcSimdFpBit28_1)
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_1_30_0),
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcSimdFpBit28_1_30_1),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(DataProcSimdFpBit28_1, 30);
