/* $Id: tcp_vrdp.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifdef RT_OS_WINDOWS
# include <iprt/win/ws2tcpip.h>
#else
# include <errno.h>
# include <sys/stat.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# ifdef RT_OS_SOLARIS
#  include <sys/filio.h>
# endif
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <sys/un.h>
# include <netdb.h>
# include <unistd.h>
#endif /* !RT_OS_WINDOWS */

#include <iprt/list.h>
#include <iprt/path.h>
#include <iprt/crypto/ssl.h>

#include "utils.h"
#include "tcp.h"


/* VRDP socket descriptor type. */
#ifdef RT_OS_WINDOWS
# define VRDPSOCKET SOCKET
# define NIL_VRDPSOCKET INVALID_SOCKET
#else
# define VRDPSOCKET int
# define NIL_VRDPSOCKET ((VRDPSOCKET)-1)
#endif /* !RT_OS_WINDOWS */

#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define socklen_t int
#endif

#ifndef SHUT_RDWR
# ifdef SD_BOTH
#  define SHUT_RDWR SD_BOTH
# else
#  define SHUT_RDWR 2
# endif
#endif


/* Connection context.
 *
 * Maps VRDP transport id to the socket and SSL connection.
 * Transport maintains a list of connections.
 *
 * The list is modified only by Listen method on INPUT thread.
 * The list is protected by a lock, because both INPUT and OUTPUT threads access it
 *
 * The context is referenced: OUTPUT thread may use it while it is deleted
 * and removed from list by INPUT.
 */
typedef struct TCPTRANSPORTIDCTX
{
    VRDPPKT pkt; /* Referenced packet. */

    RTLISTNODE nodeConnection;

    uint64_t volatile u64BytesRecv;
    uint64_t volatile u64BytesSent;

    uint64_t volatile u64LastActivity;

    VRDPTRANSPORTID id; /* Unique during the server lifetime. */

    VRDPSOCKET sock;

    struct {
        bool fTLSEnabled;
        RTCRSSL         hSsl;
        RTCRSSLSESSION  hSession;
        bool fPending;
    } TLSData;

    bool volatile fSendFailed;

    /* The first byte of the first packet received in the processFirstPacket. */
    bool fFirstPacketByte;
    uint8_t u8FirstPacketByte;
} TCPTRANSPORTIDCTX;

typedef struct TCPSOCKETLISTEN
{
    VRDPSOCKET socketListen;
    int addressFamily;
} TCPSOCKETLISTEN;

class VRDPTCPTransport: public VRDPTransport
{
    private:

        const char *m_pszAddress;
        const char *m_pszPortRange;

        uint16_t m_u16Port;
        uint16_t m_u16BindPort;

        const char *m_pszUnixSocketPath;

        int m_cbSendBuffer;
        int m_cbRecvBuffer;

        uint64_t m_u64KeepAliveTimeout;

        VRDPLock *m_pLock; /* Serialize the connections list. */
        VRDPLock *m_pLockTLS; /* Serialize SSL library. */

        VRDPTRANSPORTID m_idSource;

        int m_cSocketsListen;
        TCPSOCKETLISTEN *m_paSocketsListen;

        int m_cSocketsClient;

        RTLISTANCHOR m_listConnections;

        bool volatile m_fShutdown; /* Whether Shutdown was called. */

        int clientNewConnection(TCPSOCKETLISTEN *pSocketListen);
        void clientDisconnect(TCPTRANSPORTIDCTX *pCtx);

        TCPTRANSPORTIDCTX *ctxById(VRDPTRANSPORTID id);

        int createListeningSocketsTCP(void);
        int createListeningSocketsUNIX(void);

    public:
        VRDPTCPTransport (VRDPTransportServer *pServer, const char *pszAddress,
                          unsigned uPort, const char *pszPortRange, const char *pszUnixSocketPath);
        virtual ~VRDPTCPTransport ();

        virtual int Open(void);
        virtual int Listen(void);
        virtual void Close(void);
        virtual int Recv(VRDPTRANSPORTID id, uint8_t *pData, unsigned uLength, unsigned *puActual);
        virtual int Send(VRDPTRANSPORTID id, const uint8_t *pData, unsigned uLength);
        virtual void NotifyShutdown(void);

        virtual const char *GetInfo (VRDPTRANSPORTID id) { RT_NOREF1(id); return ""; };
        virtual uint64_t BytesRecv (VRDPTRANSPORTID id);
        virtual uint64_t BytesSent (VRDPTRANSPORTID id);
        virtual uint64_t BytesRecvTotal (void);
        virtual uint64_t BytesSentTotal (void);
        virtual unsigned GetBindPort (void) { return m_u16BindPort; };

        virtual int TLSInit(VRDPTRANSPORTID id);
        virtual int TLSStart(VRDPTRANSPORTID id);
};


static void tlsClose(TCPTRANSPORTIDCTX *pCtx);


/*
 * Socket helpers.
 */
