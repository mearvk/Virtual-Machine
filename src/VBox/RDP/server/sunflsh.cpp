/* $Id: sunflsh.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol - "SunFlsh" virtual channel.
 */

/*
 * Copyright (C) 2010-2025 Oracle and/or its affiliates.
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

#include "vrdpserv.h"
#include "utils.h"
#include "videostream.h"

#include <limits.h>

#ifdef DEBUG_sunlover
#undef SUNFLSHLOG
#define SUNFLSHLOG LogRel
#endif


typedef struct SUNFLSHPRESENTATION
{
    uint32_t u32Index;          /* 0 based index of the presentation. */
    uint32_t u32VideoStreamId;  /* The VRDP video handler stream id. */
    unsigned uScreenId;
    uint16_t id;                /* Identifier of the window. */
    RGNRECT rectShadowBuffer;   /* Rectangle in the shadow buffer coordinates. */
    RGNRECT rectClient;         /* Rectangle in the client coordinates. */
    RGNRECT  rectScaled;        /* Size of JPEG compressed image. */
    int64_t  i64BeginTimeline;   /* Sample times are relative to this. */
    bool     fScaling;
    SBHANDLE videoRectHandle;
} SUNFLSHPRESENTATION;

class VideoChannelSunFlsh: public VideoChannel
{
    private:
        bool m_fVideoRedirectionSupported;

        SUNFLSHPRESENTATION m_aPresentations[4];
        uint16_t m_u16IdSource;

        RTLISTANCHOR m_ListKeepFrames;
        int64_t m_i64LastSentTimeline;

        SUNFLSHPRESENTATION *presentationById(uint32_t u32VideoStreamId);
        SUNFLSHPRESENTATION *presentationAlloc(void);
        uint32_t presentationGetSquare(void);
        void presentationFree(SUNFLSHPRESENTATION *pPresentation);
        uint16_t presentationCreateId(void);
        void videoChannelReleaseFrames(uint32_t u32VideoStreamlId);
        void sendFrame(VHOUTPUTFRAME *pFrameToSend,
                       int64_t i64NowTimeline,
                       const RGNRECT *pRectClient,
                       const RGNRECT *pRectScaled,
                       bool fScaling,
                       uint16_t u16Id);

    public:
        VideoChannelSunFlsh(VRDPClient *pClient, PFNVIDEOCHANNELEVENT pfnEvent, void *pvEvent);
        virtual ~VideoChannelSunFlsh();

        virtual const char *VideoChannelName(void) { return "SUNFLSH"; };
        virtual int VideoChannelSetup(void);

        virtual int VideoChannelStart (const VHSTREAMPARMS *pStream);
        virtual int VideoChannelEnd (const VHSTREAMPARMS *pStream);
        virtual void VideoChannelSendFrames (VHCONTEXT *pCtx,
                                             VHSTREAMPARMS *pStream,
                                             RTLISTANCHOR *pListFramesToSend,
                                             int64_t i64NowTimeline);
        virtual void VideoChannelInput (uint32_t u32Event,
                                        const void *pvData,
                                        uint32_t cbData);
        virtual void VideoChannelVisibleRegion(const VHSTREAMPARMS *pStream,
                                               uint32_t cRects,
                                               RTRECT *paRects,
                                               RTRECT *pRect);
};


int VideoChannelSunFlshCreate(VideoChannel **ppVideoChannel,
                              VRDPClient *pClient,
                              PFNVIDEOCHANNELEVENT pfnEvent,
                              void *pvEvent)
{
    *ppVideoChannel = new VideoChannelSunFlsh(pClient, pfnEvent, pvEvent);
    return *ppVideoChannel? VINF_SUCCESS: VERR_NO_MEMORY;
}


typedef uint8_t *PBYTE;
typedef uint8_t BYTE;
typedef int16_t *PSHORT;
typedef int16_t SHORT;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;

enum VChannelDataType
{
        FLASH_VIDEO = 1,
        FLASH_RESIZE_WINDOW = 2,
        FLASH_WINDOW_COORDS = 3,
        FLASH_AUDIO_PARAM = 4,
        FLASH_CREATE_WINDOW = 5,
        FLASH_DESTROY_WINDOW = 6,
        FLASH_SHOW_WINDOW = 7,
        FLASH_HIDE_WINDOW = 8,
        FLASH_PING = 9,
        FLASH_PONG = 10,
        FLASH_VIDEO_REPLY = 11,
        FLASH_MOVE_WINDOW = 12,
        FLASH_UPDATE_CLIPLIST = 13,
        FLASH_INIT = 14,
        FLASH_INIT_REPLY = 15,
        FLASH_CARD_OUT = 16,
        FLASH_CARD_IN = 17,
        FLASH_VIDEO_SCALED = 18,
        FLASH_ENABLE_PACKET_LOSS_HANDLING = 19
};

#pragma pack(1)

