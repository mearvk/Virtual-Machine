/* $Id: vusbvideo.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * vusbvideo - defines usb related types.
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

#ifndef VBOX_INCLUDED_SRC_Video_vusbvideo_h
#define VBOX_INCLUDED_SRC_Video_vusbvideo_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* UVC1.1: A.1. Video Interface Class Code */
#define VUSB_CC_VIDEO 0x0E

/* UVC1.1: A.2. Video Interface Subclass Codes */
#define VUSB_V_SC_VIDEOUNDEFINED             0x00
#define VUSB_V_SC_VIDEOCONTROL               0x01
#define VUSB_V_SC_VIDEOSTREAMING             0x02
#define VUSB_V_SC_VIDEO_INTERFACE_COLLECTION 0x03

/* UVC1.1: A.3. Video Interface Protocol Codes */
#define VUSB_PC_PROTOCOL_UNDEFINED 0x00

/* UVC1.1: A.4. Video Class-Specific Descriptor Types */
#define VUSB_V_CS_INTERFACE 0x24
#define VUSB_V_CS_ENDPOINT  0x25

/* UVC1.1: A.5. Video Class-Specific VC Interface Descriptor Subtypes */
#define VUSB_V_VC_DESCRIPTOR_UNDEFINED 0x00
#define VUSB_V_VC_HEADER               0x01
#define VUSB_V_VC_INPUT_TERMINAL       0x02
#define VUSB_V_VC_OUTPUT_TERMINAL      0x03
#define VUSB_V_VC_SELECTOR_UNIT        0x04
#define VUSB_V_VC_PROCESSING_UNIT      0x05
#define VUSB_V_VC_EXTENSION_UNIT       0x06

/* UVC1.1: A.6. Video Class-Specific VS Interface Descriptor Subtypes */
#define VUSB_V_VS_HEADER       0x01
#define VUSB_V_VS_FORMAT_MJPEG 0x06
#define VUSB_V_VS_FRAME_JPEG   0x07
#define VUSB_V_VS_COLORFORMAT  0x0D

/* UVC1.1: A.7. Video Class-Specific Endpoint Descriptor Subtypes */
#define VUSB_V_EP_UNDEFINED 0x00
#define VUSB_V_EP_GENERAL   0x01
#define VUSB_V_EP_ENDPOINT  0x02
#define VUSB_V_EP_INTERRUPT 0x03

/* UVC1.1: A.8. Video Class-Specific Request Codes */
#define VUSB_V_REQ_RC_UNDEFINED 0x00
#define VUSB_V_REQ_SET_CUR      0x01
#define VUSB_V_REQ_GET_CUR      0x81
#define VUSB_V_REQ_GET_MIN      0x82
#define VUSB_V_REQ_GET_MAX      0x83
#define VUSB_V_REQ_GET_RES      0x84
#define VUSB_V_REQ_GET_LEN      0x85
#define VUSB_V_REQ_GET_INFO     0x86
#define VUSB_V_REQ_GET_DEF      0x87

/* UVC1.1: A.9.1. VideoControl Interface Control Selectors */
#define VUSB_V_VC_CONTROL_UNDEFINED          0x00
#define VUSB_V_VC_VIDEO_POWER_MODE_CONTROL   0x01
#define VUSB_V_VC_REQUEST_ERROR_CODE_CONTROL 0x02
#define VUSB_V_VC_MAX                        0x03

/* UVC1.1: A.9.2. Terminal Control Selectors */
#define VUSB_V_TE_CONTROL_UNDEFINED 0x00
#define VUSB_V_TE_MAX               0x01

/* UVC1.1: A.9.3. Selector Unit Control Selectors */
#define VUSB_V_SU_CONTROL_SELECT_UNDEFINED 0x0
#define VUSB_V_SU_INPUT_SELECT_CONTROL     0x1
#define VUSB_V_SU_MAX                      0x2

