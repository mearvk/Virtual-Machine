/* $Id: regions.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "regions.h"
#include "utils.h"

/* How many bricks we allocate for all regions.
 * @todo dinamically allocate as many pools as necessary.
 */
#define RGN_BRICKS_IN_POOL (65536)

#define RGN_ROWS_ALLOC_THRESHOLD (256)

/* Access to a region and its rectangles must be serialized by the caller. */

typedef struct _RGNBRICK
{
    RGNRECT rect;
    struct _RGNBRICK *nextBrick;
    struct _RGNBRICK *prevBrick;
    REGION *prgn;
} RGNBRICK;

struct _REGION
{
    /* The link in the lookaside list. */
    struct _REGION *pNextFree;

    /* Arrays of pointers to first bricks of brick rows.
     * Bricks are allocated from global pool.
     */
    RGNBRICK **ppRows;
    /* Number of rows in the given region, number of valid rows in the array. Must be signed. */
    int32_t cRows;
    /* Number of allocated elements in the pRows. Must be signed. */
    int32_t cRowsAlloc;

    RGNBRICK *RectEnumPtr;
    int32_t RectEnumRow;

    /* A value assigned to this region by caller. */
    uint32_t uniq;

    /* The region bounding rectangle. */
    RGNRECT rect;

    struct _REGIONCTX *pctx;

#ifdef RGNLEAK
    REGION *pRgnNextDbg;
    REGION *pRgnPrevDbg;
    const char *pszCaller;
    int iLine;
#endif /* RGNLEAK */
};

typedef struct _BRICKPOOL
{
    /* Bitmap for used bricks */
    uint32_t bmpUsedBricks[RGN_BRICKS_IN_POOL / (sizeof (uint32_t) * 8)];
    RGNBRICK bricks[RGN_BRICKS_IN_POOL];
} BRICKPOOL;

struct _REGIONCTX
{
    BRICKPOOL BricksPool;

    /* List of allocated rgn structures. They are reused. */
    REGION *pRgnFree;

#ifdef RGNLEAK
    REGION *pRgnListDbg;
#endif /* RGNLEAK */

#if defined(VRDP_DEBUG_RGN) && defined(DEBUG)
    int cBricks;
#endif /* VRDP_DEBUG_RGN */

    /* The context is referenced at the creation and by each created region. */
    int cRefs;
};

#ifdef LOG_ENABLED
static void rgnDump (const char *title, const REGION *prgn)
{
    RT_NOREF1(title);
    if (rgnIsEmpty (prgn))
    {
        RGNLOG(("[%s] (%p): empty. %d,%d %dx%d\n", title, prgn, prgn->rect.x, prgn->rect.y, prgn->rect.w, prgn->rect.h));
    }
    else
    {
        RGNLOG(("[%s] (%p) %d,%d %dx%d:\n", title, prgn, prgn->rect.x, prgn->rect.y, prgn->rect.w, prgn->rect.h));

        int row = 0;

        while (row < prgn->cRows)
        {
            RGNBRICK *pbrick = prgn->ppRows[row++];

            while (pbrick != NULL)
            {
                RGNLOG(("    %2d: x = %d, y = %d, w = %d, h = %d\n", row, pbrick->rect.x, pbrick->rect.y, pbrick->rect.w, pbrick->rect.h));

                pbrick = pbrick->nextBrick;
            }
        }
    }
}
#else
#define rgnDump(a, b)
#endif /* LOG_ENABLED */

REGIONCTX *rgnCtxCreate (void)
{
    REGIONCTX *pctx = (REGIONCTX *)VRDPMemAllocZ (sizeof (REGIONCTX));
    if (pctx)
    {
        pctx->cRefs++;

        TESTLOG(("created ctx %p\n", pctx));
    }
    return pctx;
}

void rgnCtxRelease (REGIONCTX *pctx)
{
    if (--pctx->cRefs > 0)
    {
        return;
    }

    while (pctx->pRgnFree != NULL)
    {
        REGION *pNext = pctx->pRgnFree->pNextFree;

        VRDPMemFree (pctx->pRgnFree->ppRows);
        VRDPMemFree (pctx->pRgnFree);

        pctx->pRgnFree = pNext;
    }

#ifdef RGNLEAK
    REGION *prgn = pctx->pRgnListDbg;

    while (prgn)
    {
        LogRel(("rgn: %p %s @%d\n", prgn, prgn->pszCaller, prgn->iLine));
        prgn = prgn->pRgnNextDbg;
    }
#endif /* RGNLEAK */

    TESTLOG(("deleted ctx %p\n", pctx));

    VRDPMemFree (pctx);
}

