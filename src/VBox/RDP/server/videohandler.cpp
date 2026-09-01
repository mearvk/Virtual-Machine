/* $Id: videohandler.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "videostream.h"
#include "videotsmf.h"
#include "vrdpserv.h"
#include "videoencoder.h"
#include "videodetector.h"
#include "bmpscale.h"

#include <math.h>


#if 0 // def DEBUG_sunlover
#undef VIDEOLOG
#define VIDEOLOG LogRel
#define VIDEOASSERT AssertLogRel
#else
#define VIDEOASSERT Assert
#endif

#ifdef DEBUG_sunlover
#define VRDP_ASSERT_VH_CONTEXT_OUTPUT(pVHCtx) VRDP_ASSERT_CONTEXT_OUTPUT((pVHCtx)->pServer)
#else
#define VRDP_ASSERT_VH_CONTEXT_OUTPUT(pVHCtx) do {} while (0)
#endif


/*
 * VideoHandler (VH)
 *
 * works partially on APP thread, INPUT thread and OUTPUT thread.
 *
 * Source frames are copied to a per stream Source Frame Buffer.
 *
 * The Source Frame Buffer contains 2 frame blocks.
 * It must provide fast writing, with minimal waiting.
 *
 * APP thread writes to one of blocks, which is not used by the OUTPUT thread.
 * Writing is a simple copy and is fast. If both blocks are full, the last block is overwritten.
 * This means that frames can be dropped but the last frame must be always written.
 *
 * OUTPUT thread reads from a block, which contains the frame data. Blocks are used one after another.
 * Reading is slow as it may include downscaling and jpeg compression.
 *
 * Blocks are ordered, so OUTPUT reads frames in the same order as they are written.
 *
 * Block data includes: bitmap description, pixel data, timestamp.
 * Block status:
 * - Empty - initial state of both blocks;
 * - Full - contains the data;
 * - Writing - APP thread is writing data to the block;
 * - Reading - OUTPUT thread is reading the block.
 *
 * Status is changed under source lock. Writing and reading are not under lock.
 *
 * Timing:
 *
 * VH maintains timing data for each detected video stream based on frames timestamps and required lipsync delay.
 * This information is per output stream.
 *
 * Stream sample times are calculated as:
 *
 *    SampleStartTime,i = SampleEndTime,i-1;
 *    SampleEndTime,i = t,i + s + LipSyncDelay
 *
 *    where t,i is the timestamp of frame i,
 *          s is the average frame period for a few latest frames.
 *
 * First frame is dropped and:
 *    SampleEndTime,0 = t,0
 *
 * Note that detected source frames timestamps are not exact and are delayed for 0..20ms, because of async
 * display update timer. This will be partially addressed by the average frame period method.
 *
 * Scaling
 *
 * Too large video areas are downsized and then JPEG compressed, because a large bitmap needs too much CPU
 * time to be compressed and because client can decode JPEGs up to a particular size.
 *
 */

/*
 * VideoHandler internal structures.
 */

typedef struct VHSTREAMDATA VHSTREAMDATA;

typedef struct SFDATA
{
    VHSTREAMDATA *pStreamData;

    int64_t i64Timestamp; /* When the frame has been detected. */

    uint8_t *pu8Bitmap;   /* 32bpp BGR0, buffer size is pStreamData->cbFrameOriginal */
} SFDATA;

typedef struct SFBBLOCK
{
   uint32_t u32Status;
   SFDATA sourceFrameData;
} SFBBLOCK;

// #define SFBSTATS

typedef struct SFB
{
    int iBlockToWrite;   /* Block which is written: 0 or 1. */
    SFBBLOCK aBlocks[2];
    VRDPLock *pLock; /* pointer to a VH lock, shared by all streams. */

#ifdef SFBSTATS
    uint64_t u64WriteStart;
    uint64_t u64ReadStart;

    uint64_t u64WriteMin;
    uint64_t u64WriteMax;
    uint64_t u64Writes;
    uint64_t u64WriteTime;

    uint64_t u64ReadMin;
    uint64_t u64ReadMax;
    uint64_t u64Reads;
    uint64_t u64ReadTime;

    int64_t i64Overwrite;
    int64_t i64SwitchOnWrite;
    int64_t i64SwitchOnRead;
#endif /* SFBSTATS */
} SFB;


/* VHSTREAMDATA::fu32StreamEvents */
#define VH_STREAM_EVENT_NONE     0x00000000
#define VH_STREAM_EVENT_GEOMETRY 0x00000001
#define VH_STREAM_EVENT_REGION   0x00000002


/* A video stream instance, which is created for every stream.
 * VHCONTEXT has a list of instances.
 * The structure is accessed under the vhLock and is referenced.
 */
struct VHSTREAMDATA
{
    VRDPPKT pkt; /* Referenced. */

    RTLISTNODE nodeStream; /* listStreams in VHCONTEXT. */

    uint32_t cbFrameOriginal; /* Size of the original 32bpp frame bitmap. The same for all frames because frame size is constant.*/
    uint32_t cbFrameScaled; /* Size of the scaled frame bitmap. The same for all frames because frame size is constant.*/

    SFB sfb;

    uint32_t u32SourceStreamStatus;

    /* Parameters and events must be updated under vhLock. */
    VHSTREAMPARMS parms;
    uint32_t fu32StreamEvents;

    VHOUTPUTFRAME *pLatestFrame; /* Saved latest processed frame. */
};

#define VHSTATCALLSTREAMDATA(sd, f, a) VHSTATCALL((sd)->parms.pCtx->pServer, f, a)

typedef struct VHOUTPUTTIMELINE
{
    /* Sample times. */
    bool fTimestampPrev;            /* Whether the previous frame timestamp is valid. */
    int64_t i64TimestampPrev;       /* Timestamp of the previous frame. */

    int64_t i64SampleEndTimePrev;   /* When last sample has ended. */

    /* Average period between frames. */
    int cDeltasHistory;  /* How many deltas to average. */
    int64_t i64DeltaSum; /* Calculated: sum of deltas between frames. */
    int cDeltas;         /* Calculated: how many deltas summed ( <= cDeltasHistory ). */
    int64_t i64Delta;    /* Calculated: average delta i64DeltaSum/cDeltas. */
} VHOUTPUTTIMELINE;

/* Video stream data used only by OUTPUT thread.
 * The pStreamData is referenced and should be accessed under vhLock.
 */
typedef struct VHOUTPUTSTREAM
{
    RTLISTNODE NodeOutputStream;

    enum StatusVideoStream enmStatus;

    RTLISTNODE ListOutputFrames; /* VHOUTPUTFRAME structures */

    int iJPEGQuality;
    VEJPEG *pEncoder;

    VHOUTPUTTIMELINE timeline;

    uint8_t *pu8BitmapScaled; /* Allocated if downscaling is needed, otherwise NULL. */
    BMPSCALE bmpScale; /* Initialized if downscaling is required. */

    VHSTREAMDATA *pStreamData;

    VHSTREAMPARMS parms; /* Copy of VHSTREAMDATA::parms, updated by OUTPUT thread under vhLock. */
} VHOUTPUTSTREAM;

/*
 * Source Frame Buffer.
 */
#define SFB_STATUS_EMPTY   0
#define SFB_STATUS_FULL    1
#define SFB_STATUS_WRITING 2
#define SFB_STATUS_READING 3

#define SOURCE_STREAM_CREATED 0
#define SOURCE_STREAM_READY   1
#define SOURCE_STREAM_DELETED 2
#define SOURCE_STREAM_FAILED  3


static int sfbLock(SFB *pSFB)
{
    return VRDPLock::Lock(pSFB->pLock);
}

static void sfbUnlock(SFB *pSFB)
{
    VRDPLock::Unlock(pSFB->pLock);
}

static SFBBLOCK *sfbWriteBegin(SFB *pSFB)
{
    SFBBLOCK *pReturnBlock = NULL;

    int rc = sfbLock(pSFB);

    if (RT_SUCCESS(rc))
    {
        /* Get the data pointer for the block to write and mark the block for writing. */
        SFBBLOCK *pBlock = &pSFB->aBlocks[pSFB->iBlockToWrite];

        Assert(pBlock->u32Status == SFB_STATUS_FULL || pBlock->u32Status == SFB_STATUS_EMPTY);

#ifdef SFBSTATS
        if (pBlock->u32Status == SFB_STATUS_FULL)
        {
            pSFB->i64Overwrite++;
        }
#endif /* SFBSTATS */

        pBlock->u32Status = SFB_STATUS_WRITING;
        pReturnBlock = pBlock;

#ifdef SFBSTATS
        pSFB->u64WriteStart = VRDPTimeNanoTS();
#endif /* SFBSTATS */

        sfbUnlock(pSFB);
    }

    return pReturnBlock;
}

