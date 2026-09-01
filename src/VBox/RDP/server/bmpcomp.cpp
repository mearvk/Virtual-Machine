/* $Id: bmpcomp.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "vrdp.h"
#include "utils.h"
#include "shadowbuffer.h"

#include "bmpcomp.h"

#include <iprt/crc.h>

#define VRDP_LOW_PROTOCOLS_RESERVE (128)

#define BMPCACHE2_SQUARE      0x0080
#define BMPCACHE2_COMPRESSED  0x0400
#define BMPCACHE2_MODE_SHIFT  3

#define BMPCACHE2_LONG_FORMAT 0x8000

#define TS_FRAME_START 0x00000000
#define TS_FRAME_END 0x00000001

#define VRDP_BITMAP_UPDATE_HDR_SIZE (18)

int VRDPTP::OutputBitmap2 (unsigned uScreenId, VRDPStream *pStream, VRDPBitmapCompressed *pBmp, const int16_t x, const int16_t y)
{
    int rc = VINF_SUCCESS;

    int xTile, yTile;

    TESTLOG(("at %d,%d\n", x, y));

    bool fFrameMarker =    (Data()->OrderSupportExFlags() & TS_ORDERFLAGS_EX_ALTSEC_FRAME_MARKER_SUPPORT) != 0
                        && pBmp->GetTilesTotal() > 1;

    if (fFrameMarker)
    {
        OutputFrameMarker(pStream, TS_FRAME_START);
    }

    /* Draw bitmap left to right, top to bottom. */
    uint16_t cTilesX = pBmp->GetTilesColumns();
    uint16_t cTilesY = pBmp->GetTilesMaxRows();

    bool fTiled64x64 = (pBmp->GetOptions() & VRDP_F_BMPCOMP_TILE) != 0;

    TESTLOG(("tiles %dx%d\n", cTilesX, cTilesY));

    yTile = fTiled64x64? 0: cTilesY;

    while (fTiled64x64? yTile < cTilesY: yTile > 0)
    {
        if (!fTiled64x64)
        {
            yTile--;
        }

        for (xTile = 0; RT_SUCCESS(rc) && xTile < cTilesX; xTile++)
        {
            const VRDPStream::MemoryBlock *pBlock;

            /* Reserve enough space for an order with complete information. */
            pBlock = pStream->BeginBlock (VRDPUpdate_Bitmap,
                                          VRDP_BITMAP_UPDATE_HDR_SIZE + VRDP_NETWORK_PACKET_SIZE_ALLOC);

            if (pBlock == NULL)
            {
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            uint32_t cbSize;
            bool fCompressed;

            const VRDPBitmapCompressedTileDescr *ptd;

            bool fBitmapProcessed = pBmp->Output (xTile, yTile, m_fBitmapCompression,
                                                  m_bpp,
                                                  pBlock->pu8DstStart + VRDP_BITMAP_UPDATE_HDR_SIZE,
                                                  VRDP_NETWORK_PACKET_SIZE_ALLOC,
                                                  &ptd, &cbSize, &fCompressed);

            if (!fBitmapProcessed)
            {
                /* Ignore the tile and cancel the block. */
                pStream->EndBlock (pBlock, 0);
                continue;
            }

            /* Check that the rectangle is ok for the client resolution. */
            RGNRECT rectBitmap;
            rectBitmap.x = x + ptd->x;
            rectBitmap.y = y + ptd->y;
            rectBitmap.w = ptd->cTileWidth;
            rectBitmap.h = ptd->cHeight;

            if (!DesktopMap()->ClipScreen2Client (uScreenId, &rectBitmap, &rectBitmap))
            {
                TESTLOG(("Skipping tile at %d,%d\n", x + ptd->x, y + ptd->y));
                pStream->EndBlock (pBlock, 0);
                continue;
            }

            int16_t i16ClientX = (int16_t)rectBitmap.x;
            int16_t i16ClientY = (int16_t)rectBitmap.y;
            uint16_t u16ClientW = (uint16_t)rectBitmap.w;
            uint16_t u16ClientH = (uint16_t)rectBitmap.h;

            TESTLOG(("Client tile %d,%d %dx%d\n", i16ClientX, i16ClientY, u16ClientW, u16ClientH));

            uint16_t *pu16Dst = (uint16_t *)pBlock->pu8DstStart;

            *pu16Dst++ = i16ClientX;                            /* left */
            *pu16Dst++ = i16ClientY;                            /* top */

            *pu16Dst++ = (uint16_t)(i16ClientX + u16ClientW - 1); /* right */
            *pu16Dst++ = (uint16_t)(i16ClientY + u16ClientH - 1); /* bottom */

            *pu16Dst++ = ptd->cBitmapWidth;                     /* width */
            *pu16Dst++ = ptd->cHeight;                          /* height */

            *pu16Dst++ = m_bpp;
            *pu16Dst++ = (uint16_t)(fCompressed? 0x401: 0);

            *pu16Dst++ = cbSize;

            /* Complete the block. Pass how many bytes were actually written. */
            pStream->EndBlock (pBlock, VRDP_BITMAP_UPDATE_HDR_SIZE + cbSize);
        }

        if (fTiled64x64)
        {
            yTile++;
        }
    }

    if (fFrameMarker)
    {
        OutputFrameMarker(pStream, TS_FRAME_END);
    }

    return rc;
}

#ifdef VRDP_BMP_STAT
typedef struct VRDPBmpStat
{
    uint64_t u64Bitmaps; /* Number of processed bitmaps. */
    uint64_t u64Square;  /* Square of all processed bitmaps in pixels. */
    uint64_t u64Tiles;   /* Total number of generated tiles. */
    uint64_t u64UncompressedBytes; /* Total number of bytes in uncompressed bitmaps. */
    uint64_t u64CompressedBytes; /* Total number of bytes in compressed tiles. */
    uint64_t u64Ticks;   /* CPU ticks for compression. */
} VRDPBmpStat;

static VRDPBmpStat gsStat;

void VRDPBmpStatReset(void)
{
    memset(&gsStat, 0, sizeof(gsStat));
}

void VRDPBmpStatLogRel(void)
{
    VRDPLOGREL(("BMPSTAT: bitmaps %llu, square %llu, tiles %llu, s/t %llu, t/b %llu\n"
                "BMPSTAT: bytes %llu, compr %llu, ratio %llu%%\n"
                "BMPSTAT: ticks %llu, ticks/s %llu\n",
                gsStat.u64Bitmaps,
                gsStat.u64Square,
                gsStat.u64Tiles,
                gsStat.u64Tiles? gsStat.u64Square / gsStat.u64Tiles: 0,
                gsStat.u64Square? gsStat.u64Tiles / gsStat.u64Bitmaps: 0,
                gsStat.u64UncompressedBytes,
                gsStat.u64CompressedBytes,
                gsStat.u64UncompressedBytes? (gsStat.u64CompressedBytes * 100) / gsStat.u64UncompressedBytes: 0,
                gsStat.u64Ticks,
                gsStat.u64Square? gsStat.u64Ticks / gsStat.u64Square: 0
                ));
}

static void vrdpBmpStatBitmap(uint32_t w, uint32_t h, uint32_t cbPixel, uint32_t cTiles, uint32_t cbCompr)
{
    gsStat.u64Bitmaps++;
    gsStat.u64Square += w * h;
    gsStat.u64Tiles += cTiles;
    gsStat.u64UncompressedBytes += w * h * cbPixel;
    gsStat.u64CompressedBytes += cbCompr;
}

static void vrdpBmpStatTicks(uint64_t u64Ticks)
{
    gsStat.u64Ticks += u64Ticks;
}

#define BMPSTAT_BITMAP vrdpBmpStatBitmap
#define BMPSTAT_TICKS vrdpBmpStatTicks

#else

#define BMPSTAT_BITMAP(w,h,cbPixel,cTiles,cbCompr) do {} while (0)
#define BMPSTAT_TICKS(u64Ticks) do {} while (0)

#endif /* VRDP_BMP_STAT */

/*
 * Bitmap compression implementation.
 */

/* Base interface for a bitmap compression implementation. */
class VRDPBitmapCompressedMethod
{
    public:
        VRDPBitmapCompressedMethod() {};
        virtual ~VRDPBitmapCompressedMethod() {};

        virtual uint32_t CompressedSize(void) = 0;
        virtual bool IsEmpty (void) = 0;
        virtual uint8_t GetTileRect (RGNRECT *pRect, uint16_t u16TileX, uint16_t u16TileY) = 0;
        virtual void GetBitmapRect (RGNRECT *pRect) = 0;
        virtual bool QueryTileHash (uint16_t u16TileX, uint16_t u16TileY, VRDPBitmapTileHash *pTileHash) = 0;
        virtual void Compress (const uint8_t *pu8Bits, uint32_t cbLine, uint16_t cWidth, uint16_t cHeight,
                               uint8_t cBitsPerPixel, uint32_t fu32Options) = 0;
        virtual uint32_t GetTilesTotal(void) = 0;
        virtual uint16_t GetTilesColumns(void) = 0;
        virtual uint16_t GetTilesMaxRows(void) = 0;
        virtual uint16_t GetTilesRows(uint16_t xTile) = 0;
        virtual bool Output (uint16_t xTile, uint16_t yTile, bool fCompress, uint8_t cBitsPerPixel,
                     uint8_t *pu8Dst, uint32_t cbDstIn, const VRDPBitmapCompressedTileDescr **pptd,
                     uint32_t *pcbDstOut, bool *pfCompressed) = 0;
};

typedef struct VRDPBitmapTilesColumn
{
    uint16_t u16Rows;                        /* Number of tiles in this column. */
    VRDPBitmapCompressedTileDescr *paTiles;  /* Pointer to array which contains u16Rows descriptions of tiles. */
} VRDPBitmapTilesColumn;

typedef struct VRDPBitmapTilesInfo
{
    uint32_t u32Tiles;

    uint16_t u16MaxRows; /* How many rows in the column with largest number of them. */

    uint16_t u16Columns;
    VRDPBitmapTilesColumn aColumns[1]; /* u16Columns elements in this array. */
} VRDPBitmapTilesInfo;

class VRDPBitmapCompressedRLE: public VRDPBitmapCompressedMethod
{
    public:
        VRDPBitmapCompressedRLE ();
        virtual ~VRDPBitmapCompressedRLE ();

        virtual uint32_t CompressedSize(void);

        virtual bool IsEmpty (void) { return (m_pTiles == NULL || m_pTiles->u32Tiles == 0); };

        virtual uint8_t GetTileRect (RGNRECT *pRect, uint16_t u16TileX, uint16_t u16TileY);
        virtual void GetBitmapRect (RGNRECT *pRect);

        virtual bool QueryTileHash (uint16_t u16TileX, uint16_t u16TileY, VRDPBitmapTileHash *pTileHash);

        virtual void Compress (const uint8_t *pu8Bits,
                       uint32_t cbLine,
                       uint16_t cWidth,
                       uint16_t cHeight,
                       uint8_t cBitsPerPixel,
                       uint32_t fu32Options);

        virtual uint32_t GetTilesTotal(void) { return m_pTiles? m_pTiles->u32Tiles: 0; }
        virtual uint16_t GetTilesColumns(void) { return m_pTiles? m_pTiles->u16Columns: 0; }
        virtual uint16_t GetTilesMaxRows(void) { return m_pTiles? m_pTiles->u16MaxRows: 0; }
        virtual uint16_t GetTilesRows(uint16_t xTile) { return xTile < GetTilesColumns()?
                                                                   m_pTiles->aColumns[xTile].u16Rows: 0; }

        virtual bool Output (uint16_t xTile,
                     uint16_t yTile,
                     bool fCompress,
                     uint8_t cBitsPerPixel,
                     uint8_t *pu8Dst,
                     uint32_t cbDstIn,
                     const VRDPBitmapCompressedTileDescr **pptd,
                     uint32_t *pcbDstOut,
                     bool *pfCompressed);


