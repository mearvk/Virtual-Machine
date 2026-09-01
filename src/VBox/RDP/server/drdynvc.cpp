/* $Id: drdynvc.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol - DRDYNVC virtual channel.
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

#include "vrdpserv.h"
#include "utils.h"

/*
 * Dynamic virtual channels implementation.
 */

/* Output event parameters. */
#define DVC_READY           0
#define DVC_CREATE_CHANNELS 1

typedef struct DVCPARM
{
    uint32_t u32Code;
} DVCPARM;

/* A dynamic channel states. */
#define DVC_CH_EMPTY        0
#define DVC_CH_CREATE       1
#define DVC_CH_CREATE_SENT  2
#define DVC_CH_CREATED      3
#define DVC_CH_CLOSE        4

VRDPChannelDVC::VRDPChannelDVC (VRDPTP *pvrdptp)
    :
    VRDPChannel (),
    m_pvrdptp (pvrdptp)
{
    initMembers ();
}

void VRDPChannelDVC::initMembers (void)
{
    m_channelOptions = 0;
    m_channelId = 0;

    m_fOperational = false;
    m_fSetup = false;

    memset (&m_channels[0], 0, sizeof (m_channels));
    AssertCompile(DVC_CH_EMPTY == 0);
}

VRDPChannelDVC::~VRDPChannelDVC ()
{
    Close ();
}

int VRDPChannelDVC::Open (uint32_t options, uint16_t id)
{
    DVCLOG(("DVC: Open id = %d\n", id));

    m_channelOptions = options;
    m_channelId = id;

    InitOutgoingChannelId (m_channelId);

    return VINF_SUCCESS;
}

void VRDPChannelDVC::Setup (void)
{
    DVCLOG(("DVC: Setup\n"));

    if (m_channelId != 0)
    {
        /* If the client supports dynamic virtual channels then normally it supports audio input channel. */
        char *pszValue = NULL;
        uint32_t cbOut = 0;
        VRDPServer *pServer = m_pvrdptp->Client()->Server();
        int rc = appFeature (pServer->ApplicationCallbacks (), pServer->ApplicationCallbackPointer (),
                             "Property/Client/DisableUpstreamAudio",
                             &pszValue, &cbOut);
        if (RT_FAILURE (rc))
        {
            pszValue = NULL;
        }

        if (   !pszValue
            || *pszValue == 0
            || RTStrCmp(pszValue, "0") == 0
            || RTStrICmp(pszValue, "false") == 0)
        {
            m_pvrdptp->Desktop ()->InterceptChannel (m_pvrdptp->Client (), VRDE_CLIENT_INTERCEPT_AUDIO_INPUT, NULL);
            VRDPLOGREL(("Enabling upstream audio.\n"));
        }

        if (pszValue)
        {
            VRDPMemFree(pszValue);
        }

        m_fSetup = true;

        m_pvrdptp->Client()->Server()->TSMF()->TSMFClientConnectedDVC(m_pvrdptp->Client()->Id());
    }

    return;
}

void VRDPChannelDVC::Close (void)
{
    unsigned i;
    for (i = 0; i < RT_ELEMENTS(m_channels); i++)
    {
        if (m_channels[i].u32ChannelStatus != DVC_CH_EMPTY)
        {
            if (m_channels[i].pu8FragmentedData)
            {
                VRDPMemFree(m_channels[i].pu8FragmentedData);
                m_channels[i].pu8FragmentedData = NULL;
            }
            VRDPMemFree(m_channels[i].pszChannelName);
            m_channels[i].pszChannelName = NULL;
        }
    }

    m_pvrdptp->Client()->Server()->TSMF()->TSMFClientDisconnectedDVC(m_pvrdptp->Client()->Id());

    initMembers ();
}

#pragma pack(1)
typedef struct DVCHdr
{
    uint8_t u8Hdr;
} DVCHdr;
AssertCompileSize(DVCHdr, 1);

typedef struct DYNVC_CAPS_VERSION1
{
    DVCHdr   hdr;
    uint8_t  Pad;
    uint16_t Version;
} DYNVC_CAPS_VERSION1;
AssertCompileSize(DYNVC_CAPS_VERSION1, 4);

typedef struct DYNVC_CAPS_VERSION2
{
    DVCHdr   hdr;
    uint8_t  Pad;
    uint16_t Version;
    uint16_t PriorityCharge0;
    uint16_t PriorityCharge1;
    uint16_t PriorityCharge2;
    uint16_t PriorityCharge3;
} DYNVC_CAPS_VERSION2;
AssertCompileSize(DYNVC_CAPS_VERSION2, 12);

typedef struct DYNVC_CAPS_RSP
{
    DVCHdr   hdr;
    uint8_t  Pad;
    uint16_t Version;
} DYNVC_CAPS_RSP;
AssertCompileSize(DYNVC_CAPS_RSP, 4);