static int socketSetNonBlock (VRDPSOCKET sock)
{
#ifdef RT_OS_WINDOWS
    ULONG ulValueEnableNonBlock = 1;
    int rc = ioctlsocket (sock, FIONBIO, &ulValueEnableNonBlock);
#else
    int iValueEnableNonBlock = 1;
    int rc = ioctl (sock, FIONBIO, (char *)&iValueEnableNonBlock);
#endif /* RT_OS_WINDOWS */

    if (rc != 0)
    {
        AssertFailed ();
        return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

static void socketBufferLog(const char *pszAction, int opt, int value, int res)
{
    TCPLOG(("%s %s(0x%X) value %d res = %d\n",
             pszAction,
             opt == SO_SNDBUF? "SO_SNDBUF":
                 opt == SO_RCVBUF? "SO_RCVBUF": "unknown",
             opt, value, res));
    NOREF(pszAction); NOREF(opt); NOREF(value); NOREF(res);
}

static void socketBufferSet(VRDPSOCKET sock, int opt, int cbBuffer)
{
    int value = 0;
    socklen_t len = sizeof(value);
    int res = getsockopt(sock, SOL_SOCKET, opt, (char *)&value, &len);
    socketBufferLog("Get", opt, value, res);

    if (res == 0)
    {
        while (value < cbBuffer)
        {
            res = setsockopt(sock, SOL_SOCKET, opt, (const char *)&cbBuffer, sizeof(int));
            socketBufferLog("Set", opt, cbBuffer, res);
            if (res == 0)
            {
                /* Successfully set. */
                break;
            }
            cbBuffer -= 8*_1K; /* Arbitrary. Just try a lower value. */
        }

        if (res < 0)
        {
            /* Unable to set a larger value. Use system default. */
            cbBuffer = value;
            res = setsockopt(sock, SOL_SOCKET, opt, (const char *)&cbBuffer, sizeof(int));
            socketBufferLog("Set default", opt, cbBuffer, res);
        }
    }
}

static void socketSetNoDelay(VRDPSOCKET sock, int flag)
{
    int res = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(int));
    TCPLOG(("TCP_NODELAY(%d) res = %d\n", flag, res));
    NOREF(res);
}

static void socketClose(VRDPSOCKET sock)
{
    if (sock != NIL_VRDPSOCKET)
    {
        TCPLOG(("closing socket %d\n", sock));
#ifdef RT_OS_WINDOWS
        closesocket(sock);
#else
        close(sock);
#endif /* RT_OS_WINDOWS */
    }
}

static int socketListenSetOptions(VRDPSOCKET socketListen, int cbSendBuffer, int cbRecvBuffer)
{
    int rc = VINF_SUCCESS;

#ifndef RT_OS_WINDOWS
    /* SO_REUSEADDR on Windows host allows to bind to an already used port. */
    int iFlag = 1;
    if (setsockopt(socketListen, SOL_SOCKET, SO_REUSEADDR, (const char *)&iFlag, sizeof(iFlag)) != 0)
    {
        TCPLOG(("Failed to set listening socket REUSEADDR!!!\n"));
        rc = VERR_NET_OPERATION_NOT_SUPPORTED;
    }
#endif /* RT_OS_WINDOWS */

    if (RT_SUCCESS(rc))
    {
        rc = socketSetNonBlock(socketListen);

        if (RT_FAILURE(rc))
        {
            TCPLOG(("Failed to set listening socket in non block!!!\n"));
            rc = VERR_NET_OPERATION_NOT_SUPPORTED;
        }
    }

    if (RT_SUCCESS(rc))
    {
        /* SO_*BUF options must be set for the listening socket.
         * They will be inherited by connected ('accept'ed) sockets.
         */
        socketBufferSet(socketListen, SO_SNDBUF, cbSendBuffer);
        socketBufferSet(socketListen, SO_RCVBUF, cbRecvBuffer);
    }

    return rc;
}

static void socketListenDeleteAll(TCPSOCKETLISTEN *paSockets, int cSockets)
{
    int i;
    for (i = 0; i < cSockets; i++)
    {
        socketClose(paSockets[i].socketListen);
    }
    VRDPMemFree(paSockets);
}

DECLINLINE(int) socketErrorGet(void)
{
#ifdef RT_OS_WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif /* RT_OS_WINDOWS */
}

DECLINLINE(void) socketErrorClear(void)
{
#ifdef RT_OS_WINDOWS
    WSASetLastError(0);
#else
    errno = 0;
#endif
}

static void socketErrorLog(const char *pszDesc)
{
    VRDPLOGRELIO(("VRDP: TCP: %s result code %d\n", pszDesc, socketErrorGet()));
}


VRDPTCPTransport::VRDPTCPTransport (VRDPTransportServer *pServer, const char *pszAddress, unsigned uPort,
                                    const char *pszPortRange, const char *pszUnixSocketPath)
    :
    VRDPTransport         (pServer),
    m_pszAddress          (pszAddress),
    m_pszPortRange        (pszPortRange),
    m_u16Port             ((uint16_t)uPort),
    m_u16BindPort         (0),
    m_pszUnixSocketPath   (pszUnixSocketPath),
    m_cbSendBuffer        (64 * _1K),
    m_cbRecvBuffer        (32 * _1K),
    m_u64KeepAliveTimeout (pServer->GetKeepAliveTimeout()),
    m_idSource            (NIL_VRDPTRANSPORTID),
    m_cSocketsListen      (0),
    m_paSocketsListen     (NULL),
    m_cSocketsClient      (0),
    m_fShutdown           (false)
{
    /* TCP port numbers are 16 bits. */
    Assert (uPort < 65536);

    RTListInit(&m_listConnections);
    VRDPLock::Create("TCP", &m_pLock);
    VRDPLock::Create("TLS", &m_pLockTLS);
}

VRDPTCPTransport::~VRDPTCPTransport()
{
    /* All clients must be closed by now. */
    AssertLogRelMsg(RTListIsEmpty(&m_listConnections), ("VRDP: Not disconnected clients!\n"));

    VRDPLock::Delete(&m_pLock);
    VRDPLock::Delete(&m_pLockTLS);

    VRDPLOGREL(("TCP server closed.\n"));
}

#if 0 /** @todo unused openCleanup */
static void openCleanup(VRDPSOCKET sock)
{
    socketClose(sock);
#ifdef RT_OS_WINDOWS
    WSACleanup();
#endif /* RT_OS_WINDOWS */
}
#endif


int VRDPTCPTransport::createListeningSocketsTCP(void)
{
    int rc = VINF_SUCCESS;
    int ret;

    /* Multiple listening sockets with IPv6. */
    int i;

    /*
     * Enumerate ports, get the address info and try to bind listening socket(s).
     */

    /* The hostname parameter for getaddrinfo. */
    const char *hostname = (m_pszAddress != NULL && *m_pszAddress)?
        m_pszAddress:
        NULL;

    PORTENUMCONTEXT ctx;
    portEnumStart(&ctx, m_pszPortRange, m_u16Port);
    for (;;)
    {
        uint16_t u16Port = portEnumNext(&ctx);
        TCPLOG(("Port %RU16\n", u16Port));

        if (u16Port == 0)
        {
            /* Port enumeration completed. */
            VRDPLOGREL(("TCP server failed to bind to a port: default %d, range [%s]\n",
                        m_u16Port, m_pszPortRange? m_pszPortRange: ""));
            rc = VERR_NET_ADDRESS_IN_USE;
            break;
        }

        /* getaddrinfo needs a string parameter. */
        char service[32];
        RTStrPrintf(service, sizeof(service), "%RU16", u16Port);

        struct addrinfo hints;
        struct addrinfo *res = NULL;

        RT_ZERO(hints);                                  /* All unused fields must be zero. */
        hints.ai_flags    = AI_PASSIVE | AI_NUMERICSERV; /* Listening, numeric port number. */
        hints.ai_family   = PF_UNSPEC;                   /* Both 4 and 6 */
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        ret = getaddrinfo(hostname, service, &hints, &res);

        if (ret != 0 || res == NULL)
        {
            TCPLOG(("getaddrinfo failed %d %s:%s\n", res, hostname? hostname: "*", service));
            continue; /* Failed, try the next port. */
        }

        /* Count how many sockets will be needed. */
        int cSockets = 0;
        struct addrinfo *iter;
        for (iter = res; iter != NULL; iter = iter->ai_next)
        {
            if (    iter->ai_family != AF_INET
                 && iter->ai_family != AF_INET6)
            {
                continue;
            }

            cSockets++;
            if (cSockets >= FD_SETSIZE - 1) /* At least one socket for the client. */
            {
                break; /* Can't have so many listening sockets anyway. */
            }
        }

        TCPLOG(("cSockets %d\n", cSockets));

        /* Create 'cSockets' sockets. */
        TCPSOCKETLISTEN *paSockets = (TCPSOCKETLISTEN *)VRDPMemAlloc(cSockets * sizeof(TCPSOCKETLISTEN));
        if (!paSockets)
        {
            freeaddrinfo(res);
            rc = VERR_NO_MEMORY;
            break;
        }

        for (i = 0; i < cSockets; i++)
        {
            paSockets[i].socketListen = NIL_VRDPSOCKET;
        }

        /* Try to create sockets and bind them. */
        uint8_t fuFamily = 0;
        for (i = 0, iter = res; iter != NULL && i < cSockets; iter = iter->ai_next)
        {
            if (    iter->ai_family != AF_INET
                    && iter->ai_family != AF_INET6)
            {
                continue;
            }

            paSockets[i].addressFamily = iter->ai_family;
            paSockets[i].socketListen = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
            if (paSockets[i].socketListen == NIL_VRDPSOCKET)
            {
                TCPLOG(("socket failed\n"));
                continue;
            }

            int rc2 = socketListenSetOptions(paSockets[i].socketListen, m_cbSendBuffer, m_cbRecvBuffer);
            if (RT_FAILURE(rc2))
            {
                socketClose(paSockets[i].socketListen);
                continue;
            }

#if defined(IPV6_V6ONLY)
            if (iter->ai_family == AF_INET6)
            {
                /* Prevent v6 socket from listening for v4. The server has separate sockets for each family. */
                int flag = 1;
                setsockopt(paSockets[i].socketListen, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&flag, sizeof(flag));
            }
#endif

            if (bind(paSockets[i].socketListen, iter->ai_addr, (int)iter->ai_addrlen) < 0)
            {
                TCPLOG(("bind failed\n"));
                socketClose(paSockets[i].socketListen);
                continue;
            }

            TCPLOG(("bind socket %d to %s: %.*Rhxs\n",
                    paSockets[i].socketListen, iter->ai_family == AF_INET? "AF_INET": "AF_INET6", iter->ai_addrlen, iter->ai_addr));

            fuFamily |= iter->ai_family == AF_INET? 0x01: 0x02;

            i++;
        }

        cSockets = i;

        /* It is OK if at least one socket was created. */
        rc = cSockets == 0? VERR_NOT_SUPPORTED: VINF_SUCCESS;

        freeaddrinfo(res);

        if (RT_SUCCESS(rc))
        {
            /* Successfully created listening socket(s). Leave the loop. */
            VRDPLOGREL(("TCP server listening on port %RU16 (%s).\n",
                        u16Port,
                        fuFamily == 0x3? "IPv4 and IPv6":
                        fuFamily == 0x2? "IPv6":
                        "IPv4"
                           ));
            m_u16BindPort = u16Port;
            m_paSocketsListen = paSockets;
            m_cSocketsListen = cSockets;
            break;
        }

        /* Close sockets, free the array and continue with the next port. */
        socketListenDeleteAll(paSockets, cSockets);
        rc = VINF_SUCCESS;
    }

    return rc;
}

int VRDPTCPTransport::createListeningSocketsUNIX(void)
{
#ifdef RT_OS_LINUX
    int rc = VINF_SUCCESS;

    struct sockaddr_un addr;
    RT_ZERO(addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, m_pszUnixSocketPath,
            sizeof(addr.sun_path)-1);
    int cSockets = 1;

    TCPLOG(("cSockets %d\n", cSockets));

    /* Create 'cSockets' sockets. */
    TCPSOCKETLISTEN *paSockets = (TCPSOCKETLISTEN *)VRDPMemAlloc(cSockets * sizeof(TCPSOCKETLISTEN));
    if (!paSockets)
    {
        return VERR_NO_MEMORY;
    }
    paSockets[0].socketListen = NIL_VRDPSOCKET;
    paSockets[0].addressFamily = AF_UNIX;
    paSockets[0].socketListen = socket(AF_UNIX, SOCK_STREAM, 0);

    if (paSockets[0].socketListen == NIL_VRDPSOCKET)
    {
        TCPLOG(("UNIX Socket creation failed\n"));
        socketListenDeleteAll(paSockets, cSockets);
        return VERR_NOT_SUPPORTED;
    }
    if (bind(paSockets[0].socketListen, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        TCPLOG(("UNIX Socket bind failed\n"));
        socketClose(paSockets[0].socketListen);
        socketListenDeleteAll(paSockets, cSockets);
        return VERR_NOT_SUPPORTED;
    }

    /* Successfully created listening socket(s). Leave the loop. */
    VRDPLOGREL(("UNIX Socket server listening on %s.\n",
                m_pszUnixSocketPath));
    m_u16BindPort = 0;
    m_paSocketsListen = paSockets;
    m_cSocketsListen = cSockets;
    return rc;
#else
    return VERR_NOT_SUPPORTED;
#endif
}

/*
 * Create listening socket(s).
 *
 * @thread INPUT
 */
int VRDPTCPTransport::Open(void)
{
    if (   !VRDPLock::IsInitialized(m_pLock)
        || !VRDPLock::IsInitialized(m_pLockTLS))
    {
        AssertFailed();
        return VERR_INVALID_STATE;
    }

    if (!Server()->IsInput())
    {
        AssertFailed();
        return VERR_INVALID_CONTEXT;
    }

#ifdef RT_OS_WINDOWS
    /* Initialize Winsock */
    WSADATA wsaData;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (ret != 0)
    {
        AssertLogRelMsgFailed(("Failed to init winsock = %d.\n", ret));
        return VERR_NET_NO_NETWORK;
    }

    if (wsaData.wVersion != MAKEWORD(2, 2))
    {
        WSACleanup();
        AssertLogRelMsgFailed(("Unsupported winsock version: 0x%04X(0x%04X).\n",
                               wsaData.wVersion, wsaData.wHighVersion));
        return VERR_VERSION_MISMATCH;
    }
#endif /* RT_OS_WINDOWS */

    int rc = VINF_SUCCESS;
    /*
     * If we have a valid UNIX Socket Path, then we will not do any TCP
     * socket processing.
     */
    if (m_pszUnixSocketPath != NULL)
    {
        rc = createListeningSocketsUNIX();
    }
    else
    {
        rc = createListeningSocketsTCP();
    }

    if (RT_FAILURE(rc))
    {
#ifdef RT_OS_WINDOWS
        WSACleanup();
#endif /* RT_OS_WINDOWS */
    }

    TCPLOG(("Open %Rrc\n", rc));
    return rc;
}

/** @thread any */
void VRDPTCPTransport::NotifyShutdown(void)
{
    /* This is called to get out of Listen, which should check the condition
     * before and after 'select'.
     */
    TCPLOG(("Setting shutdown flag\n"));
    ASMAtomicWriteBool(&m_fShutdown, true);
}

static int socketListen(TCPSOCKETLISTEN *paSockets, int cSockets, int *pcListeningSockets)
{
    /* Set sockets to the listening state. At least one socket must listen.
     * On some system a listening IPv6 socket does not allow to use an IPv4
     * socket on the same port.
     */
    int i;
    for (i = 0; i < cSockets; i++)
    {
        int res = listen(paSockets[i].socketListen, 5);
        TCPLOG(("Socket[%d]: %d res %d\n", i, paSockets[i].socketListen, res));
        if (res != 0)
        {
            /* Exclude the socket */
            socketClose(paSockets[i].socketListen);
            paSockets[i].socketListen = NIL_VRDPSOCKET;
        }
    }

    /* Check if there are any listening sockets and compact the array. */
    int cListeningSockets = 0;
    for (i = 0; i < cSockets; i++)
    {
        if (paSockets[i].socketListen != NIL_VRDPSOCKET)
        {
             paSockets[cListeningSockets] = paSockets[i];
             cListeningSockets++;
        }
    }

    if (cListeningSockets)
    {
        TCPLOG(("%d listening socket(s)\n", cListeningSockets));
        *pcListeningSockets = cListeningSockets;
        return VINF_SUCCESS;
    }

    return VERR_NET_IO_ERROR;
}

/** @thread INPUT */
int VRDPTCPTransport::Listen(void)
{
    TCPTRANSPORTIDCTX *pNext;
    TCPTRANSPORTIDCTX *pIter;
    int i;

    if (   m_cSocketsListen == 0
        || m_cSocketsListen >= FD_SETSIZE
        || m_paSocketsListen == NULL
        || m_cSocketsClient != 0)
    {
        AssertFailed();
        return VERR_INVALID_STATE;
    }

    if (!Server()->IsInput())
    {
        AssertFailed();
        return VERR_INVALID_CONTEXT;
    }

    int rc = socketListen(m_paSocketsListen, m_cSocketsListen, &m_cSocketsListen);

    if (RT_FAILURE(rc))
    {
        TCPLOG(("Failed to set socket(s) in listening state!!!\n"));
        return rc;
    }

    while (!ASMAtomicReadBool(&m_fShutdown))
    {
        /* Use 'select' to detect new connections and arriving data. */
        fd_set readfds;
        fd_set errorfds;

        FD_ZERO(&readfds);
        FD_ZERO(&errorfds);

        VRDPSOCKET iHighestSocketNumber = 0;

        /* Add listener sockets. */
        for (i = 0; i < m_cSocketsListen; i++)
        {
            VRDPSOCKET sock = m_paSocketsListen[i].socketListen;
            Assert(sock != NIL_VRDPSOCKET);

            FD_SET(sock, &readfds);
            FD_SET(sock, &errorfds);
            if (iHighestSocketNumber < sock)
            {
                iHighestSocketNumber = sock;
            }
        }

        int cTLSPending = 0; /* Immediately detect if there are any pending data on TLS sockets. */

        /* Gather information about sockets from the list.
         * No need to take the list lock because the list is modified only by this thread.
         */
        RTListForEach(&m_listConnections, pIter, TCPTRANSPORTIDCTX, nodeConnection)
        {
            if (pIter->id == NIL_VRDPTRANSPORTID)
            {
                continue;
            }

            VRDPSOCKET sock = pIter->sock;
            Assert(sock != NIL_VRDPSOCKET);

            FD_SET(sock, &readfds);
            FD_SET(sock, &errorfds);

            if (iHighestSocketNumber < sock)
            {
                iHighestSocketNumber = sock;
            }

            if (pIter->TLSData.fTLSEnabled)
            {
                /* TLS socket */
                if (RT_SUCCESS(VRDPLock::Lock(m_pLockTLS)))
                {
                    pIter->TLSData.fPending = (RTCrSslSessionPending(pIter->TLSData.hSession) != 0);
                    VRDPLock::Unlock(m_pLockTLS);
                }
                else
                {
                    AssertFailed();
                    pIter->TLSData.fPending = false;
                }

                if (pIter->TLSData.fPending)
                {
                    cTLSPending++;
                }
            }
        }

        // TCPLOG(("Calling select iHighestSocketNumber = %d, cTLSPending %d\n", iHighestSocketNumber, cTLSPending));

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = (cTLSPending == 0 && !ASMAtomicReadBool(&m_fShutdown))?
                              250 * 1000:
                              0; /* If there are TLS pending or shutdown is requested then just check the sockets. */

        int cSockets = select(iHighestSocketNumber + 1, &readfds, NULL, &errorfds, &timeout);

        // TCPLOG(("Select returned %d, listen socket 0x%08X, readfds %d, errorfds %d \n",
        //         cSockets, m_sockListen,
        //         FD_ISSET(m_sockListen, &readfds),
        //         FD_ISSET(m_sockListen, &errorfds)));

        if (ASMAtomicReadBool(&m_fShutdown))
        {
            TCPLOG(("Shutdown detected\n"));
            break;
        }

        if (cTLSPending > 0)
        {
            /* Possibly adjust cSockets. */
            if (cSockets <= 0)
            {
                cSockets = cTLSPending; /* There are at least TLS sockets which have data. */
            }
            else
            {
                /* Do nothing.
                 * The code below does not need the exact number of sockets. It only checks whether cSockets > 0.
                 */
            }
        }

        if (cSockets < 0)
        {
            VRDPThreadSleep(10);
            continue;
        }

        /* Check is there are new connections. */
        if (cSockets > 0)
        {
            for (i = 0; i < m_cSocketsListen; i++)
            {
                VRDPSOCKET sock = m_paSocketsListen[i].socketListen;
                Assert(sock != NIL_VRDPSOCKET);

                if (FD_ISSET(sock, &readfds))
                {
                    /* clientNewConnection will also make sure that total number of sockets
                     * does not exceed FD_SETSIZE.
                     */
                    rc = clientNewConnection(&m_paSocketsListen[i]);
                    TCPLOG(("New connection %Rrc\n", rc));
                }
            }
        }

        uint64_t u64NowTS = VRDPTimeMilliTS();

        RTListForEachSafe(&m_listConnections, pIter, pNext, TCPTRANSPORTIDCTX, nodeConnection)
        {
            if (pIter->id == NIL_VRDPTRANSPORTID)
            {
                continue;
            }

            uint32_t fu32Events = 0;

            VRDPSOCKET sock = pIter->sock;
            Assert(sock != NIL_VRDPSOCKET);

            if (   cSockets > 0
                && (FD_ISSET(sock, &readfds) || pIter->TLSData.fPending))
            {
                fu32Events |= VRDP_TRANSPORT_DATA_AVAILABLE;
                ASMAtomicWriteU64(&pIter->u64LastActivity, u64NowTS);
            }
            else
            {
                if (u64NowTS - ASMAtomicReadU64(&pIter->u64LastActivity) >= m_u64KeepAliveTimeout)
                {
                    fu32Events |= VRDP_TRANSPORT_KEEP_ALIVE;
                    ASMAtomicWriteU64(&pIter->u64LastActivity, u64NowTS);
                }
            }

            rc = Server()->TransportEvent(pIter->id, fu32Events);

            if (   RT_FAILURE(rc)
                || rc == VINF_VRDP_OPERATION_COMPLETED
                || ASMAtomicReadBool(&pIter->fSendFailed))
            {
                TCPLOG(("TransportEvent failed or client to be disconnected fSendFailed %d, rc = %Rrc!!!\n",
                       pIter->fSendFailed, rc));

                clientDisconnect(pIter);
            }
        }
    }

    Assert(ASMAtomicReadBool(&m_fShutdown));

    /* Transport was shut down. Make sure that clients are notified. */
    RTListForEachSafe(&m_listConnections, pIter, pNext, TCPTRANSPORTIDCTX, nodeConnection)
    {
        rc = Server()->TransportEvent(pIter->id, VRDP_TRANSPORT_SHUTDOWN);
        TCPLOG(("Transport shutdown for %d rc = %Rrc!!!\n", pIter->id, rc));

        clientDisconnect(pIter);
    }

    return VINF_SUCCESS;
}

/** @thread INPUT */
void VRDPTCPTransport::Close(void)
{
    TCPLOG(("Closing TCP\n"));

    if (!Server()->IsInput())
    {
        AssertFailed();
        return;
    }

    socketListenDeleteAll(m_paSocketsListen, m_cSocketsListen);
    m_cSocketsListen = 0;
    m_paSocketsListen = NULL;

#ifdef RT_OS_WINDOWS
    WSACleanup();
#endif /* RT_OS_WINDOWS */
}

/** @thread INPUT */
int VRDPTCPTransport::Recv(VRDPTRANSPORTID id, uint8_t *pu8Data, unsigned cbData, unsigned *pcbActual)
{
    Assert(Server()->IsInput());

    /* Find the connection.
     * No need to take the list lock because this is the INPUT thread.
     */
    TCPTRANSPORTIDCTX *pIter = ctxById(id);

    if (!pIter)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    VRDPSOCKET sock = pIter->sock;

    if (pIter->fFirstPacketByte)
    {
        /* Return the cached first byte. */
        pIter->fFirstPacketByte = false;

        *pu8Data = pIter->u8FirstPacketByte;
        *pcbActual = 1;

        ASMAtomicAddU64(&pIter->u64BytesRecv, 1);

        return VINF_SUCCESS;
    }

    socketErrorClear();

    int cb;
    if (pIter->TLSData.fTLSEnabled)
    {
        if (RT_SUCCESS(VRDPLock::Lock(m_pLockTLS)))
        {
            for (;;)
            {
                cb = (int)RTCrSslSessionRead(pIter->TLSData.hSession, pu8Data, cbData);
                if (cb <= 0)
                {
                   if (cb == VERR_TRY_AGAIN)
                   {
                       fd_set readfds;
                       FD_ZERO(&readfds);
                       FD_SET(sock, &readfds);

                       struct timeval timeout;
                       timeout.tv_sec = 0;
                       timeout.tv_usec = 250 * 1000;

                       cb = select(sock + 1, &readfds, NULL, NULL, &timeout);

                       if (cb >= 0)
                       {
                           continue; /* Can read or timeout. Retry SSL_read. */
                       }

                       VRDPLOGRELLIMIT(16, ("VRDP: RTCrSslSessionRead + select failed.\n"));
                   }
                   else
                   {
                       VRDPLOGRELLIMIT(16, ("VRDP: RTCrSslSessionRead failed (%Rrc).\n", (int)cb));
                       cb = -1;
                   }
                }

                break;
            }

            VRDPLock::Unlock(m_pLockTLS);
        }
        else
        {
            /* Drop the connection. */
            cb = -1;
        }

        TCPLOG(("RTCrSslSessionRead cb %d\n", cb));
    }
    else
    {
        cb = recv(sock, (char *)pu8Data, cbData, 0);
        if (cb < 0)
        {
            socketErrorLog("recv");
        }
    }

    if (cb <= 0)
    {
        TCPLOG(("recv returns %d\n", cb));
        return VINF_VRDP_OPERATION_COMPLETED;
    }

    *pcbActual = cb;

    ASMAtomicAddU64(&pIter->u64BytesRecv, cb);

    return VINF_SUCCESS;
}

/** @thread INPUT and OUTPUT */
int VRDPTCPTransport::Send (VRDPTRANSPORTID id, const uint8_t *pu8Data, unsigned cbData)
{
    TCPLOG (("send id = %d, cbData = %d\n", id, cbData));

    /* Find the connection.
     * Take the list lock, if not INPUT thread, and reference the connection context.
     */
    if (!Server()->IsInput())
    {
        if (RT_FAILURE(VRDPLock::Lock(m_pLock)))
        {
            AssertFailed();
            return VERR_NOT_SUPPORTED;
        }
    }

    TCPTRANSPORTIDCTX *pIter = ctxById(id);

    if (pIter)
    {
        VRDPPktAddRef(&pIter->pkt);
    }

    if (!Server()->IsInput())
    {
        VRDPLock::Unlock(m_pLock);
    }

    if (!pIter)
    {
        TCPLOG(("could not find: id %d!!!\n", id));
        return VERR_INVALID_PARAMETER;
    }

    VRDPSOCKET sock = pIter->sock;
    int cSendErrors = 0;

    do
    {
        socketErrorClear();

        /* Use 'select' only if 'send' fails. */
        fd_set writefds;
        struct timeval timeout;

        int cb = 0;

        /* Try to send. */
        if (pIter->TLSData.fTLSEnabled)
        {
            if (RT_SUCCESS(VRDPLock::Lock(m_pLockTLS)))
            {
                for (;;)
                {
                    cb = (int)RTCrSslSessionWrite(pIter->TLSData.hSession, pu8Data, cbData);
                    if (cb <= 0)
                    {
                        if (cb == VERR_TRY_AGAIN)
                        {
                            FD_ZERO(&writefds);
                            FD_SET(sock, &writefds);

                            timeout.tv_sec = 0;
                            timeout.tv_usec = 250 * 1000;

                            cb = select(sock + 1, NULL, &writefds, NULL, &timeout);

                            if (cb >= 0)
                            {
                                continue; /* Can write or timeout. Retry SSL_write. */
                            }

                            VRDPLOGRELLIMIT(16, ("VRDP: RTCrSslSessionWrite + select failed.\n"));
                        }
                        else
                        {
                            VRDPLOGRELLIMIT(16, ("VRDP: RTCrSslSessionWrite failed: %Rrc\n", (int)cb));
                            cb = -1;
                        }
                    }

                    break;
                }

                VRDPLock::Unlock(m_pLockTLS);
            }
            else
            {
                /* Drop the connection. */
                VRDPLOGRELLIMIT(16, ("VRDP: RTCrSslSessionWrite lock failed.\n"));
                cb = -1;
            }
            TCPLOG(("RTCrSslSessionWrite cb %d\n", cb));
        }
        else
        {
            TCPLOG(("send\n"));
            cb = send(sock, (const char *)pu8Data, cbData, 0);
            if (cb < 0)
            {
                socketErrorLog("send");
            }
        }

        if (cb <= 0)
        {
            cSendErrors++;

            if (cSendErrors > 32)
            {
                /* Too many send errors. */
                VRDPLOGREL(("TCP server can't send data.\n"));

                VRDPPktRelease(&pIter->pkt);
                return VERR_NOT_SUPPORTED;
            }

            /* Socket is not able to send. Use 'select'. */
            FD_ZERO(&writefds);
            FD_SET(sock, &writefds);

            /* Timeout is necessary for checking if the socket was closed. */
            timeout.tv_sec = 0;
            timeout.tv_usec = 250 * 1000;

            cb = select(sock + 1, NULL, &writefds, NULL, &timeout);

            /** @todo 'select' may return EINTR. */
            TCPLOG(("send select id %d, cb %d.\n", id, cb));

            if (   cb < 0
                || (cb == 0 && cSendErrors >= 8)) /* Two seconds. */
            {
                /* An error occurred, disconnect the client. */
                VRDPLOGREL(("TCP server failed to send data to the client!!!\n"));

                shutdown(sock, SHUT_RDWR);
                ASMAtomicWriteBool(&pIter->fSendFailed, true);

                VRDPPktRelease(&pIter->pkt);
                return VERR_NOT_SUPPORTED;
            }

            if (Server()->IsShutdown() && cSendErrors > 5)
            {
                /* The server must shut down, disconnect the client. */
                VRDPLOGREL(("TCP server shutdown while sending data.\n"));

                VRDPPktRelease(&pIter->pkt);
                return VERR_NOT_SUPPORTED;
            }

            /* Just try to send again. */
            continue;
        }

        Assert(cb > 0);
        cSendErrors = 0;

        /* Take the sent data into account. */
        cbData -= cb;
        pu8Data += cb;

        ASMAtomicAddU64(&pIter->u64BytesSent, cb);
    } while (cbData);

    /* Packet was sent to the client, so there is no need for a keep alive event in near future. */
    ASMAtomicWriteU64(&pIter->u64LastActivity, VRDPTimeMilliTS());

    VRDPPktRelease(&pIter->pkt);
    return VINF_SUCCESS;
}

uint64_t VRDPTCPTransport::BytesRecv(VRDPTRANSPORTID id)
{
    uint64_t u64 = 0;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        TCPTRANSPORTIDCTX *pIter;
        RTListForEach(&m_listConnections, pIter, TCPTRANSPORTIDCTX, nodeConnection)
        {
            if (id == pIter->id)
            {
                u64 = ASMAtomicReadU64(&pIter->u64BytesRecv);
                break;
            }
        }

        Assert(pIter);

        VRDPLock::Unlock(m_pLock);
    }

    return u64;
}

uint64_t VRDPTCPTransport::BytesSent(VRDPTRANSPORTID id)
{
    uint64_t u64 = 0;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        TCPTRANSPORTIDCTX *pIter;
        RTListForEach(&m_listConnections, pIter, TCPTRANSPORTIDCTX, nodeConnection)
        {
            if (id == pIter->id)
            {
                u64 = ASMAtomicReadU64(&pIter->u64BytesSent);
                break;
            }
        }

        Assert(pIter);

        VRDPLock::Unlock(m_pLock);
    }

    return u64;
}

