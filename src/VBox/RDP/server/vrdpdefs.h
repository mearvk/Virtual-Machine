/* $Id: vrdpdefs.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol - Header to include VBox headers and define common VRDP stuff.
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

#ifndef VRDP_INCLUDED_SRC_vrdpdefs_h
#define VRDP_INCLUDED_SRC_vrdpdefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/err.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/alloca.h>
#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/string.h>

#define LOG_GROUP LOG_GROUP_VRDP
#include <VBox/log.h>

#include <VBoxVideo.h>  /* For VBVA definitions. */
#include <VBox/VMMDev.h>

#ifdef _MSC_VER
#pragma warning(disable: 4355) /* 'this' : used in base member initializer list */
#endif /* _MSC_VER */

/* Statistics. */
// #define VRDP_STAT_MSB
// #define VRDP_STAT_TILE

// #define VRDP_ENABLE_LOGREL

#ifdef VRDP_ENABLE_LOGREL
#define LOG_ENABLED
#define VRDPLOG(a) LogRel(a)
#else
#define VRDPLOG(a) Log(a)
#endif

#define VRDPLOGRELIO LogRel5

// #define VRDPSTAT
#define VRDPLOGREL(a) do { LogRel(("VRDP: ")); LogRel(a); } while (0)

/** @todo prefix with "VRDP:" */
#define VRDPLOGRELLIMIT(limit, message) \
    do {                                \
        static int scLogged = 0;        \
        if (scLogged < limit)           \
        {                               \
            scLogged++;                 \
            LogRel(message);            \
        }                               \
    } while (0)

#define VRDP_DEBUG_TEST
//#define VRDP_DEBUG_STREAM
#define VRDP_DEBUG_BC
#define VRDP_DEBUG_BMPCOMP
// #define VRDP_DEBUG_BMPCOMP2
#define VRDP_DEBUG_FB
//#define VRDP_DEBUG_ISO
#define VRDP_DEBUG_MCS
#define VRDP_DEBUG_OUTPUT
#define VRDP_DEBUG_RESIZE
#define VRDP_DEBUG_PACKET
// #define VRDP_DEBUG_RGN
#define VRDP_DEBUG_SEC
#define VRDP_DEBUG_SB
#define VRDP_DEBUG_DU
// #define VRDP_DEBUG_SB2
#define VRDP_DEBUG_TCP
#define VRDP_DEBUG_VRDPTP
#define VRDP_DEBUG_VRDPAPI
#define VRDP_DEBUG_CB
#define VRDP_DEBUG_AUDIO
#define VRDP_DEBUG_CLIPBOARD
#define VRDP_DEBUG_CHANNEL
#define VRDP_DEBUG_USB
#define VRDP_DEBUG_SERVER
#define VRDP_DEBUG_DVC
#define VRDP_DEBUG_SUNFLSH
#define VRDP_DEBUG_VIDEO
#define VRDP_DEBUG_AI


#ifdef VRDP_DEBUG_TEST
#  define TESTLOG(a) do { VRDPLOG(("TEST::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define TESTLOG(a)
#endif /* VRDP_DEBUG_TEST */


#ifdef VRDP_DEBUG_STREAM
#  define STREAMLOG(a) do { VRDPLOG(("STREAM::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define STREAMLOG(a)
#endif /* VRDP_DEBUG_STREAM */


#ifdef VRDP_DEBUG_BC
#  define BCLOG(a) do { VRDPLOG(("BC::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define BCLOG(a)
#endif /* VRDP_DEBUG_BC */



#ifdef VRDP_DEBUG_BMPCOMP
#  define BMPLOG(a) do { VRDPLOG(("BMP::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define BMPLOG(a)
#endif /* VRDP_DEBUG_BMPCOMP */


#ifdef VRDP_DEBUG_BMPCOMP2
#define BMPLOG2(a) BMPLOG(a)
#else
#define BMPLOG2(a)
#endif /* VRDP_DEBUG_BMPCOMP2 */

#ifdef VRDP_DEBUG_FB
#define FBLOG VRDPLOG
#else
#define FBLOG(a)
#endif /* VRDP_DEBUG_FB */

#ifdef VRDP_DEBUG_ISO
#define ISOLOG VRDPLOG
#else
#define ISOLOG(a)
#endif /* VRDP_DEBUG_ISO */

#ifdef VRDP_DEBUG_MCS
#define MCSLOG VRDPLOG
#else
#define MCSLOG(a)
#endif /* VRDP_DEBUG_MCS */