typedef struct DYNVC_CREATE_REQ
{
    DVCHdr   hdr;
    uint8_t  ChannelId;      /* VRDP always uses 8 bit dynamic channel ids. */
    uint8_t  ChannelName[1]; /* Zero-terminated ANSI string. */
} DYNVC_CREATE_REQ;
AssertCompileSize(DYNVC_CREATE_REQ, 3);

typedef struct DYNVC_CREATE_RSP
{
    DVCHdr   hdr;
    uint8_t  ChannelId;
    int32_t  CreationStatus;
} DYNVC_CREATE_RSP;
AssertCompileSize(DYNVC_CREATE_RSP, 6);

/*
 * The first block of data of a fragmented message.
 */
typedef struct DYNVC_DATA_HDR
{
    DVCHdr   hdr;
    uint8_t  ChannelId;
} DYNVC_DATA_HDR;
AssertCompileSize(DYNVC_DATA_HDR, 2);

typedef struct DYNVC_DATA_FIRST_1
{
    DYNVC_DATA_HDR hdr;
    uint8_t  Length;           /* Total length of the message to be sent. */
    /* Data follow. */
} DYNVC_DATA_FIRST_1;
AssertCompileSize(DYNVC_DATA_FIRST_1, 3);

typedef struct DYNVC_DATA_FIRST_2
{
    DYNVC_DATA_HDR hdr;
    uint16_t  Length;           /* Total length of the message to be sent. */
    /* Data follow. */
} DYNVC_DATA_FIRST_2;
AssertCompileSize(DYNVC_DATA_FIRST_2, 4);

typedef struct DYNVC_DATA_FIRST_4
{
    DYNVC_DATA_HDR hdr;
    uint32_t  Length;           /* Total length of the message to be sent. */
    /* Data follow. */
} DYNVC_DATA_FIRST_4;
AssertCompileSize(DYNVC_DATA_FIRST_4, 6);

/*
 * Send both single messages and blocks of fragmented messages.
 */
typedef struct DYNVC_DATA
{
    DYNVC_DATA_HDR hdr;
    /* Data follow. */
} DYNVC_DATA;
AssertCompileSize(DYNVC_DATA, 2);

typedef struct DYNVC_CLOSE
{
    DVCHdr   hdr;
    uint8_t  ChannelId;      /* VRDP always uses 8 bit dynamic channel ids. */
} DYNVC_CLOSE;
AssertCompileSize(DYNVC_CLOSE, 2);

#pragma pack()

#define DVC_MAX_PDU_SIZE 1600

#define DVC_CMD_CREATE     0x01
#define DVC_CMD_DATA_FIRST 0x02
#define DVC_CMD_DATA       0x03
#define DVC_CMD_CLOSE      0x04
#define DVC_CMD_CAPS       0x05

#define DVC_MAKE_HDR_INTERNAL(Cmd, Sp, cbChId) (uint8_t)(  (((Cmd)    & 0xF) << 4) \
                                                         | (((Sp)     & 0x3) << 2) \
                                                         |  ((cbChId) & 0x3)      )

#define DVC_MAKE_HDR(Cmd, Sp) DVC_MAKE_HDR_INTERNAL(Cmd, Sp, 0)

#define DVC_HDR_Cmd(hdr)    (uint8_t)(((hdr) >> 4) & 0xF)
#define DVC_HDR_Sp(hdr)     (uint8_t)(((hdr) >> 2) & 0x3)
#define DVC_HDR_cbChId(hdr) (uint8_t)( (hdr)       & 0x3)

void VRDPChannelDVC::Start (void)
{
    DVCLOG(("DVC: Start\n"));

    if (!m_fSetup)
    {
        /* If the channel is not initialized. */
        DVCLOG(("channel not supported, negotiation cancelled!!!\n"));
        return;
    }

    if (m_fOperational)
    {
        DVCLOG(("channel already intialized, negotiation cancelled!!!\n"));
        return;
    }

    /* Dynamic VC Capabilities PDU. Version 1 is enough. */
    DYNVC_CAPS_VERSION1 pdu;
    pdu.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_CAPS, 0);
    pdu.Pad = 0;
    pdu.Version = 1;

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = &pdu;
    aBuffers[0].cb = sizeof (pdu);

    m_pvrdptp->SendToChannel (this /* as OutputCtx */, m_channelId, m_channelOptions,
                              RT_ELEMENTS(aBuffers), aBuffers, false);
}


