/* $Id: videotsmf.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_videotsmf_h
#define VRDP_INCLUDED_SRC_videotsmf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpdefs.h"

/*
 * Defines and data structures for TSMF video redirection channel.
 */

#pragma pack(1)

#define STREAM_ID_STUB  0x80000000
#define STREAM_ID_PROXY 0x40000000
#define STREAM_ID_NONE  0x00000000
#define STREAM_ID_MASK  0xc0000000

#define FN_RIMCALL_RELEASE                 0x00000001
#define FN_RIMCALL_QUERYINTERFACE          0x00000002
#define FN_RIM_EXCHANGE_CAPABILITY_REQUEST 0x00000100

#define RIM_CAPABILITY_VERSION_01          0x00000001

#define FN_PLAYBACK_ACK              0x00000100
#define FN_CLIENT_EVENT_NOTIFICATION 0x00000101

#define FN_EXCHANGE_CAPABILITIES_REQ 0x00000100
#define FN_SET_CHANNEL_PARAMS        0x00000101
#define FN_ADD_STREAM                0x00000102
#define FN_ON_SAMPLE                 0x00000103
#define FN_SET_VIDEO_WINDOW          0x00000104
#define FN_ON_NEW_PRESENTATION       0x00000105
#define FN_SHUTDOWN_PRESENTATION_REQ 0x00000106
#define FN_SET_TOPOLOGY_REQ          0x00000107
#define FN_CHECK_FORMAT_SUPPORT_REQ  0x00000108
#define FN_ON_PLAYBACK_STARTED       0x00000109
#define FN_ON_PLAYBACK_PAUSED        0x0000010a
#define FN_ON_PLAYBACK_STOPPED       0x0000010b
#define FN_ON_PLAYBACK_RESTARTED     0x0000010c
#define FN_ON_PLAYBACK_RATE_CHANGED  0x0000010d
#define FN_ON_FLUSH                  0x0000010e
#define FN_ON_STREAM_VOLUME          0x0000010f
#define FN_ON_CHANNEL_VOLUME         0x00000110
#define FN_ON_END_OF_STREAM          0x00000111
#define FN_SET_ALLOCATOR             0x00000112
#define FN_NOTIFY_PREROLL            0x00000113
#define FN_UPDATE_GEOMETRY_INFO      0x00000114
#define FN_REMOVE_STREAM             0x00000115

#define MMREDIR_CAPABILITY_PLATFORM_MF    0x00000001
#define MMREDIR_CAPABILITY_PLATFORM_DSHOW 0x00000002
#define MMREDIR_CAPABILITY_PLATFORM_OTHER 0x00000004

typedef struct SHARED_MSG_HEADER
{
    uint32_t InterfaceId;
    uint32_t MessageId;
    uint32_t FunctionId;
} SHARED_MSG_HEADER;
AssertCompileSize(SHARED_MSG_HEADER, 12);

typedef struct SHARED_MSG_HEADER_RESPONSE
{
    uint32_t InterfaceId;
    uint32_t MessageId;
} SHARED_MSG_HEADER_RESPONSE;
AssertCompileSize(SHARED_MSG_HEADER_RESPONSE, 8);

typedef struct RIMCALL_RELEASE
{
    SHARED_MSG_HEADER hdr;
} RIMCALL_RELEASE;
AssertCompileSize(RIMCALL_RELEASE, 12);

typedef struct RIM_EXCHANGE_CAPABILITY_REQ
{
    SHARED_MSG_HEADER hdr;
    uint32_t CapabilityValue;
} RIM_EXCHANGE_CAPABILITY_REQ;
AssertCompileSize(RIM_EXCHANGE_CAPABILITY_REQ, 16);

typedef struct RIM_EXCHANGE_CAPABILITY_RESPONSE
{
    SHARED_MSG_HEADER_RESPONSE hdr;
    uint32_t CapabilityValue;
    uint32_t Result;
} RIM_EXCHANGE_CAPABILITY_RESPONSE;
AssertCompileSize(RIM_EXCHANGE_CAPABILITY_RESPONSE, 16);