struct VChannelData
{
        BYTE type;
        DWORD length;
};
AssertCompileSize(VChannelData, 5);

struct VCWindowData : public VChannelData
{
        WORD id;
};

struct VCImageData
{
        SHORT left;
        SHORT top;
        DWORD length;
        BYTE  data[1];
};

struct VCVideoData : public VCWindowData
{
        DWORD timestamp;
        SHORT nRects;
};

struct VCVideoDataScaled : public VCWindowData
{
        DWORD timestamp;
        SHORT srcW;
        SHORT srcH;
        SHORT nRects;
};

struct VCVideoReply : public VChannelData
{
        DWORD timestamp;
        DWORD tProcessing;
};

struct VCWindowInitData : public VCWindowData
{
        SHORT left;
        SHORT top;
        WORD width;
        WORD height;
};


struct VCWindowPosData : public VCWindowData
{
        SHORT left;
        SHORT top;
};

struct VCWindowClipData : public VCWindowData
{
        SHORT left;
        SHORT top;
        SHORT nRect;
};

struct VCInitData : public VChannelData
{
        WORD major;
        WORD minor;
};
AssertCompileSize(VCInitData, 5 + 4);

struct VCInitReplyData : public VCInitData
{
        WORD error;
        DWORD capabilities;
};

struct VCEnablePacketLossData : public VCWindowData
{
        BYTE enabled;
};

#define VERSION_MAJOR 1
#define VERSION_MINOR 0

#define HAS_JPEG_CAPABILITIES(i) (i & 0x00000001)
#define HAS_SCALING_CAPABILITIES(i) (i & 0x00000002)


/* VBox specific structures. */
struct VBoxFrameData
{
    VCVideoData video;
    VCImageData image;
};

struct VBoxFrameDataScaled
{
    VCVideoDataScaled video;
    VCImageData image;
};

#pragma pack()

DECLINLINE(void) EnsureLittleEndian(SHORT& t, SHORT s) { t = s; }
DECLINLINE(void) EnsureLittleEndian(WORD& t, WORD s) { t = s; }
DECLINLINE(void) EnsureLittleEndian(DWORD& t, DWORD l) { t = l; }

VRDPChannelSunFlsh::VRDPChannelSunFlsh (VRDPTP *pvrdptp)
    :
    VRDPChannel (),
    m_pvrdptp (pvrdptp)
{
    initMembers ();
}

void VRDPChannelSunFlsh::initMembers (void)
{
    m_channelOptions = 0;
    m_channelId = 0;

    m_fOperational = false;
    m_fSetup = false;

    m_pfnSunFlshCallback = NULL;
    m_pvSunFlshCallback = NULL;
}

VRDPChannelSunFlsh::~VRDPChannelSunFlsh ()
{
    Close ();
}

int VRDPChannelSunFlsh::Open (uint32_t options, uint16_t id)
{
    SUNFLSHLOG(("SunFlsh: Open id = %d\n", id));

    VRDPServer *pServer = m_pvrdptp->Client()->Server();

    uint32_t u32IsSunFlshSupported = 0;
    appProperty (pServer->ApplicationCallbacks (), pServer->ApplicationCallbackPointer (),
                 VRDE_QP_VIDEO_CHANNEL_SUNFLSH,
                 &u32IsSunFlshSupported, sizeof (u32IsSunFlshSupported), NULL);

    SUNFLSHLOG(("u32IsSunFlshSupported %d\n", u32IsSunFlshSupported));

    if (u32IsSunFlshSupported == 0)
    {
        return VERR_NOT_SUPPORTED;
    }

    m_channelOptions = options;
    m_channelId = id;

    InitOutgoingChannelId (m_channelId);

    return VINF_SUCCESS;
}

void VRDPChannelSunFlsh::Setup (void)
{
    SUNFLSHLOG(("SunFlsh: Setup\n"));

    if (m_channelId != 0)
    {
        m_fSetup = true;
    }

    return;
}

void VRDPChannelSunFlsh::Close (void)
{
    initMembers ();
}

void VRDPChannelSunFlsh::Start (void)
{
    SUNFLSHLOG(("SunFlsh: Start\n"));

    if (!m_fSetup)
    {
        /* If the channel is not initialized. */
        SUNFLSHLOG(("channel not supported, negotiation cancelled!!!\n"));

        static int scLogged = 0;
        if (scLogged < 16)
        {
            scLogged++;

            VRDPLOGREL(("SunFlsh disabled.\n"));
        }

        return;
    }

    if (m_fOperational)
    {
        SUNFLSHLOG(("channel already intialized, negotiation cancelled!!!\n"));
        return;
    }

    m_fOperational = true;

    if (m_pfnSunFlshCallback)
    {
        m_pfnSunFlshCallback(m_pvSunFlshCallback, SUNFLSH_EVENT_CREATE, NULL, 0);
    }

    VCInitData data;
    data.type = FLASH_INIT;
    EnsureLittleEndian(data.length, sizeof(VCInitData) - sizeof(VChannelData));
    EnsureLittleEndian(data.major, VERSION_MAJOR);
    EnsureLittleEndian(data.minor, VERSION_MINOR);

    VRDPLOGREL(("SUNFLSH: init V%d.%d\n",
                data.major, data.minor));

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = &data;
    aBuffers[0].cb = sizeof (data);

    m_pvrdptp->SendToChannel (this /* as OutputCtx */, m_channelId, m_channelOptions,
                              RT_ELEMENTS(aBuffers), aBuffers, false);

    return;
}

