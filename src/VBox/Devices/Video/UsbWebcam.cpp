/* $Id: UsbWebcam.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * USB Webcam Device Emulation.
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
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_USB_WEBCAM

#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/req.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/timer.h>
#include <iprt/uuid.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/version.h>
#include <VBox/vmm/pdmusb.h>
#include <VBox/vmm/pdmwebcaminfs.h>
#include <VBox/vrdpusb.h>

#include "UsbWebcam.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* Profiling. Writes to the release log. */
// #define UWPROFILE

#define USBWEBCAM_NUM_UNITS_CONTROL   4 /* Interface + Camera + Processing + Output */
#define USBWEBCAM_NUM_UNITS_STREAMING 1 /* Interface */

/* Number of controls for each entity. Includes the 0 == *_CONTROL_UNDEFINED */
#define USBWEBCAM_NUM_CONTROLS_VC VUSB_V_VC_MAX
#define USBWEBCAM_NUM_CONTROLS_CT VUSB_V_CT_MAX
#define USBWEBCAM_NUM_CONTROLS_PU VUSB_V_PU_MAX
#define USBWEBCAM_NUM_CONTROLS_OT VUSB_V_TE_MAX
#define USBWEBCAM_NUM_CONTROLS_VS VUSB_V_VS_MAX

/*
 * UVC1.1: "Specifies the maximum number of bytes that the device can transmit
 *          or receive in a single payload transfer. This field is set by the
 *          device and read only from the host. Some host implementations restrict
 *          the maximum value permitted for this field."
 * The default value is equal to a value from a Logitech webcam.
 */
#define USBWEBCAM_MAX_PAYLOAD_TRANSFER_SIZE 3060

#define UW_F_SET_CUR  0x01
#define UW_F_GET_CUR  0x02
#define UW_F_GET_MIN  0x04
#define UW_F_GET_MAX  0x08
#define UW_F_GET_RES  0x10
#define UW_F_GET_LEN  0x20
#define UW_F_GET_INFO 0x40
#define UW_F_GET_DEF  0x80

#define UW_F_INFO_GET          0x01
#define UW_F_INFO_SET          0x02
#define UW_F_INFO_AUTODISABLED 0x04
#define UW_F_INFO_AUTOUPDATE   0x08
#define UW_F_INFO_ASYNC        0x10


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
class UWCtrl;
struct WEBCAMCTRLS;

typedef struct WEBCAMUNIT
{
    uint8_t cControls;
    UWCtrl **papControls;
} WEBCAMUNIT;

typedef struct WEBCAMINTERFACE
{
    UWCtrl *pCtrlErrorCode;

    uint8_t cUnits;
    WEBCAMUNIT *paUnits;
} WEBCAMINTERFACE;

typedef enum USBWEBCAMSTATE
{
    UW_STATE_NO_DRIVER = 0, /* No PDM driver is available. */
    UW_STATE_NO_WEBCAM,     /* No webcam is reported by the driver. */
    UW_STATE_PROXY,         /* Using a webcam provided by the driver. */
    UW_STATE_DETACHING      /* A webcam is being detached. */
} USBWEBCAMSTATE;

#define USBWEBCAM_IS_STANDALONE(pThis) ((pThis)->enmState != UW_STATE_PROXY)
#define USBWEBCAM_IS_PROXY(pThis) ((pThis)->enmState == UW_STATE_PROXY)

typedef struct USBWEBCAMPENDINGFRAME
{
    RTLISTNODE nodeFrame;

    int32_t volatile cRefs;

    VRDEVIDEOINPAYLOADHDR hdr;

    uint8_t *pu8Data;
    uint32_t cbData;
} USBWEBCAMPENDINGFRAME;

typedef struct UWSTATUSEVENT
{
    RTLISTNODE nodeEvent;
    uint8_t cbPacket; /* Size of the packet data in the union. */
    uint8_t cbValue;  /* Size of data in the bValue. */
    union
    {
        VUSBVSTATUSINTR hdr;
        VUSBVSTATUSINTRVC vc;
        VUSBVSTATUSINTRVS vs;
    } u;
    uint8_t bValue[16]; /* Enough to hold a CET_CUR result of any control */
} UWSTATUSEVENT;

/* URB queue. */
typedef struct URBQUEUE
{
    PVUSBURB pUrbHead;
    PVUSBURB *ppUrbTail;
} URBQUEUE, *PURBQUEUE;

typedef struct UWURBQUEUES
{
    URBQUEUE BulkIn; /* Pending bulk-in URBs. */
    URBQUEUE IntrIn; /* Pending intr-in URBs. */
    URBQUEUE Done;   /* Completed URBs ready for pfnUrbReap */
} UWURBQUEUES;

/*
 * When the guest submits the SET_CUR(COMMIT) request, the streaming parameters are set
 * but streaming is not enabled.
 *
 * Streaming is enabled when the guest starts to read from the bulk-in endpoint.
 *
 * The stream status reflects the two stage behavior.
 */
typedef enum USBWEBCAMSTREAMSTATUS
{
    UW_STREAM_STATUS_OFF = 0, /* No streaming. */
    UW_STREAM_STATUS_SETUP,   /* Streaming parameters were set, ok to enable streaming on a bulk-in URB. */
    UW_STREAM_STATUS_ON       /* Streaming is enabled. */
} USBWEBCAMSTREAMSTATUS;

typedef DECLCALLBACKTYPE(int, FNEMULATEDUSB,(void *pv, const char *pszId, uint32_t iEvent, const void *pvData, uint32_t cbData));
typedef FNEMULATEDUSB *PFNEMULATEDUSB;

typedef struct USBWEBCAM
{
    int             iInstance;
    PPDMUSBINS      pUsbIns;
    PDMIBASE        IBase;
    PDMIWEBCAMDEV   IWebcamDev;
    PPDMIBASE       pDrvBase;    /* Driver's base interface. */
    PPDMIWEBCAMDRV pWebcamDown; /* Interface exposed by driver. */

    RTCRITSECT lockWebcam;
    RTTIMERLR hTimerStream;
    RTREQQUEUE hRQBackend;
    PPDMTHREAD pThreadBackend; /* PDM thread which will call the backend. */
    RTSEMEVENT hEvtDoneQueue;
    /** Someone is waiting on the done queue. */
    bool       fHaveDoneQueueWaiter;

    UWURBQUEUES urbQueues;

    WEBCAMINTERFACE aInterfaces[USBWEBCAM_NUM_INTERFACES];

    WEBCAMUNIT aUnitsVC[USBWEBCAM_NUM_UNITS_CONTROL];
    WEBCAMUNIT aUnitsVS[USBWEBCAM_NUM_UNITS_STREAMING];

    UWCtrl *apControlsVC[USBWEBCAM_NUM_CONTROLS_VC]; /* Video Control interface */
    UWCtrl *apControlsCT[USBWEBCAM_NUM_CONTROLS_CT]; /* Camera Terminal */
    UWCtrl *apControlsPU[USBWEBCAM_NUM_CONTROLS_PU]; /* Processing Unit */
    UWCtrl *apControlsOT[USBWEBCAM_NUM_CONTROLS_OT]; /* Output Terminal */

    UWCtrl *apControlsVS[USBWEBCAM_NUM_CONTROLS_VS]; /* Video Streaming interface */

    struct WEBCAMCTRLS *pCtrls;

    uint32_t u32MaxPayloadTransferSize;

    uint32_t u32MinFrameInterval; /* Shortest supported frame interval reported to the guest.
                                   * In 100 nanoseconds. 0 means no limit.
                                   */

    VUSBVVSSTATE streamParmsCur; /* The currently negotiated Probe/Commit parameters. */
    VUSBVVSSTATE streamParmsMin;
    VUSBVVSSTATE streamParmsMax;
    VUSBVVSSTATE streamParmsRes;
    VUSBVVSSTATE streamParmsDef;

    bool fConstructed; /* usbWebcamConstruct successfully completed. */

    bool fStreamError; /* Stream error mode. */

    uint8_t fu8FID; /* The payload FID bit, toggles for each frame. */
    uint32_t cbLeft; /* Bytes remaining in the current frame. */
    uint32_t cbPayloadLeft; /* Bytes remaining in the current payload data. */
    uint64_t u64StartFrameMS;
    uint64_t u64LastFrameMS;

    const char *pszStandaloneJPG; /* Path of JPG file for the standalone picture. */

    USBWEBCAMSTATE enmState;

    USBWEBCAMSTREAMSTATUS enmStreamStatus;
    USBWEBCAMSTREAMSTATUS enmStoppedStreamStatus; /* At the moment when VM was suspended. */

    /* Info about the webcam. */
    struct {
        uint64_t idDevice; /* The driver's handle for this device. */
        uint32_t u32Version; /* The driver's protocol version. */
        uint32_t fu32Capabilities;  /* The driver's protocol capabilities. */

        VRDEVIDEOINDEVICEDESC desc;
        VRDEVIDEOINFORMATDESC format;
        VRDEVIDEOINFRAMEDESC *paFrameDesc; /* format.u8NumFrames elements. */
    } webcam;

    PPDMUSBDESCCACHE pDescCache;

    RTLISTANCHOR listFreeFrames;
    RTLISTANCHOR listReadyFrames;
    USBWEBCAMPENDINGFRAME aPendingFrameAlloc[4]; /* Preallocated pending frame structures. */
    USBWEBCAMPENDINGFRAME *pCurrentFrame;
    USBWEBCAMPENDINGFRAME *pLatestFrame;

    RTLISTANCHOR listStatusEvents;
    RTLISTANCHOR listFreeStatusEvents;
    UWSTATUSEVENT aStatusEventsAlloc[16]; /* Preallocated structures. */

    struct {
        /* FPS measurements. */
        uint64_t u64FPSStartMS;
        uint64_t cFPS;

        /* Log some content of the first frame in a new stream. */
        bool fFrameContentLogged;
    } stat;
    struct {
        char *pszId;
        PFNEMULATEDUSB pfnEUSB;
        void *pvEUSB;
    } emulatedUSB;
#ifdef UWPROFILE
    struct {
        /* Profiling. */
        int acLockDelay[100]; /* [i] the number of delays of i mcs. */
        uint64_t cQueue;
        uint64_t cReap;
    } profile;
#endif
} USBWEBCAM, *PUSBWEBCAM;


/*********************************************************************************************************************************
*   External Functions                                                                                                           *
*********************************************************************************************************************************/
int JPGGenCreate(uint16_t u16Width, uint16_t u16Height, int iComponent, uint8_t **ppu8Data, uint32_t *pcbData);


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
typedef uint32_t FNCOPYTOURB(PUSBWEBCAM pThis, PVUSBURB pUrb);
typedef FNCOPYTOURB *PFNCOPYTOURB;
static void usbWebcamQueueComplete(PUSBWEBCAM pThis, URBQUEUE *pQueue, PFNCOPYTOURB pfnCopyToUrb, bool fDataOnly);

/* PFNCOPYTOURB callbacks for usbWebcamQueueComplete. */
static uint32_t usbWebcamEventRead(PUSBWEBCAM pThis, PVUSBURB pUrb);
static uint32_t usbWebcamStream(PUSBWEBCAM pThis, PVUSBURB pUrb);

static uint8_t usbWebcamStreamSetup(USBWEBCAM *pThis);
static uint8_t usbWebcamStreamOff(USBWEBCAM *pThis);
static void usbWebcamStreamOn(USBWEBCAM *pThis);

static void usbWebcamStreamAbort(USBWEBCAM *pThis);

static void usbWebcamUpdateErrorCode(WEBCAMINTERFACE *pInterface, uint8_t bRequestErrorCode);

static void usbWebcamEventCreate(PUSBWEBCAM pThis,
                                 const VUSBVSTATUSINTR *pHdr, uint8_t cbHdr,
                                 const void *pvValue, uint8_t cbValue);

/*
 * Controls.
 */

class UWCtrl
{
    private:
        USBWEBCAM *mpWebcam;        /* The device instance, the control belongs to. */
        uint8_t miCtrlSel;          /* The control selector defined by UVC. */
        uint8_t mfu8Info;           /* The GET_INFO bits UW_F_INFO_*. */
        uint8_t mfu8ValidRequests;  /* Supported requests UW_F_GET_*, UW_F_SET_CUR. */
        bool mfAuto;                /* If the control is disabled due to automatic mode. */
        uint8_t mfu8FetchRequested; /* */
        uint8_t mfu8FetchReceived;  /* */

    public:
        UWCtrl(uint8_t iCtrlSel, uint8_t fu8Info, uint8_t fu8ValidRequests)
            :
            mpWebcam(NULL),
            miCtrlSel(iCtrlSel),
            mfu8Info(fu8Info & (UW_F_INFO_GET | UW_F_INFO_SET | UW_F_INFO_AUTOUPDATE | UW_F_INFO_ASYNC)),
            mfu8ValidRequests(fu8ValidRequests),
            mfAuto(false),
            mfu8FetchRequested(0),
            mfu8FetchReceived(0)

        {};
        virtual ~UWCtrl() {};

        void Init(struct USBWEBCAM *pWebcam) { mpWebcam = pWebcam; }

        USBWEBCAM *Instance(void) { return mpWebcam; }

        uint8_t CtrlSel(void) { return miCtrlSel; }
        void SetAutomaticMode(bool fAuto)
        {
            mfAuto = fAuto;
            if (fAuto)
            {
                mfu8Info |= UW_F_INFO_AUTODISABLED;
            }
            else
            {
                mfu8Info &= ~UW_F_INFO_AUTODISABLED;
            }
        }
        void FetchStart(uint8_t fu8Requests) { mfu8FetchRequested = fu8Requests; }
        void FetchCancel(uint8_t fu8Requests) { mfu8FetchRequested &= ~fu8Requests; }
        void FetchDone(uint8_t fu8Requests) { mfu8FetchRequested |= fu8Requests; }
        bool FetchCompleted(void) { return (mfu8FetchRequested & mfu8FetchReceived) == mfu8FetchRequested; }

        const uint8_t *GetInfo(void) {return &mfu8Info; }

        uint8_t CallControl(const VRDEVIDEOINCTRLHDR *pCtrl, uint32_t cbCtrl);

        virtual bool IsValidRequest(PCVUSBSETUP pSetup);

        virtual const char *CtrlName(void) = 0;

        virtual uint16_t CtrlLength(void) = 0; /* Size of the control parameters. */
        virtual uint8_t SetCur(const uint8_t *pbValue, uint16_t cbValue) = 0;
        virtual const uint8_t *GetCur(void) = 0;
        virtual const uint8_t *GetMin(void) = 0;
        virtual const uint8_t *GetMax(void) = 0;
        virtual const uint8_t *GetRes(void) = 0;
        virtual const uint8_t *GetDef(void) = 0;

        virtual void Fetch(void) = 0;

        virtual void NotifySet(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)  { RT_NOREF3(pHdr, pbParm, cbParm); return; }
        virtual void NotifyCur(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)  { RT_NOREF3(pHdr, pbParm, cbParm); return; }
        virtual void NotifyMin(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)  { RT_NOREF3(pHdr, pbParm, cbParm); return; }
        virtual void NotifyMax(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)  { RT_NOREF3(pHdr, pbParm, cbParm); return; }
        virtual void NotifyRes(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)  { RT_NOREF3(pHdr, pbParm, cbParm); return; }
        virtual void NotifyInfo(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm) { RT_NOREF3(pHdr, pbParm, cbParm); return; }
        virtual void NotifyDef(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)  { RT_NOREF3(pHdr, pbParm, cbParm); return; }
};

class UWCtrlS16: public UWCtrl
{
    private:
        int16_t mCur, mDef, mMin, mMax, mRes;
        int16_t mWebcamMin, mWebcamMax, mWebcamRes, mWebcamMapped;

    public:
        UWCtrlS16(uint8_t iCtrlSel, uint8_t fu8Info, uint8_t fu8ValidRequests,
                  int16_t aMin, int16_t aMax, int16_t aRes, int16_t aDef)
            : UWCtrl(iCtrlSel, fu8Info, fu8ValidRequests),
              mCur(aDef), mDef(aDef), mMin(aMin), mMax(aMax), mRes(aRes),
              mWebcamMin(0), mWebcamMax(0), mWebcamRes(0), mWebcamMapped(0)
        {}

        virtual uint16_t CtrlLength(void) { return sizeof(mCur); }

        virtual uint8_t SetCur(const uint8_t *pbValue, uint16_t cbValue)
        {
            /** @todo Verify range and resolution. */
            RT_NOREF1(cbValue);
            mCur = *(uint16_t *)pbValue;
            return VUSBV_INF_SUCCESS;
        }
        virtual const uint8_t *GetCur(void) { return (uint8_t *)&mCur; }
        virtual const uint8_t *GetMin(void) { return (uint8_t *)&mMin; }
        virtual const uint8_t *GetMax(void) { return (uint8_t *)&mMax; }
        virtual const uint8_t *GetRes(void) { return (uint8_t *)&mRes; }
        virtual const uint8_t *GetDef(void) { return (uint8_t *)&mDef; }

        virtual const uint8_t *QueryCurMapped(void)
        {
            mWebcamMapped = ((mWebcamMax - mWebcamMin) * mCur) / (mMax - mMin);
            if (mWebcamRes != 0)
            {
                mWebcamMapped = (mWebcamMapped / mWebcamRes) * mWebcamRes;
            }
            mWebcamMapped += mWebcamMin;
            return (uint8_t *)&mWebcamMapped;
        }

        virtual void NotifySet(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm);
        virtual void NotifyCur(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm);
        virtual void NotifyMin(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm);
        virtual void NotifyMax(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm);
        virtual void NotifyRes(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm);
        virtual void NotifyInfo(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm);
        virtual void NotifyDef(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm);
};