typedef struct MSG_SET_CHANNEL_PARAMS
{
    SHARED_MSG_HEADER hdr;
    uint8_t  PresentationId[16];
    uint32_t StreamId;
} MSG_SET_CHANNEL_PARAMS;
AssertCompileSize(MSG_SET_CHANNEL_PARAMS, 32);

typedef struct MSG_EXCHANGE_CAPABILITIES_REQ_HDR
{
    SHARED_MSG_HEADER hdr;
    uint32_t numHostCapabilities;
    /* TSMM_CAPABILITIES HostCapabilities[] follow. */
} MSG_EXCHANGE_CAPABILITIES_REQ_HDR;
AssertCompileSize(MSG_EXCHANGE_CAPABILITIES_REQ_HDR, 16);

typedef struct MSG_EXCHANGE_CAPABILITIES_RSP_HDR
{
    SHARED_MSG_HEADER_RESPONSE hdr;
    uint32_t numClientCapabilities;
    /* TSMM_CAPABILITIES ClientCapabilities[]; follow */
    /* uint32_t Result follow; */
} MSG_EXCHANGE_CAPABILITIES_RSP_HDR;
AssertCompileSize(MSG_EXCHANGE_CAPABILITIES_RSP_HDR, 12);

typedef struct TSMM_CAPABILITIES_HDR
{
    uint32_t CapabilityType;
    uint32_t CapabilityLength;
    /* CapabilityData follow. */
} TSMM_CAPABILITIES_HDR;
AssertCompileSize(TSMM_CAPABILITIES_HDR, 8);

typedef struct TSMM_CAPABILITY_VERSION
{
    TSMM_CAPABILITIES_HDR hdr;
    uint32_t Version;
} TSMM_CAPABILITY_VERSION;
AssertCompileSize(TSMM_CAPABILITY_VERSION, 12);

typedef struct TSMM_CAPABILITY_PLATFORM
{
    TSMM_CAPABILITIES_HDR hdr;
    uint32_t Platform;
} TSMM_CAPABILITY_PLATFORM;
AssertCompileSize(TSMM_CAPABILITY_PLATFORM, 12);

typedef struct MSG_EXCHANGE_CAPABILITIES_REQ
{
    SHARED_MSG_HEADER hdr;
    uint32_t numHostCapabilities;
    TSMM_CAPABILITY_VERSION Version;
    TSMM_CAPABILITY_PLATFORM Platform;
} MSG_EXCHANGE_CAPABILITIES_REQ;
AssertCompileSize(MSG_EXCHANGE_CAPABILITIES_REQ, 16 + 12 + 12);

#define TSMM_PLATFORM_COOKIE_UNDEFINED 0
#define TSMM_PLATFORM_COOKIE_MF        1
#define TSMM_PLATFORM_COOKIE_DSHOW     2

typedef struct MSG_NEW_PRESENTATION
{
    SHARED_MSG_HEADER hdr;
    uint8_t  PresentationId[16];
    uint32_t PlatformCookie;
} MSG_NEW_PRESENTATION;
AssertCompileSize(MSG_NEW_PRESENTATION, 32);

#define BYTE uint8_t
#define WORD uint16_t
#define DWORD uint32_t
#define LONG int32_t
#define LONGLONG int64_t
#define REFERENCE_TIME int64_t

typedef struct RECT
{
    LONG    left;
    LONG    top;
    LONG    right;
    LONG    bottom;
} RECT;


typedef struct BITMAPINFOHEADER
{
    DWORD      biSize;
    LONG       biWidth;
    LONG       biHeight;
    WORD       biPlanes;
    WORD       biBitCount;
    DWORD      biCompression;
    DWORD      biSizeImage;
    LONG       biXPelsPerMeter;
    LONG       biYPelsPerMeter;
    DWORD      biClrUsed;
    DWORD      biClrImportant;
} BITMAPINFOHEADER;
AssertCompileSize(BITMAPINFOHEADER, 0x28);

