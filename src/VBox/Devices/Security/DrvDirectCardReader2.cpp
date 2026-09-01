/* $Id: DrvDirectCardReader2.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * DrvDirectCardReader - Test USB Card Reader Driver allowing access
 *                       direct to the device attached to the host.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP   LOG_GROUP_USB_CARDREADER
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/log.h>
#include <iprt/errcore.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <VBox/version.h>
#include <iprt/req.h>
#include "vusbcardreader.h"
#include "UsbCardReaderDefs.h"

#ifndef RT_OS_WINDOWS
# include <PCSC/wintypes.h>
# include <PCSC/winscard.h>
# include <PCSC/reader.h>
# include <wchar.h>
#else /* RT_OS_WINDOWS */
# include <winscard.h>
#endif
#include <VBox/vmm/pdmcardreaderinfs.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct CARDREADERSTATE
{
    /* for cases emulation slots for slots of several real devices we will it
     * will depend on device having this slot, for rest cases it will be just the same for all slots.
     */
    SCARDCONTEXT hSCardReaderCtx;
    SCARDHANDLE  hCardHandle;
    char        *pszCardReaderName;
    void        *pvDevData;
} CARDREADERSTATE, *PCARDREADERSTATE;

typedef struct DRVCARDREADER
{
    PPDMDRVINS          pDrvIns;
    PDMICARDREADERDOWN  ICardReaderDown;
    PPDMICARDREADERUP   pICardReaderUp;
    CARDREADERSTATE     CardReaderState;
    /* Thread handling Cmd to card reader */
    PPDMTHREAD          pThrCardReaderCmd;
    /* Queue handling requests to cardreader */
    RTREQQUEUE          hReqQCardReaderCmd;
    /* Semaphore for sync Cmd Thread */
    RTSEMEVENT          EvtCardReaderCmd;
#if 0
    /* Thread for event handling */
    PPDMTHREAD          pThrCardReaderEvt;
    /* Queue for handling events on Event Thread */
    RTREQQUEUE          hReqQCardReaderEvt;
    /* Semaphore  for sync Event Thread */
    RTSEMEVENT          EvtCardReaderEvt;
#endif
} DRVCARDREADER, *PDRVCARDREADER;



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) drvCardReaderThrCmdWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread);
#if 0
static DECLCALLBACK(int) drvCardReaderThrEvtWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread);
#endif


/* Event queue's callbacks */
static DECLCALLBACK(void) drvCardReaderEvtQueueStatusChange(PDRVCARDREADER pThis, void *pvUser, uint32_t dwTimeout,
                                                            PDMICARDREADER_READERSTATE *pPdmReaderStats, uint32_t cReaderStats)
{
    LogFlowFunc(("ENTER: pvUser:%p, dwTimeout:%d\n", pvUser, dwTimeout));
    LONG cRc = SCARD_S_SUCCESS;
    LPSCARD_READERSTATE pScardReaderStats = (LPSCARD_READERSTATE)RTMemAllocZ(sizeof(SCARD_READERSTATE) * cReaderStats);
    if (pScardReaderStats)
    {
        uint32_t idxStats;
        for (idxStats = 0; idxStats < cReaderStats; ++idxStats)
        {
            pScardReaderStats[idxStats].szReader = pPdmReaderStats[idxStats].pszReaderName;
            pScardReaderStats[idxStats].dwCurrentState = pPdmReaderStats[idxStats].u32CurrentState;
            pScardReaderStats[idxStats].dwEventState = pPdmReaderStats[idxStats].u32EventState;
        }
        cRc = SCardGetStatusChange(pThis->CardReaderState.hSCardReaderCtx,
                                    dwTimeout == RT_INDEFINITE_WAIT ? INFINITE : dwTimeout,
                                    pScardReaderStats,
                                    cReaderStats);
        for (idxStats = 0; idxStats < cReaderStats; ++idxStats)
        {
            pPdmReaderStats[idxStats].u32CurrentState = pScardReaderStats[idxStats].dwCurrentState;
            pPdmReaderStats[idxStats].u32EventState = pScardReaderStats[idxStats].dwEventState;
            pPdmReaderStats[idxStats].cbAtr = pScardReaderStats[idxStats].cbAtr;
            memcpy(pPdmReaderStats[idxStats].au8Atr, pScardReaderStats[idxStats].rgbAtr,
                   RT_MIN(pScardReaderStats[idxStats].cbAtr, 36));
        }
        LogFunc(("SCardGetStatusChange returns %R[scardRc]\n", cRc));
        pThis->pICardReaderUp->pfnSetStatusChange(pThis->pICardReaderUp, pvUser, cRc, pPdmReaderStats, cReaderStats);
        RTMemFree(pScardReaderStats);
    }
    else
        cRc = VBOX_SCARD_E_NO_MEMORY;
    LogFlowFuncLeave();
}