static void sfbWriteEnd(SFB *pSFB, SFBBLOCK *pBlock, bool fSuccess)
{
    int rc = sfbLock(pSFB);

    if (RT_SUCCESS(rc))
    {
        AssertMsg(pBlock->u32Status == SFB_STATUS_WRITING, ("status %d\n", pBlock->u32Status));

        pBlock->u32Status = fSuccess? SFB_STATUS_FULL: SFB_STATUS_EMPTY;

        int iNextBlock = (pSFB->iBlockToWrite + 1) & 1;

        SFBBLOCK *pNextBlock = &pSFB->aBlocks[iNextBlock];

        if (pNextBlock->u32Status == SFB_STATUS_EMPTY)
        {
            pSFB->iBlockToWrite = iNextBlock;
#ifdef SFBSTATS
            pSFB->i64SwitchOnWrite++;
#endif /* SFBSTATS */
        }
        else
        {
            Assert(pNextBlock->u32Status == SFB_STATUS_FULL || pNextBlock->u32Status == SFB_STATUS_READING);
        }

#ifdef SFBSTATS
        uint64_t u64Duration = VRDPTimeNanoTS() - pSFB->u64WriteStart;

        if (pSFB->u64WriteMin > u64Duration)
        {
            pSFB->u64WriteMin = u64Duration;
        }
        if (pSFB->u64WriteMax < u64Duration)
        {
            pSFB->u64WriteMax = u64Duration;
        }

        pSFB->u64Writes++;
        pSFB->u64WriteTime += u64Duration;
#endif /* SFBSTATS */

        sfbUnlock(pSFB);
    }

    return;
}

static SFBBLOCK *sfbReadBegin(SFB *pSFB)
{
    SFBBLOCK *pReturnBlock = NULL;

    int rc = sfbLock(pSFB);

    if (RT_SUCCESS(rc))
    {
         /* Read can be done only from the block which is not for writing. */
        int iBlockToRead = (pSFB->iBlockToWrite + 1) & 1;

        SFBBLOCK *pBlock = &pSFB->aBlocks[iBlockToRead];

        /* It was either already written or never written. */
        AssertMsg(pBlock->u32Status == SFB_STATUS_FULL || pBlock->u32Status == SFB_STATUS_EMPTY, ("status %d\n", pBlock->u32Status));

        if (pBlock->u32Status == SFB_STATUS_FULL)
        {
            pBlock->u32Status = SFB_STATUS_READING;
            pReturnBlock = pBlock;
        }
        else
        {
            /* There is no block to read. Do nothing. */
        }

#ifdef SFBSTATS
        pSFB->u64ReadStart = VRDPTimeNanoTS();
#endif /* SFBSTATS */

        sfbUnlock(pSFB);
    }

    return pReturnBlock;
}

static void sfbReadEnd(SFB *pSFB, SFBBLOCK *pBlock)
{
    int rc = sfbLock(pSFB);

    if (RT_SUCCESS(rc))
    {
        AssertMsg(pBlock->u32Status == SFB_STATUS_READING, ("status %d\n", pBlock->u32Status));

        pBlock->u32Status = SFB_STATUS_EMPTY;

        /* It is ok to write to the block now, if the current write block is already full. */
        if (pSFB->aBlocks[pSFB->iBlockToWrite].u32Status == SFB_STATUS_FULL)
        {
            pSFB->iBlockToWrite = (pSFB->iBlockToWrite + 1) & 1;
            Assert(pBlock == &pSFB->aBlocks[pSFB->iBlockToWrite]);
#ifdef SFBSTATS
            pSFB->i64SwitchOnRead++;
#endif /* SFBSTATS */
        }

#ifdef SFBSTATS
        uint64_t u64Duration = VRDPTimeNanoTS() - pSFB->u64ReadStart;

        if (pSFB->u64ReadMin > u64Duration)
        {
            pSFB->u64ReadMin = u64Duration;
        }
        if (pSFB->u64ReadMax < u64Duration)
        {
            pSFB->u64ReadMax = u64Duration;
        }

        pSFB->u64Reads++;
        pSFB->u64ReadTime += u64Duration;
#endif /* SFBSTATS */

        sfbUnlock(pSFB);
    }

    return;
}

static void sfbUninit(SFB *pSFB)
{
#ifdef SFBSTATS
    LogRel(("SFBSTATS: w %llu, r %llu, o %llu, sw %llu, sr %llu\n",
            pSFB->u64Writes, pSFB->u64Reads, pSFB->i64Overwrite, pSFB->i64SwitchOnWrite, pSFB->i64SwitchOnRead));
    LogRel(("SFBSTATS: w %llu, %llu/w (min %llu, max %llu), r %llu, %llu/r (min %llu, max %llu)\n",
            pSFB->u64WriteTime, pSFB->u64Writes? pSFB->u64WriteTime / pSFB->u64Writes: 0,
            pSFB->u64WriteMin, pSFB->u64WriteMax,
            pSFB->u64ReadTime, pSFB->u64Reads? pSFB->u64ReadTime / pSFB->u64Reads: 0,
            pSFB->u64ReadMin, pSFB->u64ReadMax));
#endif /* SFBSTATS */

    unsigned i;
    for (i = 0; i < RT_ELEMENTS(pSFB->aBlocks); i++)
    {
        VRDPMemFree(pSFB->aBlocks[i].sourceFrameData.pu8Bitmap);
        pSFB->aBlocks[i].sourceFrameData.pu8Bitmap = NULL;
    }

    RT_ZERO(pSFB);
}

static int sfbInit(SFB *pSFB, VRDPLock *pLock, VHSTREAMDATA *pStreamData)
{
    int rc = VINF_SUCCESS;

    unsigned i;

    /* Initialize the structure. */
    pSFB->iBlockToWrite = 0;

    for (i = 0; i < RT_ELEMENTS(pSFB->aBlocks); i++)
    {
        pSFB->aBlocks[i].u32Status = SFB_STATUS_EMPTY;
        pSFB->aBlocks[i].sourceFrameData.pStreamData = pStreamData;

        pSFB->aBlocks[i].sourceFrameData.i64Timestamp = 0;

        pSFB->aBlocks[i].sourceFrameData.pu8Bitmap = NULL;
    }

    pSFB->pLock = pLock;

#ifdef SFBSTATS
    pSFB->u64WriteStart = 0;
    pSFB->u64ReadStart = 0;

    pSFB->u64WriteMin = UINT64_MAX;
    pSFB->u64WriteMax = 0;
    pSFB->u64Writes = 0;
    pSFB->u64WriteTime = 0;

    pSFB->u64ReadMin = UINT64_MAX;
    pSFB->u64ReadMax = 0;
    pSFB->u64Reads = 0;
    pSFB->u64ReadTime = 0;

    pSFB->i64Overwrite = 0;
    pSFB->i64SwitchOnWrite = 0;
    pSFB->i64SwitchOnRead = 0;

    LogRel(("SFBSTATS: init\n"));
#endif /* SFBSTATS */

    /* Allocate data buffers. */
    for (i = 0; i < RT_ELEMENTS(pSFB->aBlocks); i++)
    {
        pSFB->aBlocks[i].sourceFrameData.pu8Bitmap = (uint8_t *)VRDPMemAlloc(pStreamData->cbFrameOriginal);

        if (pSFB->aBlocks[i].sourceFrameData.pu8Bitmap == NULL)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
    }

    if (RT_FAILURE(rc))
    {
        sfbUninit(pSFB);
    }

    return rc;
}


/*
 * Video handler.
 */

static void vhParmsCleanup(VHSTREAMPARMS *pParms)
{
    VRDPMemFree(pParms->paVisibleRects);
    RT_ZERO(*pParms);
}

static int vhParmsUpdate(VHSTREAMPARMS *pDst, const VHSTREAMPARMS *pSrc)
{
    /* Use a temporary struct to copy everything but pointers. */
    VHSTREAMPARMS parms = *pSrc;

    if (pSrc->cVisibleRects != pDst->cVisibleRects)
    {
        /* Full update of dst visible region is needed. */
        RTRECT *paVisibleRects = NULL;
        if (pSrc->cVisibleRects)
        {
            paVisibleRects = (RTRECT *)VRDPMemAlloc(pSrc->cVisibleRects * sizeof(RTRECT));
            if (!paVisibleRects)
            {
                return VERR_NO_MEMORY;
            }
        }

        VRDPMemFree(pDst->paVisibleRects);

        pDst->paVisibleRects = paVisibleRects;
        pDst->cVisibleRects = pSrc->cVisibleRects;
    }

    if (pDst->cVisibleRects != 0)
    {
        memcpy(pDst->paVisibleRects, pSrc->paVisibleRects, pDst->cVisibleRects * sizeof(RTRECT));
    }

    parms.paVisibleRects = pDst->paVisibleRects;

    *pDst = parms;
    return VINF_SUCCESS;
}


/* The server methods.
 */
/* static */ void VRDPServer::PostVideoEvent(void *pvContext, int iCode, const void *pvData, uint32_t cbData)
{
    VRDPServer *pThis = (VRDPServer *)pvContext;
    pThis->PostOutputEvent(iCode, 0, pvData, cbData);
}

int VRDPServer::videoHandlerInit(void)
{
    VIDEOASSERT(m_pVideoHandler == NULL);
    int rc = videoHandlerCreate(&m_pVideoHandler, PostVideoEvent, this, this);
    if (RT_SUCCESS(rc))
    {
        shadowBufferRegisterVideoHandler(m_pVideoHandler);
        VRDPLOGREL(("Video channel initialized successfully.\n"));
    }
    else
    {
        VRDPLOGREL(("Failed to initialize video channel: %Rrc.\n", rc));
    }
    return rc;
}

