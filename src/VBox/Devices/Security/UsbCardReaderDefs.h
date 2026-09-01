/* $Id: UsbCardReaderDefs.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * UsbCardReaderDefs.h - smartcard constants.
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

#ifndef VBOX_INCLUDED_SRC_Security_UsbCardReaderDefs_h
#define VBOX_INCLUDED_SRC_Security_UsbCardReaderDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/*
 * The binary constants are defined according to the RDP SmartCard channel spec (RDPESC).
 */

#define VBOX_SCARD_S_SUCCESS                 ((int32_t)0x00000000)
#define VBOX_SCARD_F_INTERNAL_ERROR          ((int32_t)0x80100001)
#define VBOX_SCARD_E_CANCELLED               ((int32_t)0x80100002)
#define VBOX_SCARD_E_INVALID_HANDLE          ((int32_t)0x80100003)
#define VBOX_SCARD_E_INVALID_PARAMETER       ((int32_t)0x80100004)
#define VBOX_SCARD_E_INVALID_TARGET          ((int32_t)0x80100005)
#define VBOX_SCARD_E_NO_MEMORY               ((int32_t)0x80100006)
#define VBOX_SCARD_F_WAITED_TOO_LONG         ((int32_t)0x80100007)
#define VBOX_SCARD_E_INSUFFICIENT_BUFFER     ((int32_t)0x80100008)
#define VBOX_SCARD_E_UNKNOWN_READER          ((int32_t)0x80100009)
#define VBOX_SCARD_E_TIMEOUT                 ((int32_t)0x8010000A)
#define VBOX_SCARD_E_SHARING_VIOLATION       ((int32_t)0x8010000B)
#define VBOX_SCARD_E_NO_SMARTCARD            ((int32_t)0x8010000C)
#define VBOX_SCARD_E_UNKNOWN_CARD            ((int32_t)0x8010000D)
#define VBOX_SCARD_E_CANT_DISPOSE            ((int32_t)0x8010000E)
#define VBOX_SCARD_E_PROTO_MISMATCH          ((int32_t)0x8010000F)
#define VBOX_SCARD_E_NOT_READY               ((int32_t)0x80100010)
#define VBOX_SCARD_E_INVALID_VALUE           ((int32_t)0x80100011)
#define VBOX_SCARD_E_SYSTEM_CANCELLED        ((int32_t)0x80100012)
#define VBOX_SCARD_F_COMM_ERROR              ((int32_t)0x80100013)
#define VBOX_SCARD_F_UNKNOWN_ERROR           ((int32_t)0x80100014)
#define VBOX_SCARD_E_INVALID_ATR             ((int32_t)0x80100015)
#define VBOX_SCARD_E_NOT_TRANSACTED          ((int32_t)0x80100016)
#define VBOX_SCARD_E_READER_UNAVAILABLE      ((int32_t)0x80100017)
#define VBOX_SCARD_P_SHUTDOWN                ((int32_t)0x80100018)
#define VBOX_SCARD_E_PCI_TOO_SMALL           ((int32_t)0x80100019)
#define VBOX_SCARD_E_ICC_INSTALLATION        ((int32_t)0x80100020)
#define VBOX_SCARD_E_ICC_CREATEORDER         ((int32_t)0x80100021)
#define VBOX_SCARD_E_UNSUPPORTED_FEATURE     ((int32_t)0x80100022)
#define VBOX_SCARD_E_DIR_NOT_FOUND           ((int32_t)0x80100023)
#define VBOX_SCARD_E_FILE_NOT_FOUND          ((int32_t)0x80100024)
#define VBOX_SCARD_E_NO_DIR                  ((int32_t)0x80100025)
#define VBOX_SCARD_E_READER_UNSUPPORTED      ((int32_t)0x8010001A)
#define VBOX_SCARD_E_DUPLICATE_READER        ((int32_t)0x8010001B)
#define VBOX_SCARD_E_CARD_UNSUPPORTED        ((int32_t)0x8010001C)
#define VBOX_SCARD_E_NO_SERVICE              ((int32_t)0x8010001D)
#define VBOX_SCARD_E_SERVICE_STOPPED         ((int32_t)0x8010001E)
#define VBOX_SCARD_E_UNEXPECTED              ((int32_t)0x8010001F)
#define VBOX_SCARD_E_NO_FILE                 ((int32_t)0x80100026)
#define VBOX_SCARD_E_NO_ACCESS               ((int32_t)0x80100027)
#define VBOX_SCARD_E_WRITE_TOO_MANY          ((int32_t)0x80100028)
#define VBOX_SCARD_E_BAD_SEEK                ((int32_t)0x80100029)
#define VBOX_SCARD_E_INVALID_CHV             ((int32_t)0x8010002A)
#define VBOX_SCARD_E_UNKNOWN_RES_MSG         ((int32_t)0x8010002B)
#define VBOX_SCARD_E_NO_SUCH_CERTIFICATE     ((int32_t)0x8010002C)
#define VBOX_SCARD_E_CERTIFICATE_UNAVAILABLE ((int32_t)0x8010002D)
#define VBOX_SCARD_E_NO_READERS_AVAILABLE    ((int32_t)0x8010002E)
#define VBOX_SCARD_E_COMM_DATA_LOST          ((int32_t)0x8010002F)
#define VBOX_SCARD_E_NO_KEY_CONTAINER        ((int32_t)0x80100030)
#define VBOX_SCARD_E_SERVER_TOO_BUSY         ((int32_t)0x80100031)
#define VBOX_SCARD_E_PIN_CACHE_EXPIRED       ((int32_t)0x80100032)
#define VBOX_SCARD_E_NO_PIN_CACHE            ((int32_t)0x80100033)
#define VBOX_SCARD_E_READ_ONLY_CARD          ((int32_t)0x80100034)
#define VBOX_SCARD_W_UNSUPPORTED_CARD        ((int32_t)0x80100065)
#define VBOX_SCARD_W_UNRESPONSIVE_CARD       ((int32_t)0x80100066)
#define VBOX_SCARD_W_UNPOWERED_CARD          ((int32_t)0x80100067)
#define VBOX_SCARD_W_RESET_CARD              ((int32_t)0x80100068)
#define VBOX_SCARD_W_REMOVED_CARD            ((int32_t)0x80100069)
#define VBOX_SCARD_W_SECURITY_VIOLATION      ((int32_t)0x8010006A)
#define VBOX_SCARD_W_WRONG_CHV               ((int32_t)0x8010006B)
#define VBOX_SCARD_W_CHV_BLOCKED             ((int32_t)0x8010006C)
#define VBOX_SCARD_W_EOF                     ((int32_t)0x8010006D)
#define VBOX_SCARD_W_CANCELLED_BY_USER       ((int32_t)0x8010006E)
#define VBOX_SCARD_W_CARD_NOT_AUTHENTICATED  ((int32_t)0x8010006F)
#define VBOX_SCARD_W_CACHE_ITEM_NOT_FOUND    ((int32_t)0x80100070)
#define VBOX_SCARD_W_CACHE_ITEM_STALE        ((int32_t)0x80100071)
#define VBOX_SCARD_W_CACHE_ITEM_TOO_BIG      ((int32_t)0x80100072)


