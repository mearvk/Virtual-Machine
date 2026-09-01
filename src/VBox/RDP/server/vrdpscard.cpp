/* $Id: vrdpscard.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
 */

/*
 * Copyright (C) 2011-2025 Oracle and/or its affiliates.
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

#include "rdpdr.h"
#include "vrdpserv.h"
#include "vrdpscard.h"
#include <iprt/utf16.h>

#ifdef VRDP_DEBUG_SCARD
#  define SCARDLOG(a) do { VRDPLOG(("SCARD::%s: ", __FUNCTION__)); VRDPLOG(a); } while (0)
#else
#  define SCARDLOG(a)
#endif /* VRDP_DEBUG_SCARD */

#ifdef DEBUG_sunlover
#undef SCARDLOG
#define SCARDLOG LogRel
#endif

#define SCARD_IOCTL_ESTABLISHCONTEXT 0x00090014
#define SCARD_IOCTL_RELEASECONTEXT 0x00090018
#define SCARD_IOCTL_ISVALIDCONTEXT 0x0009001C
#define SCARD_IOCTL_LISTREADERGROUPSA 0x00090020
#define SCARD_IOCTL_LISTREADERGROUPSW 0x00090024
#define SCARD_IOCTL_LISTREADERSA 0x00090028
#define SCARD_IOCTL_LISTREADERSW 0x0009002C
#define SCARD_IOCTL_INTRODUCEREADERGROUPA 0x00090050
#define SCARD_IOCTL_INTRODUCEREADERGROUPW 0x00090054
#define SCARD_IOCTL_FORGETREADERGROUPA 0x00090058
#define SCARD_IOCTL_FORGETREADERGROUPW 0x0009005C
#define SCARD_IOCTL_INTRODUCEREADERA 0x00090060
#define SCARD_IOCTL_INTRODUCEREADERW 0x00090064
#define SCARD_IOCTL_FORGETREADERA 0x00090068
#define SCARD_IOCTL_FORGETREADERW 0x0009006C
#define SCARD_IOCTL_ADDREADERTOGROUPA 0x00090070
#define SCARD_IOCTL_ADDREADERTOGROUPW 0x00090074
#define SCARD_IOCTL_REMOVEREADERFROMGROUPA 0x00090078
#define SCARD_IOCTL_REMOVEREADERFROMGROUPW 0x0009007C
#define SCARD_IOCTL_LOCATECARDSA 0x00090098
#define SCARD_IOCTL_LOCATECARDSW 0x0009009C
#define SCARD_IOCTL_GETSTATUSCHANGEA 0x000900A0
#define SCARD_IOCTL_GETSTATUSCHANGEW 0x000900A4
#define SCARD_IOCTL_CANCEL 0x000900A8
#define SCARD_IOCTL_CONNECTA 0x000900AC
#define SCARD_IOCTL_CONNECTW 0x000900B0
#define SCARD_IOCTL_RECONNECT 0x000900B4
#define SCARD_IOCTL_DISCONNECT 0x000900B8
#define SCARD_IOCTL_BEGINTRANSACTION 0x000900BC
#define SCARD_IOCTL_ENDTRANSACTION 0x000900C0
#define SCARD_IOCTL_STATE 0x000900C4
#define SCARD_IOCTL_STATUSA 0x000900C8
#define SCARD_IOCTL_STATUSW 0x000900CC
#define SCARD_IOCTL_TRANSMIT 0x000900D0
#define SCARD_IOCTL_CONTROL 0x000900D4
#define SCARD_IOCTL_GETATTRIB 0x000900D8
#define SCARD_IOCTL_SETATTRIB 0x000900DC
#define SCARD_IOCTL_ACCESSSTARTEDEVENT 0x000900E0
#define SCARD_IOCTL_LOCATECARDSBYATRA 0x000900E8
#define SCARD_IOCTL_LOCATECARDSBYATRW 0x000900EC
#define SCARD_IOCTL_READCACHEA 0x000900F0
#define SCARD_IOCTL_READCACHEW 0x000900F4
#define SCARD_IOCTL_WRITECACHEA 0x000900F8
#define SCARD_IOCTL_WRITECACHEW 0x000900FC
#define SCARD_IOCTL_GETTRANSMITCOUNT 0x00090100
#define SCARD_IOCTL_RELEASETARTEDEVENT 0x000900E4

#define SCARD_AUTOALLOCATE 0xFFFFFFFF

#define SCARD_SCOPE_USER 0x00000000
#define SCARD_SCOPE_TERMINAL 0x00000001
#define SCARD_SCOPE_SYSTEM 0x00000002


#pragma pack(1)

/*
 * NDR structures.
 */

typedef struct RPCEHDR
{
    uint8_t u8Version;     /* Must be 1. */
    uint8_t u8Endianness;  /* 0x10 - le, 0x00 - be. */
    uint16_t u16Length;    /* Must be 8. */
    uint32_t u32Filler;    /* Must be 0xcccccccc */
} RPCEHDR;

typedef struct RPCETYPEHDR
{
    uint32_t u32Length;    /* Does not include the header, includes the padding. */
    uint32_t u32Filler;    /* Must be 0x0 */
} RPCETYPEHDR;


/*
 * Smartcard function parameters and return structures.
 */

typedef struct SCREDIR_SCARDCONTEXT
{
    uint32_t cbCtx;
    uint8_t  au8Ctx[16];
} SCREDIR_SCARDCONTEXT;

typedef struct SCREDIR_SCARDHANDLE
{
    SCREDIR_SCARDCONTEXT Context;
    uint32_t cbHandle;
    uint8_t  pbHandle[16];
} SCREDIR_SCARDHANDLE;

typedef struct SCSCardIO_Request
{
    uint32_t dwProtocol;
    uint32_t cbExtraBytes;
    uint8_t  pbExtraBytes[VRDE_SCARD_MAX_PCI_DATA];
} SCSCardIO_Request;

/*
 * *_Call
 */

typedef struct SCEstablishContext_Call
{
    uint32_t dwScope;
} SCEstablishContext_Call;

typedef struct SCListReaders_Call
{
    SCREDIR_SCARDCONTEXT Context;
} SCListReaders_Call;

typedef struct SCContext_Call
{
    SCREDIR_SCARDCONTEXT Context;
} SCContext_Call;

typedef struct SCReaderState_Common_Call
{
    uint32_t dwCurrentState;
    uint32_t dwEventState;
    uint32_t cbAtr;
    uint8_t rgbAtr[VRDE_SCARD_MAX_ATR_LENGTH];
} SCReaderState_Common_Call;

typedef struct SCReaderStateW
{
    char *szReader; /* The UTF8 reader name. */
    SCReaderState_Common_Call Common;
} SCReaderStateW;

typedef struct SCGetStatusChangeW_Call
{
    SCREDIR_SCARDCONTEXT Context;
    uint32_t dwTimeOut;
    uint32_t cReaders;
    SCReaderStateW rgReaderStates[11];
} SCGetStatusChangeW_Call;

typedef struct SCConnect_Common
{
    SCREDIR_SCARDCONTEXT Context;
    uint32_t dwShareMode;
    uint32_t dwPreferredProtocols;
} SCConnect_Common;

typedef struct SCConnectW_Call
{
    char *szReader; /* The UTF8 reader name. */
    SCConnect_Common Common;
} SCConnectW_Call;

typedef struct SCReconnect_Call
{
    SCREDIR_SCARDHANDLE hCard;
    uint32_t dwShareMode;
    uint32_t dwPreferredProtocols;
    uint32_t dwInitialization;
} SCReconnect_Call;

typedef struct SCHCardAndDisposition_Call
{
    SCREDIR_SCARDHANDLE hCard;
    uint32_t dwDisposition;
} SCHCardAndDisposition_Call;

typedef struct SCState_Call
{
    SCREDIR_SCARDHANDLE hCard;
    uint32_t fpbAtrIsNULL;
    uint32_t cbAtrLen;
} SCState_Call;

typedef struct SCStatus_Call
{
    SCREDIR_SCARDHANDLE hCard;
    uint32_t fmszReaderNamesIsNULL;
    uint32_t cchReaderLen;
    uint32_t cbAtrLen; /* Docs are wrong, this is not ignored. Set to sizeof(SCStatus_Return::pbAtr). */
} SCStatus_Call;

typedef struct SCTransmit_Call
{
    SCREDIR_SCARDHANDLE hCard;
    SCSCardIO_Request ioSendPci;
    uint32_t cbSendLength;
    uint8_t *pbSendBuffer;
    SCSCardIO_Request pioRecvPci;
    uint32_t fpbRecvBufferIsNULL;
    uint32_t cbRecvLength; /* Can't use SCARD_AUTOALLOCATE here, the client crashes. */
} SCTransmit_Call;

typedef struct SCControl_Call
{
    SCREDIR_SCARDHANDLE hCard;
    uint32_t dwControlCode;
    uint32_t cbInBufferSize;
    uint8_t *pvInBuffer;
    uint32_t fpvOutBufferIsNULL;
    uint32_t cbOutBufferSize;
} SCControl_Call;

typedef struct SCGetAttrib_Call
{
    SCREDIR_SCARDHANDLE hCard;
    uint32_t dwAttrId;
    uint32_t fpbAttrIsNULL;
    uint32_t cbAttrLen; /* Can't use SCARD_AUTOALLOCATE here, the client crashes. */
} SCGetAttrib_Call;

typedef struct SCSetAttrib_Call
{
    SCREDIR_SCARDHANDLE hCard;
    uint32_t dwAttrId;
    uint32_t cbAttrLen;
    uint8_t *pbAttr;
} SCSetAttrib_Call;

/*
 * *_Return
 */

typedef struct SCLong_Return
{
    uint32_t ReturnCode;
} SCLong_Return;

typedef struct SCEstablishContext_Return
{
    uint32_t ReturnCode;
    SCREDIR_SCARDCONTEXT Context;
} SCEstablishContext_Return;

typedef struct SCListReaders_Return
{
    uint32_t ReturnCode;
    uint32_t cBytes;    /* Size of the msz memory block. */
    char *msz;          /* UTF8 multistring of readers. */
} SCListReaders_Return;

typedef struct SCReaderState_Return
{
    uint32_t dwCurrentState;
    uint32_t dwEventState;
    uint32_t cbAtr;
    uint8_t rgbAtr[VRDE_SCARD_MAX_ATR_LENGTH];
} SCReaderState_Return;

typedef struct SCGetStatusChange_Return
{
    uint32_t ReturnCode;
    uint32_t cReaders;
    SCReaderState_Return rgReaderStates[10];
} SCGetStatusChange_Return;

typedef struct SCConnect_Return
{
    uint32_t ReturnCode;
    SCREDIR_SCARDHANDLE hCard;
    uint32_t dwActiveProtocol;
} SCConnect_Return;

typedef struct SCReconnect_Return
{
    uint32_t ReturnCode;
    uint32_t dwActiveProtocol;
} SCReconnect_Return;

typedef struct SCState_Return
{
    uint32_t ReturnCode;
    uint32_t dwState;
    uint32_t dwProtocol;
    uint32_t cbAtrLen;
    uint8_t rgAtr[VRDE_SCARD_MAX_ATR_LENGTH];
} SCState_Return;

typedef struct SCStatus_Return
{
    uint32_t ReturnCode;
    uint32_t cBytes;      /* Size of the mszReaderNames memory block. */
    char *mszReaderNames; /* UTF8 multistring of the reader names. */
    uint32_t dwState;
    uint32_t dwProtocol;
    uint8_t pbAtr[VRDE_SCARD_MAX_ATR_LENGTH];
    uint32_t cbAtrLen;
} SCStatus_Return;

typedef struct SCTransmit_Return
{
    uint32_t ReturnCode;
    SCSCardIO_Request pioRecvPci;
    uint32_t cbRecvLength;
    uint8_t *pbRecvBuffer;
} SCTransmit_Return;

typedef struct SCControl_Return
{
    uint32_t ReturnCode;
    uint32_t cbOutBufferSize;
    uint8_t *pvOutBuffer;
} SCControl_Return;

typedef struct SCGetAttrib_Return
{
    uint32_t ReturnCode;
    uint32_t cbAttrLen;
    uint8_t *pbAttr;
} SCGetAttrib_Return;


/*
 * VRDP smartcard implementation structures.
 */

/* Context creation status.
 *
 * Create 2 contexts: for GetStatusChange and for other functions.
 * Some clients hold a context lock, so a waiting GetStatusChange
 * blocks all other requests.
 *
 * Context creation order: GetStatusChange, operation
 * Context deletion order: operation, GetStatusChange
 */
typedef enum SCCONTEXTSTATUS
{
    SC_CONTEXT_NULL,
    SC_CONTEXT_REQUESTING,
    SC_CONTEXT_ESTABLISHED,
    SC_CONTEXT_RELEASING
} SCCONTEXTSTATUS;

/* The server context for smartcard devices. */
struct VRDPSCARDDEVICE
{
    RTLISTNODE NodeSCardDevice;

    int volatile cRefs;

    uint32_t u32ClientId;
    uint32_t u32DeviceId;

    SCCONTEXTSTATUS enmCtxStatus;
    SCREDIR_SCARDCONTEXT context;                /* Operations context. */

    SCCONTEXTSTATUS enmCtxStatusGetStatusChange;
    SCREDIR_SCARDCONTEXT contextGetStatusChange; /* GetStatusChange context. */
};

/* The smartcard IOCTL structure, which extends the RDPDR structure. */
typedef struct SCIOCTL
{
    RDPDRIOCTL io;
    void *pvUser;
    uint32_t u32VRDEFunction;
} SCIOCTL;

#pragma pack()


static const RPCEHDR sRpceHdr =
{
    0x01,
    0x10,
    RT_H2LE_U16(0x0008),
    0xcccccccc
};


/*
 * External wrappers for the VRDPServer Smartcard API.
 */

static DECLCALLBACK(int) VRDESCardRequest(HVRDESERVER hServer,
                                          void *pvUser,
                                          uint32_t u32Function,
                                          const void *pvData,
                                          uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    VRDPServer *pServer = (VRDPServer *)hServer;

    VRDPAPILOG(("%p\n", pServer));

    if (pServer)
    {
        rc = pServer->SCard()->SCardRequest(pvUser, u32Function, pvData, cbData);
    }

    return rc;
}

