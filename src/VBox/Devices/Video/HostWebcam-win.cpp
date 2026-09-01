/* $Id: HostWebcam-win.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * DrvHostWebcam - Windows host webcam backend.
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
#include "JPEGEnc.h"

#include <iprt/win/windows.h>
#include <iprt/win/dshow.h>
#include <Dbt.h>
#include <iprt/win/ks.h>

#include <iprt/crc.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/utf16.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define HWC_REFCNT_LOG(p) do { if (p) {            \
    ULONG c = (p)->AddRef();                       \
    UWLOG(("refcnt: %s %p %u\n", #p, (p), c - 1)); \
    (p)->Release();                                \
} } while (0)

#define HWC_RELEASE(p) do { if (p) { HWC_REFCNT_LOG(p); (p)->Release(); (p) = NULL; } } while (0)

#define HR_FAILED_BREAK(hr) if (FAILED(hr)) { UWLOG(("Failed @%d\n", __LINE__)); break; }


/*
 * COM interfaces which are not in the SDK. MSDN says they are deprecated.
 * Use them because it is convenient and they still work in Windows 8.
 */

MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85")
ISampleGrabberCB : public IUnknown
{
    virtual STDMETHODIMP SampleCB(double SampleTime, IMediaSample *pSample) = 0;
    virtual STDMETHODIMP BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen) = 0;
};

#if _MSC_VER < 1100 /* new compilers uses the IID from the MIDL_INTERFACE above. */
static const IID IID_ISampleGrabberCB =
    { 0x0579154A, 0x2B53, 0x4994, { 0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85 } };
#endif

