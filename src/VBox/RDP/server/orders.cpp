/* $Id: orders.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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
#include "regions.h"

#include "bmpcomp.h"
#include "shadowbuffer.h"

#define VRDPSTREAM (*pStream)

/* The largest size of order bounds: uint8_t present and four 16 bits coords. */
#define VRDP_PRIMARY_ORDER_BOUNDS_SIZE (9)

/* The largest size of the primary order header. Consists of:
 *     uint8_t flags + uint8_t type + up to 3 bytes of present.
 */
#define VRDP_PRIMARY_ORDER_HDR_SIZE (5 + VRDP_PRIMARY_ORDER_BOUNDS_SIZE)

/* The largest size of the secondary order header. Consists of:
 *     uint8_t flags + uint16_t length + uint16_t secflags + uint8_t type.
 */
#define VRDP_SECONDARY_ORDER_HDR_SIZE (6)

/* The largest size of BMPCACHE2 order:
 * uint8_t width + uint8_t height + 4 bytes field bufsize + 2 bytes field cache idx.
 */
#define VRDP_SECONDARY_ORDER_BMPCACHE2_SIZE (8)

/* Some BMPCACHE2 constants. */
#define BMPCACHE2_SQUARE      0x0080
#define BMPCACHE2_FLAG        0x0400
#define BMPCACHE2_MODE_SHIFT  3


union OrderPresent
{
    uint8_t  *pu8;
    uint16_t *pu16;
    uint32_t *pu32;
};


static void voAdjustPoint (VRDPTP *pvrdptp, unsigned uScreenId, VRDEORDERPOINT *pPt, int16_t i16x, int16_t i16y)
{
    int x = i16x;
    int y = i16y;

    shadowBufferTransformPoint (uScreenId, &x, &y);

    i16x = x;
    i16y = y;

    pvrdptp->DesktopMap()->Screen2ClientPoint (uScreenId, &i16x, &i16y);

    pPt->x = i16x;
    pPt->y = i16y;
}

static bool voAdjustArea (unsigned uScreenId, VRDPTP *pvrdptp, VRDEORDERAREA *pArea, int16_t i16x, int16_t i16y, uint16_t u16w, uint16_t u16h)
{
    RGNRECT rect;

    rect.x = i16x;
    rect.y = i16y;
    rect.w = u16w;
    rect.h = u16h;

    shadowBufferTransformRect (uScreenId, &rect);

    if (!pvrdptp->DesktopMap()->ClipScreen2Client (uScreenId, &rect, &rect))
    {
        return false;
    }

    pArea->x = (int16_t)rect.x;
    pArea->y = (int16_t)rect.y;
    pArea->w = (uint16_t)rect.w;
    pArea->h = (uint16_t)rect.h;

    return true;
}

static bool voEncodeCoordDelta (uint8_t **ppu8Dst, int16_t coord, int16_t coordPrev,
                                uint8_t *pu8Present, uint8_t u8PresentBit)
{
    bool fDelta = true;

    int16_t i16Delta = coord - coordPrev;

    if (i16Delta != 0)
    {
        if (-128 <= i16Delta && i16Delta <= 127)
        {
            uint8_t *pu8Dst = *ppu8Dst;

            *pu8Dst++ = (int8_t)i16Delta;

            *ppu8Dst = pu8Dst;
        }
        else
        {
            fDelta = false;
        }

        *pu8Present |= u8PresentBit;
    }

    return fDelta;
}

static bool voEncodeOrderArea (bool fDelta, const VRDEORDERAREA *pArea, uint8_t *pu8OrderFlags,
                               VRDEORDERAREA *pPrevArea, uint8_t **ppu8DstStart,
                               uint8_t *pu8Present, uint8_t u8PresentBit)
{
    /* Encode coordinates. */
    uint8_t *pu8Dst = *ppu8DstStart;

    fDelta = voEncodeCoordDelta (&pu8Dst, pArea->x, pPrevArea->x, pu8Present, u8PresentBit) && fDelta;
    u8PresentBit <<= 1;

    fDelta = voEncodeCoordDelta (&pu8Dst, pArea->y, pPrevArea->y, pu8Present, u8PresentBit) && fDelta;
    u8PresentBit <<= 1;

    fDelta = voEncodeCoordDelta (&pu8Dst, pArea->w, pPrevArea->w, pu8Present, u8PresentBit) && fDelta;
    u8PresentBit <<= 1;

    fDelta = voEncodeCoordDelta (&pu8Dst, pArea->h, pPrevArea->h, pu8Present, u8PresentBit) && fDelta;
    u8PresentBit <<= 1;

    if (fDelta)
    {
        *pu8OrderFlags |= RDP_ORDER_DELTA;
        *ppu8DstStart = pu8Dst;
    }
    else
    {
        uint16_t *pu16Dst = (uint16_t *)*ppu8DstStart;

        u8PresentBit >>= 4;

        uint8_t u8Present = *pu8Present;

        if (u8Present & u8PresentBit)
        {
            *pu16Dst++ = pArea->x;
        }
        u8PresentBit <<= 1;

        if (u8Present & u8PresentBit)
        {
            *pu16Dst++ = pArea->y;
        }
        u8PresentBit <<= 1;

        if (u8Present & u8PresentBit)
        {
            *pu16Dst++ = pArea->w;
        }
        u8PresentBit <<= 1;

        if (u8Present & u8PresentBit)
        {
            *pu16Dst++ = pArea->h;
        }
        u8PresentBit <<= 1;

        *ppu8DstStart = (uint8_t *)pu16Dst;
    }

    *pPrevArea = *pArea;

    return fDelta;
}

static uint8_t *voEncodeBoundCoord (uint8_t *pu8Dst, int16_t coord, int16_t *pCoordPrev,
                                    uint8_t *pu8BoundsPresent, uint8_t u8PresentBit)
{
    int16_t i16Delta = coord - *pCoordPrev;

    if (i16Delta != 0)
    {
        if (-128 <= i16Delta && i16Delta <= 127)
        {
            *pu8Dst++ = (int8_t)i16Delta;
            *pu8BoundsPresent |= u8PresentBit << 4;
        }
        else
        {
            /* RDP bounds are inclusive. The second point coords (present 0x04 and 0x08)
             * must be decremented.
             */
            *(uint16_t *)pu8Dst = (u8PresentBit >= 0x04)? coord - 1: coord;
            pu8Dst += 2;
            *pu8BoundsPresent |= u8PresentBit;
        }

        *pCoordPrev = coord;
    }

    return pu8Dst;
}

static uint8_t *voEncodeBounds (uint8_t *pu8DstStart, VRDEORDERBOUNDS *pBounds,
                                uint8_t *pu8OrderFlags, VRDEORDERBOUNDS *pBoundsPrev)
{
    uint8_t *pu8Dst = pu8DstStart;

    uint8_t *pu8BoundsPresent = pu8Dst++;
    *pu8BoundsPresent = 0;

    pu8Dst = voEncodeBoundCoord (pu8Dst, pBounds->pt1.x, &pBoundsPrev->pt1.x, pu8BoundsPresent, 0x01);
    pu8Dst = voEncodeBoundCoord (pu8Dst, pBounds->pt1.y, &pBoundsPrev->pt1.y, pu8BoundsPresent, 0x02);
    pu8Dst = voEncodeBoundCoord (pu8Dst, pBounds->pt2.x, &pBoundsPrev->pt2.x, pu8BoundsPresent, 0x04);
    pu8Dst = voEncodeBoundCoord (pu8Dst, pBounds->pt2.y, &pBoundsPrev->pt2.y, pu8BoundsPresent, 0x08);

    if (*pu8BoundsPresent)
    {
        *pu8OrderFlags |= RDP_ORDER_BOUNDS;
    }
    else
    {
        /* All deltas == 0. */
        *pu8OrderFlags |= RDP_ORDER_BOUNDS | RDP_ORDER_LASTBOUNDS;
        pu8Dst = pu8DstStart;
    }

    return pu8Dst;
}

static void voEncodeRectClr (uint32_t u32Clr, uint8_t **ppu8DstStart,
                             uint8_t *pu8OrderPresent, uint32_t *pu32PrevClr,
                             uint8_t u8BitsPerPixel)
{
    uint8_t *pu8Dst = *ppu8DstStart;

    uint8_t *pau8Clr     = (uint8_t *)&u32Clr;
    uint8_t *pau8ClrPrev = (uint8_t *)pu32PrevClr;

    uint8_t u8Present = *pu8OrderPresent;

    if (u8BitsPerPixel == 24)
    {
        /* RDP uses R in the first byte, G in the second and B in the third for 24 bits color. */
        if (pau8Clr[2] != pau8ClrPrev[2])
        {
            *pu8Dst++ = pau8Clr[2];
            u8Present |= RDP_PRESENT_RECT_COLOR1;
        }

        if (pau8Clr[1] != pau8ClrPrev[1])
        {
            *pu8Dst++ = pau8Clr[1];
            u8Present |= RDP_PRESENT_RECT_COLOR2;
        }

        if (pau8Clr[0] != pau8ClrPrev[0])
        {
            *pu8Dst++ = pau8Clr[0];
            u8Present |= RDP_PRESENT_RECT_COLOR3;
        }
    }
    else
    {
        if (pau8Clr[0] != pau8ClrPrev[0])
        {
            *pu8Dst++ = pau8Clr[0];
            u8Present |= RDP_PRESENT_RECT_COLOR1;
        }

        if (pau8Clr[1] != pau8ClrPrev[1])
        {
            *pu8Dst++ = pau8Clr[1];
            u8Present |= RDP_PRESENT_RECT_COLOR2;
        }

        if (pau8Clr[2] != pau8ClrPrev[2])
        {
            *pu8Dst++ = pau8Clr[2];
            u8Present |= RDP_PRESENT_RECT_COLOR3;
        }
    }

    *pu8OrderPresent = u8Present;
    *pu32PrevClr     = u32Clr;
    *ppu8DstStart    = pu8Dst;

    return;
}

static bool voSetPresentBit (union OrderPresent *pPresent, uint32_t u32PresentBit)
{
    if (u32PresentBit & 0xFF000000)
    {
        AssertFailed ();
        return false;
    }

    if (u32PresentBit & 0x00FF0000)
    {
        *pPresent->pu32 |= u32PresentBit;
        return true;
    }

    if (u32PresentBit & 0x0000FF00)
    {
        *pPresent->pu16 |= (uint16_t)u32PresentBit;
        return true;
    }

    if (u32PresentBit & 0x000000FF)
    {
        *pPresent->pu8 |= (uint8_t)u32PresentBit;
        return true;
    }

    AssertFailed ();
    return false;
}

static uint8_t *voEncodeClrP24 (uint32_t u32Clr, uint8_t *pu8Dst, uint32_t *pu32Present,
                                uint32_t *pu32PrevClr, uint32_t u32PresentBit,
                                uint8_t u8BitsPerPixel)
{
    if (u32Clr != *pu32PrevClr)
    {
        *pu32PrevClr  = u32Clr;

        uint8_t *pau8Clr = (uint8_t *)&u32Clr;

        if (u8BitsPerPixel == 24)
        {
            *pu8Dst++ = pau8Clr[2]; /* R */
            *pu8Dst++ = pau8Clr[1]; /* G */
            *pu8Dst++ = pau8Clr[0]; /* B */
        }
        else
        {
            *pu8Dst++ = pau8Clr[0];
            *pu8Dst++ = pau8Clr[1];
            *pu8Dst++ = pau8Clr[2];
        }

        *pu32Present |= u32PresentBit;
    }

    return pu8Dst;
}

static uint8_t *voEncodeClrP16 (uint32_t u32Clr, uint8_t *pu8Dst, uint16_t *pu16Present,
                                uint32_t *pu32PrevClr, uint16_t u16PresentBit,
                                uint8_t u8BitsPerPixel)
{
    if (u32Clr != *pu32PrevClr)
    {
        *pu32PrevClr  = u32Clr;

        uint8_t *pau8Clr = (uint8_t *)&u32Clr;

        if (u8BitsPerPixel == 24)
        {
            *pu8Dst++ = pau8Clr[2]; /* R */
            *pu8Dst++ = pau8Clr[1]; /* G */
            *pu8Dst++ = pau8Clr[0]; /* B */
        }
        else
        {
            *pu8Dst++ = pau8Clr[0];
            *pu8Dst++ = pau8Clr[1];
            *pu8Dst++ = pau8Clr[2];
        }

        *pu16Present |= u16PresentBit;
    }

    return pu8Dst;
}

