/* $Id: mcs.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#include "mcs.h"

#define BER_TAG_BOOLEAN         1
#define BER_TAG_INTEGER         2
#define BER_TAG_OCTET_STRING    4
#define BER_TAG_RESULT          10
#define MCS_TAG_DOMAIN_PARAMS   0x30

#define BER_TAG_OCTET_STRING_HDR_LEN 4

#define MCS_TAG_DOMAIN_PARAMS_LEN 26
#define MCS_TAG_DOMAIN_PARAMS_HDR_LEN 2

#define BER_TAG_RESULT_HDR_LEN 2
#define BER_TAG_INTEGER_HDR_LEN 2
#define MCS_CONNECT_RESPONSE_HDR_LEN 5


#define MCS_CONNECT_INITIAL     0x7f65
#define MCS_CONNECT_RESPONSE    0x7f66

enum MCS_PDU_TYPE
{
    MCS_EDRQ = 1,       /* Erect Domain Request */
    MCS_DPUM = 8,       /* Disconnect Provider Ultimatum */
    MCS_AURQ = 10,      /* Attach User Request */
    MCS_AUCF = 11,      /* Attach User Confirm */
    MCS_CJRQ = 14,      /* Channel Join Request */
    MCS_CJCF = 15,      /* Channel Join Confirm */
    MCS_SDRQ = 25,      /* Send Data Request */
    MCS_SDIN = 26       /* Send Data Indication */
};

MCSTP::MCSTP (void)
    :
    m_isotp(),
    m_mcsuserid (1) /// @todo
{
    Reset ();
}

MCSTP::~MCSTP (void)
{
}

static uint8_t *WriteBERHeader (VRDPInputCtx *pInputCtx, unsigned uTag, unsigned uLength, bool fAllocLength)
{
    unsigned uSize =   (fAllocLength? uLength: 0)
                     + (uTag >= 0x100? 2: 1)
                     + (uLength >= 0x80? 3: 1);

    uint8_t *pu8Write = pInputCtx->AppendProtocolHeader (uSize);

    if (uTag > 0xFF)
    {
        *pu8Write++ = (uint8_t)RT_HIBYTE(uTag);
        *pu8Write++ = (uint8_t)RT_LOBYTE(uTag);
    }
    else
    {
        *pu8Write++ = (uint8_t)RT_LOBYTE(uTag);
    }

    if (uLength >= 0x80)
    {
        *pu8Write++ = 0x82;
        *pu8Write++ = (uint8_t)RT_HIBYTE(uLength);
        *pu8Write++ = (uint8_t)RT_LOBYTE(uLength);
    }
    else
    {
        *pu8Write++ = (uint8_t)RT_LOBYTE(uLength);
    }

    return pu8Write;
}

static void WriteBERUint8 (VRDPInputCtx *pInputCtx, uint8_t u8Value)
{
    uint8_t *pu8Write = WriteBERHeader (pInputCtx, BER_TAG_INTEGER, 1, true);
    *pu8Write = u8Value;
}

static void WriteBERUint24 (VRDPInputCtx *pInputCtx, uint32_t u32Value)
{
    uint8_t *pu8Write = WriteBERHeader (pInputCtx, BER_TAG_INTEGER, 3, true);
    *pu8Write++ = (uint8_t)RT_BYTE3(u32Value);
    *pu8Write++ = (uint8_t)RT_BYTE2(u32Value);
    *pu8Write   = (uint8_t)RT_BYTE1(u32Value);
}

static void WriteDomainParams (VRDPInputCtx *pInputCtx, uint8_t u8Channels, uint8_t u8Users,
                                                        uint8_t u8Tokens, uint32_t u32PDUSize)
{
    /* Fields are written in reverse order. */
    WriteBERUint8  (pInputCtx, 2);
    WriteBERUint24 (pInputCtx, u32PDUSize); /* MSFT server sends the value as 24 bit integer. */
    WriteBERUint8  (pInputCtx, 1);
    WriteBERUint8  (pInputCtx, 0);
    WriteBERUint8  (pInputCtx, 1);
    WriteBERUint8  (pInputCtx, u8Tokens);
    WriteBERUint8  (pInputCtx, u8Users);
    WriteBERUint8  (pInputCtx, u8Channels);
    WriteBERHeader (pInputCtx, MCS_TAG_DOMAIN_PARAMS, MCS_TAG_DOMAIN_PARAMS_LEN, false);
}


