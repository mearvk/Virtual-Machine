/* $Id: audioin.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
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

#include "audioin.h"
#include "vrdpclient.h"
#include "vrdpserv.h"

// #define LOGWAV
#ifdef LOGWAV
static FILE *f;

#pragma pack(1)
typedef struct _WavFileHdr
{
    uint32_t u32RIFF;
    uint32_t u32Size;
    uint32_t u32WAVE;

    uint32_t u32Fmt;
    uint32_t u32Size1;
    uint16_t u16AudioFormat;
    uint16_t u16NumChannels;
    uint32_t u32SampleRate;
    uint32_t u32ByteRate;
    uint16_t u16BlockAlign;
    uint16_t u16BitsPerSample;

    uint32_t u32Data;
    uint32_t u32Size2;
} WavFileHdr;
#pragma pack()

static WavFileHdr wavhdr;

static void logwavstart(uint32_t freq, uint16_t ch, uint16_t bits)
{
    wavhdr.u32RIFF = 'FFIR';
    wavhdr.u32Size = 36;
    wavhdr.u32WAVE = 'EVAW';

    wavhdr.u32Fmt = ' tmf';
    wavhdr.u32Size1 = 16;
    wavhdr.u16AudioFormat = 1;
    wavhdr.u16NumChannels = ch;
    wavhdr.u32SampleRate = freq;
    wavhdr.u32ByteRate = freq * ch * bits / 8;
    wavhdr.u16BlockAlign = ch * bits / 8;
    wavhdr.u16BitsPerSample = bits;

    wavhdr.u32Data = 'atad';
    wavhdr.u32Size2 = 0;

    f = fopen ("wav\\vrdp-in.wav", "wb");

    if (f)
    {
        fwrite (&wavhdr, 1, sizeof (wavhdr), f);
    }
}
static void logwavend(void)
{
   if (f)
   {
       fseek (f, 0, SEEK_SET);
       fwrite (&wavhdr, 1, sizeof (wavhdr), f);
       fclose(f);
       f = NULL;
   }
}
static void logwav(const void *pv, size_t cb)
{
   if (f)
   {
       fwrite (pv, 1, cb, f);

       wavhdr.u32Size  += cb;
       wavhdr.u32Size2 += cb;
   }
}
#else
#define logwavstart(a, b, c) do {} while (0)
#define logwavend() do {} while (0)
#define logwav(__a, __b) do {} while (0)
#endif /* LOGWAV */


/* Parameters for VRDP_OUTPUT_AUDIOINPUT_OPEN event. */
typedef struct AUDIOINPUTOPENPARM
{
    void *pvCtx;
    VRDEAUDIOFORMAT audioFormat;
    uint32_t u32SamplesPerBlock;
    PFNAUDIOINPUT pfn;
    void *pv;
} AUDIOINPUTOPENPARM;

/* Parameters for VRDP_OUTPUT_AUDIOINPUT_CLOSE event. */
#define AUDIO_INPUT_CLOSE_EXTERNAL 0
#define AUDIO_INPUT_CLOSE_INTERNAL 1

typedef struct AUDIOINPUTCLOSEPARM
{
    uint32_t u32Reason;
    int32_t rc;
} AUDIOINPUTCLOSEPARM;

/*
 * The AUDIO_INPUT channel.
 */
#define MSG_SNDIN_VERSION       0x01
#define MSG_SNDIN_FORMATS       0x02
#define MSG_SNDIN_OPEN          0x03
#define MSG_SNDIN_OPEN_REPLY    0x04
#define MSG_SNDIN_DATA_INCOMING 0x05
#define MSG_SNDIN_DATA          0x06
#define MSG_SNDIN_FORMATCHANGE  0x07

/* VRDP server will use PCM format.  */
#define SNDIN_WAVE_FORMAT_PCM 0x0001

#pragma pack(1)
typedef struct SNDIN_PDU
{
    uint8_t MessageId;
} SNDIN_PDU;

typedef struct SNDIN_VERSION
{
    SNDIN_PDU Header;
    uint32_t Version;
} SNDIN_VERSION;