/* Command queue's callbacks */

/**
 * This is downcall for establishing cardreader context.
 */
static DECLCALLBACK(int) drvCardReaderCmdEstablishContext(PDRVCARDREADER pThis)
{
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    LogFlowFuncEnter();
    LONG lSCardRc = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &pThis->CardReaderState.hSCardReaderCtx);
    int rc = pThis->pICardReaderUp->pfnEstablishContext(pThis->pICardReaderUp,
                                                        lSCardRc);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(void) drvCardReaderCmdQueueStatus(PDRVCARDREADER pThis,
                                                      void *pvUser,
                                                      uint32_t cchReaderName,
                                                      uint32_t cbArtLen)
{
    LogFlowFunc(("ENTER: pvUser:%p\n", pvUser));
    Assert(pvUser);
#if 0
    LONG lSCardRc = SCARD_S_SUCCESS;
    DWORD   dwState;
    DWORD   dwProtocol;
    /**
     * @todo: http://msdn.microsoft.com/en-us/library/windows/desktop/aa379803%28v=vs.85%29.aspx doesn't specify
     * whether pszReaderName might be NULL or not
     */
    LPSTR   pszReaderName = NULL;
    LPBYTE  pbArt = NULL;
    if (   cchReaderName != VBOX_STATUS_NO_NAME
        && cchReaderName != SCARD_AUTOALLOCATE)
    {
        /** @todo check on Windows this allocation */
        pszReaderName = (LPSTR)RTMemAllocZ(cchReaderName * sizeof(char));
        lSCardRc = SCARD_E_NO_MEMORY;
        AssertPtrReturnVoid(pszReaderName);
    }
    if (   cbArtLen != VBOX_STATUS_NO_ATR
        && cbArtLen != SCARD_AUTOALLOCATE)
    {
        pbArt = (LPBYTE)RTMemAllocZ(cbArtLen);
        lSCardRc = SCARD_E_NO_MEMORY;
        AssertPtrReturnVoid(pbArt);
    }
    lSCardRc = SCardStatus(pThis->CardReaderState.hCardHandle,
                      pszReaderName,
                      cchReaderName != VBOX_STATUS_NO_NAME ? &cchReaderName : NULL,
                      &dwState,
                      &dwProtocol,
                      pbArt,
                      cbArtLen != VBOX_STATUS_NO_ATR ? &cbArtLen : NULL);
    int rc = pThis->pICardReaderUp->pfnStatus(pThis->pICardReaderUp,
                                              pvUser,
                                              lSCardRc,
                                              pszReaderName,
                                              cchReaderName,
                                              dwState,
                                              dwProtocol,
                                              pbArt,
                                              cbArtLen);
    AssertRC((rc));
    if (pszReaderName)
    {
        if (cchReaderName == SCARD_AUTOALLOCATE)
            SCardFreeMemory(pThis->CardReaderState.hSCardReaderCtx, pszReaderName);
        else
            RTMemFree(pszReaderName);
    }
    if (pbArt)
    {
        if (cbArtLen == SCARD_AUTOALLOCATE)
            SCardFreeMemory(pThis->CardReaderState.hSCardReaderCtx, pbArt);
        else
            RTMemFree(pbArt);
    }
#else
    AssertMsgFailed(("Later!"));
#endif
    LogFlowFuncLeave();
}

static DECLCALLBACK(void) drvCardReaderCmdQueueConnect(PDRVCARDREADER pThis, const char *pcszCardReaderName, void *pvUser,
                                                       uint32_t dwSharedMode, uint32_t dwPrefferedProtocols)
{
    LogFlowFunc(("ENTER: pcszCardReaderName:%s, pvUser:%p, dwSharedMode:%RX32, dwPrefferedProtocols:%RX32\n", pcszCardReaderName, pvUser, dwSharedMode, dwPrefferedProtocols));
    Assert(pvUser);
    uint32_t dwActiveProtocol = SCARD_PROTOCOL_T0;
    LONG lSCardRc = SCARD_S_SUCCESS;
    PCARDREADERSTATE pCardReaderState = &pThis->CardReaderState;
    lSCardRc = SCardConnect(pCardReaderState->hSCardReaderCtx,
                            pcszCardReaderName,
                            dwSharedMode,
                            dwPrefferedProtocols,
                            &pCardReaderState->hCardHandle,
                            (LPDWORD)&dwActiveProtocol);
    pCardReaderState->pszCardReaderName = RTStrDup(pcszCardReaderName);
    int rc = pThis->pICardReaderUp->pfnConnect(pThis->pICardReaderUp, pvUser, lSCardRc, dwActiveProtocol);
    AssertRC((rc));
    LogFlowFuncLeave();
}