int VRDPChannelDVC::RegisterChannel (const char *pszChannelName,
                                     PFNDVCCALLBACK pfnDVCCallback,
                                     void *pvDVCCallback,
                                     uint8_t *pu8ChannelId)
{
    DVCLOG(("DVC: RegisterChannel name %s\n", pszChannelName));

    DVCData *pData = NULL;
    uint8_t u8ChannelId = 0;

    unsigned i;
    for (i = 0; i < RT_ELEMENTS(m_channels); i++)
    {
        if (m_channels[i].u32ChannelStatus == DVC_CH_EMPTY)
        {
            pData = &m_channels[i];
            u8ChannelId = i + 1;
            break;
        }
    }

    if (pData == NULL)
    {
        return VERR_NOT_SUPPORTED;
    }

    size_t cbName = strlen(pszChannelName) + 1;
    pData->pszChannelName = (char *)VRDPMemAlloc(cbName);
    if (!pData->pszChannelName)
    {
        return VERR_NO_MEMORY;
    }

    memcpy(pData->pszChannelName, pszChannelName, cbName);
    pData->u32ChannelStatus = DVC_CH_CREATE;
    pData->pfnDVCCallback = pfnDVCCallback;
    pData->pvDVCCallback = pvDVCCallback;
    pData->u8ChannelId = u8ChannelId;

    *pu8ChannelId = u8ChannelId;

    DVCPARM parm;

    parm.u32Code = DVC_CREATE_CHANNELS;

    m_pvrdptp->Client()->Server()->PostOutput (VRDP_OUTPUT_DVC, m_pvrdptp->Client ()->Id(), &parm, sizeof (parm));

    return VINF_SUCCESS;
}

/** @thread OUTPUT */
int VRDPChannelDVC::sendCreateChannel (const DVCData *pData)
{
    int rc = VINF_SUCCESS;

    DVCLOG(("DVC: SendCreateChannel op %d, name %s\n", m_fOperational, pData->pszChannelName));

    if (!m_fOperational)
    {
        return rc;
    }

    size_t ChannelNameLength = strlen(pData->pszChannelName);

    DYNVC_CREATE_REQ *pPdu = (DYNVC_CREATE_REQ *)alloca(sizeof (DYNVC_CREATE_REQ) + ChannelNameLength);

    pPdu->hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_CREATE, 0);
    pPdu->ChannelId = pData->u8ChannelId;
    memcpy(pPdu->ChannelName, pData->pszChannelName, ChannelNameLength + 1);

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = pPdu;
    aBuffers[0].cb = (uint32_t)(sizeof (DYNVC_CREATE_REQ) + ChannelNameLength);

    return sendBuffers (RT_ELEMENTS(aBuffers), aBuffers);
}

/** @thread OUTPUT */
int VRDPChannelDVC::SendData(uint8_t u8ChannelId, void *pvData, uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    DVCLOG(("DVC: SendData %d, [%d], pvData %p, cbData = %d\n", m_fOperational, u8ChannelId, pvData, cbData));
    // hexdump((uint8_t *)pvData, cbData);

    if (!m_fOperational)
    {
        return rc;
    }

    VRDPBUFFER aBuffers[2];

    /* Select appropriate PDU. */
    if (cbData <= DVC_MAX_PDU_SIZE - sizeof(DYNVC_DATA))
    {
        /* All data can be sent in a single data PDU. */
        DYNVC_DATA pdu;
        pdu.hdr.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_DATA, 0);
        pdu.hdr.ChannelId = u8ChannelId;

        aBuffers[0].pv = &pdu;
        aBuffers[0].cb = sizeof(pdu);

        aBuffers[1].pv = pvData;
        aBuffers[1].cb = cbData;

        rc = sendBuffers (RT_ELEMENTS(aBuffers), aBuffers);
    }
    else
    {
        const uint8_t *pu8Data = (const uint8_t *)pvData;
        uint32_t cbRemaining = cbData;

        /* Have to fragment data. Check how many bytes takes the length. */
        if (cbData < 0x10000)
        {
            /* 16 bits for length field. */
            DYNVC_DATA_FIRST_2 pdu;
            pdu.hdr.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_DATA_FIRST, 1);
            pdu.hdr.ChannelId = u8ChannelId;
            pdu.Length = (uint16_t)cbData;

            Assert(cbData > DVC_MAX_PDU_SIZE - sizeof (pdu));

            aBuffers[0].pv = &pdu;
            aBuffers[0].cb = sizeof(pdu);

            aBuffers[1].pv = pu8Data;
            aBuffers[1].cb = DVC_MAX_PDU_SIZE - sizeof (pdu);

            rc = sendBuffers (RT_ELEMENTS(aBuffers), aBuffers);

            pu8Data += DVC_MAX_PDU_SIZE - sizeof (pdu);
            cbRemaining -= DVC_MAX_PDU_SIZE - sizeof (pdu);
        }
        else
        {
            /* 32 bits for length field. */
            DYNVC_DATA_FIRST_4 pdu;
            pdu.hdr.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_DATA_FIRST, 2);
            pdu.hdr.ChannelId = u8ChannelId;
            pdu.Length = cbData;

            Assert(cbData > DVC_MAX_PDU_SIZE - sizeof (pdu));

            aBuffers[0].pv = &pdu;
            aBuffers[0].cb = sizeof(pdu);

            aBuffers[1].pv = pu8Data;
            aBuffers[1].cb = DVC_MAX_PDU_SIZE - sizeof (pdu);

            rc = sendBuffers (RT_ELEMENTS(aBuffers), aBuffers);

            pu8Data += DVC_MAX_PDU_SIZE - sizeof (pdu);
            cbRemaining -= DVC_MAX_PDU_SIZE - sizeof (pdu);
        }

        if (RT_SUCCESS(rc))
        {
            /* Send remaining data PDUs. */
            while (cbRemaining != 0)
            {
                DYNVC_DATA pdu;
                pdu.hdr.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_DATA, 0);
                pdu.hdr.ChannelId = u8ChannelId;

                uint32_t cbToSend = RT_MIN(cbRemaining, DVC_MAX_PDU_SIZE - sizeof (pdu));

                aBuffers[0].pv = &pdu;
                aBuffers[0].cb = sizeof(pdu);

                aBuffers[1].pv = pu8Data;
                aBuffers[1].cb = cbToSend;

                rc = sendBuffers (RT_ELEMENTS(aBuffers), aBuffers);

                pu8Data += cbToSend;
                cbRemaining -= cbToSend;

                if (RT_FAILURE(rc))
                {
                    break;
                }
            }
        }
    }

    return rc;
}

