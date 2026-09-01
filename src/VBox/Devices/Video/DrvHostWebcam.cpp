/* $Id: DrvHostWebcam.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * DrvHostWebcam - the host webcam as the emulated USB webcam backend.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "DrvHostWebcam.h"

#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <VBox/err.h>
#include <VBox/version.h>


static void hwcSetDeviceIndex(PDRVHOSTWEBCAM pThis)
{
    const char *s = pThis->pszDevicePath;
    if (   s[0] == 0
        || (s[0] == '.' && s[1] == 0))
    {
        s = ".0";
    }

    if (s[0] == '.')
    {
        uint16_t u16 = 0;
        int rc = RTStrToUInt16Full(&s[1], 10, &u16);
        if (rc == VINF_SUCCESS)
        {
            pThis->i32DeviceIndex = u16;
        }
        else
        {
            /* Consider this a host path. */
            pThis->i32DeviceIndex = -1;
        }
    }
    else
    {
        /* This is a host path, apparently. */
        pThis->i32DeviceIndex = -1;
    }
}

int hwcReportAttached(PDRVHOSTWEBCAM pThis)
{
    VRDEVIDEOINDEVICEDESC *pDeviceDesc = NULL;
    uint32_t cbDeviceDesc = 0;

    int rc = hostWebcamQueryDeviceDesc(pThis, &pDeviceDesc, &cbDeviceDesc);

    if (RT_SUCCESS(rc))
    {
        uint64_t u64DeviceId = 1;
        uint32_t u32Version = VRDE_VIDEOIN_NEGOTIATE_VERSION;
        uint32_t fu32Capabilities = VRDE_VIDEOIN_NEGOTIATE_CAP_VOID;

        if (pThis->pIWebcamUp)
            pThis->pIWebcamUp->pfnAttached(pThis->pIWebcamUp,
                                           u64DeviceId,
                                           pDeviceDesc,
                                           cbDeviceDesc,
                                           u32Version,
                                           fu32Capabilities);

        RTMemFree(pDeviceDesc);
    }

    return rc;
}

/*
 * PDM driver.
 */
static DECLCALLBACK(void) drvWebcamReady(PPDMIWEBCAMDRV pInterface,
                                         bool fReady)
{
    PDRVHOSTWEBCAM pThis = RT_FROM_MEMBER(pInterface, DRVHOSTWEBCAM, IWebcamDrv);

    UWLOGF(("fReady %d\n", fReady));

    if (fReady)
    {
        hwcReportAttached(pThis);
    }
}

static DECLCALLBACK(int) drvWebcamControl(PPDMIWEBCAMDRV pInterface,
                                          void *pvUser,
                                          uint64_t u64DeviceId,
                                          struct VRDEVIDEOINCTRLHDR const *pCtrl,
                                          uint32_t cbCtrl)
{
    PDRVHOSTWEBCAM pThis = RT_FROM_MEMBER(pInterface, DRVHOSTWEBCAM, IWebcamDrv);
    UWLOGF(("u64DeviceId %lld, sel %RX16, type %RX16, cb %d, flags %RX8 status %RX8\n",
            u64DeviceId,
            pCtrl->u16ControlSelector,
            pCtrl->u16RequestType,
            pCtrl->u16ParmSize,
            pCtrl->u8Flags,
            pCtrl->u8Status
           ));

    return hostWebcamControl(pThis, pvUser, u64DeviceId, pCtrl, cbCtrl);
}

static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface,
                                              const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTWEBCAM pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTWEBCAM);

    UWLOGF(("pszIID:%s\n", pszIID));

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIWEBCAMDRV, &pThis->IWebcamDrv);
    return NULL;
}

static DECLCALLBACK(void) drvHostWebcamDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVHOSTWEBCAM pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTWEBCAM);
    UWLOGF(("iInstance %d, pIWebcamUp %p\n", pDrvIns->iInstance, pThis->pIWebcamUp));

    if (pThis->pszDevicePath)
    {
        PDMDrvHlpMMHeapFree(pDrvIns, pThis->pszDevicePath);
        pThis->pszDevicePath = NULL;
    }

    hostWebcamDestruct(pDrvIns, pThis);
}


static DECLCALLBACK(int) drvHostWebcamConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    UWLOGF(("iInstance %d, pCfg %p, fFlags 0x%x\n", pDrvIns->iInstance, pCfg, fFlags));

    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    PDRVHOSTWEBCAM pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTWEBCAM);
    PCPDMDRVHLPR3 pHlp = pDrvIns->pHlpR3;

    pThis->pDrvIns = pDrvIns;

    /* Check early that there is a device. No need to init anything if there is no device. */
    pThis->pIWebcamUp = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIWEBCAMDEV);
    if (pThis->pIWebcamUp == NULL)
    {
        UWLOG(("HOSTWEBCAM: Emulated webcam device does not exist.\n"));
        return VERR_PDM_MISSING_INTERFACE;
    }

    /* The address/path/description of the host video device to use. */
    int rc = pHlp->pfnCFGMQueryStringAllocDef(pCfg, "DevicePath", &pThis->pszDevicePath, ".0");
    if (RT_FAILURE(rc))
    {
        UWLOG(("HOSTWEBCAM: query device path %Rrc.\n", rc));
        return rc;
    }

    hwcSetDeviceIndex(pThis);

    UWLOG(("HOSTWEBCAM: index %d, path [%s]\n", pThis->i32DeviceIndex, pThis->pszDevicePath));

    /* Everything ok. Initialize. */
    pDrvIns->IBase.pfnQueryInterface = drvQueryInterface;

    pThis->IWebcamDrv.pfnReady   = drvWebcamReady;
    pThis->IWebcamDrv.pfnControl = drvWebcamControl;

    return hostWebcamConstruct(pDrvIns, pThis, pCfg);
}

extern "C" const PDMDRVREG g_DrvHostWebcam =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName[32] */
    "HostWebcam",
    /* szRCMod[32] */
    "",
    /* szR0Mod[32] */
    "",
    /* pszDescription */
    "PDM driver for host webcam.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass */
    PDM_DRVREG_CLASS_USB,
    /* cMaxInstances */
    16,
    /* cbInstance */
    sizeof(DRVHOSTWEBCAM),
    /* pfnConstruct */
    drvHostWebcamConstruct,
    /* pfnDestruct */
    drvHostWebcamDestruct,
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

