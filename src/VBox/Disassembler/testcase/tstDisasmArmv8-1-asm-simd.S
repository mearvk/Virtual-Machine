/* $Id: tstDisasmArmv8-1-asm-simd.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - SIMD.
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

BEGINPROC_HIDDEN TestProcA64Simd
    add v0.8B,  v1.8B,  v2.8B
    add v31.8B, v30.8B, v29.8B
    add v0.16B,  v1.16B,  v2.16B
    add v31.16B, v30.16B, v29.16B
    add v0.4H,  v1.4H,  v2.4H
    add v31.4H, v30.4H, v29.4H
    add v0.8H,  v1.8H,  v2.8H
    add v31.8H, v30.8H, v29.8H
    add v0.2S,  v1.2S,  v2.2S
    add v31.2S, v30.2S, v29.2S
    add v0.4S,  v1.4S,  v2.4S
    add v31.4S, v30.4S, v29.4S
    add v0.2D,  v1.2D,  v2.2D
    add v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*add d0,  d1,  d2 */
    /*add d31, d30, d29 */

    addp v0.8B,  v1.8B,  v2.8B
    addp v31.8B, v30.8B, v29.8B
    addp v0.16B,  v1.16B,  v2.16B
    addp v31.16B, v30.16B, v29.16B
    addp v0.4H,  v1.4H,  v2.4H
    addp v31.4H, v30.4H, v29.4H
    addp v0.8H,  v1.8H,  v2.8H
    addp v31.8H, v30.8H, v29.8H
    addp v0.2S,  v1.2S,  v2.2S
    addp v31.2S, v30.2S, v29.2S
    addp v0.4S,  v1.4S,  v2.4S
    addp v31.4S, v30.4S, v29.4S
    addp v0.2D,  v1.2D,  v2.2D
    addp v31.2D, v30.2D, v29.2D

    addv b0, v0.8B
    addv b0, v0.16B
    addv b31, v31.8B
    addv b31, v31.16B
    addv h0, v0.4H
    addv h0, v0.8H
    addv h31, v31.4H
    addv h31, v31.8H
    addv s0, v0.4S
    addv s31, v31.4S

    bic v0.4H, #0    /* , LSL #0 */
    bic v0.4H, #0,   LSL #8
    bic v0.4H, #63   /* , LSL #0 */
    bic v0.4H, #63,  LSL #8
    bic v0.4H, #255  /* , LSL #0 */
    bic v0.4H, #255, LSL #8
    bic v0.8H, #0    /* , LSL #0 */
    bic v0.8H, #0,   LSL #8
    bic v0.8H, #63   /* , LSL #0 */
    bic v0.8H, #63,  LSL #8
    bic v0.8H, #255   /* , LSL #0 */
    bic v0.8H, #255, LSL #8

    bic v0.2S, #0    /* , LSL #0 */
    bic v0.2S, #0,   LSL #8
    bic v0.2S, #0,   LSL #16
    bic v0.2S, #0,   LSL #24
    bic v0.2S, #63   /* , LSL #0 */
    bic v0.2S, #63,  LSL #8
    bic v0.2S, #63,  LSL #16
    bic v0.2S, #63,  LSL #24
    bic v0.2S, #255  /* , LSL #0 */
    bic v0.2S, #255, LSL #8
    bic v0.2S, #255, LSL #16
    bic v0.2S, #255, LSL #24
    bic v0.4S, #0    /* , LSL #0 */
    bic v0.4S, #0,   LSL #8
    bic v0.4S, #0,   LSL #16
    bic v0.4S, #0,   LSL #24
    bic v0.4S, #63   /* , LSL #0 */
    bic v0.4S, #63,  LSL #8
    bic v0.4S, #63,  LSL #16
    bic v0.4S, #63,  LSL #24
    bic v0.4S, #255  /* , LSL #0 */
    bic v0.4S, #255, LSL #8
    bic v0.4S, #255, LSL #16
    bic v0.4S, #255, LSL #24

    /* @todo Fix diassembler tables */
    /*bic v0.8B, v1.8B, v31.8B */
    /*bic v0.16B, v1.16B, v31.16B */

    /* @todo Fix diassembler tables */
    /*bif v0.8B,  v1.8B,  v2.8B */
    /*bif v31.8B, v30.8B, v29.8B */
    /*bif v0.16B,  v1.16B,  v2.16B */
    /*bif v31.16B, v30.16B, v29.16B */

    /* @todo Fix diassembler tables */
    /*bit v0.8B,  v1.8B,  v2.8B */
    /*bit v31.8B, v30.8B, v29.8B */
    /*bit v0.16B,  v1.16B,  v2.16B */
    /*bit v31.16B, v30.16B, v29.16B */

    cmeq v0.8B,  v1.8B,  v2.8B
    cmeq v31.8B, v30.8B, v29.8B
    cmeq v0.16B,  v1.16B,  v2.16B
    cmeq v31.16B, v30.16B, v29.16B
    cmeq v0.4H,  v1.4H,  v2.4H
    cmeq v31.4H, v30.4H, v29.4H
    cmeq v0.8H,  v1.8H,  v2.8H
    cmeq v31.8H, v30.8H, v29.8H
    cmeq v0.2S,  v1.2S,  v2.2S
    cmeq v31.2S, v30.2S, v29.2S
    cmeq v0.4S,  v1.4S,  v2.4S
    cmeq v31.4S, v30.4S, v29.4S
    cmeq v0.2D,  v1.2D,  v2.2D
    cmeq v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*cmeq d0,  d1,  d2 */
    /*cmeq d31, d30, d29 */

    cmge v0.8B,  v1.8B,  v2.8B
    cmge v31.8B, v30.8B, v29.8B
    cmge v0.16B,  v1.16B,  v2.16B
    cmge v31.16B, v30.16B, v29.16B
    cmge v0.4H,  v1.4H,  v2.4H
    cmge v31.4H, v30.4H, v29.4H
    cmge v0.8H,  v1.8H,  v2.8H
    cmge v31.8H, v30.8H, v29.8H
    cmge v0.2S,  v1.2S,  v2.2S
    cmge v31.2S, v30.2S, v29.2S
    cmge v0.4S,  v1.4S,  v2.4S
    cmge v31.4S, v30.4S, v29.4S
    cmge v0.2D,  v1.2D,  v2.2D
    cmge v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*cmge d0,  d1,  d2 */
    /*cmge d31, d30, d29 */

    cmgt v0.8B,  v1.8B,  v2.8B
    cmgt v31.8B, v30.8B, v29.8B
    cmgt v0.16B,  v1.16B,  v2.16B
    cmgt v31.16B, v30.16B, v29.16B
    cmgt v0.4H,  v1.4H,  v2.4H
    cmgt v31.4H, v30.4H, v29.4H
    cmgt v0.8H,  v1.8H,  v2.8H
    cmgt v31.8H, v30.8H, v29.8H
    cmgt v0.2S,  v1.2S,  v2.2S
    cmgt v31.2S, v30.2S, v29.2S
    cmgt v0.4S,  v1.4S,  v2.4S
    cmgt v31.4S, v30.4S, v29.4S
    cmgt v0.2D,  v1.2D,  v2.2D
    cmgt v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*cmgt d0,  d1,  d2 */
    /*cmgt d31, d30, d29 */

    cmhi v0.8B,  v1.8B,  v2.8B
    cmhi v31.8B, v30.8B, v29.8B
    cmhi v0.16B,  v1.16B,  v2.16B
    cmhi v31.16B, v30.16B, v29.16B
    cmhi v0.4H,  v1.4H,  v2.4H
    cmhi v31.4H, v30.4H, v29.4H
    cmhi v0.8H,  v1.8H,  v2.8H
    cmhi v31.8H, v30.8H, v29.8H
    cmhi v0.2S,  v1.2S,  v2.2S
    cmhi v31.2S, v30.2S, v29.2S
    cmhi v0.4S,  v1.4S,  v2.4S
    cmhi v31.4S, v30.4S, v29.4S
    cmhi v0.2D,  v1.2D,  v2.2D
    cmhi v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*cmhi d0,  d1,  d2 */
    /*cmhi d31, d30, d29 */

    cmhs v0.8B,  v1.8B,  v2.8B
    cmhs v31.8B, v30.8B, v29.8B
    cmhs v0.16B,  v1.16B,  v2.16B
    cmhs v31.16B, v30.16B, v29.16B
    cmhs v0.4H,  v1.4H,  v2.4H
    cmhs v31.4H, v30.4H, v29.4H
    cmhs v0.8H,  v1.8H,  v2.8H
    cmhs v31.8H, v30.8H, v29.8H
    cmhs v0.2S,  v1.2S,  v2.2S
    cmhs v31.2S, v30.2S, v29.2S
    cmhs v0.4S,  v1.4S,  v2.4S
    cmhs v31.4S, v30.4S, v29.4S
    cmhs v0.2D,  v1.2D,  v2.2D
    cmhs v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*cmhs d0,  d1,  d2 */
    /*cmhs d31, d30, d29 */

    cmtst v0.8B,  v1.8B,  v2.8B
    cmtst v31.8B, v30.8B, v29.8B
    cmtst v0.16B,  v1.16B,  v2.16B
    cmtst v31.16B, v30.16B, v29.16B
    cmtst v0.4H,  v1.4H,  v2.4H
    cmtst v31.4H, v30.4H, v29.4H
    cmtst v0.8H,  v1.8H,  v2.8H
    cmtst v31.8H, v30.8H, v29.8H
    cmtst v0.2S,  v1.2S,  v2.2S
    cmtst v31.2S, v30.2S, v29.2S
    cmtst v0.4S,  v1.4S,  v2.4S
    cmtst v31.4S, v30.4S, v29.4S
    cmtst v0.2D,  v1.2D,  v2.2D
    cmtst v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*cmtst d0,  d1,  d2 */
    /*cmtst d31, d30, d29 */

    eor v0.8B,  v1.8B,  v2.8B
    eor v31.8B, v30.8B, v29.8B
    eor v0.16B,  v1.16B,  v2.16B
    eor v31.16B, v30.16B, v29.16B

    /* @todo Buggy disassembler table */
    /*fadd v0.2S,  v1.2S,  v2.2S */
    /*fadd v31.2S, v30.2S, v29.2S */
    /*fadd v0.4S,  v1.4S,  v2.4S */
    /*fadd v31.4S, v30.4S, v29.4S */
    /*fadd v0.2D,  v1.2D,  v2.2D */
    /*fadd v31.2D, v30.2D, v29.2D */

    /* @todo Buggy disassembler table */
    /*fmaxnm v0.2S,  v1.2S,  v2.2S */
    /*fmaxnm v31.2S, v30.2S, v29.2S */
    /*fmaxnm v0.4S,  v1.4S,  v2.4S */
    /*fmaxnm v31.4S, v30.4S, v29.4S */
    /*fmaxnm v0.2D,  v1.2D,  v2.2D */
    /*fmaxnm v31.2D, v30.2D, v29.2D */

    /* @todo Buggy disassembler table */
    /*fmla v0.2S,  v1.2S,  v2.2S */
    /*fmla v31.2S, v30.2S, v29.2S */
    /*fmla v0.4S,  v1.4S,  v2.4S */
    /*fmla v31.4S, v30.4S, v29.4S */
    /*fmla v0.2D,  v1.2D,  v2.2D */
    /*fmla v31.2D, v30.2D, v29.2D */

    mla v0.8B,  v1.8B,  v2.8B
    mla v31.8B, v30.8B, v29.8B
    mla v0.16B,  v1.16B,  v2.16B
    mla v31.16B, v30.16B, v29.16B
    mla v0.4H,  v1.4H,  v2.4H
    mla v31.4H, v30.4H, v29.4H
    mla v0.8H,  v1.8H,  v2.8H
    mla v31.8H, v30.8H, v29.8H
    mla v0.2S,  v1.2S,  v2.2S
    mla v31.2S, v30.2S, v29.2S
    mla v0.4S,  v1.4S,  v2.4S
    mla v31.4S, v30.4S, v29.4S

    mls v0.8B,  v1.8B,  v2.8B
    mls v31.8B, v30.8B, v29.8B
    mls v0.16B,  v1.16B,  v2.16B
    mls v31.16B, v30.16B, v29.16B
    mls v0.4H,  v1.4H,  v2.4H
    mls v31.4H, v30.4H, v29.4H
    mls v0.8H,  v1.8H,  v2.8H
    mls v31.8H, v30.8H, v29.8H
    mls v0.2S,  v1.2S,  v2.2S
    mls v31.2S, v30.2S, v29.2S
    mls v0.4S,  v1.4S,  v2.4S
    mls v31.4S, v30.4S, v29.4S

    mul v0.8B,  v1.8B,  v2.8B
    mul v31.8B, v30.8B, v29.8B
    mul v0.16B,  v1.16B,  v2.16B
    mul v31.16B, v30.16B, v29.16B
    mul v0.4H,  v1.4H,  v2.4H
    mul v31.4H, v30.4H, v29.4H
    mul v0.8H,  v1.8H,  v2.8H
    mul v31.8H, v30.8H, v29.8H
    mul v0.2S,  v1.2S,  v2.2S
    mul v31.2S, v30.2S, v29.2S
    mul v0.4S,  v1.4S,  v2.4S
    mul v31.4S, v30.4S, v29.4S

    mvni v0.4H, #0    /* , LSL #0 */
    mvni v0.4H, #0,   LSL #8
    mvni v0.4H, #63   /* , LSL #0 */
    mvni v0.4H, #63,  LSL #8
    mvni v0.4H, #255  /* , LSL #0 */
    mvni v0.4H, #255, LSL #8
    mvni v0.8H, #0    /* , LSL #0 */
    mvni v0.8H, #0,   LSL #8
    mvni v0.8H, #63   /* , LSL #0 */
    mvni v0.8H, #63,  LSL #8
    mvni v0.8H, #255   /* , LSL #0 */
    mvni v0.8H, #255, LSL #8

    mvni v0.2S, #0    /* , LSL #0 */
    mvni v0.2S, #0,   LSL #8
    mvni v0.2S, #0,   LSL #16
    mvni v0.2S, #0,   LSL #24
    mvni v0.2S, #63   /* , LSL #0 */
    mvni v0.2S, #63,  LSL #8
    mvni v0.2S, #63,  LSL #16
    mvni v0.2S, #63,  LSL #24
    mvni v0.2S, #255  /* , LSL #0 */
    mvni v0.2S, #255, LSL #8
    mvni v0.2S, #255, LSL #16
    mvni v0.2S, #255, LSL #24
    mvni v0.4S, #0    /* , LSL #0 */
    mvni v0.4S, #0,   LSL #8
    mvni v0.4S, #0,   LSL #16
    mvni v0.4S, #0,   LSL #24
    mvni v0.4S, #63   /* , LSL #0 */
    mvni v0.4S, #63,  LSL #8
    mvni v0.4S, #63,  LSL #16
    mvni v0.4S, #63,  LSL #24
    mvni v0.4S, #255  /* , LSL #0 */
    mvni v0.4S, #255, LSL #8
    mvni v0.4S, #255, LSL #16
    mvni v0.4S, #255, LSL #24

    mvni v0.2S, #0,   MSL #8
    mvni v0.2S, #0,   MSL #16
    mvni v0.2S, #63,  MSL #8
    mvni v0.2S, #63,  MSL #16
    mvni v0.2S, #255, MSL #8
    mvni v0.2S, #255, MSL #16
    mvni v0.4S, #0,   MSL #8
    mvni v0.4S, #0,   MSL #16
    mvni v0.4S, #63,  MSL #8
    mvni v0.4S, #63,  MSL #16
    mvni v0.4S, #255, MSL #8
    mvni v0.4S, #255, MSL #16

    orr v0.4H, #0    /*, LSL #0 */
    orr v0.4H, #0,   LSL #8
    orr v0.4H, #63   /*, LSL #0 */
    orr v0.4H, #63,  LSL #8
    orr v0.4H, #255  /*, LSL #0 */
    orr v0.4H, #255, LSL #8
    orr v0.8H, #0    /*, LSL #0 */
    orr v0.8H, #0,   LSL #8
    orr v0.8H, #63   /*, LSL #0 */
    orr v0.8H, #63,  LSL #8
    orr v0.8H, #255  /*, LSL #0 */
    orr v0.8H, #255, LSL #8

    orr v0.2S, #0    /* , LSL #0 */
    orr v0.2S, #0,   LSL #8
    orr v0.2S, #0,   LSL #16
    orr v0.2S, #0,   LSL #24
    orr v0.2S, #63   /* , LSL #0 */
    orr v0.2S, #63,  LSL #8
    orr v0.2S, #63,  LSL #16
    orr v0.2S, #63,  LSL #24
    orr v0.2S, #255  /* , LSL #0 */
    orr v0.2S, #255, LSL #8
    orr v0.2S, #255, LSL #16
    orr v0.2S, #255, LSL #24
    orr v0.4S, #0    /* , LSL #0 */
    orr v0.4S, #0,   LSL #8
    orr v0.4S, #0,   LSL #16
    orr v0.4S, #0,   LSL #24
    orr v0.4S, #63   /* , LSL #0 */
    orr v0.4S, #63,  LSL #8
    orr v0.4S, #63,  LSL #16
    orr v0.4S, #63,  LSL #24
    orr v0.4S, #255  /* , LSL #0 */
    orr v0.4S, #255, LSL #8
    orr v0.4S, #255, LSL #16
    orr v0.4S, #255, LSL #24

    orr v0.8B, v1.8B, v31.8B
    orr v0.16B, v1.16B, v31.16B

    pmul v0.8B,  v1.8B,  v2.8B
    pmul v31.8B, v30.8B, v29.8B
    pmul v0.16B,  v1.16B,  v2.16B
    pmul v31.16B, v30.16B, v29.16B

    saba v0.8B,  v1.8B,  v2.8B
    saba v31.8B, v30.8B, v29.8B
    saba v0.16B,  v1.16B,  v2.16B
    saba v31.16B, v30.16B, v29.16B
    saba v0.4H,  v1.4H,  v2.4H
    saba v31.4H, v30.4H, v29.4H
    saba v0.8H,  v1.8H,  v2.8H
    saba v31.8H, v30.8H, v29.8H
    saba v0.2S,  v1.2S,  v2.2S
    saba v31.2S, v30.2S, v29.2S
    saba v0.4S,  v1.4S,  v2.4S
    saba v31.4S, v30.4S, v29.4S

    sabd v0.8B,  v1.8B,  v2.8B
    sabd v31.8B, v30.8B, v29.8B
    sabd v0.16B,  v1.16B,  v2.16B
    sabd v31.16B, v30.16B, v29.16B
    sabd v0.4H,  v1.4H,  v2.4H
    sabd v31.4H, v30.4H, v29.4H
    sabd v0.8H,  v1.8H,  v2.8H
    sabd v31.8H, v30.8H, v29.8H
    sabd v0.2S,  v1.2S,  v2.2S
    sabd v31.2S, v30.2S, v29.2S
    sabd v0.4S,  v1.4S,  v2.4S
    sabd v31.4S, v30.4S, v29.4S

    saddlv h0, v0.8B
    saddlv h0, v0.16B
    saddlv h31, v31.8B
    saddlv h31, v31.16B
    saddlv s0, v0.4H
    saddlv s0, v0.8H
    saddlv s31, v31.4H
    saddlv s31, v31.8H
    saddlv d0, v0.4S
    saddlv d31, v31.4S

    smax v0.8B,  v1.8B,  v2.8B
    smax v31.8B, v30.8B, v29.8B
    smax v0.16B,  v1.16B,  v2.16B
    smax v31.16B, v30.16B, v29.16B
    smax v0.4H,  v1.4H,  v2.4H
    smax v31.4H, v30.4H, v29.4H
    smax v0.8H,  v1.8H,  v2.8H
    smax v31.8H, v30.8H, v29.8H
    smax v0.2S,  v1.2S,  v2.2S
    smax v31.2S, v30.2S, v29.2S
    smax v0.4S,  v1.4S,  v2.4S
    smax v31.4S, v30.4S, v29.4S

    smaxp v0.8B,  v1.8B,  v2.8B
    smaxp v31.8B, v30.8B, v29.8B
    smaxp v0.16B,  v1.16B,  v2.16B
    smaxp v31.16B, v30.16B, v29.16B
    smaxp v0.4H,  v1.4H,  v2.4H
    smaxp v31.4H, v30.4H, v29.4H
    smaxp v0.8H,  v1.8H,  v2.8H
    smaxp v31.8H, v30.8H, v29.8H
    smaxp v0.2S,  v1.2S,  v2.2S
    smaxp v31.2S, v30.2S, v29.2S
    smaxp v0.4S,  v1.4S,  v2.4S
    smaxp v31.4S, v30.4S, v29.4S

    smaxv b0, v0.8B
    smaxv b0, v0.16B
    smaxv b31, v31.8B
    smaxv b31, v31.16B
    smaxv h0, v0.4H
    smaxv h0, v0.8H
    smaxv h31, v31.4H
    smaxv h31, v31.8H
    smaxv s0, v0.4S
    smaxv s31, v31.4S

    sminp v0.8B,  v1.8B,  v2.8B
    sminp v31.8B, v30.8B, v29.8B
    sminp v0.16B,  v1.16B,  v2.16B
    sminp v31.16B, v30.16B, v29.16B
    sminp v0.4H,  v1.4H,  v2.4H
    sminp v31.4H, v30.4H, v29.4H
    sminp v0.8H,  v1.8H,  v2.8H
    sminp v31.8H, v30.8H, v29.8H
    sminp v0.2S,  v1.2S,  v2.2S
    sminp v31.2S, v30.2S, v29.2S
    sminp v0.4S,  v1.4S,  v2.4S
    sminp v31.4S, v30.4S, v29.4S

    sminv b0, v0.8B
    sminv b0, v0.16B
    sminv b31, v31.8B
    sminv b31, v31.16B
    sminv h0, v0.4H
    sminv h0, v0.8H
    sminv h31, v31.4H
    sminv h31, v31.8H
    sminv s0, v0.4S
    sminv s31, v31.4S

    sqadd v0.8B,  v1.8B,  v2.8B
    sqadd v31.8B, v30.8B, v29.8B
    sqadd v0.16B,  v1.16B,  v2.16B
    sqadd v31.16B, v30.16B, v29.16B
    sqadd v0.4H,  v1.4H,  v2.4H
    sqadd v31.4H, v30.4H, v29.4H
    sqadd v0.8H,  v1.8H,  v2.8H
    sqadd v31.8H, v30.8H, v29.8H
    sqadd v0.2S,  v1.2S,  v2.2S
    sqadd v31.2S, v30.2S, v29.2S
    sqadd v0.4S,  v1.4S,  v2.4S
    sqadd v31.4S, v30.4S, v29.4S
    sqadd v0.2D,  v1.2D,  v2.2D
    sqadd v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*sqadd b0,  b1,  b2 */
    /*sqadd b31, b30, b29 */
    /*sqadd h0,  h1,  h2 */
    /*sqadd h31, h30, h29 */
    /*sqadd s0,  s1,  s2 */
    /*sqadd s31, s30, s29 */
    /*sqadd d0,  d1,  d2 */
    /*sqadd d31, d30, d29 */

    sqdmulh v0.4H,  v1.4H,  v2.4H
    sqdmulh v31.4H, v30.4H, v29.4H
    sqdmulh v0.8H,  v1.8H,  v2.8H
    sqdmulh v31.8H, v30.8H, v29.8H
    sqdmulh v0.2S,  v1.2S,  v2.2S
    sqdmulh v31.2S, v30.2S, v29.2S
    sqdmulh v0.4S,  v1.4S,  v2.4S
    sqdmulh v31.4S, v30.4S, v29.4S

    /* @todo Scalar variants */
    /*sqdmulh h0,  h1,  h2 */
    /*sqdmulh h31, h30, h29 */
    /*sqdmulh s0,  s1,  s2 */
    /*sqdmulh s31, s30, s29 */

    sqrshl v0.8B,  v1.8B,  v2.8B
    sqrshl v31.8B, v30.8B, v29.8B
    sqrshl v0.16B,  v1.16B,  v2.16B
    sqrshl v31.16B, v30.16B, v29.16B
    sqrshl v0.4H,  v1.4H,  v2.4H
    sqrshl v31.4H, v30.4H, v29.4H
    sqrshl v0.8H,  v1.8H,  v2.8H
    sqrshl v31.8H, v30.8H, v29.8H
    sqrshl v0.2S,  v1.2S,  v2.2S
    sqrshl v31.2S, v30.2S, v29.2S
    sqrshl v0.4S,  v1.4S,  v2.4S
    sqrshl v31.4S, v30.4S, v29.4S
    sqrshl v0.2D,  v1.2D,  v2.2D
    sqrshl v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*sqrshl b0,  b1,  b2 */
    /*sqrshl b31, b30, b29 */
    /*sqrshl h0,  h1,  h2 */
    /*sqrshl h31, h30, h29 */
    /*sqrshl s0,  s1,  s2 */
    /*sqrshl s31, s30, s29 */
    /*sqrshl d0,  d1,  d2 */
    /*sqrshl d31, d30, d29 */

    sqshl v0.8B,  v1.8B,  v2.8B
    sqshl v31.8B, v30.8B, v29.8B
    sqshl v0.16B,  v1.16B,  v2.16B
    sqshl v31.16B, v30.16B, v29.16B
    sqshl v0.4H,  v1.4H,  v2.4H
    sqshl v31.4H, v30.4H, v29.4H
    sqshl v0.8H,  v1.8H,  v2.8H
    sqshl v31.8H, v30.8H, v29.8H
    sqshl v0.2S,  v1.2S,  v2.2S
    sqshl v31.2S, v30.2S, v29.2S
    sqshl v0.4S,  v1.4S,  v2.4S
    sqshl v31.4S, v30.4S, v29.4S
    sqshl v0.2D,  v1.2D,  v2.2D
    sqshl v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*sqshl b0,  b1,  b2 */
    /*sqshl b31, b30, b29 */
    /*sqshl h0,  h1,  h2 */
    /*sqshl h31, h30, h29 */
    /*sqshl s0,  s1,  s2 */
    /*sqshl s31, s30, s29 */
    /*sqshl d0,  d1,  d2 */
    /*sqshl d31, d30, d29 */

    srhadd v0.8B,  v1.8B,  v2.8B
    srhadd v31.8B, v30.8B, v29.8B
    srhadd v0.16B,  v1.16B,  v2.16B
    srhadd v31.16B, v30.16B, v29.16B
    srhadd v0.4H,  v1.4H,  v2.4H
    srhadd v31.4H, v30.4H, v29.4H
    srhadd v0.8H,  v1.8H,  v2.8H
    srhadd v31.8H, v30.8H, v29.8H
    srhadd v0.2S,  v1.2S,  v2.2S
    srhadd v31.2S, v30.2S, v29.2S
    srhadd v0.4S,  v1.4S,  v2.4S
    srhadd v31.4S, v30.4S, v29.4S

    shsub v0.8B,  v1.8B,  v2.8B
    shsub v31.8B, v30.8B, v29.8B
    shsub v0.16B,  v1.16B,  v2.16B
    shsub v31.16B, v30.16B, v29.16B
    shsub v0.4H,  v1.4H,  v2.4H
    shsub v31.4H, v30.4H, v29.4H
    shsub v0.8H,  v1.8H,  v2.8H
    shsub v31.8H, v30.8H, v29.8H
    shsub v0.2S,  v1.2S,  v2.2S
    shsub v31.2S, v30.2S, v29.2S
    shsub v0.4S,  v1.4S,  v2.4S
    shsub v31.4S, v30.4S, v29.4S

    sqrdmulh v0.4H,  v1.4H,  v2.4H
    sqrdmulh v31.4H, v30.4H, v29.4H
    sqrdmulh v0.8H,  v1.8H,  v2.8H
    sqrdmulh v31.8H, v30.8H, v29.8H
    sqrdmulh v0.2S,  v1.2S,  v2.2S
    sqrdmulh v31.2S, v30.2S, v29.2S
    sqrdmulh v0.4S,  v1.4S,  v2.4S
    sqrdmulh v31.4S, v30.4S, v29.4S

    /* @todo Scalar variants */
    /*sqrdmulh h0,  h1,  h2 */
    /*sqrdmulh h31, h30, h29 */
    /*sqrdmulh s0,  s1,  s2 */
    /*sqrdmulh s31, s30, s29 */

    sqsub v0.8B,  v1.8B,  v2.8B
    sqsub v31.8B, v30.8B, v29.8B
    sqsub v0.16B,  v1.16B,  v2.16B
    sqsub v31.16B, v30.16B, v29.16B
    sqsub v0.4H,  v1.4H,  v2.4H
    sqsub v31.4H, v30.4H, v29.4H
    sqsub v0.8H,  v1.8H,  v2.8H
    sqsub v31.8H, v30.8H, v29.8H
    sqsub v0.2S,  v1.2S,  v2.2S
    sqsub v31.2S, v30.2S, v29.2S
    sqsub v0.4S,  v1.4S,  v2.4S
    sqsub v31.4S, v30.4S, v29.4S
    sqsub v0.2D,  v1.2D,  v2.2D
    sqsub v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*sqsub b0,  b1,  b2 */
    /*sqsub b31, b30, b29 */
    /*sqsub h0,  h1,  h2 */
    /*sqsub h31, h30, h29 */
    /*sqsub s0,  s1,  s2 */
    /*sqsub s31, s30, s29 */
    /*sqsub d0,  d1,  d2 */
    /*sqsub d31, d30, d29 */

    sshl v0.8B,  v1.8B,  v2.8B
    sshl v31.8B, v30.8B, v29.8B
    sshl v0.16B,  v1.16B,  v2.16B
    sshl v31.16B, v30.16B, v29.16B
    sshl v0.4H,  v1.4H,  v2.4H
    sshl v31.4H, v30.4H, v29.4H
    sshl v0.8H,  v1.8H,  v2.8H
    sshl v31.8H, v30.8H, v29.8H
    sshl v0.2S,  v1.2S,  v2.2S
    sshl v31.2S, v30.2S, v29.2S
    sshl v0.4S,  v1.4S,  v2.4S
    sshl v31.4S, v30.4S, v29.4S
    sshl v0.2D,  v1.2D,  v2.2D
    sshl v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*sshl d0,  d1,  d2 */
    /*sshl d31, d30, d29 */

    sub v0.8B,  v1.8B,  v2.8B
    sub v31.8B, v30.8B, v29.8B
    sub v0.16B,  v1.16B,  v2.16B
    sub v31.16B, v30.16B, v29.16B
    sub v0.4H,  v1.4H,  v2.4H
    sub v31.4H, v30.4H, v29.4H
    sub v0.8H,  v1.8H,  v2.8H
    sub v31.8H, v30.8H, v29.8H
    sub v0.2S,  v1.2S,  v2.2S
    sub v31.2S, v30.2S, v29.2S
    sub v0.4S,  v1.4S,  v2.4S
    sub v31.4S, v30.4S, v29.4S
    sub v0.2D,  v1.2D,  v2.2D
    sub v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*sub d0,  d1,  d2 */
    /*sub d31, d30, d29 */

    uaba v0.8B,  v1.8B,  v2.8B
    uaba v31.8B, v30.8B, v29.8B
    uaba v0.16B,  v1.16B,  v2.16B
    uaba v31.16B, v30.16B, v29.16B
    uaba v0.4H,  v1.4H,  v2.4H
    uaba v31.4H, v30.4H, v29.4H
    uaba v0.8H,  v1.8H,  v2.8H
    uaba v31.8H, v30.8H, v29.8H
    uaba v0.2S,  v1.2S,  v2.2S
    uaba v31.2S, v30.2S, v29.2S
    uaba v0.4S,  v1.4S,  v2.4S
    uaba v31.4S, v30.4S, v29.4S

    uaddlv h0, v0.8B
    uaddlv h0, v0.16B
    uaddlv h31, v31.8B
    uaddlv h31, v31.16B
    uaddlv s0, v0.4H
    uaddlv s0, v0.8H
    uaddlv s31, v31.4H
    uaddlv s31, v31.8H
    uaddlv d0, v0.4S
    uaddlv d31, v31.4S

    uhadd v0.8B,  v1.8B,  v2.8B
    uhadd v31.8B, v30.8B, v29.8B
    uhadd v0.16B,  v1.16B,  v2.16B
    uhadd v31.16B, v30.16B, v29.16B
    uhadd v0.4H,  v1.4H,  v2.4H
    uhadd v31.4H, v30.4H, v29.4H
    uhadd v0.8H,  v1.8H,  v2.8H
    uhadd v31.8H, v30.8H, v29.8H
    uhadd v0.2S,  v1.2S,  v2.2S
    uhadd v31.2S, v30.2S, v29.2S
    uhadd v0.4S,  v1.4S,  v2.4S
    uhadd v31.4S, v30.4S, v29.4S

    uhsub v0.8B,  v1.8B,  v2.8B
    uhsub v31.8B, v30.8B, v29.8B
    uhsub v0.16B,  v1.16B,  v2.16B
    uhsub v31.16B, v30.16B, v29.16B
    uhsub v0.4H,  v1.4H,  v2.4H
    uhsub v31.4H, v30.4H, v29.4H
    uhsub v0.8H,  v1.8H,  v2.8H
    uhsub v31.8H, v30.8H, v29.8H
    uhsub v0.2S,  v1.2S,  v2.2S
    uhsub v31.2S, v30.2S, v29.2S
    uhsub v0.4S,  v1.4S,  v2.4S
    uhsub v31.4S, v30.4S, v29.4S

    umax v0.8B,  v1.8B,  v2.8B
    umax v31.8B, v30.8B, v29.8B
    umax v0.16B,  v1.16B,  v2.16B
    umax v31.16B, v30.16B, v29.16B
    umax v0.4H,  v1.4H,  v2.4H
    umax v31.4H, v30.4H, v29.4H
    umax v0.8H,  v1.8H,  v2.8H
    umax v31.8H, v30.8H, v29.8H
    umax v0.2S,  v1.2S,  v2.2S
    umax v31.2S, v30.2S, v29.2S
    umax v0.4S,  v1.4S,  v2.4S
    umax v31.4S, v30.4S, v29.4S

    umaxp v0.8B,  v1.8B,  v2.8B
    umaxp v31.8B, v30.8B, v29.8B
    umaxp v0.16B,  v1.16B,  v2.16B
    umaxp v31.16B, v30.16B, v29.16B
    umaxp v0.4H,  v1.4H,  v2.4H
    umaxp v31.4H, v30.4H, v29.4H
    umaxp v0.8H,  v1.8H,  v2.8H
    umaxp v31.8H, v30.8H, v29.8H
    umaxp v0.2S,  v1.2S,  v2.2S
    umaxp v31.2S, v30.2S, v29.2S
    umaxp v0.4S,  v1.4S,  v2.4S
    umaxp v31.4S, v30.4S, v29.4S

    umaxv b0, v0.8B
    umaxv b0, v0.16B
    umaxv b31, v31.8B
    umaxv b31, v31.16B
    umaxv h0, v0.4H
    umaxv h0, v0.8H
    umaxv h31, v31.4H
    umaxv h31, v31.8H
    umaxv s0, v0.4S
    umaxv s31, v31.4S

    umin v0.8B,  v1.8B,  v2.8B
    umin v31.8B, v30.8B, v29.8B
    umin v0.16B,  v1.16B,  v2.16B
    umin v31.16B, v30.16B, v29.16B
    umin v0.4H,  v1.4H,  v2.4H
    umin v31.4H, v30.4H, v29.4H
    umin v0.8H,  v1.8H,  v2.8H
    umin v31.8H, v30.8H, v29.8H
    umin v0.2S,  v1.2S,  v2.2S
    umin v31.2S, v30.2S, v29.2S
    umin v0.4S,  v1.4S,  v2.4S
    umin v31.4S, v30.4S, v29.4S

    uminp v0.8B,  v1.8B,  v2.8B
    uminp v31.8B, v30.8B, v29.8B
    uminp v0.16B,  v1.16B,  v2.16B
    uminp v31.16B, v30.16B, v29.16B
    uminp v0.4H,  v1.4H,  v2.4H
    uminp v31.4H, v30.4H, v29.4H
    uminp v0.8H,  v1.8H,  v2.8H
    uminp v31.8H, v30.8H, v29.8H
    uminp v0.2S,  v1.2S,  v2.2S
    uminp v31.2S, v30.2S, v29.2S
    uminp v0.4S,  v1.4S,  v2.4S
    uminp v31.4S, v30.4S, v29.4S

    uminv b0, v0.8B
    uminv b0, v0.16B
    uminv b31, v31.8B
    uminv b31, v31.16B
    uminv h0, v0.4H
    uminv h0, v0.8H
    uminv h31, v31.4H
    uminv h31, v31.8H
    uminv s0, v0.4S
    uminv s31, v31.4S

    uqadd v0.8B,  v1.8B,  v2.8B
    uqadd v31.8B, v30.8B, v29.8B
    uqadd v0.16B,  v1.16B,  v2.16B
    uqadd v31.16B, v30.16B, v29.16B
    uqadd v0.4H,  v1.4H,  v2.4H
    uqadd v31.4H, v30.4H, v29.4H
    uqadd v0.8H,  v1.8H,  v2.8H
    uqadd v31.8H, v30.8H, v29.8H
    uqadd v0.2S,  v1.2S,  v2.2S
    uqadd v31.2S, v30.2S, v29.2S
    uqadd v0.4S,  v1.4S,  v2.4S
    uqadd v31.4S, v30.4S, v29.4S
    uqadd v0.2D,  v1.2D,  v2.2D
    uqadd v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*uqadd b0,  b1,  b2 */
    /*uqadd b31, b30, b29 */
    /*uqadd h0,  h1,  h2 */
    /*uqadd h31, h30, h29 */
    /*uqadd s0,  s1,  s2 */
    /*uqadd s31, s30, s29 */
    /*uqadd d0,  d1,  d2 */
    /*uqadd d31, d30, d29 */

    uqrshl v0.8B,  v1.8B,  v2.8B
    uqrshl v31.8B, v30.8B, v29.8B
    uqrshl v0.16B,  v1.16B,  v2.16B
    uqrshl v31.16B, v30.16B, v29.16B
    uqrshl v0.4H,  v1.4H,  v2.4H
    uqrshl v31.4H, v30.4H, v29.4H
    uqrshl v0.8H,  v1.8H,  v2.8H
    uqrshl v31.8H, v30.8H, v29.8H
    uqrshl v0.2S,  v1.2S,  v2.2S
    uqrshl v31.2S, v30.2S, v29.2S
    uqrshl v0.4S,  v1.4S,  v2.4S
    uqrshl v31.4S, v30.4S, v29.4S
    uqrshl v0.2D,  v1.2D,  v2.2D
    uqrshl v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*uqrshl b0,  b1,  b2 */
    /*uqrshl b31, b30, b29 */
    /*uqrshl h0,  h1,  h2 */
    /*uqrshl h31, h30, h29 */
    /*uqrshl s0,  s1,  s2 */
    /*uqrshl s31, s30, s29 */
    /*uqrshl d0,  d1,  d2 */
    /*uqrshl d31, d30, d29 */

    uqshl v0.8B,  v1.8B,  v2.8B
    uqshl v31.8B, v30.8B, v29.8B
    uqshl v0.16B,  v1.16B,  v2.16B
    uqshl v31.16B, v30.16B, v29.16B
    uqshl v0.4H,  v1.4H,  v2.4H
    uqshl v31.4H, v30.4H, v29.4H
    uqshl v0.8H,  v1.8H,  v2.8H
    uqshl v31.8H, v30.8H, v29.8H
    uqshl v0.2S,  v1.2S,  v2.2S
    uqshl v31.2S, v30.2S, v29.2S
    uqshl v0.4S,  v1.4S,  v2.4S
    uqshl v31.4S, v30.4S, v29.4S
    uqshl v0.2D,  v1.2D,  v2.2D
    uqshl v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*uqshl b0,  b1,  b2 */
    /*uqshl b31, b30, b29 */
    /*uqshl h0,  h1,  h2 */
    /*uqshl h31, h30, h29 */
    /*uqshl s0,  s1,  s2 */
    /*uqshl s31, s30, s29 */
    /*uqshl d0,  d1,  d2 */
    /*uqshl d31, d30, d29 */

    uqsub v0.8B,  v1.8B,  v2.8B
    uqsub v31.8B, v30.8B, v29.8B
    uqsub v0.16B,  v1.16B,  v2.16B
    uqsub v31.16B, v30.16B, v29.16B
    uqsub v0.4H,  v1.4H,  v2.4H
    uqsub v31.4H, v30.4H, v29.4H
    uqsub v0.8H,  v1.8H,  v2.8H
    uqsub v31.8H, v30.8H, v29.8H
    uqsub v0.2S,  v1.2S,  v2.2S
    uqsub v31.2S, v30.2S, v29.2S
    uqsub v0.4S,  v1.4S,  v2.4S
    uqsub v31.4S, v30.4S, v29.4S
    uqsub v0.2D,  v1.2D,  v2.2D
    uqsub v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*uqsub b0,  b1,  b2 */
    /*uqsub b31, b30, b29 */
    /*uqsub h0,  h1,  h2 */
    /*uqsub h31, h30, h29 */
    /*uqsub s0,  s1,  s2 */
    /*uqsub s31, s30, s29 */
    /*uqsub d0,  d1,  d2 */
    /*uqsub d31, d30, d29 */

    urhadd v0.8B,  v1.8B,  v2.8B
    urhadd v31.8B, v30.8B, v29.8B
    urhadd v0.16B,  v1.16B,  v2.16B
    urhadd v31.16B, v30.16B, v29.16B
    urhadd v0.4H,  v1.4H,  v2.4H
    urhadd v31.4H, v30.4H, v29.4H
    urhadd v0.8H,  v1.8H,  v2.8H
    urhadd v31.8H, v30.8H, v29.8H
    urhadd v0.2S,  v1.2S,  v2.2S
    urhadd v31.2S, v30.2S, v29.2S
    urhadd v0.4S,  v1.4S,  v2.4S
    urhadd v31.4S, v30.4S, v29.4S

    urshl v0.8B,  v1.8B,  v2.8B
    urshl v31.8B, v30.8B, v29.8B
    urshl v0.16B,  v1.16B,  v2.16B
    urshl v31.16B, v30.16B, v29.16B
    urshl v0.4H,  v1.4H,  v2.4H
    urshl v31.4H, v30.4H, v29.4H
    urshl v0.8H,  v1.8H,  v2.8H
    urshl v31.8H, v30.8H, v29.8H
    urshl v0.2S,  v1.2S,  v2.2S
    urshl v31.2S, v30.2S, v29.2S
    urshl v0.4S,  v1.4S,  v2.4S
    urshl v31.4S, v30.4S, v29.4S
    urshl v0.2D,  v1.2D,  v2.2D
    urshl v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*urshl d0,  d1,  d2 */
    /*urshl d31, d30, d29 */

    ushl v0.8B,  v1.8B,  v2.8B
    ushl v31.8B, v30.8B, v29.8B
    ushl v0.16B,  v1.16B,  v2.16B
    ushl v31.16B, v30.16B, v29.16B
    ushl v0.4H,  v1.4H,  v2.4H
    ushl v31.4H, v30.4H, v29.4H
    ushl v0.8H,  v1.8H,  v2.8H
    ushl v31.8H, v30.8H, v29.8H
    ushl v0.2S,  v1.2S,  v2.2S
    ushl v31.2S, v30.2S, v29.2S
    ushl v0.4S,  v1.4S,  v2.4S
    ushl v31.4S, v30.4S, v29.4S
    ushl v0.2D,  v1.2D,  v2.2D
    ushl v31.2D, v30.2D, v29.2D

    /* @todo Scalar variants */
    /*ushl d0,  d1,  d2 */
    /*ushl d31, d30, d29 */

    /*
     * Keep last so the testcase can catch errors in
     * the disassembly of the last instruction.
     */
    nop

BEGINPROC_HIDDEN TestProcA64Simd_EndProc
