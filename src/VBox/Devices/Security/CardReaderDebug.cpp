/* $Id: CardReaderDebug.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * UsbCardReader - Usb Card Reader implementation, Debug Logging Formaters.
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
#include <VBox/vmm/pdmusb.h>

#include "UsbCardReaderDefs.h"
#include "vusbcardreader.h"

#include <VBox/vmm/pdmcardreaderinfs.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define VBOX_CARDREADER_RC      0xdead0000
#define VBOX_CARDREADER_ATTR    0xdead0001
#define VBOX_CARDREADER_STS     0xdead0010
#define VBOX_CARDREADER_STATE   0xdead0011


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct
{
    int32_t     i32Code;
    const char  *pszCode;
} CARDREADERSCARDRC2STR, *PCARDREADERSCARDRC2STR;

typedef const PCARDREADERSCARDRC2STR PCCARDREADERSCARDRC2STR;

typedef struct
{
    uint8_t     u8Code;
    const char  *pszCode;
} MSGTYPE2STR;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#define CODE2STR_HELPER(name) {(name), #name}

static MSGTYPE2STR g_CardReaderMsgType2Str[] =
{
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ICCPOWERON),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ICCPOWEROFF),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_GETSLOTSTATUS),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_XFRBLOCK),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_GETPARAMETERS),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_RESETPARAMETERS),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_SETPARAMETERS),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ESCAPE),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ICCLOCK),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_T0APDU),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_SECURE),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_MECHANICAL),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ABORT),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_SETDATARATEANDCLOCKFREQUENCY),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_DATABLOCK),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_SLOTSTATUS),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_PARAMETERS),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_ESCAPE),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_DATARATEANDCLOCKFREQUECY),
    CODE2STR_HELPER(VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_NOTIFY_SLOT_CHANGES)
};


static CARDREADERSCARDRC2STR g_CardReaderSCardRc2Str[] =
{
    CODE2STR_HELPER(VBOX_SCARD_S_SUCCESS),
    CODE2STR_HELPER(VBOX_SCARD_F_INTERNAL_ERROR),
    CODE2STR_HELPER(VBOX_SCARD_E_CANCELLED),
    CODE2STR_HELPER(VBOX_SCARD_E_INVALID_HANDLE),
    CODE2STR_HELPER(VBOX_SCARD_E_INVALID_PARAMETER),
    CODE2STR_HELPER(VBOX_SCARD_E_INVALID_TARGET),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_MEMORY),
    CODE2STR_HELPER(VBOX_SCARD_F_WAITED_TOO_LONG),
    CODE2STR_HELPER(VBOX_SCARD_E_INSUFFICIENT_BUFFER),
    CODE2STR_HELPER(VBOX_SCARD_E_UNKNOWN_READER),
    CODE2STR_HELPER(VBOX_SCARD_E_TIMEOUT),
    CODE2STR_HELPER(VBOX_SCARD_E_SHARING_VIOLATION),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_SMARTCARD),
    CODE2STR_HELPER(VBOX_SCARD_E_UNKNOWN_CARD),
    CODE2STR_HELPER(VBOX_SCARD_E_CANT_DISPOSE),
    CODE2STR_HELPER(VBOX_SCARD_E_PROTO_MISMATCH),
    CODE2STR_HELPER(VBOX_SCARD_E_NOT_READY),
    CODE2STR_HELPER(VBOX_SCARD_E_INVALID_VALUE),
    CODE2STR_HELPER(VBOX_SCARD_E_SYSTEM_CANCELLED),
    CODE2STR_HELPER(VBOX_SCARD_F_COMM_ERROR),
    CODE2STR_HELPER(VBOX_SCARD_F_UNKNOWN_ERROR),
    CODE2STR_HELPER(VBOX_SCARD_E_INVALID_ATR),
    CODE2STR_HELPER(VBOX_SCARD_E_NOT_TRANSACTED),
    CODE2STR_HELPER(VBOX_SCARD_E_READER_UNAVAILABLE),
    CODE2STR_HELPER(VBOX_SCARD_P_SHUTDOWN),
    CODE2STR_HELPER(VBOX_SCARD_E_PCI_TOO_SMALL),
    CODE2STR_HELPER(VBOX_SCARD_E_READER_UNSUPPORTED),
    CODE2STR_HELPER(VBOX_SCARD_E_DUPLICATE_READER),
    CODE2STR_HELPER(VBOX_SCARD_E_CARD_UNSUPPORTED),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_SERVICE),
    CODE2STR_HELPER(VBOX_SCARD_E_SERVICE_STOPPED),
    CODE2STR_HELPER(VBOX_SCARD_E_UNEXPECTED),
    CODE2STR_HELPER(VBOX_SCARD_E_ICC_INSTALLATION),
    CODE2STR_HELPER(VBOX_SCARD_E_ICC_CREATEORDER),
    CODE2STR_HELPER(VBOX_SCARD_E_UNSUPPORTED_FEATURE),
    CODE2STR_HELPER(VBOX_SCARD_E_DIR_NOT_FOUND),
    CODE2STR_HELPER(VBOX_SCARD_E_FILE_NOT_FOUND),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_DIR),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_FILE),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_ACCESS),
    CODE2STR_HELPER(VBOX_SCARD_E_WRITE_TOO_MANY),
    CODE2STR_HELPER(VBOX_SCARD_E_BAD_SEEK),
    CODE2STR_HELPER(VBOX_SCARD_E_INVALID_CHV),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_SUCH_CERTIFICATE),
    CODE2STR_HELPER(VBOX_SCARD_E_CERTIFICATE_UNAVAILABLE),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_READERS_AVAILABLE),
    CODE2STR_HELPER(VBOX_SCARD_E_COMM_DATA_LOST),
    CODE2STR_HELPER(VBOX_SCARD_E_NO_KEY_CONTAINER),
    CODE2STR_HELPER(VBOX_SCARD_E_SERVER_TOO_BUSY),
    CODE2STR_HELPER(VBOX_SCARD_W_UNSUPPORTED_CARD),
    CODE2STR_HELPER(VBOX_SCARD_W_UNRESPONSIVE_CARD),
    CODE2STR_HELPER(VBOX_SCARD_W_UNPOWERED_CARD),
    CODE2STR_HELPER(VBOX_SCARD_W_RESET_CARD),
    CODE2STR_HELPER(VBOX_SCARD_W_REMOVED_CARD),
    CODE2STR_HELPER(VBOX_SCARD_W_SECURITY_VIOLATION),
    CODE2STR_HELPER(VBOX_SCARD_W_WRONG_CHV),
    CODE2STR_HELPER(VBOX_SCARD_W_CHV_BLOCKED),
    CODE2STR_HELPER(VBOX_SCARD_W_EOF),
    CODE2STR_HELPER(VBOX_SCARD_W_CANCELLED_BY_USER),
    CODE2STR_HELPER(VBOX_SCARD_W_CARD_NOT_AUTHENTICATED)
#if 0
    CODE2STR_HELPER(VBOX_SCARD_W_CACHE_ITEM_NOT_FOUND),
    CODE2STR_HELPER(VBOX_SCARD_W_CACHE_ITEM_STALE),
    CODE2STR_HELPER(VBOX_SCARD_W_CACHE_ITEM_TOO_BIG)
#endif
};

static CARDREADERSCARDRC2STR g_CardReaderSCardSts2Str[] =
{
    CODE2STR_HELPER(VBOX_SCARD_UNKNOWN),
    CODE2STR_HELPER(VBOX_SCARD_ABSENT),
    CODE2STR_HELPER(VBOX_SCARD_PRESENT),
    CODE2STR_HELPER(VBOX_SCARD_SWALLOWED),
    CODE2STR_HELPER(VBOX_SCARD_POWERED),
    CODE2STR_HELPER(VBOX_SCARD_NEGOTIABLE),
    CODE2STR_HELPER(VBOX_SCARD_SPECIFICMODE)
};

static CARDREADERSCARDRC2STR g_CardReaderSCardState2Str[] =
{
    CODE2STR_HELPER(VBOX_SCARD_STATE_UNAWARE),
    CODE2STR_HELPER(VBOX_SCARD_STATE_IGNORE),
    CODE2STR_HELPER(VBOX_SCARD_STATE_CHANGED),
    CODE2STR_HELPER(VBOX_SCARD_STATE_UNKNOWN),
    CODE2STR_HELPER(VBOX_SCARD_STATE_UNAVAILABLE),
    CODE2STR_HELPER(VBOX_SCARD_STATE_EMPTY),
    CODE2STR_HELPER(VBOX_SCARD_STATE_PRESENT),
    CODE2STR_HELPER(VBOX_SCARD_STATE_ATRMATCH),
    CODE2STR_HELPER(VBOX_SCARD_STATE_EXCLUSIVE),
    CODE2STR_HELPER(VBOX_SCARD_STATE_INUSE),
    CODE2STR_HELPER(VBOX_SCARD_STATE_MUTE),
    CODE2STR_HELPER(VBOX_SCARD_STATE_UNPOWERED)

};

static CARDREADERSCARDRC2STR g_CardReaderSCardAttr2Str[] =
{
    CODE2STR_HELPER(VBOX_SCARD_ATTR_ATR_STRING),
#if 0 /** @todo later is needed. */
    CODE2STR_HELPER(SCARD_ATTR_VENDOR_NAME),
    CODE2STR_HELPER(SCARD_ATTR_VENDOR_IFD_TYPE),
    CODE2STR_HELPER(SCARD_ATTR_VENDOR_IFD_VERSION),
    CODE2STR_HELPER(SCARD_ATTR_VENDOR_IFD_SERIAL_NO),
    CODE2STR_HELPER(SCARD_ATTR_CHANNEL_ID),