/* UVC1.1: A.9.4. Camera Terminal Control Selectors */
#define VUSB_V_CT_CONTROL_UNDEFINED              0x00
#define VUSB_V_CT_SCANNING_MODE_CONTROL          0x01
#define VUSB_V_CT_AE_MODE_CONTROL                0x02
#define VUSB_V_CT_AE_PRIORITY_CONTROL            0x03
#define VUSB_V_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL 0x04
#define VUSB_V_CT_EXPOSURE_TIME_RELATIVE_CONTROL 0x05
#define VUSB_V_CT_FOCUS_ABSOLUTE_CONTROL         0x06
#define VUSB_V_CT_FOCUS_RELATIVE_CONTROL         0x07
#define VUSB_V_CT_FOCUS_AUTO_CONTROL             0x08
#define VUSB_V_CT_IRIS_ABSOLUTE_CONTROL          0x09
#define VUSB_V_CT_IRIS_RELATIVE_CONTROL          0x0A
#define VUSB_V_CT_ZOOM_ABSOLUTE_CONTROL          0x0B
#define VUSB_V_CT_ZOOM_RELATIVE_CONTROL          0x0C
#define VUSB_V_CT_PANTILT_ABSOLUTE_CONTROL       0x0D
#define VUSB_V_CT_PANTILT_RELATIVE_CONTROL       0x0E
#define VUSB_V_CT_ROLL_ABSOLUTE_CONTROL          0x0F
#define VUSB_V_CT_ROLL_RELATIVE_CONTROL          0x10
#define VUSB_V_CT_PRIVACY_CONTROL                0x11
#define VUSB_V_CT_MAX                            0x12

/* UVC1.1: A.9.5. Processing Unit Control Selectors */
#define VUSB_V_PU_CONTROL_UNDEFINED                      0x00
#define VUSB_V_PU_BACKLIGHT_COMPENSATION_CONTROL         0x01
#define VUSB_V_PU_BRIGHTNESS_CONTROL                     0x02
#define VUSB_V_PU_CONTRAST_CONTROL                       0x03
#define VUSB_V_PU_GAIN_CONTROL                           0x04
#define VUSB_V_PU_POWER_LINE_FREQUENCY_CONTROL           0x05
#define VUSB_V_PU_HUE_CONTROL                            0x06
#define VUSB_V_PU_SATURATION_CONTROL                     0x07
#define VUSB_V_PU_SHARPNESS_CONTROL                      0x08
#define VUSB_V_PU_GAMMA_CONTROL                          0x09
#define VUSB_V_PU_WHITE_BALANCE_TEMPERATURE_CONTROL      0x0A
#define VUSB_V_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL 0x0B
#define VUSB_V_PU_WHITE_BALANCE_COMPONENT_CONTROL        0x0C
#define VUSB_V_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL   0x0D
#define VUSB_V_PU_DIGITAL_MULTIPLIER_CONTROL             0x0E
#define VUSB_V_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL       0x0F
#define VUSB_V_PU_HUE_AUTO_CONTROL                       0x10
#define VUSB_V_PU_ANALOG_VIDEO_STANDARD_CONTROL          0x11
#define VUSB_V_PU_ANALOG_LOCK_STATUS_CONTROL             0x12
#define VUSB_V_PU_MAX                                    0x13

/* UVC1.1: A.9.7. VideoStreaming Interface Control Selectors */
#define VUSB_V_VS_CONTROL_UNDEFINED            0x00
#define VUSB_V_VS_PROBE_CONTROL                0x01
#define VUSB_V_VS_COMMIT_CONTROL               0x02
#define VUSB_V_VS_STILL_PROBE_CONTROL          0x03
#define VUSB_V_VS_STILL_COMMIT_CONTROL         0x04
#define VUSB_V_VS_STILL_IMAGE_TRIGGER_CONTROL  0x05
#define VUSB_V_VS_STREAM_ERROR_CODE_CONTROL    0x06
#define VUSB_V_VS_GENERATE_KEY_FRAME_CONTROL   0x07
#define VUSB_V_VS_UPDATE_FRAME_SEGMENT_CONTROL 0x08
#define VUSB_V_VS_SYNCH_DELAY_CONTROL          0x09
#define VUSB_V_VS_MAX                          0x0A