uint64_t VRDPTCPTransport::BytesRecvTotal(void)
{
    uint64_t u64 = 0;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        TCPTRANSPORTIDCTX *pIter;
        RTListForEach(&m_listConnections, pIter, TCPTRANSPORTIDCTX, nodeConnection)
        {
            u64 += ASMAtomicReadU64(&pIter->u64BytesRecv);
        }

        VRDPLock::Unlock(m_pLock);
    }

    return u64;
}

uint64_t VRDPTCPTransport::BytesSentTotal(void)
{
    uint64_t u64 = 0;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        TCPTRANSPORTIDCTX *pIter;
        RTListForEach(&m_listConnections, pIter, TCPTRANSPORTIDCTX, nodeConnection)
        {
            u64 += ASMAtomicReadU64(&pIter->u64BytesSent);
        }

        VRDPLock::Unlock(m_pLock);
    }

    return u64;
}


/*
 * Flash domain policy support for the Flash RDP client.
 */

static const char sachFlashPolicyFileRequest[] =
    "<policy-file-request/>";

/* Simple domain police which enables connection to the VRDP server port from anywhere. */
static const char sachFlashPolicy[] =
    "<?xml version=\"1.0\"?>"
    "<!DOCTYPE cross-domain-policy SYSTEM \"/xml/dtds/cross-domain-policy.dtd\">"
    "<cross-domain-policy>"
    "   <allow-access-from domain=\"*\" to-ports=\"%d\" />"
    "</cross-domain-policy>";

