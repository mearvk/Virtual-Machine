/* $Id: vrdp.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "vrdp.h"
#include "utils.h"
#include "shadowbuffer.h"
#include "vrdpclient.h"
#include "vrdpserv.h"
#include <iprt/utf16.h>

#ifdef VRDP_DEBUG_VRDPTP
#define VRDP_DUMP_CTX_READ(__ctx) do { (__ctx)->DumpRead (); } while (0)
#define VRDP_DUMP_CTX_WRITE(__ctx) do { (__ctx)->DumpWrite (); } while (0)
#else
#define VRDP_DUMP_CTX_READ(__ctx) do {} while (0)
#define VRDP_DUMP_CTX_WRITE(__ctx) do {} while (0)
#endif /* VRDP_DEBUG_VRDPTP */


/* Colour depth codes. */
#define VRDP_BPP_4 0xca00
#define VRDP_BPP_8 0xca01
#define VRDP_BPP_15 0xca02
#define VRDP_BPP_16 0xca03
#define VRDP_BPP_24 0xca04

/* Supported color depth bit flags for RDPClientCoreData::u16SupportedColorDepths. */
#define RNS_UD_24BPP_SUPPORT 0x0001
#define RNS_UD_16BPP_SUPPORT 0x0002
#define RNS_UD_15BPP_SUPPORT 0x0004
#define RNS_UD_32BPP_SUPPORT 0x0008

/* Flags for RDPClientClusterData::u32Flags. */
#define RDP_CLUSTER_REDIRECTION_SUPPORTED            0x00000001
#define RDP_CLUSTER_REDIRECTION_VERSION_MASK         0x0000003C
#define RDP_CLUSTER_REDIRECTION_VERSION_SHIFT        2
#define RDP_CLUSTER_REDIRECTED_SESSIONID_FIELD_VALID 0x00000002
#define RDP_CLUSTER_REDIRECTED_SMARTCARD             0x00000040

/* Values of 4 bit RDP_CLUSTER_REDIRECTION_VERSION_MASK field. */
#define RDP_CLUSTER_REDIRECTION_VERSION1 0x00
#define RDP_CLUSTER_REDIRECTION_VERSION2 0x01
#define RDP_CLUSTER_REDIRECTION_VERSION3 0x02
#define RDP_CLUSTER_REDIRECTION_VERSION4 0x03
#define RDP_CLUSTER_REDIRECTION_VERSION5 0x04


/* Logon flags */
#define VRDP_LOGON_MOUSE                  0x00000001 /* The client has a mouse. */
#define VRDP_LOGON_DISABLECTRLALTDEL      0x00000002 /* Ctrl_Alt_Del is not required at the logon. */
/* not used                               0x00000004 */
#define VRDP_LOGON_AUTOLOGON              0x00000008 /* Logon using supplied username, password and domain. */
#define VRDP_LOGON_UNICODE                0x00000010 /* Strings are unicode in the packet. */
#define VRDP_LOGON_MAXIMIZESHELL          0x00000020 /* Whether the program must be started in a maximized state. */
#define VRDP_LOGON_LOGONNOTIFY            0x00000040 /* Client want a notification about logon name and session ID. */
#define VRDP_LOGON_COMPRESSION            0x00000080 /* Compression type mask is valid. */
#define VRDP_LOGON_COMPRESSIONTYPEMASK    0x00001e00
#define VRDP_LOGON_ENABLEWINDOWSKEYS      0x00000100 /* Client uses Windows keys on keyboard. Used to be the LOGON_BLOB in rdesktop? */
#define VRDP_LOGON_REMOTECONSOLESOUND     0x00002000 /* Leave sound on the remote computer. */
#define VRDP_LOGON_FORCE_ENCRYPTED_CS_PDU 0x00004000 /* Do not process unencrypted packets from the client (5.2+). */
#define VRDP_LOGON_RAIL                   0x00008000 /* This session is to launch remote programs (6.0+). */
#define VRDP_LOGON_ERRORS                 0x00010000 /* Notify the client about logon errors (6.0+). */
#define VRDP_LOGON_MOUSE_HAS_WHEEL        0x00020000 /* The client mouse has a wheel (6.0+). */
#define VRDP_LOGON_PASSWORD_IS_SC_PIN     0x00040000 /* Password field contains a smart card PIN. (6.0+). */
#define VRDP_LOGON_NOAUDIOPLAYBACK        0x00080000 /* No audio redirection or playback. (6.0+). */
#define VRDP_LOGON_USING_SAVED_CREDS      0x00100000 /* Any user credentials have been retrieved from a credential store. (6.0+). */

#define VRDP_PERF_DISABLE_NOTHING       0x00
#define VRDP_PERF_NO_WALLPAPER          0x01
#define VRDP_PERF_NO_FULLWINDOWDRAG     0x02
#define VRDP_PERF_NO_MENUANIMATIONS     0x04
#define VRDP_PERF_NO_THEMING            0x08
#define VRDP_PERF_NO_CURSOR_SHADOW      0x20
#define VRDP_PERF_NO_CURSORSETTINGS     0x40  /* disables cursor blinking */

#define RDP_CAPSET_GENERAL      1       /* Maps to generalCapabilitySet in T.128 page 138 */
#define RDP_CAPLEN_GENERAL      0x18
#define OS_MAJOR_TYPE_UNIX      4
#define OS_MINOR_TYPE_XSERVER   7

#define RDP_CAPSET_BITMAP       2 /* CAPSTYPE_BITMAP */

#define RDP_CAPSET_ORDER        3
#define RDP_CAPLEN_ORDER        0x58
#define ORDER_CAP_NEGOTIATE     2
#define ORDER_CAP_NOSUPPORT     4

#define RDP_CAPSET_BMPCACHE     4
#define RDP_CAPLEN_BMPCACHE     0x28

#define RDP_CAPSET_CONTROL      5
#define RDP_CAPLEN_CONTROL      0x0C

#define RDP_CAPSET_ACTIVATE     7
#define RDP_CAPLEN_ACTIVATE     0x0C

#define RDP_CAPSET_POINTER      8
#define RDP_CAPLEN_POINTER      0x08

#define RDP_CAPSET_SHARE        9

#define RDP_CAPSET_COLCACHE     10
#define RDP_CAPLEN_COLCACHE     0x08

#define RDP_CAPSET_INPUT        13
#define RDP_CAP_INPUT_SCANCODES 0x0001
#define RDP_CAP_INPUT_8002      0x0004
#define RDP_CAP_INPUT_UNICODE   0x0010 /* param1 == uni char? */
#define RDP_CAP_INPUT_FASTPATH  0x0020

#define RDP_CAPSET_BRUSHCACHE    15

#define RDP_CAPSET_BMPCACHE2    19
#define RDP_CAPLEN_BMPCACHE2    0x28
#define BMPCACHE2_FLAG_PERSIST  ((uint32)1<<31)

#define RDP_CAPSET_VIRTUALCHANNEL 20
#define RDP_CAPLEN_VIRTUALCHANNEL 0x08

#define RDP_CAPSET_LARGE_POINTER 27
#define LARGE_POINTER_FLAG_96x96 0x00000001

#define RDP_SOURCE              "MSTSC"

#define CHANNEL_CHUNK_LENGTH            (1600)
#define CHANNEL_FLAG_FIRST              (0x01)
#define CHANNEL_FLAG_LAST               (0x02)
#define CHANNEL_FLAG_SHOW_PROTOCOL      (0x10)

#define CHANNEL_OPTION_INITIALIZED      0x80000000 /* If not set, then do not setup the channel. */
#define CHANNEL_OPTION_ENCRYPT_RDP      0x40000000 /* Unused. */
#define CHANNEL_OPTION_ENCRYPT_SC       0x20000000 /* Unused. */
#define CHANNEL_OPTION_ENCRYPT_CS       0x10000000 /* Unused. */
#define CHANNEL_OPTION_PRI_HIGH         0x08000000
#define CHANNEL_OPTION_PRI_MED          0x04000000
#define CHANNEL_OPTION_PRI_LOW          0x02000000
#define CHANNEL_OPTION_COMPRESS_RDP     0x00800000 /* Compress if RDP compression is enabled. */
#define CHANNEL_OPTION_COMPRESS         0x00400000 /* Compress always. */
#define CHANNEL_OPTION_SHOW_PROTOCOL    0x00200000 /* Ignored. CHANNEL_FLAG_SHOW_PROTOCOL is used for each PDU. */
#define CHANNEL_OPTION_REMOTE_CONTROL_PERSISTENT 0x00100000 /* Persistent across remote control transactions. */

/* This should be in secure.cpp. */
#define RDP_SEC_REDIRECTION_PKT         0x0400

#define LB_TARGET_NET_ADDRESS           0x00000001
#define LB_LOAD_BALANCE_INFO            0x00000002
#define LB_USERNAME                     0x00000004
#define LB_DOMAIN                       0x00000008
#define LB_PASSWORD                     0x00000010
#define LB_DONTSTOREUSERNAME            0x00000020
#define LB_SMARTCARD_LOGON              0x00000040
#define LB_NOREDIRECT                   0x00000080
#define LB_TARGET_FQDN                  0x00000100
#define LB_TARGET_NETBIOS_NAME          0x00000200
#define LB_TARGET_NET_ADDRESSES         0x00000800

/* we have to limit maximum size of a network packet to 0x3FFF,
 * because rdesktop parses the mcs len incorrectly taking into account
 * only high bit 0x80 of the length first byte.
 * Other clients also take into account the 0x40
 * bit which allow to encode length > 0x3FFF.
 * So limit the length, it will always fit to 2 bytes (0x8000 | length).
 */
#define VRDP_MAX_BITMAP_SIZE    (0x4000 - 512)

VRDPTP::VRDPTP (VRDPDesktop *pdesktop, VRDPClient *pClient)
    :
    m_pdesktop (pdesktop),
    m_pClient (pClient),
    m_audio (this),
    m_usb (this),
    m_clipboard (this),
    m_dvc(this),
    m_sunflsh(this),
    m_rdpdr(this),
    m_data(this)
{
    InitMembers ();
    VRDPLOG(("Going to do Reset\n"));
    Reset ();

    m_audio.SetData(&m_data);
    m_usb.SetData(&m_data);
    m_clipboard.SetData(&m_data);
    m_dvc.SetData(&m_data);
    m_sunflsh.SetData(&m_data);

    m_rdpdr.SetData(&m_data);
}

VRDPTP::~VRDPTP (void)
{
    DestroyMembers ();
}

void VRDPTP::InitMembers (void)
{
    m_keylayout = 0;
    m_clientbuild = 0;

    m_flags = 0;

    m_numchannels = 0;

    m_domain = NULL;
    m_username = NULL;
    m_password = NULL;
    m_program = NULL;
    m_directory = NULL;
    m_clientip = NULL;
    m_dllname = NULL;
    memset(m_client, 0, sizeof(m_client));

    m_timezone = 0;
    m_performanceflags = 0;

    m_bppcode = 0;
    m_bpp = 0;
    m_white = ~0U;
    m_fBitmapCompression = false;
    m_fBitmapCaching = true; //false; /// @todo set according to clients caps.

    m_fRDP5Packets = false;

    memset (&m_caps, 0, sizeof (m_caps));

    m_fSeqInProcess = false;
    m_pSavedInternalOrders = NULL;

    ResetSavedOrders ();

    m_pointerCacheSize = 0;

    m_u32ClientClusterFlags = 0;

    m_userid = 0;

    /* Assume that the client does not handle resize requests. */
    m_fClientResolution = true;

    m_u32BrushSupportLevel = RDP_CAP_BRUSH_DEFAULT;

    /* By default connect to the primary screen. */
    m_uScreenId = 0;

    m_pDesktopMap = NULL;

#ifdef VRDPSTAT
    m_bmpSquare = 0;
    m_bmpBytes  = 0;
    m_bmpCount  = 0;
    m_dataBytes = 0;
    m_orderBytes = 0;
    memset (m_bmpSizes, 0, sizeof (m_bmpSizes));
    m_cCached  = 0;
    m_cCachedTiles  = 0;
    m_cMemblt = 0;
    m_cMembltSquare = 0;

#endif /* VRDPSTAT */
}

void VRDPTP::DestroyMembers (void)
{
    m_audio.Close ();

    m_usb.Close ();

    m_clipboard.Close ();

    m_dvc.Close ();
    m_sunflsh.Close ();

    m_rdpdr.Close ();

    if (m_domain)
    {
        RTStrFree (m_domain);
        m_domain = NULL;
    }

    if (m_username)
    {
        RTStrFree (m_username);
        m_username = NULL;
    }

    if (m_password)
    {
        RTStrFree (m_password);
        m_password = NULL;
    }

    if (m_program)
    {
        RTStrFree (m_program);
        m_program = NULL;
    }

    if (m_directory)
    {
        RTStrFree (m_directory);
        m_directory = NULL;
    }

    if (m_clientip)
    {
        RTStrFree (m_clientip);
        m_clientip = NULL;
    }

    if (m_dllname)
    {
        RTStrFree (m_dllname);
        m_dllname = NULL;
    }

    memset(m_client, 0, sizeof(m_client));

    if (m_pDesktopMap)
    {
        delete m_pDesktopMap;
        m_pDesktopMap = NULL;
    }

    /* Delete the saved internal orders list. */
    VRDPSAVEDINTERNALORDER *pSavedOrder = m_pSavedInternalOrders;
    m_pSavedInternalOrders = NULL;

    while (pSavedOrder)
    {
        VRDPSAVEDINTERNALORDER *next = pSavedOrder->next;

        VRDPMemFree (pSavedOrder);

        pSavedOrder = next;
    }
}

void VRDPTP::setClientName(const char *pszName)
{
    RTStrCopy(m_client, sizeof(m_client), pszName);
}

int VRDPTP::SendLicense (VRDPInputCtx *pInputCtx)
{
    return m_sectp.SendLicense (pInputCtx);
}