#ifdef SCARD_ATTR_ASYNC_PROTOCOL_TYPES
    CODE2STR_HELPER(SCARD_ATTR_ASYNC_PROTOCOL_TYPES),
#endif
    CODE2STR_HELPER(SCARD_ATTR_DEFAULT_CLK),
    CODE2STR_HELPER(SCARD_ATTR_MAX_CLK),
    CODE2STR_HELPER(SCARD_ATTR_DEFAULT_DATA_RATE),
    CODE2STR_HELPER(SCARD_ATTR_MAX_DATA_RATE),
    CODE2STR_HELPER(SCARD_ATTR_MAX_IFSD),
#ifdef SCARD_ATTR_SYNC_PROTOCOL_TYPES
    CODE2STR_HELPER(SCARD_ATTR_SYNC_PROTOCOL_TYPES),
#endif
    CODE2STR_HELPER(SCARD_ATTR_POWER_MGMT_SUPPORT),
    CODE2STR_HELPER(SCARD_ATTR_USER_TO_CARD_AUTH_DEVICE),
    CODE2STR_HELPER(SCARD_ATTR_USER_AUTH_INPUT_DEVICE),
    CODE2STR_HELPER(SCARD_ATTR_CHARACTERISTICS),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_PROTOCOL_TYPE),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_CLK),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_F),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_D),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_N),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_W),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_IFSC),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_IFSD),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_BWT),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_CWT),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_EBC_ENCODING),
    CODE2STR_HELPER(SCARD_ATTR_EXTENDED_BWT),
    CODE2STR_HELPER(SCARD_ATTR_ICC_PRESENCE),
    CODE2STR_HELPER(SCARD_ATTR_ICC_INTERFACE_STATUS),
    CODE2STR_HELPER(SCARD_ATTR_CURRENT_IO_STATE),
    CODE2STR_HELPER(SCARD_ATTR_ICC_TYPE_PER_ATR),
    CODE2STR_HELPER(SCARD_ATTR_ESC_RESET),
    CODE2STR_HELPER(SCARD_ATTR_ESC_CANCEL),
    CODE2STR_HELPER(SCARD_ATTR_ESC_AUTHREQUEST),
    CODE2STR_HELPER(SCARD_ATTR_MAXINPUT),
    CODE2STR_HELPER(SCARD_ATTR_DEVICE_UNIT),
    CODE2STR_HELPER(SCARD_ATTR_DEVICE_IN_USE),
    CODE2STR_HELPER(SCARD_ATTR_DEVICE_FRIENDLY_NAME_A),
    CODE2STR_HELPER(SCARD_ATTR_DEVICE_SYSTEM_NAME_A),
    CODE2STR_HELPER(SCARD_ATTR_DEVICE_FRIENDLY_NAME_W),
    CODE2STR_HELPER(SCARD_ATTR_DEVICE_SYSTEM_NAME_W),
    CODE2STR_HELPER(SCARD_ATTR_SUPRESS_T1_IFS_REQUEST)
