/* $Id: tsmf.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2012-2025 Oracle and/or its affiliates.
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

#include "videotsmf.h"
#include "vrdpserv.h"

#include <math.h>

#include "tsmf.h"
#include "vrdpclient.h"

#define VRDP_DEBUG_TSMF

#ifdef VRDP_DEBUG_TSMF
#  define TSMFLOG(a) do { VRDPLOG(("TSMF::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define TSMFLOG(a)
#endif /* VRDP_DEBUG_UTCINFO */


/* A channel status. */
#define TSMFRAW_CHANNEL_UNUSED      0 /* Dynamic channel is does not exist. */
#define TSMFRAW_CHANNEL_REGISTERING 1 /* Dynamic channel is being registered. */
#define TSMFRAW_CHANNEL_OK          3 /* Successfully exchanged RIM caps. */
#define TSMFRAW_CHANNEL_CLOSING     4 /* Channel is being closed. */
#define TSMFRAW_CHANNEL_INVALID     5 /* Unrecoverable error. */


/*
 * External wrappers for the VRDPServer TSMF API.
 */
static DECLCALLBACK(int) VRDETSMFChannelCreate(HVRDESERVER hServer,
                                               void *pvChannel,
                                               uint32_t u32Flags)
{
    int rc = VINF_SUCCESS;

    VRDPServer *pServer = (VRDPServer *)hServer;

    VRDPAPILOG(("%p\n", pServer));

    if (pServer)
    {
        rc = pServer->TSMF()->TSMFChannelCreate(pvChannel, u32Flags);
    }

    return rc;
}

static DECLCALLBACK(int) VRDETSMFChannelClose(HVRDESERVER hServer,
                                              uint32_t u32ChannelHandle)
{
    int rc = VINF_SUCCESS;

    VRDPServer *pServer = (VRDPServer *)hServer;

    VRDPAPILOG(("%p\n", pServer));

    if (pServer)
    {
        rc = pServer->TSMF()->TSMFChannelClose(u32ChannelHandle);
    }

    return rc;
}

static DECLCALLBACK(int) VRDETSMFChannelSend(HVRDESERVER hServer,
                                             uint32_t u32ChannelHandle,
                                             const void *pvData,
                                             uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    VRDPServer *pServer = (VRDPServer *)hServer;

    VRDPAPILOG(("%p\n", pServer));

    if (pServer)
    {
        rc = pServer->TSMF()->TSMFChannelSend(u32ChannelHandle, pvData, cbData);
    }

    return rc;
}

static VRDETSMFINTERFACE gTSMFInterface =
{
    { 1, sizeof (VRDETSMFINTERFACE) },

    VRDETSMFChannelCreate,
    VRDETSMFChannelClose,
    VRDETSMFChannelSend
};


/*
 * The VRDP server TSMF implementation.
 */

/*
 * Note: multiple clients are not supported by TSMFRaw.
 *       The channel is always created only for one client.
 */

/*
 * A TSMF channel context.
 */
typedef struct TSMFRAWCHANNELCTX
{
    RTLISTNODE nodeChannelCtx;   /* List of all contexts. */

    void *pvChannel;             /* The user context of this channel. */
    uint32_t u32Flags;           /* Channel creation flags. */

    uint32_t u32ChannelHandle;   /* Assigned by the VRDPTSMF for API users. */

    uint32_t u32ClientId;        /* Which client is associated with this ctx. */

    uint32_t u32ChannelId;       /* Assigned by the TSMFRaw. */
} TSMFRAWCHANNELCTX;

/*
 * Output functions.
 */
#define TSMFRAW_FN_CREATE 1
#define TSMFRAW_FN_SEND   2
#define TSMFRAW_FN_CLOSE  3

typedef struct TSMFRAWOUTHDR
{
    uint32_t u32Function;
    uint32_t u32Size;
    TSMFRAWCHANNELCTX *pContext;
} TSMFRAWOUTHDR;

typedef struct TSMFRAWOUTCREATE
{
    TSMFRAWOUTHDR hdr;
} TSMFRAWOUTCREATE;

typedef struct TSMFRAWOUTSEND
{
    TSMFRAWOUTHDR hdr;
    uint32_t u32ChannelId;
    uint32_t u32DataOffset;
    uint32_t u32DataLength;
    /* data follow */
} TSMFRAWOUTSEND;

typedef struct TSMFRAWOUTCLOSE
{
    TSMFRAWOUTHDR hdr;
    uint32_t u32ChannelId;
} TSMFRAWOUTCLOSE;


