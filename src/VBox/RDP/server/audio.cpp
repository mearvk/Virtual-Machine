/* $Id: audio.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol -VRDPServer and VRDPTP methods related to audio output.
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

#include "vrdpserv.h"
#include "utils.h"

#include <iprt/path.h>

extern "C" {
#include "mixeng/mixeng.h"
}

#include <stdio.h>

#if 0 /* Detailed debugging. */
#define LOGTIMING
#undef Assert
#define Assert AssertRelease
#endif

/* Multiconnection remote audio.
 *
 * Clients, which have requested the sound channel, will receive the same
 * audio data. Therefore, the audio data processing is mostly done by the
 * server and the client instances only send these data.
 *
 * The server receives audio samples and stores them in 200ms chunks.
 * Each chunk has a timestamp, the time when the chunk has started.
 * A chunks is sent when the current time is later than the timestamp,
 * so the chunk will be completed during the delay.
 *
 * The chunk is then converted if necessary to internal format, that is
 * frequency convertion is applied. Incomplete chunks are padded with
 * zeros.
 *
 * The chunk in internal format is passed to clients. Therefore clients
 * receive chunks every 200 ms. When sound finishes, the NULL chunk
 * is sent to clients.
 *
 * The clients sends audio data in the format that was negotiated
 * with RDP client. Currently 22050Hz stereo 16 bit.
 *
 * Client converts the chunk data to the output format and stores the
 * data in a ring buffer. The converted data are sent in 8192 bytes blocks
 * when the second chunk arrives (including NULL chunk).
 * New data blocks are sent when a new chunk arrives.
 *
 * The OUTPUT and the VM thread are synchronized by the audio lock.
 * The server lock is not used.
 *
 */

#define RDPSND_CLOSE      1 /* SNDC_CLOSE */
#define RDPSND_WRITE      2 /* SNDC_WAVE */
#define RDPSND_SET_VOLUME 3 /* SNDC_SETVOLUME */
#define RDPSND_SET_PITCH  4 /* SNDC_SETPITCH */
#define RDPSND_COMPLETION 5 /* SNDC_WAVECONFIRM */
#define RDPSND_TRAINING   6 /* SNDC_TRAINING */
#define RDPSND_NEGOTIATE  7 /* SNDC_FORMATS */


static char *g_pszAudioLogPath = NULL;

