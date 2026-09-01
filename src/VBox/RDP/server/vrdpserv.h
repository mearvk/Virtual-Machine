/* $Id: vrdpserv.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vrdpserv_h
#define VRDP_INCLUDED_SRC_vrdpserv_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "transport.h"

#include "vrdp.h"
#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/RemoteDesktop/VRDEImage.h>
#include <VBox/RemoteDesktop/VRDEMousePtr.h>

#include "shadowbuffer.h"

#include "vrdpclient.h"
#include "bmpcache.h"
#include "bmpcomp.h"
extern "C" {
#include "mixeng/mixeng.h"
}

#include "videostream.h"

#include "vhstat.h"

#include "vrdpimage.h"
#include "vrdpscard.h"
#include "vrdpvideoin.h"
#include "vrdpinput.h"

#include "tsmf.h"


#define VRDP_ASSERT_CONTEXT_EXTERN(__server) do {  Assert(!(__server)->IsOutputThread () && !(__server)->IsInputThread ()));  } while (0)
#define VRDP_ASSERT_CONTEXT_INPUT(__server) do { Assert((__server)->IsInputThread ()); } while (0)
#define VRDP_ASSERT_CONTEXT_OUTPUT(__server) do { Assert((__server)->IsOutputThread ()); } while (0)

// Codes for messages that are sent to output thread
#define VRDP_OUTPUT_BITMAP         1
#define VRDP_OUTPUT_HIDE_POINTER   4
#define VRDP_OUTPUT_CACHED_POINTER 5
#define VRDP_OUTPUT_SET_VOLUME     6
#define VRDP_OUTPUT_USB            7

#define VRDP_OUTPUT_CLIPBOARD_DATA      20
#define VRDP_OUTPUT_CLIPBOARD_PACKET    21

#define VRDP_OUTPUT_PALETTE_8_BPP  9
#define VRDP_OUTPUT_AUDIO_COMPLETION 10
#define VRDP_OUTPUT_KEEP_ALIVE     11

#define VRDP_OUTPUT_DVC            30

#define VRDP_OUTPUT_AUDIOINPUT_DATA  40
#define VRDP_OUTPUT_AUDIOINPUT_OPEN  41
#define VRDP_OUTPUT_AUDIOINPUT_CLOSE 42

#define VRDP_OUTPUT_RDPDR            60

/* Server output events. */
#define VRDP_OUTPUT_SERVER_FIRST        100
#define VRDP_OUTPUT_VIDEO_STREAM_EVENT  100
#define VRDP_OUTPUT_VIDEO_STREAM_END    101
#define VRDP_OUTPUT_VIDEO_COMPRESSION_RATE 103
#define VRDP_OUTPUT_VIDEO_STREAM_DISABLE 104
#define VRDP_OUTPUT_VIDEO_STREAM_ENABLE 105
#define VRDP_OUTPUT_VIDEO_STREAM_SIZE_LIMIT 106
#define VRDP_OUTPUT_VIDEO_STREAM_RESTART 107
#define VRDP_OUTPUT_SERVER_LAST         107

typedef struct _VRDPKeyboardModifiers
{
    bool fLeftShift;
    bool fLeftCtrl;
    bool fLeftAlt;
    bool fRightShift;
    bool fRightCtrl;
    bool fRightAlt;
} VRDPKeyboardModifiers;

class VRDPUpdateRedraw
{
    public:
        VRDPUpdateRedraw () : bmp () {};
        ~VRDPUpdateRedraw () {};

        VRDPBitmapCompressed bmp;
        int16_t x;
        int16_t y;
        bool fClearClientRect;
        RGNRECT rectClient;
};

typedef struct _VRDPSetVolumeStruct
{
    uint16_t left;
    uint16_t right;
} VRDPSetVolumeStruct;

class VRDPServer;

/* The server id is 32 bit value. High byte is the pointer index in the cache.
 * Low 3 bytes is an "unique" pointer handle.
 * Use inline functions for better type control.
 */
inline uint32_t pcMakeServerId (uint32_t u32Handle, uint8_t u8Index)
{
    Assert (u32Handle <= 0xFFFFFFU);
    return (u8Index << 24) + u32Handle;
}

inline uint32_t pcDecodeServerHandle (uint32_t u32ServerId)
{
    return u32ServerId & 0xFFFFFFU;
}

