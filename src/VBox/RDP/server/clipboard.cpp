/* $Id: clipboard.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol - VRDPServer and VRDPTP methods related to clipboard exchange.
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

/*
 * The RDP clipboard negotiation starts with server sending the CLIPRDR_CONNECT.
 * That causes the client to issue CLIPRDR_FORMAT_ANNOUNCE that lists formats
 * which are currently in clipboard and could be provided by the client.
 * The server replies with a CLIPRDR_FORMAT_ACK meaning that formats are OK.
 * The server then also issues CLIPRDR_DATA_REQUEST with a format index (Windows CF_*)
 * to receive the data. The client sends the data in the CLIPRDR_DATA_RESPONSE packet.
 * So works the client to server direction.
 *
 * Sending data from server to the client is similar. The server sends CLIPRDR_FORMAT_ANNOUNCE,
 * then expects CLIPRDR_FORMAT_ACK. The client must then issue CLIPRDR_DATA_REQUEST and
 * receives the data from the server in the CLIPRDR_DATA_RESPONSE packet.
 *
 */

#define CLIPRDR_CONNECT         1
#define CLIPRDR_FORMAT_ANNOUNCE 2
#define CLIPRDR_FORMAT_ACK      3
#define CLIPRDR_DATA_REQUEST    4
#define CLIPRDR_DATA_RESPONSE   5

#define CLIPRDR_REQUEST         0
#define CLIPRDR_RESPONSE        1
#define CLIPRDR_ERROR           2

/* These 2 clipboard formats are the same as in Windows API. */
#define RDP_CF_DIB              8
#define RDP_CF_UNICODETEXT      13

/* These formats have internal VRDP values, but they are identified by the name.
 * The value is arbitrary in range 0xc0f0..0xc0ff, because Windows assigns
 * values in the range 0xC000 through 0xFFFF.
 */
#define RDP_CF_HTML_VRDP        0xc0f0
#define RDP_CF_HTML_NAME        L"HTML Format"

/* Error value. */
#define RDP_CF_INVALID          0

static int appClipboard (const VRDECALLBACKS_4 *pCallbacks,
                         void *pvCallback,
                         void *pvIntercept,
                         uint32_t u32ClientId,
                         uint32_t u32Function,
                         uint32_t u32Format,
                         const void *pvData,
                         uint32_t cbData)
{
    int rc = VERR_NOT_SUPPORTED;

    if (pCallbacks && pCallbacks->VRDECallbackClipboard)
    {
        rc = pCallbacks->VRDECallbackClipboard (pvCallback, pvIntercept, u32ClientId, u32Function, u32Format, pvData, cbData);
    }

    return rc;
}