VRDPTSMF::VRDPTSMF(VRDPServer *pServer)
    :
    m_pServer (pServer),
    m_fShutdown (false),
    m_u32HandleSrc (0),
    m_u32ClientId (0)
{
    RT_ZERO(m_interface);
    RTListInit(&m_ListChannels);
    VRDPLock::Create("TSMF", &m_pLock);
}

/* virtual */ VRDPTSMF::~VRDPTSMF()
{
    VRDPLock::Delete(&m_pLock);
}

/* Helper to get the interface method table, called from VRDPServer::GetInterface */
int VRDPTSMF::GetInterfaceTSMF(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks, void *pvContext)
{
    int rc = VINF_SUCCESS;

    if (   pInterface->u64Version == 1
        && pInterface->u64Size == sizeof (VRDETSMFINTERFACE)
        && pCallbacks->u64Version == 1
        && pCallbacks->u64Size == sizeof (VRDETSMFCALLBACKS))
    {
        /* Interface version 1 is requested. */
        VRDETSMFINTERFACE *p = (VRDETSMFINTERFACE *)pInterface;

        m_interface.callbacks = *(VRDETSMFCALLBACKS *)pCallbacks;
        m_interface.pvContext = pvContext;

        *p = gTSMFInterface;
    }
    else
    {
        rc = VERR_VERSION_MISMATCH;
    }

    return rc;
}

int VRDPTSMF::TSMFInitialize(void)
{
    int rc = VINF_SUCCESS;
    return rc;
}

void VRDPTSMF::TSMFShutdown(void)
{
    TSMFRAWCHANNELCTX *pIter;
    TSMFRAWCHANNELCTX *pNext;
    RTListForEachSafe(&m_ListChannels, pIter, pNext, TSMFRAWCHANNELCTX, nodeChannelCtx)
    {
        TSMFLOG(("TSMFShutdown: remove %p\n",
                  pIter));

        tsmfCbNotify(VRDE_TSMF_N_DISCONNECTED,
                     pIter->pvChannel,
                     NULL,
                     0);

        tsmfChannelCtxDelete(pIter);
    }
}

int VRDPTSMF::TSMFChannelCreate(void *pvChannel,
                                uint32_t u32Flags)
{
    if (m_u32ClientId == 0)
    {
        return VERR_NOT_AVAILABLE;
    }

    /* Create a channel context. */
    TSMFRAWCHANNELCTX *pContext;
    int rc = tsmfChannelCtxCreate(&pContext, m_u32ClientId, pvChannel, u32Flags);
    if (RT_SUCCESS(rc))
    {
        /* Send to the client which announced the DVC capability.
         */
        TSMFRAWOUTCREATE pkt;

        pkt.hdr.u32Function = TSMFRAW_FN_CREATE;
        pkt.hdr.u32Size     = sizeof(pkt);
        pkt.hdr.pContext    = pContext;

        rc = m_pServer->PostOutput(VRDP_OUTPUT_TSMFRAW, m_u32ClientId, &pkt, sizeof(pkt));
        if (RT_FAILURE(rc))
            tsmfChannelCtxDelete(pContext);
    }

    return rc;
}

int VRDPTSMF::TSMFChannelClose(uint32_t u32ChannelHandle)
{
    TSMFRAWCHANNELCTX *pContext;
    int rc = tsmfChannelCtxFind(&pContext, u32ChannelHandle);

    if (RT_SUCCESS(rc))
    {
        TSMFRAWOUTCLOSE pkt;

        pkt.hdr.u32Function = TSMFRAW_FN_CLOSE;
        pkt.hdr.u32Size     = sizeof(pkt);
        pkt.hdr.pContext    = pContext;

        pkt.u32ChannelId = pContext->u32ChannelId;

        rc = m_pServer->PostOutput(VRDP_OUTPUT_TSMFRAW, pContext->u32ClientId, &pkt, sizeof(pkt));
    }

    return rc;
}