int VRDPTP::RegisterChannel (const char *pszName, uint32_t u32Options, uint16_t u16ChannelId)
{
    int rc = VINF_SUCCESS;

    VRDPTPLOG(("VRDPTP::RegisterChannel [%s] flags 0x%08X id 0x%04X\n", pszName, u32Options, u16ChannelId));

    if (RTStrICmp (pszName, "cliprdr") == 0)
    {
        VRDPTPLOG(("VRDPTP::RegisterChannel clipboard\n"));
        rc = m_clipboard.Open (u32Options, u16ChannelId);
    }
    else if (RTStrICmp (pszName, "rdpsnd") == 0)
    {
        VRDPTPLOG(("VRDPTP::RegisterChannel sound\n"));
        rc = m_audio.Open (u32Options, u16ChannelId);
    }
    else if (RTStrICmp (pszName, "vrdpusb") == 0)
    {
        VRDPTPLOG(("VRDPTP::RegisterChannel USB\n"));
        rc = m_usb.Open (u32Options, u16ChannelId);
    }
    else if (RTStrICmp (pszName, "DRDYNVC") == 0)
    {
        VRDPTPLOG(("VRDPTP::RegisterChannel DRDYNVC\n"));
        rc = m_dvc.Open (u32Options, u16ChannelId);
    }
    else if (RTStrICmp (pszName, "SUNFLSH") == 0)
    {
        VRDPTPLOG(("VRDPTP::RegisterChannel SUNFLSH\n"));
        rc = m_sunflsh.Open (u32Options, u16ChannelId);
    }
    else if (RTStrICmp (pszName, "RDPDR") == 0)
    {
        VRDPTPLOG(("VRDPTP::RegisterChannel RDPDR\n"));
        rc = m_rdpdr.Open (u32Options, u16ChannelId);
    }
    else
    {
        VRDPTPLOG(("VRDPTP::RegisterChannel unsupported\n"));
        rc = VERR_VRDP_NOT_SUPPORTED;
    }

    VRDPLOGREL(("Channel: [%s] [%d]. %s.\n",
                pszName, u16ChannelId, RT_SUCCESS(rc)? "Accepted": "Not supported"));

    return rc;
}

int VRDPTP::CheckLogon (void)
{
    VRDPLOGREL(("Logon: %s (%s) build %d. User: [%s] Domain: [%s] Screen: %d\n",
                m_client, m_clientip, m_clientbuild, m_username, m_domain, m_uScreenId));

    if (!shadowBufferVerifyScreenId (m_uScreenId))
    {
        VRDPLOGREL(("Invalid screen. Disconnecting the client!!!.\n"));
        return VINF_VRDP_OPERATION_COMPLETED;
    }

    int rc = m_pdesktop->ClientLogon (Client (),
                                      m_username? m_username: "",
                                      m_password? m_password: "",
                                      m_domain? m_domain: "");

    if (RT_FAILURE (rc))
    {
        /* That will force the client disconnect. */
        rc = VINF_VRDP_OPERATION_COMPLETED;
    }
    else
    {
        /* Get features. */
        Client ()->QueryClientFeatures ();

        Client ()->SetupChannels ();
    }

    return rc;
}

#pragma pack(1)
/* TS_UD_HEADER */
typedef struct RDPSecTagHdr
{
    uint16_t u16Tag;
    uint16_t u16Size;
} RDPSecTagHdr;

/* TS_UD_CS_CORE */
typedef struct RDPClientCoreData
{
    uint32_t u32Version;         /* RDP version: 0x00080001 == RDP 4.0;
                                  *              0x00080004 == RDP 5.0 to 7.0 clients;
                                  *              0x00080005 == RDP 10.0.
                                  */

    uint16_t u16DesktopWidth;    /* The requested desktop width. Maximum 4096 pixels. */
    uint16_t u16DesktopHeight;   /* The requested desktop width. Maximum 2048 pixels. */

    uint16_t u16ColorDepth;      /* 4bpp: 0xca00. 8bpp: 0xca01. Ignored if the postBeta2ColorDepth is present. */
    uint16_t u16SASSequence;     /* Secure access sequence. SHOULD be set to 0xaa03. */

    uint32_t u32KeyboardLayout;  /* Active input locale identifier. */

    uint32_t u32ClientBuild;     /* The build number of the client. rdesktop = 2600. */

    uint16_t au16ClientName[16]; /* Null terminated UCS2 client name. */

    uint32_t u32KeyboardType;    /* 4 == IBM enhanced (101-key or 102-key). */
    uint32_t u32KeyboardSubType; /* OEM dependent value: 0 */
    uint32_t u32KeyboardFunctionKey; /* The number of function keys: 12 */
    uint16_t au16IMEFileName[32]; /* Null terminated IME file name associated with the input locale. */

    /* Optional fields.
     * If a field is not present, all of the subsequent fields MUST NOT be present.
     */

    uint16_t u16PostBeta2ColorDepth; /* VRDP_BPP_*. Ignored if the highColorDepth field is present. */

    uint16_t u16ClientProductId; /* Should be 1. */

    uint32_t u32SerialNumber;    /* Should be 0. */

    uint16_t u16HighColorDepth;  /* The requested color depth in bits per pixel. */
    uint16_t u16SupportedColorDepths; /* Color depths supported by the client. RNS_UD_*BPP_SUPPORT */

    uint16_t u16EarlyCapabilityFlags; /* RNS_UD_CS_*. 1 */

    uint8_t au8ClientDigProductId[64]; /* Value that uniquely identifies the client. */

    uint8_t u8ConnectionType;    /* Type of network connection being used by the client.
                                  * Only contains valid data if the RNS_UD_CS_VALID_CONNECTION_TYPE (0x0020)
                                  * flag is present in the earlyCapabilityFlags field.
                                  */

    uint8_t u8Pad1;

    uint32_t u32ServerSelectedProtocol; /* value returned by the server in the selectedProtocol field
                                         * of the RDP Negotiation Response.
                                         */
} RDPClientCoreData;

/* TS_UD_CS_CLUSTER */
typedef struct RDPClientClusterData
{
    uint32_t u32Flags;               /* RDP_CLUSTER_* */
    uint32_t u32RedirectedSessionId; /* If REDIRECTED_SESSIONID_FIELD_VALID is set */
} RDPClientClusterData;

/* TS_UD_CS_SEC */
typedef struct RDPClientSecurityData
{
    uint32_t u32EncryptionMethods;    /* SEC_ENCRYPTION_FLAG_* */
    uint32_t u32ExtEncryptionMethods; /* MUST be set to 0, if u32EncryptionMethods != 0. */
} RDPClientSecurityData;

/* CHANNEL_DEF */
typedef struct RDPClientChannelDef
{
    uint8_t au8Name[8];
    uint32_t u32Options;
} RDPClientChannelDef;

/* TS_UD_CS_NET */
typedef struct RDPClientNetworkData
{
    uint32_t u32ChannelCount; /* maximum allowed 31. */
    RDPClientChannelDef aChannelDefArray[1];
} RDPClientNetworkData;

/* TS_UD_CS_MONITOR */
typedef struct RDPClientMonitorData
{
    uint32_t u32Flags;        /* Unused and reserved. */
    uint32_t u32MonitorCount; /* Maximum allowed 16. */

    TS_MONITOR_DEF aMonitorDefArray[1];
} RDPClientMonitorData;

#pragma pack ()