static void rgnReallocRows (REGION *prgn)
{
    uint32_t cRowsAlloc = prgn->cRowsAlloc + RGN_ROWS_ALLOC_THRESHOLD;

    void *p = (RGNBRICK **)VRDPMemAlloc (sizeof (RGNBRICK *) * cRowsAlloc);

    if (p)
    {
        if (prgn->cRows > 0)
        {
            memcpy (p, prgn->ppRows, prgn->cRows * sizeof (RGNBRICK *));
        }

        if (prgn->ppRows)
        {
            VRDPMemFree (prgn->ppRows);
        }

        prgn->ppRows = (RGNBRICK **)p;
        prgn->cRowsAlloc = cRowsAlloc;
    }
    else
    {
        AssertFailed();
    }
}

/* Reuse regions memory structures. Maintain lookaside list and free it on shadowBuffer delete (rgnReinit). */
static REGION *rgnAlloc (REGIONCTX *pctx)
{
    REGION *prgn = pctx->pRgnFree;

    if (prgn)
    {
        pctx->pRgnFree = pctx->pRgnFree->pNextFree;
        /* Do not overwrite the memory block because ppRows and cRowsAlloc will be reused.
         * The caller will reinitialize the structure.
         */
    }
    else
    {
        /* Make sure that the first allocation is zeroed. */
        prgn = (REGION *)VRDPMemAllocZ (sizeof (REGION));
    }

    if (prgn)
    {
        pctx->cRefs++;
    }

    return prgn;
}

static void rgnFree (REGION *prgn)
{
    prgn->pNextFree = prgn->pctx->pRgnFree;
    prgn->pctx->pRgnFree = prgn;
    rgnCtxRelease (prgn->pctx);
}


static void rgnInitBrick (RGNBRICK *pBrick, REGION *prgn, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    Assert(w > 0 && h > 0);

    pBrick->rect.x = x;
    pBrick->rect.y = y;
    pBrick->rect.w = w;
    pBrick->rect.h = h;

    pBrick->nextBrick = NULL;
    pBrick->prevBrick = NULL;

    pBrick->prgn = prgn;
}

static RGNBRICK *rgnAllocBrick (REGIONCTX *pctx)
{
    /* Search a not used brick in the pool. */
    int index = ASMBitFirstClear(pctx->BricksPool.bmpUsedBricks, RGN_BRICKS_IN_POOL);

    Assert(index != -1);

    if (index == -1)
    {
        return NULL;
    }

    ASMBitSet(pctx->BricksPool.bmpUsedBricks, index);

    RGNLOG(("pctx->cBricks = %d\n", ++pctx->cBricks));

    return &pctx->BricksPool.bricks[index];
}

static void rgnFreeBrick (RGNBRICK *pbrick)
{
    Assert(pbrick);

    REGIONCTX *pctx = pbrick->prgn->pctx;

    Assert(&pctx->BricksPool.bricks[0] <= pbrick && pbrick < &pctx->BricksPool.bricks[RT_ELEMENTS(pctx->BricksPool.bricks)]);
    Assert(((char *)pbrick - (char *)&pctx->BricksPool.bricks[0]) % sizeof (RGNBRICK) == 0);

    int index = pbrick - &pctx->BricksPool.bricks[0];

    ASMBitClear(pctx->BricksPool.bmpUsedBricks, index);

    RGNLOG(("cBricks = %d\n", --pctx->Bricks));
}

static void rgnInsertBrickAfter (RGNBRICK *pPrevBrick, int32_t x, uint32_t w)
{
    RGNBRICK *pBrick = rgnAllocBrick (pPrevBrick->prgn->pctx);

    if (!pBrick)
    {
        return;
    }

    rgnInitBrick (pBrick, pPrevBrick->prgn, x, pPrevBrick->rect.y, w, pPrevBrick->rect.h);

    pBrick->prevBrick = pPrevBrick;
    pBrick->nextBrick = pPrevBrick->nextBrick;

    if (pPrevBrick->nextBrick)
    {
        pPrevBrick->nextBrick->prevBrick = pBrick;
    }

    pPrevBrick->nextBrick = pBrick;
}

static void rgnInsertFirstBrick (REGION *prgn, int index, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    RGNBRICK *pBrick = rgnAllocBrick (prgn->pctx);

    if (!pBrick)
    {
        return;
    }

    rgnInitBrick (pBrick, prgn, x, y, w, h);

    pBrick->nextBrick = prgn->ppRows[index];
    prgn->ppRows[index]->prevBrick = pBrick;
    prgn->ppRows[index] = pBrick;
}