void VRDPServer::videoHandlerUninit(void)
{
    shadowBufferUnregisterVideoHandler(m_pVideoHandler);
    videoHandlerDelete(m_pVideoHandler);
}

/** @thread OUTPUT */
void VRDPServer::ProcessOutputEvent(OutputUpdate *pUpdate)
{
    VIDEOASSERT (pUpdate);

    switch (pUpdate->iCode)
    {
        case VRDP_OUTPUT_VIDEO_STREAM_EVENT:
        {
            VideoHandlerStreamEvent(*(uint32_t *)pUpdate->pvData);
        } break;
        case VRDP_OUTPUT_VIDEO_STREAM_END:
            AssertPtr(pUpdate->pvData);
            VIDEOASSERT(pUpdate->cbData == sizeof(uint32_t));
            VideoHandlerSourceStreamEnd(*(uint32_t *)pUpdate->pvData);
            break;
        case VRDP_OUTPUT_VIDEO_STREAM_DISABLE:
            VIDEOLOG(("VH: VRDP_OUTPUT_VIDEO_STREAM_DISABLE\n"));
            /* Sets a flag which leads to a videoDetectorReset for all streams on EMT. */
            shadowBufferVideoDetectorCmd(VD_CMD_DISABLE);
            /* Also reset all output streams. */
            videoHandlerResetOutput(VideoHandler(), this);
            break;
        case VRDP_OUTPUT_VIDEO_STREAM_ENABLE:
            VIDEOLOG(("VH: VRDP_OUTPUT_VIDEO_STREAM_ENABLE\n"));
            shadowBufferVideoDetectorCmd(VD_CMD_ENABLE);
            /* Restart direct streams. */
            VideoHandlerStreamRestart();
            break;
        case VRDP_OUTPUT_VIDEO_COMPRESSION_RATE:
            AssertPtr(pUpdate->pvData);
            VIDEOASSERT(pUpdate->cbData == sizeof(uint8_t));
            VideoHandlerCompressionRate(*(uint8_t *)pUpdate->pvData);
            break;
        case VRDP_OUTPUT_VIDEO_STREAM_SIZE_LIMIT:
        {
            AssertPtr(pUpdate->pvData);
            VHEVENTSIZELIMIT *pData = (VHEVENTSIZELIMIT *)pUpdate->pvData;
            VIDEOASSERT(pUpdate->cbData == sizeof(VHEVENTSIZELIMIT));
            VideoHandlerSizeLimit(pData);
        } break;
        case VRDP_OUTPUT_VIDEO_STREAM_RESTART:
        {
            VIDEOLOG(("VH: VRDP_OUTPUT_VIDEO_STREAM_RESTART\n"));
            VideoHandlerStreamRestart();
        } break;
        default:
            break;
    }
}

/* All timeline times are in 100 nanosecond units as required by the video redirection channel. */


static void vhOutputTimelineUpdateDelta(VHOUTPUTTIMELINE *pOutputTimeline, int64_t i64NewDelta)
{
    if (pOutputTimeline->cDeltas >= pOutputTimeline->cDeltasHistory)
    {
        /* Note: actually the correct calculation would be to add new delta and subtract
         *       the value of which was added pOutputTimeline->cDeltasMax samples ago.
         *       But ot simplify the code, the value is not saved and instead the current
         *       average is subtracted.
         */
        pOutputTimeline->i64DeltaSum += i64NewDelta - pOutputTimeline->i64Delta;
    }
    else
    {
        pOutputTimeline->cDeltas++;
        pOutputTimeline->i64DeltaSum += i64NewDelta;
    }

    pOutputTimeline->i64Delta = pOutputTimeline->i64DeltaSum / pOutputTimeline->cDeltas;
}

DECLHIDDEN(void) vhSourceComputeScaledSize(VHCONTEXT *pCtx, RGNRECT *pRectScaled, const RGNRECT *pRectOriginal, bool fDirect)
{
    pRectScaled->x = 0;
    pRectScaled->y = 0;

    uint32_t u32OriginalSquare = pRectOriginal->w * pRectOriginal->h;

    /* Do not downscale direct streams. */
    if (   !fDirect
        && pCtx->u32VideoRectSquareMax < u32OriginalSquare)
    {
        /* Proportionally downsize. */
        /// @todo a 32 bit sqrt
        pRectScaled->w = (uint32_t)sqrt((double)(pRectOriginal->w * pCtx->u32VideoRectSquareDownscale) / pRectOriginal->h);
        pRectScaled->h = (uint32_t)sqrt((double)(pRectOriginal->h * pCtx->u32VideoRectSquareDownscale) / pRectOriginal->w);
        VIDEOLOG(("SCALE: %dx%d -> %dx%d\n", pRectOriginal->w, pRectOriginal->h, pRectScaled->w, pRectScaled->h));
    }
    else
    {
        pRectScaled->w = pRectOriginal->w;
        pRectScaled->h = pRectOriginal->h;
    }
}

static bool vhLock(VHCONTEXT *pCtx)
{
    if (pCtx)
    {
        return RT_SUCCESS(VRDPLock::Lock(pCtx->pLock));
    }

    return false;
}

static void vhUnlock(VHCONTEXT *pCtx)
{
    if (pCtx)
    {
        VRDPLock::Unlock(pCtx->pLock);
    }
}

static void vhStreamDataDelete(VRDPPKT *pPkt)
{
    VHSTREAMDATA *pStreamData = (VHSTREAMDATA *)pPkt;

    VHSTATCALLSTREAMDATA(pStreamData, DeleteStreamCtx, (pStreamData->parms.pStatCtx, true));

    if (pStreamData->pLatestFrame)
    {
        VRDPPktRelease(&pStreamData->pLatestFrame->pkt);
    }

    vhParmsCleanup(&pStreamData->parms);
    sfbUninit(&pStreamData->sfb);
}

/* Common stream data structures. */
static VHSTREAMDATA *vhStreamDataCreate(VHCONTEXT *pCtx,
                                        const RGNRECT *prect,
                                        uint32_t u32SourceStreamId,
                                        unsigned uScreenId,
                                        bool fDirect,
                                        VHSTREAMCALLBACKDATA *pCallbackData)
{

    VHSTREAMDATA *pStreamData = (VHSTREAMDATA *)VRDPMemAllocZ(sizeof(VHSTREAMDATA));

    if (pStreamData)
    {
        VRDPPktInit(&pStreamData->pkt, 0, sizeof(VHSTREAMDATA), vhStreamDataDelete, NULL);
        VRDPPktAddRef(&pStreamData->pkt);

        pStreamData->parms.pCtx             = pCtx;
        pStreamData->parms.u32VideoStreamId = u32SourceStreamId;
        pStreamData->parms.uScreenId        = uScreenId;
        pStreamData->parms.rectOriginal     = *prect;
        vhSourceComputeScaledSize(pCtx, &pStreamData->parms.rectScaled, &pStreamData->parms.rectOriginal, fDirect);
        pStreamData->parms.fDirect = fDirect;
        if (pCallbackData)
        {
            pStreamData->parms.callbackData = *pCallbackData;
        }
        pStreamData->parms.pStatCtx         = NULL;

        pStreamData->cbFrameOriginal  = pStreamData->parms.rectOriginal.w * pStreamData->parms.rectOriginal.h * 4;
        pStreamData->cbFrameScaled    = pStreamData->parms.rectScaled.w * pStreamData->parms.rectScaled.h * 4;

        pStreamData->fu32StreamEvents = VH_STREAM_EVENT_NONE;

        int rc = RTUuidCreate(&pStreamData->parms.uuid);

        if (RT_SUCCESS(rc))
        {
            rc = sfbInit(&pStreamData->sfb, pCtx->pLockSFB, pStreamData);
        }

        if (RT_FAILURE(rc))
        {
            VRDPMemFree(pStreamData);
            pStreamData = NULL;
        }
    }

    return pStreamData;
}

static VHSTREAMDATA *vhStreamById(VHCONTEXT *pCtx, uint32_t u32StreamId)
{
    if (u32StreamId == 0)
    {
        return NULL;
    }

    VHSTREAMDATA *pStream = NULL;

    if (vhLock(pCtx))
    {
        VHSTREAMDATA *pIter;
        RTListForEach(&pCtx->listStreams, pIter, VHSTREAMDATA, nodeStream)
        {
            if (u32StreamId == pIter->parms.u32VideoStreamId)
            {
                VRDPPktAddRef(&pIter->pkt);
                pStream = pIter;
                break;
            }
        }

        vhUnlock(pCtx);
    }

    return pStream;
}

/* Video detector informed that there is a new video frame for the stream.
 * Copy the frame to a buffer for OUTPUT thread.
 * @thread APP
 */