inline uint8_t pcDecodeServerIndex (uint32_t u32ServerId)
{
    return (uint8_t)(u32ServerId >> 24);
}

typedef struct _VRDPPOINTERCACHEELEM
{
    bool    fUsed;

    uint8_t u8MRUNext;
    uint8_t u8MRUPrev;

    uint32_t u32ServerId;

    VRDEMOUSEPTRDATA *pPointer;

    _VRDPPOINTERCACHEELEM()
        :
        fUsed       (false),
        u8MRUNext   (VRDP_POINTER_IDX_NULL),
        u8MRUPrev   (VRDP_POINTER_IDX_NULL),
        u32ServerId (VRDP_POINTER_SERVER_ID_NULL),
        pPointer    (NULL)
     {};

} VRDPPOINTERCACHEELEM;

typedef struct _VRDPPOINTERCACHE
{
    VRDPPOINTERCACHEELEM aPointers[64];

    uint32_t u32ServerHandleSrc;

    uint8_t u8MRUHead;
    uint8_t u8MRUTail;

    uint8_t u8Pointers;
    uint8_t u8LatestPointer;

    _VRDPPOINTERCACHE ()
        :
        u32ServerHandleSrc (VRDP_POINTER_SERVER_ID_NULL),
        u8MRUHead          (VRDP_POINTER_IDX_NULL),
        u8MRUTail          (VRDP_POINTER_IDX_NULL),
        u8Pointers         (0),
        u8LatestPointer    (VRDP_POINTER_IDX_NULL)
    {};
    ~_VRDPPOINTERCACHE ()
    {
        uint8_t u8Index = u8MRUHead;

        while (u8Index != VRDP_POINTER_IDX_NULL)
        {
            VRDPPOINTERCACHEELEM *pCacheElem = &aPointers[u8Index];

            if (pCacheElem->pPointer)
            {
                VRDPMemFree (pCacheElem->pPointer);
                pCacheElem->pPointer = NULL;
            }

            u8Index = pCacheElem->u8MRUNext;
        }
    }
} VRDPPOINTERCACHE;

typedef struct _VRDPServerThreadStartCtx
{
    int rc;
    VRDPServer *pVRDPServer;
} VRDPServerThreadStartCtx;

typedef struct _VRDPFBINFO
{
    VRDPBITSRECT bitsRect;
} VRDPFBINFO;

/*
 * The VRDP server holds an associative array to map transport identifiers and clients ids to client instances.
 * The array is also used for client status changes.
 */
class VRDPClientArray
{
    private:
        VRDPLock *m_pClientArrayLock;

        typedef struct _VRDPClientArrayTransportId
        {
            VRDPClient *pClient;
            VRDPTRANSPORTID transportId;
        } VRDPClientArrayTransportId;

        typedef struct _VRDPClientArrayClientId
        {
            VRDPClient *pClient;
            uint32_t u32ClientId;
        } VRDPClientArrayClientId;

        /* Number of valid mappings in the 'paMapTableTransportId' and the 'm_paTableClientId'. */
        int m_cElements;
        /* Number of allocated elements in the 'paMapTableTransportId' and the 'm_paTableClientId' */
        int m_cAllocated;

        /* Pointer to sorted array of mappings, sorted by transportId. */
        VRDPClientArrayTransportId *m_paTableTransportId;
        /* Pointer to sorted array of mappings, sorted by clientId. */
        VRDPClientArrayClientId *m_paTableClientId;

        int m_cTotalClients;

        volatile int64_t m_i64TimeLastConnect;
        volatile int64_t m_i64TimeLastDisconnect;

        VRDPClient *m_pLastClient;

        bool lock (void);
        void unlock (void);

        int lookupIndexByTransportId (VRDPTRANSPORTID transportId);
        int lookupIndexByClientId (uint32_t u32ClientId);

    public:
        VRDPClientArray();
        ~VRDPClientArray();

        /*
         * INPUT thread methods, those change the mappings.
         * They verify that the client 'StatusOutput' and 'StatusVM'
         * are 'disabled'. They fail if that's not true.
         */
        int Insert (VRDPClient *pClient, VRDPTRANSPORTID transportId);
        int Remove (VRDPClient *pClient);

