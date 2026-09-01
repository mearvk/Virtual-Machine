/* $Id: tstDisasmArmv8-1-asm-lse.S 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - LSE.
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

BEGINPROC_HIDDEN TestProcA64Lse

        .cpu generic+lse

        /* Atomic memory operations */

        ldaddb w1,  w2,  [x0]
        ldaddb wzr, wzr, [sp]

        ldclrb w1,  w2,  [x0]
        ldclrb wzr, wzr, [sp]

        ldeorb w1,  w2,  [x0]
        ldeorb wzr, wzr, [sp]

        ldsetb w1,  w2,  [x0]
        ldsetb wzr, wzr, [sp]

        ldsmaxb w1,  w2,  [x0]
        ldsmaxb wzr, wzr, [sp]

        ldsminb w1,  w2,  [x0]
        ldsminb wzr, wzr, [sp]

        ldumaxb w1,  w2,  [x0]
        ldumaxb wzr, wzr, [sp]

        lduminb w1,  w2,  [x0]
        lduminb wzr, wzr, [sp]

        swpb w1,  w2,  [x0]
        swpb wzr, wzr, [sp]

        ldaddlb w1,  w2,  [x0]
        ldaddlb wzr, wzr, [sp]

        ldclrlb w1,  w2,  [x0]
        ldclrlb wzr, wzr, [sp]

        ldeorlb w1,  w2,  [x0]
        ldeorlb wzr, wzr, [sp]

        ldsetlb w1,  w2,  [x0]
        ldsetlb wzr, wzr, [sp]

        ldsmaxlb w1,  w2,  [x0]
        ldsmaxlb wzr, wzr, [sp]

        ldsminlb w1,  w2,  [x0]
        ldsminlb wzr, wzr, [sp]

        ldumaxlb w1,  w2,  [x0]
        ldumaxlb wzr, wzr, [sp]

        lduminlb w1,  w2,  [x0]
        lduminlb wzr, wzr, [sp]

        swplb w1,  w2,  [x0]
        swplb wzr, wzr, [sp]

        ldaddab w1,  w2,  [x0]
        ldaddab wzr, wzr, [sp]

        ldclrab w1,  w2,  [x0]
        ldclrab wzr, wzr, [sp]

        ldeorab w1,  w2,  [x0]
        ldeorab wzr, wzr, [sp]

        ldsetab w1,  w2,  [x0]
        ldsetab wzr, wzr, [sp]

        ldsmaxab w1,  w2,  [x0]
        ldsmaxab wzr, wzr, [sp]

        ldsminab w1,  w2,  [x0]
        ldsminab wzr, wzr, [sp]

        ldumaxab w1,  w2,  [x0]
        ldumaxab wzr, wzr, [sp]

        lduminab w1,  w2,  [x0]
        lduminab wzr, wzr, [sp]

        swpab w1,  w2,  [x0]
        swpab wzr, wzr, [sp]

        ldaddalb w1,  w2,  [x0]
        ldaddalb wzr, wzr, [sp]

        ldclralb w1,  w2,  [x0]
        ldclralb wzr, wzr, [sp]

        ldeoralb w1,  w2,  [x0]
        ldeoralb wzr, wzr, [sp]

        ldsetalb w1,  w2,  [x0]
        ldsetalb wzr, wzr, [sp]

        ldsmaxalb w1,  w2,  [x0]
        ldsmaxalb wzr, wzr, [sp]

        ldsminalb w1,  w2,  [x0]
        ldsminalb wzr, wzr, [sp]

        ldumaxalb w1,  w2,  [x0]
        ldumaxalb wzr, wzr, [sp]

        lduminalb w1,  w2,  [x0]
        lduminalb wzr, wzr, [sp]

        swpalb w1,  w2,  [x0]
        swpalb wzr, wzr, [sp]

        ldaddh w1,  w2,  [x0]
        ldaddh wzr, wzr, [sp]

        ldclrh w1,  w2,  [x0]
        ldclrh wzr, wzr, [sp]

        ldeorh w1,  w2,  [x0]
        ldeorh wzr, wzr, [sp]

        ldseth w1,  w2,  [x0]
        ldseth wzr, wzr, [sp]

        ldsmaxh w1,  w2,  [x0]
        ldsmaxh wzr, wzr, [sp]

        ldsminh w1,  w2,  [x0]
        ldsminh wzr, wzr, [sp]

        ldumaxh w1,  w2,  [x0]
        ldumaxh wzr, wzr, [sp]

        lduminh w1,  w2,  [x0]
        lduminh wzr, wzr, [sp]

        swph w1,  w2,  [x0]
        swph wzr, wzr, [sp]

        ldaddlh w1,  w2,  [x0]
        ldaddlh wzr, wzr, [sp]

        ldclrlh w1,  w2,  [x0]
        ldclrlh wzr, wzr, [sp]

        ldeorlh w1,  w2,  [x0]
        ldeorlh wzr, wzr, [sp]

        ldsetlh w1,  w2,  [x0]
        ldsetlh wzr, wzr, [sp]

        ldsmaxlh w1,  w2,  [x0]
        ldsmaxlh wzr, wzr, [sp]

        ldsminlh w1,  w2,  [x0]
        ldsminlh wzr, wzr, [sp]

        ldumaxlh w1,  w2,  [x0]
        ldumaxlh wzr, wzr, [sp]

        lduminlh w1,  w2,  [x0]
        lduminlh wzr, wzr, [sp]

        swplh w1,  w2,  [x0]
        swplh wzr, wzr, [sp]

        ldaddah w1,  w2,  [x0]
        ldaddah wzr, wzr, [sp]

        ldclrah w1,  w2,  [x0]
        ldclrah wzr, wzr, [sp]

        ldeorah w1,  w2,  [x0]
        ldeorah wzr, wzr, [sp]

        ldsetah w1,  w2,  [x0]
        ldsetah wzr, wzr, [sp]

        ldsmaxah w1,  w2,  [x0]
        ldsmaxah wzr, wzr, [sp]

        ldsminah w1,  w2,  [x0]
        ldsminah wzr, wzr, [sp]

        ldumaxah w1,  w2,  [x0]
        ldumaxah wzr, wzr, [sp]

        lduminah w1,  w2,  [x0]
        lduminah wzr, wzr, [sp]

        swpah w1,  w2,  [x0]
        swpah wzr, wzr, [sp]

        ldaddalh w1,  w2,  [x0]
        ldaddalh wzr, wzr, [sp]

        ldclralh w1,  w2,  [x0]
        ldclralh wzr, wzr, [sp]

        ldeoralh w1,  w2,  [x0]
        ldeoralh wzr, wzr, [sp]

        ldsetalh w1,  w2,  [x0]
        ldsetalh wzr, wzr, [sp]

        ldsmaxalh w1,  w2,  [x0]
        ldsmaxalh wzr, wzr, [sp]

        ldsminalh w1,  w2,  [x0]
        ldsminalh wzr, wzr, [sp]

        ldumaxalh w1,  w2,  [x0]
        ldumaxalh wzr, wzr, [sp]

        lduminalh w1,  w2,  [x0]
        lduminalh wzr, wzr, [sp]

        swpalh w1,  w2,  [x0]
        swpalh wzr, wzr, [sp]

        ldadd w1,  w2,  [x0]
        ldadd wzr, wzr, [sp]
        ldadd x1,  x2,  [x0]
        ldadd xzr, xzr, [sp]

        ldclr w1,  w2,  [x0]
        ldclr wzr, wzr, [sp]
        ldclr x1,  x2,  [x0]
        ldclr xzr, xzr, [sp]

        ldeor w1,  w2,  [x0]
        ldeor wzr, wzr, [sp]
        ldeor x1,  x2,  [x0]
        ldeor xzr, xzr, [sp]

        ldset w1,  w2,  [x0]
        ldset wzr, wzr, [sp]
        ldset x1,  x2,  [x0]
        ldset xzr, xzr, [sp]

        ldsmax w1,  w2,  [x0]
        ldsmax wzr, wzr, [sp]
        ldsmax x1,  x2,  [x0]
        ldsmax xzr, xzr, [sp]

        ldsmin w1,  w2,  [x0]
        ldsmin wzr, wzr, [sp]
        ldsmin x1,  x2,  [x0]
        ldsmin xzr, xzr, [sp]

        ldumax w1,  w2,  [x0]
        ldumax wzr, wzr, [sp]
        ldumax x1,  x2,  [x0]
        ldumax xzr, xzr, [sp]

        ldumin w1,  w2,  [x0]
        ldumin wzr, wzr, [sp]
        ldumin x1,  x2,  [x0]
        ldumin xzr, xzr, [sp]

        swp w1,  w2,  [x0]
        swp wzr, wzr, [sp]
        swp x1,  x2,  [x0]
        swp xzr, xzr, [sp]

        ldaddl w1,  w2,  [x0]
        ldaddl wzr, wzr, [sp]
        ldaddl x1,  x2,  [x0]
        ldaddl xzr, xzr, [sp]

        ldclrl w1,  w2,  [x0]
        ldclrl wzr, wzr, [sp]
        ldclrl x1,  x2,  [x0]
        ldclrl xzr, xzr, [sp]

        ldeorl w1,  w2,  [x0]
        ldeorl wzr, wzr, [sp]
        ldeorl x1,  x2,  [x0]
        ldeorl xzr, xzr, [sp]

        ldsetl w1,  w2,  [x0]
        ldsetl wzr, wzr, [sp]
        ldsetl x1,  x2,  [x0]
        ldsetl xzr, xzr, [sp]

        ldsmaxl w1,  w2,  [x0]
        ldsmaxl wzr, wzr, [sp]
        ldsmaxl x1,  x2,  [x0]
        ldsmaxl xzr, xzr, [sp]

        ldsminl w1,  w2,  [x0]
        ldsminl wzr, wzr, [sp]
        ldsminl x1,  x2,  [x0]
        ldsminl xzr, xzr, [sp]

        ldumaxl w1,  w2,  [x0]
        ldumaxl wzr, wzr, [sp]
        ldumaxl x1,  x2,  [x0]
        ldumaxl xzr, xzr, [sp]

        lduminl w1,  w2,  [x0]
        lduminl wzr, wzr, [sp]
        lduminl x1,  x2,  [x0]
        lduminl xzr, xzr, [sp]

        swpl w1,  w2,  [x0]
        swpl wzr, wzr, [sp]
        swpl x1,  x2,  [x0]
        swpl xzr, xzr, [sp]

        ldadda w1,  w2,  [x0]
        ldadda wzr, wzr, [sp]
        ldadda x1,  x2,  [x0]
        ldadda xzr, xzr, [sp]

        ldclra w1,  w2,  [x0]
        ldclra wzr, wzr, [sp]
        ldclra x1,  x2,  [x0]
        ldclra xzr, xzr, [sp]

        ldeora w1,  w2,  [x0]
        ldeora wzr, wzr, [sp]
        ldeora x1,  x2,  [x0]
        ldeora xzr, xzr, [sp]

        ldseta w1,  w2,  [x0]
        ldseta wzr, wzr, [sp]
        ldseta x1,  x2,  [x0]
        ldseta xzr, xzr, [sp]

        ldsmaxa w1,  w2,  [x0]
        ldsmaxa wzr, wzr, [sp]
        ldsmaxa x1,  x2,  [x0]
        ldsmaxa xzr, xzr, [sp]

        ldsmina w1,  w2,  [x0]
        ldsmina wzr, wzr, [sp]
        ldsmina x1,  x2,  [x0]
        ldsmina xzr, xzr, [sp]

        ldumaxa w1,  w2,  [x0]
        ldumaxa wzr, wzr, [sp]
        ldumaxa x1,  x2,  [x0]
        ldumaxa xzr, xzr, [sp]

        ldumina w1,  w2,  [x0]
        ldumina wzr, wzr, [sp]
        ldumina x1,  x2,  [x0]
        ldumina xzr, xzr, [sp]

        swpa w1,  w2,  [x0]
        swpa wzr, wzr, [sp]
        swpa x1,  x2,  [x0]
        swpa xzr, xzr, [sp]

        ldaddal w1,  w2,  [x0]
        ldaddal wzr, wzr, [sp]
        ldaddal x1,  x2,  [x0]
        ldaddal xzr, xzr, [sp]

        ldclral w1,  w2,  [x0]
        ldclral wzr, wzr, [sp]
        ldclral x1,  x2,  [x0]
        ldclral xzr, xzr, [sp]

        ldeoral w1,  w2,  [x0]
        ldeoral wzr, wzr, [sp]
        ldeoral x1,  x2,  [x0]
        ldeoral xzr, xzr, [sp]

        ldsetal w1,  w2,  [x0]
        ldsetal wzr, wzr, [sp]
        ldsetal x1,  x2,  [x0]
        ldsetal xzr, xzr, [sp]

        ldsmaxal w1,  w2,  [x0]
        ldsmaxal wzr, wzr, [sp]
        ldsmaxal x1,  x2,  [x0]
        ldsmaxal xzr, xzr, [sp]

        ldsminal w1,  w2,  [x0]
        ldsminal wzr, wzr, [sp]
        ldsminal x1,  x2,  [x0]
        ldsminal xzr, xzr, [sp]

        ldumaxal w1,  w2,  [x0]
        ldumaxal wzr, wzr, [sp]
        ldumaxal x1,  x2,  [x0]
        ldumaxal xzr, xzr, [sp]

        lduminal w1,  w2,  [x0]
        lduminal wzr, wzr, [sp]
        lduminal x1,  x2,  [x0]
        lduminal xzr, xzr, [sp]

        swpal w1,  w2,  [x0]
        swpal wzr, wzr, [sp]
        swpal x1,  x2,  [x0]
        swpal xzr, xzr, [sp]

        /*
         * Compare and swap
         */

        casb  w0,  w1, [x0]
        casb wzr, wzr, [sp]

        caslb  w0,  w1, [x0]
        caslb wzr, wzr, [sp]

        casab  w0,  w1, [x0]
        casab wzr, wzr, [sp]

        casalb  w0,  w1, [x0]
        casalb wzr, wzr, [sp]

        cash  w0,  w1, [x0]
        cash wzr, wzr, [sp]

        caslh  w0,  w1, [x0]
        caslh wzr, wzr, [sp]

        casah  w0,  w1, [x0]
        casah wzr, wzr, [sp]

        casalh  w0,  w1, [x0]
        casalh wzr, wzr, [sp]

        cas  w0,  w1, [x0]
        cas wzr, wzr, [sp]

        casl  w0,  w1, [x0]
        casl wzr, wzr, [sp]

        casa  w0,  w1, [x0]
        casa wzr, wzr, [sp]

        casal  w0,  w1, [x0]
        casal wzr, wzr, [sp]

        cas  x0,  x1, [x0]
        cas xzr, xzr, [sp]

        casl  x0,  x1, [x0]
        casl xzr, xzr, [sp]

        casa  x0,  x1, [x0]
        casa xzr, xzr, [sp]

        casal  x0,  x1, [x0]
        casal xzr, xzr, [sp]

        /*
         * Keep last so the testcase can catch errors in
         * the disassembly of the last instruction.
         */
        nop

BEGINPROC_HIDDEN TestProcA64Lse_EndProc