static uint8_t *voEncodeClrP8 (uint32_t u32Clr, uint8_t *pu8Dst, uint8_t *pu8Present,
                               uint32_t *pu32PrevClr, uint8_t u8PresentBit,
                               uint8_t u8BitsPerPixel)
{
    if (u32Clr != *pu32PrevClr)
    {
        *pu32PrevClr  = u32Clr;

        uint8_t *pau8Clr = (uint8_t *)&u32Clr;

        if (u8BitsPerPixel == 24)
        {
            *pu8Dst++ = pau8Clr[2]; /* R */
            *pu8Dst++ = pau8Clr[1]; /* G */
            *pu8Dst++ = pau8Clr[0]; /* B */
        }
        else
        {
            *pu8Dst++ = pau8Clr[0];
            *pu8Dst++ = pau8Clr[1];
            *pu8Dst++ = pau8Clr[2];
        }

        *pu8Present |= u8PresentBit;
    }

    return pu8Dst;
}

static uint8_t *voEncode2BytesSigned (uint8_t *pu8Dst, int16_t value)
{
    /* 0x80 - 2 bytes, 0x40 - negative. */
    if (value >= -0x40 && value <= 0x3f)
    {
        *pu8Dst++ = (uint8_t)(value & 0x7f);
    }
    else
    {
        Assert (value >= -0x3fff && value <= 0x3fff);

        value |= 0x8000;

        uint8_t *pu8Value = (uint8_t *)&value;

        *pu8Dst++ = pu8Value[1];
        *pu8Dst++ = pu8Value[0];
    }

    return pu8Dst;
}


static uint8_t *voEncodePolyPoints (const VRDEORDERPOINT *pptStart,
                                    VRDEORDERPOLYPOINTS *pPoints, uint8_t *pu8Dst, union OrderPresent *pPresent,
                                    RDPPOLYPOINTSDATA *pPrevData, uint32_t u32PresentBit)
{
    RDPPOLYPOINTSDATA data;

    if (pPoints->c == 0 || pPoints->c > RT_ELEMENTS(pPoints->a))
    {
        return pu8Dst;
    }

    /* Encode points.
     * flag_byte_0, ...;
     * delta encoded points
     */
    unsigned nFlagBytes = ((pPoints->c - 1) / 4) + 1;

    uint8_t *pu8Flags = &data.data[0];
    uint8_t *pu8Points = pu8Flags + nFlagBytes;

    memset (pu8Flags, 0, nFlagBytes);

    bool fFailed = false;

    unsigned i;

    VRDEORDERPOINT ptPrev = *pptStart;

    VRDPTPLOG(("voEncodePolyPoints: start %d,%d\n",
               ptPrev.x, ptPrev.y));

    for (i = 0; i < pPoints->c; i++)
    {
        uint8_t *pu8Flag = &pu8Flags[i / 4];

        VRDPTPLOG(("voEncodePolyPoints: %d %d,%d\n",
                   i, pPoints->a[i].x, pPoints->a[i].y));

        int16_t delta = pPoints->a[i].x - ptPrev.x;

        if (delta < -0x4000 || delta > 0x3FFF)
        {
            fFailed = true;
            break;
        }

        if (delta == 0)
        {
            /* Set flag indicating that x delta is 0. */
            *pu8Flag |= (0x80 >> ((i & 3) * 2));
        }
        else
        {
            /* Encode 2 bytes value. */
            pu8Points = voEncode2BytesSigned (pu8Points, delta);
        }

        delta = pPoints->a[i].y - ptPrev.y;

        if (delta < -0x4000 || delta > 0x3FFF)
        {
            fFailed = true;
            break;
        }

        if (delta == 0)
        {
            /* Set flag indicating that x delta is 0. */
            *pu8Flag |= (0x40 >> ((i & 3) * 2));
        }
        else
        {
            /* Encode 2 bytes value. */
            pu8Points = voEncode2BytesSigned (pu8Points, delta);
        }

        ptPrev = pPoints->a[i];
    }

    data.datasize = pu8Points - pu8Flags;

    /* If they are not equal to previous ones, append them to the order. */
    if (   !fFailed
        && (   data.datasize != pPrevData->datasize
            || memcmp (data.data, pPrevData->data, data.datasize) != 0)
        && voSetPresentBit (pPresent, u32PresentBit)
       )
    {
        *pu8Dst++ = data.datasize;

        memcpy (pu8Dst, data.data, data.datasize);
        pu8Dst += data.datasize;

        *pPrevData = data;
    }

    return pu8Dst;
}


static void voRemoveLastZeroLengthLine(VRDEORDERPOLYPOINTS *pPoints, const VRDEORDERPOINT *pptStart)
{
    uint8_t c = pPoints->c;

    if (c == 1)
    {
        if (pPoints->a[0].x == pptStart->x
            && pPoints->a[0].y == pptStart->y)
        {
            pPoints->c--;
        }
    }
    else if (c >= 2)
    {
        if (pPoints->a[c - 2].x == pPoints->a[c - 1].x
            && pPoints->a[c - 2].y == pPoints->a[c - 1].y)
        {
            pPoints->c--;
        }
    }
}

/** @todo make simplyfied versions of next 2 function. */
static uint8_t *voAdjust1BytePresent (uint8_t *pu8OrderPresent, uint8_t *pu8DstEnd, uint8_t *pu8OrderFlags)
{
    Assert (pu8DstEnd > pu8OrderPresent && pu8DstEnd - pu8OrderPresent >= 1);

    if (*pu8OrderPresent == 0)
    {
        *pu8OrderFlags |= RDP_ORDER_SMALL;

        pu8DstEnd -= 1;

        memmove (&pu8OrderPresent[0], &pu8OrderPresent[1], pu8DstEnd - &pu8OrderPresent[0]);
    }

    return pu8DstEnd;
}

static uint8_t *voAdjust2BytesPresent (uint16_t *pu16OrderPresent, uint8_t *pu8DstEnd, uint8_t *pu8OrderFlags)
{
    uint8_t *pu8OrderPresent = (uint8_t *)pu16OrderPresent;

    Assert (pu8DstEnd > pu8OrderPresent && pu8DstEnd - pu8OrderPresent >= 2);

    if (pu8OrderPresent[1] == 0)
    {
        uint8_t *pu8Trg;

        if (pu8OrderPresent[0] == 0)
        {
            *pu8OrderFlags |= RDP_ORDER_TINY;
            pu8DstEnd -= 2;
            pu8Trg = &pu8OrderPresent[0];
        }
        else
        {
            *pu8OrderFlags |= RDP_ORDER_SMALL;
            pu8DstEnd -= 1;
            pu8Trg = &pu8OrderPresent[1];
        }

        memmove (pu8Trg, &pu8OrderPresent[2], pu8DstEnd - &pu8OrderPresent[0]);
    }

    return pu8DstEnd;
}

static uint8_t *voAdjust3BytesPresent (uint32_t *pu32OrderPresent, uint8_t *pu8DstEnd, uint8_t *pu8OrderFlags)
{
    uint8_t *pu8OrderPresent = (uint8_t *)pu32OrderPresent;

    Assert (pu8DstEnd > pu8OrderPresent && pu8DstEnd - pu8OrderPresent >= 3);

    /* The RDP_ORDER_TINY and RDP_ORDER_SMALL simply encode the number of NULL bytes. */
    uint8_t u8ZeroBytes = 0;

    if (pu8OrderPresent[2] == 0)
    {
        u8ZeroBytes++;

        if (pu8OrderPresent[1] == 0)
        {
            u8ZeroBytes++;

            if (pu8OrderPresent[0] == 0)
            {
                u8ZeroBytes++;
            }
        }
    }

    if (u8ZeroBytes)
    {
        uint8_t *pu8Trg = &pu8OrderPresent[3 - u8ZeroBytes];

        pu8DstEnd -= u8ZeroBytes;

        memmove (pu8Trg, &pu8OrderPresent[3], pu8DstEnd - pu8OrderPresent);

        *pu8OrderFlags |= u8ZeroBytes << 6;
    }

    return pu8DstEnd;
}

#define VRDP_START_ORDER(__order_type) do {                           \
    pu8Dst = pBlock->pu8DstStart;                                     \
    pu8OrderFlags = pu8Dst++;                                         \
    *pu8OrderFlags = RDP_ORDER_STANDARD;                              \
    if (m_savedOrders.u8LastOrder != (__order_type))                  \
    {                                                                 \
        *pu8OrderFlags |= RDP_ORDER_CHANGE;                           \
        *pu8Dst++ = m_savedOrders.u8LastOrder = (__order_type);       \
    }                                                                 \
} while (0)

#define VRDP_INIT_PRESENT_1_BYTE() do {        \
    present.pu8 = pu8Dst++;                    \
    *present.pu8 = 0;                          \
} while (0)

#define VRDP_INIT_PRESENT_2_BYTES() do {       \
    present.pu16 = (uint16_t *)pu8Dst;         \
    pu8Dst += 2;                               \
    *present.pu16 = 0;                         \
} while (0)

#define VRDP_INIT_PRESENT_3_BYTES() do {       \
    present.pu32 = (uint32_t *)pu8Dst;         \
    pu8Dst += 3;                               \
    present.pu8[0] = 0;                        \
    present.pu8[1] = 0;                        \
    present.pu8[2] = 0;                        \
} while (0)


#define VRDP_ENCODE_BOUNDS() do {                                               \
    if (m_savedOrders.fBounds)                                                  \
    {                                                                           \
        pu8Dst = voEncodeBounds (pu8Dst, &m_savedOrders.boundsCurrent,          \
                                 pu8OrderFlags, &m_savedOrders.boundsPrevious); \
    }                                                                           \
} while (0)

static uint32_t voCalcRemoteScreenPixels (uint32_t w, uint32_t h)
{
    /* The 20 lines Y granularity is hardcoded in all supported clients. */
    return w * (((h + 19) / 20) * 20);
}

int VRDPTP::outSaveScreenOrder (VRDPStream *pStream, VRDEORDERAREA *pArea, uint8_t restore, uint32_t offset)
{
    const VRDPStream::MemoryBlock *pBlock;
    uint8_t                *pu8OrderFlags;
    uint8_t                *pu8Dst;
    union OrderPresent      present;
    VRDEORDERPOINT pt1;
    VRDEORDERPOINT pt2;

    pt1.x = pArea->x;
    pt1.y = pArea->y;

    /* Inclusive. */
    pt2.x = pArea->x + pArea->w - 1;
    pt2.y = pArea->y + pArea->h - 1;

    /* Reserve enough space for an order with complete information. */
    pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order,
                                    VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.desksave));

    if (!pBlock)
    {
        return VERR_NO_MEMORY;
    }

    VRDP_START_ORDER(RDP_ORDER_TYPE_DESKSAVE);

    /* The order has a 8 bit present field. */
    VRDP_INIT_PRESENT_1_BYTE();

    if (m_savedOrders.desksave.offset != offset)
    {
        *(uint32_t *)pu8Dst = m_savedOrders.desksave.offset = offset;
        pu8Dst += 4;
        *present.pu8 |= RDP_PRESENT_DESKSAVE_OFFSET;
    }

    uint8_t *pu8DstCoords = pu8Dst;

    bool fDelta = voEncodeCoordDelta (&pu8DstCoords, pt1.x, m_savedOrders.desksave.pt1.x, present.pu8, RDP_PRESENT_DESKSAVE_X1);
    fDelta =      voEncodeCoordDelta (&pu8DstCoords, pt1.y, m_savedOrders.desksave.pt1.y, present.pu8, RDP_PRESENT_DESKSAVE_Y1) && fDelta;
    fDelta =      voEncodeCoordDelta (&pu8DstCoords, pt2.x, m_savedOrders.desksave.pt2.x, present.pu8, RDP_PRESENT_DESKSAVE_X2) && fDelta;
    fDelta =      voEncodeCoordDelta (&pu8DstCoords, pt2.y, m_savedOrders.desksave.pt2.y, present.pu8, RDP_PRESENT_DESKSAVE_Y2) && fDelta;

    m_savedOrders.desksave.pt1 = pt1;
    m_savedOrders.desksave.pt2 = pt2;

    if (fDelta)
    {
        *pu8OrderFlags |= RDP_ORDER_DELTA;
        pu8Dst = pu8DstCoords;
    }
    else
    {
        uint16_t *pu16Dst = (uint16_t *)pu8Dst;

        if (*present.pu8 & RDP_PRESENT_DESKSAVE_X1)
        {
            *pu16Dst++ = pt1.x;
        }

        if (*present.pu8 & RDP_PRESENT_DESKSAVE_Y1)
        {
            *pu16Dst++ = pt1.y;
        }

        if (*present.pu8 & RDP_PRESENT_DESKSAVE_X2)
        {
            *pu16Dst++ = pt2.x;
        }

        if (*present.pu8 & RDP_PRESENT_DESKSAVE_Y2)
        {
            *pu16Dst++ = pt2.y;
        }

        pu8Dst = (uint8_t *)pu16Dst;
    }

    if (m_savedOrders.desksave.restore != restore)
    {
        *pu8Dst++ = m_savedOrders.desksave.restore = restore;
        *present.pu8 |= RDP_PRESENT_DESKSAVE_RESTORE;
    }

    pu8Dst = voAdjust1BytePresent (present.pu8, pu8Dst, pu8OrderFlags);

    /* Complete the block. Pass how many bytes were actually written. */
    VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);

    return VINF_SUCCESS;
}