int VRDPChannelSunFlsh::RegisterCallback (PFNDVCCALLBACK pfnSunFlshCallback, void *pvSunFlshCallback)
{
    SUNFLSHLOG(("SunFlsh: RegisterCallback\n"));

    m_pfnSunFlshCallback = pfnSunFlshCallback;
    m_pvSunFlshCallback = pvSunFlshCallback;

    return VINF_SUCCESS;
}

/** @thread OUTPUT */
int VRDPChannelSunFlsh::SendData(VRDPBUFFER *paBuffers, int cBuffers)
{
    int rc = VINF_SUCCESS;

    SUNFLSHLOG(("SunFlsh: SendData %d, paBuffers %p, cBuffers = %d\n", m_fOperational, paBuffers, cBuffers));
#ifdef DEBUG_sunlover
    int iBuffer;
    for (iBuffer = 0; iBuffer < cBuffers; iBuffer++)
    {
        SUNFLSHLOG(("SunFlsh:     [%d] pv %p, cb %d\n", iBuffer, paBuffers[iBuffer].pv, paBuffers[iBuffer].cb));
        // hexdump((uint8_t *)paBuffers[iBuffer].pv, paBuffers[iBuffer].cb);
    }
#endif

    if (!m_fOperational)
    {
        return rc;
    }

    rc = sendBuffers(cBuffers, paBuffers);

    return rc;
}

/** @thread OUTPUT */
int VRDPChannelSunFlsh::sendBuffers (int cBuffers, const VRDPBUFFER *paBuffers)
{
    /* A Reset is not necessary because the contexts automatically resets after sending a packet. */
    Assert (IsReset ());
    Assert (m_fOperational);
    Assert (m_pvrdptp->Client ()->Server ()->IsOutputThread ());

    return m_pvrdptp->SendToChannel (this /* as OutputCtx */, m_channelId, m_channelOptions,
                                     cBuffers, paBuffers, false);
}

/** @thread OUTPUT */
int VRDPChannelSunFlsh::ProcessOutput (const void *pvData, uint32_t cbData)
{
    RT_NOREF2(pvData, cbData);
    return VINF_SUCCESS;
}


/** @thread INPUT */
int VRDPChannelSunFlsh::ProcessChannelInput (const uint8_t *pu8Input, uint32_t u32Length)
{
    int rc = VINF_SUCCESS;

    if (m_pfnSunFlshCallback)
    {
        m_pfnSunFlshCallback(m_pvSunFlshCallback, SUNFLSH_EVENT_DATA, pu8Input, u32Length);
    }

    SUNFLSHLOG(("ProcessChannelInput: result %Rrc\n", rc));
    return rc;
}


/*
 * Video Channel.
 */

typedef struct OUTPUTFRAMEKEEP
{
    RTLISTNODE NodeOutputFrameKeep;
    VHOUTPUTFRAME *pOutputFrame;
    bool fObsolete;
    uint32_t u32VideoStreamId;
} OUTPUTFRAMEKEEP;


/* Return current timestamp in mcs */
DECLINLINE(uint32_t) sunFlshTimestampGet(void)
{
    return (uint32_t)(VRDPTimeNanoTS() / 1000);
}

VideoChannelSunFlsh::VideoChannelSunFlsh(VRDPClient *pClient, PFNVIDEOCHANNELEVENT pfnEvent, void *pvEvent)
    :
    VideoChannel(pClient, pfnEvent, pvEvent)
{
    m_u16IdSource = 0;
    m_fVideoRedirectionSupported = false;
    memset(m_aPresentations, 0, sizeof(m_aPresentations));
    RTListInit(&m_ListKeepFrames);
    m_i64LastSentTimeline = 0;
}

VideoChannelSunFlsh::~VideoChannelSunFlsh()
{
    videoChannelReleaseFrames(0);
}