#define VBOX_SCARD_PROTOCOL_UNDEFINED        0x00000000
#define VBOX_SCARD_PROTOCOL_T0               0x00000001
#define VBOX_SCARD_PROTOCOL_T1               0x00000002
#define VBOX_SCARD_PROTOCOL_Tx               0x00000003
#define VBOX_SCARD_PROTOCOL_RAW              0x00010000
#define VBOX_SCARD_PROTOCOL_DEFAULT          0x80000000
#define VBOX_SCARD_PROTOCOL_OPTIMAL          0x00000000


#define VBOX_SCARD_STATE_UNAWARE             0x0000
#define VBOX_SCARD_STATE_IGNORE              0x0001
#define VBOX_SCARD_STATE_CHANGED             0x0002
#define VBOX_SCARD_STATE_UNKNOWN             0x0004
#define VBOX_SCARD_STATE_UNAVAILABLE         0x0008
#define VBOX_SCARD_STATE_EMPTY               0x0010
#define VBOX_SCARD_STATE_PRESENT             0x0020
#define VBOX_SCARD_STATE_ATRMATCH            0x0040
#define VBOX_SCARD_STATE_EXCLUSIVE           0x0080
#define VBOX_SCARD_STATE_INUSE               0x0100
#define VBOX_SCARD_STATE_MUTE                0x0200
#define VBOX_SCARD_STATE_UNPOWERED           0x0400
#define VBOX_SCARD_STATE_MASK                UINT32_C(0x0000FFFF)
#define VBOX_SCARD_STATE_COUNT_MASK          UINT32_C(0xFFFF0000)


