/** @file
 * IPRT - Stack Checks.
 */

/*
 * Copyright (C) 2026 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef IPRT_INCLUDED_stackcheck_h
#define IPRT_INCLUDED_stackcheck_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>

#if defined(_MSC_VER)
# include <iprt/sanitized/intrin.h> /* for __fastfail and _AddressOfReturnAddress prototypes */
#endif



/** @defgroup grp_rt_stackcehck RTStackCheck - Stack checks.
 * @ingroup grp_rt
 * @{
 */


/** @def RT_STACK_CHECK_RET_ADDR
 * Checks the return address prior to the return.
 * @note C++ Only. */

/** @def RT_STACK_CHECK_RET_ADDR_VERIFY
 * Explicitly verifies the state of a RT_STACK_CHECK_RET_ADDR() instance.
 * @note C++ Only. */

/*
 * Only supported for Visual C++ on x86/amd64 for the time being.
 */
#if   defined(DOXYGEN_RUNNING) \
   || (   defined(__cplusplus) \
       && (   (defined(_MSC_VER) && (defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86))) \
           /*|| defined(__GNUC__) ... */ \
           ) \
      )

/** @def RT_STACK_CHECK_GET_ADDRESS_OF_RETURN_ADDRESS
 * Get the address of the return address. */

/** @def RT_STACK_CHECK_FORCE_INLINE
 * Try force inlining of code.
 * @note Typically doesn't work in unoptimized builds, making it unsafe
 *       to use RT_STACK_CHECK_GET_ADDRESS_OF_RETURN_ADDRESS() from within
 *       these methods. */

# ifdef _MSC_VER
#  pragma intrinsic(__fastfail)
#  pragma intrinsic(_AddressOfReturnAddress)
#  define RT_STACK_CHECK_GET_ADDRESS_OF_RETURN_ADDRESS()    _AddressOfReturnAddress()
#  define RT_STACK_CHECK_FORCE_INLINE                       __forceinline
# else
#  define RT_STACK_CHECK_GET_ADDRESS_OF_RETURN_ADDRESS()    RT_BREAKPOINT() /** @todo  */
#  define RT_STACK_CHECK_FORCE_INLINE                       __attribute__((__always_inline__)) /** @todo  */
# endif

/*
 * Panic macros.
 */
# if !defined(RT_STACK_CHECK_FAILED) || defined(DOXYGEN_RUNNING)
#  ifdef RT_OS_WINDOWS
#   define RT_STACK_CHECK_FAILED()                          __fastfail(2 /*FAST_FAIL_STACK_COOKIE_CHECK_FAILURE*/)
#  else
#   define RT_STACK_CHECK_FAILED()                          RTAssertDoPanic()
#  endif
# endif

# if !defined(RT_STACK_CHECK_FAILED_RET_ADDR) || defined(DOXYGEN_RUNNING)
#  ifdef RT_OS_WINDOWS
#   define RT_STACK_CHECK_FAILED_RET_ADDR()                 __fastfail(57 /*FAST_FAIL_CONTROL_INVALID_RETURN_ADDRESS*/)
#  else
#   define RT_STACK_CHECK_FAILED_RET_ADDR()                 RT_STACK_CHECK_FAILED()
#  endif
# endif


/*
 * Check just the return address.
 */
# define RT_STACK_CHECK_RET_ADDR() \
    RTStackCheckRet StackCheckRetVar((uintptr_t const volatile *)RT_STACK_CHECK_GET_ADDRESS_OF_RETURN_ADDRESS())
# define RT_STACK_CHECK_RET_ADDR_VERIFY() \
    StackCheckRetVar.manualVerify(   (uintptr_t const volatile *)RT_STACK_CHECK_GET_ADDRESS_OF_RETURN_ADDRESS())

struct RTStackCheckRet
{
    uintptr_t const volatile * const    m_puRetAddrAddr;
    uintptr_t const                     m_uRetAddr;

    RT_STACK_CHECK_FORCE_INLINE
    RTStackCheckRet(uintptr_t const volatile * const puRetAddrAddr) RT_NOEXCEPT
        : m_puRetAddrAddr(puRetAddrAddr), m_uRetAddr(*m_puRetAddrAddr)
    { }

    RT_STACK_CHECK_FORCE_INLINE
    ~RTStackCheckRet() RT_NOEXCEPT
    {
        if (*m_puRetAddrAddr == m_uRetAddr) { /* likely */ }
        else { RT_STACK_CHECK_FAILED_RET_ADDR(); }
    }

    RT_STACK_CHECK_FORCE_INLINE
    void manualVerify(uintptr_t const volatile * const puRetAddrAddr) RT_NOEXCEPT
    {
        if (puRetAddrAddr == m_puRetAddrAddr && *m_puRetAddrAddr == m_uRetAddr) { /* likely */ }
        else { RT_STACK_CHECK_FAILED_RET_ADDR(); }
    }
};

#else
# define RT_STACK_CHECK_RET_ADDR()                          ((void)0)
# define RT_STACK_CHECK_RET_ADDR_VERIFY()                   ((void)0)
#endif

/** @} */

#endif /* !IPRT_INCLUDED_stackcheck_h */