static void rgnRemoveBrick (RGNBRICK *pbrick, int rowindex)
{
    if (pbrick->nextBrick)
    {
        pbrick->nextBrick->prevBrick = pbrick->prevBrick;
    }

    if (pbrick->prevBrick)
    {
        pbrick->prevBrick->nextBrick = pbrick->nextBrick;
    }
    else
    {
        /* The brick was first in a row. Adjust the row ptr. */
        REGION *prgn = pbrick->prgn;

        rgnDump("rgnRemoveBrick: 1)", prgn);

        Assert(prgn->cRows > 0);

        Assert(0 <= rowindex && rowindex < prgn->cRows);

        RGNLOG(("index %d of %d\n", rowindex, prgn->cRows));

        if (pbrick->nextBrick)
        {
            /* There are other bricks in the row. */
            prgn->ppRows[rowindex] = pbrick->nextBrick;
        }
        else
        {
            /* No more bricks in the row. Remove the row. */
            prgn->cRows--;

            if (rowindex < prgn->cRows)
            {
                memmove (&prgn->ppRows[rowindex], &prgn->ppRows[rowindex + 1], sizeof (RGNBRICK *) * (prgn->cRows - rowindex));
            }
        }

        rgnDump("rgnRemoveBrick: 2)", prgn);
    }

    rgnFreeBrick (pbrick);
}

static void rgnSplitRow (REGION *prgn, int32_t index, int32_t ySplit)
{
    Assert(prgn);
    Assert(0 <= index && index < prgn->cRows);

    RGNBRICK *pBrick = prgn->ppRows[index];

    Assert(pBrick);

    if (prgn->cRows + 1 >= prgn->cRowsAlloc)
    {
        rgnReallocRows (prgn);
    }

    rgnDump("rgnSplitRow: 1)", prgn);

    /* Create a first brick in the new row before changing the region in order to
     * leave on the failed alloc.
     */
    RGNBRICK *pNewBrick = rgnAllocBrick (prgn->pctx);

    if (!pNewBrick)
    {
        return;
    }

    /* Index of the new row. */
    index++;

    if (index < prgn->cRows)
    {
        /* Make a place for new bricks ptr in the array. */
        memmove (&prgn->ppRows[index + 1], &prgn->ppRows[index], sizeof (RGNBRICK *) * (prgn->cRows - index));
    }

    prgn->cRows++;

    /* Copy bricks from the previous row to new one and adjust the y and h of both rows. */
    uint32_t hRow = ySplit - pBrick->rect.y;
    uint32_t hNewRow = pBrick->rect.h - hRow;

    rgnInitBrick (pNewBrick, prgn, pBrick->rect.x, pBrick->rect.y + hRow, pBrick->rect.w, hNewRow);

    prgn->ppRows[index] = pNewBrick;

    pBrick->rect.h = hRow;

    pBrick = pBrick->nextBrick;

    while (pBrick)
    {
        pBrick->rect.h = hRow;

        rgnInsertBrickAfter (pNewBrick, pBrick->rect.x, pBrick->rect.w);

        pNewBrick = pNewBrick->nextBrick;
        pBrick = pBrick->nextBrick;
    }

    rgnDump("rgnSplitRow: 2)", prgn);
}

static void rgnInsertRow (REGION *prgn, int index, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    /* Insert a rectangular row in the region at index, moving all lower rows down. */

    if (prgn->cRows + 1 >= prgn->cRowsAlloc)
    {
        rgnReallocRows (prgn);
    }

    /* Create a first brick in the new row. */
    RGNBRICK *pNewBrick = rgnAllocBrick (prgn->pctx);

    if (!pNewBrick)
    {
        return;
    }

    if (index == -1)
    {
        /* Special case. Insert as last row. */
        rgnInitBrick (pNewBrick, prgn, x, y, w, h);

        prgn->ppRows[prgn->cRows++] = pNewBrick;
    }
    else
    {
        /* Make a place for new bricks ptr in the array. */
        memmove (&prgn->ppRows[index + 1], &prgn->ppRows[index], sizeof (RGNBRICK *) * (prgn->cRows - index));

        rgnInitBrick (pNewBrick, prgn, x, y, w, h);

        prgn->ppRows[index] = pNewBrick;
        prgn->cRows++;
    }
}