/** @thread OUTPUT */
int VRDPChannelDVC::SendDataBuffers(uint8_t u8ChannelId, VRDPBUFFER *paInputBuffers, int cInputBuffers)
{
    int rc = VINF_SUCCESS;

    int i;

    DVCLOG(("DVC: SendDataBuffers %d, [%d], paBuffers %p, cBuffers = %d\n", m_fOperational, u8ChannelId, paInputBuffers, cInputBuffers));
#ifdef DEBUG_sunlover
    for (i = 0; i < cInputBuffers; i++)
    {
        DVCLOG(("DVC:     [%d] pv %p, cb %d\n", i, paInputBuffers[i].pv, paInputBuffers[i].cb));
        // hexdump((uint8_t *)paInputBuffers[i].pv, paInputBuffers[i].cb);
    }
#endif

    if (!m_fOperational)
    {
        return rc;
    }

    /* Compute total size of data to send. It is used to choose correct DYNVC_DATA_FIRST/DYNVC_DATA sequence. */
    uint32_t cbSend = 0;

    for (i = 0; i < cInputBuffers; i++)
    {
        cbSend += paInputBuffers[i].cb;
    }

    DVCLOG(("DVC: SendDataBuffers cbSend = %d\n", cbSend));

    /** @todo Allocate VRDPBUFFER aBuffers[cBuffers + 1].
     * Currently maximum 2 buffers is used by TSMF.
     */
    if (cInputBuffers > 4)
    {
        DVCLOG(("DVC: SendDataBuffers too many buffers!!!\n"));
        return VERR_NOT_SUPPORTED;
    }

    VRDPBUFFER aBuffers[5];

    /* Select appropriate PDU. */
    if (cbSend <= DVC_MAX_PDU_SIZE - sizeof(DYNVC_DATA))
    {
        /* All data can be sent in a single DVC_CMD_DATA PDU. */
        DYNVC_DATA pdu;
        pdu.hdr.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_DATA, 0);
        pdu.hdr.ChannelId = u8ChannelId;

        aBuffers[0].pv = &pdu;
        aBuffers[0].cb = sizeof(pdu);

        memcpy(&aBuffers[1], &paInputBuffers[0], cInputBuffers * sizeof (VRDPBUFFER));

        rc = sendBuffers (cInputBuffers + 1, aBuffers);
    }
    else
    {
        /* Have to fragment data: DYNVC_DATA_FIRST + DYNVC_DATA + ...
         *
         * Preallocate memory for the PDU header.
         */
        uint8_t au8PDU[RT_MAX(sizeof (DYNVC_DATA_FIRST_2), sizeof (DYNVC_DATA_FIRST_4))];
        uint32_t cbPDU;

        /* Check how many bytes to send and choose the appropriate DATA_FIRST. */
        if (cbSend < 0x10000)
        {
            /* 16 bits for length field. */
            DYNVC_DATA_FIRST_2 *pPdu = (DYNVC_DATA_FIRST_2 *)&au8PDU[0];
            cbPDU = sizeof(DYNVC_DATA_FIRST_2);

            pPdu->hdr.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_DATA_FIRST, 1);
            pPdu->hdr.ChannelId = u8ChannelId;
            pPdu->Length = (uint16_t)cbSend;
        }
        else
        {
            /* 32 bits for length field. */
            DYNVC_DATA_FIRST_4 *pPdu = (DYNVC_DATA_FIRST_4 *)&au8PDU[0];
            cbPDU = sizeof(DYNVC_DATA_FIRST_4);

            pPdu->hdr.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_DATA_FIRST, 2);
            pPdu->hdr.ChannelId = u8ChannelId;
            pPdu->Length = (uint32_t)cbSend;
        }

        aBuffers[0].pv = &au8PDU[0];
        aBuffers[0].cb = cbPDU;

        /* How many bytes to send in DATA_FIRST. */
        const uint32_t cbDataFirst = DVC_MAX_PDU_SIZE - cbPDU;

        Assert(cbSend > cbDataFirst);

        /* Set up as many buffers as necessary to provide 'cbDataFirst' bytes. */
        int iCurrentInputBuffer = 0;       /* Index of input buffer currently used for preparing output buffers. */
        uint32_t cbCurrentInputBuffer = 0; /* How many bytes were used from the current buffer. */

        uint32_t cbDataRequired = cbDataFirst;

        int iOutputBuffer = 1; /* Already [0] for PDU. */
        for (i = 0; i < cInputBuffers; i++)
        {
            if (cbDataRequired < paInputBuffers[i].cb)
            {
                /* Last buffer for the DATA_FIRST. */
                if (cbDataRequired > 0)
                {
                    aBuffers[iOutputBuffer].pv = paInputBuffers[i].pv;
                    aBuffers[iOutputBuffer].cb = cbDataRequired;
                    iOutputBuffer++;
                }

                iCurrentInputBuffer = i;
                cbCurrentInputBuffer += cbDataRequired;

                break;
            }

            /* Copy entire buffer. */
            aBuffers[iOutputBuffer].pv = paInputBuffers[i].pv;
            aBuffers[iOutputBuffer].cb = paInputBuffers[i].cb;

            Assert(cbDataRequired >= aBuffers[iOutputBuffer].cb);
            cbDataRequired -= aBuffers[iOutputBuffer].cb;

            iOutputBuffer++;
        }

        /* iOutputBuffer is the number of prepared output buffers. */
        Assert(iOutputBuffer <= cInputBuffers + 1);