#define TEXT2_WRITE(_type, _saved, _value, _presentflag) do {  \
    if (m_savedOrders.text2._saved != _value)                  \
    {                                                          \
        *(_type *)pu8Dst = m_savedOrders.text2._saved = _value;\
        pu8Dst += sizeof (_type);                              \
        *present.pu32 |= _presentflag;                         \
    }                                                          \
} while (0)

int VRDPTP::outText2 (unsigned uScreenId,
                      VRDPStream *pStream,
                      uint8_t u8RDPFontHandle,
                      uint8_t u8Flags,
                      uint8_t u8CharInc,
                      uint32_t rgbFG,
                      uint32_t rgbBG,
                      const VRDEORDERRECT *pBkGround,
                      const VRDEORDERRECT *pOpaque,
                      const VRDEORDERPOINT *pOrigin,
                      uint8_t u8TextLength,
                      const uint8_t *pu8Text
                     )
{
    const VRDPStream::MemoryBlock *pBlock;
    uint8_t                *pu8Dst;
    uint8_t                *pu8OrderFlags;
    union OrderPresent      present;

    VRDEORDERAREA opaque;
    VRDEORDERAREA bkground;
    VRDEORDERPOINT origin;
    uint32_t u32ClrDst;
    int16_t i16Tmp;

    /* RDP_ORDER_TEXT2 */
    VRDPTPLOG(("VRDPTP::OutputText2: rgbFG 0x%06X rgbBG 0x%06X\n",
               rgbFG, rgbBG));

    /* Optimization: if the opaque rectangle is the same as the bkground one, then
     * it is possible to set the u8MixMode to 1 and send only the bkground rectangle.
     */
    uint8_t u8MixMode = 0;

    if (   pOpaque->left   == pBkGround->left
        && pOpaque->top    == pBkGround->top
        && pOpaque->right  == pBkGround->right
        && pOpaque->bottom == pBkGround->bottom
       )
    {
        memset (&opaque, 0, sizeof (opaque));
        u8MixMode = 1;
    }
    else
    {
        if (pOpaque->right != pOpaque->left)
        {
            if (!voAdjustArea (uScreenId, this, &opaque,
                               pOpaque->left,
                               pOpaque->top,
                               pOpaque->right -  pOpaque->left,
                               pOpaque->bottom - pOpaque->top))
            {
                return VWRN_INVALID_PARAMETER;
            }
        }
        else
        {
            memset (&opaque, 0, sizeof (opaque));
        }
    }

    if (pBkGround->right != pBkGround->left)
    {
        if (!voAdjustArea (uScreenId, this, &bkground,
                           pBkGround->left,
                           pBkGround->top,
                           pBkGround->right - pBkGround->left,
                           pBkGround->bottom - pBkGround->top))
        {
            return VWRN_INVALID_PARAMETER;
        }
    }
    else
    {
        memset (&bkground, 0, sizeof (bkground));
    }

    voAdjustPoint (this, uScreenId, &origin, pOrigin->x, pOrigin->y);

    /* Reserve enough space for an order with complete information. */
    pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order, VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.text2));

    if (!pBlock)
    {
        return VERR_NO_MEMORY;
    }

    VRDP_START_ORDER(RDP_ORDER_TYPE_TEXT2);

    /* The order has a 24 bit present field. */
    VRDP_INIT_PRESENT_3_BYTES();

    VRDP_ENCODE_BOUNDS();

    TEXT2_WRITE(uint8_t,  font,        u8RDPFontHandle, RDP_PRESENT_TEXT2_FONT);
    TEXT2_WRITE(uint8_t,  flags,       u8Flags,         RDP_PRESENT_TEXT2_FLAGS);
    TEXT2_WRITE(uint8_t,  charinc,     u8CharInc,       RDP_PRESENT_TEXT2_CHARINC);
    TEXT2_WRITE(uint8_t,  mixmode,     u8MixMode,                   RDP_PRESENT_TEXT2_MIXMODE);

    u32ClrDst = 0;
    ConvertColors ((uint8_t *)&rgbFG, 32, 1,
                   (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));
    pu8Dst = voEncodeClrP24 (u32ClrDst, pu8Dst, present.pu32, &m_savedOrders.text2.fgcolour, RDP_PRESENT_TEXT2_FG, m_bpp);

    u32ClrDst = 0;
    ConvertColors ((uint8_t *)&rgbBG, 32, 1,
                   (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));
    pu8Dst = voEncodeClrP24 (u32ClrDst, pu8Dst, present.pu32, &m_savedOrders.text2.bgcolour, RDP_PRESENT_TEXT2_BG, m_bpp);

    TEXT2_WRITE(uint16_t, clipleft,    bkground.x,                  RDP_PRESENT_TEXT2_CLIPLEFT);
    TEXT2_WRITE(uint16_t, cliptop,     bkground.y,                  RDP_PRESENT_TEXT2_CLIPTOP);
    i16Tmp = bkground.x + bkground.w;
    TEXT2_WRITE(uint16_t, clipright,   i16Tmp,                      RDP_PRESENT_TEXT2_CLIPRIGHT);
    i16Tmp = bkground.y + bkground.h;
    TEXT2_WRITE(uint16_t, clipbottom,  i16Tmp,                      RDP_PRESENT_TEXT2_CLIPBOTTOM);

    TEXT2_WRITE(uint16_t, boxleft,     opaque.x,                    RDP_PRESENT_TEXT2_BOXLEFT);
    TEXT2_WRITE(uint16_t, boxtop,      opaque.y,                    RDP_PRESENT_TEXT2_BOXTOP);
    i16Tmp = opaque.x + opaque.w;
    TEXT2_WRITE(uint16_t, boxright,    i16Tmp,                      RDP_PRESENT_TEXT2_BOXRIGHT);
    i16Tmp = opaque.y + opaque.h;
    TEXT2_WRITE(uint16_t, boxbottom,   i16Tmp,                      RDP_PRESENT_TEXT2_BOXBOTTOM);

    TEXT2_WRITE(uint8_t,  brush.style, 0,                           RDP_PRESENT_BRUSH_STYLE << RDP_PRESENT_TEXT2_BRUSHSHIFT);

    TEXT2_WRITE(uint16_t, x,           origin.x,                    RDP_PRESENT_TEXT2_X);
    TEXT2_WRITE(uint16_t, y,           origin.y,                    RDP_PRESENT_TEXT2_Y);

    if (   u8TextLength != m_savedOrders.text2.length
        || (   u8TextLength != 0
            && memcmp (m_savedOrders.text2.text, pu8Text, u8TextLength) != 0)
       )
    {
        *pu8Dst++ = m_savedOrders.text2.length = u8TextLength;
        if (u8TextLength)
        {
            memcpy (m_savedOrders.text2.text, pu8Text, u8TextLength);
            memcpy (pu8Dst, pu8Text, u8TextLength);
            pu8Dst += u8TextLength;
        }
        *present.pu32 |= RDP_PRESENT_TEXT2_TEXT;
    }

    pu8Dst = voAdjust3BytesPresent (present.pu32, pu8Dst, pu8OrderFlags);

    /* Complete the block. Pass how many bytes were actually written. */
    VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);

    return VINF_SUCCESS;
}

#undef TEXT2_WRITE

int VRDPTP::outMemblt (unsigned uScreenId,
                       VRDPStream *pStream,
                       uint16_t u16CacheId,
                       uint16_t u16Idx,
                       uint16_t u16DstX,
                       uint16_t u16DstY,
                       uint16_t u16Width,
                       uint16_t u16Height,
                       uint16_t u16SrcX,
                       uint16_t u16SrcY,
                       uint8_t u8ROP
                      )
{
    const VRDPStream::MemoryBlock *pBlock;
    VRDEORDERAREA           dst;
    uint8_t                *pu8Dst;
    uint8_t                *pu8OrderFlags;
    union OrderPresent      present;

    VRDPTPLOG(("VRDPTP::outMemblt: screen %d, pStream %p, cache id %d, idx %d, dst %d,%d %dx%d src %d,%d\n",
               uScreenId,
               pStream,
               u16CacheId,
               u16Idx,
               u16DstX,
               u16DstY,
               u16Width,
               u16Height,
               u16SrcX,
               u16SrcY));
    /*
     * Memblt order.
     */

    if (!voAdjustArea (uScreenId, this, &dst,u16DstX, u16DstY, u16Width, u16Height))
    {
        /* Nothing to draw. */
        return VINF_SUCCESS;
    }

    VRDPTPLOG(("VRDPTP::outMemblt: dst area %d,%d %dx%d\n", dst.x, dst.y, dst.w, dst.h));

    /* Reserve enough space for an order with complete information. */
    pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order,
                                    VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.memblt));

    if (pBlock == NULL)
    {
        return VERR_NO_MEMORY;
    }

    VRDP_START_ORDER(RDP_ORDER_TYPE_MEMBLT);

    /* The order has a 16 bit present field. */
    VRDP_INIT_PRESENT_2_BYTES();

    /* Memblt order has no bounds. */

    if (m_savedOrders.memblt.cacheid != u16CacheId)
    {
        *(uint16_t *)pu8Dst = m_savedOrders.memblt.cacheid = u16CacheId;
        pu8Dst += 2;
        *present.pu16 |= RDP_PRESENT_MEMBLT_CACHEID;
    }

    int16_t deltaSrcX = u16SrcX - m_savedOrders.memblt.src.x;
    int16_t deltaSrcY = u16SrcY - m_savedOrders.memblt.src.y;

    bool fDelta =    (-128 <= deltaSrcX && deltaSrcX <= 127)
                  && (-128 <= deltaSrcY && deltaSrcY <= 127);

    fDelta = voEncodeOrderArea (fDelta, &dst, pu8OrderFlags,
                                &m_savedOrders.memblt.area, &pu8Dst,
                                (uint8_t *)present.pu16, RDP_PRESENT_MEMBLT_X);

    if (m_savedOrders.memblt.rop != u8ROP)
    {
        *pu8Dst++ = m_savedOrders.memblt.rop = u8ROP;
        *present.pu16 |= RDP_PRESENT_MEMBLT_ROP;
    }

    if (deltaSrcX)
    {
        *present.pu16 |= RDP_PRESENT_MEMBLT_XSRC;

        m_savedOrders.memblt.src.x = u16SrcX;

        if (fDelta)
        {
            *pu8Dst++ = (int8_t)deltaSrcX;
        }
        else
        {
            *(uint16_t *)pu8Dst = u16SrcX;
            pu8Dst += 2;
        }
    }

    if (deltaSrcY)
    {
        *present.pu16 |= RDP_PRESENT_MEMBLT_YSRC;

        m_savedOrders.memblt.src.y = u16SrcY;

        if (fDelta)
        {
            *pu8Dst++ = (int8_t)deltaSrcY;
        }
        else
        {
            *(uint16_t *)pu8Dst = u16SrcY;
            pu8Dst += 2;
        }
    }

    if (m_savedOrders.memblt.cacheidx != u16Idx)
    {
        *(uint16_t *)pu8Dst = m_savedOrders.memblt.cacheidx = u16Idx;
        pu8Dst += 2;
        *present.pu16 |= RDP_PRESENT_MEMBLT_CACHEIDX;
    }

    pu8Dst = voAdjust2BytesPresent (present.pu16, pu8Dst, pu8OrderFlags);

    /* Complete the block. Pass how many bytes were actually written. */
    VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);

    return VINF_SUCCESS;
}