bool rgnIsEmpty (const REGION *prgn)
{
    bool fEmpty = (prgn == NULL || prgn->cRows == 0);
    AssertMsg (!prgn || (!prgn->cRows || (prgn->cRows && prgn->ppRows)), ("prgn %p\n", prgn));
    RGNLOG(("%p: %d\n", prgn, fEmpty));
    return fEmpty;
}

#ifdef RGNLEAK
REGION *rgnCreateEmptyDbg (REGIONCTX *pctx, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t uniq, const char *pszCaller, int iLine)
#else
REGION *rgnCreateEmpty (REGIONCTX *pctx, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t uniq)
#endif /* RGNLEAK */
{
    REGION *prgn = rgnAlloc (pctx);

    if (prgn)
    {
        /* Initialize everything but ppRows and cRowsAlloc because they stay in a reused rgn. */
        prgn->pNextFree = NULL;

        prgn->cRows = 0;

        prgn->RectEnumPtr = NULL;
        prgn->RectEnumRow = -1;

        prgn->uniq = uniq;

        prgn->rect.x = x;
        prgn->rect.y = y;
        prgn->rect.w = w;
        prgn->rect.h = h;

        prgn->pctx = pctx;

        RGNLOG(("%p\n", prgn));
#ifdef RGNLEAK
        prgn->pszCaller = pszCaller;
        prgn->iLine = iLine;
        prgn->pRgnNextDbg = pctx->pRgnListDbg;
        prgn->pRgnPrevDbg = NULL;
        if (pctx->pRgnListDbg)
        {
            pctx->pRgnListDbg->pRgnPrevDbg = prgn;
        }
        pctx->pRgnListDbg = prgn;
#endif /* RGNLEAK */
    }

    return prgn;
}

void rgnDelete (REGION *prgn)
{
    if (prgn)
    {
#ifdef RGNLEAK
        if (prgn->pRgnNextDbg)
        {
            prgn->pRgnNextDbg->pRgnPrevDbg = prgn->pRgnPrevDbg;
        }
        else
        {
            /* do nothing */
        }

        if (prgn->pRgnPrevDbg)
        {
            prgn->pRgnPrevDbg->pRgnNextDbg = prgn->pRgnNextDbg;
        }
        else
        {
            prgn->pctx->pRgnListDbg = prgn->pRgnNextDbg;
        }
#endif /* RGNLEAK */
        /* Make the region empty, deallocate bricks. */
        rgnReset (prgn, 0);

        /* Free region structure memory. */
        rgnFree (prgn);
    }
}