    private:

        void destroyMembers (void);

        VRDPBitmapCompressedTileDescr *tlDescrFromPos (uint16_t u16TileX, uint16_t u16TileY);

        int tlDescrsAllocColumns(uint16_t u16Columns);
        void tlDescrsFree (void);

        int tlDescrsAdd (uint16_t cTileX, uint16_t cTileY);

        /* The color depth of the bitmap. */
        uint8_t m_cBitsPerPixel;
        uint8_t m_cbPixel;

        VRDPBitmapTilesInfo *m_pTiles;

        uint16_t m_cWidth;
        uint16_t m_cHeight;

        uint16_t m_xLastTile;
        uint16_t m_yLastTile;

        /* Preallocated memory buffer for compressed tiles. */
        VRDPCompressedDataBuffer m_data;
};


/* Implementation of the bitmap compression interface.
 * @todo currently it is a proxy, which forwards calls to the actual implementation.
 * When another bitmap compression method will be implemented, some code might be shared.
 */
VRDPBitmapCompressed::VRDPBitmapCompressed()
    :
    pMethod(0),
    m_fu32Options(0)
{
}

VRDPBitmapCompressed::~VRDPBitmapCompressed()
{
    if (pMethod)
    {
        delete pMethod;
    }
}

uint32_t VRDPBitmapCompressed::CompressedSize(void)
{
    return pMethod? pMethod->CompressedSize(): 0;
}

bool VRDPBitmapCompressed::IsEmpty (void)
{
    return pMethod? pMethod->IsEmpty(): true;
}

uint8_t VRDPBitmapCompressed::GetTileRect (RGNRECT *pRect, uint16_t u16TileX, uint16_t u16TileY)
{
    if (pMethod)
    {
        return pMethod->GetTileRect(pRect, u16TileX, u16TileY);
    }

    memset(pRect, 0, sizeof(RGNRECT));
    return 0xFF;
}

void VRDPBitmapCompressed::GetBitmapRect(RGNRECT *pRect)
{
    if (pMethod)
    {
        pMethod->GetBitmapRect(pRect);
    }
    else
    {
        memset(pRect, 0, sizeof(RGNRECT));
    }
}

bool VRDPBitmapCompressed::QueryTileHash (uint16_t u16TileX, uint16_t u16TileY, VRDPBitmapTileHash *pTileHash)
{
    return pMethod? pMethod->QueryTileHash (u16TileX, u16TileY, pTileHash): false;
}

void VRDPBitmapCompressed::Compress (const uint8_t *pu8Bits,
               uint32_t cbLine,
               uint16_t cWidth,
               uint16_t cHeight,
               uint8_t cBitsPerPixel,
               uint32_t fu32Options)
{
    if (!pMethod)
    {
        pMethod = new VRDPBitmapCompressedRLE();
    }

    if (pMethod)
    {
        pMethod->Compress(pu8Bits, cbLine, cWidth, cHeight, cBitsPerPixel, fu32Options);
        if (!IsEmpty())
        {
            m_fu32Options = fu32Options;
        }
    }
}

uint32_t VRDPBitmapCompressed::GetTilesTotal(void)
{
    return pMethod? pMethod->GetTilesTotal(): 0;
}

uint16_t VRDPBitmapCompressed::GetTilesColumns(void)
{
    return pMethod? pMethod->GetTilesColumns(): 0;
}

uint16_t VRDPBitmapCompressed::GetTilesMaxRows(void)
{
    return pMethod? pMethod->GetTilesMaxRows(): 0;
}

uint16_t VRDPBitmapCompressed::GetTilesRows(uint16_t xTile)
{
    return pMethod? pMethod->GetTilesRows(xTile): 0;
}

bool VRDPBitmapCompressed::Output (uint16_t xTile,
             uint16_t yTile,
             bool fCompress,
             uint8_t cBitsPerPixel,
             uint8_t *pu8Dst,
             uint32_t cbDstIn,
             const VRDPBitmapCompressedTileDescr **pptd,
             uint32_t *pcbDstOut,
             bool *pfCompressed)
{
    return pMethod? pMethod->Output(xTile, yTile, fCompress, cBitsPerPixel, pu8Dst,
                                    cbDstIn, pptd, pcbDstOut, pfCompressed): false;
}


static void bmpHash (VRDPBitmapTileHash *pTileHash,
                     const uint8_t * pu8Bits,
                     uint16_t u16Width,
                     uint16_t u16Height,
                     uint32_t cbLine,
                     uint8_t cBitsPerPixel)
{
    /* Save the tile dimensions. */
    pTileHash->u16Width  = u16Width;
    pTileHash->u16Height = u16Height;

    /* And also compute CRC64 for bits. */
    uint32_t cBytesPerLine = u16Width * ((cBitsPerPixel + 7) / 8);

    uint64_t u64CRC = RTCrc64Start ();

    while (u16Height > 0)
    {
        u64CRC = RTCrc64Process (u64CRC, pu8Bits, cBytesPerLine);
        pu8Bits += cbLine;
        u16Height--;
    }

    u64CRC = RTCrc64Finish (u64CRC);

    pTileHash->u64CRC = u64CRC;
}

static bool tlCopy (uint8_t *pu8Dst,
                    uint32_t cbDstIn,
                    uint32_t *pcbDstOut,
                    VRDPBitmapCompressedTileDescr *ptd)
{
    TESTLOG(("cbDstIn %d, ptd->cbData %d, *pcbDstOut %d\n", cbDstIn, ptd->cbData, *pcbDstOut));

    if (cbDstIn >= ptd->cbData)
    {
        memcpy (pu8Dst, ptd->pu8Data, ptd->cbData);
        *pcbDstOut = ptd->cbData;
        return true;
    }

    return false;
}

DECLINLINE(unsigned) bmpcomp_getpixel_ex (const uint8_t *pu8Src, unsigned x,
                                          PFNGETPIXEL pfnGetPixel,
                                          uint32_t cNewWidth, uint32_t cWidth)
{
    RT_NOREF1(cNewWidth);
    Assert (cWidth > 0);
    Assert (x < cNewWidth);

    if (x >= cWidth)
    {
        x = cWidth - 1;
    }

    return pfnGetPixel (pu8Src, x);
}


/*
 * Bitmap compression implementation:
 * 1) large bitmaps, which are not tiled for caching (64x64), are tiled to
 *    columns of 256 bytes. This should give a better compression ratio,
 *    because the first scanline can not be compressed as efficient as the
 *    next ones. And 1920 pixels of first scanline use too much space in
 *    the compressed data buffer.
 * 2) The bitmap compression function compresses as many scanlines as possible
 *    and returns the number of compressed scanlines. So it has to compress
 *    each scan line separately.
 */
static int VRDPBMPCompressEx(bool fCompress,
                             bool fPartialOK,
                             const uint8_t *pu8SrcBits,
                             uint32_t cbSrcLine,
                             uint32_t cSrcWidth,
                             uint32_t cSrcHeight,
                             uint32_t cSrcBitsPerPixel,
                             uint8_t *pu8Dst,
                             const uint32_t cbDst,
                             uint32_t *pcbDstOut,
                             bool *pfCompressed,
                             uint32_t *pu32NewWidth,
                             uint32_t *pu32NewHeight);
static int VRDPBMPCompressEx32(bool fCompress,
                               bool fPartialOK,
                               const uint8_t *pu8SrcBits,
                               uint32_t cbSrcLine,
                               uint32_t cSrcWidth,
                               uint32_t cSrcHeight,
                               uint8_t *pu8Dst,
                               const uint32_t cbDst,
                               uint32_t *pcbDstOut,
                               bool *pfCompressed,
                               uint32_t *pu32NewWidth,
                               uint32_t *pu32NewHeight);

VRDPBitmapCompressedRLE::VRDPBitmapCompressedRLE ()
    :
    m_cBitsPerPixel (0),
    m_cbPixel (0),
    m_pTiles (NULL),
    m_cWidth (0),
    m_cHeight (0),
    m_xLastTile (0),
    m_yLastTile (0)
{
    m_data.cbStatic = 0;
}

VRDPBitmapCompressedRLE::~VRDPBitmapCompressedRLE ()
{
    destroyMembers ();
}

void VRDPBitmapCompressedRLE::destroyMembers (void)
{
    tlDescrsFree ();

    m_cBitsPerPixel = 0;
    m_cbPixel = 0;

    m_data.cbStatic = 0;
}

uint32_t VRDPBitmapCompressedRLE::CompressedSize(void)
{
    uint32_t u32CompressedSize = 0;

    uint16_t u16TileX, u16TileY;

    uint16_t cTilesX = GetTilesColumns();

    for (u16TileX = 0; u16TileX < cTilesX; u16TileX++)
    {
        uint16_t cTilesY = GetTilesRows(u16TileX);

        for (u16TileY = 0; u16TileY < cTilesY; u16TileY++)
        {
            VRDPBitmapCompressedTileDescr *ptd = tlDescrFromPos (u16TileX, u16TileY);
            u32CompressedSize += ptd->cbData;
        }
    }

    return u32CompressedSize;
}

uint8_t VRDPBitmapCompressedRLE::GetTileRect (RGNRECT *pRect, uint16_t u16TileX, uint16_t u16TileY)
{
    VRDPBitmapCompressedTileDescr *ptd = tlDescrFromPos (u16TileX, u16TileY);

    if (ptd)
    {
        pRect->x = ptd->x;
        pRect->y = ptd->y;
        pRect->w = ptd->cTileWidth;
        pRect->h = ptd->cHeight;

        return ptd->u8TileSizeClass;
    }

    memset (pRect, 0, sizeof (RGNRECT));

    return 0xFF;
}

void VRDPBitmapCompressedRLE::GetBitmapRect (RGNRECT *pRect)
{
    pRect->x = 0;
    pRect->y = 0;
    pRect->w = m_cWidth;
    pRect->h = m_cHeight;
}

bool VRDPBitmapCompressedRLE::QueryTileHash (uint16_t u16TileX, uint16_t u16TileY, VRDPBitmapTileHash *pTileHash)
{
    VRDPBitmapCompressedTileDescr *ptd = tlDescrFromPos (u16TileX, u16TileY);

    if (ptd)
    {
        *pTileHash = ptd->tileHash;
        return true;
    }

    return false;
}

VRDPBitmapCompressedTileDescr *VRDPBitmapCompressedRLE::tlDescrFromPos (uint16_t u16TileX, uint16_t u16TileY)
{
    VRDPBitmapCompressedTileDescr *ptd = NULL;

    if (m_pTiles)
    {
        if (u16TileX < m_pTiles->u16Columns)
        {
            if (u16TileY < m_pTiles->aColumns[u16TileX].u16Rows)
            {
                ptd = &m_pTiles->aColumns[u16TileX].paTiles[u16TileY];
            }
        }
    }

    return ptd;
}

int VRDPBitmapCompressedRLE::tlDescrsAllocColumns(uint16_t u16Columns)
{
    if (m_pTiles)
    {
        AssertFailed(); /* Should not happpen. */
        tlDescrsFree();
    }

    if (   u16Columns == 0
        || u16Columns > 32*_1K)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /* One VRDPBitmapTilesColumn is already in the VRDPBitmapTilesInfo. */
    size_t cbAlloc = sizeof(VRDPBitmapTilesInfo) + (u16Columns - 1) * sizeof(VRDPBitmapTilesColumn);

    m_pTiles = (VRDPBitmapTilesInfo *)VRDPMemAllocZ(cbAlloc);

    if (m_pTiles == NULL)
    {
        return VERR_NO_MEMORY;
    }

    m_pTiles->u16Columns = u16Columns;

    return VINF_SUCCESS;
}

