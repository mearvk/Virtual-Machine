/* $Id: orders.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_orders_h
#define VRDP_INCLUDED_SRC_orders_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "bmpcache.h"
#include "textcache.h"

/*
 * The server internal orders.
 */
#define VRDP_ORDER_INTERNALCLS         (-1)
#define VRDP_ORDER_INTERNALTEXT2       (-2)
#define VRDP_ORDER_INTERNALMEMBLT      (-3)
/*                                     (-4)    not used. */
#define VRDP_ORDER_INTERNALCANCEL      (-5)
#define VRDP_ORDER_INTERNALPATBLTCACHED (-6)
/*                                     (-7)    not used. */
#define VRDP_ORDER_INTERNALREDRAW      (-8) /* schedule a redraw. */
#define VRDP_ORDER_INTERNAL_SAVESCREEN_RESTORE (-9) /* Restore a saved screen area. */

/*
 * The situation when an order from the application will result in 2 or more
 * internal orders must be marked as the sequence.
 *
 * The internal order sequence is remembered by the VRDP transport
 * and when a REPEAT order follows the sequence, the entire sequence
 * is replayed.
 *
 * The internal order structures must contain all information
 * required for the order because the structure is saved
 * in memory and the copy is reused in a REPEAT order.
 */
#define VRDP_ORDER_INTERNALSEQBEGIN    (-10)
#define VRDP_ORDER_INTERNALSEQEND      (-11)

typedef struct VRDPORDERINTERNALCLS
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
} VRDPORDERINTERNALCLS;

typedef struct VRDPORDERINTERNALTEXT2
{
    uint8_t        u8RDPFontHandle;
    uint8_t        u8Flags;
    uint8_t        u8CharInc;
    uint32_t       rgbFG;
    uint32_t       rgbBG;
    VRDEORDERRECT  bkground;
    VRDEORDERRECT  opaque;
    VRDEORDERPOINT origin;
    uint8_t        u8TextLength;
    uint8_t        au8Text[VRDP_TC_NUM_GLYPHS];
} VRDPORDERINTERNALTEXT2;

typedef struct VRDPORDERINTERNALMEMBLT
{
    uint16_t u16CacheId;
    uint16_t u16Idx;
    uint16_t u16DstX;
    uint16_t u16DstY;
    uint16_t u16Width;
    uint16_t u16Height;
    uint16_t u16SrcX;
    uint16_t u16SrcY;
    uint8_t u8ROP;
} VRDPORDERINTERNALMEMBLT;

typedef struct VRDPORDERINTERNALREDRAW
{
    uint32_t u32Reserved;
} VRDPORDERINTERNALREDRAW;

/*
 * Structures which can hold a RDP order data.
 * The size of the structures must be greater than the
 * maximum number of bytes required by a order,
 * because the sizeof is used to reserve space in VRDP packet.
 *
 */

#define RDP_PRESENT_BRUSH_X       (0x01)
#define RDP_PRESENT_BRUSH_Y       (0x02)
#define RDP_PRESENT_BRUSH_STYLE   (0x04)
#define RDP_PRESENT_BRUSH_HATCH   (0x08)
#define RDP_PRESENT_BRUSH_PATTERN (0x10)

#define RDP_PRESENT_PATBLT_X      (0x0001)
#define RDP_PRESENT_PATBLT_Y      (0x0002)
#define RDP_PRESENT_PATBLT_CX     (0x0004)
#define RDP_PRESENT_PATBLT_CY     (0x0008)
#define RDP_PRESENT_PATBLT_ROP    (0x0010)
#define RDP_PRESENT_PATBLT_BG     (0x0020)
#define RDP_PRESENT_PATBLT_FG     (0x0040)

#define RDP_PRESENT_RECT_X        (0x01)
#define RDP_PRESENT_RECT_Y        (0x02)
#define RDP_PRESENT_RECT_CX       (0x04)
#define RDP_PRESENT_RECT_CY       (0x08)
#define RDP_PRESENT_RECT_COLOR1   (0x10)
#define RDP_PRESENT_RECT_COLOR2   (0x20)
#define RDP_PRESENT_RECT_COLOR3   (0x40)

#define RDP_PRESENT_DSTBLT_X      (0x01)
#define RDP_PRESENT_DSTBLT_Y      (0x02)
#define RDP_PRESENT_DSTBLT_CX     (0x04)
#define RDP_PRESENT_DSTBLT_CY     (0x08)
#define RDP_PRESENT_DSTBLT_ROP    (0x10)