int VRDPTP::OutputOrder (unsigned uScreenId, VRDPStream *pStream, int32_t i32Op, void *pvOrder)
{
    int rc = VINF_VRDP_SUCCESS;

    const VRDPStream::MemoryBlock *pBlock;
    VRDEORDERAREA           area;
    uint8_t                *pu8Dst;
    uint8_t                *pu8OrderFlags;
    union OrderPresent      present;

    uint32_t u32ClrDst = 0;

    uint32_t cbInternalOrder = 0;

    bool fBounds = false;

    VRDPTPLOG(("VRDPTP::OutputOrder: screen %d\n", uScreenId));

    switch (i32Op)
    {
        case VRDE_ORDER_DIRTY_RECT:
        {
            /* The order should be already processed by the shadow buffer. */
            AssertFailed ();
        } break;

        case VRDP_ORDER_INTERNALCANCEL:
        {
            /* An order was replaced with a bitmap update, reset bounds.
             * m_savedOrders.fBounds will be updated in the 'if' at the end of this function.
             */
            Assert(fBounds == false);
        } break;

        case VRDP_ORDER_INTERNALTEXT2:
        {
            VRDPORDERINTERNALTEXT2 *pOrder = (VRDPORDERINTERNALTEXT2 *)pvOrder;

            /* RDP_ORDER_TEXT2 */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDP_ORDER_INTERNALTEXT2\n"));

            cbInternalOrder = sizeof (VRDPORDERINTERNALTEXT2);

            rc = outText2 (uScreenId,
                           pStream,
                           pOrder->u8RDPFontHandle,
                           pOrder->u8Flags,
                           pOrder->u8CharInc,
                           pOrder->rgbFG,
                           pOrder->rgbBG,
                           &pOrder->bkground,
                           &pOrder->opaque,
                           &pOrder->origin,
                           pOrder->u8TextLength,
                           pOrder->au8Text);
        } break;

        case VRDP_ORDER_INTERNALMEMBLT:
        {
            VRDPORDERINTERNALMEMBLT *pOrder = (VRDPORDERINTERNALMEMBLT *)pvOrder;

            /* RDP_ORDER_MEMBLT */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDP_ORDER_INTERNALMEMBLT\n"));

            cbInternalOrder = sizeof (VRDPORDERINTERNALMEMBLT);

            rc = outMemblt (uScreenId,
                            pStream,
                            pOrder->u16CacheId,
                            pOrder->u16Idx,
                            pOrder->u16DstX,
                            pOrder->u16DstY,
                            pOrder->u16Width,
                            pOrder->u16Height,
                            pOrder->u16SrcX,
                            pOrder->u16SrcY,
                            pOrder->u8ROP);
        } break;

        case VRDP_ORDER_INTERNALSEQBEGIN:
        {
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDP_ORDER_INTERNALSEQBEGIN\n"));

            /* Set a flag that indicates that:
             *   1) the consequent internal orders must be saved;
             *   2) bounds must not be reset.
             */
            Assert (m_fSeqInProcess == false);
            m_fSeqInProcess = true;

            /* Clear saved internal orders. */
            VRDPSAVEDINTERNALORDER *pSavedOrder = m_pSavedInternalOrders;

            m_pSavedInternalOrders = NULL;

            while (pSavedOrder)
            {
                VRDPSAVEDINTERNALORDER *next = pSavedOrder->next;

                VRDPMemFree (pSavedOrder);

                pSavedOrder = next;
            }
        } break;

        case VRDP_ORDER_INTERNALSEQEND:
        {
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDP_ORDER_INTERNALSEQEND\n"));

            /* The sequence has been completed.
             * Bounds can be reset now.
             * Any REPEAT order from now on must repeat entire saved sequence.
             */
            m_fSeqInProcess = false;

            /* Reverse the saved internal orders list. */
            VRDPSAVEDINTERNALORDER *pSavedOrder = m_pSavedInternalOrders;

            m_pSavedInternalOrders = NULL;

            while (pSavedOrder)
            {
                VRDPSAVEDINTERNALORDER *next = pSavedOrder->next;

                pSavedOrder->next = m_pSavedInternalOrders;
                m_pSavedInternalOrders = pSavedOrder;

                pSavedOrder = next;
            }
        } break;

        case VRDP_ORDER_INTERNALCLS:
        {
            VRDPORDERINTERNALCLS *pOrder = (VRDPORDERINTERNALCLS *)pvOrder;

            /* RDP_ORDER_RECT */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDP_ORDER_INTERNALCLS %d,%d %dx%d\n",
                       pOrder->x, pOrder->y, pOrder->w, pOrder->h));

            if (pOrder->x == -1 && pOrder->y == -1)
            {
                area.x = 0;
                area.y = 0;
                area.w = DesktopMap()->GetClientWidth();
                area.h = DesktopMap()->GetClientHeight();
            }
            else
            {
                area.x = pOrder->x;
                area.y = pOrder->y;
                area.w = pOrder->w;
                area.h = pOrder->h;
            }

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order, VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.rect));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_RECT);

            /* The order has a 8 bit present field. */
            VRDP_INIT_PRESENT_1_BYTE();

            /* Reset the clip region on the client.
             * Workaround for rdesktop 1.5.0 bug.
             */
            VRDEORDERBOUNDS bounds;

            bounds.pt1.x = 0;
            bounds.pt1.y = 0;
            bounds.pt2.x = DesktopMap()->GetClientWidth();
            bounds.pt2.y = DesktopMap()->GetClientHeight();

            pu8Dst = voEncodeBounds (pu8Dst, &bounds,
                                     pu8OrderFlags, &m_savedOrders.boundsPrevious);

            voEncodeOrderArea (true, &area, pu8OrderFlags,
                               &m_savedOrders.rect.area, &pu8Dst,
                               present.pu8, RDP_PRESENT_RECT_X);

            voEncodeRectClr (u32ClrDst, &pu8Dst, present.pu8, &m_savedOrders.rect.clr, m_bpp);

            pu8Dst = voAdjust1BytePresent (present.pu8, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDE_ORDER_SOLIDRECT:
        {
            VRDEORDERSOLIDRECT *pOrder = (VRDEORDERSOLIDRECT *)pvOrder;

            /* RDP_ORDER_RECT */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SOLIDRECT %d,%d %dx%d rgb 0x%06X\n",
                       pOrder->x, pOrder->y, pOrder->w, pOrder->h, pOrder->rgb));

            if (!voAdjustArea (uScreenId, this, &area, pOrder->x, pOrder->y, pOrder->w, pOrder->h))
            {
                break;
            }

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order, VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.rect));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_RECT);

            /* The order has a 8 bit present field. */
            VRDP_INIT_PRESENT_1_BYTE();

            VRDP_ENCODE_BOUNDS();

            voEncodeOrderArea (true, &area, pu8OrderFlags,
                               &m_savedOrders.rect.area, &pu8Dst,
                               present.pu8, RDP_PRESENT_RECT_X);

            ConvertColors ((uint8_t *)&pOrder->rgb, 32, 1,
                           (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));

            VRDPTPLOG(("VRDPTP::OutputOrder: clr 0x%06X, bpp %d\n", u32ClrDst, m_bpp));

            voEncodeRectClr (u32ClrDst, &pu8Dst, present.pu8, &m_savedOrders.rect.clr, m_bpp);

            pu8Dst = voAdjust1BytePresent (present.pu8, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDE_ORDER_SOLIDBLT:
        {
            VRDEORDERSOLIDBLT *pOrder = (VRDEORDERSOLIDBLT *)pvOrder;

            /* RDP_ORDER_PATBLT */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SOLIDBLT %d,%d %dx%d rgb 0x%06X rop 0x%02X\n",
                       pOrder->x, pOrder->y, pOrder->w, pOrder->h, pOrder->rgb, pOrder->rop));

            if (!voAdjustArea (uScreenId, this, &area, pOrder->x, pOrder->y, pOrder->w, pOrder->h))
            {
                break;
            }

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order, VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.patblt));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_PATBLT);

            /* The order has a 16 bit present field. */
            VRDP_INIT_PRESENT_2_BYTES();

            VRDP_ENCODE_BOUNDS();

            voEncodeOrderArea (true, &area, pu8OrderFlags,
                               &m_savedOrders.patblt.area, &pu8Dst,
                               (uint8_t *)present.pu16, RDP_PRESENT_PATBLT_X);

            if (m_savedOrders.patblt.rop != pOrder->rop)
            {
                *pu8Dst++ = m_savedOrders.patblt.rop = pOrder->rop;
                *present.pu16 |= RDP_PRESENT_PATBLT_ROP;
            }

            pu8Dst = voEncodeClrP16 (0, pu8Dst, present.pu16, &m_savedOrders.patblt.clrB, RDP_PRESENT_PATBLT_BG, m_bpp);

            ConvertColors ((uint8_t *)&pOrder->rgb, 32, 1,
                           (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));

            pu8Dst = voEncodeClrP16 (u32ClrDst, pu8Dst, present.pu16, &m_savedOrders.patblt.clrF, RDP_PRESENT_PATBLT_FG, m_bpp);

            if (m_savedOrders.patblt.brush.style != 0)
            {
                *pu8Dst++ = m_savedOrders.patblt.brush.style = 0;
                *present.pu16 |= RDP_PRESENT_BRUSH_STYLE << 7;
            }

            pu8Dst = voAdjust2BytesPresent (present.pu16, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDE_ORDER_DSTBLT:
        {
            VRDEORDERDSTBLT *pOrder = (VRDEORDERDSTBLT *)pvOrder;

            /* RDP_ORDER_DESTBLT */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_DSTBLT %d,%d %dx%d rop 0x%02X\n",
                       pOrder->x, pOrder->y, pOrder->w, pOrder->h, pOrder->rop));

            if (!voAdjustArea (uScreenId, this, &area, pOrder->x, pOrder->y, pOrder->w, pOrder->h))
            {
                break;
            }

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order, VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.dstblt));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_DESTBLT);

            /* The order has a 8 bit present field. */
            VRDP_INIT_PRESENT_1_BYTE();

            VRDP_ENCODE_BOUNDS();

            voEncodeOrderArea (true, &area, pu8OrderFlags,
                               &m_savedOrders.dstblt.area, &pu8Dst,
                               present.pu8, RDP_PRESENT_DSTBLT_X);

            if (m_savedOrders.dstblt.rop != pOrder->rop)
            {
                *pu8Dst++ = m_savedOrders.dstblt.rop = pOrder->rop;
                *present.pu8 |= RDP_PRESENT_DSTBLT_ROP;
            }

            pu8Dst = voAdjust1BytePresent (present.pu8, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDE_ORDER_SCREENBLT:
        {
            VRDEORDERSCREENBLT *pOrder = (VRDEORDERSCREENBLT *)pvOrder;

            /* RDP_ORDER_SCREENBLT */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SCREENBLT %d,%d %dx%d from %d,%d rop 0x%02X\n",
                       pOrder->x, pOrder->y, pOrder->w, pOrder->h, pOrder->xSrc, pOrder->ySrc, pOrder->rop));

            if (!voAdjustArea (uScreenId, this, &area, pOrder->x, pOrder->y, pOrder->w, pOrder->h))
            {
                break;
            }

            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SCREENBLT dst %d,%d %dx%d\n",
                       area.x, area.y, area.w, area.h));

            VRDEORDERAREA areaSrc;
            if (!voAdjustArea (uScreenId, this, &areaSrc, pOrder->xSrc, pOrder->ySrc, pOrder->w, pOrder->h))
            {
                break;
            }

            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SCREENBLT src %d,%d %dx%d\n",
                       areaSrc.x, areaSrc.y, areaSrc.w, areaSrc.h));

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order, VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.screenblt));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_SCREENBLT);

            /* The order has a 8 bit present field. */
            VRDP_INIT_PRESENT_1_BYTE();

            VRDP_ENCODE_BOUNDS();

            int16_t deltaSrcX = areaSrc.x - m_savedOrders.screenblt.src.x;
            int16_t deltaSrcY = areaSrc.y - m_savedOrders.screenblt.src.y;

            bool fDelta =    (-128 <= deltaSrcX && deltaSrcX <= 127)
                          && (-128 <= deltaSrcY && deltaSrcY <= 127);

            fDelta = voEncodeOrderArea (fDelta, &area, pu8OrderFlags,
                                        &m_savedOrders.screenblt.area, &pu8Dst,
                                        present.pu8, RDP_PRESENT_SCREENBLT_X);

            if (m_savedOrders.screenblt.rop != pOrder->rop)
            {
                *pu8Dst++ = m_savedOrders.screenblt.rop = pOrder->rop;
                *present.pu8 |= RDP_PRESENT_SCREENBLT_ROP;
            }

            if (deltaSrcX)
            {
                *present.pu8 |= RDP_PRESENT_SCREENBLT_XSRC;

                m_savedOrders.screenblt.src.x = areaSrc.x;

                if (fDelta)
                {
                    *pu8Dst++ = (int8_t)deltaSrcX;
                }
                else
                {
                    *(uint16_t *)pu8Dst = areaSrc.x;
                    pu8Dst += 2;
                }
            }

            if (deltaSrcY)
            {
                *present.pu8 |= RDP_PRESENT_SCREENBLT_YSRC;

                m_savedOrders.screenblt.src.y = areaSrc.y;

                if (fDelta)
                {
                    *pu8Dst++ = (int8_t)deltaSrcY;
                }
                else
                {
                    *(uint16_t *)pu8Dst = areaSrc.y;
                    pu8Dst += 2;
                }
            }

            pu8Dst = voAdjust1BytePresent (present.pu8, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDP_ORDER_INTERNALPATBLTCACHED:
        case VRDE_ORDER_PATBLTBRUSH:
        {
            VRDEORDERPATBLTBRUSH *pOrder = (VRDEORDERPATBLTBRUSH *)pvOrder;

            /* RDP_ORDER_PATBLT */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_PATBLTBRUSH%s %d,%d %dx%d from %d,%d rop 0x%02X rgbFG 0x%06X rgbBG 0x%06X\n",
                       i32Op == VRDE_ORDER_PATBLTBRUSH? "": "cached",
                       pOrder->x, pOrder->y, pOrder->w, pOrder->h, pOrder->xSrc, pOrder->ySrc, pOrder->rop, pOrder->rgbFG, pOrder->rgbBG));

            if (!voAdjustArea (uScreenId, this, &area, pOrder->x, pOrder->y, pOrder->w, pOrder->h))
            {
                break;
            }

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order, VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.patblt));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_PATBLT);

            /* The order has a 16 bit present field. */
            VRDP_INIT_PRESENT_2_BYTES();

            VRDP_ENCODE_BOUNDS();

            voEncodeOrderArea (true, &area, pu8OrderFlags,
                               &m_savedOrders.patblt.area, &pu8Dst,
                               (uint8_t *)present.pu16, RDP_PRESENT_PATBLT_X);

            if (m_savedOrders.patblt.rop != pOrder->rop)
            {
                *pu8Dst++ = m_savedOrders.patblt.rop = pOrder->rop;
                *present.pu16 |= RDP_PRESENT_PATBLT_ROP;
            }

            ConvertColors ((uint8_t *)&pOrder->rgbBG, 32, 1,
                           (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));

            pu8Dst = voEncodeClrP16 (u32ClrDst, pu8Dst, present.pu16, &m_savedOrders.patblt.clrB, RDP_PRESENT_PATBLT_BG, m_bpp);

            ConvertColors ((uint8_t *)&pOrder->rgbFG, 32, 1,
                           (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));

            pu8Dst = voEncodeClrP16 (u32ClrDst, pu8Dst, present.pu16, &m_savedOrders.patblt.clrF, RDP_PRESENT_PATBLT_FG, m_bpp);

            if (m_savedOrders.patblt.brush.x != pOrder->xSrc)
            {
                *pu8Dst++ = m_savedOrders.patblt.brush.x = pOrder->xSrc;
                *present.pu16 |= RDP_PRESENT_BRUSH_X << 7;
            }

            if (m_savedOrders.patblt.brush.y != pOrder->ySrc)
            {
                *pu8Dst++ = m_savedOrders.patblt.brush.y = pOrder->ySrc;
                *present.pu16 |= RDP_PRESENT_BRUSH_Y << 7;
            }

            if (i32Op == VRDP_ORDER_INTERNALPATBLTCACHED)
            {
                if (m_savedOrders.patblt.brush.style != (0x80 | RDP_BRUSH_FMT_1BPP))
                {
                    *pu8Dst++ = m_savedOrders.patblt.brush.style = (0x80 | RDP_BRUSH_FMT_1BPP);
                    *present.pu16 |= RDP_PRESENT_BRUSH_STYLE << 7;
                }

                if (m_savedOrders.patblt.brush.pattern[0] != pOrder->pattern[0])
                {
                    *pu8Dst++ = m_savedOrders.patblt.brush.pattern[0] = pOrder->pattern[0];
                    *present.pu16 |= RDP_PRESENT_BRUSH_HATCH << 7;
                }
            }
            else
            {
                if (m_savedOrders.patblt.brush.style != 3)
                {
                    *pu8Dst++ = m_savedOrders.patblt.brush.style = 3;
                    *present.pu16 |= RDP_PRESENT_BRUSH_STYLE << 7;
                }

                if (memcmp (m_savedOrders.patblt.brush.pattern, pOrder->pattern, sizeof (pOrder->pattern)) != 0)
                {
                    memcpy (m_savedOrders.patblt.brush.pattern, pOrder->pattern, sizeof (pOrder->pattern));
                    /* The rows are encoded in reverse order. */
                    *pu8Dst++ = pOrder->pattern[7];
                    *pu8Dst++ = pOrder->pattern[6];
                    *pu8Dst++ = pOrder->pattern[5];
                    *pu8Dst++ = pOrder->pattern[4];
                    *pu8Dst++ = pOrder->pattern[3];
                    *pu8Dst++ = pOrder->pattern[2];
                    *pu8Dst++ = pOrder->pattern[1];
                    *pu8Dst++ = pOrder->pattern[0];
                    *present.pu16 |= (RDP_PRESENT_BRUSH_HATCH | RDP_PRESENT_BRUSH_PATTERN) << 7;
                }
            }

            pu8Dst = voAdjust2BytesPresent (present.pu16, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDE_ORDER_MEMBLT:
        {
            /* That is processed by the server. */
            AssertFailed ();
        } break;

        case VRDE_ORDER_CACHED_BITMAP:
        {
            /* That is processed by the server. */
            AssertFailed ();
        } break;

        case VRDE_ORDER_DELETED_BITMAP:
        {
            /* That is processed by the server. */
            AssertFailed ();
        } break;

        case VRDE_ORDER_LINE:
        {
            VRDEORDERLINE *pOrder = (VRDEORDERLINE *)pvOrder;

            /* RDP_ORDER_LINE */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_LINE %d,%d to %d,%d bounds %d %d,%d %d,%d mix 0x%02X rgb %08X\n",
                       pOrder->x1, pOrder->y1, pOrder->x2, pOrder->y2,
                       m_savedOrders.fBounds,
                       pOrder->xBounds1, pOrder->yBounds1, pOrder->xBounds2, pOrder->yBounds2,
                       pOrder->mix, pOrder->rgb));

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order,
                                          VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.line));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_LINE);

            /* The order has a 16 bit present field. */
            VRDP_INIT_PRESENT_2_BYTES();

            if (m_savedOrders.fBounds)
            {
                pu8Dst = voEncodeBounds (pu8Dst, &m_savedOrders.boundsCurrent,
                                         pu8OrderFlags, &m_savedOrders.boundsPrevious);
            }
            else if (pOrder->xBounds1 != ~0)
            {
                /* Support for old additions. */
                VRDEORDERBOUNDS bounds;

                VRDEORDERAREA areaBounds;
                voAdjustArea (uScreenId, this, &areaBounds,
                              pOrder->xBounds1,
                              pOrder->yBounds1,
                              pOrder->xBounds2 - pOrder->xBounds1,
                              pOrder->yBounds2 - pOrder->yBounds1);

                bounds.pt1.x = areaBounds.x;
                bounds.pt1.y = areaBounds.y;
                bounds.pt2.x = areaBounds.x + areaBounds.w;
                bounds.pt2.y = areaBounds.y + areaBounds.h;

                pu8Dst = voEncodeBounds (pu8Dst, &bounds,
                                         pu8OrderFlags, &m_savedOrders.boundsPrevious);
            }

            VRDEORDERPOINT pt1;
            VRDEORDERPOINT pt2;

            voAdjustPoint (this, uScreenId, &pt1, pOrder->x1, pOrder->y1);
            voAdjustPoint (this, uScreenId, &pt2, pOrder->x2, pOrder->y2);

            if (m_savedOrders.line.mixMode != 1)
            {
                *(uint16_t *)pu8Dst = m_savedOrders.line.mixMode = 1;
                pu8Dst += 2;
                *present.pu16 |= RDP_PRESENT_LINE_MIXMODE;
            }

            uint8_t *pu8DstCoords = pu8Dst;

            bool fDelta = voEncodeCoordDelta (&pu8DstCoords, pt1.x, m_savedOrders.line.start.x, (uint8_t *)present.pu16, RDP_PRESENT_LINE_X1);
            fDelta = voEncodeCoordDelta (&pu8DstCoords, pt1.y, m_savedOrders.line.start.y, (uint8_t *)present.pu16, RDP_PRESENT_LINE_Y1) && fDelta;
            fDelta = voEncodeCoordDelta (&pu8DstCoords, pt2.x, m_savedOrders.line.end.x,   (uint8_t *)present.pu16, RDP_PRESENT_LINE_X2) && fDelta;
            fDelta = voEncodeCoordDelta (&pu8DstCoords, pt2.y, m_savedOrders.line.end.y,   (uint8_t *)present.pu16, RDP_PRESENT_LINE_Y2) && fDelta;

            m_savedOrders.line.start = pt1;
            m_savedOrders.line.end = pt2;

            if (fDelta)
            {
                *pu8OrderFlags |= RDP_ORDER_DELTA;
                pu8Dst = pu8DstCoords;
            }
            else
            {
                uint16_t *pu16Dst = (uint16_t *)pu8Dst;

                if (*present.pu16 & RDP_PRESENT_LINE_X1)
                {
                    *pu16Dst++ = pt1.x;
                }

                if (*present.pu16 & RDP_PRESENT_LINE_Y1)
                {
                    *pu16Dst++ = pt1.y;
                }

                if (*present.pu16 & RDP_PRESENT_LINE_X2)
                {
                    *pu16Dst++ = pt2.x;
                }

                if (*present.pu16 & RDP_PRESENT_LINE_Y2)
                {
                    *pu16Dst++ = pt2.y;
                }

                pu8Dst = (uint8_t *)pu16Dst;
            }

            if (m_savedOrders.line.mix != pOrder->mix)
            {
                *pu8Dst++ = m_savedOrders.line.mix = pOrder->mix;
                *present.pu16 |= RDP_PRESENT_LINE_MIX;
            }

            if (m_savedOrders.line.width != 1)
            {
                *pu8Dst++ = m_savedOrders.line.width = 1;
                *present.pu16 |= RDP_PRESENT_LINE_WIDTH;
            }

            ConvertColors ((uint8_t *)&pOrder->rgb, 32, 1,
                           (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));

            pu8Dst = voEncodeClrP16 (u32ClrDst, pu8Dst, present.pu16, &m_savedOrders.line.clr, RDP_PRESENT_LINE_CLR, m_bpp);

            pu8Dst = voAdjust2BytesPresent (present.pu16, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDE_ORDER_POLYLINE:
        {
            VRDEORDERPOLYLINE *pOrder = (VRDEORDERPOLYLINE *)pvOrder;

            /* RDP_ORDER_LINE */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_POLYLINE start, %d,%d bounds %d mix 0x%02X rgb %08X nPoints %d\n",
                       pOrder->ptStart.x, pOrder->ptStart.y,
                       m_savedOrders.fBounds,
                       pOrder->mix, pOrder->rgb,
                       pOrder->points.c));

            VRDEORDERPOINT pt;
            voAdjustPoint (this, uScreenId, &pt, pOrder->ptStart.x, pOrder->ptStart.y);

            voRemoveLastZeroLengthLine(&pOrder->points, &pt);

            if (pOrder->points.c == 0)
            {
                VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_POLYLINE no points!!!\n"));
                break;
            }

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order,
                                          VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.polyline));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_POLYLINE);

            /* The order has a 8 bit present field. */
            VRDP_INIT_PRESENT_1_BYTE();

            VRDP_ENCODE_BOUNDS();

            uint8_t *pu8DstCoords = pu8Dst;

            bool fDelta = voEncodeCoordDelta (&pu8DstCoords, pt.x, m_savedOrders.polyline.start.x, present.pu8, RDP_PRESENT_POLYLINE_X);
            fDelta = voEncodeCoordDelta (&pu8DstCoords, pt.y, m_savedOrders.polyline.start.y, present.pu8, RDP_PRESENT_POLYLINE_Y) && fDelta;

            m_savedOrders.polyline.start = pt;

            if (fDelta)
            {
                *pu8OrderFlags |= RDP_ORDER_DELTA;
                pu8Dst = pu8DstCoords;
            }
            else
            {
                uint16_t *pu16Dst = (uint16_t *)pu8Dst;

                if (*present.pu8 & RDP_PRESENT_POLYLINE_X)
                {
                    *pu16Dst++ = pt.x;
                }

                if (*present.pu8 & RDP_PRESENT_POLYLINE_Y)
                {
                    *pu16Dst++ = pt.y;
                }

                pu8Dst = (uint8_t *)pu16Dst;
            }

            if (m_savedOrders.polyline.mix != pOrder->mix)
            {
                *pu8Dst++ = m_savedOrders.polyline.mix = pOrder->mix;
                *present.pu8 |= RDP_PRESENT_POLYLINE_MIX;
            }

            ConvertColors ((uint8_t *)&pOrder->rgb, 32, 1,
                           (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));

            pu8Dst = voEncodeClrP8 (u32ClrDst, pu8Dst, present.pu8, &m_savedOrders.polyline.clr, RDP_PRESENT_POLYLINE_CLR, m_bpp);

            if (m_savedOrders.polyline.lines != pOrder->points.c)
            {
                *pu8Dst++ = m_savedOrders.polyline.lines = pOrder->points.c;
                *present.pu8 |= RDP_PRESENT_POLYLINE_LINES;
            }

            /* Encode points data. */
            pu8Dst = voEncodePolyPoints (&pt, &pOrder->points, pu8Dst, &present, &m_savedOrders.polyline.data, RDP_PRESENT_POLYLINE_DATA);

            pu8Dst = voAdjust1BytePresent (present.pu8, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDE_ORDER_ELLIPSE:
        {
            VRDEORDERELLIPSE *pOrder = (VRDEORDERELLIPSE *)pvOrder;

            /* RDP_ORDER_ELLIPSE */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_ELLIPSE %d,%d %d,%d bounds %d mix 0x%02X fillMode %d rgb %08X\n",
                       pOrder->pt1.x, pOrder->pt1.y,
                       pOrder->pt2.x, pOrder->pt2.y,
                       m_savedOrders.fBounds,
                       pOrder->mix, pOrder->fillMode, pOrder->rgb));

            VRDEORDERPOINT pt1;
            VRDEORDERPOINT pt2;
            voAdjustPoint (this, uScreenId, &pt1, pOrder->pt1.x, pOrder->pt1.y);
            voAdjustPoint (this, uScreenId, &pt2, pOrder->pt2.x, pOrder->pt2.y);

            /* Reserve enough space for an order with complete information. */
            pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order,
                                          VRDP_PRIMARY_ORDER_HDR_SIZE + sizeof (m_savedOrders.ellipse));

            if (!pBlock)
            {
                break;
            }

            VRDP_START_ORDER(RDP_ORDER_TYPE_ELLIPSE);

            /* The order has a 8 bit present field. */
            VRDP_INIT_PRESENT_1_BYTE();

            VRDP_ENCODE_BOUNDS();

            uint8_t *pu8DstCoords = pu8Dst;

            bool fDelta = voEncodeCoordDelta (&pu8DstCoords, pt1.x, m_savedOrders.ellipse.pt1.x, present.pu8, RDP_PRESENT_ELLIPSE_X1);
            fDelta =      voEncodeCoordDelta (&pu8DstCoords, pt1.y, m_savedOrders.ellipse.pt1.y, present.pu8, RDP_PRESENT_ELLIPSE_Y1) && fDelta;
            fDelta =      voEncodeCoordDelta (&pu8DstCoords, pt2.x, m_savedOrders.ellipse.pt2.x, present.pu8, RDP_PRESENT_ELLIPSE_X2) && fDelta;
            fDelta =      voEncodeCoordDelta (&pu8DstCoords, pt2.y, m_savedOrders.ellipse.pt2.y, present.pu8, RDP_PRESENT_ELLIPSE_Y2) && fDelta;

            m_savedOrders.ellipse.pt1 = pt1;
            m_savedOrders.ellipse.pt2 = pt2;

            if (fDelta)
            {
                *pu8OrderFlags |= RDP_ORDER_DELTA;
                pu8Dst = pu8DstCoords;
            }
            else
            {
                uint16_t *pu16Dst = (uint16_t *)pu8Dst;

                if (*present.pu8 & RDP_PRESENT_ELLIPSE_X1)
                {
                    *pu16Dst++ = pt1.x;
                }

                if (*present.pu8 & RDP_PRESENT_ELLIPSE_Y1)
                {
                    *pu16Dst++ = pt1.y;
                }

                if (*present.pu8 & RDP_PRESENT_ELLIPSE_X2)
                {
                    *pu16Dst++ = pt2.x;
                }

                if (*present.pu8 & RDP_PRESENT_ELLIPSE_Y2)
                {
                    *pu16Dst++ = pt2.y;
                }

                pu8Dst = (uint8_t *)pu16Dst;
            }

            if (m_savedOrders.ellipse.mix != pOrder->mix)
            {
                *pu8Dst++ = m_savedOrders.ellipse.mix = pOrder->mix;
                *present.pu8 |= RDP_PRESENT_ELLIPSE_MIX;
            }

            if (m_savedOrders.ellipse.fillMode != pOrder->fillMode)
            {
                *pu8Dst++ = m_savedOrders.ellipse.fillMode = pOrder->fillMode;
                *present.pu8 |= RDP_PRESENT_ELLIPSE_FILLMODE;
            }

            ConvertColors ((uint8_t *)&pOrder->rgb, 32, 1,
                           (uint8_t *)&u32ClrDst, m_bpp, sizeof (u32ClrDst));

            pu8Dst = voEncodeClrP8 (u32ClrDst, pu8Dst, present.pu8, &m_savedOrders.ellipse.clr, RDP_PRESENT_ELLIPSE_CLR, m_bpp);

            pu8Dst = voAdjust1BytePresent (present.pu8, pu8Dst, pu8OrderFlags);

            /* Complete the block. Pass how many bytes were actually written. */
            VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
        } break;

        case VRDE_ORDER_BOUNDS:
        {
            VRDEORDERBOUNDS *pOrder = (VRDEORDERBOUNDS *)pvOrder;

            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_BOUNDS %d,%d %d,%d\n",
                       pOrder->pt1.x, pOrder->pt1.y, pOrder->pt2.x, pOrder->pt2.y));

            VRDEORDERAREA areaBounds;
            voAdjustArea (uScreenId, this, &areaBounds,
                          pOrder->pt1.x,
                          pOrder->pt1.y,
                          pOrder->pt2.x - pOrder->pt1.x,
                          pOrder->pt2.y - pOrder->pt1.y);

            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_BOUNDS adjusted %d,%d %d,%d\n",
                       areaBounds.x, areaBounds.y, areaBounds.w, areaBounds.h));

            m_savedOrders.boundsCurrent.pt1.x = areaBounds.x;
            m_savedOrders.boundsCurrent.pt1.y = areaBounds.y;
            m_savedOrders.boundsCurrent.pt2.x = areaBounds.x + areaBounds.w;
            m_savedOrders.boundsCurrent.pt2.y = areaBounds.y + areaBounds.h;

            fBounds = true;
        } break;

        case VRDE_ORDER_REPEAT:
        {
            VRDEORDERREPEAT *pOrder = (VRDEORDERREPEAT *)pvOrder;

            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_REPEAT %d,%d %d,%d\n",
                       pOrder->bounds.pt1.x, pOrder->bounds.pt1.y, pOrder->bounds.pt2.x, pOrder->bounds.pt2.y));

            if (m_pSavedInternalOrders)
            {
                /* There was a sequence of internal orders. Replay it. */

                /* Orders will have bounds. */
                Assert (m_savedOrders.fBounds == false);

                VRDEORDERAREA areaBounds;
                voAdjustArea (uScreenId, this, &areaBounds,
                              pOrder->bounds.pt1.x,
                              pOrder->bounds.pt1.y,
                              pOrder->bounds.pt2.x - pOrder->bounds.pt1.x,
                              pOrder->bounds.pt2.y - pOrder->bounds.pt1.y);

                VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_REPEAT saved orders adjusted %d,%d %dx%d\n",
                           areaBounds.x, areaBounds.y, areaBounds.w, areaBounds.h));

                m_savedOrders.boundsCurrent.pt1.x = areaBounds.x;
                m_savedOrders.boundsCurrent.pt1.y = areaBounds.y;
                m_savedOrders.boundsCurrent.pt2.x = areaBounds.x + areaBounds.w;
                m_savedOrders.boundsCurrent.pt2.y = areaBounds.y + areaBounds.h;

                m_savedOrders.fBounds = true;

                VRDPSAVEDINTERNALORDER *pSavedOrder = m_pSavedInternalOrders;

                while (pSavedOrder)
                {
                    /* Directly call corresponding order function. */
                    switch (pSavedOrder->i32Op)
                    {
                        case VRDP_ORDER_INTERNALTEXT2:
                        {
                            VRDPORDERINTERNALTEXT2 *pOrder2 = (VRDPORDERINTERNALTEXT2 *)&pSavedOrder[1];
                            outText2 (uScreenId,
                                      pStream,
                                      pOrder2->u8RDPFontHandle,
                                      pOrder2->u8Flags,
                                      pOrder2->u8CharInc,
                                      pOrder2->rgbFG,
                                      pOrder2->rgbBG,
                                      &pOrder2->bkground,
                                      &pOrder2->opaque,
                                      &pOrder2->origin,
                                      pOrder2->u8TextLength,
                                      pOrder2->au8Text);
                        } break;

                        default:
                            AssertFailed ();
                    }

                    pSavedOrder = pSavedOrder->next;
                }

                m_savedOrders.fBounds = false;
            }
            else
            {
                 /* Generate a simple REPEAT order. */
                /* Reserve enough space for an order with complete information. */
                pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order,
                                              VRDP_PRIMARY_ORDER_HDR_SIZE);

                if (!pBlock)
                {
                    break;
                }

                pu8Dst = pBlock->pu8DstStart;

                pu8OrderFlags = pu8Dst++;

                /* Set proper orders flags: no change and bounds and all zero present bytes. */
                *pu8OrderFlags = RDP_ORDER_STANDARD | RDP_ORDER_BOUNDS;

                switch (m_savedOrders.u8LastOrder)
                {
                    /** @todo use array uint8_t cbPresent = au8PresentBytes[m_savedOrders.u8LastOrder],
                     *       RDP_ORDER_SMALL/_TINY are number of null present bytes.
                     */
                    /* Orders those have 1 bytes present field. */
                    case RDP_ORDER_TYPE_RECT:
                    case RDP_ORDER_TYPE_DESTBLT:
                    case RDP_ORDER_TYPE_SCREENBLT:
                    case RDP_ORDER_TYPE_POLYLINE:
                    case RDP_ORDER_TYPE_ELLIPSE:
                    {
                        *pu8OrderFlags |= RDP_ORDER_SMALL;
                    } break;

                    /* Orders those have 2 bytes present field. */
                    case RDP_ORDER_TYPE_MEMBLT:
                    case RDP_ORDER_TYPE_PATBLT:
                    case RDP_ORDER_TYPE_LINE:
                    {
                        *pu8OrderFlags |= RDP_ORDER_TINY;
                    } break;

                    /* Orders those have 3 bytes present field. */
                    case RDP_ORDER_TYPE_TEXT2:
                    {
                        *pu8OrderFlags |= (RDP_ORDER_SMALL | RDP_ORDER_TINY);
                    } break;

                    default:
                        AssertFailed();
                }

                /* Encode the bounds. */
                VRDEORDERBOUNDS bounds;

                VRDEORDERAREA areaBounds;
                voAdjustArea (uScreenId, this, &areaBounds,
                              pOrder->bounds.pt1.x,
                              pOrder->bounds.pt1.y,
                              pOrder->bounds.pt2.x - pOrder->bounds.pt1.x,
                              pOrder->bounds.pt2.y - pOrder->bounds.pt1.y);

                VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_REPEAT adjusted %d,%d %dx%d\n",
                           areaBounds.x, areaBounds.y, areaBounds.w, areaBounds.h));

                bounds.pt1.x = areaBounds.x;
                bounds.pt1.y = areaBounds.y;
                bounds.pt2.x = areaBounds.x + areaBounds.w;
                bounds.pt2.y = areaBounds.y + areaBounds.h;

                pu8Dst = voEncodeBounds (pu8Dst, &bounds,
                                         pu8OrderFlags, &m_savedOrders.boundsPrevious);

                /* Complete the block. Pass how many bytes were actually written. */
                VRDPSTREAM.EndBlock (pBlock, pu8Dst - pBlock->pu8DstStart);
            }
        } break;

        case VRDE_ORDER_SAVESCREEN:
        {
            VRDEORDERSAVESCREEN *pOrder = (VRDEORDERSAVESCREEN *)pvOrder;

            /* RDP_ORDER_DESKSAVE */
            VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SAVESCREEN %d,%d %d,%d ident %d restore %d\n",
                       pOrder->pt1.x, pOrder->pt1.y,
                       pOrder->pt2.x, pOrder->pt2.y,
                       pOrder->ident, pOrder->restore));

            voAdjustArea (uScreenId, this, &area,
                          pOrder->pt1.x,
                          pOrder->pt1.y,
                          pOrder->pt2.x - pOrder->pt1.x,
                          pOrder->pt2.y - pOrder->pt1.y);

            if (pOrder->restore)
            {
                if (pOrder->ident == 0)
                {
                    VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SAVESCREEN invalid ident!!!\n"));
                    break;
                }

                if (m_savedOrders.cLocalSlotsUsed > pOrder->ident)
                {
                    VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SAVESCREEN LocalSlotsUsed %d ident %d!!!\n",
                               m_savedOrders.cLocalSlotsUsed, pOrder->ident));

                    while (m_savedOrders.cLocalSlotsUsed > pOrder->ident)
                    {
                        m_savedOrders.cRemotePixelsUsed -= m_savedOrders.aRemoteScreenCacheInfo[--m_savedOrders.cLocalSlotsUsed].size;
                    }
                }

                if (m_savedOrders.cLocalSlotsUsed == pOrder->ident)
                {
                    m_savedOrders.cRemotePixelsUsed -= m_savedOrders.aRemoteScreenCacheInfo[--m_savedOrders.cLocalSlotsUsed].size;

                    VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SAVESCREEN LocalSlotUsed %d RemotePixelUsed %d\n",
                               m_savedOrders.cLocalSlotsUsed, m_savedOrders.cRemotePixelsUsed));

                    outSaveScreenOrder (pStream, &area, pOrder->restore, m_savedOrders.cRemotePixelsUsed);
                }
                else
                {
                    VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SAVESCREEN Restoring bits LocalSlotUsed %d ident %d\n",
                               m_savedOrders.cLocalSlotsUsed, pOrder->ident));

                    VRDEDATABITS *pBitsHdr = (VRDEDATABITS *)((uint8_t *)pOrder + sizeof(VRDEORDERSAVESCREEN));
                    uint8_t *pu8Bits       = (uint8_t *)pBitsHdr + sizeof(VRDEDATABITS);

                    VRDPBitmapCompressed bmp;

                    bmp.Compress (pu8Bits, pBitsHdr->cbPixel * pBitsHdr->cWidth, pBitsHdr->cWidth, pBitsHdr->cHeight, pBitsHdr->cbPixel * 8, 0);

                    OutputBitmap2 (uScreenId, pStream, &bmp, area.x, area.y);
                }
            }
            else
            {
                if (pOrder->ident == 0 || pOrder->ident > RT_ELEMENTS(m_savedOrders.aRemoteScreenCacheInfo))
                {
                    VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SAVESCREEN invalid ident!!!\n"));
                    break;
                }

                if (m_savedOrders.cLocalSlotsUsed >= pOrder->ident)
                {
                    VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SAVESCREEN LocalSlotsUsed %d ident %d in SAVE!!!\n",
                               m_savedOrders.cLocalSlotsUsed, pOrder->ident));

                    while (m_savedOrders.cLocalSlotsUsed >= pOrder->ident)
                    {
                        m_savedOrders.cRemotePixelsUsed -= m_savedOrders.aRemoteScreenCacheInfo[--m_savedOrders.cLocalSlotsUsed].size;
                    }
                }

                /* Saving screen bits. */
                uint32_t cRemotePixels = voCalcRemoteScreenPixels (area.w, area.h);

                if (cRemotePixels + m_savedOrders.cRemotePixelsUsed > m_caps.order.u32DesktopCacheSize)
                {
                    VRDPTPLOG(("VRDPTP::OutputOrder: VRDE_ORDER_SAVESCREEN not enough space in remote cache: need %d, already used %d, size %d!!!\n",
                               cRemotePixels, m_savedOrders.cRemotePixelsUsed, m_caps.order.u32DesktopCacheSize));
                    break;
                }

                if (RT_SUCCESS(outSaveScreenOrder (pStream, &area, pOrder->restore, m_savedOrders.cRemotePixelsUsed)))
                {
                    m_savedOrders.aRemoteScreenCacheInfo[pOrder->ident - 1].offset = m_savedOrders.cRemotePixelsUsed;
                    m_savedOrders.aRemoteScreenCacheInfo[pOrder->ident - 1].size = cRemotePixels;
                    m_savedOrders.cRemotePixelsUsed += cRemotePixels;
                    m_savedOrders.cLocalSlotsUsed = pOrder->ident;
                }
            }

        } break;

        default:
        {
            rc = VERR_NOT_SUPPORTED;
        }
    }

    if (m_fSeqInProcess == false)
    {
        /* Update the bounds only if there is no sequence of internal orders in process. */
        m_savedOrders.fBounds = fBounds;

        /* Check if the saved internal orders list must be cleared. */
        if (   i32Op != VRDE_ORDER_REPEAT
            && i32Op != VRDP_ORDER_INTERNALSEQEND)
        {
            /* A new order which is not a REPEAT and not the SEQEND arrived.
             * Clear the saved internal orders.
             */
            VRDPSAVEDINTERNALORDER *pSavedOrder = m_pSavedInternalOrders;

            m_pSavedInternalOrders = NULL;

            while (pSavedOrder)
            {
                VRDPSAVEDINTERNALORDER *next = pSavedOrder->next;

                VRDPMemFree (pSavedOrder);

                pSavedOrder = next;
            }
        }
    }
    else
    {
        AssertMsg (i32Op < 0, ("Only internal orders allowed: %d\n", i32Op));

        if (i32Op != VRDP_ORDER_INTERNALSEQBEGIN)
        {
            /* Save the internal order. */
            VRDPSAVEDINTERNALORDER *pSavedOrder = (VRDPSAVEDINTERNALORDER *)VRDPMemAlloc (sizeof (VRDPSAVEDINTERNALORDER) + cbInternalOrder);

            if (pSavedOrder)
            {
                pSavedOrder->i32Op = i32Op;
                pSavedOrder->cbInternalOrder = cbInternalOrder;

                if (cbInternalOrder != 0)
                {
                    memcpy (&pSavedOrder[1], pvOrder, cbInternalOrder);
                }

                pSavedOrder->next = m_pSavedInternalOrders;
                m_pSavedInternalOrders = pSavedOrder;
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }
        }
    }

    VRDPTPLOG(("VRDPTP::OutputOrder: rc = %Rrc\n", rc));

    return rc;
}