static int ParseBERHeader (VRDPInputCtx *pInputCtx, unsigned uTag, unsigned *puLength)
{
    const uint8_t *pu8;

    unsigned uTmp;

    MCSLOG(("expecting tag %d\n", uTag));

    if (uTag > 0xFF)
    {
        pu8 = pInputCtx->Read (2);

        if (pu8 == NULL)
        {
            MCSLOG(("failed to read tag!!!\n"));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        uTmp = pu8[0] * 0x100 + pu8[1];
    }
    else
    {
        pu8 = pInputCtx->Read (1);

        if (pu8 == NULL)
        {
            MCSLOG(("failed to read tag!!!\n"));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        uTmp = pu8[0];
    }

    if (uTag != uTmp)
    {
        MCSLOG(("got tag %d!!!\n", uTmp));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    pu8 = pInputCtx->Read (1);

    if (pu8 == NULL)
    {
        MCSLOG(("failed to read length first byte!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    MCSLOG(("len first byte 0x%02X\n", pu8[0]));

    if (pu8[0] & 0x80)
    {
        unsigned i = pu8[0] & ~0x80;

        pu8 = pInputCtx->Read (i);

        if (i == 0 || pu8 == NULL)
        {
            MCSLOG(("failed to read length, %d bytes!!!\n", i));
            return VERR_VRDP_PROTOCOL_ERROR;
        }

        uTmp = 0;

        while (i--)
        {
            uTmp *= 0x100;
            uTmp += *pu8++;
        }
    }
    else
    {
        uTmp = pu8[0];
    }

    MCSLOG(("length %d, %d bytes left\n", uTmp, pInputCtx->BytesToRead ()));

    if (pInputCtx->BytesToRead () < uTmp)
    {
        MCSLOG(("ParseBERHeader len is greater than bytes left in packet!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    *puLength = uTmp;

    return VINF_SUCCESS;
}

static int ParseDomainParams (VRDPInputCtx *pInputCtx)
{
    unsigned uLength;

    int rc = ParseBERHeader (pInputCtx, MCS_TAG_DOMAIN_PARAMS, &uLength);

    if (RT_SUCCESS (rc))
    {
        MCSLOG(("Domain params length = %d\n", uLength));

        const uint8_t *pu8 = pInputCtx->Read (uLength);

        if (!pu8)
        {
            MCSLOG(("Failed to read %d bytes!!!\n", uLength));
            rc = VERR_VRDP_PROTOCOL_ERROR;
        }
    }

    return rc;
}

int MCSTP::SendConnectResponse (VRDPInputCtx *pInputCtx, uint8_t u8Result, int cChannels)
{
    RT_NOREF1(cChannels);
    uint8_t *pu8Write;

    uint16_t u16Length = pInputCtx->SizeOfWrite ();

    /* Construct the response from tail to begin. */
    WriteBERHeader (pInputCtx, BER_TAG_OCTET_STRING, u16Length, false);

    WriteDomainParams (pInputCtx, 0x22, 0x03, 0x00, 0xFFF8); /* Values taken from MSFT server response. */

    WriteBERUint8 (pInputCtx, 0); /* connect id */

    pu8Write = WriteBERHeader (pInputCtx, BER_TAG_RESULT, 1, true); /* result */
    *pu8Write = u8Result;

    u16Length = pInputCtx->SizeOfWrite ();

    WriteBERHeader (pInputCtx, MCS_CONNECT_RESPONSE, u16Length, false);

    return m_isotp.Send (3, pInputCtx);
}

void MCSTP::Disconnect (VRDPInputCtx *pInputCtx)
{
    /* Send MCS_DPUM. */
    uint8_t *pu8Write = pInputCtx->AppendProtocolHeader (2);

    Assert (pu8Write);

    *pu8Write++ = (MCS_DPUM << 2);             /* code */
    *pu8Write   = 0x80;                        /* result */

    m_isotp.Send (3, pInputCtx);
}

int MCSTP::Recv (VRDPInputCtx *pInputCtx)
{
    int rc = m_isotp.Recv (pInputCtx);

    if (rc != VINF_VRDP_PROCESS_PDU)
    {
        return rc;
    }

    const uint8_t *pu8;
    uint8_t *pu8Write;

    switch (m_enmStatus)
    {
        case VRDP_MCS_Status_RecvConnectInitial:
        {
            MCSLOG(("VRDP_MCS_Status_RecvConnectInitial\n"));

            unsigned uLength = 0;

            rc = ParseBERHeader (pInputCtx, MCS_CONNECT_INITIAL, &uLength);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            /* Calling domain. */
            rc = ParseBERHeader (pInputCtx, BER_TAG_OCTET_STRING, &uLength);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            MCSLOG(("Calling domain length = %d\n", uLength));

            pu8 = pInputCtx->Read (uLength);

            if (!pu8)
            {
                MCSLOG(("Failed to read %d bytes!!!\n", uLength));
                return VERR_VRDP_PROTOCOL_ERROR;
            }

            /* Called domain. */
            rc = ParseBERHeader (pInputCtx, BER_TAG_OCTET_STRING, &uLength);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            MCSLOG(("Called domain length = %d\n", uLength));

            pu8 = pInputCtx->Read (uLength);

            if (!pu8)
            {
                MCSLOG(("Failed to read %d bytes!!!\n", uLength));
                return VERR_VRDP_PROTOCOL_ERROR;
            }

            /* Upward flag */
            rc = ParseBERHeader (pInputCtx, BER_TAG_BOOLEAN, &uLength);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            MCSLOG(("Upward flag length = %d\n", uLength));

            pu8 = pInputCtx->Read (uLength);

            if (!pu8)
            {
                MCSLOG(("Failed to read %d bytes!!!\n", uLength));
                return VERR_VRDP_PROTOCOL_ERROR;
            }

            rc = ParseDomainParams (pInputCtx);  // target params (34, 2, 0, 0xffff)

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            rc = ParseDomainParams (pInputCtx);  // min params (1, 1, 1, 0x420)

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            rc = ParseDomainParams (pInputCtx);  // max params (0xffff, 0xfc17, 0xffff, 0xffff)

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            /* Length of following upper protocol data */
            rc = ParseBERHeader (pInputCtx, BER_TAG_OCTET_STRING, &uLength);

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            MCSLOG(("Upper protocol data length = %d\n", uLength));

            m_enmStatus = VRDP_MCS_Status_MCS;

            return VINF_VRDP_PROCESS_PDU;
        } break;

        case VRDP_MCS_Status_MCS:
        {
            if (pInputCtx->PacketVersion () != 3)
            {
                /* Only process standard ISO packets. RDP5 packets go directly to SECURE layer. */
                return VINF_VRDP_PROCESS_PDU;
            }

            /* The first byte is the type of the packet. */
            pu8 = pInputCtx->Read (1);

            if (pu8 == NULL)
            {
                MCSLOG(("failed to read code!!!\n"));
                return VERR_VRDP_PROTOCOL_ERROR;
            }

            switch (pu8[0] >> 2)
            {
                case MCS_SDRQ:
                {
                    /* 0,1: uint16_be mcs_userid;
                     * 2,3: uint16_be channel;
                     *   4: byte flags;
                     *   5: 2 bytes length.
                     * Only the 'channel' field is used, all other are skipped.
                     */
                    pu8 = pInputCtx->Read (6);

                    if (pu8 == NULL)
                    {
                        MCSLOG(("failed to read MCS_SDRQ!!!\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    pInputCtx->StoreIncomingChannelId (pu8[2] * 0x100 + pu8[3]);

                    if (pu8[5] & 0x80)
                    {
                        /* The 'length' field has second byte. */
                        pu8 = pInputCtx->Read (1);

                        if (pu8 == NULL)
                        {
                            MCSLOG(("failed to read second byte in length!!!\n"));
                            return VERR_VRDP_PROTOCOL_ERROR;
                        }
                    }

                    return VINF_VRDP_PROCESS_PDU;
                }

                case MCS_EDRQ:
                {
                    /* 0,1: uint16_be height;   ?
                     * 2,3: uint16_be interval; ?
                     */
                    pu8 = pInputCtx->Read (4);

                    if (pu8 == NULL)
                    {
                        MCSLOG(("failed to read MCS_EDRQ!!!\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    if (pInputCtx->BytesToRead () != 0)
                    {
                        MCSLOG(("Expected MCS_EDRQ too many bytes %d\n", pInputCtx->BytesToRead ()));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    /* No further processing required. */
                    return VINF_SUCCESS;
                }

                case MCS_AURQ:
                {
                    if (pInputCtx->BytesToRead () != 0)
                    {
                        MCSLOG(("Expected MCS_AURQ too many bytes %d\n", pInputCtx->BytesToRead ()));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    /* Reply with MCS_AUCF. */
                    pu8Write = pInputCtx->AppendProtocolHeader (4);

                    Assert (pu8Write);

                    *pu8Write++ = (MCS_AUCF << 2) | 2;          /* code */
                    *pu8Write++ = 0;                            /* result */
                    *pu8Write++ = (uint8_t)RT_HIBYTE(m_mcsuserid); /* uint16_be userid (high byte) */
                    *pu8Write   = (uint8_t)RT_LOBYTE(m_mcsuserid); /* uint16_be userid (low byte) */

                    rc = m_isotp.Send (3, pInputCtx);

                    if (RT_FAILURE (rc))
                    {
                        return rc;
                    }

                    /* No further processing required. */
                    return VINF_SUCCESS;
                }

                case MCS_CJRQ:
                {
                    /* 0,1: uint16_be userid;
                     * 2,3: uint16_be channelid;
                     */
                    pu8 = pInputCtx->Read (4);

                    if (pu8 == NULL)
                    {
                        MCSLOG(("failed to read MCS_CJRQ!!!\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    if (pInputCtx->BytesToRead () != 0)
                    {
                        MCSLOG(("Expected MCS_CJRQ too many bytes %d\n", pInputCtx->BytesToRead ()));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    uint16_t u16ChannelId = (uint16_t)(pu8[2] * 0x100 + pu8[3]);

                    /* Reply with MCS_CJCF */
                    pu8Write = pInputCtx->AppendProtocolHeader (8);

                    Assert (pu8Write);

                    *pu8Write++ = (MCS_CJCF << 2) | 2;           /* code */
                    *pu8Write++ = 0;                             /* result */
                    *pu8Write++ = (uint8_t)RT_HIBYTE(m_mcsuserid);  /* uint16_be userid (high byte) */
                    *pu8Write++ = (uint8_t)RT_LOBYTE(m_mcsuserid);  /* uint16_be userid (low byte) */
                    *pu8Write++ = (uint8_t)RT_HIBYTE(u16ChannelId);
                    *pu8Write++ = (uint8_t)RT_LOBYTE(u16ChannelId);
                    *pu8Write++ = (uint8_t)RT_HIBYTE(u16ChannelId);
                    *pu8Write   = (uint8_t)RT_LOBYTE(u16ChannelId);

                    rc = m_isotp.Send (3, pInputCtx);

                    if (RT_FAILURE (rc))
                    {
                        return rc;
                    }

                    /* No further processing required. */
                    return VINF_SUCCESS;
                }

                default:
                {
                    MCSLOG(("Unsupported code %02X (%d)!!!\n", pu8[0], pu8[0] >> 2));
                } break;
            }
        } break;
    }

    return VERR_VRDP_PROTOCOL_ERROR;
}

int MCSTP::Send (uint8_t u8Version, VRDPCtx *pCtx)
{
    /* RDP5 packets does not have the MCS layer. */
    if (u8Version == 3)
    {
        uint8_t *pu8Write;

        Assert (m_enmStatus == VRDP_MCS_Status_MCS);

        uint16_t u16Length = pCtx->SizeOfWrite ();
        uint16_t u16ChannelId = pCtx->QueryOutgoingChannelId ();

        /* Reply with a DT packet. */
        pu8Write = pCtx->AppendProtocolHeader (7 + (u16Length >= 0x80? 1: 0));

        Assert (pu8Write);

        *pu8Write++ = (MCS_SDIN << 2);               /* code */
        *pu8Write++ = (uint8_t)RT_HIBYTE(m_mcsuserid);  /* uint16_be userid (high byte) */
        *pu8Write++ = (uint8_t)RT_LOBYTE(m_mcsuserid);  /* uint16_be userid (low byte) */
        *pu8Write++ = (uint8_t)RT_HIBYTE(u16ChannelId); /* uint16_be channel (high byte) */
        *pu8Write++ = (uint8_t)RT_LOBYTE(u16ChannelId); /* uint16_be channel (low byte) */
        *pu8Write++ = 0x70;                          /* flags */

        if (u16Length < 0x80)
        {
            *pu8Write = (uint8_t)u16Length;
        }
        else
        {
            u16Length |= 0x8000;
            *pu8Write++ = (uint8_t)RT_HIBYTE(u16Length);
            *pu8Write   = (uint8_t)RT_LOBYTE(u16Length);
        }
    }

    return m_isotp.Send (u8Version, pCtx);
}

void MCSTP::Reset (void)
{
    m_isotp.Reset ();
    m_enmStatus = VRDP_MCS_Status_RecvConnectInitial;
}