/** @thread VM */
void VRDPServer::ProcessClipboard (uint32_t u32Function, uint32_t u32Format, void *pvData, uint32_t cbData, uint32_t *pcbActualRead)
{
    if (u32Function == VRDE_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE)
    {
        CLIPBOARDLOG(("VRDE_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE\n"));

        /* The guest announces clipboard formats. This must be delivered to all clients. */
        VRDPClipboardData clipboardData;

        Assert(pvData == NULL && cbData == 0);

        clipboardData.u32Function = u32Function;
        clipboardData.u32Format   = u32Format;
        clipboardData.pvData      = NULL;
        clipboardData.cbData      = 0;

        /* Post the data block. */
        PostOutput (VRDP_OUTPUT_CLIPBOARD_DATA, 0, &clipboardData, sizeof (VRDPClipboardData));
    }
    else if (u32Function == VRDE_CLIPBOARD_FUNCTION_DATA_READ)
    {
        /* The guest requests the clipboard data. This must be delivered to the client
         * that has announced the requested format most recently. And then the data from
         * the client must be received.
         */

        CLIPBOARDLOG(("VRDE_CLIPBOARD_FUNCTION_DATA_READ\n"));

        /* Search the client that has announced the format. */
        uint32_t u32LatestClientId = 0;
        uint64_t u64LatestAnnounceTime = 0;

        VRDPClient *pClient;
        uint32_t u32ClientId = 0;

        while ((pClient = m_clientArray.ThreadContextGetNextClient (&u32ClientId, VRDP_CONTEXT_VM)) != NULL)
        {
            uint32_t u32AnnouncedFormats;
            uint64_t u64AnnounceTime;

            pClient->TP ()->Clipboard ()->QueryClipboardFormats (&u32AnnouncedFormats, &u64AnnounceTime);

            /* Leave the lock as fast as possible. */
            pClient->ThreadContextRelease (VRDP_CONTEXT_VM);

            if (u32AnnouncedFormats & u32Format)
            {
                if (u64LatestAnnounceTime < u64AnnounceTime)
                {
                    u64LatestAnnounceTime = u64AnnounceTime;
                    u32LatestClientId = u32ClientId;
                }
            }
        }

        if (u32LatestClientId == 0)
        {
            /* No client has announced the format. */
            VRDPLOGREL(("Guest requested clipboard format 0x%08X that is not announced by VRDP clients. Ignoring the request!!!\n",
                         u32Format));
            *pcbActualRead = 0;
        }
        else
        {
            /* Check if the client already has data. */
            pClient = m_clientArray.ThreadContextGetClient (u32LatestClientId, VRDP_CONTEXT_VM);

            if (!pClient)
            {
                *pcbActualRead = 0;
            }
            else
            {
                pClient->ThreadContextRelease (VRDP_CONTEXT_VM);

                /* Send the request to the client. */
                VRDPClipboardData clipboardData;

                clipboardData.u32Function = u32Function;
                clipboardData.u32Format   = u32Format;
                clipboardData.pvData      = NULL;
                clipboardData.cbData      = 0;

                pClient->TP ()->Clipboard ()->DiscardClipboardData ();

                PostOutput (VRDP_OUTPUT_CLIPBOARD_DATA, u32LatestClientId, &clipboardData, sizeof (VRDPClipboardData));

                /* Wait for reply. */
                for (;;)
                {
                    VRDPThreadSleep (10);

                    pClient = m_clientArray.ThreadContextGetClient (u32LatestClientId, VRDP_CONTEXT_VM);

                    if (!pClient)
                    {
                        *pcbActualRead = 0;
                        break;
                    }

                    bool fGotData = pClient->TP ()->Clipboard ()->QueryClipboardData (u32Format, pvData, cbData, pcbActualRead);

                    pClient->ThreadContextRelease (VRDP_CONTEXT_VM);

                    if (fGotData)
                    {
                        break;
                    }
                }
            }
        }
    }
    else if (u32Function == VRDE_CLIPBOARD_FUNCTION_DATA_WRITE)
    {
        /* The guest has responded to a data request. Send the reply to the client that
         * has initiated the request.
         */
        VRDPClipboardData clipboardData;

        CLIPBOARDLOG(("VRDE_CLIPBOARD_FUNCTION_DATA_WRITE: pvData %p, cbData %d\n", pvData, cbData));

        clipboardData.u32Function = u32Function;
        clipboardData.u32Format   = u32Format;

        if (cbData)
        {
            clipboardData.pvData = VRDPMemAlloc (cbData);

            if (clipboardData.pvData)
            {
                clipboardData.cbData = cbData;
            }
            else
            {
                /* Failed to allocate a memory buffer for data,
                 * send a 0 byte response to keep the protocol in sync.
                 */
                clipboardData.cbData = 0;
            }
        }
        else
        {
            clipboardData.pvData = NULL;
            clipboardData.cbData = 0;
        }

        if (clipboardData.pvData)
        {
            memcpy (clipboardData.pvData, pvData, cbData);
        }

        uint32_t u32ClientId = ASMAtomicXchgU32 (&mu32ClipboardDataWriteClientId, 0);

        CLIPBOARDLOG(("u32ClientId = %d\n", u32ClientId));

        if (u32ClientId)
        {
            /* Post the data block. */
            PostOutput (VRDP_OUTPUT_CLIPBOARD_DATA, u32ClientId, &clipboardData, sizeof (VRDPClipboardData));
        }
        else
        {
            CLIPBOARDLOG(("Invalid client id, cannot send clipboard data!!!\n"));
            VRDPMemFree (clipboardData.pvData);
        }
    }
    else
    {
        AssertFailed ();
    }
}

/** @thread INPUT */
bool VRDPServer::NotifyClipboardDataRequest (uint32_t u32ClientId)
{
    CLIPBOARDLOG(("u32ClientId = %d, mu32ClipboardDataWriteClientId = %d\n", u32ClientId, mu32ClipboardDataWriteClientId));
    return ASMAtomicCmpXchgU32 (&mu32ClipboardDataWriteClientId, u32ClientId, 0);
}

/** @thread INPUT */
void VRDPServer::NotifyClipboardFormatAnnounce (uint32_t u32ClientId)
{
    RT_NOREF1(u32ClientId);
    ASMAtomicXchgU32 (&mu32ClipboardDataWriteClientId, 0);
}


/*
 * The clipboard channel implementation.
 */