void VideoChannelSunFlsh::videoChannelReleaseFrames(uint32_t u32VideoStreamId)
{
    /*
     * Release frames.
     */
    OUTPUTFRAMEKEEP *pFrameKeep = RTListGetFirst(&m_ListKeepFrames,
                                                 OUTPUTFRAMEKEEP,
                                                 NodeOutputFrameKeep);

    while (pFrameKeep)
    {
        OUTPUTFRAMEKEEP *pNext = RTListNodeIsLast(&m_ListKeepFrames, &pFrameKeep->NodeOutputFrameKeep)?
                                     NULL:
                                     RTListNodeGetNext(&pFrameKeep->NodeOutputFrameKeep,
                                                       OUTPUTFRAMEKEEP, NodeOutputFrameKeep);

        if (u32VideoStreamId == 0 || u32VideoStreamId == pFrameKeep->u32VideoStreamId)
        {
            VRDPPktRelease(&pFrameKeep->pOutputFrame->pkt);
            RTListNodeRemove(&pFrameKeep->NodeOutputFrameKeep);
            VRDPMemFree(pFrameKeep);
        }

        pFrameKeep = pNext;
    }
}

int VideoChannelSunFlsh::VideoChannelSetup(void)
{
    videoChannelReleaseFrames(0);
    m_fVideoRedirectionSupported = false;
    memset(m_aPresentations, 0, sizeof(m_aPresentations));
    RTListInit(&m_ListKeepFrames);
    m_i64LastSentTimeline = 0;
    return VINF_SUCCESS;
}

SUNFLSHPRESENTATION *VideoChannelSunFlsh::presentationById(uint32_t u32VideoStreamId)
{
    SUNFLSHPRESENTATION *pPresentation = NULL;

    unsigned i;
    for (i = 0; i < RT_ELEMENTS(m_aPresentations); i++)
    {
        if (m_aPresentations[i].u32VideoStreamId == u32VideoStreamId)
        {
            pPresentation = &m_aPresentations[i];
            break;
        }
    }

    return pPresentation;
}

SUNFLSHPRESENTATION *VideoChannelSunFlsh::presentationAlloc(void)
{
    SUNFLSHPRESENTATION *pPresentation = NULL;

    unsigned i;
    for (i = 0; i < RT_ELEMENTS(m_aPresentations); i++)
    {
        if (m_aPresentations[i].id == 0)
        {
            pPresentation = &m_aPresentations[i];
            memset(pPresentation, 0, sizeof (SUNFLSHPRESENTATION));
            pPresentation->u32Index = i;
            pPresentation->videoRectHandle = SB_HANDLE_NULL;
            break;
        }
    }

    return pPresentation;
}

uint32_t VideoChannelSunFlsh::presentationGetSquare(void)
{
    uint32_t u32Square = 0;

    unsigned i;
    for (i = 0; i < RT_ELEMENTS(m_aPresentations); i++)
    {
        if (m_aPresentations[i].id != 0)
        {
            u32Square += m_aPresentations[i].rectScaled.w * m_aPresentations[i].rectScaled.h;
        }
    }

    return u32Square;
}

void VideoChannelSunFlsh::presentationFree(SUNFLSHPRESENTATION *pPresentation)
{
    memset(pPresentation, 0, sizeof (SUNFLSHPRESENTATION));
}

uint16_t VideoChannelSunFlsh::presentationCreateId(void)
{
    uint16_t id = 0;

    for (;;)
    {
        m_u16IdSource++;
        if (m_u16IdSource == 0)
        {
            m_u16IdSource++;
        }

        bool fDuplicate = false;

        unsigned i;
        for (i = 0; i < RT_ELEMENTS(m_aPresentations); i++)
        {
            if (m_aPresentations[i].id == m_u16IdSource)
            {
                fDuplicate = true;
                break;
            }
        }

        if (!fDuplicate)
        {
            id = m_u16IdSource;
            break;
        }
    }

    return id;
}