typedef struct VIDEOINFOHEADER
{
    RECT rcSource;
    RECT rcTarget;
    DWORD dwBitRate;
    DWORD dwBitErrorRate;
    REFERENCE_TIME AvgTimePerFrame;
    BITMAPINFOHEADER bmiHeader;
} VIDEOINFOHEADER;
AssertCompileSize(VIDEOINFOHEADER, 0x58);

typedef struct TS_AM_MEDIA_TYPE
{
    uint8_t MajorType[16];
    uint8_t SubType[16];
    uint32_t bFixedSizeSamples;
    uint32_t bTemporalCompression;
    uint32_t SampleSize;
    uint8_t FormatType[16];
    uint32_t cbFormat;
    VIDEOINFOHEADER Format;
} TS_AM_MEDIA_TYPE;

#define NOROLLOVERFLAGS_CHECK_ALTERNATIVE  0x00000000
#define NOROLLOVERFLAGS_IGNORE_ALTERNATIVE 0x00000001

typedef struct MSG_CHECK_FORMAT_SUPPORT_REQ
{
    SHARED_MSG_HEADER hdr;
    uint32_t PlatformCookie;
    uint32_t NoRolloverFlags;
    uint32_t numMediaType;
    TS_AM_MEDIA_TYPE MediaType;
} MSG_CHECK_FORMAT_SUPPORT_REQ;

typedef struct MSG_CHECK_FORMAT_SUPPORT_RSP
{
    SHARED_MSG_HEADER_RESPONSE hdr;
    uint32_t FormatSupported;
    uint32_t PlatformCookie;
    uint32_t Result;
} MSG_CHECK_FORMAT_SUPPORT_RSP;

typedef struct MSG_ADD_STREAM
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t StreamId;
    uint32_t numMediaType;
    TS_AM_MEDIA_TYPE MediaType;
} MSG_ADD_STREAM;

typedef struct MSG_SET_TOPOLOGY_REQ
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
} MSG_SET_TOPOLOGY_REQ;

typedef struct MSG_SET_TOPOLOGY_RSP
{
    SHARED_MSG_HEADER_RESPONSE hdr;
    uint32_t TopologyReady;
    uint32_t Result;
} MSG_SET_TOPOLOGY_RSP;

typedef struct MSG_REMOVE_STREAM
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t StreamId;
} MSG_REMOVE_STREAM;

typedef struct MSG_SHUTDOWN_PRESENTATION_REQ
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
} MSG_SHUTDOWN_PRESENTATION_REQ;

typedef struct MSG_SHUTDOWN_PRESENTATION_RSP
{
    SHARED_MSG_HEADER_RESPONSE hdr;
    uint32_t Result;
} MSG_SHUTDOWN_PRESENTATION_RSP;

typedef struct MSG_ON_PLAYBACK_STARTED
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint64_t PlaybackStartOffset;
    uint32_t IsSeek;
} MSG_ON_PLAYBACK_STARTED;

typedef struct MSG_ON_PLAYBACK_PAUSED
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
} MSG_ON_PLAYBACK_PAUSED;

typedef struct MSG_ON_PLAYBACK_RESTARTED
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
} MSG_ON_PLAYBACK_RESTARTED;

typedef struct MSG_ON_PLAYBACK_STOPPED
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
} MSG_ON_PLAYBACK_STOPPED;

typedef struct MSG_ON_PLAYBACK_RATE_CHANGED
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t NewRate;
} MSG_ON_PLAYBACK_RATE_CHANGED;

typedef struct MSG_SET_ALLOCATOR
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t StreamId;
    uint32_t cBuffers;
    uint32_t cbBuffer;
    uint32_t cbAlign;
    uint32_t cbPrefix;
} MSG_SET_ALLOCATOR;

typedef struct MSG_NOTIFY_PREROLL
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t StreamId;
} MSG_NOTIFY_PREROLL;