int VRDPTP::RecvConnectInitial (VRDPInputCtx *pInputCtx)
{
    /* Generic Conference Control (T.124) ConferenceCreateRequest:
     *  unknown 16_be 5
     *  unknown 16_be 14
     *  unknown 8     0x7c
     *  unknown 16_be 1
     *  16_be (remaining length | 0x8000)
     *  unknown 16_be 6
     *  unknown 16_be 16
     *  unknown 8     0
     *  unknown 16_le 0xc001
     *  unknown 8     0
     *  32_le OEM ID: 'Duca' in rdesktop
     *  16_be (remaining length | 0x8000)
     */
    const uint8_t *pu8 = pInputCtx->Read (23);

    if (pu8 == NULL)
    {
        VRDPTPLOG(("failed to read ConferenceCreateRequest!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    VRDPData *pData = pInputCtx->Data();

    while (pInputCtx->BytesToRead () > 0)
    {
        const RDPSecTagHdr *pSecTagHdr = (const RDPSecTagHdr *)pInputCtx->Read (sizeof (RDPSecTagHdr));

        if (pSecTagHdr == NULL)
        {
            VRDPTPLOG(("Failed to read the SecTagHdr!!!\n"));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        const int cbPacket = pSecTagHdr->u16Size - sizeof (RDPSecTagHdr);
        if (cbPacket < 0)
        {
            VRDPTPLOG(("Invalid SecTagHdr %d!!!\n", pSecTagHdr->u16Size));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        switch (pSecTagHdr->u16Tag)
        {
            case SEC_TAG_CLI_INFO:
            {
                /* The data contain optional fields. So take all available bytes, but access
                 * only actually present fields.
                 */
                if (cbPacket < RT_OFFSETOF(RDPClientCoreData, u16PostBeta2ColorDepth))
                {
                    VRDPTPLOG(("SEC_TAG_CLI_INFO is too small %d (must be at least %d)!!!\n",
                               cbPacket, RT_OFFSETOF(RDPClientCoreData, u16PostBeta2ColorDepth)));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                const RDPClientCoreData *pClientCoreData = (const RDPClientCoreData *)pInputCtx->Read (cbPacket);

                if (pClientCoreData == NULL)
                {
                    VRDPTPLOG(("failed to read SEC_TAG_CLI_INFO!!!\n"));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPTPLOG(("protocol version: 0x%08X\n", pClientCoreData->u32Version));

                if (RT_HI_U16(pClientCoreData->u32Version) != 0x0008)
                {
                    VRDPLOGREL(("Unsupported protocol version: 0x%08X\n", pClientCoreData->u32Version));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                pData->DesktopWidth(pClientCoreData->u16DesktopWidth);
                pData->DesktopHeight(pClientCoreData->u16DesktopHeight);
                VRDPTPLOG(("desktop: %dx%d\n", pClientCoreData->u16DesktopWidth, pClientCoreData->u16DesktopHeight));

                m_keylayout = pClientCoreData->u32KeyboardLayout;
                VRDPTPLOG(("m_keylayout: 0x%04X\n", m_keylayout));

                m_clientbuild = pClientCoreData->u32ClientBuild;

                char *pszClientName = &m_client[0];
                AssertCompile(sizeof(pClientCoreData->au16ClientName[0]) == sizeof(RTUTF16));
                RTUtf16ToUtf8Ex((PCRTUTF16)pClientCoreData->au16ClientName,
                                RT_ELEMENTS(pClientCoreData->au16ClientName),
                                &pszClientName, sizeof(m_client), NULL);
                VRDPTPLOG(("m_client: %s\n", m_client));

                /* Optional fields. */

                m_bpp = 0;
                m_bppcode = 0;

                if (cbPacket >= RT_OFFSETOF(RDPClientCoreData, u16PostBeta2ColorDepth)
                                + (int)RT_SIZEOFMEMB(RDPClientCoreData, u16PostBeta2ColorDepth))
                {
                    m_bppcode = pClientCoreData->u16PostBeta2ColorDepth; /* color depth code: VRDP_BPP_* */
                }

                if (cbPacket >= RT_OFFSETOF(RDPClientCoreData, u16HighColorDepth)
                                + (int)RT_SIZEOFMEMB(RDPClientCoreData, u16HighColorDepth))
                {
                    m_bpp = pClientCoreData->u16HighColorDepth;
                    VRDPTPLOG(("Client's bpp is %d\n", m_bpp));
                }
                else
                {
                    /* If u16HighColorDepth is not present. */
                    switch (m_bppcode)
                    {
                        case VRDP_BPP_8:  m_bpp = 8; break;
                        case VRDP_BPP_15: m_bpp = 15; break;
                        case VRDP_BPP_16: m_bpp = 16; break;
                        case VRDP_BPP_24: m_bpp = 24; break;
                        default: break;
                    }
                }

                if (m_bpp == 0)
                {
                    VRDPLOGREL(("Unsupported client color depth code: 0x%04X\n", m_bppcode));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                if (cbPacket >= RT_OFFSETOF(RDPClientCoreData, u16EarlyCapabilityFlags)
                                + (int)RT_SIZEOFMEMB(RDPClientCoreData, u16EarlyCapabilityFlags))
                {
                    /* RNS_UD_CS_* */
                    VRDPTPLOG(("Client's earlyCapabilityFlags is 0x%04X\n", pClientCoreData->u16EarlyCapabilityFlags));
                    pData->EarlyCapabilityFlags(pClientCoreData->u16EarlyCapabilityFlags);

                    if (   pClientCoreData->u16EarlyCapabilityFlags & RNS_UD_CS_WANT_32BPP_SESSION
                        && m_bpp == 24)
                    {
                        /** @todo m_bpp = 32; */
                    }
                }

                VRDPTPLOG(("Selected bpp is %d\n", m_bpp));

                if (cbPacket >= RT_OFFSETOF(RDPClientCoreData, au8ClientDigProductId)
                                + (int)RT_SIZEOFMEMB(RDPClientCoreData, au8ClientDigProductId))
                {
                    VRDPTPLOG(("Client's au8ClientDigProductId is\n%.*Rhxd\n",
                               RT_SIZEOFMEMB(RDPClientCoreData, au8ClientDigProductId),
                               pClientCoreData->au8ClientDigProductId));
                    pData->ClientDigProductId(pClientCoreData->au8ClientDigProductId);
                }
            } break;

            case SEC_TAG_CLI_CLUSTER:
            {
                if (cbPacket < (int)sizeof (RDPClientClusterData))
                {
                    VRDPTPLOG(("SEC_TAG_CLI_4 is too small %d (must be at least %d)!!!\n",
                               cbPacket, sizeof (RDPClientClusterData)));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                const RDPClientClusterData *pClientClusterData = (const RDPClientClusterData *)pInputCtx->Read (cbPacket);

                if (pClientClusterData == NULL)
                {
                    VRDPTPLOG(("failed to read SEC_TAG_CLI_CLUSTER!!!\n"));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                m_u32ClientClusterFlags = pClientClusterData->u32Flags;
            } break;

            case SEC_TAG_CLI_CRYPT:
            {
                /* This information is only relevant when Standard RDP Security mechanisms will be used. */
                if (cbPacket < (int)sizeof (RDPClientSecurityData))
                {
                    VRDPTPLOG(("SEC_TAG_CLI_CRYPT is too small %d (must be at least %d)!!!\n",
                               cbPacket, sizeof (RDPClientSecurityData)));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                const RDPClientSecurityData *pClientSecurityData = (const RDPClientSecurityData *)pInputCtx->Read (cbPacket);

                if (pClientSecurityData == NULL)
                {
                    VRDPTPLOG(("failed to read SEC_TAG_CLI_CRYPT!!!\n"));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPLOGREL(("Methods 0x%08x\n",
                            pClientSecurityData->u32EncryptionMethods));

                /* Do not enable Standard RDP Security if Enhanced Security is used. */
                uint32_t u32EncryptionMethods = pData->EnhancedSecurity()?
                                                    0:
                                                    pClientSecurityData->u32EncryptionMethods;

                int rc = m_sectp.EnableEncryption (u32EncryptionMethods);

                if (RT_FAILURE(rc))
                {
                    VRDPLOGREL(("Failed to enable secure connection %Rrc.\n", rc));
                    return rc;
                }
            } break;

            case SEC_TAG_CLI_CHANNELS:
            {
                if (cbPacket < RT_OFFSETOF(RDPClientNetworkData, aChannelDefArray))
                {
                    VRDPTPLOG(("SEC_TAG_CLI_CHANNELS is too small %d (must be at least %d)!!!\n",
                               cbPacket, RT_OFFSETOF(RDPClientNetworkData, aChannelDefArray)));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                const RDPClientNetworkData *pClientNetworkData = (const RDPClientNetworkData *)pInputCtx->Read (cbPacket);

                if (pClientNetworkData == NULL)
                {
                    VRDPTPLOG(("failed to read SEC_TAG_CLI_CHANNELS!!!\n"));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                const uint32_t u32ChannelCount = pClientNetworkData->u32ChannelCount;

                /* Number of CHANNEL_DEF structs in the packet. */
                const uint32_t cChannels = (cbPacket - RT_OFFSETOF(RDPClientNetworkData, aChannelDefArray)) / sizeof(RDPClientChannelDef);

                /* [MS-RDPBCGR]: 2.2.1.3.4 Client Network Data (TS_UD_CS_NET):
                 * "The number of requested static virtual channels (the maximum allowed is 31)."
                 */
                if (   u32ChannelCount > 31
                    || u32ChannelCount > cChannels)
                {
                    VRDPTPLOG(("Invalid number of channels %u (packet can contain %d)!!!\n", u32ChannelCount, cChannels));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                m_numchannels = u32ChannelCount;

                VRDPTPLOG(("ConnectInitial numchannels %d\n", m_numchannels));

                uint32_t i;
                for (i = 0; i < m_numchannels; i++)
                {
                    char achName[sizeof (pClientNetworkData->aChannelDefArray[i].au8Name) + 1];
                    memcpy (achName, pClientNetworkData->aChannelDefArray[i].au8Name, sizeof (pClientNetworkData->aChannelDefArray[i].au8Name));
                    achName[sizeof (pClientNetworkData->aChannelDefArray[i].au8Name)] = 0;

                    /// @todo in rdesktop the options are sent as 'be' should be 'le'as in MS client???
                    VRDPTPLOG(("ConnectInitial channel %s (%8.8x)\n", achName, pClientNetworkData->aChannelDefArray[i].u32Options));

                    RegisterChannel (achName, pClientNetworkData->aChannelDefArray[i].u32Options, MCS_GLOBAL_CHANNEL + 1 + i);
                }
            } break;

            case SEC_TAG_CLI_MONITOR:
            {
                if (cbPacket < RT_OFFSETOF(RDPClientMonitorData, aMonitorDefArray))
                {
                    VRDPTPLOG(("SEC_TAG_CLI_MONITOR is too small %d (must be at least %d)!!!\n",
                               cbPacket, RT_OFFSETOF(RDPClientMonitorData, aMonitorDefArray)));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                const RDPClientMonitorData *pClientMonitorData = (const RDPClientMonitorData *)pInputCtx->Read (cbPacket);

                if (pClientMonitorData == NULL)
                {
                    VRDPTPLOG(("failed to read SEC_TAG_CLI_MONITOR!!!\n"));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPTPLOG(("SEC_TAG_CLI_MONITOR: flags 0x%08X, count %d\n",
                           pClientMonitorData->u32Flags, pClientMonitorData->u32MonitorCount));

                const uint32_t u32MonitorCount = pClientMonitorData->u32MonitorCount;

                /* Number of TS_MONITOR_DEF structs in the packet. */
                const uint32_t cMonitors = (cbPacket - RT_OFFSETOF(RDPClientMonitorData, aMonitorDefArray)) / sizeof(TS_MONITOR_DEF);

                /* 2.2.1.3.6 Client Monitor Data (TS_UD_CS_MONITOR):
                 * "The number of display monitor definitions in the monitorDefArray field (the maximum allowed is 16)."
                 */
                if (   u32MonitorCount > 16
                    || u32MonitorCount > cMonitors)
                {
                    VRDPTPLOG(("Invalid number of monitors %u (packet can contain %d)!!!\n", u32MonitorCount, cMonitors));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                pData->MonitorLayout(true);
                pData->MonitorCount(u32MonitorCount);

                uint32_t i;
                for (i = 0; i < u32MonitorCount; i++)
                {
                    const TS_MONITOR_DEF *p = &pClientMonitorData->aMonitorDefArray[i];

                    VRDPTPLOG(("SEC_TAG_CLI_MONITOR: [%02d] %d-%d, %d-%d, flags 0x%08X\n",
                               i,
                               p->i32Left,
                               p->i32Right,
                               p->i32Top,
                               p->i32Bottom,
                               p->u32Flags));

                    pData->MonitorDef(i, p);
                }
            } break;

            default:
                VRDPLOGREL(("Unsupported SEC_TAG: 0x%04X/%d. Skipping.\n", pSecTagHdr->u16Tag, pSecTagHdr->u16Size));
                pu8 = pInputCtx->Read (cbPacket);
                if (pu8 == NULL)
                {
                    VRDPTPLOG(("failed to read unsupported SEC_TAG!!!\n"));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }
                break;
        }
    }

    return VINF_VRDP_SUCCESS;
}

int VRDPTP::SendConnectResponse (VRDPInputCtx *pInputCtx, uint8_t u8Result)
{
    return m_sectp.SendConnectResponse (pInputCtx, u8Result, m_numchannels);
}

#define VRDP_SCALAR_FIELD_READ(__t, __dst) do {                                                     \
    pu8 = pInputCtx->Read (sizeof (__t));                                                           \
                                                                                                    \
    if (pu8 == NULL)                                                                                \
    {                                                                                               \
        VRDPTPLOG(("Recv logon info failed to read %s %s at line %d!!!\n", #__t, #__dst, __LINE__));\
        return VERR_VRDP_PROTOCOL_ERROR;                                                            \
    }                                                                                               \
                                                                                                    \
    (__dst) = *(__t *)pu8;                                                                          \
} while (0)

#define VRDP_SCALAR_FIELD_SKIP(__t) do {                                                            \
    pu8 = pInputCtx->Read (sizeof (__t));                                                           \
                                                                                                    \
    if (pu8 == NULL)                                                                                \
    {                                                                                               \
        VRDPTPLOG(("Recv logon info failed to skip %s at line %d!!!\n", #__t, __LINE__));           \
        return VERR_VRDP_PROTOCOL_ERROR;                                                            \
    }                                                                                               \
} while (0)

#define VRDP_STRING_FIELD_READ(__cb, __dst) do {                                                    \
    pu8 = pInputCtx->Read ((__cb) + 2);                                                             \
                                                                                                    \
    if (pu8 == NULL || *(uint16_t *)&pu8[__cb] != 0)                                                \
    {                                                                                               \
        VRDPTPLOG(("Recv logon info failed to read %s = %d %s line %d!!!\n", #__cb, __cb, #__dst, __LINE__));  \
        return VERR_VRDP_PROTOCOL_ERROR;                                                            \
    }                                                                                               \
                                                                                                    \
    RTUtf16ToUtf8 ((RTUTF16 *)pu8, &(__dst));                                                       \
} while (0)

#define VRDP_FIELD_SKIP(__cb) do {                                                                  \
    pu8 = pInputCtx->Read ((__cb));                                                                 \
                                                                                                    \
    if (pu8 == NULL)                                                                                \
    {                                                                                               \
        VRDPTPLOG(("Recv logon info failed to skip %s line %d!!!\n", #__cb, __LINE__));             \
        return VERR_VRDP_PROTOCOL_ERROR;                                                            \
    }                                                                                               \
} while (0)


int VRDPTP::RecvLogonInfo (VRDPInputCtx *pInputCtx)
{
    /*
     * Parse logon info.
     * 2.2.1.11.1.1 Info Packet (TS_INFO_PACKET)
     * It consists of many variable length fields, someof which are optional,
     * so parse it field by field.
     */
    const uint8_t *pu8;

    pInputCtx->DumpRead ();

    uint16_t cbdomain = 0, cbusername = 0, cbpassword = 0,
             cbprogram = 0, cbdirectory = 0, cbip = 0, cbdllname = 0;

    uint32_t u32Codepage;
    VRDP_SCALAR_FIELD_READ(uint32_t, u32Codepage);      /* Codepage used by the client, unless INFO_UNICODE flag is set. */

    VRDP_SCALAR_FIELD_READ(uint32_t, m_flags);          /* VRDP_LOGON_ flags */
    VRDPTPLOG(("logon flags: %8.8X\n", m_flags));

    VRDP_SCALAR_FIELD_READ(uint16_t, cbdomain);         // domain string length (excluding the mandatory null terminator)
    VRDP_SCALAR_FIELD_READ(uint16_t, cbusername);       // username string length (excluding the mandatory null terminator)
    VRDP_SCALAR_FIELD_READ(uint16_t, cbpassword);       // password string length (excluding the mandatory null terminator)
    VRDP_SCALAR_FIELD_READ(uint16_t, cbprogram);        // program string length (excluding the mandatory null terminator)
    VRDP_SCALAR_FIELD_READ(uint16_t, cbdirectory);      // directory string length (excluding the mandatory null terminator)

    VRDP_STRING_FIELD_READ(cbdomain, m_domain);
    VRDPTPLOG(("domain: %s\n", m_domain? m_domain: ""));

    /* After reading the field the 'pu8' points to original USC2 string.
     * Domain might specify the screen "@1". Find the last '@' in the string,
     * and decode the number. Both no suffix and "@1" means primary screen.
     */
    PRTUTF16 pucs2Domain = (PRTUTF16)pu8;
    PRTUTF16 pucs2DomainLast = NULL;

    while (*pucs2Domain != 0)
    {
        if (*pucs2Domain == L'@')
        {
            pucs2DomainLast = pucs2Domain;
        }

        pucs2Domain++;
    }

    if (pucs2DomainLast)
    {
        /* Found a point, skip it and verify that it is followed by digits. */
        unsigned uScreenId = 0;

        PRTUTF16 pucs2DomainPoint = pucs2DomainLast;

        pucs2DomainLast++;

        pucs2Domain = pucs2DomainLast;
        while (*pucs2Domain
               && (L'0' <= *pucs2Domain && *pucs2Domain <= L'9'))
        {
            uScreenId = uScreenId * 10  + *pucs2Domain - '0';
            pucs2Domain++;
        }

        if (   *pucs2Domain == 0
            && pucs2Domain != pucs2DomainLast)
        {
            /* 'pucs2DomainLast' points to a string of digits. */
            if (uScreenId > 0) uScreenId--; /* The screenId is 0 based. */
            VRDPTPLOG(("screen string: %ls, %d\n", pucs2DomainLast, uScreenId));
            m_uScreenId = uScreenId;

            /* Adjust the m_domain string by removing the .dd suffix. */
            size_t cChars = pucs2Domain - pucs2DomainPoint;
            size_t cbDomain = strlen (m_domain);
            Assert (cbDomain >= cChars);
            m_domain[cbDomain - cChars] = 0;
            VRDPTPLOG(("cChars = %d, cbDomain = %d, m_domain = %s\n", cChars, cbDomain, m_domain));
        }
    }

    VRDP_STRING_FIELD_READ(cbusername, m_username);
    VRDPTPLOG(("username: %s\n", m_username? m_username: ""));

    VRDP_STRING_FIELD_READ(cbpassword, m_password);
    VRDPTPLOG(("password: %s\n", m_password? m_password: ""));

    VRDP_STRING_FIELD_READ(cbprogram, m_program);
    VRDPTPLOG(("program: %s\n", m_program? m_program: ""));

    VRDP_STRING_FIELD_READ(cbdirectory, m_directory);
    VRDPTPLOG(("directory: %s\n", m_directory? m_directory: ""));

    /*
     * Extra info starts here (5.0+).
     */

    VRDP_SCALAR_FIELD_SKIP(uint16_t);                   // Skip clientAddressFamily (always AF_INET (0x0002))

    VRDP_SCALAR_FIELD_READ(uint16_t, cbip);             // client ip string length including the trailing null

    if (cbip > 0)
    {
        if (cbip == 1)
        {
            VRDPLOGREL(("Recv logon info: incorrect client address length.\n"));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        /* Read the string if it is there. */
        VRDP_STRING_FIELD_READ(cbip - 2, m_clientip);
        VRDPTPLOG(("client ip: %s\n", m_clientip? m_clientip: ""));
        /// @todo verify that the IP is correct?
    }

    VRDP_SCALAR_FIELD_READ(uint16_t, cbdllname);        // a dll name string length including the trailing null
                                                        // "C:\\WINNT\\System32\\mstscax.dll"

    if (cbdllname != 0)
    {
        if (cbdllname == 1)
        {
            VRDPLOGREL(("Recv logon info: incorrect working directory length.\n"));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        /* Read the string if it is there.
         *
         * A version of the MS RDP client for Mac does not null terminate the field,
         * so a special processing is required.
         */
        pu8 = pInputCtx->Read (cbdllname);

        if (pu8 == NULL)
        {
            VRDPLOGREL(("Recv logon info failed to read the dllname = %d!!!\n", cbdllname));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        if (*(uint16_t *)&pu8[cbdllname - 2] != 0)
        {
            /* Not a null terminated. Make a null terminated copy. */
            uint8_t *pu8Tmp = (uint8_t *)alloca (cbdllname + sizeof (uint16_t));

            memcpy (pu8Tmp, pu8, cbdllname);
            *(uint16_t *)&pu8Tmp[cbdllname] = 0;

            RTUtf16ToUtf8 ((PRTUTF16)pu8Tmp, &m_dllname);
        }
        else
        {
            RTUtf16ToUtf8 ((PRTUTF16)pu8, &m_dllname);
        }

        VRDPTPLOG(("dllname: %s\n", m_dllname? m_dllname: ""));
    }

    /*
     * Time zone information.
     */
    VRDP_SCALAR_FIELD_READ(int32_t, m_timezone);     // clients timezone offset,
                                                     // difference with GMT in minutes
    VRDPTPLOG(("timezone: %d\n", m_timezone));

    VRDP_FIELD_SKIP(64);                   /* Timezone normal. */

    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 0xa0000
    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 0x50000
    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 3
    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 0
    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 0

    VRDP_FIELD_SKIP(64);                   /* Timezone DST. */

    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 0x30000
    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 0x50000
    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 2
    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 0
    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // unknown 32_le 0xffffffc4


    VRDP_SCALAR_FIELD_SKIP(uint32_t);      // client session id

    VRDP_SCALAR_FIELD_READ(uint32_t, m_performanceflags); // VRDP_PERF flags
    VRDPTPLOG(("Performance flags: 0x%08X\n", m_performanceflags));

    /* Skip the rest of the packet. */
    uint32_t cbAfterPerformanceFlags = pInputCtx->BytesToRead ();
    VRDPTPLOG(("cbAfterPerformanceFlags: 0x%08X\n", cbAfterPerformanceFlags));
    VRDP_FIELD_SKIP(cbAfterPerformanceFlags);

    /* Guess which client has connected. By default assume that it is a MSFT
     * client that does not support resizing. That is set in the init members.
     */
    Assert (m_fClientResolution == true);

    if (   m_flags < 0x4000                                             /* rdesktop does not use flags > 0x2000 */
        && m_dllname != NULL
        && RTStrCmp (m_dllname, "C:\\WINNT\\System32\\mstscax.dll") == 0  /* this is hardcoded in rdesktop */
        && m_clientbuild <= 2600                                        /* rdesktop 1.5 claims to be 2600 */
        && u32Codepage == 0                                             /* rdesktop hardcodes this to 0 */
       )
    {
        /* Likely it is rdesktop. */
        m_fClientResolution = false;
        m_sectp.SetRdesktop (true);
    }

    VRDPLOGREL(("Client seems to be %s.\n", m_fClientResolution? "MSFT": "rdesktop"));

    /* Here it is enough information to choose the desktop mode. */
    int rc = VRDPClientDesktopMap::CreateDesktopMap(&m_pDesktopMap, this, m_uScreenId);
    if (RT_FAILURE(rc))
    {
        AssertRC(rc);
        return rc;
    }

    return CheckLogon ();
}
#undef VRDP_FIELD_SKIP
#undef VRDP_STRING_FIELD_READ
#undef VRDP_SCALAR_FIELD_SKIP
#undef VRDP_SCALAR_FIELD_READ


#pragma pack(1)
typedef struct _RDPDemandActiveHdr
{
    uint16_t u16UserId;
    uint32_t u32ShareId;
    uint16_t u16SourceLength;
    uint16_t u16CapabilitiesLength;
    uint8_t  au8Source[4];
} RDPDemandActiveHdr;

typedef struct _RDPAllCapsHdr
{
    uint16_t u16CapSets;
    uint16_t u16Pad;
} RDPAllCapsHdr;

typedef struct _RDPCapsHdr
{
    uint16_t u16CapSet;
    uint16_t u16CapLen;
} RDPCapsHdr;

typedef struct _RDPCapsInput
{
    uint16_t u16Flags;
    uint16_t u16Pad;
    uint8_t  au8Unknown[ 88 /* total length */
                        - 4 /* caps hdr */
                        - 4 /* u16Flags and u16Pad */]; /* MS RDP 6.01 requires this. */
} RDPCapsInput;

typedef struct _RDPCapsPointer
{
    uint16_t u16ColorFlag;  /* Ignored, assumed to be TRUE. */
    uint16_t u16CacheSize;  /* Slots in the 24BPP Color Pointer Update cache. */
    uint16_t u16CacheSize2; /* Slots in the New Color Pointer Update cache. */
} RDPCapsPointer;

typedef struct _RDPCapsLargePointer
{
    uint16_t u16LargePointerSupportFlags;
} RDPCapsLargePointer;

typedef struct _RDPCapsBrushCache
{
    uint32_t u32BrushSupportLevel;
} RDPCapsBrushCache;

typedef struct _RDPCapsColCache
{
    uint32_t u32TableCacheSize;
} RDPCapsColCache;

/* TS_ORDER_CAPABILITYSET */
typedef struct _RDPCapsOrder
{
    uint8_t au8TerminalDescriptor[16]; /* Ignored and SHOULD be set to all zeros. */
    uint32_t u32Pad1;
    uint16_t u16DesktopSaveXGranularity; /* Ignored and assumed to be 1. */
    uint16_t u16DesktopSaveYGranularity; /* Ignored and assumed to be 20. */
    uint16_t u16Pad2;
    uint16_t u16MaximumOrderLevel;       /* Ignored and SHOULD be set to ORD_LEVEL_1_ORDERS (1). */
    uint16_t u16NumberFonts;             /* Ignored and SHOULD be set to 0. */
    uint16_t u16OrderFlags;              /* TS_NEGOTIATEORDERSUPPORT, etc. */
    uint8_t au8OrderSupport[32];
    uint16_t u16TextFlags;               /* Values in this field MUST be ignored. */
    uint16_t u16OrderSupportExFlags;     /* TS_ORDERFLAGS_EX_* */
    uint32_t u32Pad3;
    uint32_t u32DesktopSaveSize;         /* This field is ignored by the client and assumed to be 230400 bytes (480 * 480). */
    uint16_t u16Pad4;
    uint16_t u16Pad5;
    uint16_t u16TextANSICodePage;        /* Ignored by the client and SHOULD be set to 0 by the server. */
    uint16_t u16Pad6;
} RDPCapsOrder;

/* TS_BITMAP_CAPABILITYSET */
typedef struct RDPCapsBitmap
{
    uint16_t u16PreferredBitsPerPixel; /* The server MUST set this field to the color depth of the session. */

    uint16_t u16Receive1BitPerPixel;   /* Ignored and SHOULD be set to TRUE (0x0001). */
    uint16_t u16Receive4BitPerPixel;   /* Ignored and SHOULD be set to TRUE (0x0001). */
    uint16_t u16Receive8BitPerPixel;   /* Ignored and SHOULD be set to TRUE (0x0001). */

    uint16_t u16DesktopWidth;
    uint16_t u16DesktopHeight;

    uint16_t u16Pad;

    uint16_t u16DesktopResizeFlag;     /* Resizing the desktop by using a deactivation-reactivation sequence. */
    uint16_t u16BitmapCompressionFlag; /* MUST be set to TRUE (0x0001). */

    uint8_t  u8HighColorFlags;         /* This field is ignored and SHOULD be set to 0. */
    uint8_t  u8DrawingFlags;           /* Flags describing support for 32 bpp bitmaps: DRAW_ALLOW_*. */

    uint16_t u16MultipleRectangleSupport; /* MUST be set to TRUE (0x0001). */
    uint16_t u16Pad2;
} RDPCapsBitmap;

/* RDPCapsGeneral::u16RDP5Flag */
#define FASTPATH_OUTPUT_SUPPORTED 0x0001

typedef struct _RDPCapsGeneral
{
    uint16_t u16OSMajorType;
    uint16_t u16OSMinorType;
    uint16_t u16ProtocolVersion;
    uint16_t u16Pad1;
    uint16_t u16GeneralCompressionTypes; /* MUST be 0. */
    uint16_t u16RDP5Flag;                /* rdp5 "fast output" flag in bit0. */
    uint16_t u16UpdateCapabilityFlag;    /* MUST be 0. */
    uint16_t u16RemoteUnshareFlag;       /* MUST be 0. */
    uint16_t u16GeneralCompressionLevel; /* MUST be 0. */
    uint8_t  u8RefreshRectSupport;
    uint8_t  u8SuppressOutputSupport;
} RDPCapsGeneral;

/* TS_SHARE_CAPABILITYSET */
typedef struct RDPCapsShare
{
    uint16_t u16NodeId;
    uint16_t u16Pad1;
} RDPCapsShare;

typedef struct _RDPCapsVirtualChannel
{
    uint32_t u32Flags;
} RDPCapsVirtualChannel;

typedef struct RDPCapsBmpCache
{
    uint32_t pad1;
    uint32_t pad2;
    uint32_t pad3;
    uint32_t pad4;
    uint32_t pad5;
    uint32_t pad6;
    uint16_t Cache1Entries;
    uint16_t Cache1MaximumCellSize;
    uint16_t Cache2Entries;
    uint16_t Cache2MaximumCellSize;
    uint16_t Cache3Entries;
    uint16_t Cache3MaximumCellSize;
} RDPCapsBmpCache;

#define PERSISTENT_KEYS_EXPECTED_FLAG 0x0001
#define ALLOW_CACHE_WAITING_LIST_FLAG 0x0002

typedef struct RDPCapsBmpCache2
{
    uint16_t CacheFlags;
    uint8_t pad2;
    uint8_t NumCellCaches;
    uint32_t CellCacheInfo[5];
    uint8_t Pad3[12];
} RDPCapsBmpCache2;
#pragma pack()

static void vrdpWriteCapsHdr (VRDPInputCtx *pInputCtx, uint16_t u16CapSet, uint16_t u16CapLen)
{
    RDPCapsHdr *p = (RDPCapsHdr *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsHdr));
    Assert (p);

    p->u16CapSet = u16CapSet;
    p->u16CapLen = u16CapSet? u16CapLen + sizeof (RDPCapsHdr): 0;
}

int VRDPTP::SendDemandActive (VRDPInputCtx *pInputCtx)
{
    VRDPTPLOG(("SendDemandActive\n"));

    Assert (m_enmStatus == VRDP_RDP_Status_RDP);

    ResetSavedOrders ();

    uint16_t cCapSets = 0;

    /*
     * Send server capabilities from last to first.
     */

    /*
     * Trailing NULL caps. Not included in the cCapSets counter.
     */
    vrdpWriteCapsHdr (pInputCtx, 0, 0);


    /*
     * Send virtual channel caps. Some clients can't use channels at all, if this is not sent.
     */
    RDPCapsVirtualChannel *pCapsVirtualChannel = (RDPCapsVirtualChannel *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsVirtualChannel));
    Assert (pCapsVirtualChannel);

    pCapsVirtualChannel->u32Flags = 0;

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_VIRTUALCHANNEL, sizeof (RDPCapsVirtualChannel));
    cCapSets++;

    /*
     * Send input caps.
     */
    RDPCapsInput *pCapsInput = (RDPCapsInput *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsInput));
    Assert (pCapsInput);

    pCapsInput->u16Flags = RDP_CAP_INPUT_SCANCODES;
    pCapsInput->u16Pad   = 0;
    memset (pCapsInput->au8Unknown, 0, sizeof (pCapsInput->au8Unknown));

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_INPUT, sizeof (RDPCapsInput));
    cCapSets++;


    /*
     * Send Pointer caps.
     */
    RDPCapsPointer *pCapsPointer = (RDPCapsPointer *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsPointer));
    Assert (pCapsPointer);

    pCapsPointer->u16ColorFlag = 1;
    pCapsPointer->u16CacheSize = 64;
    pCapsPointer->u16CacheSize2 = 64;

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_POINTER, sizeof (RDPCapsPointer));
    cCapSets++;


    /*
     * Send Large Pointer caps.
     */
    RDPCapsLargePointer *pCapsLargePointer = (RDPCapsLargePointer *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsLargePointer));
    Assert (pCapsLargePointer);

    pCapsLargePointer->u16LargePointerSupportFlags = LARGE_POINTER_FLAG_96x96;

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_LARGE_POINTER, sizeof (RDPCapsLargePointer));
    cCapSets++;

    /*
     * Send Color Cache caps.
     */
    RDPCapsColCache *pCapsColCache = (RDPCapsColCache *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsColCache));
    Assert (pCapsColCache);

    pCapsColCache->u32TableCacheSize = 6;

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_COLCACHE, sizeof (RDPCapsColCache));
    cCapSets++;


    /*
     * Send order caps.
     */
    RDPCapsOrder *pCapsOrder = (RDPCapsOrder *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsOrder));
    Assert (pCapsOrder);

    memset(pCapsOrder->au8TerminalDescriptor, 0, sizeof(pCapsOrder->au8TerminalDescriptor));
    pCapsOrder->u32Pad1                    = 0xf4240; /* Same as the other server. */
    pCapsOrder->u16DesktopSaveXGranularity = 1;
    pCapsOrder->u16DesktopSaveYGranularity = 20;
    pCapsOrder->u16Pad2                    = 0;
    pCapsOrder->u16MaximumOrderLevel       = 1;
    pCapsOrder->u16NumberFonts             = 0;
    pCapsOrder->u16OrderFlags              =   TS_NEGOTIATEORDERSUPPORT
                                             | TS_ZEROBOUNDSDELTASSUPPORT
                                             | TS_COLORINDEXSUPPORT
                                             | TS_ORDERFLAGS_EXTRA_FLAGS;

    /* The server reports the same order support set as the MS RDP server. */
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_DESTBLT           ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_PATBLT            ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_SCREENBLT         ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_MEMBLT            ] = 1;

    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_TRIBLT            ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_5                 ] = 0;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_6                 ] = 0;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_DRAWNINEGRID      ] = 1;

    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_LINE              ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_MULTI_DRAWNINEGRID] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_RECT              ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_DESKSAVE          ] = 1;

    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_12                ] = 0;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_13                ] = 0;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_14                ] = 0;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_MULTI_DESTBLT     ] = 1;

    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_MULTI_PATBLT      ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_MULTI_SCREENBLT   ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_MULTI_RECT        ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_FAST_INDEX        ] = 1;

    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_POLYGON           ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_POLYGON2          ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_POLYLINE          ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_23                ] = 0;

    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_FAST_GLYPH        ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_ELLIPSE           ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_ELLIPSE2          ] = 1;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_TEXT2             ] = 1;

    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_28                ] = 0;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_29                ] = 0;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_30                ] = 0;
    pCapsOrder->au8OrderSupport[RDP_ORDER_NEG_INDEX_31                ] = 0;

    pCapsOrder->u16TextFlags               = 0x06a1;
    pCapsOrder->u16OrderSupportExFlags     = TS_ORDERFLAGS_EX_ALTSEC_FRAME_MARKER_SUPPORT;
    pCapsOrder->u32Pad3                    = 0xf4240; /* Same as the other server. */
    pCapsOrder->u32DesktopSaveSize         = 0xf4240; /* Same as the other server. */
    pCapsOrder->u16Pad4                    = 1; /* Same as the other server. */
    pCapsOrder->u16Pad5                    = 0;
    pCapsOrder->u16TextANSICodePage        = 0;
    pCapsOrder->u16Pad6                    = 0;

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_ORDER, sizeof (RDPCapsOrder));
    cCapSets++;


    /*
     * Send bitmap caps
     */
    RDPCapsBitmap *pCapsBitmap = (RDPCapsBitmap *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsBitmap));
    Assert (pCapsBitmap);

    VRDPTPLOG(("Sending bpp %d\n", m_bpp));
    pCapsBitmap->u16PreferredBitsPerPixel    = m_bpp;

    pCapsBitmap->u16Receive1BitPerPixel      = 1;
    pCapsBitmap->u16Receive4BitPerPixel      = 1;
    pCapsBitmap->u16Receive8BitPerPixel      = 1;
    pCapsBitmap->u16DesktopWidth             = DesktopMap()->GetClientWidth();
    pCapsBitmap->u16DesktopHeight            = DesktopMap()->GetClientHeight();
    pCapsBitmap->u16Pad                      = 0;
    pCapsBitmap->u16DesktopResizeFlag        = 1;
    pCapsBitmap->u16BitmapCompressionFlag    = 1;
    pCapsBitmap->u8HighColorFlags            = 0;
    pCapsBitmap->u8DrawingFlags              = 0; /** @todo TS_DRAW_ALLOW_SKIP_ALPHA */
    pCapsBitmap->u16MultipleRectangleSupport = 1;
    pCapsBitmap->u16Pad2                     = 0;

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_BITMAP, sizeof (RDPCapsBitmap));
    cCapSets++;


    /*
     * Send general caps.
     */
    RDPCapsGeneral *pCapsGeneral = (RDPCapsGeneral *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsGeneral));
    Assert (pCapsGeneral);

    pCapsGeneral->u16OSMajorType             = 1; /* Windows */
    pCapsGeneral->u16OSMinorType             = 3; /* NT */
    pCapsGeneral->u16ProtocolVersion         = 0x0200;
    pCapsGeneral->u16Pad1                    = 0;
    pCapsGeneral->u16GeneralCompressionTypes = 0;
    pCapsGeneral->u16RDP5Flag                = FASTPATH_OUTPUT_SUPPORTED;
    pCapsGeneral->u16UpdateCapabilityFlag    = 0;
    pCapsGeneral->u16RemoteUnshareFlag       = 0;
    pCapsGeneral->u16GeneralCompressionLevel = 0;
    pCapsGeneral->u8RefreshRectSupport       = 1;
    pCapsGeneral->u8SuppressOutputSupport    = 1;

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_GENERAL, sizeof (RDPCapsGeneral));
    cCapSets++;


    /*
     * Send Share caps.
     */
    RDPCapsShare *pCapsShare = (RDPCapsShare *)pInputCtx->AppendProtocolHeader (sizeof (RDPCapsShare));
    Assert (pCapsShare);

    pCapsShare->u16NodeId = 0x03EA; /* This field SHOULD be set to the server channel ID by the server (0x03EA). */
    pCapsShare->u16Pad1 = 0;

    vrdpWriteCapsHdr (pInputCtx, RDP_CAPSET_SHARE, sizeof (RDPCapsShare));
    cCapSets++;


    uint16_t u16CombinedCapsSize = pInputCtx->SizeOfWrite ();

    /*
     * All caps header.
     */
    RDPAllCapsHdr *pAllHdr = (RDPAllCapsHdr *)pInputCtx->AppendProtocolHeader (sizeof (RDPAllCapsHdr));
    Assert (pAllHdr);

    pAllHdr->u16CapSets = cCapSets;
    pAllHdr->u16Pad     = 0;

    /*
     * Demand active header.
     */
    RDPDemandActiveHdr *pHdr = (RDPDemandActiveHdr *)pInputCtx->AppendProtocolHeader (sizeof (RDPDemandActiveHdr));
    Assert (pHdr);

    pHdr->u16UserId             = 0;
    pHdr->u32ShareId            = 0x00010001; /* "consist of the MCS User ID (in the most significant 16 bits)
                                               *  and a monotonically increasing unsigned and wrapping counter
                                               * (in the least significant 16 bits)." */
    pHdr->u16SourceLength       = 4;
    pHdr->u16CapabilitiesLength = u16CombinedCapsSize;
    memcpy (pHdr->au8Source, "RDP", 4);

    return SendPDU (pInputCtx, RDP_PDU_DEMAND_ACTIVE);
}