/* UVC1.1: B.1. USB Terminal Types */
#define VUSB_V_TT_VENDOR_SPECIFIC 0x0100
#define VUSB_V_TT_STREAMING       0x0101

/* UVC1.1: B.2. Input Terminal Types */
#define VUSB_V_ITT_VENDOR_SPECIFIC       0x0200
#define VUSB_V_ITT_CAMERA                0x0201
#define VUSB_V_ITT_MEDIA_TRANSPORT_INPUT 0x0202


/* Camera Terminal bmControls bits. */
#define VUSB_V_CT_CTRL_SCANNING_MODE            0x00000001 /* D0: Scanning Mode */
#define VUSB_V_CT_CTRL_AE_MODE                  0x00000002 /* D1: Auto-Exposure Mode */
#define VUSB_V_CT_CTRL_AE_PRIORITY              0x00000004 /* D2: Auto-Exposure Priority */
#define VUSB_V_CT_CTRL_EXPOSURE_TIME_ABSOLUTE   0x00000008 /* D3: Exposure Time (Absolute) */
#define VUSB_V_CT_CTRL_EXPOSURE_TIME_RELATIVE   0x00000010 /* D4: Exposure Time (Relative) */
#define VUSB_V_CT_CTRL_FOCUS_ABSOLUTE           0x00000020 /* D5: Focus (Absolute) */
#define VUSB_V_CT_CTRL_FOCUS_RELATIVE           0x00000040 /* D6: Focus (Relative) */
#define VUSB_V_CT_CTRL_IRIS_ABSOLUTE            0x00000080 /* D7: Iris (Absolute) */
#define VUSB_V_CT_CTRL_IRIS_RELATIVE            0x00000100 /* D8: Iris (Relative) */
#define VUSB_V_CT_CTRL_ZOOM_ABSOLUTE            0x00000200 /* D9: Zoom (Absolute) */
#define VUSB_V_CT_CTRL_ZOOM_RELATIVE            0x00000400 /* D10: Zoom (Relative) */
#define VUSB_V_CT_CTRL_PANTILT_ABSOLUTE         0x00000800 /* D11: PanTilt (Absolute) */
#define VUSB_V_CT_CTRL_PANTILT_RELATIVE         0x00001000 /* D12: PanTilt (Relative) */
#define VUSB_V_CT_CTRL_ROLL_ABSOLUTE            0x00002000 /* D13: Roll (Absolute) */
#define VUSB_V_CT_CTRL_ROLL_RELATIVE            0x00004000 /* D14: Roll (Relative) */
#define VUSB_V_CT_CTRL_RESERVED1                0x00008000 /* D15: Reserved */
#define VUSB_V_CT_CTRL_RESERVED2                0x00010000 /* D16: Reserved */
#define VUSB_V_CT_CTRL_FOCUS_AUTO               0x00020000 /* D17: Focus, Auto */
#define VUSB_V_CT_CTRL_PRIVACY                  0x00040000 /* D18: Privacy */

/* Processing Unit bmControls bits. */
#define VUSB_V_PU_CTRL_BRIGHTNESS                0x00000001 /* D0: Brightness */
#define VUSB_V_PU_CTRL_CONTRAST                  0x00000002 /* D1: Contrast */
#define VUSB_V_PU_CTRL_HUE                       0x00000004 /* D2: Hue */
#define VUSB_V_PU_CTRL_SATURATION                0x00000008 /* D3: Saturation */
#define VUSB_V_PU_CTRL_SHARPNESS                 0x00000010 /* D4: Sharpness */
#define VUSB_V_PU_CTRL_GAMMA                     0x00000020 /* D5: Gamma */
#define VUSB_V_PU_CTRL_WHITE_BALANCE_TEMPERATURE 0x00000040 /* D6: White Balance Temperature */
#define VUSB_V_PU_CTRL_WHITE_BALANCE_COMPONENT   0x00000080 /* D7: White Balance Component */
#define VUSB_V_PU_CTRL_BACKLIGHT_COMPENSATION    0x00000100 /* D8: Backlight Compensation */
#define VUSB_V_PU_CTRL_GAIN                      0x00000200 /* D9: Gain */
#define VUSB_V_PU_CTRL_POWER_LINE_FREQUENCY      0x00000400 /* D10: Power Line Frequency */
#define VUSB_V_PU_CTRL_HUE_AUTO                  0x00000800 /* D11: Hue, Auto */
#define VUSB_V_PU_CTRL_WHITE_BALANCE_TEMPERATURE_AUTO 0x00001000 /* D12: White Balance Temperature, Auto */
#define VUSB_V_PU_CTRL_WHITE_BALANCE_COMPONENT_AUTO   0x00002000 /* D13: White Balance Component, Auto */
#define VUSB_V_PU_CTRL_DIGITAL_MULTIPLIER        0x00004000 /* D14: Digital Multiplier */
#define VUSB_V_PU_CTRL_DIGITAL_MULTIPLIER_LIMIT  0x00008000 /* D15: Digital Multiplier Limit */

