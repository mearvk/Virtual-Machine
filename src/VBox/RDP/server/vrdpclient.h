/* $Id: vrdpclient.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vrdpclient_h
#define VRDP_INCLUDED_SRC_vrdpclient_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdp.h"
#include "textcache.h"
#include "bmpcomp.h"
#include "audioin.h"
#include "utcinfo.h"
#include "shadowbuffer.h"
#include "videostream.h"
#include <iprt/uuid.h>
#include <iprt/semaphore.h>

#include "tsmf.h"
#include "vrdpvideoin.h"
#include "vrdpinput.h"

extern "C" {
#include "mixeng/mixeng.h"
}

class VRDPServer;
class VRDPClient;

struct _OutputUpdate;
typedef struct _OutputUpdate OutputUpdate;

/* A generic output message which will invoke a callback. */
#define VRDP_OUTPUT_GENERIC 1000

typedef struct VRDPOUTPUTGENERICHDR VRDPOUTPUTGENERICHDR;

typedef DECLCALLBACKTYPE(void, FNOUTPUTGENERIC,(VRDPClient *pClient, const VRDPOUTPUTGENERICHDR *pHdr));
typedef FNOUTPUTGENERIC *PFNOUTPUTGENERIC;

#pragma pack(1)
struct VRDPOUTPUTGENERICHDR
{
    PFNOUTPUTGENERIC pfnCallback;
    void *pvUser;
    uint32_t u32Size; /* Size of the structure: the hDR + possible additional data. */
    uint32_t u32Parm; /* A generic parameter. */
    /* The callback specific data follow. */
};
#pragma pack()

/* Helper for the client usage state by VM and OUTPUT threads. */
class VRDPClientUseStatus
{
    private:
        enum ClientUseStateEnum
        {
            ClientUseStateEnum_Free,
            ClientUseStateEnum_Use,
            ClientUseStateEnum_Disabled
        };

        volatile uint32_t m_u32Status;

    public:
        VRDPClientUseStatus () : m_u32Status (ClientUseStateEnum_Disabled) {};
        ~VRDPClientUseStatus () {};

        uint32_t Status (void) { return m_u32Status; }

        /* Called by user (VM or OUTPUT thread) to prevent client deletion by owner (INPUT thread). */
        bool ClientUseStateFree2Use (void)
        {
            return ASMAtomicCmpXchgU32 (&m_u32Status, ClientUseStateEnum_Use, ClientUseStateEnum_Free);
        }

        /* Called by user (VM or OUTPUT thread) to free the client for owner (INPUT thread). */
        bool ClientUseStateUse2Free (void)
        {
            /* That must always be successful, because that can be only called after successful 'ClientUseStateFree2Use'. */
            bool f = ASMAtomicCmpXchgU32 (&m_u32Status, ClientUseStateEnum_Free, ClientUseStateEnum_Use);
            Assert(f);
            return f;
        }

        /* Called by user (OUTPUT thread) to disable further output. */
        bool ClientUseStateUse2Disabled (void)
        {
            /* That must always be successful, because that can be only called after successful 'ClientUseStateFree2Use'. */
            bool f = ASMAtomicCmpXchgU32 (&m_u32Status, ClientUseStateEnum_Disabled, ClientUseStateEnum_Use);
            Assert(f);
            return f;
        }

        /* Called by owner (INPUT thread) to prevent client access by the user (OUTPUT or VM thread). */
        bool ClientUseStateFree2Disabled (void)
        {
            return ASMAtomicCmpXchgU32 (&m_u32Status, ClientUseStateEnum_Disabled, ClientUseStateEnum_Free);
        }

        /* Called by owner (INPUT thread) to allow client access by the user (OUTPUT or VM thread). */
        bool ClientUseStateDisabled2Free (void)
        {
            return ASMAtomicCmpXchgU32 (&m_u32Status, ClientUseStateEnum_Free, ClientUseStateEnum_Disabled);
        }

        /* Called by owner (INPUT thread) to verify that the access is disabled for the user (OUTPUT or VM thread). */
        bool ClientUseStateCheckDisabled (void)
        {
            return ASMAtomicCmpXchgU32 (&m_u32Status, ClientUseStateEnum_Disabled, ClientUseStateEnum_Disabled);
        }
};

/*
 * There are following redraw request sources:
 *   1) fullscreen redraw made by the server;
 *   2) client's UPDATE_RECT requests;
 *   3) partial updates from the server for example to do a graphics order fallback.
 * For 1, VRDPRedrawInfo::fFullscreen is set, and overwrites all other redraw request.
 * For 2, VRDPRedrawClient is used.
 * For 3, VRDPRedrawShadowBuffer is used.
 */