int VRDPBitmapCompressedRLE::tlDescrsAdd (uint16_t cTileX, uint16_t cTileY)
{
    /* Existing columns may grow. */
    if (   m_pTiles == NULL
        || cTileX >= m_pTiles->u16Columns
        || cTileY > 32*_1K)
    {
        return VERR_INVALID_PARAMETER;
    }

    VRDPBitmapTilesColumn *pColumn = &m_pTiles->aColumns[cTileX];

    /* Check if there is a need to extend the rows array. */
    uint16_t u16NewRows = cTileY + 1;

    if (u16NewRows <= pColumn->u16Rows)
    {
        /* There is already a tile. Should not happen with current code. */
        AssertFailed();
        return VWRN_INVALID_PARAMETER;
    }

    /* Try to allocate enough space in the column. */
    size_t cbAlloc = sizeof(VRDPBitmapCompressedTileDescr) * u16NewRows;
    void *pv = VRDPMemRealloc(pColumn->paTiles, cbAlloc);

    if (pv == NULL)
    {
        AssertFailed();
        return VERR_NO_MEMORY;
    }

    uint16_t cNewTiles = u16NewRows - pColumn->u16Rows;

    m_pTiles->u32Tiles += cNewTiles;

    pColumn->paTiles = (VRDPBitmapCompressedTileDescr *)pv;

    /* Clear allocated tiles. */
    memset(&pColumn->paTiles[pColumn->u16Rows], 0, sizeof(VRDPBitmapCompressedTileDescr) * cNewTiles);

    pColumn->u16Rows = u16NewRows;

    if (m_pTiles->u16MaxRows < pColumn->u16Rows)
    {
        m_pTiles->u16MaxRows = pColumn->u16Rows;
    }

    return VINF_SUCCESS;
}

void VRDPBitmapCompressedRLE::tlDescrsFree(void)
{
    /* Free the tiles memory allocated dynamically. */
    uint16_t u16TileX, u16TileY;

    uint16_t cTilesX = GetTilesColumns();

    for (u16TileX = 0; u16TileX < cTilesX; u16TileX++)
    {
        uint16_t cTilesY = GetTilesRows(u16TileX);

        for (u16TileY = 0; u16TileY < cTilesY; u16TileY++)
        {
            VRDPBitmapCompressedTileDescr *ptd = tlDescrFromPos (u16TileX, u16TileY);

            if (   &m_data.au8Static[0] <= ptd->pu8Data
                && ptd->pu8Data < &m_data.au8Static[sizeof (m_data.au8Static)])
            {
                /* Do nothing. The memory was allocated from the static buffer. */
            }
            else
            {
                VRDPMemFree ((void *)ptd->pu8Data);
            }
        }

        /* Deallocate data for this column. */
        VRDPMemFree(m_pTiles->aColumns[u16TileX].paTiles);
        m_pTiles->aColumns[u16TileX].paTiles = NULL;
        m_pTiles->aColumns[u16TileX].u16Rows = 0;
    }

    VRDPMemFree(m_pTiles);
    m_pTiles = NULL;
}

void VRDPBitmapCompressedRLE::Compress (const uint8_t *pu8Bits,
                                     uint32_t cbLine,
                                     uint16_t cWidth,
                                     uint16_t cHeight,
                                     uint8_t cBitsPerPixel,
                                     uint32_t fu32Options)
{
    /* The upper level code must not call the method twice. Having a runtime check
     * like 'if (!IsEmpty ()) return;' is a waste of CPU. So just assert.
     */
    Assert (IsEmpty ());

    VRDPTPLOG(("pu8Bits = %p, cbLine = %d, cWidth = %d, cHeight = %d, cBitsPerPixel = %d, fu32Options = 0x%08X\n",
               pu8Bits, cbLine, cWidth, cHeight, cBitsPerPixel, fu32Options));

    Assert (   pu8Bits
            && cbLine
            && cWidth
            && cHeight
            && (cBitsPerPixel == 8 || cBitsPerPixel == 16 || cBitsPerPixel == 24 || cBitsPerPixel == 32)
            && (fu32Options & ~(VRDP_F_BMPCOMP_TILE)) == 0
            && cbLine >= (uint32_t)(cWidth * ((cBitsPerPixel + 7) / 8))
           );


    int rc = VINF_SUCCESS;

    bool fCompressed = false;
    uint32_t u32NewHeight = 0;
    uint32_t u32NewWidth = 0;
    uint32_t cbData = 0;

    /*
     * Bitmap update, sent to the client, is restricted in size:
     *   1) uncompressed bitmap data must not exceed VRDP_MSFT_CLIENT_MAX_BMP_SIZE;
     *   2) bitmap data, which is sent, must not exceed the network packet limit (VRDP_MAX_BMP_NETWORK_SIZE).
     *
     * If tiling is forced, the bitmap is compressed to 64x64 tiles. That is used
     * for cached bitmaps.
     *
     * If no tiling is requested then try to compress bitmaps tiles using fixed width columns
     * with variable number of tiles (as few as possible) in each column.
     * A small width of a column produces better compression ratio, because the first scanline
     * can not be compressed well, but other scanlines are usually similar to the first one
     * and use a DIFF compression.
     *
     * 128 pixels should be good enough. The bitmap size at 24bpp is 128*83.
     *
     */

    bool fBitmapProcessed = false;

    m_cBitsPerPixel = cBitsPerPixel;
    m_cbPixel = (cBitsPerPixel + 7) / 8;

    if (fu32Options & VRDP_F_BMPCOMP_TILE)
    {
        /* The bitmap must be tiled. */
    }
    else
    {
        /* Try to compress bitmap columns. */
        #define VRDP_BITMAP_COLUMN_WIDTH 128

        uint16_t u16Columns = (uint16_t)((cWidth + VRDP_BITMAP_COLUMN_WIDTH - 1) / VRDP_BITMAP_COLUMN_WIDTH);

        /* The bitmap is compressed as columns. */
        rc = tlDescrsAllocColumns(u16Columns);

        if (RT_SUCCESS(rc))
        {
            /* Size of the buffer for a maximum network packet. */
            uint32_t cbCompressedSize;
            if (m_cbPixel < 3)
            {
                /* The compressed data could be expanded to 24 bpp. Limit the compressed
                 * data size so it will not exceed the network packet after expansion.
                 */
                cbCompressedSize = (VRDP_MAX_BMP_NETWORK_SIZE / 3) * m_cbPixel; /* 24_bpp_pixels * actual_pixel_size. */
            }
            else
            {
                /* 24->24 or 32->24 when sending data to the client. */
                cbCompressedSize = VRDP_MAX_BMP_NETWORK_SIZE;
            }

            uint32_t u32RemainingWidth = cWidth;

            uint16_t u16TileX;
            for (u16TileX = 0; u16TileX < u16Columns && RT_SUCCESS(rc); u16TileX++)
            {
                /* Try to compress as many scanlines as possible in each column
                 * to have a bitmap which is not larger then VRDP_MSFT_CLIENT_MAX_BMP_SIZE
                 * when uncompressed and which fits into the network packet compressed.
                 */
                uint32_t u32ColumnWidth = RT_MIN(u32RemainingWidth, VRDP_BITMAP_COLUMN_WIDTH);

                uint32_t u32RemainingHeight = cHeight;

                uint16_t u16TileY = 0;
                while (u32RemainingHeight > 0)
                {
                    bool fCompress = true;

                    /* Check how many lines in a 24bpp uncompressed bitmap will fit into the client's buffer. */
                    u32NewHeight = VRDP_MSFT_CLIENT_MAX_BMP_SIZE / (((u32ColumnWidth + 3) & ~3) * 3); /* maxsize / linesize */

                    if (u32NewHeight > u32RemainingHeight)
                    {
                        u32NewHeight = u32RemainingHeight;
                    }

                    uint8_t *pu8Dst = (uint8_t *)VRDPMemAlloc(cbCompressedSize);

                    if (pu8Dst == NULL)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    const uint8_t *pu8TileSrc =   pu8Bits + u16TileX * VRDP_BITMAP_COLUMN_WIDTH * m_cbPixel
                                                + (u32RemainingHeight - u32NewHeight) * cbLine;

                    if (m_cbPixel == 4)
                    {
                        rc = VRDPBMPCompressEx32(fCompress,
                                                 /* fPartialOK = */ true,
                                                 pu8TileSrc,
                                                 cbLine,
                                                 u32ColumnWidth,
                                                 u32NewHeight,
                                                 pu8Dst,
                                                 cbCompressedSize,
                                                 &cbData,
                                                 &fCompress,
                                                 &u32NewWidth,
                                                 &u32NewHeight);
                    }
                    else
                    {
                        rc = VRDPBMPCompressEx(fCompress,
                                               /* fPartialOK = */ true,
                                               pu8TileSrc,
                                               cbLine,
                                               u32ColumnWidth,
                                               u32NewHeight,
                                               cBitsPerPixel,
                                               pu8Dst,
                                               cbCompressedSize,
                                               &cbData,
                                               &fCompress,
                                               &u32NewWidth,
                                               &u32NewHeight);
                    }

                    if (RT_FAILURE(rc))
                    {
                        VRDPMemFree(pu8Dst);
                        break;
                    }

                    /* Add tile. */
                    rc = tlDescrsAdd(u16TileX, u16TileY);

                    if (RT_FAILURE(rc))
                    {
                        VRDPMemFree(pu8Dst);
                        break;
                    }

                    /* Most likely the compressed data is smaller than the original buffer. */
                    pu8Dst = (uint8_t *)VRDPMemRealloc(pu8Dst, cbData);

                    u32RemainingHeight -= u32NewHeight;

                    VRDPBitmapCompressedTileDescr *ptd = tlDescrFromPos(u16TileX, u16TileY);

                    TESTLOG(("tile %d,%d\n", u16TileX, u16TileY));

                    ptd->pu8Data         = pu8Dst;
                    ptd->cbData          = cbData;
                    ptd->fCompressed     = fCompress;
                    ptd->x               = u16TileX * VRDP_BITMAP_COLUMN_WIDTH;
                    ptd->y               = (uint16_t)u32RemainingHeight;
                    ptd->cTileWidth      = u32ColumnWidth;
                    ptd->cHeight         = (uint16_t)u32NewHeight;
                    ptd->cBitmapWidth    = (uint16_t)u32NewWidth;
                    ptd->u8TileSizeClass = 0xFF;

                    bmpHash(&ptd->tileHash, pu8TileSrc, ptd->cTileWidth, ptd->cHeight, cbLine, cBitsPerPixel);

                    u16TileY++;
                }

                u32RemainingWidth -= u32ColumnWidth;
            }
        }

        if (RT_SUCCESS (rc))
        {
            fBitmapProcessed = true;
        }
        else
        {
            tlDescrsFree ();
        }
    }

    if (!fBitmapProcessed)
    {
        /*
         * Do tiling. Also as a fallback for failed horizontal slices compression.
         */

        /* Compressed tile destination buffer. */
        uint8_t au8Dst[VRDP_MAX_BMP_TILE_SIZE];

        /* Compute how many tiles the bitmap will contain. */
        uint16_t cTilesX = (uint16_t)((cWidth  + VRDP_BMP_TILE_SIZE - 1) / VRDP_BMP_TILE_SIZE);
        uint16_t cTilesY = (uint16_t)((cHeight + VRDP_BMP_TILE_SIZE - 1) / VRDP_BMP_TILE_SIZE);

        /* Compute the size of the last tile. */
        m_xLastTile = (uint16_t)(cWidth  + VRDP_BMP_TILE_SIZE - cTilesX * VRDP_BMP_TILE_SIZE);
        m_yLastTile = (uint16_t)(cHeight + VRDP_BMP_TILE_SIZE - cTilesY * VRDP_BMP_TILE_SIZE);

        TESTLOG(("cTilesX = %d, cTilesY = %d, xLastTile = %d, yLastTile = %d\n", cTilesX, cTilesY, m_xLastTile, m_yLastTile));

        rc = tlDescrsAllocColumns(cTilesX);

        TESTLOG(("pu8Bits %p, pu8Last %p\n", pu8Bits, pu8Bits + cbLine * cHeight));

        /*
         * Compress tiles.
         */
        int tileX, tileY;
        fCompressed = false;

        for (tileX = 0; tileX < cTilesX && RT_SUCCESS(rc); tileX++)
        {
            const uint8_t *pu8 = pu8Bits + m_cbPixel * tileX * VRDP_BMP_TILE_SIZE;

            for (tileY = 0; tileY < cTilesY;
                 tileY++, fCompressed = false, pu8 += cbLine * VRDP_BMP_TILE_SIZE)
            {
                /* Add tile. */
                rc = tlDescrsAdd(tileX, tileY);

                if (RT_FAILURE (rc))
                {
                    break;
                }

                VRDPBitmapCompressedTileDescr *ptd = tlDescrFromPos (tileX, tileY);

                uint16_t cTileWidth  = (tileX == cTilesX - 1)? m_xLastTile: VRDP_BMP_TILE_SIZE;
                uint16_t cTileHeight = (tileY == cTilesY - 1)? m_yLastTile: VRDP_BMP_TILE_SIZE;

                u32NewWidth = 0;
                cbData = 0;

                rc = VRDPBMPCompressEx(true,
                                       /* fPartialOK = */ false,
                                       pu8,
                                       cbLine,
                                       cTileWidth,
                                       cTileHeight,
                                       cBitsPerPixel,
                                       au8Dst,
                                       sizeof (au8Dst),
                                       &cbData,
                                       &fCompressed,
                                       &u32NewWidth,
                                       &u32NewHeight);

                TESTLOG(("VRDP_COMPRESS_EX: %Rrc %dx%d -> %dx%d compr %d, %d bytes, preallocated %d bytes\n",
                          rc, cTileWidth, cTileHeight, u32NewWidth, u32NewHeight, fCompressed, cbData, sizeof (m_data.au8Static) - m_data.cbStatic));

                /* There is always enough space for a complete tile. */
                Assert(u32NewHeight == cTileHeight);

                if (RT_SUCCESS (rc))
                {
                    /* Copy the data to the preallocated buffer or alloc a buffer for the tile. */
                    if (cbData < sizeof (m_data.au8Static) - m_data.cbStatic)
                    {
                         ptd->pu8Data = &m_data.au8Static[m_data.cbStatic];
                         m_data.cbStatic += cbData;

                         memcpy (ptd->pu8Data, au8Dst, cbData);
                         ptd->cbData = cbData;
                    }
                    else
                    {
                         ptd->pu8Data = (uint8_t *)VRDPMemAlloc (cbData);

                         if (ptd->pu8Data)
                         {
                             memcpy (ptd->pu8Data, au8Dst, cbData);
                             ptd->cbData = cbData;
                         }
                         else
                         {
                             rc = VERR_NO_MEMORY;
                         }
                    }
                }

                if (RT_SUCCESS (rc))
                {
                    TESTLOG(("ptd->pu8Data = %p, ptd->cbData = %d, u32NewWidth = %d\n", ptd->pu8Data, ptd->cbData, u32NewWidth));

                    Assert (cbData <= VRDP_MAX_BMP_TILE_SIZE);

                    Assert (ptd->pu8Data && ptd->cbData);

                    ptd->fCompressed  = fCompressed;
                    ptd->x            = tileX * VRDP_BMP_TILE_SIZE;
                    ptd->y            = tileY * VRDP_BMP_TILE_SIZE;
                    ptd->cTileWidth   = cTileWidth;
                    ptd->cHeight      = (uint16_t)u32NewHeight;
                    ptd->cBitmapWidth = (uint16_t)u32NewWidth;
                    bmpHash (&ptd->tileHash, pu8,
                             cTileWidth,
                             cTileHeight,
                             cbLine,
                             cBitsPerPixel);

                    int cSquare = ptd->cBitmapWidth * ptd->cHeight;

                    if (cSquare <= 0x100)
                    {
                        ptd->u8TileSizeClass = 0;
                    }
                    else if (cSquare <= 0x400)
                    {
                        ptd->u8TileSizeClass = 1;
                    }
                    else if (cSquare <= 0x1000)
                    {
                        ptd->u8TileSizeClass = 2;
                    }
                    else
                    {
                        ptd->u8TileSizeClass = 0xFF;
                        AssertFailed ();
                    }
                }

                if (RT_FAILURE(rc))
                {
                    AssertFailed();
                    break;
                }
            }
        }

        Assert (u32NewWidth != 0);
    }

    if (RT_FAILURE(rc))
    {
        destroyMembers();
        return;
    }

    m_cWidth = cWidth;
    m_cHeight = cHeight;

    BMPSTAT_BITMAP(cWidth, cHeight, m_cbPixel, GetTilesTotal(), CompressedSize());

    return;
}

