/* $Id: input.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2004-2025 Oracle and/or its affiliates.
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
#include "tcp.h"
#include "utils.h"

/*
 * The VRDP server methods which are executed on INPUT thread.
 * And the client/transportid associative array.
 */

/* INPUT thread that informs the server that the thread has started
 * and then calls the transport to listen for incoming connections
 * and receiving incoming data.
 */
int VRDPServer::InputThread (RTTHREAD self, VRDPServerThreadStartCtx *pCtx)
{
    int rc = VINF_SUCCESS;

    /* Set it immediately so any called code can check the context. */
    m_inputThread = self;

    /* The transport is already initialized. Open it. */
    rc = m_pTransport->Open();

    if (RT_SUCCESS(rc))
    {
        /* Set this before notification to the server. */
        m_u32BindPort = m_pTransport->GetBindPort();
    }

    notifyThreadStarted(self, pCtx, rc);

    if (RT_FAILURE(rc))
    {
        m_inputThread = NIL_RTTHREAD;
        return rc;
    }

    SERVERLOG(("Input thread goes to listen: m_fShutdownThreads = %d\n", m_fShutdownThreads));

    while (!m_fShutdownThreads)
    {
        /* Prevent clients connections until they are enabled. */
        if (!m_fConnectionsEnabled)
            rc = RTSemEventWaitNoResume (m_hEvtInput, RT_MS_15SEC);
        else
        {
            /* New connections and available data will be reported via VRDPTransportServer callbacks. */
            rc = m_pTransport->Listen ();
            SERVERLOG(("InputThread:listen: rc = %Rrc\n", rc));
        }

        if (!m_fShutdownThreads && RT_FAILURE (rc) && rc != VERR_TIMEOUT && rc != VERR_INTERRUPTED)
        {
            rc = RTSemEventWaitNoResume (m_hEvtInput, 50);
            if (RT_FAILURE (rc) && rc != VERR_TIMEOUT && rc != VERR_INTERRUPTED && !m_fShutdownThreads)
                VRDPThreadSleep (50);
        }
    }

    m_pTransport->Close();

    SERVERLOG(("Input thread finished\n"));

    m_inputThread = NIL_RTTHREAD;

    return rc;
}

/*
 * VRDPTransportServer methods begin.
 */
uint64_t VRDPServer::GetKeepAliveTimeout(void)
{
    /** @todo Make configurable. */
    return 30 * 1000; /* Millisec. */
}