/* Add a rectangle to another region. */
void rgnAddRect (REGION *prgn, const RGNRECT *prect)
{
    if (!prgn || !prect)
    {
        return;
    }

    RGNLOG(("Enter: %d,%d %dx%d, rgn: %d,%d %dx%d\n",
            prect->x, prect->y, prect->w, prect->h,
            prgn->rect.x, prgn->rect.y, prgn->rect.w, prgn->rect.h));

    /* Find intersection between the region rectangle and the rect
     * to have a new rectangle that actually will be inserted to the region.
     */

    RGNRECT rectSect;

    if (!rgnIntersectRects(&rectSect, &prgn->rect, prect))
    {
        RGNLOG(("empty rectangle\n"));
        return;
    }

    RGNLOG(("rgnAddRect: rectSect %d,%d, %dx%d\n",
            rectSect.x, rectSect.y, rectSect.w, rectSect.h));

    int32_t xLeft   = rectSect.x;
    int32_t xRight  = rectSect.x + rectSect.w;
    int32_t yTop    = rectSect.y;
    int32_t yBottom = rectSect.y + rectSect.h;

    /* Add the rectangle to the region, splitting rows and adding new rows/bricks if necessary. */
    int i = -1;
    for (;;)
    {
        rgnDump("rgnAddRect", prgn);

        i++;

        if (i >= prgn->cRows || yTop == yBottom)
        {
            RGNLOG(("rgnAddRect: no more rows i = %d, cRow = %d\n", i, prgn->cRows));
            break;
        }

        RGNBRICK *pbrick = prgn->ppRows[i];

        /* Check if the row is intersected with the rectangle by y. */
        if (pbrick->rect.y >= yBottom)
        {
            /* The entire row is lower than the rectangle.
             * All remaining rows are also lower.
             */
            RGNLOG(("rgnAddRect: entire row is lower than the rectangle, inserting rect as a new row\n"));
            rgnInsertRow (prgn, i, xLeft, yTop, xRight - xLeft, yBottom - yTop);
            yTop = yBottom; /* entire rectangle was inserted. */
            break;
        }

        if (pbrick->rect.y + pbrick->rect.h <= (uint32_t)yTop)
        {
            /* The entire row is higher than the rectangle.
             * Continue with a next row.
             */
            RGNLOG(("rgnAddRect: entire row is higher than the rectangle, continuing with a next row.\n"));
            continue;
        }

        /* Check if the upper part of rectangle must form a new row above the current row. */
        if (yTop < pbrick->rect.y)
        {
            int hNew = pbrick->rect.y - yTop;

            RGNLOG(("rgnAddRect: upper part of rectangle must form a new row above the current row: yTop = %d, row y = %d, new height = %d\n", yTop, pbrick->rect.y, hNew));

            rgnInsertRow (prgn, i, xLeft, yTop, xRight - xLeft, hNew);

            Assert (pbrick->rect.y < yBottom);

            yTop = pbrick->rect.y; /* Cut the inserted part. */

            RGNLOG(("rgnAddRect: new yTop = %d\n", yTop));

            /* Continue with a next row. */
            continue;
        }

        /* Check if the row must be splitted. */

        if (yTop > pbrick->rect.y)
        {
            Assert((uint32_t)yTop < pbrick->rect.y + pbrick->rect.h);

            RGNLOG(("rgnAddRect: row must be splitted because yTop = %d and row y = %d\n", yTop, pbrick->rect.y));

            rgnSplitRow (prgn, i, yTop);

            /* Continue with row loop. The just created row will be processed.
             */
            continue;
        }

        if ((uint32_t)yBottom < pbrick->rect.y + pbrick->rect.h)
        {
            Assert(yBottom > pbrick->rect.y);

            RGNLOG(("rgnAddRect: row must be splitted because yBottom = %d and row ybottom = %d\n", yBottom, pbrick->rect.y + pbrick->rect.h));

            rgnSplitRow (prgn, i, yBottom);

            /* Continue with adding bricks in the row, which is now aligned with rectangle. */
        }

        /*
         * Add new bricks to the row that intersects with the rectangle.
         * After adding as new brick we will have the same or less or 1 more
         * bricks. So modify bricks coordinates in place and then at the end
         * either add a new brick or remove now unused ones.
         */

        RGNBRICK *prevBrick = NULL;

        /* Skip all bricks those will are lefter than the new one. */
        while (pbrick && pbrick->rect.x < xLeft && pbrick->rect.x + pbrick->rect.w < (uint32_t)xLeft)
        {
            RGNLOG(("rgnAddRect: skipping brick %d-%d\n", pbrick->rect.x, pbrick->rect.x + pbrick->rect.w));

            prevBrick = pbrick;
            pbrick = pbrick->nextBrick;
        }

        if (pbrick)
        {
            /* Here either the new brick is lefter than the current brick,
             * or intersects with the current brick.
             */
            if (pbrick->rect.x > xRight)
            {
                /* The new brick is lefter. */
                RGNLOG(("rgnAddRect: new brick is lefter: brick left %d, xRight = %d\n", pbrick->rect.x, xRight));

                if (prevBrick)
                {
                    rgnInsertBrickAfter (prevBrick, xLeft, xRight - xLeft);
                }
                else
                {
                    rgnInsertFirstBrick (prgn, i, xLeft, pbrick->rect.y, xRight - xLeft, pbrick->rect.h);
                }
            }
            else
            {
                /* The new brick intersects with the brick. So no additional bricks
                 * will be formed and probably some existing bricks will be
                 * overlapped by the new brick.
                 */

                int32_t xLeftBrick = RT_MIN (xLeft, pbrick->rect.x);
                int32_t xRightBrick = RT_MAX (xRight, pbrick->rect.x + (int32_t)pbrick->rect.w);

                RGNLOG(("rgnAddRect: new brick %d-%d intersects with %d-%d\n", xLeft, xRight, pbrick->rect.x, pbrick->rect.x + pbrick->rect.w));

                /* Save which brick we are updating. */
                prevBrick = pbrick;
                pbrick = pbrick->nextBrick;

                /* Delete all bricks those are overlapped by the just created brick. */
                while (pbrick && pbrick->rect.x < xRightBrick)
                {
                    xRightBrick = RT_MAX (xRightBrick, pbrick->rect.x + (int32_t)pbrick->rect.w);

                    RGNBRICK *pnext = pbrick->nextBrick;
                    RGNLOG(("rgnAddRect: brick %d-%d removed\n", pbrick->rect.x, pbrick->rect.x + pbrick->rect.w));
                    rgnRemoveBrick (pbrick, i);
                    pbrick = pnext;
                }

                RGNLOG(("rgnAddRect: formed new brick %d-%d\n", xLeftBrick, xRightBrick));

                prevBrick->rect.x = xLeftBrick;
                prevBrick->rect.w = xRightBrick - xLeftBrick;
            }
        }
        else
        {
            /* The new brick is righter than all bricks. */
            Assert(prevBrick);
            RGNLOG(("rgnAddRect: new brick is righter than all bricks: pref brick %d-%d, xLeft = %d\n", prevBrick->rect.x, prevBrick->rect.x + prevBrick->rect.w, xRight));
            rgnInsertBrickAfter (prevBrick, xLeft, xRight - xLeft);
        }

        pbrick = prgn->ppRows[i];
        yTop = pbrick->rect.y + pbrick->rect.h; /* Cut the inserted part. */

        RGNLOG(("rgnAddRect: after bricks yTop = %d\n", yTop));
    }

    if (yTop < yBottom)
    {
        /* Insert the remaining rectangle as last row. */
        RGNLOG(("rgnAddRect: inserting remaining %d to %d as last row\n", yTop, yBottom));
        rgnInsertRow (prgn, -1, xLeft, yTop, xRight - xLeft, yBottom - yTop);
    }

    rgnDump("2) rgnAddRect", prgn);

    RGNLOG(("Leave\n"));
}

