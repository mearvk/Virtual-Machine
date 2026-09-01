/* $Id: vusbcardreader.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * vusbcardreader.h - defines usb related types.
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

#ifndef VBOX_INCLUDED_SRC_Security_vusbcardreader_h
#define VBOX_INCLUDED_SRC_Security_vusbcardreader_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Class specific requests */
#define VUSB_CARDREADER_REQ_ABORT                  1
#define VUSB_CARDREADER_REQ_GET_DATA_RATES         2
#define VUSB_CARDREADER_REQ_GET_CLOCK_FREQUENCIES  3

#pragma pack(1)
/* The bulk message header. 10 bytes. */
typedef struct VUSBCARDREADERBULKHDR
{
    /* VUSBCARDREADER_MSG_TYPE_* */
    uint8_t     bMessageType;
    /* Message specific data length  */
    uint32_t    dwLength;
    /* Identifies slot number for this command */
    uint8_t     bSlot;
    /* Sequence number for this command */
    uint8_t     bSeq;
    /* 3 message specific bytes. */
    union
    {
       /* Out. */
       struct
       {
           union
           {
               struct
               {
                   uint8_t bPowerSelect;
                   uint8_t abRFU[2];
               } IccPowerOn;
               struct
               {
                   uint8_t abRFU[3];
               } IccPowerOff;
               struct
               {
                   uint8_t abRFU[3];
               } GetSlotStatus;
               struct
               {
                   uint8_t bBWI;
                   uint16_t wLevelParameter;
               } XfrBlock;
               struct
               {
                   uint8_t abRFU[3];
               } GetParameters;
               struct
               {
                   uint8_t abRFU[3];
               } ResetParameters;
               struct
               {
                   uint8_t bProtocolNum;
                   uint8_t abRFU[2];
               } SetParameters;
               struct
               {
                   uint8_t abRFU[3];
               } Escape;
               struct
               {
                   uint8_t bClockCommand;
                   uint8_t abRFU[2];
               } IccClock;
               struct
               {
                   uint8_t bmChanges;
                   uint8_t bClassGetResponse;
                   uint8_t bClassEnvelope;
               } T0APDU;
               struct
               {
                   uint8_t bBWI;
                   uint16_t wLevelParameter;
               } Secure;
               struct
               {
                   uint8_t bFunction;
                   uint8_t abRFU[2];
               } Mechanical;
               struct
               {
                   uint8_t abRFU[3];
               } Abort;
               struct
               {
                   uint8_t abRFU[3];
               } SetDataRateAndClockFrequency;
           } u;
       } PC_to_RDR;

       /* In. */
       struct
       {
           uint8_t bSlotStatus;
           uint8_t bSlotError;
           union
           {
               struct
               {
                   uint8_t bChainParameter;
               } DataBlock;
               struct
               {
                   uint8_t bClockStatus;
               } SlotStatus;
               struct
               {
                   uint8_t bProtocolNum;
               } Parameters;
               struct
               {
                   uint8_t bRFU;
               } Escape;
               struct
               {
                   uint8_t bRFU;
               } DataRateAndClockFrequency;
               uint8_t u8Raw;
           } u;
       } RDR_to_PC;

       /* Always 3 bytes. */
       uint8_t au8Raw[3];
    } u;
} VUSBCARDREADERBULKHDR;
AssertCompileSize(VUSBCARDREADERBULKHDR, 10);

typedef struct VUSBCARDREADERPARMST0
{
    uint8_t bmFindexDindex;
    uint8_t bmTCCKST0;
    uint8_t bGuardTimeT0;
    uint8_t bWaitingIntegerT0;
    uint8_t bClockStop;
} VUSBCARDREADERPARMST0;
AssertCompileSize(VUSBCARDREADERPARMST0, 5);

typedef struct VUSBCARDREADERPARMST1
{
    uint8_t bmFindexDindex;
    uint8_t bmTCCKST1;
    uint8_t bGuardTimeT1;
    uint8_t bmWaitingIntegersT1;
    uint8_t bClockStop;
    uint8_t bIFSC;
    uint8_t bNadValue;
} VUSBCARDREADERPARMST1;
AssertCompileSize(VUSBCARDREADERPARMST1, 7);