#define RDP_PRESENT_SCREENBLT_X      (0x01)
#define RDP_PRESENT_SCREENBLT_Y      (0x02)
#define RDP_PRESENT_SCREENBLT_CX     (0x04)
#define RDP_PRESENT_SCREENBLT_CY     (0x08)
#define RDP_PRESENT_SCREENBLT_ROP    (0x10)
#define RDP_PRESENT_SCREENBLT_XSRC   (0x20)
#define RDP_PRESENT_SCREENBLT_YSRC   (0x40)

#define RDP_PRESENT_MEMBLT_CACHEID  (0x0001)
#define RDP_PRESENT_MEMBLT_X        (0x0002)
#define RDP_PRESENT_MEMBLT_Y        (0x0004)
#define RDP_PRESENT_MEMBLT_CX       (0x0008)
#define RDP_PRESENT_MEMBLT_CY       (0x0010)
#define RDP_PRESENT_MEMBLT_ROP      (0x0020)
#define RDP_PRESENT_MEMBLT_XSRC     (0x0040)
#define RDP_PRESENT_MEMBLT_YSRC     (0x0080)
#define RDP_PRESENT_MEMBLT_CACHEIDX (0x0100)

#define RDP_PRESENT_LINE_MIXMODE      (0x0001)
#define RDP_PRESENT_LINE_X1           (0x0002)
#define RDP_PRESENT_LINE_Y1           (0x0004)
#define RDP_PRESENT_LINE_X2           (0x0008)
#define RDP_PRESENT_LINE_Y2           (0x0010)
#define RDP_PRESENT_LINE_MIX          (0x0040)
#define RDP_PRESENT_LINE_WIDTH        (0x0100)
#define RDP_PRESENT_LINE_CLR          (0x0200)

#define RDP_PRESENT_POLYLINE_X            (0x01)
#define RDP_PRESENT_POLYLINE_Y            (0x02)
#define RDP_PRESENT_POLYLINE_MIX          (0x04)
#define RDP_PRESENT_POLYLINE_CLR          (0x10)
#define RDP_PRESENT_POLYLINE_LINES        (0x20)
#define RDP_PRESENT_POLYLINE_DATA         (0x40)

#define RDP_PRESENT_ELLIPSE_X1           (0x01)
#define RDP_PRESENT_ELLIPSE_Y1           (0x02)
#define RDP_PRESENT_ELLIPSE_X2           (0x04)
#define RDP_PRESENT_ELLIPSE_Y2           (0x08)
#define RDP_PRESENT_ELLIPSE_MIX          (0x10)
#define RDP_PRESENT_ELLIPSE_FILLMODE     (0x20)
#define RDP_PRESENT_ELLIPSE_CLR          (0x40)

#define RDP_PRESENT_DESKSAVE_OFFSET       (0x01)
#define RDP_PRESENT_DESKSAVE_X1           (0x02)
#define RDP_PRESENT_DESKSAVE_Y1           (0x04)
#define RDP_PRESENT_DESKSAVE_X2           (0x08)
#define RDP_PRESENT_DESKSAVE_Y2           (0x10)
#define RDP_PRESENT_DESKSAVE_RESTORE      (0x20)

#define RDP_PRESENT_TEXT2_FONT           0x000001
#define RDP_PRESENT_TEXT2_FLAGS          0x000002
#define RDP_PRESENT_TEXT2_CHARINC        0x000004
#define RDP_PRESENT_TEXT2_MIXMODE        0x000008
#define RDP_PRESENT_TEXT2_FG             0x000010
#define RDP_PRESENT_TEXT2_BG             0x000020
#define RDP_PRESENT_TEXT2_CLIPLEFT       0x000040
#define RDP_PRESENT_TEXT2_CLIPTOP        0x000080
#define RDP_PRESENT_TEXT2_CLIPRIGHT      0x000100
#define RDP_PRESENT_TEXT2_CLIPBOTTOM     0x000200
#define RDP_PRESENT_TEXT2_BOXLEFT        0x000400
#define RDP_PRESENT_TEXT2_BOXTOP         0x000800
#define RDP_PRESENT_TEXT2_BOXRIGHT       0x001000
#define RDP_PRESENT_TEXT2_BOXBOTTOM      0x002000
#define RDP_PRESENT_TEXT2_BRUSHSHIFT     14
#define RDP_PRESENT_TEXT2_X              0x080000
#define RDP_PRESENT_TEXT2_Y              0x100000
#define RDP_PRESENT_TEXT2_TEXT           0x200000


