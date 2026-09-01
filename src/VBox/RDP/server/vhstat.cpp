/* $Id: vhstat.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2011-2025 Oracle and/or its affiliates.
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

#include "vhstat.h"
#include "utils.h"

/*
 * A dummy stats collector which exists but does nothing.
 */
class VHStatEmpty : public VHStat
{
    public:
        VHStatEmpty() {};
        virtual ~VHStatEmpty() {};

        virtual int Initialize(void) { return VINF_SUCCESS; }
        virtual const char *Description(void) { return "empty"; }

        virtual void CreateStreamCtx(VHSTATSTREAM **ppStreamCtx, uint64_t u64CurrentMS, const RGNRECT *prect, uint32_t u32StreamId)
        {
            *ppStreamCtx = NULL;
            NOREF(u64CurrentMS);
            NOREF(prect);
            NOREF(u32StreamId);
        }

        virtual void DeleteStreamCtx(VHSTATSTREAM *pStreamCtx, bool fProcessed)
        {
            NOREF(pStreamCtx);
            NOREF(fProcessed);
        }

        virtual void SourceFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64CurrentMS)
        {
            NOREF(pStreamCtx);
            NOREF(u64CurrentMS);
        }

        virtual void SourceFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed)
        {
            NOREF(pStreamCtx);
            NOREF(fProcessed);
        }

        virtual void OutputFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64FrameMS)
        {
            NOREF(pStreamCtx);
            NOREF(u64FrameMS);
        }

        virtual void OutputFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed)
        {
            NOREF(pStreamCtx);
            NOREF(fProcessed);
        }

        virtual void SendFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64SampleStartTL, uint64_t u64FrameTL)
        {
            NOREF(pStreamCtx);
            NOREF(u64SampleStartTL);
            NOREF(u64FrameTL);
        }

        virtual void SendFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed)
        {
            NOREF(pStreamCtx);
            NOREF(fProcessed);
        }

        virtual void AudioStartSamples(uint64_t u64NowNS)
        {
            NOREF(u64NowNS);
        }

        virtual void AudioEndSamples(uint64_t u64NowNS)
        {
            NOREF(u64NowNS);
        }

        virtual void AudioAppendSamples(uint64_t u64NowNS, int32_t cSamples, uint32_t u32Freq)
        {
            NOREF(u64NowNS);
            NOREF(cSamples);
            NOREF(u32Freq);
        }

        virtual void AudioConvertSamples(int32_t cSamples, uint32_t u32Freq)
        {
            NOREF(cSamples);
            NOREF(u32Freq);
        }
};


/*
 * A complete stats collector.
 */
typedef struct VHSTATAUDIO
{
    /* Common audio stats.
     */
    uint64_t u64SamplesReceivedNS; /* How many samples were received from VM in nanoseconds. */
    uint64_t u64SamplesStartNS;    /* When the first sample was received in the current stream. */
    uint64_t u64SamplesLastNS;     /* When the last samples was received in the current stream. */
    uint64_t u64DurationLastNS;    /* The duration of the last chunk of samples. */

    uint64_t u64LastLogNS;         /* Last logged. */

    uint64_t u64ConvertStartNS;    /* When first samples were converted to the internal format. */
    uint64_t u64ConvertLastLogNS;  /* Last logged to converted samples. */
    uint64_t u64SamplesConvertNS;  /* How many samples were received from VM in nanoseconds. */
    uint64_t u64SamplesConvertSamples; /* How many converted samples. */

    bool fStarted;
} VHSTATAUDIO;

class VHStatFull : public VHStat
{
    private:
        VHSTATAUDIO m_audio;

    public:
        VHStatFull();
        virtual ~VHStatFull();

        virtual int Initialize(void);
        virtual const char *Description(void);

        virtual void CreateStreamCtx(VHSTATSTREAM **ppStreamCtx, uint64_t u64CurrentMS, const RGNRECT *prect, uint32_t u32StreamId);
        virtual void DeleteStreamCtx(VHSTATSTREAM *pStreamCtx, bool fProcessed);

        virtual void SourceFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64CurrentMS);
        virtual void SourceFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed);

        virtual void OutputFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64FrameMS);
        virtual void OutputFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed);

        virtual void SendFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64SampleStartTL, uint64_t u64FrameTL);
        virtual void SendFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed);

        virtual void AudioStartSamples(uint64_t u64NowNS);
        virtual void AudioEndSamples(uint64_t u64NowNS);
        virtual void AudioAppendSamples(uint64_t u64NowNS, int32_t cSamples, uint32_t u32Freq);
        virtual void AudioConvertSamples(int32_t cSamples, uint32_t u32Freq);
};