typedef struct SNDIN_FORMATS
{
    SNDIN_PDU Header;
    uint32_t NumFormats;
    uint32_t cbSizeFormatsPacket;
    /* SoundFormats, SNDIN_AUDIO_FORMAT structures. */
    /* ExtraData, arbitrary, must be ignored by recipient. */
} SNDIN_FORMATS;

typedef struct SNDIN_OPEN
{
    SNDIN_PDU Header;
    uint32_t FramesPerPacket;
    uint32_t initialFormat;
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
    /* WAVEFORMAT_EXTENSIBLE ExtraFormatData cbSize bytes. */
} SNDIN_OPEN;

typedef struct SNDIN_OPEN_REPLY
{
    SNDIN_PDU Header;
    uint32_t Result;
} SNDIN_OPEN_REPLY;

typedef struct SNDIN_DATA_INCOMING
{
    SNDIN_PDU Header;
} SNDIN_DATA_INCOMING;

typedef struct SNDIN_DATA
{
    SNDIN_PDU Header;
    /* Data */
} SNDIN_DATA;

typedef struct SNDIN_FORMATCHANGE
{
    SNDIN_PDU Header;
    uint32_t NewFormat;
} SNDIN_FORMATCHANGE;
#pragma pack()


AudioInputClient::AudioInputClient (VRDPClient *pClient)
    :
    m_pClient (pClient),
    m_u8ActiveDVCChannelId(0)
{
    RTListInit(&m_listSessions);
    VRDPLock::Create("AudioInput", &m_pLock);
}

AudioInputClient::~AudioInputClient ()
{
    AUDIOIN_SESSION *pIter;
    AUDIOIN_SESSION *pNext;
    RTListForEachSafe(&m_listSessions, pIter, pNext, AUDIOIN_SESSION, nodeSession)
    {
        RTListNodeRemove(&pIter->nodeSession);
        VRDPMemFree(pIter);
    }
    VRDPLock::Delete(&m_pLock);
}

/** @thread INPUT */
/* static */ void AudioInputClient::dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    AUDIOIN_SESSION *pSession = (AUDIOIN_SESSION *)pvCtx;

    pSession->pAudioInputClient->processInput(pSession, u32Event, pvData, cbData);
}

typedef struct SENDCONTEXT
{
    VRDPClient *pClient;
    bool       fOutputThread;
    uint8_t    u8ChannelId;
} SENDCONTEXT;

static void initSendContext(SENDCONTEXT *pSend,
                            bool fOutputThread,
                            VRDPClient *pClient,
                            uint8_t u8ChannelId)
{
    pSend->fOutputThread = fOutputThread;
    pSend->pClient       = pClient;
    pSend->u8ChannelId   = u8ChannelId;
}

static void sendPDU (SENDCONTEXT *pSend,
                     void *pvData,
                     uint32_t cbData)
{
    if (pSend->fOutputThread)
    {
        pSend->pClient->TP()->DVC()->SendData (pSend->u8ChannelId,
                                               pvData,
                                               cbData);
    }
    else
    {
        size_t cbTmpBuffer = sizeof(pSend->u8ChannelId) + cbData;
        uint8_t *pu8TmpBuffer = (uint8_t *)VRDPMemAlloc(cbTmpBuffer);

        if (pu8TmpBuffer != NULL)
        {
            pu8TmpBuffer[0] = pSend->u8ChannelId;
            memcpy(&pu8TmpBuffer[1], pvData, cbData);

            pSend->pClient->Server()->PostOutput (VRDP_OUTPUT_AUDIOINPUT_DATA,
                                                  pSend->pClient->Id (),
                                                  pu8TmpBuffer,
                                                  (uint32_t)cbTmpBuffer);
            VRDPMemFree(pu8TmpBuffer);
        }
    }
}

void AudioInputClient::audioCallback(AUDIOIN_SESSION *pSession, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
   if (pSession->pfnAudioInputCallback)
   {
       pSession->pfnAudioInputCallback(pSession->pvAudioInputCallback,
                                       pSession->pvCtx, m_pClient->Id(), u32Event, pvData, cbData);
   }
}

