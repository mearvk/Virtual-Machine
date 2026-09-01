/* $Id: bmpcomp.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2006-2025 Oracle and/or its affiliates.
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

#ifndef VRDP_INCLUDED_SRC_bmpcomp_h
#define VRDP_INCLUDED_SRC_bmpcomp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpdefs.h"

#define VRDP_BMP_TILE_SIZE (64)

#define VRDP_MSFT_CLIENT_MAX_BMP_SIZE (0x7D00)
#define VRDP_MAX_BMP_NETWORK_SIZE     (0x3000)

/* Maximum size of a tile data 64x64x32bpp */
#define VRDP_MAX_BMP_TILE_SIZE        (0x4000)

/* Force bitmap tiling to 64x64 pieces. If not set, an attempt will be made
 * to compress entire bitmap, tiling will be done only when the compressed
 * data (at 24BPP) will not fit in the VRDP_MAX_BMP_NETWORK_SIZE or uncompressed
 * data (at 24BPP) will be larger than VRDP_MSFT_CLIENT_MAX_BMP_SIZE.
 */
#define VRDP_F_BMPCOMP_TILE            (0x00000001)

typedef struct _VRDPCompressedDataBuffer
{
    /* A preallocated buffer. May contain data for a few tiles.
     * It is limited to the maximum size of a bitmap in a network
     * packet because the buffer is used in an attempt to compress
     * entire bitmap without tiling.
     */
    uint8_t    au8Static[VRDP_MAX_BMP_NETWORK_SIZE];

    /* How many bytes are used in the 'au8Static'. */
    uint16_t   cbStatic;
} VRDPCompressedDataBuffer;

#pragma pack(1)
typedef struct _VRDPBitmapTileHash
{
    uint64_t u64CRC;
    uint16_t u16Width;
    uint16_t u16Height;
} VRDPBitmapTileHash;
#pragma pack()

inline bool IsBitmapHashEqual (const VRDPBitmapTileHash *ph1, const VRDPBitmapTileHash *ph2)
{
    return    ph1->u64CRC    == ph2->u64CRC
           && ph1->u16Width  == ph2->u16Width
           && ph1->u16Height == ph2->u16Height;
}

typedef struct _VRDPBitmapCompressedTileDescr
{
    uint8_t       *pu8Data;
    uint16_t       cbData;

    bool           fCompressed;

    uint16_t       x;
    uint16_t       y;

    uint16_t       cTileWidth;
    uint16_t       cHeight;

    uint16_t       cBitmapWidth;

    uint8_t        u8TileSizeClass;

    VRDPBitmapTileHash tileHash;
} VRDPBitmapCompressedTileDescr;

/* Provides the bitmap compression and the reusing of compressed data.
 * Large bitmaps can be tiled to 64x64 pieces.
 */

/*
 * Interface definition.
 *
 * A large bitmap can be tiled either to 64x64 pieces or to a fixed
 * number of columns with variable number of rows in each column.
 */

class VRDPBitmapCompressedMethod;

/* Base interface. */
class VRDPBitmapCompressed
{
    public:
        VRDPBitmapCompressed ();
        ~VRDPBitmapCompressed ();

        /* Size in bytes of the compressed bitmap data. */
        uint32_t CompressedSize(void);

        /* Whether there is compressed bitmap information. */
        bool IsEmpty (void);

        uint8_t GetTileRect (RGNRECT *pRect, uint16_t u16TileX, uint16_t u16TileY);
        void GetBitmapRect (RGNRECT *pRect);

        bool QueryTileHash (uint16_t u16TileX, uint16_t u16TileY, VRDPBitmapTileHash *pTileHash);

        /* Compress given bitmap data to internal buffer.
         *
         * @param pu8Bits          Points to upper left corner of bitmap to be compressed.
         * @param cbLine           Size in bytes of a scanline of the bitmap.
         * @param cWidth           Width of the bitmap in pixels.
         * @param cHeight          Height of the bitmap in pixels.
         * @param cBitsPerPixel    Color depth of the bitmap.
         * @param fu32Options      How the compression must be performed VRDP_F_BMPCOMP_*.
         *
         * @note Returns nothing. If compression fails, which is very unlikely,
         *       the instance will remain empty.
         */
        void Compress (const uint8_t *pu8Bits,
                       uint32_t cbLine,
                       uint16_t cWidth,
                       uint16_t cHeight,
                       uint8_t cBitsPerPixel,
                       uint32_t fu32Options);

        uint32_t GetOptions(void) { return m_fu32Options; }

        /* How many tiles in this bitmap. */
        uint32_t GetTilesTotal(void);

        /* How many columns of tiles in this bitmap. */
        uint16_t GetTilesColumns(void);

        /* How many rows in the column with maximum number of rows. */
        uint16_t GetTilesMaxRows(void);

        /* How many tiles in a column of this bitmap.
         *
         * @param xTile The tile column.
         */
        uint16_t GetTilesRows(uint16_t xTile);

        /*
         * Emits bitmap tile to specified buffer in RDP format. If the data cannot be compressed, emits
         * uncompressed bitmap.
         *
         * @param xTile            The tile column.
         * @param yTile            The tile row.
         * @param fCompress        Enables compression. If false, uncompressed bitmap data is written.
         * @param cBitsPerPixel    Bits per pixel of the target bitmap.
         * @param pu8Dst           Memory buffer for the target bitmap.
         * @param cbDstIn          Initial size of the memory buffer.
         * @param pcbDstOut        Where to store now many bytes of resulting bitmap were placed in the buffer.
         * @param pfCompressed     Pointer to the compression result. True if bitmap was compressed.
         *                         False if uncompressed data were written.
         *
         * @return Whether the operation was successful. It may fail when the input parameters
         *         are not valid, or the 'cbDstIn' bytes is not enough .
         */
        bool Output (uint16_t xTile,
                     uint16_t yTile,
                     bool fCompress,
                     uint8_t cBitsPerPixel,
                     uint8_t *pu8Dst,
                     uint32_t cbDstIn,
                     const VRDPBitmapCompressedTileDescr **pptd,
                     uint32_t *pcbDstOut,
                     bool *pfCompressed);


    private:
        /* The internal compression method implementation. */
        VRDPBitmapCompressedMethod *pMethod;

        /* Compression options. */
        uint32_t m_fu32Options;
};

/* Enables bitmap compression statistics. */
// #define VRDP_BMP_STAT

#ifdef VRDP_BMP_STAT
void VRDPBmpStatReset(void);
void VRDPBmpStatLogRel(void);
#endif /* VRDP_BMP_STAT */

#endif /* !VRDP_INCLUDED_SRC_bmpcomp_h */