MIDL_INTERFACE("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown
{
    virtual STDMETHODIMP SetOneShot(BOOL OneShot) = 0;
    virtual STDMETHODIMP SetMediaType(const AM_MEDIA_TYPE *pType) = 0;
    virtual STDMETHODIMP GetConnectedMediaType(AM_MEDIA_TYPE *pType) = 0;
    virtual STDMETHODIMP SetBufferSamples(BOOL BufferThem) = 0;
    virtual STDMETHODIMP GetCurrentBuffer(long *pBufferSize, long *pBuffer) = 0;
    virtual STDMETHODIMP GetCurrentSample(IMediaSample **ppSample) = 0;
    virtual STDMETHODIMP SetCallback(ISampleGrabberCB *pCallback, long WhichMethodToCallback) = 0;
};

#if _MSC_VER < 1100 /* new compilers uses the IID from the MIDL_INTERFACE above. */
static const IID IID_ISampleGrabber =
    { 0x6B652FFF, 0x11FE, 0x4fce, { 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F } };
#endif

static const CLSID CLSID_SampleGrabber =
    { 0xC1F400A0, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

static const CLSID CLSID_NullRenderer =
    { 0xC1F400A4, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

static const GUID MEDIASUBTYPE_I420 =
    { 0x30323449, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };


class SampleGrabberCallback;

/* The host specific data. */
struct HOSTWEBCAM
{
    VRDEVIDEOINCTRL_VS_SETUP lastSetup;

    BSTR bstrDevicePath;
    char *pszDeviceName;
    char *pszSerial;

    IGraphBuilder  *pGraphBuilder;
    IMediaControl  *pControl;
    IBaseFilter    *pGrabberFilter;
    ISampleGrabber *pGrabber;
    IBaseFilter    *pNullFilter;
    IBaseFilter    *pDeviceFilter;
    IPin           *pCapturePin;

    IBaseFilter    *pAviDecFilter;
    IBaseFilter    *pColourFilter;

    SampleGrabberCallback *pSampleCallback;

    GUID guidFormat; /* Current webcam format used for streaming. */

    struct {
        PPDMTHREAD pThreadMonitor;
        HWND hwnd;
    } monitor;
};

static void freeMediaType(AM_MEDIA_TYPE *pMT)
{
    if (pMT != NULL)
    {
        if (pMT->cbFormat != 0)
        {
            CoTaskMemFree(pMT->pbFormat);
            pMT->cbFormat = 0;
            pMT->pbFormat = NULL;
        }
        if (pMT->pUnk != NULL)
        {
            pMT->pUnk->Release();
            pMT->pUnk = NULL;
        }

        CoTaskMemFree(pMT);
    }
}

static void dumpMediaType(AM_MEDIA_TYPE *pMT)
{
    UWLOG(("MT: majortype %RTuuid\n"
           "    subtype %RTuuid\n"
           "    bFixedSizeSamples %d\n"
           "    bTemporalCompression %d\n"
           "    lSampleSize %d\n"
           "    formattype %RTuuid\n"
           "    pUnk %p\n"
           "    cbFormat %d\n"
           "    pbFormat %p\n"
           ,
           &pMT->majortype,
           &pMT->subtype,
           pMT->bFixedSizeSamples,
           pMT->bTemporalCompression,
           pMT->lSampleSize,
           &pMT->formattype,
           pMT->pUnk,
           pMT->cbFormat,
           pMT->pbFormat
          ));

    if (pMT->formattype == FORMAT_VideoInfo)
    {
        VIDEOINFOHEADER *pVIH = (VIDEOINFOHEADER *)pMT->pbFormat;
        UWLOG(("VIDEOINFOHEADER:\n"
               "    rcSource %d-%d %d-%d\n"
               "    rcTarget %d-%d %d-%d\n"
               "    dwBitRate %d, dwBitErrorRate %d\n"
               "    AvgTimePerFrame %lld\n"
               "    bmiHeader cb %d, %dx%d, planes %d, bits %d, c 0x%RX32(%c%c%c%c), size %d, pels %dx%d clr %d/%d\n",
               pVIH->rcSource.left, pVIH->rcSource.right, pVIH->rcSource.top, pVIH->rcSource.bottom,
               pVIH->rcTarget.left, pVIH->rcTarget.right, pVIH->rcTarget.top, pVIH->rcTarget.bottom,
               pVIH->dwBitRate, pVIH->dwBitErrorRate,
               pVIH->AvgTimePerFrame,
               pVIH->bmiHeader.biSize,
               pVIH->bmiHeader.biWidth,
               pVIH->bmiHeader.biHeight,
               pVIH->bmiHeader.biPlanes,
               pVIH->bmiHeader.biBitCount,
               pVIH->bmiHeader.biCompression,
               RT_BYTE1(pVIH->bmiHeader.biCompression), RT_BYTE2(pVIH->bmiHeader.biCompression),
               RT_BYTE3(pVIH->bmiHeader.biCompression), RT_BYTE4(pVIH->bmiHeader.biCompression),
               pVIH->bmiHeader.biSizeImage,
               pVIH->bmiHeader.biXPelsPerMeter,
               pVIH->bmiHeader.biYPelsPerMeter,
               pVIH->bmiHeader.biClrUsed,
               pVIH->bmiHeader.biClrImportant
             ));
    }
}

typedef struct HWC_MEDIA_TYPE
{
    AM_MEDIA_TYPE mt;
    VIDEOINFOHEADER vih;
} HWC_MEDIA_TYPE;

static HRESULT setStreamFormat(IPin *pPin, int cWidth, int cHeight, uint32_t u32FrameInterval,
                               const GUID *pPreferredFormat, GUID *pSelectedFormat)
{
    UWLOG(("%ux%u interval %u\n", cWidth, cHeight, u32FrameInterval));

    IAMStreamConfig *pStreamConfig = NULL;
    HRESULT hr = pPin->QueryInterface(&pStreamConfig);
    if (SUCCEEDED(hr))
    {
        AM_MEDIA_TYPE *pMT = NULL;
        hr = pStreamConfig->GetFormat(&pMT);
        if (SUCCEEDED(hr))
        {
            if (pMT->formattype == FORMAT_VideoInfo)
            {
                if (UWLOGENABLED())
                {
                    dumpMediaType(pMT);
                }

                /* Make a copy which will be used for SetFormat calls. */
                HWC_MEDIA_TYPE mt;
                mt.mt = *pMT;
                mt.mt.pbFormat = (BYTE *)&mt.vih;
                mt.mt.cbFormat = sizeof(VIDEOINFOHEADER);
                mt.vih = *(VIDEOINFOHEADER *)pMT->pbFormat;

                VIDEOINFOHEADER *pVIH = &mt.vih;

                if (cWidth)
                {
                    pVIH->bmiHeader.biWidth = cWidth;
                }
                if (cHeight)
                {
                    pVIH->bmiHeader.biHeight = cHeight;
                }
                if (u32FrameInterval)
                {
                    pVIH->AvgTimePerFrame = (REFERENCE_TIME)u32FrameInterval;
                }

                mt.mt.lSampleSize = cWidth * cHeight * ((pVIH->bmiHeader.biBitCount + 7) / 8);
                int cFPS = pVIH->AvgTimePerFrame? (int)((REFERENCE_TIME)10000000 / pVIH->AvgTimePerFrame): 0;
                pVIH->dwBitRate = mt.mt.lSampleSize * 8 * cFPS;
                pVIH->bmiHeader.biSizeImage = mt.mt.lSampleSize;

                /* Try to set MJPEG format. */
                mt.mt.subtype = *pPreferredFormat;

                hr = pStreamConfig->SetFormat(&mt.mt);
                UWLOG(("SetFormat MJPEG 0x%08X\n", hr));

                if (FAILED(hr))
                {
                    /* If webcam format is not one of supported, then try to set a supported format. */
                    BOOL fSupportedFormat = FALSE;
                    if (   pMT->subtype == MEDIASUBTYPE_YUY2
                        || pMT->subtype == MEDIASUBTYPE_I420
                        || pMT->subtype == MEDIASUBTYPE_IYUV)
                    {
                        fSupportedFormat = TRUE;
                    }

                    if (fSupportedFormat)
                    {
                        /* Set the supported webcam format. */
                        mt.mt.subtype = pMT->subtype;

                        hr = pStreamConfig->SetFormat(&mt.mt);
                        UWLOG(("SetFormat supported 0x%08X\n", hr));
                    }
                    else
                    {
                        /* Try to set one of supported formats. */

                        /* Retry with YUY2 format. */
                        mt.mt.subtype = MEDIASUBTYPE_YUY2;

                        hr = pStreamConfig->SetFormat(&mt.mt);
                        UWLOG(("SetFormat YUY2 0x%08X\n", hr));

                        if (FAILED(hr))
                        {
                            /* Retry with I420 format. */
                            mt.mt.subtype = MEDIASUBTYPE_I420;

                            hr = pStreamConfig->SetFormat(&mt.mt);
                            UWLOG(("SetFormat I420 0x%08X\n", hr));
                        }
                    }

                    if (FAILED(hr))
                    {
                        /* Retry with webcam format. */
                        mt.mt.subtype = pMT->subtype;

                        hr = pStreamConfig->SetFormat(&mt.mt);
                        UWLOG(("SetFormat webcam 0x%08X (fallback to RGB32)\n", hr));

                        /* At least try to get uncompressed in SampleGrabber. */
                        mt.mt.subtype = MEDIASUBTYPE_RGB32;
                    }
                }

                if (SUCCEEDED(hr))
                {
                    *pSelectedFormat = mt.mt.subtype;
                }
            }
            else
            {
                UWLOG(("SetFormat not video\n"));
                hr = E_FAIL;
            }

            freeMediaType(pMT);
        }

        HWC_RELEASE(pStreamConfig);
    }

    return hr;
}

static HRESULT hwcGetCaptureDevice(BSTR devicePath, IBaseFilter **ppDevice)
{
    UWLOG(("devicePath [%ls]\n", devicePath));

    IBaseFilter *pDevice = NULL;

    ICreateDevEnum *pDevEnum = NULL;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC,
                                  IID_ICreateDevEnum, (void **)&pDevEnum);
    if (SUCCEEDED(hr))
    {
        IEnumMoniker *pEnum = NULL;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (hr == S_FALSE)
        {
            hr = VFW_E_NOT_FOUND; /* No devices. */
        }
        HWC_RELEASE(pDevEnum);

        if (SUCCEEDED(hr))
        {
            IMoniker *pMoniker = NULL;
            while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
            {
                IPropertyBag *pPropBag = NULL;
                hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
                if (SUCCEEDED(hr))
                {
                    VARIANT var;
                    VariantInit(&var);

                    hr = pPropBag->Read(L"DevicePath", &var, 0);
                    if (SUCCEEDED(hr))
                    {
                        /* Check if that is the device. */
                        if (RTUtf16ICmp(devicePath, var.bstrVal) == 0)
                        {
                            hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pDevice);
                            if (SUCCEEDED(hr))
                            {
                                *ppDevice = pDevice;
                            }
                        }

                        VariantClear(&var);
                    }
                    HWC_RELEASE(pPropBag);
                }
                HWC_RELEASE(pMoniker);

                if (pDevice)
                {
                    break;
                }
            }
            HWC_RELEASE(pEnum);
        }
    }

    if (SUCCEEDED(hr) && pDevice == NULL)
    {
        hr = VFW_E_NOT_FOUND; /* No device. */
    }

    UWLOG(("hr 0x%08X\n", hr));
    return hr;
}

static HRESULT hwcCheckCapturePin(IPin* pPin)
{
    IKsPropertySet *pKSPropertySet = NULL;
    HRESULT hr = pPin->QueryInterface(IID_PPV_ARGS(&pKSPropertySet));
    if (SUCCEEDED(hr))
    {
        DWORD cbReturned = 0;
        GUID pinCategory;
        RT_ZERO(pinCategory);
        hr = pKSPropertySet->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0,
                                 &pinCategory, sizeof(GUID),
                                 &cbReturned);
        if (SUCCEEDED(hr))
        {
            if (cbReturned == sizeof(GUID))
            {
                hr = pinCategory == PIN_CATEGORY_CAPTURE? S_OK: S_FALSE;
            }
            else
            {
                hr = E_FAIL;
            }
        }
        HWC_RELEASE(pKSPropertySet);
    }
    return hr;
}

static HRESULT hwcGetPin(IBaseFilter *pFilter, PIN_DIRECTION requiredDirection, IPin **ppPin)
{
    BOOL fFound = FALSE;

    IEnumPins *pEnumPins = NULL;
    HRESULT hr = pFilter->EnumPins(&pEnumPins);
    if (SUCCEEDED(hr))
    {
        /* Find an unconnected OUTPUT pin. */
        IPin *pPin = NULL;
        while (pEnumPins->Next(1, &pPin, NULL) == S_OK)
        {
            IPin *pConnectedTo = NULL;
            hr = pPin->ConnectedTo(&pConnectedTo);
            if (SUCCEEDED(hr))
            {
                /* Pin already connected, skip it. */
                HWC_RELEASE(pConnectedTo);
            }
            else if (hr == VFW_E_NOT_CONNECTED)
            {
                /* Unconnected pin. Check its direction. */
                PIN_DIRECTION pinDirection;
                hr = pPin->QueryDirection(&pinDirection);
                if (SUCCEEDED(hr))
                {
                    if (pinDirection == requiredDirection)
                    {
                        /* In case of an error still try to use this pin. */
                        if (hwcCheckCapturePin(pPin) != S_FALSE)
                        {
                            /* Found it. */
                            fFound = TRUE;
                            *ppPin = pPin;
                            break;
                        }
                    }
                }
            }

            HWC_RELEASE(pPin);
        }

        HWC_RELEASE(pEnumPins);
    }

    UWLOG(("hr 0x%08X fFound %d\n", hr, fFound));
    return fFound? S_OK: VFW_E_NOT_FOUND;
}