typedef struct VRDPRedrawClient
{
    /* The rectangle is in the client coords. */
    RGNRECT rectRedraw;

} VRDPRedrawClient;

typedef struct VRDPRedrawShadowBuffer
{
    /* The rectangle is the shadow buffer coords of the corresponding screen. */
    RGNRECT rectRedraw;

    /* The screen to be updated */
    unsigned uScreenId;

} VRDPRedrawShadowBuffer;

typedef struct VRDPRedrawInfo
{
    int cRectsClient;
    VRDPRedrawClient aRectsClient[8];

    int cRectsShadowBuffer;
    VRDPRedrawShadowBuffer aRectsShadowBuffer[32];

    /* Whether a fullscreen update is pending. */
    bool fFullscreen;

} VRDPRedrawInfo;

/*
 * Pointer cache.
 */
#define VRDP_POINTER_IDX_NULL (0xFF)
#define VRDP_POINTER_SERVER_ID_NULL (0)
#define VRDP_POINTER_CLIENT_ID_NULL (0xFFFF)

typedef struct _VRDPCLIENTPOINTERCACHEELEM
{
    uint32_t u32ServerId;

    uint16_t u16RemoteId;

    uint8_t u8MRUNext;
    uint8_t u8MRUPrev;
} VRDPCLIENTPOINTERCACHEELEM;

typedef struct _VRDPCLIENTPOINTERCACHE
{
    uint8_t u8Pointers;

    uint8_t u8MRUHead;
    uint8_t u8MRUTail;

    VRDPCLIENTPOINTERCACHEELEM *paPointers;

    _VRDPCLIENTPOINTERCACHE() { init (); };

    void init (void)
    {
        u8Pointers = 0;
        u8MRUHead = VRDP_POINTER_IDX_NULL;
        u8MRUTail = VRDP_POINTER_IDX_NULL;
        paPointers = NULL;
    }
} VRDPCLIENTPOINTERCACHE;


/*
 * Bitmap cache.
 */
#define VRDP_CLIENT_BC_IDX_NULL (0xFFFF)

#define VRDP_CLIENT_BC_MAX_BITMAPS_0 (0x78)
#define VRDP_CLIENT_BC_MAX_BITMAPS_1 (0x78)
#define VRDP_CLIENT_BC_MAX_BITMAPS_2 (0x151)

typedef struct _VRDPCLIENTBITMAPCACHEELEM
{
    uint16_t u16MRUNext;
    uint16_t u16MRUPrev;

    VRDPBitmapTileHash tileHash;
    uint16_t u16TileX;
    uint16_t u16TileY;

    uint16_t u16RemoteId;
} VRDPCLIENTBITMAPCACHEELEM;

typedef struct _VRDPCLIENTBITMAPCACHE
{
    uint16_t au16MRUHead[3];
    uint16_t au16MRUTail[3];

    VRDPCLIENTBITMAPCACHEELEM aCache0[VRDP_CLIENT_BC_MAX_BITMAPS_0];
    VRDPCLIENTBITMAPCACHEELEM aCache1[VRDP_CLIENT_BC_MAX_BITMAPS_1];
    VRDPCLIENTBITMAPCACHEELEM aCache2[VRDP_CLIENT_BC_MAX_BITMAPS_2];
} VRDPCLIENTBITMAPCACHE;

#pragma pack(1)
typedef struct _TEXTFRAGMENTELEM
{
   uint64_t u64CRC;
   uint8_t  u8Index;
   uint8_t  u8Length;
   uint8_t  u8FontHandle;
} TEXTFRAGMENTELEM;
#pragma pack()

typedef struct _TEXTFRAGMENTCACHE
{
   TEXTFRAGMENTELEM aElems[0xc0];
   uint8_t u8FreeElem; /* Index of element which will be considered free and will
                        * be reused if a new fragment has to be added to the cache.
                        */
} TEXTFRAGMENTCACHE;

/* Can be up to 64 according to RDP specs. */
#define RDP_BRUSH_CACHE_SIZE 16

typedef struct _BRUSHCACHEELEM
{
    uint8_t au8Pattern[8];
    bool fCached;
} BRUSHCACHEELEM;

typedef struct _BRUSHCACHE
{
    BRUSHCACHEELEM aBrushes[RDP_BRUSH_CACHE_SIZE];
    int iLastBrush;
} BRUSHCACHE;


class VRDPClient;
class VideoChannel;

/*
 * Interface for a video channel implementation.
 */
typedef int FNVIDEOCHANNELEVENT(void *pvCtx, uint32_t u32Event, void *pvData, size_t cbData);
typedef FNVIDEOCHANNELEVENT *PFNVIDEOCHANNELEVENT;