typedef struct VUSBCARDREADERMSGHEADERCORE
{
    /* Type of the message see VUSBCARDREADER_MSG_TYPE_* for details */
    uint8_t     bMessageType;
} VUSBCARDREADERMSGHEADERCORE, *PVUSBCARDREADERMSGHEADERCORE;
typedef struct VUSBCARDREADERMSGBULKHEADERCORE
{
    VUSBCARDREADERMSGHEADERCORE Core;
    /* Message specific data length  */
    uint32_t    dwLength;
    /* Identifies slot number for this command */
    uint8_t     bSlot;
    /* Sequence number for this command */
    uint8_t     bSeq;
} VUSBCARDREADERMSGBULKHEADERCORE, *PVUSBCARDREADERMSGBULKHEADERCORE;

typedef const PVUSBCARDREADERMSGBULKHEADERCORE PCVUSBCARDREADERMSGBULKHEADERCORE;

/* Requests messages */
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ICCPOWERON                    0x62
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ICCPOWEROFF                   0x63
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_GETSLOTSTATUS                 0x65
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_XFRBLOCK                      0x6f
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_GETPARAMETERS                 0x6c
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_RESETPARAMETERS               0x6d
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_SETPARAMETERS                 0x61
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ESCAPE                        0x6b
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ICCLOCK                       0x6e
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_T0APDU                        0x6a
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_SECURE                        0x69
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_MECHANICAL                    0x71
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_ABORT                         0x72
#define VUSBCARDREADER_MSG_TYPE_PC_TO_RDR_SETDATARATEANDCLOCKFREQUENCY  0x73

/* Response messages */
#define VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_DATABLOCK                     0x80
#define VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_SLOTSTATUS                    0x81
#define VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_PARAMETERS                    0x82
#define VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_ESCAPE                        0x83
#define VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_DATARATEANDCLOCKFREQUECY      0x84

/* Interrupt endpoint messages. */
#define VUSBCARDREADER_MSG_TYPE_RDR_TO_PC_NOTIFY_SLOT_CHANGES           0x50


/* REQUESTS */

typedef struct VUSBCARDREADERMSGICCPOWERON
{
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    uint8_t                     bPowerSelect;
    /* Reserved for future use */
    uint8_t                     abRFU[2];
} VUSBCARDREADERMSGICCPOWERON;
AssertCompileSize(VUSBCARDREADERMSGICCPOWERON, 10);
typedef VUSBCARDREADERMSGICCPOWERON *PVUSBCARDREADERMSGICCPOWERON;

typedef struct VUSBCARDREADERMSGICCPOWEROFF
{
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    /* Reserved for future use */
    uint8_t                     abRFU[3];
} VUSBCARDREADERMSGICCPOWEOFF;
AssertCompileSize(VUSBCARDREADERMSGICCPOWEOFF, 10);


typedef VUSBCARDREADERMSGICCPOWEOFF VUSBCARDREADERMSGGETSLOTSTATUS;
AssertCompileSize(VUSBCARDREADERMSGGETSLOTSTATUS, 10);

/**
 * The block should never exceed the dwMaxCCIDMessageLength-10 in the Class Descriptor.
 */
typedef struct VUSBCARDREADERMSGXFRBLOCKCORE
{
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    /* Used only for protocol T=1 */
    uint8_t                     bBWI;
    /**
     * For reference, the absolute maximum block size for a TPDU T=0 block is 260 bytes (5 bytes command; 255 bytes data),
     * or for a TPDU T=1 block is 259 bytes, or for a short APDU T=1 block is 261 bytes, or for an extended APDU T=1 block is 65544 bytes.
     */
    uint16_t                    wLevelParameter;
    /* uint8_t                  abData[::Core.dwLength]*/
} VUSBCARDREADERMSGXFRBLOCKCORE;
typedef VUSBCARDREADERMSGXFRBLOCKCORE *PVUSBCARDREADERMSGXFRBLOCKCORE;
typedef const PVUSBCARDREADERMSGXFRBLOCKCORE PCVUSBCARDREADERMSGXFRBLOCKCORE;
AssertCompileSize(VUSBCARDREADERMSGXFRBLOCKCORE, 10);

typedef VUSBCARDREADERMSGICCPOWEOFF VUSBCARDREADERMSGGETPARAMETERS;
AssertCompileSize(VUSBCARDREADERMSGGETPARAMETERS, 10);

typedef VUSBCARDREADERMSGICCPOWEOFF VUSBCARDREADERMSGRESETPARAMETERS;
AssertCompileSize(VUSBCARDREADERMSGRESETPARAMETERS, 10);