#ifdef VRDP_DEBUG_OUTPUT
#define OUTPUTLOG VRDPLOG
#else
#define OUTPUTLOG(a)
#endif /* VRDP_DEBUG_OUTPUT */

#ifdef VRDP_DEBUG_RESIZE
#  define RESIZELOG(a) do { VRDPLOG(("RESIZE::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define RESIZELOG(a)
#endif /* VRDP_DEBUG_RESIZE */

#ifdef VRDP_DEBUG_PACKET
#define PACKETLOG VRDPLOG
#else
#define PACKETLOG(a)
#endif /* VRDP_DEBUG_PACKET */


#ifdef VRDP_DEBUG_RGN
#  define RGNLOG(a) do { VRDPLOG(("RGN::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define RGNLOG(a)
#endif /* VRDP_DEBUG_RGN */


#ifdef VRDP_DEBUG_SEC
#define SECLOG VRDPLOG
#else
#define SECLOG(a)
#endif /* VRDP_DEBUG_SEC */


#ifdef VRDP_DEBUG_SB
#  define SBLOG(a) do { VRDPLOG(("SB::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define SBLOG(a)
#endif /* VRDP_DEBUG_SB */

#ifdef VRDP_DEBUG_SB2
#  define SB2LOG(a) SBLOG(a)
#else
#  define SB2LOG(a)
#endif /* VRDP_DEBUG_SB2 */

#ifdef VRDP_DEBUG_DU
#  define DULOG(a) do { VRDPLOG(("DU::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define DULOG(a)
#endif /* VRDP_DEBUG_DU */

#ifdef VRDP_DEBUG_TCP
#define TCPLOG VRDPLOG
#else
#define TCPLOG(a)
#endif /* VRDP_DEBUG_TCP */

#ifdef VRDP_DEBUG_VRDPTP
#define VRDPTPLOG VRDPLOG
#else
#define VRDPTPLOG(a)
#endif /* VRDP_DEBUG_VRDPTP */

#ifdef VRDP_DEBUG_VRDPAPI
#define VRDPAPILOG(a) do { VRDPLOG(("VRDPAPI::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#define VRDPAPILOG(a)
#endif /* VRDP_DEBUG_VRDPAPI */

#ifdef VRDP_DEBUG_CB
#define CBLOG VRDPLOG
#else
#define CBLOG(a)
#endif /* VRDP_DEBUG_CB */

#ifdef VRDP_DEBUG_SERVER
#define SERVERLOG VRDPLOG
#else
#define SERVERLOG(a)
#endif /* VRDP_DEBUG_SERVER */

#ifdef VRDP_DEBUG_AUDIO
#  define AUDIOLOG(a) do { VRDPLOG(("AUDIO::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define AUDIOLOG(a)
#endif /* VRDP_DEBUG_AUDIO */

#ifdef VRDP_DEBUG_CLIPBOARD
#  define CLIPBOARDLOG(a) do { VRDPLOG(("CLIPBOARD::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define CLIPBOARDLOG(a)
#endif /* VRDP_DEBUG_CLIPBOARD */

#ifdef VRDP_DEBUG_CHANNEL
#  define CHANNELLOG(a) do { VRDPLOG(("CHANNEL::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define CHANNELLOG(a)
#endif /* VRDP_DEBUG_CHANNEL */

#ifdef VRDP_DEBUG_USB
#  define USBLOG(a) do { VRDPLOG(("USB::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define USBLOG(a)
#endif /* VRDP_DEBUG_USB */

#ifdef VRDP_DEBUG_DVC
#  define DVCLOG(a) do { VRDPLOG(("DVC::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define DVCLOG(a)
#endif /* VRDP_DEBUG_DVC */

#ifdef VRDP_DEBUG_VIDEO
#  define VIDEOLOG(a) do { VRDPLOG(("VIDEO::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define VIDEOLOG(a)
#endif /* VRDP_DEBUG_VIDEO */

#ifdef VRDP_DEBUG_SUNFLSH
#  define SUNFLSHLOG(a) do { VRDPLOG(("SUNFLSH::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define SUNFLSHLOG(a)
#endif /* VRDP_DEBUG_SUNFLSH */

#ifdef VRDP_DEBUG_AI
#  define AILOG(a) do { VRDPLOG(("AI::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define AILOG(a)
#endif /* VRDP_DEBUG_AI */

#endif /* !VRDP_INCLUDED_SRC_vrdpdefs_h */
