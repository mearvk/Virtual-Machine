/* $Id: DrvHostWebcam.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * DrvHostWebcam - the PDM driver for host webcam.
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

#ifndef VBOX_INCLUDED_SRC_Video_DrvHostWebcam_h
#define VBOX_INCLUDED_SRC_Video_DrvHostWebcam_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define LOG_GROUP LOG_GROUP_USB_WEBCAM
#include <VBox/log.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmwebcaminfs.h>
#include <VBox/RemoteDesktop/VRDEVideoIn.h>


/* For important logging. When UsbWebcam is idle nothing should go to the log file. */
#define UWLOG(a) do { LogRel2((LOG_FN_FMT ": ", __FUNCTION__)); LogRel2(a); } while(0)
#define UWLOG_NO_FN(a) do { LogRel2(a); } while(0)
#define UWLOGENABLED() LogRelIs2Enabled()
/* For repeated logging, like URB submit. Interrupt endpoint, etc. */
#define UWLOGF(a) do { LogRel3((LOG_FN_FMT ": ", __FUNCTION__)); LogRel3(a); } while(0)
#define UWLOGF_NO_FN(a) do { LogRel3(a); } while(0)
#define UWLOGFENABLED() LogRelIs3Enabled()

#define UWLOGFLEAVERC(rc) UWLOGF(("LEAVE: %Rrc\n", (rc)))


/* Each host backend has its own platform specific structure. */
typedef struct HOSTWEBCAM HOSTWEBCAM;


typedef struct DRVHOSTWEBCAM
{
    /*
     * Common data for all platforms.
     */
    PDMIWEBCAMDRV  IWebcamDrv;
    PPDMIWEBCAMDEV pIWebcamUp;

    PPDMDRVINS pDrvIns;

    char *pszDevicePath; /* The host device to use. ".0" means default device,
                          * ".1", ".2", etc mean the first, second, etc device
                          * in a host list of video capture devices.
                          * Any other value must be interpreted by the host backend.
                          * ".{uuid}VVVVPPPP:address" string is used to find a webcam by
                          * the USB device description: 'uuid' assigned by VBox USB,
                          * 'VVVV' is the Vendor ID, 'PPPP' is the ProductID,
                          * 'address' is the host specific USB device identifier.
                          */
    int32_t i32DeviceIndex; /* -1: pszDevicePath must be interpreted by the host backend.
                             *  0: default system video capture device must be used.
                             * >0: position of the system video capture device in a list.
                             * Host backend is allowed to change this field.
                             */

    /*
     * Platform specific data.
     */
    HOSTWEBCAM *pHostWebcam;
} DRVHOSTWEBCAM, *PDRVHOSTWEBCAM;


int hostWebcamControl(PDRVHOSTWEBCAM pThis,
                      void *pvUser,
                      uint64_t u64DeviceId,
                      const VRDEVIDEOINCTRLHDR *pHdr,
                      uint32_t cbCtrl);
int hostWebcamQueryDeviceDesc(PDRVHOSTWEBCAM pThis,
                              VRDEVIDEOINDEVICEDESC **ppDeviceDesc,
                              uint32_t *pcbDeviceDesc);
int hostWebcamConstruct(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis, PCFGMNODE pCfg);
void hostWebcamDestruct(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis);

int hwcReportAttached(PDRVHOSTWEBCAM pThis);

/** @todo These typedefs must be accessible in HostVideoInputDeviceImpl.cpp. */
typedef DECLCALLBACKTYPE(int, FNVBOXHOSTWEBCAMADD,(void *pvUser,
                                                   const char *pszName,
                                                   const char *pszPath,
                                                   const char *pszAlias,
                                                   uint64_t *pu64Result));
typedef FNVBOXHOSTWEBCAMADD *PFNVBOXHOSTWEBCAMADD;

typedef DECLCALLBACKTYPE(int, FNVBOXHOSTWEBCAMLIST,(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd,
                                                    void *pvUser,
                                                    uint64_t *pu64WebcamAddResult));
typedef FNVBOXHOSTWEBCAMLIST *PFNVBOXHOSTWEBCAMLIST;

extern "C" DECLEXPORT(int) VBoxHostWebcamList(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd,
                                              void *pvUser,
                                              uint64_t *pu64WebcamAddResult);

#endif /* !VBOX_INCLUDED_SRC_Video_DrvHostWebcam_h */
