/* $Id: vhstat.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_vhstat_h
#define VRDP_INCLUDED_SRC_vhstat_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "regions.h"

typedef struct VHSTATSTREAM VHSTATSTREAM;

/*
 * Timestamps suffix:
 *  MS - milliseconds;
 *  TL - timeline units 100ns.
 */

class VHStat
{
    public:
        VHStat() {};
        virtual ~VHStat() {};

        virtual int Initialize(void) = 0;
        virtual const char *Description(void) = 0;

        virtual void CreateStreamCtx(VHSTATSTREAM **ppStreamCtx, uint64_t u64CurrentMS, const RGNRECT *prect, uint32_t u32StreamId) = 0;
        virtual void DeleteStreamCtx(VHSTATSTREAM *pStreamCtx, bool fProcessed) = 0;

        virtual void SourceFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64CurrentMS) = 0;
        virtual void SourceFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed) = 0;

        virtual void OutputFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64FrameMS) = 0;
        virtual void OutputFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed) = 0;

        virtual void SendFrameBegin(VHSTATSTREAM *pStreamCtx, uint64_t u64SampleStartTL, uint64_t u64FrameTL) = 0;
        virtual void SendFrameEnd(VHSTATSTREAM *pStreamCtx, bool fProcessed) = 0;

        virtual void AudioStartSamples(uint64_t u64NowNS) = 0;
        virtual void AudioEndSamples(uint64_t u64NowNS) = 0;
        virtual void AudioAppendSamples(uint64_t u64NowNS, int32_t cSamples, uint32_t u32Freq) = 0;
        virtual void AudioConvertSamples(int32_t cSamples, uint32_t u32Freq) = 0;
};

#define VH_STAT_EMPTY 0
#define VH_STAT_FULL 1

int VHStatCreate(VHStat **ppStat, int iLevel);
void VHStatDelete(VHStat *pStat);

/* Always enable VHSTATLOG in builds where logging is enabled. */
#ifdef LOG_ENABLED
#define VHSTATLOG VRDPLOG
#else
/* Note: this must be consistent with videohandler.cpp::vhStatInit */
#define VHSTATLOG LogRel6
#endif

#endif /* !VRDP_INCLUDED_SRC_vhstat_h */
