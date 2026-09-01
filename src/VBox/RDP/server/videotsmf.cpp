/* $Id: videotsmf.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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
#include "videodetector.h" /*for vhSourceComputeScaledSize*/

#if 0 // def DEBUG_sunlover
#undef VIDEOLOG
#define VIDEOLOG LogRel
#define VIDEOASSERT AssertLogRel
#else
#define VIDEOASSERT Assert
#endif


/*
 * TSMF: Video redirection channel.
 */

/*
 * A video redirection channel takes a video stream from the video handler
 * and creates a TSMF presentation consisting of control channel and MJPEG channel.
 *
 * The server uses DShow platform but:
 *   - does not send SET_ALLOCATOR;
 *   - EXCHANGE_CAPABILITIES_REQ is sent for all channels.
 *
 * Notes:
 *   - REMOVE_STREAM is not needed;
 *   - PLAYBACK messages are sent for entire presentation, not for each stream.
 */

#define TSMF_DATA_STREAM_ID_MJPEG 0xDA110000 /* Arbitrary != 0. */

typedef struct TSMFPRESENTATION TSMFPRESENTATION;
class VideoChannelTSMF;

/* TSMF stream flags. */
#define TSMFSTREAM_F_DVC_EXISTS 0x01  /* The corresponding DVC channel has been established. */

/* A single TSMF stream uses a separate DVC channel instance. */
typedef struct TSMFSTREAM
{
    TSMFPRESENTATION *pPresentation; /* Backlink. */
    uint32_t u32StreamId;            /* TSMF stream id. */
    uint8_t  u8DVCChannelId;         /* The DVC channel id.*/
    uint8_t  fu8StreamFlags;         /* TSMFSTREAM_F_*. */
} TSMFSTREAM;

#define TSMF_PS_CREATED  0 /* INPUT thread is preparing the presentation. */
#define TSMF_PS_READY    1 /* Preparation is finished, OUTPUT thread can take control over it. */
#define TSMF_PS_SETUP    2 /* OUTPUT thread took control. */
#define TSMF_PS_PLAYBACK 3 /* Playback started. */
#define TSMF_PS_SHUTDOWN 4 /* Must be shutdown. */
#define TSMF_PS_INVALID  5 /* Must be ignored. */

/* TSMF presentation structure is used by both INPUT and OUTPUT threads and is referenced.
 * The list of presentations is protected by a lock.
 */
typedef struct TSMFPRESENTATION
{
    VRDPPKT pkt;

    RTLISTNODE nodePresentation;
    RTLISTNODE nodeCopy;        /* Const list of references for the OUTPUT thread. */

    VideoChannelTSMF *pVideoChannelTSMF; /* Backlink. */

    uint32_t volatile u32Status; /* TSMF_PS_* */

    /*
     * "Constant" fields assigned during initialization.
     */
    unsigned uScreenId;
    uint32_t u32VideoStreamId;  /* The VRDP video handler stream id. */

    RTUUID uuid;                /* TSMF UUID of this presentation. */

    uint64_t u64VideoWindowId;  /* Arbitrary, the server uses address of this field. */
    uint64_t u64ParentWindowId; /* Arbitrary, the server uses address of this field. */

    bool fDirect;               /* The stream is not autodetected. */

    /*
     * Fields accessed only by output thread.
     */
    RGNRECT rectShadowBuffer;   /* Rectangle in the shadow buffer coordinates. */
    RGNRECT rectClient;         /* Rectangle in the client coordinates. */
    RGNRECT rectScaled;         /* Size of JPEG compressed image. */

    int64_t i64BeginTimeline;   /* Sample times are relative to this. */
    SBHANDLE videoRectHandle;
    int64_t i64BeginTimestamp;  /* Timestamp of first frame in this presentation. */
    VHSTREAMCALLBACKDATA callbackData; /* Notification about the presentation status. */
    uint64_t u64FirstFrameNanoTS;
    int64_t i64LipSyncDelay;
    uint64_t u64AudioTimeNS;    /* How many audio samples were actually played since the presentation start. */
    uint64_t u64LastTimeLineCorrection; /* When i64BeginTimeline has been adjusted. */
    int64_t i64MaxSampleStartTime; /* Used to make sure that iSampleStartTime is increasing always. */
    bool fProcessStartPlayback; /* OnAudioSend started playback. */

    /*
     * Stream structures are mostly "constant", fu8StreamFlags is only accessed by INPUT thread.
     */
    TSMFSTREAM control;
    TSMFSTREAM mjpeg;
} TSMFPRESENTATION;

class VideoChannelTSMF: public VideoChannel
{
    private:
        bool m_fAvailabilityReported;
        bool m_fUTTSCSyncMode;
        bool m_fAudioClosed;

        VRDPLock *m_pLock; /* Serialize the presentation list. */

        RTLISTANCHOR m_listPresentations; /* TSMFPRESENTATION structures. */

        /* Return referenced pointer. */
        TSMFPRESENTATION *tsmfPresentationById(uint32_t u32VideoStreamId);

        /* Remove the presentation structure from the list and release the structure. */
        void tsmfPresentationDelete(TSMFPRESENTATION *pPresentation);

        /* Get a "snapshot" of current presentations. */
        int tsmfPresentationListCopyGet(RTLISTANCHOR *pList);
        void tsmfPresentationListCopyRelease(RTLISTANCHOR *pList);

        void vcSendFramesUTTSC(VHCONTEXT *pCtx,
                               VHSTREAMPARMS *pStream,
                               RTLISTANCHOR *pListFramesToSend,
                               int64_t i64NowTimeline,
                               TSMFPRESENTATION *pPresentation);

        void vcSendFramesMSTSC(VHCONTEXT *pCtx,
                               VHSTREAMPARMS *pStream,
                               RTLISTANCHOR *pListFramesToSend,
                               int64_t i64NowTimeline,
                               TSMFPRESENTATION *pPresentation);

        bool isUTTSCSyncMode(void) { return m_fUTTSCSyncMode; }

        static void dvcTSMF(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData);
        void processInput(TSMFSTREAM *pStream, uint32_t u32Event, const void *pvData, uint32_t cbData);

        int processServerData(TSMFSTREAM *pStream, const SHARED_MSG_HEADER_RESPONSE *pHdr, uint32_t cbData);
        int processClientNotifications(TSMFSTREAM *pStream, const SHARED_MSG_HEADER *pHdr, uint32_t cbData);
        int processInterfaceManipulation(TSMFSTREAM *pStream, const SHARED_MSG_HEADER_RESPONSE *pMsgHdr,
                                         uint32_t cbData);

        void tsmfCloseChannel(uint8_t u8DVCChannelId);

    public:
        VideoChannelTSMF(VRDPClient *pClient, PFNVIDEOCHANNELEVENT pfnEvent, void *pvEvent);
        virtual ~VideoChannelTSMF();

        virtual const char *VideoChannelName(void) { return "TSMF"; };
        virtual int VideoChannelSetup(void);

        virtual int VideoChannelStart (const VHSTREAMPARMS *pStream);
        virtual int VideoChannelEnd (const VHSTREAMPARMS *pStream);
        virtual void VideoChannelSendFrames (VHCONTEXT *pCtx,
                                             VHSTREAMPARMS *pStream,
                                             RTLISTANCHOR *pListFramesToSend,
                                             int64_t i64NowTimeline);
        virtual void VideoChannelInput (uint32_t u32Event,
                                        const void *pvData,
                                        uint32_t cbData) { /* Not used. */ RT_NOREF3(u32Event, pvData, cbData); return; }
        virtual void VideoChannelVisibleRegion(const VHSTREAMPARMS *pStream,
                                               uint32_t cRects,
                                               RTRECT *paRects,
                                               RTRECT *pRect);
        virtual void OnAudioSend(uint64_t u64PacketStartNanoTS,
                                 uint64_t u64PacketEndNanoTS,
                                 uint64_t u64DurationNano);
        virtual void OnAudioClose(void);
};


int VideoChannelTSMFCreate(VideoChannel **ppVideoChannel,
                           VRDPClient *pClient,
                           PFNVIDEOCHANNELEVENT pfnEvent,
                           void *pvEvent)
{
    *ppVideoChannel = new VideoChannelTSMF(pClient, pfnEvent, pvEvent);
    return *ppVideoChannel? VINF_SUCCESS: VERR_NO_MEMORY;
}


static const uint8_t TS_AM_MEDIATYPE_Video[16] =
    { 0x76, 0x69, 0x64, 0x73, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };

static const uint8_t TS_AM_MEDIASUBTYPE_MJPG[16] =
    { 0x4D, 0x4A, 0x50, 0x47, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };

static const uint8_t TS_AM_FORMAT_VideoInfo[16] =
    { 0x80, 0x9f, 0x58, 0x05, 0x56, 0xc3, 0xce, 0x11, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a };

#define VRDP_FOURCC_MJPG (RT_MAKE_U32_FROM_U8('M','J','P','G'))

typedef struct TSMFSENDCONTEXT
{
    bool       fOutputThread;
    VRDPClient *pClient;
    RTUUID     PresentationId;
} TSMFSENDCONTEXT;

static void initTSMFSendContext(TSMFSENDCONTEXT *pSend,
                                bool fOutputThread,
                                VRDPClient *pClient,
                                const TSMFPRESENTATION *pPresentation)
{
    pSend->fOutputThread  = fOutputThread;
    pSend->pClient        = pClient;
    pSend->PresentationId = pPresentation->uuid;
}

static void initTSMFSendContextNull(TSMFSENDCONTEXT *pSend,
                                    bool fOutputThread,
                                    VRDPClient *pClient)
{
    pSend->fOutputThread  = fOutputThread;
    pSend->pClient        = pClient;
    RTUuidClear(&pSend->PresentationId);
}

/** @thread OUTPUT */
static void tsmfSendData(VRDPClient *pClient, uint8_t u8DVCChannelId, void *pvData, uint32_t cbData)
{
    VRDPLOGRELIO(("TSMF: send 0x%RX8 %d\n%.*Rhxd\n", u8DVCChannelId, cbData, cbData, pvData));

    pClient->TP()->DVC()->SendData(u8DVCChannelId, pvData, cbData);
}

typedef struct VRDPOUTPUTSENDTSMF
{
    VRDPOUTPUTGENERICHDR hdr;
    void *pvData;
    uint32_t cbData;
} VRDPOUTPUTSENDTSMF;

/** @thread OUTPUT */
static DECLCALLBACK(void) processSendTSMF(VRDPClient *pClient,
                                          const VRDPOUTPUTGENERICHDR *pHdr)
{
    VRDPOUTPUTSENDTSMF *pPkt = (VRDPOUTPUTSENDTSMF *)pHdr;
    uint8_t u8DVCChannelId = (uint8_t)pPkt->hdr.u32Parm;

    tsmfSendData(pClient, u8DVCChannelId, pPkt->pvData, pPkt->cbData);

    RTMemFree(pPkt->pvData);
}

/** @thread output/input */
static void sendTSMF(TSMFSENDCONTEXT *pSend,
                     uint8_t u8DVCChannelId,
                     void *pvData,
                     uint32_t cbData)
{
    VRDPClient *pClient = pSend->pClient;
    if (pSend->fOutputThread)
    {
        tsmfSendData(pClient, u8DVCChannelId, pvData, cbData);
    }
    else
    {
        /* Forward to the output thread. @todo avoid mem dup */
        VRDPOUTPUTSENDTSMF pkt;

        pkt.hdr.pfnCallback = processSendTSMF;
        pkt.hdr.pvUser      = NULL;
        pkt.hdr.u32Size     = sizeof(pkt);
        pkt.hdr.u32Parm     = u8DVCChannelId;
        pkt.pvData = RTMemDup(pvData, cbData);
        if (!pkt.pvData)
        {
            AssertFailed();
            return;
        }
        pkt.cbData = cbData;

        pClient->Server()->PostOutput(VRDP_OUTPUT_GENERIC, pClient->Id(), &pkt, sizeof(pkt));
    }
}

static void sendTSMFOutputBuffers(TSMFSENDCONTEXT *pSend,
                                  uint8_t u8DVCChannelId,
                                  VRDPBUFFER *paBuffers,
                                  int cBuffers)
{
    VRDPClient *pClient = pSend->pClient;
    if (pSend->fOutputThread)
    {
        pClient->TP()->DVC()->SendDataBuffers(u8DVCChannelId, paBuffers, cBuffers);
    }
    else
    {
        AssertFailed();
    }
}