static int recvWithTimeout (VRDPSOCKET sock, uint8_t *pu8ToRecv, int cbToRecv, int *pcbRecv, unsigned uSeconds)
{
    fd_set readfds;
    fd_set errorfds;
    FD_ZERO (&readfds);
    FD_ZERO (&errorfds);

    FD_SET (sock, &readfds);
    FD_SET (sock, &errorfds);

    struct timeval timeout;
    timeout.tv_sec = uSeconds;
    timeout.tv_usec = 0;

    int c = select (sock + 1, &readfds, NULL, &errorfds, &timeout);

    if (c <= 0 || !FD_ISSET (sock, &readfds))
    {
        TCPLOG(("recvWithTimeout: failed to select, c = %d!!!\n", c));
        return VERR_TIMEOUT;
    }

    c = recv (sock, (char *)pu8ToRecv, cbToRecv, 0);

    if (c <= 0)
    {
        TCPLOG(("recvWithTimeout: failed to recv, c = %d!!!\n", c));
        return VERR_NOT_SUPPORTED;
    }

    *pcbRecv = c;

    return VINF_SUCCESS;
}

static int sendWithTimeout (VRDPSOCKET sock, const uint8_t *pu8ToSend, int cbToSend, unsigned uSeconds)
{
    do
    {
        fd_set writefds;
        FD_ZERO (&writefds);

        FD_SET (sock, &writefds);

        struct timeval timeout;
        timeout.tv_sec = uSeconds;
        timeout.tv_usec = 0;

        int c = select (sock + 1, NULL, &writefds, NULL, &timeout);

        if (c <= 0 || !FD_ISSET (sock, &writefds))
        {
            TCPLOG(("sendWithTimeout: select failed, c = %d!!!\n", c));
            return VERR_NOT_SUPPORTED;
        }

        c = send (sock, (const char *)pu8ToSend, cbToSend, 0);

        if (c < 0)
        {
            TCPLOG(("sendWithTimeout: failed to send, c = %d!!!\n", c));
            return VERR_NOT_SUPPORTED;
        }

        cbToSend -= c;
        pu8ToSend += c;
    } while (cbToSend);

    return VINF_SUCCESS;
}

