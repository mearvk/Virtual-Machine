/* $Id: transport.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_transport_h
#define VRDP_INCLUDED_SRC_transport_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "vrdpdefs.h"

typedef unsigned VRDPTRANSPORTID;
#define NIL_VRDPTRANSPORTID ((VRDPTRANSPORTID)~0)

/* The transport events. */
#define VRDP_TRANSPORT_DATA_AVAILABLE 0x00000001 /* Data arrived from a client. */
#define VRDP_TRANSPORT_KEEP_ALIVE     0x00000002 /* Inactive for at least 'KeepAliveTimeout' milliseconds. */
#define VRDP_TRANSPORT_SHUTDOWN       0x00000004 /* Transport is being shutdown. */

/* The transport callback interface, used for notifications. */
class VRDPTransportServer
{
    public:
        virtual ~VRDPTransportServer();

        /* The transport queries some configuration values through following methods. */
        virtual uint64_t GetKeepAliveTimeout(void) = 0;
        virtual int QueryFeature(const char *pszName, char **ppszValue, uint32_t *pcbOut) = 0;

        /* New transport connection has been created. */
        virtual int TransportConnect(VRDPTRANSPORTID id) = 0;

        /* Transport connection has been closed. No more transport events for the id. */
        virtual void TransportDisconnect(VRDPTRANSPORTID id) = 0;

        /* Data is available for the given transport ID or any other event happens.
         * The actual data can be read using the Recv method of the transport interface.
         * Return VINF_VRDP_OPERATION_COMPLETED to close the connection.
         */
        virtual int TransportEvent(VRDPTRANSPORTID id, uint32_t fu32Events) = 0;

        /* The transport queries if it must shut down. */
        virtual bool IsShutdown(void) = 0;
        /* Whether it is OUTPUT thread. */
        virtual bool IsOutput(void) = 0;
        /* Whether it is INPUT thread. */
        virtual bool IsInput(void) = 0;
};

/*
 * The transport supports multiple connections.
 * Each connection has an unique VRDPTRANSPORTID.
 */
class VRDPTransport
{
    private:
        VRDPTransportServer *m_pServer;

    public:
        VRDPTransport(VRDPTransportServer *pServer);
        virtual ~VRDPTransport();

        VRDPTransportServer *Server(void) { return m_pServer; }

        /* Initialize the transport. */
        virtual int Open(void) = 0;

        /* Send notification about shutdown for the transport. */
        virtual void NotifyShutdown(void) = 0;

        /* Allow incoming connections. The transport will use VRDPTransportServer
         * callbacks when a new connection is created or there are data available
         * on a existing connection, etc
         * BLOCKING.
         */
        virtual int Listen(void) = 0;

        /* Uninitialize. */
        virtual void Close(void) = 0;

        /* Initialize TLS for the transport connection. */
        virtual int TLSInit(VRDPTRANSPORTID id) = 0;

        /* Enable TLS for the transport connection. */
        virtual int TLSStart(VRDPTRANSPORTID id) = 0;

        /* Read specified amount of bytes from transport connection.
         * The method must be called only within the TransportEvent callback.
         * NON BLOCKING.
         */
        virtual int Recv(VRDPTRANSPORTID id, uint8_t *pu8Data, unsigned cbData, unsigned *pcbActual) = 0;

        /* Send data for the transport connection.
         * BLOCKING.
         */
        virtual int Send(VRDPTRANSPORTID id, const uint8_t *pu8Data, unsigned cbData) = 0;

        virtual const char *GetInfo(VRDPTRANSPORTID id) = 0;
        virtual uint64_t BytesRecv(VRDPTRANSPORTID id) = 0;
        virtual uint64_t BytesSent(VRDPTRANSPORTID id) = 0;
        virtual uint64_t BytesRecvTotal(void) = 0;
        virtual uint64_t BytesSentTotal(void) = 0;
        virtual unsigned GetBindPort(void) = 0;
};

#endif /* !VRDP_INCLUDED_SRC_transport_h */