#pragma pack(1)
typedef struct _RDPSecondaryOrderHdr
{
    uint8_t  u8Flags;
    uint16_t u16Length;
    uint16_t u16SecondaryFlags;
} RDPSecondaryOrderHdr;

/* Note: the order can be encoded in a lesser number of bytes.
 * But here a fixed size structure declared as this simplifies
 * the code: the value of the compressed tile size is not required,
 * and the tile can be written at a fixed offset in the memory block.
 */

typedef struct _RDPBitmapCache
{
    RDPSecondaryOrderHdr hdr;

    uint8_t  u8Order;

    /* Actually 'width' and 'height' are 2 byte fields. But since the
     * maximum tile size is < 64, the 2 byte field is encoded as 1 byte,
     * high bit is zero.
     */
    uint8_t  u8Width;
    uint8_t  u8Height;

    /* The 'cbBits' is actually 4 bytes field. But rdesktop treats it as
     * 2 bytes big endian.
     */
    uint16_t u16BitsSize;

    /* The cache idx is 2 bytes field.
     */
    uint16_t u16Idx;
} RDPBitmapCache;
#pragma pack()

bool VRDPTP::OutputBitmapCache (VRDPStream *pStream,
                                VRDPBitmapCompressed *pBmp,
                                uint16_t u16TileX,
                                uint16_t u16TileY,
                                uint16_t u16CacheId,
                                uint16_t u16Idx)
{
    const VRDPStream::MemoryBlock *pBlock;

    /*
     * BMPCACHE2 order.
     */

    /* Reserve enough space for an order with complete information and largest possible bitmap.
     * The space, reserved for the bitmap data, should be able to hold entire uncompressed
     * bitmap 64x64 at 4 BPP. The pBmp->Output method will try to write the bitmap
     * so it does not exceed the VRDP_MAX_BMP_NETWORK_SIZE.
     */
    pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order,
                                    sizeof (RDPBitmapCache) +
                                    VRDP_NETWORK_PACKET_SIZE_ALLOC);

    if (pBlock == NULL)
    {
        return false;
    }

    const VRDPBitmapCompressedTileDescr *ptd;
    bool fCompressed;
    uint32_t cbSize;

    bool fBitmapProcessed = pBmp->Output (u16TileX, u16TileY, m_fBitmapCompression,
                                          m_bpp,
                                          pBlock->pu8DstStart + sizeof (RDPBitmapCache),
                                          VRDP_NETWORK_PACKET_SIZE_ALLOC,
                                          &ptd, &cbSize, &fCompressed);

    if (!fBitmapProcessed)
    {
        VRDPSTREAM.EndBlock (pBlock, 0);
        return false;
    }