static DECLCALLBACK(void) drvCardReaderCmdQueueDisconnect(PDRVCARDREADER pThis, void *pvUser, uint32_t dwMode)
{
    LogFlowFunc(("ENTER: pvUser:%p, dwMode:%RX32\n", pvUser, dwMode));
    PCARDREADERSTATE pCardReaderState = &pThis->CardReaderState;
    LONG cRc = SCardDisconnect(pCardReaderState->hCardHandle, dwMode);
    /** @todo protocol should be fixed */
    int rc = pThis->pICardReaderUp->pfnDisconnect(pThis->pICardReaderUp, pvUser, cRc);
    AssertRC((rc));
    LogFlowFuncLeave();
}

static DECLCALLBACK(void) drvCardReaderCmdTransmit(PDRVCARDREADER pThis,
                                                 void *pvUser,
                                                 LPSCARD_IO_REQUEST pioSendRequest,
                                                 LPBYTE pu8SendBuffer,
                                                 DWORD cbSendBuffer,
                                                 DWORD cbRecvBuffer)
{
    LogFlowFunc(("ENTER: pThis:%p, pvUser:%p, pioSendRequest:%R[scardioreq], pu8SendBuffer:%p, cbSendBuffer:%d, cbRecvBuffer:%d\n",
                 pThis, pvUser, pioSendRequest, pu8SendBuffer, cbSendBuffer, cbRecvBuffer));
    PCARDREADERSTATE pCardReaderState = &pThis->CardReaderState;
    SCARD_IO_REQUEST IoRecvPci;
    LPBYTE pu8RecvBuffer = NULL;
    AssertPtrReturnVoid(pioSendRequest);
    IoRecvPci.dwProtocol = pioSendRequest->dwProtocol;
    IoRecvPci.cbPciLength = sizeof(SCARD_IO_REQUEST);
    if (cbSendBuffer)
    {
        LogFunc(("pu8SendBuffer: %.*Rhxs\n", cbSendBuffer, pu8SendBuffer));
        AssertPtrReturnVoid(pu8SendBuffer);
    }
    if (cbRecvBuffer)
    {
        pu8RecvBuffer = (LPBYTE)RTMemAllocZ(cbRecvBuffer * sizeof(BYTE));
        AssertPtrReturnVoid(pu8RecvBuffer);
    }
    LONG lSCardRc = SCardTransmit(pCardReaderState->hCardHandle,
                                  pioSendRequest,
                                  pu8SendBuffer,
                                  cbSendBuffer,
                                  &IoRecvPci,
                                  pu8RecvBuffer,
                                  (LPDWORD)&cbRecvBuffer);

    PDMICARDREADER_IO_REQUEST PdmIoRecvPci;
    RT_ZERO(PdmIoRecvPci);
    PdmIoRecvPci.u32Protocol = IoRecvPci.dwProtocol;
    /** @todo for now assume SCardTransmit returns IoRecvPci without attachment */
    Assert(IoRecvPci.cbPciLength == sizeof(SCARD_IO_REQUEST));
    PdmIoRecvPci.cbPciLength = sizeof(PDMICARDREADER_IO_REQUEST);

    int rc = pThis->pICardReaderUp->pfnTransmit(pThis->pICardReaderUp,
                                                pvUser,
                                                lSCardRc,
                                                &PdmIoRecvPci,
                                                pu8RecvBuffer,
                                                (uint32_t)cbRecvBuffer);
    AssertRC((rc));
    /* Clean up buffers allocated by driver */
    if (pioSendRequest)
        RTMemFree(pioSendRequest);

    if (pu8SendBuffer)
        RTMemFree(pu8SendBuffer);

    if (pu8RecvBuffer)
    {
        LogFunc(("pu8RecvBuffer: %.*Rhxs\n", cbRecvBuffer, pu8RecvBuffer));
        RTMemFree(pu8RecvBuffer);
    }
    LogFlowFuncLeave();
}