typedef struct VUSBCARDREADERMSGSETPARAMETERSCORE
{
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    /* 0 - T0, 1 - T1 */
    uint8_t                     bProtocolNum;
    uint8_t                     abRFU[2];
} VUSBCARDREADERMSGSETPARAMETERSCORE;
AssertCompileSize(VUSBCARDREADERMSGSETPARAMETERSCORE, 10);
typedef VUSBCARDREADERMSGSETPARAMETERSCORE *PVUSBCARDREADERMSGSETPARAMETERSCORE;
typedef const PVUSBCARDREADERMSGSETPARAMETERSCORE PCVUSBCARDREADERMSGSETPARAMETERSCORE;

/* Core::Core::dwLength = 05, Core::bProtocolNum = 0 */
typedef struct VUSBCARDREADERMSGSETPARAMETERST0
{
    VUSBCARDREADERMSGSETPARAMETERSCORE  Core;
    uint8_t                             bmFindexDindex;
    uint8_t                             bmTCCKST0;
    uint8_t                             bGuardTimeT0;
    uint8_t                             bWarningIntegerT0;
    uint8_t                             bControlStop;
}VUSBCARDREADERMSGSETPARAMETERST0;
AssertCompileSize(VUSBCARDREADERMSGSETPARAMETERST0, 15);
typedef VUSBCARDREADERMSGSETPARAMETERST0 *PVUSBCARDREADERMSGSETPARAMETERST0;

/* Core::Core::dwLength = 07, Core::bProtocolNum = 1 */
typedef struct VUSBCARDREADERMSGSETPARAMETERST1
{
    VUSBCARDREADERMSGSETPARAMETERSCORE  Core;
    uint8_t                             bmFindexDindex;
    uint8_t                             bmTCCKST1;
    uint8_t                             bGuardTimeT1;
    uint8_t                             bWarningIntegerT1;
    uint8_t                             bControlStop;
    uint8_t                             bFSC;
    uint8_t                             bNadValue;
}VUSBCARDREADERMSGSETPARAMETERST1;
AssertCompileSize(VUSBCARDREADERMSGSETPARAMETERST1, 17);
typedef VUSBCARDREADERMSGSETPARAMETERST1 *PVUSBCARDREADERMSGSETPARAMETERST1;

/* Note Core::dwLength should points bytes in attachement to escape message */
typedef VUSBCARDREADERMSGICCPOWEOFF VUSBCARDREADERMSGESCAPECORE;
AssertCompileSize(VUSBCARDREADERMSGESCAPECORE, 10);

typedef struct VUSBCARDREADERMSGICCCLOCK
{
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    /* 0 - restart clock, 1 - stop clock */
    uint8_t                     bClockCommand;
    uint8_t                     abRFU[2];
} VUSBCARDREADERMSGICCCLOCK;
AssertCompileSize(VUSBCARDREADERMSGICCCLOCK, 10);

typedef struct VUSBCARDREADERMSGT0APDU
{
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    uint8_t                     bmChanges;
    uint8_t                     bGetClassRespose;
    uint8_t                     bGetClassEnvilope;
} VUSBCARDREADERMSGT0APDU;
AssertCompileSize(VUSBCARDREADERMSGT0APDU, 10);

typedef struct VUSBCARDREADERMSGSECURECORE
{
    /* Note Core::dwLength should point number of bytes of attachment */
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    uint8_t                     bBWI;
    /**
     * Note: if wLevelParameter == 0 || wLevelParameter == 1 attachment is PIN Operation Data structure
     * other values means continuation of previously sent RDP_to_PC_Secure.
     */
    uint16_t                    wLevelParameter;
} VUSBCARDREADERMSGSECURECORE;
AssertCompileSize(VUSBCARDREADERMSGSECURECORE, 10);

typedef struct CARDREADERPINOPCORE
{
    /**
     * bPinOperation is 0 then operation is verification,
     * bPinOperation is 1 PIN Modification
     * bPinOperation is 2 transfer PIN
     * bPinOperation is 3 wait ICC response
     * bPinOperation is 4 cancel PIN function
     * bPinOperation is 5 re-send last I-block (T=1)
     * bPinOperation is 6 send next ADPU (T=1)
     */
    uint8_t     bPinOperation;
} CARDREADERPINOPCORE;