#ifdef VRDP_REPLACE_CACHED_BITMAPS
    /* Replace the bitmap with a single color compressed RLE. */
    if (!fCompressed)
    {
        uint8_t *pu8Dst = pBlock->pu8DstStart + sizeof (RDPBitmapCache);
        *pu8Dst++ = 0xF3;
        *pu8Dst++ = (uint8_t)(ptd->cBitmapWidth * ptd->cHeight);
        *pu8Dst++ = (uint8_t)((ptd->cBitmapWidth * ptd->cHeight) >> 8);
        switch (m_bpp)
        {
            case 8:
               *pu8Dst++ = 0x02;
               break;
            case 16:
               *pu8Dst++ = 0x1f;
               *pu8Dst++ = 0x00;
               break;
            case 24:
               *pu8Dst++ = 0x00;
               *pu8Dst++ = 0x00;
               *pu8Dst++ = 0xFF;
               break;
            default:
               AssertFailed ();
        }
        cbSize = pu8Dst - (pBlock->pu8DstStart + sizeof (RDPBitmapCache));
        fCompressed = true;
    }
#endif /* VRDP_REPLACE_CACHED_BITMAPS */

    RDPBitmapCache *p = (RDPBitmapCache *)pBlock->pu8DstStart;

    p->hdr.u8Flags           = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
    p->hdr.u16Length         = (uint16_t)(sizeof (RDPBitmapCache) + cbSize - 6 - 7);
    p->hdr.u16SecondaryFlags = (((m_bpp / 8) + 2) << BMPCACHE2_MODE_SHIFT) | u16CacheId;

    p->hdr.u16SecondaryFlags |= BMPCACHE2_FLAG;

    p->u8Order = fCompressed? RDP_ORDER_BMPCACHE2: RDP_ORDER_RAW_BMPCACHE2;

    /* Actually 'width' and 'height' are 2 byte fields. But since the
     * maximum tile size is < 64, the 2 byte field is encoded as 1 byte,
     * high bit is zero.
     */
    p->u8Width  = (uint8_t)ptd->cBitmapWidth;
    p->u8Height = (uint8_t)ptd->cHeight;

    /* The 'cbBits' is actually 4 bytes field. But rdesktop treats it as
     * 2 bytes big endian.
     */
    p->u16BitsSize = RT_H2BE_U16(cbSize | 0x4000);

    /* The cache idx is 2 bytes field.
     */
    p->u16Idx = RT_H2BE_U16(u16Idx | 0x8000);

    VRDPTPLOG(("RDP_ORDER_BMPCACHE2: %dx%d id %d, idx %d\n", p->u8Width, p->u8Height, u16CacheId, u16Idx));

    /* Complete the block. Pass how many bytes were actually written. */
    VRDPSTREAM.EndBlock (pBlock, sizeof (RDPBitmapCache) + cbSize);

    return true;
}