#ifdef DEBUG_sunlover
        uint32_t cbBuffersCopied = 0;
        int j;
        for (j = 0; j < iOutputBuffer; j++)
        {
             cbBuffersCopied += aBuffers[j].cb;
        }
        Assert(cbBuffersCopied == DVC_MAX_PDU_SIZE);
#endif

        rc = sendBuffers (iOutputBuffer, aBuffers);

        if (RT_SUCCESS(rc))
        {
            /* Need to send 'cbSend - cbDataFirst' bytes as DYNVC_DATA packets(s)
             * starting from 'paInputBuffers[iCurrentInputBuffer]' at offset 'cbCurrentInputBuffer'.
             * 'iCurrentInputBuffer' is the last buffer used in DATA_FIRST.
             */
            uint32_t cbRemaining = cbSend - cbDataFirst;

            while (cbRemaining != 0)
            {
                DYNVC_DATA pdu;
                pdu.hdr.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_DATA, 0);
                pdu.hdr.ChannelId = u8ChannelId;

                const uint32_t cbToSend = RT_MIN(cbRemaining, DVC_MAX_PDU_SIZE - sizeof (pdu));

                aBuffers[0].pv = &pdu;
                aBuffers[0].cb = sizeof(pdu);

                cbDataRequired = cbToSend;

                iOutputBuffer = 1;  /* Already [0] for PDU. */
                for (i = iCurrentInputBuffer; i < cInputBuffers; i++)
                {
                    Assert(paInputBuffers[i].cb >= cbCurrentInputBuffer);

                    if (cbDataRequired < paInputBuffers[i].cb - cbCurrentInputBuffer)
                    {
                        /* Last buffer for the current DYNVC_DATA. */
                        if (cbDataRequired > 0)
                        {
                            aBuffers[iOutputBuffer].pv = (uint8_t *)paInputBuffers[i].pv + cbCurrentInputBuffer;
                            aBuffers[iOutputBuffer].cb = cbDataRequired;
                            iOutputBuffer++;
                        }

                        iCurrentInputBuffer = i;
                        cbCurrentInputBuffer += cbDataRequired;

                        break;
                    }

                    /* Copy buffer. */
                    aBuffers[iOutputBuffer].pv = (uint8_t *)paInputBuffers[i].pv + cbCurrentInputBuffer;
                    aBuffers[iOutputBuffer].cb = paInputBuffers[i].cb - cbCurrentInputBuffer;

                    Assert(cbDataRequired >= aBuffers[iOutputBuffer].cb);
                    cbDataRequired -= aBuffers[iOutputBuffer].cb;

                    iOutputBuffer++;

                    /* The current buffer completely consumed and index will be incremented. */
                    cbCurrentInputBuffer = 0;

                }

                Assert(iOutputBuffer <= cInputBuffers + 1);

#ifdef DEBUG_sunlover
                cbBuffersCopied = 0;
                for (j = 0; j < iOutputBuffer; j++)
                {
                     cbBuffersCopied += aBuffers[j].cb;
                }
                Assert(cbBuffersCopied == cbToSend + sizeof (DYNVC_DATA));
#endif

                rc = sendBuffers (iOutputBuffer, aBuffers);

                if (RT_FAILURE(rc))
                {
                    break;
                }

                cbRemaining -= cbToSend;
            }
        }
    }

    return rc;
}

