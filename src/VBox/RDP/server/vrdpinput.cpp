/* $Id: vrdpinput.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2013-2025 Oracle and/or its affiliates.
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
#include "vrdpinput.h"


#ifdef VRDP_DEBUG_INPUT
#  define INPUTLOG(a) do { VRDPLOG(("INPUT::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define INPUTLOG(a)
#endif /* VRDP_DEBUG_INPUT */

#ifdef DEBUG_sunlover
#  undef INPUTLOG
#  define INPUTLOG(a) do { LogRel(("INPUT::%s: ", __FUNCTION__)); LogRel(a); } while (0)
#endif

#define VRDP_PKT_TYPE_INPUT_CHANNEL  200

/* Information about a channel instance. One instance per client. */
typedef enum INPUTCHANNELSTATUS
{
    INPUT_CHANNEL_VOID,
    INPUT_CHANNEL_NEGOTIATING,
    INPUT_CHANNEL_ESTABLISHED,
    INPUT_CHANNEL_CLOSING
} INPUTCHANNELSTATUS;

typedef struct INPUTCHANNEL
{
    VRDPPKT pkt;

    RTLISTNODE nodeChannels;

    InputClient *pClientChannel;
    uint32_t u32ClientId;

    uint32_t u32Flags;
    uint32_t u32ProtocolVersion;
    uint16_t u16MaxTouchContacts;

    INPUTCHANNELSTATUS enmStatus;
} INPUTCHANNEL;

/*
 * Interface entry points.
 */

static DECLCALLBACK(void) VRDEInputSetup(HVRDESERVER hServer,
                                         uint32_t u32Method,
                                         uint32_t fu32Flags,
                                         const void *pvSetup,
                                         uint32_t cbSetup)
{
    VRDPServer *pServer = (VRDPServer *)hServer;

    VRDPAPILOG(("%p\n", pServer));

    if (pServer)
    {
        pServer->Input()->InputSetup(u32Method, fu32Flags, pvSetup, cbSetup);
    }

    return;
}

static VRDEINPUTINTERFACE gInputInterface =
{
    { 1, sizeof (VRDEINPUTINTERFACE) },

    VRDEInputSetup
};


/*
 * The VRDP server input implementation.
 */

VRDPInput::VRDPInput(VRDPServer *pServer)
    :
    m_pServer (pServer),
    m_fShutdown (false)
{
    RT_ZERO(m_interface);
    RTListInit(&m_listChannels);
    VRDPLock::Create("Input", &m_pLock);
}

/* virtual */ VRDPInput::~VRDPInput()
{
    VRDPLock::Delete(&m_pLock);
}

/* Helper to get the interface method table, called from VRDPServer::GetInterface */
int VRDPInput::GetInterfaceInput(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks, void *pvContext)
{
    int rc = VINF_SUCCESS;

    if (   pInterface->u64Version == 1
        && pInterface->u64Size == sizeof(VRDEINPUTINTERFACE)
        && pCallbacks->u64Version == 1
        && pCallbacks->u64Size == sizeof(VRDEINPUTCALLBACKS))
    {
        /* Interface version 1 is requested. */
        VRDEINPUTINTERFACE *p = (VRDEINPUTINTERFACE *)pInterface;

        m_interface.callbacks = *(VRDEINPUTCALLBACKS *)pCallbacks;
        m_interface.pvContext = pvContext;

        *p = gInputInterface;
    }
    else
    {
        rc = VERR_VERSION_MISMATCH;
    }

    return rc;
}

int VRDPInput::InputInitialize(void)
{
    INPUTLOG(("\n"));

    int rc = inInitialize();

    if (RT_FAILURE(rc))
    {
        inShutdown();
    }

    return rc;
}

void VRDPInput::InputShutdown(void)
{
    INPUTLOG(("\n"));

    inShutdown();
}

void VRDPInput::inShutdown(void)
{
    INPUTLOG(("\n"));
}