#define VIDEO_CHANNEL_SUPPORTED 0
#define VIDEO_CHANNEL_ACTIVE    1

typedef struct VCESupported
{
    VideoChannel *pVideoChannel;
} VCESupported;

typedef struct VCEActive
{
    VideoChannel *pVideoChannel;
} VCEActive;

class VideoChannel
{
    private:
        VRDPClient *m_pClient;
        PFNVIDEOCHANNELEVENT m_pfnEvent;
        void *m_pvEvent;

    public:
        VideoChannel(VRDPClient *pClient, PFNVIDEOCHANNELEVENT pfnEvent, void *pvEvent)
            :
            m_pClient(pClient),
            m_pfnEvent(pfnEvent),
            m_pvEvent(pvEvent)
            {};
        virtual ~VideoChannel() {};

        VRDPClient *Client(void) { return m_pClient; }
        int Event(uint32_t u32Event, void * pvData, size_t cbData);

        virtual const char *VideoChannelName(void) = 0;
        virtual int VideoChannelSetup(void) = 0;
        virtual int VideoChannelStart (const VHSTREAMPARMS *pStream) = 0;
        virtual int VideoChannelEnd (const VHSTREAMPARMS *pStream) = 0;
        virtual void VideoChannelSendFrames (VHCONTEXT *pCtx, /** @todo context not needed? */
                                             VHSTREAMPARMS *pStream,
                                             RTLISTANCHOR *pListFramesToSend,
                                             int64_t i64NowTimeline) = 0;
        virtual void VideoChannelInput (uint32_t u32Event,
                                        const void *pvData,
                                        uint32_t cbData) = 0;
        virtual void VideoChannelVisibleRegion(const VHSTREAMPARMS *pStream,
                                               uint32_t cRects,
                                               RTRECT *paRects,
                                               RTRECT *pRect) = 0;

        virtual void OnAudioSend(uint64_t u64PacketStartNanoTS,
                                 uint64_t u64PacketEndNanoTS,
                                 uint64_t u64DurationNano)
        {
            NOREF(u64PacketStartNanoTS);
            NOREF(u64PacketEndNanoTS);
            NOREF(u64DurationNano);
            return;
        }
        virtual void OnAudioClose(void) { return; }
};

/*
 * The NULL video channel implementation.
 */
class VideoChannelNull: public VideoChannel
{
    public:
        VideoChannelNull(VRDPClient *pClient, PFNVIDEOCHANNELEVENT pfnEvent, void *pvEvent)
            :
            VideoChannel(pClient, pfnEvent, pvEvent)
            {};
        virtual ~VideoChannelNull() {};

        virtual const char *VideoChannelName(void) { return "NULL"; };

        virtual int VideoChannelSetup(void)
        {
            return VERR_NOT_SUPPORTED;
        }

        virtual int VideoChannelStart (const VHSTREAMPARMS *pStream)
        {
            RT_NOREF1(pStream);
            VIDEOLOG(("VideoChannelNull::VideoChannelStart\n"));
            return VERR_NOT_SUPPORTED;
        }

        virtual int VideoChannelEnd (const VHSTREAMPARMS *pStream)
        {
            RT_NOREF1(pStream);
            return VERR_NOT_SUPPORTED;
        }

        virtual void VideoChannelSendFrames (VHCONTEXT *pCtx,
                                             VHSTREAMPARMS *pStream,
                                             RTLISTANCHOR *pListFramesToSend,
                                             int64_t i64NowTimeline)
        {
            RT_NOREF4(pCtx, pStream, pListFramesToSend, i64NowTimeline);
            return;
        }

        virtual void VideoChannelInput (uint32_t u32Event,
                                        const void *pvData,
                                        uint32_t cbData)
        {
            RT_NOREF3(u32Event, pvData, cbData);
            return;
        }
        virtual void VideoChannelVisibleRegion(const VHSTREAMPARMS *pStream,
                                               uint32_t cRects,
                                               RTRECT *paRects,
                                               RTRECT *pRect)
        {
            RT_NOREF4(pStream, cRects, paRects, pRect);
            return;
        }
};

int VideoChannelTSMFCreate(VideoChannel **ppVideoChannel,
                           VRDPClient *pClient,
                           PFNVIDEOCHANNELEVENT pfnEvent,
                           void *pvEvent);
int VideoChannelSunFlshCreate(VideoChannel **ppVideoChannel,
                              VRDPClient *pClient,
                              PFNVIDEOCHANNELEVENT pfnEvent,
                              void *pvEvent);