static int processFirstPacket(VRDPSOCKET sock, uint16_t u16BindPort, uint8_t *pu8FirstPacketByte)
{
    /* Get 1 (one) byte, which is enough to distinguish between RDP and Flash request.
     * Note: the socket is in non-blocking mode, so use select.
     */
    int cbRecv = 0;
    uint8_t u8FirstPacketByte = 0;

    int rc = recvWithTimeout (sock, &u8FirstPacketByte, 1, &cbRecv, 60);

    if (RT_FAILURE (rc))
    {
        TCPLOG(("processFirstPacket: failed to read the first byte!!!\n"));
        return VERR_TIMEOUT;
    }

    /* RDP connection starts with a standard ISO version 3 packet.
     * Check if the Flash client is trying to get the policy file.
     */
    if (u8FirstPacketByte == 0x03)
    {
        /* Continue processing in standard RDP mode.*/
        *pu8FirstPacketByte = u8FirstPacketByte;
        return VINF_SUCCESS;
    }

    /* The data buffer does not contain 0x03 as the first byte,
     * so it cant be a standard RDP connection request.
     */

    rc = VINF_VRDP_OPERATION_COMPLETED; /* This will close the new TCP connection. */

    if (u8FirstPacketByte == sachFlashPolicyFileRequest[0])
    {
        /* The first byte is '<' here, try to read entire request.
         * Flash Player sends the string <policy-file-request/> followed by a NULL byte
         *
         * Allocate a buffer enough to store either request or reply + space for %d->port number.
         */
        uint8_t au8Buffer[RT_MAX(sizeof (sachFlashPolicyFileRequest), sizeof (sachFlashPolicy) + 32)];

        uint8_t *pu8ToRecv = &au8Buffer[0];
        int cbToRecv = sizeof (sachFlashPolicyFileRequest);

        /* Already have the first byte. */
        *pu8ToRecv++ = u8FirstPacketByte;
        cbToRecv--;

        while (cbToRecv > 0)
        {
            rc = recvWithTimeout (sock, pu8ToRecv, cbToRecv, &cbRecv, 60);

            if (RT_FAILURE (rc))
            {
                TCPLOG(("processFirstPacket: failed to read the first packet: rc = %Rrc\n", rc));
                return rc;
            }

            TCPLOG(("policy file request cbRecv %d\n", cbRecv));
            pu8ToRecv += cbRecv;
            cbToRecv -= cbRecv;
        }

        if (memcmp (au8Buffer, sachFlashPolicyFileRequest, sizeof (sachFlashPolicyFileRequest)) != 0)
        {
            /* It's not a policy file request and not a valid RDP packet. */
            TCPLOG(("processFirstPacket: invalid packet!!!\n"));
            return VERR_NOT_SUPPORTED;
        }

        /* It is the policy file request. Send the reply. */
        size_t cbPolicy = RTStrPrintf ((char *)&au8Buffer[0], sizeof(au8Buffer), sachFlashPolicy, u16BindPort);
        TESTLOG(("%s\n", au8Buffer));

        rc = sendWithTimeout (sock, au8Buffer, (int)(cbPolicy + 1), 60);
        TCPLOG(("policy file request send rc %Rrc\n", rc));

        if (RT_SUCCESS (rc))
        {
            /* Send data, the socket is not needed anymore. */
            shutdown (sock, SHUT_RDWR);

            /* Tell the caller that there is no need to continue. */
            rc = VINF_VRDP_OPERATION_COMPLETED;
        }
    }

    return rc;
}