typedef struct VHSTATFPS
{
    int cFPSFrames;             /* Number of frames during a time interval for FPS calculation. */
    uint64_t u64FPSStartMS;     /* Start timestamp of the time interval used for FPS calculation. */
} VHSTATFPS;

struct VHSTATSTREAM
{
    uint32_t u32StreamId;
    uint64_t u64StreamStartMS;  /* Start timestamp of the stream in milliseconds. */
    int cFrames;                /* Number of frames in the stream. */

    /* What is received from the guest. */
    struct {
        VHSTATFPS fps;
    } src;

    /* What videohandler has processed. */
    struct {
        VHSTATFPS fps;
        uint64_t u64OutputFrameBeginNS;
        uint64_t u64OutputFrameTimeAccumNS;
        uint64_t u64OutputFrameTimeTotalNS;
    } out;

    /* What is sent to the client. */
    struct {
        VHSTATFPS fps;
        uint64_t u64FrameDelayAccumMS;
        uint64_t u64VideoDelayAccumMS;
    } send;
};


VHStatFull::VHStatFull()
{
    RT_ZERO(m_audio);
}

/* virtual */ VHStatFull::~VHStatFull()
{
}

/* virtual */ int VHStatFull::Initialize(void)
{
    return VINF_SUCCESS;
}

/* virtual */ const char *VHStatFull::Description(void)
{
    return "full";
}

/* virtual */ void VHStatFull::CreateStreamCtx(VHSTATSTREAM **ppStreamCtx, uint64_t u64CurrentMS,
                                               const RGNRECT *prect, uint32_t u32StreamId)
{
    VHSTATSTREAM *pStreamCtx = (VHSTATSTREAM *)VRDPMemAllocZ(sizeof (VHSTATSTREAM));

    if (pStreamCtx)
    {
        pStreamCtx->u32StreamId = u32StreamId;
        pStreamCtx->u64StreamStartMS = u64CurrentMS;
        VHSTATLOG(("VHSTAT: stream id%d started %d,%d %dx%d\n",
                   u32StreamId, prect->x, prect->y, prect->w, prect->h));
    }

    *ppStreamCtx = pStreamCtx;
}

/* virtual */ void VHStatFull::DeleteStreamCtx(VHSTATSTREAM *pStreamCtx, bool fProcessed)
{
    if (fProcessed)
    {
        uint64_t u64StreamDurationMS = VRDPTimeMilliTS() - pStreamCtx->u64StreamStartMS;
        uint64_t u64OutConsumed = (10000 * pStreamCtx->out.u64OutputFrameTimeTotalNS) / (1000000 * u64StreamDurationMS);
        VHSTATLOG(("VHSTAT: stream id%d completed: %d frames in %lldms, out: consumed %lldns (%lld.%02lld%%)\n",
                   pStreamCtx->u32StreamId,
                   pStreamCtx->cFrames, u64StreamDurationMS,
                   pStreamCtx->out.u64OutputFrameTimeTotalNS,
                   u64OutConsumed / 100, u64OutConsumed % 100));
    }

    VRDPMemFree(pStreamCtx);
}

/* virtual */ void VHStatFull::SourceFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64CurrentMS)
{
    if (!pStreamCtx)
    {
        return;
    }

    uint64_t u64Duration = u64CurrentMS - pStreamCtx->src.fps.u64FPSStartMS;

    if (pStreamCtx->src.fps.u64FPSStartMS == 0)
    {
        pStreamCtx->src.fps.u64FPSStartMS = u64CurrentMS;
        pStreamCtx->src.fps.cFPSFrames = 1;
    }
    else if (u64Duration >= 1000)
    {
        VHSTATLOG(("VHSTAT: FPS(in): %lld (%d/%llu)\n",
                   (pStreamCtx->src.fps.cFPSFrames * 1000) / u64Duration,
                   pStreamCtx->src.fps.cFPSFrames,
                   u64Duration));

        pStreamCtx->src.fps.u64FPSStartMS = u64CurrentMS;
        pStreamCtx->src.fps.cFPSFrames = 1;
    }
    else
    {
        pStreamCtx->src.fps.cFPSFrames++;
    }
}

/* virtual */ void VHStatFull::SourceFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed)
{
    RT_NOREF2(pStreamCtx, fProcessed);
}