static void initServerSharedHeader(SHARED_MSG_HEADER *pHdr, uint32_t u32FunctionId)
{
    pHdr->InterfaceId = 0 | STREAM_ID_PROXY;
    pHdr->MessageId   = u32FunctionId; /* Use it to distinguish responses. */
    pHdr->FunctionId  = u32FunctionId;
}

static void initVideoTypeMJPEG(TS_AM_MEDIA_TYPE *pMediaType, const RGNRECT *pRect)
{
    memcpy(pMediaType->MajorType,      TS_AM_MEDIATYPE_Video,   sizeof (pMediaType->MajorType));
    memcpy(pMediaType->SubType,        TS_AM_MEDIASUBTYPE_MJPG, sizeof (pMediaType->SubType));
    pMediaType->bFixedSizeSamples    = false;
    pMediaType->bTemporalCompression = false;
    pMediaType->SampleSize           = 1;
    memcpy(pMediaType->FormatType,     TS_AM_FORMAT_VideoInfo,  sizeof(pMediaType->FormatType));
    pMediaType->cbFormat             = sizeof (pMediaType->Format);

    memset(&pMediaType->Format, 0, sizeof (pMediaType->Format));

    pMediaType->Format.AvgTimePerFrame = 0x51615; /* 33,(3) ms per frame = 30 FPS
                                                   * probably does not matter because duration of each
                                                   * frame is specified in the ON_SAMPLE message.
                                                   */

    pMediaType->Format.bmiHeader.biSize        = sizeof (pMediaType->Format.bmiHeader);
    pMediaType->Format.bmiHeader.biWidth       = pRect->w;
    pMediaType->Format.bmiHeader.biHeight      = pRect->h;
    pMediaType->Format.bmiHeader.biPlanes      = 1;
    pMediaType->Format.bmiHeader.biBitCount    = 24;
    pMediaType->Format.bmiHeader.biCompression = VRDP_FOURCC_MJPG;
    pMediaType->Format.bmiHeader.biSizeImage   = pMediaType->Format.bmiHeader.biWidth *
                                                 pMediaType->Format.bmiHeader.biHeight *
                                                 ((pMediaType->Format.bmiHeader.biBitCount + 7) / 8);
}

static void sendSetChannelParams(TSMFSENDCONTEXT *pSend,
                                 uint8_t u8DVCChannelId,
                                 uint32_t TSMFStreamId)
{
    MSG_SET_CHANNEL_PARAMS ChannelParams;
    initServerSharedHeader (&ChannelParams.hdr, FN_SET_CHANNEL_PARAMS);

    memcpy(ChannelParams.PresentationId, &pSend->PresentationId, sizeof (ChannelParams.PresentationId));
    ChannelParams.StreamId = TSMFStreamId;

    sendTSMF(pSend, u8DVCChannelId, &ChannelParams, sizeof (ChannelParams));
}

static void sendExchangeCapabilitiesReq(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId)
{
    MSG_EXCHANGE_CAPABILITIES_REQ Caps;
    initServerSharedHeader (&Caps.hdr, FN_EXCHANGE_CAPABILITIES_REQ);

    Caps.numHostCapabilities           = 2;
    Caps.Version.hdr.CapabilityType    = 1;
    Caps.Version.hdr.CapabilityLength  = 4;
    Caps.Version.Version               = 2;
    Caps.Platform.hdr.CapabilityType   = 2;
    Caps.Platform.hdr.CapabilityLength = 4;
    Caps.Platform.Platform             = MMREDIR_CAPABILITY_PLATFORM_DSHOW;

    sendTSMF(pSend, u8DVCChannelId, &Caps, sizeof (Caps));
}

static void sendNewPresentation(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId)
{
    MSG_NEW_PRESENTATION NewPresentation;
    initServerSharedHeader (&NewPresentation.hdr, FN_ON_NEW_PRESENTATION);

    memcpy(NewPresentation.PresentationId, &pSend->PresentationId, sizeof (NewPresentation.PresentationId));
    NewPresentation.PlatformCookie = TSMM_PLATFORM_COOKIE_DSHOW;

    sendTSMF(pSend, u8DVCChannelId, &NewPresentation, sizeof (NewPresentation));
}

static void sendShutdownPresentation(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId)
{
    MSG_SHUTDOWN_PRESENTATION_REQ ShutdownPresentation;
    initServerSharedHeader (&ShutdownPresentation.hdr, FN_SHUTDOWN_PRESENTATION_REQ);

    memcpy(ShutdownPresentation.PresentationId, &pSend->PresentationId, sizeof (ShutdownPresentation.PresentationId));

    sendTSMF(pSend, u8DVCChannelId, &ShutdownPresentation, sizeof (ShutdownPresentation));
}

static void sendCheckFormatSupportMJPEG(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId, const RGNRECT *pRect)
{
    MSG_CHECK_FORMAT_SUPPORT_REQ CheckFormat;
    initServerSharedHeader (&CheckFormat.hdr, FN_CHECK_FORMAT_SUPPORT_REQ);

    CheckFormat.PlatformCookie  = TSMM_PLATFORM_COOKIE_DSHOW;
    CheckFormat.NoRolloverFlags = NOROLLOVERFLAGS_IGNORE_ALTERNATIVE;
    CheckFormat.numMediaType    = sizeof (CheckFormat.MediaType);
    initVideoTypeMJPEG(&CheckFormat.MediaType, pRect);

    sendTSMF(pSend, u8DVCChannelId, &CheckFormat, sizeof (CheckFormat));
}

static void sendAddStreamMJPEG(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId, uint32_t TSMFStreamId, const RGNRECT *pRect)
{
    MSG_ADD_STREAM AddStream;
    initServerSharedHeader (&AddStream.hdr, FN_ADD_STREAM);

    memcpy(AddStream.PresentationId, &pSend->PresentationId, sizeof (AddStream.PresentationId));
    AddStream.StreamId                       = TSMFStreamId;
    AddStream.numMediaType                   = sizeof (AddStream.MediaType);
    initVideoTypeMJPEG(&AddStream.MediaType, pRect);

    sendTSMF(pSend, u8DVCChannelId, &AddStream, sizeof (AddStream));
}

static void sendSetTopology(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId)
{
    MSG_SET_TOPOLOGY_REQ SetTopology;
    initServerSharedHeader (&SetTopology.hdr, FN_SET_TOPOLOGY_REQ);

    memcpy(SetTopology.PresentationId, &pSend->PresentationId, sizeof (SetTopology.PresentationId));

    sendTSMF(pSend, u8DVCChannelId, &SetTopology, sizeof (SetTopology));
}

static void sendNotifyPreroll(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId,
                              uint32_t TSMFStreamId)
{
    MSG_NOTIFY_PREROLL NotifyPreroll;
    initServerSharedHeader (&NotifyPreroll.hdr, FN_NOTIFY_PREROLL);

    memcpy(NotifyPreroll.PresentationId, &pSend->PresentationId, sizeof (NotifyPreroll.PresentationId));
    NotifyPreroll.StreamId = TSMFStreamId;

    sendTSMF(pSend, u8DVCChannelId, &NotifyPreroll, sizeof (NotifyPreroll));
}

static void sendSetVideoWindow(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId,
                               uint64_t u64VideoWindowId,
                               uint64_t u64ParentWindowId)
{
    MSG_SET_VIDEO_WINDOW VideoWindow;
    initServerSharedHeader (&VideoWindow.hdr, FN_SET_VIDEO_WINDOW);

    memcpy(VideoWindow.PresentationId, &pSend->PresentationId, sizeof (VideoWindow.PresentationId));
    VideoWindow.VideoWindowId = u64VideoWindowId;
    VideoWindow.HwndParent = u64ParentWindowId;

    sendTSMF(pSend, u8DVCChannelId, &VideoWindow, sizeof (VideoWindow));
}

static void sendOnPlaybackStarted(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId)
{
    MSG_ON_PLAYBACK_STARTED PlaybackStarted;
    initServerSharedHeader (&PlaybackStarted.hdr, FN_ON_PLAYBACK_STARTED);

    memcpy(PlaybackStarted.PresentationId, &pSend->PresentationId, sizeof (PlaybackStarted.PresentationId));
    PlaybackStarted.PlaybackStartOffset = 0;
    PlaybackStarted.IsSeek = 0;

    sendTSMF(pSend, u8DVCChannelId, &PlaybackStarted, sizeof (PlaybackStarted));
}

static void sendUpdateGeometryInfo(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId,
                                   uint64_t u64VideoWindowId,
                                   const RGNRECT *pRect,
                                   uint32_t cVisibleRects,
                                   const RTRECT *paVisibleRects)
{
    uint32_t cbGeometryInfo = RT_UOFFSETOF(MSG_UPDATE_GEOMETRY_INFO, aVisibleRect) + cVisibleRects * sizeof (TS_RECT);
    MSG_UPDATE_GEOMETRY_INFO *pGeometryInfo = (MSG_UPDATE_GEOMETRY_INFO *)VRDPMemAlloc(cbGeometryInfo);

    if (!pGeometryInfo)
    {
        return;
    }

    initServerSharedHeader (&pGeometryInfo->hdr, FN_UPDATE_GEOMETRY_INFO);

    memcpy(pGeometryInfo->PresentationId, &pSend->PresentationId, sizeof (pGeometryInfo->PresentationId));
    pGeometryInfo->numGeometryInfo = sizeof (pGeometryInfo->GeometryInfo);

    pGeometryInfo->GeometryInfo.VideoWindowId    = u64VideoWindowId;
    pGeometryInfo->GeometryInfo.VideoWindowState = TS_WNDFLAG_NEW | TS_WNDFLAG_VISRGN;
    pGeometryInfo->GeometryInfo.Width            = pRect->w;
    pGeometryInfo->GeometryInfo.Height           = pRect->h;
    pGeometryInfo->GeometryInfo.Left             = pRect->x;
    pGeometryInfo->GeometryInfo.Top              = pRect->y;
    pGeometryInfo->GeometryInfo.Reserved         = 0;
    pGeometryInfo->GeometryInfo.ClientLeft       = pRect->x;
    pGeometryInfo->GeometryInfo.ClientTop        = pRect->y;
    pGeometryInfo->GeometryInfo.Padding          = 0;

    VIDEOLOG(("Geometry %RTuuid %d,%d %dx%d\n",
              &pSend->PresentationId,
              pRect->x, pRect->y, pRect->w, pRect->h));

    pGeometryInfo->cbVisibleRect = cVisibleRects * sizeof (TS_RECT);

    uint32_t i;
    for (i = 0; i < cVisibleRects; i++)
    {
        /** @todo clip paVisibleRects to the pRect. */
        pGeometryInfo->aVisibleRect[i].Top    = paVisibleRects[i].yTop;
        pGeometryInfo->aVisibleRect[i].Left   = paVisibleRects[i].xLeft;
        pGeometryInfo->aVisibleRect[i].Bottom = paVisibleRects[i].yBottom;
        pGeometryInfo->aVisibleRect[i].Right  = paVisibleRects[i].xRight;
        VIDEOLOG(("Geometry %RTuuid [%d] %d,%d %dx%d\n",
                  &pSend->PresentationId, i,
                  paVisibleRects[i].xLeft,
                  paVisibleRects[i].yTop,
                  paVisibleRects[i].xRight - paVisibleRects[i].xLeft,
                  paVisibleRects[i].yBottom - paVisibleRects[i].yTop));
    }

    sendTSMF(pSend, u8DVCChannelId, pGeometryInfo, cbGeometryInfo);

    VRDPMemFree(pGeometryInfo);
}

static void sendOnSample(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId,
                         uint32_t TSMFStreamId,
                         uint32_t u32SampleExtensions,
                         const void *pvSample,
                         uint32_t cbSample,
                         int64_t i64SampleStartTime,
                         int64_t i64SampleEndTime)
{
    MSG_ON_SAMPLE msg;
    initServerSharedHeader(&msg.hdr, FN_ON_SAMPLE);

    memcpy(msg.PresentationId, &pSend->PresentationId, sizeof (msg.PresentationId));
    msg.StreamId = TSMFStreamId;
    msg.numSample = sizeof(msg.Sample) + cbSample;
    if (u32SampleExtensions & TSMM_SAMPLE_EXT_HAS_NO_TIMESTAMPS)
    {
        msg.Sample.SampleStartTime = 0;
        msg.Sample.SampleEndTime = 0;
        msg.Sample.ThrottleDuration = 333333;
    }
    else
    {
        msg.Sample.SampleStartTime = i64SampleStartTime;
        msg.Sample.SampleEndTime = i64SampleEndTime;
        msg.Sample.ThrottleDuration = i64SampleEndTime - i64SampleStartTime;
    }
    msg.Sample.SampleFlags = 0;
    msg.Sample.SampleExtensions = u32SampleExtensions;
    msg.Sample.cbData = cbSample;

    VIDEOLOG(("onSample %RTuuid 0x%x size %d, ext 0x%x start %lldms end %lldms\n",
              &pSend->PresentationId, TSMFStreamId, cbSample, msg.Sample.SampleExtensions,
              i64SampleStartTime / 10000, i64SampleEndTime / 10000));

    VRDPBUFFER aBuffers[2];

    aBuffers[0].pv = &msg;
    aBuffers[0].cb = sizeof (msg);

    aBuffers[1].pv = pvSample;
    aBuffers[1].cb = cbSample;

    sendTSMFOutputBuffers(pSend, u8DVCChannelId, aBuffers, RT_ELEMENTS(aBuffers));
}