        /*
         * INPUT thread. Does not change client state. The client pointer is
         * in use only by the INPUT thread.
         */
        VRDPClient *LookupTransportId (VRDPTRANSPORTID transportId);
        VRDPClient *GetNextClient (VRDPClient *pClient);

        /*
         * VM/OUTPUT thread. Client enumeration, changes the clients 'Status' to 'use'.
         */
        VRDPClient *ThreadContextGetNextClient (uint32_t *pu32ClientId, int iContext);
        VRDPClient *ThreadContextGetClient (uint32_t u32ClientId, int iContext);

        /* Statistics and information. */
        uint32_t NumberOfClientsActive (void);
        uint32_t NumberOfClientsInactive (void);
        int64_t TimeLastConnect (void);
        int64_t TimeLastDisconnect (void);
        uint64_t BytesSentLast (void);
        uint64_t BytesSentAll (void);
        uint64_t BytesRecvLast (void);
        uint64_t BytesRecvAll (void);
        uint32_t StringLastUser (void *pvBuffer, size_t cbBuffer);
        uint32_t StringLastDomain (void *pvBuffer, size_t cbBuffer);
        uint32_t StringLastClientName (void *pvBuffer, size_t cbBuffer);
        uint32_t StringLastClientIP (void *pvBuffer, size_t cbBuffer);
        uint32_t NumberLastClientVersion (void);
        uint32_t LastClientEncryptionStyle(void);
};

typedef struct _OutputUpdate
{
    struct _OutputUpdate *pNext;
    struct _OutputUpdate *pPrev;

    uint32_t u32TargetClientId;

    int       iCode;
    void     *pvData;
    unsigned  cbData;

} OutputUpdate;

class OutputQueue
{
    public:
        OutputQueue (void);
        ~OutputQueue ();

        bool IsEmpty (void)
        {
            /* Atomically compare the queue head with NULL. */
            return ASMAtomicCmpXchgPtr (&m_pHead, NULL, NULL);
        }

        OutputUpdate *CreateUpdate (int iCode, uint32_t u32TargetClientId, const void *pvData, unsigned cbData);
        void DeleteUpdate (OutputUpdate *pUpdate);

        void InsertUpdate (OutputUpdate *pUpdate);
        OutputUpdate *Read (void);

    private:
        OutputUpdate *m_pHead;
        OutputUpdate *m_pTail;

        VRDPLock *m_pOutputQueueLock;
};

#define VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ (22050)

#define VRDP_AUDIO_CHUNK_TIME_MS (200)

/* Size of a chunk buffer that accumulates mixeng 22KHz data.
 *     (samples_per_sec * chunk_time_millisec) / millisec_per_sec.
 */
#define VRDP_AUDIO_CHUNK_SIZE_SAMPLES(__freq) (((__freq) * VRDP_AUDIO_CHUNK_TIME_MS) / 1000)

/* Chunk are maintained as a linked list. After sending to clients,
 * the audio chunks are deleted. Chunks are send to clients with a
 * delay equal to the chunk duration.
 */
typedef struct _VRDPAudioChunk
{
    struct _VRDPAudioChunk *pNext;
    struct _VRDPAudioChunk *pPrev;

    /* When chunk was created. */
    uint64_t u64CreatedTS;

    /* When the chunk is to be played. The chunk is sent to clients after CHUNK_TIME. */
    uint64_t u64StartTS;

    /* When the chunk samples started to play as a nanoseconds timestamp. */
    uint64_t u64SamplesStartNanoTS;

    /* Index in the 'aSamples' where to write. */
    int iWriteIdx;

    /* The frequency of the samples. */
    int iFreq;

    /* The number of the samples. */
    int cSamples;

    bool fFirst;

    /* Audio data, the actual size depends on iFreq. */
    st_sample_t astSamples[1];
} VRDPAudioChunk;

typedef struct VRDPAUDIORATECTX
{
    void *rate;
} VRDPAUDIORATECTX;