int VideoChannelSunFlsh::VideoChannelStart (const VHSTREAMPARMS *pStream)
{
    SUNFLSHLOG(("VideoChannelSunFlsh::VideoChannelStart\n"));

    /* Do not support scaling on SunFlsh. */
    bool fScaling =   (pStream->rectScaled.w != pStream->rectOriginal.w)
                   || (pStream->rectScaled.h != pStream->rectOriginal.h);

    if (fScaling)
    {
        SUNFLSHLOG(("VRDP server does not support scaling with SunFlsh (%dx%d -> %dx%d)!!!\n",
                     pStream->rectOriginal.w, pStream->rectOriginal.h, pStream->rectScaled.w, pStream->rectScaled.h));
        return VERR_NOT_SUPPORTED;
    }

    /* Check if there is already presentation with for the stream. */
    SUNFLSHPRESENTATION *pPresentation = presentationById(pStream->u32VideoStreamId);

    if (pPresentation)
    {
        return VINF_ALREADY_INITIALIZED;
    }

    /* Check if the total flash size does not exceed the 1024*768 limit. */
    uint32_t u32Square = presentationGetSquare();

    if (u32Square + pStream->rectScaled.w * pStream->rectScaled.h > 1024 * 768)
    {
        SUNFLSHLOG(("Total square would exceed limit: current %d, + new %d!!!\n",
                     u32Square, pStream->rectScaled.w * pStream->rectScaled.h));
        return VERR_NOT_SUPPORTED;
    }

    /* Allocate a presentation state for this stream on this client. */
    pPresentation = presentationAlloc();

    if (!pPresentation)
    {
        SUNFLSHLOG(("Failed to allocate a presentation!!!\n"));
        return VERR_NO_MEMORY;
    }

    /* Initialize the presentation.
     */
    SUNFLSHLOG(("VideoChannelSunFlsh::VideoChannelStart presentation created streamid %d\n",
                pStream->u32VideoStreamId));

    pPresentation->uScreenId  = pStream->uScreenId;
    pPresentation->u32VideoStreamId  = pStream->u32VideoStreamId;
    pPresentation->id = presentationCreateId();

    /* Presentation rect is in the client coordinates, stream - in shadow buffer. */
    pPresentation->rectShadowBuffer  = pStream->rectOriginal;
    pPresentation->rectScaled        = pStream->rectScaled;
    Client()->TP()->DesktopMap()->Screen2Client (pPresentation->uScreenId,
                                                 &pPresentation->rectShadowBuffer,
                                                 &pPresentation->rectClient);
    pPresentation->fScaling =    (pPresentation->rectScaled.w != pPresentation->rectClient.w)
                              || (pPresentation->rectScaled.h != pPresentation->rectClient.h);

    WORD id = pPresentation->id;
    RGNRECT rectClient = pPresentation->rectClient;
    RGNRECT rectShadowBuffer = pPresentation->rectShadowBuffer;

    VCWindowInitData data;

    data.type = FLASH_CREATE_WINDOW;
    EnsureLittleEndian(data.length, sizeof(VCWindowInitData) - sizeof(VChannelData));
    EnsureLittleEndian(data.id, id);
    EnsureLittleEndian(data.left, (SHORT) rectClient.x);
    EnsureLittleEndian(data.top, (SHORT) rectClient.y);
    EnsureLittleEndian(data.width, (WORD) rectClient.w);
    EnsureLittleEndian(data.height, (WORD) rectClient.h);

    SUNFLSHLOG(("SunFlsh: FLASH_CREATE_WINDOW %d %d,%d %dx%d\n", data.id, data.left, data.top, data.width, data.height));

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = &data;
    aBuffers[0].cb = sizeof (VCWindowInitData);

    Client()->TP()->SunFlsh()->SendData(&aBuffers[0], RT_ELEMENTS(aBuffers));

#if 0 /* Not needed. */
    DWORD dwSize = sizeof(VCWindowClipData) + 4 * sizeof(SHORT);
    VCWindowClipData *pData = (VCWindowClipData *)VRDPMemAlloc(dwSize);

    pData->type = FLASH_UPDATE_CLIPLIST;
    EnsureLittleEndian(pData->length, dwSize - sizeof(VChannelData));
    EnsureLittleEndian(pData->id, id);
    EnsureLittleEndian(pData->left, (SHORT) rectClient.x);
    EnsureLittleEndian(pData->top, (SHORT) rectClient.y);
    EnsureLittleEndian(pData->nRect, (SHORT) 1);

    PSHORT pVCData = (PSHORT) ((PBYTE) pData + sizeof(VCWindowClipData));
    EnsureLittleEndian(*pVCData++, (SHORT) 0);
    EnsureLittleEndian(*pVCData++, (SHORT) 0);
    EnsureLittleEndian(*pVCData++, (SHORT) rectClient.w - 1);
    EnsureLittleEndian(*pVCData++, (SHORT) rectClient.h - 1);

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = pData;
    aBuffers[0].cb = dwSize;

    Client()->TP()->SunFlsh()->SendData(&aBuffers[0], RT_ELEMENTS(aBuffers));
    VRDPMemFree(pData);
#endif

    pPresentation->videoRectHandle = shadowBufferCoverAdd(pPresentation->uScreenId, &rectShadowBuffer);

    return VINF_SUCCESS;
}