bool videoHandlerSourceFrame(VHCONTEXT *pCtx,
                             uint32_t u32SourceStreamId,
                             int64_t i64Timestamp,
                             const RGNRECT *pRect,
                             PFNHANDLERBITMAPREAD pfnBitmapRead,
                             void *pvBitmapRead)
{
    RT_NOREF1(pRect);
    if (!pCtx)
    {
        VIDEOLOG(("VH: frame ignored, no clients!!!\n"));
        return false;
    }

    VHSTREAMDATA *pStream = vhStreamById(pCtx, u32SourceStreamId);

    if (!pStream)
    {
        VIDEOLOG(("VH: frame ignored, no stream!!!\n"));
        return false;
    }

    VHSTATCALL(pCtx->pServer, SourceFrameBegin, (pStream->parms.pStatCtx, i64Timestamp));

    /* Copy the frame to the current buffer. */
    SFBBLOCK *pSFBBlock = sfbWriteBegin(&pStream->sfb);

    if (pSFBBlock == NULL)
    {
        VHSTATCALL(pCtx->pServer, SourceFrameEnd, (pStream->parms.pStatCtx, false));
        VRDPPktRelease(&pStream->pkt);
        VIDEOLOG(("VH: frame ignored, no SFB!!!\n"));
        return false;
    }

    /* Copy the bitmap. No downscale here. */
    int rc = pfnBitmapRead(pvBitmapRead,
                           pSFBBlock->sourceFrameData.pu8Bitmap,
                           pStream->cbFrameOriginal);

    if (RT_SUCCESS(rc))
    {
        VIDEOLOG(("VH: frame written\n"));
        pSFBBlock->sourceFrameData.i64Timestamp = i64Timestamp;

        sfbWriteEnd(&pStream->sfb, pSFBBlock, true);
    }
    else
    {
        VIDEOLOG(("VH: Failed to read bitmap %Rrc!!!\n", rc));

        sfbWriteEnd(&pStream->sfb, pSFBBlock, false);

        VHSTATCALL(pCtx->pServer, SourceFrameEnd, (pStream->parms.pStatCtx, false));
        VRDPPktRelease(&pStream->pkt);
        return false;
    }

    VHSTATCALL(pCtx->pServer, SourceFrameEnd, (pStream->parms.pStatCtx, true));

    VRDPPktRelease(&pStream->pkt);
    return true;
}

static bool vhIsDownscaleRequired(VHCONTEXT *pCtx, bool fDirect, uint32_t u32OriginalSquare)
{
    if (   !fDirect
        && pCtx->u32VideoRectSquareMax < u32OriginalSquare)
    {
        return true;
    }

    return false;
}

/* Video detector found a new video.
 * @thread APP
 */
bool videoHandlerSourceStreamStart(VHCONTEXT *pCtx,
                                   uint32_t u32SourceStreamId,
                                   const RGNRECT *prect,
                                   int64_t timeStart,
                                   unsigned uScreenId,
                                   bool fDirect,
                                   VHSTREAMCALLBACKDATA *pCallbackData)
{
    if (!pCtx)
    {
        return false;
    }

    uint32_t u32OriginalSquare = prect->w * prect->h;

    /* Filter streams which will not be supported by the client anyway. */
    if (   u32OriginalSquare < pCtx->u32VideoRectSquareMin
        || prect->w < pCtx->u32VideoRectWidthMin
        || prect->h < pCtx->u32VideoRectHeightMin
       )
    {
        return false;
    }

    if (pCtx->iDownscaleProtection != 0)
    {
        /* Videos, which will be downscaled, must be processed separately.
         * If the size is equal to the shadowbuffer size, then consider it
         * a fullscreen video.
         * But if the size is between fullscreen and downscale threshold,
         * then do not do the video, because actually this could be a
         * application window, and it will be unreadable when downscaled.
         */
        if (vhIsDownscaleRequired(pCtx, fDirect, u32OriginalSquare))
        {
            /* Video will be downscaled. Check if it looks like a fullscreen video. */
            if (pCtx->iDownscaleProtection == 2)
            {
                /* All downscaled videos must be rejected. */
                VIDEOLOG(("VH: Reject downscaled video!!!\n"));
                return false;
            }

            /* Only non fullscreen downscaled videos must be rejected. */
            RGNRECT rectSB;
            shadowBufferQueryRect (uScreenId, &rectSB);
            if (rectSB.w >= prect->w && rectSB.w - prect->w < 4)
            {
                /* Video looks like a fullscreen. */
            }
            else
            {
                VIDEOLOG(("VH: Reject downscaled video sb %d rect %d!!!\n", rectSB.w, prect->w));
                return false;
            }
        }
    }

    VHSTREAMDATA *pStream = vhStreamDataCreate(pCtx, prect, u32SourceStreamId, uScreenId, fDirect, pCallbackData);
    if (!pStream)
    {
        return false;
    }

    VHSTATCALL(pCtx->pServer, CreateStreamCtx, (&pStream->parms.pStatCtx, timeStart, prect, u32SourceStreamId));

    pStream->u32SourceStreamStatus = SOURCE_STREAM_CREATED;

    /* The new stream must be added to the streams list under lock. */
    bool fRc = true;
    if (vhLock(pCtx))
    {
        RTListPrepend(&pCtx->listStreams, &pStream->nodeStream);
        vhUnlock(pCtx);
    }
    else
    {
        VHSTATCALL(pCtx->pServer, DeleteStreamCtx, (pStream->parms.pStatCtx, false));
        VRDPPktRelease(&pStream->pkt);
        fRc = false;
    }
    return fRc;
}

/* Video detector found out that the video has ended.
 * @thread APP
 */
void videoHandlerSourceStreamStop(VHCONTEXT *pCtx,
                                  uint32_t u32SourceStreamId)
{
    if (!pCtx)
    {
        return;
    }

    VHSTREAMDATA *pStream = vhStreamById(pCtx, u32SourceStreamId);

    if (pStream)
    {
        /* Remove the stream under the lock. */
        if (vhLock(pCtx))
        {
            RTListNodeRemove(&pStream->nodeStream);
            vhUnlock(pCtx);

            VRDPPktRelease(&pStream->pkt); /* For being in the list. */

            /* Tell the OUTPUT thread that the stream ends. */
            pCtx->pfnPostVideoEvent(pCtx->pvPostVideoEvent, VRDP_OUTPUT_VIDEO_STREAM_END, &u32SourceStreamId, sizeof (u32SourceStreamId));
        }

        VRDPPktRelease(&pStream->pkt); /* For vhStreamById. */
    }
}

bool videoHandlerDirectStreamStart(uint32_t *pu32DirectStreamId,
                                   const RGNRECT *pRect,
                                   unsigned uScreenId,
                                   VHSTREAMCALLBACKDATA *pCallbackData)
{
    VHCONTEXT *pCtx = shadowBufferGetVideoHandler();
    uint32_t u32StreamId = idCreate();
    int64_t i64Timestamp = VRDPTimeMilliTS();

    bool fAccepted = videoHandlerSourceStreamStart(pCtx, u32StreamId, pRect, i64Timestamp, uScreenId, true, pCallbackData);

    VIDEOLOG(("VH: videoHandlerDirectStreamStart: fAccepted %d, id %d, %d,%d %dx%d\n",
              fAccepted, u32StreamId,
              pRect->x, pRect->y,
              pRect->w, pRect->h));

    if (fAccepted)
    {
        *pu32DirectStreamId = u32StreamId;
    }

    return fAccepted;
}

void videoHandlerDirectFrame(uint32_t u32DirectStreamId,
                             const RGNRECT *pRect,
                             PFNHANDLERBITMAPREAD pfnBitmapRead,
                             void *pvBitmapRead)
{
    VHCONTEXT *pCtx = shadowBufferGetVideoHandler();

    if (!pCtx)
    {
        return;
    }

    int64_t i64Timestamp = VRDPTimeMilliTS();

    videoHandlerSourceFrame(pCtx, u32DirectStreamId, i64Timestamp,
                            pRect, pfnBitmapRead, pvBitmapRead);
}

void videoHandlerDirectStreamStop(uint32_t u32DirectStreamId)
{
    VHCONTEXT *pCtx = shadowBufferGetVideoHandler();

    if (!pCtx)
    {
        return;
    }

    VIDEOLOG(("VH: videoHandlerDirectStreamStop: id %d\n", u32DirectStreamId));

    videoHandlerSourceStreamStop(pCtx, u32DirectStreamId);
}

void videoHandlerDirectRegion(uint32_t u32DirectStreamId,
                              uint32_t cRects,
                              const RTRECT *paRects)
{
    VHCONTEXT *pCtx = shadowBufferGetVideoHandler();

    VIDEOLOG(("VH: videoHandlerDirectRegion: id %d, cRects %d\n", u32DirectStreamId, cRects));

    if (   !pCtx
        || cRects > 65536)
    {
        return;
    }

    /* Make a copy of the region. cRects == 0 means hide. */
    RTRECT *paVisibleRects = NULL;
    if (cRects)
    {
        paVisibleRects = (RTRECT *)VRDPMemAlloc(cRects * sizeof(RTRECT));
        if (!paVisibleRects)
        {
            return;
        }
        memcpy(paVisibleRects, paRects, cRects * sizeof(RTRECT));
    }

    /* Update the stream parameters under the lock. */
    bool fUpdated = false;
    if (vhLock(pCtx))
    {
        /* Find the stream. */
        VHSTREAMDATA *pStream = vhStreamById(pCtx, u32DirectStreamId);

        if (pStream)
        {
            RTRECT *paTmp = pStream->parms.paVisibleRects;
            pStream->parms.paVisibleRects = paVisibleRects;
            paVisibleRects = paTmp;

            pStream->parms.cVisibleRects = cRects;

            pStream->fu32StreamEvents |= VH_STREAM_EVENT_REGION;

            VRDPPktRelease(&pStream->pkt); /* For vhStreamById. */

            VIDEOLOG(("VH: videoHandlerDirectRegion: updated id %d, cRects %d\n", u32DirectStreamId, cRects));
            fUpdated = true;
        }

        vhUnlock(pCtx);
    }

    /* paVisibleRects is set by the above code: either new buffer or old buffer to be freed. */
    VRDPMemFree(paVisibleRects);

    if (fUpdated)
    {
        pCtx->pfnPostVideoEvent(pCtx->pvPostVideoEvent, VRDP_OUTPUT_VIDEO_STREAM_EVENT,
                                &u32DirectStreamId, sizeof(u32DirectStreamId));
    }

    return;
}

