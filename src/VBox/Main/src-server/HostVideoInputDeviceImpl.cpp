/* $Id: HostVideoInputDeviceImpl.cpp 113679 2026-03-30 13:39:59Z knut.osmundsen@oracle.com $ */
/** @file
 * Host video capture device implementation.
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

#define LOG_GROUP LOG_GROUP_MAIN_HOSTVIDEOINPUTDEVICE
#include "HostVideoInputDeviceImpl.h"
#include "LoggingNew.h"
#include "VirtualBoxImpl.h"

#include <iprt/err.h>
#include <iprt/ldr.h>
#include <iprt/path.h>

#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
# include <errno.h>
# include <fcntl.h>
# include <unistd.h>
# include <sys/mman.h>
# include <sys/poll.h>
# include <sys/stat.h>

# if defined(RT_OS_SOLARIS)
#  include <stropts.h>
#  include <sys/videodev2.h>
# else
#  include <sys/ioctl.h>
#  include <linux/videodev2.h>
# endif

/* According to V4L2 API spec: 4.1. Video Capture Interface */
# define HWC_V4L2_MAX_DEVICES 64

# if defined RT_OS_LINUX
#  ifndef V4L2_CAP_META_CAPTURE /* Since kernel 4.12.0. */
#   define V4L2_CAP_META_CAPTURE (0x00800000)
#  endif

#  ifndef KERNEL_VERSION
#   define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#  endif

/* In kernel 4.12.0, V4L stack introduced V4L2_CAP_META_CAPTURE marco.
 * Devices which report this capability do not provide actual image data.
 * Therefore, such devices need to be filtered out when discovering all
 * the available V4L devices in system.
 *
 * Original struct @v4l2_capability was extended with field @device_caps
 * in kernel 3.4.0. This field reflects actual capabilities of /dev/videoX
 * and required to skip V4L META devices when iterating over the list.
 *
 * To filter META devices, we only use @device_caps if device reports
 * V4L API version >= 4.12.0 in @version field. According to kernel
 * documentation, starting from kernel 3.1 @version field
 * in struct v4l2_capability should generally match to kernel version.
 *
 * https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/querycap.html.
 */
struct v4l2_capability_3_4_0
{
    uint8_t    driver[16];
    uint8_t    card[32];
    uint8_t    bus_info[32];
    uint32_t   version;
    uint32_t   capabilities;
    uint32_t   device_caps;
    uint32_t   reserved[3];
};

/* This macro represents LINUX_VERSION_CODE macro which corresponds
 * to 4.12.0 kernel. Field @device_caps of struct v4l2_capability_3_4 should
 * only be used if its @version >= VBOX_WITH_V4L2_CAP_META_CAPTURE.
 */
#  define VBOX_WITH_V4L2_CAP_META_CAPTURE KERNEL_VERSION(4,12,0)
# endif
#elif defined(RT_OS_WINDOWS)
# include <iprt/win/windows.h>
# include <iprt/win/dshow.h>
# include <Dbt.h>
# include <iprt/win/ks.h>
#endif

/*
 * HostVideoInputDevice implementation.
 */
DEFINE_EMPTY_CTOR_DTOR(HostVideoInputDevice)

HRESULT HostVideoInputDevice::FinalConstruct()
{
    return BaseFinalConstruct();
}

void HostVideoInputDevice::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

/*
 * Initializes the instance.
 */
HRESULT HostVideoInputDevice::init(const com::Utf8Str &name, const com::Utf8Str &path, const com::Utf8Str &alias)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m.name = name;
    m.path = path;
    m.alias = alias;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/*
 * Uninitializes the instance.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HostVideoInputDevice::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m.name.setNull();
    m.path.setNull();
    m.alias.setNull();
}

static HRESULT hostVideoInputDeviceAdd(HostVideoInputDeviceList *pList,
                                       const com::Utf8Str &name,
                                       const com::Utf8Str &path,
                                       const com::Utf8Str &alias)
{
    ComObjPtr<HostVideoInputDevice> obj;
    HRESULT hrc = obj.createObject();
    if (SUCCEEDED(hrc))
    {
        hrc = obj->init(name, path, alias);
        if (SUCCEEDED(hrc))
            pList->push_back(obj);
    }
    return hrc;
}

static DECLCALLBACK(int) hostWebcamAdd(void *pvUser,
                                       const char *pszName,
                                       const char *pszPath,
                                       const char *pszAlias,
                                       uint64_t *pu64Result)
{
    HostVideoInputDeviceList *pList = (HostVideoInputDeviceList *)pvUser;
    HRESULT hrc = hostVideoInputDeviceAdd(pList, pszName, pszPath, pszAlias);
    if (FAILED(hrc))
    {
        *pu64Result = (uint64_t)hrc;
        return VERR_NOT_SUPPORTED;
    }
    return VINF_SUCCESS;
}

/** @todo These typedefs must be in a header. */
typedef DECLCALLBACKTYPE(int, FNVBOXHOSTWEBCAMADD,(void *pvUser,
                                                   const char *pszName,
                                                   const char *pszPath,
                                                   const char *pszAlias,
                                                   uint64_t *pu64Result));