#pragma pack(1)
typedef struct _RDPBrushCache
{
    RDPSecondaryOrderHdr hdr;

    uint8_t u8Order;

    uint8_t u8CacheEntry;
    uint8_t u8BitmapFormat;
    uint8_t u8Width;
    uint8_t u8Height;
    uint8_t u8Style;
    uint8_t u8Bytes;
    uint8_t au8Pattern[8];
} RDPBrushCache;
#pragma pack()

bool VRDPTP::OutputBrushCache (VRDPStream *pStream,
                               uint8_t u8CacheEntry,
                               uint8_t u8Width,
                               uint8_t u8Height,
                               uint8_t *au8Pattern,
                               size_t cbPattern)
{
    RT_NOREF2(u8Width, u8Height);
    Assert(cbPattern == 8 && u8Width == 8 && u8Height == 8); /* Only support 1BPP 8x8 brushes. */

    const VRDPStream::MemoryBlock *pBlock;

    /*
     * BRUSHCACHE order.
     */

    pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order,
                                    sizeof (RDPBrushCache));

    if (pBlock == NULL)
    {
        return false;
    }

    RDPBrushCache *p = (RDPBrushCache *)pBlock->pu8DstStart;

    p->hdr.u8Flags           = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
    p->hdr.u16Length         = (uint16_t)(sizeof (RDPBrushCache) - 6 - 7);
    p->hdr.u16SecondaryFlags = 0;

    p->u8Order = RDP_ORDER_BRUSHCACHE;

    p->u8CacheEntry = u8CacheEntry;
    p->u8BitmapFormat = RDP_BRUSH_FMT_1BPP;
    p->u8Width = 8;
    p->u8Height = 8;
    p->u8Style = 0;
    p->u8Bytes = (uint8_t)cbPattern;
    /* The rows are encoded in reverse order. */
    p->au8Pattern[0] = au8Pattern[7];
    p->au8Pattern[1] = au8Pattern[6];
    p->au8Pattern[2] = au8Pattern[5];
    p->au8Pattern[3] = au8Pattern[4];
    p->au8Pattern[4] = au8Pattern[3];
    p->au8Pattern[5] = au8Pattern[2];
    p->au8Pattern[6] = au8Pattern[1];
    p->au8Pattern[7] = au8Pattern[0];

    /* Complete the block. Pass how many bytes were actually written. */
    VRDPSTREAM.EndBlock (pBlock, sizeof (RDPBrushCache));

    return true;
}