#if 0 /* UNICODE defined ??? */
    CODE2STR_HELPER(SCARD_ATTR_DEVICE_FRIENDLY_NAME),
    CODE2STR_HELPER(CARD_ATTR_DEVICE_SYSTEM_NAME),
#endif
#endif
};

#undef CODE2STR_HELPER
static DECLCALLBACK(size_t) usbCardReaderFormatMsg(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                   const char *pszType, void const *pvValue,
                                                   int cchWidth, int cchPrecision, unsigned fFlags,
                                                   void *pvUser)
{
    RT_NOREF4(cchWidth, cchPrecision, fFlags, pvUser);
    AssertReturn(strcmp(pszType, "cardreadermsg") == 0, 0);
    PVUSBCARDREADERMSGBULKHEADERCORE pMsg = (PVUSBCARDREADERMSGBULKHEADERCORE)pvValue;
    if (!pMsg)
        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "NULL_MSG");
    for (unsigned idxType = 0; idxType < RT_ELEMENTS(g_CardReaderMsgType2Str); ++idxType)
    {
        if (g_CardReaderMsgType2Str[idxType].u8Code == pMsg->Core.bMessageType)
        {
            if (pMsg->Core.bMessageType == VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_NOTIFY_SLOT_CHANGES)
                return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%p %s", pMsg, g_CardReaderMsgType2Str[idxType].pszCode);
            return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%p %s: dwLength:%d, bSlot:%RX8, bSeq:%RX8",
                               pMsg, g_CardReaderMsgType2Str[idxType].pszCode, pMsg->dwLength,
                               pMsg->bSlot, pMsg->bSeq);
        }
    }
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%p INVALID_MSG(%RX8)", pMsg, pMsg->Core.bMessageType);
}