/* All structures in this file. */
#pragma pack(1)

/*
 * Video Class Descriptors.
 */

/* UVC1.1: 3.7.2 Class-Specific VC Interface Descriptor */
typedef struct VUSBVDESCVCHEADER
{
    uint8_t  bLength; /* Includes baInterfaceNr. */
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint16_t bcdUVC;
    uint16_t wTotalLength;
    uint32_t dwClockFrequency;
    uint8_t  bInCollection;
    /* baInterfaceNr[bInCollection] follows. */
} VUSBVDESCVCHEADER;
AssertCompileSize(VUSBVDESCVCHEADER, 12);

/* UVC1.1: 3.7.2.1 Input Terminal Descriptor */
typedef struct VUSBVDESCINPUTTERMINAL
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bTerminalID;
    uint16_t wTerminalType;
    uint8_t  bAssocTerminal;
    uint8_t  iTerminal;
} VUSBVDESCINPUTTERMINAL;

/* UVC1.1: 3.7.2.2 Output Terminal Descriptor */
typedef struct VUSBVDESCOUTPUTTERMINAL
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bTerminalID;
    uint16_t wTerminalType;
    uint8_t  bAssocTerminal;
    uint8_t  bSourceID; /* Unit or Terminal to which this Terminal is connected. */
    uint8_t  iTerminal;
} VUSBVDESCOUTPUTTERMINAL;

/* UVC1.1: 3.7.2.3 Camera Terminal Descriptor */
typedef struct VUSBVDESCCAMERATERMINAL
{
    VUSBVDESCINPUTTERMINAL Core;
    uint16_t wObjectiveFocalLengthMin;
    uint16_t wObjectiveFocalLengthMax;
    uint16_t wOcularFocalLength;
    uint8_t  bControlSize;
    uint8_t  bmControls[3];
} VUSBVDESCCAMERATERMINAL;

/* UVC1.1: 3.7.2.5 Processing Unit Descriptor */
typedef struct VUSBVDESCPROCESSINGUNIT
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bUnitID;
    uint8_t  bSourceID;
    uint16_t wMaxMultiplier;
    uint8_t  bControlSize;
    uint8_t  bmControls[3];
    uint8_t  iProcessing;
} VUSBVDESCPROCESSINGUNIT;

/* UVC1.1: 3.8.2.2 Class-specific VC Interrupt Endpoint Descriptor */
typedef struct VUSBVDESCVCENDPOINT
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint16_t wMaxTransferSize;
} VUSBVDESCVCENDPOINT;

/* UVC1.1: 3.9.2.1 Input Header Descriptor */
typedef struct VUSBVDESCVSINPUTHEADER
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bNumFormats;
    uint16_t wTotalLenght;
    uint8_t  bEndpointAddress;
    uint8_t  bmInfo;
    uint8_t  bTerminalLink;
    uint8_t  bStillCaptureMethod;
    uint8_t  bTriggerSupport;
    uint8_t  bTriggerUsage;
    uint8_t  bControlSize; /* Size of each bmaControls in bytes */
    /* bmaControls[bNumFormats] follow. */
} VUSBVDESCVSINPUTHEADER;