static HRESULT hwcConnectPin(IGraphBuilder *pGraphBuilder, IPin *pOutputPin, IBaseFilter *pDst)
{
    IPin *pInputPin = NULL;
    HRESULT hr = hwcGetPin(pDst, PINDIR_INPUT, &pInputPin);
    if (SUCCEEDED(hr))
    {
        hr = pGraphBuilder->Connect(pOutputPin, pInputPin);
        HWC_RELEASE(pInputPin);
    }
    return hr;
}

static HRESULT hwcConnectFilters(IGraphBuilder *pGraphBuilder, IBaseFilter *pSrc, IBaseFilter *pDst)
{
    IPin *pOutputPin = NULL;
    HRESULT hr = hwcGetPin(pSrc, PINDIR_OUTPUT, &pOutputPin);
    if (SUCCEEDED(hr))
    {
        hr = hwcConnectPin(pGraphBuilder, pOutputPin, pDst);
        HWC_RELEASE(pOutputPin);
    }
    return hr;
}


class SampleGrabberCallback: public ISampleGrabberCB
{
    private:
        PDRVHOSTWEBCAM mpThis;
        LONG refcnt;
        HWCJPEG *mpEnc;

        enum HWC_MEDIASUBTYPE
        {
            HWC_MEDIASUBTYPE_MJPG = 0,
            HWC_MEDIASUBTYPE_YUY2 = 1,
            HWC_MEDIASUBTYPE_I420 = 2,
            HWC_MEDIASUBTYPE_RGB32 = 3
        };

        enum HWC_MEDIASUBTYPE mMediaSubtype;

    public:
        SampleGrabberCallback(PDRVHOSTWEBCAM pThis)
            :
            mpThis(pThis),
            refcnt(0),
            mpEnc(0),
            mMediaSubtype(HWC_MEDIASUBTYPE_MJPG)
        {
            return;
        }
        virtual ~SampleGrabberCallback()
        {
            HWCJPEGDestroy(mpEnc);
            UWLOG(("\n"));
        }

        HRESULT sampleCallbackInit(void)
        {
            HRESULT hr = S_OK;
            if (mpThis->pHostWebcam->guidFormat == MEDIASUBTYPE_YUY2)
            {
                mMediaSubtype = HWC_MEDIASUBTYPE_YUY2;
            }
            else if (   mpThis->pHostWebcam->guidFormat == MEDIASUBTYPE_I420
                     || mpThis->pHostWebcam->guidFormat == MEDIASUBTYPE_IYUV) /* Identical */
            {
                mMediaSubtype = HWC_MEDIASUBTYPE_I420;
            }
            else if (mpThis->pHostWebcam->guidFormat == MEDIASUBTYPE_RGB32)
            {
                mMediaSubtype = HWC_MEDIASUBTYPE_RGB32;
            }

            if (mMediaSubtype != HWC_MEDIASUBTYPE_MJPG)
            {
                int rc = HWCJPEGInit(&mpEnc);
                UWLOG(("%Rrc\n", rc));
                if (RT_FAILURE(rc))
                {
                    hr = E_FAIL;
                }
            }
            return hr;
        }

        ULONG STDMETHODCALLTYPE AddRef(void)
        {
            return ::InterlockedIncrement(&refcnt);
        }

        ULONG STDMETHODCALLTYPE Release(void)
        {
            LONG cnt = ::InterlockedDecrement(&refcnt);
            if (cnt == 0)
                delete this;
            return cnt;
        }

        STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject)
        {
            if (ppvObject == NULL) return E_POINTER;
            if (riid == __uuidof(IUnknown))
            {
                *ppvObject = static_cast<IUnknown*>(this);
                return S_OK;
            }
            if (riid == __uuidof(ISampleGrabberCB))
            {
                *ppvObject = static_cast<ISampleGrabberCB*>(this);
                return S_OK;
            }
            return E_NOTIMPL;
        }

        STDMETHODIMP SampleCB(double SampleTime, IMediaSample *pSample)
        {
            NOREF(SampleTime);

            LONG cbActual = pSample->GetActualDataLength();
            BYTE *pData = NULL;
            HRESULT hr = pSample->GetPointer(&pData);

            UWLOGF(("SampleCB: %p cbActual %d hr %x\n%.*Rhxd\n",
                    pData, cbActual, hr, cbActual <= 32? cbActual: 32, pData));

            if (SUCCEEDED(hr))
            {
                if (mpThis && mpThis->pIWebcamUp)
                {
                    int rc;
                    uint8_t *pu8Encoded;
                    uint32_t cbEncoded;

                    void *pvFrame = NULL;
                    LONG cbFrame = 0;

                    switch (mMediaSubtype)
                    {
                        default:
                        case HWC_MEDIASUBTYPE_MJPG:
                        {
                            pvFrame = pData;
                            cbFrame = cbActual;
                        } break;

                        case HWC_MEDIASUBTYPE_YUY2:
                        {
                            pu8Encoded = 0;
                            cbEncoded = 0;

                            rc = HWCJPEGEncodeFrameRaw_2vuy_yuvs(mpEnc, 75,
                                                                 &pu8Encoded, &cbEncoded,
                                                                 pData, cbActual,
                                                                 mpThis->pHostWebcam->lastSetup.u16Width,
                                                                 mpThis->pHostWebcam->lastSetup.u16Height,
                                                                 true);
                            if (RT_SUCCESS(rc))
                            {
                                pvFrame = pu8Encoded;
                                cbFrame = cbEncoded;
                            }
                        } break;

                        case HWC_MEDIASUBTYPE_I420:
                        {
                            pu8Encoded = 0;
                            cbEncoded = 0;

                            rc = HWCJPEGEncodeFrameRaw_I420(mpEnc, 75,
                                                            &pu8Encoded, &cbEncoded,
                                                            pData, cbActual,
                                                            mpThis->pHostWebcam->lastSetup.u16Width,
                                                            mpThis->pHostWebcam->lastSetup.u16Height);
                            if (RT_SUCCESS(rc))
                            {
                                pvFrame = pu8Encoded;
                                cbFrame = cbEncoded;
                            }
                        } break;

                        case HWC_MEDIASUBTYPE_RGB32:
                        {
                            pu8Encoded = 0;
                            cbEncoded = 0;

                            rc = HWCJPEGEncodeFrameBGR0(mpEnc, 75,
                                                        &pu8Encoded, &cbEncoded,
                                                        pData, cbActual,
                                                        mpThis->pHostWebcam->lastSetup.u16Width,
                                                        mpThis->pHostWebcam->lastSetup.u16Height);
                            if (RT_SUCCESS(rc))
                            {
                                pvFrame = pu8Encoded;
                                cbFrame = cbEncoded;
                            }
                        } break;
                    }

                    if (pvFrame)
                    {
                        VRDEVIDEOINPAYLOADHDR hdr;
                        hdr.u8HeaderLength      = sizeof(VRDEVIDEOINPAYLOADHDR);
                        hdr.u8HeaderInfo        = VRDE_VIDEOIN_PAYLOAD_F_EOF;
                        hdr.u32PresentationTime = 0;
                        hdr.u32SourceTimeClock  = 0;
                        hdr.u16Reserved         = 0;
                        mpThis->pIWebcamUp->pfnFrame(mpThis->pIWebcamUp,
                                                     1,
                                                     &hdr,
                                                     sizeof(hdr),
                                                     pvFrame,
                                                     cbFrame);
                        if (pvFrame != pData)
                        {
                            RTMemFree(pvFrame);
                        }
                    }
                }
            }

            return S_OK;
        }

        STDMETHODIMP BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen)
        {
            NOREF(SampleTime);
            NOREF(pBuffer);
            NOREF(BufferLen);
            return E_NOTIMPL;
        }
};