/*
 * Client connection methods.
 */

/* Called when a connection is deleted. */
static void tcpTransportIdCtxDelete(VRDPPKT *pPkt)
{
    TCPTRANSPORTIDCTX *pCtx = (TCPTRANSPORTIDCTX *)pPkt;
    socketClose(pCtx->sock);
    tlsClose(pCtx);
}

/** @thread INPUT */
int VRDPTCPTransport::clientNewConnection(TCPSOCKETLISTEN *pSocketListen)
{
    TCPLOG(("New connection IPv%s\n", pSocketListen->addressFamily == AF_INET? "4": "6"));

    union
    {
        sockaddr_in client;
        sockaddr_in6 client6;
    } u;

    socklen_t addrlen;
    struct sockaddr *addr;

    if (pSocketListen->addressFamily == AF_INET6)
    {
        addrlen = sizeof(u.client6);
        addr = (struct sockaddr *)&u.client6;
    }
    else
    {
        addrlen = sizeof(u.client);
        addr = (struct sockaddr *)&u.client;
    }

    memset(addr, 0, addrlen);

    VRDPSOCKET sock = accept(pSocketListen->socketListen, addr, &addrlen);
    if (sock == NIL_VRDPSOCKET)
    {
        TCPLOG(("accept failed!!!\n"));
        return VERR_NOT_SUPPORTED;
    }

    /* Try to set socket options. */
    int rc = socketSetNonBlock(sock);
    if (RT_FAILURE(rc))
    {
        TCPLOG(("Failed to set clients socket in non block!!!\n"));
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        socketSetNoDelay(sock, 1);

        if (m_cSocketsClient + m_cSocketsListen >= FD_SETSIZE)
        {
            TCPLOG(("Too many sockets.\n"));
            rc = VERR_NOT_SUPPORTED;
        }
    }

    if (RT_FAILURE(rc))
    {
        shutdown(sock, SHUT_RDWR);
        socketClose(sock);
        return rc;
    }

    /* Special handling for Flash client. */
    uint8_t u8FirstPacketByte = 0;
    rc = processFirstPacket(sock, m_u16BindPort, &u8FirstPacketByte);

    /* Only VINF_SUCCESS means that we have to continue with RDP. */
    if (rc != VINF_SUCCESS)
    {
        /* Drop the connection. processFirstPacket already called 'shutdown'. */
        socketClose(sock);
        return rc;
    }

    /* Create the transport instance data. */
    TCPTRANSPORTIDCTX *pCtx = (TCPTRANSPORTIDCTX *)VRDPPktAlloc(0, sizeof(TCPTRANSPORTIDCTX),
                                                                tcpTransportIdCtxDelete, sizeof(uint64_t));

    if (pCtx)
    {
        VRDPPktAddRef(&pCtx->pkt); /* In use by the INPUT thread. */

        pCtx->u64BytesRecv = 0;
        pCtx->u64BytesSent = 0;
        pCtx->u64LastActivity = VRDPTimeMilliTS();
        pCtx->id = ++m_idSource;
        if (pCtx->id == NIL_VRDPTRANSPORTID)
        {
            pCtx->id = ++m_idSource;
        }
        pCtx->sock = sock;
        pCtx->fSendFailed = false;
        pCtx->fFirstPacketByte = true;
        pCtx->u8FirstPacketByte = u8FirstPacketByte;

        /* Inform the server about the new connection. */
        rc = Server()->TransportConnect(pCtx->id);

        if (RT_SUCCESS(rc))
        {
            /* Add the context to the list. Take the lock because OUTPUT may access the list. */
            if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
            {
                RTListPrepend(&m_listConnections, &pCtx->nodeConnection);
                VRDPLock::Unlock(m_pLock);
            }
            else
            {
                rc = VERR_NOT_SUPPORTED;
            }

            if (RT_SUCCESS(rc))
            {
                VRDPLOGREL(("Connection opened (IPv%s): %d\n",
                            pSocketListen->addressFamily == AF_INET? "4": "6", pCtx->id));
            }
            else
            {
                /* Could not add the connection to the list. */
                Server()->TransportDisconnect(pCtx->id);
            }
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        m_cSocketsClient++;
    }
    else
    {
        if (pCtx)
        {
            VRDPPktRelease(&pCtx->pkt);
        }

        shutdown(sock, SHUT_RDWR);
        socketClose(sock);
    }

    return rc;
}

void VRDPTCPTransport::clientDisconnect(TCPTRANSPORTIDCTX *pCtx)
{
    VRDPLOGREL(("Connection closed: %d\n", pCtx->id));

    TCPLOG(("excluding %p, id %d\n", pCtx, pCtx->id));

    shutdown(pCtx->sock, SHUT_RDWR);

    /* Exclude the connection from the list. */
    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        RTListNodeRemove(&pCtx->nodeConnection);
        VRDPLock::Unlock(m_pLock);
    }
    else
    {
        /* Should not happen. The structure remains in the list. */
        AssertFailed();

        VRDPPktAddRef(&pCtx->pkt); /* Make sure it is not freed. */
    }

    Server()->TransportDisconnect(pCtx->id);

    pCtx->id = NIL_VRDPTRANSPORTID; /* Mark as invalid. */

    VRDPPktRelease(&pCtx->pkt);

    m_cSocketsClient--;
}