/* Ensures that the output bitmap size is less than or equal to VRDP_MAX_BMP_NETWORK_SIZE. */
bool VRDPBitmapCompressedRLE::Output (uint16_t xTile,
                                   uint16_t yTile,
                                   bool fCompress,
                                   uint8_t cBitsPerPixel,
                                   uint8_t *pu8Dst,
                                   uint32_t cbDstIn,
                                   const VRDPBitmapCompressedTileDescr **pptd,
                                   uint32_t *pcbDstOut,
                                   bool *pfCompressed)
{
    TESTLOG(("tile %d,%d, fC %d, to bpp %d, cbDstIn = %d\n", xTile, yTile, fCompress, cBitsPerPixel, cbDstIn));

    VRDPBitmapCompressedTileDescr *ptd = tlDescrFromPos (xTile, yTile);

    if (ptd == NULL)
    {
         return false;
    }

    TESTLOG(("ptd: pu8Data = %p, cbData = %d, fC = %d, %dx%d (bmp w %d) bpp %d\n",
              ptd->pu8Data, ptd->cbData, ptd->fCompressed, ptd->cTileWidth, ptd->cHeight, ptd->cBitmapWidth, m_cBitsPerPixel));

    Assert (cbDstIn == VRDP_NETWORK_PACKET_SIZE_ALLOC);

    *pptd = ptd;

    if (ptd->fCompressed)
    {
        if (fCompress)
        {
            /* compressed -> compressed */
            *pfCompressed = true;

            if (cBitsPerPixel == m_cBitsPerPixel)
            {
                /* Raw copy. */
                bool f = tlCopy (pu8Dst, cbDstIn, pcbDstOut, ptd);

                if (f && *pcbDstOut > VRDP_MAX_BMP_NETWORK_SIZE)
                {
                    TESTLOG(("Uncompressed bitmap too big %04X!!!\n", *pcbDstOut));
                    return false;
                }

                return f;
            }

            /* Color depth convertion. */
            const uint8_t *s = ptd->pu8Data;
            const uint8_t *sEnd = ptd->pu8Data + ptd->cbData;
            uint8_t *d = pu8Dst;

            uint16_t n;

            uint8_t cbDstPixel = (cBitsPerPixel + 7) / 8;

            BMPLOG2(("Compressed data:\n%.*Rhxd\n", ptd->cbData, ptd->pu8Data));

            /* Note: 'memcpy' is used to let the compiler to optimize the memory transfer. */
            while (s < sEnd)
            {
                switch (*s)
                {
                    case 0xFE:    /* Black pixel. */
                    case 0xFD:    /* White pixel. */
                        BMPLOG2(("S %02X\n", *s));
                        *d++ = *s++;
                        break;

                    case 0x00:    /* Diff [32..256 + 31] */
                        BMPLOG2(("D 2\n"));
                        memcpy (d, s, 2);
                        d += 2;
                        s += 2;
                        break;

                    case 0xF0:    /* Diff > 256 + 31 */
                        BMPLOG2(("D 3\n"));
                        memcpy (d, s, 3);
                        d += 3;
                        s += 3;
                        break;

                    case 0x60:    /* RLE [32..256 + 31] */
                        BMPLOG2(("R 2\n"));
                        memcpy (d, s, 2);
                        d += 2;
                        s += 2;

                        /* Convert 1 pixel. Pass the 'cbDstPixel' as available destination size, because
                         * the buffer has enough size.
                         */
                        ConvertColors (s, m_cBitsPerPixel, 1, d, cBitsPerPixel, cbDstPixel);
                        s += m_cbPixel;
                        d += cbDstPixel;
                        break;

                    case 0xF3:    /* RLE > 256 + 31 */
                        BMPLOG2(("R 3\n"));
                        memcpy (d, s, 3);
                        d += 3;
                        s += 3;

                        /* Convert 1 pixel. Pass the 'cbDstPixel' as available destination size, because
                         * the buffer has enough size.
                         */
                        ConvertColors (s, m_cBitsPerPixel, 1, d, cBitsPerPixel, cbDstPixel);
                        s += m_cbPixel;
                        d += cbDstPixel;
                        break;

                    case 0x80:    /* Color [32..256 + 31] */
                        BMPLOG2(("C 2\n"));
                        n = s[1] + 32;

                        memcpy (d, s, 2);
                        d += 2;
                        s += 2;

                        /* Convert n pixels. */
                        ConvertColors (s, m_cBitsPerPixel, n, d, cBitsPerPixel, n * cbDstPixel);
                        s += m_cbPixel * n;
                        d += cbDstPixel * n;
                        break;

                    case 0xF4:    /* Color > 256 + 31 */
                        BMPLOG2(("C 3\n"));
                        n = *(uint16_t *)&s[1];

                        memcpy (d, s, 3);
                        d += 3;
                        s += 3;

                        /* Convert n pixels. */
                        ConvertColors (s, m_cBitsPerPixel, n, d, cBitsPerPixel, n * cbDstPixel);
                        s += m_cbPixel * n;
                        d += cbDstPixel * n;
                        break;

                    case 0xE0:    /* Bicolor [32..256 + 15] */
                        BMPLOG2(("B 2\n"));
                        memcpy (d, s, 2);
                        d += 2;
                        s += 2;

                        /* Convert 2 pixels. */
                        ConvertColors (s, m_cBitsPerPixel, 2, d, cBitsPerPixel, 2 * cbDstPixel);
                        s += m_cbPixel * 2;
                        d += cbDstPixel * 2;
                        break;

                    case 0xF8:    /* Bicolor > 256 + 31 */
                        BMPLOG2(("B 3\n"));
                        memcpy (d, s, 3);
                        d += 3;
                        s += 3;

                        /* Convert n pixels. */
                        ConvertColors (s, m_cBitsPerPixel, 2, d, cBitsPerPixel, 2 * cbDstPixel);
                        s += m_cbPixel * 2;
                        d += cbDstPixel * 2;
                        break;

                    default:
                        switch (*s & 0xE0) /* 5 lower bits contain the length. */
                        {
                            case 0x00:    /* Diff <= 31 */
                                BMPLOG2(("D 1\n"));
                                *d++ = *s++;
                                break;

                            case 0x60:    /* RLE <= 31 */
                                BMPLOG2(("R 1\n"));
                                *d++ = *s++;

                                /* Convert 1 pixel. Pass the 'cbDstPixel' as available destination size, because
                                 * the buffer has enough size.
                                 */
                                ConvertColors (s, m_cBitsPerPixel, 1, d, cBitsPerPixel, cbDstPixel);
                                s += m_cbPixel;
                                d += cbDstPixel;

                                break;

                            case 0x80:    /* Color <= 31 */
                                BMPLOG2(("C 1\n"));
                                n = *s & 0x1F;

                                *d++ = *s++;

                                /* Convert n pixels. */
                                ConvertColors (s, m_cBitsPerPixel, n, d, cBitsPerPixel, n * cbDstPixel);
                                s += m_cbPixel * n;
                                d += cbDstPixel * n;

                                break;

                            case 0xE0:    /* Bicolor <= 15 */
                                BMPLOG2(("B 1\n"));
                                Assert ((*s & 0x10) == 0);

                                *d++ = *s++;

                                /* Convert 2 pixels. */
                                ConvertColors (s, m_cBitsPerPixel, 2, d, cBitsPerPixel, 2 * cbDstPixel);
                                s += m_cbPixel * 2;
                                d += cbDstPixel * 2;

                                break;

                            default:
                                AssertFailed ();
                        }
                }
            }

            *pcbDstOut = d - pu8Dst;

            /* It is designed that the dest buffer is big enough. */
            Assert (*pcbDstOut <= cbDstIn);
            Assert (s == sEnd);

            if (*pcbDstOut <= VRDP_MAX_BMP_NETWORK_SIZE)
            {
                return true;
            }

            TESTLOG(("Compressed bitmap too big %04X!!! Will output uncompressed.\n", *pcbDstOut));

            fCompress = false;

            /* Go to the next 'if (!fCompress)' statement. */
        }

        if (!fCompress)
        {
            /* compressed -> uncompressed */
            *pfCompressed = false;

            const uint8_t *s = ptd->pu8Data;
            const uint8_t *sEnd = ptd->pu8Data + ptd->cbData;
            uint8_t *d = pu8Dst;

            uint16_t n;

            uint8_t cbDstPixel = (cBitsPerPixel + 7) / 8;
            uint32_t cbDstLine = cbDstPixel * ptd->cBitmapWidth;

            uint32_t u32PixelSrc;
            uint32_t u32PixelDst;
            uint32_t u32PixelDst2;

            while (s < sEnd)
            {
                switch (*s)
                {
                    case 0xFE:    /* Black pixel. */
                        d = storepixel (d, 0, cBitsPerPixel);
                        s++;
                        break;

                    case 0xFD:    /* White pixel. */
                        d = storepixel (d, UINT32_MAX, cBitsPerPixel);
                        s++;
                        break;

                    case 0x00:    /* Diff [32..256 + 31] */
                        s++;

                        n = s[0] + 32;
                        s++;

                        Assert (d - cbDstLine >= pu8Dst);

                        while (n--)
                        {
                            /* Using pixels from the dest, they were already unpacked. */
                            u32PixelSrc = getpixel (d - cbDstLine, 0, cBitsPerPixel);
                            d = storepixel (d, u32PixelSrc, cBitsPerPixel);
                        }

                        break;

                    case 0xF0:    /* Diff > 256 + 31 */
                        s++;

                        n = s[0] + s[1] * 0x100;
                        s += 2;

                        Assert (d - cbDstLine >= pu8Dst);

                        while (n--)
                        {
                            u32PixelSrc = getpixel (d - cbDstLine, 0, cBitsPerPixel);
                            d = storepixel (d, u32PixelSrc, cBitsPerPixel);
                        }

                        break;

                    case 0x60:    /* RLE [32..256 + 31] */
                        s++;

                        n = s[0] + 32;
                        s++;

                        ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst, cBitsPerPixel, cbDstPixel);
                        s += m_cbPixel;

                        while (n--)
                        {
                            d = storepixel (d, u32PixelDst, cBitsPerPixel);
                        }

                        break;

                    case 0xF3:    /* RLE > 256 + 31 */
                        s++;

                        n = s[0] + s[1] * 0x100;
                        s += 2;

                        ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst, cBitsPerPixel, cbDstPixel);
                        s += m_cbPixel;

                        while (n--)
                        {
                            d = storepixel (d, u32PixelDst, cBitsPerPixel);
                        }

                        break;

                    case 0x80:    /* Color [32..256 + 31] */
                        s++;

                        n = s[0] + 32;
                        s++;

                        ConvertColors (s, m_cBitsPerPixel, n, d, cBitsPerPixel, n * cbDstPixel);
                        s += m_cbPixel * n;
                        d += cbDstPixel * n;

                        break;

                    case 0xF4:    /* Color > 256 + 31 */
                        s++;

                        n = s[0] + s[1] * 0x100;
                        s += 2;

                        ConvertColors (s, m_cBitsPerPixel, n, d, cBitsPerPixel, n * cbDstPixel);
                        s += m_cbPixel * n;
                        d += cbDstPixel * n;

                        break;

                    case 0xE0:    /* Bicolor [16..256 + 15] */
                        s++;

                        n = s[0] + 16;
                        s++;

                        ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst, cBitsPerPixel, cbDstPixel);
                        s += m_cbPixel;

                        ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst2, cBitsPerPixel, cbDstPixel);
                        s += m_cbPixel;

                        while (n--)
                        {
                            d = storepixel (d, u32PixelDst, cBitsPerPixel);
                            d = storepixel (d, u32PixelDst2, cBitsPerPixel);
                        }

                        break;

                    case 0xF8:    /* Bicolor > 256 + 31 */
                        s++;

                        n = s[0] + s[1] * 0x100;
                        s += 2;

                        ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst, cBitsPerPixel, cbDstPixel);
                        s += m_cbPixel;

                        ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst2, cBitsPerPixel, cbDstPixel);
                        s += m_cbPixel;

                        while (n--)
                        {
                            d = storepixel (d, u32PixelDst, cBitsPerPixel);
                            d = storepixel (d, u32PixelDst2, cBitsPerPixel);
                        }

                        break;

                    default:
                        switch (*s & 0xE0) /* 5 lower bits contain the length. */
                        {
                            case 0x00:    /* Diff <= 31 */
                                n = s[0] & 0x1F;
                                s++;

                                Assert (d - cbDstLine >= pu8Dst);

                                while (n--)
                                {
                                    u32PixelSrc = getpixel (d - cbDstLine, 0, cBitsPerPixel);
                                    d = storepixel (d, u32PixelSrc, cBitsPerPixel);
                                }

                                break;

                            case 0x60:    /* RLE <= 31 */
                                n = s[0] & 0x1F;
                                s++;

                                ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst, cBitsPerPixel, cbDstPixel);
                                s += m_cbPixel;

                                while (n--)
                                {
                                    d = storepixel (d, u32PixelDst, cBitsPerPixel);
                                }

                                break;

                            case 0x80:    /* Color <= 31 */
                                n = *s & 0x1F;
                                s++;

                                ConvertColors (s, m_cBitsPerPixel, n, d, cBitsPerPixel, n * cbDstPixel);
                                s += m_cbPixel * n;
                                d += cbDstPixel * n;

                                break;

                            case 0xE0:    /* Bicolor <= 15 */
                                Assert ((*s & 0x10) == 0);

                                /* This is a 4 bit opcode, with length encoded in 4 lower bits. */
                                n = *s & 0x0F;
                                s++;

                                ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst, cBitsPerPixel, cbDstPixel);
                                s += m_cbPixel;

                                ConvertColors (s, m_cBitsPerPixel, 1, (uint8_t *)&u32PixelDst2, cBitsPerPixel, cbDstPixel);
                                s += m_cbPixel;

                                while (n--)
                                {
                                    d = storepixel (d, u32PixelDst, cBitsPerPixel);
                                    d = storepixel (d, u32PixelDst2, cBitsPerPixel);
                                }

                                break;

                            default:
                                AssertFailed ();
                        }
                }
            }

            *pcbDstOut = d - pu8Dst;

            /* It is designed that the dest buffer is big enough. */
            Assert (*pcbDstOut <= cbDstIn);
            Assert (s == sEnd);

            if (*pcbDstOut > VRDP_MAX_BMP_NETWORK_SIZE)
            {
                TESTLOG(("Uncompressed bitmap too big %04X!!!\n", *pcbDstOut));
                return false;
            }

            return true;
        }
    }
    else
    {
        /* uncompressed -> uncompressed */
        *pfCompressed = false;

        if (cBitsPerPixel == m_cBitsPerPixel)
        {
            /* Raw copy. */
            bool f = tlCopy (pu8Dst, cbDstIn, pcbDstOut, ptd);

            if (f && *pcbDstOut > VRDP_MAX_BMP_NETWORK_SIZE)
            {
                TESTLOG(("Uncompressed bitmap too big %04X!!!\n", *pcbDstOut));
                return false;
            }

            return f;
        }
        else
        {
            /* Copy with color convertion. */
            uint32_t cPixels    = ptd->cBitmapWidth * ptd->cHeight;
            uint8_t  cbDstPixel = (cBitsPerPixel + 7) / 8;
            uint32_t cbDst      = cPixels * cbDstPixel;

            TESTLOG(("YYY cPixels %d, cbDstPixel %d cbDst %d\n", cPixels, cbDstPixel, cbDst));

            Assert (cbDst <= cbDstIn);

            ConvertColors (ptd->pu8Data, m_cBitsPerPixel, cPixels, pu8Dst, cBitsPerPixel, cbDst);

            *pcbDstOut = cbDst;

            if (*pcbDstOut > VRDP_MAX_BMP_NETWORK_SIZE)
            {
                TESTLOG(("Uncompressed bitmap too big %04X!!!\n", *pcbDstOut));
                return false;
            }

            return true;
        }
    }

    AssertFailed ();
    return false;
}