/** @thread any */
void AudioInputClient::postAudioInputClose(uint32_t u32Reason, int32_t rc)
{
    AUDIOINPUTCLOSEPARM parm;

    parm.u32Reason = u32Reason;
    parm.rc = rc;

    m_pClient->Server()->PostOutput (VRDP_OUTPUT_AUDIOINPUT_CLOSE,
                                     m_pClient->Id (),
                                     &parm,
                                     sizeof(parm));
}

/** @thread INPUT */
void AudioInputClient::processInput(AUDIOIN_SESSION *pSession, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    AILOG(("pSession %p, u32Event %d, pvData %p, cbData %d\n", pSession, u32Event, pvData, cbData));

    SENDCONTEXT sendContext;
    bool fOutputThread = false;

    initSendContext(&sendContext, fOutputThread, m_pClient, pSession->u8DVCChannelId);

    switch (u32Event)
    {
        case DVC_EVENT_CREATE:
        {
            /* Client accepted the channel. Server must send the version PDU. */
            SNDIN_VERSION pdu;
            pdu.Header.MessageId = MSG_SNDIN_VERSION;
            pdu.Version = 1;

            sendPDU (&sendContext, &pdu, sizeof (SNDIN_VERSION));
        } break;

        case DVC_EVENT_DATA:
        {
#ifdef DEBUG_sunlover
            AILOG(("AUDIO_INOUT dvc data:\n"));
            hexdump((uint8_t *)pvData, cbData);
#endif
            if (cbData < sizeof(SNDIN_PDU))
            {
                AILOG(("cbData < SNDIN_PDU!!!\n"));
                break;
            }

            const SNDIN_PDU *pHdr = (SNDIN_PDU *)pvData;

            AILOG(("received MessageId %d\n", pHdr->MessageId));

            switch (pHdr->MessageId)
            {
                case MSG_SNDIN_VERSION:
                {
                    if (cbData < sizeof(SNDIN_VERSION))
                    {
                        AILOG(("cbData < SNDIN_VERSION!!!\n"));
                        break;
                    }

                    const SNDIN_VERSION *pPDU = (SNDIN_VERSION *)pvData;

                    AILOG(("SNDIN_VERSION: 0x%x\n", pPDU->Version)); RT_NOREF_PV(pPDU);

                    /* Send Sound Formats PDU.
                     * @todo Here it is possible to make a list of formats, including the requested one.
                     *       Then choose most appropriate format from the list received in MSG_SNDIN_FORMATS.
                     */
#pragma pack(1)
                    struct {
                        SNDIN_FORMATS hdr;
                        SNDIN_AUDIO_FORMAT fmt;
                    } pdu;
#pragma pack()
                    pdu.hdr.Header.MessageId = MSG_SNDIN_FORMATS;
                    pdu.hdr.NumFormats = 1;
                    pdu.hdr.cbSizeFormatsPacket = 0;

                    pdu.fmt = pSession->fmt;

                    sendPDU (&sendContext, &pdu, sizeof (pdu));
                } break;

                case MSG_SNDIN_FORMATS:
                {
                    if (cbData < sizeof(SNDIN_FORMATS))
                    {
                        AILOG(("cbData < SNDIN_FORMATS!!!\n"));
                        break;
                    }

                    const SNDIN_FORMATS *pPDU = (SNDIN_FORMATS *)pvData;

                    AILOG(("SNDIN_FORMATS: NumFormats %d, cbPacket %d\n",
                           pPDU->NumFormats, pPDU->cbSizeFormatsPacket));

                    const SNDIN_AUDIO_FORMAT *pFormat = (SNDIN_AUDIO_FORMAT *)((uint8_t *)pPDU + sizeof (SNDIN_FORMATS));
                    size_t cbLeft = cbData - sizeof(SNDIN_FORMATS);

                    unsigned i;
                    for (i = 0; i < pPDU->NumFormats; i++)
                    {
                        if (cbLeft < sizeof (SNDIN_AUDIO_FORMAT))
                        {
                            AILOG(("cbLeft < SNDIN_AUDIO_FORMATS for format %d!!!\n", i));
                            break;
                        }

                        AILOG(("[%d]: tag 0x%04x, ch %d, sps %5d, abps %5d, ba %d, bits %2d, cb %d\n",
                               i,
                               pFormat->wFormatTag,
                               pFormat->nChannels,
                               pFormat->nSamplesPerSec,
                               pFormat->nAvgBytesPerSec,
                               pFormat->nBlockAlign,
                               pFormat->wBitsPerSample,
                               pFormat->cbSize));

                        cbLeft -= sizeof (SNDIN_AUDIO_FORMAT);

                        if (pFormat->cbSize > 0)
                        {
                            if (cbLeft < pFormat->cbSize)
                            {
                                AILOG(("cbLeft < SNDIN_AUDIO_FORMATS for format %d!!!\n", i));
                                break;
                            }
                            cbLeft -= pFormat->cbSize;
                        }

                        pFormat++;
                    }

                    /*
                     * Start recording.
                     */
                    SNDIN_OPEN pdu;

                    pdu.Header.MessageId = MSG_SNDIN_OPEN;
                    pdu.initialFormat   = 0;
                    pdu.wFormatTag      = pSession->fmt.wFormatTag;
                    pdu.nChannels       = pSession->fmt.nChannels;
                    pdu.nSamplesPerSec  = pSession->fmt.nSamplesPerSec;
                    pdu.nAvgBytesPerSec = pSession->fmt.nAvgBytesPerSec;
                    pdu.nBlockAlign     = pSession->fmt.nBlockAlign;
                    pdu.wBitsPerSample  = pSession->fmt.wBitsPerSample;
                    pdu.cbSize = 0;

                    pdu.FramesPerPacket = pSession->u32SamplesPerBlock;

                    sendPDU (&sendContext, &pdu, sizeof (pdu));
                } break;

                case MSG_SNDIN_OPEN:
                {
                    AILOG(("SNDIN_OPEN is not expected from client!!!\n"));
                } break;

                case MSG_SNDIN_OPEN_REPLY:
                {
                    if (cbData < sizeof(SNDIN_OPEN_REPLY))
                    {
                        AILOG(("cbData < SNDIN_OPEN_REPLY!!!\n"));
                        break;
                    }

                    const SNDIN_OPEN_REPLY *pPDU = (SNDIN_OPEN_REPLY *)pvData;

                    AILOG(("SNDIN_OPEN_REPLY: 0x%x\n", pPDU->Result));

                    if (pPDU->Result != 0)
                    {
                        VRDPLOGRELLIMIT(16, ("AUDIO_INPUT: SNDIN_OPEN_REPLY Result 0x%08X\n", pPDU->Result));
                        postAudioInputClose(AUDIO_INPUT_CLOSE_INTERNAL, VERR_NOT_SUPPORTED);
                    }
                    else
                    {
                        VRDEAUDIOINBEGIN parms;
                        parms.fmt = VRDE_AUDIO_FMT_MAKE(pSession->fmt.nSamplesPerSec, pSession->fmt.nChannels,
                                                        pSession->fmt.wBitsPerSample, 0);

                        audioCallback(pSession, VRDE_AUDIOIN_BEGIN, &parms, sizeof(parms));

                        logwavstart(pSession->fmt.nSamplesPerSec, pSession->fmt.nChannels, pSession->fmt.wBitsPerSample);
                    }
                } break;

                case MSG_SNDIN_DATA_INCOMING:
                {
                    if (cbData < sizeof(SNDIN_DATA_INCOMING))
                    {
                        AILOG(("cbData < SNDIN_DATA_INCOMING!!!\n"));
                        break;
                    }

                    AILOG(("SNDIN_DATA_INCOMING\n"));
                } break;

                case MSG_SNDIN_DATA:
                {
                    if (cbData < sizeof(SNDIN_DATA))
                    {
                        AILOG(("cbData < SNDIN_DATA!!!\n"));
                        break;
                    }

                    const uint8_t *pu8Audio = (uint8_t *)pvData + sizeof(SNDIN_DATA);
                    uint32_t cbAudio = cbData - sizeof(SNDIN_DATA);

                    AILOG(("SNDIN_DATA: %d bytes\n", cbAudio));

                    logwav(pu8Audio, cbAudio);

                    audioCallback(pSession, VRDE_AUDIOIN_DATA, pu8Audio, cbAudio);
                } break;

                case MSG_SNDIN_FORMATCHANGE:
                {
                    if (cbData < sizeof(SNDIN_FORMATCHANGE))
                    {
                        AILOG(("cbData < SNDIN_FORMATCHANGE!!!\n"));
                        break;
                    }

                    const SNDIN_FORMATCHANGE *pPDU = (SNDIN_FORMATCHANGE *)pvData;

                    AILOG(("SNDIN_FORMATCHANGE: NewFormat %d\n",
                           pPDU->NewFormat)); RT_NOREF_PV(pPDU);

                    /* Currently there is only 1 format, so nothing to do. */
                } break;

                default:
                {
                    AILOG(("wrong MessageId!!!\n"));
                } break;
            }
        } break;

        case DVC_EVENT_CLOSE:
        {
           logwavend();

           audioCallback(pSession, VRDE_AUDIOIN_END, NULL, 0);

           /* The channel is not used anymore. */
           AILOG(("closing session dvcch %d\n", pSession->u8DVCChannelId));
           VRDPLock::Lock(m_pLock);
           RTListNodeRemove(&pSession->nodeSession);
           VRDPLock::Unlock(m_pLock);

           RT_ZERO(*pSession);
           VRDPMemFree(pSession);
        } break;

        default:
            AssertFailed();
    }
}