#pragma pack (1)
typedef struct _RDPDisconnect
{
    uint32_t u32Reason;
} RDPDisconnect;
#pragma pack ()

void VRDPTP::Disconnect (VRDPInputCtx *pInputCtx, uint32_t u32Reason)
{
    VRDPLOGREL(("Logoff: %s (%s) build %d. User: [%s] Domain: [%s] Reason 0x%04X.\n",
                m_client, m_clientip, m_clientbuild, m_username, m_domain, u32Reason));

    /* Send RDP_DATA_PDU_DISCONNECT. */
    RDPDisconnect *pDisconnect = (RDPDisconnect *)pInputCtx->AppendProtocolHeader (sizeof (RDPDisconnect));
    Assert (pDisconnect);

    pDisconnect->u32Reason = u32Reason;

    SendData (pInputCtx, RDP_DATA_PDU_DISCONNECT); /// @todo this is actually SET_ERROR_INFO

    SendDeactivate (pInputCtx);

    m_sectp.Disconnect (pInputCtx);
}

#pragma pack (1)
typedef struct _RDPRedirectUniStr
{
    uint32_t u32Length;
    uint16_t au16UniStr[1];
} RDPRedirectUniStr;

typedef struct _RDPRedirectCookie
{
    uint32_t u32Length;
    uint8_t au8Cookie[1];
} RDPRedirectCookie;