/** @thread OUTPUT */
int VRDPChannelDVC::SendCloseChannel (uint8_t u8ChannelId)
{
    int rc = VINF_SUCCESS;

    if (u8ChannelId == 0 || u8ChannelId > RT_ELEMENTS(m_channels))
    {
        return VERR_INVALID_PARAMETER;
    }

    DVCData *pData = &m_channels[u8ChannelId - 1];

    DVCLOG(("DVC: SendCloseChannel op %d, [%d], name %s\n", m_fOperational, u8ChannelId, pData->pszChannelName));

    if (!m_fOperational)
    {
        return rc;
    }

    pData->u32ChannelStatus = DVC_CH_CLOSE;

    DYNVC_CLOSE pdu;

    pdu.hdr.u8Hdr = DVC_MAKE_HDR(DVC_CMD_CLOSE, 0);
    pdu.ChannelId = u8ChannelId;

    VRDPBUFFER aBuffers[1];

    aBuffers[0].pv = &pdu;
    aBuffers[0].cb = sizeof (pdu);

    return sendBuffers (RT_ELEMENTS(aBuffers), aBuffers);
}

/** @thread OUTPUT */
int VRDPChannelDVC::sendBuffers (int cBuffers, const VRDPBUFFER *paBuffers)
{
    /* A Reset is not necessary because the contexts automatically resets after sending a packet. */
    Assert (IsReset ());
    Assert (m_fOperational);
    Assert (m_pvrdptp->Client ()->Server ()->IsOutputThread ());

    return m_pvrdptp->SendToChannel (this /* as OutputCtx */, m_channelId, m_channelOptions,
                                     cBuffers, paBuffers, false);
}

/** @thread OUTPUT */
int VRDPChannelDVC::ProcessOutput (const void *pvData, uint32_t cbData)
{
    NOREF(cbData);

    const DVCPARM *pParm = (DVCPARM *)pvData;

    if (pParm->u32Code == DVC_READY)
    {
        /* Create channels which were registered already. */
        createChannels();
    }
    else if (pParm->u32Code == DVC_CREATE_CHANNELS)
    {
        /* Create channels which were registered, if DVC is ready.
         * Otherwise ignore this request. Channels will be created
         * by DVC_READY.
         */
        if (m_fOperational)
        {
            createChannels();
        }
    }

    return VINF_SUCCESS;
}

/** @thread OUTPUT */
void VRDPChannelDVC::createChannels (void)
{
    /*
     * Create registered channels.
     */
    unsigned i;
    for (i = 0; i < RT_ELEMENTS(m_channels); i++)
    {
        if (m_channels[i].u32ChannelStatus == DVC_CH_CREATE)
        {
            m_channels[i].u32ChannelStatus = DVC_CH_CREATE_SENT;
            sendCreateChannel (&m_channels[i]);
        }
    }
}