static DECLCALLBACK(void) drvCardReaderCmdGetAttr(PDRVCARDREADER pThis, void *pvUser, uint32_t dwAttrId, uint32_t cbAttrib)
{
    LogFlowFunc(("ENTER: pThis:%p, pvUser:%p, dwAttrId:%R[scardAttr], cbAttrib:%d\n",
                 pThis, pvUser, dwAttrId, cbAttrib));
    PCARDREADERSTATE pCardReaderState = &pThis->CardReaderState;
    LPBYTE pvAttrib = NULL;
    bool fAutoAllocate = (cbAttrib == SCARD_AUTOALLOCATE);
    if (   cbAttrib != 0
        && !fAutoAllocate)
    {
        pvAttrib = (LPBYTE)RTMemAllocZ(cbAttrib);
        AssertPtrReturnVoid(pvAttrib);
    }
    LONG lSCardRc = SCardGetAttrib(pCardReaderState->hCardHandle,
                                   dwAttrId,
                                   pvAttrib,
                                   (LPDWORD)&cbAttrib);
    int rc = pThis->pICardReaderUp->pfnGetAttrib(pThis->pICardReaderUp,
                                                 pvUser,
                                                 lSCardRc,
                                                 dwAttrId,
                                                 cbAttrib ? pvAttrib : NULL,
                                                 cbAttrib);
    AssertRC((rc));
    if (pvAttrib)
    {
        if (fAutoAllocate)
            SCardFreeMemory(pThis->CardReaderState.hSCardReaderCtx, pvAttrib);
        else
            RTMemFree(pvAttrib);
    }
    LogFlowFuncLeave();
}

static DECLCALLBACK(void) drvCardReaderCmdSetAttr(PDRVCARDREADER pThis, void *pvUser, uint32_t dwAttrId, LPCBYTE pvAttrib, uint32_t cbAttrib)
{
    LogFlowFunc(("ENTER: pThis:%p, pvUser:%p, dwAttrId:%R[scardAttr], pvAttrib:%p, cbAttrib:%d\n",
                 pThis, pvUser, dwAttrId, pvAttrib, cbAttrib));
    PCARDREADERSTATE pCardReaderState = &pThis->CardReaderState;
    LONG lSCardRc = SCardSetAttrib(pCardReaderState->hCardHandle,
                                   dwAttrId,
                                   pvAttrib,
                                   cbAttrib);
    int rc = pThis->pICardReaderUp->pfnSetAttrib(pThis->pICardReaderUp,
                                                 pvUser,
                                                 lSCardRc,
                                                 dwAttrId);
    AssertRC((rc));
    if (pvAttrib)
        RTMemFree((void *)pvAttrib);
    LogFlowFuncLeave();
}

static DECLCALLBACK(void) drvCardReaderCmdControl(PDRVCARDREADER pThis,
                                                  void *pvUser,
                                                  uint32_t dwControlCode,
                                                  LPVOID pvInBuffer,
                                                  uint32_t cbInBuffer,
                                                  uint32_t cbOutBuffer)
{
    LogFlowFunc(("ENTER: pThis:%p, pvUser:%p, dwControlCode:%RX32, pvInBuffer:%p, cbInBuffer:%d, cbOutBuffer:%d\n",
                pThis, pvUser, dwControlCode, pvInBuffer, cbInBuffer, cbOutBuffer));
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pvUser);
    PCARDREADERSTATE pCardReaderState = &pThis->CardReaderState;
    LPVOID pvOutBuffer = NULL;
    if (cbOutBuffer)
    {
        pvOutBuffer = RTMemAllocZ(cbOutBuffer);
        AssertPtrReturnVoid(pvOutBuffer);
    }
    LONG lSCardRcT = SCardBeginTransaction(pCardReaderState->hCardHandle);
    Assert(lSCardRcT == SCARD_S_SUCCESS);
    LONG lSCardRc = SCardControl(pCardReaderState->hCardHandle,
                                 dwControlCode,
                                 pvInBuffer,
                                 cbInBuffer,
                                 pvOutBuffer,
                                 cbOutBuffer,
                                 (LPDWORD)&cbOutBuffer);
    lSCardRcT = SCardEndTransaction(pCardReaderState->hCardHandle, SCARD_LEAVE_CARD);
    Assert(lSCardRcT == SCARD_S_SUCCESS);
    int rc = pThis->pICardReaderUp->pfnControl(pThis->pICardReaderUp,
                                               pvUser,
                                               lSCardRc,
                                               dwControlCode,
                                               pvOutBuffer,
                                               cbOutBuffer);
    AssertRC((rc));
    if (pvInBuffer)
        RTMemFree(pvInBuffer);
    if (pvOutBuffer)
        RTMemFree((void *)pvOutBuffer);
    LogFlowFuncLeave();
}