/*
 * UVC1.1: 3.9.2.5 Still Image Frame Descriptor
 *
 *     VUSBVDESCVSSTILLIMAGEFRAMEHDR hdr;
 *     VUSBVDESCVSSTILLIMAGEFRAMESIZEHDR hdrSize;
 *     width1, height1,
 *     widthN, heightN,
 *     VUSBVDESCVSSTILLIMAGEFRAMECOMPRHDR hdrCompr;
 *     bCompression1,
 *     bCompressionN
 */
typedef struct VUSBVDESCVSSTILLIMAGEFRAMEHDR
{
    uint8_t bLength; /* Includes Size and Compression patterns. */
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bEndpointAddress;
} VUSBVDESCVSSTILLIMAGEFRAMEHDR;

typedef struct VUSBVDESCVSSTILLIMAGEFRAMESIZEHDR
{
    uint8_t bNumImageSizePatterns;
} VUSBVDESCVSSTILLIMAGEFRAMESIZEHDR;

typedef struct VUSBVDESCVSSTILLIMAGEFRAMECOMPRHDR
{
    uint8_t bNumCompressionPatterns;
} VUSBVDESCVSSTILLIMAGEFRAMECOMPRHDR;

/* UVC1.1: 3.9.2.6 Color Matching Descriptor */
typedef struct VUSBVDESCVSCOLORMATCHING
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bColorPrimaries;
    uint8_t bTransferCharacteristics;
    uint8_t bMatrixCoefficients;
} VUSBVDESCVSCOLORMATCHING;

/*
 * Status Interrupt.
 */

#define VUSB_V_STATUSINTR_TYPE_VC 1
#define VUSB_V_STATUSINTR_TYPE_VS 2

typedef struct VUSBVSTATUSINTR
{
    uint8_t bStatusType; /* VUSB_V_STATUSINTR_TYPE_* */
    uint8_t bOriginator; /* Id of the Terminal or Unit. */
} VUSBVSTATUSINTR;

#define VUSB_V_STATUSINTR_VC_EV_CONTROL_CHANGE 0

#define VUSB_V_STATUSINTR_VC_ATTR_VALUE   0
#define VUSB_V_STATUSINTR_VC_ATTR_INFO    1
#define VUSB_V_STATUSINTR_VC_ATTR_FAILURE 2

typedef struct VUSBVSTATUSINTRVC
{
    VUSBVSTATUSINTR Core;
    uint8_t bEvent;       /* VUSB_V_STATUSINTR_VC_EV_ */
    uint8_t bSelector;    /* Control Selector */
    uint8_t bAttribute;   /* VUSB_V_STATUSINTR_VC_ATTR_* */
    /* bValue follow, the format is defined by bAttribute and bSelector.  */
} VUSBVSTATUSINTRVC;

#define VUSB_V_STATUSINTR_VS_EV_BUTTON   0x00
/* 0x01..0xFF = STREAM_ERROR */

#define VUSB_V_STATUSINTR_VS_BUTTON_RELEASED    0
#define VUSB_V_STATUSINTR_VS_BUTTON_PRESSED     1

typedef struct VUSBVSTATUSINTRVS
{
    VUSBVSTATUSINTR Core;
    uint8_t bEvent;
    /* bValue[n] follows, n - number of buttons. */
} VUSBVSTATUSINTRVS;

/* UVC1.1: 4.2.1.2 Request Error Code Control */
#define VUSBV_INF_SUCCESS           0
#define VUSBV_ERR_NOT_READY         1
#define VUSBV_ERR_WRONG_STATE       2
#define VUSBV_ERR_POWER             3
#define VUSBV_ERR_OUT_OF_RANGE      4
#define VUSBV_ERR_INVALID_UNIT      5
#define VUSBV_ERR_INVALID_CONTROL   6
#define VUSBV_ERR_INVALID_REQUEST   7
#define VUSBV_ERR_UNKNOWN           0xff