/** @thread OUTPUT */
int VideoChannelSunFlsh::VideoChannelEnd (const VHSTREAMPARMS *pStream)
{
    /*VHCONTEXT *pCtx = pStream->pCtx; - unused */

    if (pStream->u32VideoStreamId != 0)
    {
        /* If any frames are kept for the stream, remove them. */
        videoChannelReleaseFrames(pStream->u32VideoStreamId);
    }
    else
    {
#ifdef DEBUG_sunlover
        /* This method should not receive the 0 stream id. */
        AssertFailed();
#endif
        return VERR_NOT_SUPPORTED;
    }

    /* Check if a ready presentation exists for this stream. */
    SUNFLSHPRESENTATION *pPresentation = presentationById(pStream->u32VideoStreamId);

    if (!pPresentation)
    {
        /* The presentation for the stream Id has not been started at all. */
        SUNFLSHLOG(("VideoChannelSunFlsh::VideoChannelEnd no presentation for streamid %d!!!\n",
                    pStream->u32VideoStreamId));
        return VINF_SUCCESS;
    }

    WORD id = pPresentation->id;
    SBHANDLE videoRectHandle = pPresentation->videoRectHandle;
    int uScreenId = pPresentation->uScreenId;

    SUNFLSHLOG(("VideoChannelSunFlsh::VideoChannelEnd presentation deleted streamid %d\n",
                pPresentation->u32VideoStreamId));

    presentationFree(pPresentation);

    shadowBufferCoverRemove(uScreenId, videoRectHandle);

    VCWindowData data;
    data.type = FLASH_DESTROY_WINDOW;
    EnsureLittleEndian(data.length, sizeof(VCWindowData) - sizeof(VChannelData));
    EnsureLittleEndian(data.id, id);

    SUNFLSHLOG(("SunFlsh: FLASH_DESTROY_WINDOW %d\n", data.id));

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = &data;
    aBuffers[0].cb = sizeof (VCWindowData);

    Client()->TP()->SunFlsh()->SendData(&aBuffers[0], RT_ELEMENTS(aBuffers));

    return VINF_SUCCESS;
}

void VideoChannelSunFlsh::sendFrame(VHOUTPUTFRAME *pFrameToSend,
                                    int64_t i64NowTimeline,
                                    const RGNRECT *pRectClient,
                                    const RGNRECT *pRectScaled,
                                    bool fScaling,
                                    uint16_t u16Id)
{
    NOREF(pRectScaled);

    /*
     * Send the frame.
     * @todo variable framerate, m_i64LastSentTimeline => presentation.
     */
    if ((i64NowTimeline - m_i64LastSentTimeline) > VH_TIMELINE_TIME_FROM_MS(0))
    {
        m_i64LastSentTimeline = i64NowTimeline;

        SUNFLSHLOG(("SUNFLSH: sending frame diff %lld\n", pFrameToSend->i64SampleStartTime - i64NowTimeline));

        /* Preallocate enough memory for any variant. */
        uint8_t au8Data[RT_MAX(sizeof (VBoxFrameData), sizeof (VBoxFrameDataScaled))];

        DWORD dwPacketSize;
        VCImageData *pImageData;

        /*
         * The timestamp field is not used by the client.
         * The server stores current time and can compute the frame processing delay.
         */
        DWORD timestampMCS = sunFlshTimestampGet();

        if (fScaling)
        {
            VBoxFrameDataScaled *pData = (VBoxFrameDataScaled *)&au8Data[0];
            dwPacketSize = (uintptr_t)&pData->image.data - (uintptr_t)pData; // RT_OFFSETOF(VBoxFrameDataScaled, image.data);
            pImageData = &pData->image;

            pData->video.type = FLASH_VIDEO_SCALED;
            EnsureLittleEndian(pData->video.length, dwPacketSize + pFrameToSend->cbJPEG - sizeof(VChannelData));
            EnsureLittleEndian(pData->video.id, u16Id);

            EnsureLittleEndian(pData->video.timestamp, timestampMCS);
            pData->video.srcW = pRectClient->w;
            pData->video.srcH = pRectClient->h;
            EnsureLittleEndian(pData->video.nRects, 1);
        }
        else
        {
            VBoxFrameData *pData = (VBoxFrameData *)&au8Data[0];
            dwPacketSize = (uintptr_t)&pData->image.data - (uintptr_t)pData; // RT_OFFSETOF(VBoxFrameData, image.data);
            pImageData = &pData->image;

            pData->video.type = FLASH_VIDEO;
            EnsureLittleEndian(pData->video.length, dwPacketSize + pFrameToSend->cbJPEG - sizeof(VChannelData));
            EnsureLittleEndian(pData->video.id, u16Id);

            EnsureLittleEndian(pData->video.timestamp, timestampMCS);
            EnsureLittleEndian(pData->video.nRects, 1);
        }

        EnsureLittleEndian(pImageData->left, 0);
        EnsureLittleEndian(pImageData->top, 0);
        EnsureLittleEndian(pImageData->length, pFrameToSend->cbJPEG);

        SUNFLSHLOG(("SunFlsh: %s %d (%dx%d, original %dx%d) @%d,%d %d bytes timestamp %u\n",
                     fScaling? "FLASH_VIDEO_SCALED": "FLASH_VIDEO",
                     u16Id, pRectScaled->w, pRectScaled->h, pRectClient->w, pRectClient->h,
                     pImageData->left, pImageData->top, pImageData->length,
                     timestampMCS));

        VRDPBUFFER aBuffers[2];

        aBuffers[0].pv = &au8Data[0];
        aBuffers[0].cb = dwPacketSize;

        aBuffers[1].pv = pFrameToSend->pu8JPEG;
        aBuffers[1].cb = pFrameToSend->cbJPEG;

        Client()->TP()->SunFlsh()->SendData(&aBuffers[0], RT_ELEMENTS(aBuffers));
    }
}