static DECLCALLBACK(size_t) usbCardReaderFormatMsgReq(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                      const char *pszType, void const *pvValue,
                                                      int cchWidth, int cchPrecision, unsigned fFlags,
                                                      void *pvUser)
{
    RT_NOREF4(cchWidth, cchPrecision, fFlags, pvUser);
    AssertReturn(strcmp(pszType, "cardreadermsgreq") == 0, 0);
    PVUSBCARDREADERMSGRESPONSECORE pMsg = (PVUSBCARDREADERMSGRESPONSECORE)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "[%R[cardreadermsg]]", pMsg);
}

static DECLCALLBACK(size_t) usbCardReaderFormatMsgResp(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                       const char *pszType, void const *pvValue,
                                                       int cchWidth, int cchPrecision, unsigned fFlags,
                                                       void *pvUser)
{
    RT_NOREF4(cchWidth, cchPrecision, fFlags, pvUser);
    AssertReturn(strcmp(pszType, "cardreadermsgresp") == 0, 0);
    PVUSBCARDREADERMSGRESPONSECORE pMsg = (PVUSBCARDREADERMSGRESPONSECORE)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "[%R[cardreadermsg] bStatus:%RX8, bError:%RX8]", pMsg,
                                                        pMsg ? pMsg->bStatus : 0,
                                                        pMsg ? pMsg->bError : 0);
}