VRDPChannelClipboard::VRDPChannelClipboard (VRDPTP *pvrdptp)
    :
    m_pvrdptp (pvrdptp),
    m_channelOptions (0),
    m_channelId (0),
    m_fOperational(false),
    m_fSetup(false),
    m_pvCallbackUser (NULL),
    m_u32RequestedFormat (RDP_CF_INVALID),
    m_u32FormatHTML (0),
    m_u64AnnounceTime (0),
    m_u32AnnouncedFormats (0),
    m_u32ClipboardDataVRDPFormat (0),
    m_pvClipboardData (0),
    m_cbClipboardData (0),
    m_u64ServerAnnouncedTime (0)
{
    VRDPLock::Create("ClipboardData", &m_pLockClipboardData);
}

VRDPChannelClipboard::~VRDPChannelClipboard ()
{
    Close ();
    VRDPLock::Delete(&m_pLockClipboardData);
}

VRDPServer *VRDPChannelClipboard::Server (void)
{
    return m_pvrdptp->Client ()->Server ();
}

/** @thread INPUT */
int VRDPChannelClipboard::Open (uint32_t options, uint16_t id)
{
    Assert (Server ()->IsInputThread ());

    m_channelOptions = options;
    m_channelId      = id;

    InitOutgoingChannelId (m_channelId);

    return VINF_SUCCESS;
}

/** @thread INPUT */
void VRDPChannelClipboard::Setup (void)
{
    Assert (Server ()->IsInputThread ());
    if (m_channelId != 0)
    {
        int rc = m_pvrdptp->Desktop ()->InterceptChannel (m_pvrdptp->Client (), VRDE_CLIENT_INTERCEPT_CLIPBOARD, &m_pvCallbackUser);

        if (RT_SUCCESS (rc))
        {
            m_fSetup = true;
        }
    }
}

/** @thread INPUT */
void VRDPChannelClipboard::Close (void)
{
    m_channelOptions = 0;
    m_channelId      = 0;

    m_fOperational = false;
    m_fSetup = false;

    m_pvCallbackUser = NULL;

    m_u32RequestedFormat = RDP_CF_INVALID;
    m_u32FormatHTML = 0;

    m_u64AnnounceTime = 0;
    m_u32AnnouncedFormats = 0;

    m_u64ServerAnnouncedTime = 0;

    DiscardClipboardData ();
}

#pragma pack(1)
/*
 * RDP Clipboard Protocol structures.
 */
typedef struct _RDPClipboardPacketHdr
{
    uint16_t u16Type;
    uint16_t u16Status;
    uint32_t u32Length;
} RDPClipboardPacketHdr;

typedef struct _RDPClipboardConnect
{
    RDPClipboardPacketHdr hdr;
} RDPClipboardConnect;

typedef struct _RDPClipboardFormat
{
    uint32_t u32Id;
    uint16_t au16Name[16];
} RDPClipboardFormat;

typedef struct _RDPClipboardFormatAnnounce
{
    RDPClipboardPacketHdr hdr;

    RDPClipboardFormat aFormats[1];
} RDPClipboardFormatAnnounce;

typedef struct _RDPClipboardFormatAck
{
    RDPClipboardPacketHdr hdr;
} RDPClipboardFormatAck;

typedef struct _RDPClipboardDataRequest
{
    RDPClipboardPacketHdr hdr;

    uint32_t u32Format;
} RDPClipboardDataRequest;

typedef struct _RDPClipboardDataResponse
{
    RDPClipboardPacketHdr hdr;

    /* Data follow. */
} RDPClipboardDataResponse;
#pragma pack()


/**
 * Fill the packet header and send the packet to the client:
 *   CLIPRDR_CONNECT
 *   CLIPRDR_FORMAT_ANNOUNCE
 *   CLIPRDR_FORMAT_ACK
 *   CLIPRDR_DATA_REQUEST
 *   CLIPRDR_DATA_RESPONSE (error report only)
 * @thread output.
 */
static void clipboardSendPacket (VRDPOutputCtx *pOutputCtx,
                                 VRDPTP *pVRDPTP,
                                 uint16_t u16ChannelId,
                                 uint32_t u32ChannelOptions,
                                 RDPClipboardPacketHdr *pHdr,
                                 uint16_t u16Type,
                                 uint16_t u16Status,
                                 size_t cbTotal)
{
    uint32_t u32TotalLength = (uint32_t)cbTotal;
    Assert (pVRDPTP->Client ()->Server ()->IsOutputThread ());

    if (!u16ChannelId)
    {
        CLIPBOARDLOG(("Channel is not requested by the client!!!\n"));
        return;
    }

    Assert(u32TotalLength >= sizeof (RDPClipboardPacketHdr));

    pHdr->u16Type   = u16Type;
    pHdr->u16Status = u16Status;
    /* The field is the length after the header. */
    pHdr->u32Length = u32TotalLength - sizeof (RDPClipboardPacketHdr);

    VRDPBUFFER aBuffers[2];

    aBuffers[0].pv = pHdr;
    aBuffers[0].cb = u32TotalLength;

    static uint32_t u32Pad = 0;

    aBuffers[1].pv = &u32Pad;
    aBuffers[1].cb = sizeof (u32Pad);

    pVRDPTP->SendToChannel (pOutputCtx, u16ChannelId, u32ChannelOptions, RT_ELEMENTS (aBuffers), aBuffers, false);
}