/*
 * Compress as many lines of the source bitmap as fit to the destination buffer.
 *
 * If compressed bitmap data size is greater than uncompressed, then return uncompressed data.
 *
 * Return the number of actually processed scan lines in *pu32NewHeight.
 */
static int VRDPBMPCompressEx(bool fCompress,
                             bool fPartialOK,
                             const uint8_t *pu8SrcBits,
                             uint32_t cbSrcLine,
                             uint32_t cSrcWidth,
                             uint32_t cSrcHeight,
                             uint32_t cSrcBitsPerPixel,
                             uint8_t *pu8Dst,
                             const uint32_t cbDst,
                             uint32_t *pcbDstOut,
                             bool *pfCompressed,
                             uint32_t *pu32NewWidth,
                             uint32_t *pu32NewHeight)
{
#ifdef VRDP_BMP_STAT
    uint64_t u64TSCStart = ASMReadTSC();
#endif

    bool fBufferOverflow = false;

    uint8_t *pu8DstStart = pu8Dst;

    int cbDstLeft = cbDst;

    /* Precompute bytes per pixel value. */
    uint32_t cbBytesPerPixel = (cSrcBitsPerPixel + 7) / 8;

    /* Compute new width to be 4 pixels aligned. */
    uint32_t cNewWidth = (cSrcWidth + 3) & ~3;

    BMPLOG2(("%dx%d (%dx...), BPP: %d\n",
             cSrcWidth, cSrcHeight, cNewWidth, cSrcBitsPerPixel));

    PFNGETPIXEL pfnGetPixel = GetPixelFn (cbBytesPerPixel);

    #define GET_PIXEL(_src, _x) bmpcomp_getpixel_ex (_src, _x, pfnGetPixel, cNewWidth, cSrcWidth)

    /* Computes the logically previous scanline pointer. */
    #define PREV_LINE (pu8Src + cbSrcLine)

    #define STORE_BYTE(b) do {                                                     \
        if (cbDstLeft == 0)                                                        \
        {                                                                          \
            fBufferOverflow = true;                                                \
            goto l_EndCompression;                                                 \
        }                                                                          \
        *pu8Dst++ = (b);                                                           \
        cbDstLeft--;                                                               \
    } while (0)

    #define STORE_PIXEL(_pixel, _BPP) do {                                         \
        switch (_BPP)                                                              \
        {                                                                          \
            case 4:                                                                \
                cbDstLeft -= 4;                                                    \
                if (cbDstLeft < 0)                                                 \
                {                                                                  \
                    fBufferOverflow = true;                                        \
                    goto l_EndCompression;                                         \
                }                                                                  \
                *(uint32_t *)pu8Dst = (_pixel);                                    \
                pu8Dst += 4;                                                       \
                break;                                                             \
            case 3:                                                                \
                STORE_BYTE((uint8_t)(_pixel));                                     \
                STORE_BYTE((uint8_t)((_pixel) >> 8));                              \
                STORE_BYTE((uint8_t)((_pixel) >> 16));                             \
                break;                                                             \
            case 2:                                                                \
                cbDstLeft -= 2;                                                    \
                if (cbDstLeft < 0)                                                 \
                {                                                                  \
                    fBufferOverflow = true;                                        \
                    goto l_EndCompression;                                         \
                }                                                                  \
                *(uint16_t *)pu8Dst = (_pixel);                                    \
                pu8Dst += 2;                                                       \
                break;                                                             \
            case 1:                                                                \
                STORE_BYTE((uint8_t)(_pixel));                                     \
                break;                                                             \
            default: AssertFailed ();                                              \
        }                                                                          \
    } while (0)

    #define STORE_SINGLE_PIXEL_OPCODE(_pixel) do {                                 \
        if (_pixel == 0)                                                           \
        {                                                                          \
            /* Special case of black pixel */                                      \
            STORE_BYTE(0xFE);                                                      \
            BMPLOG2(("B %04X\n", _pixel));                                       \
        }                                                                          \
        else                                                                       \
        {                                                                          \
            /* Single pixel */                                                     \
            STORE_BYTE(0x60 | 1);                                                  \
            STORE_PIXEL(_pixel, cbBytesPerPixel);                               \
            BMPLOG2(("S %04X\n", _pixel));                                       \
        }                                                                          \
    } while (0)

    uint16_t x = 0;
    uint16_t y = 0;

    uint8_t *pu8DstLastLine = pu8DstStart;

    if (fCompress)
    {
        /*
         * Scan bitmap lines and apply RLE and DIFF.
         */

        /* Source pointer in the uncompressed bitmap data.
         * According to T.128, we have to send bitmap lines
         * from last to first.
         */
        const uint8_t *pu8SrcScan = pu8SrcBits + cbSrcLine * (cSrcHeight - 1);

        /* Indicates that the last opcode was FILL (i.e. DIFF). */
        bool fFill = false;

        for (y = 0; y < cSrcHeight; y++, pu8SrcScan -= cbSrcLine)
        {
            /* Scan all pixels in the current scanline. */
            const uint8_t *pu8Src = pu8SrcScan;

            /* Remember where the compressed data of current scan line starts. */
            pu8DstLastLine = pu8Dst;

            uint16_t n; /* A run length. */

            x = 0;
            while (x < cNewWidth)
            {
                n = 0;

                /* If this is not the first line, then compare the current line
                 * with the previous line to produce a DIFF opcode.
                 */
                if (y > 0 && !fFill)
                {
                    while (   x < cNewWidth
                           && GET_PIXEL(pu8Src, x) == GET_PIXEL(PREV_LINE, x)
                           && n < 65535)
                    {
                        n++;
                        x++;
                    }

                    if (n > 0)
                    {
                        BMPLOG2(("D %d\n", n));

                        if (n <= 31)
                        {
                            STORE_BYTE((uint8_t)n);
                        }
                        else if (n <= 256 + 31)
                        {
                            STORE_BYTE(0x00);
                            STORE_BYTE((uint8_t)(n - 32));
                        }
                        else /* if (n <= 65535) -- always true */
                        {
                            STORE_BYTE(0xF0);
                            STORE_BYTE((uint8_t)n);
                            STORE_BYTE((uint8_t)(n >> 8));
                        }

                        fFill = true;

                        continue;
                    }
                }

                /* Previous line pixel is different or it's a first line.
                 * Try RLE until we got a match with previous line.
                 */

                /* RLE part. For first line we can use only RLE but for next lines
                 * we will favour DIFF over RLE above.
                 *
                 * We scan the current line starting with current pixel
                 * and count how many equal pixels the line have in a row,
                 * until we encounter a different pixel.
                 *
                 * Also all pixels in a separate scanline those do not fit to either RLE or DIFF,
                 * go to the COLOR buffer.
                 *
                 * 'src' points to the next pixel.
                 */

                /* An opcode will be written that is not a DIFF. */
                fFill = false;

                n = 1; /* how many equal pixels in this scanline. */

                /* Obtain the current pixel value. */
                unsigned pixStart = GET_PIXEL (pu8Src, x);

                BMPLOG2(("pre rle x = %d. pixel = %08X\n", x, pixStart));

                for (;;)
                {
                    x++;

                    if (   x >= cNewWidth
                        || n >= 65535
                        || GET_PIXEL (pu8Src, x) != pixStart)
                    {
                        break;
                    }

                    n++;
                }

                if (n > 1)
                {
                    BMPLOG2(("R %d, %04X\n", n, pixStart));

                    /* There were some equal pixels in a row. */
                    if (n <= 31)
                    {
                        STORE_BYTE((uint8_t)(0x60 | n));
                    }
                    else if (n <= 256 + 31)
                    {
                        STORE_BYTE(0x60);
                        STORE_BYTE((uint8_t)(n - 32));
                    }
                    else /* if (n <= 65535) -- always true */
                    {
                        STORE_BYTE(0xF3);
                        STORE_BYTE((uint8_t)n);
                        STORE_BYTE((uint8_t)(n >> 8));
                    }

                    STORE_PIXEL(pixStart, cbBytesPerPixel);

                    /* Restart loop, may be a DIFF can be applied on the current pixel. */
                    continue;
                }

                /* Here n == 1 and pixStart is the single pixel that did not fit to DIFF/RLE parts
                 * and pu8Src[x] points to next 'pixel'. */
                if (x >= cNewWidth)
                {
                    /* This was the last pixel, encode it as single pixel. */
                    STORE_SINGLE_PIXEL_OPCODE(pixStart);

                    /* Go to the loop completion logic in the begin. */
                    continue;
                }

                /* Back one pixel. So the pixel points to the first pixel that do not fit to RLE and DIFF.
                 * Pixel value is 'pixStart'
                 */
                Assert(x > 0);
                x--;
                Assert (pixStart == GET_PIXEL (pu8Src, x));

                /*
                 * Count how many pixels are in the current scanline which should be encoded as COPY,
                 * that is pixels which do not equal to the pixel of prev scanline and do not equal to
                 * previous pixel in the same scanline.
                 *
                 * The loop stops:
                 *      at the end of scanline;
                 *   or if the next pixel is equal to the previous pixel;
                 *   or if the pixel is equal to corresponding pixel in the previous scanline.
                 *
                 */

                /* Remember the starting point of color pixels. */
                const uint8_t *pu8ColorStart = pu8Src;
                unsigned xColorStart = x;
                unsigned pixColorStart = pixStart;

                BMPLOG2(("color: starting at x = %d.\n", x));

                /* How many color pixels. */
                n = 1;

                for (;;)
                {
                    x++;

                    if (x >= cNewWidth)
                    {
                        /* End of scanline. */
                        BMPLOG2(("color: end of scanline.\n"));
                        break;
                    }

                    unsigned pixCurrent = GET_PIXEL (pu8Src, x);

                    if (y > 0 && pixCurrent == GET_PIXEL (PREV_LINE, x))
                    {
                        /* DIFF can be applied starting from the pixel. */
                        BMPLOG2(("color: diff from x = %d, pixCurrent %x, pixStart %x.\n", x, pixCurrent, pixStart));
                        break;
                    }

                    if (pixCurrent == pixStart)
                    {
                        /* RLE can be applied starting from previous pixel. */
                        x--;
                        n--;
                        BMPLOG2(("color: rle from x = %d.\n", x));
                        break;
                    }

                    pixStart = pixCurrent;

                    n++;
                }

                BMPLOG2(("color: %d pixels.\n", n));
                if (n == 0)
                {
                    continue;
                }

                if (n == 1)
                {
                    /* It was the last pixel in the scanline. Encode it as a single pixel. */
                    STORE_SINGLE_PIXEL_OPCODE(pixColorStart);

                    /* Restart the main loop to either apply RLE/DIFF or with a next scanline. */
                    continue;
                }

                /* Here there is a sequence of pixels which do not fit to either RLE or DIFF.
                 * Encode them as a COPY opcode.
                 */
                unsigned offset = 0;

                if (n >= 4)
                {
                    /* It is worth to look for BI-COLOR encoding possibility. */

                    BMPLOG2(("BI: %d colors\n", n));

                    unsigned cColorsSingle = 0;
                    unsigned xColorsSingle = offset;

                    unsigned cColorsDouble = 0;

                    unsigned cColorsTotal = n;

                    unsigned pixels[4];
                    int pixelIndex = 0;

                    pixels[0] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                    BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[0]));
                    pixels[1] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                    BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[1]));
                    pixels[2] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                    BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[2]));
                    pixels[3] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                    BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[3]));

                    n -= 4;

                    for (;;)
                    {
                        BMPLOG2(("BI: 1) offset = %d, cColorsSingle = %d, xColorsSingle = %d, cColorsDouble = %d, cColors = %d\n", offset, cColorsSingle, xColorsSingle, cColorsDouble, n));

                        if (   pixels[pixelIndex] == pixels[(pixelIndex + 2) % 4]
                            && pixels[(pixelIndex + 1) % 4] == pixels[(pixelIndex + 3) % 4])
                        {
                            cColorsDouble++;

                            BMPLOG2(("BI: double color cColorsDouble = %d\n", cColorsDouble));

                            if (n >= 2)
                            {
                                pixels[(pixelIndex + 2) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                                BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 2) % 4]));
                                n--;
                                pixels[(pixelIndex + 3) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                                BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 3) % 4]));
                                n--;
                                continue;
                            }
                        }

                        if (cColorsDouble)
                        {
                            cColorsDouble++;
                        }

                        BMPLOG2(("BI: 2) offset = %d, cColorsSingle = %d, xColorsSingle = %d, cColorsDouble = %d, cColors = %d\n", offset, cColorsSingle, xColorsSingle, cColorsDouble, n));

                        if (cColorsSingle && cColorsDouble)
                        {
                            BMPLOG2(("C %d\n", cColorsSingle));

                            if (cColorsSingle <= 31)
                            {
                                STORE_BYTE((uint8_t)(0x80 | cColorsSingle));
                            }
                            else if (cColorsSingle <= 256 + 31)
                            {
                                STORE_BYTE(0x80);
                                STORE_BYTE((uint8_t)(cColorsSingle - 32));
                            }
                            else if (cColorsSingle <= 65535)
                            {
                                STORE_BYTE(0xF4);
                                STORE_BYTE((uint8_t)cColorsSingle);
                                STORE_BYTE((uint8_t)(cColorsSingle >> 8));
                            }
                            else
                            {
                                AssertFailed ();
                            }

                            while (cColorsSingle--)
                            {
                                unsigned pixel = GET_PIXEL (pu8ColorStart, xColorStart + xColorsSingle++);
                                STORE_PIXEL(pixel, cbBytesPerPixel);
                            }

                            cColorsSingle = 0;
                        }

                        BMPLOG2(("BI: 3) offset = %d, cColorsSingle = %d, xColorsSingle = %d, cColorsDouble = %d, cColors = %d\n", offset, cColorsSingle, xColorsSingle, cColorsDouble, n));

                        if (cColorsDouble)
                        {
                            xColorsSingle += cColorsDouble * 2;

                            BMPLOG2(("I %d\n", cColorsDouble));

                            /* BICOLOR is a 4 bit opcode, so 15 is the maximum length for a single byte encoding. */
                            if (cColorsDouble <= 15)
                            {
                                STORE_BYTE((uint8_t)(0xE0 | cColorsDouble));
                            }
                            else if (cColorsDouble <= 256 + 15)
                            {
                                STORE_BYTE(0xE0);
                                STORE_BYTE((uint8_t)(cColorsDouble - 16));
                            }
                            else if (cColorsDouble <= 65535)
                            {
                                STORE_BYTE(0xF8);
                                STORE_BYTE((uint8_t)cColorsDouble);
                                STORE_BYTE((uint8_t)(cColorsDouble >> 8));
                            }
                            else
                            {
                                AssertFailed ();
                            }

                            unsigned pixel = pixels[pixelIndex];
                            STORE_PIXEL(pixel, cbBytesPerPixel);

                            pixel = pixels[(pixelIndex + 1) % 4];
                            STORE_PIXEL(pixel, cbBytesPerPixel);

                            cColorsDouble = 0;

                            if (n >= 2)
                            {
                                pixelIndex = (pixelIndex + 2) % 4;
                                pixels[(pixelIndex + 2) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                                BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 2) % 4]));
                                n--;
                                pixels[(pixelIndex + 3) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                                BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 3) % 4]));
                                n--;
                                continue;
                            }
                            else
                            {
                                offset = xColorsSingle;
                                n = cColorsTotal - xColorsSingle;
                                break;
                            }
                        }

                        BMPLOG2(("BI: 4) offset = %d, cColorsSingle = %d, xColorsSingle = %d, cColorsDouble = %d, cColors = %d\n", offset, cColorsSingle, xColorsSingle, cColorsDouble, n));

                        if (n == 0)
                        {
                            offset = xColorsSingle;
                            n = cColorsTotal - xColorsSingle;
                            break;
                        }

                        cColorsSingle++;
                        pixelIndex = (pixelIndex + 1) % 4;
                        pixels[(pixelIndex + 3) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                        BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 3) % 4]));
                        n--;
                    }
                }

                BMPLOG2(("BI: 5) offset = %d, cColors = %d\n", offset, n));

                if (n > 0)
                {
                    BMPLOG2(("C %d\n", n));

                    if (n <= 31)
                    {
                        STORE_BYTE((uint8_t)(0x80 | n));
                    }
                    else if (n <= 256 + 31)
                    {
                        STORE_BYTE(0x80);
                        STORE_BYTE((uint8_t)(n - 32));
                    }
                    else /* if (n <= 65535) -- always true */
                    {
                        STORE_BYTE(0xF4);
                        STORE_BYTE((uint8_t)n);
                        STORE_BYTE((uint8_t)(n >> 8));
                    }

                    while (n--)
                    {
                        unsigned pixel = GET_PIXEL (pu8ColorStart, xColorStart + offset);
                        STORE_PIXEL(pixel, cbBytesPerPixel);
                        offset++;
                    }
                }
            }
        }

        /* Check if compressed size is smaller than uncompressed. */
        BMPLOG2(("Compress bitmap 2: %d/%d\n", pu8Dst - pu8DstStart, cNewWidth * y * cbBytesPerPixel));
    }