static DECLCALLBACK(size_t) usbCardReaderFormatMsgScardGeneric(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                               const char *pszType, void const *pvValue,
                                                               int cchWidth, int cchPrecision, unsigned fFlags,
                                                               void *pvUser)
{
    RT_NOREF3(cchWidth, cchPrecision, fFlags);
    uint32_t u32Hint = (uint32_t)(uintptr_t)pvUser;
    PCARDREADERSCARDRC2STR pCardreaderConst2StrTbl = NULL;
    size_t       cCardreaderConst2StrTbl = 0;
    const char   *pcszDefaultMsg;
    /* check that we haven't received any garbage */
    Assert (   ((u32Hint & 0xffff0000) == 0xdead0000)
            && ((u32Hint & 0xf0) == 0));
    switch (u32Hint)
    {
        case VBOX_CARDREADER_RC:
            AssertReturn(strcmp(pszType, "scardRc") == 0, 0);
            pCardreaderConst2StrTbl = g_CardReaderSCardRc2Str;
            cCardreaderConst2StrTbl  = RT_ELEMENTS(g_CardReaderSCardRc2Str);
            pcszDefaultMsg = "INVALIDE_SCARD_RC_CODE(0x%RX32)";
        break;
        case VBOX_CARDREADER_ATTR:
            AssertReturn(strcmp(pszType, "scardAttr") == 0, 0);
            pCardreaderConst2StrTbl = g_CardReaderSCardAttr2Str;
            cCardreaderConst2StrTbl  = RT_ELEMENTS(g_CardReaderSCardAttr2Str);
            pcszDefaultMsg = "INVALIDE_SCARD_ATTR_CODE(0x%RX32)";
        break;
        default:
            AssertMsgReturn(0, ("Unsupported tag %RX32", u32Hint), 0);
    }
    const int32_t scardConst = (const int32_t)(uintptr_t)pvValue;
    unsigned int idxCode = 0;

    Assert((pCardreaderConst2StrTbl));

    for (idxCode = 0; idxCode < cCardreaderConst2StrTbl; ++idxCode)
    {
        if (pCardreaderConst2StrTbl[idxCode].i32Code == scardConst)
            return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%s", pCardreaderConst2StrTbl[idxCode].pszCode);
    }
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, pcszDefaultMsg, scardConst);
}

static DECLCALLBACK(size_t) usbCardReaderFormatMsgScardGenericOr(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                                 const char *pszType, void const *pvValue,
                                                                 int cchWidth, int cchPrecision, unsigned fFlags,
                                                                 void *pvUser)
{
    RT_NOREF3(cchWidth, cchPrecision, fFlags);
    uint32_t u32Hint = (uint32_t)(uintptr_t)pvUser;
    PCARDREADERSCARDRC2STR pCardreaderConst2StrTbl = NULL;
    size_t       cCardreaderConst2StrTbl = 0;
    const char   *pcszDefaultMsg;
    /* check that we haven't received any garbage */
    Assert (   ((u32Hint & 0xffff0000) == 0xdead0000)
            && ((u32Hint & 0xf0) == 0x10));
    switch (u32Hint)
    {
        case VBOX_CARDREADER_STS:
            AssertReturn(strcmp(pszType, "scardSts") == 0, 0);
            pCardreaderConst2StrTbl = g_CardReaderSCardSts2Str;
            cCardreaderConst2StrTbl  = RT_ELEMENTS(g_CardReaderSCardSts2Str);
            pcszDefaultMsg = "INVALIDE_SCARD_STS_CODE(0x%RX32)";
        break;
        case VBOX_CARDREADER_STATE:
            AssertReturn(strcmp(pszType, "scardState") == 0, 0);
            pCardreaderConst2StrTbl = g_CardReaderSCardState2Str;
            cCardreaderConst2StrTbl  = RT_ELEMENTS(g_CardReaderSCardState2Str);
            pcszDefaultMsg = "INVALIDE_SCARD_STATE_CODE(0x%RX32)";
        break;
        default:
            AssertMsgReturn(0, ("Unsupported tag %RX32", u32Hint), 0);
    }
    const int32_t scardConst = (const int32_t)(uintptr_t)pvValue;
    unsigned int idxCode = 0;
    bool fFirst = true;
    size_t cbPrintf = 0;
    for (idxCode = 0; idxCode < cCardreaderConst2StrTbl; ++idxCode)
    {
        if (pCardreaderConst2StrTbl[idxCode].i32Code & scardConst)
        {
            cbPrintf += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "%s%s",
                                    fFirst ? "" : "|",
                                    pCardreaderConst2StrTbl[idxCode].pszCode);
            fFirst = false;
        }
    }
    if (fFirst)
        return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, pcszDefaultMsg, scardConst);
    else
        return cbPrintf;
}