typedef struct _RDPRedirectFlags
{
    uint32_t u32Flags;
} RDPRedirectFlags;

typedef struct _RDPSessionID
{
    uint32_t u32SessionID;
} RDPSessionID;
#pragma pack ()

static int vrdpUtf8ToUtf16(const char *pszString, PRTUTF16 *ppwszUniStr)
{
    if (!pszString || !ppwszUniStr)
    {
        return VERR_INVALID_PARAMETER;
    }

    /// @todo RTUtf8ToUtf16

    size_t cbStr = strlen (pszString);

    PRTUTF16 pwszUniStr = (PRTUTF16)VRDPMemAlloc ((cbStr+1) * 2);
    if (!pwszUniStr)
    {
        return VERR_NO_MEMORY;
    }

    size_t i;
    for (i = 0; i < cbStr; i++)
    {
        pwszUniStr[i] = (uint16_t)pszString[i];
    }
    pwszUniStr[i] = '\0';

    *ppwszUniStr = pwszUniStr;

    return VINF_SUCCESS;
}


static void vrdpWriteRedirectUniStr (VRDPInputCtx *pInputCtx, const char *pszString)
{
    PRTUTF16 pwszUniStr = NULL;
    size_t cUniLen = 0;

    if (pszString)
    {
        int rc = vrdpUtf8ToUtf16(pszString, &pwszUniStr);

        if (RT_SUCCESS (rc))
        {
            cUniLen = RTUtf16Len (pwszUniStr);
        }
    }

    RDPRedirectUniStr *p = (RDPRedirectUniStr *)pInputCtx->AppendProtocolHeader (sizeof (RDPRedirectUniStr) + ((cUniLen+1)*2) - sizeof (p->au16UniStr[0]));
    Assert (p);

    p->u32Length = (uint32_t)((cUniLen+1)*2);

    if (pwszUniStr)
    {
        memcpy (p->au16UniStr, pwszUniStr, p->u32Length);
    }

    if (pwszUniStr)
    {
        /// @todo RTStrFree
        VRDPMemFree (pwszUniStr);
    }
}

void VRDPTP::Redirect (VRDPInputCtx *pInputCtx,
                       uint32_t u32Flags,
                       const char *pszServer,
                       const char *pszUser,
                       const char *pszDomain,
                       const char *pszPassword,
                       uint32_t u32SessionId,
                       const char *pszCookie)
{
    /** @todo if (EnhancedSecurity()) TS_ENHANCED_SECURITY_SERVER_REDIRECTION */
    if (pszPassword)
    {
        vrdpWriteRedirectUniStr (pInputCtx, pszPassword);
        u32Flags |= LB_PASSWORD;
    }

    if (pszDomain)
    {
        vrdpWriteRedirectUniStr (pInputCtx, pszDomain);
        u32Flags |= LB_DOMAIN;
    }

    if (pszUser)
    {
        vrdpWriteRedirectUniStr (pInputCtx, pszUser);
        u32Flags |= LB_USERNAME;
    }

    if (pszCookie)
    {
        RDPRedirectCookie *p = (RDPRedirectCookie *)pInputCtx->AppendProtocolHeader (sizeof (RDPRedirectCookie) + strlen(pszCookie) - sizeof(p->au8Cookie[0]));
        Assert (p);
        p->u32Length = (uint32_t)strlen(pszCookie);
        memcpy(p->au8Cookie, pszCookie, p->u32Length);
        u32Flags |= LB_LOAD_BALANCE_INFO;
    }

    if (pszServer)
    {
        u32Flags |= LB_TARGET_NET_ADDRESS;
        vrdpWriteRedirectUniStr (pInputCtx, pszServer);
    }

    {
        RDPRedirectFlags *p = (RDPRedirectFlags *)pInputCtx->AppendProtocolHeader (sizeof (RDPRedirectFlags));
        Assert (p);
        p->u32Flags = u32Flags;
    }

    {
        RDPSessionID *p = (RDPSessionID *)pInputCtx->AppendProtocolHeader (sizeof (RDPSessionID));
        Assert (p);
        p->u32SessionID = u32SessionId;
    }

    uint8_t *pu8Length = pInputCtx->AppendProtocolHeader(2);
    uint8_t *pu8Flags  = pInputCtx->AppendProtocolHeader(2);

    *(uint16_t *)pu8Flags  = RDP_SEC_REDIRECTION_PKT;
    *(uint16_t *)pu8Length = pInputCtx->SizeOfWrite ();   /* Total PDU length, including this header. */

    VRDPTPLOG(("Sending redirection packet\n"));
    m_sectp.Send(3, pInputCtx, RDP_SEC_REDIRECTION_PKT);
}

void VRDPTP::Reconnect (VRDPInputCtx *pInputCtx)
{
    Redirect (pInputCtx, 0, NULL, NULL, NULL, NULL, 0, NULL);
}

int VRDPTP::ProcessChannel (VRDPInputCtx *pInputCtx, uint32_t u32Length, uint32_t u32Flags)
{
    VRDPTPLOG(("ProcessChannel: id %d\n", pInputCtx->QueryIncomingChannelId ()));

    VRDPChannel *pChannel = NULL;

    if (m_audio.Id () == pInputCtx->QueryIncomingChannelId ())
    {
        pChannel = &m_audio;
    }
    else if (m_usb.Id () == pInputCtx->QueryIncomingChannelId ())
    {
        pChannel = &m_usb;
    }
    else if (m_clipboard.Id () == pInputCtx->QueryIncomingChannelId ())
    {
        pChannel = &m_clipboard;
    }
    else if (m_dvc.Id () == pInputCtx->QueryIncomingChannelId ())
    {
        pChannel = &m_dvc;
    }
    else if (m_sunflsh.Id () == pInputCtx->QueryIncomingChannelId ())
    {
        pChannel = &m_sunflsh;
    }
    else if (m_rdpdr.Id () == pInputCtx->QueryIncomingChannelId ())
    {
        pChannel = &m_rdpdr;
    }

    if (pChannel)
    {
        if ((u32Flags & (CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST)) == (CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST))
        {
            /* Not fragmented, call the corresponding channel processor. */
            const uint8_t *pu8 = pInputCtx->Read (u32Length);

            if (pu8)
            {
                pChannel->ProcessChannelInput (pu8, u32Length);
            }
        }
        else
        {
            VRDPTPLOG(("WARNING: chunked channel input flags = 0x%08X\n", u32Flags));
            pInputCtx->DumpRead ();

            /* Input is chunked, put the current chunk in the channels defragmentation packet. */
            if (u32Flags & CHANNEL_FLAG_FIRST)
            {
                pChannel->DefragmentationBegin (u32Length);
            }

            pChannel->DefragmentationAdd (pInputCtx);

            if (u32Flags & CHANNEL_FLAG_LAST)
            {
                const uint8_t *pu8 = pChannel->DefragmentationEnd (&u32Length);

                pChannel->ProcessChannelInput (pu8, u32Length);

                pChannel->DefragmentationRelease ();
            }
        }
    }

    return VINF_SUCCESS;
}

#pragma pack(1)
typedef struct _RDPChannelHdr
{
    uint32_t u32Length;
    uint32_t u32Flags;
} RDPChannelHdr;
#pragma pack()

/* Send to channel from a few linear buffers and optionally prepend data with total length. */
int VRDPTP::SendToChannel (VRDPOutputCtx *pOutputCtx, uint16_t u16ChannelId, uint32_t u32Options,
                           int cBuffers, const VRDPBUFFER *paBuffers, bool fInsertLengthField)
{
    RT_NOREF1(u16ChannelId);
    int rc = VINF_SUCCESS;

    uint8_t au8OutputBuffer[VRDP_NETWORK_PACKET_SIZE_ALLOC];

    uint32_t u32ExtraFlags = (u32Options & CHANNEL_OPTION_SHOW_PROTOCOL)? CHANNEL_FLAG_SHOW_PROTOCOL : 0;

    /* Compute the total length of buffers. */
    uint32_t cbBuffers = 0;

    int i;
    for (i = 0; i < cBuffers; i++)
    {
        cbBuffers += paBuffers[i].cb;
    }

    /* Channel header information. */
    uint32_t u32Flags = CHANNEL_FLAG_FIRST;
    uint32_t u32TotalLength = cbBuffers + (fInsertLengthField? sizeof (uint32_t): 0);

    /* Data pointer inside the buffers */
    int iBufferIndex            = -1;
    const uint8_t *pu8BufferSrc = NULL;
    uint32_t cbBufferLeft       = 0;

    uint32_t cbToSend = u32TotalLength;

    while (cbToSend > 0)
    {
        /* Destination pointer. */
        uint8_t *pu8Dst = &au8OutputBuffer[VRDP_LOW_PROTOCOL_RESERVE_SIZE];

        uint32_t cbChunk = RT_MIN (cbToSend, CHANNEL_CHUNK_LENGTH);

        if (cbToSend <= CHANNEL_CHUNK_LENGTH)
        {
            u32Flags |= CHANNEL_FLAG_LAST;
        }

        RDPChannelHdr *pHdr = (RDPChannelHdr *)pu8Dst;
        pu8Dst += sizeof (RDPChannelHdr);

        pHdr->u32Length = u32TotalLength;
        pHdr->u32Flags  = u32Flags | u32ExtraFlags;

        if (   fInsertLengthField
            && (u32Flags & CHANNEL_FLAG_FIRST))
        {
            /* All data is prepended with the length. */
            *(uint32_t *)pu8Dst = cbBuffers;
            pu8Dst += sizeof (uint32_t);

            cbChunk -= sizeof (uint32_t);
            cbToSend -= sizeof (uint32_t);
        }

        cbToSend -= cbChunk;

        /* Put buffers to the packet. */
        while (cbChunk)
        {
            Log(("1) cbChunk = %d, cb = %d, index = %d\n", cbChunk, cbBufferLeft, iBufferIndex));

            if (cbBufferLeft == 0)
            {
                /* No more bytes left in the current buffer. Switch to next buffer, skip zero length ones. */
                do {
                    iBufferIndex++;
                    AssertRelease (iBufferIndex < cBuffers);
                    pu8BufferSrc = (uint8_t *)paBuffers[iBufferIndex].pv;
                    cbBufferLeft = paBuffers[iBufferIndex].cb;
                } while (cbBufferLeft == 0);
            }

            Log(("cbBufferLeft = %d\n", cbBufferLeft));

            if (cbBufferLeft > cbChunk)
            {
                /* The buffer contains more data than required for the chunk.
                 * Write part of the buffer to the packet.
                 */
                memcpy (pu8Dst, pu8BufferSrc, cbChunk);
                pu8Dst += cbChunk;

                pu8BufferSrc += cbChunk;
                cbBufferLeft -= cbChunk;
                cbChunk = 0;

                Log(("2) cbChunk = %d, cb = %d, index = %d\n", cbChunk, cbBufferLeft, iBufferIndex));
            }
            else if (cbBufferLeft > 0)
            {
                /* The buffer contains less data than required for the chunk.
                 * Write entire buffer to the packet.
                 */
                memcpy (pu8Dst, pu8BufferSrc, cbBufferLeft);
                pu8Dst += cbBufferLeft;

                cbChunk -= cbBufferLeft;
                cbBufferLeft = 0;

                Log(("3) cbChunk = %d, cb = %d, index = %d\n", cbChunk, cbBufferLeft, iBufferIndex));
            }
        }

        pOutputCtx->SetOutputBuffer (&au8OutputBuffer[VRDP_LOW_PROTOCOL_RESERVE_SIZE],
                                     &au8OutputBuffer[0],
                                     pu8Dst - &au8OutputBuffer[VRDP_LOW_PROTOCOL_RESERVE_SIZE]);

#ifdef VRDP_DEBUG_CHANNEL
        CHANNELLOG(("CHANNEL: SendToChannel: %d.\n", u16ChannelId));
        pOutputCtx->DumpWrite ();
#endif /* VRDP_DEBUG_CHANNEL */

        rc = m_sectp.Send (3, pOutputCtx, 0);

        CHANNELLOG(("CHANNEL: SendToChannel: rc = %Rrc.\n", rc));

        if (RT_FAILURE(rc))
        {
            break;
        }

        u32Flags = 0;
    }

    return rc;
}