/** @thread OUTPUT */
int AudioInputClient::audioInputStart(void *pvCtx, VRDEAUDIOFORMAT audioFormat, uint32_t u32SamplesPerBlock, PFNAUDIOINPUT pfn, void *pv)
{
    if (m_u8ActiveDVCChannelId)
    {
        /* Current session should be already closed at this point. */
        AILOG(("old session is not yet closed dvcch %d!!!\n", m_u8ActiveDVCChannelId));
        AssertFailed();
        return VERR_INVALID_STATE;
    }

    uint16_t cBitsPerSample = (uint16_t)VRDE_AUDIO_FMT_BITS_PER_SAMPLE(audioFormat);
    uint16_t cChannels = (uint16_t)VRDE_AUDIO_FMT_CHANNELS(audioFormat);

    if (   (cChannels != 1 && cChannels != 2)
        || (cBitsPerSample != 8 && cBitsPerSample != 16)
       )
    {
        return VERR_INVALID_PARAMETER;
    }

    AUDIOIN_SESSION *pSession = (AUDIOIN_SESSION *)VRDPMemAlloc(sizeof(AUDIOIN_SESSION));
    if (!pSession)
    {
        return VERR_NO_MEMORY;
    }

    RT_ZERO(pSession->nodeSession);
    pSession->pAudioInputClient = this;

    pSession->fmt.wFormatTag      = SNDIN_WAVE_FORMAT_PCM;
    pSession->fmt.nChannels       = cChannels;
    pSession->fmt.nSamplesPerSec  = VRDE_AUDIO_FMT_SAMPLE_FREQ(audioFormat);
    pSession->fmt.wBitsPerSample  = cBitsPerSample;
    pSession->fmt.nBlockAlign     = pSession->fmt.nChannels * pSession->fmt.wBitsPerSample / 8;
    pSession->fmt.nAvgBytesPerSec = pSession->fmt.nSamplesPerSec * pSession->fmt.nBlockAlign;
    pSession->fmt.cbSize          = 0;

    pSession->pfnAudioInputCallback = pfn;
    pSession->pvAudioInputCallback  = pv;
    pSession->pvCtx                 = pvCtx;
    pSession->u32SamplesPerBlock    = u32SamplesPerBlock;
    pSession->u8DVCChannelId        = 0;

    int rc = m_pClient->TP()->DVC ()->RegisterChannel("AUDIO_INPUT",
                                                      AudioInputClient::dvcInput,
                                                      pSession,
                                                      &pSession->u8DVCChannelId);

    if (RT_SUCCESS(rc))
    {
        m_u8ActiveDVCChannelId = pSession->u8DVCChannelId;

        VRDPLock::Lock(m_pLock);
        RTListAppend(&m_listSessions, &pSession->nodeSession);
        VRDPLock::Unlock(m_pLock);
    }
    else
    {
        VRDPMemFree(pSession);
    }

    return rc;
}