/* virtual */ void UWCtrlS16::NotifySet(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
{
    RT_NOREF2(pHdr, pbParm);
    AssertReturnVoid(cbParm >= sizeof(uint16_t));
}

/* virtual */ void UWCtrlS16::NotifyCur(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
{
    RT_NOREF2(pHdr, pbParm);
    AssertReturnVoid(cbParm >= sizeof(uint16_t));
}

/* virtual */ void UWCtrlS16::NotifyMin(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
{
    RT_NOREF1(pHdr);
    AssertReturnVoid(cbParm >= sizeof(uint16_t));
    mWebcamMin = *(uint16_t *)pbParm;
    FetchDone(UW_F_GET_MIN);
}

/* virtual */ void UWCtrlS16::NotifyMax(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
{
    RT_NOREF1(pHdr);
    AssertReturnVoid(cbParm >= sizeof(uint16_t));
    mWebcamMax = *(uint16_t *)pbParm;
    FetchDone(UW_F_GET_MAX);
}

/* virtual */ void UWCtrlS16::NotifyRes(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
{
    RT_NOREF1(pHdr);
    AssertReturnVoid(cbParm >= sizeof(uint16_t));
    mWebcamRes = *(uint16_t *)pbParm;
    FetchDone(UW_F_GET_RES);
}

/* virtual */ void UWCtrlS16::NotifyInfo(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
{
    RT_NOREF2(pHdr, pbParm);
    AssertReturnVoid(cbParm >= sizeof(uint16_t));
}

/* virtual */ void UWCtrlS16::NotifyDef(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
{
    RT_NOREF2(pHdr, pbParm);
    AssertReturnVoid(cbParm >= sizeof(uint16_t));
}

class UWCtrl_CONTROL_UNDEFINED: public UWCtrl
{
    public:
        UWCtrl_CONTROL_UNDEFINED()
            :
            UWCtrl(0, 0, 0)
        {};

        virtual const char *CtrlName(void) { return "CONTROL_UNDEFINED"; }
        virtual uint16_t CtrlLength(void) { return 0; };

        virtual uint8_t SetCur(const uint8_t *pbValue, uint16_t cbValue) { RT_NOREF2(pbValue, cbValue); return VUSBV_ERR_INVALID_REQUEST; }
        virtual const uint8_t *GetCur(void) { return NULL; }
        virtual const uint8_t *GetMin(void) { return NULL; }
        virtual const uint8_t *GetMax(void) { return NULL; }
        virtual const uint8_t *GetRes(void) { return NULL; }
        virtual const uint8_t *GetDef(void) { return NULL; }

        virtual void Fetch(void) { return; }
};

class UWCtrl_VC_REQUEST_ERROR_CODE_CONTROL: public UWCtrl
{
    private:
        uint8_t mCur;

    public:
        UWCtrl_VC_REQUEST_ERROR_CODE_CONTROL()
            :
            UWCtrl(VUSB_V_VC_REQUEST_ERROR_CODE_CONTROL, UW_F_INFO_GET, UW_F_GET_CUR | UW_F_GET_INFO),
            mCur(0)
        {};

        virtual const char *CtrlName(void) { return "VC_REQUEST_ERROR_CODE_CONTROL"; }
        virtual uint16_t CtrlLength(void) { return sizeof(mCur); };

        virtual uint8_t SetCur(const uint8_t *pbValue, uint16_t cbValue)
        {
            RT_NOREF1(cbValue);
            mCur = *(uint8_t *)pbValue;
            return VUSBV_INF_SUCCESS;
        }
        virtual const uint8_t *GetCur(void) { return (uint8_t *)&mCur; }
        virtual const uint8_t *GetMin(void) { return NULL; }
        virtual const uint8_t *GetMax(void) { return NULL; }
        virtual const uint8_t *GetRes(void) { return NULL; }
        virtual const uint8_t *GetDef(void) { return NULL; }

        virtual void Fetch(void) { return; }
};

#define UW_REQ_INIT(req, sel, reqtype)  do {                               \
       (req).hdr.u16ControlSelector = (sel);                               \
       (req).hdr.u16RequestType     = (reqtype);                           \
       (req).hdr.u16ParmSize        = sizeof(req) - sizeof((req).hdr);     \
       (req).hdr.u8Flags            = 0;                                   \
       (req).hdr.u8Status           = VRDE_VIDEOIN_CTRLHDR_STATUS_SUCCESS; \
    } while(0)


class UWCtrl_PU_BRIGHTNESS_CONTROL: public UWCtrlS16
{
    private:
    public:
        UWCtrl_PU_BRIGHTNESS_CONTROL()
            :
            UWCtrlS16(VUSB_V_PU_BRIGHTNESS_CONTROL, UW_F_INFO_GET | UW_F_INFO_SET,
                      UW_F_SET_CUR | UW_F_GET_CUR | UW_F_GET_MIN | UW_F_GET_MAX | UW_F_GET_RES | UW_F_GET_INFO | UW_F_GET_DEF,
                      0, 100, 1, 50)
        {};

        virtual const char *CtrlName(void) { return "PU_BRIGHTNESS_CONTROL"; }

        virtual uint8_t SetCur(const uint8_t *pbValue, uint16_t cbValue)
        {
            uint8_t u8Result = UWCtrlS16::SetCur(pbValue, cbValue);
            if (   u8Result == VUSBV_INF_SUCCESS
                && USBWEBCAM_IS_PROXY(Instance()))
            {
                VRDEVIDEOINCTRL_PU_BRIGHTNESS req;
                UW_REQ_INIT(req, VRDE_VIDEOIN_CTRLSEL_PU_BRIGHTNESS, VRDE_VIDEOIN_CTRLREQ_SET_CUR);
                req.u16Brightness = *(uint16_t *)QueryCurMapped();

                /*u8Result = */ CallControl(&req.hdr, sizeof(req));
            }
            /* Discard the backend result if backend does not support the request,
             * as if setting brightness is always successful.
             *
             * The emulated webcam hardcodes support for the brightness control,
             * regardless of backend capabilities. Returning success here
             * avoids unnecessary guest confusion, since no backend supports
             * brightness currently.
             */
            return VUSBV_INF_SUCCESS;
        }

        virtual void Fetch(void);
};

/* virtual */ void UWCtrl_PU_BRIGHTNESS_CONTROL::Fetch(void)
{
    VRDEVIDEOINCTRL_PU_BRIGHTNESS req;

    FetchStart(UW_F_GET_MIN | UW_F_GET_MAX | UW_F_GET_RES);

    UW_REQ_INIT(req, VRDE_VIDEOIN_CTRLSEL_PU_BRIGHTNESS, VRDE_VIDEOIN_CTRLREQ_GET_MIN);
    req.u16Brightness = 0;
    uint8_t u8Result = CallControl(&req.hdr, sizeof(req));
    if (u8Result != VUSBV_INF_SUCCESS)
    {
        FetchCancel(UW_F_GET_MIN);
    }

    UW_REQ_INIT(req, VRDE_VIDEOIN_CTRLSEL_PU_BRIGHTNESS, VRDE_VIDEOIN_CTRLREQ_GET_MAX);
    req.u16Brightness = 0;
    u8Result = CallControl(&req.hdr, sizeof(req));
    if (u8Result != VUSBV_INF_SUCCESS)
    {
        FetchCancel(UW_F_GET_MAX);
    }

    UW_REQ_INIT(req, VRDE_VIDEOIN_CTRLSEL_PU_BRIGHTNESS, VRDE_VIDEOIN_CTRLREQ_GET_RES);
    req.u16Brightness = 0;
    u8Result = CallControl(&req.hdr, sizeof(req));
    if (u8Result != VUSBV_INF_SUCCESS)
    {
        FetchCancel(UW_F_GET_RES);
    }

    return;
}

/*
 *  Probe/Commit
 *
 * @todo The first implementation accepts anything and tries to map it to the webcam.
 */
static void usbWebcamStreamParmsInit(USBWEBCAM *pWebcam)
{
    /* UVC 1.1: "Unsupported fields shall be set to zero by the host and the device."
     *
     * Note: a real webcam reports dwMaxVideoFrameSize as w*h*2
     */
    pWebcam->streamParmsDef.bmHint                   = 0;
    pWebcam->streamParmsDef.bFormatIndex             = 1;
    pWebcam->streamParmsDef.bFrameIndex              = 1;
    pWebcam->streamParmsDef.dwFrameInterval          = 666666;
    pWebcam->streamParmsDef.wKeyFrameRate            = 1;
    pWebcam->streamParmsDef.wPFrameRate              = 0;
    pWebcam->streamParmsDef.wCompQuality             = 5000;
    pWebcam->streamParmsDef.wCompWindowSize          = 10;
    pWebcam->streamParmsDef.wDelay                   = 16;
    pWebcam->streamParmsDef.dwMaxVideoFrameSize      = 640*480*4;
    pWebcam->streamParmsDef.dwMaxPayloadTransferSize = pWebcam->u32MaxPayloadTransferSize;
    pWebcam->streamParmsDef.dwClockFrequency         = 1000000; /* 1MHz */
    pWebcam->streamParmsDef.bmFramingInfo            = 0; /* Ignored for MJPEG */
    pWebcam->streamParmsDef.bPreferedVersion         = 1;
    pWebcam->streamParmsDef.bMinVersion              = 1;
    pWebcam->streamParmsDef.bMaxVersion              = 1;

    pWebcam->streamParmsMin.bmHint                   = 0;
    pWebcam->streamParmsMin.bFormatIndex             = 1;
    pWebcam->streamParmsMin.bFrameIndex              = 1;
    pWebcam->streamParmsMin.dwFrameInterval          = 333333;
    pWebcam->streamParmsMin.wKeyFrameRate            = 1;
    pWebcam->streamParmsMin.wPFrameRate              = 0;
    pWebcam->streamParmsMin.wCompQuality             = 0;
    pWebcam->streamParmsMin.wCompWindowSize          = 10;
    pWebcam->streamParmsMin.wDelay                   = 1;
    pWebcam->streamParmsMin.dwMaxVideoFrameSize      = 160*120*4;
    pWebcam->streamParmsMin.dwMaxPayloadTransferSize = pWebcam->u32MaxPayloadTransferSize;
    pWebcam->streamParmsMin.dwClockFrequency         = 1000000; /* 1MHz */
    pWebcam->streamParmsMin.bmFramingInfo            = 0; /* Ignored for MJPEG */
    pWebcam->streamParmsMin.bPreferedVersion         = 1;
    pWebcam->streamParmsMin.bMinVersion              = 1;
    pWebcam->streamParmsMin.bMaxVersion              = 1;

    pWebcam->streamParmsMax.bmHint                   = 0;
    pWebcam->streamParmsMax.bFormatIndex             = 1;
    pWebcam->streamParmsMax.bFrameIndex              = usbWebcamDescriptorsFrameCount(pWebcam->pDescCache);
    pWebcam->streamParmsMax.dwFrameInterval          = 2000000;
    pWebcam->streamParmsMax.wKeyFrameRate            = 1;
    pWebcam->streamParmsMax.wPFrameRate              = 0;
    pWebcam->streamParmsMax.wCompQuality             = 10000;
    pWebcam->streamParmsMax.wCompWindowSize          = 10;
    pWebcam->streamParmsMax.wDelay                   = 2000;
    pWebcam->streamParmsMax.dwMaxVideoFrameSize      = 1280*720*4;
    pWebcam->streamParmsMax.dwMaxPayloadTransferSize = pWebcam->u32MaxPayloadTransferSize;
    pWebcam->streamParmsMax.dwClockFrequency         = 1000000; /* 1MHz */
    pWebcam->streamParmsMax.bmFramingInfo            = 0; /* Ignored for MJPEG */
    pWebcam->streamParmsMax.bPreferedVersion         = 1;
    pWebcam->streamParmsMax.bMinVersion              = 1;
    pWebcam->streamParmsMax.bMaxVersion              = 1;

    /** @todo */
    pWebcam->streamParmsRes.bmHint                   = 0;
    pWebcam->streamParmsRes.bFormatIndex             = 1;
    pWebcam->streamParmsRes.bFrameIndex              = 1;
    pWebcam->streamParmsRes.dwFrameInterval          = 1;
    pWebcam->streamParmsRes.wKeyFrameRate            = 1;
    pWebcam->streamParmsRes.wPFrameRate              = 0;
    pWebcam->streamParmsRes.wCompQuality             = 100;
    pWebcam->streamParmsRes.wCompWindowSize          = 10;
    pWebcam->streamParmsRes.wDelay                   = 1;
    pWebcam->streamParmsRes.dwMaxVideoFrameSize      = 1;
    pWebcam->streamParmsRes.dwMaxPayloadTransferSize = 0;
    pWebcam->streamParmsRes.dwClockFrequency         = 1;
    pWebcam->streamParmsRes.bmFramingInfo            = 0;
    pWebcam->streamParmsRes.bPreferedVersion         = 1;
    pWebcam->streamParmsRes.bMinVersion              = 1;
    pWebcam->streamParmsRes.bMaxVersion              = 1;

    pWebcam->streamParmsCur = pWebcam->streamParmsDef;
}

static void usbWebcamLogProbeCommit(const char *pszCtrl, const VUSBVVSSTATE *pStreamParms)
{
    UWLOG(("%s:\n"
           "    bmHint                   0x%04X\n"
           "    bFormatIndex             %d\n"
           "    bFrameIndex              %d\n"
           "    dwFrameInterval          %d\n"
           "    wKeyFrameRate            %d\n"
           "    wPFrameRate              %d\n"
           "    wCompQuality             %d\n"
           "    wCompWindowSize          %d\n"
           "    wDelay                   %d\n"
           "    dwMaxVideoFrameSize      %d\n"
           "    dwMaxPayloadTransferSize %d\n"
           "    dwClockFrequency         %d\n"
           "    bmFramingInfo            0x%02x\n"
           "    bPreferedVersion         %d\n"
           "    bMinVersion              %d\n"
           "    bMaxVersion              %d\n",
           pszCtrl,
           pStreamParms->bmHint,
           pStreamParms->bFormatIndex,
           pStreamParms->bFrameIndex,
           pStreamParms->dwFrameInterval,
           pStreamParms->wKeyFrameRate,
           pStreamParms->wPFrameRate,
           pStreamParms->wCompQuality,
           pStreamParms->wCompWindowSize,
           pStreamParms->wDelay,
           pStreamParms->dwMaxVideoFrameSize,
           pStreamParms->dwMaxPayloadTransferSize,
           pStreamParms->dwClockFrequency,
           pStreamParms->bmFramingInfo,
           pStreamParms->bPreferedVersion,
           pStreamParms->bMinVersion,
           pStreamParms->bMaxVersion));
}


class UWCtrl_VS_PROBECOMMIT_COMMON: public UWCtrl
{
    private:
    public:
        UWCtrl_VS_PROBECOMMIT_COMMON(uint8_t iCtrlSel, uint8_t fu8Info, uint8_t fu8ValidRequests)
            :
            UWCtrl(iCtrlSel, fu8Info, fu8ValidRequests)
        {};

        virtual uint16_t CtrlLength(void) { return sizeof(VUSBVVSSTATE); };
        virtual const uint8_t *GetCur(void) { return (uint8_t *)&Instance()->streamParmsCur; }
        virtual const uint8_t *GetMin(void) { return (uint8_t *)&Instance()->streamParmsMin; }
        virtual const uint8_t *GetMax(void) { return (uint8_t *)&Instance()->streamParmsMax; }
        virtual const uint8_t *GetRes(void) { return (uint8_t *)&Instance()->streamParmsRes; }
        virtual const uint8_t *GetDef(void) { return (uint8_t *)&Instance()->streamParmsDef; }

        /* These controls do not depend on the webcam. */
        virtual void Fetch(void) { return; }
};

class UWCtrl_VS_PROBE_CONTROL: public UWCtrl_VS_PROBECOMMIT_COMMON
{
    private:
    public:
        UWCtrl_VS_PROBE_CONTROL()
            :
            UWCtrl_VS_PROBECOMMIT_COMMON(VUSB_V_VS_PROBE_CONTROL, UW_F_INFO_GET | UW_F_INFO_SET,
                                         UW_F_SET_CUR | UW_F_GET_CUR | UW_F_GET_MIN | UW_F_GET_MAX |
                                         UW_F_GET_RES | UW_F_GET_INFO | UW_F_GET_DEF | UW_F_GET_LEN)
        {};

        virtual const char *CtrlName(void) { return "VS_PROBE_CONTROL"; }

        virtual uint8_t SetCur(const uint8_t *pbValue, uint16_t cbValue);
        virtual void NotifySet(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
        { RT_NOREF3(pHdr, pbParm, cbParm); return; }
};

class UWCtrl_VS_COMMIT_CONTROL: public UWCtrl_VS_PROBECOMMIT_COMMON
{
    private:
    public:
        UWCtrl_VS_COMMIT_CONTROL()
            :
            UWCtrl_VS_PROBECOMMIT_COMMON(VUSB_V_VS_COMMIT_CONTROL, UW_F_INFO_GET | UW_F_INFO_SET,
                                         UW_F_SET_CUR | UW_F_GET_CUR | UW_F_GET_INFO | UW_F_GET_LEN)
        {};

        virtual const char *CtrlName(void) { return "VS_COMMIT_CONTROL"; }

        virtual uint8_t SetCur(const uint8_t *pbValue, uint16_t cbValue);
        virtual void NotifySet(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm);
};


/*
 * Given the list of frames supported by the remote webcam and list
 * of frames which can be requested by the guest, get dimensions of
 * a supported frame, which is the best fit to the requested frame.
 *
 * The supported frame must be either equal to the requested frame
 * or must be the largest of the supported frames, which are smaller
 * than the requested frame.
 *
 * If the requested frame is smaller than smallest supported frame,
 * then return the index of the requested frame which is larger of equal to
 * the smallest supported frame.
 *
 * Note: in the current implementation the pbSuggestedFrameIndex is informative
 *       and is used only for logging.
 *
 * @return VINF_SUCCESS if a frame was found and pu16Width, pu16Height contain the dimensions.
 *         VINF_NOT_SUPPORTED if frame was not found, pbSuggestedFrameIndex is what should be requested.
 *         VERR_* if frame was not found and it is not possible to use the requested frame.
 */
static int usbWebcamFindFrameSize(USBWEBCAM *pThis,
                                  uint16_t *pu16Width, uint16_t *pu16Height,
                                  uint8_t bFrameIndex,
                                  VRDEVIDEOINFRAMEDESC *paFrames,
                                  uint8_t cFrames,
                                  uint8_t *pbSuggestedFrameIndex)
{
    int i;

    const VUSBVDESCVSMJPEGFRAME *pDescFrame = usbWebcamDescriptorsFindFrame(pThis->pDescCache, bFrameIndex);
    if (!pDescFrame)
    {
        UWLOG(("Invalid frame index %d!!!\n", bFrameIndex));
        return VERR_INVALID_PARAMETER;
    }

    if (paFrames == NULL)
    {
        *pu16Width = pDescFrame->wWidth;
        *pu16Height = pDescFrame->wHeight;
        UWLOG(("Selected %dx%d\n", pDescFrame->wWidth, pDescFrame->wHeight));
        return VINF_SUCCESS;
    }

    /* The pDescFrame points to the requested frame. Search for a similar supported frame.
     * Lookup a frame with the same dimensions or with lesser dimensions and a similar aspect ratio.
     */
    VRDEVIDEOINFRAMEDESC *pBest = &paFrames[0];
    for (i = 0; i < cFrames; i++)
    {
        VRDEVIDEOINFRAMEDESC *pCur = &paFrames[i];

        if (   pCur->u16Width == pDescFrame->wWidth
            && pCur->u16Height == pDescFrame->wHeight)
        {
            /* Exact match. */
            pBest = pCur;
            break;
        }

        if (   pCur->u16Width <= pDescFrame->wWidth
            && pCur->u16Height <= pDescFrame->wHeight)
        {
            if (   pCur->u16Width > pBest->u16Width
                && pCur->u16Height > pBest->u16Height)
            {
                /* Partial match. Continue as there may be a better match. */
                pBest = pCur;
            }
        }
    }

    if (   pBest->u16Width <= pDescFrame->wWidth
        && pBest->u16Height <= pDescFrame->wHeight)
    {
        *pu16Width = pBest->u16Width;
        *pu16Height = pBest->u16Height;
        UWLOG(("Requested %dx%d, selected %dx%d\n",
               pDescFrame->wWidth, pDescFrame->wHeight,
               *pu16Width, *pu16Height));
        return VINF_SUCCESS;
    }

    /* The requested frame is smaller than all supported frames.
     * Find the smallest supported frame and then find the requested frame that is greater.
     */
    VRDEVIDEOINFRAMEDESC *pMin = &paFrames[0];
    for (i = 0; i < cFrames; i++)
    {
        VRDEVIDEOINFRAMEDESC *pCur = &paFrames[i];
        if (   pCur->u16Width < pMin->u16Width
            && pCur->u16Height < pMin->u16Height)
        {
            pMin = pCur;
        }
    }

    UWLOG(("Requested %dx%d is smaller than smallest supported %dx%d\n",
           pDescFrame->wWidth, pDescFrame->wHeight,
           pMin->u16Width, pMin->u16Height));

    const VUSBVDESCVSMJPEGFRAME *pSuggestedFrame = usbWebcamDescriptorsSuggestFrame(pThis->pDescCache,
                                                                                    pMin->u16Width, pMin->u16Height);
    if (pSuggestedFrame)
    {
        UWLOG(("Must use frame %d %dx%d\n",
                pSuggestedFrame->bFrameIndex, pSuggestedFrame->wWidth, pSuggestedFrame->wHeight));
        if (pbSuggestedFrameIndex)
        {
            *pbSuggestedFrameIndex = pSuggestedFrame->bFrameIndex;
        }
        return VINF_NOT_SUPPORTED;
    }

    /* No supported frames can be used for the requested frame. */
    UWLOG(("No supported for %d, %dx%d\n",
           bFrameIndex, pDescFrame->wWidth, pDescFrame->wHeight));

    return VERR_NOT_SUPPORTED;
}

/* virtual */ uint8_t UWCtrl_VS_PROBE_CONTROL::SetCur(const uint8_t *pbValue, uint16_t cbValue)
{
    RT_NOREF1(cbValue);
    /* UVC 1.1: "This request shall stall in case of negotiation failure." */
    const VUSBVVSSTATE *pStreamParms = (VUSBVVSSTATE *)pbValue;

    usbWebcamLogProbeCommit("PROBE", pStreamParms);

    /* Adopt applicable parameters. */
    Instance()->streamParmsCur.bFrameIndex = pStreamParms->bFrameIndex;
    if (pStreamParms->dwFrameInterval)
    {
        Instance()->streamParmsCur.dwFrameInterval = pStreamParms->dwFrameInterval;
    }
    if (pStreamParms->wCompQuality)
    {
        Instance()->streamParmsCur.wCompQuality = pStreamParms->wCompQuality;
    }

    /* Check that the remote webcam supports the frame size. */
    VRDEVIDEOINFORMATDESC *pFormat = &Instance()->webcam.format;
    VRDEVIDEOINFRAMEDESC *paFrameDesc = Instance()->webcam.paFrameDesc;
    uint16_t u16Width = 0;
    uint16_t u16Height = 0;
    uint8_t bSuggestedFrameIndex = 0;
    int rc = usbWebcamFindFrameSize(Instance(), &u16Width, &u16Height,
                                    pStreamParms->bFrameIndex, paFrameDesc, pFormat->u8NumFrames,
                                    &bSuggestedFrameIndex);

    if (RT_FAILURE(rc))
    {
        UWLOG(("Failed\n"));
        return VUSBV_ERR_INVALID_REQUEST;
    }

    if (rc != VINF_SUCCESS)
    {
        UWLOG(("Suggested %d\n", bSuggestedFrameIndex));
        return VUSBV_ERR_INVALID_REQUEST;
    }

    return 0;
}

/* virtual */ uint8_t UWCtrl_VS_COMMIT_CONTROL::SetCur(const uint8_t *pbValue, uint16_t cbValue)
{
    RT_NOREF1(cbValue);
    const VUSBVVSSTATE *pStreamParms = (VUSBVVSSTATE *)pbValue;

    usbWebcamLogProbeCommit("COMMIT", pStreamParms);

    /* Reset FPS measurements. */
    Instance()->stat.u64FPSStartMS = 0;
    Instance()->stat.cFPS = 0;

    /* UVC 1.1 FAQ: "The commit SET_CUR requests must still validate the required parameters."
     * A probe/commit request will fail if:
     * * bFormatIndex and/or bFrameIndex have invalid values
     * * A given stream specified by bFormatIndex and bFrameIndex cannot be supported
     *   with a specified dwMaxPayloadTransferSize.
     * All the other fields will be negotiated.
     */

    uint8_t u8Result = usbWebcamStreamSetup(Instance());

    UWLOG(("VS_SETUP request: u8Result %d\n",
           u8Result));
    return u8Result;
}

/* virtual */ void UWCtrl_VS_COMMIT_CONTROL::NotifySet(const VRDEVIDEOINCTRLHDR *pHdr, const uint8_t *pbParm, uint32_t cbParm)
{
    RT_NOREF2(pbParm, cbParm);
    /** @todo u16Delay and u32ClockFrequency and switch to Streaming mode is success. */
    const VRDEVIDEOINCTRL_VS_SETUP *p = (VRDEVIDEOINCTRL_VS_SETUP *)pHdr;

    if (pHdr->u16ControlSelector == VRDE_VIDEOIN_CTRLSEL_VS_SETUP)
    {
        UWLOG(("VS_SETUP response: delay %d, freq %d\n",
               p->u16Delay, p->u32ClockFrequency));

        Instance()->enmStreamStatus = UW_STREAM_STATUS_SETUP;

        if (Instance()->urbQueues.BulkIn.pUrbHead)
        {
            /* The guest already wants the frame data. Enable streaming. */
            usbWebcamStreamOn(Instance());
        }
    }
    else if (pHdr->u16ControlSelector == VRDE_VIDEOIN_CTRLSEL_VS_OFF)
    {
        UWLOG(("VS_OFF response\n"));
    }
    else if (pHdr->u16ControlSelector == VRDE_VIDEOIN_CTRLSEL_VS_ON)
    {
        UWLOG(("VS_ON response\n"));
    }

    return;
}

class UWCtrl_VS_STREAM_ERROR_CODE_CONTROL: public UWCtrl
{
    private:
        uint8_t mCur;

    public:
        UWCtrl_VS_STREAM_ERROR_CODE_CONTROL()
            :
            UWCtrl(VUSB_V_VS_STREAM_ERROR_CODE_CONTROL, UW_F_INFO_GET, UW_F_GET_CUR | UW_F_GET_INFO),
            mCur(0)
        {};

        virtual const char *CtrlName(void) { return "VS_STREAM_ERROR_CODE_CONTROL"; }
        virtual uint16_t CtrlLength(void) { return sizeof(mCur); };

        virtual uint8_t SetCur(const uint8_t *pbValue, uint16_t cbValue)
        {
            RT_NOREF1(cbValue);
            mCur = *(uint8_t *)pbValue;
            return VUSBV_INF_SUCCESS;
        }
        virtual const uint8_t *GetCur(void) { return (uint8_t *)&mCur; }
        virtual const uint8_t *GetMin(void) { return NULL; }
        virtual const uint8_t *GetMax(void) { return NULL; }
        virtual const uint8_t *GetRes(void) { return NULL; }
        virtual const uint8_t *GetDef(void) { return NULL; }

        virtual void Fetch(void) { return; }
};

/* virtual */ bool UWCtrl::IsValidRequest(PCVUSBSETUP pSetup)
{
    /* Check bRequest and bmRequestType direction. */
    uint8_t fu8Req = 0;
    switch (pSetup->bRequest)
    {
        case VUSB_V_REQ_SET_CUR: fu8Req = UW_F_SET_CUR; break;
        case VUSB_V_REQ_GET_MIN: fu8Req = UW_F_GET_MIN; break;
        case VUSB_V_REQ_GET_MAX: fu8Req = UW_F_GET_MAX; break;
        case VUSB_V_REQ_GET_RES: fu8Req = UW_F_GET_RES; break;
        case VUSB_V_REQ_GET_LEN: fu8Req = UW_F_GET_LEN; break;
        case VUSB_V_REQ_GET_INFO: fu8Req = UW_F_GET_INFO; break;
        case VUSB_V_REQ_GET_DEF: fu8Req = UW_F_GET_DEF; break;
        case VUSB_V_REQ_GET_CUR: fu8Req = UW_F_GET_CUR; break;
        default: break;
    }

    if ((fu8Req & mfu8ValidRequests) == 0)
    {
        return false;
    }

    if ((pSetup->bmRequestType & VUSB_DIR_MASK) == VUSB_DIR_TO_HOST)
    {
        if (pSetup->bRequest == VUSB_V_REQ_SET_CUR)
        {
            return false;
        }
    }
    else
    {
        if (pSetup->bRequest != VUSB_V_REQ_SET_CUR)
        {
            return false;
        }

        if (mfu8Info & UW_F_INFO_AUTODISABLED)
        {
            return false;
        }
    }

    return true;
}

static DECLCALLBACK(void) usbWebcamCallControl(UWCtrl *pThis, PUSBWEBCAM pWebcam,
                                               VRDEVIDEOINCTRLHDR *pCtrl, uint32_t cbCtrl)
{
    UWLOG(("\n"));
    void *pvUser = pThis;
    pWebcam->pWebcamDown->pfnControl(pWebcam->pWebcamDown,
                                     pvUser,
                                     pWebcam->webcam.idDevice,
                                     pCtrl,
                                     cbCtrl);
    RTMemFree(pCtrl);
}

uint8_t UWCtrl::CallControl(const VRDEVIDEOINCTRLHDR *pCtrl, uint32_t cbCtrl)
{
    UWLOG(("CTRL: [%s] %d req 0x%02X, wLength %d\n",
            CtrlName(), CtrlSel(), pCtrl->u16RequestType, pCtrl->u16ParmSize));

    int rc;
    void *pvCtrl = RTMemDup(pCtrl, cbCtrl);
    if (pvCtrl)
    {
        rc = RTReqQueueCallEx(Instance()->hRQBackend, NULL, 0, RTREQFLAGS_VOID | RTREQFLAGS_NO_WAIT,
                              (PFNRT)usbWebcamCallControl, 4, this, Instance(), pvCtrl, cbCtrl);
        if (RT_FAILURE(rc))
        {
            RTMemFree(pvCtrl);
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
    {
        return VUSBV_ERR_INVALID_REQUEST;
    }

    return VUSBV_INF_SUCCESS;
}


/* Allocated using "new" to properly construct the members. */
typedef struct WEBCAMCTRLS
{
    UWCtrl_CONTROL_UNDEFINED     ctrl_CONTROL_UNDEFINED;

    UWCtrl_VC_REQUEST_ERROR_CODE_CONTROL ctrl_VC_REQUEST_ERROR_CODE_CONTROL;

    UWCtrl_PU_BRIGHTNESS_CONTROL ctrl_PU_BRIGHTNESS_CONTROL;

    UWCtrl_VS_PROBE_CONTROL  ctrl_VS_PROBE_CONTROL;
    UWCtrl_VS_COMMIT_CONTROL ctrl_VS_COMMIT_CONTROL;
    UWCtrl_VS_STREAM_ERROR_CODE_CONTROL ctrl_VS_STREAM_ERROR_CODE_CONTROL;

    UWCtrl *VRDEControlSelToCtrl(uint16_t u16ControlSelector);
} WEBCAMCTRLS;


UWCtrl *WEBCAMCTRLS::VRDEControlSelToCtrl(uint16_t u16ControlSelector)
{
    UWCtrl *pCtrl = NULL;

    switch(u16ControlSelector)
    {
        case VRDE_VIDEOIN_CTRLSEL_PU_BRIGHTNESS: pCtrl = &ctrl_PU_BRIGHTNESS_CONTROL; break;
        case VRDE_VIDEOIN_CTRLSEL_VS_SETUP:      pCtrl = &ctrl_VS_COMMIT_CONTROL; break;
        case VRDE_VIDEOIN_CTRLSEL_VS_OFF:        pCtrl = &ctrl_VS_COMMIT_CONTROL; break;
        case VRDE_VIDEOIN_CTRLSEL_VS_ON:         pCtrl = &ctrl_VS_COMMIT_CONTROL; break;
        default: break;
    }

    return pCtrl;
}

static int usbWebcamJPGCreate(USBWEBCAM *pThis, uint16_t u16Width, uint16_t u16Height,
                              uint8_t **ppu8Data, uint32_t *pcbData)
{
    int rc = VINF_SUCCESS;

    if (pThis->pszStandaloneJPG)
    {
        uint8_t *pu8Data = NULL;
        uint32_t cbData = 0;

        RTFILE h = NIL_RTFILE;
        rc = RTFileOpen(&h, pThis->pszStandaloneJPG, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            uint64_t cbFile;
            rc = RTFileQuerySize(h, &cbFile);
            if (RT_SUCCESS(rc))
            {
                if (cbFile > 0 && cbFile < 16 * _1M)
                {
                    pu8Data = (uint8_t *)RTMemAlloc(cbFile);
                    if (pu8Data != NULL)
                    {
                        cbData = (uint32_t)cbFile;

                        rc = RTFileRead(h, pu8Data, cbData, NULL);

                        if (RT_FAILURE(rc))
                        {
                            RTMemFree(pu8Data);
                        }
                    }
                    else
                    {
                        rc = VERR_NO_MEMORY;
                    }
                }
                else
                {
                    rc = VERR_TOO_MUCH_DATA;
                }
            }

            RTFileClose(h);

            if (RT_SUCCESS(rc))
            {
                *ppu8Data = pu8Data;
                *pcbData = cbData;
            }
        }
    }
    else
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_FAILURE(rc))
    {
        /* By default use a blue jpg. */
        rc = JPGGenCreate(u16Width, u16Height, 0, ppu8Data, pcbData);
    }

    return rc;
}

static void usbWebcamJPGFree(uint8_t *pu8Data)
{
    RTMemFree(pu8Data);
}

static void usbWebcamJPGFrameLog(const uint8_t *pu8Data, uint32_t cbData)
{
    if (cbData == 0)
    {
        return;
    }

    const uint8_t *pu8Src = pu8Data;
    const uint8_t *pu8End = pu8Data + cbData;

    UWLOG(("FRAME: data %p, %d bytes\n", pu8Data, cbData));

    while ((uintptr_t)pu8Src < (uintptr_t)pu8End)
    {
        const uint8_t *pu8Marker = pu8Src;
        uint32_t cbMarker = 2;

        if (   (uintptr_t)pu8Marker + cbMarker > (uintptr_t)pu8End
            || pu8Marker[0] != 0xFF)
        {
            break;
        }

        if (pu8Marker[1] == 0xD8) /* SOI */
        {
            /* Nothing to do. */
        }
        else if (   pu8Marker[1] == 0xE0 /* APP0 */
                 || pu8Marker[1] == 0xDB /* DQT */
                 || pu8Marker[1] == 0xDD /* DRI */
                 || pu8Marker[1] == 0xC0 /* SOF0 */
                 || pu8Marker[1] == 0xDA /* SOS */
                )
        {
            if ((uintptr_t)pu8Marker + cbMarker + 2 > (uintptr_t)pu8End)
            {
                break;
            }

            cbMarker += RT_BE2H_U16(*(uint16_t *)&pu8Marker[2]);
        }
        else
        {
            /* Skip everything. */
            UWLOG(("FRAME: skipped at %RX8\n", pu8Marker[1]));
            break;
        }

        if (pu8Marker[1] == 0xC0) /* Log SOF */
        {
            UWLOG(("FRAME: SOF\n%.*Rhxd\n", cbMarker, pu8Marker));
        }
        else if (pu8Marker[1] == 0xDA) /* Stop at SOS */
        {
            break;
        }

        pu8Src += cbMarker;
    }
}

static int usbWebcamFramePrepare(USBWEBCAM *pThis,
                                 USBWEBCAMPENDINGFRAME *pFrame)
{
    /* Currently no processing is applied. */

    if (UWLOGENABLED())
    {
        if (!pThis->stat.fFrameContentLogged)
        {
            pThis->stat.fFrameContentLogged = true;

            usbWebcamJPGFrameLog(pFrame->pu8Data, pFrame->cbData);
        }
    }

    return VINF_SUCCESS;
}

static int32_t usbWebcamFrameAddRef(USBWEBCAMPENDINGFRAME *pFrame)
{
    return ASMAtomicIncS32(&pFrame->cRefs);
}

static void usbWebcamFrameRelease(USBWEBCAM *pThis,
                                  USBWEBCAMPENDINGFRAME *pFrame)
{
    if (pFrame)
    {
        int32_t c = ASMAtomicDecS32(&pFrame->cRefs);
        Assert(c >= 0);
        if (c == 0)
        {
            RTMemFree(pFrame->pu8Data);
            pFrame->pu8Data = NULL;

            UWLOGF(("Frame %p deleted\n", pFrame));

            RTListPrepend(&pThis->listFreeFrames, &pFrame->nodeFrame);
        }
    }
}

static int usbWebcamFrameCreate(USBWEBCAM *pThis,
                                USBWEBCAMPENDINGFRAME **ppFrame,
                                const uint8_t *pu8Data,
                                uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    USBWEBCAMPENDINGFRAME *pFrame = RTListGetFirst(&pThis->listFreeFrames, USBWEBCAMPENDINGFRAME, nodeFrame);

    if (pFrame)
    {
        Assert(pFrame->cRefs == 0);
        RTListNodeRemove(&pFrame->nodeFrame);

        usbWebcamFrameAddRef(pFrame);

        if (cbData > 0)
        {
            pFrame->pu8Data = (uint8_t *)RTMemDup(pu8Data, cbData);

            if (pFrame->pu8Data)
            {
                pFrame->cbData = cbData;
                UWLOGF(("Frame %p created\n", pFrame));
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }
        }
        else
        {
            pFrame->pu8Data = NULL;
            pFrame->cbData = 0;
        }
    }
    else
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *ppFrame = pFrame;
    }
    else
    {
        usbWebcamFrameRelease(pThis, pFrame);
    }

    return rc;
}

static void usbWebcamFrameQueueAdd(USBWEBCAM *pThis,
                                   USBWEBCAMPENDINGFRAME *pFrame)
{
    RTListAppend(&pThis->listReadyFrames, &pFrame->nodeFrame);
}

static int usbWebcamFrameUpdate(USBWEBCAM *pThis,
                                const uint8_t *pu8Data,
                                uint32_t cbData,
                                const VRDEVIDEOINPAYLOADHDR *pHdr)
{
    /* Prepare a frame which will be sent to the guest. */
    USBWEBCAMPENDINGFRAME *pFrame = NULL;
    int rc = usbWebcamFrameCreate(pThis, &pFrame, pu8Data, cbData);

    if (RT_SUCCESS(rc))
    {
        pFrame->hdr = *pHdr;

        int rc2 = usbWebcamFramePrepare(pThis, pFrame);
        if (RT_FAILURE(rc2))
        {
            /* Restore the frame content. */
            memcpy(pFrame->pu8Data, pu8Data, pFrame->cbData);
        }

        /* Update the "latest frame" which will be resent to the guest
         * if no new frames arrive for too long.
         */
        usbWebcamFrameRelease(pThis, pThis->pLatestFrame);
        usbWebcamFrameAddRef(pFrame);
        pThis->pLatestFrame = pFrame;

        usbWebcamFrameRelease(pThis, pFrame);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static void usbWebcamFramesCleanup(USBWEBCAM *pThis)
{
    usbWebcamFrameRelease(pThis, pThis->pLatestFrame);
    pThis->pLatestFrame = NULL;

    USBWEBCAMPENDINGFRAME *pIter;
    USBWEBCAMPENDINGFRAME *pNext;
    RTListForEachSafe(&pThis->listReadyFrames, pIter, pNext, USBWEBCAMPENDINGFRAME, nodeFrame)
    {
        /* Move the frame structure to the free list. */
        UWLOG(("Releasing ready frame %p\n", pIter));
        RTListNodeRemove(&pIter->nodeFrame);
        usbWebcamFrameRelease(pThis, pIter);
    }
}

static uint8_t usbWebcamStreamSetup(USBWEBCAM *pThis)
{
    uint8_t u8Result = 0;

    UWLOG(("enmStreamStatus %d\n", pThis->enmStreamStatus));

    if (USBWEBCAM_IS_PROXY(pThis))
    {
        VRDEVIDEOINFORMATDESC *pFormat = &pThis->webcam.format;
        VRDEVIDEOINFRAMEDESC *paFrameDesc = pThis->webcam.paFrameDesc;

        VRDEVIDEOINCTRL_VS_SETUP req;
        UW_REQ_INIT(req, VRDE_VIDEOIN_CTRLSEL_VS_SETUP, VRDE_VIDEOIN_CTRLREQ_SET_CUR);

        /* Map pStreamParms to the client's pFormat and paFrames */
        req.u8FormatId          = pFormat->u8FormatId;
        req.u8FramingInfo       = VRDEVIDEOINCTRL_F_VS_SETUP_FID | VRDEVIDEOINCTRL_F_VS_SETUP_EOF;

        int rc = usbWebcamFindFrameSize(pThis, &req.u16Width, &req.u16Height,
                                        pThis->streamParmsCur.bFrameIndex, paFrameDesc, pFormat->u8NumFrames,
                                        NULL);

        if (rc != VINF_SUCCESS)
        {
            u8Result = VUSBV_ERR_INVALID_REQUEST;
        }
        else
        {
            req.u32FrameInterval    = pThis->streamParmsCur.dwFrameInterval;
            req.u16CompQuality      = pThis->streamParmsCur.wCompQuality;

            req.u16Delay            = 0; /* set by the client when it replies to this control. */
            req.u32ClockFrequency   = 0; /* set by the client when it replies to this control. */

            u8Result = pThis->pCtrls->ctrl_VS_COMMIT_CONTROL.CallControl(&req.hdr, sizeof(req));

            /* The status will be updated when the VS_SETUP response will be received. */
        }
    }
    else
    {
        /* Standalone mode, accept anything. */
        u8Result = 0;

        uint16_t u16Width = 0;
        uint16_t u16Height = 0;
        usbWebcamFindFrameSize(pThis, &u16Width, &u16Height, pThis->streamParmsCur.bFrameIndex, NULL, 0, NULL);

        uint8_t *pu8StandaloneFrame = NULL;
        uint32_t cbStandaloneFrame = 0;

        int rc = usbWebcamJPGCreate(pThis, u16Width, u16Height, &pu8StandaloneFrame, &cbStandaloneFrame);
        if (RT_FAILURE(rc))
        {
            pu8StandaloneFrame = NULL;
            cbStandaloneFrame = 0;
            u8Result = VUSBV_ERR_INVALID_REQUEST;
        }
        else
        {
            UWLOG(("Standalone frame %dx%d, cb %d, %Rrc\n",
                   u16Width, u16Height, cbStandaloneFrame, rc));

            pThis->enmStreamStatus = UW_STREAM_STATUS_SETUP;

            VRDEVIDEOINPAYLOADHDR hdr;
            hdr.u8HeaderLength      = 2;
            hdr.u8HeaderInfo        = VRDE_VIDEOIN_PAYLOAD_F_EOH;
            hdr.u32PresentationTime = 0;
            hdr.u32SourceTimeClock  = 0;
            hdr.u16Reserved         = 0;

            usbWebcamFrameUpdate(pThis, pu8StandaloneFrame, cbStandaloneFrame, &hdr);

            usbWebcamJPGFree(pu8StandaloneFrame);
        }

        if (u8Result == 0 && pThis->urbQueues.BulkIn.pUrbHead)
        {
            /* The guest already wants the frame data. Enable streaming. */
            usbWebcamStreamOn(pThis);
        }
    }

    return u8Result;
}

static uint8_t usbWebcamStreamOff(USBWEBCAM *pThis)
{
    uint8_t u8Result = 0;

    UWLOG(("enmStreamStatus %d\n", pThis->enmStreamStatus));

    RTTimerLRStop(pThis->hTimerStream);

    if (USBWEBCAM_IS_PROXY(pThis))
    {
        VRDEVIDEOINCTRL_VS_OFF req;
        UW_REQ_INIT(req, VRDE_VIDEOIN_CTRLSEL_VS_OFF, VRDE_VIDEOIN_CTRLREQ_SET_CUR);

        u8Result = pThis->pCtrls->ctrl_VS_COMMIT_CONTROL.CallControl(&req.hdr, sizeof(req));
    }
    else
    {
        /* Standalone mode. */
        u8Result = 0;
    }

    pThis->enmStreamStatus = UW_STREAM_STATUS_OFF;

#ifdef UWPROFILE
    int iDelay;
    for (iDelay = 0; iDelay < RT_ELEMENTS(pThis->profile.acLockDelay); iDelay++)
    {
        if (pThis->profile.acLockDelay[iDelay] != 0)
        {
            LogRel(("UW delay[%d] %d\n", iDelay, pThis->profile.acLockDelay[iDelay]));
        }
    }

    RT_ZERO(pThis->profile.acLockDelay);
    LogRel(("Queue %lld, Reap %lld\n", pThis->profile.cQueue, pThis->profile.cReap));
    pThis->profile.cQueue = 0;
    pThis->profile.cReap = 0;
#endif

    return u8Result;
}

static void usbWebcamStreamOn(USBWEBCAM *pThis)
{
    UWLOG(("enmStreamStatus %d\n", pThis->enmStreamStatus));

    Assert(pThis->enmStreamStatus == UW_STREAM_STATUS_SETUP);

    /* Possibly log the frame in the new stream. */
    pThis->stat.fFrameContentLogged = false;

    if (!pThis->fStreamError)
    {
        if (USBWEBCAM_IS_PROXY(pThis))
        {
            VRDEVIDEOINCTRL_VS_ON req;
            UW_REQ_INIT(req, VRDE_VIDEOIN_CTRLSEL_VS_ON, VRDE_VIDEOIN_CTRLREQ_SET_CUR);

            pThis->pCtrls->ctrl_VS_COMMIT_CONTROL.CallControl(&req.hdr, sizeof(req));
        }
        else
        {
            if (   pThis->pLatestFrame
                && RTListIsEmpty(&pThis->listReadyFrames))
            {
                usbWebcamFrameAddRef(pThis->pLatestFrame);
                usbWebcamFrameQueueAdd(pThis, pThis->pLatestFrame);
            }
        }
    }

    RTTimerLRStart(pThis->hTimerStream, 0);

    /* Set the status immediately and do not wait for the VS_ON response.
     * In worst case the webcam will not receive any frames.
     *
     * Also setting status to the UW_STREAM_STATUS_ON will prevent calling this
     * if another bulk URB is submitted.
     */
    pThis->enmStreamStatus = UW_STREAM_STATUS_ON;
}

static void usbWebcamStreamAbort(USBWEBCAM *pThis)
{
    /* Stop the frame streaming. */
    usbWebcamStreamOff(pThis);

    UWLOG(("enmStreamStatus %d\n", pThis->enmStreamStatus));

    /* Reset USB streaming. */
    pThis->fu8FID = 0;
    pThis->cbLeft = 0;
    pThis->fStreamError = false;
    pThis->u64StartFrameMS = 0;
    pThis->u64LastFrameMS = 0;

    usbWebcamFramesCleanup(pThis);

    /*
     * Complete all pending bulk-in, intr-in URBs. Must be done after the frame cleanup to
     * avoid sending partial frames to the guest.
     */
    usbWebcamQueueComplete(pThis, &pThis->urbQueues.BulkIn, usbWebcamStream, false /* fDataOnly */);
    usbWebcamQueueComplete(pThis, &pThis->urbQueues.IntrIn, usbWebcamEventRead, false /* fDataOnly */);
}

static void usbWebcamOnProxyModeChange(USBWEBCAM *pThis, USBWEBCAMSTATE enmNewState)
{
    if (pThis->enmStreamStatus != UW_STREAM_STATUS_OFF)
    {
        usbWebcamStreamOff(pThis);
        pThis->enmState = enmNewState;

        /* New stream will be started. */
        pThis->fStreamError = false;

        /* If the status was UW_STREAM_STATUS_ON, then usbWebcamStreamSetup will reenable streaming
         * if there are pending bulk-in URBs. Or a submitted bulk-in URB will enable streaming.
         */
        uint8_t u8Result = usbWebcamStreamSetup(pThis);
        if (u8Result != 0)
        {
            UWLOG(("Entering stream error mode\n"));

            /* Report an error to the guest in the payload header. */
            pThis->fStreamError = true;

            WEBCAMINTERFACE *pInterface = &pThis->aInterfaces[USBWEBCAM_INTERFACE_STREAMING];
            usbWebcamUpdateErrorCode(pInterface, VUSBV_ERR_STREAM_FORMAT_CHANGE);

            /* Enter the "stream error" mode */
            pThis->enmStreamStatus = UW_STREAM_STATUS_SETUP;

            if (pThis->urbQueues.BulkIn.pUrbHead)
            {
                /* The guest already wants the frame data. Enable streaming. */
                usbWebcamStreamOn(pThis);
            }

            VUSBVSTATUSINTRVS ev;
            ev.Core.bStatusType = VUSB_V_STATUSINTR_TYPE_VS;
            ev.Core.bOriginator = USBWEBCAM_INTERFACE_STREAMING;
            ev.bEvent = VUSBV_ERR_STREAM_FORMAT_CHANGE;

            usbWebcamEventCreate(pThis, &ev.Core, sizeof(ev), NULL, 0);
        }
    }
    else
    {
        pThis->enmState = enmNewState;
    }
}

/*
 * URB queue maintenance.
 */
static void urbQueueInit(PURBQUEUE pQueue)
{
    pQueue->pUrbHead = NULL;
    pQueue->ppUrbTail = &pQueue->pUrbHead;
}

static void urbQueueAddTail(PURBQUEUE pQueue, PVUSBURB pUrb)
{
    pUrb->Dev.pNext = NULL;
    *pQueue->ppUrbTail = pUrb;
    pQueue->ppUrbTail  = &pUrb->Dev.pNext;
}

static PVUSBURB urbQueueRemoveHead(PURBQUEUE pQueue)
{
    PVUSBURB pUrb = pQueue->pUrbHead;

    if (pUrb)
    {
        pQueue->pUrbHead = pUrb->Dev.pNext;
        if (pUrb->Dev.pNext == NULL)
        {
            pQueue->ppUrbTail = &pQueue->pUrbHead;
        }

        pUrb->Dev.pNext = NULL;
    }

    return pUrb;
}

static bool urbQueueRemove(PURBQUEUE pQueue, PVUSBURB pUrb)
{
    PVUSBURB pIter = pQueue->pUrbHead;

    if (pIter == pUrb)
    {
        pQueue->pUrbHead = pIter->Dev.pNext;
        if (pIter->Dev.pNext == NULL)
        {
            pQueue->ppUrbTail = &pQueue->pUrbHead;
        }
    }
    else
    {
        while (pIter)
        {
            if (pIter->Dev.pNext == pUrb)
            {
                pIter->Dev.pNext = pUrb->Dev.pNext;
                break;
            }
            pIter = pIter->Dev.pNext;
        }

        if (!pIter)
        {
            return false;
        }

        if (pIter->Dev.pNext == NULL)
        {
            pQueue->ppUrbTail = &pIter->Dev.pNext;
        }
    }

    pUrb->Dev.pNext = NULL;
    return true;
}


/*
 * URB completion helpers.
 */

static void usbWebcamLinkDone(PUSBWEBCAM pThis, PVUSBURB pUrb)
{
    urbQueueAddTail(&pThis->urbQueues.Done, pUrb);

    if (pThis->fHaveDoneQueueWaiter)
    {
        int rc = RTSemEventSignal(pThis->hEvtDoneQueue);
        AssertRC(rc);
    }
}

static int usbWebcamCompleteOk(PUSBWEBCAM pThis, PVUSBURB pUrb, uint32_t cbData)
{
    UWLOGF(("pUrb:%p cbData:%d\n", pUrb, cbData));
    pUrb->enmStatus = VUSBSTATUS_OK;
    pUrb->cbData    = cbData;

    if (cbData)
    {
        UWLOGF(("URB data\n%.*Rhxd\n", cbData > 32? 32: cbData, pUrb->pbData));
    }

    usbWebcamLinkDone(pThis, pUrb);
    return VINF_SUCCESS;
}

static int usbWebcamCompleteStall(PUSBWEBCAM pThis, PVUSBURB pUrb)
{
    UWLOG(("pUrb:%p\n", pUrb));

    pUrb->enmStatus = VUSBSTATUS_STALL;

    usbWebcamLinkDone(pThis, pUrb);
    return VINF_SUCCESS;
}

static int usbWebcamCompleteSetupUnsupported(PUSBWEBCAM pThis, PVUSBURB pUrb)
{
    static int cReported = 0;
    if (cReported < 8)
    {
        cReported++;

        PVUSBSETUP pSetup = (PVUSBSETUP)pUrb->pbData;
        LogRel(("USBWEBCAM: request 0x%02X 0x%02X 0x%04X 0x%04X %d, cb %d\n",
                pSetup->bmRequestType,
                pSetup->bRequest,
                pSetup->wValue,
                pSetup->wIndex,
                pSetup->wLength,
                pUrb->cbData
              ));
    }

    return usbWebcamCompleteStall(pThis, pUrb);
}

static int usbWebcamCompleteSetupOk(PUSBWEBCAM pThis, PVUSBURB pUrb,
                                    uint32_t cbData, const void *pvDataOptional)
{
    if (RT_LIKELY(pUrb->cbData >= sizeof(VUSBSETUP)))
    {
        cbData = RT_MIN(cbData, pUrb->cbData - sizeof(VUSBSETUP));

        /* pvDataOptional is NULL if the data is already copied. */
        if (pvDataOptional)
        {
            memcpy(&pUrb->pbData[sizeof(VUSBSETUP)], pvDataOptional, cbData);
        }

        return usbWebcamCompleteOk(pThis, pUrb, sizeof(VUSBSETUP) + cbData);
    }

    AssertFailed();
    return usbWebcamCompleteSetupUnsupported(pThis, pUrb);
}

static void usbWebcamQueueComplete(PUSBWEBCAM pThis, URBQUEUE *pQueue, PFNCOPYTOURB pfnCopyToUrb, bool fDataOnly)
{
    PVUSBURB pUrb = pQueue->pUrbHead;
    while (pUrb)
    {
        uint32_t cbDataReturned = pfnCopyToUrb?
                                      pfnCopyToUrb(pThis, pUrb):
                                      0;

        if (fDataOnly && cbDataReturned == 0)
        {
            break;
        }

        bool fRemoved = urbQueueRemove(pQueue, pUrb);
        Assert(fRemoved); NOREF(fRemoved);

        UWLOGF(("completing %s pUrb %p cbData %d, cbDataReturned %d, enmStreamStatus %d\n",
                 pQueue == &pThis->urbQueues.IntrIn? "intr-in":
                 pQueue == &pThis->urbQueues.BulkIn? "bulk-in":
                 "unknown",
                 pUrb, pUrb->cbData, cbDataReturned, pThis->enmStreamStatus));

        usbWebcamCompleteOk(pThis, pUrb, cbDataReturned);

        pUrb = pQueue->pUrbHead;
    }
}


static void usbWebcamUpdateErrorCode(WEBCAMINTERFACE *pInterface, uint8_t bRequestErrorCode)
{
    if (pInterface->pCtrlErrorCode)
    {
        pInterface->pCtrlErrorCode->SetCur(&bRequestErrorCode, sizeof(bRequestErrorCode));
    }
}

static int usbWebcamReqClassInterface(PUSBWEBCAM pThis, PVUSBURB pUrb, PCVUSBSETUP pSetup)
{
    UWLOGF(("iInstance:%d pUrb:%p pSetup:%p\n",
             pThis->pUsbIns->iInstance, pUrb, pSetup));

    uint8_t iInterface = (uint8_t)pSetup->wIndex;
    uint8_t iUnit      = (uint8_t)(pSetup->wIndex >> 8);
    uint8_t iCtrlSel   = (uint8_t)(pSetup->wValue >> 8);

    UWLOGF(("iInterface %d, iUnit %d, iCtrlSel %d\n", iInterface, iUnit, iCtrlSel));

    /*
     * Verify the address.
     * UVC1.1: "If the request specifies an unknown or non-entity ID or an unknown
     *      interface or endpoint number, the control pipe must indicate a stall."
     */
    if (iInterface >= RT_ELEMENTS(pThis->aInterfaces))
    {
        UWLOG(("Invalid iInterface %d!!!\n", iInterface));
        return usbWebcamCompleteStall(pThis, pUrb);
    }

    WEBCAMINTERFACE *pInterface = &pThis->aInterfaces[iInterface];

    if (iUnit >= pInterface->cUnits)
    {
        UWLOG(("Invalid iUnit %d!!!\n", iUnit));
        usbWebcamUpdateErrorCode(pInterface, VUSBV_ERR_INVALID_UNIT);
        return usbWebcamCompleteStall(pThis, pUrb);
    }

    WEBCAMUNIT *pUnit = &pInterface->paUnits[iUnit];

    if (iCtrlSel > pUnit->cControls)
    {
        UWLOG(("Invalid iCtrlSel %d!!!\n", iCtrlSel));
        usbWebcamUpdateErrorCode(pInterface, VUSBV_ERR_INVALID_CONTROL);
        return usbWebcamCompleteStall(pThis, pUrb);
    }

    UWCtrl *pControl = pUnit->papControls[iCtrlSel];
    Assert(pControl && pControl->CtrlSel() == iCtrlSel);

    UWLOGF(("pControl %p, name [%s], CtrlSel %d\n",
            pControl, pControl? pControl->CtrlName(): "NULL", pControl? pControl->CtrlSel(): 0));

    /* Check bRequest and bmRequestType direction. */
    if (!pControl || !pControl->IsValidRequest(pSetup))
    {
        /* UVC1.1: "If the addressed Control or entity does not support modification of a certain attribute,
         *      the control pipe must indicate a stall when an attempt is made to modify that attribute."
         *      and "If the addressed Control or entity does not support readout of a certain attribute,
         *      the control pipe must indicate a stall when an attempt is made to read that attribute."
         */
        UWLOG(("Unsupported bRequest 0x%02X!!!\n", pSetup->bRequest));
        usbWebcamUpdateErrorCode(pInterface, VUSBV_ERR_INVALID_REQUEST);
        return usbWebcamCompleteStall(pThis, pUrb);
    }

    UWLOG(("CTRL: [%s] %d req 0x%02X, wLength %d\n",
            pControl->CtrlName(), pControl->CtrlSel(), pSetup->bRequest, pSetup->wLength));

    uint8_t bRequestErrorCode = VUSBV_INF_SUCCESS;
    uint16_t cbControl = pControl->CtrlLength();

    const uint8_t *pu8Data = NULL;
    uint16_t cbData = 0;

    switch (pSetup->bRequest)
    {
        case VUSB_V_REQ_GET_MIN:
            pu8Data = pControl->GetMin();
            cbData = cbControl;
            break;
        case VUSB_V_REQ_GET_MAX:
            pu8Data = pControl->GetMax();
            cbData = cbControl;
            break;
        case VUSB_V_REQ_GET_RES:
            pu8Data = pControl->GetRes();
            cbData = cbControl;
            break;
        case VUSB_V_REQ_GET_LEN:
            pu8Data = (uint8_t *)&cbControl;
            cbData = sizeof(cbControl); /** @todo make sure that it is sizeof(cbControl) and not 1 byte. */
            break;
        case VUSB_V_REQ_GET_INFO:
            pu8Data = pControl->GetInfo();
            cbData = sizeof(uint8_t);
            break;
        case VUSB_V_REQ_GET_DEF:
            pu8Data = pControl->GetDef();
            cbData = cbControl;
            break;
        case VUSB_V_REQ_GET_CUR:
            pu8Data = pControl->GetCur();
            cbData = cbControl;
            break;
        case VUSB_V_REQ_SET_CUR:
            if (   USBWEBCAM_IS_STANDALONE(pControl->Instance())
                || pControl->FetchCompleted())
            {
                bRequestErrorCode = pControl->SetCur(&pUrb->pbData[sizeof(VUSBSETUP)], pSetup->wLength);
                cbData = pSetup->wLength;
            }
            else
            {
                bRequestErrorCode = VUSBV_ERR_NOT_READY;
            }
            break;
        default:
            /* Should not happen because requests must be verified in IsValidRequest */
            UWLOG(("Invalid bRequest %d!!!\n", pSetup->bRequest));
            AssertFailed();
            bRequestErrorCode = VUSBV_ERR_INVALID_REQUEST;
            break;
    }

    /* Always update the error code.
     *
     * UVC1.1: "[The Error Code control] will be reset to 0 (No error) upon the successful completion
     *          of any control request (including requests to the [Error Code] control).
     */
    usbWebcamUpdateErrorCode(pInterface, bRequestErrorCode);

    int rc;
    if (bRequestErrorCode == VUSBV_INF_SUCCESS)
    {
        rc = usbWebcamCompleteSetupOk(pThis, pUrb, cbData, pu8Data);
    }
    else
    {
        rc = usbWebcamCompleteStall(pThis, pUrb);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static int usbWebcamRequestClass(PUSBWEBCAM pThis, PVUSBURB pUrb, PCVUSBSETUP pSetup)
{
    int rc = VINF_SUCCESS;

    UWLOGF(("iInstance:%d pUrb:%p pSetup:%p\n",
            pThis->pUsbIns->iInstance, pUrb, pSetup));

    switch (pSetup->bmRequestType & VUSB_RECIP_MASK)
    {
        case VUSB_TO_INTERFACE:
            rc = usbWebcamReqClassInterface(pThis, pUrb, pSetup);
            break;

        case VUSB_TO_ENDPOINT: /** @todo UVC spec mentions that SET_CUR can be for VS endpoint. */
        case VUSB_TO_OTHER:
        case VUSB_TO_DEVICE:
        default:
            rc = usbWebcamCompleteSetupUnsupported(pThis, pUrb);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

/* Standard requests */
static int usbWebcamRequestStandardFromDevice(PUSBWEBCAM pThis, PVUSBURB pUrb, PCVUSBSETUP pSetup)
{
    int rc = VINF_SUCCESS;

    UWLOGF(("iInstance:%d pUrb:%p pSetup:%p\n",
             pThis->pUsbIns->iInstance, pUrb, pSetup));

    switch (pSetup->bRequest)
    {
        case VUSB_REQ_GET_STATUS:
        {
            UWLOG(("GET_STATUS\n"));
            uint32_t u32Status = 0; /* no remote wakeup, bus-powered. */
            rc = usbWebcamCompleteSetupOk(pThis, pUrb, sizeof(uint32_t), &u32Status);
        } break;

        case VUSB_REQ_GET_DESCRIPTOR:
        {
            UWLOG(("GET_DESCRIPTOR\n"));
            rc = usbWebcamCompleteSetupUnsupported(pThis, pUrb);
        } break;

        case VUSB_REQ_GET_CONFIGURATION:
        case VUSB_REQ_GET_INTERFACE:
        default:
            rc = usbWebcamCompleteSetupUnsupported(pThis, pUrb);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static int usbWebcamRequestStandard(PUSBWEBCAM pThis, PVUSBURB pUrb, PCVUSBSETUP pSetup)
{
    int rc = VINF_SUCCESS;

    UWLOGF(("iInstance:%d pUrb:%p pSetup:%p\n",
             pThis->pUsbIns->iInstance, pUrb, pSetup));

    /* Check direction. */
    if ((pSetup->bmRequestType & VUSB_DIR_MASK) == VUSB_DIR_TO_HOST)
    {
        switch (pSetup->bmRequestType & VUSB_RECIP_MASK)
        {
            case VUSB_TO_DEVICE:
                rc = usbWebcamRequestStandardFromDevice(pThis, pUrb, pSetup);
                break;

            case VUSB_TO_ENDPOINT:
            case VUSB_TO_INTERFACE:
            case VUSB_TO_OTHER:
            default:
                rc = usbWebcamCompleteSetupUnsupported(pThis, pUrb);
        }
    }
    else
    {
        switch (pSetup->bmRequestType & VUSB_RECIP_MASK)
        {
            case VUSB_TO_ENDPOINT:
            {
                if (pSetup->bRequest == VUSB_REQ_CLEAR_FEATURE)
                {
                    UWLOG(("endpoint:CLEAR_FEATURE: wValue %d, wIndex 0x%02X, StreamStatus %d\n",
                            pSetup->wValue, pSetup->wIndex, pThis->enmStreamStatus));

                    if (pThis->enmStreamStatus == UW_STREAM_STATUS_ON)
                    {
                        usbWebcamStreamAbort(pThis);
                    }

                    rc = usbWebcamCompleteOk(pThis, pUrb, pUrb->cbData);
                }
                else
                {
                    rc = usbWebcamCompleteSetupUnsupported(pThis, pUrb);
                }
            } break;

            case VUSB_TO_DEVICE:
            case VUSB_TO_INTERFACE:
            case VUSB_TO_OTHER:
            default:
                rc = usbWebcamCompleteSetupUnsupported(pThis, pUrb);
        }
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static int usbWebcamHandlePipeDefault(PUSBWEBCAM pThis, PVUSBURB pUrb)
{
    int rc = VINF_SUCCESS;

    UWLOGF(("iInstance:%d pUrb:%p\n",
             pThis->pUsbIns->iInstance, pUrb));

    PVUSBSETUP pSetup = (PVUSBSETUP)pUrb->pbData;

    switch (pSetup->bmRequestType & VUSB_REQ_MASK)
    {
        case VUSB_REQ_STANDARD:
            rc = usbWebcamRequestStandard(pThis, pUrb, pSetup);
            break;

        case VUSB_REQ_CLASS:
            rc = usbWebcamRequestClass(pThis, pUrb, pSetup);
            break;

        case VUSB_REQ_VENDOR:
        default:
            rc = usbWebcamCompleteSetupUnsupported(pThis, pUrb);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static void usbWebcamEventCreate(PUSBWEBCAM pThis,
                                 const VUSBVSTATUSINTR *pHdr, uint8_t cbHdr,
                                 const void *pvValue, uint8_t cbValue)
{
    UWSTATUSEVENT *pEvent = RTListGetFirst(&pThis->listFreeStatusEvents, UWSTATUSEVENT, nodeEvent);
    if (pEvent)
    {
        RTListNodeRemove(&pEvent->nodeEvent);

        pEvent->cbPacket = (uint8_t)RT_MIN(cbHdr, sizeof(pEvent->u));
        pEvent->cbValue = (uint8_t)RT_MIN(cbValue, sizeof(pEvent->bValue));

        memcpy(&pEvent->u, pHdr, pEvent->cbPacket);
        if (pvValue && pEvent->cbValue)
        {
            memcpy(&pEvent->bValue, pvValue, pEvent->cbValue);
        }

        RTListAppend(&pThis->listStatusEvents, &pEvent->nodeEvent);

        usbWebcamQueueComplete(pThis, &pThis->urbQueues.IntrIn, usbWebcamEventRead, true /* fDataOnly */);
    }
}

static uint32_t usbWebcamEventRead(PUSBWEBCAM pThis, PVUSBURB pUrb)
{
    /* UVC 1.1 FAQ: "completions of asynchronous control operations (via the Status
     *               Interrupt Endpoint) do not affect the Request Error Code Control."
     *
     * @todo Each control can add an event to the listStatusEvents.
     * This routine reads events from the list and generates the right packet.
     */
    uint32_t cbReturned = 0;

    UWSTATUSEVENT *pEvent = RTListGetFirst(&pThis->listStatusEvents, UWSTATUSEVENT, nodeEvent);
    if (pEvent)
    {
        RTListNodeRemove(&pEvent->nodeEvent);

        UWLOG(("Delivering event %p: type %d\n", pEvent, pEvent->u.hdr.bStatusType));

        uint32_t cbRequired = pEvent->cbPacket + pEvent->cbValue;

        if (cbRequired <= pUrb->cbData)
        {
            memcpy(&pUrb->pbData[0],                &pEvent->u, pEvent->cbPacket);
            memcpy(&pUrb->pbData[pEvent->cbPacket], pEvent->bValue, pEvent->cbValue);

            cbReturned = cbRequired;
        }
        else
        {
            UWLOG(("Ignoring event %p: required %d, cbData %d\n",
                   pEvent, cbRequired, pUrb->cbData));
            AssertFailed();
        }

        RTListPrepend(&pThis->listFreeStatusEvents, &pEvent->nodeEvent);
    }

    return cbReturned;
}

static int usbWebcamHandlePipeInterrupt(PUSBWEBCAM pThis, PVUSBURB pUrb)
{
    UWLOG(("intr pUrb:%p, cbData:%d\n",
           pUrb, pUrb->cbData));

    /* If there is a pending URB, complete it without data. It is most likely cancelled.
     *
     * The webcam does not complete URB if there is no data. Therefore if a new intr-in URB is submitted,
     * it means that the old URB is not ok anymore and returned data in it may be lost.
     */
    usbWebcamQueueComplete(pThis, &pThis->urbQueues.IntrIn, NULL /* no data */, false /* fDataOnly */);

    urbQueueAddTail(&pThis->urbQueues.IntrIn, pUrb);
    usbWebcamQueueComplete(pThis, &pThis->urbQueues.IntrIn, usbWebcamEventRead, true /* fDataOnly */);

    return VINF_SUCCESS;
}

static uint32_t usbWebcamStreamError(PUSBWEBCAM pThis, uint8_t *pu8Data, uint32_t cbData)
{
    RT_NOREF1(pThis);
    if (cbData < sizeof(VUSBVPAYLOADHEADER))
    {
        /* Too small. Ignore. */
        return 0;
    }

    /* The payload header must be returned. */
    uint32_t cbHdr = sizeof(VUSBVPAYLOADHEADER);

    VUSBVPAYLOADHEADER *pHdr = (VUSBVPAYLOADHEADER *)pu8Data;
    pHdr->bHeaderLength = (uint8_t)cbHdr;
    pHdr->bmHeaderInfo = VUSB_V_PAYLOAD_F_ERR | VUSB_V_PAYLOAD_F_EOF | VUSB_V_PAYLOAD_F_EOH;

    UWLOG(("Stream error hdr 0x%02X\n",
           pHdr->bmHeaderInfo));

    return cbHdr;
}

/*
 * Fills the URB data with payload transfers.
 *
 * Payload transfer consist of a header and the data.
 * A video frame can include a few payload transfers.
 *
 * Each payload transfer size is pThis->streamParmsCur.dwMaxPayloadTransferSize.
 * The last payload transfer size may be less than dwMaxPayloadTransferSize.
 *
 * One payload transfer may be split between subsequent URBs, if the URB size
 * is less than the size of the payload transfer.
 */
static uint32_t usbWebcamStreamFrame(PUSBWEBCAM pThis,
                                     uint8_t *pu8Data,
                                     uint32_t cbData,
                                     const uint8_t *pu8Frame,
                                     uint32_t cbFrame,
                                     const void *pvPTS,
                                     const void *pvSCR)
{
    if (pu8Frame == NULL)
    {
        return 0;
    }

    if (cbData < sizeof(VUSBVPAYLOADHEADER) + 4 + 6) /* PTS + SCR */
    {
        /* Too small for video streaming. No guest will seriously use such URBs. Ignore. */
        return 0;
    }

    if (pThis->cbLeft == 0)
    {
        /* Start a new frame. */
        pThis->fu8FID ^= VUSB_V_PAYLOAD_F_FID;
        pThis->cbLeft = cbFrame;
        pThis->cbPayloadLeft = 0;

        UWLOG(("Frame started cb %d\n", cbFrame));
    }

    uint32_t cbReturned;

    if (pThis->cbPayloadLeft == 0)
    {
        /* The payload header must be returned. */
        uint32_t cbHdr = sizeof(VUSBVPAYLOADHEADER) +
                         (pvPTS? 4: 0) +
                         (pvSCR? 6: 0);

        /* Have to return the header and frame data. */
        uint32_t cbPayload = pThis->cbLeft + cbHdr;

        /* Can not return more than the negotiated payload size. */
        pThis->cbPayloadLeft = RT_MIN(cbPayload, pThis->streamParmsCur.dwMaxPayloadTransferSize);

        uint8_t *pu8Hdr = pu8Data;
        VUSBVPAYLOADHEADER *pBulkHeader = (VUSBVPAYLOADHEADER *)pu8Data;
        pBulkHeader->bHeaderLength = (uint8_t)cbHdr;
        pBulkHeader->bmHeaderInfo = pThis->fu8FID | VUSB_V_PAYLOAD_F_EOH;

        if (cbPayload <= pThis->streamParmsCur.dwMaxPayloadTransferSize)
        {
            /* All remaining frame data can be returned in this payload transfer. */
            pBulkHeader->bmHeaderInfo |= VUSB_V_PAYLOAD_F_EOF;
        }
        pu8Hdr += sizeof(VUSBVPAYLOADHEADER);

        if (pvPTS)
        {
            pBulkHeader->bmHeaderInfo |= VUSB_V_PAYLOAD_F_PTS;
            memcpy(pu8Hdr, pvPTS, 4);
            pu8Hdr += 4;
        }

        if (pvSCR)
        {
            pBulkHeader->bmHeaderInfo |= VUSB_V_PAYLOAD_F_SCR;
            memcpy(pu8Hdr, pvSCR, 6);
            pu8Hdr += 6;
        }

        UWLOGF(("Payload cb %d, hdr 0x%02X\n",
               pThis->cbPayloadLeft, pBulkHeader->bmHeaderInfo));

        cbReturned = cbHdr;
    }
    else
    {
        cbReturned = 0;
    }

    uint32_t cbFrameDataToCopy = RT_MIN(pThis->cbPayloadLeft, cbData) - cbReturned;

    memcpy(&pu8Data[cbReturned],
           &pu8Frame[cbFrame - pThis->cbLeft],
           cbFrameDataToCopy);
    cbReturned += cbFrameDataToCopy;

    pThis->cbLeft -= cbFrameDataToCopy;
    pThis->cbPayloadLeft -= cbReturned;

    UWLOGF(("cb %d, fd %d l %d pl %d\n",
            cbReturned, cbFrameDataToCopy, pThis->cbLeft, pThis->cbPayloadLeft));

    if (pThis->cbLeft == 0)
    {
        UWLOG(("Frame end\n"));
    }

    return cbReturned;
}

static uint32_t usbWebcamStream(PUSBWEBCAM pThis, PVUSBURB pUrb)
{
    uint32_t cbDataReturned = 0;

    uint64_t u64NowMS = RTTimeMilliTS();

    if (RT_UNLIKELY(pThis->fStreamError && pThis->pCurrentFrame == NULL))
    {
        if (u64NowMS - pThis->u64LastFrameMS >= 200) /* 5FPS */
        {
            pThis->u64StartFrameMS = u64NowMS;
            cbDataReturned = usbWebcamStreamError(pThis, pUrb->pbData, pUrb->cbData);
            pThis->u64LastFrameMS = pThis->u64StartFrameMS;
        }

        return cbDataReturned;
    }

    if (pThis->cbLeft == 0)
    {
        /* Start a new frame. */
        usbWebcamFrameRelease(pThis, pThis->pCurrentFrame);

        pThis->pCurrentFrame = RTListGetFirst(&pThis->listReadyFrames, USBWEBCAMPENDINGFRAME, nodeFrame);

        if (pThis->pCurrentFrame)
        {
            RTListNodeRemove(&pThis->pCurrentFrame->nodeFrame);

            pThis->u64StartFrameMS = u64NowMS;
            pThis->u64LastFrameMS = 0;
        }
    }

    if (pThis->pCurrentFrame)
    {
        /** @todo PTS/SCR */
        cbDataReturned = usbWebcamStreamFrame(pThis, pUrb->pbData, pUrb->cbData,
                                              pThis->pCurrentFrame->pu8Data, pThis->pCurrentFrame->cbData,
                                              NULL, NULL);

        /* If last chunk of data for the frame. */
        if (pThis->cbLeft == 0)
        {
            usbWebcamFrameRelease(pThis, pThis->pCurrentFrame);
            pThis->pCurrentFrame = NULL;

            pThis->u64LastFrameMS = pThis->u64StartFrameMS;
        }
    }

    return cbDataReturned;
}

static int usbWebcamHandlePipeBulkIn(PUSBWEBCAM pThis, PVUSBURB pUrb)
{
    UWLOGF(("iInstance:%d pUrb %p cbData %d, enmStreamStatus %d\n",
            pThis->pUsbIns->iInstance, pUrb, pUrb->cbData, pThis->enmStreamStatus));

    if (pThis->enmStreamStatus == UW_STREAM_STATUS_SETUP)
    {
        usbWebcamStreamOn(pThis);
    }

    /* It could it be that there are already URBs to be completed before the submitted one.
     * Add the URB to the BulkIn queue and complete URBs from the queue.
     */
    urbQueueAddTail(&pThis->urbQueues.BulkIn, pUrb);
    usbWebcamQueueComplete(pThis, &pThis->urbQueues.BulkIn, usbWebcamStream, true /* fDataOnly */);

    return VINF_SUCCESS;
}

static int usbWebcamAttach(USBWEBCAM *pThis, const VRDEVIDEOINDEVICEDESC *pDeviceDesc, uint32_t cbDeviceDesc)
{
    /* Save the webcam description and start collecting the webcam controls info. */

    int rc = VINF_SUCCESS;
    uint8_t idxFrame;

    const uint8_t *pu8Data = (uint8_t *)pDeviceDesc;
    const uint8_t *pu8End = pu8Data + cbDeviceDesc;

    Assert(pu8End - pu8Data > (int)sizeof(VRDEVIDEOINDEVICEDESC));

    UWLOG(("Focal %d-%d/%d mult %d CT 0x%08X PU 0x%08X caps 0x%02X formats %d cbExt %d\n",
           pDeviceDesc->u16ObjectiveFocalLengthMin,
           pDeviceDesc->u16ObjectiveFocalLengthMax,
           pDeviceDesc->u16OcularFocalLength,
           pDeviceDesc->u16MaxMultiplier,
           pDeviceDesc->fu32CameraControls,
           pDeviceDesc->fu32ProcessingControls,
           pDeviceDesc->fu8DeviceCaps,
           pDeviceDesc->u8NumFormats,
           pDeviceDesc->cbExt));

    /* Verify the data and find the MJPEG format description. */
    const VRDEVIDEOINFORMATDESC *pFormatMJPEG = NULL;

    pu8Data += sizeof(VRDEVIDEOINDEVICEDESC);

    if (pDeviceDesc->u8NumFormats > 0)
    {
        if (pu8End - pu8Data > pDeviceDesc->cbExt)
        {
            pu8Data += pDeviceDesc->cbExt;

            uint8_t idxFormat;
            for (idxFormat = 0; idxFormat < pDeviceDesc->u8NumFormats && RT_SUCCESS(rc); idxFormat++)
            {
                const VRDEVIDEOINFORMATDESC *pFormat = (const VRDEVIDEOINFORMATDESC *)pu8Data;

                if (   pu8End - pu8Data >= (int)sizeof(VRDEVIDEOINFORMATDESC)
                    && pu8End - pu8Data >= pFormat->cbFormat)
                {
                    pu8Data += pFormat->cbFormat;

                    UWLOG(("Format[%d]: cb %d, id %d, type 0x%02X, flags 0x%02X, frames %d, reserved 0x%04X\n",
                           idxFormat,
                           pFormat->cbFormat,
                           pFormat->u8FormatId,
                           pFormat->u8FormatType,
                           pFormat->u8FormatFlags,
                           pFormat->u8NumFrames,
                           pFormat->u16Reserved));

                    /*
                     * Check if this is a format which is useful for the emulated webcam.
                     */
                    if (!pFormatMJPEG)
                    {
                        if (pFormat->u8FormatType == VRDE_VIDEOIN_FORMAT_MJPEG)
                        {
                            pFormatMJPEG = pFormat;
                        }
                    }

                    for (idxFrame = 0; idxFrame < pFormat->u8NumFrames && RT_SUCCESS(rc); idxFrame++)
                    {
                        const VRDEVIDEOINFRAMEDESC *pFrame = (const VRDEVIDEOINFRAMEDESC *)pu8Data;

                        if (   pu8End - pu8Data >= (int)sizeof(VRDEVIDEOINFRAMEDESC)
                            && pu8End - pu8Data >= pFrame->cbFrame)
                        {
                            pu8Data += pFrame->cbFrame;
                        }
                        else
                        {
                            rc = VERR_INVALID_PARAMETER;
                            break;
                        }
                    }
                }
                else
                {
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
            }
        }
        else
        {
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else
    {
        rc = VERR_INVALID_PARAMETER;
    }

    /* Make a copy of relevant information. */
    if (RT_SUCCESS(rc))
    {
        if (pFormatMJPEG)
        {
            UWLOG(("Found MJPEG format: id %d\n",
                   pFormatMJPEG->u8FormatId));

            pThis->webcam.desc = *pDeviceDesc;
            pThis->webcam.desc.cbExt = 0;

            pThis->webcam.format = *pFormatMJPEG;
            pThis->webcam.format.cbFormat = sizeof(VRDEVIDEOINFORMATDESC);

            pThis->webcam.paFrameDesc = (VRDEVIDEOINFRAMEDESC *)RTMemAlloc(pThis->webcam.format.u8NumFrames * sizeof(VRDEVIDEOINFRAMEDESC));

            if (pThis->webcam.paFrameDesc)
            {
                pu8Data = (const uint8_t *)pFormatMJPEG;
                pu8Data += pFormatMJPEG->cbFormat;

                for (idxFrame = 0; idxFrame < pThis->webcam.format.u8NumFrames; idxFrame++)
                {
                    const VRDEVIDEOINFRAMEDESC *pFrame = (const VRDEVIDEOINFRAMEDESC *)pu8Data;

                    UWLOG(("Frame[%d]: cb %d, id %d, flags 0x%02X, size %dx%d intervals %d (%d, %d)\n",
                           idxFrame,
                           pFrame->cbFrame,
                           pFrame->u8FrameId,
                           pFrame->u8FrameFlags,
                           pFrame->u16Width, pFrame->u16Height,
                           pFrame->u32NumFrameIntervals,
                           pFrame->u32MinFrameInterval,
                           pFrame->u32MaxFrameInterval));

                    pThis->webcam.paFrameDesc[idxFrame] = *pFrame;
                    pThis->webcam.paFrameDesc[idxFrame].cbFrame = sizeof(VRDEVIDEOINFRAMEDESC);

                    pu8Data += pFrame->cbFrame;
                }
            }
            else
            {
                rc = VERR_NO_MEMORY;
            }
        }
        else
        {
            rc = VERR_NOT_SUPPORTED;
        }
    }

    if (RT_SUCCESS(rc))
    {
        /* Build USB descriptors of the device, but only during construction because
         * descriptors can not be changed later.
         */
        if (!pThis->fConstructed)
        {
            PPDMUSBDESCCACHE pDC = NULL;
            rc = usbWebcamDescriptorsBuild(&pDC, pThis->iInstance, pDeviceDesc, cbDeviceDesc, pThis->u32MinFrameInterval);
            if (RT_SUCCESS(rc))
            {
                usbWebcamDescriptorsFree(pThis->pDescCache);
                pThis->pDescCache = pDC;
            }
        }
    }

    /* Collect the webcam control information. */
    if (RT_SUCCESS(rc))
    {
        unsigned iInterface;
        for (iInterface = 0; iInterface < RT_ELEMENTS(pThis->aInterfaces); iInterface++)
        {
            WEBCAMINTERFACE *pInterface = &pThis->aInterfaces[iInterface];
            int iUnit;
            for (iUnit = 0; iUnit < pInterface->cUnits; iUnit++)
            {
                WEBCAMUNIT *pUnit = &pInterface->paUnits[iUnit];
                int iControl;
                for (iControl = 0; iControl < pUnit->cControls; iControl++)
                {
                    pUnit->papControls[iControl]->Fetch();
                }
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        RTMemFree(pThis->webcam.paFrameDesc);
        RT_ZERO(pThis->webcam);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static int usbWebcamDetach(USBWEBCAM *pThis)
{
    /** @todo */
    int rc = VINF_SUCCESS;

    RTMemFree(pThis->webcam.paFrameDesc);
    RT_ZERO(pThis->webcam);

    UWLOGFLEAVERC(rc);
    return rc;
}

static int usbWebcamControl(USBWEBCAM *pThis, bool fResponse, void *pvUser,
                            const VRDEVIDEOINCTRLHDR *pControlHdr, uint32_t cbCtrl)
{
    RT_NOREF3(fResponse, pvUser, cbCtrl);
    int rc = VINF_SUCCESS;

    UWLOG(("sel 0x%04X, type 0x%04X, cb %d, flags 0x%02X, status 0x%02X\n",
            pControlHdr->u16ControlSelector,
            pControlHdr->u16RequestType,
            pControlHdr->u16ParmSize,
            pControlHdr->u8Flags,
            pControlHdr->u8Status));

    /* Find the relevant control, check which operation is this. */
    UWCtrl *pCtrl = pThis->pCtrls->VRDEControlSelToCtrl(pControlHdr->u16ControlSelector);

    if (pCtrl)
    {
        if ((pControlHdr->u8Flags & VRDE_VIDEOIN_CTRLHDR_F_FAIL) == 0)
        {
            const uint8_t *pu8ControlParms = (const uint8_t *)pControlHdr + sizeof(VRDEVIDEOINCTRLHDR);
            uint32_t cbControlParms = pControlHdr->u16ParmSize;

            switch(pControlHdr->u16RequestType)
            {
                case VRDE_VIDEOIN_CTRLREQ_SET_CUR:
                    pCtrl->NotifySet(pControlHdr, pu8ControlParms, cbControlParms);
                    break;
                case VRDE_VIDEOIN_CTRLREQ_GET_CUR:
                    pCtrl->NotifyCur(pControlHdr, pu8ControlParms, cbControlParms);
                    break;
                case VRDE_VIDEOIN_CTRLREQ_GET_MIN:
                    pCtrl->NotifyMin(pControlHdr, pu8ControlParms, cbControlParms);
                    break;
                case VRDE_VIDEOIN_CTRLREQ_GET_MAX:
                    pCtrl->NotifyMax(pControlHdr, pu8ControlParms, cbControlParms);
                    break;
                case VRDE_VIDEOIN_CTRLREQ_GET_RES:
                    pCtrl->NotifyRes(pControlHdr, pu8ControlParms, cbControlParms);
                    break;
                case VRDE_VIDEOIN_CTRLREQ_GET_INFO:
                    pCtrl->NotifyInfo(pControlHdr, pu8ControlParms, cbControlParms);
                    break;
                case VRDE_VIDEOIN_CTRLREQ_GET_DEF:
                    pCtrl->NotifyDef(pControlHdr, pu8ControlParms, cbControlParms);
                    break;
                case VRDE_VIDEOIN_CTRLREQ_GET_LEN:
                default:
                    break;
            }
        }
        else
        {
            /** @todo generate a control interrupt event from pControlHdr->u8Status.
             * Only for async controls, skip not async, if any?
             * Equivalent to the result of a GET_CUR request on VC_REQUEST_ERROR_CODE_CONTROL.
             */
        }
    }
    else
    {
        UWLOG(("Unsupported control selector 0x%04X\n", pControlHdr->u16ControlSelector));
        AssertFailed();
        rc = VERR_NOT_SUPPORTED;
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static int usbWebcamFrame(USBWEBCAM *pThis, const VRDEVIDEOINPAYLOADHDR *pHdr, uint32_t cbHdr,
                          const void *pvFrame, uint32_t cbFrame)
{
    RT_NOREF1(cbHdr);
    int rc = VINF_SUCCESS;

    UWLOG(("len %d, info 0x%02X, PT %u, ST %u, reserved 0x%04X, cb %d\n",
            pHdr->u8HeaderLength,
            pHdr->u8HeaderInfo,
            pHdr->u32PresentationTime,
            pHdr->u32SourceTimeClock,
            pHdr->u16Reserved,
            cbFrame));

    if (pThis->enmStreamStatus == UW_STREAM_STATUS_ON)
    {
        rc = usbWebcamFrameUpdate(pThis, (const uint8_t *)pvFrame, cbFrame, pHdr);
        if (RT_SUCCESS(rc))
        {
            /* Put the frame into queue to be delivered to the guest. */
            usbWebcamFrameAddRef(pThis->pLatestFrame);
            usbWebcamFrameQueueAdd(pThis, pThis->pLatestFrame);

            /* Check if there are pending bulk-in URBs and complete them. */
            usbWebcamQueueComplete(pThis, &pThis->urbQueues.BulkIn, usbWebcamStream, true /* fDataOnly */);
        }
    }
    else
    {
        UWLOG(("Streaming not enabled, frame dropped\n"));
        rc = VERR_INVALID_STATE;
    }

    UWLOGFLEAVERC(rc);
    return rc;
}


/*
 * USBWEBCAM helpers.
 */

DECLINLINE(int) usbWebcamLock(PUSBWEBCAM pThis)
{
#ifdef UWPROFILE
    uint64_t u64Time = RTTimeNanoTS();
#endif

    int rc = RTCritSectEnter(&pThis->lockWebcam);

#ifdef UWPROFILE
    u64Time = RTTimeNanoTS() - u64Time;
    u64Time /= 1000;
    if (u64Time >= RT_ELEMENTS(pThis->profile.acLockDelay))
    {
        u64Time = RT_ELEMENTS(pThis->profile.acLockDelay) - 1;
    }
    pThis->profile.acLockDelay[u64Time] += 1;
#endif

    return rc;
}

DECLINLINE(void) usbWebcamUnlock(PUSBWEBCAM pThis)
{
    RTCritSectLeave(&pThis->lockWebcam);
}

static void usbWebcamInitInterfaces(PUSBWEBCAM pThis)
{
    /* Init interfaces for a webcam device:
     * VideoControl interface
     *    Interface
     *    Camera Terminal
     *    Processing Unit
     *    Output Terminal
     * Video Streaming interface
     *    Interface
     */
    WEBCAMINTERFACE *pInterface;

    RT_ZERO(pThis->aInterfaces);

    pInterface = &pThis->aInterfaces[USBWEBCAM_INTERFACE_CONTROL];
    pInterface->pCtrlErrorCode = pThis->apControlsVC[VUSB_V_VC_REQUEST_ERROR_CODE_CONTROL];
    pInterface->paUnits        = &pThis->aUnitsVC[0];
    pInterface->cUnits         = RT_ELEMENTS(pThis->aUnitsVC);

    pInterface = &pThis->aInterfaces[USBWEBCAM_INTERFACE_STREAMING];
    pInterface->pCtrlErrorCode = pThis->apControlsVS[VUSB_V_VS_STREAM_ERROR_CODE_CONTROL];
    pInterface->paUnits        = &pThis->aUnitsVS[0];
    pInterface->cUnits         = RT_ELEMENTS(pThis->aUnitsVS);


    RT_ZERO(pThis->aUnitsVC);

    pThis->aUnitsVC[0].papControls = &pThis->apControlsVC[0];
    pThis->aUnitsVC[0].cControls  = RT_ELEMENTS(pThis->apControlsVC);

    pThis->aUnitsVC[1].papControls = &pThis->apControlsCT[0];
    pThis->aUnitsVC[1].cControls  = RT_ELEMENTS(pThis->apControlsCT);

    pThis->aUnitsVC[2].papControls = &pThis->apControlsPU[0];
    pThis->aUnitsVC[2].cControls  = RT_ELEMENTS(pThis->apControlsPU);

    pThis->aUnitsVC[3].papControls = &pThis->apControlsOT[0];
    pThis->aUnitsVC[3].cControls  = RT_ELEMENTS(pThis->apControlsOT);
    AssertCompile(USBWEBCAM_NUM_UNITS_CONTROL == 4);


    RT_ZERO(pThis->aUnitsVS);

    pThis->aUnitsVS[0].papControls = &pThis->apControlsVS[0];
    pThis->aUnitsVS[0].cControls  = RT_ELEMENTS(pThis->apControlsVS);
    AssertCompile(USBWEBCAM_NUM_UNITS_STREAMING == 1);


    RT_ZERO(pThis->apControlsVC);
    pThis->apControlsVC[VUSB_V_VC_CONTROL_UNDEFINED         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsVC[VUSB_V_VC_VIDEO_POWER_MODE_CONTROL  ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsVC[VUSB_V_VC_REQUEST_ERROR_CODE_CONTROL] = &pThis->pCtrls->ctrl_VC_REQUEST_ERROR_CODE_CONTROL;

    RT_ZERO(pThis->apControlsCT);
    pThis->apControlsCT[VUSB_V_CT_CONTROL_UNDEFINED             ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_SCANNING_MODE_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_AE_MODE_CONTROL               ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_AE_PRIORITY_CONTROL           ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_EXPOSURE_TIME_RELATIVE_CONTROL] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_FOCUS_ABSOLUTE_CONTROL        ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_FOCUS_RELATIVE_CONTROL        ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_FOCUS_AUTO_CONTROL            ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_IRIS_ABSOLUTE_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_IRIS_RELATIVE_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_ZOOM_ABSOLUTE_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_ZOOM_RELATIVE_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_PANTILT_ABSOLUTE_CONTROL      ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_PANTILT_RELATIVE_CONTROL      ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_ROLL_ABSOLUTE_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_ROLL_RELATIVE_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsCT[VUSB_V_CT_PRIVACY_CONTROL               ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;

    RT_ZERO(pThis->apControlsPU);
    pThis->apControlsPU[VUSB_V_PU_CONTROL_UNDEFINED                     ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_BACKLIGHT_COMPENSATION_CONTROL        ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_BRIGHTNESS_CONTROL                    ] = &pThis->pCtrls->ctrl_PU_BRIGHTNESS_CONTROL;
    pThis->apControlsPU[VUSB_V_PU_CONTRAST_CONTROL                      ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_GAIN_CONTROL                          ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_POWER_LINE_FREQUENCY_CONTROL          ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_HUE_CONTROL                           ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_SATURATION_CONTROL                    ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_SHARPNESS_CONTROL                     ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_GAMMA_CONTROL                         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_WHITE_BALANCE_TEMPERATURE_CONTROL     ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_WHITE_BALANCE_COMPONENT_CONTROL       ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL  ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_DIGITAL_MULTIPLIER_CONTROL            ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL      ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_HUE_AUTO_CONTROL                      ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_ANALOG_VIDEO_STANDARD_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsPU[VUSB_V_PU_ANALOG_LOCK_STATUS_CONTROL            ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;

    RT_ZERO(pThis->apControlsOT);
    pThis->apControlsOT[VUSB_V_TE_CONTROL_UNDEFINED] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;

    RT_ZERO(pThis->apControlsVS);
    pThis->apControlsVS[VUSB_V_VS_CONTROL_UNDEFINED           ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsVS[VUSB_V_VS_PROBE_CONTROL               ] = &pThis->pCtrls->ctrl_VS_PROBE_CONTROL;
    pThis->apControlsVS[VUSB_V_VS_COMMIT_CONTROL              ] = &pThis->pCtrls->ctrl_VS_COMMIT_CONTROL;
    pThis->apControlsVS[VUSB_V_VS_STILL_PROBE_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsVS[VUSB_V_VS_STILL_COMMIT_CONTROL        ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsVS[VUSB_V_VS_STILL_IMAGE_TRIGGER_CONTROL ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsVS[VUSB_V_VS_STREAM_ERROR_CODE_CONTROL   ] = &pThis->pCtrls->ctrl_VS_STREAM_ERROR_CODE_CONTROL;
    pThis->apControlsVS[VUSB_V_VS_GENERATE_KEY_FRAME_CONTROL  ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsVS[VUSB_V_VS_UPDATE_FRAME_SEGMENT_CONTROL] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;
    pThis->apControlsVS[VUSB_V_VS_SYNCH_DELAY_CONTROL         ] = &pThis->pCtrls->ctrl_CONTROL_UNDEFINED;


    unsigned iInterface;
    for (iInterface = 0; iInterface < RT_ELEMENTS(pThis->aInterfaces); iInterface++)
    {
        pInterface = &pThis->aInterfaces[iInterface];
        int iUnit;
        for (iUnit = 0; iUnit < pInterface->cUnits; iUnit++)
        {
            WEBCAMUNIT *pUnit = &pInterface->paUnits[iUnit];
            int iControl;
            for (iControl = 0; iControl < pUnit->cControls; iControl++)
            {
                pUnit->papControls[iControl]->Init(pThis);
            }
        }
    }
}

static DECLCALLBACK(void) usbWebcamTimerCallback(RTTIMERLR hTimerLR, void *pvUser, uint64_t iTick)
{
    RT_NOREF1(hTimerLR);
    PUSBWEBCAM pThis = (PUSBWEBCAM)pvUser;

    UWLOGF(("iTick %lld\n",
            iTick));

    int rc = usbWebcamLock(pThis);
    if (RT_FAILURE(rc))
    {
        return;
    }

    if (pThis->fStreamError)
    {
        /* Special "stream error" mode. usbWebcamStream sends empty data payloads to the guest. */
        usbWebcamQueueComplete(pThis, &pThis->urbQueues.BulkIn, usbWebcamStream, true /* fDataOnly */);
    }
    else
    {
        /* Resend last frame if there were no frames for too long.
         * If frames are not sent periodically, guest application may be upset.
         */
        if (pThis->u64LastFrameMS != 0)
        {
            uint64_t u64Elapsed = RTTimeMilliTS() - pThis->u64LastFrameMS;

            if (u64Elapsed >= 200)
            {
               UWLOG(("Resending frame after %lld, list empty %d\n", u64Elapsed, RTListIsEmpty(&pThis->listReadyFrames)));

               /* Only if there are no ready frames and there is a latest frame. */
               if (pThis->pLatestFrame)
               {
                   if (RTListIsEmpty(&pThis->listReadyFrames))
                   {
                       /* Put the latest frame into queue to be delivered to the guest. */
                       usbWebcamFrameAddRef(pThis->pLatestFrame);
                       usbWebcamFrameQueueAdd(pThis, pThis->pLatestFrame);
                   }

                   /* Complete pending URBs if any */
                   usbWebcamQueueComplete(pThis, &pThis->urbQueues.BulkIn, usbWebcamStream, true /* fDataOnly */);
               }
            }
        }
    }

    usbWebcamUnlock(pThis);
}


/*
 * The device interface.
 */

static DECLCALLBACK(int) usbWebcamUpAttached(PPDMIWEBCAMDEV pInterface, uint64_t idDevice,
                                             struct VRDEVIDEOINDEVICEDESC const *pDeviceDesc, uint32_t cbDeviceDesc,
                                             uint32_t uVersion, uint32_t fCapabilities)
{
    PUSBWEBCAM pThis = RT_FROM_MEMBER(pInterface, USBWEBCAM, IWebcamDev);

    UWLOG(("uVersion %d, fCapabilities 0x%08X, idDevice %llu(%llu), cbDeviceDesc %d\n%.*Rhxd\n",
           uVersion, fCapabilities, idDevice, pThis->webcam.idDevice, cbDeviceDesc, cbDeviceDesc, pDeviceDesc));

    int rc = usbWebcamLock(pThis);
    if (RT_FAILURE(rc))
    {
        return rc;
    }

    if (pThis->enmState == UW_STATE_NO_WEBCAM)
    {
        if (cbDeviceDesc >= sizeof(VRDEVIDEOINDEVICEDESC))
            rc = usbWebcamAttach(pThis, pDeviceDesc, cbDeviceDesc);
        else
            rc = VERR_INVALID_PARAMETER;

        if (RT_SUCCESS(rc))
        {
            pThis->webcam.idDevice = idDevice;
            pThis->webcam.u32Version = uVersion;
            pThis->webcam.fu32Capabilities = fCapabilities;

            /* Started the process of querying the webcam controls.
             * Controls will still not work until they will get
             * the info about the webcam.
             */
            UWLOG(("Enabled PROXY mode\n"));
            usbWebcamOnProxyModeChange(pThis, UW_STATE_PROXY);
        }
    }
    else
    {
        /* Only one attached webcam allowed. */
        UWLOG(("Invalid state %d\n", pThis->enmState));
        AssertFailed(); /* The PDM driver must not allow this. */
        rc = VERR_INVALID_STATE;
    }

    usbWebcamUnlock(pThis);

    UWLOGFLEAVERC(rc);
    return rc;
}

static DECLCALLBACK(void) usbWebcamUpDetached(PPDMIWEBCAMDEV pInterface,
                                              uint64_t idDevice)
{
    /** @todo reset all controls */
    PUSBWEBCAM pThis = RT_FROM_MEMBER(pInterface, USBWEBCAM, IWebcamDev);

    UWLOG(("idDevice %llu(%llu)\n",
           idDevice, pThis->webcam.idDevice));

    int rc = usbWebcamLock(pThis);
    if (RT_FAILURE(rc))
    {
        return;
    }

    UWLOG(("State %d\n", pThis->enmState));

    if (pThis->enmState == UW_STATE_PROXY)
    {
        rc = usbWebcamDetach(pThis);

        if (RT_SUCCESS(rc))
        {
            pThis->webcam.idDevice = 0;

            UWLOG(("Disabled PROXY mode\n"));
            usbWebcamOnProxyModeChange(pThis, UW_STATE_NO_WEBCAM);
        }
    }

    /* Inform emulated USB. */
    if (pThis->emulatedUSB.pfnEUSB)
    {
        pThis->emulatedUSB.pfnEUSB(pThis->emulatedUSB.pvEUSB, pThis->emulatedUSB.pszId, 0, NULL, 0);
    }

    usbWebcamUnlock(pThis);

    UWLOGFLEAVERC(rc);
    return;
}

static DECLCALLBACK(void) usbWebcamUpControl(PPDMIWEBCAMDEV pInterface, bool fResponse, void *pvUser, uint64_t idDevice,
                                             const struct VRDEVIDEOINCTRLHDR *pCtrl, uint32_t cbCtrl)
{
    PUSBWEBCAM pThis = RT_FROM_MEMBER(pInterface, USBWEBCAM, IWebcamDev);

    UWLOG(("idDevice %llu(%llu), cbCtrl %d\n%.*Rhxd\n",
           idDevice, pThis->webcam.idDevice, cbCtrl, cbCtrl, pCtrl));

    int rc = usbWebcamLock(pThis);
    if (RT_FAILURE(rc))
        return;

    if (pThis->webcam.idDevice == idDevice)
    {
        if (pThis->enmState == UW_STATE_PROXY)
        {
            if (   cbCtrl >= sizeof(VRDEVIDEOINCTRLHDR)
                && cbCtrl >= sizeof(VRDEVIDEOINCTRLHDR) + pCtrl->u16ParmSize)
                rc = usbWebcamControl(pThis, fResponse, pvUser, pCtrl, cbCtrl);
            else
                rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            /* Probably some late response. Ignore. */
            UWLOG(("Ignoring in state %d\n", pThis->enmState));
        }
    }
    else
    {
        UWLOG(("Invalid device!\n"));
        AssertFailed();
        rc = VERR_INVALID_STATE;
    }

    usbWebcamUnlock(pThis);

    UWLOGFLEAVERC(rc);
    return;
}


static DECLCALLBACK(void) usbWebcamUpFrame(PPDMIWEBCAMDEV pInterface, uint64_t idDevice,
                                           struct VRDEVIDEOINPAYLOADHDR const *pHeader,
                                           uint32_t cbHeader, const void *pvFrame, uint32_t cbFrame)
{
    PUSBWEBCAM pThis = RT_FROM_MEMBER(pInterface, USBWEBCAM, IWebcamDev);

    UWLOG(("idDevice %llu(%llu), cbFrame %d, hdr:\n%.*Rhxd\n",
           idDevice, pThis->webcam.idDevice, cbFrame, RT_MIN(cbHeader, 32), pHeader));

    int rc = usbWebcamLock(pThis);
    if (RT_FAILURE(rc))
        return;

    if (pThis->webcam.idDevice == idDevice)
    {
        if (UWLOGENABLED())
        {
            /* Count this frame. */
            uint64_t u64NowMS = RTTimeMilliTS();
            uint64_t u64ElapsedMS = u64NowMS - pThis->stat.u64FPSStartMS;

            if (u64ElapsedMS >= 1000)
            {
                if (pThis->stat.u64FPSStartMS != 0)
                {
                    UWLOG(("FPS %lld/%lld\n", pThis->stat.cFPS, u64ElapsedMS));
                }
                pThis->stat.u64FPSStartMS = u64NowMS;
                pThis->stat.cFPS = 1;
            }
            else
                pThis->stat.cFPS++;
        }

        /* Process the frame. */
        if (pThis->enmState == UW_STATE_PROXY)
        {
            if (   cbHeader >= sizeof(VRDEVIDEOINPAYLOADHDR)
                && cbHeader >= pHeader->u8HeaderLength)
            {
                rc = usbWebcamFrame(pThis, pHeader, cbHeader, pvFrame, cbFrame);
                if (RT_FAILURE(rc))
                    UWLOG(("Frame dropped %Rrc\n", rc));
            }
            else
                rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            /* Probably some late response. Ignore. */
            UWLOG(("Ignoring in state %d\n", pThis->enmState));
        }
    }
    else
    {
        UWLOG(("Invalid device!\n"));
        AssertFailed();
        rc = VERR_INVALID_STATE;
    }

    usbWebcamUnlock(pThis);

    UWLOGFLEAVERC(rc);
    return;
}


/**
 * @callback_method_impl{PFNPDMTHREADUSB}
 */
static DECLCALLBACK(int) usbWebcamThreadBackendWorker(PPDMUSBINS pUsbIns, PPDMTHREAD pThread)
{
    UWLOG(("#%d: state %d\n", pUsbIns->iInstance, pThread->enmState));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        rc = RTReqQueueProcess(pThis->hRQBackend, RT_INDEFINITE_WAIT);

        AssertMsg(rc == VWRN_STATE_CHANGED,
                  ("Left RTReqProcess and error code is not VWRN_STATE_CHANGED rc=%Rrc\n",
                   rc));
    }

    UWLOGFLEAVERC(rc);
    return rc;
}


/**
 * Request callback used by usbWebcamThreadBackendWakeup to forc
 * RTReqQueueProcess to return.
 */
static DECLCALLBACK(int) usbWebcamThreadBackendWakeupFunc(PUSBWEBCAM pThis)
{
    NOREF(pThis);
    /* Returning a VINF_* will cause RTReqQueueProcess return. */
    return VWRN_STATE_CHANGED;
}


/**
 * @callback_method_impl{PFNPDMTHREADUSB}
 */
static DECLCALLBACK(int) usbWebcamThreadBackendWakeup(PPDMUSBINS pUsbIns, PPDMTHREAD pThread)
{
    RT_NOREF1(pThread);
    UWLOG(("#%d\n", pUsbIns->iInstance));

    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    AssertReturn(pThis->hRQBackend != NIL_RTREQQUEUE, VERR_INVALID_STATE);

    PRTREQ pReq;
    int rc = RTReqQueueCall(pThis->hRQBackend, &pReq, 10000, (PFNRT)usbWebcamThreadBackendWakeupFunc, 1, pThis);
    if (RT_SUCCESS(rc))
        RTReqRelease(pReq);
    else
    {
        /** @todo handle VERR_TIMEOUT */
        UWLOG(("RTReqQueueCall failed rc=%Rrc\n", rc));
    }

    return rc;
}

static int usbWebcamEmulatedUSBInit(PPDMUSBINS pUsbIns, PUSBWEBCAM pThis, PCFGMNODE pCfg)
{
    int rc = VINF_SUCCESS;
    PCFGMNODE pEUSBNode = pUsbIns->pHlpR3->pfnCFGMGetChild(pCfg, "EmulatedUSB");
    if (pEUSBNode)
    {
        rc = pUsbIns->pHlpR3->pfnCFGMQueryStringAlloc(pEUSBNode, "Id", &pThis->emulatedUSB.pszId);
        if (RT_SUCCESS(rc))
        {
            RTUUID UuidEmulatedUsbIf;
            rc = RTUuidFromStr(&UuidEmulatedUsbIf, EMULATEDUSBIF_OID); AssertRC(rc);

            PEMULATEDUSBIF pEmulatedUsbIf = (PEMULATEDUSBIF)PDMUsbHlpQueryGenericUserObject(pUsbIns, &UuidEmulatedUsbIf);
            AssertPtrReturn(pEmulatedUsbIf, VERR_INVALID_PARAMETER);

            rc = pEmulatedUsbIf->pfnQueryEmulatedUsbDataById(pEmulatedUsbIf->pvUser, pThis->emulatedUSB.pszId,
                                                             (void **)&pThis->emulatedUSB.pfnEUSB, &pThis->emulatedUSB.pvEUSB, NULL /*ppvObject*/);
            if (RT_SUCCESS(rc))
                UWLOG(("EmulatedUSB: id %s, pfn %p\n", pThis->emulatedUSB.pszId, pThis->emulatedUSB.pfnEUSB));
        }
    }

    UWLOGFLEAVERC(rc);
    return rc;
}

static void usbWebcamEmulatedUSBDestroy(PPDMUSBINS pUsbIns, PUSBWEBCAM pThis)
{
    if (pThis->emulatedUSB.pszId)
    {
        PDMUsbHlpMMHeapFree(pUsbIns, pThis->emulatedUSB.pszId);
        pThis->emulatedUSB.pszId = NULL;
    }
    RT_ZERO(pThis->emulatedUSB);
}


static void usbWebcamBackendStop(PUSBWEBCAM pThis)
{
    PCPDMUSBHLP pHlp = pThis->pUsbIns->pHlpR3;
    int rc = usbWebcamLock(pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->enmStoppedStreamStatus = pThis->enmStreamStatus;

        usbWebcamStreamAbort(pThis);

        usbWebcamUnlock(pThis);
    }

    /* Make sure that backend operations are complete. */
    if (pThis->pThreadBackend)
        pHlp->pfnThreadSuspend(pThis->pThreadBackend);
}

static void usbWebcamBackendResume(PUSBWEBCAM pThis)
{
    int rc = usbWebcamLock(pThis);
    if (RT_SUCCESS(rc))
    {
        if (pThis->enmStoppedStreamStatus == UW_STREAM_STATUS_ON)
        {
            usbWebcamStreamOn(pThis);

            pThis->enmStoppedStreamStatus = pThis->enmStreamStatus;
        }

        usbWebcamUnlock(pThis);
    }
}



/*********************************************************************************************************************************
*   PDM USB Device Methods                                                                                                       *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) usbWebcamQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PUSBWEBCAM pThis = RT_FROM_MEMBER(pInterface, USBWEBCAM, IBase);
    UWLOGF(("pszIID:%s\n", pszIID));

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIWEBCAMDEV, &pThis->IWebcamDev);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    return NULL;
}


/**
 * @interface_method_impl{PDMUSBREG,pfnWakeup}
 */
static DECLCALLBACK(int) usbWebcamWakeup(PPDMUSBINS pUsbIns)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);

    return RTSemEventSignal(pThis->hEvtDoneQueue);
}


/**
 * @interface_method_impl{PDMUSBREG,pfnUrbReap}
 */
static DECLCALLBACK(PVUSBURB) usbWebcamUrbReap(PPDMUSBINS pUsbIns, RTMSINTERVAL cMillies)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);

#ifdef UWPROFILE
    pThis->profile.cReap++;
#endif

    PVUSBURB pUrb = NULL;

    int rc = usbWebcamLock(pThis);

    if (RT_SUCCESS(rc))
    {
        bool fLock = true;

        pUrb = urbQueueRemoveHead(&pThis->urbQueues.Done);

        if (!pUrb && cMillies)
        {
            pThis->fHaveDoneQueueWaiter = true;
            usbWebcamUnlock(pThis);

            RTSemEventWait(pThis->hEvtDoneQueue, cMillies);

            rc = usbWebcamLock(pThis);
            if (RT_SUCCESS(rc))
            {
                pThis->fHaveDoneQueueWaiter = false;
                fLock = true;
                pUrb = urbQueueRemoveHead(&pThis->urbQueues.Done);
            }
        }

        if (fLock)
        {
            usbWebcamUnlock(pThis);
        }
    }

    if (pUrb)
    {
        UWLOGF(("iInstance=%d cMillies=%d pUrb=%p\n",
                pUsbIns->iInstance, cMillies, pUrb));
    }

    return pUrb;
}


/**
 * @interface_method_impl{PDMUSBREG,pfnUrbCancel}
 */
static DECLCALLBACK(int) usbWebcamUrbCancel(PPDMUSBINS pUsbIns, PVUSBURB pUrb)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    UWLOG(("iInstance:%d pUrb:%p:%s EndPt:%x\n",
           pUsbIns->iInstance, pUrb, pUrb->pszDesc, pUrb->EndPt));

    int rc = usbWebcamLock(pThis);

    if (RT_SUCCESS(rc))
    {
        /* Look for the URB in all queues. */
        bool fRemoved = urbQueueRemove(&pThis->urbQueues.BulkIn, pUrb);
        if (!fRemoved)
        {
            fRemoved = urbQueueRemove(&pThis->urbQueues.IntrIn, pUrb);
        }

        if (fRemoved)
        {
            UWLOG(("Cancelled URB pUrb:%p:%s EndPt:%x\n",
                    pUrb, pUrb->pszDesc, pUrb->EndPt));
            usbWebcamLinkDone(pThis, pUrb);
        }

        usbWebcamUnlock(pThis);
    }

    UWLOGFLEAVERC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMUSBREG,pfnUrbQueue}
 */
static DECLCALLBACK(int) usbWebcamUrbQueue(PPDMUSBINS pUsbIns, PVUSBURB pUrb)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    UWLOGF(("iInstance:%d pUrb:%p:%s EndPt:0x%02x\n",
             pUsbIns->iInstance, pUrb, pUrb->pszDesc, pUrb->EndPt));

#ifdef UWPROFILE
    pThis->profile.cQueue++;
#endif

    int rc = usbWebcamLock(pThis);

    if (RT_SUCCESS(rc))
    {
        switch (pUrb->EndPt)
        {
            case 0: /* Control */
                rc = usbWebcamHandlePipeDefault(pThis, pUrb);
                break;
            case 1: /* VC Interrupt IN */
                rc = usbWebcamHandlePipeInterrupt(pThis, pUrb);
                break;
            case 2: /* VS Bulk IN */
                rc = usbWebcamHandlePipeBulkIn(pThis, pUrb);
                break;
            default:
                AssertMsgFailed(("EndPt:%x\n", pUrb->EndPt));
                rc = VERR_VUSB_FAILED_TO_QUEUE_URB;
        }

        usbWebcamUnlock(pThis);
    }
    else
    {
        rc = VERR_VUSB_FAILED_TO_QUEUE_URB;
    }

    UWLOGFLEAVERC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMUSBREG,pfnUsbSetInterface}
 */
static DECLCALLBACK(int) usbWebcamUsbSetInterface(PPDMUSBINS pUsbIns, uint8_t bInterfaceNumber, uint8_t bAlternateSetting)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);

    UWLOG(("inum %d, alt set %d\n", bInterfaceNumber, bAlternateSetting));

    if (bInterfaceNumber == USBWEBCAM_INTERFACE_STREAMING && bAlternateSetting == 0)
    {
        int rc = usbWebcamLock(pThis);

        if (RT_SUCCESS(rc))
        {
            usbWebcamStreamAbort(pThis);

            usbWebcamUnlock(pThis);
        }
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMUSBREG,pfnGetDescriptorCache}
 */
static DECLCALLBACK(PCPDMUSBDESCCACHE) usbWebcamGetDescriptorCache(PPDMUSBINS pUsbIns)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    UWLOGF(("pUsbIns:%p dc %p\n", pUsbIns, pThis->pDescCache));
    Assert(pThis->pDescCache);
    return pThis->pDescCache;
}


/**
 * @interface_method_impl{PDMUSBREG,pfnDriverDetach}
 */
static DECLCALLBACK(void) usbWebcamDriverDetach(PPDMUSBINS pUsbIns, unsigned iLUN, uint32_t fFlags)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    UWLOG(("pUsbIns:%p iLUN %d\n", pUsbIns, iLUN));

    NOREF(fFlags);

    usbWebcamBackendStop(pThis);
}


/**
 * @interface_method_impl{PDMUSBREG,pfnVMPowerOff}
 */
static DECLCALLBACK(void) usbWebcamVMPowerOff(PPDMUSBINS pUsbIns)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    UWLOG(("pUsbIns:%p\n", pUsbIns));

    usbWebcamBackendStop(pThis);
}


/**
 * @interface_method_impl{PDMUSBREG,pfnVMResume}
 */
static DECLCALLBACK(void) usbWebcamVMResume(PPDMUSBINS pUsbIns)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    UWLOG(("pUsbIns:%p\n", pUsbIns));

    usbWebcamBackendResume(pThis);
}


/**
 * @interface_method_impl{PDMUSBREG,pfnVMSuspend}
 */
static DECLCALLBACK(void) usbWebcamVMSuspend(PPDMUSBINS pUsbIns)
{
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    UWLOG(("pUsbIns:%p\n", pUsbIns));

    usbWebcamBackendStop(pThis);
}



/**
 * @interface_method_impl{PDMUSBREG,pfnDestruct}
 */
static DECLCALLBACK(void) usbWebcamDestruct(PPDMUSBINS pUsbIns)
{
    PDMUSB_CHECK_VERSIONS_RETURN_VOID(pUsbIns);
    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    PCPDMUSBHLP pHlp = pUsbIns->pHlpR3;
    UWLOG(("iInstance:%d\n", pUsbIns->iInstance));

    if (pThis->hTimerStream)
    {
        RTTimerLRDestroy(pThis->hTimerStream);
        pThis->hTimerStream = NULL;
    }

    if (pThis->pThreadBackend)
    {
        int rcThread = VINF_SUCCESS;
        pHlp->pfnThreadDestroy(pThis->pThreadBackend, &rcThread);
        pThis->pThreadBackend = NULL;
    }

    if (pThis->hRQBackend != NIL_RTREQQUEUE)
    {
        int rc = RTReqQueueDestroy(pThis->hRQBackend);
        AssertRC(rc);
        pThis->hRQBackend = NIL_RTREQQUEUE;
    }

    if (RTCritSectIsInitialized(&pThis->lockWebcam))
    {
        RTCritSectEnter(&pThis->lockWebcam);
        RTCritSectLeave(&pThis->lockWebcam);
        RTCritSectDelete(&pThis->lockWebcam);
    }

    if (pThis->hEvtDoneQueue != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(pThis->hEvtDoneQueue);
        pThis->hEvtDoneQueue = NIL_RTSEMEVENT;
    }

    /** @todo URB queues cleanup if needed. */

    usbWebcamFramesCleanup(pThis);

    RTMemFree(pThis->webcam.paFrameDesc);
    RT_ZERO(pThis->webcam);

    if (pThis->pCtrls)
    {
        delete pThis->pCtrls;
        pThis->pCtrls = NULL;
    }

    usbWebcamDescriptorsFree(pThis->pDescCache);
    pThis->pDescCache = NULL;

    usbWebcamEmulatedUSBDestroy(pUsbIns, pThis);
}


/**
 * @interface_method_impl{PDMUSBREG,pfnConstruct}
 */
static DECLCALLBACK(int) usbWebcamConstruct(PPDMUSBINS pUsbIns, int iInstance, PCFGMNODE pCfg, PCFGMNODE pCfgGlobal)
{
    RT_NOREF1(pCfgGlobal);
    PDMUSB_CHECK_VERSIONS_RETURN(pUsbIns);

    PUSBWEBCAM pThis = PDMINS_2_DATA(pUsbIns, PUSBWEBCAM);
    PCPDMUSBHLP pHlp = pUsbIns->pHlpR3;
    UWLOG(("iInstance:%d\n", pUsbIns->iInstance));

    pThis->iInstance = iInstance;
    pThis->pUsbIns = pUsbIns;

    /*
     * Initialize everything.
     */
    pThis->fConstructed = false;

    pThis->IBase.pfnQueryInterface = usbWebcamQueryInterface;

    pThis->IWebcamDev.pfnAttached = usbWebcamUpAttached;
    pThis->IWebcamDev.pfnDetached = usbWebcamUpDetached;
    pThis->IWebcamDev.pfnControl  = usbWebcamUpControl;
    pThis->IWebcamDev.pfnFrame    = usbWebcamUpFrame;

    urbQueueInit(&pThis->urbQueues.BulkIn);
    urbQueueInit(&pThis->urbQueues.IntrIn);
    urbQueueInit(&pThis->urbQueues.Done);

    pThis->u32MaxPayloadTransferSize = USBWEBCAM_MAX_PAYLOAD_TRANSFER_SIZE;

    pThis->fStreamError = false;
    pThis->fu8FID = 0;
    pThis->cbLeft = 0;
    pThis->cbPayloadLeft = 0;
    pThis->u64StartFrameMS = 0;
    pThis->u64LastFrameMS = 0;
    pThis->enmState = UW_STATE_NO_DRIVER;
    pThis->enmStreamStatus = UW_STREAM_STATUS_OFF;
    pThis->enmStoppedStreamStatus = UW_STREAM_STATUS_OFF;

    pThis->pszStandaloneJPG = NULL;

    RT_ZERO(pThis->webcam);

    RT_ZERO(pThis->aPendingFrameAlloc);
    RTListInit(&pThis->listFreeFrames);
    RTListInit(&pThis->listReadyFrames);
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aPendingFrameAlloc); i++)
        RTListAppend(&pThis->listFreeFrames, &pThis->aPendingFrameAlloc[i].nodeFrame);
    pThis->pCurrentFrame = NULL;
    pThis->pLatestFrame = NULL;

    RTListInit(&pThis->listFreeStatusEvents);
    RTListInit(&pThis->listStatusEvents);
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aStatusEventsAlloc); i++)
        RTListAppend(&pThis->listFreeStatusEvents, &pThis->aStatusEventsAlloc[i].nodeEvent);

    RT_ZERO(pThis->stat);
#ifdef UWPROFILE
    RT_ZERO(pThis->profile);
#endif

    int rc = usbWebcamDescriptorsBuild(&pThis->pDescCache, iInstance, NULL, 0, 0);
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: Failed to create default descriptors"));

    /* Construct control objects. */
    pThis->pCtrls = new WEBCAMCTRLS();
    if (!pThis->pCtrls)
        return PDMUsbHlpVMSetError(pUsbIns, VERR_NO_MEMORY, RT_SRC_POS, N_("USBWEBCAM: Failed to allocate controls"));

    rc = RTCritSectInit(&pThis->lockWebcam);
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: Failed to create lock"));

    /* Create a suspended timer for standalone streaming. */
    rc = RTTimerLRCreateEx(&pThis->hTimerStream, 200 * UINT64_C(1000000), 0, usbWebcamTimerCallback, pThis);
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: Failed to create timer"));

    rc = RTReqQueueCreate(&pThis->hRQBackend);
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: Failed to create request queue"));

    pThis->hEvtDoneQueue = NIL_RTSEMEVENT;
    rc = RTSemEventCreate(&pThis->hEvtDoneQueue);
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: Failed to create event semaphore"));

    rc = PDMUsbHlpThreadCreate(pUsbIns,
                               &pThis->pThreadBackend,
                               pThis,
                               usbWebcamThreadBackendWorker,
                               usbWebcamThreadBackendWakeup,
                               0, RTTHREADTYPE_IO, "UWCBACKEND");

    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: Failed to create control thread"));

    /*
     * Read the configuration.
     */
    rc = pHlp->pfnCFGMQueryU32(pCfg, "MaxPayloadTransferSize",
                               &pThis->u32MaxPayloadTransferSize);
    UWLOG(("MaxPayloadTransferSize = %d (%Rrc)\n", pThis->u32MaxPayloadTransferSize, rc));
    if (RT_FAILURE(rc) || pThis->u32MaxPayloadTransferSize <= sizeof(VUSBVPAYLOADHEADER))
        pThis->u32MaxPayloadTransferSize = USBWEBCAM_MAX_PAYLOAD_TRANSFER_SIZE;

    uint32_t u32MaxFramerate = UINT32_MAX;
    rc = pHlp->pfnCFGMQueryU32(pCfg, "MaxFramerate", &u32MaxFramerate);
    if (RT_FAILURE(rc) || u32MaxFramerate == 0)
        u32MaxFramerate = UINT32_MAX;
    if (u32MaxFramerate != UINT32_MAX)
    {
        pThis->u32MinFrameInterval = 10000000 / u32MaxFramerate;
        UWLOG(("MinFrameInterval %d\n", pThis->u32MinFrameInterval));
    }

    rc = usbWebcamEmulatedUSBInit(pUsbIns, pThis, pCfg);
    if (RT_FAILURE(rc))
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: Failed to read emulated USB config"));

    usbWebcamInitInterfaces(pThis);
    usbWebcamStreamParmsInit(pThis);

    /* Get the driver interface. */
    rc = PDMUsbHlpDriverAttach(pUsbIns, 0 /*iLun*/, &pThis->IBase, &pThis->pDrvBase, "Emulated webcam backend");
    UWLOG(("DriverAttach: %Rrc\n", rc));
    if (rc == VERR_NOT_FOUND)
        return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: no device found"));

    if (RT_SUCCESS(rc))
        pThis->pWebcamDown = (PPDMIWEBCAMDRV)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMIWEBCAMDRV_IID);
    UWLOG(("pWebcamDown: %p\n", pThis->pWebcamDown));

    /* A PDM thread is created in SUSPENDED state. */
    pHlp->pfnThreadResume(pThis->pThreadBackend);

    if (pThis->pWebcamDown)
    {
        pThis->enmState = UW_STATE_NO_WEBCAM;

        /* The driver must be informed that the device is constructed. */
        PRTREQ pReq = NULL;
        rc = RTReqQueueCallVoid(pThis->hRQBackend, &pReq, RT_INDEFINITE_WAIT, (PFNRT)pThis->pWebcamDown->pfnReady, 2,
                                pThis->pWebcamDown, true);
        if (RT_FAILURE(rc))
        {
            return PDMUsbHlpVMSetError(pUsbIns, rc, RT_SRC_POS, N_("USBWEBCAM: Failed to call the backend"));
        }

        RTReqRelease(pReq);

        pThis->fConstructed = true;
    }
    else
    {
        /* No PDM driver, webcam will work in the 'Standalone' mode. */
        LogRel(("USBWEBCAM: standalone mode.\n"));
    }

    return VINF_SUCCESS;
}


/*
 * The USB Webcam registration record.
 */
extern "C" const PDMUSBREG g_DevWebcam =
{
    /* u32Version */
    PDM_USBREG_VERSION,
    /* szName */
    "Webcam",
    /* pszDescription */
    "Emulated USB Webcam",
    /* fFlags */
    0,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(USBWEBCAM),
    /* pfnConstruct */
    usbWebcamConstruct,
    /* pfnDestruct */
    usbWebcamDestruct,
    /* pfnVMInitComplete */
    NULL,
    /* pfnVMPowerOn */
    NULL,
    /* pfnVMReset */
    NULL,
    /* pfnVMSuspend */
    usbWebcamVMSuspend,
    /* pfnVMResume */
    usbWebcamVMResume,
    /* pfnVMPowerOff */
    usbWebcamVMPowerOff,
    /* pfnHotPlugged */
    NULL,
    /* pfnHotUnplugged */
    NULL,
    /* pfnDriverAttach */
    NULL,
    /* pfnDriverDetach */
    usbWebcamDriverDetach,
    /* pfnQueryInterface */
    NULL,
    /* pfnUsbReset */
    NULL,
    /* pfnGetDescriptorCache */
    usbWebcamGetDescriptorCache,
    /* pfnUsbSetConfiguration */
    NULL,
    /* pfnUsbSetInterface */
    usbWebcamUsbSetInterface,
    /* pfnUsbClearHaltedEndpoint */
    NULL,
    /* pfnUrbNew */
    NULL,
    /* pfnUrbQueue */
    usbWebcamUrbQueue,
    /* pfnUrbCancel */
    usbWebcamUrbCancel,
    /* pfnUrbReap */
    usbWebcamUrbReap,
    /* pfnWakeup */
    usbWebcamWakeup,
    /* u32TheEnd */
    PDM_USBREG_VERSION
};