typedef struct _VRDPServerAudioData
{
    VRDPLock *m_pAudioDataLock;

    VRDPAudioChunk *pChunksHead;
    VRDPAudioChunk *pChunksTail;

    bool fInitialized;

    bool fFinished;
    uint64_t u64FinishedTS;

    int iDstFreq;

    uint32_t u32RateCorrectionMode;
    bool fRateStarted;
    uint64_t u64RateSamplesStartNS;
    uint64_t u64RateSamplesNS;

    VRDPAUDIORATECTX rate;

    int aDstFreqSamples[8];
    int iNextDstFreqSample;
    int cFreqSamples;

    uint32_t volatile cLastPackets;
    int32_t volatile iFreqDelta;

    st_sample_t astSamples[2 * VRDP_AUDIO_CHUNK_SIZE_SAMPLES(VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ)];
} VRDPServerAudioData;

#define VRDP_OF_STATUS_NONE             0
#define VRDP_OF_STATUS_REDRAW_PENDING   1
#define VRDP_OF_STATUS_REDRAW_COMPLETED 2

#define VHSTATCALL(c, f, a) do { VHSTAT *pStat = (c)->Stat(); if (LogRelIs6Enabled()) pStat->pStat->f a; } while (0)

typedef struct VHSTAT
{
    VHStat *pStat;
} VHSTAT;

class VRDPServer: public VRDPDesktop, public VRDPTransportServer
{
    private:

       VRDECALLBACKS_4 m_ApplicationCallbacks; /* Copy of the callbacks structure. */
       const VRDECALLBACKS_4 *m_pApplicationCallbacks; /* Points to m_ApplicationCallbacks. */
       void *m_pvApplicationCallback;

       bool volatile m_fConnectionsEnabled;

       RTTHREAD m_inputThread;
       RTTHREAD m_outputThread;

       RTSEMEVENT m_hEvtInput;          /**< Sempahore the input thread "sleeps" on. */
       RTSEMEVENTMULTI m_outputsem;

       VRDPTransport *m_pTransport;

       unsigned m_port;
       char *m_pszAddress;
       char *m_pszPortRange;
       uint32_t m_u32BindPort;

       char *m_pszUnixSocketPath;

       int m_mouseButtons;
       int m_mousex;
       int m_mousey;

       bool m_fKeyboardExtension;

       bool volatile m_fShutdownThreads;

       int StartThread (RTTHREAD *pthread, PFNRTTHREAD func, const char *pszThreadName);

       void notifyThreadStarted (RTTHREAD self, VRDPServerThreadStartCtx *pCtx, int rc);

       void ShutdownThreads (void);

       VRDPLock *m_pServerLock;

       VRDPPOINTERCACHE m_pointerCache;

       /* The bitmap cache is processed only by OUTPUT thread. */
       PBMPCACHE m_pbc;

       /* The text cache is processed only by OUTPUT thread. */
       PTEXTCACHE m_ptc;

       OutputQueue m_outputQueue;

       VRDPClientArray m_clientArray;

       uint32_t u32ClientIdSrc;

       uint32_t clientGenId (void);

       /* Output timer callbacks. */
       static DECLCALLBACK(uint64_t) vrdpOutputCallbackFlush (VRDPServer *pServer, uint64_t u64EventTS);
       static DECLCALLBACK(uint64_t) vrdpOutputCallbackAudio (VRDPServer *pServer, uint64_t u64EventTS);
       static DECLCALLBACK(uint64_t) vrdpOutputCallbackVideo (VRDPServer *pServer, uint64_t u64EventTS);

       /* Audio. */
       VRDPServerAudioData m_AudioData;

       void audioInit (void);
       void audioUninit (void);

       bool audioLock (void);
       void audioUnlock (void);

       bool audioIsLocked (void);

       VRDPAudioChunk *audioChunkAlloc (size_t cbChunk);
       void audioChunkFree (VRDPAudioChunk *pChunk);

       void audioChunkAppend (const uint64_t u64NewChunkStart, int iFreq, uint64_t u64NewChunkSamplesStartNanoTS);
       void audioChunkWriteSamples (const st_sample_t **ppSamples, int32_t *pcSamples, const VRDEAUDIOFORMAT format);
       void audioAppendSilence (const int64_t i64Duration);
       void audioAppendSamples (const st_sample_t *pSamples, int32_t cSamples, const VRDEAUDIOFORMAT format);

       uint64_t audioTimerHandler (uint64_t u64EventTS);
       void audioOutputChunk (VRDPAudioChunk *pChunk, bool fFinished);