static void captureStop(PDRVHOSTWEBCAM pThis)
{
    UWLOGF(("Stopping capture\n"));

    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    if (pHostWebcam->pControl)
    {
        pHostWebcam->pControl->Stop();
    }

    HWC_RELEASE(pHostWebcam->pControl);
    HWC_RELEASE(pHostWebcam->pGraphBuilder);

    HWC_RELEASE(pHostWebcam->pNullFilter);

    HWC_RELEASE(pHostWebcam->pGrabber);
    HWC_RELEASE(pHostWebcam->pGrabberFilter);

    HWC_RELEASE(pHostWebcam->pAviDecFilter);
    HWC_RELEASE(pHostWebcam->pColourFilter);

    HWC_RELEASE(pHostWebcam->pCapturePin);
    HWC_RELEASE(pHostWebcam->pDeviceFilter);

    HWC_RELEASE(pHostWebcam->pSampleCallback);

    RT_ZERO(pHostWebcam->guidFormat);
}


static HRESULT captureStart(PDRVHOSTWEBCAM pThis)
{
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    /* Make sure that old resources are released. */
    captureStop(pThis);

    HRESULT hr = S_OK;

    do /* break loop */
    {
        /* Create objects. */
        pHostWebcam->pSampleCallback = new SampleGrabberCallback(pThis);
        if (!pHostWebcam->pSampleCallback)
        {
            hr = E_OUTOFMEMORY;
            break;
        }
        pHostWebcam->pSampleCallback->AddRef();

        hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&pHostWebcam->pGraphBuilder));
        HR_FAILED_BREAK(hr);

        hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&pHostWebcam->pGrabberFilter));
        HR_FAILED_BREAK(hr);

        hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&pHostWebcam->pNullFilter));
        HR_FAILED_BREAK(hr);

        /* Add SampleGrabber to the graph. */
        hr = pHostWebcam->pGraphBuilder->AddFilter(pHostWebcam->pGrabberFilter, L"Sample Grabber");
        HR_FAILED_BREAK(hr);

        hr = pHostWebcam->pGrabberFilter->QueryInterface(IID_PPV_ARGS(&pHostWebcam->pGrabber));
        HR_FAILED_BREAK(hr);

        /* Get webcam, add it to the graph and set requested format. */
        hr = hwcGetCaptureDevice(pThis->pHostWebcam->bstrDevicePath, &pHostWebcam->pDeviceFilter);
        HR_FAILED_BREAK(hr);

        hr = hwcGetPin(pHostWebcam->pDeviceFilter, PINDIR_OUTPUT, &pHostWebcam->pCapturePin);
        HR_FAILED_BREAK(hr);

        hr = pHostWebcam->pGraphBuilder->AddFilter(pHostWebcam->pDeviceFilter, L"Capture Source");
        HR_FAILED_BREAK(hr);

        /* First set parameters and then connect the pin.
         * Changing parameters on a connected pin may not work.
         */
        hr = setStreamFormat(pHostWebcam->pCapturePin,
                             pHostWebcam->lastSetup.u16Width,
                             pHostWebcam->lastSetup.u16Height,
                             pHostWebcam->lastSetup.u32FrameInterval,
                             &MEDIASUBTYPE_MJPG, &pHostWebcam->guidFormat);
        HR_FAILED_BREAK(hr);

        /* Set grabber format. */
        AM_MEDIA_TYPE mt;
        RT_ZERO(mt);
        mt.majortype = MEDIATYPE_Video;
        mt.subtype = pHostWebcam->guidFormat;
        hr = pHostWebcam->pGrabber->SetMediaType(&mt);
        HR_FAILED_BREAK(hr);

        /* Connect webcam to grabber. */
        hr = hwcConnectPin(pHostWebcam->pGraphBuilder, pHostWebcam->pCapturePin, pHostWebcam->pGrabberFilter);
        if (hr == VFW_E_CANNOT_CONNECT)
        {
            /* Try to insert a color space conversion filter between webcam and grabber. */
            UWLOG(("Trying CaptureGraphBuilder2\n"));
            ICaptureGraphBuilder2 *pCaptureBuilder = 0;
            do {
                hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, 0, CLSCTX_INPROC,
                                      IID_PPV_ARGS(&pCaptureBuilder));
                HR_FAILED_BREAK(hr);

                hr = CoCreateInstance(CLSID_AVIDec, 0, CLSCTX_INPROC,
                                      IID_PPV_ARGS(&pHostWebcam->pAviDecFilter));
                HR_FAILED_BREAK(hr);

                hr = CoCreateInstance(CLSID_Colour, 0, CLSCTX_INPROC,
                                      IID_PPV_ARGS(&pHostWebcam->pColourFilter));
                HR_FAILED_BREAK(hr);

                hr = pHostWebcam->pGraphBuilder->AddFilter(pHostWebcam->pAviDecFilter, L"AviDec Filter");
                HR_FAILED_BREAK(hr);

                hr = pHostWebcam->pGraphBuilder->AddFilter(pHostWebcam->pColourFilter, L"Colour Filter");
                HR_FAILED_BREAK(hr);

                hr = pCaptureBuilder->SetFiltergraph(pHostWebcam->pGraphBuilder);
                HR_FAILED_BREAK(hr);

                hr = pCaptureBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                                   pHostWebcam->pCapturePin, 0, pHostWebcam->pAviDecFilter);
                HR_FAILED_BREAK(hr);

                hr = pCaptureBuilder->RenderStream(0, &MEDIATYPE_Video,
                                                   pHostWebcam->pAviDecFilter, 0, pHostWebcam->pGrabberFilter);
                if (FAILED(hr))
                {
                    /* Retry with CLSID_Colour. */
                    hr = pCaptureBuilder->RenderStream(0, &MEDIATYPE_Video,
                                                       pHostWebcam->pAviDecFilter, pHostWebcam->pColourFilter,
                                                       pHostWebcam->pGrabberFilter);
                }

                HR_FAILED_BREAK(hr);
            } while(0);

            HWC_RELEASE(pCaptureBuilder);
        }
        HR_FAILED_BREAK(hr);

        /* Add renderer and connect grabber to renderer. */
        hr = pHostWebcam->pGraphBuilder->AddFilter(pHostWebcam->pNullFilter, L"Null Renderer");
        HR_FAILED_BREAK(hr);

        hr = hwcConnectFilters(pHostWebcam->pGraphBuilder, pHostWebcam->pGrabberFilter, pHostWebcam->pNullFilter);
        HR_FAILED_BREAK(hr);

        hr = pHostWebcam->pSampleCallback->sampleCallbackInit();
        HR_FAILED_BREAK(hr);

        /* 'FALSE' should be default, but set it anyway. Ignore result. */
        pHostWebcam->pGrabber->SetOneShot(FALSE);

        hr = pHostWebcam->pGrabber->SetCallback(pHostWebcam->pSampleCallback, 0);
        HR_FAILED_BREAK(hr);

        if (UWLOGENABLED())
        {
            RT_ZERO(mt);
            hr = pHostWebcam->pGrabber->GetConnectedMediaType(&mt);
            dumpMediaType(&mt);
        }

        hr = pHostWebcam->pGraphBuilder->QueryInterface(IID_PPV_ARGS(&pHostWebcam->pControl));
        HR_FAILED_BREAK(hr);

        hr = pHostWebcam->pControl->Run();
        HR_FAILED_BREAK(hr);
    } while (0);

    if (FAILED(hr))
    {
        captureStop(pThis);
    }

    UWLOG(("hr 0x%08X\n", hr));
    return hr;
};