typedef struct VRDPCLIENTORDERSCREENDATA
{
    /* Saved bounds. */
    struct
    {
        VRDEORDERBOUNDS savedBounds;
        bool fBounds;
    } bounds;

    /* Saved order. */
    struct
    {
        int32_t i32Op;
        uint32_t cbOrder;
        void *pvOrder;              /* Points to the actual buffer wich contains cbOrder bytes. */

        uint8_t au8SavedOrder[256]; /* Preallocated buffer for small orders. */
        uint8_t *pu8SavedOrder;     /* Dynamically allocated for large orders. */
        uint32_t cbAllocated;
    } order;

    struct
    {
        bool fSequenceInProcess;
        bool fSavedSequence;
        VRDPSAVEDINTERNALORDER *pSavedInternalOrders;
    } sequence;
} VRDPCLIENTORDERSCREENDATA;

typedef struct VRDPCLIENTORDERDATA
{
    /* The screen id of the last processed order. Used to detect screen id changes. */
    unsigned uLastScreenId;

    /* Per screen information. */
    VRDPCLIENTORDERSCREENDATA aScreens[RDP_MAX_SCREENS];

} VRDPCLIENTORDERDATA;

/*
 * Manages a single VRDP client.
 * Maintains the RDP connection status.
 */
class VRDPClient
{
    private:

        /* The server instance that the client belongs to.
         * A single server can have a few clients.
         */
        VRDPServer *m_pServer;

        /* The RDP protocol implementation instance. The client receives incoming and
         * sends outgoing RDP packets via this interface.
         */
        VRDPTP m_vrdptp;

        /* Indicates that the RDP connection has been established. */
        bool m_fConnected;

        /* The client input context. */
        VRDPInputCtx m_InputCtx;

        /* The client output context. */
        VRDPStream m_Stream;

        /* The transport identifier for the client. */
        VRDPTRANSPORTID m_TransportId;

        /* An unique server side identifier for the client. */
        uint32_t m_u32ClientId;

        VRDPClientUseStatus m_StatusOutput;
        VRDPClientUseStatus m_StatusVM;

        VRDPCLIENTPOINTERCACHE m_PointerCache;
        VRDPCLIENTBITMAPCACHE m_BitmapCache;
        TCCLIENTCACHE m_FontCache;
        TEXTFRAGMENTCACHE m_TextFragmentCache;
        BRUSHCACHE m_BrushCache;

        bool m_fDemandActiveInProcess;
        volatile uint32_t m_u32PendingResize;

        VRDPRedrawInfo m_redraw;

        uint32_t m_fu32Intercepted;

        volatile uint32_t m_u32EventFlags;

        uint32_t m_fu32SupportedOrders;

        /* Video channel implementations. */
        VideoChannelNull *m_pVideoChannelNull;
        VideoChannel *m_pVideoChannelTSMF;
        VideoChannel *m_pVideoChannelSunFlsh;

        /* Currently used video channel. */
        VideoChannel *m_pVideoChannel;
        bool m_fVideoChannelActive;
        char m_achLastVideoChannelName[32];

        static void sunFlsh(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData);
        static int videoChannelEvent(void *pvCtx, uint32_t u32Event, void *pvData, size_t cbData);

        TSMFRaw m_TSMFRaw;

        AudioInputClient m_ai;

        UTCINFOClient m_UTCINFO;

        bool m_fDisableDisplay;
        bool m_fDisableInput;
        bool m_fDisableAudio;
        bool m_fDisableUSB;
        bool m_fDisableClipboard;
        bool m_fDisableRDPDR;

        VRDPCLIENTORDERDATA m_orderData;

        VideoInClient m_VideoIn;
        InputClient m_Input;

        void bitmapCacheSetup (void);
        void fontCacheSetup (void);
        void brushCacheSetup (void);

        void pointerCacheSetup (void);
        void pointerCacheDestroy (void);

        void pointerCacheOutput (uint32_t u32ServerId);

        uint8_t textFragmentCache (uint8_t *pu8Text, uint8_t *pcbText, uint8_t u8CharInc, int16_t i16Delta, uint8_t u8FontHandle);
        void textFragmentRemove (uint8_t u8Fragment);

        void setupMultiMonitor(void);

        int processOutputOrder (unsigned uScreenId, int32_t i32Op, void *pvOrder, uint32_t cbOrder);

    public:

        VRDPClient (VRDPServer *pServer, uint32_t u32ClientId);
        virtual ~VRDPClient ();

        /* Called by the server when a new client established a transport connection. */
        int Start (VRDPTransport *pTransport, VRDPTRANSPORTID id);