int VRDPServer::TransportConnect (VRDPTRANSPORTID transportId)
{
    /* That is a notification from transport that a new client has connected.
     * The method instantiates a new client and start connection establishing.
     */
    VRDP_ASSERT_CONTEXT_INPUT(this);

    VRDPLOGREL(("New connection: %s\n", m_pTransport->GetInfo (transportId)));

    SERVERLOG(("transportId = %d\n", transportId));

    int rc = VINF_SUCCESS;

    /* Allocate the client. */
    VRDPClient *pClient = new VRDPClient (this, clientGenId ());

    SERVERLOG(("allocated client %p\n", pClient));

    if (pClient)
    {
        /* Insert the client into an associative array that resolves
         * transport Ids to client instances.
         */
        rc = m_clientArray.Insert (pClient, transportId);

        if (RT_SUCCESS (rc))
        {
            /* Assign the transport id to the client. */
            rc = pClient->Start(m_pTransport, transportId);
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE (rc))
    {
        m_clientArray.Remove (pClient);
        delete pClient;
    }

    SERVERLOG(("rc = %Rrc\n", rc));
    return rc;
}

void VRDPServer::TransportDisconnect(VRDPTRANSPORTID transportId)
{
    VRDP_ASSERT_CONTEXT_INPUT(this);

    /* Notification from the transport that the client was disconnected.
     * That means that the client can be deleted.
     * The transportId is invalid already.
     */

    VRDPClient *pClient = m_clientArray.LookupTransportId (transportId);

    if (pClient == NULL)
    {
        AssertMsgFailed(("failed to find client instance for transport id %d\n", transportId));
        return;
    }

    /* Remove the client from the array, so it is not used anymore. */
    m_clientArray.Remove (pClient);

    /* The method ensures that the client is not in use by VM and OUTPUT threads anymore.
     * and calls the ClientDisconnect VRDPDesktop callback.
     */
    int rc = pClient->NotifyDelete();

    if (RT_SUCCESS(rc))
    {
        delete pClient;
    }
    else
    {
        SERVERLOG(("Failed to cleanly disconnect client %p\n", pClient));
        /** @todo Use reference counting for clients. */
    }

    SERVERLOG(("deleted client %p\n", pClient));
}

int VRDPServer::TransportEvent (VRDPTRANSPORTID transportId, uint32_t fu32Events)
{
    VRDP_ASSERT_CONTEXT_INPUT(this);

    VRDPClient *pClient = m_clientArray.LookupTransportId (transportId);

    if (!pClient)
    {
        AssertMsgFailed (("no instance\n"));
        return VERR_NOT_SUPPORTED;
    }

#ifdef DEBUG
    if (fu32Events)
    {
        SERVERLOG(("Events 0x%08X for transportId %d, client %p, client id %d\n",
                   fu32Events, transportId, pClient, pClient->Id ()));
    }
#endif /* DEBUG */

    /* Fetch ISO packets from the transport. */
    VRDPInputCtx *pInputCtx = pClient->InputCtx ();

    int rc = VINF_VRDP_PROCESS_PDU;

    if (fu32Events & VRDP_TRANSPORT_KEEP_ALIVE)
    {
        PostOutput (VRDP_OUTPUT_KEEP_ALIVE, pClient->Id (), NULL, 0);
    }

    if (fu32Events & VRDP_TRANSPORT_DATA_AVAILABLE)
    {
        rc = pInputCtx->Recv ();
        SERVERLOG(("pInputCtx->Recv rc %Rrc\n", rc));
    }

    if (rc == VINF_VRDP_PROCESS_PDU)
    {
        rc = pClient->ProcessIncoming (fu32Events);

        if (fu32Events & VRDP_TRANSPORT_DATA_AVAILABLE)
        {
            /* Reset the input context only if a packet was processed,
             * that is the pInputCtx->Recv () above returned the
             * VINF_VRDP_PROCESS_PDU status.
             */
            pInputCtx->Reset ();
        }
    }

    return rc;
}
/*
 * VRDPTransportServer methods end.
 */


uint32_t VRDPServer::clientGenId (void)
{
    VRDP_ASSERT_CONTEXT_INPUT(this);

    u32ClientIdSrc++;

    if (u32ClientIdSrc == 0)
    {
        u32ClientIdSrc++;
    }

    return u32ClientIdSrc;
}


/*
 * The VRDPClientArray associative array.
 * Insert/Remove performance is not important.
 * Lookups must be fast.
 */
VRDPClientArray::VRDPClientArray()
    :
    m_cElements (0),
    m_cAllocated (0),
    m_paTableTransportId (NULL),
    m_paTableClientId (NULL),
    m_cTotalClients (0),
    m_i64TimeLastConnect (0),
    m_i64TimeLastDisconnect (0),
    m_pLastClient (NULL)
{
    int rc = VRDPLock::Create("ClientArray", &m_pClientArrayLock);
    AssertRC(rc);
}

VRDPClientArray::~VRDPClientArray()
{
    VRDPMemFree (m_paTableTransportId);
    VRDPMemFree (m_paTableClientId);

    VRDPLock::Delete(&m_pClientArrayLock);
}

int VRDPClientArray::Insert (VRDPClient *pClient, VRDPTRANSPORTID transportId)
{
    int rc = VERR_ACCESS_DENIED;

    if (lock ())
    {
        if (   !pClient->StatusVM()->ClientUseStateCheckDisabled ()
            || !pClient->StatusOutput()->ClientUseStateCheckDisabled ())
        {
            AssertFailed ();
            unlock ();
            return VERR_ACCESS_DENIED;
        }

        if (m_cElements == m_cAllocated)
        {
            /* Need to reallocate the array. */
            size_t cbNewSizeTransportId = (m_cAllocated + 8) * sizeof (VRDPClientArrayTransportId);
            size_t cbNewSizeClientId = (m_cAllocated + 8) * sizeof (VRDPClientArrayClientId);

            void *pTransportId = VRDPMemRealloc (m_paTableTransportId, cbNewSizeTransportId);
            void *pClientId = VRDPMemRealloc (m_paTableClientId, cbNewSizeClientId);

            if (!pTransportId || !pClientId)
            {
                SERVERLOG(("no memory!!!"));
                VRDPMemFree (pClientId);
                VRDPMemFree (pTransportId);
                unlock ();
                return VERR_NO_MEMORY;
            }

            m_paTableTransportId = (VRDPClientArrayTransportId *)pTransportId;
            m_paTableClientId = (VRDPClientArrayClientId *)pClientId;

            m_cAllocated += 8;
        }

        /* Find the index in the transportId array where the new mapping would be placed. */
        int indexTransportId = lookupIndexByTransportId (transportId);

        /* Check that the place is not yet occupied with the same transport id. */
        if (indexTransportId < m_cElements && m_paTableTransportId[indexTransportId].transportId == transportId)
        {
            SERVERLOG(("element at index %d has already the same transport id %d!!! m_cElements = %d\n",
                       indexTransportId, transportId, m_cElements));
            unlock ();
            return VERR_ACCESS_DENIED;
        }

        /* Find the index in the clientId array where the new mapping would be placed. */
        int indexClientId = lookupIndexByClientId (pClient->Id ());

        /* Check that the place is not yet occupied with the same client id. */
        if (indexClientId < m_cElements && m_paTableClientId[indexClientId].u32ClientId == pClient->Id ())
        {
            SERVERLOG(("element at index %d has already the same transport id %d!!! m_cElements = %d\n",
                       indexClientId, transportId, m_cElements));
            unlock ();
            return VERR_ACCESS_DENIED;
        }

        if (indexTransportId < m_cElements)
        {
            /* Existing elements must be moved to make place for the new one. */
            memmove (&m_paTableTransportId[indexTransportId + 1],
                     &m_paTableTransportId[indexTransportId],
                     (m_cElements - indexTransportId) *  sizeof (VRDPClientArrayTransportId));
        }
        else
        {
            Assert (indexTransportId == m_cElements);
        }

        if (indexClientId < m_cElements)
        {
            /* Existing elements must be moved to make place for the new one. */
            memmove (&m_paTableClientId[indexClientId + 1],
                     &m_paTableClientId[indexClientId],
                     (m_cElements - indexClientId) *  sizeof (VRDPClientArrayClientId));
        }
        else
        {
            Assert (indexClientId == m_cElements);
        }

        /* Count the new element. */
        m_cElements++;
        m_cTotalClients++;
        VRDPQueryCurrentTimeMilli(&m_i64TimeLastConnect);
        m_pLastClient = pClient;

        /* Initialize the new elements. */
        m_paTableTransportId[indexTransportId].pClient     = pClient;
        m_paTableTransportId[indexTransportId].transportId = transportId;

        m_paTableClientId[indexClientId].pClient     = pClient;
        m_paTableClientId[indexClientId].u32ClientId = pClient->Id ();

        rc = VINF_SUCCESS;

        unlock ();
    }

    return rc;
}

int VRDPClientArray::Remove (VRDPClient *pClient)
{
    if (pClient == NULL)
    {
        AssertFailed ();
        return VERR_INVALID_PARAMETER;
    }

    int rc = VERR_ACCESS_DENIED;

    if (lock ())
    {
        /* Get indexes in sorted arrays. */
        int indexTransportId = lookupIndexByTransportId (pClient->TransportId ());
        int indexClientId = lookupIndexByClientId (pClient->Id ());

        /* Check that the index is valid. */
        if (   indexTransportId < 0
            || indexTransportId >= m_cElements
            || m_paTableTransportId[indexTransportId].pClient != pClient
            || indexClientId < 0
            || indexClientId >= m_cElements
            || m_paTableClientId[indexClientId].pClient != pClient)
        {
            AssertMsgFailed (("Invalid client index: pClient = %p, indexTransportId = %d, indexClientId = %d\n", pClient, indexTransportId, indexClientId));
            unlock ();
            return VERR_INVALID_PARAMETER;
        }

        /* Adjust the sorted arrays. */
        if (indexTransportId < m_cElements)
        {
            memmove (&m_paTableTransportId[indexTransportId],
                     &m_paTableTransportId[indexTransportId + 1],
                     (m_cElements - indexTransportId) *  sizeof (VRDPClientArrayTransportId));
        }
        else
        {
            Assert (indexTransportId == m_cElements);
        }

        if (indexClientId < m_cElements)
        {
            memmove (&m_paTableClientId[indexClientId],
                     &m_paTableClientId[indexClientId + 1],
                     (m_cElements - indexClientId) *  sizeof (VRDPClientArrayClientId));
        }
        else
        {
            Assert (indexClientId == m_cElements);
        }

        m_cElements--;
        VRDPQueryCurrentTimeMilli(&m_i64TimeLastDisconnect);
        if (pClient == m_pLastClient)
        {
            m_pLastClient = NULL;
        }

        unlock ();

        rc = VINF_SUCCESS;
    }

    return rc;
}

VRDPClient *VRDPClientArray::LookupTransportId (VRDPTRANSPORTID transportId)
{
    VRDPClient *pClient = NULL;

    if (lock ())
    {
        int index = lookupIndexByTransportId (transportId);

        if (index < m_cElements && m_paTableTransportId[index].transportId == transportId)
        {
            pClient = m_paTableTransportId[index].pClient;
        }

        unlock ();
    }

    return pClient;
}

VRDPClient *VRDPClientArray::GetNextClient (VRDPClient *pClient)
{
    VRDPClient *pNextClient = NULL;

    if (lock ())
    {
        int index = 0;

        if (pClient != NULL)
        {
            for (index = 0; index < m_cElements; index++)
            {
                if (m_paTableClientId[index].u32ClientId == pClient->Id ())
                {
                    /* Index of next client. */
                    index++;
                    break;
                }
            }
        }

        if (index < m_cElements)
        {
            pNextClient = m_paTableClientId[index].pClient;
        }

        unlock ();
    }

    return pNextClient;
}


/*
 * VM/OUTPUT thread. Client enumeration, changes the clients 'Status' to 'use'.
 */
VRDPClient *VRDPClientArray::ThreadContextGetNextClient (uint32_t *pu32ClientId, int iContext)
{
    VRDPClient *pClient = NULL;

    uint32_t u32ClientId = *pu32ClientId;
    uint32_t u32NextClientId = 0;

    if (lock ())
    {
l_GetClientIndex:

        int index = lookupIndexByClientId (u32ClientId);

        /* 'index' is where the 'u32ClientId' would be inserted. */
        if (index < m_cElements)
        {
            /* The 'u32ClientId' would be places within the array. */
            if (m_paTableClientId[index].u32ClientId == u32ClientId)
            {
                /* Existing 'u32ClientId', get the next in the array. */
                index++;

                if (index < m_cElements)
                {
                    u32NextClientId = m_paTableClientId[index].u32ClientId;
                    Assert (u32ClientId < u32NextClientId);
                }
                else
                {
                    u32NextClientId = 0;
                }
            }
            else
            {
                /* 'u32ClientId' does not exist in the array, which means that:
                 *     a) 'u32ClientId' == 0 and 'index' == 0 here, the lookup has been started;
                 *     b) 'u32Client' is an element that was removed between calls and 'index'
                 *        points to the element with greater clientId which must be returned.
                 */
                u32NextClientId = m_paTableClientId[index].u32ClientId;
                Assert (   (u32ClientId == 0 && index == 0)
                        || (u32ClientId != 0 && u32ClientId < u32NextClientId));
            }
        }

        if (u32NextClientId != 0)
        {
            pClient = m_paTableClientId[index].pClient;

            VRDPClientUseStatus *pStatus = NULL;

            switch (iContext)
            {
                case VRDP_CONTEXT_VM: pStatus = pClient->StatusVM (); break;
                case VRDP_CONTEXT_OUTPUT: pStatus = pClient->StatusOutput (); break;
                default: break;
            }

            if (!pStatus)
            {
                AssertFailed ();
                pClient = NULL;
            }
            else
            {
                if (!pStatus->ClientUseStateFree2Use ())
                {
                    pClient = NULL;
                }
            }

            if (!pClient)
            {
                u32ClientId = u32NextClientId;
                goto l_GetClientIndex;
            }
        }

        unlock ();
    }

    *pu32ClientId = u32NextClientId;

    Assert(!pClient || (pClient->Id () == u32NextClientId));

    return pClient;
}

VRDPClient *VRDPClientArray::ThreadContextGetClient (uint32_t u32ClientId, int iContext)
{
    VRDPClient *pClient = NULL;

    if (lock ())
    {
        int index = lookupIndexByClientId (u32ClientId);

        /* 'index' is where the 'u32ClientId' would be placed. */
        if (index < m_cElements)
        {
            /* The 'u32ClientId' would be places within the array. */
            if (m_paTableClientId[index].u32ClientId == u32ClientId)
            {
                pClient = m_paTableClientId[index].pClient;

                VRDPClientUseStatus *pStatus = NULL;

                switch (iContext)
                {
                    case VRDP_CONTEXT_VM: pStatus = pClient->StatusVM (); break;
                    case VRDP_CONTEXT_OUTPUT: pStatus = pClient->StatusOutput (); break;
                    default: break;
                }

                if (!pStatus)
                {
                    AssertFailed ();
                    pClient = NULL;
                }
                else
                {
                    if (!pStatus->ClientUseStateFree2Use ())
                    {
                        pClient = NULL;
                    }
                }
            }
        }

        unlock ();
    }

    return pClient;
}

bool VRDPClientArray::lock (void)
{
    return RT_SUCCESS(VRDPLock::Lock(m_pClientArrayLock));
}

void VRDPClientArray::unlock (void)
{
    VRDPLock::Unlock(m_pClientArrayLock);
}

int VRDPClientArray::lookupIndexByTransportId (VRDPTRANSPORTID transportId)
{
    Assert(m_cElements <= (int)((~0U) >> 2));

    /* Search the sorted array. */
    int indexL = 0;
    int indexR = m_cElements;

    while (indexL < indexR)
    {
        int indexM = ((unsigned)(indexR + indexL)) / 2;

        VRDPTRANSPORTID value = m_paTableTransportId[indexM].transportId;

        if (value < transportId)
        {
            indexL = indexM + 1;
        }
        else
        {
            /* Do not decrement the R index, it has to be the element that is >= transportId. */
            indexR = indexM;
        }
    }

    Assert (indexR == m_cElements || m_paTableTransportId[indexR].transportId >= transportId);

    return indexR;
}

int VRDPClientArray::lookupIndexByClientId (uint32_t u32ClientId)
{
    Assert(m_cElements <= (int)((~0U) >> 2));

    /* Search the sorted array. */
    int indexL = 0;
    int indexR = m_cElements;

    while (indexL < indexR)
    {
        int indexM = ((unsigned)(indexR + indexL)) / 2;

        VRDPTRANSPORTID value = m_paTableClientId[indexM].u32ClientId;

        if (value < u32ClientId)
        {
            indexL = indexM + 1;
        }
        else
        {
            /* Do not decrement the R index, it has to be the element that is >= transportId. */
            indexR = indexM;
        }
    }

    Assert (indexR == m_cElements || m_paTableClientId[indexR].u32ClientId >= u32ClientId);

    return indexR;
}

uint32_t VRDPClientArray::NumberOfClientsActive (void)
{
    uint32_t result = 0;

    if (lock ())
    {
        result = m_cElements;
        unlock ();
    }

    return result;
}

uint32_t VRDPClientArray::NumberOfClientsInactive (void)
{
    uint32_t result = 0;

    if (lock ())
    {
        result = m_cTotalClients;
        unlock ();
    }

    return result;
}

int64_t VRDPClientArray::TimeLastConnect (void)
{
    int64_t result = 0;

    if (lock ())
    {
        result = m_i64TimeLastConnect;
        unlock ();
    }

    return result;
}

int64_t VRDPClientArray::TimeLastDisconnect (void)
{
    int64_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            /* Have to return the current time in this case. */
            VRDPQueryCurrentTimeMilli(&result);
        }
        else
        {
            result = m_i64TimeLastDisconnect;
        }
        unlock ();
    }

    return result;
}