#define CARDREADERPINOP_PIN_VERIFICATION    0
#define CARDREADERPINOP_PIN_MODIFICATION    1
#define CARDREADERPINOP_PIN_TRANSFER        2
#define CARDREADERPINOP_PIN_WAIT_ICC        3
#define CARDREADERPINOP_PIN_CANCEL_FUNCTION 4
#define CARDREADERPINOP_PIN_RESEND          5
#define CARDREADERPINOP_PIN_SEND_NEXT_ADPU  6
typedef struct CARDREADERPINOPVERIFICATIONCORE
{
    /* Core::bPinOperation == CARDREADERPINOP_PIN_VERIFICATION */
    CARDREADERPINOPCORE Core;
    uint8_t             bTimeOut;
    uint8_t             bmFormatString;
    uint8_t             bmPINBlockString;
    uint8_t             bmPinLengthFormat;
    uint16_t            wPINMaxExtraDigit;
    uint8_t             bEntryValidationCondition;
    uint8_t             bNumberMessage;
    uint16_t            wLangId;
    uint8_t             bMsgIndex;
    uint8_t             bTeoProlog[3];
    /* uint8_t          abPINApdu[super::Core::dwLength - 15]*/
} CARDREADERPINOPVERIFICATIONCORE;
AssertCompileSize(CARDREADERPINOPVERIFICATIONCORE, 15);

typedef struct CARDREADERPINOPMODIFICATIONCORE
{
    /* Core::bPinOperation == CARDREADERPINOP_PIN_MODIFICATION */
    CARDREADERPINOPCORE Core;
    uint8_t             bPinTimeout;
    uint8_t             bmFormatString;
    uint8_t             bmPINBlockString;
    uint8_t             bmPinLengthFormat;
    uint8_t             bInsertionOffsetOld;
    uint8_t             bInsertionOffsetNew;
    uint16_t            wPINMaxExtraDigit;
    uint8_t             bConfirmPIN;
    uint8_t             bEntryValidationCondition;
    uint8_t             bNumberMessage;
    uint16_t            wLangId;
    uint8_t             bMsgIndex1;
} CARDREADERPINOPMODIFICATIONCORE;
AssertCompileSize(CARDREADERPINOPMODIFICATIONCORE, 15);

typedef struct CARDREADERPINOPMODIFICATIONMSG1CORE
{
    CARDREADERPINOPMODIFICATIONCORE     Core;
    uint8_t                             bTeoProlog[3];
    /* uint8_t          abPINApdu[super::Core::dwLength - 18]*/
} CARDREADERPINOPMODIFICATIONMSG1CORE;
AssertCompileSize(CARDREADERPINOPMODIFICATIONMSG1CORE, 18);

typedef struct CARDREADERPINOPMODIFICATIONMSG2CORE
{
    CARDREADERPINOPMODIFICATIONCORE     Core;
    uint8_t                             bMsgIndex2;
    uint8_t                             bTeoProlog[3];
    /* uint8_t          abPINApdu[super::Core::dwLength - 19]*/
} CARDREADERPINOPMODIFICATIONMSG2CORE;
AssertCompileSize(CARDREADERPINOPMODIFICATIONMSG2CORE, 19);

typedef struct CARDREADERPINOPMODIFICATIONMSG3CORE
{
    CARDREADERPINOPMODIFICATIONCORE     Core;
    uint8_t                             bMsgIndex2;
    uint8_t                             bMsgIndex3;
    uint8_t                             bTeoProlog[3];
    /* uint8_t          abPINApdu[super::Core::dwLength - 20]*/
} CARDREADERPINOPMODIFICATIONMSG3CORE;
AssertCompileSize(CARDREADERPINOPMODIFICATIONMSG3CORE, 20);

typedef struct VUSBCARDREADERMSGMECHANICAL
{
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    uint8_t                     bFunction;
    uint8_t                     abRFU[2];
} VUSBCARDREADERMSGMECHANICAL;
AssertCompileSize(VUSBCARDREADERMSGMECHANICAL, 10);

typedef VUSBCARDREADERMSGICCPOWEOFF VUSBCARDREADERMSGABORT;
AssertCompileSize(VUSBCARDREADERMSGABORT, 10);