int videoHandlerDirectGeometry(uint32_t u32DirectStreamId,
                               const RTRECT *pRect)
{
    VHCONTEXT *pCtx = shadowBufferGetVideoHandler();

    if (   !pCtx
        || !pRect)
    {
        return VERR_INVALID_PARAMETER;
    }

    VIDEOLOG(("VH: videoHandlerDirectGeometry: id %d, %d,%d %dx%d\n",
              u32DirectStreamId,
              pRect->xLeft, pRect->yTop,
              pRect->xRight - pRect->xLeft,
              pRect->yBottom - pRect->yTop));

    /*
     * Support only the position change. If the size is changed then
     * the videochannel (TSMF) must be restarted, because a video
     * stream has a fixed frame dimension. In this case it is simpler
     * to let the upper level to just create a new image output instance.
     */

    int rc = VINF_SUCCESS;

    /* Access the stream parameters under the lock. */
    bool fUpdated = false;
    if (vhLock(pCtx))
    {
        /* Find the stream. */
        VHSTREAMDATA *pStream = vhStreamById(pCtx, u32DirectStreamId);

        if (pStream)
        {
            uint32_t w = pRect->xRight - pRect->xLeft;
            uint32_t h = pRect->yBottom - pRect->yTop;

            if (   pStream->parms.rectOriginal.w == w
                && pStream->parms.rectOriginal.h == h)
            {
                pStream->parms.rectOriginal.x = pRect->xLeft;
                pStream->parms.rectOriginal.y = pRect->yTop;

                /* Scaled rect, size of frame etc remain the same. */

                pStream->fu32StreamEvents |= VH_STREAM_EVENT_GEOMETRY;

                VIDEOLOG(("VH: videoHandlerDirectGeometry: updated id %d\n", u32DirectStreamId));
                fUpdated = true;
            }
            else
            {
                rc = VERR_NOT_SUPPORTED;
            }

            VRDPPktRelease(&pStream->pkt); /* For vhStreamById. */
        }
        else
        {
            rc = VERR_INVALID_PARAMETER;
        }

        vhUnlock(pCtx);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (fUpdated)
    {
        pCtx->pfnPostVideoEvent(pCtx->pvPostVideoEvent, VRDP_OUTPUT_VIDEO_STREAM_EVENT,
                                &u32DirectStreamId, sizeof(u32DirectStreamId));
    }

    return rc;
}

/*
 * Destroy any output streams for all clients.
 * @thread OUTPUT
 */
void videoHandlerResetOutput(VHCONTEXT *pCtx, VRDPServer *pServer)
{
    if (!pCtx)
    {
        return;
    }

    VHOUTPUTSTREAM *pIter;
    VHOUTPUTSTREAM *pNext;
    RTListForEachSafe(&pCtx->ListOutputStreams, pIter, pNext, VHOUTPUTSTREAM, NodeOutputStream)
    {
        pServer->VideoHandlerSourceStreamEnd(pIter->parms.u32VideoStreamId);
    }
}

static void vhOutputFrameFree(VRDPPKT *pPkt)
{
    VHOUTPUTFRAME *p = (VHOUTPUTFRAME *)pPkt;
    if (p->pu8JPEG)
    {
        VRDPMemFree(p->pu8JPEG);
    }
    VIDEOLOG(("free output frame %p\n", p));
}

static VHOUTPUTFRAME *vhOutputFrameAlloc(VHOUTPUTSTREAM *pOutputStream)
{
    NOREF(pOutputStream); /* A couple of preallocated VHOUTPUTFRAMEs. */
    VHOUTPUTFRAME *pOutputFrame = (VHOUTPUTFRAME *)VRDPMemAllocZ(sizeof(VHOUTPUTFRAME));
    if (pOutputFrame)
    {
        VRDPPktInit(&pOutputFrame->pkt, 0, sizeof(VHOUTPUTFRAME), vhOutputFrameFree, NULL);
        VRDPPktAddRef(&pOutputFrame->pkt);
    }
    return pOutputFrame;
}

static void vhPrepareOutputFrames(VHOUTPUTSTREAM *pOutputStream)
{
    /** @todo Proper output frame throttling: should use a pOutputStream->i64LastAddedFrameTimestamp;
     * should not add the new frame to ListOutputFrames if the frame is sooner than expected,
     * even if the ListOutputFrames is empty; should add a postponed frame if its too old to make sure
     * that the last frame is shown.
     */
    VHOUTPUTFRAME *pOutputFrame;

    /* Check if there is something to read from SFB. */
    SFBBLOCK *pBlock;
    unsigned i;

    /* Limit the number of processed frames to the number of buffers.
     * Otherwise it is possible that while a buffer is processed here, the other buffer will
     * be filled and this function will keep processing frames without forwarding them to the caller.
     */
    for (i = 0; i < RT_ELEMENTS(pOutputStream->pStreamData->sfb.aBlocks); i++)
    {
        pBlock = sfbReadBegin(&pOutputStream->pStreamData->sfb);

        if (pBlock == NULL)
        {
            break;
        }

        VIDEOLOG(("Read frame %p\n", pBlock));

        VHSTATCALLSTREAMDATA(pOutputStream->pStreamData, OutputFrameBegin, (pOutputStream->parms.pStatCtx, pBlock->sourceFrameData.i64Timestamp));

        pOutputFrame = vhOutputFrameAlloc(pOutputStream);

        if (pOutputFrame)
        {
            pOutputFrame->i64Timestamp = VH_TIMELINE_TIME_FROM_MS(pBlock->sourceFrameData.i64Timestamp);

            VHOUTPUTTIMELINE *pTimeline = &pOutputStream->timeline;
            bool fDropFrame = false;

            if (RT_LIKELY(pTimeline->fTimestampPrev))
            {
                /* This output stream already processed a frame. */
                VIDEOASSERT(pOutputStream->parms.fDirect || (pOutputFrame->i64Timestamp > pTimeline->i64TimestampPrev));
                int64_t i64NewDelta = pOutputFrame->i64Timestamp - pTimeline->i64TimestampPrev;

                VIDEOLOG(("i64NewDelta %lld, i64Delta %lld\n", i64NewDelta, pTimeline->i64Delta));

                vhOutputTimelineUpdateDelta(pTimeline, i64NewDelta);

                /// @todo average Timestamp jitter
                // int64_t i64AverageTimestamp = pTimeline->ai64TimestampHistory[pTimeline->iTimestampHistory] +
                //                               pTimeline->cDeltas * pTimeline->i64Delta;
                // pTimeline->ai64TimestampHistory[pTimeline->iTimestampHistory] = i64AverageTimestamp;
                // pTimeline->iTimestampHistory = (pTimeline->iTimestampHistory + 1) % pTimeline->cDeltasHistory;

                int64_t i64AverageTimestamp = pOutputFrame->i64Timestamp;

                pOutputFrame->i64SampleStartTime = pTimeline->i64SampleEndTimePrev;
                pOutputFrame->i64SampleEndTime = i64AverageTimestamp + pTimeline->i64Delta
                                                 + pOutputStream->parms.i64LipSyncDelay; /// @todo add i64LipSyncDelay once

                if (RT_UNLIKELY(pOutputFrame->i64SampleEndTime < pOutputFrame->i64SampleStartTime))
                {
                    /* Drop the frame. Adjust time value for further calculations. */
                    pOutputFrame->i64SampleStartTime = pOutputFrame->i64SampleEndTime;

                    /** @todo */
                    if (!pOutputStream->parms.fDirect)
                    {
                        fDropFrame = true;
                    }
                }
            }
            else
            {
                pTimeline->fTimestampPrev = true;

                /* Simply drop the very first frame. The sample is required for calculation of next frames. */
                /// @todo variable lipsync for first 10 frames, to avoid video discontinuity
                pOutputFrame->i64SampleStartTime = pOutputFrame->i64Timestamp + pOutputStream->parms.i64LipSyncDelay;
                pOutputFrame->i64SampleEndTime = pOutputFrame->i64SampleStartTime;

                /** @todo */
                if (!pOutputStream->parms.fDirect)
                {
                    fDropFrame = true;
                }
            }

            VIDEOLOG(("Timeline: delta %llu, SampleStart - Timestamp %llu, End - Start %llu\n",
                      pTimeline->i64Delta, pOutputFrame->i64SampleStartTime - pOutputFrame->i64Timestamp,
                      pOutputFrame->i64SampleEndTime - pOutputFrame->i64SampleStartTime));

            pTimeline->i64TimestampPrev = pOutputFrame->i64Timestamp;

            pTimeline->i64SampleEndTimePrev = pOutputFrame->i64SampleEndTime;

            int rc;
            if (RT_LIKELY(fDropFrame == false))
            {
                /* Downscale and compress the frame. */
                uint8_t *pu8Bitmap;
                if (pOutputStream->pu8BitmapScaled != NULL)
                {
                    /* Scale. */
                    pOutputStream->bmpScale.pfnScale(pOutputStream->bmpScale.pCtx,
                                                     pOutputStream->pu8BitmapScaled,
                                                     pBlock->sourceFrameData.pu8Bitmap,
                                                     pOutputStream->parms.rectOriginal.w * 4);

                    pu8Bitmap = pOutputStream->pu8BitmapScaled;
                }
                else
                {
                    pu8Bitmap = pBlock->sourceFrameData.pu8Bitmap;
                }

                rc =  VEJPEGEncodeFrameBGR0(pOutputStream->pEncoder,
                                            pOutputStream->iJPEGQuality,
                                            &pOutputFrame->pu8JPEG,
                                            &pOutputFrame->cbJPEG,
                                            pu8Bitmap,
                                            pOutputStream->parms.rectScaled.w,
                                            pOutputStream->parms.rectScaled.h);
            }
            else
            {
                rc = VERR_NOT_SUPPORTED;
            }

            if (RT_SUCCESS(rc))
            {
                /* Insert at the tail to keep the list sorted. */
                RTListAppend(&pOutputStream->ListOutputFrames, &pOutputFrame->NodeOutputFrame);
                VHSTATCALLSTREAMDATA(pOutputStream->pStreamData, OutputFrameEnd, (pOutputStream->parms.pStatCtx, true));
            }
            else
            {
                VRDPPktRelease(&pOutputFrame->pkt);
                VHSTATCALLSTREAMDATA(pOutputStream->pStreamData, OutputFrameEnd, (pOutputStream->parms.pStatCtx, false));
            }
        }
        else
        {
            VHSTATCALLSTREAMDATA(pOutputStream->pStreamData, OutputFrameEnd, (pOutputStream->parms.pStatCtx, false));
        }

        sfbReadEnd(&pOutputStream->pStreamData->sfb, pBlock);
    }

    /* Update the latest frame. */
    pOutputFrame = RTListGetLast(&pOutputStream->ListOutputFrames, VHOUTPUTFRAME, NodeOutputFrame);
    if (pOutputFrame)
    {
        if (pOutputStream->pStreamData->pLatestFrame)
        {
            VRDPPktRelease(&pOutputStream->pStreamData->pLatestFrame->pkt);
        }

        VRDPPktAddRef(&pOutputFrame->pkt);
        pOutputStream->pStreamData->pLatestFrame = pOutputFrame;
    }
}


static void vhOutputFrameCheckToSend(VHOUTPUTSTREAM *pOutputStream, RTLISTANCHOR *pListFramesToSend, int64_t i64NowTimeline)
{
    VHOUTPUTFRAME *pOutputFrame;
    VHOUTPUTFRAME *pNext;

    if (pOutputStream->parms.fDirect)
    {
        /* Send everything for direct streams. */
        RTListForEachSafe(&pOutputStream->ListOutputFrames, pOutputFrame, pNext, VHOUTPUTFRAME, NodeOutputFrame)
        {
            RTListNodeRemove(&pOutputFrame->NodeOutputFrame);
            /* Move the frame to the "to be sent" list. */
            RTListAppend(pListFramesToSend, &pOutputFrame->NodeOutputFrame);
        }
    }
    else
    {
        RTListForEachSafe(&pOutputStream->ListOutputFrames, pOutputFrame, pNext, VHOUTPUTFRAME, NodeOutputFrame)
        {
            /* Send everything that is not too old. */
            RTListNodeRemove(&pOutputFrame->NodeOutputFrame);

            if (RT_LIKELY(pOutputFrame->i64SampleEndTime >= i64NowTimeline))
            {
                /* Move the frame to the "to be sent" list. */
                RTListAppend(pListFramesToSend, &pOutputFrame->NodeOutputFrame);
            }
            else
            {
                /* Frame is too old. */
                VRDPPktRelease(&pOutputFrame->pkt);
            }
        }
    }
}

static void vhOutputFrameReleaseList(VHOUTPUTSTREAM *pOutputStream, RTLISTANCHOR *pList)
{
    NOREF(pOutputStream);

    VHOUTPUTFRAME *pOutputFrame;
    VHOUTPUTFRAME *pNext;
    RTListForEachSafe(pList, pOutputFrame, pNext, VHOUTPUTFRAME, NodeOutputFrame)
    {
        RTListNodeRemove(&pOutputFrame->NodeOutputFrame);
        VRDPPktRelease(&pOutputFrame->pkt);
    }
}


/** @thread OUTPUT */
static VHOUTPUTSTREAM *vhOutputStreamFindById(VHCONTEXT *pCtx, uint32_t u32VideoStreamId)
{
    VHOUTPUTSTREAM *pOutputStream = NULL;

    VHOUTPUTSTREAM *pIter;
    RTListForEach(&pCtx->ListOutputStreams, pIter, VHOUTPUTSTREAM, NodeOutputStream)
    {
        if (pIter->parms.u32VideoStreamId == u32VideoStreamId)
        {
            pOutputStream = pIter;
            break;
        }
    }

    return pOutputStream;
}

/** @thread APP */
int videoHandlerCreate(VHCONTEXT **ppCtx, PFNPOSTVIDEOEVENT pfnPostVideoEvent, void *pvPostVideoEvent, VRDPServer *pVRDPServer)
{
    int rc = VINF_SUCCESS;

    VHCONTEXT *pCtx = (VHCONTEXT *)VRDPMemAllocZ(sizeof (VHCONTEXT));

    if (!pCtx)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        pCtx->pServer = pVRDPServer;
        pCtx->cClients = 0;
        pCtx->i64VideoPeriodMinMS = 40;
        RTListInit(&pCtx->listStreams);
        RTListInit(&pCtx->ListOutputStreams);
        pCtx->pfnPostVideoEvent = pfnPostVideoEvent;
        pCtx->pvPostVideoEvent = pvPostVideoEvent;

        /** @todo configurable */
        pCtx->u32VideoRectSquareMax = 800*600;
        pCtx->u32VideoRectSquareDownscale = 640*480; // 160*120;

        /* Initial values are the same as in VD. */
        pCtx->u32VideoRectSquareMin = 32*32;
        pCtx->u32VideoRectWidthMin = 8;
        pCtx->u32VideoRectHeightMin = 8;

        /* 0 means that the configured default quality must be used. */
        pCtx->u32RequestedVideoQuality = 0;

        int64_t i64 = 0;
        queryPropertyInt64Def(pVRDPServer, "Property/VideoChannel/DownscaleProtection",
                              &i64, 0);

        if (0 <= i64 && i64 <= 2)
        {
            pCtx->iDownscaleProtection = (int)i64;
        }
        else
        {
            pCtx->iDownscaleProtection = 0;
        }

        if (pCtx->iDownscaleProtection)
        {
            VRDPLOGREL(("Property VideoChannel/DownscaleProtection is enabled: %d.\n", pCtx->iDownscaleProtection));
        }

        rc = VRDPLock::Create("VH", &pCtx->pLock);

        if (RT_SUCCESS(rc))
        {
            rc = VRDPLock::Create("VHSFB", &pCtx->pLockSFB);
        }
    }

    if (RT_SUCCESS(rc))
    {
        *ppCtx = pCtx;
    }
    else
    {
        videoHandlerDelete(pCtx);
        VRDPLOGREL(("Video initialization failed. %Rrc. Video channel remains disabled!!!\n", rc));
    }

    return rc;
}