static VRDESCARDINTERFACE gScardInterface =
{
    { 1, sizeof (VRDESCARDINTERFACE) },

    VRDESCardRequest
};


/*
 * The VRDP server smartcard implementation.
 */

VRDPSCard::VRDPSCard(VRDPServer *pServer)
    :
    m_pServer (pServer),
    m_fShutdown (false)
{
    RT_ZERO(m_interface);
    RTListInit(&m_ListSCardDevices);
    VRDPLock::Create("SCard", &m_pLock);
}

/* virtual */ VRDPSCard::~VRDPSCard ()
{
    VRDPLock::Delete(&m_pLock);
}

/* Helper to get the interface method table, called from VRDPServer::GetInterface */
int VRDPSCard::GetInterfaceSCard(VRDEINTERFACEHDR *pInterface, const VRDEINTERFACEHDR *pCallbacks, void *pvContext)
{
    int rc = VINF_SUCCESS;

    if (   pInterface->u64Version == 1
        && pInterface->u64Size == sizeof (VRDESCARDINTERFACE)
        && pCallbacks->u64Version == 1
        && pCallbacks->u64Size == sizeof (VRDESCARDCALLBACKS))
    {
        /* Interface version 1 is requested. */
        VRDESCARDINTERFACE *p = (VRDESCARDINTERFACE *)pInterface;

        m_interface.callbacks = *(VRDESCARDCALLBACKS *)pCallbacks;
        m_interface.pvContext = pvContext;

        *p = gScardInterface;
    }
    else
    {
        rc = VERR_VERSION_MISMATCH;
    }

    return rc;
}

int VRDPSCard::SCardInitialize(void)
{
    SCARDLOG(("SCardInitialize\n"));

    int rc = scInitialize();

    /*
     * Since VRDPSCard is a sub object of the VRDPServer, the server will call
     * VRDPSCard methods for RDPDR notifications.
     *
     * @todo Here it is possible to register a RDPDR listener. But currently VRDPSCard
     * is the only user of RDPDR, so there is no immediate need for a dynamic RDPDR
     * listeners implementation.
     */

    if (RT_FAILURE(rc))
    {
        scShutdown();
    }

    return rc;
}

void VRDPSCard::SCardShutdown(void)
{
    SCARDLOG(("SCardShutdown\n"));

    scShutdown();
}

void VRDPSCard::scShutdown(void)
{
    /* This is called when the server did not close RDP connection yet.
     * So try to close contexts of every scard RDPDR device.
     */
    m_fShutdown = true;

    bool fWait = false;

    VRDPSCARDDEVICE *pIter = NULL;

    /* Check if the input thread exists at all. */
    RTTHREAD pInputThread = m_pServer->InputThread();

    if (pInputThread != NIL_RTTHREAD)
    {
        /* The input thread exists, so close all contexts. */
        RTThreadUserReset(pInputThread);

        SCContext_Call parms;

        pIter = scDeviceFirst();
        while (pIter)
        {
            if (pIter->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
            {
                Assert(pIter->enmCtxStatusGetStatusChange == SC_CONTEXT_ESTABLISHED);

                SCARDLOG(("scShutdown: closing context %p: [%d,%d]\n",
                          pIter, pIter->u32ClientId, pIter->u32DeviceId));

                pIter->enmCtxStatus = SC_CONTEXT_RELEASING;

                parms.Context = pIter->context;
                scSubmitIOCTL(SCARD_IOCTL_RELEASECONTEXT, pIter, &parms, sizeof(parms),
                              &pIter->context, 0);

                /* No wait because only GetStatusChange will be waited. */
            }

            if (pIter->enmCtxStatusGetStatusChange == SC_CONTEXT_ESTABLISHED)
            {
                SCARDLOG(("scShutdown: closing GetStatusChange context %p: [%d,%d]\n",
                          pIter, pIter->u32ClientId, pIter->u32DeviceId));

                pIter->enmCtxStatusGetStatusChange = SC_CONTEXT_RELEASING;

                parms.Context = pIter->contextGetStatusChange;
                scSubmitIOCTL(SCARD_IOCTL_RELEASECONTEXT, pIter, &parms, sizeof(parms),
                              &pIter->contextGetStatusChange, 0);

                fWait = true;
            }

            pIter = scDeviceNext(pIter);
        }

        if (fWait)
        {
            /* Wait a bit for the client replies. @todo for multiple clients. */
            RTThreadUserWaitNoResume(pInputThread, 5000);
        }
    }

    /* Delete m_ListSCardDevices */
    VRDPSCARDDEVICE *pNext;
    RTListForEachSafe(&m_ListSCardDevices, pIter, pNext, VRDPSCARDDEVICE, NodeSCardDevice)
    {
        SCARDLOG(("scShutdown: remove %p: [%d,%d]\n",
                  pIter, pIter->u32ClientId, pIter->u32DeviceId));

        SCardDetach(pIter->u32ClientId, pIter->u32DeviceId);
    }
}

int VRDPSCard::scInitialize(void)
{
    m_fShutdown = false;
    RTListInit(&m_ListSCardDevices);
    /** @todo */
    return VINF_SUCCESS;
}

static int32_t scDeviceAddRef(VRDPSCARDDEVICE *p)
{
    int c = ASMAtomicIncS32(&p->cRefs);
#ifdef DEBUG_sunlover
    LogRel(("VRDPSCARDDEVICE %p -> %d\n", p, c));
#endif
    return c;
}

static void scDeviceRelease(VRDPSCARDDEVICE *p)
{
    int32_t c = ASMAtomicDecS32(&p->cRefs);
#ifdef DEBUG_sunlover
    LogRel(("VRDPSCARDDEVICE %p -> %d\n", p, c));
#endif
    Assert(c >= 0);
    if (c == 0)
    {
#ifdef DEBUG_sunlover
        LogRel(("VRDPSCARDDEVICE %p deleted\n", p));
#endif
        VRDPMemFree(p);
    }
}

VRDPSCARDDEVICE *VRDPSCard::scDeviceFind(uint32_t u32ClientId, uint32_t u32DeviceId)
{
    SCARDLOG(("scDeviceFind: [%d,%d]\n",
               u32ClientId, u32DeviceId));

    VRDPSCARDDEVICE *pDev = NULL;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        VRDPSCARDDEVICE *pIter;
        RTListForEach(&m_ListSCardDevices, pIter, VRDPSCARDDEVICE, NodeSCardDevice)
        {
            if (   pIter->u32DeviceId == u32DeviceId
                && pIter->u32ClientId == u32ClientId)
            {
                pDev = pIter;
                scDeviceAddRef(pDev);
                break;
            }
        }

        VRDPLock::Unlock(m_pLock);
    }

    return pDev;
}

VRDPSCARDDEVICE *VRDPSCard::scDeviceFindByContext(const VRDESCARDCONTEXT *pContext)
{
    SCARDLOG(("scDeviceFindByContext\n"));

    VRDPSCARDDEVICE *pDev = NULL;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        VRDPSCARDDEVICE *pIter;
        RTListForEach(&m_ListSCardDevices, pIter, VRDPSCARDDEVICE, NodeSCardDevice)
        {
            if (   pIter->enmCtxStatus == SC_CONTEXT_ESTABLISHED
                && pContext->u32ContextSize == pIter->context.cbCtx
                && memcmp(pIter->context.au8Ctx, pContext->au8Context, pContext->u32ContextSize) == 0)
            {
                pDev = pIter;
                scDeviceAddRef(pDev);
                break;
            }
        }

        VRDPLock::Unlock(m_pLock);
    }

    return pDev;
}

VRDPSCARDDEVICE *VRDPSCard::scDeviceAdd(uint32_t u32ClientId, uint32_t u32DeviceId)
{
    /* The caller already checked the device is not in the list. */

    SCARDLOG(("scDeviceAdd: [%d,%d]\n",
               u32ClientId, u32DeviceId));

    VRDPSCARDDEVICE *pDev = (VRDPSCARDDEVICE *)VRDPMemAllocZ(sizeof(VRDPSCARDDEVICE));

    if (pDev)
    {
        pDev->u32ClientId = u32ClientId;
        pDev->u32DeviceId = u32DeviceId;

        if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
        {
            RTListAppend(&m_ListSCardDevices, &pDev->NodeSCardDevice);
            scDeviceAddRef(pDev);

            VRDPLock::Unlock(m_pLock);
        }
        else
        {
            VRDPMemFree(pDev);
            pDev = NULL;
        }
    }

    return pDev;
}

void VRDPSCard::scDeviceRemove(VRDPSCARDDEVICE *pDev)
{
    SCARDLOG(("scDeviceRemove: %p, [%d,%d]\n",
              pDev, pDev? pDev->u32ClientId: 0, pDev? pDev->u32DeviceId: 0));

    if (pDev)
    {
        if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
        {
            RTListNodeRemove(&pDev->NodeSCardDevice);

            VRDPLock::Unlock(m_pLock);

            scDeviceRelease(pDev);
        }
    }
}

VRDPSCARDDEVICE *VRDPSCard::scDeviceFirst(void)
{
    SCARDLOG(("scDeviceFirst\n"));

    VRDPSCARDDEVICE *pDev = NULL;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        pDev = RTListGetFirst(&m_ListSCardDevices, VRDPSCARDDEVICE, NodeSCardDevice);

        if (pDev)
        {
            scDeviceAddRef(pDev);
        }

        VRDPLock::Unlock(m_pLock);
    }

    return pDev;
}

VRDPSCARDDEVICE *VRDPSCard::scDeviceNext(VRDPSCARDDEVICE *pDev)
{
    SCARDLOG(("scDeviceNext\n"));

    VRDPSCARDDEVICE *pDevNext = NULL;

    if (RT_SUCCESS(VRDPLock::Lock(m_pLock)))
    {
        pDevNext = RTListGetNext(&m_ListSCardDevices, pDev, VRDPSCARDDEVICE, NodeSCardDevice);

        if (pDevNext)
        {
            scDeviceAddRef(pDevNext);
        }

        VRDPLock::Unlock(m_pLock);

        scDeviceRelease(pDev);
    }

    return pDevNext;
}