        /* Called by VRDP transport when RDP connection has been established. */
        void NotifyConnected (void);

        /* Called by VRDP transport when the RDP connection should be closed. */
        void NotifyDisconnect (bool fReconnect);

        /* Called by VRDP server from TransportDisconnect callback. */
        int NotifyDelete(void);

        /* Called by VRDP server to redirect the current client connection. */
        void Redirect(const char *pszServer, const char *pszUser, const char *pszDomain, const char *pszPassword, uint32_t u32SessionId, const char *pszCookie);

        VRDPInputCtx *InputCtx (void) { return &m_InputCtx; };
        VRDPStream *Stream (void) { return &m_Stream; };
        VRDPTP *TP(void) { return &m_vrdptp; };

        VRDPTRANSPORTID TransportId (void) { return m_TransportId; };

        uint32_t Id (void) { return m_u32ClientId; };

        VRDPServer *Server (void) { return m_pServer; };

        uint32_t QueryIntercepted (void) { return m_fu32Intercepted; };

        void NotifyIntercepted (uint32_t fu32Intercepted) { m_fu32Intercepted |= fu32Intercepted; };

        bool IsClientResizable (void);

        /** @thread OUTPUT */
        void NotifyResizeAndThreadContextDisableBoth (void);

        int QueryRedraw (VRDPRedrawInfo *pRedraw);
        /* AddRedraw takes either client's or shadowbuffer's coords. */
        void AddRedraw (const RGNRECT *pRect, bool fShadowBuffer, unsigned uScreenId);

        VRDPClientUseStatus *StatusOutput (void) { return &m_StatusOutput; }
        VRDPClientUseStatus *StatusVM (void)     { return &m_StatusVM; }

        /*
         * VM/OUTPUT thread. Changes the clients 'Status' to 'free' for the specified context.
         */
        void ThreadContextRelease (int iContext);

        /* Processes an incoming transport level packet.
         * This is called on INPUT thread.
         */
        int ProcessIncoming (uint32_t fu32Events);

        int ThreadContextDisableBoth (void);

        void Output (OutputUpdate *pUpdate);

        int OutputBitmap2 (unsigned uScreenId, VRDPBitmapCompressed *pBmp, uint16_t x, uint16_t y);

        int OutputOrder (unsigned uScreenId, int32_t i32Op, void *pvOrder, uint32_t cbOrder);

        void OutputText2 (unsigned uScreenId, const TCFONTTEXT2 *pFontText2);

        bool OutputMemblt (unsigned uScreenId, VRDPBitmapCompressed *pBmp, uint16_t u16TileX, uint16_t u16TileY,
                           uint16_t u16DstX, uint16_t u16DstY,
                           uint16_t u16SrcX, uint16_t u16SrcY,
                           uint16_t u16SrcW, uint16_t u16SrcH,
                           uint8_t u8ROP,
                           const RGNRECT *pRectTile,
                           uint8_t u8TileSizeClass);

        void OutputAudio (st_sample_t *paSamples, int cSamples, bool fFinished, uint64_t u64SamplesStartNanoTS);

        void AudioCompletion (uint8_t u8PacketIdx);

        /** @todo TP()->DesktopMap()->IsScreenMatched() */
        bool IsScreenIdMatched (unsigned uScreenId) { return IsMultiMonitor() || TP()->QueryScreenId () == uScreenId; }
        bool IsMultiMonitor(void) { return TP()->DesktopMap()->DesktopMode() == VRDP_CLIENT_DESKTOP_MAP_MULTI_MONITOR; }
        bool IsOrderSupported (int iOrder);
        int VideoStreamStart (const VHSTREAMPARMS *pStream);
        int VideoStreamEnd (const VHSTREAMPARMS *pStream);
        void VideoStreamVisibleRegion(const VHSTREAMPARMS *pStream, uint32_t cRects, RTRECT *paRects, RTRECT *pRect);
        void VideoStreamSendFrames (VHCONTEXT *pCtx, VHSTREAMPARMS *pStream, RTLISTANCHOR *pListFramesToSend, int64_t i64NowTimeline);

        void QueryClientFeatures(void);
        void SetupChannels(void);
        bool IsInputDisabled (void) { return m_fDisableInput; };
        void DisableDisplay(bool fDisableDisplay) { m_fDisableDisplay = fDisableDisplay; }
        bool IsDisplayDisabled(void) { return m_fDisableDisplay; }

        void OnAudioClose (void);
        void OnAudioSend (uint64_t u64PacketStartNanoTS, uint64_t u64PacketEndNanoTS, uint64_t u64DurationNano);
};

#endif /* !VRDP_INCLUDED_SRC_vrdpclient_h */