/* UVC1.1: 4.3.1.1 Video Probe and Commit Controls */
typedef struct VUSBVVSSTATE
{
    uint16_t bmHint;
    uint8_t  bFormatIndex;
    uint8_t  bFrameIndex;
    uint32_t dwFrameInterval;
    uint16_t wKeyFrameRate;
    uint16_t wPFrameRate;
    uint16_t wCompQuality;
    uint16_t wCompWindowSize;
    uint16_t wDelay;
    uint32_t dwMaxVideoFrameSize;
    uint32_t dwMaxPayloadTransferSize;
    uint32_t dwClockFrequency;
    uint8_t  bmFramingInfo;
    uint8_t  bPreferedVersion;
    uint8_t  bMinVersion;
    uint8_t  bMaxVersion;
} VUSBVVSSTATE;

typedef struct VUSBVVSSTILLSTATE
{
    uint8_t  bFormatIndex;
    uint8_t  bFrameIndex;
    uint8_t  bCompressionIndex;
    uint32_t dwMaxVideoFrameSize;
    uint32_t dwMaxMaxPayloadTransferSize;
} VUSBVVSSTILLSTATE;

/* UVC1.1: 2.4.3.3 Video and Still Image Payload Headers */
typedef struct VUSBVPAYLOADHEADER
{
    uint8_t bHeaderLength;
    uint8_t bmHeaderInfo; /* VUSB_V_PAYLOAD_F_* */
} VUSBVPAYLOADHEADER;

#define VUSB_V_PAYLOAD_F_FID     0x01 /* Frame ID */
#define VUSB_V_PAYLOAD_F_EOF     0x02 /* End of Frame */
#define VUSB_V_PAYLOAD_F_PTS     0x04 /* Presentation Time */
#define VUSB_V_PAYLOAD_F_SCR     0x08 /* Source Clock Reference */
#define VUSB_V_PAYLOAD_F_RES     0x10 /* Reserved */
#define VUSB_V_PAYLOAD_F_STI     0x20 /* Still Image */
#define VUSB_V_PAYLOAD_F_ERR     0x40 /* Error */
#define VUSB_V_PAYLOAD_F_EOH     0x80 /* End of header */

/* UVC1.1: 4.3.1.7 Stream Error Code Control */
#define VUSBV_INF_STREAM_SUCCESS          0
#define VUSBV_ERR_STREAM_PROTECTED        1
#define VUSBV_ERR_STREAM_INPUT_UNDERRUN   2
#define VUSBV_ERR_STREAM_DISCONTINUITY    3
#define VUSBV_ERR_STREAM_OUTPUT_UNDERRUN  4
#define VUSBV_ERR_STREAM_OUTPUT_OVERRUN   5
#define VUSBV_ERR_STREAM_FORMAT_CHANGE    6
#define VUSBV_ERR_STREAM_STILL_CAPTURE    7
#define VUSBV_ERR_STREAM_UNKNOWN          8


/* MJPEG format descriptor. */
typedef struct VUSBVDESCVSMJPEGFMT
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bFormatIndex;
    uint8_t bNumFrameDescriptors;
    uint8_t bmFlags;
    uint8_t bDefaultFrameIndex;
    uint8_t bAspectRatioX;
    uint8_t bAspectRatioY;
    uint8_t bmInterlaceFlags;
    uint8_t bCopyProtect;
} VUSBVDESCVSMJPEGFMT, *PVUSBVDESCVSMJPEGFMT;

/* MJPEG frame descriptor. */
typedef struct VUSBVDESCVSMJPEGFRAME
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bFrameIndex;
    uint8_t  bmCapabilities;
    uint16_t wWidth;
    uint16_t wHeight;
    uint32_t dwMinBitRate;
    uint32_t dwMaxBitRate;
    uint32_t dwMaxVideoFrameBufSize;
    uint32_t dwDefaultFrameInterval;
    uint8_t  bFrameIntervalType;
    /* Interval information depending on bFrameIntervalType. */
} VUSBVDESCVSMJPEGFRAME;
#pragma pack()

#endif /* !VBOX_INCLUDED_SRC_Video_vusbvideo_h */