int VRDPSCard::SCardRequest(void *pvUser, uint32_t u32Function, const void *pvData, uint32_t cbData)
{
    RT_NOREF1(cbData);
    int rc = VINF_SUCCESS;

    SCARDLOG(("SCardRequest %p %d %p %d\n",
               pvUser, u32Function, pvData, cbData));

    switch (u32Function)
    {
        case VRDE_SCARD_FN_ESTABLISHCONTEXT:
        {
            VRDESCARDESTABLISHCONTEXTREQ *pReq = (VRDESCARDESTABLISHCONTEXTREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFind(pReq->u32ClientId, pReq->u32DeviceId);

            if (pDev)
            {
                if (   pDev->enmCtxStatusGetStatusChange == SC_CONTEXT_NULL
                    && pDev->enmCtxStatus == SC_CONTEXT_NULL)
                {
                    /*
                     * Submit a request to create the GetStatusChange context.
                     */

                    pDev->enmCtxStatusGetStatusChange = SC_CONTEXT_REQUESTING;

                    SCEstablishContext_Call parms;
                    parms.dwScope = SCARD_SCOPE_SYSTEM;
                    rc = scSubmitIOCTL(SCARD_IOCTL_ESTABLISHCONTEXT, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    /* This was already called by the user. It should release context first. */
                    rc = VERR_NOT_SUPPORTED;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_LISTREADERS:
        {
            VRDESCARDLISTREADERSREQ *pReq = (VRDESCARDLISTREADERSREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCListReaders_Call parms;
                    parms.Context = pDev->context;
                    rc = scSubmitIOCTL(SCARD_IOCTL_LISTREADERSW, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_RELEASECONTEXT:
        {
            VRDESCARDRELEASECONTEXTREQ *pReq = (VRDESCARDRELEASECONTEXTREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->Context);

            if (pDev)
            {
                SCContext_Call parms;

                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    Assert(pDev->enmCtxStatusGetStatusChange == SC_CONTEXT_ESTABLISHED);

                    pDev->enmCtxStatus = SC_CONTEXT_RELEASING;

                    parms.Context = pDev->context;
                    rc = scSubmitIOCTL(SCARD_IOCTL_RELEASECONTEXT, pDev, &parms, sizeof(parms),
                                       &pDev->context, 0);
                }

                if (pDev->enmCtxStatusGetStatusChange == SC_CONTEXT_ESTABLISHED)
                {
                    pDev->enmCtxStatusGetStatusChange = SC_CONTEXT_RELEASING;

                    parms.Context = pDev->contextGetStatusChange;
                    int rc2 = scSubmitIOCTL(SCARD_IOCTL_RELEASECONTEXT, pDev, &parms, sizeof(parms),
                                            pvUser, u32Function);
                    if (RT_SUCCESS(rc))
                    {
                        rc = rc2;
                    }
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_GETSTATUSCHANGE:
        {
            VRDESCARDGETSTATUSCHANGEREQ *pReq = (VRDESCARDGETSTATUSCHANGEREQ *)pvData;

            if (pReq->cReaders > VRDE_SCARD_MAX_READERS)
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCGetStatusChangeW_Call parms;
                    parms.Context = pDev->contextGetStatusChange;
                    parms.dwTimeOut = pReq->u32Timeout;
                    parms.cReaders = pReq->cReaders;

                    uint32_t i;
                    for (i = 0; i < pReq->cReaders; i++)
                    {
                        parms.rgReaderStates[i].szReader = pReq->aReaderStates[i].pszReader;
                        parms.rgReaderStates[i].Common.dwCurrentState = pReq->aReaderStates[i].u32CurrentState;
                        parms.rgReaderStates[i].Common.dwEventState = VRDE_SCARD_STATE_UNAWARE;
                        parms.rgReaderStates[i].Common.cbAtr = VRDE_SCARD_MAX_ATR_LENGTH;
                        RT_ZERO(parms.rgReaderStates[i].Common.rgbAtr);
                    }

                    rc = scSubmitIOCTL(SCARD_IOCTL_GETSTATUSCHANGEW, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_CANCEL:
        {
            VRDESCARDCANCELREQ *pReq = (VRDESCARDCANCELREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCContext_Call parms;
                    parms.Context = pDev->context;
                    rc = scSubmitIOCTL(SCARD_IOCTL_CANCEL, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_CONNECT:
        {
            VRDESCARDCONNECTREQ *pReq = (VRDESCARDCONNECTREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCConnectW_Call parms;
                    parms.szReader = pReq->pszReader;
                    parms.Common.Context = pDev->context;
                    parms.Common.dwShareMode = pReq->u32ShareMode;
                    parms.Common.dwPreferredProtocols = pReq->u32PreferredProtocols;
                    rc = scSubmitIOCTL(SCARD_IOCTL_CONNECTW, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_RECONNECT:
        {
            VRDESCARDRECONNECTREQ *pReq = (VRDESCARDRECONNECTREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCReconnect_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.dwShareMode = pReq->u32ShareMode;
                    parms.dwPreferredProtocols = pReq->u32PreferredProtocols;
                    parms.dwInitialization = pReq->u32Initialization;
                    rc = scSubmitIOCTL(SCARD_IOCTL_RECONNECT, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_DISCONNECT:
        {
            VRDESCARDDISCONNECTREQ *pReq = (VRDESCARDDISCONNECTREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCHCardAndDisposition_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.dwDisposition = pReq->u32Disposition;
                    rc = scSubmitIOCTL(SCARD_IOCTL_DISCONNECT, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_BEGINTRANSACTION:
        {
            VRDESCARDBEGINTRANSACTIONREQ *pReq = (VRDESCARDBEGINTRANSACTIONREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCHCardAndDisposition_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.dwDisposition = pReq->u32Disposition;
                    rc = scSubmitIOCTL(SCARD_IOCTL_BEGINTRANSACTION, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_ENDTRANSACTION:
        {
            VRDESCARDENDTRANSACTIONREQ *pReq = (VRDESCARDENDTRANSACTIONREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCHCardAndDisposition_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.dwDisposition = pReq->u32Disposition;
                    rc = scSubmitIOCTL(SCARD_IOCTL_ENDTRANSACTION, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_STATE:
        {
            VRDESCARDSTATEREQ *pReq = (VRDESCARDSTATEREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCState_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.fpbAtrIsNULL = 0;
                    parms.cbAtrLen = RT_SIZEOFMEMB(SCState_Return, rgAtr);
                    rc = scSubmitIOCTL(SCARD_IOCTL_STATE, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_STATUS:
        {
            VRDESCARDSTATUSREQ *pReq = (VRDESCARDSTATUSREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCStatus_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.fmszReaderNamesIsNULL = 0;
                    parms.cchReaderLen = SCARD_AUTOALLOCATE;
                    parms.cbAtrLen = RT_SIZEOFMEMB(SCStatus_Return, pbAtr); /* Docs are wrong, this is not ignored. */
                    rc = scSubmitIOCTL(SCARD_IOCTL_STATUSW, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_TRANSMIT:
        {
            VRDESCARDTRANSMITREQ *pReq = (VRDESCARDTRANSMITREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCTransmit_Call parms;
                    RT_ZERO(parms);

                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);

                    parms.ioSendPci.dwProtocol = pReq->ioSendPci.u32Protocol;
                    parms.ioSendPci.cbExtraBytes = pReq->ioSendPci.u32PciLength < 2 * sizeof(uint32_t)?
                                                       2 * sizeof(uint32_t):
                                                       pReq->ioSendPci.u32PciLength - 2 * sizeof(uint32_t);
                    memcpy(parms.ioSendPci.pbExtraBytes, pReq->ioSendPci.au8PciData, parms.ioSendPci.cbExtraBytes);

                    parms.cbSendLength = pReq->u32SendLength;
                    parms.pbSendBuffer = pReq->pu8SendBuffer;
                    parms.pioRecvPci = parms.ioSendPci;
                    parms.fpbRecvBufferIsNULL = 0;
                    /* Can't use SCARD_AUTOALLOCATE here, the client crashes.
                     * _128K is an arbitrary limit greater than the allowed buffer length.
                     */
                    parms.cbRecvLength = pReq->u32RecvLength > _128K?
                                            _128K:
                                            pReq->u32RecvLength;

                    rc = scSubmitIOCTL(SCARD_IOCTL_TRANSMIT, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_CONTROL:
        {
            VRDESCARDCONTROLREQ *pReq = (VRDESCARDCONTROLREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCControl_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.dwControlCode = pReq->u32ControlCode;
                    parms.cbInBufferSize = pReq->u32InBufferSize;
                    parms.pvInBuffer = pReq->pu8InBuffer;
                    parms.fpvOutBufferIsNULL = 0;
                    /* Can't use SCARD_AUTOALLOCATE here, the client crashes.
                     * _128K is an arbitrary limit greater than the allowed buffer length.
                     */
                    parms.cbOutBufferSize = pReq->u32OutBufferSize > _128K?
                                            _128K:
                                            pReq->u32OutBufferSize;
                    rc = scSubmitIOCTL(SCARD_IOCTL_CONTROL, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_GETATTRIB:
        {
            VRDESCARDGETATTRIBREQ *pReq = (VRDESCARDGETATTRIBREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCGetAttrib_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.dwAttrId = pReq->u32AttrId;
                    parms.fpbAttrIsNULL = 0;
                    parms.cbAttrLen = pReq->u32AttrLen;
                    rc = scSubmitIOCTL(SCARD_IOCTL_GETATTRIB, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        case VRDE_SCARD_FN_SETATTRIB:
        {
            VRDESCARDSETATTRIBREQ *pReq = (VRDESCARDSETATTRIBREQ *)pvData;

            VRDPSCARDDEVICE *pDev = scDeviceFindByContext(&pReq->hCard.Context);

            if (pDev)
            {
                if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
                {
                    SCSetAttrib_Call parms;
                    parms.hCard.Context = pDev->context;
                    parms.hCard.cbHandle = pReq->hCard.u32HandleSize;
                    memcpy(parms.hCard.pbHandle, pReq->hCard.au8Handle, parms.hCard.cbHandle);
                    parms.dwAttrId = pReq->u32AttrId;
                    parms.pbAttr = pReq->pu8Attr;
                    rc = scSubmitIOCTL(SCARD_IOCTL_SETATTRIB, pDev, &parms, sizeof (parms), pvUser, u32Function);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }

                scDeviceRelease(pDev);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        } break;

        default:
            rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

/** @thread INPUT */
int VRDPSCard::SCardAttach(uint32_t u32ClientId, uint32_t u32DeviceId)
{
    SCARDLOG(("SCardAttach: [%d,%d]\n",
               u32ClientId, u32DeviceId));

    /* RDPDR informs that there is a SmartCard device on this client. */
    VRDPLOGREL(("SCARD enabled for %u\n", u32ClientId));

    /* The SCARD device exists on the client.
     *
     * Remember this smartcard device.
     *
     * Establish a context, which will be used
     * for further requests.
     */

    /* Check if there is currently an attached device already. */
    VRDPSCARDDEVICE *pDev = scDeviceFind(u32ClientId, u32DeviceId);

    if (pDev)
    {
        /* Should not happen. Treat it as a reinitialization. */
        SCContext_Call parms;

        if (pDev->enmCtxStatus == SC_CONTEXT_ESTABLISHED)
        {
            Assert(pDev->enmCtxStatusGetStatusChange == SC_CONTEXT_ESTABLISHED);

            pDev->enmCtxStatus = SC_CONTEXT_RELEASING;

            parms.Context = pDev->context;
            scSubmitIOCTL(SCARD_IOCTL_RELEASECONTEXT, pDev, &parms, sizeof(parms),
                          &pDev->context, 0);
        }

        if (pDev->enmCtxStatusGetStatusChange == SC_CONTEXT_ESTABLISHED)
        {
            pDev->enmCtxStatusGetStatusChange = SC_CONTEXT_RELEASING;

            parms.Context = pDev->contextGetStatusChange;
            scSubmitIOCTL(SCARD_IOCTL_RELEASECONTEXT, pDev, &parms, sizeof(parms),
                          &pDev->contextGetStatusChange, 0);
        }

        scDeviceRelease(pDev);

#ifdef DEBUG_sunlover
        AssertFailed();
#endif
        SCardDetach(u32ClientId, u32DeviceId);
    }

    pDev = scDeviceAdd(u32ClientId, u32DeviceId);

    if (!pDev)
    {
        return VERR_NOT_SUPPORTED;
    }

    VRDESCARDNOTIFYATTACH notify;
    notify.u32ClientId = u32ClientId;
    notify.u32DeviceId = u32DeviceId;
    return scCallbackNotify(VRDE_SCARD_NOTIFY_ATTACH, &notify, sizeof(notify));
}

/** @thread INPUT */
int VRDPSCard::SCardDetach(uint32_t u32ClientId, uint32_t u32DeviceId)
{
    SCARDLOG(("SCardDetach: [%d,%d]\n",
               u32ClientId, u32DeviceId));

    /* RDPDR informs that the SmartCard device was detached.
     * Note that there is not need to do a SCReleaseContext call
     * because the device already gone.
     * Find the device, release it and notify the interface user.
     */

    /* Check if there is currently an attached device already. */
    VRDPSCARDDEVICE *pDev = scDeviceFind(u32ClientId, u32DeviceId);

    if (!pDev)
    {
        SCARDLOG(("SCardDetach: not found\n"));
        return VERR_INVALID_PARAMETER;
    }

    scDeviceRemove(pDev);
    scDeviceRelease(pDev);

    VRDESCARDNOTIFYDETACH notify;
    notify.u32ClientId = u32ClientId;
    notify.u32DeviceId = u32DeviceId;
    return scCallbackNotify(VRDE_SCARD_NOTIFY_DETACH, &notify, sizeof(notify));
}

/*
 * Helpers.
 */
static int _RTUtf16NLenEx(PCRTUTF16 pwszString, size_t cbString, size_t *pcwc)
{
    /* Allow unaligned cbString, for example 3 bytes, but process only full UTF16 characters. */
    size_t cbStringAligned = RT_ALIGN_Z(cbString, sizeof(RTUTF16));
    if (cbStringAligned > cbString)
        cbString = cbStringAligned - sizeof(RTUTF16);

    if (   !pwszString
        || cbString == 0)
        return VERR_INVALID_PARAMETER;

    PCRTUTF16 pwsz = pwszString;
    PCRTUTF16 pwszEnd = (PCRTUTF16)((uint8_t *)pwszString + cbString);
    while (   pwsz != pwszEnd
           && *pwsz)
        pwsz++;
    *pcwc = pwsz - pwszString;
    return VINF_SUCCESS;
}

static int scHlpMultiUTF16toUTF8(char **ppmsz,
                                 uint32_t *pcb,
                                 const RTUTF16 *pSrc,
                                 uint32_t cbSrc)
{
    int rc = VINF_SUCCESS;

    /* Validate the input string and compute required size for the UTF8 result. */
    const RTUTF16 *pCurrent = pSrc;
    size_t cbCurrent = cbSrc;

    size_t cbUtf8Alloc = 0;
    size_t l;

    for (;;)
    {
        /* Length of the current UTF16 string. */
        l = 0;
        rc = _RTUtf16NLenEx(pCurrent, cbCurrent, &l);
        if (   RT_FAILURE(rc)
            || l == 0)
        {
            break;
        }

        /* Size in bytes of the current utf8 string. */
        size_t cbUtf8 = 0;
        rc = RTUtf16CalcUtf8LenEx(pCurrent, l, &cbUtf8);
        if (RT_FAILURE(rc))
        {
            break;
        }

        cbUtf8Alloc += cbUtf8 + 1;

        pCurrent += l;
        cbCurrent -= l * sizeof(RTUTF16);

        if (*pCurrent != 0)
        {
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        pCurrent++;
        cbCurrent -= sizeof(RTUTF16);
    }

    if (RT_SUCCESS(rc))
    {
        cbUtf8Alloc += 1; /* Trailing 0. */

        char *pmsz = (char *)VRDPMemAlloc(cbUtf8Alloc);
        if (!pmsz)
        {
            rc = VERR_NO_MEMORY;
        }
        else
        {
            char *pszUtf8 = pmsz;
            size_t cchAvail = cbUtf8Alloc;

            pCurrent = pSrc;

            for (;;)
            {
                l = RTUtf16Len(pCurrent);
                if (l == 0)
                {
                    *pszUtf8 = 0;
                    break;
                }

                l++; /* Trailing nul. */

                size_t cchUtf8 = 0;
                rc = RTUtf16ToUtf8Ex(pCurrent, l, &pszUtf8, cchAvail, &cchUtf8);
                if (RT_FAILURE(rc))
                {
                    break;
                }
                cchUtf8++; /* Trailing nul. */

                SCARDLOG(("scHlpMultiUTF16toUTF8: [%s]\n",
                           pszUtf8));

                pCurrent += l;

                pszUtf8 += cchUtf8;
                cchAvail -= cchUtf8;
            }

            if (RT_SUCCESS(rc))
            {
                *ppmsz = pmsz;
                *pcb = (uint32_t)cbUtf8Alloc;
            }
            else
            {
                VRDPMemFree(pmsz);
            }
        }
    }

    return rc;
}

static int scHlpUTF8toUTF16(RTUTF16 **ppwsz, uint32_t *pcb, const char *psz)
{
    size_t cwc = 0;
    int rc = RTStrCalcUtf16LenEx(psz, RTSTR_MAX, &cwc);

    if (RT_SUCCESS(rc))
    {
        size_t cbUTF16 = (cwc + 1) * sizeof(RTUTF16);
        RTUTF16 *pwsz = (RTUTF16 *)VRDPMemAlloc(cbUTF16);
        if (pwsz == NULL)
        {
            rc = VERR_NO_MEMORY;
        }
        else
        {
            rc = RTStrToUtf16Ex(psz, RTSTR_MAX, &pwsz, cwc + 1, NULL);

            if (RT_SUCCESS(rc))
            {
                pwsz[cwc] = 0;
                *ppwsz = pwsz;
                *pcb = (uint32_t)cbUTF16;
            }
            else
            {
                VRDPMemFree(pwsz);
            }
        }
    }

    return rc;
}


/*
 * NDR encoder and decoder.
 */

struct VRDPSCARDNDRCTX
{
    uint32_t     u32ReferentId;
    RPCETYPEHDR *pEncodeHeader;
    uint8_t     *pu8Buffer;
    uint8_t     *pu8End;
    uint8_t     *pu8Pos;
    bool        fDecode;
};


#define NDR_BEGIN       bool fNDRSuccess; for (fNDRSuccess = true;;) {
#define NDR_END         break; }
#define NDR_BREAK       break
#ifdef DEBUG_sunlover
#define NDR_ERROR_BREAK ASMBreakpoint(); fNDRSuccess = false; break
#else
#define NDR_ERROR_BREAK fNDRSuccess = false; break
#endif /* DEBUG_sunlover */
#define NDR_IS_SUCCESS  fNDRSuccess
#define NDR_CALL(f)     do { fNDRSuccess = f; } while (0)

#define NDR_CHECK_SUCCESS if (!NDR_IS_SUCCESS) NDR_BREAK
#define NDR_CHECK_DO(f) f; if (!NDR_IS_SUCCESS) NDR_BREAK

#define _NDR_DECODE_TYPEPTR(ndrctx, type, ptr) if (true) {  \
    if (NDRContextGetBytesLeft(ndrctx) < sizeof(type))      \
    {                                                       \
        NDR_ERROR_BREAK;                                    \
    }                                                       \
    ptr = (type *)(ndrctx)->pu8Pos;                         \
    (ndrctx)->pu8Pos += sizeof(type);                       \
} else do {} while (0)
#define NDR_DECODE_TYPEPTR(ndrctx, type, ptr) NDR_CHECK_DO(_NDR_DECODE_TYPEPTR(ndrctx, type, ptr));

#define _NDR_DECODE_TYPE(ndrctx, type, value) if (true) {   \
    if (NDRContextGetBytesLeft(ndrctx) < sizeof(type))      \
    {                                                       \
        NDR_ERROR_BREAK;                                    \
    }                                                       \
    value = *(type *)(ndrctx)->pu8Pos;                      \
    (ndrctx)->pu8Pos += sizeof(type);                       \
} else do {} while (0)
#define NDR_DECODE_TYPE(ndrctx, type, ptr) NDR_CHECK_DO(_NDR_DECODE_TYPE(ndrctx, type, ptr));

#define NDR_DECODE_LONG(ndrctx, value)  NDR_DECODE_TYPE(ndrctx, int32_t, value)
#define NDR_DECODE_ULONG(ndrctx, value) NDR_DECODE_TYPE(ndrctx, uint32_t, value)

#define _NDR_DECODE_POINTER(ndrctx, isNULL) if (true) {       \
    NDR_DECODE_ULONG(ndrctx, (ndrctx)->u32ReferentId); \
    (isNULL) = ((ndrctx)->u32ReferentId == 0);         \
} else do {} while(0)
#define NDR_DECODE_POINTER(ndrctx, isNULL) NDR_CHECK_DO(_NDR_DECODE_POINTER(ndrctx, isNULL));

#define _NDR_DECODE_BYTES(ndrctx, ptr, maxsize, size) if (true) { \
    if ((maxsize) < (size))                                     \
    {                                                           \
        NDR_ERROR_BREAK;                                        \
    }                                                           \
    if (NDRContextGetBytesLeft(ndrctx) < (size))                \
    {                                                           \
        NDR_ERROR_BREAK;                                        \
    }                                                           \
    memcpy((ptr), (ndrctx)->pu8Pos, (size));                    \
    (ndrctx)->pu8Pos += (size);                                 \
} else do {} while (0)

#define NDR_DECODE_BYTES(ndrctx, ptr, maxsize, size) NDR_CHECK_DO(_NDR_DECODE_BYTES(ndrctx, ptr, maxsize, size));

#define _NDR_DECODE_ALIGN(ndrctx, value) if (true) {                   \
    uint32_t cb##__LINE__ = NDRContextGetBytesProcessed(ndrctx);       \
    uint32_t cbAligned##__LINE__ = RT_ALIGN_32(cb##__LINE__, (value)); \
    if (cbAligned##__LINE__ > NDRContextGetSize(ndrctx))               \
    {                                                                  \
        NDR_ERROR_BREAK;                                               \
    }                                                                  \
    (ndrctx)->pu8Pos += cbAligned##__LINE__ - cb##__LINE__;            \
} else do {} while (0)
#define NDR_DECODE_ALIGN(ndrctx, value) NDR_CHECK_DO(_NDR_DECODE_ALIGN(ndrctx, value));

#define _NDR_ENCODE_TYPE(ndrctx, type, value) if (true) {              \
    if (NDRContextGetBytesLeft(ndrctx) < sizeof(type))                 \
    {                                                                  \
        if (RT_FAILURE(NDRContextExtend((ndrctx),                      \
            RT_MAX(4096, sizeof(type)) + NDRContextGetSize(ndrctx))))  \
        {                                                              \
            NDR_ERROR_BREAK;                                           \
        }                                                              \
    }                                                                  \
    *(type *)(ndrctx)->pu8Pos = (value);                               \
    (ndrctx)->pu8Pos += sizeof(type);                                  \
} else do {} while (0)
#define NDR_ENCODE_TYPE(ndrctx, type, value) NDR_CHECK_DO(_NDR_ENCODE_TYPE(ndrctx, type, value));

#define _NDR_ENCODE_ALIGN(ndrctx, value) if (true) {                   \
    uint32_t cb##__LINE__ = NDRContextGetBytesProcessed(ndrctx);       \
    uint32_t cbAligned##__LINE__ = RT_ALIGN_32(cb##__LINE__, (value)); \
    if (cbAligned##__LINE__ > NDRContextGetSize(ndrctx))               \
    {                                                                  \
        if (RT_FAILURE(NDRContextExtend((ndrctx),                      \
            RT_MAX(4096, cbAligned##__LINE__))))                       \
        {                                                              \
            NDR_ERROR_BREAK;                                           \
        }                                                              \
    }                                                                  \
    (ndrctx)->pu8Pos += cbAligned##__LINE__ - cb##__LINE__;            \
} else do {} while (0)
#define NDR_ENCODE_ALIGN(ndrctx, value) NDR_CHECK_DO(_NDR_ENCODE_ALIGN(ndrctx, value));

#define NDR_ENCODE_LONG(ndrctx, value) NDR_ENCODE_TYPE(ndrctx, int32_t, value)
#define NDR_ENCODE_ULONG(ndrctx, value) NDR_ENCODE_TYPE(ndrctx, uint32_t, value)

#define _NDR_ENCODE_POINTER(ndrctx, ptr) if (true) {       \
    if ((ptr) == NULL)                                     \
    {                                                      \
        NDR_ENCODE_ULONG(ndrctx, 0);                       \
    }                                                      \
    else                                                   \
    {                                                      \
        NDR_ENCODE_ULONG(ndrctx, (ndrctx)->u32ReferentId); \
        (ndrctx)->u32ReferentId += 4;                      \
    }                                                      \
} else do {} while(0)

#define _NDR_ENCODE_POINTER_NULL(ndrctx) if (true) {       \
    NDR_ENCODE_ULONG(ndrctx, 0);                           \
} else do {} while(0)

#define NDR_ENCODE_POINTER(ndrctx, ptr) NDR_CHECK_DO(_NDR_ENCODE_POINTER(ndrctx, ptr));
#define NDR_ENCODE_POINTER_NULL(ndrctx) NDR_CHECK_DO(_NDR_ENCODE_POINTER_NULL(ndrctx));

#define _NDR_ENCODE_BYTES(ndrctx, ptr, size) if (true) {        \
    if (NDRContextGetBytesLeft(ndrctx) < (size))                \
    {                                                           \
        if (RT_FAILURE(NDRContextExtend((ndrctx),               \
            RT_MAX(4096, (size) + NDRContextGetSize(ndrctx))))) \
        {                                                       \
            NDR_ERROR_BREAK;                                    \
        }                                                       \
    }                                                           \
    memcpy((ndrctx)->pu8Pos, (ptr), (size));                    \
    (ndrctx)->pu8Pos += (size);                                 \
} else do {} while (0)

#define NDR_ENCODE_BYTES(ndrctx, ptr, size) NDR_CHECK_DO(_NDR_ENCODE_BYTES(ndrctx, ptr, size));

/*
 * NDR context methods.
 */

static int NDRContextInit(VRDPSCARDNDRCTX *pNdrCtx, const uint8_t *pu8Buffer, uint32_t u32BufferLength, bool fDecode)
{
    int rc = VINF_SUCCESS;

    if (u32BufferLength >= sizeof(RPCEHDR) + sizeof(RPCETYPEHDR))
    {
        pNdrCtx->u32ReferentId = 0;
        pNdrCtx->pEncodeHeader = NULL;
        pNdrCtx->pu8Buffer = (uint8_t *)pu8Buffer;
        pNdrCtx->pu8End = pNdrCtx->pu8Buffer + u32BufferLength;
        pNdrCtx->pu8Pos = pNdrCtx->pu8Buffer;
        pNdrCtx->fDecode = fDecode;
    }
    else
    {
        rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}

static uint32_t NDRContextGetBytesLeft(VRDPSCARDNDRCTX *pNdrCtx)
{
    return (uint32_t)(pNdrCtx->pu8End - pNdrCtx->pu8Pos);
}

static uint32_t NDRContextGetBytesProcessed(VRDPSCARDNDRCTX *pNdrCtx)
{
    return (uint32_t)(pNdrCtx->pu8Pos - pNdrCtx->pu8Buffer);
}

static uint32_t NDRContextGetSize(VRDPSCARDNDRCTX *pNdrCtx)
{
    return (uint32_t)(pNdrCtx->pu8End - pNdrCtx->pu8Buffer);
}

static int NDRContextExtend(VRDPSCARDNDRCTX *pNdrCtx, uint32_t cb)
{
    int rc = VINF_SUCCESS;

    if (!pNdrCtx->fDecode)
    {
        if (cb > NDRContextGetSize(pNdrCtx))
        {
            uint32_t offHdr = (uint32_t)((uint8_t *)pNdrCtx->pEncodeHeader - pNdrCtx->pu8Buffer);
            uint32_t offPos = NDRContextGetBytesProcessed(pNdrCtx);

            void *p = VRDPMemAlloc(cb);

            if (p)
            {
                memcpy(p, pNdrCtx->pu8Buffer, offPos);
                VRDPMemFree(pNdrCtx->pu8Buffer);

                pNdrCtx->pu8Buffer = (uint8_t *)p;
                pNdrCtx->pu8End = pNdrCtx->pu8Buffer + cb;
                pNdrCtx->pu8Pos = pNdrCtx->pu8Buffer + offPos;
                pNdrCtx->pEncodeHeader = (RPCETYPEHDR *)(pNdrCtx->pu8Buffer + offHdr);

                memset(pNdrCtx->pu8Pos, 0, NDRContextGetBytesLeft(pNdrCtx));
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }
        }
    }
    else
    {
        AssertFailed();
        rc = VERR_NOT_SUPPORTED;
    }

    SCARDLOG(("NDRContextExtend: %Rrc, %d bytes\n", rc, cb));

    return rc;
}

static int NDRContextDecodeStart(VRDPSCARDNDRCTX *pNdrCtx)
{
    int rc = VINF_SUCCESS;

    pNdrCtx->u32ReferentId = 0;
    pNdrCtx->pu8Pos = pNdrCtx->pu8Buffer;

    RPCEHDR *pRPCEHdr;
    RPCETYPEHDR *pTypeHdr;

    NDR_BEGIN

       NDR_DECODE_TYPEPTR(pNdrCtx, RPCEHDR, pRPCEHdr);
       if (   pRPCEHdr->u8Version != 1
           || pRPCEHdr->u8Endianness != 0x10
           || pRPCEHdr->u16Length != 8)
       {
           rc = VERR_INVALID_PARAMETER;
           NDR_BREAK;
       }

       NDR_DECODE_TYPEPTR(pNdrCtx, RPCETYPEHDR, pTypeHdr);
       if (pTypeHdr->u32Length > NDRContextGetBytesLeft(pNdrCtx))
       {
           rc = VERR_TOO_MUCH_DATA;
           NDR_BREAK;
       }

       pNdrCtx->pu8End = pNdrCtx->pu8Pos + pTypeHdr->u32Length;

    NDR_END

    if (RT_FAILURE(rc))
    {
        return rc;
    }

    return NDR_IS_SUCCESS? VINF_SUCCESS: VERR_INVALID_PARAMETER;
}

static int NDRContextEncodeStart(VRDPSCARDNDRCTX *pNdrCtx)
{
    int rc = VINF_SUCCESS;

    Assert(NDRContextGetSize(pNdrCtx) > sizeof(RPCEHDR) + sizeof(RPCETYPEHDR));

    pNdrCtx->u32ReferentId = 0x00020000;
    pNdrCtx->pu8Pos = pNdrCtx->pu8Buffer;

    /* Write RPCEHDR. */
    memcpy(pNdrCtx->pu8Pos, &sRpceHdr, sizeof(sRpceHdr));
    pNdrCtx->pu8Pos += sizeof(sRpceHdr);

    /* Save pointer to the type header. */
    pNdrCtx->pEncodeHeader = (RPCETYPEHDR *)pNdrCtx->pu8Pos;
    pNdrCtx->pu8Pos += sizeof(RPCETYPEHDR);

    memset(pNdrCtx->pu8Pos, 0, NDRContextGetBytesLeft(pNdrCtx));

    return rc;
}

static int NDRContextEncodeFinish(VRDPSCARDNDRCTX *pNdrCtx)
{
    int rc = VINF_SUCCESS;

    uint32_t cb = NDRContextGetBytesProcessed(pNdrCtx);

    if (cb >= sizeof(RPCETYPEHDR) + sizeof(RPCEHDR))
    {
        uint32_t cbAligned = RT_ALIGN_32(cb, 8);

        rc = NDRContextExtend(pNdrCtx, cbAligned);

        if (RT_SUCCESS(rc))
        {
            if (cbAligned > cb)
            {
                memset(pNdrCtx->pu8Pos, 0, cbAligned - cb);
                pNdrCtx->pu8Pos += cbAligned - cb;
            }

            cb = cbAligned - sizeof(RPCETYPEHDR) - sizeof(RPCEHDR);

            pNdrCtx->pEncodeHeader->u32Length = cb;
            pNdrCtx->pEncodeHeader->u32Filler = 0;
        }
    }
    else
    {
        AssertFailed();
        rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}


/*
 * Encoders for the smartcard protocol structures.
 */

static bool scEncodeEstablishContext_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCEstablishContext_Call), false);

    const SCEstablishContext_Call *p = (SCEstablishContext_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->dwScope);

    NDR_END

    return NDR_IS_SUCCESS;
}

static const RTUTF16 VRDP_SCARD_ALL_READERS_W[] =
{
   'S', 'C', 'a', 'r', 'd', '$', 'A', 'l', 'l', 'R', 'e', 'a', 'd', 'e', 'r', 's', '\0', '\0'
};

static bool scEncodeListReaders_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCListReaders_Call), false);

    const SCListReaders_Call *p = (SCListReaders_Call *)pvParms;

    const RTUTF16 *mszGroups = &VRDP_SCARD_ALL_READERS_W[0];
    uint32_t cBytes = sizeof(VRDP_SCARD_ALL_READERS_W);

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx,   cBytes);
        NDR_ENCODE_POINTER(pNdrCtx, mszGroups);

        NDR_ENCODE_LONG(pNdrCtx,    0); /* fmszReadersIsNULL */
        NDR_ENCODE_ULONG(pNdrCtx,   0xFFFFFFFF); /* cchReaders == SCARD_AUTOALLOCATE */

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx,   4);
        NDR_ENCODE_ULONG(pNdrCtx,   p->Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx,   &p->Context.au8Ctx[0], p->Context.cbCtx);

        /* byte *mszGroups */
        NDR_ENCODE_ALIGN(pNdrCtx,   4);
        NDR_ENCODE_ULONG(pNdrCtx,   cBytes);
        NDR_ENCODE_BYTES(pNdrCtx,   mszGroups, cBytes);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeContext_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCContext_Call), false);

    const SCContext_Call *p = (SCContext_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->Context.au8Ctx[0]);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->Context.au8Ctx[0], p->Context.cbCtx);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeGetStatusChangeW_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCGetStatusChangeW_Call), false);

    const SCGetStatusChangeW_Call *p = (SCGetStatusChangeW_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->dwTimeOut);
        NDR_ENCODE_ULONG(pNdrCtx, p->cReaders);

        if (p->cReaders)
        {
            NDR_ENCODE_POINTER(pNdrCtx, &p->rgReaderStates);
        }
        else
        {
            NDR_ENCODE_POINTER_NULL(pNdrCtx);
        }

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->Context.au8Ctx[0], p->Context.cbCtx);

        if (p->cReaders)
        {
            /* rgReaderStates */
            NDR_ENCODE_ALIGN(pNdrCtx, 4);
            NDR_ENCODE_ULONG(pNdrCtx, p->cReaders);

            const SCReaderStateW *pRS;
            uint32_t i;
            for (i = 0; i < p->cReaders; ++i)
            {
                pRS = &p->rgReaderStates[i];
                NDR_ENCODE_ALIGN(pNdrCtx, 4);
                NDR_ENCODE_POINTER(pNdrCtx, pRS->szReader);

                NDR_ENCODE_ULONG(pNdrCtx, pRS->Common.dwCurrentState);
                NDR_ENCODE_ULONG(pNdrCtx, pRS->Common.dwEventState);
                NDR_ENCODE_ULONG(pNdrCtx, pRS->Common.cbAtr);

                NDR_ENCODE_ALIGN(pNdrCtx, 4);
                NDR_ENCODE_BYTES(pNdrCtx, &pRS->Common.rgbAtr[0], VRDE_SCARD_MAX_ATR_LENGTH);
            }

            /* szReader bytes. */
            for (i = 0; i < p->cReaders; ++i)
            {
                pRS = &p->rgReaderStates[i];
                if (pRS->szReader)
                {
                    NDR_ENCODE_ALIGN(pNdrCtx, 4);

                    RTUTF16 *pwszReader;
                    uint32_t cbReader;
                    int rc = scHlpUTF8toUTF16(&pwszReader, &cbReader, pRS->szReader);
                    if (RT_FAILURE(rc))
                    {
                        NDR_ERROR_BREAK;
                    }
                    NDR_ENCODE_ULONG(pNdrCtx, cbReader / sizeof(RTUTF16));
                    NDR_ENCODE_ULONG(pNdrCtx, 0);
                    NDR_ENCODE_ULONG(pNdrCtx, cbReader / sizeof(RTUTF16));
                    NDR_ENCODE_BYTES(pNdrCtx, pwszReader, cbReader);
                    VRDPMemFree(pwszReader);
                }
            }
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeConnectW_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCConnectW_Call), false);

    const SCConnectW_Call *p = (SCConnectW_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_POINTER(pNdrCtx, p->szReader);

        NDR_ENCODE_ULONG(pNdrCtx, p->Common.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->Common.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->Common.dwShareMode);
        NDR_ENCODE_ULONG(pNdrCtx, p->Common.dwPreferredProtocols);

        if (p->szReader)
        {
            NDR_ENCODE_ALIGN(pNdrCtx, 4);

            RTUTF16 *pwszReader;
            uint32_t cbReader;
            int rc = scHlpUTF8toUTF16(&pwszReader, &cbReader, p->szReader);
            if (RT_FAILURE(rc))
            {
                NDR_ERROR_BREAK;
            }
            NDR_ENCODE_ULONG(pNdrCtx, cbReader / sizeof(RTUTF16));
            NDR_ENCODE_ULONG(pNdrCtx, 0);
            NDR_ENCODE_ULONG(pNdrCtx, cbReader / sizeof(RTUTF16));
            NDR_ENCODE_BYTES(pNdrCtx, pwszReader, cbReader);
            VRDPMemFree(pwszReader);
        }

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->Common.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->Common.Context.au8Ctx[0], p->Common.Context.cbCtx);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeReconnect_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCReconnect_Call), false);

    const SCReconnect_Call *p = (SCReconnect_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.pbHandle[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->dwShareMode);
        NDR_ENCODE_ULONG(pNdrCtx, p->dwPreferredProtocols);
        NDR_ENCODE_ULONG(pNdrCtx, p->dwInitialization);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.Context.au8Ctx[0], p->hCard.Context.cbCtx);

        /* handle */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.pbHandle[0], p->hCard.cbHandle);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeHCardAndDisposition_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCHCardAndDisposition_Call), false);

    const SCHCardAndDisposition_Call *p = (SCHCardAndDisposition_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.pbHandle[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->dwDisposition);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.Context.au8Ctx[0], p->hCard.Context.cbCtx);

        /* handle */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.pbHandle[0], p->hCard.cbHandle);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeState_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCState_Call), false);

    const SCState_Call *p = (SCState_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.pbHandle[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->fpbAtrIsNULL);
        NDR_ENCODE_ULONG(pNdrCtx, p->cbAtrLen);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.Context.au8Ctx[0], p->hCard.Context.cbCtx);

        /* handle */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.pbHandle[0], p->hCard.cbHandle);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeStatus_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCStatus_Call), false);

    const SCStatus_Call *p = (SCStatus_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.pbHandle[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->fmszReaderNamesIsNULL);
        NDR_ENCODE_ULONG(pNdrCtx, p->cchReaderLen);
        NDR_ENCODE_ULONG(pNdrCtx, p->cbAtrLen);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.Context.au8Ctx[0], p->hCard.Context.cbCtx);

        /* handle */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.pbHandle[0], p->hCard.cbHandle);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeTransmit_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCTransmit_Call), false);

    const SCTransmit_Call *p = (SCTransmit_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.pbHandle[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->ioSendPci.dwProtocol);
        NDR_ENCODE_ULONG(pNdrCtx, p->ioSendPci.cbExtraBytes);
        if (p->ioSendPci.cbExtraBytes)
        {
            NDR_ENCODE_POINTER(pNdrCtx, &p->ioSendPci.pbExtraBytes[0]);
        }
        else
        {
            NDR_ENCODE_POINTER_NULL(pNdrCtx);
        }

        NDR_ENCODE_ULONG(pNdrCtx, p->cbSendLength);
        NDR_ENCODE_POINTER(pNdrCtx, p->pbSendBuffer);
        NDR_ENCODE_POINTER(pNdrCtx, &p->pioRecvPci);
        NDR_ENCODE_ULONG(pNdrCtx, p->fpbRecvBufferIsNULL);
        NDR_ENCODE_ULONG(pNdrCtx, p->cbRecvLength);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.Context.au8Ctx[0], p->hCard.Context.cbCtx);

        /* handle */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.pbHandle[0], p->hCard.cbHandle);

        if (p->ioSendPci.cbExtraBytes)
        {
            NDR_ENCODE_ALIGN(pNdrCtx, 4);

            NDR_ENCODE_ULONG(pNdrCtx, p->ioSendPci.cbExtraBytes);
            NDR_ENCODE_BYTES(pNdrCtx, &p->ioSendPci.pbExtraBytes[0], p->ioSendPci.cbExtraBytes);
        }

        if (p->pbSendBuffer)
        {
            NDR_ENCODE_ALIGN(pNdrCtx, 4);

            NDR_ENCODE_ULONG(pNdrCtx, p->cbSendLength);
            NDR_ENCODE_BYTES(pNdrCtx, p->pbSendBuffer, p->cbSendLength);
        }

        NDR_ENCODE_ULONG(pNdrCtx, p->pioRecvPci.dwProtocol);
        NDR_ENCODE_ULONG(pNdrCtx, p->pioRecvPci.cbExtraBytes);
        if (p->pioRecvPci.cbExtraBytes)
        {
            NDR_ENCODE_POINTER(pNdrCtx, &p->pioRecvPci.pbExtraBytes[0]);
        }
        else
        {
            NDR_ENCODE_POINTER_NULL(pNdrCtx);
        }

        if (p->ioSendPci.cbExtraBytes)
        {
            NDR_ENCODE_ALIGN(pNdrCtx, 4);

            NDR_ENCODE_ULONG(pNdrCtx, p->pioRecvPci.cbExtraBytes);
            NDR_ENCODE_BYTES(pNdrCtx, &p->pioRecvPci.pbExtraBytes[0], p->pioRecvPci.cbExtraBytes);
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeControl_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCControl_Call), false);

    const SCControl_Call *p = (SCControl_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.pbHandle[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->dwControlCode);
        NDR_ENCODE_ULONG(pNdrCtx, p->cbInBufferSize);
        NDR_ENCODE_POINTER(pNdrCtx, p->pvInBuffer);
        NDR_ENCODE_ULONG(pNdrCtx, p->fpvOutBufferIsNULL);
        NDR_ENCODE_ULONG(pNdrCtx, p->cbOutBufferSize);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.Context.au8Ctx[0], p->hCard.Context.cbCtx);

        /* handle */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.pbHandle[0], p->hCard.cbHandle);

        if (p->pvInBuffer)
        {
            NDR_ENCODE_ALIGN(pNdrCtx, 4);

            NDR_ENCODE_ULONG(pNdrCtx, p->cbInBufferSize);
            NDR_ENCODE_BYTES(pNdrCtx, p->pvInBuffer, p->cbInBufferSize);
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeGetAttrib_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCGetAttrib_Call), false);

    const SCGetAttrib_Call *p = (SCGetAttrib_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.pbHandle[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->dwAttrId);
        NDR_ENCODE_ULONG(pNdrCtx, p->fpbAttrIsNULL);
        NDR_ENCODE_ULONG(pNdrCtx, p->cbAttrLen);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.Context.au8Ctx[0], p->hCard.Context.cbCtx);

        /* handle */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.pbHandle[0], p->hCard.cbHandle);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scEncodeSetAttrib_Call(VRDPSCARDNDRCTX *pNdrCtx, const void *pvParms, uint32_t cbParms)
{
    AssertReturn(cbParms >= sizeof(SCSetAttrib_Call), false);

    const SCSetAttrib_Call *p = (SCSetAttrib_Call *)pvParms;

    NDR_BEGIN

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.Context.au8Ctx[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_POINTER(pNdrCtx, &p->hCard.pbHandle[0]);

        NDR_ENCODE_ULONG(pNdrCtx, p->dwAttrId);
        NDR_ENCODE_ULONG(pNdrCtx, p->cbAttrLen);
        NDR_ENCODE_POINTER(pNdrCtx, p->pbAttr);

        /* context */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.Context.cbCtx);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.Context.au8Ctx[0], p->hCard.Context.cbCtx);

        /* handle */
        NDR_ENCODE_ALIGN(pNdrCtx, 4);
        NDR_ENCODE_ULONG(pNdrCtx, p->hCard.cbHandle);
        NDR_ENCODE_BYTES(pNdrCtx, &p->hCard.pbHandle[0], p->hCard.cbHandle);

        if (p->pbAttr)
        {
            NDR_ENCODE_ALIGN(pNdrCtx, 4);

            NDR_ENCODE_ULONG(pNdrCtx, p->cbAttrLen);
            NDR_ENCODE_BYTES(pNdrCtx, p->pbAttr, p->cbAttrLen);
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

/*
 * Decoders for the smartcard protocol structures.
 */

static bool scDecodeLong_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCLong_Return), false);

    SCLong_Return *pResult = (SCLong_Return *)pvResult;

    NDR_BEGIN

        /* ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeEstablishContext_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCEstablishContext_Return), false);

    SCEstablishContext_Return *pResult = (SCEstablishContext_Return *)pvResult;

    bool fIsNull;
    uint32_t cb;

    NDR_BEGIN

        /* ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* REDIR_SCARDCONTEXT::long cbContext */
        NDR_DECODE_ULONG(pNdrCtx, pResult->Context.cbCtx);
        if (pResult->Context.cbCtx > sizeof(pResult->Context.au8Ctx))
        {
            NDR_ERROR_BREAK;
        }

        /* REDIR_SCARDCONTEXT::bytes *pbHandle */
        NDR_DECODE_POINTER(pNdrCtx, fIsNull);
        if (fIsNull)
        {
            NDR_ERROR_BREAK;
        }

        NDR_DECODE_ALIGN(pNdrCtx, 4);

        NDR_DECODE_ULONG(pNdrCtx, cb);
        if (cb != pResult->Context.cbCtx)
        {
            NDR_ERROR_BREAK;
        }

        NDR_DECODE_BYTES(pNdrCtx, pResult->Context.au8Ctx, sizeof(pResult->Context.au8Ctx), cb);

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeListReaders_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCListReaders_Return), false);

    SCListReaders_Return *pResult = (SCListReaders_Return *)pvResult;

    uint32_t cBytes;
    bool fIsNull;

    NDR_BEGIN

        /* long ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* unsigned long cBytes */
        NDR_DECODE_ULONG(pNdrCtx, cBytes);

        /* byte *msz */
        NDR_DECODE_POINTER(pNdrCtx, fIsNull);

        if (fIsNull || cBytes == 0)
        {
            pResult->msz = NULL;
            pResult->cBytes = 0;
        }
        else
        {
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            /* Data for msz */
            uint32_t cb;
            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != cBytes)
            {
                NDR_ERROR_BREAK;
            }

            int rc = scHlpMultiUTF16toUTF8(&pResult->msz, &pResult->cBytes, (PRTUTF16)pNdrCtx->pu8Pos, cb);
            if (RT_FAILURE(rc))
            {
                NDR_ERROR_BREAK;
            }
            pNdrCtx->pu8Pos += cb;
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeGetStatusChange_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCGetStatusChange_Return), false);

    SCGetStatusChange_Return *pResult = (SCGetStatusChange_Return *)pvResult;

    bool fIsNull;

    NDR_BEGIN

        /* long ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* unsigned long cReaders */
        NDR_DECODE_ULONG(pNdrCtx, pResult->cReaders);

        if (pResult->cReaders > 10)
        {
            NDR_ERROR_BREAK;
        }

        /* ReaderState_Return* rgReaderStates */
        NDR_DECODE_POINTER(pNdrCtx, fIsNull);

        if (fIsNull)
        {
            pResult->cReaders = 0;
        }
        else
        {
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            uint32_t cElements;
            NDR_DECODE_ULONG(pNdrCtx, cElements);

            if (cElements != pResult->cReaders)
            {
                NDR_ERROR_BREAK;
            }

            uint32_t i;
            for (i = 0; i < pResult->cReaders; ++i)
            {
                SCReaderState_Return *p = &pResult->rgReaderStates[i];

                NDR_DECODE_ULONG(pNdrCtx, p->dwCurrentState);
                NDR_DECODE_ULONG(pNdrCtx, p->dwEventState);
                NDR_DECODE_ULONG(pNdrCtx, p->cbAtr);
                if (p->cbAtr > VRDE_SCARD_MAX_ATR_LENGTH)
                {
                    NDR_ERROR_BREAK;
                }
                NDR_DECODE_BYTES(pNdrCtx, p->rgbAtr, sizeof(p->rgbAtr), p->cbAtr);
            }
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeConnect_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCConnect_Return), false);

    SCConnect_Return *pResult = (SCConnect_Return *)pvResult;

    bool fContextIsNull;
    bool fHandleIsNull;
    uint32_t cb;

    NDR_BEGIN

        /* ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* REDIR_SCARDHANDLE::Context long cbContext */
        NDR_DECODE_ULONG(pNdrCtx, pResult->hCard.Context.cbCtx);
        if (pResult->hCard.Context.cbCtx > sizeof(pResult->hCard.Context.au8Ctx))
        {
            NDR_ERROR_BREAK;
        }

        /* REDIR_SCARDCONTEXT::Context bytes *pbContext */
        NDR_DECODE_POINTER(pNdrCtx, fContextIsNull);
        /* Context can be NULL. */

        /* REDIR_SCARDHANDLE::cbHandle */
        NDR_DECODE_ULONG(pNdrCtx, pResult->hCard.cbHandle);
        if (pResult->hCard.cbHandle > sizeof(pResult->hCard.pbHandle))
        {
            NDR_ERROR_BREAK;
        }

        /* REDIR_SCARDCONTEXT::bytes *pbHandle */
        NDR_DECODE_POINTER(pNdrCtx, fHandleIsNull);

        /* dwActiveProtocol */
        NDR_DECODE_ULONG(pNdrCtx, pResult->dwActiveProtocol);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        if (!fContextIsNull)
        {
            /* pbContext bytes. */
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != pResult->hCard.Context.cbCtx)
            {
                NDR_ERROR_BREAK;
            }

            NDR_DECODE_BYTES(pNdrCtx, pResult->hCard.Context.au8Ctx, sizeof(pResult->hCard.Context.au8Ctx), cb);
        }

        if (!fHandleIsNull)
        {
            /* pbHandle bytes. */
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != pResult->hCard.cbHandle)
            {
                NDR_ERROR_BREAK;
            }

            NDR_DECODE_BYTES(pNdrCtx, pResult->hCard.pbHandle, sizeof(pResult->hCard.pbHandle), cb);
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeReconnect_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCReconnect_Return), false);

    SCReconnect_Return *pResult = (SCReconnect_Return *)pvResult;

    NDR_BEGIN

        /* ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* dwActiveProtocol */
        NDR_DECODE_ULONG(pNdrCtx, pResult->dwActiveProtocol);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeState_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCState_Return), false);

    SCState_Return *pResult = (SCState_Return *)pvResult;

    bool fIsNull;
    uint32_t cb;

    NDR_BEGIN

        /* long ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        NDR_DECODE_ULONG(pNdrCtx, pResult->dwState);
        NDR_DECODE_ULONG(pNdrCtx, pResult->dwProtocol);
        NDR_DECODE_ULONG(pNdrCtx, pResult->cbAtrLen);
        if (pResult->cbAtrLen > VRDE_SCARD_MAX_ATR_LENGTH)
        {
            NDR_ERROR_BREAK;
        }

        /* byte* rgAtr */
        NDR_DECODE_POINTER(pNdrCtx, fIsNull);

        if (fIsNull)
        {
            pResult->cbAtrLen = 0;
        }
        else
        {
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != pResult->cbAtrLen)
            {
                NDR_ERROR_BREAK;
            }

            NDR_DECODE_BYTES(pNdrCtx, pResult->rgAtr, sizeof(pResult->rgAtr), pResult->cbAtrLen);
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeStatus_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCStatus_Return), false);

    SCStatus_Return *pResult = (SCStatus_Return *)pvResult;

    bool fIsNull;
    uint32_t cBytes;

    NDR_BEGIN

        /* long ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* unsigned long cBytes */
        NDR_DECODE_ULONG(pNdrCtx, cBytes);

        /* byte *mszReaderNames */
        NDR_DECODE_POINTER(pNdrCtx, fIsNull);

        NDR_DECODE_ULONG(pNdrCtx, pResult->dwState);
        NDR_DECODE_ULONG(pNdrCtx, pResult->dwProtocol);

        NDR_DECODE_ALIGN(pNdrCtx, 4);
        NDR_DECODE_BYTES(pNdrCtx, pResult->pbAtr, sizeof(pResult->pbAtr), 32);
        NDR_DECODE_ALIGN(pNdrCtx, 4);

        NDR_DECODE_ULONG(pNdrCtx, pResult->cbAtrLen);
        if (pResult->cbAtrLen > 32)
        {
            NDR_ERROR_BREAK;
        }

        if (fIsNull || cBytes == 0)
        {
            pResult->mszReaderNames = NULL;
            pResult->cBytes = 0;
        }
        else
        {
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            /* Data for mszReaderNames */
            uint32_t cb;
            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != cBytes)
            {
                NDR_ERROR_BREAK;
            }

            int rc = scHlpMultiUTF16toUTF8(&pResult->mszReaderNames, &pResult->cBytes, (PRTUTF16)pNdrCtx->pu8Pos, cb);
            if (RT_FAILURE(rc))
            {
                NDR_ERROR_BREAK;
            }
            pNdrCtx->pu8Pos += cb;
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeTransmit_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCTransmit_Return), false);

    SCTransmit_Return *pResult = (SCTransmit_Return *)pvResult;

    bool fBufferIsNull;
    bool fPciIsNull;
    bool fExtraBytesIsNull;
    uint32_t cb;

    NDR_BEGIN

        /* ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* SCardIO_Request* pioRecvPci */
        NDR_DECODE_POINTER(pNdrCtx, fPciIsNull);

        /* cbRecvLength */
        NDR_DECODE_ULONG(pNdrCtx, pResult->cbRecvLength);

        /* byte* pbRecvBuffer */
        NDR_DECODE_POINTER(pNdrCtx, fBufferIsNull);

        /* pioRecvPci */
        if (fPciIsNull)
        {
            pResult->pioRecvPci.dwProtocol = 0;
            pResult->pioRecvPci.cbExtraBytes = 0;
            fExtraBytesIsNull = true;
        }
        else
        {
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            NDR_DECODE_ULONG(pNdrCtx, pResult->pioRecvPci.dwProtocol);
            NDR_DECODE_ULONG(pNdrCtx, pResult->pioRecvPci.cbExtraBytes);

            /* byte* pbExtraBytes */
            NDR_DECODE_POINTER(pNdrCtx, fExtraBytesIsNull);
        }

        if (fBufferIsNull || pResult->cbRecvLength == 0)
        {
            pResult->cbRecvLength = 0;
            pResult->pbRecvBuffer = NULL;
        }
        else
        {
            /* pbRecvBuffer */
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != pResult->cbRecvLength)
            {
                NDR_ERROR_BREAK;
            }

            pResult->pbRecvBuffer = (uint8_t *)VRDPMemAlloc(cb);
            if (pResult->pbRecvBuffer == NULL)
            {
                NDR_ERROR_BREAK;
            }

            NDR_DECODE_BYTES(pNdrCtx, pResult->pbRecvBuffer, cb, cb);
        }

        if (fExtraBytesIsNull || pResult->pioRecvPci.cbExtraBytes == 0)
        {
            pResult->pioRecvPci.cbExtraBytes = 0;
        }
        else
        {
            /* pbExtraBytes */
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != pResult->pioRecvPci.cbExtraBytes)
            {
                NDR_ERROR_BREAK;
            }

            NDR_DECODE_BYTES(pNdrCtx, pResult->pioRecvPci.pbExtraBytes, sizeof(pResult->pioRecvPci.pbExtraBytes), cb);
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeControl_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCControl_Return), false);

    SCControl_Return *pResult = (SCControl_Return *)pvResult;

    bool fIsNull;
    uint32_t cb;

    NDR_BEGIN

        /* ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* cbOutBufferSize */
        NDR_DECODE_ULONG(pNdrCtx, pResult->cbOutBufferSize);

        /* byte* pvOutBuffer */
        NDR_DECODE_POINTER(pNdrCtx, fIsNull);

        if (fIsNull || pResult->cbOutBufferSize == 0)
        {
            pResult->cbOutBufferSize = 0;
            pResult->pvOutBuffer = NULL;
        }
        else
        {
            /* pvOutBuffer */
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != pResult->cbOutBufferSize)
            {
                NDR_ERROR_BREAK;
            }

            pResult->pvOutBuffer = (uint8_t *)VRDPMemAlloc(cb);
            if (pResult->pvOutBuffer == NULL)
            {
                NDR_ERROR_BREAK;
            }

            NDR_DECODE_BYTES(pNdrCtx, pResult->pvOutBuffer, cb, cb);
        }

    NDR_END

    return NDR_IS_SUCCESS;
}