/* PDMICARDREADERDOWN - interface */
static DECLCALLBACK(int) drvCardReaderIDown_Connect(PPDMICARDREADERDOWN pInterface,
                                                    void *pvUser, const char *pcszCardReaderName,
                                                    uint32_t dwSharedMode, uint32_t dwPrefferedProtocols)
{
    LogFlowFunc(("ENTER: pcszCardReaderName:%s, pvUser:%p, dwSharedMode:%RX32, dwPrefferedProtocols:%RX32\n", pcszCardReaderName, pvUser, dwSharedMode, dwPrefferedProtocols));
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);
    Assert(pvUser);
    int rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvCardReaderCmdQueueConnect, 5,
                              pThis, pcszCardReaderName, pvUser, dwSharedMode, dwPrefferedProtocols);
    AssertRC((rc));
    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    AssertRC((rc));
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvCardReaderIDown_Disconnect(PPDMICARDREADERDOWN pInterface, void *pvUser, uint32_t dwMode)
{
    LogFlowFunc(("ENTER: pvUser:%p, dwMode:%RX32\n", pvUser, dwMode));
    AssertPtrReturn(pInterface, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);
    int rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvCardReaderCmdQueueDisconnect, 3, pThis, pvUser, dwMode);
    AssertRC((rc));
    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    AssertRC((rc));
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvCardReaderIDown_EstablishContext(PPDMICARDREADERDOWN pInterface)
{
    LogFlowFuncLeave();
    AssertPtrReturn(pInterface, VERR_INVALID_PARAMETER);
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);
    int rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvCardReaderCmdEstablishContext, 1, pThis);
    AssertRC((rc));
    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    LogFlowFuncLeaveRC(rc);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvCardReaderIDown_ReleaseContext(PPDMICARDREADERDOWN pInterface, void *pvUser)
{
    LogFlowFunc(("ENTER: pvUser:%p\n", pvUser));
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);
    LONG cRc = SCardReleaseContext(pThis->CardReaderState.hSCardReaderCtx);
    Assert(cRc == SCARD_S_SUCCESS);
    LogFlowFunc(("LEAVE: VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvCardReaderIDown_Status(PPDMICARDREADERDOWN pInterface,
                                                                              void *pvUser,
                                                                              uint32_t cchReaderName,
                                                                              uint32_t cbArtLen)
{
    LogFlowFunc(("ENTER: pvUser:%p\n", pvUser));
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);
    Assert(pvUser);
    int rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                             (PFNRT)drvCardReaderCmdQueueStatus, 4, pThis, pvUser, cchReaderName, cbArtLen);
    AssertRC((rc));
    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    AssertRC((rc));
    LogFlowFunc(("LEAVE: %Rrc\n", rc));
    return rc;
}

static DECLCALLBACK(int) drvCardReaderIDown_StatusChange(PPDMICARDREADERDOWN pInterface,
                                                                                    void *pvUser,
                                                                                    uint32_t dwTimeout,
                                                                                    PDMICARDREADER_READERSTATE *pPdmReaderState,
                                                                                    uint32_t cReaderStats)
{
    LogFlowFunc(("ENTER: pvUser: %p, dwTimeout:%d\n", pvUser, dwTimeout));
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);

    int rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)drvCardReaderEvtQueueStatusChange, 5, pThis, pvUser, dwTimeout, pPdmReaderState, cReaderStats);
    AssertRC((rc));
    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvCardReaderIDown_BeginTransaction(PPDMICARDREADERDOWN pInterface, void *pvUser)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("pInterface:%p, pvUser:%p, dwDisposition:%RX32\n", pInterface, pvUser));
    AssertMsgFailed(("DebugME!!!"));
    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvCardReaderIDown_EndTransaction(PPDMICARDREADERDOWN pInterface,
                                                                                  void *pvUser,
                                                                                  uint32_t dwDisposition)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("pInterface:%p, pvUser:%p, dwDisposition:%RX32\n", pInterface, pvUser, dwDisposition));
    AssertMsgFailed(("DebugME!!!"));
    LogFlowFunc(("LEAVE:%Rrc\n", rc));
    return rc;
}