typedef FNVBOXHOSTWEBCAMADD *PFNVBOXHOSTWEBCAMADD;

/** @todo This needs to go into the respective host specific sub directories. */
#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)

# if defined(RT_OS_LINUX)
static bool hwcLinuxVideoCaptureDevice(struct v4l2_capability_3_4_0 *pCaps)
{
    if (pCaps)
    {
        /* Starting from kernel version 4.12.0, V4L2_CAP_META_CAPTURE was introduced.
         * We filter out devices that report this capability because they do not provide
         * actual image data. The way to exclude such devices is to look into @device_caps
         * field instead of @capabilities. Field @device_caps was introduced in
         * kernel 3.1.0. We detect if we can use @device_caps in runtime in order to
         * support build on older platforms. */
        uint32_t caps = (pCaps->version >= VBOX_WITH_V4L2_CAP_META_CAPTURE) ?
            pCaps->device_caps : pCaps->capabilities;

        return ((caps & V4L2_CAP_VIDEO_CAPTURE) != 0
            && (caps & V4L2_CAP_STREAMING) != 0);
    }

    return false;
}
# endif

static int hwcOpen(const char *pszPath, int *pHandle)
{
    int vrc = VINF_SUCCESS;

    struct stat st;
    int ret = stat(pszPath, &st);
    if (   ret == 0
        && S_ISCHR(st.st_mode))
    {
        /* V4L device must be opened with these flags. */
        int h = open(pszPath, O_RDWR | O_NONBLOCK);
        if (h != -1)
            *pHandle = h;
        else
            vrc = VERR_OPEN_FAILED;
    }
    else
        vrc = VERR_NOT_FOUND;

    return vrc;
}

static void hwcClose(int handle)
{
    if (handle != -1)
        close(handle);
}

# ifdef RT_OS_SOLARIS
static int hwcIoctl(int handle, int fn, void *pv)
# else
static int hwcIoctl(int handle, unsigned long fn, void *pv)
# endif
{
    int ret;
    do
    {
        ret = ioctl(handle, fn, pv);
    } while (ret == -1 && errno == EINTR);

# ifdef RT_OS_SOLARIS
    if (ret == 0)
        return VINF_SUCCESS;
    Assert(ret == -1 && errno > 0);
    return RTErrConvertFromErrno((unsigned)errno);
# else
    return ret == 0 ? VINF_SUCCESS : VERR_FILE_IO_ERROR;
# endif
}

static int hostWebcamList(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd, void *pvUser, uint64_t *pu64WebcamAddResult)
{
    int vrc = VINF_SUCCESS;

    int iDevice = 0;
    while (iDevice < HWC_V4L2_MAX_DEVICES)
    {
        iDevice++;

        char *pszPath = NULL;
        RTStrAPrintf(&pszPath, "/dev/video%d", iDevice - 1); /* First device is /dev/video0 */
        if (pszPath)
        {
            int handle = -1;
            int vrc2 = hwcOpen(pszPath, &handle);
            if (RT_SUCCESS(vrc2))
            {
# if defined(RT_OS_LINUX)
                struct v4l2_capability_3_4_0 caps;
# else
                struct v4l2_capability caps;
# endif
                RT_ZERO(caps);
                vrc2 = hwcIoctl(handle, VIDIOC_QUERYCAP, &caps);
                if (RT_SUCCESS(vrc2))
                {
                    if (
# if defined(RT_OS_LINUX)
                        /* On Linux host make sure that actual device has required
                         * capabilities. Filter out meta devices (V4L2_CAP_META_CAPTURE). */
                        hwcLinuxVideoCaptureDevice(&caps))
# else
                           (caps.capabilities & V4L2_CAP_VIDEO_CAPTURE) != 0
                        && (caps.capabilities & V4L2_CAP_STREAMING) != 0)
# endif
                    {
                        char *pszAlias = NULL;
                        RTStrAPrintf(&pszAlias, ".%d", iDevice);
                        if (pszAlias)
                            vrc = pfnWebcamAdd(pvUser,
                                               (char *)caps.card,
                                               pszPath,
                                               pszAlias,
                                               pu64WebcamAddResult);
                        else
                            vrc = VERR_NO_MEMORY;

                        RTStrFree(pszAlias);
                    }
                }

                hwcClose(handle);
            }

            RTStrFree(pszPath);
        }
        else
        {
            vrc = VERR_NO_MEMORY;
        }

        if (RT_FAILURE(vrc))
            break;
    }

    return vrc;
}

#elif defined(RT_OS_WINDOWS)
/*
 * Device emumeration entry point.
 */

static HRESULT hwcCreateEnumerator(IEnumMoniker **ppEnumMoniker)
{
    ICreateDevEnum *pCreateDevEnum = NULL;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pCreateDevEnum));
    if (SUCCEEDED(hr))
    {
        hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, ppEnumMoniker, 0);
        pCreateDevEnum->Release();
    }
    return hr;
}