l_EndCompression:

    uint32_t u32NewHeight = 0;

    if (fBufferOverflow)
    {
        BMPLOG(("Buffer overflow y = %d\n", y));

        /* The current scanline did not fit into the destination buffer. */
        if (y == 0)
        {
            /* Even the first scanline did not fit. This should not happen with the current code. */
            AssertFailed();
            return VERR_BUFFER_OVERFLOW;
        }

        /* Check if the caller wants a complete bitmap. */
        if (fPartialOK)
        {
            /* Still can return some compressed lines. */
            u32NewHeight = y;
            pu8Dst = pu8DstLastLine;
        }
        else
        {
            /* Try to return uncompressed data, because compressed does not fit to the buffer. */
            u32NewHeight = cSrcHeight;
            fCompress = false;
        }
    }
    else
    {
        u32NewHeight = cSrcHeight;
    }

    /* Check if uncompressed data must be written, also when uncompressed data is smaller than compressed. */
    unsigned cbUncompressed = u32NewHeight * cNewWidth * cbBytesPerPixel;

    if (!fCompress || (unsigned)(pu8Dst - pu8DstStart) >= cbUncompressed)
    {
        TESTLOG(("fCompress = %d, pu8Dst - pu8DstStart = %d, cbUncompressed = %d\n",
                  fCompress, pu8Dst - pu8DstStart, cbUncompressed));

        if (cbUncompressed > cbDst)
        {
            BMPLOG(("Buffer overflow uncompressed: size %d, buffer size %d.\n", cbUncompressed, cbDst));
            return VERR_BUFFER_OVERFLOW;
        }

        /* Write uncompressed data. */
        const uint8_t *pu8Src = pu8SrcBits + cbSrcLine * (cSrcHeight - 1);
        pu8Dst = pu8DstStart;

        for (y = 0; y < u32NewHeight; y++, pu8Src -= cbSrcLine)
        {
            memcpy (pu8Dst, pu8Src, cbBytesPerPixel * cSrcWidth);
            pu8Dst += cbBytesPerPixel * cSrcWidth;

            if (cSrcWidth != cNewWidth)
            {
                /* Append zeroes. */
                memset (pu8Dst, 0, (cNewWidth - cSrcWidth) * cbBytesPerPixel);
                pu8Dst += (cNewWidth - cSrcWidth) * cbBytesPerPixel;
            }
        }

        *pfCompressed = false;
    }
    else
    {
        *pfCompressed = true;
    }

    *pu32NewWidth = cNewWidth;
    *pu32NewHeight = u32NewHeight;

    *pcbDstOut = pu8Dst - pu8DstStart;