/* Add a region to another region, merge. */
void rgnAdd (REGION *prgn, REGION *padd)
{
    RGNLOG(("Enter\n"));

    const RGNRECT *prect;

    rgnEnumRect (padd);

    while ((prect = rgnNextRect (padd)) != NULL)
    {
        rgnAddRect (prgn, prect);
    }

    RGNLOG(("Leave\n"));
}

void rgnRemoveEmptyBricks (REGION *prgn)
{
    RGNLOG(("Enter\n"));

    if (rgnIsEmpty (prgn))
    {
        RGNLOG(("Leave\n"));
        return;
    }

    /* Start from the last row to the first. Since rows could be removed. */
    int row = prgn->cRows - 1;

    Assert (row >= 0);

    while (row >= 0)
    {
        RGNBRICK *pbrick = prgn->ppRows[row];

        while (pbrick != NULL)
        {
            if (pbrick->rect.w == 0)
            {
                RGNBRICK *next = pbrick->nextBrick;

                rgnRemoveBrick (pbrick, row);

                pbrick = next;
            }
            else
            {
                pbrick = pbrick->nextBrick;
            }
        }

        row--;
    }

    RGNLOG(("Leave\n"));
}

void rgnMergeAdjacentRows (REGION *prgn)
{
    RGNLOG(("Enter\n"));

    Assert (!prgn->cRows || (prgn->cRows && prgn->ppRows));

    if (prgn->cRows > 1)
    {
        /* Merge adjacent rows, only equal rows are merged */
        rgnDump ("rgnMergeAdjacentRows: 1)", prgn);

        int rowindex = 0;

        while (rowindex < prgn->cRows - 1)
        {
            RGNLOG(("rowindex %d, prgn->cRows %d\n", rowindex, prgn->cRows));

            RGNBRICK *prow = prgn->ppRows[rowindex];
            RGNBRICK *pnextrow = prgn->ppRows[rowindex + 1];

            if (prow->rect.y + prow->rect.h == (uint32_t)pnextrow->rect.y)
            {
                 /* Check if both rows have same bricks. */
                 while (prow && pnextrow)
                 {
                     if (   prow->rect.x != pnextrow->rect.x
                         || prow->rect.w != pnextrow->rect.w)
                     {
                         break;
                     }

                     prow = prow->nextBrick;
                     pnextrow = pnextrow->nextBrick;
                 }

                 if (prow == NULL && pnextrow == NULL)
                 {
                      /* adjust height or the row. */

                      RGNBRICK *pbrick = prgn->ppRows[rowindex];

                      uint32_t h = pbrick->rect.h + prgn->ppRows[rowindex + 1]->rect.h;

                      while (pbrick != NULL)
                      {
                          pbrick->rect.h = h;

                          pbrick = pbrick->nextBrick;
                      }

                      /* free the next row */

                      pbrick = prgn->ppRows[rowindex + 1];

                      while (pbrick != NULL)
                      {
                          RGNBRICK *next = pbrick->nextBrick;

                          rgnRemoveBrick (pbrick, rowindex + 1);

                          pbrick = next;
                      }

                      /* Do not advance the row index. The new row might be adjacent to the next one. */
                      continue;
                 }
            }

            rowindex++;
        }

        rgnDump ("rgnMergeAdjacentRows: 2)", prgn);
    }

    RGNLOG(("Leave\n"));
}