/** @thread APP */
void videoHandlerDelete(VHCONTEXT *pCtx)
{
    if (pCtx)
    {
        VRDPLock::Delete(&pCtx->pLockSFB);
        VRDPLock::Delete(&pCtx->pLock);

        VRDPMemFree(pCtx);
    }
}

void videoHandlerNotifyClientConnect(VHCONTEXT *pCtx)
{
    ASMAtomicIncS32(&pCtx->cClients);
    VIDEOLOG(("new video redirection client %d\n", pCtx->cClients));
}

void videoHandlerNotifyClientDisconnect(VHCONTEXT *pCtx)
{
    int cClients = ASMAtomicDecS32(&pCtx->cClients);
    VIDEOLOG(("disconnected video redirection client %d\n", cClients));
    VIDEOASSERT(pCtx->cClients >= 0);
    if (cClients == 0)
    {
        shadowBufferCoverResetAll();
    }
}

/** @thread OUTPUT */
static int vhOutputStreamCreate(VHCONTEXT *pCtx,
                                VHSTREAMDATA *pStreamData,
                                uint32_t u32VideoQuality)
{
    VRDP_ASSERT_VH_CONTEXT_OUTPUT(pCtx);
    VIDEOASSERT(pCtx == pStreamData->parms.pCtx);

    VHOUTPUTSTREAM *pOutputStream = (VHOUTPUTSTREAM *)VRDPMemAllocZ(sizeof(VHOUTPUTSTREAM));

    if (!pOutputStream)
    {
        return VERR_NO_MEMORY;
    }

    /* Already under vhLock, so can copy the parameters. */
    int rc = vhParmsUpdate(&pOutputStream->parms, &pStreamData->parms);
    if (RT_FAILURE(rc))
    {
        /* vhOutputStreamFree(pOutputStream); */
        VRDPMemFree(pOutputStream);
        return rc;
    }

    rc = VEJPEGInit(&pOutputStream->pEncoder);
    if (RT_FAILURE(rc))
    {
        vhParmsCleanup(&pOutputStream->parms);
        VRDPMemFree(pOutputStream);
        return rc;
    }

    if (pStreamData->cbFrameScaled != pStreamData->cbFrameOriginal)
    {
        /* Downscale is required. Frames will be downscaled to the buffer before compression. */
        pOutputStream->pu8BitmapScaled = (uint8_t *)VRDPMemAlloc(pStreamData->cbFrameScaled);
        if (pOutputStream->pu8BitmapScaled == NULL)
        {
            vhParmsCleanup(&pOutputStream->parms);
            VRDPMemFree(pOutputStream);
            return VERR_NO_MEMORY;
        }

        /* Initialize an appropriate downscaling method. */
        rc = BmpScaleInit(&pOutputStream->bmpScale,
                          pOutputStream->parms.rectScaled.w, pOutputStream->parms.rectScaled.h,
                          pOutputStream->parms.rectOriginal.w, pOutputStream->parms.rectOriginal.h,
                          BMPSCALE_AUTO);

        if (RT_FAILURE(rc))
        {
            vhParmsCleanup(&pOutputStream->parms);
            VRDPMemFree(pOutputStream);
            return rc;
        }
    }
    else
    {
        VHSTATLOG(("VHSTAT: unscaled %dx%d\n", pOutputStream->parms.rectOriginal.w, pOutputStream->parms.rectOriginal.h));
        pOutputStream->pu8BitmapScaled = NULL;
    }

    if (u32VideoQuality == 0)
    {
        u32VideoQuality = 75;
    }
    else
    {
        /* The SIMD JPEG library does not work correctly with higher (> 95) quality values. */
        u32VideoQuality = RT_CLAMP(u32VideoQuality, 10, 95);
    }

    pOutputStream->iJPEGQuality = u32VideoQuality;
    pOutputStream->parms.i64LipSyncDelay = pOutputStream->parms.fDirect?
                                         VH_TIMELINE_TIME_FROM_MS(0):
                                         VH_TIMELINE_TIME_FROM_MS(500);
    pOutputStream->timeline.cDeltasHistory = 10;
    RTListInit(&pOutputStream->ListOutputFrames);
    pOutputStream->enmStatus = StatusVideoStream_Ready;

    pOutputStream->pStreamData = pStreamData;

    RTListPrepend(&pCtx->ListOutputStreams, &pOutputStream->NodeOutputStream);

    return VINF_SUCCESS;
}