TCPTRANSPORTIDCTX *VRDPTCPTransport::ctxById(VRDPTRANSPORTID id)
{
    TCPTRANSPORTIDCTX *pIter;
    RTListForEach(&m_listConnections, pIter, TCPTRANSPORTIDCTX, nodeConnection)
    {
        if (id == pIter->id)
        {
            return pIter;
        }
    }

    AssertFailed ();
    return NULL;
}


/*
 * TLS support.
 */
/** @thread INPUT */
/* virtual */ int VRDPTCPTransport::TLSInit(VRDPTRANSPORTID id)
{
    TCPTRANSPORTIDCTX *pCtx = ctxById(id);
    if (!pCtx)
    {
        AssertFailed();
        return VERR_NOT_SUPPORTED;
    }

    Assert(!pCtx->TLSData.fTLSEnabled);

    /* Create TLS instance. */
    int rc = RTCrSslCreate(&pCtx->TLSData.hSsl, 0);
    AssertLogRelRCReturn(rc, rc);

    /* Configure the TLS instance. */
    uint32_t cbOut    = 0;
    char    *pszValue = NULL;
    rc = Server()->QueryFeature("Property/Security/ServerCertificate", &pszValue, &cbOut);
    if (RT_SUCCESS(rc))
    {
        rc = RTCrSslSetCertificateFile(pCtx->TLSData.hSsl, pszValue, RTCRSSL_FILE_F_PEM);
        if (RT_SUCCESS(rc))
        {
            VRDPMemFree(pszValue);
            pszValue = NULL;
            cbOut = 0;
            rc = Server()->QueryFeature("Property/Security/ServerPrivateKey", &pszValue, &cbOut);
            if (RT_SUCCESS(rc))
            {
                rc = RTCrSslSetPrivateKeyFile(pCtx->TLSData.hSsl, pszValue, RTCRSSL_FILE_F_PEM);
                if (RT_SUCCESS(rc))
                {
                    VRDPMemFree(pszValue);
                    pszValue = NULL;
                    cbOut = 0;
                    rc = Server()->QueryFeature("Property/Security/CACertificate", &pszValue, &cbOut);
                    if (RT_SUCCESS(rc))
                    {
                        if (*pszValue)
                            rc = RTCrSslLoadTrustedRootCerts(pCtx->TLSData.hSsl, pszValue, NULL);
                        VRDPMemFree(pszValue);
                        if (RT_SUCCESS(rc))
                        {
                            rc = RTCrSslSetNoPeerVerify(pCtx->TLSData.hSsl);
                            if (RT_SUCCESS(rc))
                            {
                                /** @todo Optionally set min protocol level? */
                                pCtx->TLSData.hSession = NIL_RTCRSSLSESSION;
                                return rc;
                            }
                            AssertLogRelRC(rc);
                        }
                        else
                            VRDPLOGRELLIMIT(8, ("VRDP: failed to access the server CA certificates file '%s': %Rrc\n",
                                                pszValue, rc));
                    }
                    else
                        VRDPLOGRELLIMIT(8, ("VRDP: failed to query the server CA certificates file: %Rrc\n", rc));
                }
                else
                {
                    VRDPLOGRELLIMIT(8, ("VRDP: failed to access the server PrivateKey file '%s': %Rrc\n", pszValue, rc));
                    VRDPMemFree(pszValue);
                }
            }
            else
                VRDPLOGRELLIMIT(8, ("VRDP: failed to query the server PrivateKey file: %Rrc\n", rc));
        }
        else
        {
            VRDPLOGRELLIMIT(8, ("VRDP: failed to access the server certificate file '%s': %Rrc\n", pszValue, rc));
            VRDPMemFree(pszValue);
        }
    }
    else
        VRDPLOGRELLIMIT(8, ("VRDP: failed query the server certificate file: %Rrc\n", rc));

    RTCrSslRelease(pCtx->TLSData.hSsl);
    pCtx->TLSData.hSsl = NIL_RTCRSSL;

    return rc;
}