void rgnEnumRect (REGION *prgn)
{
    if (prgn)
    {
        prgn->RectEnumPtr = NULL;
        prgn->RectEnumRow = -1;

        rgnDump ("rgnEnumRect", prgn);
    }
}

RGNRECT *rgnNextRect (REGION *prgn)
{
    RGNBRICK *pbrick = NULL;

    rgnDump ("rgnNextRect", prgn);

    if (prgn)
    {
        pbrick = prgn->RectEnumPtr? prgn->RectEnumPtr->nextBrick: NULL;

        if (pbrick == NULL && prgn->RectEnumRow < prgn->cRows)
        {
            /* End of row, advance. */
            prgn->RectEnumRow++;

            if (prgn->RectEnumRow < prgn->cRows)
            {
                pbrick = prgn->ppRows[prgn->RectEnumRow];
            }
        }

        prgn->RectEnumPtr = pbrick;
    }

    return (RGNRECT *)pbrick;
}

void rgnUpdateRectWidth (RGNRECT *prect, int32_t x, uint32_t w)
{
    prect->x = x;
    prect->w = w;
}

uint32_t rgnGetUniq (REGION *prgn)
{
    if (prgn)
    {
        return prgn->uniq;
    }

    return 0;
}


void rgnReset (REGION *prgn, uint32_t uniq)
{
    RT_NOREF1(uniq);
    RGNLOG(("Enter: %p, %d\n", prgn, uniq));
    rgnDump ("rgnReset", prgn);

    if (prgn && prgn->ppRows)
    {
        int i;

        for (i = 0; i < prgn->cRows; i++)
        {
            RGNBRICK *pbrick = prgn->ppRows[i];

            while (pbrick)
            {
                RGNBRICK *pnext = pbrick->nextBrick;

                rgnFreeBrick (pbrick);

                pbrick = pnext;
            }
        }

        /* Do not deallocate ppRows and keep cRowsAlloc, because rgns structs are reused. */
        prgn->cRows = 0;
    }

    RGNLOG(("Leave\n"));
}

bool rgnIntersectRects (RGNRECT *prectResult,
                        const RGNRECT *prect1,
                        const RGNRECT *prect2)
{
    /* Calculations are easier with left, right, top, bottom. */
    int xLeft1   = prect1->x;
    int xRight1  = prect1->x + prect1->w;

    int xLeft2   = prect2->x;
    int xRight2  = prect2->x + prect2->w;

    int yTop1    = prect1->y;
    int yBottom1 = prect1->y + prect1->h;

    int yTop2    = prect2->y;
    int yBottom2 = prect2->y + prect2->h;

    /* Initialize result to empty record. */
    memset (prectResult, 0, sizeof (RGNRECT));

    int xLeftResult = RT_MAX (xLeft1, xLeft2);
    int xRightResult = RT_MIN (xRight1, xRight2);

    if (xLeftResult < xRightResult)
    {
        /* There is intersection by X. */

        int yTopResult = RT_MAX (yTop1, yTop2);
        int yBottomResult = RT_MIN (yBottom1, yBottom2);

        if (yTopResult < yBottomResult)
        {
            /* There is intersection by Y. */

            prectResult->x = xLeftResult;
            prectResult->y = yTopResult;
            prectResult->w = xRightResult - xLeftResult;
            prectResult->h = yBottomResult - yTopResult;
            return true;
        }
    }

    return false;
}

void rgnMergeRects (RGNRECT *prectResult,
                    const RGNRECT *prect1,
                    const RGNRECT *prect2)
{
    /* Calculations are easier with left, right, top, bottom. */
    int xLeft1   = prect1->x;
    int xRight1  = prect1->x + prect1->w;

    int xLeft2   = prect2->x;
    int xRight2  = prect2->x + prect2->w;

    int yTop1    = prect1->y;
    int yBottom1 = prect1->y + prect1->h;

    int yTop2    = prect2->y;
    int yBottom2 = prect2->y + prect2->h;

    int xLeftResult = RT_MIN (xLeft1, xLeft2);
    int xRightResult = RT_MAX (xRight1, xRight2);
    int yTopResult = RT_MIN (yTop1, yTop2);
    int yBottomResult = RT_MAX (yBottom1, yBottom2);

    prectResult->x = xLeftResult;
    prectResult->y = yTopResult;
    prectResult->w = xRightResult - xLeftResult;
    prectResult->h = yBottomResult - yTopResult;

    return;
}