/*
 * Device removal monitor.
 *
 * Explicitly use unicode API (*W) because it will give unicode dbcc_name, which can be
 * directly compared with pThis->pHostWebcam->bstrDevicePath.
 */
static LRESULT CALLBACK hwcMonitorWindowProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    if (uiMsg == WM_DEVICECHANGE)
    {
        PDRVHOSTWEBCAM pThis = (PDRVHOSTWEBCAM)GetWindowLongPtrW(hwnd, 0);
        UWLOG(("WM_DEVICECHANGE: wParam 0x%x, lParam %p, pThis %p\n", wParam, lParam, pThis));
        if (pThis)
        {
            /* Check that it is a removal of the monitored device. */
            DEV_BROADCAST_HDR *pHdr = (DEV_BROADCAST_HDR *)lParam;
            if (pHdr && pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            {
                DEV_BROADCAST_DEVICEINTERFACE_W *pDi = (DEV_BROADCAST_DEVICEINTERFACE_W *)pHdr;
                UWLOG(("WM_DEVICECHANGE: name %ls\n", pDi->dbcc_name));

                if (_wcsicmp(pThis->pHostWebcam->bstrDevicePath, pDi->dbcc_name) == 0)
                {
                    /* Our device. */
                    if (wParam == DBT_DEVICEREMOVECOMPLETE)
                        pThis->pIWebcamUp->pfnDetached(pThis->pIWebcamUp, 1);
                    else if (wParam == DBT_DEVICEARRIVAL)
                    {
                        /* Rettach device. Implement later. */
                        // hwcReportAttached(pThis);
                    }
                }
            }
        }
        return 0;
    }

    return DefWindowProcW(hwnd, uiMsg, wParam, lParam);
}

static HWND hwcMonitorWindowCreate(RTUTF16 *pwszClassName)
{
    HWND hwnd = 0;
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandleW(NULL);

    UWLOG(("Class name %ls\n", pwszClassName));

    WNDCLASSW wndClass;
    RT_ZERO(wndClass);
    wndClass.lpfnWndProc = hwcMonitorWindowProc;
    wndClass.cbWndExtra = sizeof(uintptr_t);
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = pwszClassName;

    if (RegisterClassW(&wndClass))
    {
        hwnd = CreateWindowExW(0, wndClass.lpszClassName, NULL, 0,
                               0, 0, 0, 0, HWND_MESSAGE,
                               NULL, hInstance, NULL);
        if (!hwnd)
        {
            /* Cleanup. */
            UnregisterClassW(wndClass.lpszClassName, hInstance);
        }
    }
    else
    {
        UWLOG(("RegisterClass failed\n"));
    }

    return hwnd;
}

static void hwcMonitorWindowDestroy(HWND hwnd, RTUTF16 *pwszClassName)
{
    if (hwnd)
    {
        HINSTANCE hInstance = (HINSTANCE)GetModuleHandleW(NULL);
        DestroyWindow(hwnd);
        UnregisterClassW(pwszClassName, hInstance);
    }
}

static HDEVNOTIFY hwcMonitorRegisterNotification(HWND hwnd)
{
    DEV_BROADCAST_DEVICEINTERFACE di;
    RT_ZERO(di);
    di.dbcc_size = sizeof(di);
    di.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    di.dbcc_classguid = KSCATEGORY_CAPTURE;

    return RegisterDeviceNotificationW(hwnd, &di, DEVICE_NOTIFY_WINDOW_HANDLE);
}

/** @callback_method_impl{FNPDMTHREADDRV} */
static DECLCALLBACK(int) hwcThreadMonitorWorker(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    UWLOG(("#%d: state %d\n", pDrvIns->iInstance, pThread->enmState));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
    {
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    PDRVHOSTWEBCAM pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTWEBCAM);
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;

    /*
     * Create a message window and register device change notification.
     */

    /* Use iInstance to generate uniq class name for the process. */
    RTUTF16 wszClassName[64] = L"VIRTUALBOX.HWCMONITOR";

    char szInstance[32];
    RTStrPrintf(szInstance, sizeof(szInstance), "%d", pThis->pDrvIns->iInstance);

    RTUTF16 *pwsz = &wszClassName[RTUtf16Len(wszClassName)];
    const char *psz = &szInstance[0];
    while (*psz)
    {
        *pwsz++ = (RTUTF16)*psz++;
    }
    *pwsz = 0;

    HWND hwnd = hwcMonitorWindowCreate(&wszClassName[0]);
    if (hwnd)
    {
        SetWindowLongPtrW(hwnd, 0, (LONG_PTR)pThis);

        HDEVNOTIFY hdevnotify = hwcMonitorRegisterNotification(hwnd);
        if (hdevnotify)
        {
            UWLOG(("#%d: Monitoring\n", pDrvIns->iInstance));
            pHostWebcam->monitor.hwnd = hwnd;
            MSG msg;
            while(   pThread->enmState == PDMTHREADSTATE_RUNNING
                  && GetMessageW(&msg, NULL, 0, 0) > 0)
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            pHostWebcam->monitor.hwnd = 0;

            UnregisterDeviceNotification(hdevnotify);
        }
        else
        {
            UWLOG(("Failed devnotify\n"));
            rc = VERR_NOT_SUPPORTED;
        }

        hwcMonitorWindowDestroy(hwnd, &wszClassName[0]);
    }
    else
    {
        UWLOG(("Failed hwnd\n"));
        rc = VERR_NOT_SUPPORTED;
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

/** @callback_method_impl{FNPDMTHREADWAKEUPDRV} */
static DECLCALLBACK(int) hwcThreadMonitorWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTWEBCAM pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTWEBCAM);
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;
    RT_NOREF(pThread);

    UWLOG(("#%d: hwnd %p\n", pDrvIns->iInstance, pHostWebcam->monitor.hwnd));
    if (pHostWebcam->monitor.hwnd)
        PostMessageW(pHostWebcam->monitor.hwnd, WM_CLOSE, 0, 0); /* Any message should do. */

    return VINF_SUCCESS;
}

static int hwcMonitorStart(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis)
{
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;
    int rc = PDMDrvHlpThreadCreate(pDrvIns, &pHostWebcam->monitor.pThreadMonitor,
                                   NULL, hwcThreadMonitorWorker, hwcThreadMonitorWakeup,
                                   0, RTTHREADTYPE_IO, "HWCMONITOR");
    if (RT_SUCCESS(rc))
        PDMDrvHlpThreadResume(pDrvIns, pHostWebcam->monitor.pThreadMonitor);

    return rc;
}

