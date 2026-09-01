/* $Id: detect.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * IPRT - Compression Detection.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/zip.h>
#include "internal/iprt.h"



RTDECL(bool) RTZipGzipIsStartOfCompressedStream(uint8_t const *pbStart, size_t cbStart)
{
    /*
     * The gzip header starts with a two byte magic, which isn't much to go on.
     * Fortunately, the 3rd byte which specifies the compression method, is fixed
     * to 8 (deflate).  We can also check the reserved flag bits of the 4th bytes,
     * which according to RFC-1952 must be zero.
     */
    return cbStart >= 4
        && pbStart[0] == 0x1f           /* ID1 */
        && pbStart[1] == 0x8b           /* ID2 */
        && pbStart[2] == 0x08           /* CM - deflate */
        && (pbStart[3] & 0xe0) == 0;    /* FLG - bits 5, 6 & 7 MBZ. */
}


RTDECL(bool) RTZipBzip2IsStartOfCompressedStream(uint8_t const *pbStart, size_t cbStart)
{
    /*
     * The first 10 bytes of the Bzip2 header has 9 fixed bytes and the only
     * variable one is severely restricted in range.  This makes it reasonably
     * easy to detect.
     */
    return cbStart >= 6
        && pbStart[0] == 0x42           /* magic[0] = 'B' */
        && pbStart[1] == 0x5a           /* magic[0] = 'Z' */
        && pbStart[2] == 0x68           /* version  = 'h' - huffman encoding; BZip1 uses '0' (0x30). */
        && pbStart[3] >= 0x31           /* block-size (ascii) in 100KB units. */
        && pbStart[3] <= 0x39
        && pbStart[4] == 0x31           /* 6 byte block header = 0x314159265359 (pi) */
        && pbStart[5] == 0x41
        && (cbStart <= 6 || pbStart[6] == 0x59)
        && (cbStart <= 7 || pbStart[7] == 0x26)
        && (cbStart <= 8 || pbStart[8] == 0x53)
        && (cbStart <= 9 || pbStart[9] == 0x59);
}


RTDECL(bool) RTZipXzIsStartOfCompressedStream(uint8_t const *pbStart, size_t cbStart)
{
    /* The XZ magic is 6 bytes long. */
    return cbStart >= 6
        && pbStart[0] == 0xfd
        && pbStart[1] == 0x37
        && pbStart[2] == 0x7a
        && pbStart[3] == 0x58
        && pbStart[4] == 0x5a
        && pbStart[5] == 0x00;
}

