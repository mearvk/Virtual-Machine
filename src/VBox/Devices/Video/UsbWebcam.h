/* $Id: UsbWebcam.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * UsbWebcam.h - emulated USB webcam common defines.
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

#ifndef VBOX_INCLUDED_SRC_Video_UsbWebcam_h
#define VBOX_INCLUDED_SRC_Video_UsbWebcam_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/log.h>
#include <VBox/RemoteDesktop/VRDEVideoIn.h>

#include "vusbvideo.h"


/* For important logging. When UsbWebcam is idle nothing should go to the log file. */
#define UWLOG(a) do { LogRel2((LOG_FN_FMT ": ", __FUNCTION__)); LogRel2(a); } while(0)
#define UWLOG_NO_FN(a) do { LogRel2(a); } while(0)
#define UWLOGENABLED() LogRelIs2Enabled()
/* For repeated logging, like URB submit. Interrupt endpoint, etc. */
#define UWLOGF(a) do { LogRel3((LOG_FN_FMT ": ", __FUNCTION__)); LogRel3(a); } while(0)
#define UWLOGF_NO_FN(a) do { LogRel3(a); } while(0)
#define UWLOGFENABLED() LogRelIs3Enabled()

#define UWLOGFLEAVERC(rc) UWLOGF(("LEAVE: %Rrc\n", (rc)))


#define USBWEBCAM_INTERFACE_CONTROL   0
#define USBWEBCAM_INTERFACE_STREAMING 1
#define USBWEBCAM_NUM_INTERFACES      2


int usbWebcamDescriptorsBuild(PPDMUSBDESCCACHE *ppDescCache,
                              int iInstance,
                              const VRDEVIDEOINDEVICEDESC *pDeviceDesc,
                              uint32_t cbDeviceDesc,
                              uint32_t u32MinFrameInterval);
void usbWebcamDescriptorsFree(PPDMUSBDESCCACHE pDesc);

const VUSBVDESCVSMJPEGFRAME *usbWebcamDescriptorsFindFrame(const PPDMUSBDESCCACHE pDescCache,
                                                           uint8_t bFrameIndex);
const VUSBVDESCVSMJPEGFRAME *usbWebcamDescriptorsSuggestFrame(const PPDMUSBDESCCACHE pDescCache,
                                                              uint16_t u16Width, uint16_t u16Height);
uint8_t usbWebcamDescriptorsFrameCount(const PPDMUSBDESCCACHE pDescCache);

#endif /* !VBOX_INCLUDED_SRC_Video_UsbWebcam_h */