int VRDPInput::inInitialize(void)
{
    INPUTLOG(("\n"));
    return VINF_SUCCESS;
}

void VRDPInput::InputSetup(uint32_t u32Method, uint32_t fu32Flags, const void *pvSetup, uint32_t cbSetup)
{
    RT_NOREF2(pvSetup, cbSetup);
    INPUTLOG(("InputSetup: u32Method %d, fu32Flags %RX32, cbSetup %d\n%.*Rhxd\n",
               u32Method, fu32Flags, cbSetup, cbSetup, pvSetup));

    int rc = VINF_SUCCESS;
    if (u32Method == VRDE_INPUT_METHOD_TOUCH)
    {
        if (fu32Flags & VRDE_INPUT_F_ENABLE)
        {
            /** @todo */
        }
    }
    else
    {
        rc = VERR_INVALID_PARAMETER;
    }

    /** @todo callback */

    return;
}

static int inChannelCreate(INPUTCHANNEL **ppChannel)
{
    int rc = VINF_SUCCESS;

    INPUTCHANNEL *pChannel = (INPUTCHANNEL *)VRDPMemAllocZ(sizeof(INPUTCHANNEL));

    if (pChannel)
    {
        VRDPPktInit(&pChannel->pkt, VRDP_PKT_TYPE_INPUT_CHANNEL, sizeof(INPUTCHANNEL), NULL, NULL);
        VRDPPktAddRef(&pChannel->pkt);
        *ppChannel = pChannel;
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

int VRDPInput::inChannelAdd(INPUTCHANNEL *pChannel)
{
    int rc = VINF_SUCCESS;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        RTListAppend(&m_listChannels, &pChannel->nodeChannels);

        VRDPLock::Unlock(m_pLock);
    }
    else
    {
        VRDPPktRelease(&pChannel->pkt);

        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

void VRDPInput::inChannelRemove(INPUTCHANNEL *pChannel)
{
    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        RTListNodeRemove(&pChannel->nodeChannels);
        VRDPPktRelease(&pChannel->pkt);

        VRDPLock::Unlock(m_pLock);
    }
}

INPUTCHANNEL *VRDPInput::inChannelFind(uint32_t u32ClientId)
{
    INPUTCHANNEL *pChannel = NULL;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        INPUTCHANNEL *pIter;
        RTListForEach(&m_listChannels, pIter, INPUTCHANNEL, nodeChannels)
        {
            if (pIter->u32ClientId == u32ClientId)
            {
                pChannel = pIter;

                VRDPPktAddRef(&pChannel->pkt);

                break;
            }
        }

        VRDPLock::Unlock(m_pLock);
    }

    return pChannel;
}

int VRDPInput::inCallbackSetup(int rcRequest,
                               const void *pvResult,
                               uint32_t cbResult)
{
    INPUTLOG(("inCallbackSetup: %Rrc %p %d\n",
              rcRequest, pvResult, cbResult));

    if (m_interface.callbacks.VRDECallbackInputSetup)
    {
        m_interface.callbacks.VRDECallbackInputSetup(m_interface.pvContext, VRDE_INPUT_METHOD_TOUCH,
                                                     rcRequest, pvResult, cbResult);
        return VINF_SUCCESS;
    }

    return VERR_NOT_SUPPORTED;
}

int VRDPInput::inCallbackEvent(const void *pvEvent,
                               uint32_t cbEvent)
{
    INPUTLOG(("inCallbackEvent: %p %d\n",
              pvEvent, cbEvent));

    if (m_interface.callbacks.VRDECallbackInputEvent)
    {
        m_interface.callbacks.VRDECallbackInputEvent(m_interface.pvContext, VRDE_INPUT_METHOD_TOUCH,
                                                     pvEvent, cbEvent);
        return VINF_SUCCESS;
    }

    return VERR_NOT_SUPPORTED;
}

/** @thread INPUT */
int VRDPInput::inSendReady(InputClient *pClientChannel)
{
    VRDEINPUT_SC_READY_PDU msg;
    msg.header.u16EventId = VRDEINPUT_EVENTID_SC_READY;
    msg.header.u32PDULength = sizeof(msg);
    msg.u32ProtocolVersion = VRDEINPUT_PROTOCOL_V1;

    return pClientChannel->SendData(&msg, sizeof(msg));
}

void VRDPInput::inChannelCleanup(INPUTCHANNEL *pChannel)
{
    /* The dynamic channel was closed or requested a negotiation. */
    InputClient *pClientChannel = pChannel->pClientChannel;
    uint32_t u32ClientId = pChannel->u32ClientId;

    /** @todo */
    RT_NOREF3(pChannel, pClientChannel, u32ClientId);

    return;
}

/** @thread INPUT */
int VRDPInput::InputOnCreate(InputClient *pClientChannel)
{
    /* A new channel.
     * There can be only one channel per client.
     * Check that the client does not have the channel yet,
     * then create a context and start negotiation.
     */
    uint32_t u32ClientId = pClientChannel->Client()->Id();

    INPUTLOG(("OnCreate: %d\n",
              u32ClientId));

    INPUTCHANNEL *pChannel = inChannelFind(u32ClientId);
    if (pChannel)
    {
        VRDPPktRelease(&pChannel->pkt);
        INPUTLOG(("OnCreate: channel exists for client %u!!!\n", u32ClientId));
        return VERR_NOT_SUPPORTED;
    }

    /* Create a context. */
    int rc = inChannelCreate(&pChannel);

    if (RT_SUCCESS(rc))
    {
        pChannel->pClientChannel = pClientChannel;
        pChannel->u32ClientId = u32ClientId;
        pChannel->enmStatus = INPUT_CHANNEL_VOID;

        VRDPPktAddRef(&pChannel->pkt);
        rc = inChannelAdd(pChannel);

        if (RT_SUCCESS(rc))
        {
            /* Start negotiations with the client. */
            rc = inSendReady(pClientChannel);

            if (RT_SUCCESS(rc))
            {
                pChannel->enmStatus = INPUT_CHANNEL_NEGOTIATING;
                VRDPLOGREL(("INPUT created for %d\n", u32ClientId));
            }

            if (RT_FAILURE(rc))
            {
                inChannelRemove(pChannel);
            }
        }

        VRDPPktRelease(&pChannel->pkt);
    }

    return rc;
}

int VRDPInput::InputOnData(InputClient *pClientChannel,
                           const void *pvData, uint32_t cbData)
{
    /* Process a data packet from the client. Wrong packets are ignored. */
    int rc = VINF_SUCCESS;

    VRDPLOGRELIO(("INPUT recv %d:\n%.*Rhxd\n", cbData, RT_MIN(64, cbData), pvData));

    uint32_t u32ClientId = pClientChannel->Client()->Id();

    INPUTCHANNEL *pChannel = inChannelFind(u32ClientId);

    if (pChannel)
    {
        if (cbData >= sizeof(VRDEINPUTHEADER))
        {
            const VRDEINPUTHEADER *pHdr = (const VRDEINPUTHEADER *)pvData;

            INPUTLOG(("IN: u16EventId %d, u32PDULength %d\n",
                      pHdr->u16EventId, pHdr->u32PDULength));

            if (pChannel->enmStatus == INPUT_CHANNEL_NEGOTIATING)
            {
                /* Only a negotiation response is expected. */
                if (pHdr->u16EventId == VRDEINPUT_EVENTID_CS_READY)
                {
                    if (cbData >= sizeof(VRDEINPUT_CS_READY_PDU))
                    {
                        const VRDEINPUT_CS_READY_PDU *pMsg = (const VRDEINPUT_CS_READY_PDU *)pHdr;

                        VRDPLOGREL(("INPUT for %d: flags 0x%RX32, ver 0x%RX32, contacts %d\n",
                                    u32ClientId, pMsg->u32Flags, pMsg->u32ProtocolVersion, pMsg->u16MaxTouchContacts));

                        if (   pMsg->u32ProtocolVersion == VRDEINPUT_PROTOCOL_V1
                            || pMsg->u32ProtocolVersion == VRDEINPUT_PROTOCOL_V101)
                        {
                            /* A good client, the channel is ready. */
                            pChannel->enmStatus = INPUT_CHANNEL_ESTABLISHED;
                            INPUTLOG(("Client accepted\n"));

                            pChannel->u32Flags = pMsg->u32Flags;
                            pChannel->u32ProtocolVersion = pMsg->u32ProtocolVersion;
                            pChannel->u16MaxTouchContacts = pMsg->u16MaxTouchContacts;
                        }
                        else
                        {
                            VRDPLOGREL(("INPUT closing for %d\n", u32ClientId));
                            pClientChannel->CloseChannel();
                            inChannelRemove(pChannel);
                        }
                    }
                    else
                    {
                        INPUTLOG(("Invalid length\n"));
                        rc = VERR_NOT_SUPPORTED;
                    }
                }
                else
                {
                    INPUTLOG(("Invalid status\n"));
                    rc = VERR_NOT_SUPPORTED;
                }
            }
            else if (pChannel->enmStatus == INPUT_CHANNEL_ESTABLISHED)
            {
                switch(pHdr->u16EventId)
                {
                    case VRDEINPUT_EVENTID_TOUCH:
                    {
                        rc = inOnTouch(pClientChannel, pHdr, cbData);
                    } break;

                    case VRDEINPUT_EVENTID_DISMISS_HOVERING_CONTACT:
                    {
                        rc = inOnDismiss(pClientChannel, pHdr, cbData);
                    } break;

                    default:
                    {
                        INPUTLOG(("Invalid function\n"));
                        rc = VERR_NOT_SUPPORTED;
                    } break;
                }
            }
            else
            {
                INPUTLOG(("Invalid channel status\n"));
                rc = VERR_NOT_SUPPORTED;
            }
        }
        else
        {
            INPUTLOG(("Invalid message length\n"));
            rc = VERR_NOT_SUPPORTED;
        }

        VRDPPktRelease(&pChannel->pkt);
    }
    else
    {
        INPUTLOG(("no channel\n"));
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_FAILURE(rc))
    {
        INPUTLOG(("InputOnData: %Rrc\n%.*Rhxd\n",
                  rc, cbData, pvData));
    }

    return rc;
}

/** @thread INPUT */
int VRDPInput::InputOnClose(InputClient *pClientChannel)
{
    /** @todo The dynamic channel has been closed.
     *
     * Make sure that all pending IOs are cancelled
     * and the application is informed about device detach.
     */
    int rc = VINF_SUCCESS;

    INPUTLOG(("\n"));

    uint32_t u32ClientId = pClientChannel->Client()->Id();

    INPUTCHANNEL *pChannel = inChannelFind(u32ClientId);

    if (pChannel)
    {
        inChannelCleanup(pChannel);

        pChannel->enmStatus = INPUT_CHANNEL_VOID;

        /* Remove this channel from channels list. */
        inChannelRemove(pChannel);

        VRDPPktRelease(&pChannel->pkt);
    }
    else
    {
        INPUTLOG(("no channel\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}

#define ENSURE_SRC_SIZE(s) if ((uintptr_t)(s) > (uintptr_t)pu8SrcEnd - (uintptr_t)pu8Src) return VERR_INVALID_PARAMETER
#define ENSURE_DST_SIZE(s) if ((uintptr_t)(s) > (uintptr_t)pu8DstEnd - (uintptr_t)pu8Dst) return VERR_BUFFER_OVERFLOW

static int rdpDecodeTwoByte(uint16_t *pu16Result, const uint8_t **ppu8Src, const uint8_t *pu8SrcEnd, bool fSign)
{
    int rc = VINF_SUCCESS;
    const uint8_t *pu8Src = *ppu8Src;

    ENSURE_SRC_SIZE(1);
    uint8_t c = (*pu8Src >> 7) & 0x1;

    ENSURE_SRC_SIZE(c + 1);

    *pu16Result = *pu8Src++;
    *pu16Result &= 0x7F;

    if (fSign && (*pu16Result & 0x40))
    {
        *pu16Result |= 0x80;
    }

    while (c--)
    {
        *pu16Result <<= 8;
        *pu16Result |= *pu8Src++;
    }

    *ppu8Src = pu8Src;
    return rc;
}

static int rdpDecodeFourByte(uint32_t *pu32Result, const uint8_t **ppu8Src, const uint8_t *pu8SrcEnd, bool fSign)
{
    int rc = VINF_SUCCESS;
    const uint8_t *pu8Src = *ppu8Src;

    ENSURE_SRC_SIZE(1);
    uint8_t c = (*pu8Src >> 6) & 0x3;

    ENSURE_SRC_SIZE(c + 1);

    *pu32Result = *pu8Src++;
    *pu32Result &= 0x3F;

    if (fSign && (*pu32Result & 0x20))
    {
        *pu32Result |= 0xC0;
    }

    while (c--)
    {
        *pu32Result <<= 8;
        *pu32Result |= *pu8Src++;
    }

    *ppu8Src = pu8Src;
    return rc;
}

static int rdpDecodeEightByte(uint64_t *pu64Result, const uint8_t **ppu8Src, const uint8_t *pu8SrcEnd, bool fSign)
{
    int rc = VINF_SUCCESS;
    const uint8_t *pu8Src = *ppu8Src;

    ENSURE_SRC_SIZE(1);
    uint8_t c = (*pu8Src >> 5) & 0x7;

    ENSURE_SRC_SIZE(c + 1);

    *pu64Result = *pu8Src++;
    *pu64Result &= 0x1F;

    if (fSign && (*pu64Result & 0x10))
    {
        *pu64Result |= 0xE0;
    }

    while (c--)
    {
        *pu64Result <<= 8;
        *pu64Result |= *pu8Src++;
    }

    *ppu8Src = pu8Src;
    return rc;
}

#ifdef _MSC_VER
# pragma warning(disable:4366) /* The result of the unary '&' operator may be unaligned */
#endif

static int inTouchDecode(uint8_t *pu8DstStart, uint32_t cbDst, uint32_t *pu32Event,
                         const VRDEINPUTHEADER *pHdr, uint32_t cbMsg)
{
    int rc = VINF_SUCCESS;

    const uint8_t *pu8Src = (const uint8_t *)pHdr;
    const uint8_t *pu8SrcEnd = pu8Src + cbMsg;

    uint8_t *pu8Dst = pu8DstStart;
    uint8_t *pu8DstEnd = pu8Dst + cbDst;

    ENSURE_DST_SIZE(RT_OFFSETOF(VRDEINPUT_TOUCH_EVENT_PDU, aFrames[0]));
    VRDEINPUT_TOUCH_EVENT_PDU *pTouchPDU = (VRDEINPUT_TOUCH_EVENT_PDU *)pu8Dst;

    ENSURE_SRC_SIZE(sizeof(VRDEINPUTHEADER));
    memcpy(pu8Dst, pHdr, sizeof(VRDEINPUTHEADER));
    pu8Src += sizeof(VRDEINPUTHEADER);

    rc = rdpDecodeFourByte(&pTouchPDU->u32EncodeTime, &pu8Src, pu8SrcEnd, false);
    if (RT_FAILURE(rc)) return rc;
    INPUTLOG(("encodeTime 0x%RX32\n", pTouchPDU->u32EncodeTime));

    rc = rdpDecodeTwoByte(&pTouchPDU->u16FrameCount, &pu8Src, pu8SrcEnd, false);
    if (RT_FAILURE(rc)) return rc;
    INPUTLOG(("frameCount %d\n", pTouchPDU->u16FrameCount));

    pu8Dst += RT_OFFSETOF(VRDEINPUT_TOUCH_EVENT_PDU, aFrames[0]);

    uint16_t iFrame;
    for (iFrame = 0; iFrame < pTouchPDU->u16FrameCount; iFrame++)
    {
        ENSURE_DST_SIZE(RT_OFFSETOF(VRDEINPUT_TOUCH_FRAME, aContacts[0]));
        VRDEINPUT_TOUCH_FRAME *pFrame = (VRDEINPUT_TOUCH_FRAME *)pu8Dst;

        rc = rdpDecodeTwoByte(&pFrame->u16ContactCount, &pu8Src, pu8SrcEnd, false);
        if (RT_FAILURE(rc)) return rc;
        INPUTLOG(("contactCount %d\n", pFrame->u16ContactCount));

        rc = rdpDecodeEightByte(&pFrame->u64FrameOffset, &pu8Src, pu8SrcEnd, false);
        if (RT_FAILURE(rc)) return rc;
        INPUTLOG(("frameOffset 0x%RX64\n", pFrame->u64FrameOffset));

        pu8Dst += RT_OFFSETOF(VRDEINPUT_TOUCH_FRAME, aContacts[0]);

        uint16_t iContact;
        for (iContact = 0; iContact < pFrame->u16ContactCount; iContact++)
        {
            ENSURE_DST_SIZE(sizeof(VRDEINPUT_CONTACT_DATA));
            VRDEINPUT_CONTACT_DATA *pContact = (VRDEINPUT_CONTACT_DATA *)pu8Dst;

            ENSURE_SRC_SIZE(1);
            pContact->u8ContactId = *pu8Src++;
            INPUTLOG(("contactId 0x%RX8\n", pContact->u8ContactId));

            rc = rdpDecodeTwoByte(&pContact->u16FieldsPresent, &pu8Src, pu8SrcEnd, false);
            if (RT_FAILURE(rc)) return rc;
            INPUTLOG(("fieldsPresent %RX16\n", pContact->u16FieldsPresent));

            rc = rdpDecodeFourByte((uint32_t *)&pContact->i32X, &pu8Src, pu8SrcEnd, true);
            if (RT_FAILURE(rc)) return rc;
            INPUTLOG(("x %d\n", pContact->i32X));

            rc = rdpDecodeFourByte((uint32_t *)&pContact->i32Y, &pu8Src, pu8SrcEnd, true);
            if (RT_FAILURE(rc)) return rc;
            INPUTLOG(("y %d\n", pContact->i32Y));

            rc = rdpDecodeFourByte(&pContact->u32ContactFlags, &pu8Src, pu8SrcEnd, false);
            if (RT_FAILURE(rc)) return rc;
            INPUTLOG(("contactFlags 0x%RX32\n", pContact->u32ContactFlags));

            if (pContact->u16FieldsPresent & VRDEINPUT_CONTACT_DATA_CONTACTRECT_PRESENT)
            {
                rc = rdpDecodeTwoByte((uint16_t *)&pContact->i16ContactRectLeft, &pu8Src, pu8SrcEnd, true);
                if (RT_FAILURE(rc)) return rc;
                INPUTLOG(("contactRectLeft %d\n", pContact->i16ContactRectLeft));

                rc = rdpDecodeTwoByte((uint16_t *)&pContact->i16ContactRectTop, &pu8Src, pu8SrcEnd, true);
                if (RT_FAILURE(rc)) return rc;
                INPUTLOG(("contactRectTop %d\n", pContact->i16ContactRectTop));

                rc = rdpDecodeTwoByte((uint16_t *)&pContact->i16ContactRectRight, &pu8Src, pu8SrcEnd, true);
                if (RT_FAILURE(rc)) return rc;
                INPUTLOG(("contactRectRight %d\n", pContact->i16ContactRectRight));

                rc = rdpDecodeTwoByte((uint16_t *)&pContact->i16ContactRectBottom, &pu8Src, pu8SrcEnd, true);
                if (RT_FAILURE(rc)) return rc;
                INPUTLOG(("contactRectBottom %d\n", pContact->i16ContactRectBottom));
            }
            else
            {
                pContact->i16ContactRectLeft = 0;
                pContact->i16ContactRectTop = 0;
                pContact->i16ContactRectRight = 0;
                pContact->i16ContactRectBottom = 0;
            }

            if (pContact->u16FieldsPresent & VRDEINPUT_CONTACT_DATA_ORIENTATION_PRESENT)
            {
                rc = rdpDecodeFourByte(&pContact->u32Orientation, &pu8Src, pu8SrcEnd, false);
                if (RT_FAILURE(rc)) return rc;
                INPUTLOG(("orientation %d\n", pContact->u32Orientation));
            }
            else
            {
                pContact->u32Orientation = 0;
            }

            if (pContact->u16FieldsPresent & VRDEINPUT_CONTACT_DATA_PRESSURE_PRESENT)
            {
                rc = rdpDecodeFourByte(&pContact->u32Pressure, &pu8Src, pu8SrcEnd, false);
                if (RT_FAILURE(rc)) return rc;
                INPUTLOG(("pressure %d\n", pContact->u32Pressure));
            }
            else
            {
                pContact->u32Pressure = 0;
            }

            pu8Dst += sizeof(VRDEINPUT_CONTACT_DATA);
        }
    }

    *pu32Event = (uint32_t)(pu8Dst - pu8DstStart);

    return rc;
}

#ifdef _MSC_VER
# pragma warning(default:4366)
#endif

#undef ENSURE_DST_SIZE
#undef ENSURE_SRC_SIZE

int VRDPInput::inOnTouch(InputClient *pClientChannel, const VRDEINPUTHEADER *pHdr, uint32_t cbMsg)
{
    RT_NOREF1(pClientChannel);
    int rc = VINF_SUCCESS;

    /* Convert RDP struct. */
    uint32_t cbAlloc = 4096;
    VRDEINPUT_TOUCH_EVENT_PDU *pEvent = (VRDEINPUT_TOUCH_EVENT_PDU *)VRDPMemAlloc(cbAlloc);
    if (!pEvent)
    {
        return VERR_NO_MEMORY;
    }

    uint32_t cbEvent = 0;
    rc = inTouchDecode((uint8_t *)pEvent, cbAlloc, &cbEvent, pHdr, RT_MIN(cbMsg, pHdr->u32PDULength));

    if (RT_SUCCESS(rc))
    {
        rc = inCallbackEvent(pEvent, cbEvent);
    }

    VRDPMemFree(pEvent);

    return rc;
}

int VRDPInput::inOnDismiss(InputClient *pClientChannel, const VRDEINPUTHEADER *pHdr, uint32_t cbMsg)
{
    RT_NOREF3(pClientChannel, pHdr, cbMsg);
    int rc = VINF_SUCCESS;
    return rc;
}


/*
 * DVC channel proxy, which forwards the data to the server.
 * There can be only one channel per client.
 * So it is ok to use only the client id to distinguish
 * channel instances.
 *
 * @todo make this generic, it should be used by server objects to talk to a channel instance (TSMF, VideoIn, etc).
 */

InputClient::InputClient(VRDPClient *pClient)
    :
    m_pClient (pClient),
    m_u8DVCChannelId (0)
{
}

InputClient::~InputClient()
{
    if (m_u8DVCChannelId)
    {
        m_pClient->Server()->Input()->InputOnClose(this);
    }
}

int InputClient::RegisterChannel(void)
{
    int rc = m_pClient->TP()->DVC()->RegisterChannel("Microsoft::Windows::RDS::Input",
                                                      dvcInput,
                                                      this,
                                                      &m_u8DVCChannelId);

    INPUTLOG(("RegisterChannel %Rrc\n", rc));

    return rc;
}

/** @thread OUTPUT */
/* static */ DECLCALLBACK(void) InputClient::processCloseChannel(VRDPClient *pClient,
                                                                 const VRDPOUTPUTGENERICHDR *pHdr)
{
    InputClient *pThis = (InputClient *)pHdr->pvUser;

    pClient->TP()->DVC()->SendCloseChannel(pThis->m_u8DVCChannelId);
}

void InputClient::CloseChannel(void)
{
    VRDPOUTPUTGENERICHDR pkt;

    pkt.pfnCallback = processCloseChannel;
    pkt.pvUser      = this;
    pkt.u32Size     = sizeof(pkt);
    pkt.u32Parm     = 0;

    m_pClient->Server()->PostOutput(VRDP_OUTPUT_GENERIC, m_pClient->Id(), &pkt, sizeof(pkt));
    m_pClient->Server()->RaiseOutputEvent();
}

typedef struct VRDPOUTPUTSENDDATA
{
    VRDPOUTPUTGENERICHDR hdr;
    void *pvData;
    uint32_t cbData;
} VRDPOUTPUTSENDDATA;

/** @thread OUTPUT */
/* static */ DECLCALLBACK(void) InputClient::processOutput(VRDPClient *pClient,
                                                           const VRDPOUTPUTGENERICHDR *pHdr)
{
    InputClient *pThis = (InputClient *)pHdr->pvUser;

    VRDPOUTPUTSENDDATA *pPkt = (VRDPOUTPUTSENDDATA *)pHdr;

    pClient->TP()->DVC()->SendData(pThis->m_u8DVCChannelId, pPkt->pvData, pPkt->cbData);

    RTMemFree(pPkt->pvData);
}

int InputClient::SendData(const void *pvData, uint32_t cbData)
{
    /** @todo VRDPLOGRELIO */ INPUTLOG(("INPUT send %d:\n%.*Rhxd\n", cbData, RT_MIN(64, cbData), pvData));

    if (m_pClient->Server()->IsOutputThread())
    {
        return m_pClient->TP()->DVC()->SendData(m_u8DVCChannelId, (void *)pvData, cbData);
    }

    /* Forward to the output thread. */
    VRDPOUTPUTSENDDATA pkt;

    pkt.hdr.pfnCallback = processOutput;
    pkt.hdr.pvUser      = this;
    pkt.hdr.u32Size     = sizeof(pkt);
    pkt.hdr.u32Parm     = 0;
    pkt.pvData = RTMemDup(pvData, cbData);
    if (!pkt.pvData)
    {
        return VERR_NO_MEMORY;
    }
    pkt.cbData = cbData;

    return m_pClient->Server()->PostOutput(VRDP_OUTPUT_GENERIC, m_pClient->Id(), &pkt, sizeof(pkt));
}


/** @thread INPUT */
/* static */ void InputClient::dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    InputClient *pThis = (InputClient *)pvCtx;

    pThis->processInput(u32Event, pvData, cbData);
}

/** @thread INPUT */
void InputClient::processInput(uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    INPUTLOG(("this %p, u32Event %d, pvData %p, cbData %d\n",
              this, u32Event, pvData, cbData));

    int rc = VINF_SUCCESS;

    switch (u32Event)
    {
        case DVC_EVENT_CREATE:
        {
            VRDPLOGREL(("Input[%d] enabled: %d\n",
                        m_pClient->Id(), m_u8DVCChannelId));

            /* Client accepted the channel. */
            m_pClient->Server()->Input()->InputOnCreate(this);
        } break;

        case DVC_EVENT_DATA:
        {
            m_pClient->Server()->Input()->InputOnData(this, pvData, cbData);
        } break;

        case DVC_EVENT_CLOSE:
        {
            VRDPLOGREL(("Input[%d] closed: %d\n", m_pClient->Id(), m_u8DVCChannelId));

            m_u8DVCChannelId = 0;

            m_pClient->Server()->Input()->InputOnClose(this);
        } break;

        default:
        {
            AssertFailed();
        }
    }

    if (RT_FAILURE(rc))
    {
        VRDPLOGRELLIMIT(32, ("VRDP: INPUT: input %Rrc!!!\n", rc));
    }
}