/* virtual */ void VHStatFull::OutputFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64FrameMS)
{
    RT_NOREF1(u64FrameMS);
    if (!pStreamCtx)
    {
        return;
    }

    uint64_t u64CurrentMS = VRDPTimeMilliTS();

    /** @todo the average value of U64CurrentMS - u64FrameMS */

    uint64_t u64Duration = u64CurrentMS - pStreamCtx->out.fps.u64FPSStartMS;

    if (pStreamCtx->out.fps.u64FPSStartMS == 0)
    {
        pStreamCtx->out.fps.u64FPSStartMS = u64CurrentMS;
        pStreamCtx->out.fps.cFPSFrames = 1;
        pStreamCtx->out.u64OutputFrameTimeAccumNS = 0;
    }
    else if (u64Duration >= 1000)
    {
        VHSTATLOG(("VHSTAT: FPS(out): %lld (%d/%llu) consumed %lldns\n",
                   (pStreamCtx->out.fps.cFPSFrames * 1000) / u64Duration,
                   pStreamCtx->out.fps.cFPSFrames,
                   u64Duration,
                   pStreamCtx->out.u64OutputFrameTimeAccumNS));

        pStreamCtx->out.fps.u64FPSStartMS = u64CurrentMS;
        pStreamCtx->out.fps.cFPSFrames = 1;
        pStreamCtx->out.u64OutputFrameTimeAccumNS = 0;
    }
    else
    {
        pStreamCtx->out.fps.cFPSFrames++;
    }

    pStreamCtx->cFrames++;
    pStreamCtx->out.u64OutputFrameBeginNS = VRDPTimeNanoTS();
}

/* virtual */ void VHStatFull::OutputFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed)
{
    if (fProcessed)
    {
        uint64_t u64OutputFrameTimeNS = VRDPTimeNanoTS() - pStreamCtx->out.u64OutputFrameBeginNS;

        pStreamCtx->out.u64OutputFrameTimeAccumNS += u64OutputFrameTimeNS;
        pStreamCtx->out.u64OutputFrameTimeTotalNS += u64OutputFrameTimeNS;
    }

    pStreamCtx->out.u64OutputFrameBeginNS = 0;
}

/* virtual */ void VHStatFull::SendFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64SampleStartTL, uint64_t u64FrameTL)
{
    if (!pStreamCtx)
    {
        return;
    }

    uint64_t u64CurrentMS = VRDPTimeMilliTS();

    uint64_t u64FrameDelayMS = u64CurrentMS - u64FrameTL / 10000;
    uint64_t u64VideoDelayMS = (u64SampleStartTL - u64FrameTL) / 10000;

    uint64_t u64Duration = u64CurrentMS - pStreamCtx->send.fps.u64FPSStartMS;

    if (pStreamCtx->send.fps.u64FPSStartMS == 0)
    {
        pStreamCtx->send.fps.u64FPSStartMS = u64CurrentMS;
        pStreamCtx->send.fps.cFPSFrames = 1;
        pStreamCtx->send.u64FrameDelayAccumMS = u64FrameDelayMS;
        pStreamCtx->send.u64VideoDelayAccumMS = u64VideoDelayMS;
    }
    else if (u64Duration >= 1000)
    {
        int cFrames = pStreamCtx->send.fps.cFPSFrames;

        VHSTATLOG(("VHSTAT: FPS(send): %lld (%d/%llu)\n",
                   (cFrames * 1000) / u64Duration,
                   cFrames,
                   u64Duration));

        VHSTATLOG(("VHSTAT: FPS(send): frame delay %lld video delay %lld\n",
                   cFrames? pStreamCtx->send.u64FrameDelayAccumMS / cFrames: 0,
                   cFrames? pStreamCtx->send.u64VideoDelayAccumMS / cFrames: 0));

        pStreamCtx->send.fps.u64FPSStartMS = u64CurrentMS;
        pStreamCtx->send.fps.cFPSFrames = 1;
        pStreamCtx->send.u64FrameDelayAccumMS = u64FrameDelayMS;
        pStreamCtx->send.u64VideoDelayAccumMS = u64VideoDelayMS;
    }
    else
    {
        pStreamCtx->send.fps.cFPSFrames++;
        pStreamCtx->send.u64FrameDelayAccumMS += u64FrameDelayMS;
        pStreamCtx->send.u64VideoDelayAccumMS += u64VideoDelayMS;
    }
}

/* virtual */ void VHStatFull::SendFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed)
{
    NOREF(pStreamCtx);
    NOREF(fProcessed);
}

DECLINLINE(uint64_t) vhsNanoFromSeconds(uint64_t u64Seconds)
{
    return u64Seconds * UINT64_C(1000000000);
}

/* virtual */ void VHStatFull::AudioStartSamples(uint64_t u64NowNS)
{
    if (m_audio.fStarted)
    {
        return;
    }

    m_audio.u64SamplesReceivedNS = 0;
    m_audio.u64SamplesStartNS = u64NowNS;
    m_audio.u64SamplesLastNS = 0;
    m_audio.u64DurationLastNS = 0;
    m_audio.u64LastLogNS = u64NowNS;
    m_audio.fStarted = true;
    m_audio.u64ConvertStartNS = 0;
    m_audio.u64ConvertLastLogNS = 0;
    m_audio.u64SamplesConvertNS = 0;
    m_audio.u64SamplesConvertSamples = 0;

    VHSTATLOG(("VHSTAT: audio started at %lld\n",
               u64NowNS));
}