static void hwcMonitorStop(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis)
{
    HOSTWEBCAM *pHostWebcam = pThis->pHostWebcam;
    if (pHostWebcam->monitor.pThreadMonitor)
    {
        int rcThread = VINF_SUCCESS;
        PDMDrvHlpThreadDestroy(pDrvIns, pHostWebcam->monitor.pThreadMonitor, &rcThread);
        pHostWebcam->monitor.pThreadMonitor = NULL;
    }
}


/*
 * Device helpers.
 */

static void hwcStoreDeviceInfo(PDRVHOSTWEBCAM pThis, IPropertyBag *pPropBag, BSTR devicePath)
{
    /* Use FriendlyName or Description as pszDeviceName. */
    RTStrFree(pThis->pHostWebcam->pszDeviceName);
    pThis->pHostWebcam->pszDeviceName = NULL;

    VARIANT var;
    VariantInit(&var);
    HRESULT hr = pPropBag->Read(L"FriendlyName", &var, 0);
    if (FAILED(hr))
    {
        hr = pPropBag->Read(L"Description", &var, 0);
    }

    if (SUCCEEDED(hr))
    {
        int rc = RTUtf16ToUtf8((PRTUTF16)var.bstrVal, &pThis->pHostWebcam->pszDeviceName);
        if (RT_FAILURE(rc))
        {
            pThis->pHostWebcam->pszDeviceName = NULL;
        }
        VariantClear(&var);
    }

    /* Use CRC64 of devicePath as the serial.
     * Windows guest uses the serial number to distinguish devices
     * and uses cached information for the same device.
     * Use CRC64 of the uniq devicePath to have a constant serial number
     * for a specific host device.
     */
    RTStrFree(pThis->pHostWebcam->pszSerial);
    pThis->pHostWebcam->pszSerial = NULL;
    uint64_t u64 = RTCrc64(devicePath, SysStringByteLen(devicePath));
    RTStrAPrintf(&pThis->pHostWebcam->pszSerial, "%RX64", u64);
}

static HRESULT hwcFindDevice(PDRVHOSTWEBCAM pThis)
{
    /* Find the webcam and save the DevicePath property to bstrDevicePath. */
    if (pThis->pHostWebcam->bstrDevicePath)
    {
        SysFreeString(pThis->pHostWebcam->bstrDevicePath);
        pThis->pHostWebcam->bstrDevicePath = NULL;
    }

    UWLOG(("idx %d, path [%s]\n", pThis->i32DeviceIndex, pThis->pszDevicePath));

    int rc;
    uint16_t u16VID = 0;
    uint16_t u16PID = 0;
    if (pThis->i32DeviceIndex == -1)
    {
        /* Deal with special ".{uuid}VVVVPPPP:address" path. */
        if (   pThis->pszDevicePath[0] == '.'
            && pThis->pszDevicePath[1] == '{')
        {
            /* Extract vendor id and product id. */
            const char *p = strchr(pThis->pszDevicePath, '}');
            if (p)
            {
                p++;
                char *pszNext = NULL;
                uint32_t u32 = 0;
                rc = RTStrToUInt32Ex(p, &pszNext, 16, &u32);
                if (rc == VINF_SUCCESS || rc == VWRN_TRAILING_CHARS)
                {
                    if (*pszNext == ':')
                    {
                        u16VID = (uint16_t)((u32 >> 16) & 0xFFFF);
                        u16PID = (uint16_t)(u32 & 0xFFFF);
                        pThis->i32DeviceIndex = -2;
                    }
                }
            }
        }
    }

    PRTUTF16 pwszPath = NULL;
    if (pThis->i32DeviceIndex == -1)
    {
        /* Convert UTF8 requested path to UTF16. */
        rc = RTStrToUtf16(pThis->pszDevicePath, &pwszPath);
        if (RT_FAILURE(rc))
        {
            return E_FAIL;
        }
    }
    else if (pThis->i32DeviceIndex == -2)
    {
        char *psz = NULL;
        rc = RTStrAPrintf(&psz, "\\\\?\\usb#vid_%04x&pid_%04x&", u16VID, u16PID);
        if (rc > 0)
        {
            rc = RTStrToUtf16(psz, &pwszPath);
            RTStrFree(psz);
        }
        else
            rc = VERR_NO_MEMORY;
        if (RT_FAILURE(rc))
        {
            return E_FAIL;
        }
    }

    ICreateDevEnum *pDevEnum = NULL;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pDevEnum));
    if (SUCCEEDED(hr))
    {
        IEnumMoniker *pEnum = NULL;
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (hr == S_FALSE)
        {
            hr = VFW_E_NOT_FOUND; /* No video devices. */
        }
        HWC_RELEASE(pDevEnum);

        if (SUCCEEDED(hr))
        {
            int32_t iDevice = 0; /* Count devices 1..N. */
            IMoniker *pMoniker = NULL;
            while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
            {
                /* Device index must be the same as in hwcFillList. */
                iDevice++;

                IPropertyBag *pPropBag = NULL;
                hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
                if (SUCCEEDED(hr))
                {
                    VARIANT var;
                    VariantInit(&var);

                    hr = pPropBag->Read(L"DevicePath", &var, 0);
                    if (SUCCEEDED(hr))
                    {
                        BSTR devicePath = NULL;

                        /* Check if that is the device. */
                        if (pThis->i32DeviceIndex == -1)
                        {
                            /* By path. */
                            if (RTUtf16ICmp(pwszPath, var.bstrVal) == 0)
                            {
                                devicePath = var.bstrVal;
                            }
                        }
                        else if (pThis->i32DeviceIndex == -2)
                        {
                            /* Check if the path starts with pwszPath prefix. */
                            PRTUTF16 pwsz1 = pwszPath;
                            PRTUTF16 pwsz2 = var.bstrVal;
                            while (*pwsz1 && *pwsz2 && *pwsz1 == *pwsz2)
                            {
                                pwsz1++;
                                pwsz2++;
                            }
                            if (*pwsz1 == 0)
                            {
                               devicePath = var.bstrVal;
                            }
                        }
                        else
                        {
                            /* By index. Check that the current device index is equal to requested.
                             * Consider 0 == 1 for pThis->i32DeviceIndex, that is the default device
                             * is the first device found.
                             */
                            if (pThis->i32DeviceIndex <= iDevice)
                            {
                                devicePath = var.bstrVal;
                            }
                        }

                        /* Found it. Loop will be terminated below. */
                        if (devicePath)
                        {
                            pThis->pHostWebcam->bstrDevicePath = SysAllocString(devicePath);

                            hwcStoreDeviceInfo(pThis, pPropBag, devicePath);
                        }

                        VariantClear(&var);
                    }
                    HWC_RELEASE(pPropBag);
                }
                HWC_RELEASE(pMoniker);

                /* Found it. */
                if (pThis->pHostWebcam->bstrDevicePath != NULL)
                {
                    UWLOG(("Device path: %ls\n", pThis->pHostWebcam->bstrDevicePath));
                    break;
                }
            }
            HWC_RELEASE(pEnum);
        }
    }

    RTUtf16Free(pwszPath);

    return pThis->pHostWebcam->bstrDevicePath? S_OK: E_FAIL;
}