#ifdef VRDP_BMP_STAT
    BMPSTAT_TICKS(ASMReadTSC() - u64TSCStart);
#endif
    return VINF_SUCCESS;

#undef GET_PIXEL
#undef PREV_LINE
#undef STORE_BYTE
#undef STORE_PIXEL
#undef STORE_SINGLE_PIXEL_OPCODE
}

DECLINLINE(uint32_t) bmpcomp_getpixel32(const uint8_t *pu8Src, uint16_t x, uint32_t cSrcWidth)
{
    if (x < cSrcWidth)
    {
        return *(uint32_t *)(pu8Src + x * 4);
    }

    return *(uint32_t *)(pu8Src + (cSrcWidth - 1) * 4);
}

static int VRDPBMPCompressEx32(bool fCompress,
                               bool fPartialOK,
                               const uint8_t *pu8SrcBits,
                               uint32_t cbSrcLine,
                               uint32_t cSrcWidth,
                               uint32_t cSrcHeight,
                               uint8_t *pu8Dst,
                               const uint32_t cbDst,
                               uint32_t *pcbDstOut,
                               bool *pfCompressed,
                               uint32_t *pu32NewWidth,
                               uint32_t *pu32NewHeight)
{
#ifdef VRDP_BMP_STAT
    uint64_t u64TSCStart = ASMReadTSC();
#endif

    bool fBufferOverflow = false;

    uint8_t *pu8DstStart = pu8Dst;

    int cbDstLeft = cbDst;

    /* Compute new width to be 4 pixels aligned. */
    uint32_t cNewWidth = (cSrcWidth + 3) & ~3;

    BMPLOG2(("%dx%d (%dx...), BPP32\n",
             cSrcWidth, cSrcHeight, cNewWidth));

    #define GET_PIXEL(_src, _x) bmpcomp_getpixel32(_src, _x, cSrcWidth)

    /* Computes the logically previous scanline pointer. */
    #define PREV_LINE (pu8Src + cbSrcLine)

    #define STORE_BYTE(b) do {                                                     \
        if (cbDstLeft == 0)                                                        \
        {                                                                          \
            fBufferOverflow = true;                                                \
            goto l_EndCompression;                                                 \
        }                                                                          \
        *pu8Dst++ = (b);                                                           \
        cbDstLeft--;                                                               \
    } while (0)

    #define STORE_PIXEL(_pixel) do {                                               \
                cbDstLeft -= 4;                                                    \
                if (cbDstLeft < 0)                                                 \
                {                                                                  \
                    fBufferOverflow = true;                                        \
                    goto l_EndCompression;                                         \
                }                                                                  \
                *(uint32_t *)pu8Dst = (_pixel);                                    \
                pu8Dst += 4;                                                       \
    } while (0)

    #define STORE_SINGLE_PIXEL_OPCODE(_pixel) do {                                 \
        if (_pixel == 0)                                                           \
        {                                                                          \
            /* Special case of black pixel */                                      \
            STORE_BYTE(0xFE);                                                      \
            BMPLOG2(("B %04X\n", _pixel));                                         \
        }                                                                          \
        else                                                                       \
        {                                                                          \
            /* Single pixel */                                                     \
            STORE_BYTE(0x60 | 1);                                                  \
            STORE_PIXEL(_pixel);                                                   \
            BMPLOG2(("S %04X\n", _pixel));                                         \
        }                                                                          \
    } while (0)

    uint16_t x = 0;
    uint16_t y = 0;

    uint8_t *pu8DstLastLine = pu8DstStart;

    if (fCompress)
    {
        /*
         * Scan bitmap lines and apply RLE and DIFF.
         */

        /* Source pointer in the uncompressed bitmap data.
         * According to T.128, we have to send bitmap lines
         * from last to first.
         */
        const uint8_t *pu8SrcScan = pu8SrcBits + cbSrcLine * (cSrcHeight - 1);

        /* Indicates that the last opcode was FILL (i.e. DIFF). */
        bool fFill = false;

        for (y = 0; y < cSrcHeight; y++, pu8SrcScan -= cbSrcLine)
        {
            /* Scan all pixels in the current scanline. */
            const uint8_t *pu8Src = pu8SrcScan;

            /* Remember where the compressed data of current scan line starts. */
            pu8DstLastLine = pu8Dst;

            uint16_t n; /* A run length. */

            x = 0;
            while (x < cNewWidth)
            {
                n = 0;

                /* If this is not the first line, then compare the current line
                 * with the previous line to produce a DIFF opcode.
                 */
                if (y > 0 && !fFill)
                {
                    while (   x < cNewWidth
                           && GET_PIXEL(pu8Src, x) == GET_PIXEL(PREV_LINE, x)
                           && n < 65535)
                    {
                        n++;
                        x++;
                    }

                    if (n > 0)
                    {
                        BMPLOG2(("D %d\n", n));

                        if (n <= 31)
                        {
                            STORE_BYTE((uint8_t)n);
                        }
                        else if (n <= 256 + 31)
                        {
                            STORE_BYTE(0x00);
                            STORE_BYTE((uint8_t)(n - 32));
                        }
                        else /* if (n <= 65535) -- always true */
                        {
                            STORE_BYTE(0xF0);
                            STORE_BYTE((uint8_t)n);
                            STORE_BYTE((uint8_t)(n >> 8));
                        }

                        fFill = true;

                        continue;
                    }
                }

                /* Previous line pixel is different or it's a first line.
                 * Try RLE until we got a match with previous line.
                 */

                /* RLE part. For first line we can use only RLE but for next lines
                 * we will favour DIFF over RLE above.
                 *
                 * We scan the current line starting with current pixel
                 * and count how many equal pixels the line have in a row,
                 * until we encounter a different pixel.
                 *
                 * Also all pixels in a separate scanline those do not fit to either RLE or DIFF,
                 * go to the COLOR buffer.
                 *
                 * 'src' points to the next pixel.
                 */

                /* An opcode will be written that is not a DIFF. */
                fFill = false;

                n = 1; /* how many equal pixels in this scanline. */

                /* Obtain the current pixel value. */
                unsigned pixStart = GET_PIXEL (pu8Src, x);

                BMPLOG2(("pre rle x = %d. pixel = %08X\n", x, pixStart));

                for (;;)
                {
                    x++;

                    if (   x >= cNewWidth
                        || n >= 65535
                        || GET_PIXEL (pu8Src, x) != pixStart)
                    {
                        break;
                    }

                    n++;
                }

                if (n > 1)
                {
                    BMPLOG2(("R %d, %04X\n", n, pixStart));

                    /* There were some equal pixels in a row. */
                    if (n <= 31)
                    {
                        STORE_BYTE((uint8_t)(0x60 | n));
                    }
                    else if (n <= 256 + 31)
                    {
                        STORE_BYTE(0x60);
                        STORE_BYTE((uint8_t)(n - 32));
                    }
                    else /* if (n <= 65535) -- always true */
                    {
                        STORE_BYTE(0xF3);
                        STORE_BYTE((uint8_t)n);
                        STORE_BYTE((uint8_t)(n >> 8));
                    }

                    STORE_PIXEL(pixStart);

                    /* Restart loop, may be a DIFF can be applied on the current pixel. */
                    continue;
                }

                /* Here n == 1 and pixStart is the single pixel that did not fit to DIFF/RLE parts
                 * and pu8Src[x] points to next 'pixel'. */
                if (x >= cNewWidth)
                {
                    /* This was the last pixel, encode it as single pixel. */
                    STORE_SINGLE_PIXEL_OPCODE(pixStart);

                    /* Go to the loop completion logic in the begin. */
                    continue;
                }

                /* Back one pixel. So the pixel points to the first pixel that do not fit to RLE and DIFF.
                 * Pixel value is 'pixStart'
                 */
                Assert(x > 0);
                x--;
                Assert (pixStart == GET_PIXEL (pu8Src, x));

                /*
                 * Count how many pixels are in the current scanline which should be encoded as COPY,
                 * that is pixels which do not equal to the pixel of prev scanline and do not equal to
                 * previous pixel in the same scanline.
                 *
                 * The loop stops:
                 *      at the end of scanline;
                 *   or if the next pixel is equal to the previous pixel;
                 *   or if the pixel is equal to corresponding pixel in the previous scanline.
                 *
                 */

                /* Remember the starting point of color pixels. */
                const uint8_t *pu8ColorStart = pu8Src;
                unsigned xColorStart = x;
                unsigned pixColorStart = pixStart;

                BMPLOG2(("color: starting at x = %d.\n", x));

                /* How many color pixels. */
                n = 1;

                for (;;)
                {
                    x++;

                    if (x >= cNewWidth)
                    {
                        /* End of scanline. */
                        BMPLOG2(("color: end of scanline.\n"));
                        break;
                    }

                    unsigned pixCurrent = GET_PIXEL (pu8Src, x);

                    if (y > 0 && pixCurrent == GET_PIXEL (PREV_LINE, x))
                    {
                        /* DIFF can be applied starting from the pixel. */
                        BMPLOG2(("color: diff from x = %d, pixCurrent %x, pixStart %x.\n", x, pixCurrent, pixStart));
                        break;
                    }

                    if (pixCurrent == pixStart)
                    {
                        /* RLE can be applied starting from previous pixel. */
                        x--;
                        n--;
                        BMPLOG2(("color: rle from x = %d.\n", x));
                        break;
                    }

                    pixStart = pixCurrent;

                    n++;
                }

                BMPLOG2(("color: %d pixels.\n", n));
                if (n == 0)
                {
                    continue;
                }

                if (n == 1)
                {
                    /* It was the last pixel in the scanline. Encode it as a single pixel. */
                    STORE_SINGLE_PIXEL_OPCODE(pixColorStart);

                    /* Restart the main loop to either apply RLE/DIFF or with a next scanline. */
                    continue;
                }

                /* Here there is a sequence of pixels which do not fit to either RLE or DIFF.
                 * Encode them as a COPY opcode.
                 */
                unsigned offset = 0;

                if (n >= 4)
                {
                    /* It is worth to look for BI-COLOR encoding possibility. */

                    BMPLOG2(("BI: %d colors\n", n));

                    unsigned cColorsSingle = 0;
                    unsigned xColorsSingle = offset;

                    unsigned cColorsDouble = 0;

                    unsigned cColorsTotal = n;

                    unsigned pixels[4];
                    int pixelIndex = 0;

                    pixels[0] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                    BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[0]));
                    pixels[1] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                    BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[1]));
                    pixels[2] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                    BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[2]));
                    pixels[3] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                    BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[3]));

                    n -= 4;

                    for (;;)
                    {
                        BMPLOG2(("BI: 1) offset = %d, cColorsSingle = %d, xColorsSingle = %d, cColorsDouble = %d, cColors = %d\n", offset, cColorsSingle, xColorsSingle, cColorsDouble, n));

                        if (   pixels[pixelIndex] == pixels[(pixelIndex + 2) % 4]
                            && pixels[(pixelIndex + 1) % 4] == pixels[(pixelIndex + 3) % 4])
                        {
                            cColorsDouble++;

                            BMPLOG2(("BI: double color cColorsDouble = %d\n", cColorsDouble));

                            if (n >= 2)
                            {
                                pixels[(pixelIndex + 2) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                                BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 2) % 4]));
                                n--;
                                pixels[(pixelIndex + 3) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                                BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 3) % 4]));
                                n--;
                                continue;
                            }
                        }

                        if (cColorsDouble)
                        {
                            cColorsDouble++;
                        }

                        BMPLOG2(("BI: 2) offset = %d, cColorsSingle = %d, xColorsSingle = %d, cColorsDouble = %d, cColors = %d\n", offset, cColorsSingle, xColorsSingle, cColorsDouble, n));

                        if (cColorsSingle && cColorsDouble)
                        {
                            BMPLOG2(("C %d\n", cColorsSingle));

                            if (cColorsSingle <= 31)
                            {
                                STORE_BYTE((uint8_t)(0x80 | cColorsSingle));
                            }
                            else if (cColorsSingle <= 256 + 31)
                            {
                                STORE_BYTE(0x80);
                                STORE_BYTE((uint8_t)(cColorsSingle - 32));
                            }
                            else if (cColorsSingle <= 65535)
                            {
                                STORE_BYTE(0xF4);
                                STORE_BYTE((uint8_t)cColorsSingle);
                                STORE_BYTE((uint8_t)(cColorsSingle >> 8));
                            }
                            else
                            {
                                AssertFailed ();
                            }

                            while (cColorsSingle--)
                            {
                                unsigned pixel = GET_PIXEL (pu8ColorStart, xColorStart + xColorsSingle++);
                                STORE_PIXEL(pixel);
                            }

                            cColorsSingle = 0;
                        }

                        BMPLOG2(("BI: 3) offset = %d, cColorsSingle = %d, xColorsSingle = %d, cColorsDouble = %d, cColors = %d\n", offset, cColorsSingle, xColorsSingle, cColorsDouble, n));

                        if (cColorsDouble)
                        {
                            xColorsSingle += cColorsDouble * 2;

                            BMPLOG2(("I %d\n", cColorsDouble));

                            /* BICOLOR is a 4 bit opcode, so 15 is the maximum length for a single byte encoding. */
                            if (cColorsDouble <= 15)
                            {
                                STORE_BYTE((uint8_t)(0xE0 | cColorsDouble));
                            }
                            else if (cColorsDouble <= 256 + 15)
                            {
                                STORE_BYTE(0xE0);
                                STORE_BYTE((uint8_t)(cColorsDouble - 16));
                            }
                            else if (cColorsDouble <= 65535)
                            {
                                STORE_BYTE(0xF8);
                                STORE_BYTE((uint8_t)cColorsDouble);
                                STORE_BYTE((uint8_t)(cColorsDouble >> 8));
                            }
                            else
                            {
                                AssertFailed ();
                            }

                            unsigned pixel = pixels[pixelIndex];
                            STORE_PIXEL(pixel);

                            pixel = pixels[(pixelIndex + 1) % 4];
                            STORE_PIXEL(pixel);

                            cColorsDouble = 0;

                            if (n >= 2)
                            {
                                pixelIndex = (pixelIndex + 2) % 4;
                                pixels[(pixelIndex + 2) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                                BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 2) % 4]));
                                n--;
                                pixels[(pixelIndex + 3) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                                BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 3) % 4]));
                                n--;
                                continue;
                            }
                            else
                            {
                                offset = xColorsSingle;
                                n = cColorsTotal - xColorsSingle;
                                break;
                            }
                        }

                        BMPLOG2(("BI: 4) offset = %d, cColorsSingle = %d, xColorsSingle = %d, cColorsDouble = %d, cColors = %d\n", offset, cColorsSingle, xColorsSingle, cColorsDouble, n));

                        if (n == 0)
                        {
                            offset = xColorsSingle;
                            n = cColorsTotal - xColorsSingle;
                            break;
                        }

                        cColorsSingle++;
                        pixelIndex = (pixelIndex + 1) % 4;
                        pixels[(pixelIndex + 3) % 4] = GET_PIXEL (pu8ColorStart, xColorStart + offset++);
                        BMPLOG2(("BI: pixel %d: %X\n", offset - 1, pixels[(pixelIndex + 3) % 4]));
                        n--;
                    }
                }

                BMPLOG2(("BI: 5) offset = %d, cColors = %d\n", offset, n));

                if (n > 0)
                {
                    BMPLOG2(("C %d\n", n));

                    if (n <= 31)
                    {
                        STORE_BYTE((uint8_t)(0x80 | n));
                    }
                    else if (n <= 256 + 31)
                    {
                        STORE_BYTE(0x80);
                        STORE_BYTE((uint8_t)(n - 32));
                    }
                    else /* if (n <= 65535) -- always true */
                    {
                        STORE_BYTE(0xF4);
                        STORE_BYTE((uint8_t)n);
                        STORE_BYTE((uint8_t)(n >> 8));
                    }

                    while (n--)
                    {
                        unsigned pixel = GET_PIXEL (pu8ColorStart, xColorStart + offset);
                        STORE_PIXEL(pixel);
                        offset++;
                    }
                }
            }
        }

        /* Check if compressed size is smaller than uncompressed. */
        BMPLOG2(("Compress bitmap 2: %d/%d\n", pu8Dst - pu8DstStart, cNewWidth * y * 4));
    }