/* virtual */ void VHStatFull::AudioEndSamples(uint64_t u64NowNS)
{
    uint64_t u64TimeNoLastNS = m_audio.u64SamplesLastNS - m_audio.u64SamplesStartNS;

    VHSTATLOG(("VHSTAT: audio stopped at %lld ns\n"
               "        real time without last %lld ns\n"
               "        audio time - real time without last %lld ns\n"
               "        last at +%lld ns (-%lld ns), %lld ns\n"
               "",
               u64NowNS,
               u64TimeNoLastNS,
               m_audio.u64SamplesReceivedNS - m_audio.u64DurationLastNS - u64TimeNoLastNS,
               m_audio.u64SamplesLastNS - m_audio.u64SamplesStartNS,
               u64NowNS - m_audio.u64SamplesLastNS,
               m_audio.u64DurationLastNS
             ));

    m_audio.fStarted = false;
}

/* virtual */ void VHStatFull::AudioAppendSamples(uint64_t u64NowNS, int32_t cSamples, uint32_t u32Freq)
{
    /* Time elasped since audio start. */
    uint64_t u64TimeNS = u64NowNS - m_audio.u64SamplesStartNS;

    /* Do a log every 10 seconds. */
    if (u64NowNS - m_audio.u64LastLogNS > vhsNanoFromSeconds(10))
    {
        /* What was received earlier than the current chunk of samples. */
        VHSTATLOG(("VHSTAT: audio real time %llu ns\n"
                   "        audio time - real time %lld ns\n"
                   "",
                   u64TimeNS,
                   m_audio.u64SamplesReceivedNS - u64TimeNS
                 ));

        m_audio.u64LastLogNS = u64NowNS;
    }

    uint64_t u64DurationNS = ((uint64_t)cSamples * vhsNanoFromSeconds(1)) / u32Freq;

    m_audio.u64SamplesReceivedNS += u64DurationNS;
    m_audio.u64SamplesLastNS = u64NowNS;
    m_audio.u64DurationLastNS = u64DurationNS;
}

/* virtual */ void VHStatFull::AudioConvertSamples(int32_t cSamples, uint32_t u32Freq)
{
    uint64_t u64NowNS = VRDPTimeNanoTS();

    if (m_audio.u64ConvertStartNS == 0)
    {
        m_audio.u64ConvertStartNS = u64NowNS;
        m_audio.u64ConvertLastLogNS = u64NowNS;
        m_audio.u64SamplesConvertNS = 0;
        m_audio.u64SamplesConvertSamples = 0;
    }

    /* Time elasped since audio start. */
    uint64_t u64TimeNS = u64NowNS - m_audio.u64SamplesStartNS;

    /* Do a log every 10 seconds. */
    if (u64NowNS - m_audio.u64ConvertLastLogNS > vhsNanoFromSeconds(10))
    {
        /* What was converted earlier than the current chunk of samples. */
        VHSTATLOG(("VHSTAT: audio cnv real time %llu ns\n"
                   "        audio cnv time - real time %lld ns\n"
                   "        audio cnv samples time - real time %lld ns\n"
                   "        audio cnv samples per sec %lld\n"
                   "",
                   u64TimeNS,
                   m_audio.u64SamplesConvertNS - u64TimeNS,
                   ((uint64_t)m_audio.u64SamplesConvertSamples * vhsNanoFromSeconds(1)) / u32Freq - u64TimeNS,
                   m_audio.u64SamplesConvertSamples * vhsNanoFromSeconds(1) / u64TimeNS
                 ));

        m_audio.u64ConvertLastLogNS = u64NowNS;
    }

    uint64_t u64DurationNS = ((uint64_t)cSamples * vhsNanoFromSeconds(1)) / u32Freq;

    m_audio.u64SamplesConvertNS += u64DurationNS;
    m_audio.u64SamplesConvertSamples += cSamples;
}


int VHStatCreate(VHStat **ppStat, int iLevel)
{
    int rc = VINF_SUCCESS;

    VHStat *p = NULL;

    if (iLevel == VH_STAT_EMPTY)
    {
        p = new VHStatEmpty();
    }
    else if (iLevel == VH_STAT_FULL)
    {
        p = new VHStatFull();
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        if (p != NULL)
        {
            rc = p->Initialize();
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }
    }

    if (RT_SUCCESS(rc))
    {
        *ppStat = p;
    }
    else
    {
        if (p)
        {
            delete p;
        }
    }

    return rc;
}

void VHStatDelete(VHStat *pStat)
{
    delete pStat;
}