typedef struct RDPBRUSH
{
    uint8_t x;
    uint8_t y;
    uint8_t style;
    uint8_t pattern[8];
} RDPBRUSH;

typedef struct RDPPOLYPOINTSDATA
{
    uint8_t datasize;
    uint8_t data[255];
} RDPPOLYPOINTSDATA;

typedef struct REMOTESCREENCACHEINFO
{
    uint32_t offset;
    uint32_t size;
} REMOTESCREENCACHEINFO;

/*
 * Saved data of a RDP order.
 */
typedef struct PATBLTORDERDATA
{
    VRDEORDERAREA area;
    uint8_t rop;
    uint32_t clrB;
    uint32_t clrF;
    RDPBRUSH brush;
} PATBLTORDERDATA;

typedef struct RECTORDERDATA
{
    VRDEORDERAREA area;
    uint32_t clr;
} RECTORDERDATA;

typedef struct DSTBLTORDERDATA
{
    VRDEORDERAREA area;
    uint8_t rop;
} DSTBLTORDERDATA;

typedef struct SCREENBLTORDERDATA
{
    VRDEORDERAREA area;
    uint8_t rop;
    VRDEORDERPOINT src;
} SCREENBLTORDERDATA;

typedef struct MEMBLTORDERDATA
{
    uint16_t cacheid;
    VRDEORDERAREA area;
    uint8_t rop;
    VRDEORDERPOINT src;
    uint16_t cacheidx;
} MEMBLTORDERDATA;

typedef struct LINEORDERDATA
{
    uint16_t mixMode;
    VRDEORDERPOINT start;
    VRDEORDERPOINT end;
    uint8_t mix;
    uint8_t width;
    uint32_t clr;
} LINEORDERDATA;

typedef struct POLYLINEORDERDATA
{
    VRDEORDERPOINT start;
    uint8_t mix;
    uint32_t clr;
    uint8_t lines;
    RDPPOLYPOINTSDATA data;
} POLYLINEORDERDATA;

typedef struct ELLIPSEORDERDATA
{
    VRDEORDERPOINT pt1;
    VRDEORDERPOINT pt2;
    uint8_t mix;
    uint8_t fillMode;
    uint32_t clr;
} ELLIPSEORDERDATA;

typedef struct DESKSAVEORDERDATA
{
    uint32_t offset;
    VRDEORDERPOINT pt1;
    VRDEORDERPOINT pt2;
    uint8_t restore;
} DESKSAVEORDERDATA;

typedef struct TEXT2ORDERDATA
{
    uint8_t  font;
    uint8_t  flags;
    uint8_t  charinc;
    uint8_t  mixmode;
    uint32_t fgcolour;
    uint32_t bgcolour;
    int16_t  clipleft;
    int16_t  cliptop;
    int16_t  clipright;
    int16_t  clipbottom;
    int16_t  boxleft;
    int16_t  boxtop;
    int16_t  boxright;
    int16_t  boxbottom;
    RDPBRUSH brush;
    int16_t  x;
    int16_t  y;
    uint8_t  length;
    uint8_t  text[VRDP_TC_NUM_GLYPHS];
} TEXT2ORDERDATA;

/*
 * State information for the RDP orders generator.
 */
typedef struct RDPSAVEDORDERS
{
    uint8_t u8LastOrder;

    bool fBounds;
    VRDEORDERBOUNDS boundsPrevious;
    VRDEORDERBOUNDS boundsCurrent;

    REMOTESCREENCACHEINFO aRemoteScreenCacheInfo[4];
    uint32_t cRemotePixelsUsed;
    uint32_t cLocalSlotsUsed;

    /* Information of the last order, which was sent to the client, for each type of order.
     * Used for calculation of the PRESENT bit mask.
     */
    PATBLTORDERDATA    patblt;
    RECTORDERDATA      rect;
    DSTBLTORDERDATA    dstblt;
    SCREENBLTORDERDATA screenblt;
    MEMBLTORDERDATA    memblt;
    LINEORDERDATA      line;
    POLYLINEORDERDATA  polyline;
    ELLIPSEORDERDATA   ellipse;
    DESKSAVEORDERDATA  desksave;
    TEXT2ORDERDATA     text2;

} RDPSAVEDORDERS;

#endif /* !VRDP_INCLUDED_SRC_orders_h */