l_EndCompression:

    uint32_t u32NewHeight = 0;

    if (fBufferOverflow)
    {
        BMPLOG(("Buffer overflow y = %d\n", y));

        /* The current scanline did not fit into the destination buffer. */
        if (y == 0)
        {
            /* Even the first scanline did not fit. This should not happen with the current code. */
            AssertFailed();
            return VERR_BUFFER_OVERFLOW;
        }

        /* Check if the caller wants a complete bitmap. */
        if (fPartialOK)
        {
            /* Still can return some compressed lines. */
            u32NewHeight = y;
            pu8Dst = pu8DstLastLine;
        }
        else
        {
            /* Try to return uncompressed data, because compressed does not fit to the buffer. */
            u32NewHeight = cSrcHeight;
            fCompress = false;
        }
    }
    else
    {
        u32NewHeight = cSrcHeight;
    }

    /* Check if uncompressed data must be written, also when uncompressed data is smaller than compressed. */
    unsigned cbUncompressed = u32NewHeight * cNewWidth * 4;

    if (!fCompress || (unsigned)(pu8Dst - pu8DstStart) >= cbUncompressed)
    {
        TESTLOG(("fCompress = %d, pu8Dst - pu8DstStart = %d, cbUncompressed = %d\n",
                  fCompress, pu8Dst - pu8DstStart, cbUncompressed));

        if (cbUncompressed > cbDst)
        {
            BMPLOG(("Buffer overflow uncompressed: size %d, buffer size %d.\n", cbUncompressed, cbDst));
            return VERR_BUFFER_OVERFLOW;
        }

        /* Write uncompressed data. */
        const uint8_t *pu8Src = pu8SrcBits + cbSrcLine * (cSrcHeight - 1);
        pu8Dst = pu8DstStart;

        for (y = 0; y < u32NewHeight; y++, pu8Src -= cbSrcLine)
        {
            memcpy (pu8Dst, pu8Src, 4 * cSrcWidth);
            pu8Dst += 4 * cSrcWidth;

            if (cSrcWidth != cNewWidth)
            {
                /* Append zeroes. */
                memset (pu8Dst, 0, (cNewWidth - cSrcWidth) * 4);
                pu8Dst += (cNewWidth - cSrcWidth) * 4;
            }
        }

        *pfCompressed = false;
    }
    else
    {
        *pfCompressed = true;
    }

    *pu32NewWidth = cNewWidth;
    *pu32NewHeight = u32NewHeight;

    *pcbDstOut = pu8Dst - pu8DstStart;

#ifdef VRDP_BMP_STAT
    BMPSTAT_TICKS(ASMReadTSC() - u64TSCStart);
#endif
    return VINF_SUCCESS;

#undef GET_PIXEL
#undef PREV_LINE
#undef STORE_BYTE
#undef STORE_PIXEL
#undef STORE_SINGLE_PIXEL_OPCODE
}