static bool scDecodeGetAttrib_Return(VRDPSCARDNDRCTX *pNdrCtx, void *pvResult, uint32_t cbResult)
{
    AssertReturn(cbResult >= sizeof(SCGetAttrib_Return), false);

    SCGetAttrib_Return *pResult = (SCGetAttrib_Return *)pvResult;

    bool fIsNull;
    uint32_t cb;

    NDR_BEGIN

        /* ReturnCode */
        NDR_DECODE_ULONG(pNdrCtx, pResult->ReturnCode);
        if (pResult->ReturnCode != RDPDR_STATUS_SUCCESS)
        {
            NDR_BREAK;
        }

        /* cbAttrLen */
        NDR_DECODE_ULONG(pNdrCtx, pResult->cbAttrLen);

        /* byte* pbAttr */
        NDR_DECODE_POINTER(pNdrCtx, fIsNull);

        if (fIsNull || pResult->cbAttrLen == 0)
        {
            pResult->cbAttrLen = 0;
            pResult->pbAttr = NULL;
        }
        else
        {
            /* pbAttr */
            NDR_DECODE_ALIGN(pNdrCtx, 4);

            NDR_DECODE_ULONG(pNdrCtx, cb);
            if (cb != pResult->cbAttrLen)
            {
                NDR_ERROR_BREAK;
            }

            pResult->pbAttr = (uint8_t *)VRDPMemAlloc(cb);
            if (pResult->pbAttr == NULL)
            {
                NDR_ERROR_BREAK;
            }

            NDR_DECODE_BYTES(pNdrCtx, pResult->pbAttr, cb, cb);
        }

    NDR_END

    return NDR_IS_SUCCESS;
}