       int audioClientQueueCorrection(void);

#if ARCH_BITS == 32
       /** 32-bit gcc aligns uint64_t on a 4 byte instead of 8 byte boundrary,
        * causing ASMAtomicReadU64 to assert. */
       uint32_t m_u32Padding;
#endif
       /* Variable update frequency. */
       volatile uint64_t m_u64LastInputTS;

       VHCONTEXT *m_pVideoHandler;

       int videoHandlerInit(void);
       void videoHandlerUninit(void);

       bool isInputActivity (uint64_t u64NowTS);
       void notifyInputActivity (void);

       volatile uint32_t mu32ClipboardDataWriteClientId;

       /** @thread VM */
       VRDPFBINFO *m_paFBInfos;
       unsigned m_cMonitors;

       /* Status of the Shift, Alt, Ctrl keys. */
       VRDPKeyboardModifiers m_KeyboardModifiers;

       uint32_t volatile m_u32OrderFallbackStatus;

       VRDEImageInterfaceData m_imageInterface;

       int internalStart (void);

       static DECLCALLBACK(void) audioInputCallback(void *pvCallback, void *pvCtx, uint32_t u32ClientId, uint32_t u32Event,
                                                    const void *pvData, uint32_t cbData);

       VHSTAT m_stat;

       VRDPSCard m_scard;

       VRDPTSMF m_tsmf;

       VRDPVideoIn m_videoin;
       VRDPInput m_input;

    public:

       VRDPServer (int callbackVersion, const VRDEINTERFACEHDR *pCallbackHdr, void *pvCallback);
       virtual ~VRDPServer ();

       int Enter (void);
       void Exit (void);

       int Start (void);
//       void Stop (void);

       int InputThread (RTTHREAD self, VRDPServerThreadStartCtx *pCtx);
       int OutputThread (RTTHREAD self, VRDPServerThreadStartCtx *pCtx);

       /* VRDPTransportServer method */
       virtual uint64_t GetKeepAliveTimeout (void);
       virtual int TransportConnect (VRDPTRANSPORTID id);
       virtual void TransportDisconnect(VRDPTRANSPORTID id);
       virtual int TransportEvent (VRDPTRANSPORTID id, uint32_t fu32Events);
       virtual int QueryFeature (const char *pszName, char **ppszValue, uint32_t *pcbOut);
       virtual bool IsShutdown (void) { return m_fShutdownThreads; };
       virtual bool IsOutput(void) { return IsOutputThread(); };
       virtual bool IsInput(void) { return IsInputThread(); };

       /* VRDPDesktop methods */
       virtual void ProcessInput (unsigned uScreenId, VRDPDesktop::InputMsg *pmsg);

       virtual int ClientAttach (VRDPClient *pClient, bool fAttached);
       virtual int ClientLogon (VRDPClient *pClient, const char *pszUser, const char *pszPassword, const char *pszDomain);
       virtual int ClientLocationInfo (VRDPClient *pClient,const char *pszCName, const char *pszCIPAddr, const char *pszCLocation, const char *pszCOtherInfo);
       virtual int InterceptChannel (VRDPClient *pClient, uint32_t fu32Intercept, void **ppvIntercept);
       virtual void NotifyClientResolution (int w, int h, unsigned uScreenId);

       void ClientConnect (VRDPClient *pClient);
       void ClientDisconnect (VRDPClient *pClient);

       void RaiseOutputEvent (void);
       int PostOutput (int iCode, uint32_t u32TargetClientId, const void *pvData, unsigned cbData);
       int PostOutputEvent (int iCode, uint32_t u32TargetClientId, const void *pvData, unsigned cbData);

       bool FrameBufferQueryLock (VRDPBITSRECT *pBitsRect, RGNRECT *pRect, unsigned uScreenId);

       void ProcessUpdateComplete (void);
       void ProcessResize (void);
       void ProcessBitmapUpdate (unsigned uScreenId, unsigned x, unsigned y, unsigned w, unsigned h);
       void ProcessOutputUpdate (unsigned uScreenId, void *pvUpdate, unsigned cbUpdate);
       void ProcessPointer (const VRDEMOUSEPTRDATA *pPointer);

       void ProcessUSBSend (uint32_t u32ClientId, const void *pvParm, uint32_t cbParm);