#pragma pack (1)
typedef struct _RDPPalette
{
    uint16_t u16Pad1;
    uint16_t u16Entries;
    uint16_t u16Pad2;

    /* 256 entries of 3 bytes RGB values. */
    uint8_t au8RGB[256 * 3];
} RDPPalette;

typedef struct _RDPColCache
{
    uint8_t  u8Flags;
    uint16_t u16Length;
    uint16_t u16SecondaryFlags;
    uint8_t  u8Type;

    uint8_t  u8CacheId;
    uint16_t u16Entries;

    /* 256 entries of 3 bytes RGB values + 1 byte pad. */
    uint8_t au8RGB[256 * 4];
} RDPColCache;
#pragma pack ()

void VRDPTP::OutputPalette (VRDPStream *pStream)
{
    const VRDPStream::MemoryBlock *pBlock;

    /* Reserve enough space for an palette update. */
    pBlock = pStream->BeginBlock (VRDPUpdate_Palette, sizeof (RDPPalette));

    if (pBlock == NULL)
    {
        return;
    }

    RDPPalette *pRDPPalette = (RDPPalette *)pBlock->pu8DstStart;

    pRDPPalette->u16Pad1 = 0;
    pRDPPalette->u16Entries = 256;
    pRDPPalette->u16Pad2 = 0;

    memcpy (pRDPPalette->au8RGB, colorPalette256, sizeof (colorPalette256));

    /* Complete the block. Pass how many bytes were actually written. */
    pStream->EndBlock (pBlock, sizeof (RDPPalette));

    /* Also output a COLCACHE secondary order to have a palette id = 0 for MEMBLT orders. */
    pBlock = pStream->BeginBlock (VRDPUpdate_Order,
                                  sizeof (RDPColCache));

    if (pBlock == NULL)
    {
        return;
    }

    RDPColCache *p = (RDPColCache *)pBlock->pu8DstStart;

    p->u8Flags           = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
    p->u16Length         = sizeof (RDPColCache) - 13;
    p->u16SecondaryFlags = 0;

    p->u8Type            = RDP_ORDER_COLCACHE;

    p->u8CacheId         = 0;
    p->u16Entries        = 256;

    int i;
    for (i = 0; i < 256; i++)
    {
        p->au8RGB[i * 4 + 0] = colorPalette256[i * 3 + 2];
        p->au8RGB[i * 4 + 1] = colorPalette256[i * 3 + 1];
        p->au8RGB[i * 4 + 2] = colorPalette256[i * 3 + 0];
        p->au8RGB[i * 4 + 3] = 0;
    }

    /* Complete the block. Pass how many bytes were actually written. */
    pStream->EndBlock (pBlock, sizeof (RDPColCache));
}

#pragma pack(1)
typedef struct _RDPHdr
{
    uint16_t u16Length;   /* Total PDU length, including this header. */
    uint16_t u16PDUType;  /* Type and version of the PDU */
} RDPHdr;

typedef struct _RDPDataHdr
{
    uint16_t u16UserId;           /* MCS user id + 1001. @todo this must be in the RDPHdr. */
    uint32_t u32ShareId;          /* Share id. */
    uint8_t  u8Pad;               /* Pad. */
    uint8_t  u8StreamId;          /* Stream id. */
    uint16_t u16Length;           /* rdesktop set this to remaining length after the field.
                                   * But some other client set this to some arbitrary value.
                                   */
    uint8_t  u8Type;              /* The type of data PDU. */
    uint8_t  u8Compression;
    uint16_t u16CompressionSize;
} RDPDataHdr;

typedef struct _RDPPDURefreshRect
{
    uint32_t u32NumRects;
    uint16_t u16Left;
    uint16_t u16Top;
    uint16_t u16Right;
    uint16_t u16Bottom;
} RDPPDURefreshRect;

typedef struct RDPPDUSuppressOutput
{
    uint8_t  u8AllowDisplayUpdates;
    uint8_t  au8Pad[3];
    /* RDPPDUSuppressOutputRect may follow. */
} RDPPDUSuppressOutput;

typedef struct RDPPDUSuppressOutputRect
{
    uint16_t u16Left;
    uint16_t u16Top;
    uint16_t u16Right;
    uint16_t u16Bottom;
} RDPPDUSuppressOutputRect;

typedef struct _RDPPDUControl
{
    uint16_t u16Action;
    uint16_t u16UserId;
    uint32_t u32ControlId;
} RDPPDUControl;

typedef struct _RDPPDUInput
{
    uint16_t u16Count;
    uint16_t u16Pad;
} RDPPDUInput;

typedef struct _RDPPDUInputEvent
{
    uint32_t u32Time;
    uint16_t u16Type;
    uint16_t u16DeviceFlags;
    uint16_t u16Param1;
    uint16_t u16Param2;
} RDPPDUInputEvent;

typedef struct _RDPPDUSynchronise
{
    uint16_t u16Type;      /* The message type. This field MUST be set to SYNCMSGTYPE_SYNC (1). */
    uint16_t u16UserId;    /* The MCS channel ID of the target user. */
} RDPPDUSynchronise;

typedef struct _RDPPDUFont2
{
    uint16_t cFonts;       /* hardcoded in rdesktop (0) */
    uint16_t u16Pad;       /* pad, hardcoded in rdesktop (0) */
    uint16_t u16Seq;       /* rdesktop uses 3 for rdp5 and 1,2 for rdp4 */
    uint16_t cbEntry;      /* hardcoded in rdesktop (0x32) */
} RDPPDUFont2;

typedef struct _RDPCAHdr
{
    uint16_t u16MCSUserId; /* hardcoded in rdesktop (0x3ea) */
    uint32_t u32ShareId;   /* Share id. */
    uint16_t u16UserId;    /* hardcoded in rdesktop (0x3ea) */
    uint16_t u16SrcSize;   /* sizeof (RDP_SOURCE) that follows the hdr. */
    uint16_t u16CapLen;    /* caplen, we do not use it */
} RDPCAHdr;

typedef struct _RDPPDUMonitorLayout
{
    uint32_t u32MonitorCount;
    TS_MONITOR_DEF aMonitorDefArray[1];
} RDPPDUMonitorLayout;

#pragma pack()

void VRDPTP::OutputKeepAlive (VRDPStream *pStream)
{
    const VRDPStream::MemoryBlock *pBlock;

    pBlock = pStream->BeginBlock (VRDPUpdate_KeepAlive, sizeof (RDPPDUSynchronise));

    if (pBlock == NULL)
    {
        return;
    }

    RDPPDUSynchronise *pRDPPDUSynchronise = (RDPPDUSynchronise *)pBlock->pu8DstStart;

    pRDPPDUSynchronise->u16Type   = SYNCMSGTYPE_SYNC;
    pRDPPDUSynchronise->u16UserId = 0; /* Send the unknown user id which must be ignored by the client. */

    /* Complete the block. Pass how many bytes were actually written. */
    pStream->EndBlock (pBlock, sizeof (RDPPDUSynchronise));
}

int VRDPTP::SendPDU (VRDPInputCtx *pInputCtx, uint16_t u16Type)
{
    RDPHdr *pHdr = (RDPHdr *)pInputCtx->AppendProtocolHeader (sizeof (RDPHdr));
    Assert (pHdr);

    pHdr->u16Length = pInputCtx->SizeOfWrite ();    /* Total PDU length, including this header. */
    pHdr->u16PDUType = VRDP_MAKE_PDU_TYPE(u16Type); /* Type and version of the PDU */

    return m_sectp.Send (3, pInputCtx, 0);
}

int VRDPTP::SendDeactivate (VRDPInputCtx *pInputCtx)
{
    return SendPDU (pInputCtx, RDP_PDU_DEACTIVATE);
}

int VRDPTP::SendData (VRDPInputCtx *pInputCtx, uint8_t u8Type)
{
    RDPDataHdr *pDataHdr = (RDPDataHdr *)pInputCtx->AppendProtocolHeader (sizeof (RDPDataHdr));
    Assert (pDataHdr);

    uint16_t u16Length = pInputCtx->SizeOfWrite ();

    /* Most of the header fields are equal to 0. */
    memset (pDataHdr, 0, sizeof (RDPDataHdr));

    pDataHdr->u8StreamId = 0x01;

    /* In rdesktop this is length of packet after the data pdu header.
     * But MS server sets this to the length of entire DATA PDU.
     * That is equal to the length of RDP packet 'length + 12 + 4'.
     * Use the MS variant here.
     */
    pDataHdr->u16Length = u16Length + sizeof (RDPHdr);
    pDataHdr->u8Type    = u8Type;

    VRDPTPLOG(("Sending DATA PDU len = %d\n", u16Length));
    VRDP_DUMP_CTX_WRITE(pInputCtx);

    return SendPDU (pInputCtx, RDP_PDU_DATA);
}

int VRDPTP::SendControl (VRDPInputCtx *pInputCtx, uint16_t u16Action)
{
    RDPPDUControl *pControl = (RDPPDUControl *)pInputCtx->AppendProtocolHeader (sizeof (RDPPDUControl));
    Assert (pControl);

    pControl->u16Action    = u16Action;
    pControl->u16UserId    = 0;
    pControl->u32ControlId = 0;

    return SendData (pInputCtx, RDP_DATA_PDU_CONTROL);
}

int VRDPTP::SendMonitorLayout (VRDPInputCtx *pInputCtx)
{
    /* Get actual quantity of monitors and their dimensions from pInputCtx->Data(). */
    VRDPData *pData = pInputCtx->Data();

    uint32_t cMonitors = pData->MonitorCount();

    if (cMonitors == 0 || cMonitors > 16) /* RDP restriction. */
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    uint32_t cbMonitorLayout = RT_OFFSETOF(RDPPDUMonitorLayout, aMonitorDefArray) + cMonitors * sizeof(TS_MONITOR_DEF);

    RDPPDUMonitorLayout *pMonitorLayout = (RDPPDUMonitorLayout *)pInputCtx->AppendProtocolHeader (cbMonitorLayout);
    Assert (pMonitorLayout);

    pMonitorLayout->u32MonitorCount = cMonitors;

    uint32_t i;
    for (i = 0; i < cMonitors; i++)
    {
        TS_MONITOR_DEF *pMonitor = &pMonitorLayout->aMonitorDefArray[i];
        const TS_MONITOR_DEF *pSrc = pData->MonitorDef(i);

        *pMonitor = *pSrc;
    }

    return SendData (pInputCtx, RDP_DATA_PDU_MONITOR_LAYOUT_PDU);
}