/*
 * Generic encode/decode routines.
 */

int VRDPSCard::scDecode(const uint8_t *pu8Buffer,
                        uint32_t cbBuffer,
                        uint32_t u32ReturnedOutputLength,
                        PFNSCARDDECODE pfnDecode,
                        void *pvResult,
                        uint32_t cbResult)
{
    memset(pvResult, 0, cbResult);

    if (u32ReturnedOutputLength > cbBuffer)
    {
        return VERR_TOO_MUCH_DATA;
    }

    VRDPSCARDNDRCTX ndr;

    int rc = NDRContextInit(&ndr, pu8Buffer, u32ReturnedOutputLength, /* fDecode = */ true);

    if (RT_SUCCESS(rc))
    {
        rc = NDRContextDecodeStart(&ndr);

        if (RT_SUCCESS(rc))
        {
            bool fDecoded = pfnDecode(&ndr, pvResult, cbResult);
            if (!fDecoded)
            {
                rc = VERR_INVALID_PARAMETER;
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        SCARDLOG(("scDecode failed %Rrc\n", rc));
    }

    return rc;
}

int VRDPSCard::scEncode(uint8_t **ppu8Buffer,
                        uint32_t *pcbBuffer,
                        uint32_t cbEncodedEstimate,
                        PFNSCARDENCODE pfnEncode,
                        const void *pvParms,
                        uint32_t cbParms)
{
    uint32_t cbAlloc = cbEncodedEstimate;
    uint8_t *pu8Buffer = (uint8_t *)VRDPMemAlloc(cbAlloc);

    if (!pu8Buffer)
    {
        return VERR_NO_MEMORY;
    }

    VRDPSCARDNDRCTX ndr;

    int rc = NDRContextInit(&ndr, pu8Buffer, cbAlloc, /* fDecode = */ false);

    if (RT_SUCCESS(rc))
    {
        rc = NDRContextEncodeStart(&ndr);

        if (RT_SUCCESS(rc))
        {
            bool fEncoded = pfnEncode(&ndr, pvParms, cbParms);
            if (fEncoded)
            {
                NDRContextEncodeFinish(&ndr);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        *ppu8Buffer = ndr.pu8Buffer;
        *pcbBuffer = NDRContextGetBytesProcessed(&ndr);
    }
    else
    {
        SCARDLOG(("scEncode failed %Rrc\n", rc));
        VRDPMemFree(ndr.pu8Buffer);
        ndr.pu8Buffer = NULL;
    }

    return rc;
}

static int scStatusToRC(uint32_t u32IoStatus)
{
    switch(u32IoStatus)
    {
        case RDPDR_STATUS_SUCCESS: return VINF_SUCCESS;
        default:
            break;
    }

    return VERR_INVALID_PARAMETER;
}

int VRDPSCard::scProcessIOResponse(VRDPSCARDDEVICE *pDev, RDPDRIOCTL *pIO)
{
    int rc = VINF_SUCCESS;

    SCIOCTL *pSCIO = (SCIOCTL *)pIO;

    SCARDLOG(("scProcessIOResponse: pvUser: %p, u32VRDEFunction %d\n",
              pSCIO->pvUser, pSCIO->u32VRDEFunction));

    switch(pIO->u32Function)
    {
        case SCARD_IOCTL_ESTABLISHCONTEXT:
        {
            SCEstablishContext_Return ret;
            VRDESCARDESTABLISHCONTEXTRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeEstablishContext_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_ESTABLISHCONTEXT: return code 0x%08X\n",
                          ret.ReturnCode));

                if (pDev->enmCtxStatusGetStatusChange == SC_CONTEXT_REQUESTING)
                {
                    /* Response to the GetStatusChange context creation. */
                    if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                    {
                        pDev->enmCtxStatusGetStatusChange = SC_CONTEXT_ESTABLISHED;
                        pDev->contextGetStatusChange = ret.Context;

                        /* Create the operations context. */
                        SCEstablishContext_Call parms;
                        parms.dwScope = SCARD_SCOPE_SYSTEM;
                        rc = scSubmitIOCTL(SCARD_IOCTL_ESTABLISHCONTEXT, pDev, &parms, sizeof(parms),
                                           pSCIO->pvUser, pSCIO->u32VRDEFunction);

                        pDev->enmCtxStatus = SC_CONTEXT_REQUESTING;
                    }
                    else
                    {
                        pDev->enmCtxStatusGetStatusChange = SC_CONTEXT_NULL;

                        /* Make the error response. */
                        rsp.u32ReturnCode = ret.ReturnCode;
                        RT_ZERO(rsp.Context);

                        scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));
                    }

                    break;
                }

                /* GetStatusChange context was created. */
                Assert(pDev->enmCtxStatusGetStatusChange == SC_CONTEXT_ESTABLISHED);
                Assert(pDev->enmCtxStatus == SC_CONTEXT_REQUESTING);

                rsp.u32ReturnCode = ret.ReturnCode;
                RT_ZERO(rsp.Context);

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    pDev->enmCtxStatus = SC_CONTEXT_ESTABLISHED;
                    pDev->context = ret.Context;

                    rsp.Context.u32ContextSize = ret.Context.cbCtx;
                    memcpy(rsp.Context.au8Context, ret.Context.au8Ctx, rsp.Context.u32ContextSize);
                }
                else
                {
                    pDev->enmCtxStatus = SC_CONTEXT_NULL;

                    pDev->enmCtxStatusGetStatusChange = SC_CONTEXT_RELEASING;

                    /* Delete already created GetStatusChange context. */
                    SCContext_Call parms;
                    parms.Context = pDev->contextGetStatusChange;
                    /* Last parameter 0 because it is an internally issued request. */
                    rc = scSubmitIOCTL(SCARD_IOCTL_RELEASECONTEXT, pDev, &parms, sizeof(parms),
                                       &pDev->contextGetStatusChange, 0);
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_LISTREADERSW:
        {
            SCListReaders_Return ret;
            VRDESCARDLISTREADERSRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeListReaders_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_LISTREADERSW: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
                RT_ZERO(rsp.apszNames);

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    /** @todo ->cReaders and paNames[10]. */
                    rsp.cReaders = 1;
                    rsp.apszNames[0] = ret.msz;
                }
                else
                {
                    rsp.cReaders = 0;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Deallocate everything. */
            VRDPMemFree(ret.msz);
        } break;

        case SCARD_IOCTL_RELEASECONTEXT:
        {
            /*
             * Check which context is being released.
             */
            bool fGetStatusChangeContext;

            if (   pSCIO->u32VRDEFunction != 0
                || pSCIO->pvUser == (void *)&pDev->contextGetStatusChange)
            {
                /* If the function is not zero, then the GetStatusChange context is being released.
                 * And if zero, then just compare the pointer.
                 */
                Assert(pDev->enmCtxStatusGetStatusChange == SC_CONTEXT_RELEASING);
                pDev->enmCtxStatusGetStatusChange = SC_CONTEXT_NULL;

                fGetStatusChangeContext = true;
            }
            else
            {
                /* Operations context. */
                Assert(pSCIO->pvUser == (void *)&pDev->context);

                Assert(pDev->enmCtxStatus == SC_CONTEXT_RELEASING);
                pDev->enmCtxStatus = SC_CONTEXT_NULL;

                fGetStatusChangeContext = false;
            }

            SCLong_Return ret;
            VRDESCARDRELEASECONTEXTRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeLong_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_RELEASECONTEXT: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
            }

            if (fGetStatusChangeContext)
            {
                /* This is only done for the GetStatusChange context. */
                scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

                if (m_fShutdown)
                {
                    RTThreadUserSignal(RTThreadSelf());
                }
            }

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_GETSTATUSCHANGEW:
        {
            SCGetStatusChange_Return ret;
            VRDESCARDGETSTATUSCHANGERSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeGetStatusChange_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_GETSTATUSCHANGEW: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
                RT_ZERO(rsp.aReaderStates);

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    rsp.cReaders = ret.cReaders > VRDE_SCARD_MAX_READERS?
                                       VRDE_SCARD_MAX_READERS:
                                       ret.cReaders;

                    uint32_t i;
                    for (i = 0; i < rsp.cReaders; i++)
                    {
                        rsp.aReaderStates[i].u32CurrentState = ret.rgReaderStates[i].dwCurrentState;
                        rsp.aReaderStates[i].u32EventState = ret.rgReaderStates[i].dwEventState;
                        rsp.aReaderStates[i].u32AtrLength =  ret.rgReaderStates[i].cbAtr > VRDE_SCARD_MAX_ATR_LENGTH?
                                                                 VRDE_SCARD_MAX_ATR_LENGTH:
                                                                 ret.rgReaderStates[i].cbAtr;
                        memcpy(rsp.aReaderStates[i].au8Atr, ret.rgReaderStates[i].rgbAtr, rsp.aReaderStates[i].u32AtrLength);
                    }
                }
                else
                {
                    rsp.cReaders = 0;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_CANCEL:
        {
            SCLong_Return ret;
            VRDESCARDCANCELRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeLong_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_CANCEL: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_CONNECTW:
        {
            SCConnect_Return ret;
            VRDESCARDCONNECTRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeConnect_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_CONNECT: return code 0x%08X, dwActiveProtocol 0x%08X\n",
                          ret.ReturnCode, ret.dwActiveProtocol));

                rsp.u32ReturnCode = ret.ReturnCode;
                RT_ZERO(rsp.hCard);

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    if (ret.hCard.Context.cbCtx)
                    {
                        rsp.hCard.Context.u32ContextSize = ret.hCard.Context.cbCtx;
                        memcpy(rsp.hCard.Context.au8Context, ret.hCard.Context.au8Ctx, rsp.hCard.Context.u32ContextSize);
                    }
                    else
                    {
                        /* MSFT client replies with a NULL context. */
                        RT_ZERO(rsp.hCard.Context);
                        rsp.hCard.Context.u32ContextSize = pDev->context.cbCtx;
                        memcpy(rsp.hCard.Context.au8Context, pDev->context.au8Ctx, pDev->context.cbCtx);
                    }

                    rsp.hCard.u32HandleSize = ret.hCard.cbHandle;
                    memcpy(rsp.hCard.au8Handle, ret.hCard.pbHandle, rsp.hCard.u32HandleSize);

                    rsp.u32ActiveProtocol = ret.dwActiveProtocol;
                }
                else
                {
                    rsp.u32ActiveProtocol = 0;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_RECONNECT:
        {
            SCReconnect_Return ret;
            VRDESCARDRECONNECTRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeReconnect_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_RECONNECT: return code 0x%08X, dwActiveProtocol 0x%08X\n",
                          ret.ReturnCode, ret.dwActiveProtocol));

                rsp.u32ReturnCode = ret.ReturnCode;

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    rsp.u32ActiveProtocol = ret.dwActiveProtocol;
                }
                else
                {
                    rsp.u32ActiveProtocol = 0;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_DISCONNECT:
        {
            SCLong_Return ret;
            VRDESCARDDISCONNECTRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeLong_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_DISCONNECT: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_BEGINTRANSACTION:
        {
            SCLong_Return ret;
            VRDESCARDBEGINTRANSACTIONRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeLong_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_BEGINTRANSACTION: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_ENDTRANSACTION:
        {
            SCLong_Return ret;
            VRDESCARDENDTRANSACTIONRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeLong_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_ENDTRANSACTION: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_STATE:
        {
            SCState_Return ret;
            VRDESCARDSTATERSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeState_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_STATE: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
                RT_ZERO(rsp.au8Atr);

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    rsp.u32State = ret.dwState;
                    rsp.u32Protocol = ret.dwProtocol;
                    rsp.u32AtrLength = ret.cbAtrLen > VRDE_SCARD_MAX_ATR_LENGTH?
                                           VRDE_SCARD_MAX_ATR_LENGTH:
                                           ret.cbAtrLen;
                    memcpy(rsp.au8Atr, ret.rgAtr, rsp.u32AtrLength);
                }
                else
                {
                    rsp.u32State = 0;
                    rsp.u32Protocol = 0;
                    rsp.u32AtrLength = 0;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        case SCARD_IOCTL_STATUSW:
        {
            SCStatus_Return ret;
            VRDESCARDSTATUSRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeStatus_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_STATUS: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
                RT_ZERO(rsp.au8Atr);

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    rsp.szReader = ret.mszReaderNames; /* Return one name. */
                    rsp.u32State = ret.dwState;
                    rsp.u32Protocol = ret.dwProtocol;
                    rsp.u32AtrLength = ret.cbAtrLen > VRDE_SCARD_MAX_ATR_LENGTH?
                                           VRDE_SCARD_MAX_ATR_LENGTH:
                                           ret.cbAtrLen;
                    memcpy(rsp.au8Atr, ret.pbAtr, rsp.u32AtrLength);
                }
                else
                {
                    rsp.szReader = NULL;
                    rsp.u32State = 0;
                    rsp.u32Protocol = 0;
                    rsp.u32AtrLength = 0;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Deallocate everything. */
            VRDPMemFree(ret.mszReaderNames);
        } break;

        case SCARD_IOCTL_TRANSMIT:
        {
            SCTransmit_Return ret;
            VRDESCARDTRANSMITRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeTransmit_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_TRANSMIT: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
                RT_ZERO(rsp.ioRecvPci);

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    rsp.ioRecvPci.u32Protocol = ret.pioRecvPci.dwProtocol;
                    rsp.ioRecvPci.u32PciLength = ret.pioRecvPci.cbExtraBytes > VRDE_SCARD_MAX_PCI_DATA?
                                                       VRDE_SCARD_MAX_PCI_DATA:
                                                       ret.pioRecvPci.cbExtraBytes;
                    memcpy(rsp.ioRecvPci.au8PciData, ret.pioRecvPci.pbExtraBytes, rsp.ioRecvPci.u32PciLength);
                    rsp.ioRecvPci.u32PciLength += 2 * sizeof(uint32_t);

                    rsp.u32RecvLength = ret.cbRecvLength;
                    rsp.pu8RecvBuffer = ret.pbRecvBuffer;
                }
                else
                {
                    rsp.u32RecvLength = 0;
                    rsp.pu8RecvBuffer = NULL;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Deallocate everything. */
            VRDPMemFree(ret.pbRecvBuffer);
        } break;

        case SCARD_IOCTL_CONTROL:
        {
            SCControl_Return ret;
            VRDESCARDCONTROLRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeControl_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_CONTROL: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    rsp.u32OutBufferSize = ret.cbOutBufferSize;
                    rsp.pu8OutBuffer = ret.pvOutBuffer;
                }
                else
                {
                    rsp.u32OutBufferSize = 0;
                    rsp.pu8OutBuffer = NULL;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Deallocate everything. */
            VRDPMemFree(ret.pvOutBuffer);
        } break;

        case SCARD_IOCTL_GETATTRIB:
        {
            SCGetAttrib_Return ret;
            VRDESCARDGETATTRIBRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeGetAttrib_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_GETATTRIB: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;

                if (ret.ReturnCode == RDPDR_STATUS_SUCCESS)
                {
                    rsp.u32AttrLength = ret.cbAttrLen;
                    rsp.pu8Attr = ret.pbAttr;
                }
                else
                {
                    rsp.u32AttrLength = 0;
                    rsp.pu8Attr = NULL;
                }
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Deallocate everything. */
            VRDPMemFree(ret.pbAttr);
        } break;

        case SCARD_IOCTL_SETATTRIB:
        {
            SCLong_Return ret;
            VRDESCARDSETATTRIBRSP rsp;

            rc = scDecode(pIO->pu8OutputBuffer,
                          pIO->u32OutputBufferSize,
                          pIO->u32ReturnedOutputLength,
                          scDecodeLong_Return,
                          &ret, sizeof(ret));

            if (RT_SUCCESS(rc))
            {
                SCARDLOG(("SCARD_IOCTL_SETATTRIB: return code 0x%08X\n",
                          ret.ReturnCode));

                rsp.u32ReturnCode = ret.ReturnCode;
            }

            scCallbackResponse(rc, pSCIO->pvUser, pSCIO->u32VRDEFunction, &rsp, sizeof(rsp));

            /* Nothing to deallocate. */
        } break;

        /* These are not used by VRDP. */
        case SCARD_IOCTL_ISVALIDCONTEXT:
        case SCARD_IOCTL_LISTREADERGROUPSA:
        case SCARD_IOCTL_LISTREADERGROUPSW:
        case SCARD_IOCTL_LISTREADERSA:
        case SCARD_IOCTL_INTRODUCEREADERGROUPA:
        case SCARD_IOCTL_INTRODUCEREADERGROUPW:
        case SCARD_IOCTL_FORGETREADERGROUPA:
        case SCARD_IOCTL_FORGETREADERGROUPW:
        case SCARD_IOCTL_INTRODUCEREADERA:
        case SCARD_IOCTL_INTRODUCEREADERW:
        case SCARD_IOCTL_FORGETREADERA:
        case SCARD_IOCTL_FORGETREADERW:
        case SCARD_IOCTL_ADDREADERTOGROUPA:
        case SCARD_IOCTL_ADDREADERTOGROUPW:
        case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
        case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
        case SCARD_IOCTL_LOCATECARDSA:
        case SCARD_IOCTL_LOCATECARDSW:
        case SCARD_IOCTL_GETSTATUSCHANGEA:
        case SCARD_IOCTL_CONNECTA:
        case SCARD_IOCTL_STATUSA:
        case SCARD_IOCTL_ACCESSSTARTEDEVENT:
        case SCARD_IOCTL_LOCATECARDSBYATRA:
        case SCARD_IOCTL_LOCATECARDSBYATRW:
        case SCARD_IOCTL_READCACHEA:
        case SCARD_IOCTL_READCACHEW:
        case SCARD_IOCTL_WRITECACHEA:
        case SCARD_IOCTL_WRITECACHEW:
        case SCARD_IOCTL_GETTRANSMITCOUNT:
        case SCARD_IOCTL_RELEASETARTEDEVENT:
        default:
            AssertFailed();
            rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

/** @thread INPUT */
int VRDPSCard::SCardIOCompletion(uint32_t u32ClientId,
                                 uint32_t u32DeviceId,
                                 uint32_t u32IoStatus,
                                 RDPDRIOCTL *pIO)
{
    /* RDPDR informs an IO request was completed for the SmartCard device. */
    Assert(pIO->hdr.pktSize == sizeof(SCIOCTL));

    SCIOCTL *pSCIO = (SCIOCTL *)pIO;

    SCARDLOG(("SCardIOCompletion: [%d,%d] status 0x%08X, out %d, cb %d\n",
               u32ClientId, u32DeviceId, u32IoStatus, pIO->u32ReturnedOutputLength, pIO->u32OutputBufferSize));

    Assert(m_pServer->IsInputThread());

    VRDPSCARDDEVICE *pDev = scDeviceFind(u32ClientId, u32DeviceId);

    if (!pDev)
    {
        SCARDLOG(("SCardIOCompletion: no such device\n"));

        scCallbackResponse(VERR_INVALID_PARAMETER, pSCIO->pvUser, pSCIO->u32VRDEFunction, NULL, 0);

        /* Ignore the invalid packet. */
        RDPDRPktRelease(&pIO->hdr);
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;

    if (u32IoStatus == RDPDR_STATUS_BUFFER_TOO_SMALL)
    {
        /* The message SHOULD be retransmitted, doubling the previously requested buffer length. */
        SCARDLOG(("SCardIOCompletion: RDPDR_STATUS_BUFFER_TOO_SMALL for %d\n",
                   pIO->u32RequestedOutputLength));

        if (pIO->u32RequestedOutputLength < 256*_1K)
        {
            pIO->u32RequestedOutputLength *= 2;
            pIO->u32ReturnedOutputLength = 0;
            pIO->pu8OutputBuffer = NULL;
            pIO->u32OutputBufferSize  = 0;

            /* Give the buffer to RDPDR. */
            RDPDRPktAddRef(&pIO->hdr);

            RDPDROUT outdata;
            outdata.pPktHdr = &pIO->hdr;

            m_pServer->PostOutput(VRDP_OUTPUT_RDPDR, u32ClientId, &outdata, sizeof(outdata));
        }
    }
    else
    {
        if (u32IoStatus == RDPDR_STATUS_SUCCESS)
        {
            if (pIO->u32OutputBufferSize >= pIO->u32ReturnedOutputLength)
            {
                rc = scProcessIOResponse(pDev, pIO);
            }
            else
            {
                /* Ignore the invalid packet. */
                SCARDLOG(("SCardIOCompletion: returned length is invalid: %d (expected max %d)\n",
                          pIO->u32ReturnedOutputLength, pIO->u32OutputBufferSize));

                scCallbackResponse(VERR_INVALID_PARAMETER, pSCIO->pvUser, pSCIO->u32VRDEFunction, NULL, 0);

                rc = VERR_INVALID_PARAMETER;
            }
        }
        else
        {
            SCARDLOG(("SCardIOCompletion: failure u32IoStatus: 0x%08X\n",
                      u32IoStatus));

            scCallbackResponse(scStatusToRC(u32IoStatus), pSCIO->pvUser, pSCIO->u32VRDEFunction, NULL, 0);
        }
    }

    RDPDRPktRelease(&pIO->hdr);
    scDeviceRelease(pDev);
    return rc;
}

int VRDPSCard::scEncodeIOCTL(uint8_t **ppu8Buffer,
                             uint32_t *pcbBuffer,
                             uint32_t u32Code,
                             const void *pvParms,
                             uint32_t cbParms)
{
    int rc = VINF_SUCCESS;

    uint32_t cbEncodedEstimate = 0;
    uint32_t cbBuffer = 0;
    uint8_t *pu8Buffer = NULL;
    /* uint8_t *pu8Encoded = NULL; - unused */

    switch(u32Code)
    {
        case SCARD_IOCTL_ESTABLISHCONTEXT:
        {
            cbEncodedEstimate = sizeof(sRpceHdr) + sizeof(RPCETYPEHDR) + 2 * sizeof(uint32_t);

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeEstablishContext_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_LISTREADERSW:
        {
            cbEncodedEstimate = 4096;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeListReaders_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_RELEASECONTEXT:
        case SCARD_IOCTL_CANCEL:
        {
            cbEncodedEstimate = 256;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeContext_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_GETSTATUSCHANGEW:
        {
            cbEncodedEstimate = 4096;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeGetStatusChangeW_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_CONNECTW:
        {
            cbEncodedEstimate = 4096;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeConnectW_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_RECONNECT:
        {
            cbEncodedEstimate = 256;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeReconnect_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_DISCONNECT:
        case SCARD_IOCTL_BEGINTRANSACTION:
        case SCARD_IOCTL_ENDTRANSACTION:
        {
            cbEncodedEstimate = 256;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeHCardAndDisposition_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_STATE:
        {
            cbEncodedEstimate = 256;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeState_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_STATUSW:
        {
            cbEncodedEstimate = 256;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeStatus_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_TRANSMIT:
        {
            cbEncodedEstimate = 68 * _1K;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeTransmit_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_CONTROL:
        {
            cbEncodedEstimate = 66 * _1K;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeControl_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_GETATTRIB:
        {
            cbEncodedEstimate = 256;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeGetAttrib_Call, pvParms, cbParms);
        } break;

        case SCARD_IOCTL_SETATTRIB:
        {
            cbEncodedEstimate = 66 * _1K;

            rc = scEncode(&pu8Buffer, &cbBuffer, cbEncodedEstimate,
                          scEncodeSetAttrib_Call, pvParms, cbParms);
        } break;

        /* These are not used by VRDP. */
        case SCARD_IOCTL_ISVALIDCONTEXT:
        case SCARD_IOCTL_LISTREADERGROUPSA:
        case SCARD_IOCTL_LISTREADERGROUPSW:
        case SCARD_IOCTL_LISTREADERSA:
        case SCARD_IOCTL_INTRODUCEREADERGROUPA:
        case SCARD_IOCTL_INTRODUCEREADERGROUPW:
        case SCARD_IOCTL_FORGETREADERGROUPA:
        case SCARD_IOCTL_FORGETREADERGROUPW:
        case SCARD_IOCTL_INTRODUCEREADERA:
        case SCARD_IOCTL_INTRODUCEREADERW:
        case SCARD_IOCTL_FORGETREADERA:
        case SCARD_IOCTL_FORGETREADERW:
        case SCARD_IOCTL_ADDREADERTOGROUPA:
        case SCARD_IOCTL_ADDREADERTOGROUPW:
        case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
        case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
        case SCARD_IOCTL_LOCATECARDSA:
        case SCARD_IOCTL_LOCATECARDSW:
        case SCARD_IOCTL_GETSTATUSCHANGEA:
        case SCARD_IOCTL_CONNECTA:
        case SCARD_IOCTL_STATUSA:
        case SCARD_IOCTL_ACCESSSTARTEDEVENT:
        case SCARD_IOCTL_LOCATECARDSBYATRA:
        case SCARD_IOCTL_LOCATECARDSBYATRW:
        case SCARD_IOCTL_READCACHEA:
        case SCARD_IOCTL_READCACHEW:
        case SCARD_IOCTL_WRITECACHEA:
        case SCARD_IOCTL_WRITECACHEW:
        case SCARD_IOCTL_GETTRANSMITCOUNT:
        case SCARD_IOCTL_RELEASETARTEDEVENT:
        default:
            AssertFailed();
            rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        *ppu8Buffer = pu8Buffer;
        *pcbBuffer = cbBuffer;
    }
    else
    {
        VRDPMemFree(pu8Buffer);
    }

    return rc;
}

static void scardIOCTLDelete(RDPDRPKTHDR *pHdr)
{
    Assert(pHdr->pktSize == sizeof(SCIOCTL));
    SCIOCTL *pSCIO = (SCIOCTL *)pHdr;
    VRDPMemFree(pSCIO->io.pu8InputBuffer);
    pSCIO->io.pu8InputBuffer = NULL;
    return;
}

/* Prepare an async RDPDR IOCTL call and pass it to the OUTPUT thread.
 * The response from the RDP client will be received later on INPUT thread
 * and processed in VRDPSCard::SCardIOCompletion.
 *
 * @thread INPUT
 */
int VRDPSCard::scSubmitIOCTL(uint32_t u32Code,
                             VRDPSCARDDEVICE *pDev,
                             const void *pvParms,
                             uint32_t cbParms,
                             void *pvUser,
                             uint32_t u32VRDEFunction)
{
    SCARDLOG(("scSubmitIOCTL: 0x%08X %p %p %d %p\n",
               u32Code, pDev, pvParms, cbParms, pvUser));

    /*
     * Encode parameters.
     */
    uint8_t *pu8Buffer = NULL;
    uint32_t cbBuffer = 0;

    int rc = scEncodeIOCTL(&pu8Buffer, &cbBuffer, u32Code, pvParms, cbParms);

    if (RT_SUCCESS(rc))
    {
        /*
         * Prepare RDPDR IOCTL structure to send encoded parameters.
         */
        SCIOCTL *pSCIO = (SCIOCTL *)VRDPMemAlloc(sizeof(SCIOCTL));

        RDPDRPktInit(&pSCIO->io.hdr, RDPDR_OUT_IOCTL, sizeof(SCIOCTL), scardIOCTLDelete, pDev->u32DeviceId);

        pSCIO->io.pvCtx                    = this;
        pSCIO->io.u32Function              = u32Code;
        pSCIO->io.u32InputBufferLength     = cbBuffer;
        pSCIO->io.pu8InputBuffer           = pu8Buffer;
        pSCIO->io.u32InputBufferSize       = pSCIO->io.u32InputBufferLength;
        pSCIO->io.u32RequestedOutputLength = 2048;
        pSCIO->io.u32ReturnedOutputLength  = 0;
        pSCIO->io.pu8OutputBuffer          = NULL;
        pSCIO->io.u32OutputBufferSize      = 0;

        pSCIO->pvUser          = pvUser;
        pSCIO->u32VRDEFunction = u32VRDEFunction;

        /* Give the buffer to RDPDR. */
        RDPDRPktAddRef(&pSCIO->io.hdr);

        RDPDROUT outdata;
        outdata.pPktHdr = &pSCIO->io.hdr;

        m_pServer->PostOutput(VRDP_OUTPUT_RDPDR, pDev->u32ClientId, &outdata, sizeof(outdata));
    }

    return rc;
}

int VRDPSCard::scCallbackNotify(uint32_t u32Id,
                                void *pvData,
                                uint32_t cbData)
{
    SCARDLOG(("scCallbackNotify: %d %p %d\n",
               u32Id, pvData, cbData));

    if (m_interface.callbacks.VRDESCardCbNotify)
    {
        return m_interface.callbacks.VRDESCardCbNotify(m_interface.pvContext,
                                                       u32Id,
                                                       pvData,
                                                       cbData);
    }

    return VERR_NOT_SUPPORTED;
}

int VRDPSCard::scCallbackResponse(int rcRequest,
                                  void *pvUser,
                                  uint32_t u32VRDEFunction,
                                  void *pvData,
                                  uint32_t cbData)
{
    SCARDLOG(("scCallbackResponse: %Rrc %p %d %p %d\n",
               rcRequest, pvUser, u32VRDEFunction, pvData, cbData));

    if (u32VRDEFunction == 0)
    {
        /* This was an internal IO, no need for notification. */
        return VINF_SUCCESS;
    }

    if (m_interface.callbacks.VRDESCardCbResponse)
    {
        if (RT_FAILURE(rcRequest))
        {
            /* Do not give random memory to the caller. */
            pvData = NULL;
            cbData = 0;
        }

        return m_interface.callbacks.VRDESCardCbResponse(m_interface.pvContext,
                                                         rcRequest,
                                                         pvUser,
                                                         u32VRDEFunction,
                                                         pvData,
                                                         cbData);
    }

    return VERR_NOT_SUPPORTED;
}