static DECLCALLBACK(int) drvCardReaderIDown_Transmit(PPDMICARDREADERDOWN pInterface,
                                                                            void *pvUser,
                                                                            const PDMICARDREADER_IO_REQUEST *pPdmIoSendRequest,
                                                                            const uint8_t *pu8SendBuffer,
                                                                            uint32_t cbSendBuffer,
                                                                            uint32_t cbRecvBuffer)
{
    int rc;
    LogFlowFunc(("pInterface:%p, pvUser:%p, pioSendRequest:%p, pu8SendBuffer:%p, cbSendBuffer:%d, cbRecvBuffer:%d\n",
                 pInterface, pvUser, pPdmIoSendRequest, pu8SendBuffer, cbSendBuffer, cbRecvBuffer));
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);
    /* !Someone need pioRecvPci */
    LPBYTE pu8SendBufferCopy = NULL;
    if (   pu8SendBuffer
        && cbSendBuffer)
    {
        pu8SendBufferCopy = (LPBYTE)RTMemDup(pu8SendBuffer, cbSendBuffer);
        AssertPtrReturn(pu8SendBufferCopy, VERR_NO_MEMORY);
    }
    LPSCARD_IO_REQUEST pioSendRequestCopy = NULL;

    pioSendRequestCopy = (LPSCARD_IO_REQUEST)RTMemAllocZ(sizeof(SCARD_IO_REQUEST) + pPdmIoSendRequest->cbPciLength - sizeof(PDMICARDREADER_IO_REQUEST));
    AssertPtrReturn(pioSendRequestCopy, VERR_NO_MEMORY);

    pioSendRequestCopy->dwProtocol = pPdmIoSendRequest->u32Protocol;
    pioSendRequestCopy->cbPciLength = pPdmIoSendRequest->cbPciLength - sizeof(PDMICARDREADER_IO_REQUEST) + sizeof(SCARD_IO_REQUEST);
    if (pPdmIoSendRequest->cbPciLength > sizeof(PDMICARDREADER_IO_REQUEST))
        memcpy(&pioSendRequestCopy[1], &pPdmIoSendRequest[1], pPdmIoSendRequest->cbPciLength - sizeof(PDMICARDREADER_IO_REQUEST));

    rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0,RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                          (PFNRT)drvCardReaderCmdTransmit, 6,
                          pThis, pvUser, pioSendRequestCopy, pu8SendBufferCopy, (DWORD)cbSendBuffer, (DWORD)cbRecvBuffer);
    AssertRC((rc));
    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvCardReaderIDown_GetAttr(PPDMICARDREADERDOWN pInterface, void *pvUser, uint32_t dwAttribId, uint32_t cbAttrib)
{
    int rc;
    LogFlowFunc(("ENTER: pInterface:%p, pvUser:%p, dwAttribId:%R[scardAttr], cbAttrib:%d\n",
                 pInterface, pvUser, dwAttribId, cbAttrib));
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);
    rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                          (PFNRT)drvCardReaderCmdGetAttr, 4, pThis, pvUser, dwAttribId, cbAttrib);
    AssertRC((rc));
    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvCardReaderIDown_SetAttr(PPDMICARDREADERDOWN pInterface, void *pvUser, uint32_t dwAttribId, const void *pvAttrib, uint32_t cbAttrib)
{
    int rc;
    LogFlowFunc(("ENTER: pInterface:%p, pvUser:%p, dwAttribId:%R[scardAttr], pvAttrib:%p, cbAttrib:%d\n",
                 pInterface, pvUser, dwAttribId, pvAttrib, cbAttrib));
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);
    void *pvAttribCopy = NULL;
    if (   pvAttrib
        && cbAttrib)
    {
        pvAttribCopy = RTMemDup(pvAttrib, cbAttrib);
        AssertPtrReturn(pvAttribCopy, VERR_NO_MEMORY);
    }
    rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                          (PFNRT)drvCardReaderCmdSetAttr, 5, pThis, pvUser, dwAttribId, pvAttribCopy, cbAttrib);
    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvCardReaderIDown_Control(PPDMICARDREADERDOWN pInterface,
                                                                           void *pvUser,
                                                                           uint32_t dwControlCode,
                                                                           const void *pcvInBuffer,
                                                                           uint32_t cbInBuffer,
                                                                           uint32_t cbOutBuffer)
{
    int rc;
    LogFlowFunc(("ENTER: pInterface:%p, pvUser:%p, dwControlCode:%RX32 pcvInBuffer:%p, cbInBuffer:%d, cbOutBuffer:%d\n",
                 pInterface, pvUser, dwControlCode, pcvInBuffer, cbInBuffer, cbOutBuffer));
    AssertPtrReturn(pInterface, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    PDRVCARDREADER  pThis = RT_FROM_MEMBER(pInterface, DRVCARDREADER, ICardReaderDown);

    LPVOID pvInBufferCopy = NULL;
    if (   pcvInBuffer
        && cbInBuffer)
    {
        pvInBufferCopy = RTMemDup(pcvInBuffer, cbInBuffer);
        AssertPtrReturn(pvInBufferCopy, VERR_NO_MEMORY);
    }
    rc = RTReqQueueCallEx(pThis->hReqQCardReaderCmd, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                          (PFNRT)drvCardReaderCmdControl, 6,
                          pThis, pvUser, dwControlCode, pvInBufferCopy, cbInBuffer, cbOutBuffer);

    rc = drvCardReaderThrCmdWakeup(pThis->pDrvIns, pThis->pThrCardReaderCmd);
    LogFlowFunc(("LEAVE: %Rrc\n", rc));
    return rc;
}

/* Cardreader driver thread routines */

DECLINLINE(int) drvThreadQueueProcessing(PPDMTHREAD pThread, RTREQQUEUE ReqQueue, RTSEMEVENT SemQueue)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("ENTER: pThread:%RTthrd, SemQueue:%RTsem\n", pThread, SemQueue));
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        /* do processing the queue */
        RTReqQueueProcess(ReqQueue, 0);
        rc = RTSemEventWait(SemQueue, RT_INDEFINITE_WAIT);
        AssertRC((rc));
    }
    LogFlowFunc(("LEAVE: %Rrc\n", rc));
    return rc;
}