int VRDPTSMF::TSMFChannelSend(uint32_t u32ChannelHandle, const void *pvData, uint32_t cbData)
{
    TSMFRAWCHANNELCTX *pContext;
    int rc = tsmfChannelCtxFind(&pContext, u32ChannelHandle);

    if (RT_SUCCESS(rc))
    {
        uint32_t cbAlloc = sizeof(TSMFRAWOUTSEND) + cbData;
        TSMFRAWOUTSEND *pPkt = (TSMFRAWOUTSEND *)VRDPMemAlloc(cbAlloc);

        if (pPkt != NULL)
        {
            pPkt->hdr.u32Function = TSMFRAW_FN_SEND;
            pPkt->hdr.u32Size     = cbAlloc;
            pPkt->hdr.pContext    = pContext;

            pPkt->u32ChannelId  = pContext->u32ChannelId;
            pPkt->u32DataOffset = sizeof(TSMFRAWOUTSEND);
            pPkt->u32DataLength = cbData;

            memcpy(&pPkt[1], pvData, cbData);

            rc = m_pServer->PostOutput(VRDP_OUTPUT_TSMFRAW, pContext->u32ClientId, pPkt, cbAlloc);

            VRDPMemFree(pPkt);
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

void VRDPTSMF::TSMFClientConnectedDVC(uint32_t u32ClientId)
{
   if (m_u32ClientId == 0)
   {
       m_u32ClientId = u32ClientId;
   }
}

void VRDPTSMF::TSMFClientDisconnectedDVC(uint32_t u32ClientId)
{
   if (m_u32ClientId == u32ClientId)
   {
       m_u32ClientId = 0;
       /** @todo delete channels associated with this client. */
   }
}

int VRDPTSMF::TSMFOnCreate(uint32_t u32ClientId,
                           TSMFRAWCHANNELCTX *pChannel,
                           uint32_t u32ChannelId,
                           int rcCreate)
{
    if (u32ClientId != m_u32ClientId)
    {
        return VERR_INVALID_PARAMETER;
    }

    if (RT_SUCCESS(rcCreate))
    {
        pChannel->u32ChannelId = u32ChannelId;

        VRDETSMFNOTIFYCREATEACCEPTED parm;
        parm.u32ChannelHandle = pChannel->u32ChannelHandle;

        tsmfCbNotify(VRDE_TSMF_N_CREATE_ACCEPTED,
                     pChannel->pvChannel,
                     &parm,
                     sizeof(parm));
    }
    else
    {
        tsmfCbNotify(VRDE_TSMF_N_CREATE_DECLINED,
                     pChannel->pvChannel,
                     NULL,
                     0);

        tsmfChannelCtxDelete(pChannel);
    }

    return VINF_SUCCESS;
}

int VRDPTSMF::TSMFOnData(uint32_t u32ClientId,
                         TSMFRAWCHANNELCTX *pChannel,
                         const void *pvData,
                         uint32_t cbData)
{
    RT_NOREF1(u32ClientId);
    VRDETSMFNOTIFYDATA parm;
    parm.pvData = pvData;
    parm.cbData = cbData;

    tsmfCbNotify(VRDE_TSMF_N_DATA,
                 pChannel->pvChannel,
                 &parm,
                 sizeof(parm));

    return VINF_SUCCESS;
}

int VRDPTSMF::TSMFOnClose(uint32_t u32ClientId,
                          TSMFRAWCHANNELCTX *pChannel)
{
    RT_NOREF1(u32ClientId);
    tsmfCbNotify(VRDE_TSMF_N_DISCONNECTED,
                 pChannel->pvChannel,
                 NULL,
                 0);

    tsmfChannelCtxDelete(pChannel);

    return VINF_SUCCESS;
}

/* Interface callbacks. */
void VRDPTSMF::tsmfCbNotify(uint32_t u32Notification,
                            void *pvChannel,
                            const void *pvParm,
                            uint32_t cbParm)
{
    if (m_interface.callbacks.VRDETSMFCbNotify)
    {
         m_interface.callbacks.VRDETSMFCbNotify(m_interface.pvContext, u32Notification,
                                                pvChannel, pvParm, cbParm);
    }
}

int VRDPTSMF::tsmfChannelCtxCreate(TSMFRAWCHANNELCTX **ppContext,
                                   uint32_t u32ClientId,
                                   void *pvChannel,
                                   uint32_t u32Flags)
{
    TSMFRAWCHANNELCTX *pContext = (TSMFRAWCHANNELCTX *)VRDPMemAllocZ(sizeof(TSMFRAWCHANNELCTX));
    if (RT_UNLIKELY(!pContext))
        return VERR_NO_MEMORY;

    pContext->pvChannel    = pvChannel;
    pContext->u32Flags     = u32Flags;
    pContext->u32ClientId  = u32ClientId;
    pContext->u32ChannelId = 0; /* No channel id yet. */
    pContext->u32ChannelHandle = ASMAtomicIncU32(&m_u32HandleSrc);
    if (pContext->u32ChannelHandle == 0)
        pContext->u32ChannelHandle = ASMAtomicIncU32(&m_u32HandleSrc);

    VRDPLock::Lock(m_pLock);
    RTListAppend(&m_ListChannels, &pContext->nodeChannelCtx);
    VRDPLock::Unlock(m_pLock);

    *ppContext = pContext;
    return VINF_SUCCESS;
}

int VRDPTSMF::tsmfChannelCtxFind(TSMFRAWCHANNELCTX **ppContext,
                                 uint32_t u32ChannelHandle)
{
    int rc = VINF_SUCCESS;

    TSMFRAWCHANNELCTX *pContext = NULL;

    VRDPLock::Lock(m_pLock);

    TSMFRAWCHANNELCTX *pIter;
    RTListForEach(&m_ListChannels, pIter, TSMFRAWCHANNELCTX, nodeChannelCtx)
    {
        if (pIter->u32ChannelHandle == u32ChannelHandle)
        {
            pContext = pIter;
            break;
        }
    }

    VRDPLock::Unlock(m_pLock);

    if (pContext)
    {
        *ppContext = pContext;
    }
    else
    {
        rc = VERR_NOT_FOUND;
    }

    return rc;
}

void VRDPTSMF::tsmfChannelCtxDelete(TSMFRAWCHANNELCTX *pContext)
{
    /** @todo check that the pContext is still in the list. */
    /** @todo r=aeichner Shouldn't the list removal be protected by VRDPLock::Unlock(m_pLock) and unlock? */
    RTListNodeRemove(&pContext->nodeChannelCtx);
    VRDPMemFree(pContext);
}


TSMFRaw::TSMFRaw (VRDPClient *pClient)
    :
    m_pClient (pClient)
{
    RT_ZERO(m_aChannels);
}

TSMFRaw::~TSMFRaw ()
{
    destroy();
}

void TSMFRaw::destroy(void)
{
}

TSMFRAWCHANNEL *TSMFRaw::findChannel(uint32_t u32ChannelId)
{
    TSMFRAWCHANNEL *pChannel = NULL;

    uint32_t i;
    for (i = 0; i < RT_ELEMENTS(m_aChannels); i++)
    {
        if (   ASMAtomicReadU32(&m_aChannels[i].u32Status) != TSMFRAW_CHANNEL_UNUSED
            && m_aChannels[i].u32ChannelId == u32ChannelId)
        {
            pChannel = &m_aChannels[i];

            break;
        }
    }

    return pChannel;
}

/** @thread INPUT */
/* static */ void TSMFRaw::dvcInput(void *pvCtx, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    TSMFRAWCHANNEL *pInstance = (TSMFRAWCHANNEL *)pvCtx;

    pInstance->pTSMFRaw->processInput(pInstance, u32Event, pvData, cbData);
}

/** @thread INPUT */
void TSMFRaw::processInput(TSMFRAWCHANNEL *pInstance, uint32_t u32Event, const void *pvData, uint32_t cbData)
{
    TSMFLOG(("pInstance %p, u32Event %d, pvData %p, cbData %d\n",
             pInstance, u32Event, pvData, cbData));

    int rc = VINF_SUCCESS;

    switch (u32Event)
    {
        case DVC_EVENT_CREATE:
        {
            VRDPLOGREL(("TSMFRaw[%d] enabled: %d\n",
                        pInstance->u32Index, pInstance->u8DVCChannelId));

            /* Client accepted the channel. */
            Assert(pInstance->u32Status == TSMFRAW_CHANNEL_REGISTERING);
            ASMAtomicWriteU32(&pInstance->u32Status, TSMFRAW_CHANNEL_OK);

            m_pClient->Server()->TSMF()->TSMFOnCreate(m_pClient->Id(), (TSMFRAWCHANNELCTX *)pInstance->pvContext,
                                                      pInstance->u32ChannelId, VINF_SUCCESS);
        } break;

        case DVC_EVENT_DATA:
        {
            m_pClient->Server()->TSMF()->TSMFOnData(m_pClient->Id(), (TSMFRAWCHANNELCTX *)pInstance->pvContext,
                                                    pvData, cbData);
        } break;

        case DVC_EVENT_CLOSE:
        {
            VRDPLOGREL(("TSMFRaw dvc id %d close\n", pInstance->u8DVCChannelId));

            m_pClient->Server()->TSMF()->TSMFOnClose(m_pClient->Id(), (TSMFRAWCHANNELCTX *)pInstance->pvContext);

            ASMAtomicWriteU32(&pInstance->u32Status, TSMFRAW_CHANNEL_UNUSED);
        } break;

        default:
            AssertFailed();
    }

    if (RT_FAILURE(rc))
    {
        VRDPLOGRELLIMIT(32, ("VRDP: TSMFRaw: input %Rrc!!!\n", rc));
    }
}

/* Allocate the handle for this channel instance.
 * @thread OUTPUT
 */
int TSMFRaw::registerChannel(uint32_t *pu32ChannelId, void *pvContext)
{
    TSMFLOG(("TSMFRaw registerChannel\n"));

    int rc = VINF_SUCCESS;

    TSMFRAWCHANNEL *pChannel = NULL;

    uint32_t i;
    for (i = 0; i < RT_ELEMENTS(m_aChannels); i++)
    {
        bool fAllocated = ASMAtomicCmpXchgU32(&m_aChannels[i].u32Status,
                                              TSMFRAW_CHANNEL_REGISTERING, TSMFRAW_CHANNEL_UNUSED);
        if (fAllocated)
        {
            pChannel = &m_aChannels[i];

            pChannel->pTSMFRaw = this;
            pChannel->u32Index = i;
            pChannel->u32ChannelId = i + 1;
            pChannel->u8DVCChannelId = 0;
            pChannel->pvContext = pvContext;

            break;
        }
    }

    if (pChannel)
    {
        rc = m_pClient->TP()->DVC()->RegisterChannel("TSMF",
                                                     dvcInput,
                                                     pChannel,
                                                     &pChannel->u8DVCChannelId);
        if (RT_SUCCESS(rc))
        {
            *pu32ChannelId = pChannel->u32ChannelId;
        }
        else
        {
            ASMAtomicWriteU32(&pChannel->u32Status, TSMFRAW_CHANNEL_UNUSED);
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    return rc;
}

/* Deallocate the channel instance.
 * @thread OUTPUT
 */
int TSMFRaw::closeChannel(uint32_t u32ChannelId)
{
    TSMFRAWCHANNEL *pChannel = findChannel(u32ChannelId);

    if (pChannel)
    {
        ASMAtomicWriteU32(&pChannel->u32Status, TSMFRAW_CHANNEL_CLOSING);

        m_pClient->TP()->DVC()->SendCloseChannel(pChannel->u8DVCChannelId);
    }

    return VINF_SUCCESS;
}

/** @thread OUTPUT */
void TSMFRaw::TSMFRawOutput(void *pvData, uint32_t cbData)
{
    TSMFLOG(("TSMFRawOutput %p, %d\n", pvData, cbData));

    if (cbData < sizeof(TSMFRAWOUTHDR))
    {
        AssertFailed();
        return;
    }

    int rc = VINF_SUCCESS;

    TSMFRAWOUTHDR *pHdr = (TSMFRAWOUTHDR *)pvData;

    switch (pHdr->u32Function)
    {
        case TSMFRAW_FN_CREATE:
        {
            TSMFRAWOUTCREATE *p = (TSMFRAWOUTCREATE *)pHdr;

            /* Send a channel create request to the client. */
            uint32_t u32ChannelId = 0;
            rc = registerChannel(&u32ChannelId, p->hdr.pContext);

            TSMFLOG(("TSMFRAW_FN_CREATE: %Rrc h 0x%08X\n", rc, u32ChannelId));

            /* If there is no error then the reply to the guest will be sent later
             * when the channel is created.
             */
            if (RT_FAILURE(rc))
            {
                /* Report about the channel creation failure. */
                m_pClient->Server()->TSMF()->TSMFOnCreate(m_pClient->Id(), p->hdr.pContext, u32ChannelId, rc);
            }
        } break;

        case TSMFRAW_FN_SEND:
        {
            TSMFRAWOUTSEND *p = (TSMFRAWOUTSEND *)pHdr;

            TSMFRAWCHANNEL *pChannel = findChannel(p->u32ChannelId);

            if (pChannel)
            {
                m_pClient->TP()->DVC()->SendData(pChannel->u8DVCChannelId,
                                                 (uint8_t *)p + p->u32DataOffset,
                                                 p->u32DataLength);
            }
        } break;

        case TSMFRAW_FN_CLOSE:
        {
            TSMFRAWOUTCLOSE *p = (TSMFRAWOUTCLOSE *)pHdr;

            rc = closeChannel(p->u32ChannelId);

            TSMFLOG(("TSMFRAW_FN_CHANNEL_CLOSE: %Rrc h 0x%08X\n", rc, p->u32ChannelId));
        } break;

        default:
        {
            AssertFailed();
        } break;
    }
}