/*
 * Frames are displayed on the client immediately and timestamp is ignored.
 * VideoChannelSendFrames gets a list of frames, which have to be displayed later.
 * So frames must be queued here on the server.
 *
 * Each output frame has a reference counter. VideoChannelSendFrames references
 * them and saves pointers to them. On each invocation, these saved frames are
 * checked and are sent if their timestamp is about the same as i64NowTimeline.
 */
void VideoChannelSunFlsh::VideoChannelSendFrames (VHCONTEXT *pCtx,
                                                  VHSTREAMPARMS *pStream,
                                                  RTLISTANCHOR *pListFramesToSend,
                                                  int64_t i64NowTimeline)
{
    RT_NOREF1(pCtx);
    uint32_t u32VideoStreamId = pStream->u32VideoStreamId;

    SUNFLSHPRESENTATION *pPresentation = presentationById(u32VideoStreamId);

    if (!pPresentation)
    {
        /* No Presentation for the stream Id or the presentation is not yet initialized. */
        return;
    }

    /* Make local copies of the presentation data. */
    WORD id = pPresentation->id;
    RGNRECT rectScaled = pPresentation->rectScaled;
    RGNRECT rectClient = pPresentation->rectClient;
    bool scaling = pPresentation->fScaling;

    int64_t i64BeginTimeline = pPresentation->i64BeginTimeline;
    bool fStart = false;
    if (RT_UNLIKELY(!i64BeginTimeline))
    {
        VHOUTPUTFRAME *pOutputFrame = RTListGetFirst(pListFramesToSend,
                                                     VHOUTPUTFRAME,
                                                     NodeOutputFrame);
        if (pOutputFrame)
        {
            i64BeginTimeline = pOutputFrame->i64Timestamp;
            pPresentation->i64BeginTimeline = i64BeginTimeline;
            fStart = true;
        }
    }

    /* Sending frames does not require the VideoChannelLock. */

    /*
     * 1) Add pListFramesToSend to the local ListKeepFrames and keep these frames.
     */
    VHOUTPUTFRAME *pOutputFrame = RTListGetFirst(pListFramesToSend,
                                                 VHOUTPUTFRAME,
                                                 NodeOutputFrame);

    while (pOutputFrame)
    {
        VHOUTPUTFRAME *pNext = RTListNodeIsLast(pListFramesToSend, &pOutputFrame->NodeOutputFrame)?
                                    NULL:
                                    RTListNodeGetNext(&pOutputFrame->NodeOutputFrame,
                                                      VHOUTPUTFRAME, NodeOutputFrame);

        OUTPUTFRAMEKEEP *pFrameKeep = (OUTPUTFRAMEKEEP *)VRDPMemAllocZ(sizeof (OUTPUTFRAMEKEEP));

        if (pFrameKeep)
        {
            VRDPPktAddRef(&pOutputFrame->pkt);

            pFrameKeep->pOutputFrame = pOutputFrame;
            pFrameKeep->fObsolete = false;
            pFrameKeep->u32VideoStreamId = u32VideoStreamId;
            RTListAppend(&m_ListKeepFrames, &pFrameKeep->NodeOutputFrameKeep);
        }

        pOutputFrame = pNext;
    }

    /*
     * 2) Scan entire m_ListKeepFrames to find frames which must be sent now.
     */
    OUTPUTFRAMEKEEP *pFrameKeep = RTListGetFirst(&m_ListKeepFrames,
                                                 OUTPUTFRAMEKEEP,
                                                 NodeOutputFrameKeep);

    while (pFrameKeep)
    {
        OUTPUTFRAMEKEEP *pNext = RTListNodeIsLast(&m_ListKeepFrames, &pFrameKeep->NodeOutputFrameKeep)?
                                     NULL:
                                     RTListNodeGetNext(&pFrameKeep->NodeOutputFrameKeep,
                                                       OUTPUTFRAMEKEEP, NodeOutputFrameKeep);

        if (pFrameKeep->u32VideoStreamId == u32VideoStreamId)
        {
            pOutputFrame = pFrameKeep->pOutputFrame;

            if (pOutputFrame->i64SampleEndTime <= i64NowTimeline)
            {
                /* Frame is too old. */
                pFrameKeep->fObsolete = true;

                if (pNext == NULL)
                {
                    /* This is the last frame, so send anyway. */
                    sendFrame(pOutputFrame, i64NowTimeline, &rectClient, &rectScaled, scaling, id);
                }
            }
            else if (pOutputFrame->i64SampleStartTime <= i64NowTimeline)
            {
                /* Frame should be displayed now. */
                pFrameKeep->fObsolete = true;
                sendFrame(pOutputFrame, i64NowTimeline, &rectClient, &rectScaled, scaling, id);
            }
        }

        pFrameKeep = pNext;
    }

    /*
     * Release obsolete frames.
     */
    pFrameKeep = RTListGetFirst(&m_ListKeepFrames,
                                OUTPUTFRAMEKEEP,
                                NodeOutputFrameKeep);

    while (pFrameKeep)
    {
        OUTPUTFRAMEKEEP *pNext = RTListNodeIsLast(&m_ListKeepFrames, &pFrameKeep->NodeOutputFrameKeep)?
                                     NULL:
                                     RTListNodeGetNext(&pFrameKeep->NodeOutputFrameKeep,
                                                       OUTPUTFRAMEKEEP, NodeOutputFrameKeep);

        if (pFrameKeep->fObsolete)
        {
            SUNFLSHLOG(("SUNFLSH: released frame diff %lld\n", pFrameKeep->pOutputFrame->i64SampleStartTime - i64NowTimeline));

            VRDPPktRelease(&pFrameKeep->pOutputFrame->pkt);
            RTListNodeRemove(&pFrameKeep->NodeOutputFrameKeep);
            VRDPMemFree(pFrameKeep);
        }

        pFrameKeep = pNext;
    }
}