int hostWebcamControl(PDRVHOSTWEBCAM pThis,
                      void *pvUser,
                      uint64_t u64DeviceId,
                      const VRDEVIDEOINCTRLHDR *pHdr,
                      uint32_t cbCtrl)
{
    int rc = VINF_SUCCESS;

    switch (pHdr->u16ControlSelector)
    {
        case VRDE_VIDEOIN_CTRLSEL_VS_SETUP:
        {
            UWLOGF(("VRDE_VIDEOIN_CTRLSEL_VS_SETUP\n"));
            VRDEVIDEOINCTRL_VS_SETUP *pSetup = (VRDEVIDEOINCTRL_VS_SETUP *)pHdr;
            pThis->pHostWebcam->lastSetup = *pSetup;
            pThis->pIWebcamUp->pfnControl(pThis->pIWebcamUp,
                                          true, /* fResponse */
                                          pvUser,
                                          u64DeviceId,
                                          pHdr,
                                          cbCtrl);
        } break;

        case VRDE_VIDEOIN_CTRLSEL_VS_ON:
        {
            UWLOGF(("VRDE_VIDEOIN_CTRLSEL_VS_ON\n"));
            rc = captureStart(pThis);
            if (RT_SUCCESS(rc))
            {
                pThis->pIWebcamUp->pfnControl(pThis->pIWebcamUp,
                                              true, /* fResponse */
                                              pvUser,
                                              u64DeviceId,
                                              pHdr,
                                              cbCtrl);
            }
        } break;

        case VRDE_VIDEOIN_CTRLSEL_VS_OFF:
        {
            UWLOGF(("VRDE_VIDEOIN_CTRLSEL_VS_OFF\n"));
            captureStop(pThis);
            pThis->pIWebcamUp->pfnControl(pThis->pIWebcamUp,
                                          true, /* fResponse */
                                          pvUser,
                                          u64DeviceId,
                                          pHdr,
                                          cbCtrl);
        } break;

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    return rc;
}

#define HWC_MAX_INTERVALS 6

#pragma pack(1)
typedef struct HWCFRAMEDESC
{
    VRDEVIDEOINFRAMEDESC frame;
    uint16_t cbIntervals;
    uint32_t au32Intervals[HWC_MAX_INTERVALS];
} HWCFRAMEDESC;

typedef struct HWCDEVICEEXT
{
    VRDEVIDEOINDEVICEEXT ext;
    VRDEVIDEOINDEVICEFIELDHDR hdrProduct;
    char achProduct[128];
    VRDEVIDEOINDEVICEFIELDHDR hdrSerial;
    char achSerial[64];
} HWCDEVICEEXT;

typedef struct HWCDEVICEDESC
{
    VRDEVIDEOINDEVICEDESC desc;
    HWCDEVICEEXT ext;
    VRDEVIDEOINFORMATDESC format;
    HWCFRAMEDESC aFrames[1];
} HWCDEVICEDESC;
#pragma pack()

static uint32_t sau32Intervals[HWC_MAX_INTERVALS] =
{            /* FPS */
    333333,  /* 30  */
    400000,  /* 25  */
    500000,  /* 20  */
    666666,  /* 15  */
    1000000, /* 10  */
    2000000  /* 5   */
};

static void hwcFrameDescTryAdd(HWCDEVICEDESC *pDesc, int *pcFrames, VIDEOINFOHEADER *pVIH,
                               const GUID *pSubtype, int iExistingFrame)
{
    UWLOG(("VIDEOINFOHEADER: %dx%d %lld iExisting %d%s {%RTuuid}\n",
           pVIH->bmiHeader.biWidth, pVIH->bmiHeader.biHeight, pVIH->AvgTimePerFrame,
           iExistingFrame, *pSubtype == MEDIASUBTYPE_MJPG? " MJPG": "", pSubtype));

    int iFrame = iExistingFrame == -1? *pcFrames: iExistingFrame;

    VRDEVIDEOINFRAMEDESC *p = &pDesc->aFrames[iFrame].frame;

    /* u8FrameFlags is temporarily used to indicate that the frame description was from MJPG format. */
    if (iExistingFrame != -1 && p->u8FrameFlags != 0)
    {
        /* Do not alter the frame anymore. */
        return;
    }

    if (iExistingFrame == -1)
    {
        p->cbFrame      = sizeof(HWCFRAMEDESC);
        p->u8FrameId    = (uint8_t)(iFrame + 1);
        p->u8FrameFlags = 0;
        p->u16Width     = pVIH->bmiHeader.biWidth;
        p->u16Height    = pVIH->bmiHeader.biHeight;

        pDesc->aFrames[iFrame].cbIntervals = sizeof(pDesc->aFrames[iFrame].au32Intervals);
    }

    if (*pSubtype == MEDIASUBTYPE_MJPG)
    {
        /* Mark the frame as complete. */
        p->u8FrameFlags =   VRDE_VIDEOIN_F_FRM_DISCRETE_INTERVALS
                          | VRDE_VIDEOIN_F_FRM_SIZE_OF_FIELDS;
    }

    if (iExistingFrame != -1)
    {
        /* Check if pVIH->AvgTimePerFrame indicates higher framerate then the existing frame description. */
        if (pVIH->AvgTimePerFrame >= p->u32MinFrameInterval)
        {
            /* No need to update anything. */
            return;
        }
    }

    /* DirectShow returns only default framerate. Fill framerates which are lower than the default. */
    p->u32MinFrameInterval = 0xFFFFFFFF;
    p->u32MaxFrameInterval = 0;
    RT_ZERO(pDesc->aFrames[iFrame].au32Intervals);

    uint32_t iSrc, iDst;
    for (iSrc = 0, iDst = 0; iSrc < HWC_MAX_INTERVALS; iSrc++)
    {
        if (pVIH->AvgTimePerFrame > sau32Intervals[iSrc])
        {
            continue;
        }

        pDesc->aFrames[iFrame].au32Intervals[iDst] = sau32Intervals[iSrc];
        p->u32MinFrameInterval = RT_MIN(p->u32MinFrameInterval, sau32Intervals[iSrc]);
        p->u32MaxFrameInterval = RT_MAX(p->u32MaxFrameInterval, sau32Intervals[iSrc]);
        iDst++;
    }

    if (iDst == 0)
    {
        /* Just use this one frame large interval anyway. */
        pDesc->aFrames[iFrame].au32Intervals[0] = (uint32_t)pVIH->AvgTimePerFrame;
        iDst++;

        p->u32MinFrameInterval = (uint32_t)pVIH->AvgTimePerFrame;
        p->u32MaxFrameInterval = p->u32MinFrameInterval;
    }

    p->u32NumFrameIntervals = iDst;

    /* It was a new frame. */
    if (iExistingFrame == -1)
    {
        *pcFrames = iFrame + 1;
    }
}

static int hwcFrameDescFind(HWCDEVICEDESC *pDesc, int cFrames, uint16_t u16Width, uint16_t u16Height)
{
    int iIndex;
    for (iIndex = 0; iIndex < cFrames; iIndex++)
    {
        VRDEVIDEOINFRAMEDESC *p = &pDesc->aFrames[iIndex].frame;
        if (p->u16Width == u16Width && p->u16Height == u16Height)
        {
            return iIndex;
        }
    }
    return -1;
}

static int hwcBuildDeviceDesc(PDRVHOSTWEBCAM pThis,
                              IAMStreamConfig *pStreamConfig,
                              int iCount,
                              BYTE *pSCC,
                              VRDEVIDEOINDEVICEDESC **ppDeviceDesc,
                              uint32_t *pcbDeviceDesc)
{
    int rc = VINF_SUCCESS;

    uint32_t cbDesc = sizeof(HWCDEVICEDESC) + sizeof(HWCFRAMEDESC) * (iCount - 1);
    HWCDEVICEDESC *pDesc = (HWCDEVICEDESC *)RTMemAllocZ(cbDesc);

    if (pDesc)
    {
        int cFrames = 0;

        int iIndex;
        for (iIndex = 0; iIndex < iCount && cFrames < 255; iIndex++)
        {
            AM_MEDIA_TYPE *pMT = NULL;
            HRESULT hr = pStreamConfig->GetStreamCaps(iIndex, &pMT, pSCC);
            if (FAILED(hr))
            {
                UWLOG(("GetStreamCaps[%d] %x\n", iIndex, hr));
                continue;
            }

            if (pMT->formattype == FORMAT_VideoInfo)
            {
                VIDEOINFOHEADER *pVIH = (VIDEOINFOHEADER *)pMT->pbFormat;

                /* Check for duplicates. */
                int iFrame = hwcFrameDescFind(pDesc, cFrames,
                                              (uint16_t)pVIH->bmiHeader.biWidth,
                                              (uint16_t)pVIH->bmiHeader.biHeight);
                hwcFrameDescTryAdd(pDesc, &cFrames, pVIH, &pMT->subtype, iFrame);
            }

            freeMediaType(pMT);
        }

        if (cFrames > 0)
        {
            /* Set proper frame flags. The field was used temporarily by the above code. */
            for (iIndex = 0; iIndex < cFrames; iIndex++)
            {
                VRDEVIDEOINFRAMEDESC *p = &pDesc->aFrames[iIndex].frame;
                p->u8FrameFlags =   VRDE_VIDEOIN_F_FRM_DISCRETE_INTERVALS
                                  | VRDE_VIDEOIN_F_FRM_SIZE_OF_FIELDS;
            }

            cbDesc = sizeof(HWCDEVICEDESC) + sizeof(HWCFRAMEDESC) * (cFrames - 1);

            pDesc->desc.u16ObjectiveFocalLengthMin = 0;
            pDesc->desc.u16ObjectiveFocalLengthMax = 0;
            pDesc->desc.u16OcularFocalLength       = 0;
            pDesc->desc.u16MaxMultiplier           = 0;
            pDesc->desc.fu32CameraControls         = 0;
            pDesc->desc.fu32ProcessingControls     = VRDE_VIDEOIN_F_PU_CTRL_BRIGHTNESS;
            pDesc->desc.fu8DeviceCaps              = 0;
            pDesc->desc.u8NumFormats               = 1;
            pDesc->desc.cbExt                      = sizeof(HWCDEVICEEXT);

            pDesc->ext.ext.fu32Fields = 0;
            if (pThis->pHostWebcam->pszDeviceName)
            {
                pDesc->ext.ext.fu32Fields |= VRDE_VIDEOIN_F_DEV_EXT_NAME;
                pDesc->ext.hdrProduct.cbField =   sizeof(VRDEVIDEOINDEVICEFIELDHDR)
                                                + sizeof(pDesc->ext.achProduct);
                RTStrCopy(pDesc->ext.achProduct, sizeof(pDesc->ext.achProduct),
                          pThis->pHostWebcam->pszDeviceName);
            }

            if (pThis->pHostWebcam->pszSerial)
            {
                pDesc->ext.ext.fu32Fields |= VRDE_VIDEOIN_F_DEV_EXT_SERIAL;
                pDesc->ext.hdrSerial.cbField =   sizeof(VRDEVIDEOINDEVICEFIELDHDR)
                                               + sizeof(pDesc->ext.achSerial);
                RTStrCopy(pDesc->ext.achSerial, sizeof(pDesc->ext.achSerial),
                          pThis->pHostWebcam->pszSerial);
            }

            pDesc->format.cbFormat      = sizeof(VRDEVIDEOINFORMATDESC);
            pDesc->format.u8FormatId    = 1;
            pDesc->format.u8FormatType  = VRDE_VIDEOIN_FORMAT_MJPEG;
            pDesc->format.u8FormatFlags = 0;
            pDesc->format.u8NumFrames   = (uint8_t)cFrames;
            pDesc->format.u16Reserved   = 0;
        }
        else
        {
            rc = VERR_NOT_SUPPORTED;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *ppDeviceDesc = &pDesc->desc;
        *pcbDeviceDesc = cbDesc;
    }

    return rc;
}

int hostWebcamQueryDeviceDesc(PDRVHOSTWEBCAM pThis,
                              VRDEVIDEOINDEVICEDESC **ppDeviceDesc,
                              uint32_t *pcbDeviceDesc)
{
    IBaseFilter *pDevice = NULL;
    IPin *pPin = NULL;
    HRESULT hr = hwcGetCaptureDevice(pThis->pHostWebcam->bstrDevicePath, &pDevice);
    if (SUCCEEDED(hr))
    {
        hr = hwcGetPin(pDevice, PINDIR_OUTPUT, &pPin);
        HWC_RELEASE(pDevice);
    }

    if (FAILED(hr))
        return VERR_NOT_SUPPORTED;

    int rc = VINF_SUCCESS;

    IAMStreamConfig *pStreamConfig = NULL;
    hr = pPin->QueryInterface(&pStreamConfig);
    if (SUCCEEDED(hr))
    {
        int iCount = 0;
        int iSize = 0;
        hr = pStreamConfig->GetNumberOfCapabilities(&iCount, &iSize);
        if (SUCCEEDED(hr))
        {
            UWLOG(("iCount %d, iSize %d\n", iCount, iSize));

            BYTE *pSCC = (BYTE *)RTMemAlloc(iSize);
            if (pSCC)
                rc = hwcBuildDeviceDesc(pThis, pStreamConfig, iCount, pSCC, ppDeviceDesc, pcbDeviceDesc);
            else
                rc = VERR_NO_MEMORY;
        }
        else
        {
            UWLOG(("GetNumberOfCapabilities %x\n", hr));
            rc = VERR_NOT_SUPPORTED;
        }

        HWC_RELEASE(pStreamConfig);
    }
    else
    {
        UWLOG(("IAMStreamConfig %x\n", hr));
        rc = VERR_NOT_SUPPORTED;
    }

    HWC_RELEASE(pPin);
    return rc;
}

int hostWebcamConstruct(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis, PCFGMNODE pCfg)
{
    /* One time initialization of the backend. */
    NOREF(pCfg);

    pThis->pHostWebcam = (HOSTWEBCAM *)RTMemAllocZ(sizeof(HOSTWEBCAM));
    AssertReturn(pThis->pHostWebcam, VERR_NO_MEMORY);

    HRESULT hrc = hwcFindDevice(pThis);
    if (SUCCEEDED(hrc))
        hwcMonitorStart(pDrvIns, pThis);
    else
        return VERR_NOT_FOUND;
    return VINF_SUCCESS;
}

void hostWebcamDestruct(PPDMDRVINS pDrvIns, PDRVHOSTWEBCAM pThis)
{
    if (pThis->pHostWebcam)
    {
        hwcMonitorStop(pDrvIns, pThis);

        /* Destruction of the backend. */
        if (pThis->pHostWebcam->bstrDevicePath)
        {
           SysFreeString(pThis->pHostWebcam->bstrDevicePath);
           pThis->pHostWebcam->bstrDevicePath = NULL;
        }

        RTStrFree(pThis->pHostWebcam->pszDeviceName);
        RTStrFree(pThis->pHostWebcam->pszSerial);

        RTMemFree(pThis->pHostWebcam);
        pThis->pHostWebcam = NULL;
    }
}
