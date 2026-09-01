/* $Id: iso.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "vrdpserv.h"

#define ISO_HDR_LEN 4
#define ISO_CR_LEN 7
#define ISO_CC_LEN 7
#define ISO_DT_LEN 3

#define TYPE_RDP_NEG_REQ     0x01
#define TYPE_RDP_NEG_RSP     0x02
#define TYPE_RDP_NEG_FAILURE 0x03

#define EXTENDED_CLIENT_DATA_SUPPORTED 0x01

#pragma pack(1)
typedef struct RDP_NEG
{
    uint8_t  u8Type;
    uint8_t  u8Flags;
    uint16_t u16Length;
    union
    {
        struct
        {
            uint32_t u32RequestedProtocols;
        } req;
        struct
        {
            uint32_t u32SelectedProtocol;
        } rsp;
        struct
        {
            uint32_t u32FailureCode;
        } failure;
    } u;
} RDP_NEG;
#pragma pack()

ISOTP::ISOTP (void)
{
    Reset ();
}

ISOTP::~ISOTP (void)
{
}

int ISOTP::Recv (VRDPInputCtx *pInputCtx)
{
    const uint8_t *pu8;

#ifdef VRDP_DEBUG_ISO
    pInputCtx->DumpRead ();
#endif /* VRDP_DEBUG_ISO */

    switch (m_enmStatus)
    {
        case VRDP_ISO_Status_CR:
        {
            return isoProcessCR(pInputCtx);
        }

        case VRDP_ISO_Status_DT:
        {
            /* DT packet is expected. */
            switch (pInputCtx->PacketVersion ())
            {
                case VRDP_CTX_VER_NULL:
                {
                    ISOLOG(("DT: no version!!!\n"));
                    AssertFailed ();
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                case 3:
                {
                    /* ISO version 3. */
                    pu8 = pInputCtx->Read (ISO_HDR_LEN);

                    if (pu8 == NULL)
                    {
                        ISOLOG(("DT: failed to read header.\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    /* byte version;
                     * byte reserved;
                     * uint16_be length;
                     */

                    unsigned uLength = pu8[2] * 0x100 + pu8[3];

                    if (   uLength <= ISO_HDR_LEN
                        || !pInputCtx->IsReadOK (uLength - ISO_HDR_LEN))
                    {
                        ISOLOG(("DT: Packet length is incorrect %d (to read %d)!!!\n", uLength, pInputCtx->BytesToRead ()));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    pu8 = pInputCtx->Read (ISO_DT_LEN);

                    if (!pu8)
                    {
                        ISOLOG(("CR: failed to read packet!!!\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    /* byte header_length;
                     * byte packet_code;
                     * byte eot;
                     */
                    if (pu8[1] == TPDU_DR)
                    {
                        VRDPLOGREL(("Received the Disconnect Request packet.\n"));
                        return VINF_VRDP_OPERATION_COMPLETED;
                    }

                    if (pu8[1] != TPDU_DT)
                    {
                        ISOLOG(("DT: not DT 0x%02X!!!\n", pu8[1]));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    /* The packet processing must continue. */
                    return VINF_VRDP_PROCESS_PDU;
                }

                default:
                {
                    /* Most likely RDP5:
                     *     version | ENCRYPTED 0x80: byte;
                     *     length: 2 bytes encoded;
                     *     sectp level data.
                     */
                    pu8 = pInputCtx->Read (1);

                    if (pu8 == NULL)
                    {
                        ISOLOG(("DT: failed to read version.\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    ISOLOG(("ISOTP::RecvPacket Non standard ISO version: 0x%X\n", *pu8));

                    pu8 = pInputCtx->Read (1);

                    if (pu8 == NULL)
                    {
                        ISOLOG(("DT: failed to read length 1st byte.\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    unsigned uLength = *pu8;

                    if (uLength & 0x80)
                    {
                        uLength &= ~0x80;
                        uLength <<= 8;

                        pu8 = pInputCtx->Read (1);

                        if (pu8 == NULL)
                        {
                            ISOLOG(("DT: failed to read length 2nd byte.\n"));
                            return VERR_VRDP_PROTOCOL_ERROR;
                        }
                        uLength += *pu8;
                    }

                    if (   uLength <= ISO_HDR_LEN
                        || !pInputCtx->IsReadOK (uLength - ISO_HDR_LEN))
                    {
                        ISOLOG(("DT: Packet length is incorrect %d (to read %d)!!!\n", uLength, pInputCtx->BytesToRead ()));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    ISOLOG(("ISOTP::RecvPacket ISO length is: %d\n", uLength));

                    /* The packet processing must continue. */
                    return VINF_VRDP_PROCESS_PDU;
                }
            }
        }
    }

    /* At that point there was a problem in the state processors. */
    AssertFailed ();
    return VERR_VRDP_PROTOCOL_ERROR;
}

#pragma pack(1)
typedef struct _ISODTHdr
{
    uint8_t u8Version;
    uint8_t u8Reserved;
    uint16_t u16LengthBE;
    uint8_t u8HeaderLength;
    uint8_t u8Code;
    uint8_t u8EOT;
} ISODTHdr;
#pragma pack()

int ISOTP::Send (uint8_t u8Version, VRDPCtx *pCtx)
{
    if (m_enmStatus != VRDP_ISO_Status_DT)
    {
        /* The protocol is not yet initialized. */
        return VERR_NOT_SUPPORTED;
    }

    if (u8Version == 3)
    {
        /* Reply with a DT packet. */
        ISODTHdr *pDTHdr = (ISODTHdr *)pCtx->AppendProtocolHeader (sizeof (ISODTHdr));
        Assert (pDTHdr);

        pDTHdr->u8Version      = 3;
        pDTHdr->u8Reserved     = 0;
        pDTHdr->u16LengthBE    = RT_H2BE_U16(pCtx->SizeOfWrite ());
        pDTHdr->u8HeaderLength = ISO_DT_LEN - 1;
        pDTHdr->u8Code         = TPDU_DT;
        pDTHdr->u8EOT          = 0x80;
    }
    else
    {
        /* RDP5 packet. */
        uint16_t u16Length = pCtx->SizeOfWrite () + 3; /* u8Version and possibly 2 bytes length */

        if (u16Length <= 0x80)
        {
            /* Encode the length as 1 byte. */
            u16Length--;

            uint8_t *pu8 = pCtx->AppendProtocolHeader (2);
            Assert (pu8);

            *pu8++ = u8Version;
            *pu8++ = (uint8_t)RT_LOBYTE(u16Length);
        }
        else
        {
            /* Encode the length as the two bytes value. */
            u16Length |= 0x8000;

            uint8_t *pu8 = pCtx->AppendProtocolHeader (3);
            Assert (pu8);

            *pu8++ = u8Version;
            *pu8++ = (uint8_t)RT_HIBYTE(u16Length);
            *pu8++ = (uint8_t)RT_LOBYTE(u16Length);
        }
    }

#ifdef VRDP_DEBUG_ISO
    pCtx->DumpWrite ();
#endif /* VRDP_DEBUG_ISO */

    return pCtx->SendPacket ();
}

void ISOTP::Reset (void)
{
    m_enmStatus = VRDP_ISO_Status_CR;
}

int ISOTP::isoProcessCR (VRDPInputCtx *pInputCtx)
{
    int rc = VINF_SUCCESS;

    const uint8_t *pu8;
    uint8_t *pu8Write;

    /* 3.3.5.3.1 Processing X.224 Connection Request PDU. */

    /* CR packet is expected.
     *
     * ISO header:
     *   uint8     version = 3;
     *   uint8     reserved = 0;
     *   uint16_be length;
     */
    pu8 = pInputCtx->Read (ISO_HDR_LEN);

    if (   pu8 == NULL
        || pInputCtx->PacketVersion () != 3)
    {
        ISOLOG(("CR: failed to read header!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    unsigned uLength = pu8[2] * 0x100 + pu8[3];

    if (   uLength < ISO_HDR_LEN + ISO_CR_LEN
        || uLength > ISO_HDR_LEN + 0xFF) /* Maximum possible length for a CR packet. */
    {
        ISOLOG(("CR: invalid packet length %d!!!\n", uLength));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* Connection Request:
     *   uint8     header_length;
     *   uint8     packet_code;
     *   uint16_be dst_ref;
     *   uint16_be src_ref;
     *   uint8     class;
     */

    uLength -= ISO_HDR_LEN;

    pu8 = pInputCtx->Read (uLength);

    if (!pu8)
    {
        ISOLOG(("CR: failed to read packet!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    if (pu8[1] != TPDU_CR)
    {
        ISOLOG(("CR: not CR %d!!!\n", pu8[1]));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* Skip the cookie or the routing token field to check whether RDP_NEG_REQ is sent.
     * That is search for a 0x0d, 0x0a sequence.
     */

    bool fFound = false;
    unsigned i;
    for (i = ISO_CR_LEN; i < uLength - 1; i++)
    {
        if (pu8[i] == 0x0d && pu8[i + 1] == 0x0a)
        {
            i += 2;
            fFound = true;
            break;
        }
    }

    if (!fFound)
    {
        i = ISO_CR_LEN;
    }

    /* 'i' is the index of first byte where the RDP_NEG_REQ may be stored. */
    RDP_NEG *pReq = (RDP_NEG *)&pu8[i];

    if (   uLength - i == 8
        && pReq->u8Type == TYPE_RDP_NEG_REQ
        && pReq->u8Flags == 0
        && pReq->u16Length == RT_H2LE_U16_C(8))
    {
        /* Looks like it is a negotiation request. */
    }
    else
    {
        pReq = NULL;
    }

    VRDPData *pData = pInputCtx->Data();
    Assert(pData);

    if (pReq)
    {
        pData->RDPNegotiationRequest(true);
        pData->RDPNegotiationRequestedProtocols(RT_LE2H_U32(pReq->u.req.u32RequestedProtocols));
    }
    else
    {
        /* The client does not negotiate and therefore supports RDP only. */
        pData->RDPNegotiationRequest(false);
        pData->RDPNegotiationRequestedProtocols(PROTOCOL_RDP);
    }

    /*
     * Reply with a CC packet.
     */
    uLength = 0;

    /*
     * Check if the server supports the requested security protocol.
     */
    VRDPServer *pServer = pData->TP()->Client()->Server();

    uint32_t u32ResponseCode = 0;
    bool fSupported = pServer->SelectSecurityProtocol(pData->RDPNegotiationRequestedProtocols(),
                                                      &u32ResponseCode);

    TESTLOG(("SelectSecurityProtocol %d, req 0x%08X, resp 0x%08X\n",
             fSupported, pData->RDPNegotiationRequestedProtocols(),
             u32ResponseCode));

    if (   fSupported
        && (u32ResponseCode & PROTOCOL_SSL))
    {
        /* Try to initialize TLS and load necessary certificates. */
        rc = pInputCtx->Transport()->TLSInit(pInputCtx->TransportId());

        TESTLOG(("TLSInit rc %Rrc\n", rc));

        if (RT_FAILURE(rc))
        {
            fSupported = false;
            u32ResponseCode = SSL_CERT_NOT_ON_SERVER;
        }
    }

    if (pData->RDPNegotiationRequest())
    {
        /*  Send a RDP Negotiation Response/Failure. */
        pu8Write = pInputCtx->AppendProtocolHeader(sizeof(RDP_NEG));
        Assert (pu8Write);

        uLength += sizeof(RDP_NEG);

        RDP_NEG *p = (RDP_NEG *)pu8Write;

        if (fSupported)
        {
            p->u8Type = TYPE_RDP_NEG_RSP;
            p->u8Flags = EXTENDED_CLIENT_DATA_SUPPORTED; /* Always support in case it was a RDP_NEG from client. */
            p->u16Length = RT_H2LE_U16_C(8);
            p->u.rsp.u32SelectedProtocol = RT_H2LE_U32(u32ResponseCode);

            pData->RDPNegotiationSelectedProtocol(u32ResponseCode);
        }
        else
        {
            p->u8Type = TYPE_RDP_NEG_FAILURE;
            p->u8Flags = 0;
            p->u16Length = RT_H2LE_U16_C(8);
            p->u.failure.u32FailureCode = RT_H2LE_U32(u32ResponseCode);

            pData->RDPNegotiationSelectedProtocol(PROTOCOL_RDP);
        }
    }

    pu8Write = pInputCtx->AppendProtocolHeader (ISO_HDR_LEN + ISO_CC_LEN);
    Assert (pu8Write);

    uLength += ISO_HDR_LEN + ISO_CC_LEN;

    *pu8Write++ = 3;                         /* version */
    *pu8Write++ = 0;                         /* reserved */
    *pu8Write++ = 0;                         /* uint16_be size (high byte) */
    *pu8Write++ = (uint8_t)uLength;          /* uint16_be size (low byte) */
    *pu8Write++ = (uint8_t)(uLength - 5);    /* header length */
    *pu8Write++ = TPDU_CC;                   /* the code */
    *pu8Write++ = 0;                         /* uint16_be dst_ref (high byte) */
    *pu8Write++ = 0;                         /* uint16_be dst_ref (low byte) */
    *pu8Write++ = 0x34;                      /* uint16_be src_ref (high byte) */
    *pu8Write++ = 0x12;                      /* uint16_be src_ref (low byte) */
    *pu8Write   = 0;                         /* class */

    rc = pInputCtx->SendPacket ();

    if (RT_FAILURE (rc))
    {
        return rc;
    }

    if (!fSupported)
    {
        /* After sending the RDP Negotiation Failure structure the server MUST close the connection.
         * Also close the connection if the client did not negotiate and the server requires TLS.
         */
        return VINF_VRDP_OPERATION_COMPLETED;
    }

    /* From now only DT PDUs will go at this layer. */
    m_enmStatus = VRDP_ISO_Status_DT;

    /* Check if TLS transport will be used. */
    if (pData->RDPNegotiationSelectedProtocol() & PROTOCOL_SSL)
    {
        /* Since input is single threaded it is ok to enable TLS after sending the CC reply.
         * Notify the Transport instance of the client.
         */
        rc = pInputCtx->Transport()->TLSStart(pInputCtx->TransportId());

        TESTLOG(("TLSStart rc %Rrc\n", rc));

        if (RT_FAILURE(rc))
        {
            return rc;
        }

        pData->EnhancedSecurity(true);
    }

    /* Return success meaning that no processing by upper levels is necessary. */
    return VINF_SUCCESS;
}