typedef struct VUSBCARDREADERMSGSETDATARATEANDCLOCKFREQUENCY
{
    /* Core::dwLength = 8 */
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    uint8_t                     abRFU[3];
    uint32_t                    dwClockFrequency;
    uint32_t                    dwDataRate;
} VUSBCARDREADERMSGSETDATARATEANDCLOCKFREQUENCY;

/* RESPONSES */

/**
 * from spec 6.2.6 table 6.2-3
 * note: 3 value is RFU for both bmICCStatus and bmCommandStatus
 */
#define VUSBCARDREADER_MSG_STATUS_MAKE_STATUS_REGISTER(bmICCStatus, bmCommandStatus) ((((bmCommandStatus) & 0x3) << 6) | ((bmICCStatus) & 0x3))

/* bmICCStatus */
#define VUSBCARDREADER_ICC_STATUS_PRESENT  0
#define VUSBCARDREADER_ICC_STATUS_INACTIVE 1
#define VUSBCARDREADER_ICC_STATUS_NO_CARD  2

/* from spec these error codes are actual if bmCommandStatus = 1 */
#define VUSBCARDREADER_MSG_STATUS_ERR_CMD_ABORTED                   0xff
#define VUSBCARDREADER_MSG_STATUS_ERR_ICC_MUTE                      0xfe
#define VUSBCARDREADER_MSG_STATUS_ERR_XFR_PARITY_ERROR              0xfd
#define VUSBCARDREADER_MSG_STATUS_ERR_XFR_OVERRUN                   0xfc
#define VUSBCARDREADER_MSG_STATUS_ERR_HW_ERROR                      0xfb
#define VUSBCARDREADER_MSG_STATUS_ERR_BAD_ATR_TS                    0xf8
#define VUSBCARDREADER_MSG_STATUS_ERR_BAD_ATR_TCK                   0xf7
#define VUSBCARDREADER_MSG_STATUS_ERR_ICC_PROTOCOL_NOT_SUPPORTED    0xf6
#define VUSBCARDREADER_MSG_STATUS_ERR_ICC_CLASS_NOT_SUPPORTED       0xf5
#define VUSBCARDREADER_MSG_STATUS_ERR_PROCEDURE_BYTE_CONFLICT       0xf4
#define VUSBCARDREADER_MSG_STATUS_ERR_DEACTIVATED_PROTOCOL          0xf3
#define VUSBCARDREADER_MSG_STATUS_ERR_BUSY_WITH_AUTO_SEQUENCE       0xf2
#define VUSBCARDREADER_MSG_STATUS_ERR_PIN_TIMEOUT                   0xf0
#define VUSBCARDREADER_MSG_STATUS_ERR_PIN_CANCELED                  0xef
#define VUSBCARDREADER_MSG_STATUS_ERR_CMD_SLOT_BUSY                 0xe0
#define VUSBCARDREADER_MSG_STATUS_ERR_SLOT_DOESNT_EXIST             0x5
#define VUSBCARDREADER_MSG_STATUS_ERR_NON                           0x0
typedef struct VUSBCARDREADERMSGRESPONSECORE
{
    VUSBCARDREADERMSGBULKHEADERCORE Core;
    uint8_t                     bStatus;
    uint8_t                     bError;
} VUSBCARDREADERMSGRESPONSECORE, *PVUSBCARDREADERMSGRESPONSECORE;
typedef PVUSBCARDREADERMSGRESPONSECORE *PPVUSBCARDREADERMSGRESPONSECORE;


typedef struct VUSBCARDREADERMSGRESPONSEDATABLOCKCORE
{
    VUSBCARDREADERMSGRESPONSECORE   Core;
    uint8_t                         bChainParameter;
    /* uint8_t                      abData[Core::Core::dwLength]*/
} VUSBCARDREADERMSGRESPONSEDATABLOCKCORE;
AssertCompileSize(VUSBCARDREADERMSGRESPONSEDATABLOCKCORE, 10);
typedef VUSBCARDREADERMSGRESPONSEDATABLOCKCORE *PVUSBCARDREADERMSGRESPONSEDATABLOCKCORE;

typedef struct VUSBCARDREADERMSGRESPONSESLOTSTATUS
{
    VUSBCARDREADERMSGRESPONSECORE   Core;
    uint8_t                         bClockStatus;
} VUSBCARDREADERMSGRESPONSESLOTSTATUS, *PVUSBCARDREADERMSGRESPONSESLOTSTATUS;
AssertCompileSize(VUSBCARDREADERMSGRESPONSESLOTSTATUS, 10);