static HRESULT hwcFillList(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd,
                           void *pvUser,
                           uint64_t *pu64WebcamAddResult,
                           IEnumMoniker *pEnumMoniker)
{
    int iDevice = 0; /* Count devices 1..N. */
    IMoniker *pMoniker = NULL;
    while (pEnumMoniker->Next(1, &pMoniker, NULL) == S_OK)
    {
        /* Device index must be the same as in hwcFindDevice */
        ++iDevice;

        IPropertyBag *pPropBag = NULL;
        _Pragma("warning(push)");_Pragma("warning(disable:6387)"); /* MSC /analyze: param(1) shouldn't be NULL. incorrect annotation? */
        HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
        _Pragma("warning(pop)");
        if (FAILED(hr))
        {
            pMoniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);

        hr = pPropBag->Read(L"DevicePath", &var, 0);
        if (FAILED(hr))
        {
            /* Can't use a device without path. */
            pMoniker->Release();
            continue;
        }

        char *pszPath = NULL;
        int vrc = RTUtf16ToUtf8((PRTUTF16)var.bstrVal, &pszPath);
        if (RT_FAILURE(vrc))
            pszPath = NULL;

        VariantClear(&var);

        hr = pPropBag->Read(L"FriendlyName", &var, 0);
        if (FAILED(hr))
        {
            hr = pPropBag->Read(L"Description", &var, 0);
        }

        char *pszName = NULL;
        if (SUCCEEDED(hr))
        {
            vrc = RTUtf16ToUtf8((PRTUTF16)var.bstrVal, &pszName);
            if (RT_FAILURE(vrc))
                pszName = NULL;
            VariantClear(&var);
        }
        else
        {
            RTStrAPrintf(&pszName, "Video Input Device #%d", iDevice);
        }

        char *pszAlias = NULL;
        RTStrAPrintf(&pszAlias, ".%d", iDevice);

        if (pszName && pszPath && pszAlias)
        {
            vrc = pfnWebcamAdd(pvUser, pszName, pszPath, pszAlias, pu64WebcamAddResult);
            if (RT_FAILURE(vrc))
                hr = E_FAIL;
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }

        RTStrFree(pszPath);
        RTStrFree(pszName);
        RTStrFree(pszAlias);

        pPropBag->Release();
        pMoniker->Release();

        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

static HRESULT hwcDeviceList(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd,
                             void *pvUser,
                             uint64_t *pu64WebcamAddResult)
{
    IEnumMoniker *pEnumMoniker = NULL;
    HRESULT hr = hwcCreateEnumerator(&pEnumMoniker);
    if (SUCCEEDED(hr))
    {
        if (hr != S_FALSE)
        {
            /* List not empty */
            hr = hwcFillList(pfnWebcamAdd, pvUser, pu64WebcamAddResult, pEnumMoniker);
            pEnumMoniker->Release();
        }
        else
            hr = S_OK; /* Return empty list. */
    }
    return hr;
}

static int hostWebcamList(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd, void *pvUser, uint64_t *pu64WebcamAddResult)
{
    HRESULT hr = hwcDeviceList(pfnWebcamAdd, pvUser, pu64WebcamAddResult);
    if (FAILED(hr))
        return VERR_NOT_SUPPORTED;
    return VINF_SUCCESS;
}

#elif defined(RT_OS_DARWIN)
DECLHIDDEN(int) DarwinHostWebcamList(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd, void *pvUser, uint64_t *pu64WebcamAddResult);
# define hostWebcamList DarwinHostWebcamList

#else
/** @todo The other hosts. */
static int hostWebcamList(PFNVBOXHOSTWEBCAMADD pfnWebcamAdd, void *pvUser, uint64_t *pu64WebcamAddResult)
{
    RT_NOREF(pfnWebcamAdd, pvUser);
    *pu64WebcamAddResult = (uint64_t)E_FAIL;
    return VERR_NOT_SUPPORTED;
}
#endif

static HRESULT fillDeviceList(VirtualBox *pVirtualBox, HostVideoInputDeviceList *pList)
{
    uint64_t u64Result = S_OK;
    HRESULT hrc = S_OK;
    int vrc = hostWebcamList(hostWebcamAdd, pList, &u64Result);
    Log(("VBoxHostWebcamList vrc %Rrc, result 0x%08RX64\n", vrc, u64Result));
    if (RT_FAILURE(vrc))
        hrc = (HRESULT)u64Result;

    if (SUCCEEDED(hrc))
    {
        if (RT_FAILURE(vrc))
            hrc = pVirtualBox->setErrorBoth(VBOX_E_IPRT_ERROR, vrc,
                                            HostVideoInputDevice::tr("Failed to get webcam list: %Rrc"), vrc);
    }

    return hrc;
}

/* static */ HRESULT HostVideoInputDevice::queryHostDevices(VirtualBox *pVirtualBox, HostVideoInputDeviceList *pList)
{
    HRESULT hrc = fillDeviceList(pVirtualBox, pList);
    if (FAILED(hrc))
        pList->clear();
    return hrc;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