/**
 * Send clipboard data to the client:
 *   CLIPRDR_DATA_RESPONSE
 * @thread output
 */
static void clipboardSendDataResponse (VRDPOutputCtx *pOutputCtx,
                                       VRDPTP *pVRDPTP,
                                       uint16_t u16ChannelId,
                                       uint32_t u32ChannelOptions,
                                       const void *pvData,
                                       uint32_t cbData)
{
    Assert (pVRDPTP->Client ()->Server ()->IsOutputThread ());

    if (!u16ChannelId)
    {
        CLIPBOARDLOG(("Channel is not requested by the client!!!\n"));
        return;
    }

    RDPClipboardDataResponse resp;

    resp.hdr.u16Type   = CLIPRDR_DATA_RESPONSE;
    resp.hdr.u16Status = CLIPRDR_RESPONSE;
    /* The field is the length after the header. */
    resp.hdr.u32Length = cbData;

    /* No extra fields to initialize in the 'resp'. */

    VRDPBUFFER aBuffers[3];

    aBuffers[0].pv = &resp;
    aBuffers[0].cb = sizeof (resp);

    aBuffers[1].pv = pvData;
    aBuffers[1].cb = cbData;

    static uint32_t u32Pad = 0;

    aBuffers[2].pv = &u32Pad;
    aBuffers[2].cb = sizeof (u32Pad);

    pVRDPTP->SendToChannel (pOutputCtx, u16ChannelId, u32ChannelOptions, RT_ELEMENTS (aBuffers), aBuffers, false);
}


/** @thread INPUT */
void VRDPChannelClipboard::Start (void)
{
    Assert(Server ()->IsInputThread ());

    if (!m_fSetup)
    {
        /* If the channel is not initialized. */
        CLIPBOARDLOG(("channel not requested by client!!!\n"));
        return;
    }

    if (m_fOperational)
    {
        CLIPBOARDLOG(("channel already intialized, negotiation cancelled!!!\n"));
        return;
    }

    m_fOperational = true;

    RDPClipboardConnect req;

    req.hdr.u16Type   = CLIPRDR_CONNECT;
    req.hdr.u16Status = CLIPRDR_REQUEST;
    req.hdr.u32Length = 0;

    /* Send the required packet to the output thread. */
    Server ()-> PostOutput (VRDP_OUTPUT_CLIPBOARD_PACKET, m_pvrdptp->Client ()->Id (), &req, sizeof (req));
}