DECLINLINE(int) drvThreadWakeupQueueProcessing(RTSEMEVENT SemQueue)
{
    LogFlowFunc(("ENTER: %RTsem\n", SemQueue));
    int rc = RTSemEventSignal(SemQueue);
    LogFlowFunc(("LEAVE: %Rrc\n", rc));
    return rc;
}

/**
 * This routine should poll the queue hReqQCardReaderCmd
 */
static DECLCALLBACK(int) drvCardReaderThrCmd(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    int rc = VINF_SUCCESS;
    PDRVCARDREADER pThis = PDMINS_2_DATA(pDrvIns, PDRVCARDREADER);
    LogFlowFunc(("ENTER: pDrvIns:%i\n", pDrvIns->iInstance));
    rc = drvThreadQueueProcessing(pThread, pThis->hReqQCardReaderCmd, pThis->EvtCardReaderCmd);
    LogFlowFunc(("LEAVE: %Rrc\n", rc));
    return rc;
}

static DECLCALLBACK(int) drvCardReaderThrCmdWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVCARDREADER pThis = PDMINS_2_DATA(pDrvIns, PDRVCARDREADER);
    LogFlowFunc(("ENTER: pDrvIns:%i\n", pDrvIns->iInstance));
    int rc = drvThreadWakeupQueueProcessing(pThis->EvtCardReaderCmd);
    AssertRC(rc);
    LogFlowFunc(("LEAVE: VINF_SUCCESS\n"));
    return VINF_SUCCESS;

}

#if 0
/**
 * This routine should poll the queue hReqQCardReaderCmd
 */
static DECLCALLBACK(int) drvCardReaderThrEvt(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    int rc = VINF_SUCCESS;
    PDRVCARDREADER pThis = PDMINS_2_DATA(pDrvIns, PDRVCARDREADER);
    LogFlowFunc(("ENTER: pDrvIns:%i\n", pDrvIns->iInstance));
    rc = drvThreadQueueProcessing(pThread, pThis->hReqQCardReaderCmd, pThis->EvtCardReaderEvt);
    LogFlowFunc(("LEAVE: %Rrc\n", rc));
    return rc;
}

static DECLCALLBACK(int) drvCardReaderThrEvtWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVCARDREADER pThis = PDMINS_2_DATA(pDrvIns, PDRVCARDREADER);
    LogFlowFunc(("ENTER: pDrvIns:%i\n", pDrvIns->iInstance));
    int rc = drvThreadWakeupQueueProcessing(pThis->EvtCardReaderEvt);
    AssertRC(rc);
    LogFlowFunc(("LEAVE: VINF_SUCCESS\n"));
    return VINF_SUCCESS;

}
#endif

/**
 * @interface_method_impl{PDMDRVREG,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvCardReaderIBase_pfnQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    LogFlowFunc(("ENTER: pInterface:%p, pszIID:%RTuuid\n", pInterface, pszIID));
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVCARDREADER pThis = PDMINS_2_DATA(pDrvIns, PDRVCARDREADER);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMICARDREADERDOWN, &pThis->ICardReaderDown);
    return NULL;
}

/**
 * @interface_method_impl{PDMDRVREG,pfnDestruct}
 */
static DECLCALLBACK(void) drvCardReaderDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    LogFlowFunc(("ENTER: pDrvIns:%i\n", pDrvIns->iInstance));
    PDRVCARDREADER pThis = PDMINS_2_DATA(pDrvIns, PDRVCARDREADER);

    /* Command thread synchronization primitives distruction */
    int rc;
    rc = RTSemEventDestroy(pThis->EvtCardReaderCmd);    AssertRC(rc);
    rc = RTReqQueueDestroy(pThis->hReqQCardReaderCmd);  AssertRC(rc);

    /* Event thread synchronization primitives distruction */
#if 0
    rc = RTSemEventDestroy(pThis->EvtCardReaderEvt);    AssertRC(rc);
    rc = RTReqQueueDestroy(pThis->hReqQCardReaderEvt);  AssertRC(rc);