       uint32_t PointerCacheQueryLatest (void);
       VRDEMOUSEPTRDATA *PointerCacheLock (uint32_t u32ServerId);
       void PointerCacheUnlock (uint32_t u32ServerId);

       bool IsInputThread (void) { return RTThreadSelf () == m_inputThread; }
       bool IsOutputThread (void) { return RTThreadSelf () == m_outputThread; }

       RTTHREAD InputThread (void) { return m_inputThread; }
       RTTHREAD OutputThread (void) { return m_outputThread; }

       void ProcessAudio (const void *pvSamples, uint32_t cSamples, VRDEAUDIOFORMAT format);
       void ProcessClipboard (uint32_t u32Function, uint32_t u32Format, void *pvData, uint32_t cbData, uint32_t *pcbActualRead);
       bool NotifyClipboardDataRequest (uint32_t u32ClientId);
       void NotifyClipboardFormatAnnounce (uint32_t u32ClientId);

       void QueryInfo (uint32_t index, void *pvBuffer, uint32_t cbBuffer, uint32_t *pcbOut);

       const VRDECALLBACKS_4 *ApplicationCallbacks(void) { return m_pApplicationCallbacks; };
       void *ApplicationCallbackPointer (void) { return m_pvApplicationCallback; };

       int EnableConnections (bool fEnable);
       int Disconnect (uint32_t u32ClientId, bool fReconnect);
       int Redirect (uint32_t u32ClientId, const char *pszServer, const char *pszUser, const char *pszDomain, const char *pszPassword, uint32_t u32SessionId, const char *pszCookie);

       static void PostVideoEvent(void *pvContext, int iCode, const void *pvData, uint32_t cbData);
       VHCONTEXT *VideoHandler() {return m_pVideoHandler; };
       uint64_t VideoHandlerOutput(uint64_t u64EventTS);
       void VideoHandlerStreamEvent(uint32_t u32VideoStreamId);
       void VideoHandlerSourceStreamEnd(uint32_t u32VideoStreamId);
       void ProcessOutputEvent (OutputUpdate *pUpdate);
       void VideoHandlerReset(void);
       void VideoHandlerCompressionRate(uint32_t u32VideoQuality);
       void VideoHandlerSizeLimit(VHEVENTSIZELIMIT *pData);
       void VideoHandlerStreamRestart(void);

       PBMPCACHE BC(void) { return m_pbc; };

       void ProcessAudioInOpen (void *pvCtx, uint32_t u32ClientId, VRDEAUDIOFORMAT audioFormat, uint32_t u32SamplesPerBlock);
       void ProcessAudioInClose (uint32_t u32ClientId);

       int GetInterface (const char *pszId,
                         VRDEINTERFACEHDR *pInterface,
                         const VRDEINTERFACEHDR *pCallbacks,
                         void *pvContext);

       int ImageHandleCreate (VRDPImage **ppImage,
                              void *pvUser,
                              uint32_t u32Screen,
                              uint32_t fu32Flags,
                              const RTRECT *pRect,
                              const char *pszFormatId,
                              const void *pvFormat,
                              uint32_t cbFormat,
                              uint32_t *pu32CompletionFlags);

       bool SelectSecurityProtocol(uint32_t u32RequestedProtocols,
                                   uint32_t *u32ResponseCode);

       VHSTAT *Stat(void) { return &m_stat; }

       VRDPSCard *SCard(void) { return &m_scard; }
       VRDPTSMF *TSMF(void) { return &m_tsmf; }
       VRDPVideoIn *VideoIn(void) { return &m_videoin; }
       VRDPInput *Input(void) { return &m_input; }

       void AudioNotifyClientQueue(VRDPClient *pClient, uint32_t cPackets);
};

int appProperty (const VRDECALLBACKS_4 *pCallbacks,
                      void *pvCallback,
                      uint32_t index,
                      void *pvBuffer,
                      uint32_t cbBuffer,
                      uint32_t *pcbOut);

int appFeature (const VRDECALLBACKS_4 *pCallbacks,
                void *pvCallback,
                const char *pszName,
                char **ppszValue,
                uint32_t *pcbOut);

/*
 * Helpers for getting interface function tables.
 */
int vrdpGetInterfaceImage(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks);
int vrdpGetInterfaceMousePtr(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks);

#endif /* !VRDP_INCLUDED_SRC_vrdpserv_h */