#define VBOX_SCARD_SHARE_EXCLUSIVE           0x00000001
#define VBOX_SCARD_SHARE_SHARED              0x00000002
#define VBOX_SCARD_SHARE_DIRECT              0x00000003


#define VBOX_SCARD_LEAVE_CARD                0x00000000
#define VBOX_SCARD_RESET_CARD                0x00000001
#define VBOX_SCARD_UNPOWER_CARD              0x00000002
#define VBOX_SCARD_EJECT_CARD                0x00000003


#define VBOX_SCARD_UNKNOWN                   0x00000000
#define VBOX_SCARD_ABSENT                    0x00000001
#define VBOX_SCARD_PRESENT                   0x00000002
#define VBOX_SCARD_SWALLOWED                 0x00000003
#define VBOX_SCARD_POWERED                   0x00000004
#define VBOX_SCARD_NEGOTIABLE                0x00000005
#define VBOX_SCARD_SPECIFICMODE              0x00000006


#define VBOX_SCARD_ATTR_VALUE(Class, Tag) ((((uint32_t)(Class)) << 16) | ((uint32_t)(Tag)))

#define VBOX_SCARD_CLASS_VENDOR_INFO     1   // Vendor information definitions
#define VBOX_SCARD_CLASS_COMMUNICATIONS  2   // Communication definitions
#define VBOX_SCARD_CLASS_PROTOCOL        3   // Protocol definitions
#define VBOX_SCARD_CLASS_POWER_MGMT      4   // Power Management definitions
#define VBOX_SCARD_CLASS_SECURITY        5   // Security Assurance definitions
#define VBOX_SCARD_CLASS_MECHANICAL      6   // Mechanical characteristic definitions
#define VBOX_SCARD_CLASS_VENDOR_DEFINED  7   // Vendor specific definitions
#define VBOX_SCARD_CLASS_IFD_PROTOCOL    8   // Interface Device Protocol options
#define VBOX_SCARD_CLASS_ICC_STATE       9   // ICC State specific definitions
#define VBOX_SCARD_CLASS_PERF       0x7ffe   // performace counters
#define VBOX_SCARD_CLASS_SYSTEM     0x7fff   // System-specific definitions

#define VBOX_SCARD_ATTR_ATR_STRING VBOX_SCARD_ATTR_VALUE(VBOX_SCARD_CLASS_ICC_STATE, 0x0303)


#define VBOX_SCARD_CTL_CODE(code) ((0x31 << 16) | ((code) << 2))

/* This CTL code isn't defined in PCSC framework, but recommended by Microsoft
 * for switching protocols see note to dwPreferedProtocols in
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa379473%28v=vs.85%29.aspx
 */
#define VBOX_IOCTL_SMARTCARD_SET_PROTOCOL VBOX_SCARD_CTL_CODE(12)

#endif /* !VBOX_INCLUDED_SRC_Security_UsbCardReaderDefs_h */