int VRDPTP::ProcessDataPDUSynchronise (VRDPInputCtx *pInputCtx)
{
    const RDPPDUSynchronise *pSynchronise = (const RDPPDUSynchronise *)pInputCtx->Read (sizeof (RDPPDUSynchronise));

    if (pSynchronise == NULL)
    {
        VRDPTPLOG(("Failed to RDPPDUSynchronise!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* Send the information in pSynchronise back to client. */
    RDPPDUSynchronise *pSynchroniseWrite = (RDPPDUSynchronise *)pInputCtx->AppendProtocolHeader (sizeof (RDPPDUSynchronise));
    Assert (pSynchroniseWrite);

    *pSynchroniseWrite = *pSynchronise;

    return SendData (pInputCtx, RDP_DATA_PDU_SYNCHRONISE);
}

int VRDPTP::ProcessDataPDURefreshRect (VRDPInputCtx *pInputCtx)
{
    const RDPPDURefreshRect *pRefreshRect = (const RDPPDURefreshRect *)pInputCtx->Read (sizeof (RDPPDURefreshRect));

    if (pRefreshRect == NULL)
    {
        VRDPTPLOG(("Failed to RDPPDURefreshRect!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    if (pRefreshRect->u32NumRects != 1)
    {
        VRDPTPLOG(("RDP_DATA_PDU_REFRESH_RECT num of rects %d. Unsupported!!!\n",
                   pRefreshRect->u32NumRects));
        return VWRN_VRDP_PDU_NOT_SUPPORTED;
    }

    VRDPTPLOG(("RDP_DATA_PDU_REFRESH_RECT: %d,%d %d,%d\n",
               pRefreshRect->u16Left, pRefreshRect->u16Top,
               pRefreshRect->u16Right, pRefreshRect->u16Bottom
             ));

    RGNRECT rectUpdate;

    rectUpdate.x = pRefreshRect->u16Left;
    rectUpdate.y = pRefreshRect->u16Top;
    rectUpdate.w = pRefreshRect->u16Right - pRefreshRect->u16Left + 1;
    rectUpdate.h = pRefreshRect->u16Bottom - pRefreshRect->u16Top + 1;

    Client ()->AddRedraw (&rectUpdate, false, 0);

    return VINF_SUCCESS;
}

int VRDPTP::ProcessDataPDUSuppressOutput (VRDPInputCtx *pInputCtx)
{
    const RDPPDUSuppressOutput *pSuppressOutput = (RDPPDUSuppressOutput *)pInputCtx->Read (sizeof (RDPPDUSuppressOutput));

    if (pSuppressOutput == NULL)
    {
        VRDPTPLOG(("Failed to read RDPPDUSuppressOutput!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    if (pSuppressOutput->u8AllowDisplayUpdates)
    {
        const RDPPDUSuppressOutputRect *pRect = (RDPPDUSuppressOutputRect *)pInputCtx->Read (sizeof (RDPPDUSuppressOutputRect));

        if (pRect == NULL)
        {
            VRDPTPLOG(("Failed to read RDPPDUSuppressOutputRect!!!\n"));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        VRDPTPLOG(("RDP_DATA_PDU_SUPPRESS_OUTPUT: allow %d,%d %d,%d\n",
                   pRect->u16Left, pRect->u16Top,
                   pRect->u16Right, pRect->u16Bottom
                 ));

        RGNRECT rectUpdate;

        rectUpdate.x = pRect->u16Left;
        rectUpdate.y = pRect->u16Top;
        rectUpdate.w = pRect->u16Right - pRect->u16Left + 1;
        rectUpdate.h = pRect->u16Bottom - pRect->u16Top + 1;

        Client ()->DisableDisplay (false);
        Client ()->AddRedraw (&rectUpdate, false, 0);
        Client()->Server()->PostOutput(VRDP_OUTPUT_VIDEO_STREAM_ENABLE, Client()->Id(), NULL, 0);
    }
    else
    {
        VRDPTPLOG(("RDP_DATA_PDU_SUPPRESS_OUTPUT: suppress\n"));

        Client ()->DisableDisplay (true);
        Client()->Server()->PostOutput(VRDP_OUTPUT_VIDEO_STREAM_DISABLE, Client()->Id(), NULL, 0);
    }

    return VINF_SUCCESS;
}

int VRDPTP::ProcessDataPDUControl (VRDPInputCtx *pInputCtx)
{
    const RDPPDUControl *pControl = (const RDPPDUControl *)pInputCtx->Read (sizeof (RDPPDUControl));

    if (pControl == NULL)
    {
        VRDPTPLOG(("Failed to RDPPDUControl!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    switch (pControl->u16Action)
    {
        default:
        {
            /* Unknown action, ignore the packet. */
            VRDPTPLOG(("Unsupported Control PDU Action %d!!!\n", pControl->u16Action));
            VRDP_DUMP_CTX_READ(pInputCtx);
            return VWRN_VRDP_PDU_NOT_SUPPORTED;
        }

        /* rdesktop actually does not parse these control PDU's,
         * and just expects a reply.
         */
        case RDP_CTL_REQUEST_CONTROL:
        {
            return SendControl (pInputCtx, RDP_CTL_GRANT_CONTROL);
        }

        case RDP_CTL_COOPERATE:
        {
            return SendControl (pInputCtx, RDP_CTL_COOPERATE);
        }
    }
    /* not reached */
}

int VRDPTP::ProcessDataPDUInput (VRDPInputCtx *pInputCtx)
{
    const RDPPDUInput *pInput = (const RDPPDUInput *)pInputCtx->Read (sizeof (RDPPDUInput));

    if (pInput == NULL)
    {
        VRDPTPLOG(("Failed to RDPPDUInput!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    VRDPLOGRELIO(("DATA_PDU_INPUT recv %d event(s):\n", pInput->u16Count));

    unsigned i;
    for (i = 0; i < pInput->u16Count; i++)
    {
        const RDPPDUInputEvent *pInputEvent = (const RDPPDUInputEvent *)pInputCtx->Read (sizeof (RDPPDUInputEvent));

        if (pInputEvent == NULL)
        {
            VRDPTPLOG(("Failed to RDPPDUInputEvent!!!\n"));
            VRDPLOGRELIO(("DATA_PDU_INPUT [%d] no data!!!\n", i));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        VRDPLOGRELIO(("DATA_PDU_INPUT [%d]: 0x%08X 0x%04X 0x%04X 0x%04X 0x%04X\n",
                      i, pInputEvent->u32Time, pInputEvent->u16Type, pInputEvent->u16DeviceFlags,
                      pInputEvent->u16Param1, pInputEvent->u16Param2));

        VRDPDesktop::InputMsg m;

        m.time         = pInputEvent->u32Time;
        m.message_type = pInputEvent->u16Type;
        m.device_flags = pInputEvent->u16DeviceFlags;
        m.param1       = pInputEvent->u16Param1;
        m.param2       = pInputEvent->u16Param2;

        unsigned uScreenId = QueryScreenId ();
        if (m.message_type == RDP_INPUT_MOUSE)
        {
            DesktopMap()->Client2ScreenPoint(&uScreenId, (int16_t *)&m.param1, (int16_t *)&m.param2);

            int x = m.param1;
            int y = m.param2;

            shadowBufferTransformPointToFB (uScreenId, &x, &y);

            m.param1 = x;
            m.param2 = y;
        }

        if (!Client()->IsInputDisabled())
        {
            m_pdesktop->ProcessInput (uScreenId, &m);
        }
    }

    return VINF_SUCCESS;
}

int VRDPTP::ProcessDataPDUFont2 (VRDPInputCtx *pInputCtx)
{
    const RDPPDUFont2 *pFont2 = (const RDPPDUFont2 *)pInputCtx->Read (sizeof (RDPPDUFont2));

    if (pFont2 == NULL)
    {
        VRDPTPLOG(("Failed to RDPPDUFont2!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    const uint8_t *pu8 = pInputCtx->Read (pFont2->cFonts * pFont2->cbEntry);

    if (pu8 == NULL)
    {
        VRDPTPLOG(("Failed to RDPPDUFont2 fonts!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* if pFont2->u16Seq is equal to 2 or 3 then we have to send a reply. */
    if (pFont2->u16Seq < 2)
    {
        /* otherwise just skip the packet. */
        VRDPTPLOG(("pFont2->u16Seq == %d, skipping!!!\n", pFont2->u16Seq));
        return VINF_SUCCESS;
    }

    VRDPTPLOG(("RDPPDUFont2 replaying.\n"));

    /* Irrelevant for VRDP, but have to send a reply,
     * rdesktop does not parse this packet at all
     * but expects it.
     */
    int rc = SendData (pInputCtx, 0x28);

    if (RT_FAILURE (rc))
    {
         VRDPTPLOG(("RDPPDUFont2 send rc = %Rrc.\n", rc));
         return rc;
    }

    /* The DEMAND_ACTIVE sequence has completed. The server can
     * start sending output updates.
     */
    return VINF_VRDP_OUTPUT_ENABLE;
}

int VRDPTP::ProcessConfirmActivePDU (VRDPInputCtx *pInputCtx)
{
    /*
     * Parse CONFIRM_ACTIVE PDU.
     */
    const RDPCAHdr *pCAHdr = (const RDPCAHdr *)pInputCtx->Read (sizeof (RDPCAHdr));

    if (pCAHdr == NULL)
    {
        VRDPTPLOG(("Failed to read CA hdr!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* Read the RDP source string. */
    const uint8_t *pu8 = pInputCtx->Read (pCAHdr->u16SrcSize);

    if (pu8 == NULL)
    {
        VRDPTPLOG(("Failed to read RDP source!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* Now read the NumberOfCaps header. */
    RDPAllCapsHdr *pAllHdr = (RDPAllCapsHdr *)pInputCtx->Read (sizeof (RDPAllCapsHdr));

    if (pAllHdr == NULL)
    {
        VRDPTPLOG(("Failed to read NumberOfCaps!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /*
     * The client sent us capabilities.
     */
    unsigned i = pAllHdr->u16CapSets;

    while (i--)
    {
        RDPCapsHdr *pHdr = (RDPCapsHdr *)pInputCtx->Read (sizeof (RDPCapsHdr));

        if (pHdr == NULL)
        {
            VRDPTPLOG(("Failed to read CapsHdr %d!!!\n", i));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        if (pHdr->u16CapSet == 0 && pHdr->u16CapLen == 0)
        {
            continue;
        }

        pu8 = pInputCtx->Read (pHdr->u16CapLen - sizeof (RDPCapsHdr));

        if (pu8 == NULL)
        {
            VRDPTPLOG(("Failed to read CapsData %d, Set %d!!!\n", i, pHdr->u16CapSet));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        switch (pHdr->u16CapSet)
        {
            case RDP_CAPSET_GENERAL:
            {
                RDPCapsGeneral *pCapsGeneral = (RDPCapsGeneral *)pu8;

                if (sizeof (RDPCapsGeneral) + sizeof (RDPCapsHdr) > pHdr->u16CapLen)
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPTPLOG(("VRDPTP::RDP_CAPSET_GENERAL:\n"
                           "    u16OSMajorType             0x%04X\n"
                           "    u16OSMinorType             0x%04X\n"
                           "    u16ProtocolVersion         0x%04X\n"
                           "    u16Pad1                    0x%04X\n"
                           "    u16GeneralCompressionTypes 0x%04X\n"
                           "    u16RDP5Flag                0x%04X\n"
                           "    u16UpdateCapabilityFlag    0x%04X\n"
                           "    u16RemoteUnshareFlag       0x%04X\n"
                           "    u16GeneralCompressionLevel 0x%04X\n"
                           "    u8RefreshRectSupport       0x%02X\n"
                           "    u8SuppressOutputSupport    0x%02X\n",
                            pCapsGeneral->u16OSMajorType,
                            pCapsGeneral->u16OSMinorType,
                            pCapsGeneral->u16ProtocolVersion,
                            pCapsGeneral->u16Pad1,
                            pCapsGeneral->u16GeneralCompressionTypes,
                            pCapsGeneral->u16RDP5Flag,
                            pCapsGeneral->u16UpdateCapabilityFlag,
                            pCapsGeneral->u16RemoteUnshareFlag,
                            pCapsGeneral->u16GeneralCompressionLevel,
                            pCapsGeneral->u8RefreshRectSupport,
                            pCapsGeneral->u8SuppressOutputSupport
                         ));

                m_fRDP5Packets = (pCapsGeneral->u16RDP5Flag & FASTPATH_OUTPUT_SUPPORTED) != 0;
            } break;

            case RDP_CAPSET_BITMAP:
            {
                RDPCapsBitmap *pCapsBitmap = (RDPCapsBitmap *)pu8;

                if (sizeof (RDPCapsBitmap) + sizeof (RDPCapsHdr) > pHdr->u16CapLen)
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPTPLOG(("VRDPTP::RDP_CAPSET_BITMAP:\n"
                            "    PreferredBitsPerPixel    0x%04X\n"
                            "    Receive1BitPerPixel      0x%04X\n"
                            "    Receive4BitPerPixel      0x%04X\n"
                            "    Receive8BitPerPixel      0x%04X\n"
                            "    DesktopWidth             0x%04X\n"
                            "    DesktopHeight            0x%04X\n"
                            "    Pad                      0x%04X\n"
                            "    DesktopResizeFlag        0x%04X\n"
                            "    BitmapCompressionFlag    0x%04X\n"
                            "    HighColorFlags           0x%02X\n"
                            "    DrawingFlags             0x%02X\n"
                            "    MultipleRectangleSupport 0x%04X\n"
                            "    Pad2                     0x%04X\n",
                            pCapsBitmap->u16PreferredBitsPerPixel,
                            pCapsBitmap->u16Receive1BitPerPixel,
                            pCapsBitmap->u16Receive4BitPerPixel,
                            pCapsBitmap->u16Receive8BitPerPixel,
                            pCapsBitmap->u16DesktopWidth,
                            pCapsBitmap->u16DesktopHeight,
                            pCapsBitmap->u16Pad,
                            pCapsBitmap->u16DesktopResizeFlag,
                            pCapsBitmap->u16BitmapCompressionFlag,
                            pCapsBitmap->u8HighColorFlags,
                            pCapsBitmap->u8DrawingFlags,
                            pCapsBitmap->u16MultipleRectangleSupport,
                            pCapsBitmap->u16Pad2));

                m_fBitmapCompression = (pCapsBitmap->u16BitmapCompressionFlag != 0);
            } break;

            case RDP_CAPSET_POINTER:
            {
                RDPCapsPointer *pCapsPointer = (RDPCapsPointer *)pu8;

                if (sizeof (RDPCapsHdr) > pHdr->u16CapLen)
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                if (pHdr->u16CapLen >= 10) /* The client may support New pointers. */
                {
                    VRDPTPLOG(("VRDPTP::RDP_CAPSET_POINTER: len %d, color = %d, size = %d, size2 %d\n",
                               pHdr->u16CapLen, pCapsPointer->u16ColorFlag, pCapsPointer->u16CacheSize, pCapsPointer->u16CacheSize2));

                    uint16_t u16CacheSize;

                    if (pCapsPointer->u16CacheSize2 != 0)
                    {
                        u16CacheSize = pCapsPointer->u16CacheSize2;
                        Data()->NewMousePointers(true);
                    }
                    else
                    {
                        u16CacheSize = pCapsPointer->u16CacheSize;
                    }

                    /* Make available cache size 1 less than reported,
                     * because slot(index) 0 is reserved for pointer which are not
                     * cached.
                     */
                    m_pointerCacheSize = u16CacheSize > 0? u16CacheSize - 1: 0;
                }
                else if (pHdr->u16CapLen >= 8) /* The client does not use the u16CacheSize2 field. */
                {
                    VRDPTPLOG(("VRDPTP::RDP_CAPSET_POINTER: len %d, color = %d, size = %d\n",
                               pHdr->u16CapLen, pCapsPointer->u16ColorFlag, pCapsPointer->u16CacheSize));

                    /* Make available cache size 1 less than reported,
                     * because slot(index) 0 is reserved for pointer which are not
                     * cached.
                     */
                    m_pointerCacheSize = pCapsPointer->u16CacheSize > 0? pCapsPointer->u16CacheSize - 1: 0;
                }
                else
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                }
            } break;

            case RDP_CAPSET_LARGE_POINTER:
            {
                RDPCapsLargePointer *pCapsLargePointer = (RDPCapsLargePointer *)pu8;

                if (sizeof (RDPCapsLargePointer) + sizeof (RDPCapsHdr) > pHdr->u16CapLen)
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPTPLOG(("VRDPTP::RDP_CAPSET_LARGE_POINTER: len %d, flag = %d\n",
                           pHdr->u16CapLen, pCapsLargePointer->u16LargePointerSupportFlags));

                if (pCapsLargePointer->u16LargePointerSupportFlags & LARGE_POINTER_FLAG_96x96)
                {
                    Data()->LargeMousePointers(true);
                }
            } break;

            case RDP_CAPSET_ORDER:
            {
                RDPCapsOrder *pCapsOrder = (RDPCapsOrder *)pu8;

                if (sizeof (RDPCapsOrder) + sizeof (RDPCapsHdr) > pHdr->u16CapLen)
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                m_caps.order.u16CacheGranularityX   = pCapsOrder->u16DesktopSaveXGranularity;
                m_caps.order.u16CacheGranularityY   = pCapsOrder->u16DesktopSaveYGranularity;
                m_caps.order.u16MaxOrderLevel       = pCapsOrder->u16MaximumOrderLevel;
                m_caps.order.u16NumberOfFonts       = pCapsOrder->u16NumberFonts;
                m_caps.order.u16CapabilityFlags     = pCapsOrder->u16OrderFlags;

                memcpy (m_caps.order.au8SupportedOrders, pCapsOrder->au8OrderSupport, 32);

                m_caps.order.u16TextCapabilityFlags = pCapsOrder->u16TextFlags;
                m_caps.order.u32DesktopCacheSize    = pCapsOrder->u32DesktopSaveSize;

                Data()->OrderFlags(pCapsOrder->u16OrderFlags);
                if (pCapsOrder->u16OrderFlags & TS_ORDERFLAGS_EXTRA_FLAGS)
                {
                    Data()->OrderSupportExFlags(pCapsOrder->u16OrderSupportExFlags);
                }
                else
                {
                    Data()->OrderSupportExFlags(0);
                }

                VRDPTPLOG(("VRDPTP::RDP_CAPSET_ORDER:\n"
                           "    au8TerminalDescriptor %.*Rhxd\n"
                           "    u32Pad1                    0x%08X\n"
                           "    u16DesktopSaveXGranularity 0x%04X\n"
                           "    u16DesktopSaveYGranularity 0x%04X\n"
                           "    u16Pad2                    0x%04X\n"
                           "    u16MaximumOrderLevel       0x%04X\n"
                           "    u16NumberFonts             0x%04X\n"
                           "    u16OrderFlags              0x%04X\n"
                           "    au8OrderSupport %.*Rhxd\n"
                           "    u16TextFlags               0x%04X\n"
                           "    u16OrderSupportExFlags     0x%04X\n"
                           "    u32Pad3                    0x%08X\n"
                           "    u32DesktopSaveSize         0x%08X\n"
                           "    u16Pad4                    0x%04X\n"
                           "    u16Pad5                    0x%04X\n"
                           "    u16TextANSICodePage        0x%04X\n"
                           "    u16Pad6                    0x%04X\n",
                           sizeof(pCapsOrder->au8TerminalDescriptor), pCapsOrder->au8TerminalDescriptor,
                           pCapsOrder->u32Pad1,
                           pCapsOrder->u16DesktopSaveXGranularity,
                           pCapsOrder->u16DesktopSaveYGranularity,
                           pCapsOrder->u16Pad2,
                           pCapsOrder->u16MaximumOrderLevel,
                           pCapsOrder->u16NumberFonts,
                           pCapsOrder->u16OrderFlags,
                           sizeof(pCapsOrder->au8OrderSupport), pCapsOrder->au8OrderSupport,
                           pCapsOrder->u16TextFlags,
                           pCapsOrder->u16OrderSupportExFlags,
                           pCapsOrder->u32Pad3,
                           pCapsOrder->u32DesktopSaveSize,
                           pCapsOrder->u16Pad4,
                           pCapsOrder->u16Pad5,
                           pCapsOrder->u16TextANSICodePage,
                           pCapsOrder->u16Pad6));
            } break;

            case RDP_CAPSET_BRUSHCACHE:
            {
                RDPCapsBrushCache *pCapsBrushCache = (RDPCapsBrushCache *)pu8;

                if (sizeof (RDPCapsBrushCache) + sizeof (RDPCapsHdr) > pHdr->u16CapLen)
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPTPLOG(("VRDPTP::RDP_CAPSET_BRUSHCACHE: 0x%08X\n",
                            pCapsBrushCache->u32BrushSupportLevel));

                m_u32BrushSupportLevel = pCapsBrushCache->u32BrushSupportLevel;
            } break;

            case RDP_CAPSET_BMPCACHE:
            {
                RDPCapsBmpCache *pCapsBmpCache = (RDPCapsBmpCache *)pu8;
                RT_NOREF_PV(pCapsBmpCache);

                if (sizeof (RDPCapsBmpCache) + sizeof (RDPCapsHdr) > pHdr->u16CapLen)
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPTPLOG(("VRDPTP::RDP_CAPSET_BMPCACHE: %d/%d %d/%d %d/%d\n",
                            pCapsBmpCache->Cache1Entries,
                            pCapsBmpCache->Cache1MaximumCellSize,
                            pCapsBmpCache->Cache2Entries,
                            pCapsBmpCache->Cache2MaximumCellSize,
                            pCapsBmpCache->Cache3Entries,
                            pCapsBmpCache->Cache3MaximumCellSize
                         ));
            } break;

            case RDP_CAPSET_BMPCACHE2:
            {
                RDPCapsBmpCache2 *pCapsBmpCache2 = (RDPCapsBmpCache2 *)pu8;

                if (sizeof (RDPCapsBmpCache2) + sizeof (RDPCapsHdr) > pHdr->u16CapLen)
                {
                    VRDPTPLOG(("CapsLen (number %d) set %d, len %d is too small!!!\n",
                                i, pHdr->u16CapSet, pHdr->u16CapLen));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                VRDPTPLOG(("VRDPTP::RDP_CAPSET_BMPCACHE2: flags 0x%04X %d:\n",
                            pCapsBmpCache2->CacheFlags,
                            pCapsBmpCache2->NumCellCaches
                         ));

                unsigned iCell = 0;
                for (; iCell < pCapsBmpCache2->NumCellCaches && iCell < RT_ELEMENTS(pCapsBmpCache2->CellCacheInfo); iCell++)
                {
                    VRDPTPLOG(("VRDPTP::RDP_CAPSET_BMPCACHE2: [%d] entries %d, 64bit key %d\n",
                                iCell,
                                pCapsBmpCache2->CellCacheInfo[iCell] & 0x7FFFFFFFU,
                                (pCapsBmpCache2->CellCacheInfo[iCell] & 0x80000000U) != 0
                             ));
                }
            } break;

            default:
            {
                VRDPTPLOG(("VRDPTP::CAPSET: 0x%04X\n",
                            pHdr->u16CapSet));
                hexdump((unsigned char *)pu8, pHdr->u16CapLen - sizeof (RDPCapsHdr));
            } break;
        }
    }

    return VINF_SUCCESS;
}

int VRDPTP::RecvRDP5 (VRDPInputCtx *pInputCtx)
{
    RT_NOREF1(pInputCtx);
    VRDPTPLOG(("Unsupported RDP5 style PDU!!!\n"));
    return VWRN_VRDP_PDU_NOT_SUPPORTED;
}

int VRDPTP::Recv (VRDPInputCtx *pInputCtx)
{
    int rc = m_sectp.Recv (pInputCtx);

    if (rc != VINF_VRDP_PROCESS_PDU)
    {
        return rc;
    }

    switch (m_enmStatus)
    {
        case VRDP_RDP_Status_RecvConnectInitial:
        {
            VRDPTPLOG(("VRDP_RDP_Status_RecvConnectInitial\n"));

            rc = RecvConnectInitial (pInputCtx);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            /* Reply with the connect response. */
            rc = SendConnectResponse (pInputCtx, 0);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            m_enmStatus = VRDP_RDP_Status_RecvLogonInfo;

            /* No need to further process the RDP packet. */
            return VINF_SUCCESS;
        }

        case VRDP_RDP_Status_RecvLogonInfo:
        {
            VRDPTPLOG(("VRDP_RDP_Status_RecvLogonInfo\n"));

            rc = RecvLogonInfo (pInputCtx);

            if (rc != VINF_SUCCESS) /* Return VINF* as well. */
            {
                return rc;
            }

            rc = SendLicense (pInputCtx);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            m_pClient->NotifyConnected ();

            m_enmStatus = VRDP_RDP_Status_RDP;

            /* The DEMAND_ACTIVE (re)starts the RDP protocol flow. */
            rc = SendDemandActive (pInputCtx);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            /* No need to further process the RDP packet. */
            return VINF_SUCCESS;
        }

        case VRDP_RDP_Status_RDP:
        {
            if (pInputCtx->PacketVersion () != 3)
            {
                return RecvRDP5 (pInputCtx);
            }

            if (pInputCtx->QueryIncomingChannelId () != MCS_GLOBAL_CHANNEL)
            {
                const RDPChannelHdr *pChannelHdr = (const RDPChannelHdr *)pInputCtx->Read (sizeof (RDPChannelHdr));

                if (pChannelHdr == NULL)
                {
                    VRDPTPLOG(("Failed to read channel hdr!!!\n"));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                ProcessChannel (pInputCtx, pChannelHdr->u32Length, pChannelHdr->u32Flags);

                return VINF_SUCCESS;
            }

            /*
             * Parse VRDP PDU.
             */
            const RDPHdr *pHdr = (const RDPHdr *)pInputCtx->Read (sizeof (RDPHdr));

            if (pHdr == NULL)
            {
                VRDPTPLOG(("Failed to read vrdp hdr!!!\n"));
                return VERR_VRDP_PROTOCOL_ERROR;
            }

            if (pInputCtx->BytesToRead () + sizeof (RDPHdr) < pHdr->u16Length)
            {
                VRDPTPLOG(("Insufficient data: %d %d!!!\n", pInputCtx->BytesToRead () + sizeof (RDPHdr), pHdr->u16Length));
                AssertFailed ();
                return VERR_VRDP_PROTOCOL_ERROR;
            }

            uint16_t u16PDUType = pHdr->u16PDUType & 0xF;

            /* Branch to supported PDUs processing */
            switch (u16PDUType)
            {
                case RDP_PDU_DATA:
                {
                    VRDPTPLOG(("RDP_PDU_DATA\n"));

                    /*
                     * Parse data PDU.
                     */
                    const RDPDataHdr *pDataHdr = (const RDPDataHdr *)pInputCtx->Read (sizeof (RDPDataHdr));

                    if (pDataHdr == NULL)
                    {
                        VRDPTPLOG(("Failed to read data hdr!!!\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    VRDPTPLOG(("Type: %d\n", pDataHdr->u8Type));

                    if (   pDataHdr->u8Compression != 0
                        || pDataHdr->u16CompressionSize != 0)
                    {
                        VRDPTPLOG(("Compression unsupported: type %d, size %d!!!\n",
                                    pDataHdr->u8Compression, pDataHdr->u16CompressionSize));
                        return VERR_VRDP_NOT_SUPPORTED;
                    }

                    /* Preparse data pdu a bit */
                    switch (pDataHdr->u8Type)
                    {
                        case RDP_DATA_PDU_REFRESH_RECT:
                        {
                            return ProcessDataPDURefreshRect (pInputCtx);
                        } break;

                        case RDP_DATA_PDU_SUPPRESS_OUTPUT:
                        {
                            return ProcessDataPDUSuppressOutput (pInputCtx);
                        } break;

                        case RDP_DATA_PDU_POINTER:
                        case RDP_DATA_PDU_UPDATE:
                        case RDP_DATA_PDU_BELL:
                        case RDP_DATA_PDU_LOGON:
                        case RDP_DATA_PDU_DISCONNECT:
                        {
                            /* Only server may send these pdu, so ignore them. */
                            VRDPTPLOG(("Data PDU %d is not expected from client!!!\n", pDataHdr->u8Type));
                            return VWRN_VRDP_PDU_NOT_SUPPORTED;
                        }

                        case RDP_DATA_PDU_CLOSE:
                        {
                            /* Client sends the PDU with not further data when close button */

                            VRDPTPLOG(("RDP_DATA_PDU_CLOSE: Client requested close.\n"));
                            VRDP_DUMP_CTX_READ(pInputCtx);

                            pInputCtx->Reset();
                            Disconnect(pInputCtx, exDiscReasonNoInfo);

                            return VINF_VRDP_OPERATION_COMPLETED;
                        }

                        case RDP_DATA_PDU_CONTROL:
                        {
                            return ProcessDataPDUControl (pInputCtx);
                        }

                        case RDP_DATA_PDU_INPUT:
                        {
                            return ProcessDataPDUInput (pInputCtx);
                        }

                        case RDP_DATA_PDU_SYNCHRONISE:
                        {
                            return ProcessDataPDUSynchronise (pInputCtx);
                        }

                        case RDP_DATA_PDU_FONT2:
                        {
                            return ProcessDataPDUFont2 (pInputCtx);
                        }

                        default:
                        {
                            VRDPTPLOG(("Unsupported Data PDU %d!!!\n", pDataHdr->u8Type));
                            VRDP_DUMP_CTX_READ(pInputCtx);
                            return VWRN_VRDP_PDU_NOT_SUPPORTED;
                        }
                    }

                    /* not reached. */
                    break;
                }

                case RDP_PDU_CONFIRM_ACTIVE:
                {
                    VRDPTPLOG(("RDP_PDU_CONFIRM_ACTIVE\n"));

                    pInputCtx->DumpRead ();

                    return ProcessConfirmActivePDU (pInputCtx);
                }

                default:
                {
                    VRDPTPLOG(("Unsupported RDP PDU type %d!!!\n", u16PDUType));
                    VRDP_DUMP_CTX_READ(pInputCtx);
                    return VWRN_VRDP_PDU_NOT_SUPPORTED;
                }
            }
        }
    }

    /* It goes here only because of a bug. */
    AssertFailed ();
    return VERR_VRDP_PROTOCOL_ERROR;
}

int VRDPTP::Send (bool fRDP5, VRDPCtx *pCtx)
{
    if (m_enmStatus == VRDP_RDP_Status_RDP)
    {
        uint8_t u8Version = fRDP5? 0: 3;
        return m_sectp.Send (u8Version, pCtx, 0);
    }

    /* The protocol was reset. */
    VRDPTPLOG(("Protocol state is invalid %d!!!\n", m_enmStatus));
    return VERR_VRDP_PROTOCOL_ERROR;
}

void VRDPTP::Reset (void)
{
    DestroyMembers ();
    InitMembers ();

    m_sectp.Reset ();

    m_enmStatus = VRDP_RDP_Status_RecvConnectInitial;
}