/** @thread INPUT */
int VRDPChannelClipboard::ProcessChannelInput (const uint8_t *pu8Input, uint32_t u32Length)
{
    int rc = VINF_SUCCESS;

    if (u32Length < sizeof (RDPClipboardPacketHdr))
    {
        CLIPBOARDLOG(("header not received: expected %d, received %d!!!\n", sizeof (RDPClipboardPacketHdr), u32Length));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    hexdump((uint8_t *)pu8Input, u32Length);

    const RDPClipboardPacketHdr *pHdr = (const RDPClipboardPacketHdr *)pu8Input;

    if (pHdr->u16Status == CLIPRDR_ERROR)
    {
        /* Process errors from client. */
        CLIPBOARDLOG(("CLIPRDR_ERROR!!!\n"));
        switch (pHdr->u16Type)
        {
            case CLIPRDR_FORMAT_ACK:
            {
            } break;

            default:
            {
            } break;
        }
    }
    else
    {
        switch (pHdr->u16Type)
        {
            case CLIPRDR_FORMAT_ANNOUNCE:
            {
                /* Client announces available formats. */
                CLIPBOARDLOG(("CLIPRDR_FORMAT_ANNOUNCE \n"));

                const RDPClipboardFormatAnnounce *pFormatAnnounce = (const RDPClipboardFormatAnnounce *)pu8Input;

                if (sizeof (RDPClipboardPacketHdr) + pHdr->u32Length > u32Length)
                {
                    CLIPBOARDLOG(("not enough data: expected %d (pHdr->u32Length = %d), received %d\n",
                                  sizeof (RDPClipboardPacketHdr) + pHdr->u32Length, pHdr->u32Length, u32Length));

                    /* Do not reply, just ignore the packet. */
                    rc = VERR_VRDP_PROTOCOL_ERROR;
                    break;
                }

                /* Check whether the clipboard formats are supported. */
                uint32_t u32Formats = VRDE_CLIPBOARD_FORMAT_NULL;

                const RDPClipboardFormat *pRDPFormat = &pFormatAnnounce->aFormats[0];

                int cFormats = pHdr->u32Length / sizeof (RDPClipboardFormat);

                while (cFormats--)
                {
                    CLIPBOARDLOG(("format 0x%08X\n", pRDPFormat->u32Id));

                    if (pRDPFormat->u32Id == RDP_CF_DIB)
                    {
                        u32Formats |= VRDE_CLIPBOARD_FORMAT_BITMAP;
                        CLIPBOARDLOG(("RDP_CF_DIB\n"));
                    }
                    else if (pRDPFormat->u32Id == RDP_CF_UNICODETEXT)
                    {
                        u32Formats |= VRDE_CLIPBOARD_FORMAT_UNICODE_TEXT;
                        CLIPBOARDLOG(("RDP_CF_UNICODETEXT\n"));
                    }
                    else if (pRDPFormat->u32Id >= 0xC000)
                    {
                        if (memcmp (pRDPFormat->au16Name, RDP_CF_HTML_NAME, sizeof (RDP_CF_HTML_NAME)) == 0)
                        {
                            m_u32FormatHTML = pRDPFormat->u32Id;
                            u32Formats |= VRDE_CLIPBOARD_FORMAT_HTML;
                            CLIPBOARDLOG(("RDP_CF_HTML\n"));
                        }
                    }

                    pRDPFormat++;
                }

                uint64_t u64ServerAnnouncedTime = ASMAtomicReadU64(&m_u64ServerAnnouncedTime);

                if (u64ServerAnnouncedTime + 1000 >= VRDPTimeMilliTS ())
                {
                    /* Less than 1 second since last announcement from server.
                     * That is most likely a client resending the server announced formats.
                     */
                    u32Formats = VRDE_CLIPBOARD_FORMAT_NULL;
                    CLIPBOARDLOG(("Client reannounced formats, ignoring!!!\n"));
                }

                /* Reply with acknowledgement. */
                RDPClipboardFormatAck req;

                req.hdr.u16Type   = CLIPRDR_FORMAT_ACK;
                req.hdr.u16Status = CLIPRDR_RESPONSE;
                req.hdr.u32Length = 0;

                /* Send the required packet to the output thread. */
                Server ()-> PostOutput (VRDP_OUTPUT_CLIPBOARD_PACKET, m_pvrdptp->Client ()->Id (), &req, sizeof (req));

                if (u32Formats != VRDE_CLIPBOARD_FORMAT_NULL)
                {
                    CLIPBOARDLOG(("u32Formats = %x\n", u32Formats));

                    /* Remember when and which formats were received. */
                    ASMAtomicXchgU64(&m_u64AnnounceTime, VRDPTimeMilliTS ());
                    ASMAtomicXchgU32(&m_u32AnnouncedFormats, u32Formats);

                    /* Notify the server that the data transfer from client must be cancelled. */
                    Server ()->NotifyClipboardFormatAnnounce (Id ());

                    /* Inform the VM about available formats. */
                    appClipboard (m_pvrdptp->Client ()->Server ()->ApplicationCallbacks (),
                                  m_pvrdptp->Client ()->Server ()->ApplicationCallbackPointer (),
                                  m_pvCallbackUser,
                                  m_pvrdptp->Client ()->Id(),
                                  VRDE_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE,
                                  u32Formats,
                                  NULL,
                                  0);
                }
                else
                {
                    CLIPBOARDLOG(("VRDP: can't find suitable clipboard data format (or re-announcement)\n"));
                }
            } break;

            case CLIPRDR_FORMAT_ACK:
            {
                /* The previously sent CLIPRDR_FORMAT_ANNOUNCE was accepted by the client.
                 * Do nothing.
                 */
                CLIPBOARDLOG(("CLIPRDR_FORMAT_ACK\n"));
            } break;

            case CLIPRDR_DATA_REQUEST:
            {
                /* The client requests clipboard data. */
                const RDPClipboardDataRequest *pDataRequest = (const RDPClipboardDataRequest *)pu8Input;

                CLIPBOARDLOG(("CLIPRDR_DATA_REQUEST: 0x%04X\n", pDataRequest->u32Format));

                uint32_t u32Format = VRDE_CLIPBOARD_FORMAT_NULL;

                if (pDataRequest->u32Format == RDP_CF_DIB)
                {
                    u32Format = VRDE_CLIPBOARD_FORMAT_BITMAP;
                    CLIPBOARDLOG(("RDP_CF_DIB\n"));
                }
                else if (pDataRequest->u32Format == RDP_CF_UNICODETEXT)
                {
                    u32Format = VRDE_CLIPBOARD_FORMAT_UNICODE_TEXT;
                    CLIPBOARDLOG(("RDP_CF_UNICODETEXT\n"));
                }
                else if (pDataRequest->u32Format == RDP_CF_HTML_VRDP)
                {
                    u32Format = VRDE_CLIPBOARD_FORMAT_HTML;
                    CLIPBOARDLOG(("RDP_CF_HTML\n"));
                }

                int rc2 = VERR_NOT_SUPPORTED;
                if (u32Format != VRDE_CLIPBOARD_FORMAT_NULL)
                {
                    /* Inform the VM that the client needs the clipboard data.
                     * The VM will call VRDPClipboard entry point with
                     * VRDE_CLIPBOARD_FUNCTION_DATA_WRITE.
                     */

                    /* Remember which client is requesting data. */
                    if (Server ()->NotifyClipboardDataRequest (m_pvrdptp->Client ()->Id ()))
                    {
                        /* Notification succeed. */
                        rc2 = appClipboard (m_pvrdptp->Client ()->Server ()->ApplicationCallbacks (),
                                            m_pvrdptp->Client ()->Server ()->ApplicationCallbackPointer (),
                                            m_pvCallbackUser,
                                            m_pvrdptp->Client ()->Id(),
                                            VRDE_CLIPBOARD_FUNCTION_DATA_READ,
                                            u32Format,
                                            NULL,
                                            0);
                    }
                    else
                        CLIPBOARDLOG(("The server is busy. Cancel the data transfer!!!\n"));
                }
                else
                    CLIPBOARDLOG(("VRDP: no suitable data in clipboard\n"));

                /* Report error to RDP client in case of failure. Otherwise it might hang on remote side. */
                if (RT_FAILURE(rc2))
                {
                    RDPClipboardDataResponse req;

                    req.hdr.u16Type   = CLIPRDR_DATA_RESPONSE;
                    req.hdr.u16Status = CLIPRDR_ERROR;
                    req.hdr.u32Length = 0;

                    /* Send the packet to the output thread. */
                    Server ()-> PostOutput (VRDP_OUTPUT_CLIPBOARD_PACKET, m_pvrdptp->Client ()->Id (), &req, sizeof (req));
                }

            } break;

            case CLIPRDR_DATA_RESPONSE:
            {
                /* The client responded with a clipboard data. */
                CLIPBOARDLOG(("CLIPRDR_DATA_RESPONSE\n"));

                const RDPClipboardDataResponse *pDataResponse = (const RDPClipboardDataResponse *)pu8Input;

                if (sizeof (RDPClipboardPacketHdr) + pHdr->u32Length > u32Length)
                {
                    CLIPBOARDLOG(("not enough data: expected %d (pHdr->u32Length = %d), received %d\n",
                                  sizeof (RDPClipboardPacketHdr) + pHdr->u32Length, pHdr->u32Length, u32Length));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                const void *pvData = (const uint8_t *)pDataResponse + sizeof (RDPClipboardDataResponse);

                uint32_t u32Format = VRDE_CLIPBOARD_FORMAT_NULL;

                if (m_u32RequestedFormat == RDP_CF_UNICODETEXT)
                {
                    u32Format = VRDE_CLIPBOARD_FORMAT_UNICODE_TEXT;
                    CLIPBOARDLOG(("Received UNICODETEXT: [%ls]\n", pvData));
                }
                else if (m_u32RequestedFormat == RDP_CF_DIB)
                {
                    u32Format = VRDE_CLIPBOARD_FORMAT_BITMAP;
                    CLIPBOARDLOG(("Received BITMAP\n"));
                }
                else if (m_u32RequestedFormat == m_u32FormatHTML)
                {
                    u32Format = VRDE_CLIPBOARD_FORMAT_HTML;
                    CLIPBOARDLOG(("Received HTML: [%s]\n", pvData));
                }
                else
                {
                    AssertMsgFailed(("Invalid format %d!!!\n", m_u32RequestedFormat));
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                /* Save the arrived data and set the flag. */
                clipboardDataCache (u32Format, pvData, pHdr->u32Length);

                m_u32RequestedFormat = RDP_CF_INVALID;
            } break;

            case CLIPRDR_CONNECT: /* That is not expected from the client. Report an error. */
            default:
            {
                VRDPLOGREL(("Clipboard: Unsupported packet type 0x%04X!!!\n", pHdr->u16Type));
                return VERR_VRDP_PROTOCOL_ERROR;
            }
        }
    }

    return rc;
}

/** Called on behalf of the input thread when it has to send a reply to the client.
 * @thread OUTPUT
 */
int VRDPChannelClipboard::SendPacket (const void *pv, uint32_t cb)
{
    Assert(Server ()->IsOutputThread ());
    Assert(cb >= sizeof (RDPClipboardPacketHdr));
    Assert(m_channelId != 0);

    VRDPBUFFER aBuffers[2];

    aBuffers[0].pv = pv;
    aBuffers[0].cb = cb;

    static uint32_t u32Pad = 0;

    aBuffers[1].pv = &u32Pad;
    aBuffers[1].cb = sizeof (u32Pad);

    return m_pvrdptp->SendToChannel (this /* as OutputCtx */,
                                     m_channelId, m_channelOptions,
                                     RT_ELEMENTS (aBuffers), aBuffers, false);
}

/** Processes the requests which were initiated by the VM.
 *
 * @thread OUTPUT
 */
int VRDPChannelClipboard::SendData (const void *pv, uint32_t cb)
{
    RT_NOREF1(cb);
    int rc = VINF_SUCCESS;

    VRDPClipboardData *pClipboardData = (VRDPClipboardData *)pv;

    switch (pClipboardData->u32Function)
    {
        case VRDE_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE:
        {
            /* The guest has announced available clipboard formats. */
            CLIPBOARDLOG(("VRDE_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE 0x%08X\n", pClipboardData->u32Format));

            /* A format entry in the list consists of 32 bit format identifier CF_*
             * followed by 32 bytes of some descriptions. These bytes can be 0.
             */

            /* Allocate the packet that is enough for 32 formats. */
            size_t cbReq = sizeof (RDPClipboardFormatAnnounce) + (32 - 1) * sizeof (RDPClipboardFormat);

            RDPClipboardFormatAnnounce *pReq = (RDPClipboardFormatAnnounce *)alloca (cbReq);

            /* Index in the array of formats. */
            int iFormat = 0;

            if (pClipboardData->u32Format & VRDE_CLIPBOARD_FORMAT_UNICODE_TEXT)
            {
                CLIPBOARDLOG(("RDP_CF_UNICODETEXT\n"));
                pReq->aFormats[iFormat].u32Id = RDP_CF_UNICODETEXT;
                memset (pReq->aFormats[iFormat].au16Name, 0, sizeof (pReq->aFormats[iFormat].au16Name));
                iFormat++;
            }

            if (pClipboardData->u32Format & VRDE_CLIPBOARD_FORMAT_BITMAP)
            {
                CLIPBOARDLOG(("RDP_CF_DIB\n"));
                pReq->aFormats[iFormat].u32Id = RDP_CF_DIB;
                memset (pReq->aFormats[iFormat].au16Name, 0, sizeof (pReq->aFormats[iFormat].au16Name));
                iFormat++;
            }

            if (pClipboardData->u32Format & VRDE_CLIPBOARD_FORMAT_HTML)
            {
                CLIPBOARDLOG(("RDP_CF_HTML\n"));
                pReq->aFormats[iFormat].u32Id = RDP_CF_HTML_VRDP;
                memset (pReq->aFormats[iFormat].au16Name, 0, sizeof (pReq->aFormats[iFormat].au16Name));
                memcpy (pReq->aFormats[iFormat].au16Name, RDP_CF_HTML_NAME, sizeof (RDP_CF_HTML_NAME));
                iFormat++;
            }

            if (iFormat == 0)
            {
                CLIPBOARDLOG(("VRDE_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE: iFormat = %d!!!\n", iFormat));
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            ASMAtomicXchgU64(&m_u64ServerAnnouncedTime, VRDPTimeMilliTS ());

            /* Compute cbReq. The [0] element is already counted by sizeof (RDPClipboardFormatAnnounce). */
            cbReq = sizeof (RDPClipboardFormatAnnounce) + (iFormat - 1) * sizeof (RDPClipboardFormat);

            clipboardSendPacket (this /* as OutputCtx */, m_pvrdptp, m_channelId, m_channelOptions,
                                 &pReq->hdr, CLIPRDR_FORMAT_ANNOUNCE, CLIPRDR_REQUEST, cbReq);
        } break;

        case VRDE_CLIPBOARD_FUNCTION_DATA_READ:
        {
            /* Guest requests clipboard data. */
            CLIPBOARDLOG(("VRDE_CLIPBOARD_FUNCTION_DATA_READ\n"));

            RDPClipboardDataRequest req;
            req.u32Format = 0; /* (shut up MSC) */

            int iFormat = 0;

            if (pClipboardData->u32Format & VRDE_CLIPBOARD_FORMAT_UNICODE_TEXT)
            {
                req.u32Format = RDP_CF_UNICODETEXT;
                iFormat++;
                CLIPBOARDLOG(("RDP_CF_UNICODETEXT\n"));
            }

            if (pClipboardData->u32Format & VRDE_CLIPBOARD_FORMAT_BITMAP)
            {
                req.u32Format = RDP_CF_DIB;
                iFormat++;
                CLIPBOARDLOG(("RDP_CF_DIB\n"));
            }

            if (pClipboardData->u32Format & VRDE_CLIPBOARD_FORMAT_HTML)
            {
                req.u32Format = m_u32FormatHTML;
                iFormat++;
                CLIPBOARDLOG(("RDP_CF_HTML\n"));
            }

            if (iFormat != 1)
            {
                AssertMsgFailed(("VRDE_CLIPBOARD_FUNCTION_DATA_READ: iFormat = %d!!!\n", iFormat));
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            m_u32RequestedFormat = req.u32Format;

            clipboardSendPacket (this /* as OutputCtx */, m_pvrdptp, m_channelId, m_channelOptions,
                                 &req.hdr, CLIPRDR_DATA_REQUEST, CLIPRDR_REQUEST, sizeof (req));
        } break;

        case VRDE_CLIPBOARD_FUNCTION_DATA_WRITE:
        {
            CLIPBOARDLOG(("VRDE_CLIPBOARD_FUNCTION_DATA_WRITE\n"));

            /* Host responds to the client's request for clipboard data. */
            clipboardSendDataResponse (this /* as pOutputCtx */, m_pvrdptp, m_channelId, m_channelOptions,
                                       pClipboardData->pvData, pClipboardData->cbData);
        } break;

        default:
            AssertMsgFailed (("fn 0x%X\n", pClipboardData->u32Function));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    return rc;
}

void VRDPChannelClipboard::QueryClipboardFormats (volatile uint32_t *pu32AnnouncedFormats, volatile uint64_t *pu64AnnounceTime)
{
    ASMAtomicXchgU64(pu64AnnounceTime, m_u64AnnounceTime);
    ASMAtomicXchgU32(pu32AnnouncedFormats, m_u32AnnouncedFormats);
}

bool VRDPChannelClipboard::QueryClipboardData (uint32_t u32Format, void *pvData, uint32_t cbData, uint32_t *pcbActualRead)
{
    bool fRc = false;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLockClipboardData)))
    {
        fRc = (u32Format == m_u32ClipboardDataVRDPFormat);

        if (fRc)
        {
            /* How many bytes the client has. */
            *pcbActualRead = m_cbClipboardData;

            /* Copy the data to the callers buffer if it is big enough. */
            if (cbData >= m_cbClipboardData)
            {
                if (m_cbClipboardData)
                {
                    Assert(m_pvClipboardData);
                    memcpy (pvData, m_pvClipboardData, m_cbClipboardData);
                }
            }
        }

        CLIPBOARDLOG(("fRc = %d, u32Format = 0x%x, m_u32ClipboardDataVRDPFormat = 0x%x\n", fRc, u32Format, m_u32ClipboardDataVRDPFormat));

        VRDPLock::Unlock(m_pLockClipboardData);
    }

    return fRc;
}

void VRDPChannelClipboard::DiscardClipboardData (void)
{
    if (RT_SUCCESS(VRDPLock::Lock(m_pLockClipboardData)))
    {
        CLIPBOARDLOG(("\n"));
        m_u32ClipboardDataVRDPFormat = VRDE_CLIPBOARD_FORMAT_NULL;
        VRDPMemFree (m_pvClipboardData);
        m_pvClipboardData = NULL;
        m_cbClipboardData = 0;

        VRDPLock::Unlock(m_pLockClipboardData);
    }
}

void VRDPChannelClipboard::clipboardDataCache (uint32_t u32Format, const void *pvData, uint32_t cbData)
{
    if (RT_SUCCESS(VRDPLock::Lock(m_pLockClipboardData)))
    {
        /* Discard existing data. */
        DiscardClipboardData ();

        CLIPBOARDLOG(("cbData = %d\n", cbData));
        if (cbData)
        {
            void *pv = VRDPMemAlloc (cbData);

            memcpy (pv, pvData, cbData);

            m_pvClipboardData = pv;
        }

        m_cbClipboardData = cbData;
        m_u32ClipboardDataVRDPFormat = u32Format;

        VRDPLock::Unlock(m_pLockClipboardData);
    }
}