#endif
    LogFlowFuncLeave();
}

/**
 * @interface_method_impl{PDMDRVREG,pfnConstruct}
 */
static DECLCALLBACK(int) drvCardReaderConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    LogFlowFunc(("ENTER: pDrvIns:%p, pCfg:%p, fFlags:%RX32\n", pDrvIns, pCfg, fFlags));
    PDRVCARDREADER pThis = PDMINS_2_DATA(pDrvIns, PDRVCARDREADER);

    pThis->pDrvIns = pDrvIns;
    pThis->EvtCardReaderCmd = NIL_RTSEMEVENT;
    pThis->hReqQCardReaderCmd = NIL_RTREQQUEUE;
#if 0
    pThis->EvtCardReaderEvt = NIL_RTSEMEVENT;
    pThis->hReqQCardReaderEvt = NIL_RTREQQUEUE;
#endif

    pDrvIns->IBase.pfnQueryInterface = drvCardReaderIBase_pfnQueryInterface;

    pThis->ICardReaderDown.pfnEstablishContext  = drvCardReaderIDown_EstablishContext;
    pThis->ICardReaderDown.pfnStatus            = drvCardReaderIDown_Status;
    pThis->ICardReaderDown.pfnConnect           = drvCardReaderIDown_Connect;
    pThis->ICardReaderDown.pfnDisconnect        = drvCardReaderIDown_Disconnect;
    pThis->ICardReaderDown.pfnReleaseContext    = drvCardReaderIDown_ReleaseContext;
    pThis->ICardReaderDown.pfnGetStatusChange   = drvCardReaderIDown_StatusChange;
    pThis->ICardReaderDown.pfnBeginTransaction  = drvCardReaderIDown_BeginTransaction;
    pThis->ICardReaderDown.pfnEndTransaction    = drvCardReaderIDown_EndTransaction;
    pThis->ICardReaderDown.pfnTransmit          = drvCardReaderIDown_Transmit;
    pThis->ICardReaderDown.pfnGetAttr           = drvCardReaderIDown_GetAttr;
    pThis->ICardReaderDown.pfnSetAttr           = drvCardReaderIDown_SetAttr;
    pThis->ICardReaderDown.pfnControl           = drvCardReaderIDown_Control;

    pThis->pICardReaderUp = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMICARDREADERUP);

    /* Command Thread Synchronization primitives */
    int rc;
    rc = RTSemEventCreate(&pThis->EvtCardReaderCmd);    AssertRCReturn(rc, rc);
    rc = RTReqQueueCreate(&pThis->hReqQCardReaderCmd);  AssertLogRelRCReturn(rc, rc);
#if 0
    /* Event Thread Synchronization primitives */
    rc = RTSemEventCreate(&pThis->EvtCardReaderEvt);    AssertRCReturn(rc, rc);
    rc = RTReqQueueCreate(&pThis->hReqQCardReaderEvt);  AssertLogRelRCReturn(rc, rc);
#endif

#if 1
    /* from CFGM ??? */
    pThis->CardReaderState.pszCardReaderName  = RTStrDup("SCM SCR 335 [CCID Interface] (21120844306107) 00 00");
#endif

    rc = PDMDrvHlpThreadCreate(pDrvIns,
                               &pThis->pThrCardReaderCmd,
                               pThis,
                               drvCardReaderThrCmd /* worker routine */,
                               drvCardReaderThrCmdWakeup /* wakeup routine */,
                               128 * _1K, RTTHREADTYPE_IO, "CARDCMD");

#if 0
    rc = PDMDrvHlpThreadCreate(pDrvIns,
                               &pThis->pThrCardReaderCmd,
                               pThis,
                               drvCardReaderThrEvt /* worker routine */,
                               drvCardReaderThrEvtWakeup /* wakeup routine */,
                               128 * _1K, RTTHREADTYPE_IO, "CARDCMD");

#endif
    LogFlowFunc(("LEAVE: %Rrc\n", rc));
    return rc;
}

const PDMDRVREG g_DrvCardReaderReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName[32] */
    "DrvDirectCardReader",
    /* szRCMod[32] */
    "",
    /* szR0Mod[32] */
    "",
    /* pszDescription */
    "Test Driver fo Card Reader Device",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass */
    PDM_DRVREG_CLASS_USB,
    /* cMaxInstances */
    16,
    /* cbInstance */
    sizeof(DRVCARDREADER),
    /* pfnConstruct */
    drvCardReaderConstruct,
    /* pfnDestruct */
    drvCardReaderDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DRVREG_VERSION
};
