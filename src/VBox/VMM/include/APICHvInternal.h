/* $Id: APICHvInternal.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * APIC - Advanced Programmable Interrupt Controller - NEM Hyper-V backend, Internal header.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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

#ifndef VMM_INCLUDED_SRC_include_APICHvInternal_h
#define VMM_INCLUDED_SRC_include_APICHvInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/apic.h>
#include <VBox/vmm/pdmapic.h>
#include <VBox/vmm/stam.h>

/** @defgroup grp_apic_hv_int       Internal
 * @ingroup grp_apic
 * @internal
 * @{
 */

#ifdef VBOX_INCLUDED_vmm_pdmapic_h
# if defined(RT_OS_WINDOWS)
/** The NEM Hyper-V APIC backend. */
extern const PDMAPICBACKEND g_ApicNemBackend;
# endif
#endif

/** The APIC hardware version we are emulating. */
#define XAPIC_HARDWARE_VERSION                  XAPIC_HARDWARE_VERSION_P4

#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
#define XAPIC_SVR_VALID                         XAPIC_SVR_VALID_P4
#define XAPIC_ID_BROADCAST_MASK                 XAPIC_ID_BROADCAST_MASK_P4
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif

#define VMCPU_TO_XAPICPAGE(a_pVCpu)             ((PXAPICPAGE)(CTX_SUFF((a_pVCpu)->apic.s.pvApicPage)))
#define VMCPU_TO_CXAPICPAGE(a_pVCpu)            ((PCXAPICPAGE)(CTX_SUFF((a_pVCpu)->apic.s.pvApicPage)))

#define VMCPU_TO_X2APICPAGE(a_pVCpu)            ((PX2APICPAGE)(CTX_SUFF((a_pVCpu)->apic.s.pvApicPage)))
#define VMCPU_TO_CX2APICPAGE(a_pVCpu)           ((PCX2APICPAGE)(CTX_SUFF((a_pVCpu)->apic.s.pvApicPage)))

#define VMCPU_TO_HVAPICCPU(a_pVCpu)             (&(a_pVCpu)->apic.s)
#define VM_TO_HVAPIC(a_pVM)                     (&(a_pVM)->apic.s)
#define VM_TO_HVAPICDEV(a_pVM)                  CTX_SUFF(VM_TO_APIC(a_pVM)->pApicDev)
#ifdef IN_RING3
# define VMCPU_TO_DEVINS(a_pVCpu)               ((a_pVCpu)->pVMR3->apic.s.pDevInsR3)
#elif defined(IN_RING0)
# define VMCPU_TO_DEVINS(a_pVCpu)               ((a_pVCpu)->pGVM->apicr0.s.pDevInsR0)
#endif

#define HVAPICCPU_TO_HVXAPICPAGE(a_ApicCpu)     ((PXAPICPAGE)(CTX_SUFF((a_ApicCpu)->pvApicPage)))
#define HVAPICCPU_TO_CHVXAPICPAGE(a_ApicCpu)    ((PCXAPICPAGE)(CTX_SUFF((a_ApicCpu)->pvApicPage)))

/**
 * Hyper-V APIC VM instance data.
 */
typedef struct HVAPIC
{
    /** The ring-3 device instance. */
    PPDMDEVINSR3                pDevInsR3;
    /** The partition handle. */
# ifdef _WINHVAPIDEFS_H_
    WHV_PARTITION_HANDLE        hPartition;
# else
    RTHCUINTPTR                 hPartition;
# endif

    /** @name Miscellaneous data.
     * @{ */
    /** Whether Hyper-V x2APIC compatibility mode is enabled. */
    bool                        fHyperVCompatMode;
    /** Whether TSC-deadline timer mode is supported for the guest. */
    bool                        fSupportsTscDeadline;
    /** The max supported APIC mode from CFGM.  */
    PDMAPICMODE                 enmMaxMode;
    /** @} */
} HVAPIC;
/** Pointer to Hyper-V APIC VM instance data. */
typedef HVAPIC *PHVAPIC;
/** Pointer to const Hyper-V APIC VM instance data. */
typedef HVAPIC const *PCHVAPIC;

/**
 * Hyper-V APIC VMCPU instance data.
 */
typedef struct HVAPICCPU
{
    /** The APIC page virtual address - R3 ptr. */
    R3PTRTYPE(void *)           pvApicPageR3;
    /** The APIC page passed to and used by Hyper-V - R3 ptr. */
    R3PTRTYPE(void *)           pvHvPageR3;

    /** The APIC base MSR.*/
    uint64_t                    uApicBaseMsr;
    /** The error status register's internal state. */
    uint32_t                    uEsrInternal;

    /** @name The APIC timer.
     * @{ */
    /** The timer. */
    TMTIMERHANDLE               hTimer;
    /** The time stamp when the timer was initialized.
     * @note Access protected by the timer critsect.  */
    uint64_t                    u64TimerInitial;
    /** Cache of timer initial count of the frequency hint to TM. */
    uint32_t                    uHintedTimerInitialCount;
    /** Cache of timer shift of the frequency hint to TM. */
    uint32_t                    uHintedTimerShift;
    /** The timer description. */
    char                        szTimerDesc[32];
    /** @} */

    /** @name APIC statistics.
     * @{ */
#ifdef VBOX_WITH_STATISTICS
    /** Number of times the one-shot timer is invoked. */
    STAMCOUNTER                 StatTimerOneShot;
    /** Number of times the periodic timer is invoked. */
    STAMCOUNTER                 StatTimerPeriodic;
    /** Number of times the TSC deadline timer is invoked. */
    STAMCOUNTER                 StatTimerTscDeadline;
#endif
    /** @} */
} HVAPICCPU;
/** Pointer to Hyper-V APIC VMCPU instance data. */
typedef HVAPICCPU *PHVAPICCPU;
/** Pointer to a const Hyper-V APIC VMCPU instance data. */
typedef HVAPICCPU const *PCHVAPICCPU;

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_APICHvInternal_h */