typedef struct VUSBCARDREADERMSGRESPONSEPARAMETERSCORE
{
    VUSBCARDREADERMSGRESPONSECORE   Core;
    uint8_t                         bProtocolNumber;
} VUSBCARDREADERMSGRESPONSEPARAMETERSCORE;
AssertCompileSize(VUSBCARDREADERMSGRESPONSEPARAMETERSCORE, 10);
typedef VUSBCARDREADERMSGRESPONSEPARAMETERSCORE *PVUSBCARDREADERMSGRESPONSEPARAMETERSCORE;

typedef struct VUSBCARDREADERMSGRESPONSEESCAPECORE
{
    VUSBCARDREADERMSGRESPONSECORE   Core;
    uint8_t                         bRFU;
    /* uint8_t                      abData[Core::Core::dwLength] */
} VUSBCARDREADERMSGRESPONSEESCAPECORE;
AssertCompileSize(VUSBCARDREADERMSGRESPONSEESCAPECORE, 10);
typedef VUSBCARDREADERMSGRESPONSEESCAPECORE *PVUSBCARDREADERMSGRESPONSEESCAPECORE;

typedef struct VUSBCARDREADERMSGRESPONSEDATARATEANDCLOCKFREQUENCY
{
    /* Core::Core::dwLength = 8 */
    VUSBCARDREADERMSGRESPONSECORE   Core;
    uint8_t                         bRFU;
    uint32_t                        dwClockFrequency;
    uint32_t                        dwDataRate;
} VUSBCARDREADERMSGRESPONSEDATARATEANDCLOCKFREQUENCY;
AssertCompileSize(VUSBCARDREADERMSGRESPONSEDATARATEANDCLOCKFREQUENCY, 18);
typedef VUSBCARDREADERMSGRESPONSEDATARATEANDCLOCKFREQUENCY *PVUSBCARDREADERMSGRESPONSEDATARATEANDCLOCKFREQUENCY;

typedef struct VUSBCARDREADERDESCDEVICECLASS
{
    uint8_t     bLength;
    uint8_t     bDescriptorType;
    uint16_t    bcdCCID;
    uint8_t     bMaxSlotIndex;
    uint8_t     bVoltageSupport;
    uint32_t    dwProtocols;
    uint32_t    dwDefaultClock;
    uint32_t    dwMaximumClock;
    uint8_t     bNumClockSupported;
    uint32_t    dwDataRate;
    uint32_t    dwMaximumDataRate;
    uint8_t     bNumDataRateSupported;
    uint32_t    dwMaxIFSD;
    uint32_t    dwSyncProtocols;
    uint32_t    dwMechanical;
    uint32_t    dwFeatures;
    uint32_t    dwMaxCCIDMessageLength;
    uint8_t     bClassGetResponse;
    uint8_t     bClassGetEnvilope;
    uint16_t    wLcdLayout;
    uint8_t     bPINSupport;
    uint8_t     bMaxCCIDBusySlots;
} VUSBCARDREADERDESCDEVICECLASS;

#define VUSBCARDREADER_FEATURE_NOFUTURES                    0
#define VUSBCARDREADER_FEATURE_AUTOMATIC_PARAMETER_CONF     RT_BIT(1)
#define VUSBCARDREADER_FEATURE_AUTOMATIC_ACTIVATION         RT_BIT(2)
#define VUSBCARDREADER_FEATURE_AUTOMATIC_VOLTAGE_SELECTION  RT_BIT(3)
#define VUSBCARDREADER_FEATURE_AUTOMATIC_VOLTAGE_SELECTION  RT_BIT(3)

AssertCompileSize(VUSBCARDREADERDESCDEVICECLASS, 0x36);

typedef struct VUSBCARDREADERNOTIFYSLOTCHANGE
{
    VUSBCARDREADERMSGHEADERCORE Core;
    /* no more than 4 slots */
    uint8_t                     bmSlotICCState;
} VUSBCARDREADERNOTIFYSLOTCHANGE, *PVUSBCARDREADERNOTIFYSLOTCHANGE;
AssertCompileSize(VUSBCARDREADERNOTIFYSLOTCHANGE, 2);

#pragma pack()

#endif /* !VBOX_INCLUDED_SRC_Security_vusbcardreader_h */