/** @thread OUTPUT */
void AudioInputClient::OutputAudioInputOpen(void *pvData, uint32_t cbData)
{
    RT_NOREF1(cbData);
    const AUDIOINPUTOPENPARM *pParm = (AUDIOINPUTOPENPARM *)pvData;

    int rc = audioInputStart (pParm->pvCtx, pParm->audioFormat, pParm->u32SamplesPerBlock, pParm->pfn, pParm->pv);

    if (RT_FAILURE(rc))
    {
        if (pParm->pfn)
        {
            pParm->pfn(pParm->pv, pParm->pvCtx, m_pClient->Id(), VRDE_AUDIOIN_END, NULL, 0);
        }
    }
}

/** @thread OUTPUT */
void AudioInputClient::OutputAudioInputData(void *pvData, uint32_t cbData)
{
    if (cbData <= 1)
    {
        AssertFailed();
        return;
    }

    uint8_t *pu8Data = (uint8_t *)pvData;
    uint8_t u8ChannelId = pu8Data[0];

    if (u8ChannelId != 0)
    {
        m_pClient->TP()->DVC()->SendData(u8ChannelId, &pu8Data[1], cbData - 1);
    }
}

/** @thread OUTPUT */
void AudioInputClient::OutputAudioInputClose(void *pvData, uint32_t cbData)
{
    const AUDIOINPUTCLOSEPARM *pParm = (AUDIOINPUTCLOSEPARM *)pvData;
    RT_NOREF2(pParm, cbData);

    uint8_t u8DVCChannelId = m_u8ActiveDVCChannelId;
    m_u8ActiveDVCChannelId = 0;

    if (u8DVCChannelId != 0)
    {
        m_pClient->TP()->DVC()->SendCloseChannel(u8DVCChannelId);
    }
    else
    {
        AILOG(("current session already closed!!!\n"));
    }
}