static DECLCALLBACK(size_t) usbCardReaderFormatMsgScardIoReq(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                             const char *pszType, void const *pvValue,
                                                             int cchWidth, int cchPrecision, unsigned fFlags,
                                                             void *pvUser)
{
    RT_NOREF4(cchWidth, cchPrecision, fFlags, pvUser);
    size_t cbPrintf = 0;
    AssertReturn(strcmp(pszType, "scardioreq") == 0, 0);
    PDMICARDREADER_IO_REQUEST *pScardIoReq = (PDMICARDREADER_IO_REQUEST *)pvValue;
    if (pScardIoReq)
    {
        if (RT_UNLIKELY(pScardIoReq->cbPciLength > sizeof(PDMICARDREADER_IO_REQUEST)))
            cbPrintf = RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,"PDMICARDREADER_IO_REQUEST[u32Protocol:%d, cbPciLength:%d, %.*Rhxs]",
                                pScardIoReq->u32Protocol,
                                pScardIoReq->cbPciLength,
                                &pScardIoReq[1], pScardIoReq->cbPciLength - sizeof(PDMICARDREADER_IO_REQUEST) + 1);
        else
            cbPrintf = RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,"PDMICARDREADER_IO_REQUEST[u32Protocol:%d, cbPciLength:%d]",
                                pScardIoReq->u32Protocol,
                                pScardIoReq->cbPciLength);
    }
    else
        cbPrintf = RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,"PDMICARDREADER_IO_REQUEST[NULL]");
    return cbPrintf;
}


void usbCardReaderInitLogging(void)
{
    static int g_fCardReaderLogging;
    int rc = VINF_SUCCESS;
    if (!g_fCardReaderLogging)
    {
        rc = RTStrFormatTypeRegister("cardreadermsg", usbCardReaderFormatMsg, NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("cardreadermsgreq", usbCardReaderFormatMsgReq, NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("cardreadermsgresp", usbCardReaderFormatMsgResp, NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("scardRc", usbCardReaderFormatMsgScardGeneric, (void*)(uintptr_t)VBOX_CARDREADER_RC);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("scardSts", usbCardReaderFormatMsgScardGenericOr, (void*)(uintptr_t)VBOX_CARDREADER_STS);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("scardState", usbCardReaderFormatMsgScardGenericOr, (void*)(uintptr_t)VBOX_CARDREADER_STATE);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("scardAttr", usbCardReaderFormatMsgScardGeneric, (void*)(uintptr_t)VBOX_CARDREADER_ATTR);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("scardioreq", usbCardReaderFormatMsgScardIoReq, NULL);
        AssertRC(rc);
        g_fCardReaderLogging = true;
    }
}