static void vhOutputStreamDelete(VHCONTEXT *pCtx, VHOUTPUTSTREAM *pOutputStream)
{
    RT_NOREF1(pCtx);
    VRDP_ASSERT_VH_CONTEXT_OUTPUT(pCtx);
    if (pOutputStream)
    {
        RTListNodeRemove(&pOutputStream->NodeOutputStream);

        /* Delete pending output frames, usually there should be no frames pending. */
        vhOutputFrameReleaseList(pOutputStream, &pOutputStream->ListOutputFrames);

        vhParmsCleanup(&pOutputStream->parms);

        VEJPEGDestroy(pOutputStream->pEncoder);
        VRDPMemFree(pOutputStream->pu8BitmapScaled);
        if (pOutputStream->bmpScale.pfnDestroy)
        {
            pOutputStream->bmpScale.pfnDestroy(pOutputStream->bmpScale.pCtx);
        }
        VRDPPktRelease(&pOutputStream->pStreamData->pkt);
        VRDPMemFree(pOutputStream);
    }
}


/*
 * Called either result of VRDP_OUTPUT_VIDEO_STREAM_END, that is when detector
 * finds out that the stream has ended.
 * @thread OUTPUT
 */
void VRDPServer::VideoHandlerSourceStreamEnd(uint32_t u32VideoStreamId)
{
    VHCONTEXT *pCtx = m_pVideoHandler;

    if (!pCtx)
    {
        return;
    }

    VIDEOLOG(("StreamEnd id %d\n", u32VideoStreamId));

    VHOUTPUTSTREAM *pOutputStream = vhOutputStreamFindById(pCtx, u32VideoStreamId);
    if (!pOutputStream)
    {
        VIDEOLOG(("id %d not found!!!\n", u32VideoStreamId));
        return;
    }

    /* The source stream VHSOURCESTREAM has already been deleted.
     * Shutdown TSMF presentation on clients and delete the corresponding VHOUTPUTSTREAM.
     */
    VRDPClient *pClient = NULL;
    uint32_t u32ClientId = 0;

    while ((pClient = m_clientArray.ThreadContextGetNextClient (&u32ClientId, VRDP_CONTEXT_OUTPUT)) != NULL)
    {
        if (pClient->IsScreenIdMatched (pOutputStream->parms.uScreenId))
        {
            pClient->VideoStreamEnd (&pOutputStream->parms);
        }

        pClient->ThreadContextRelease (VRDP_CONTEXT_OUTPUT);
    }

    vhOutputStreamDelete(pCtx, pOutputStream);
}

/*
 * Called as result of VRDP_OUTPUT_VIDEO_STREAM_EVENT.
 * @thread OUTPUT
 */
void VRDPServer::VideoHandlerStreamEvent(uint32_t u32VideoStreamId)
{
    VHCONTEXT *pCtx = m_pVideoHandler;

    if (!pCtx)
    {
        return;
    }

    VIDEOLOG(("VideoHandlerStreamEvent id %d\n", u32VideoStreamId));

    /* If the output stream exists, then update its parameters.
     * If it does not exist, then the parameters will be taken when the output stream is created.
     */
    VHOUTPUTSTREAM *pOutputStream = vhOutputStreamFindById(pCtx, u32VideoStreamId);
    if (pOutputStream)
    {
        if (vhLock(pCtx))
        {
            uint32_t fu32StreamEvents = VH_STREAM_EVENT_NONE;

            /* Make a copy of parameters. */
            int rc = vhParmsUpdate(&pOutputStream->parms, &pOutputStream->pStreamData->parms);

            if (RT_SUCCESS(rc))
            {
                fu32StreamEvents = pOutputStream->pStreamData->fu32StreamEvents;
                pOutputStream->pStreamData->fu32StreamEvents = VH_STREAM_EVENT_NONE;
            }

            VIDEOLOG(("VideoHandlerStreamEvent rc %Rrc @%d,%d\n",
                      rc, pOutputStream->parms.rectOriginal.x, pOutputStream->parms.rectOriginal.y));

            vhUnlock(pCtx);

            /* Deliver the update to the clients. */
            VRDPClient *pClient = NULL;
            uint32_t u32ClientId = 0;

            while ((pClient = m_clientArray.ThreadContextGetNextClient(&u32ClientId, VRDP_CONTEXT_OUTPUT)) != NULL)
            {
                if (pClient->IsScreenIdMatched(pOutputStream->parms.uScreenId))
                {
                    if (fu32StreamEvents & (VH_STREAM_EVENT_REGION | VH_STREAM_EVENT_GEOMETRY))
                    {
                        /** @todo only pass parms and the event mask.
                         *       Each stream must have its copy of event mask.
                         */
                        RTRECT rectOriginal;
                        rectOriginal.xLeft   = pOutputStream->parms.rectOriginal.x;
                        rectOriginal.yTop    = pOutputStream->parms.rectOriginal.y;
                        rectOriginal.xRight  = rectOriginal.xLeft + pOutputStream->parms.rectOriginal.w;
                        rectOriginal.yBottom = rectOriginal.yTop + pOutputStream->parms.rectOriginal.h;

                        pClient->VideoStreamVisibleRegion(&pOutputStream->parms,
                                                          pOutputStream->parms.cVisibleRects,
                                                          pOutputStream->parms.paVisibleRects,
                                                          (fu32StreamEvents & VH_STREAM_EVENT_GEOMETRY)?
                                                              &rectOriginal:
                                                              NULL);
                    }
                }

                pClient->ThreadContextRelease(VRDP_CONTEXT_OUTPUT);
            }
        }
    }
}

/*
 * Called as result of VRDP_OUTPUT_VIDEO_COMPRESSION_RATE
 * @thread OUTPUT
 */