/*
 *
 */
#pragma pack(1)
typedef struct _RDPFontCache
{
    RDPSecondaryOrderHdr hdr;

    uint8_t u8Order;

    uint8_t u8FontHandle;
    uint8_t u8Glyphs;

    /* 'u8Glyphs' of 'RDPFontCacheGlyph' structures follow. */
} RDPFontCache;

typedef struct _RDPFontCacheGlyph
{
    uint16_t index;
    uint16_t offset;
    uint16_t baseline;
    uint16_t w;
    uint16_t h;

    /* 1BPP. Size is (h * ((w + 7) / 8) + 3) & ~3 */
    uint8_t *au8Bitmap[1];
} RDPFontCacheGlyph;
#pragma pack()

bool VRDPTP::OutputFontCache (VRDPStream *pStream, const TCFONTTEXT2 *pFontText2, const uint8_t *pau8Indexes, int cIndexes)
{
    const VRDPStream::MemoryBlock *pBlock;

    Assert (cIndexes > 0);

    VRDPTPLOG(("VRDPTP::OutputFontCache: %d indexes\n", cIndexes));

    /*
     * FONTCACHE order.
     */
    const uint8_t *pu8Index = &pau8Indexes[0];
    int i = 0;

    while (i < cIndexes)
    {
        /* How many bytes were put to the block. */
        unsigned cbBlock = 0;

        /* Reserve maximum possible space for an order to fit to the network packet. */
        pBlock = VRDPSTREAM.BeginBlock (VRDPUpdate_Order, VRDP_NETWORK_PACKET_SIZE);

        if (pBlock == NULL)
        {
            return false;
        }

        RDPFontCache *pFontCache = (RDPFontCache *)pBlock->pu8DstStart;

        pFontCache->u8Order      = RDP_ORDER_FONTCACHE;
        pFontCache->u8FontHandle = pFontText2->u8RDPFontHandle;
        pFontCache->u8Glyphs     = 0;

        cbBlock += sizeof (RDPFontCache);

        /* Glyphs follow after the order header. */

        while (i < cIndexes)
        {
            RDPFontCacheGlyph *pFontCacheGlyph = (RDPFontCacheGlyph *)(pBlock->pu8DstStart + cbBlock);

            const TCGLYPH *pGlyph = &pFontText2->aGlyphs[*pu8Index];

            int cbBitmap = (pGlyph->pCachedGlyph->h * ((pGlyph->pCachedGlyph->w + 7) / 8) + 3) & ~3;

            int cbGlyph = sizeof (RDPFontCacheGlyph) - sizeof (pFontCacheGlyph->au8Bitmap) + cbBitmap;

            cbBlock += cbGlyph;

            if (cbBlock > VRDP_NETWORK_PACKET_SIZE)
            {
                /* Not enough place for the new glyph. Output the order. */
                cbBlock -= cbGlyph;
                break;
            }

            const TCCACHEDGLYPH *pCachedGlyph = pGlyph->pCachedGlyph;


            pFontCacheGlyph->index    = pCachedGlyph->u8Index;
            pFontCacheGlyph->offset   = pCachedGlyph->xOrigin;
            pFontCacheGlyph->baseline = pCachedGlyph->yOrigin;
            pFontCacheGlyph->w        = pCachedGlyph->w;
            pFontCacheGlyph->h        = pCachedGlyph->h;

            memcpy (pFontCacheGlyph->au8Bitmap, pCachedGlyph->pu8Bitmap, cbBitmap);

            pFontCache->u8Glyphs++;
            pu8Index++;
            i++;
        }

        Assert (pFontCache->u8Glyphs != 0);

        pFontCache->hdr.u8Flags           = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
        pFontCache->hdr.u16Length         = cbBlock - 6 - 7;
        pFontCache->hdr.u16SecondaryFlags = 0;

        /* Complete the block. Pass how many bytes were actually written. */
        VRDPSTREAM.EndBlock (pBlock, cbBlock);
    }

    return true;
}

#pragma pack(1)
typedef struct RDPAlternateSecondaryOrderHdr
{
    uint8_t  u8ControlFlags; /* 2 low bits == RDP_ORDER_SECONDARY, 6 high bits == orderType. */
} RDPAlternateSecondaryOrderHdr;

typedef struct RDPFrameMarker
{
    RDPAlternateSecondaryOrderHdr hdr;

    uint32_t u32Action;
} RDPFrameMarker;
#pragma pack()

#define TS_ALTSEC_FRAME_MARKER 0x0D

bool VRDPTP::OutputFrameMarker(VRDPStream *pStream,
                               uint32_t u32Action)
{
    const VRDPStream::MemoryBlock *pBlock;

    /*
     * Frame Marker order.
     */
    VRDPTPLOG(("VRDPTP::OutputFrameMarker: %d\n", u32Action));

    pBlock = VRDPSTREAM.BeginBlock(VRDPUpdate_Order,
                                   sizeof(RDPFrameMarker));

    if (pBlock == NULL)
    {
        return false;
    }

    RDPFrameMarker *p = (RDPFrameMarker *)pBlock->pu8DstStart;

    p->hdr.u8ControlFlags = RDP_ORDER_SECONDARY | (TS_ALTSEC_FRAME_MARKER << 2);

    p->u32Action = u32Action;

    /* Complete the block. Pass how many bytes were actually written. */
    VRDPSTREAM.EndBlock(pBlock, sizeof(RDPFrameMarker));

    return true;
}


#undef VRDP_SET_ORDER
#undef VRDP_INIT_PRESENT_1_BYTE
#undef VRDP_INIT_PRESENT_2_BYTES
#undef VRDP_ENCODE_BOUNDS

void VRDPTP::ResetSavedOrders (void)
{
    memset (&m_savedOrders, 0, sizeof (m_savedOrders));
    m_savedOrders.u8LastOrder = RDP_ORDER_TYPE_PATBLT;

    /* VRDP server uses exclusive bounds internally.*/
    m_savedOrders.boundsPrevious.pt2.x = 1;
    m_savedOrders.boundsPrevious.pt2.y = 1;
}

uint32_t VRDPTP::GetSupportedOrders (void)
{
    /* Some internal orders are always supported. */
    uint32_t u32 = RT_BIT_32(VRDE_ORDER_DIRTY_RECT);
    u32 |= RT_BIT_32(VRDE_ORDER_BOUNDS);
    u32 |= RT_BIT_32(VRDE_ORDER_REPEAT);

    if (   m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_DESTBLT]
        || m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_MULTI_DESTBLT])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_DSTBLT);
    }

    if (   m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_PATBLT]
        || m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_MULTI_PATBLT])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_SOLIDBLT);
        u32 |= RT_BIT_32(VRDE_ORDER_PATBLTBRUSH);
    }

    if (   m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_SCREENBLT]
        || m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_MULTI_SCREENBLT])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_SCREENBLT);
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_LINE])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_LINE);
    }

    /* Note: MS RDP Client does not report RDP_ORDER_NEG_INDEX_RECT as supported
     * and reports RDP_ORDER_NEG_INDEX_MULTI_RECT, but actually processes the simple
     * RECT order too.
     */
    if (   m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_RECT]
        || m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_MULTI_RECT])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_SOLIDRECT);
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_DESKSAVE])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_SAVESCREEN);
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_MEMBLT])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_MEMBLT);
        u32 |= RT_BIT_32(VRDE_ORDER_CACHED_BITMAP);
        u32 |= RT_BIT_32(VRDE_ORDER_DELETED_BITMAP);
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_TRIBLT])
    {
        /* not yet supported u32 |= RT_BIT_32(VRDE_ORDER_TRIBLT); */
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_POLYGON])
    {
        /* not yet supported u32 |= RT_BIT_32(VRDE_ORDER_POLYGON); */
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_POLYGON2])
    {
        /* not yet supported u32 |= RT_BIT_32(VRDE_ORDER_POLYGON2); */
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_POLYLINE])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_POLYLINE);
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_ELLIPSE])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_ELLIPSE);
    }

    if (m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_ELLIPSE2])
    {
        /* not yet supported u32 |= RT_BIT_32(VRDE_ORDER_ELLIPSE2); */
    }

    /* Note: MS RDP Client does not report RDP_ORDER_NEG_INDEX_TEXT2 as supported
     * and reports RDP_ORDER_NEG_INDEX_FAST_INDEX/_GLYPH, but actually processes
     * the old _TEXT2 (_INDEX) order too.
     */
    if (   m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_TEXT2]
        || m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_FAST_INDEX]
        || m_caps.order.au8SupportedOrders[RDP_ORDER_NEG_INDEX_FAST_GLYPH])
    {
        u32 |= RT_BIT_32(VRDE_ORDER_TEXT);
    }

    return u32;
}