#ifdef LOGTIMING
#define LOGTIMING_DECL(__name)                            \
static uint64_t u64##__name##TS;                          \
static void logtiming##__name(void)                       \
{                                                         \
    uint64_t u64NowTS = VRDPTimeMilliTS ();               \
    if (u64##__name##TS != 0)                             \
        LogRel(("T " #__name " ts = %lld, delta %lld\n",  \
                u64NowTS, u64NowTS - u64##__name##TS));   \
    u64##__name##TS = u64NowTS;                           \
}
LOGTIMING_DECL(ChunkCreated)
LOGTIMING_DECL(ChunkCompleted)
LOGTIMING_DECL(ChunkSent)
LOGTIMING_DECL(PacketSent)
#else
#define logtimingChunkCreated() do {} while (0)
#define logtimingChunkCompleted() do {} while (0)
#define logtimingChunkSent() do {} while (0)
#define logtimingPacketSent() do {} while (0)
#endif /* LOGTIMING */

/* Different audio processing options:
 *   0 - input audio resampled to 22Khz, the original old mode;
 *   1 - the target 22Khz audio corresponds to the real time;
 *   2 - low pass filter.
 *
 * The default bitmask of the audio processing options is:
 *   VRDP_AUDIO_MODE_RC | VRDP_AUDIO_MODE_LPF = 3
 */
#define VRDP_AUDIO_MODE_VOID  0x00  /* Nothing */
#define VRDP_AUDIO_MODE_RC    0x01  /* Rate correction. */
#define VRDP_AUDIO_MODE_LPF   0x02  /* Low Pass Filter */
#define VRDP_AUDIO_MODE_CS    0x04  /* Client Sync avoid under/overflow of the client queue. */


/*
 * Audio rate conversion.
 */
static void audioRateDestroy(VRDPAUDIORATECTX *pCtx)
{
    AUDIOLOG(("\n"));

    if (pCtx->rate)
    {
        st_rate_stop(pCtx->rate);
        pCtx->rate = NULL;
    }
}

static void audioRateStart(VRDPAUDIORATECTX *pCtx,
                           int iSrcFreq,
                           int iDstFreq,
                           bool fFilter)
{
    AUDIOLOG(("%d->%d\n", iSrcFreq, iDstFreq));

    /* This is either a new start or a reinit of existing rate conversion object. */
    if (pCtx->rate)
    {
        st_rate_update(pCtx->rate, iSrcFreq, iDstFreq);
        return;
    }

    fFilter = fFilter && (iSrcFreq > iDstFreq);

    AUDIOLOG(("fFilter %d\n", fFilter));

    pCtx->rate = st_rate_start (iSrcFreq, iDstFreq, fFilter);
}

static void audioRateUpdate(VRDPAUDIORATECTX *pCtx,
                            int iSrcFreq,
                            int iDstFreq)
{
    AUDIOLOG(("%d->%d\n", iSrcFreq, iDstFreq));

    if (pCtx->rate)
    {
        st_rate_update(pCtx->rate, iSrcFreq, iDstFreq);
    }
}

static void audioRateConvert(VRDPAUDIORATECTX *pCtx,
                             const st_sample_t *pSrcSamples,
                             int *pcSrcSamples,
                             st_sample_t *pDstSamples,
                             int *pcDstSamples)
{
    if (pCtx->rate)
    {
        /* Note: The st_rate_flow function takes care of *pcSamples, *pcDstSamples
         *       and converts as many samples as minimum of the counts.
         */
        st_rate_flow_ex(pCtx->rate,
                        (st_sample_t *)pSrcSamples, pDstSamples,
                        pcSrcSamples, pcDstSamples);

    }
}


/** @thread OUTPUT */
void VRDPServer::audioInit (void)
{
    m_AudioData.pChunksHead = NULL;
    m_AudioData.pChunksTail = NULL;

    m_AudioData.iDstFreq = VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ;

    /* Enable rate correction by default. */
    m_AudioData.u32RateCorrectionMode = VRDP_AUDIO_MODE_RC | VRDP_AUDIO_MODE_LPF;

    char *pszValue = NULL;
    uint32_t cbOut = 0;

    int rc = QueryFeature("Property/Audio/RateCorrectionMode", &pszValue, &cbOut);
    if (RT_SUCCESS(rc))
    {
        if (pszValue && *pszValue != '\0')
        {
            uint32_t u32Value = 0;
            rc = RTStrToUInt32Full(pszValue, 10, &u32Value);
            if (RT_SUCCESS(rc))
            {
                m_AudioData.u32RateCorrectionMode = u32Value;
            }
        }

        VRDPMemFree(pszValue);
    }

    VRDPLOGREL(("Audio: rate correction mode 0x%x.\n", m_AudioData.u32RateCorrectionMode));

    pszValue = NULL;
    cbOut = 0;
    rc = QueryFeature("Property/Audio/LogPath", &pszValue, &cbOut);
    if (RT_SUCCESS(rc))
    {
        if (pszValue && *pszValue != '\0')
        {
            g_pszAudioLogPath = pszValue;
        }

        if (g_pszAudioLogPath)
        {
            VRDPLOGREL(("Audio: log path [%s]\n", g_pszAudioLogPath));
        }
        else
        {
            VRDPMemFree(pszValue);
        }
    }

    m_AudioData.fRateStarted = false;
    m_AudioData.u64RateSamplesStartNS = 0;
    m_AudioData.u64RateSamplesNS = 0;

    RT_ZERO(m_AudioData.aDstFreqSamples);
    m_AudioData.iNextDstFreqSample = 0;
    m_AudioData.cFreqSamples = 0;

    ASMAtomicWriteS32(&m_AudioData.iFreqDelta, 0);
    ASMAtomicWriteU32(&m_AudioData.cLastPackets, 0);

    RT_ZERO(m_AudioData.rate);

    rc = VRDPLock::Create("AudioData", &m_AudioData.m_pAudioDataLock);

    /* The flag is the last to be initialized. When it is set the VM thread starts to deliver audio. */
    m_AudioData.fInitialized = RT_SUCCESS (rc);

    if (!m_AudioData.fInitialized)
    {
        VRDPLOGREL(("Audio initialization failed. %Rrc. Audio channel remains disabled!!!\n", rc));
    }
}

/** @thread OUTPUT */
void VRDPServer::audioUninit (void)
{
    if (m_AudioData.fInitialized)
    {
        /* Obtain the critical section to be sure that all later requests will fail. */
        audioLock ();

        m_AudioData.fInitialized = false;

        audioUnlock ();

        /* Deallocate. */
        audioRateDestroy(&m_AudioData.rate);

        VRDPLock::Delete(&m_AudioData.m_pAudioDataLock);
    }

    VRDPMemFree(g_pszAudioLogPath);
    g_pszAudioLogPath = NULL;
}

bool VRDPServer::audioLock (void)
{
    if (m_AudioData.fInitialized)
    {
        return RT_SUCCESS(VRDPLock::Lock(m_AudioData.m_pAudioDataLock));
    }
    return false;
}

void VRDPServer::audioUnlock (void)
{
    VRDPLock::Unlock(m_AudioData.m_pAudioDataLock);
}

bool VRDPServer::audioIsLocked (void)
{
    return VRDPLock::IsOwner(m_AudioData.m_pAudioDataLock);
}

DECLINLINE(uint64_t) samplesToNanoseconds(int cSamples, int iFreq)
{
    uint64_t u64DurationNano = ((uint64_t)cSamples * 1000 * 1000 * 1000) / iFreq;
    return u64DurationNano;
}

/** @thread VM */
VRDPAudioChunk *VRDPServer::audioChunkAlloc (size_t cbChunk)
{
    Assert (audioIsLocked ());

    return (VRDPAudioChunk *)VRDPMemAlloc (cbChunk);
}

/** @thread OUTPUT */
void VRDPServer::audioChunkFree (VRDPAudioChunk *pChunk)
{
    Assert (audioIsLocked ());

    VRDPMemFree (pChunk);
}

/** @thread VM */
void VRDPServer::audioChunkAppend (const uint64_t u64NewChunkStart, int iFreq, uint64_t u64NewChunkSamplesStartNanoTS)
{
    /*
     * Allocate 200 milliseconds chunk for the given freq.
     */

    Assert (audioIsLocked ());

    Assert (   !m_AudioData.pChunksTail
            || ((u64NewChunkStart - m_AudioData.pChunksTail->u64StartTS) % VRDP_AUDIO_CHUNK_TIME_MS) == 0);

    VRDPAudioChunk *pChunk;

    uint32_t cSamples = VRDP_AUDIO_CHUNK_SIZE_SAMPLES(iFreq);

    size_t cbChunk = sizeof (VRDPAudioChunk)
                     + (cSamples - RT_ELEMENTS(pChunk->astSamples)) * sizeof (st_sample_t);

    pChunk = audioChunkAlloc (cbChunk);

    if (!pChunk)
    {
         AssertFailed ();
         return;
    }

    AUDIOLOG(("Creating chunk %p at %lld\n", pChunk, u64NewChunkStart));

    pChunk->pNext       = NULL;
    pChunk->pPrev       = m_AudioData.pChunksTail;

    pChunk->u64CreatedTS = VRDPTimeMilliTS();
    pChunk->u64StartTS  = u64NewChunkStart;
    pChunk->iWriteIdx   = 0;
    pChunk->iFreq       = iFreq;
    pChunk->cSamples    = cSamples;
    pChunk->u64SamplesStartNanoTS = u64NewChunkSamplesStartNanoTS;

    /* Append at the tile of the list. */
    if (m_AudioData.pChunksTail)
    {
        pChunk->fFirst = false;
        m_AudioData.pChunksTail->pNext = pChunk;
    }
    else
    {
        pChunk->fFirst = true;
        m_AudioData.pChunksHead = pChunk;
    }

    m_AudioData.pChunksTail = pChunk;

    logtimingChunkCreated();
}

/** @thread VM */
void VRDPServer::audioChunkWriteSamples (const st_sample_t **ppSamples, int32_t *pcSamples, const VRDEAUDIOFORMAT format)
{
    RT_NOREF1(format);
    Assert (audioIsLocked ());
    Assert (m_AudioData.pChunksTail != NULL);
    Assert (m_AudioData.pChunksTail->iWriteIdx < m_AudioData.pChunksTail->cSamples);

    int32_t cSamplesForChunk = *pcSamples;

    /* Convert the samples to internal format. May convert the frequency. */
    if (cSamplesForChunk > 0)
    {
        /* How many samples of the internal format the chunk can hold. */
        int32_t cDstSamples = m_AudioData.pChunksTail->cSamples - m_AudioData.pChunksTail->iWriteIdx;

        if (cDstSamples > 0)
        {
            int32_t cSamples = RT_MIN(cSamplesForChunk, cDstSamples);

            memcpy (&m_AudioData.pChunksTail->astSamples[m_AudioData.pChunksTail->iWriteIdx],
                    *ppSamples,
                    cSamples * sizeof (st_sample_t)
                   );

            m_AudioData.pChunksTail->iWriteIdx += cSamples;

            Assert (m_AudioData.pChunksTail->iWriteIdx <= m_AudioData.pChunksTail->cSamples);

            *pcSamples -= cSamples;
            *ppSamples += cSamples;
        }
    }
}

void VRDPServer::AudioNotifyClientQueue(VRDPClient *pClient, uint32_t cPackets)
{
    RT_NOREF1(pClient);
    if ((m_AudioData.u32RateCorrectionMode & VRDP_AUDIO_MODE_CS) == 0)
    {
        /* Not enabled. */
        return;
    }

    uint32_t cLastPackets = ASMAtomicReadU32(&m_AudioData.cLastPackets);
    if (cLastPackets != 0)
    {
        int32_t iFreqDelta = ASMAtomicReadS32(&m_AudioData.iFreqDelta);

        /* Target is 4..5 packets on the client. */
        if (cPackets < 3 && cLastPackets > cPackets)
        {
            iFreqDelta += 50;
        }
        else if (cPackets > 5 && cLastPackets < cPackets)
        {
            iFreqDelta -= 50;
        }

        VHSTATLOG(("VHSTAT: queue size %u, iFreqDelta %d\n", cPackets, iFreqDelta));

        ASMAtomicWriteS32(&m_AudioData.iFreqDelta, iFreqDelta);
    }

    ASMAtomicWriteU32(&m_AudioData.cLastPackets, cPackets);
}

/* Return freq delta, which compensates a faster consumption of audio by the client. */
int VRDPServer::audioClientQueueCorrection(void)
{
    if ((m_AudioData.u32RateCorrectionMode & VRDP_AUDIO_MODE_CS) == 0)
    {
        /* Not enabled. */
        return 0;
    }

    int iFreqDelta = ASMAtomicReadS32(&m_AudioData.iFreqDelta);

    VHSTATLOG(("VHSTAT: audio: iFreqDelta %d\n",
                iFreqDelta));

    return iFreqDelta;
}

/** @thread VM */
void VRDPServer::audioAppendSamples (const st_sample_t *pSamples,
                                     int32_t cSamples,
                                     const VRDEAUDIOFORMAT format)
{
    /** @todo Ignore cSamples == 0 case. */
    AssertReturnVoid(VRDE_AUDIO_FMT_SAMPLE_FREQ(format) != 0);

    /* Remember when VRDP received these samples.
     * Note: the received samples have been already played by the guest,
     *       so the end of the samples (u64Now) is near the current guest audio position/time.
     */
    uint64_t u64NowNanoTS = VRDPTimeNanoTS ();
    uint64_t u64DurationNano;

    Assert (audioIsLocked ());

    int iSrcFreq = VRDE_AUDIO_FMT_SAMPLE_FREQ(format);

    if (   m_AudioData.pChunksTail == NULL
        || iSrcFreq != m_AudioData.pChunksTail->iFreq)
    {
        AUDIOLOG(("Creating new chunk: m_AudioData.pChunksTail = %p, freq %d, old freq %d!!!\n",
                  m_AudioData.pChunksTail, iSrcFreq, m_AudioData.pChunksTail? m_AudioData.pChunksTail->iFreq: 0));

        VHSTATLOG(("VHSTAT: audio: input rate: %d.\n",
                   iSrcFreq));

        /* Reinitialize the resampling. */
        audioRateStart(&m_AudioData.rate, iSrcFreq, m_AudioData.iDstFreq,
                       (m_AudioData.u32RateCorrectionMode & VRDP_AUDIO_MODE_LPF) != 0);

        /* Starting a new chunk sequence because it is first one or because
         * the audio frequency has changed.
         */
        uint64_t u64NewChunkStartTS;

        if (m_AudioData.pChunksTail == NULL)
        {
            /* Start a new sequence. */
            u64NewChunkStartTS = u64NowNanoTS / 1000000; /* nano -> milli */
            VHSTATCALL(this, AudioStartSamples, (u64NowNanoTS));
        }
        else
        {
            /* Use standard start time relative to current tail. */
            u64NewChunkStartTS = m_AudioData.pChunksTail->u64StartTS + VRDP_AUDIO_CHUNK_TIME_MS;
        }

        u64DurationNano = samplesToNanoseconds(cSamples, iSrcFreq);
        audioChunkAppend (u64NewChunkStartTS, iSrcFreq, u64NowNanoTS - u64DurationNano);

        if (  m_AudioData.pChunksTail == NULL
           || m_AudioData.pChunksTail->iWriteIdx != 0)
        {
            /* A new chunk was not allocated. */
            AssertFailed ();
            return;
        }
    }

    if ((m_AudioData.u32RateCorrectionMode & VRDP_AUDIO_MODE_RC) != 0)
    {
        /* Monitor how many samples are received and what is the actual frequency. */
        uint64_t u64DurationNS = samplesToNanoseconds(cSamples, VRDE_AUDIO_FMT_SAMPLE_FREQ(format));

        if (RT_LIKELY(m_AudioData.fRateStarted))
        {
            int64_t i64TimeElapsedNS = u64NowNanoTS - m_AudioData.u64RateSamplesStartNS;

            /* Correct every two seconds. */
            if (RT_LIKELY(i64TimeElapsedNS < INT64_C(2000000000)))
            {
                m_AudioData.u64RateSamplesNS += u64DurationNS;
            }
            else
            {
                int iDstFreq = (int)((VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ * i64TimeElapsedNS) / m_AudioData.u64RateSamplesNS);

                /* Avoid too large changes. */
                if (iDstFreq < VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ - 20)
                {
                    iDstFreq = VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ - 20;
                }
                else if (iDstFreq > VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ + 20)
                {
                    iDstFreq = VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ + 20;
                }

                /* Get a moving average for the iDstFreq to avoid large changes. */
                m_AudioData.aDstFreqSamples[m_AudioData.iNextDstFreqSample] = iDstFreq;

                m_AudioData.iNextDstFreqSample = (m_AudioData.iNextDstFreqSample + 1) % RT_ELEMENTS(m_AudioData.aDstFreqSamples);
                if (m_AudioData.cFreqSamples < (int)RT_ELEMENTS(m_AudioData.aDstFreqSamples))
                {
                    m_AudioData.cFreqSamples++;
                }
                else
                {
                    /* Enough samples to calculate the moving average. */
                    int iDstFreqAvg = 0;

                    int i;
                    for (i = 0; i < m_AudioData.cFreqSamples; i++)
                    {
                        iDstFreqAvg += m_AudioData.aDstFreqSamples[i];
                    }

                    m_AudioData.iDstFreq = iDstFreqAvg / m_AudioData.cFreqSamples;

                    VHSTATLOG(("VHSTAT: audio: rate: time %lld\n"
                               "                     aud %lld\n"
                               "                     diff %lld\n"
                               "                     freq %d %d\n"
                               "",
                               i64TimeElapsedNS,
                               m_AudioData.u64RateSamplesNS,
                               i64TimeElapsedNS - m_AudioData.u64RateSamplesNS,
                               m_AudioData.iDstFreq,
                               iDstFreq
                             ));
                }

                /* Start the next period. */
                m_AudioData.u64RateSamplesNS = u64DurationNS;
                m_AudioData.u64RateSamplesStartNS = u64NowNanoTS;
            }
        }
        else
        {
            /* Initialize the rate adjustment. */
            m_AudioData.u64RateSamplesNS = u64DurationNS;
            m_AudioData.u64RateSamplesStartNS = u64NowNanoTS;

            m_AudioData.fRateStarted = true;
        }
    }

    int iDstFreqDelta = audioClientQueueCorrection();

    /* The destination frequency may be changed there, inform the resampling about this. */
    audioRateUpdate(&m_AudioData.rate, iSrcFreq, m_AudioData.iDstFreq + iDstFreqDelta);

    VHSTATCALL(this, AudioAppendSamples, (u64NowNanoTS, cSamples, iSrcFreq));

    /* Add at the end of the tail chunk.
     *
     * 1) Append into the tail of chunk list.
     * 2) Form new chunks, if needed.
     */
    uint64_t u64LastChunkEndTS = m_AudioData.pChunksTail->u64StartTS + VRDP_AUDIO_CHUNK_TIME_MS;

    while (cSamples > 0)
    {
        if (m_AudioData.pChunksTail->cSamples == m_AudioData.pChunksTail->iWriteIdx)
        {
            /* No more place in the tail. Create new chunk. */
            logtimingChunkCompleted();

            AUDIOLOG(("Completed chunk: %p\n", m_AudioData.pChunksTail));

            uint64_t u64NewChunkStartTS = u64LastChunkEndTS;

            u64LastChunkEndTS += VRDP_AUDIO_CHUNK_TIME_MS;

            /* Update the duration, some samples could be written to the previous chunk already. */
            u64DurationNano = samplesToNanoseconds(cSamples, VRDE_AUDIO_FMT_SAMPLE_FREQ(format));
            audioChunkAppend (u64NewChunkStartTS, VRDE_AUDIO_FMT_SAMPLE_FREQ(format), u64NowNanoTS - u64DurationNano);
        }

        audioChunkWriteSamples (&pSamples, &cSamples, format);
    }

    Assert (cSamples == 0);
}

/** @thread VM */
void VRDPServer::ProcessAudio (const void *pvSamples, uint32_t cSamples, VRDEAUDIOFORMAT format)
{
    AssertMsgReturnVoid(   VRDE_AUDIO_FMT_BYTES_PER_SAMPLE(format) == sizeof(int64_t)
                        && VRDE_AUDIO_FMT_CHANNELS(format) == 2,
                        ("This callback should always get raw mixer frames (stereo S64)!\n"));
    if (audioLock ())
    {
        audioAppendSamples ((const st_sample_t *)pvSamples, cSamples, format);

        audioUnlock ();
    }
}

/** @thread OUTPUT */
uint64_t VRDPServer::audioTimerHandler (uint64_t u64EventTS)
{
    uint64_t u64NowNanoTS = VRDPTimeNanoTS ();
    uint64_t u64NowTS = u64NowNanoTS / 1000000;

    if (u64EventTS == 0)
    {
        return u64NowTS + VRDP_AUDIO_CHUNK_TIME_MS;
    }

    if (audioLock ())
    {
        /* Send to clients chunks which are on time.
         */
        if (m_AudioData.pChunksHead == NULL)
        {
            /* Nothing to send. */
            bool fFinished;

            if (   m_AudioData.fFinished
                && u64EventTS - m_AudioData.u64FinishedTS > 1000)
            {
                AUDIOLOG(("Seq: Sound finished.\n"));
                m_AudioData.fFinished = false;
                fFinished = true;

                VHSTATCALL(this, AudioEndSamples, (u64NowNanoTS));

                if ((m_AudioData.u32RateCorrectionMode & VRDP_AUDIO_MODE_RC) != 0)
                {
                    m_AudioData.iDstFreq = VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ;

                    m_AudioData.fRateStarted = false;
                    m_AudioData.u64RateSamplesStartNS = 0;
                    m_AudioData.u64RateSamplesNS = 0;

                    RT_ZERO(m_AudioData.aDstFreqSamples);
                    m_AudioData.iNextDstFreqSample = 0;
                    m_AudioData.cFreqSamples = 0;

                    VHSTATLOG(("VHSTAT: audio: rate: reset.\n"));
                }

                ASMAtomicWriteS32(&m_AudioData.iFreqDelta, 0);
                ASMAtomicWriteU32(&m_AudioData.cLastPackets, 0);

                audioRateDestroy(&m_AudioData.rate);
            }
            else
            {
                fFinished = false;
            }

            audioUnlock ();

            audioOutputChunk (NULL, fFinished);
        }
        else
        {
            m_AudioData.fFinished = false;

            while (m_AudioData.pChunksHead)
            {
                VRDPAudioChunk *pChunk = m_AudioData.pChunksHead;

                /* Chunks are sent after a delay greater than the chunk interval.
                 * That way the client receives complete chunks in a constant rate.
                 *
                 * First chunk is send after a 2 * VRDP_AUDIO_CHUNK_TIME_MS interval.
                 * Other complete chunks are sent immediately.
                 * Incomplete chunks are send after VRDP_AUDIO_CHUNK_TIME_MS interval.
                 */
                if (pChunk->fFirst)
                {
                    if (pChunk->u64StartTS + 2 * VRDP_AUDIO_CHUNK_TIME_MS > u64NowTS)
                    {
                        /* It is too early to send the chunk. */
                        break;
                    }
                }
                else
                {
                    if (   pChunk->iWriteIdx <= pChunk->cSamples
                        && pChunk->u64StartTS + VRDP_AUDIO_CHUNK_TIME_MS > u64NowTS)
                    {
                        /* It is too early to send the chunk. */
                        break;
                    }
                }

                /* Remove the chunk from the list.
                 */
                if (pChunk->pNext)
                {
                    pChunk->pNext->pPrev = NULL;
                }
                else
                {
                    Assert (pChunk == m_AudioData.pChunksTail);
                    m_AudioData.pChunksTail = NULL;
                }

                m_AudioData.pChunksHead = pChunk->pNext;

                /* Leave the lock before sending.
                 */
                audioUnlock ();

                AUDIOLOG(("Sending chunk %p at u64StartTS %lld delayed %lld\n",
                           pChunk,
                           pChunk->u64StartTS,
                           u64NowTS - pChunk->u64StartTS));

                audioOutputChunk (pChunk, false);

                if (!audioLock ())
                {
                    break;
                }

                audioChunkFree (pChunk);
            }

            /* Postpone the "finished" notification in a one second. */
            if (m_AudioData.pChunksHead == 0)
            {
                AUDIOLOG(("Seq: Pause detected.\n"));
                m_AudioData.fFinished = true;
                m_AudioData.u64FinishedTS = u64EventTS;
            }

            audioUnlock ();
        }
    }

    return u64EventTS + VRDP_AUDIO_CHUNK_TIME_MS / 2;
}

/** @thread OUTPUT */
void VRDPServer::audioOutputChunk (VRDPAudioChunk *pChunk, bool fFinished)
{
    st_sample_t *pOutSamples;
    int cOutSamplesActual;

    if (pChunk == NULL)
    {
        /* Sound completion case or output of buffered samples. */
        pOutSamples       = NULL;
        cOutSamplesActual = 0;
    }
    else
    {
        pOutSamples       = &m_AudioData.astSamples[0];
        cOutSamplesActual = RT_ELEMENTS(m_AudioData.astSamples);

        int cSrcSamples = pChunk->iWriteIdx;

        audioRateConvert(&m_AudioData.rate, pChunk->astSamples,
                         &cSrcSamples,
                         pOutSamples,
                         &cOutSamplesActual);

        VHSTATCALL(this, AudioConvertSamples, (cOutSamplesActual, VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ));

        logtimingChunkSent();
    }

    /* An audio output is broadcasted to all clients which have audio channel.
     */
    VRDPClient *pClient = NULL;
    uint32_t u32ClientId = 0;
    uint64_t u64SamplesStartNanoTS = pChunk? pChunk->u64SamplesStartNanoTS: 0;

    while ((pClient = m_clientArray.ThreadContextGetNextClient (&u32ClientId, VRDP_CONTEXT_OUTPUT)) != NULL)
    {
        pClient->OutputAudio (pOutSamples, cOutSamplesActual, fFinished, u64SamplesStartNanoTS);

        pClient->ThreadContextRelease (VRDP_CONTEXT_OUTPUT);
    }
}

/** @thread OUTPUT */
void VRDPClient::OutputAudio (st_sample_t *paSamples, int cSamples, bool fFinished, uint64_t u64SamplesStartNanoTS)
{
    m_vrdptp.Audio ()->Send (paSamples, cSamples, fFinished, u64SamplesStartNanoTS);
}

/** @thread INPUT */
void VRDPClient::AudioCompletion (uint8_t u8PacketIdx)
{
    m_pServer->PostOutput (VRDP_OUTPUT_AUDIO_COMPLETION, Id (), &u8PacketIdx, sizeof (u8PacketIdx));
}


/*
 * The Audio channel implementation.
 */

#pragma pack(1)
typedef struct WavFileHdr
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

typedef struct LOGWAVCTX
{
    FILE *f;
    WavFileHdr wavhdr;
} LOGWAVCTX;

#define VRDP_FOURCC(c0, c1, c2, c3) RT_H2LE_U32_C(RT_MAKE_U32_FROM_U8(c0, c1, c2, c3))

static void logwavstart(void **ppvCtx, const char *pszAudioLogPath, uint32_t u32ClientId)
{
    LOGWAVCTX *pCtx = (LOGWAVCTX *)VRDPMemAllocZ(sizeof(LOGWAVCTX));
    if (!pCtx)
    {
        return;
    }

    bool fSuccess = false;

    pCtx->wavhdr.u32RIFF = VRDP_FOURCC('R','I','F','F');
    pCtx->wavhdr.u32Size = 36;
    pCtx->wavhdr.u32WAVE = VRDP_FOURCC('W','A','V','E');

    pCtx->wavhdr.u32Fmt = VRDP_FOURCC('f','m','t',' ');
    pCtx->wavhdr.u32Size1 = 16;
    pCtx->wavhdr.u16AudioFormat = 1;
    pCtx->wavhdr.u16NumChannels = 2;
    pCtx->wavhdr.u32SampleRate = 22050;
    pCtx->wavhdr.u32ByteRate = 22050 * 2 * 2;
    pCtx->wavhdr.u16BlockAlign = 2 * 2;
    pCtx->wavhdr.u16BitsPerSample = 16;

    pCtx->wavhdr.u32Data = VRDP_FOURCC('d','a','t','a');
    pCtx->wavhdr.u32Size2 = 0;

    /* Construct the file name. */
    char szWavName[64]; /* Need 5 + 10 + 1 + 16 + 4 + 1 = 37 chars max. */
    size_t len = RTStrPrintf(&szWavName[0], sizeof(szWavName), "vrdp-%u-%RX64.wav", u32ClientId, RTTimeNanoTS());
    if (len > 0)
    {
        char *pszWavPath = RTPathJoinA(pszAudioLogPath, szWavName);
        if (pszWavPath)
        {
            pCtx->f = fopen(pszWavPath, "wb");
            if (pCtx->f)
            {
                fwrite(&pCtx->wavhdr, 1, sizeof(pCtx->wavhdr), pCtx->f);
                fSuccess = true;
            }

            RTStrFree(pszWavPath);
        }
    }

    if (fSuccess)
    {
        *ppvCtx = pCtx;
    }
    else
    {
        VRDPMemFree(pCtx);
    }
}

static void logwavend(void *pvCtx)
{
    LOGWAVCTX *pCtx = (LOGWAVCTX *)pvCtx;
    if (pCtx)
    {
        fseek(pCtx->f, 0, SEEK_SET);
        fwrite(&pCtx->wavhdr, 1, sizeof(pCtx->wavhdr), pCtx->f);
        fclose(pCtx->f);

        VRDPMemFree(pCtx);
    }
}

static void logwav(void *pvCtx, const void *pv, size_t cb)
{
    LOGWAVCTX *pCtx = (LOGWAVCTX *)pvCtx;
    if (pCtx)
    {
        fwrite(pv, 1, cb, pCtx->f);

        pCtx->wavhdr.u32Size  += (uint32_t)cb;
        pCtx->wavhdr.u32Size2 += (uint32_t)cb;
    }
}

VRDPChannelAudio::VRDPChannelAudio (VRDPTP *pvrdptp)
    :
    m_pvrdptp (pvrdptp),
    m_channelOptions (0),
    m_channelId (0),
    m_fOperational (false),
    m_fSetup (false),
    m_fAccumulating (true),
    m_u8PacketIdx (0),
    m_u8PacketIdxLast (0),
    m_u8PacketIdxLastCompleted (0),
    m_fSendClose (false),
    m_cPacketsToSkip (0),
    m_u32OutputTail (0),
    m_u32OutputHead (0),
    m_u32TimeRefPosition (0),
    m_u64TimeRefNanoTS (0),
    m_pLogCtx (NULL)
{
    RT_ZERO(m_clientQueue);
}

VRDPChannelAudio::~VRDPChannelAudio ()
{
    Close ();
}

/** @thread INPUT */
int VRDPChannelAudio::Open (uint32_t options, uint16_t id)
{
    m_channelOptions = options;
    m_channelId      = id;

    InitOutgoingChannelId (m_channelId);

    return VINF_SUCCESS;
}

/** @thread INPUT */
void VRDPChannelAudio::Setup (void)
{
    if (g_pszAudioLogPath)
    {
        /* Enabled. */
        logwavstart(&m_pLogCtx, g_pszAudioLogPath, m_pvrdptp->Client()->Id());
    }

    if (m_channelId != 0)
    {
        int rc = m_pvrdptp->Desktop ()->InterceptChannel (m_pvrdptp->Client (), VRDE_CLIENT_INTERCEPT_AUDIO, NULL);

        if (RT_SUCCESS (rc))
        {
            m_fSetup = true;
        }
    }

    return;
}

/** @thread INPUT */
void VRDPChannelAudio::Close (void)
{
    logwavend(m_pLogCtx);
    m_pLogCtx = NULL;

    m_channelOptions = 0;
    m_channelId      = 0;
    m_fOperational   = false;
    m_fAccumulating  = true;
    m_u8PacketIdx    = 0;
    m_u8PacketIdxLast= 0;
    m_u8PacketIdxLastCompleted = 0;
    m_fSendClose     = false;
    m_cPacketsToSkip = 0;
    m_u32OutputTail  = 0;
    m_u32OutputHead  = 0;
    RT_ZERO(m_clientQueue);
}

#pragma pack(1)
/* SNDPROLOG */
typedef struct _RDPAudioCommandHdr
{
    uint8_t u8Type;
    uint8_t u8Pad;                 /* Unused, arbitrary and MUST be ignored on receipt. */
    uint16_t u16Length;            /* Size of the data that follows the header.
                                    * If u8Type is set to 0x02 (SNDC_WAVE), then
                                    * the size of the WaveInfo PDU plus the size of the data field
                                    * of the Wave PDU that immediately follows this packet
                                    * minus the size of the Header.
                                    */
} RDPAudioCommandHdr;

/* SERVER_AUDIO_VERSION_AND_FORMATS */
typedef struct _RDPAudioNegotiateReqHdr
{
    RDPAudioCommandHdr hdr;

    uint32_t u32Flags;             /* Unused, arbitrary and MUST be ignored on receipt. */
    uint32_t u32Volume;            /* Unused, arbitrary and MUST be ignored on receipt. */
    uint32_t u32Pitch;             /* Unused, arbitrary and MUST be ignored on receipt. */
    uint16_t u16DGramPort;         /* Unused, arbitrary and MUST be ignored on receipt. */
    uint16_t u16NumberOfFormats;
    uint8_t  u8LastBlockConfirmed; /* The value sent by the server is arbitrary. */
    uint16_t u16Version;
    uint8_t  u8Pad;
} RDPAudioNegotiateReqHdr;

/* AUDIO_FORMAT */
typedef struct _RDPAudioFormat
{
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
} RDPAudioFormat;

typedef struct _RDPAudioNegotiateReq
{
    RDPAudioNegotiateReqHdr hdr;

    RDPAudioFormat aFormats[1];
} RDPAudioNegotiateReq;

/* CLIENT_AUDIO_VERSION_AND_FORMATS */
typedef struct _RDPAudioNegotiateResponse
{
    uint32_t u32Flags;
    uint32_t u32Volume;
    uint32_t u32Pitch;
    uint16_t u16DGramPortBE;       /* MUST be specified by using big-endian byte ordering. */
    uint16_t u16NumberOfFormats;
    uint8_t  u8LastBlockConfirmed; /* Unused, arbitrary and MUST be ignored on receipt. */
    uint16_t u16Version;
    uint8_t  u8Pad;

    RDPAudioFormat aFormats[1];
} RDPAudioNegotiateResponse;

typedef struct _RDPAudioSetVolume
{
    RDPAudioCommandHdr hdr;

    uint16_t u16Left;
    uint16_t u16Right;
} RDPAudioSetVolume;

/* SNDWAVINFO */
typedef struct _RDPAudioWrite
{
    RDPAudioCommandHdr hdr;

    uint16_t u16TimeStamp; /* Time that represents when this PDU is built */
    uint16_t u16FormatNo;
    uint8_t  u8BlockNo;    /* Block ID of the audio data. RDPAudioCompletion::u8ConfirmedBlockNo */
    uint8_t  au8Pad[3];
    uint8_t  au8Data[4];   /* The first four bytes of the audio data. */
} RDPAudioWrite;

typedef struct _RDPAudioClose
{
    RDPAudioCommandHdr hdr;
} RDPAudioClose;

/* SNDWAV_CONFIRM */
typedef struct _RDPAudioCompletion
{
    uint16_t u16TimeStamp;
    uint8_t u8ConfirmedBlockNo;
    uint8_t u8Pad;
} RDPAudioCompletion;
#pragma pack()

/** @thread INPUT */
void VRDPChannelAudio::Start (void)
{
    /* Negotiate audio capabilities. */
    if (!m_fSetup)
    {
        /* If the channel is not initialized. */
        AUDIOLOG(("channel not supported, negotiation cancelled!!!\n"));
        return;
    }

    if (m_fOperational)
    {
        AUDIOLOG(("channel already intialized, negotiation cancelled!!!\n"));
        return;
    }

    RDPAudioNegotiateReq req;

    /* RDPAudioCommandHdr */
    req.hdr.hdr.u8Type = RDPSND_NEGOTIATE;
    req.hdr.hdr.u8Pad = 0;
    req.hdr.hdr.u16Length = sizeof (req) - sizeof (req.hdr.hdr);

    /* RDPAudioNegotiateReqHdr */
    req.hdr.u32Flags    = 0;
    req.hdr.u32Volume   = 0;
    req.hdr.u32Pitch    = 0;
    req.hdr.u16DGramPort = 0;
    req.hdr.u16NumberOfFormats = 1;
    req.hdr.u8LastBlockConfirmed = m_u8PacketIdx;
    req.hdr.u16Version = 5;
    req.hdr.u8Pad = 0;

    ++m_u8PacketIdx; /* u8BlockNo field in the first WaveInfo PDU MUST be one more than
                      * the u8LastBlockConfirmed field of the Server Audio Formats and Version PDU
                      * sent by the server to the client. */

    /* RDPAudioNegotiateReq */
    req.aFormats[0].wFormatTag      = 1; /* WAVE_FORMAT_PCM */
    req.aFormats[0].nChannels       = 2; /* Stereo */
    req.aFormats[0].nSamplesPerSec  = VRDP_AUDIO_CHUNK_INTERNAL_FREQ_HZ;
    req.aFormats[0].nBlockAlign     = req.aFormats[0].nChannels * 2; /* 16 bit or 2 bytes per sample. */
    req.aFormats[0].nAvgBytesPerSec = req.aFormats[0].nSamplesPerSec * req.aFormats[0].nBlockAlign;
    req.aFormats[0].wBitsPerSample  = 16;
    req.aFormats[0].cbSize          = 0;

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = &req;
    aBuffers[0].cb = sizeof (req);

    /* Because of special state of the channel, the sendBuffers can not be used. */
    m_pvrdptp->SendToChannel (this /* as OutputCtx */, m_channelId, m_channelOptions,
                              RT_ELEMENTS(aBuffers), aBuffers, false);
}

/** @thread OUTPUT */
void VRDPChannelAudio::Send (const st_sample_t *paSamples, int cSamples, bool fFinished, uint64_t u64SamplesStartNanoTS)
{
    if (!m_fOperational)
    {
        return;
    }

    /* Cancel any pending close. */
    m_fSendClose = false;

    /*
     * The send process is controlled by m_fAccumulating:
     *   it is set initially or when NULL chunk arrives,
     *   indicates that the arrived chunk must be
     *   not sent but only saved in the buffer.
     */

    if (paSamples != NULL)
    {
        /* Convert the samples to the client's format and put them in the ring buffer.
         * Since currently the freq of the output format is the same as the internal
         * one, the rate convertion is skipped.
         */
        f_sample *clip = mixeng_clip[1 /* 0 - mono, 1 - stereo     */]
                                    [1 /* 0 - unsigned, 1 - signed */]
                                    [1 /* 0 - 8 bit, 1 - 16 bit    */];

        /* Clip to the output buffer. */
        uint8_t *pu8Dst = &m_au8OutputBuffer[m_u32OutputTail];

        uint32_t u32OutputBytes = cSamples * 2 * 2; /* Stereo 2 bytes per sample. */

        /* Check overflow condition in the ring buffer. */
        int32_t iDiff = m_u32OutputHead - m_u32OutputTail;

        uint32_t u32FreeBytesInBuffer = iDiff > 0? iDiff: sizeof (m_au8OutputBuffer) + iDiff;

        if (u32OutputBytes >= u32FreeBytesInBuffer)
        {
            VRDPLOGREL(("Audio buffer overflow: free %d, needed %d, head %d, tail %d. Skipping %d samples!!!\n",
                        u32FreeBytesInBuffer, u32OutputBytes, m_u32OutputHead, m_u32OutputTail, cSamples));

            /* Just skip these samples. */
            return;
        }

        uint32_t u32BytesTillBoundary = sizeof (m_au8OutputBuffer) - m_u32OutputTail;
        Assert (u32BytesTillBoundary > 0);
        Assert ((u32BytesTillBoundary % 4) == 0);

        if (u32OutputBytes < u32BytesTillBoundary)
        {
            clip (pu8Dst, paSamples, cSamples);
            logwav(m_pLogCtx, pu8Dst, u32OutputBytes);

            m_u32OutputTail += u32OutputBytes;
        }
        else
        {
            int cSamplesTillBoundary = u32BytesTillBoundary / 4;

            clip (pu8Dst, paSamples, cSamplesTillBoundary);
            logwav(m_pLogCtx, pu8Dst, u32BytesTillBoundary);

            uint32_t u32BytesAfterBoundary = u32OutputBytes - u32BytesTillBoundary;
            Assert ((u32BytesAfterBoundary % 4) == 0);

            if (u32BytesAfterBoundary > 0)
            {
                pu8Dst = &m_au8OutputBuffer[0];

                clip (pu8Dst, &paSamples[cSamplesTillBoundary], cSamples - cSamplesTillBoundary);
                logwav(m_pLogCtx, pu8Dst, u32BytesAfterBoundary);
            }

            m_u32OutputTail = u32BytesAfterBoundary;
        }

        /* Update the time reference point. For simplification one point is used, and
         * timestamps are extrapolated for other samples.
         */
        m_u32TimeRefPosition = m_u32OutputTail;
        m_u64TimeRefNanoTS = u64SamplesStartNanoTS;
    }

    /** @todo Verify if it is still necessary. Accumulation is actually done on the chunk level:
     * 2 first chunks are usually sent together, which means that this method will be called
     * 2 times without a delay and fAccumulating will be reset on the second time.
     */
    if (m_fAccumulating)
    {
        AUDIOLOG(("Finishing accumulation.\n"));

        m_fAccumulating = false;
    }
    else
    {
        /* Send only complete blocks. */
        sendWrite (false);
    }

    if (fFinished)
    {
        AUDIOLOG(("Seq: Sound completed.\n"));

        /* Send all blocks, also the incomplete last block. */
        sendWrite (true);

        m_fAccumulating = true;
    }
}

/** @thread OUTPUT */
void VRDPChannelAudio::Completion (uint8_t u8PacketIdx)
{
    AUDIOLOG(("m_u8PacketIdx %d, u8PacketIdx %d\n", m_u8PacketIdx, u8PacketIdx));

    uint32_t u32Diff = m_u8PacketIdx > u8PacketIdx?
                           m_u8PacketIdx - u8PacketIdx:
                           256 - (u8PacketIdx - m_u8PacketIdx);

    VHSTATLOG(("VHSTAT: audio: processing Completion: %d %d %d.\n",
                u32Diff, m_u8PacketIdx, u8PacketIdx));

    uint32_t u32ClientQueueLimit = 8;

    m_clientQueue.aDiffSamples[m_clientQueue.iNextDiffSample] = u32Diff;

    m_clientQueue.iNextDiffSample = (m_clientQueue.iNextDiffSample + 1) % RT_ELEMENTS(m_clientQueue.aDiffSamples);

    if (m_clientQueue.cDiffSamples < (int)RT_ELEMENTS(m_clientQueue.aDiffSamples))
    {
        m_clientQueue.cDiffSamples++;
    }
    else
    {
        /* Enough samples to calculate the moving average. */
        uint32_t u32DiffAvg = 0;

        int i;
        for (i = 0; i < m_clientQueue.cDiffSamples; i++)
        {
            u32DiffAvg += m_clientQueue.aDiffSamples[i];
        }

        u32DiffAvg = u32DiffAvg / m_clientQueue.cDiffSamples;

        VHSTATLOG(("VHSTAT: audio: client queue: avg %d, current %d, avg change %d\n"
                   "",
                   u32DiffAvg,
                   u32Diff,
                   m_clientQueue.u32Diff - u32DiffAvg
                 ));

        m_clientQueue.u32Diff = u32DiffAvg;

        /* Simply solution:
         * first avg is the recommended size, if queue increases > 4 + recommended, drop 4 packets.
         */
         if (m_clientQueue.u32QueueLimit == 0)
         {
             m_clientQueue.u32QueueLimit = m_clientQueue.u32Diff;

             if (m_clientQueue.u32QueueLimit < 8)
             {
                 m_clientQueue.u32QueueLimit = 8;
             }

             VHSTATLOG(("VHSTAT: audio: client queue: limit %d\n"
                        "",
                        m_clientQueue.u32QueueLimit
                      ));
         }

         u32ClientQueueLimit = m_clientQueue.u32QueueLimit;

         if (u32Diff > u32ClientQueueLimit)
         {
             VHSTATLOG(("VHSTAT: audio: client queue size: %d %d %d.\n",
                         u32Diff, m_u8PacketIdx, u8PacketIdx));

             if (m_cPacketsToSkip == 0)
             {
                 uint32_t u32Overflow = u32Diff - u32ClientQueueLimit;
                 if (u32Overflow > 4)
                 {
                     u32Overflow = 4;

                     VHSTATLOG(("VHSTAT: audio: client queue size: skip %d\n",
                                 u32Overflow));
                 }
                 else
                 {
                     u32Overflow = 0;
                 }

                 /* This will be processed in sendWrite. */
                 m_cPacketsToSkip = u32Overflow;
             }
         }

         m_pvrdptp->Client()->Server()->AudioNotifyClientQueue(m_pvrdptp->Client(), m_clientQueue.u32Diff);
    }

    m_u8PacketIdxLastCompleted = u8PacketIdx;

    if (u8PacketIdx == m_u8PacketIdxLast && m_fSendClose)
    {
        m_fSendClose = false;

        sendClose ();
    }
}

/** @thread OUTPUT */
/** @todo fEnd must be processed in VRDPChannelAudio::Send. */
void VRDPChannelAudio::sendWrite (bool fEnd)
{
    /* Send as many data as possible from the ring buffer.
     * Also always send the last incomplete block to avoid a break in the audio data.
     */

    /* Up to 3 buffers will be required for sending the audio data. */
    VRDPBUFFER aBuffers[3];

    RDPAudioWrite req;

    Assert (m_fOperational);
    Assert (QueryOutgoingChannelId () == m_channelId);

    bool fFullBufferSent = false;

    int32_t iDiff = m_u32OutputTail - m_u32OutputHead;

    uint32_t u32BytesInBuffer = iDiff < 0? sizeof (m_au8OutputBuffer) + iDiff: iDiff;

    while (u32BytesInBuffer > 0)
    {
        /* How many bytes will be fetched from the buffer for the block being sent. */
        uint32_t u32BytesFromBuffer;

        if (u32BytesInBuffer < VRDP_AUDIO_OUTPUT_BLOCK_SIZE)
        {
            /* Only send the incomplete buffer if there were no full buffer sent before. */
            if (fFullBufferSent)
            {
                break;
            }

            AUDIOLOG(("Seq: Sending incomplete buffer %d bytes.\n", u32BytesInBuffer));
            u32BytesFromBuffer = u32BytesInBuffer;
        }
        else
        {
            u32BytesFromBuffer = VRDP_AUDIO_OUTPUT_BLOCK_SIZE;
            fFullBufferSent = true;
        }

        /* Calculate timestamps of the audio samples. */
        uint64_t u64PacketDurationNano = samplesToNanoseconds(u32BytesFromBuffer / 4, 22050);
        AUDIOLOG(("OnAudioSend: pkt duration %llu, bytes from buffer %d\n", u64PacketDurationNano, u32BytesFromBuffer));

        /* Distance from current head to the reference point. */
        int32_t i32RefDiff = m_u32TimeRefPosition - m_u32OutputHead;

        uint32_t u32Distance = i32RefDiff < 0? sizeof (m_au8OutputBuffer) + i32RefDiff: i32RefDiff;
        AUDIOLOG(("OnAudioSend: ref %u, head %u, diff %d, dist %u\n",
                  m_u32TimeRefPosition, m_u32OutputHead, i32RefDiff, u32Distance));

        /* The reference point must always be within valid data in the buffer. */
        Assert(u32Distance <= u32BytesInBuffer);

        /* The packets start is before the ref time in nanoseconds neede for u32Distance bytes. */
        uint64_t u64PacketStartNanoTS = m_u64TimeRefNanoTS - samplesToNanoseconds(u32Distance / 4, 22050);

        /* Simple approach. */
        uint64_t u64PacketEndNanoTS = u64PacketStartNanoTS + u64PacketDurationNano;
        AUDIOLOG(("OnAudioSend: pkt start %llu, end %llu\n", u64PacketStartNanoTS, u64PacketEndNanoTS));

        /*
         * The RDPSND_WRITE command
         */

        /* RDPAudioCommandHdr */
        req.hdr.u8Type    = RDPSND_WRITE;
        req.hdr.u8Pad     = 0;
        req.hdr.u16Length = u32BytesFromBuffer + 8;   /* MUST be set to 8 bytes more than the size of the entire audio sample.
                                                       * Size of the req excluding 4 first samples bytes and header. */
        req.u16TimeStamp = (uint16_t)VRDPTimeMilliTS(); /* Windows sets this field to the number of milliseconds
                                                       * that have elapsed since the system was started. */
        req.u16FormatNo  = 0;
        req.u8BlockNo    = m_u8PacketIdx;
        RT_ZERO(req.au8Pad);

        /*
         * Audio data
         */

        /* Take into account the buffer boundary. */
        uint8_t *pu8SrcA;
        uint32_t cbSrcA;
        uint8_t *pu8SrcB;
        uint32_t cbSrcB;

        uint32_t u32BytesTillBoundary = sizeof (m_au8OutputBuffer) - m_u32OutputHead;

        if (u32BytesTillBoundary > u32BytesFromBuffer)
        {
            pu8SrcA = &m_au8OutputBuffer[m_u32OutputHead];
            cbSrcA = u32BytesFromBuffer;
            pu8SrcB = NULL;
            cbSrcB = 0;

            m_u32OutputHead += u32BytesFromBuffer;
        }
        else if (u32BytesTillBoundary == u32BytesFromBuffer)
        {
            pu8SrcA = &m_au8OutputBuffer[m_u32OutputHead];
            cbSrcA = u32BytesFromBuffer;
            pu8SrcB = NULL;
            cbSrcB = 0;

            m_u32OutputHead = 0;
        }
        else
        {
            pu8SrcA = &m_au8OutputBuffer[m_u32OutputHead];
            cbSrcA = u32BytesTillBoundary;
            pu8SrcB = &m_au8OutputBuffer[0];
            cbSrcB = u32BytesFromBuffer - u32BytesTillBoundary;

            m_u32OutputHead = cbSrcB;
        }

        aBuffers[1].pv = pu8SrcA;
        aBuffers[1].cb = cbSrcA;

        /* The SendToChannel method is aware of zero length buffers. */
        aBuffers[2].pv = pu8SrcB;
        aBuffers[2].cb = cbSrcB;

        /* Extract first 4 bytes from the buffers [1..] and put these bytes into the req. */
        uint32_t cbNeeded = sizeof (req.au8Data);

        uint32_t i;
        for (i = 1; i < RT_ELEMENTS(aBuffers) && cbNeeded > 0; i++)
        {
            int cbFromBuffer = RT_MIN(aBuffers[i].cb, cbNeeded);

            if (cbFromBuffer == 0)
            {
                continue;
            }

            memcpy (&req.au8Data[sizeof(req.au8Data) - cbNeeded],
                    aBuffers[i].pv,
                    cbFromBuffer);

            aBuffers[i].cb -= cbFromBuffer;
            cbNeeded       -= cbFromBuffer;

            if (aBuffers[i].cb == 0)
            {
                aBuffers[i].pv = NULL;
            }
            else
            {
                aBuffers[i].pv = (uint8_t *)aBuffers[i].pv + cbFromBuffer;
            }
        }

        Assert (cbNeeded == 0);

        if (RT_LIKELY(m_cPacketsToSkip == 0))
        {
            /* Current packet is ready to be sent, update the packet index. */
            m_u8PacketIdxLast = m_u8PacketIdx;
            m_u8PacketIdx++;

            /* Report the time when the samples were received from the VM. */
            m_pvrdptp->Client ()->OnAudioSend (u64PacketStartNanoTS, u64PacketEndNanoTS, u64PacketDurationNano);

            /* The command itself is sent as one buffer. */
            aBuffers[0].pv = &req;
            aBuffers[0].cb = sizeof (req);

            sendBuffers (1, aBuffers);

            /* Leading zero for the audio data. */
            static uint8_t au8Zero[4];
            aBuffers[0].pv = &au8Zero[0];
            aBuffers[0].cb = 4;

            Assert (aBuffers[0].cb + aBuffers[1].cb + aBuffers[2].cb == u32BytesFromBuffer);

            sendBuffers (RT_ELEMENTS(aBuffers), aBuffers);

            logtimingPacketSent();
            AUDIOLOG(("pkt: send %d bytes %dms index = %d\n",
                      u32BytesFromBuffer, (1000 * u32BytesFromBuffer) / 4 / 22050, m_u8PacketIdxLast));
        }
        else
        {
            /* Skipped the packet. */
            ASMAtomicDecS32(&m_cPacketsToSkip);
        }

        u32BytesInBuffer -= u32BytesFromBuffer;
    }

    /* Remember that this was the very last send before the device close. */
    if (fEnd)
    {
        AUDIOLOG(("Set m_fSendClose\n"));

        if (m_u8PacketIdxLastCompleted == m_u8PacketIdxLast)
        {
            /* Immediately close, because the last sent packet has been already completed. */
            Assert(m_fSendClose == false);

            m_fSendClose = false; /** @todo needed? */

            sendClose ();
        }
        else
        {
            /* Remember that a close must be sent, when all packets are completed.
             * Since the check in VRDPChannelAudio::Completion is done on the same
             * OUTPUT thread there is not need for any synchronization.
             */
            m_fSendClose = true;
        }
    }

    return;
}

/** @thread OUTPUT */
void VRDPChannelAudio::SendSetVolume (uint16_t left, uint16_t right)
{
    if (!m_fOperational)
    {
        return;
    }

    AUDIOLOG(("volume left %d, right %d.\n", left, right));

    RDPAudioSetVolume req;

    /* RDPAudioCommandHdr */
    req.hdr.u8Type    = RDPSND_SET_VOLUME;
    req.hdr.u8Pad     = 0;
    req.hdr.u16Length = sizeof (req) - sizeof (req.hdr);

    req.u16Left  = left;
    req.u16Right = right;

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = &req;
    aBuffers[0].cb = sizeof (req);

    sendBuffers (RT_ELEMENTS (aBuffers), aBuffers);
}

/** @thread OUTPUT */
void VRDPChannelAudio::sendClose (void)
{
    Assert (m_fOperational);

    AUDIOLOG(("Closing audio device.\n"));

    m_pvrdptp->Client ()->OnAudioClose ();

    RDPAudioClose req;

    /* RDPAudioCommandHdr */
    req.hdr.u8Type    = RDPSND_CLOSE;
    req.hdr.u8Pad     = 0;
    req.hdr.u16Length = sizeof (req) - sizeof (req.hdr);

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = &req;
    aBuffers[0].cb = sizeof (req);

    sendBuffers (RT_ELEMENTS (aBuffers), aBuffers);

    RT_ZERO(m_clientQueue);
}

/** @thread OUTPUT */
int VRDPChannelAudio::sendBuffers (int cBuffers, const VRDPBUFFER *paBuffers)
{
    Assert (m_fOperational);
    Assert (m_pvrdptp->Client ()->Server ()->IsOutputThread ());

    return m_pvrdptp->SendToChannel (this /* as OutputCtx */, m_channelId, m_channelOptions,
                                     cBuffers, paBuffers, false);
}

/** @thread INPUT */
int VRDPChannelAudio::processNegotiateResponse (const uint8_t *pu8Input, uint32_t u32Length)
{
    const RDPAudioNegotiateResponse *pResponse = (const RDPAudioNegotiateResponse *)pu8Input;

    /* Since at least 1 supported format is expected, compare the length with the full structure size. */
    if (u32Length < sizeof (RDPAudioNegotiateResponse))
    {
        Close ();
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    AUDIOLOG(("AUDIO: processNegotiateResponse: u32Flags 0x%08X, u32Volume 0x%08X, u32Pitch  0x%08X\n"
              "    u16DGramPort %d, u16NumberOfFormats = %d, u8LastBlockConfirmed 0x%02X, u16Version %d, u8Pad 0x%02X\n",
              pResponse->u32Flags,
              pResponse->u32Volume,
              pResponse->u32Pitch,
              pResponse->u16DGramPortBE,
              pResponse->u16NumberOfFormats,
              pResponse->u8LastBlockConfirmed,
              pResponse->u16Version,
              pResponse->u8Pad));

    /* It is expected that the client sends back exactly the requested format. */
    if (pResponse->u16NumberOfFormats != 1)
    {
        Close ();
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    AUDIOLOG(("AUDIO: processNegotiateResponse: wFormatTag 0x%04X, nChannels 0x%04X, nSamplesPerSec %d\n"
              "    nAvgBytesPerSec %d, nBlockAlign = %d, wBitsPerSample %d, cbSize %d\n",
              pResponse->aFormats[0].wFormatTag,
              pResponse->aFormats[0].nChannels,
              pResponse->aFormats[0].nSamplesPerSec,
              pResponse->aFormats[0].nAvgBytesPerSec,
              pResponse->aFormats[0].nBlockAlign,
              pResponse->aFormats[0].wBitsPerSample,
              pResponse->aFormats[0].cbSize));

    m_fOperational = true;

    return VINF_SUCCESS;
}

/** @thread INPUT */
int VRDPChannelAudio::ProcessChannelInput (const uint8_t *pu8Input, uint32_t u32Length)
{
    int rc = VINF_SUCCESS;

    if (u32Length < sizeof (RDPAudioCommandHdr))
    {
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    const RDPAudioCommandHdr *pHdr = (const RDPAudioCommandHdr *)pu8Input;

    pu8Input += sizeof (RDPAudioCommandHdr);
    u32Length -= sizeof (RDPAudioCommandHdr);

    AUDIOLOG(("AUDIO: VRDPChannelAudio::ProcessChannelInput: type = %d, pad = 0x%02X, datalen = %d\n", pHdr->u8Type, pHdr->u8Pad, pHdr->u16Length));

    switch (pHdr->u8Type)
    {
        case RDPSND_NEGOTIATE:
        {
            rc = processNegotiateResponse (pu8Input, u32Length);
        } break;

        case RDPSND_COMPLETION:
        {
            const RDPAudioCompletion *pCompletion = (const RDPAudioCompletion *)pu8Input;

            AUDIOLOG(("pkt: completion tick = %d, index = %d\n",
                       pCompletion->u16TimeStamp, pCompletion->u8ConfirmedBlockNo));

            VHSTATLOG(("VHSTAT: audio: received Completion: %d.\n",
                        pCompletion->u8ConfirmedBlockNo));

            /* Pass the event to OUTPUT thread. */
            m_pvrdptp->Client ()-> AudioCompletion (pCompletion->u8ConfirmedBlockNo);
        } break;

        default:
        {
            /* Do nothing, skip unsupported. */
        } break;
    }

    return rc;
}