void VRDPServer::VideoHandlerCompressionRate(uint32_t u32VideoQuality)
{
    VHCONTEXT *pCtx = m_pVideoHandler;

    if (!pCtx)
    {
        return;
    }

    VHSTATLOG(("VHSTAT: video quality %u\n", u32VideoQuality));

    pCtx->u32RequestedVideoQuality = u32VideoQuality;

    if (u32VideoQuality == 0)
    {
        appProperty (ApplicationCallbacks (), ApplicationCallbackPointer (),
                     VRDE_QP_VIDEO_CHANNEL_QUALITY,
                     &u32VideoQuality, sizeof (u32VideoQuality), NULL);
    }

    /* Process output streams and change the compression ratio.
     */
    VHOUTPUTSTREAM *pOutputStream;
    RTListForEach(&pCtx->ListOutputStreams, pOutputStream, VHOUTPUTSTREAM, NodeOutputStream)
    {
        /* Change the quality only for video streams. Direct streams like 3D
         * must have constant quality.
         */
        if (!pOutputStream->parms.fDirect)
        {
            pOutputStream->iJPEGQuality = u32VideoQuality;
        }
    }
}

/*
 * Called as result of VRDP_OUTPUT_VIDEO_CLIENT_SIZE_LIMIT
 * @thread OUTPUT
 */
void VRDPServer::VideoHandlerSizeLimit(VHEVENTSIZELIMIT *pData)
{
    VHCONTEXT *pCtx = m_pVideoHandler;

    if (!pCtx)
    {
        return;
    }

    VIDEOLOG(("\n"));

    /* Apply the changed parameters and stop/restart all streams. */
    uint32_t u32ClientMaxSquare = pData->u32ClientMaxArea;

    if (u32ClientMaxSquare != 0)
    {
        /* No threshold in this case. Both squares are the same. */
        pCtx->u32VideoRectSquareDownscale = u32ClientMaxSquare;
        pCtx->u32VideoRectSquareMax = u32ClientMaxSquare;
    }

    /* min area, w, h. */
    if (pData->u16ClientMinArea != 0)
    {
        pCtx->u32VideoRectSquareMin = pData->u16ClientMinArea;
    }
    if (pData->u16ClientMinWidth != 0)
    {
        pCtx->u32VideoRectWidthMin = pData->u16ClientMinWidth;
    }
    if (pData->u16ClientMinHeight != 0)
    {
        pCtx->u32VideoRectHeightMin = pData->u16ClientMinHeight;
    }

    /* Restart all streams. */
    shadowBufferVideoDetectorCmd(VD_CMD_RESET);
}

/* Maintain output streams. */
uint64_t VRDPServer::VideoHandlerOutput(uint64_t u64EventTS)
{
    VHCONTEXT *pCtx = m_pVideoHandler;

    uint64_t u64NowTS = VRDPTimeMilliTS();

    if (u64EventTS == 0)
    {
        return u64NowTS + 200;
    }

    if (!pCtx)
    {
        return u64NowTS + 200;
    }

    /*
     * Process streams and create the VHOUTPUTSTREAM instances if necessary.
     */
    if (vhLock(pCtx))
    {
        VHSTREAMDATA *pStream;
        RTListForEach(&pCtx->listStreams, pStream, VHSTREAMDATA, nodeStream)
        {
            if (pStream->u32SourceStreamStatus == SOURCE_STREAM_CREATED)
            {
                /* A new output stream must be created. */
                VIDEOLOG(("SOURCE_STREAM_CREATED %d\n", pStream->parms.u32VideoStreamId));

                uint32_t u32VideoQuality = pStream->parms.fDirect? 0: pCtx->u32RequestedVideoQuality;
                if (u32VideoQuality == 0)
                {
                    appProperty(ApplicationCallbacks(), ApplicationCallbackPointer(),
                                VRDE_QP_VIDEO_CHANNEL_QUALITY,
                                &u32VideoQuality, sizeof(u32VideoQuality), NULL);
                }

                VRDPPktAddRef(&pStream->pkt);
                int rc = vhOutputStreamCreate(pCtx, pStream, u32VideoQuality);

                if (RT_SUCCESS(rc))
                {
                    pStream->u32SourceStreamStatus = SOURCE_STREAM_READY;
                }
                else
                {
                    /* Ignore the error, the stream remains in the CREATED state. */
                    VRDPPktRelease(&pStream->pkt);
                }
            }
        }

        vhUnlock(pCtx);
    }

    /* Process output streams:
     *     compress source frames;
     *     start creation of TSMF presentations.
     */
    VHOUTPUTSTREAM *pOutputStream;
    RTListForEach(&pCtx->ListOutputStreams, pOutputStream, VHOUTPUTSTREAM, NodeOutputStream)
    {
        if (pOutputStream->enmStatus == StatusVideoStream_Ready)
        {
            /* Encode source frames. */
            vhPrepareOutputFrames(pOutputStream);

            /* Try to start the new presentation for this output stream. A new client may connect
             * and will need the presentation to be started.
             * @todo check if this can be optimized.
             */
            VRDPClient *pClient = NULL;
            uint32_t u32ClientId = 0;

            while ((pClient = m_clientArray.ThreadContextGetNextClient (&u32ClientId, VRDP_CONTEXT_OUTPUT)) != NULL)
            {
                if (pClient->IsScreenIdMatched (pOutputStream->parms.uScreenId))
                {
                    /* The following call will do nothing if the corresponding presentation
                     * has been started already.
                     */
                    pClient->VideoStreamStart (&pOutputStream->parms);
                }

                pClient->ThreadContextRelease (VRDP_CONTEXT_OUTPUT);
            }
        }
    }

    int64_t i64NowTimeline = VH_TIMELINE_TIME_FROM_MS(u64NowTS);

    /*
     * Send frames which are on time in ready streams.
     */
    RTListForEach(&pCtx->ListOutputStreams, pOutputStream, VHOUTPUTSTREAM, NodeOutputStream)
    {
        if (pOutputStream->enmStatus == StatusVideoStream_Ready)
        {
            RTLISTANCHOR ListFramesToSend;
            RTListInit(&ListFramesToSend);

            vhOutputFrameCheckToSend(pOutputStream, &ListFramesToSend, i64NowTimeline);

            if (!RTListIsEmpty(&ListFramesToSend) || pOutputStream->parms.fDirect)
            {
                VRDPClient *pClient = NULL;
                uint32_t u32ClientId = 0;

                while ((pClient = m_clientArray.ThreadContextGetNextClient (&u32ClientId, VRDP_CONTEXT_OUTPUT)) != NULL)
                {
                    if (pClient->IsScreenIdMatched (pOutputStream->parms.uScreenId))
                    {
                        pClient->VideoStreamSendFrames (pCtx, &pOutputStream->parms, &ListFramesToSend, i64NowTimeline);
                    }

                    pClient->ThreadContextRelease (VRDP_CONTEXT_OUTPUT);
                }
            }

            vhOutputFrameReleaseList(pOutputStream, &ListFramesToSend);
        }
    }

    return u64NowTS + 20;  /// @todo at the next frame timestamp + lipsync delay
}

void videoHandlerInsertLatestFrame(const VHSTREAMPARMS *pStream, RTLISTANCHOR *pListFramesToSend)
{
    const VHOUTPUTSTREAM *pOutputStream = RT_FROM_MEMBER(pStream, VHOUTPUTSTREAM, parms);
    VHOUTPUTFRAME *pLatestFrame = pOutputStream->pStreamData->pLatestFrame;

    VIDEOLOG(("pLatestFrame = %p\n", pLatestFrame));
    if (pLatestFrame)
    {
        VRDPPktAddRef(&pLatestFrame->pkt);
        RTListPrepend(pListFramesToSend, &pLatestFrame->NodeOutputFrame);
    }
}

/*
 * Called as result of VRDP_OUTPUT_VIDEO_STREAM_RESTART.
 * Recreate output streams for all source direct streams.
 * @thread OUTPUT
 */
void VRDPServer::VideoHandlerStreamRestart(void)
{
    VHCONTEXT *pCtx = m_pVideoHandler;

    if (!pCtx)
    {
        return;
    }

    VIDEOLOG(("VideoHandlerStreamRestart\n"));

    /*
     * Check whether the output stream already exists for all direct streams
     * and create the VHOUTPUTSTREAM instance if necessary.
     */
    if (vhLock(pCtx))
    {
        VHSTREAMDATA *pStream = NULL;
        RTListForEach(&pCtx->listStreams, pStream, VHSTREAMDATA, nodeStream)
        {
            if (pStream->parms.fDirect)
            {
                VHOUTPUTSTREAM *pOutputStream = vhOutputStreamFindById(pCtx, pStream->parms.u32VideoStreamId);

                VIDEOLOG(("VideoHandlerStreamRestart: found direct id %d/%p\n",
                          pStream->parms.u32VideoStreamId,
                          pOutputStream));

                if (!pOutputStream)
                {
                    uint32_t u32VideoQuality = 0;
                    appProperty(ApplicationCallbacks(), ApplicationCallbackPointer(),
                                VRDE_QP_VIDEO_CHANNEL_QUALITY,
                                &u32VideoQuality, sizeof(u32VideoQuality), NULL);

                    VRDPPktAddRef(&pStream->pkt);
                    int rc = vhOutputStreamCreate(pCtx, pStream, u32VideoQuality);

                    if (RT_SUCCESS(rc))
                    {
                        pStream->u32SourceStreamStatus = SOURCE_STREAM_READY;
                    }
                    else
                    {
                        /* Ignore the error, the stream remains in the CREATED state. */
                        VRDPPktRelease(&pStream->pkt);
                    }
                }
            }
        }

        vhUnlock(pCtx);
    }
}