#define TSMM_SAMPLE_EXT_CLEANPOINT          0x00000001
#define TSMM_SAMPLE_EXT_DISCONTINUITY       0x00000002
#define TSMM_SAMPLE_EXT_INTERLACED          0x00000004
#define TSMM_SAMPLE_EXT_BOTTOMFIELDFIRST    0x00000008
#define TSMM_SAMPLE_EXT_REPEATFIELDFIRST    0x00000010
#define TSMM_SAMPLE_EXT_SINGLEFIELD         0x00000020
#define TSMM_SAMPLE_EXT_DERIVEDFROMTOPFIELD 0x00000040
#define TSMM_SAMPLE_EXT_HAS_NO_TIMESTAMPS   0x00000080
#define TSMM_SAMPLE_EXT_RELATIVE_TIMESTAMPS 0x00000100
#define TSMM_SAMPLE_EXT_ABSOLUTE_TIMESTAMPS 0x00000200

typedef struct TS_MM_DATA_SAMPLE
{
    uint64_t SampleStartTime;
    uint64_t SampleEndTime;
    uint64_t ThrottleDuration;
    uint32_t SampleFlags;
    uint32_t SampleExtensions;
    uint32_t cbData;
    /* 'Data' */
} TS_MM_DATA_SAMPLE;

typedef struct MSG_ON_SAMPLE
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t StreamId;
    uint32_t numSample;
    TS_MM_DATA_SAMPLE Sample;
} MSG_ON_SAMPLE;

typedef struct MSG_ON_FLUSH
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t StreamId;
} MSG_ON_FLUSH;

typedef struct MSG_ON_END_OF_STREAM
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t StreamId;
} MSG_ON_END_OF_STREAM;

typedef struct MSG_SET_VIDEO_WINDOW
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint64_t VideoWindowId;
    uint64_t HwndParent;
} MSG_SET_VIDEO_WINDOW;

#define TS_WNDFLAG_NEW 0x00000001
#define TS_WNDFLAG_DELETED 0x00000002
#define TS_WNDFLAG_VISRGN 0x00001000

typedef struct GEOMETRY_INFO
{
    uint64_t VideoWindowId;
    uint32_t VideoWindowState;
    uint32_t Width;
    uint32_t Height;
    uint32_t Left;
    uint32_t Top;
    uint64_t Reserved;
    uint32_t ClientLeft;
    uint32_t ClientTop;
    uint32_t Padding;
} GEOMETRY_INFO;

typedef struct TS_RECT
{
    uint32_t Top;
    uint32_t Left;
    uint32_t Bottom;
    uint32_t Right;
} TS_RECT;

typedef struct MSG_UPDATE_GEOMETRY_INFO
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t numGeometryInfo;
    GEOMETRY_INFO GeometryInfo;
    uint32_t cbVisibleRect;
    TS_RECT aVisibleRect[1];
} MSG_UPDATE_GEOMETRY_INFO;

typedef struct MSG_ON_STREAM_VOLUME
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t NewVolume;
    uint32_t bMuted;
} MSG_ON_STREAM_VOLUME;

typedef struct MSG_ON_CHANNEL_VOLUME
{
    SHARED_MSG_HEADER hdr;
    uint8_t PresentationId[16];
    uint32_t ChannelVolume;
    uint32_t ChangedChannel;
} MSG_ON_CHANNEL_VOLUME;


/*
 * Client notification interface.
 */
typedef struct MSG_PLAYBACK_ACK
{
    SHARED_MSG_HEADER hdr;
    uint32_t StreamId;
    uint64_t DataDuration;
    uint64_t cbData;
} MSG_PLAYBACK_ACK;

#define TSMM_CLIENT_EVENT_ENDOFSTREAM     0x0064
#define TSMM_CLIENT_EVENT_STOP_COMPLETED  0x00C8
#define TSMM_CLIENT_EVENT_START_COMPLETED 0x00C9
#define TSMM_CLIENT_EVENT_MONITORCHANGED  0x012C

typedef struct MSG_CLIENT_EVENT_NOTIFICATION_HDR
{
    SHARED_MSG_HEADER hdr;
    uint32_t StreamId;
    uint32_t EventId;
    uint32_t cbData;
    /* Data */
} MSG_CLIENT_EVENT_NOTIFICATION_HDR;

#pragma pack()

#endif /* !VRDP_INCLUDED_SRC_videotsmf_h */