uint64_t VRDPClientArray::BytesSentLast (void)
{
    uint64_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            /* It is assumed that the same transport is used for all IO contexts. */
            result = m_pLastClient->InputCtx()->Transport ()->BytesSent (m_pLastClient->TransportId ());
        }

        unlock ();
    }

    return result;
}

uint64_t VRDPClientArray::BytesSentAll (void)
{
    uint64_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            /* It is assumed that the same transport is used for all IO contexts. */
            result = m_pLastClient->InputCtx()->Transport ()->BytesSentTotal ();
        }

        unlock ();
    }

    return result;
}

uint64_t VRDPClientArray::BytesRecvLast (void)
{
    uint64_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            /* It is assumed that the same transport is used for all IO contexts. */
            result = m_pLastClient->InputCtx()->Transport ()->BytesRecv (m_pLastClient->TransportId ());
        }

        unlock ();
    }

    return result;
}

uint64_t VRDPClientArray::BytesRecvAll (void)
{
    uint64_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            /* It is assumed that the same transport is used for all IO contexts. */
            result = m_pLastClient->InputCtx()->Transport ()->BytesRecvTotal ();
        }

        unlock ();
    }

    return result;
}

uint32_t VRDPClientArray::StringLastUser (void *pvBuffer, size_t cbBuffer)
{
    uint32_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            const char *s = m_pLastClient->TP()->getUser ();

            size_t cbOut = strlen (s) + 1;

            if (cbBuffer >= cbOut)
            {
                memcpy (pvBuffer, s, cbOut);
            }

            result = (uint32_t)cbOut;
        }

        unlock ();
    }

    return result;
}