/** @thread INPUT */
/* virtual */ int VRDPTCPTransport::TLSStart(VRDPTRANSPORTID id)
{
    TCPTRANSPORTIDCTX *pCtx = ctxById(id);
    if (!pCtx)
    {
        AssertFailed();
        return VERR_NOT_SUPPORTED;
    }

    Assert(!pCtx->TLSData.fTLSEnabled);
    int rc = RTCrSslCreateSessionForNativeSocket(pCtx->TLSData.hSsl, pCtx->sock,
                                                 RTCRSSLSESSION_F_NON_BLOCKING, &pCtx->TLSData.hSession);
    if (RT_SUCCESS(rc))
    {
        rc = RTCrSslSessionAccept(pCtx->TLSData.hSession, 0);
        while (rc == VERR_TRY_AGAIN)
        {
            VRDPThreadSleep(10);
            rc = RTCrSslSessionAccept(pCtx->TLSData.hSession, 0);
        }
    }

    if (RT_SUCCESS(rc))
    {
        pCtx->TLSData.fTLSEnabled = true;

        VRDPLOGREL(("Using TLS connection. Protocol: %s\n", RTCrSslSessionGetVersion(pCtx->TLSData.hSession)));

        char szBuf[512];
        if (RT_SUCCESS(RTCrSslSessionGetCertIssuerNameAsString(pCtx->TLSData.hSession, szBuf, sizeof(szBuf), NULL)))
            VRDPLOGREL(("Server certificate: [%s]\n", szBuf));
    }
    else
    {
        VRDPLOGREL(("Failed to establish a TLS connection (%Rrc).\n", rc));

        tlsClose(pCtx);
    }

    return rc;
}

/** @thread INPUT */
static void tlsClose(TCPTRANSPORTIDCTX *pCtx)
{
    RTCrSslSessionRelease(pCtx->TLSData.hSession);
    pCtx->TLSData.hSession = NIL_RTCRSSLSESSION;

    RTCrSslRelease(pCtx->TLSData.hSsl);
    pCtx->TLSData.hSsl = NIL_RTCRSSL;
    pCtx->TLSData.fTLSEnabled = false;
    pCtx->TLSData.fPending = false;
}


int TCPTransportCreate(VRDPTransport **ppTransport,
                       VRDPTransportServer *pServer,
                       const char *pszAddress, unsigned uPort,
                       const char *pszPortRange,
                       const char *pszUnixSocketPath)
{
    VRDPTransport *pTransport = new VRDPTCPTransport(pServer, pszAddress, uPort, pszPortRange, pszUnixSocketPath);

    if (!pTransport)
    {
        return VERR_NO_MEMORY;
    }

    *ppTransport = pTransport;
    return VINF_SUCCESS;
}