/*
 * VRDPServer methods.
 */

void VRDPServer::ProcessAudioInOpen (void *pvCtx, uint32_t u32ClientId, VRDEAUDIOFORMAT audioFormat, uint32_t u32SamplesPerBlock)
{
    AILOG(("ProcessAudioInOpen: ctx %p, block %d, client %d, fmt %d,%d,%d,%d\n",
            pvCtx,
            u32SamplesPerBlock,
            u32ClientId,
            VRDE_AUDIO_FMT_SAMPLE_FREQ(audioFormat),
            VRDE_AUDIO_FMT_CHANNELS(audioFormat),
            VRDE_AUDIO_FMT_SIGNED(audioFormat),
            VRDE_AUDIO_FMT_BITS_PER_SAMPLE(audioFormat)));

    AUDIOINPUTOPENPARM parm;

    parm.audioFormat = audioFormat;
    parm.pvCtx = pvCtx;
    parm.u32SamplesPerBlock = u32SamplesPerBlock;

    parm.pfn = audioInputCallback;
    parm.pv = this;

    PostOutput (VRDP_OUTPUT_AUDIOINPUT_OPEN, u32ClientId, &parm, sizeof(parm));

    RaiseOutputEvent ();
}

void VRDPServer::ProcessAudioInClose (uint32_t u32ClientId)
{
    AILOG(("ProcessAudioInClose: client %d\n",
            u32ClientId));

    AUDIOINPUTCLOSEPARM parm;

    parm.u32Reason = AUDIO_INPUT_CLOSE_EXTERNAL;
    parm.rc = VINF_SUCCESS;

    PostOutput (VRDP_OUTPUT_AUDIOINPUT_CLOSE, u32ClientId, &parm, sizeof (parm));

    RaiseOutputEvent ();
}

/* static */ DECLCALLBACK(void) VRDPServer::audioInputCallback(void *pvCallback,
                                                               void *pvCtx,
                                                               uint32_t u32ClientId,
                                                               uint32_t u32Event,
                                                               const void *pvData,
                                                               uint32_t cbData)
{
    VRDPServer *pServer = (VRDPServer *)pvCallback;

    const VRDECALLBACKS_4 *pCallbacks = pServer->ApplicationCallbacks ();

    if (pCallbacks && pCallbacks->VRDECallbackAudioIn)
    {
        pCallbacks->VRDECallbackAudioIn (pServer->ApplicationCallbackPointer (),
                                         pvCtx, u32ClientId, u32Event, pvData, cbData);
    }
}