static void sendEndOfStream(TSMFSENDCONTEXT *pSend, uint8_t u8DVCChannelId,
                            uint32_t TSMFStreamId)
{
    MSG_ON_END_OF_STREAM EndOfStream;
    initServerSharedHeader (&EndOfStream.hdr, FN_ON_END_OF_STREAM);

    memcpy(EndOfStream.PresentationId, &pSend->PresentationId, sizeof (EndOfStream.PresentationId));
    EndOfStream.StreamId = TSMFStreamId;

    sendTSMF(pSend, u8DVCChannelId, &EndOfStream, sizeof (EndOfStream));
}


static int parse_EXCHANGE_CAPABILITIES_RSP(const SHARED_MSG_HEADER_RESPONSE *pHdr, uint32_t cbData, uint32_t *pResult)
{
    if (cbData < sizeof(MSG_EXCHANGE_CAPABILITIES_RSP_HDR))
    {
        VIDEOLOG(("cbData < MSG_EXCHANGE_CAPABILITIES_RSP_HDR!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* "The server MUST ignore any capabilities it does not recognize,
     *  which MAY include additional capabilities that are added to
     *  future versions of this protocol."
     */
    const MSG_EXCHANGE_CAPABILITIES_RSP_HDR *pRsp = (MSG_EXCHANGE_CAPABILITIES_RSP_HDR *)pHdr;

    VIDEOLOG(("MSG_EXCHANGE_CAPABILITIES_RSP %d caps.\n", pRsp->numClientCapabilities));

    const uint8_t *pu8Cap = (const uint8_t *)pRsp + sizeof (MSG_EXCHANGE_CAPABILITIES_RSP_HDR);
    uint32_t cbCap = cbData - sizeof (MSG_EXCHANGE_CAPABILITIES_RSP_HDR);

    unsigned i;
    for (i = 0; i < pRsp->numClientCapabilities; i++)
    {
        if (cbCap < sizeof (TSMM_CAPABILITIES_HDR))
        {
            VIDEOLOG(("cbCap < sizeof (TSMM_CAPABILITIES_HDR)!!!\n"));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        const TSMM_CAPABILITIES_HDR *pCap = (const TSMM_CAPABILITIES_HDR *)pu8Cap;

        VIDEOLOG(("MSG_EXCHANGE_CAPABILITIES_RSP type %d, len %d.\n", pCap->CapabilityType, pCap->CapabilityLength));

        if (cbCap < pCap->CapabilityLength + sizeof (TSMM_CAPABILITIES_HDR))
        {
            VIDEOLOG(("cbCap < pCap->CapabilityLength!!!\n"));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        pu8Cap += pCap->CapabilityLength + sizeof (TSMM_CAPABILITIES_HDR);
        cbCap -= pCap->CapabilityLength + sizeof (TSMM_CAPABILITIES_HDR);
    }

    /* Result. */
    if (cbCap < sizeof (uint32_t))
    {
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    *pResult = *(uint32_t *)pu8Cap;

    VIDEOLOG(("MSG_EXCHANGE_CAPABILITIES_RSP result 0x%08X.\n", *pResult));

    return VINF_SUCCESS;
}

static int parse_CHECK_FORMAT_SUPPORT_RSP(const SHARED_MSG_HEADER_RESPONSE *pHdr, uint32_t cbData, uint32_t *pResult, uint32_t *pFormatSupported)
{
    if (cbData < sizeof(MSG_CHECK_FORMAT_SUPPORT_RSP))
    {
        VIDEOLOG(("cbData < MSG_CHECK_FORMAT_SUPPORT_RSP!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* "For a presentation with multiple streams, the server MUST ensure that
     *  the same platform is used to render data for each stream. If the client
     *  uses an inconsistent platform for any of the streams, the server
     *  MUST renegotiate format support until a common platform is found
     *  or the server MUST shut down the presentation."
     */
    const MSG_CHECK_FORMAT_SUPPORT_RSP *pRsp = (MSG_CHECK_FORMAT_SUPPORT_RSP *)pHdr;

    VIDEOLOG(("MSG_CHECK_FORMAT_SUPPORT_RSP %d, %d, 0x%08X.\n",
              pRsp->FormatSupported, pRsp->PlatformCookie, pRsp->Result));

    *pResult = pRsp->Result;
    *pFormatSupported = pRsp->FormatSupported;

    return VINF_SUCCESS;
}

static int parse_SET_TOPOLOGY_RSP(const SHARED_MSG_HEADER_RESPONSE *pHdr, uint32_t cbData, uint32_t *pResult, uint32_t *pTopologyReady)
{
    if (cbData < sizeof(MSG_SET_TOPOLOGY_RSP))
    {
        VIDEOLOG(("cbData < MSG_SET_TOPOLOGY_RSP!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    const MSG_SET_TOPOLOGY_RSP *pRsp = (MSG_SET_TOPOLOGY_RSP *)pHdr;

    VIDEOLOG(("MSG_SET_TOPOLOGY_RSP %d, 0x%08X.\n",
              pRsp->TopologyReady, pRsp->Result));

    *pResult = pRsp->Result;
    *pTopologyReady = pRsp->TopologyReady;

    return VINF_SUCCESS;
}

static int parse_SHUTDOWN_PRESENTATION_RSP(const SHARED_MSG_HEADER_RESPONSE *pHdr, uint32_t cbData, uint32_t *pResult)
{
    if (cbData < sizeof(MSG_SHUTDOWN_PRESENTATION_RSP))
    {
        VIDEOLOG(("cbData < MSG_SHUTDOWN_PRESENTATION_RSP!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    const MSG_SHUTDOWN_PRESENTATION_RSP *pRsp = (MSG_SHUTDOWN_PRESENTATION_RSP *)pHdr;

    VIDEOLOG(("MSG_SHUTDOWN_PRESENTATION_RSP 0x%08X.\n",
              pRsp->Result));

    *pResult = pRsp->Result;

    return VINF_SUCCESS;
}

/** @thread INPUT */
/* static */ void VideoChannelTSMF::dvcTSMF(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    TSMFSTREAM *pStream = (TSMFSTREAM *)pvCtx;
    pStream->pPresentation->pVideoChannelTSMF->processInput(pStream, u32Event, pvData, cbData);
}

/** @thread INPUT */
void VideoChannelTSMF::processInput(TSMFSTREAM *pStream, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    VIDEOLOG(("processInput: stream 0x%x, u32Event %d, pvData %p, cbData %d\n",
              pStream->u32StreamId, u32Event, pvData, cbData));

    switch (u32Event)
    {
        case DVC_EVENT_CREATE:
        {
            pStream->fu8StreamFlags |= TSMFSTREAM_F_DVC_EXISTS;

            RIM_EXCHANGE_CAPABILITY_REQ RIMCaps;
            RIMCaps.hdr.InterfaceId = 2 | STREAM_ID_NONE;
            RIMCaps.hdr.MessageId   = FN_RIM_EXCHANGE_CAPABILITY_REQUEST;
            RIMCaps.hdr.FunctionId  = FN_RIM_EXCHANGE_CAPABILITY_REQUEST;
            RIMCaps.CapabilityValue = RIM_CAPABILITY_VERSION_01;

            TSMFSENDCONTEXT sendContext;
            bool fOutputThread = false;
            initTSMFSendContextNull(&sendContext, fOutputThread, Client());

            sendTSMF(&sendContext, pStream->u8DVCChannelId, &RIMCaps, sizeof(RIMCaps));
        } break;

        case DVC_EVENT_DATA:
        {
            /* Process a message from the client.
             * "Malformed, unrecognized, and out-of-sequence packets
             *  MUST be ignored by the server and the client."
             */

            VRDPLOGRELIO(("TSMF: recv 0x%RX8 %d:\n%.*Rhxd\n", pStream->u8DVCChannelId, cbData, cbData, pvData));

            if (cbData < sizeof(SHARED_MSG_HEADER_RESPONSE))
            {
                VIDEOLOG(("cbData < SHARED_MSG_HEADER_RESPONSE!!!\n"));
                break; /* Ignore the message. */
            }

            const SHARED_MSG_HEADER_RESPONSE *pHdr = (SHARED_MSG_HEADER_RESPONSE *)pvData;

            VIDEOLOG(("dvcTSMF: InterfaceId = 0x%RX32, MessageId 0x%RX32\n",
                      pHdr->InterfaceId, pHdr->MessageId));

            uint32_t InterfaceId = pHdr->InterfaceId & ~STREAM_ID_MASK;

            int rc = VINF_SUCCESS;

            if (InterfaceId == 0)
            {
                rc = processServerData(pStream, pHdr, cbData);
            }
            else if (InterfaceId == 1)
            {
                if (cbData <= sizeof(SHARED_MSG_HEADER))
                {
                    VIDEOLOG(("cbData < SHARED_MSG_HEADER!!!\n"));
                    break; /* Ignore the message. */
                }

                rc = processClientNotifications(pStream, (SHARED_MSG_HEADER *)pHdr, cbData);
            }
            else if (InterfaceId == 2)
            {
                rc = processInterfaceManipulation(pStream, pHdr, cbData);
            }
            else
            {
                VIDEOLOG(("DVC_EVENT_DATA unexpected interface id %d!!!\n",
                          InterfaceId));
                break; /* Ignore the message. */
            }

            if (RT_FAILURE(rc))
            {
                /* This means that DVC channels must be closed. */
                TSMFPRESENTATION *pPresentation = pStream->pPresentation;
                if (pPresentation->mjpeg.fu8StreamFlags & TSMFSTREAM_F_DVC_EXISTS)
                {
                    tsmfCloseChannel(pPresentation->mjpeg.u8DVCChannelId);
                }
                if (pPresentation->control.fu8StreamFlags & TSMFSTREAM_F_DVC_EXISTS)
                {
                    tsmfCloseChannel(pPresentation->control.u8DVCChannelId);
                }
            }
        } break;

        case DVC_EVENT_CLOSE:
        {
            pStream->fu8StreamFlags &= ~TSMFSTREAM_F_DVC_EXISTS;

            TSMFPRESENTATION *pPresentation = pStream->pPresentation;

            if (   (pPresentation->control.fu8StreamFlags & TSMFSTREAM_F_DVC_EXISTS) == 0
                && (pPresentation->mjpeg.fu8StreamFlags & TSMFSTREAM_F_DVC_EXISTS) == 0)
            {
                /* Both channels were closed. Destroy the presentation structure. */
                tsmfPresentationDelete(pPresentation);
            }
        } break;

        default:
            AssertFailed();
    }
}

/** @thread INPUT */
int VideoChannelTSMF::processServerData(TSMFSTREAM *pStream,
                                        const SHARED_MSG_HEADER_RESPONSE *pHdr,
                                        uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    TSMFSENDCONTEXT sendContext;
    bool fOutputThread = false;

    TSMFPRESENTATION *pPresentation = pStream->pPresentation;

    /* The server has set MessageId == FunctionId. */
    switch (pHdr->MessageId)
    {
        case FN_EXCHANGE_CAPABILITIES_REQ:
        {
            uint32_t Result = 0;
            rc = parse_EXCHANGE_CAPABILITIES_RSP(pHdr, cbData, &Result);

            VIDEOLOG(("EXCHANGE_CAPABILITIES_RSP rc = %Rrc, Result = 0x%08X\n",
                      rc, Result));

            if (RT_FAILURE(rc) || Result != 0)
            {
                VRDPLOGRELLIMIT(16, ("TSMF: %Rrc, result 0x%RX32\n", rc, Result));

                if (pStream->u32StreamId != 0)
                {
                    /* Failed to create video stream. Shutdown presentation. */
                    initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

                    sendShutdownPresentation(&sendContext, pPresentation->control.u8DVCChannelId);
                    rc = VINF_NOT_SUPPORTED; /* Ignore the message. */
                }
                else
                {
                    rc = VERR_NOT_SUPPORTED; /* Close DVC channels. */
                }
                break;
            }

            initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

            if (pStream->u32StreamId == 0)
            {
                /* Create a new presentation. */
                sendNewPresentation(&sendContext, pPresentation->control.u8DVCChannelId);

                /* Control channel. Check if the format is supported. */
                sendCheckFormatSupportMJPEG(&sendContext, pPresentation->control.u8DVCChannelId,
                                            &pPresentation->rectScaled);

                VIDEOLOG(("check format: @%d,%d %dx%d\n",
                          pPresentation->rectScaled.x, pPresentation->rectScaled.y,
                          pPresentation->rectScaled.w, pPresentation->rectScaled.h));
            }
            else
            {
                Assert(pStream->u32StreamId == pPresentation->mjpeg.u32StreamId);

                /* Media stream. Add the new stream. Use the control channel. */
                sendAddStreamMJPEG(&sendContext, pPresentation->control.u8DVCChannelId,
                                   pStream->u32StreamId, &pPresentation->rectScaled);

                VIDEOLOG(("add stream: @%d,%d %dx%d\n",
                          pPresentation->rectScaled.x, pPresentation->rectScaled.y,
                          pPresentation->rectScaled.w, pPresentation->rectScaled.h));

                sendSetTopology(&sendContext, pPresentation->control.u8DVCChannelId);
            }
        } break;

        case FN_CHECK_FORMAT_SUPPORT_REQ:
        {
            Assert(pStream->u32StreamId == 0);

            uint32_t Result = 0;
            uint32_t FormatSupported = 0;
            rc = parse_CHECK_FORMAT_SUPPORT_RSP(pHdr, cbData, &Result, &FormatSupported);

            VIDEOLOG(("CHECK_FORMAT_SUPPORT_RSP: rc = %Rrc, Result = 0x%08X, FormatSupported %d\n",
                      rc, Result, FormatSupported));

            if (RT_FAILURE(rc) || Result != 0 || FormatSupported == 0)
            {
                VRDPLOGRELLIMIT(16, ("TSMF: format support %Rrc result 0x%RX32 FormatSupported %d (%dx%d)\n",
                                     rc, Result, FormatSupported,
                                     pPresentation->rectScaled.w, pPresentation->rectScaled.h));

                /* Shutdown presentation. */
                initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

                sendShutdownPresentation(&sendContext, pPresentation->control.u8DVCChannelId);
                rc = VINF_NOT_SUPPORTED;
                break;
            }

            /* Create DVC channel for video stream. */
            rc = Client()->TP()->DVC()->RegisterChannel("TSMF", dvcTSMF,
                                                        &pPresentation->mjpeg,
                                                        &pPresentation->mjpeg.u8DVCChannelId);

            if (RT_SUCCESS(rc))
            {
                VIDEOLOG(("Initialized presentation and sent TSMF data channel request.\n"));
            }
            else
            {
                AssertFailed();

                initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

                sendShutdownPresentation(&sendContext, pPresentation->control.u8DVCChannelId);
                rc = VINF_NOT_SUPPORTED;
            }
        } break;

        case FN_SET_TOPOLOGY_REQ:
        {
            Assert(pStream->u32StreamId == 0);

            uint32_t Result = 0;
            uint32_t TopologyReady = 0;
            rc = parse_SET_TOPOLOGY_RSP(pHdr, cbData, &Result, &TopologyReady);

            VIDEOLOG(("SET_TOPOLOGY_RSP: rc = %Rrc, Result = 0x%08X, TopologyReady %d\n",
                      rc, Result, TopologyReady));

            if (RT_FAILURE(rc) || Result != 0 || TopologyReady == 0)
            {
                VRDPLOGRELLIMIT(16, ("TSMF: topology setup %Rrc result 0x%RX32 ready %d\n",
                                     rc, Result, TopologyReady));
                /* Shutdown the presentation. */
                initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

                sendShutdownPresentation(&sendContext, pPresentation->control.u8DVCChannelId);
                rc = VINF_NOT_SUPPORTED;
                break;
            }

            /* Set u32Status from TSMF_PS_CREATED to TSMF_PS_READY
             * so OUTPUT thread can use the presentation.
             */
            uint32_t u32OldStatus = TSMF_PS_INVALID;
            if (!ASMAtomicCmpXchgExU32(&pPresentation->u32Status, TSMF_PS_READY, TSMF_PS_CREATED, &u32OldStatus))
            {
                /* The presentation was shutdown by the OUTPUT thread. */
                VIDEOLOG(("TSMF_PS_READY not set from %d\n", u32OldStatus));
                Assert(u32OldStatus == TSMF_PS_SHUTDOWN);

                /* Shutdown the presentation. */
                initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

                sendShutdownPresentation(&sendContext, pPresentation->control.u8DVCChannelId);
                rc = VINF_NOT_SUPPORTED;
                break;
            }

            /* The presentation is in a "ready" state, the OUTPUT thread takes control over it. */
        } break;

        case FN_SHUTDOWN_PRESENTATION_REQ:
        {
            Assert(pStream->u32StreamId == 0);

            uint32_t Result = 0;
            rc = parse_SHUTDOWN_PRESENTATION_RSP(pHdr, cbData, &Result);

            VIDEOLOG(("SHUTDOWN_PRESENTATION_RSP: rc = %Rrc, Result = 0x%08X\n",
                      rc, Result));

            rc = VERR_NOT_SUPPORTED; /* Close DVC channels in any case. */
        } break;

        default:
        {
            VIDEOLOG(("DVC_EVENT_DATA[0] unexpected response 0x%RX32!!!\n",
                      pHdr->MessageId));
            rc = VINF_NOT_SUPPORTED;
        } break;
    }

    return rc;
}

int VideoChannelTSMF::processClientNotifications(TSMFSTREAM *pStream,
                                                 const SHARED_MSG_HEADER *pMsgHdr,
                                                 uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    TSMFPRESENTATION *pPresentation = pStream->pPresentation;

    /* Client notification interface. */
    VIDEOLOG(("dvcTSMF: Client FunctionId 0x%04X\n",
              pMsgHdr->FunctionId));

    switch (pMsgHdr->FunctionId)
    {
        case FN_PLAYBACK_ACK:
        {
            if (cbData < sizeof(MSG_PLAYBACK_ACK))
            {
                VIDEOLOG(("cbData < MSG_PLAYBACK_ACK!!!\n"));
                break;
            }

            /* "If the server is controlling the rate at which data is being sent to the client,
             *  this message SHOULD be used as the trigger for sending more data."
             */
            MSG_PLAYBACK_ACK *pPlaybackAck = (MSG_PLAYBACK_ACK *)pMsgHdr;
            RT_NOREF_PV(pPlaybackAck);
            VIDEOLOG(("PLAYBACK_ACK: id 0x%08X, DataDuration %d, cbData %d\n",
                      pPlaybackAck->StreamId, pPlaybackAck->DataDuration, pPlaybackAck->cbData));
        } break;

        case FN_CLIENT_EVENT_NOTIFICATION:
        {
            if (cbData < sizeof(MSG_CLIENT_EVENT_NOTIFICATION_HDR))
            {
                VIDEOLOG(("cbData < MSG_CLIENT_EVENT_NOTIFICATION_HDR!!!\n"));
                break;
            }

            MSG_CLIENT_EVENT_NOTIFICATION_HDR *pNotificationHdr = (MSG_CLIENT_EVENT_NOTIFICATION_HDR *)pMsgHdr;

            VIDEOLOG(("CLIENT_EVENT_NOTIFICATION: id 0x%08X, event 0x%04X, cbData %d\n",
                      pNotificationHdr->StreamId, pNotificationHdr->EventId, pNotificationHdr->cbData));

            if (pNotificationHdr->EventId == TSMM_CLIENT_EVENT_ENDOFSTREAM)
            {
                VIDEOLOG(("TSMM_CLIENT_EVENT_ENDOFSTREAM\n"));

                /* "stream completion has been processed"
                 * Shutdown the presentation.
                 */
                TSMFSENDCONTEXT sendContext;
                bool fOutputThread = false;

                initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

                sendShutdownPresentation(&sendContext, pPresentation->control.u8DVCChannelId);
                rc = VINF_NOT_SUPPORTED;
            }
            else if (pNotificationHdr->EventId == TSMM_CLIENT_EVENT_STOP_COMPLETED)
            {
                VIDEOLOG(("TSMM_CLIENT_EVENT_STOP_COMPLETED\n"));
                /* "playback stop has been processed" */
            }
            else if (pNotificationHdr->EventId == TSMM_CLIENT_EVENT_START_COMPLETED)
            {
                VIDEOLOG(("TSMM_CLIENT_EVENT_START_COMPLETED\n"));
                /* "playback start has been processed" */
            }
            else if (pNotificationHdr->EventId == TSMM_CLIENT_EVENT_MONITORCHANGED)
            {
                VIDEOLOG(("TSMM_CLIENT_EVENT_MONITORCHANGED\n"));
                /* "renderer display settings have changed" */
            }
        } break;
        default:
        {
            rc = VINF_NOT_SUPPORTED;
        } break;
    }

    return rc;
}

int VideoChannelTSMF::processInterfaceManipulation(TSMFSTREAM *pStream,
                                                   const SHARED_MSG_HEADER_RESPONSE *pHdr,
                                                   uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    TSMFSENDCONTEXT sendContext;
    bool fOutputThread = false;

    TSMFPRESENTATION *pPresentation = pStream->pPresentation;

    if (pHdr->MessageId == FN_RIM_EXCHANGE_CAPABILITY_REQUEST)
    {
        if (cbData >= sizeof(RIM_EXCHANGE_CAPABILITY_RESPONSE))
        {
            const RIM_EXCHANGE_CAPABILITY_RESPONSE *pRsp = (RIM_EXCHANGE_CAPABILITY_RESPONSE *)pHdr;
            VIDEOLOG(("RIM_EXCHANGE_CAPABILITY_RESPONSE: CapabilityValue %d, Result 0x%08X\n",
                       pRsp->CapabilityValue, pRsp->Result));

            if (   pRsp->CapabilityValue == RIM_CAPABILITY_VERSION_01
                && pRsp->Result == 0)
            {
                VIDEOLOG(("RIM OK\n"));
            }
            else
            {
                VRDPLOGRELLIMIT(16, ("TSMF: not enabled. RIM caps %x, result %x\n",
                                     pRsp->CapabilityValue, pRsp->Result));
                rc = VERR_NOT_SUPPORTED;
            }
        }
        else
        {
            VIDEOLOG(("cbData < RIM_EXCHANGE_CAPABILITY_RESPONSE!!!\n"));
            rc = VERR_NOT_SUPPORTED;
        }

        initTSMFSendContextNull(&sendContext, fOutputThread, Client());

        /* Have to send Release request. */
        RIMCALL_RELEASE RIMRelease;
        RIMRelease.hdr.InterfaceId = 2 | STREAM_ID_NONE;
        RIMRelease.hdr.MessageId   = FN_RIMCALL_RELEASE;
        RIMRelease.hdr.FunctionId  = FN_RIMCALL_RELEASE;
        sendTSMF(&sendContext, pStream->u8DVCChannelId, &RIMRelease, sizeof(RIMRelease));

        if (RT_SUCCESS(rc))
        {
            /* TSMF messages start. Init the context for them. */
            initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

            /* Also send the channel setup messages for the channel. */
            sendSetChannelParams(&sendContext, pStream->u8DVCChannelId, pStream->u32StreamId);

            sendExchangeCapabilitiesReq(&sendContext, pStream->u8DVCChannelId);
        }
        else
        {
            if (pStream->u32StreamId != 0)
            {
                /* Failed to create video stream. Shutdown presentation. */
                initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

                sendShutdownPresentation(&sendContext, pPresentation->control.u8DVCChannelId);
            }
            else
            {
                /* rc is a VERR_ here and DVC streams will be closed. */
            }
        }
    }
    else
    {
        VIDEOLOG(("DVC_EVENT_DATA[2] unexpected response 0x%RX32!!!\n",
                  pHdr->MessageId));
        rc = VINF_NOT_SUPPORTED;
    }

    return rc;
}

/** @thread OUTPUT */
static DECLCALLBACK(void) processCloseChannel(VRDPClient *pClient,
                                              const VRDPOUTPUTGENERICHDR *pHdr)
{
    uint8_t u8DVCChannelId = (uint8_t)pHdr->u32Parm;
    pClient->TP()->DVC()->SendCloseChannel(u8DVCChannelId);
}

void VideoChannelTSMF::tsmfCloseChannel(uint8_t u8DVCChannelId)
{
    VIDEOLOG(("close DVC %d\n", u8DVCChannelId));

    VRDPOUTPUTGENERICHDR pkt;
    pkt.pfnCallback = processCloseChannel;
    pkt.pvUser      = NULL;
    pkt.u32Size     = sizeof(pkt);
    pkt.u32Parm     = u8DVCChannelId;

    Client()->Server()->PostOutput(VRDP_OUTPUT_GENERIC, Client()->Id(), &pkt, sizeof(pkt));
    Client()->Server()->RaiseOutputEvent();
}

/*
 * TSMF video channel.
 */
VideoChannelTSMF::VideoChannelTSMF(VRDPClient *pClient, PFNVIDEOCHANNELEVENT pfnEvent, void *pvEvent)
    :
    VideoChannel(pClient, pfnEvent, pvEvent),
    m_fAvailabilityReported(false),
    m_fUTTSCSyncMode (false),
    m_fAudioClosed (true)
{
    RTListInit(&m_listPresentations);
    VRDPLock::Create("VideoTSMF", &m_pLock);
}

VideoChannelTSMF::~VideoChannelTSMF()
{
    Assert(RTListIsEmpty(&m_listPresentations));
    VRDPLock::Delete(&m_pLock);
}

TSMFPRESENTATION *VideoChannelTSMF::tsmfPresentationById(uint32_t u32VideoStreamId)
{
    if (RT_FAILURE(VRDPLock::Lock(m_pLock)))
    {
        return NULL;
    }

    TSMFPRESENTATION *pIter;
    RTListForEach(&m_listPresentations, pIter, TSMFPRESENTATION, nodePresentation)
    {
        if (u32VideoStreamId == pIter->u32VideoStreamId)
        {
            VRDPPktAddRef(&pIter->pkt);
            VRDPLock::Unlock(m_pLock);
            return pIter;
        }
    }

    VRDPLock::Unlock(m_pLock);
    return NULL;
}

int VideoChannelTSMF::tsmfPresentationListCopyGet(RTLISTANCHOR *pList)
{
    if (RT_FAILURE(VRDPLock::Lock(m_pLock)))
    {
        return VERR_NOT_SUPPORTED;
    }

    RTListInit(pList);

    TSMFPRESENTATION *pIter;
    RTListForEach(&m_listPresentations, pIter, TSMFPRESENTATION, nodePresentation)
    {
        VRDPPktAddRef(&pIter->pkt);
        RTListAppend(pList, &pIter->nodeCopy);
    }

    VRDPLock::Unlock(m_pLock);
    return VINF_SUCCESS;
}

void VideoChannelTSMF::tsmfPresentationListCopyRelease(RTLISTANCHOR *pList)
{
    TSMFPRESENTATION *pIter;
    TSMFPRESENTATION *pNext;
    RTListForEachSafe(pList, pIter, pNext, TSMFPRESENTATION, nodeCopy)
    {
        RTListNodeRemove(&pIter->nodeCopy);
        VRDPPktRelease(&pIter->pkt);
    }
}

void VideoChannelTSMF::tsmfPresentationDelete(TSMFPRESENTATION *pPresentation)
{
    /* Remove from list. */
    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        RTListNodeRemove(&pPresentation->nodePresentation);
        VRDPLock::Unlock(m_pLock);
    }
    else
    {
        /* Should not happen. The structure remains in the list. */
        AssertFailed();

        Assert(pPresentation->u32VideoStreamId == 0);
        ASMAtomicWriteU32(&pPresentation->u32Status, TSMF_PS_INVALID);
        VRDPPktAddRef(&pPresentation->pkt); /* Make sure it is not freed. */
    }

    VRDPPktRelease(&pPresentation->pkt);
}

/** @thread OUTPUT */
int VideoChannelTSMF::VideoChannelSetup(void)
{
    /** @todo Init/term, VINF_VRDP_OUTPUT_ENABLE, DVC. */

    VRDPData *pData = Client()->TP()->Data();

    if (pData)
    {
        const uint8_t *pu8 = pData->ClientDigProductId();
        if (   pu8[0] == 'S'
            && pu8[1] == 'R'
            && pu8[2] == 'W'
            && pu8[3] == 'C'
           )
        {
            m_fUTTSCSyncMode = true;
        }
    }

    if (m_fUTTSCSyncMode)
    {
        VRDPLOGREL(("Using UTTSC sync mode.\n"));
    }

    /* Always report the channel as available for now.
     * In principle it is necessary to try to create a TSMF channel to verify this.
     */
    VHCONTEXT *pCtx = Client()->Server()->VideoHandler();
    if (pCtx)
    {
        if (!m_fAvailabilityReported)
        {
            /* Report once, because this method is called can be called multiple times.
             * But notification about client disconnect is called only when the client
             * is destroyed.
             */
            m_fAvailabilityReported = true;

            videoHandlerNotifyClientConnect(pCtx);
        }

        Event(VIDEO_CHANNEL_SUPPORTED, NULL, 0);
        Event(VIDEO_CHANNEL_ACTIVE, NULL, 0);
    }
    return VINF_SUCCESS;
}

/* Called when a presentation structure is deallocated. */
static void tsmfPresentationFree(VRDPPKT *pPkt)
{
    TSMFPRESENTATION *p = (TSMFPRESENTATION *)pPkt;
    VIDEOLOG(("free presentation %p\n", p));
    NOREF(p);
}

/** @thread OUTPUT */
int VideoChannelTSMF::VideoChannelStart(const VHSTREAMPARMS *pStream)
{
    /*
     * Create a new TSMF presentation.
     */

    /* Check if there is already presentation with for the stream. */
    TSMFPRESENTATION *pPresentation = tsmfPresentationById(pStream->u32VideoStreamId);

    if (pPresentation)
    {
        VRDPPktRelease(&pPresentation->pkt);
        return VINF_ALREADY_INITIALIZED;
    }

    VIDEOLOG(("VideoChannelTSMF::VideoChannelStart\n"));

    /* Allocate a presentation state for this stream on this client. */
    pPresentation = (TSMFPRESENTATION *)VRDPMemAllocZ(sizeof(TSMFPRESENTATION));

    if (!pPresentation)
    {
        VIDEOLOG(("Failed to allocate a presentation!!!\n"));
        return VERR_NO_MEMORY;
    }

    /* Initialize the presentation. */
    VRDPPktInit(&pPresentation->pkt, 0, sizeof(TSMFPRESENTATION), tsmfPresentationFree, NULL);
    VRDPPktAddRef(&pPresentation->pkt);

    pPresentation->videoRectHandle = SB_HANDLE_NULL;
    pPresentation->pVideoChannelTSMF = this;
    pPresentation->u32Status = TSMF_PS_CREATED;
    pPresentation->uScreenId  = pStream->uScreenId;
    pPresentation->u32VideoStreamId  = pStream->u32VideoStreamId;
    memcpy(&pPresentation->uuid, &pStream->uuid, sizeof(pPresentation->uuid));

    /* Presentation rect is in the client coordinates, stream - in shadow buffer. */
    pPresentation->rectShadowBuffer  = pStream->rectOriginal;
    pPresentation->rectScaled        = pStream->rectScaled;
    Client()->TP()->DesktopMap()->Screen2Client (pPresentation->uScreenId,
                                                 &pPresentation->rectShadowBuffer,
                                                 &pPresentation->rectClient);
    pPresentation->u64VideoWindowId  = (uint64_t)(uintptr_t)&pPresentation->u64VideoWindowId; /* Arbitrary. */
    pPresentation->u64ParentWindowId = (uint64_t)(uintptr_t)&pPresentation->u64ParentWindowId; /* Arbitrary. */
    pPresentation->fDirect           = pStream->fDirect;
    pPresentation->callbackData      = pStream->callbackData;
    pPresentation->i64LipSyncDelay   = pStream->i64LipSyncDelay;

    pPresentation->control.pPresentation   = pPresentation;
    pPresentation->control.u32StreamId     = 0;
    pPresentation->control.u8DVCChannelId  = 0;
    pPresentation->control.fu8StreamFlags  = 0;

    pPresentation->mjpeg.pPresentation     = pPresentation;
    pPresentation->mjpeg.u32StreamId       = TSMF_DATA_STREAM_ID_MJPEG;
    pPresentation->mjpeg.u8DVCChannelId    = 0;
    pPresentation->mjpeg.fu8StreamFlags    = 0;

    int rc = VINF_SUCCESS;

    /* Add to the list of presentations. Must be done before registering the DVC channel. */
    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        RTListPrepend(&m_listPresentations, &pPresentation->nodePresentation);
        VRDPLock::Unlock(m_pLock);
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        /* Create DVC channel for TSMF control stream. */
        rc = Client()->TP()->DVC()->RegisterChannel("TSMF", dvcTSMF,
                                                    &pPresentation->control,
                                                    &pPresentation->control.u8DVCChannelId);
    }

    if (RT_SUCCESS(rc))
    {
        VIDEOLOG(("Initialized presentation and sent TSMF control channel request.\n"));
    }
    else
    {
        VIDEOLOG(("%Rrc\n", rc));
        tsmfPresentationDelete(pPresentation);
    }

    return rc;
}

/* virtual */ void VideoChannelTSMF::OnAudioSend(uint64_t u64PacketStartNanoTS,
                                                 uint64_t u64PacketEndNanoTS,
                                                 uint64_t u64DurationNano)
{
    NOREF(u64PacketStartNanoTS);

    if (!isUTTSCSyncMode())
    {
        return;
    }

    uint64_t u64NowNS = VRDPTimeNanoTS();

    int64_t i64BeginTimeline;

    /* Check all presentations. */
    RTLISTANCHOR list;
    int rc = tsmfPresentationListCopyGet(&list);
    if (RT_FAILURE(rc))
    {
        return;
    }

    TSMFPRESENTATION *pPresentation = NULL;
    RTListForEach(&list, pPresentation, TSMFPRESENTATION, nodeCopy)
    {
        if (ASMAtomicCmpXchgU32(&pPresentation->u32Status, TSMF_PS_SETUP, TSMF_PS_READY))
        {
            /* The OUTPUT thread can take control over the presentation. */

            /* Notify upper components that the channel has been created. */
            if (pPresentation->callbackData.VHStreamNotify)
            {
                uint32_t u32VideoStreamId = pPresentation->u32VideoStreamId;
                pPresentation->callbackData.VHStreamNotify(pPresentation->callbackData.pvContext,
                                                           VRDE_IMAGE_NOTIFY_HANDLE_CREATE,
                                                           &u32VideoStreamId,
                                                           sizeof(u32VideoStreamId));
            }
        }

        uint32_t u32Status = ASMAtomicReadU32(&pPresentation->u32Status);

        bool fStartPlayback = false;

        if (u32Status == TSMF_PS_SETUP)
        {
            VIDEOLOG(("OnAudioSend: presentation %RTuuid is Ready\n", &pPresentation->uuid));

            /* Check if it is time to send OnPlaybackStarted. */
            uint64_t u64FirstFrameNanoTS = pPresentation->u64FirstFrameNanoTS;

            if (u64FirstFrameNanoTS == 0)
            {
                /* No frames for the presentation yet. */
                VIDEOLOG(("OnAudioSend: no frames yet!!!\n"));
                continue;
            }

            /* Start the video if it is a direct stream or the frame must be synchronized with the packet. */
            if (   pPresentation->fDirect
                || u64FirstFrameNanoTS <= u64PacketEndNanoTS)
            {
                VIDEOLOG(("OnAudioSend: first %lld < pkt end %lld, diff to end %lld, diff to start %lld\n",
                          u64FirstFrameNanoTS,
                          u64PacketEndNanoTS,
                          u64PacketEndNanoTS - u64FirstFrameNanoTS,
                          u64PacketStartNanoTS - u64FirstFrameNanoTS));
#ifdef LOG_ENABLED
                if (u64PacketStartNanoTS > pPresentation->u64FirstFrameNanoTS)
                {
                     VIDEOLOG(("OnAudioSend: frame is before the packet!!!\n"));
                }
#endif

                /* The packet either includes or follows the frame. Start the video. */
                pPresentation->videoRectHandle = shadowBufferCoverAdd(pPresentation->uScreenId,
                                                                      &pPresentation->rectShadowBuffer);
                ASMAtomicWriteU32(&pPresentation->u32Status, TSMF_PS_PLAYBACK);

                i64BeginTimeline = u64PacketEndNanoTS / 100;   /* Note: this is only correct when the first frame
                                                                * is within the packet, which usually should be true.
                                                                */

                i64BeginTimeline += pPresentation->i64LipSyncDelay; /* Exclude lipsync again. */

                pPresentation->i64BeginTimeline = i64BeginTimeline; /* Update the value. */

                /* At the playback start the pPresentation->u64AudioTimeNS is already 0. */
                Assert(pPresentation->u64AudioTimeNS == 0);

                pPresentation->u64LastTimeLineCorrection = u64NowNS;

                fStartPlayback = true;

                VIDEOLOG(("Presentation %RTuuid start playback, begin tl adjusted to %lld.\n",
                          &pPresentation->uuid, i64BeginTimeline));
            }
        }
        else if (u32Status == TSMF_PS_PLAYBACK)
        {
            VIDEOLOG(("OnAudioSend: presentation %RTuuid is Playback\n", &pPresentation->uuid));

            if (!pPresentation->fDirect)
            {
                /* Periodically update pPresentation->i64BeginTimeline. */
                if (m_fAudioClosed)
                {
                    /* Restart AudioTime counting. */
                    pPresentation->u64AudioTimeNS = u64PacketEndNanoTS - pPresentation->i64BeginTimeline * 100;
                    pPresentation->u64LastTimeLineCorrection = u64NowNS; /* Next correction after awhile. */

                    VHSTATLOG(("VHSTAT: audio: OnAudioSend: restart audio [%RTuuid] %lld %lld\n",
                               &pPresentation->uuid, u64NowNS, pPresentation->u64AudioTimeNS));
                }
                else
                {
                    pPresentation->u64AudioTimeNS += u64DurationNano;

                    /* Every 2 seconds. */
                    if (u64NowNS - pPresentation->u64LastTimeLineCorrection > UINT64_C(2000000000))
                    {
                        pPresentation->u64LastTimeLineCorrection = u64NowNS;

                        int64_t i64BeginTimelineSync = (u64PacketEndNanoTS - pPresentation->u64AudioTimeNS) / 100;
                        i64BeginTimelineSync += pPresentation->i64LipSyncDelay; /* Exclude lipsync again. */

                        VHSTATLOG(("VHSTAT: audio: OnAudioSend: time: [%RTuuid] %lld %lld %lld\n",
                                   &pPresentation->uuid,
                                   u64NowNS / UINT64_C(1000000),
                                   pPresentation->u64AudioTimeNS / UINT64_C(1000000),
                                   i64BeginTimelineSync / UINT64_C(10000)));

                        pPresentation->i64BeginTimeline = i64BeginTimelineSync; /* Update the value. */
                    }
                }
            }
        }

        if (fStartPlayback)
        {
            bool fOutputThread = true;

            TSMFSENDCONTEXT sendContext;
            initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

            sendSetVideoWindow(&sendContext, pPresentation->control.u8DVCChannelId,
                               pPresentation->u64VideoWindowId, pPresentation->u64ParentWindowId);
            sendOnPlaybackStarted(&sendContext, pPresentation->control.u8DVCChannelId);

            /* SendFrames must send the window visile region, etc. */
            pPresentation->fProcessStartPlayback = true;
        }
    }

    tsmfPresentationListCopyRelease(&list);

    m_fAudioClosed = false;

    return;
}

/* virtual */ void VideoChannelTSMF::OnAudioClose(void)
{
    VHSTATLOG(("VHSTAT: audio: OnAudioClose: %lld\n", VRDPTimeNanoTS()));
    m_fAudioClosed = true;
    return;
}

/** @thread OUTPUT */
void VideoChannelTSMF::vcSendFramesUTTSC(VHCONTEXT *pCtx,
                                         VHSTREAMPARMS *pStream,
                                         RTLISTANCHOR *pListFramesToSend,
                                         int64_t i64NowTimeline,
                                         TSMFPRESENTATION *pPresentation)
{
    uint32_t u32VideoStreamId = pStream->u32VideoStreamId;
    RT_NOREF_PV(u32VideoStreamId); /* logging */

    bool fStart = false; /* Indicates that this is the first frames in the presentation. */

    /* Save timestamp of the first frame in this presentation. */
    int64_t i64BeginTimeline = pPresentation->i64BeginTimeline;
    if (RT_UNLIKELY(!i64BeginTimeline))
    {
        VHOUTPUTFRAME *pOutputFrame = RTListGetFirst(pListFramesToSend,
                                                     VHOUTPUTFRAME,
                                                     NodeOutputFrame);
        Assert(pOutputFrame);
        if (pOutputFrame)
        {
            i64BeginTimeline = pOutputFrame->i64SampleStartTime;
            pPresentation->i64BeginTimeline = i64BeginTimeline;
            pPresentation->i64BeginTimestamp = pOutputFrame->i64Timestamp;
            fStart = true;
            VIDEOLOG(("Presentation start for id %d, timeline %lld\n", u32VideoStreamId, i64BeginTimeline));

            /** @todo already have i64BeginTimestamp? */
            pPresentation->u64FirstFrameNanoTS = pOutputFrame->i64Timestamp * 100; /* timeline -> ns */
            VIDEOLOG(("Saved first frame timestamp %lld for id %d\n", pPresentation->u64FirstFrameNanoTS, u32VideoStreamId));
        }
    }

    bool fStartPlayback = false;
    bool fPreroll = false;

    if (ASMAtomicReadU32(&pPresentation->u32Status) == TSMF_PS_SETUP)
    {
        /* Check if it is time to actually start playback in case there is no audio. */
        if (pPresentation->fDirect)
        {
            /* The playback must start immediately, also direct presentations do not use covers. */
            ASMAtomicWriteU32(&pPresentation->u32Status, TSMF_PS_PLAYBACK);
            VIDEOLOG(("Force presentation start playback for direct id %d, timeline %lld, now %lld, delay %lld\n",
                      u32VideoStreamId, i64BeginTimeline, i64NowTimeline, i64NowTimeline - pPresentation->i64BeginTimestamp));
            fStartPlayback = true;
        }
        else if (i64NowTimeline >= i64BeginTimeline)
        {
            /* If audio exists, then PlaybackStarted is actually sent earlier than the i64BeginTimeline. */
            pPresentation->videoRectHandle = shadowBufferCoverAdd(pPresentation->uScreenId, &pPresentation->rectShadowBuffer);
            ASMAtomicWriteU32(&pPresentation->u32Status, TSMF_PS_PLAYBACK);
            VIDEOLOG(("Force presentation start playback for id %d, timeline %lld, now %lld, delay %lld\n",
                      u32VideoStreamId, i64BeginTimeline, i64NowTimeline, i64NowTimeline - pPresentation->i64BeginTimestamp));
            fStartPlayback = true;
        }
        else
        {
            if (fStart)
            {
                /* This is the first frame and it is too early to start playback. */
                VIDEOLOG(("Presentation preroll for id %d, timeline %lld, now %lld\n", u32VideoStreamId, i64BeginTimeline, i64NowTimeline));
                fPreroll = true;
            }
        }
    }

    /* Make local copies of the presentation data. */
    uint32_t u32StreamIdMJPEG = pPresentation->mjpeg.u32StreamId;
    RGNRECT rectClient = pPresentation->rectClient;
    /*RGNRECT rectShadowBuffer = pPresentation->rectShadowBuffer; - unused */
    uint64_t u64VideoWindowId = pPresentation->u64VideoWindowId;
    uint64_t u64ParentWindowId = pPresentation->u64ParentWindowId;
    bool fDirect = pPresentation->fDirect;
    int64_t i64MaxSampleStartTime = pPresentation->i64MaxSampleStartTime;

    bool fOutputThread = true;

    TSMFSENDCONTEXT sendContext;
    initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

    if (fPreroll)
    {
        sendNotifyPreroll(&sendContext, pPresentation->mjpeg.u8DVCChannelId, u32StreamIdMJPEG);
    }
    else if (fStartPlayback || pPresentation->fProcessStartPlayback)
    {
        if (pPresentation->fProcessStartPlayback)
        {
            /* OnAudioSend already started playback. */
            pPresentation->fProcessStartPlayback = false;
            VIDEOLOG(("Presentation %RTuuid playback started by OnAudioSend\n",
                      &pPresentation->uuid));
        }
        else
        {
            sendSetVideoWindow(&sendContext, pPresentation->control.u8DVCChannelId, u64VideoWindowId, u64ParentWindowId);
            sendOnPlaybackStarted(&sendContext, pPresentation->control.u8DVCChannelId);

            /* At the playback start the pPresentation->u64AudioTimeNS is already 0. */
            Assert(pPresentation->u64AudioTimeNS == 0);

            if (fDirect)
            {
                int64_t i64StartPlayback = VRDPTimeMilliTS();
                i64BeginTimeline = VH_TIMELINE_TIME_FROM_MS(i64StartPlayback);
                pPresentation->i64BeginTimeline = i64BeginTimeline;

            }
        }

        if (pStream->cVisibleRects)
        {
            VIDEOLOG(("Sending %d rectangles for UpdateGeometryInfo.\n", pStream->cVisibleRects));

            sendUpdateGeometryInfo(&sendContext, pPresentation->control.u8DVCChannelId, u64VideoWindowId, &rectClient,
                                   pStream->cVisibleRects, pStream->paVisibleRects);
        }
        else
        {
            VIDEOLOG(("No rectangles for UpdateGeometryInfo.\n"));

            RTRECT visibleRect;
            visibleRect.xLeft = 0;
            visibleRect.yTop = 0;
            visibleRect.xRight = rectClient.w - 1;
            visibleRect.yBottom = rectClient.h - 1;
            sendUpdateGeometryInfo(&sendContext, pPresentation->control.u8DVCChannelId, u64VideoWindowId, &rectClient, 1, &visibleRect);
        }
    }

    VHOUTPUTFRAME *pOutputFrame;
    RTListForEach(pListFramesToSend, pOutputFrame, VHOUTPUTFRAME, NodeOutputFrame)
    {
        VHSTATCALL(pCtx->pServer, SendFrameBegin, (pStream->pStatCtx,
                                                   pOutputFrame->i64SampleStartTime, pOutputFrame->i64Timestamp));

        if (fDirect)
        {
            /* Frames in direct streams are displayed immediately by the client.
             * Send only the last one if many are pending.
             */
            if (RTListNodeIsLast(pListFramesToSend, &pOutputFrame->NodeOutputFrame))
            {
                uint32_t u32SampleExtensions = TSMM_SAMPLE_EXT_CLEANPOINT | TSMM_SAMPLE_EXT_RELATIVE_TIMESTAMPS
                                               | TSMM_SAMPLE_EXT_HAS_NO_TIMESTAMPS | TSMM_SAMPLE_EXT_DISCONTINUITY;

                sendOnSample(&sendContext, pPresentation->mjpeg.u8DVCChannelId, u32StreamIdMJPEG,
                             u32SampleExtensions,
                             pOutputFrame->pu8JPEG,
                             pOutputFrame->cbJPEG,
                             0,
                             0);
                VHSTATCALL(pCtx->pServer, SendFrameEnd, (pStream->pStatCtx, true));
            }
            else
            {
                VHSTATCALL(pCtx->pServer, SendFrameEnd, (pStream->pStatCtx, false));
            }
        }
        else
        {
            uint32_t u32SampleExtensions = TSMM_SAMPLE_EXT_CLEANPOINT | TSMM_SAMPLE_EXT_RELATIVE_TIMESTAMPS;

            if (fStart)
            {
                fStart = false;
                u32SampleExtensions |= TSMM_SAMPLE_EXT_DISCONTINUITY;
            }

            int64_t i64SampleStartTime = pOutputFrame->i64SampleStartTime - i64BeginTimeline;
            int64_t i64SampleEndTime = pOutputFrame->i64SampleEndTime - i64BeginTimeline;

            bool fSendFrame;

            /* Make sure that iSampleStartTime is increasing always. */
            if (   i64MaxSampleStartTime == 0
                || i64MaxSampleStartTime < i64SampleStartTime)
            {
                i64MaxSampleStartTime = i64SampleStartTime;
                fSendFrame = true;
            }
            else
            {
                VHSTATLOG(("VHSTAT: video: SST: %lld %lld %lld\n",
                           i64MaxSampleStartTime, i64SampleStartTime, i64MaxSampleStartTime - i64SampleStartTime));
                fSendFrame = false;
            }

            if (   fSendFrame
                && pOutputFrame->i64SampleStartTime >= i64BeginTimeline) /* Exclude negative relative timestamps. */
            {
                sendOnSample(&sendContext, pPresentation->mjpeg.u8DVCChannelId, u32StreamIdMJPEG,
                             u32SampleExtensions,
                             pOutputFrame->pu8JPEG,
                             pOutputFrame->cbJPEG,
                             i64SampleStartTime,
                             i64SampleEndTime);
                VHSTATCALL(pCtx->pServer, SendFrameEnd, (pStream->pStatCtx, true));
            }
            else
            {
                VHSTATCALL(pCtx->pServer, SendFrameEnd, (pStream->pStatCtx, false));
            }
        }
    }

    pPresentation->i64MaxSampleStartTime = i64MaxSampleStartTime;
}

/** @thread OUTPUT */
void VideoChannelTSMF::vcSendFramesMSTSC(VHCONTEXT *pCtx,
                                         VHSTREAMPARMS *pStream,
                                         RTLISTANCHOR *pListFramesToSend,
                                         int64_t i64NowTimeline,
                                         TSMFPRESENTATION *pPresentation)
{
    uint32_t u32VideoStreamId = pStream->u32VideoStreamId;
    RT_NOREF_PV(u32VideoStreamId); /* logging */

    /* Save timestamp of the first frame in this presentation. */
    int64_t i64BeginTimeline = pPresentation->i64BeginTimeline;
    bool fStart = false;
    if (RT_UNLIKELY(!i64BeginTimeline))
    {
        VHOUTPUTFRAME *pOutputFrame = RTListGetFirst(pListFramesToSend,
                                                     VHOUTPUTFRAME,
                                                     NodeOutputFrame);
        Assert(pOutputFrame);
        if (pOutputFrame)
        {
            i64BeginTimeline = pOutputFrame->i64SampleStartTime;
            pPresentation->i64BeginTimeline = i64BeginTimeline;
            pPresentation->i64BeginTimestamp = pOutputFrame->i64Timestamp;
            fStart = true;
            VIDEOLOG(("Presentation start for id %d, timeline %lld\n", u32VideoStreamId, i64BeginTimeline));
        }
    }

    bool fPreroll = false;
    bool fStartPlayback = false;

    if (ASMAtomicReadU32(&pPresentation->u32Status) == TSMF_PS_SETUP)
    {
        /* Check if it is time to actually start playback */
        if (pPresentation->fDirect)
        {
            /* The playback must start immediately, also direct presentations do not use covers. */
            ASMAtomicWriteU32(&pPresentation->u32Status, TSMF_PS_PLAYBACK);
            VIDEOLOG(("Presentation start playback for direct id %d, timeline %lld, now %lld, delay %lld\n",
                      u32VideoStreamId, i64BeginTimeline, i64NowTimeline, i64NowTimeline - pPresentation->i64BeginTimestamp));
            fStartPlayback = true;
        }
        else if (i64NowTimeline >= i64BeginTimeline)
        {
            pPresentation->videoRectHandle = shadowBufferCoverAdd(pPresentation->uScreenId, &pPresentation->rectShadowBuffer);
            ASMAtomicWriteU32(&pPresentation->u32Status, TSMF_PS_PLAYBACK);
            VIDEOLOG(("Presentation start playback for id %d, timeline %lld, now %lld, delay %lld\n",
                      u32VideoStreamId, i64BeginTimeline, i64NowTimeline, i64NowTimeline - pPresentation->i64BeginTimestamp));
            fStartPlayback = true;
        }
        else
        {
            if (fStart)
            {
                /* This is the first frame and it is too early to start playback. */
                VIDEOLOG(("Presentation preroll for id %d, timeline %lld, now %lld\n", u32VideoStreamId, i64BeginTimeline, i64NowTimeline));
                fPreroll = true;
            }
        }
    }

    /* Make local copies of the presentation data. */
    uint32_t u32StreamIdMJPEG = pPresentation->mjpeg.u32StreamId;
    RGNRECT rectClient = pPresentation->rectClient;
    /*RGNRECT rectShadowBuffer = pPresentation->rectShadowBuffer; - unused */
    uint64_t u64VideoWindowId = pPresentation->u64VideoWindowId;
    uint64_t u64ParentWindowId = pPresentation->u64ParentWindowId;
    bool fDirect = pPresentation->fDirect;

    bool fOutputThread = true;

    TSMFSENDCONTEXT sendContext;
    initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

    if (fPreroll)
    {
        sendNotifyPreroll(&sendContext, pPresentation->mjpeg.u8DVCChannelId, u32StreamIdMJPEG);
    }
    else if (fStartPlayback)
    {
        Assert(!pPresentation->fProcessStartPlayback); /* OnAudioSend is not used for MSTSC. */

        sendSetVideoWindow(&sendContext, pPresentation->control.u8DVCChannelId, u64VideoWindowId, u64ParentWindowId);
        sendOnPlaybackStarted(&sendContext, pPresentation->control.u8DVCChannelId);

        /* Frames use timestamps which are relative to the PlaybackStarted message.
         *
         * For autodetected streams the first frame relative timestamp is always 0,
         * and the playback start is delayed and frames are queued on the client.
         * The first few frame were already sent to the client when PlaybackStarted is issued.
         * This means the "lipsync" effect is to delay the PlaybackStarted message.
         * The delay is included in the first frame absolute SampleStart timestamp.
         *
         * @todo No need to include "lipsynch" to every frame? Check here if "lipsynch"
         * time is elapsed since first frame was detected?
         *
         * For direct streams the delay must not exist. The frames must be displayed
         * as soon as possible on the client. Their SampleStart does not include "lipsync"
         * and is equal to the averaged timestamp of the frame. Which means their
         * relative timestamps must be calculated relative to the time when PlaybackStarted
         * was sent. So they are displayed in real time.
         */
        if (fDirect)
        {
            int64_t i64StartPlayback = VRDPTimeMilliTS();
            i64BeginTimeline = VH_TIMELINE_TIME_FROM_MS(i64StartPlayback);
            pPresentation->i64BeginTimeline = i64BeginTimeline;

        }

        if (pStream->cVisibleRects)
        {
            VIDEOLOG(("Sending %d rectangles for UpdateGeometryInfo.\n", pStream->cVisibleRects));

            sendUpdateGeometryInfo(&sendContext, pPresentation->control.u8DVCChannelId, u64VideoWindowId, &rectClient,
                                   pStream->cVisibleRects, pStream->paVisibleRects);
        }
        else
        {
            VIDEOLOG(("No rectangles for UpdateGeometryInfo.\n"));

            RTRECT visibleRect;
            visibleRect.xLeft = 0;
            visibleRect.yTop = 0;
            visibleRect.xRight = rectClient.w - 1;
            visibleRect.yBottom = rectClient.h - 1;
            sendUpdateGeometryInfo(&sendContext, pPresentation->control.u8DVCChannelId, u64VideoWindowId, &rectClient, 1, &visibleRect);
        }
    }

    VHOUTPUTFRAME *pOutputFrame;
    RTListForEach(pListFramesToSend, pOutputFrame, VHOUTPUTFRAME, NodeOutputFrame)
    {
        VHSTATCALL(pCtx->pServer, SendFrameBegin, (pStream->pStatCtx,
                                                   pOutputFrame->i64SampleStartTime, pOutputFrame->i64Timestamp));

        if (fDirect)
        {
            /* Frames in direct streams are displayed immediately by the client.
             * Send only the last one if many are pending.
             */
            if (RTListNodeIsLast(pListFramesToSend, &pOutputFrame->NodeOutputFrame))
            {
                uint32_t u32SampleExtensions = TSMM_SAMPLE_EXT_CLEANPOINT | TSMM_SAMPLE_EXT_RELATIVE_TIMESTAMPS
                                               | TSMM_SAMPLE_EXT_HAS_NO_TIMESTAMPS | TSMM_SAMPLE_EXT_DISCONTINUITY;

                sendOnSample(&sendContext, pPresentation->mjpeg.u8DVCChannelId, u32StreamIdMJPEG,
                             u32SampleExtensions,
                             pOutputFrame->pu8JPEG,
                             pOutputFrame->cbJPEG,
                             0,
                             0);

                VHSTATCALL(pCtx->pServer, SendFrameEnd, (pStream->pStatCtx, true));
            }
            else
            {
                VHSTATCALL(pCtx->pServer, SendFrameEnd, (pStream->pStatCtx, false));
            }
        }
        else
        {
            if (pOutputFrame->i64SampleStartTime >= i64BeginTimeline) /* Exclude negative relative timestamps. */
            {
                uint32_t u32SampleExtensions = TSMM_SAMPLE_EXT_CLEANPOINT | TSMM_SAMPLE_EXT_RELATIVE_TIMESTAMPS;

                if (fStart)
                {
                    fStart = false;
                    u32SampleExtensions |= TSMM_SAMPLE_EXT_DISCONTINUITY;
                }

                sendOnSample(&sendContext, pPresentation->mjpeg.u8DVCChannelId, u32StreamIdMJPEG,
                             u32SampleExtensions,
                             pOutputFrame->pu8JPEG,
                             pOutputFrame->cbJPEG,
                             pOutputFrame->i64SampleStartTime - i64BeginTimeline,
                             pOutputFrame->i64SampleEndTime - i64BeginTimeline);

                VHSTATCALL(pCtx->pServer, SendFrameEnd, (pStream->pStatCtx, true));
            }
            else
            {
                VHSTATCALL(pCtx->pServer, SendFrameEnd, (pStream->pStatCtx, false));
            }
        }
    }

    return;
}

/** @thread OUTPUT */
void VideoChannelTSMF::VideoChannelSendFrames(VHCONTEXT *pCtx, VHSTREAMPARMS *pStream,
                                              RTLISTANCHOR *pListFramesToSend, int64_t i64NowTimeline)
{
    /* Check if the presentation is ready to use by OUTPUT thread. */
    uint32_t u32VideoStreamId = pStream->u32VideoStreamId;

    TSMFPRESENTATION *pPresentation = tsmfPresentationById(u32VideoStreamId);

    if (!pPresentation)
    {
        VIDEOLOG(("No presentation for id %d!!!\n", u32VideoStreamId));
        return;
    }

    if (ASMAtomicCmpXchgU32(&pPresentation->u32Status, TSMF_PS_SETUP, TSMF_PS_READY))
    {
        /* The OUTPUT thread can take control over the presentation. */

        /* Notify upper components that the channel has been created. */
        if (pPresentation->callbackData.VHStreamNotify)
        {
            pPresentation->callbackData.VHStreamNotify(pPresentation->callbackData.pvContext,
                                                       VRDE_IMAGE_NOTIFY_HANDLE_CREATE,
                                                       &u32VideoStreamId,
                                                       sizeof(u32VideoStreamId));
        }

        if (RTListIsEmpty(pListFramesToSend) && pStream->fDirect)
        {
            videoHandlerInsertLatestFrame(pStream, pListFramesToSend);
        }
    }

    /* Video handle calls this periodically to let the videoHandlerInsertLatestFrame work. */
    if (RTListIsEmpty(pListFramesToSend))
    {
        VRDPPktRelease(&pPresentation->pkt);
        return;
    }

    uint32_t u32Status = ASMAtomicReadU32(&pPresentation->u32Status);
    if (   u32Status != TSMF_PS_SETUP
        && u32Status != TSMF_PS_PLAYBACK)
    {
        /* No Presentation for the stream Id or the presentation is not yet initialized. */
        VRDPPktRelease(&pPresentation->pkt);
        VIDEOLOG(("No presentation for id %d!!!\n", u32VideoStreamId));
        return;
    }

    if (isUTTSCSyncMode())
    {
        vcSendFramesUTTSC(pCtx, pStream, pListFramesToSend, i64NowTimeline, pPresentation);
    }
    else
    {
        vcSendFramesMSTSC(pCtx, pStream, pListFramesToSend, i64NowTimeline, pPresentation);
    }

    VRDPPktRelease(&pPresentation->pkt);
    return;
}

/** @thread OUTPUT */
int VideoChannelTSMF::VideoChannelEnd(const VHSTREAMPARMS *pStream)
{
    /* Check if a ready presentation exists for this stream. */
    TSMFPRESENTATION *pPresentation = tsmfPresentationById(pStream->u32VideoStreamId);

    if (!pPresentation)
    {
        /* The presentation for the stream Id has not been started at all. */
        VIDEOLOG(("VideoChannelEnd: No presentation for id %d\n", pStream->u32VideoStreamId));
        return VINF_SUCCESS;
    }

    /* A new presentation with the same id might be created by video handler.
     * Make sure that this one can not be found by id anymore.
     */
    pPresentation->u32VideoStreamId = 0;

    uint32_t u32Status = ASMAtomicXchgU32(&pPresentation->u32Status, TSMF_PS_SHUTDOWN);

    shadowBufferCoverRemove(pPresentation->uScreenId, pPresentation->videoRectHandle);

    TSMFSENDCONTEXT sendContext;
    bool fOutputThread = true;
    initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

    if (   u32Status == TSMF_PS_PLAYBACK
        || u32Status == TSMF_PS_SETUP
        || u32Status == TSMF_PS_READY)
    {
        if (u32Status == TSMF_PS_PLAYBACK)
        {
            /* END_OF_STREAM is sent to the MJPEG DVC channel, because the message
             * belongs to "data streaming" like NOTIFY_PREROLL and ON_SAMPLE.
             */
            sendEndOfStream(&sendContext, pPresentation->mjpeg.u8DVCChannelId, pPresentation->mjpeg.u32StreamId);
        }

        sendShutdownPresentation(&sendContext, pPresentation->control.u8DVCChannelId);
    }
    else
    {
        /* This means that DVC channels must be closed. */
        if (pPresentation->mjpeg.fu8StreamFlags & TSMFSTREAM_F_DVC_EXISTS)
        {
            tsmfCloseChannel(pPresentation->mjpeg.u8DVCChannelId);
        }
        if (pPresentation->control.fu8StreamFlags & TSMFSTREAM_F_DVC_EXISTS)
        {
            tsmfCloseChannel(pPresentation->control.u8DVCChannelId);
        }
    }

    VRDPPktRelease(&pPresentation->pkt);
    return VINF_SUCCESS;
}

/** @thread OUTPUT */
void VideoChannelTSMF::VideoChannelVisibleRegion(const VHSTREAMPARMS *pStream,
                                                 uint32_t cRects,
                                                 RTRECT *paRects,
                                                 RTRECT *pRect)
{
    VIDEOLOG(("VideoChannelVisibleRegion: id %d, cRects %d, pRect %p\n",
              pStream->u32VideoStreamId, cRects, pRect));

    TSMFPRESENTATION *pPresentation = tsmfPresentationById(pStream->u32VideoStreamId);

    if (!pPresentation)
    {
        return;
    }

    /* Update the region for this presentation anyway. */
    uint32_t u32Status = ASMAtomicReadU32(&pPresentation->u32Status);

    VIDEOLOG(("GEOM: pRect %p, status %d\n", pRect, u32Status));

    if (pRect != NULL)
    {
        /* The window geometry has been changed. */
        RGNRECT rectOriginal;
        rectOriginal.x = pRect->xLeft;
        rectOriginal.y = pRect->yTop;
        rectOriginal.w = pRect->xRight - pRect->xLeft;
        rectOriginal.h = pRect->yBottom - pRect->yTop;

        VIDEOLOG(("GEOM: %d,%d %dx%d\n", rectOriginal.x, rectOriginal.y, rectOriginal.w, rectOriginal.h));

        pPresentation->rectShadowBuffer  = rectOriginal;

        VHCONTEXT *pCtx = Client()->Server()->VideoHandler();
        vhSourceComputeScaledSize(pCtx, &pPresentation->rectScaled, &rectOriginal, pPresentation->fDirect);

        Client()->TP()->DesktopMap()->Screen2Client (pPresentation->uScreenId,
                                                     &pPresentation->rectShadowBuffer,
                                                     &pPresentation->rectClient);
    }

    /* Make local copies of the presentation data. */
    /*uint32_t u32StreamIdMJPEG = pPresentation->mjpeg.u32StreamId; - unused */
    RGNRECT rectClient = pPresentation->rectClient;
    /*RGNRECT rectShadowBuffer = pPresentation->rectShadowBuffer; - unused */
    uint64_t u64VideoWindowId = pPresentation->u64VideoWindowId;
    /*uint64_t u64ParentWindowId = pPresentation->u64ParentWindowId; - unused */
    /*bool fDirect = pPresentation->fDirect; - unused */

    bool fOutputThread = true;

    TSMFSENDCONTEXT sendContext;
    initTSMFSendContext(&sendContext, fOutputThread, Client(), pPresentation);

    if (u32Status == TSMF_PS_PLAYBACK)
    {
        VIDEOLOG(("VideoChannelVisibleRegion: sending id %d, cRects %d, pRect %p\n",
                  pStream->u32VideoStreamId, cRects, pRect));

        if (cRects == 0)
        {
            RTRECT visibleRect;
            visibleRect.xLeft = 0;
            visibleRect.yTop = 0;
            visibleRect.xRight = rectClient.w - 1;
            visibleRect.yBottom = rectClient.h - 1;
            sendUpdateGeometryInfo(&sendContext, pPresentation->control.u8DVCChannelId, u64VideoWindowId, &rectClient, 1, &visibleRect);
        }
        else
        {
            sendUpdateGeometryInfo(&sendContext, pPresentation->control.u8DVCChannelId, u64VideoWindowId, &rectClient, cRects, paRects);
        }

        /* For direct streams resend the latest frame. */
        if (pStream->fDirect)
        {
            RTLISTANCHOR list;
            RTListInit(&list);
            videoHandlerInsertLatestFrame(pStream, &list);

            VHOUTPUTFRAME *pOutputFrame;
            VHOUTPUTFRAME *pNext;
            RTListForEachSafe(&list, pOutputFrame, pNext, VHOUTPUTFRAME, NodeOutputFrame)
            {
                uint32_t u32SampleExtensions = TSMM_SAMPLE_EXT_CLEANPOINT | TSMM_SAMPLE_EXT_RELATIVE_TIMESTAMPS
                                               | TSMM_SAMPLE_EXT_HAS_NO_TIMESTAMPS | TSMM_SAMPLE_EXT_DISCONTINUITY;

                sendOnSample(&sendContext, pPresentation->mjpeg.u8DVCChannelId, pPresentation->mjpeg.u32StreamId,
                             u32SampleExtensions,
                             pOutputFrame->pu8JPEG,
                             pOutputFrame->cbJPEG,
                             0,
                             0);

                VRDPPktRelease(&pOutputFrame->pkt);
            }
        }
    }

    VRDPPktRelease(&pPresentation->pkt);
}


/*
 * Base video channel methods.
 */
int VideoChannel::Event(uint32_t u32Event, void *pvData, size_t cbData)
{
    NOREF(pvData);
    NOREF(cbData);

    int rc = VINF_SUCCESS;

    if (m_pfnEvent)
    {
        if (u32Event == VIDEO_CHANNEL_SUPPORTED)
        {
            VCESupported data;
            data.pVideoChannel = this;
            rc = m_pfnEvent(m_pvEvent, u32Event, &data, sizeof (VCESupported));
        }
        else if (u32Event == VIDEO_CHANNEL_ACTIVE)
        {
            VCEActive data;
            data.pVideoChannel = this;
            rc = m_pfnEvent(m_pvEvent, u32Event, &data, sizeof (VCEActive));
        }
        else
        {
           rc = VERR_NOT_SUPPORTED;
        }
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}


/*
 * VRDPClient video channel wrappers.
 */

/** @thread OUTPUT */
int VRDPClient::VideoStreamStart (const VHSTREAMPARMS *pStream)
{
    // VIDEOLOG(("VRDPClient::VideoStreamStart\n"));

    return m_pVideoChannel->VideoChannelStart(pStream);
}

/** @thread OUTPUT */
int VRDPClient::VideoStreamEnd (const VHSTREAMPARMS *pStream)
{
    VIDEOLOG(("VRDPClient::VideoStreamEnd\n"));

    return m_pVideoChannel->VideoChannelEnd(pStream);
}

/** @thread OUTPUT */
void VRDPClient::VideoStreamVisibleRegion(const VHSTREAMPARMS *pStream, uint32_t cRects, RTRECT *paRects, RTRECT *pRect)
{
    VIDEOLOG(("VRDPClient::VideoStreamVisibleRegion\n"));

    m_pVideoChannel->VideoChannelVisibleRegion(pStream, cRects, paRects, pRect);
}


/** @thread OUTPUT */
void VRDPClient::VideoStreamSendFrames (VHCONTEXT *pCtx, VHSTREAMPARMS *pOutputStream, RTLISTANCHOR *pListFramesToSend, int64_t i64NowTimeline)
{
    return m_pVideoChannel->VideoChannelSendFrames(pCtx, pOutputStream, pListFramesToSend, i64NowTimeline);
}

/* Channel creation, an incoming packet or channel close.
 * @thread INPUT
 */
/* static */ void VRDPClient::sunFlsh(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    VRDPClient *pThis = (VRDPClient *)pvCtx;

    pThis->m_pVideoChannelSunFlsh->VideoChannelInput(u32Event, pvData, cbData);
}

/* static */ int VRDPClient::videoChannelEvent(void *pvCtx, uint32_t u32Event, void *pvData, size_t cbData)
{
    RT_NOREF1(cbData);
    int rc = VINF_SUCCESS;

    VRDPClient *pThis = (VRDPClient *)pvCtx;

    if (u32Event == VIDEO_CHANNEL_SUPPORTED)
    {
        VCESupported *pData = (VCESupported *)pvData;

        const char *pszName = pData->pVideoChannel->VideoChannelName ();

        VIDEOLOG(("VIDEO_CHANNEL_SUPPORTED %s\n", pszName));

        if (RTStrCmp(pszName, "TSMF") == 0)
        {
            pThis->m_pVideoChannel = pThis->m_pVideoChannelTSMF;
        }
        else if (RTStrCmp(pszName, "SUNFLSH") == 0)
        {
            pThis->m_pVideoChannel = pThis->m_pVideoChannelSunFlsh;
        }
        else
        {
            pThis->m_pVideoChannel = pThis->m_pVideoChannelNull;
        }

        if (RTStrCmp(pszName, pThis->m_achLastVideoChannelName) != 0)
        {
            RTStrCopy(pThis->m_achLastVideoChannelName, sizeof (pThis->m_achLastVideoChannelName), pszName);

            static int scLogged = 0;
            if (scLogged < 16)
            {
                scLogged++;
                VRDPLOGREL(("Supported video redirection channel [%s]\n", pszName));
            }
        }
    }
    else if (u32Event == VIDEO_CHANNEL_ACTIVE)
    {
        VCEActive *pData = (VCEActive *)pvData;
        const char *pszName = pData->pVideoChannel->VideoChannelName ();
        VIDEOLOG(("VIDEO_CHANNEL_ACTIVE %s\n", pszName));
        NOREF(pszName);
        pThis->m_fVideoChannelActive = true;
    }
    else
    {
       rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

#ifdef DEBUG_sunlover
static uint64_t gu64TotalDuration;
static uint64_t gu64StartAudio;
#endif /* DEBUG_sunlover */

/** @thread OUTPUT */
void VRDPClient::OnAudioClose (void)
{
    VIDEOLOG(("VRDPClient::OnAudioClose\n"));
#ifdef DEBUG_sunlover
    gu64TotalDuration = 0;
#endif /* DEBUG_sunlover */
    m_pVideoChannel->OnAudioClose();
}

/** @thread OUTPUT */
void VRDPClient::OnAudioSend (uint64_t u64PacketStartNanoTS, uint64_t u64PacketEndNanoTS, uint64_t u64DurationNano)
{
    VIDEOLOG(("VRDPClient::OnAudioSend: %lluns-%lluns duration %lluns\n", u64PacketStartNanoTS, u64PacketEndNanoTS, u64DurationNano));

#ifdef DEBUG_sunlover
    if (gu64TotalDuration == 0)
    {
       gu64StartAudio = VRDPTimeNanoTS();
    }

    VIDEOLOG(("VRDPClient::OnAudioSend: audio send delay %llums\n", (VRDPTimeNanoTS() - u64PacketStartNanoTS) / 1000000));

    uint64_t u64ClientStartNanoTS = gu64StartAudio + gu64TotalDuration;
    VIDEOLOG(("VRDPClient::OnAudioSend: audio playback delay %llu\n", (u64ClientStartNanoTS - u64PacketStartNanoTS) / 1000000));
    VIDEOLOG(("VRDPClient::OnAudioSend: total duration %llu\n", gu64TotalDuration / 1000000));

    /// @todo May be use cSamples and iFreq instead of u64DurationNano.
    // Also investigate how audio time is related to the system time.

    gu64TotalDuration += u64DurationNano;
#endif /* DEBUG_sunlover */

    m_pVideoChannel->OnAudioSend(u64PacketStartNanoTS, u64PacketEndNanoTS, u64DurationNano);
}