bool rgnIsRectWithin (const RGNRECT *pRect, const RGNRECT *pRectTest)
{
    int rectRight      = pRect->x + pRect->w;
    int rectBottom     = pRect->y + pRect->h;
    int rectTestRight  = pRectTest->x + pRectTest->w;
    int rectTestBottom = pRectTest->y + pRectTest->h;

    return    pRect->x <= pRectTest->x
           && pRect->y <= pRectTest->y
           && rectRight >= rectTestRight
           && rectBottom >= rectTestBottom;
}

bool rgnIsRectEmpty (const RGNRECT *prect)
{
    return (prect->w == 0) || (prect->h == 0);
}

void rgnInvert (REGION *prgn)
{
    RGNLOG(("Enter\n"));

    rgnDump ("rgnInvert pre", prgn);

    if (rgnIsEmpty (prgn))
    {
        /* Special case of empty region, just make it full one. */
        rgnAddRect (prgn, &prgn->rect);
    }
    else
    {
        int cRow = 0;

        /* Invert existing rows. */
        while (cRow < prgn->cRows)
        {
            /* Invert the row. */
            RGNBRICK *pbrick = prgn->ppRows[cRow++];

            Assert(pbrick);

            /* Each encountered brick is replaced with a brick [xLeft, pbrick->xLeft)
             * xLeft becomes pbrick->xRight.
             */
            int xLeft = prgn->rect.x;
            RGNBRICK *pbrickLast = pbrick;

            while (pbrick != NULL)
            {
                Assert (pbrick->rect.x >= xLeft);

                /* Situation with no empty space before the brick also handled
                 * by creating a 0 width first brick.
                 */
                int xNewLeft = pbrick->rect.x + pbrick->rect.w;

                pbrick->rect.w = pbrick->rect.x - xLeft;
                pbrick->rect.x = xLeft;

                xLeft = xNewLeft;

                pbrickLast = pbrick;
                pbrick = pbrick->nextBrick;
            }

            if (xLeft < prgn->rect.x + (int)prgn->rect.w)
            {
                rgnInsertBrickAfter (pbrickLast, xLeft, (prgn->rect.x + prgn->rect.w) - xLeft);
            }
            else
            {
                Assert ((unsigned)xLeft == prgn->rect.x + prgn->rect.w);
            }
        }

        /* Add new full rows at the space between existing rows. */
        int y = prgn->rect.y;
        cRow = 0;

        while (y < prgn->rect.y + (int)prgn->rect.h)
        {
            if (cRow >= prgn->cRows)
            {
                break;
            }

            int yRow = prgn->ppRows[cRow]->rect.y;

            if (y < yRow)
            {
                rgnInsertRow (prgn, cRow, prgn->rect.x, y, prgn->rect.w, yRow - y);
                cRow++;
            }

            y = yRow + prgn->ppRows[cRow]->rect.h;

            cRow++;
        }

        if ((uint32_t)y != prgn->rect.y + prgn->rect.h)
        {
            /* Create the bottom row. */
            rgnInsertRow (prgn, -1, prgn->rect.x, y, prgn->rect.w, prgn->rect.y + prgn->rect.h - y);
        }

        /* Empty leading bricks might be created. */
        rgnRemoveEmptyBricks (prgn);
    }

    rgnDump ("rgnInvert post", prgn);

    RGNLOG(("Leave\n"));

    return;
}

void rgnCut (REGION *prgnSect, const REGION *prgn, const RGNRECT *prectCut)
{
    RGNLOG(("Enter: rectCut %d,%d %dx%d\n", prectCut->x, prectCut->y, prectCut->w, prectCut->h));
    rgnDump ("rgnCut pre", prgn);

    int cRow = 0;

    while (cRow < prgn->cRows)
    {
        /* Cut the row. */
        RGNBRICK *pbrick = prgn->ppRows[cRow++];

        Assert(pbrick);

        while (pbrick != NULL)
        {
            RGNRECT rectResult;

            if (rgnIntersectRects (&rectResult, &pbrick->rect, prectCut))
            {
                rgnAddRect (prgnSect, &rectResult);
            }

            pbrick = pbrick->nextBrick;
        }
    }

    rgnDump ("rgnCut post", prgnSect);

    RGNLOG(("Leave\n"));

    return;
}