/** @thread INPUT */
int VRDPChannelDVC::ProcessChannelInput(const uint8_t *pu8Input, uint32_t cbInput)
{
    int rc = VINF_SUCCESS;

    if (cbInput < sizeof(DVCHdr))
    {
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    DVCHdr const *pHdr = (const DVCHdr *)pu8Input;

    uint8_t const Cmd = DVC_HDR_Cmd(pHdr->u8Hdr);
    DVCLOG(("ProcessChannelInput: Cmd = %d\n", Cmd));

    switch (Cmd)
    {
        case DVC_CMD_CREATE:
        {
            if (cbInput < sizeof(DYNVC_CREATE_RSP))
            {
                rc = VERR_VRDP_PROTOCOL_ERROR;
                break;
            }

            DYNVC_CREATE_RSP const *pPDU = (DYNVC_CREATE_RSP *)pu8Input;
            DVCLOG(("ProcessChannelInput: DYNVC_CREATE_RSP: id %d, status 0x%x\n",
                    pPDU->ChannelId, pPDU->CreationStatus));

            if (pPDU->CreationStatus != 0)
            {
                /** @todo pfnDVCCallback to inform that create failed */
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            if (pPDU->ChannelId == 0 || pPDU->ChannelId > RT_ELEMENTS(m_channels))
            {
                /// @todo Post sendCloseChannel
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            DVCData *pData = &m_channels[pPDU->ChannelId - 1];
            Assert(pData->u32ChannelStatus == DVC_CH_CREATE_SENT);

            ASMAtomicCmpXchgU32(&pData->u32ChannelStatus, DVC_CH_CREATED, DVC_CH_CREATE_SENT);

            if (pData->pfnDVCCallback)
            {
                pData->pfnDVCCallback(pData->pvDVCCallback, DVC_EVENT_CREATE, NULL, 0);
            }
        } break;

        case DVC_CMD_DATA_FIRST:
        {
            if (cbInput < sizeof(DYNVC_DATA_HDR))
            {
                rc = VERR_VRDP_PROTOCOL_ERROR;
                break;
            }

            DYNVC_DATA_HDR const *pDataHdr = (DYNVC_DATA_HDR *)pu8Input;
            DVCLOG(("ProcessChannelInput: DYNVC_DATA_FIRST: id %d, cb %d, Sp %d\n",
                    pDataHdr->ChannelId, cbInput, DVC_HDR_Sp(pDataHdr->hdr.u8Hdr)));

            if (pDataHdr->ChannelId == 0 || pDataHdr->ChannelId > RT_ELEMENTS(m_channels))
            {
                /// @todo Post sendCloseChannel
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            DVCData *pData = &m_channels[pDataHdr->ChannelId - 1];
            if (pData->u32ChannelStatus != DVC_CH_CREATED)
            {
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            /* Reset current state. */
            VRDPMemFree(pData->pu8FragmentedData);
            pData->pu8FragmentedData        = NULL;
            pData->cbFragmentedLength       = 0;
            pData->fFragmentedInput         = false;
            pData->pu8FragmentedDataCurrent = NULL;
            pData->cbFragmentedRemaining    = 0;

            /* Get the total length of the message. */
            uint32_t cbTotal = 0;
            switch (DVC_HDR_Sp(pDataHdr->hdr.u8Hdr))
            {
                case 0:
                {
                    if (cbInput < sizeof(DYNVC_DATA_FIRST_1))
                    {
                        rc = VERR_VRDP_PROTOCOL_ERROR;
                        break;
                    }

                    DYNVC_DATA_FIRST_1 const *pPDU = (DYNVC_DATA_FIRST_1 *)pDataHdr;
                    cbTotal = pPDU->Length;

                    pu8Input += sizeof(DYNVC_DATA_FIRST_1);
                    cbInput -= sizeof(DYNVC_DATA_FIRST_1);
                } break;
                case 1:
                {
                    if (cbInput < sizeof(DYNVC_DATA_FIRST_2))
                    {
                        rc = VERR_VRDP_PROTOCOL_ERROR;
                        break;
                    }

                    DYNVC_DATA_FIRST_2 const *pPDU = (DYNVC_DATA_FIRST_2 *)pDataHdr;
                    cbTotal = pPDU->Length;

                    pu8Input += sizeof(DYNVC_DATA_FIRST_2);
                    cbInput -= sizeof(DYNVC_DATA_FIRST_2);
                } break;
                default:
                {
                    if (cbInput < sizeof(DYNVC_DATA_FIRST_4))
                    {
                        rc = VERR_VRDP_PROTOCOL_ERROR;
                        break;
                    }

                    DYNVC_DATA_FIRST_4 const *pPDU = (DYNVC_DATA_FIRST_4 *)pDataHdr;
                    cbTotal = pPDU->Length;

                    pu8Input += sizeof(DYNVC_DATA_FIRST_4);
                    cbInput -= sizeof(DYNVC_DATA_FIRST_4);
                } break;
            }

            if (RT_FAILURE(rc))
            {
                break;
            }

            /* Allocate memory for the message. */
            pData->pu8FragmentedData = (uint8_t *)VRDPMemAlloc(cbTotal);
            if (!pData->pu8FragmentedData)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            pData->cbFragmentedLength       = cbTotal;
            pData->fFragmentedInput         = true;
            pData->pu8FragmentedDataCurrent = pData->pu8FragmentedData;
            pData->cbFragmentedRemaining    = pData->cbFragmentedLength;

            uint32_t const cbFragment = RT_MIN(pData->cbFragmentedRemaining, cbInput);
            memcpy(pData->pu8FragmentedDataCurrent, pu8Input, cbFragment);
            pData->pu8FragmentedDataCurrent += cbFragment;
            pData->cbFragmentedRemaining -= cbFragment;

            if (pData->cbFragmentedRemaining == 0)
            {
                DVCLOG(("ProcessChannelInput: DYNVC_DATA_FIRST: all data in the first packet!!!\n"));
#ifdef DEBUG_sunlover
                AssertFailed();
#endif
                pData->fFragmentedInput = false;
                if (pData->pfnDVCCallback)
                {
                    pData->pfnDVCCallback(pData->pvDVCCallback, DVC_EVENT_DATA, pData->pu8FragmentedData, pData->cbFragmentedLength);
                }
            }
        } break;

        case DVC_CMD_DATA:
        {
            if (cbInput < sizeof(DYNVC_DATA))
            {
                rc = VERR_VRDP_PROTOCOL_ERROR;
                break;
            }

            DYNVC_DATA const *pPDU = (DYNVC_DATA *)pu8Input;
            pu8Input += sizeof(DYNVC_DATA);
            cbInput -= sizeof(DYNVC_DATA);
            DVCLOG(("ProcessChannelInput: DYNVC_DATA: id %d, cb %d\n", pPDU->hdr.ChannelId, cbInput));

            if (pPDU->hdr.ChannelId == 0 || pPDU->hdr.ChannelId > RT_ELEMENTS(m_channels))
            {
                /// @todo Post sendCloseChannel
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            DVCData *pData = &m_channels[pPDU->hdr.ChannelId - 1];
            if (pData->u32ChannelStatus != DVC_CH_CREATED)
            {
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            if (pData->fFragmentedInput)
            {
                uint32_t const cbFragment = RT_MIN(pData->cbFragmentedRemaining, cbInput);
                memcpy(pData->pu8FragmentedDataCurrent, pu8Input, cbFragment);
                pData->pu8FragmentedDataCurrent += cbFragment;
                pData->cbFragmentedRemaining -= cbFragment;

                if (pData->cbFragmentedRemaining == 0)
                {
                    pData->fFragmentedInput = false;
                    if (pData->pfnDVCCallback)
                    {
                        pData->pfnDVCCallback(pData->pvDVCCallback, DVC_EVENT_DATA, pData->pu8FragmentedData, pData->cbFragmentedLength);
                    }
                }
            }
            else
            {
                if (pData->pfnDVCCallback)
                {
                    pData->pfnDVCCallback(pData->pvDVCCallback, DVC_EVENT_DATA, pu8Input, cbInput);
                }
            }
        } break;

        case DVC_CMD_CLOSE:
        {
            if (cbInput < sizeof(DYNVC_CLOSE))
            {
                rc = VERR_VRDP_PROTOCOL_ERROR;
                break;
            }

            DYNVC_CLOSE const *pPDU = (DYNVC_CLOSE *)pu8Input;
            DVCLOG(("ProcessChannelInput: DVC_CLOSE: ChannelId %d\n", pPDU->ChannelId));

            if (pPDU->ChannelId == 0 || pPDU->ChannelId > RT_ELEMENTS(m_channels))
            {
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            DVCData *pData = &m_channels[pPDU->ChannelId - 1];
            if (pData->u32ChannelStatus == DVC_CH_CLOSE)
            {
                DVCLOG(("ProcessChannelInput: DVC_CLOSE: client responded to a close request for %s\n", pData->pszChannelName));
            }
            else
            {
                DVCLOG(("ProcessChannelInput: DVC_CLOSE: client asked to close channel!!!\n"));
            }
            pData->u32ChannelStatus = DVC_CH_EMPTY;

            if (pData->pfnDVCCallback)
            {
                pData->pfnDVCCallback(pData->pvDVCCallback, DVC_EVENT_CLOSE, NULL, 0);
            }

            VRDPMemFree(pData->pszChannelName);
            VRDPMemFree(pData->pu8FragmentedData);

            memset(pData, 0, sizeof (DVCData));
        } break;

        case DVC_CMD_CAPS:
        {
            if (cbInput < sizeof(DYNVC_CAPS_RSP))
            {
                rc = VERR_VRDP_PROTOCOL_ERROR;
                break;
            }

            DYNVC_CAPS_RSP const *pCaps = (DYNVC_CAPS_RSP *)pu8Input;
            RT_NOREF_PV(pCaps);
            DVCLOG(("ProcessChannelInput: DVC_CMD_CAPS: version %d\n", pCaps->Version));

            m_fOperational = true;

            DVCPARM parm;
            parm.u32Code = DVC_READY;
            m_pvrdptp->Client()->Server()->PostOutput(VRDP_OUTPUT_DVC, m_pvrdptp->Client()->Id(), &parm, sizeof(parm));
        } break;

        default:
        {
            /* Do nothing, skip unsupported. */
            DVCLOG(("ProcessChannelInput: Unsupported!!!\n"));
        } break;
    }

    DVCLOG(("ProcessChannelInput: result %Rrc\n", rc));
    return rc;
}