void VideoChannelSunFlsh::VideoChannelInput (uint32_t u32Event,
                                             const void *pvData,
                                             uint32_t cbData)
{
    SUNFLSHLOG(("sunFlsh: u32Event %d, pvData %p, cbData %d\n", u32Event, pvData, cbData));

    VHCONTEXT *pCtx = Client()->Server()->VideoHandler();

    if (!pCtx)
    {
        /* Video redirection is not enabled. */
        return;
    }

    switch (u32Event)
    {
        case SUNFLSH_EVENT_CREATE:
        {
            Event(VIDEO_CHANNEL_SUPPORTED, NULL, 0);
        } break;

        case SUNFLSH_EVENT_DATA:
        {
#ifdef DEBUG_sunlover
            SUNFLSHLOG(("sunFlsh Data:\n"));
            hexdump((uint8_t *)pvData, cbData);
#endif

            if (cbData < sizeof(VChannelData))
            {
                SUNFLSHLOG(("cbData < VCChannelData!!!\n"));
                break;
            }

            const VChannelData *pMsgHdr = (VChannelData *)pvData;
            SUNFLSHLOG(("sunFlsh: type %d, length %d\n",
                      pMsgHdr->type, pMsgHdr->length));

            if (pMsgHdr->type == FLASH_INIT_REPLY)
            {
                if (cbData < sizeof(VCInitReplyData))
                {
                    SUNFLSHLOG(("cbData < VCInitReplyData!!!\n"));
                    break;
                }

                VCInitReplyData *pReply = (VCInitReplyData *)pMsgHdr;

                SUNFLSHLOG(("VCInitReplyData: V%d.%d err %d caps 0x%08X\n",
                          pReply->major, pReply->minor, pReply->error, pReply->capabilities));

                VRDPLOGREL(("SUNFLSH: V%d.%d, status %d, caps 0x%08X\n",
                          pReply->major, pReply->minor, pReply->error, pReply->capabilities));

                if (   pReply->error == 0
                    && HAS_JPEG_CAPABILITIES(pReply->capabilities)
                    && HAS_SCALING_CAPABILITIES(pReply->capabilities))
                {
                    videoHandlerNotifyClientConnect(pCtx);
                    Event(VIDEO_CHANNEL_ACTIVE, NULL, 0);
                    m_fVideoRedirectionSupported = true;
                }
                else
                {
                    static int scLogged = 0;
                    if (scLogged < 16)
                    {
                        scLogged++;
                        VRDPLOGREL(("SunFlsh video redirection channel not enabled: caps %x, result %x\n",
                                    pReply->capabilities, pReply->error));
                    }
                }
            }
            else if (pMsgHdr->type == FLASH_VIDEO_REPLY)
            {
                if (cbData < sizeof(VCVideoReply))
                {
                    SUNFLSHLOG(("cbData < VCVideoReply!!!\n"));
                    break;
                }

                VCVideoReply *pReply = (VCVideoReply *)pMsgHdr;
                RT_NOREF_PV(pReply);
                SUNFLSHLOG(("VCVideoReply: timestamp %u processing %d, elapsed %d\n",
                            pReply->timestamp, pReply->tProcessing, sunFlshTimestampGet() - pReply->timestamp));
            }
            else
            {
                SUNFLSHLOG(("Ignored!!!\n"));
            }
        } break;

        case SUNFLSH_EVENT_CLOSE:
        {
            /** @todo */
        } break;

        default:
            AssertFailed();
    }
}

void VideoChannelSunFlsh::VideoChannelVisibleRegion(const VHSTREAMPARMS *pStream,
                                                    uint32_t cRects,
                                                    RTRECT *paRects,
                                                    RTRECT *pRect)
{
    /// @todo
    NOREF(pStream);
    NOREF(cRects);
    NOREF(paRects);
    NOREF(pRect);
}

