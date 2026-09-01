/* $Id: HostGetWebcamDarwin.mm 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * Main - Return accessible webcams, Darwin implementation.
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
#define LOG_GROUP LOG_GROUP_MAIN_HOST

#import <AVFoundation/AVFoundation.h>

#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/string.h>

/** @todo These typedefs must be in a header. */
typedef DECLCALLBACKTYPE(int, FNVBOXHOSTWEBCAMADD,(void *pvUser,
                                                   const char *pszName,
                                                   const char *pszPath,
                                                   const char *pszAlias,
                                                   uint64_t *pu64Result));
typedef FNVBOXHOSTWEBCAMADD *PFNVBOXHOSTWEBCAMADD;


DECLHIDDEN(int) DarwinHostWebcamList(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd, void *pvUser, uint64_t *pu64WebcamAddResult)
{
    int vrc = VINF_SUCCESS;

    NSAutoreleasePool* localpool = [[NSAutoreleasePool alloc] init];

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 101500
    RT_GCC_NO_WARN_DEPRECATED_BEGIN /* AVCaptureDeviceTypeExternalUnknown is deprecated since 15.0, was renamed to AVCaptureDeviceTypeExternal. */
    AVCaptureDeviceDiscoverySession *DiscoverySession = [AVCaptureDeviceDiscoverySession
                                                         discoverySessionWithDeviceTypes:@[ /* only types available on macOS: */
                                                             AVCaptureDeviceTypeBuiltInWideAngleCamera,
                                                             AVCaptureDeviceTypeExternalUnknown]
                                                         mediaType:AVMediaTypeVideo
                                                         position:AVCaptureDevicePositionUnspecified];
    RT_GCC_NO_WARN_DEPRECATED_END
    NSArray<AVCaptureDevice *> *devices = DiscoverySession.devices;
#else /* < 10.15 */
    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#endif

    int iDevice = 0;
    for (AVCaptureDevice *device in devices)
    {
        iDevice++;

        /* Skip unconnected devices. */
        if (device.connected == NO)
            continue;

        char *pszAlias = NULL;
        RTStrAPrintf(&pszAlias, ".%d", iDevice);

        if (pszAlias)
        {
            vrc = pfnWebcamAdd(pvUser,
                               device.localizedName.UTF8String,
                               device.uniqueID.UTF8String,
                               pszAlias,
                               pu64WebcamAddResult);
        }
        else
            vrc = VERR_NO_MEMORY;

        RTStrFree(pszAlias);

        if (RT_FAILURE(vrc))
            break;
    }

    [localpool drain];
    return vrc;
}
