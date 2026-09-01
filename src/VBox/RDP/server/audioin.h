/* $Id: audioin.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_audioin_h
#define VRDP_INCLUDED_SRC_audioin_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "utils.h"

#include <iprt/list.h>

#include <VBox/RemoteDesktop/VRDE.h>

class VRDPClient;

typedef DECLCALLBACKTYPE(void, FNAUDIOINPUT,(void *pvCallback,
                                             void *pvCtx,
                                             uint32_t u32ClientId,
                                             uint32_t u32Event,
                                             const void *pvData,
                                             uint32_t cbData));
typedef FNAUDIOINPUT *PFNAUDIOINPUT;

typedef struct SNDIN_AUDIO_FORMAT
{
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
    /* data cbSize bytes */
} SNDIN_AUDIO_FORMAT;

class AudioInputClient;

typedef struct AUDIOIN_SESSION
{
    RTLISTNODE nodeSession;

    AudioInputClient *pAudioInputClient;

    uint32_t u32ClientVersion;

    PFNAUDIOINPUT pfnAudioInputCallback;
    void *pvAudioInputCallback;

    void *pvCtx; /* Application context. */

    uint32_t u32SamplesPerBlock;

    uint8_t u8DVCChannelId;

    SNDIN_AUDIO_FORMAT fmt;

} AUDIOIN_SESSION;

class AudioInputClient
{
    private:
        VRDPClient *m_pClient;

        uint8_t m_u8ActiveDVCChannelId; /* Accessed only from OUTPUT thread */

        VRDPLock *m_pLock;
        RTLISTANCHOR m_listSessions; /* All sessions, used for cleanup. Protected by lock. */

        static void dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData);
        void processInput(AUDIOIN_SESSION *pSession, uint32_t u32Event, const void *pvData, uint32_t cbData);

        void audioCallback(AUDIOIN_SESSION *pSession, uint32_t u32Event, const void *pvData, uint32_t cbData);
        void postAudioInputClose(uint32_t u32Reason, int32_t rc);

        int audioInputStart(void *pvCtx, VRDEAUDIOFORMAT audioFormat, uint32_t u32SamplesPerBlock, PFNAUDIOINPUT pfn, void *pv);

    public:
        AudioInputClient(VRDPClient *pClient);
        ~AudioInputClient();

        void OutputAudioInputOpen(void *pvData, uint32_t cbData);
        void OutputAudioInputData(void *pvData, uint32_t cbData);
        void OutputAudioInputClose(void *pvData, uint32_t cbData);
};

#endif /* !VRDP_INCLUDED_SRC_audioin_h */