uint32_t VRDPClientArray::StringLastDomain (void *pvBuffer, size_t cbBuffer)
{
    uint32_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            const char *s = m_pLastClient->TP()->getDomain ();

            size_t cbOut = strlen (s) + 1;

            if (cbBuffer >= cbOut)
            {
                memcpy (pvBuffer, s, cbOut);
            }

            result = (uint32_t)cbOut;
        }

        unlock ();
    }

    return result;
}

uint32_t VRDPClientArray::StringLastClientName (void *pvBuffer, size_t cbBuffer)
{
    uint32_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            const char *s = m_pLastClient->TP()->getClientName ();

            size_t cbOut = strlen (s) + 1;

            if (cbBuffer >= cbOut)
            {
                memcpy (pvBuffer, s, cbOut);
            }

            result = (uint32_t)cbOut;
        }

        unlock ();
    }

    return result;
}

uint32_t VRDPClientArray::StringLastClientIP (void *pvBuffer, size_t cbBuffer)
{
    uint32_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            const char *s = m_pLastClient->TP()->getClientIP ();

            size_t cbOut = strlen (s) + 1;

            if (cbBuffer >= cbOut)
            {
                memcpy (pvBuffer, s, cbOut);
            }

            result = (uint32_t)cbOut;
        }

        unlock ();
    }

    return result;
}

uint32_t VRDPClientArray::NumberLastClientVersion (void)
{
    uint32_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            result = m_pLastClient->TP()->getClientVersion ();
        }

        unlock ();
    }

    return result;
}

uint32_t VRDPClientArray::LastClientEncryptionStyle(void)
{
    uint32_t result = 0;

    if (lock ())
    {
        if (m_pLastClient)
        {
            result = m_pLastClient->TP()->Data()->EnhancedSecurity()? 1: 0;
        }

        unlock ();
    }

    return result;
}
