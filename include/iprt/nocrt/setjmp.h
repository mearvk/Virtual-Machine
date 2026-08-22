/** @file
 * IPRT / No-CRT - Our own setjmp header.
 */

/*
 * Copyright (C) 2006-2025 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_nocrt_setjmp_h
#define IPRT_INCLUDED_nocrt_setjmp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

RT_C_DECLS_BEGIN

#ifdef RT_ARCH_AMD64
# ifdef RT_OS_WINDOWS /* Completely different layout; saves frame ptr, sse&fpu CW, xmm6-xmm15. */
typedef uint64_t RT_NOCRT(jmp_buf)[32];
# else
typedef uint64_t RT_NOCRT(jmp_buf)[8];
# endif
#else
# ifdef RT_OS_WINDOWS /* Completely different layout - extra unwind and exception. */
typedef int RT_NOCRT(jmp_buf)[16];
# else
typedef uint32_t RT_NOCRT(jmp_buf)[6+2];
# endif
#endif

extern DECL_RETURNS_TWICE(int) RT_NOCRT(setjmp)(RT_NOCRT(jmp_buf));
extern DECL_NO_RETURN(int)     RT_NOCRT(longjmp)(RT_NOCRT(jmp_buf), int) RT_NOEXCEPT;

#if !defined(RT_WITHOUT_NOCRT_WRAPPERS) && !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
# define jmp_buf RT_NOCRT(jmp_buf)
# define setjmp  RT_NOCRT(setjmp)
# define longjmp RT_NOCRT(longjmp)
#endif


/*
 * With Visual C++ (or clang in Visual C++ mode) on Windows, the _setjmp call is
 * treated as an intrinsic/builtin and tranformed (lowered in clang terms) to a
 * _setjmp3, _setjmp or _setjmpex call depending on the architecture (x86, amd64,
 * and arm/arm64 respectively). The latter two gets an implicit frame pointer
 * argument added, whereas _setjmp3 takes zero or more extra restore parameters.
 *
 * We must use the underscored variant w/o any nocrt_ prefix for the compiler to
 * recognize the setjmp call as an intrinsic.
 *
 * The longjmp function is also an instrinsic, but it doesn't get extra arguments
 * added, but it makes the compiler aware what're doing (esp. for asan builds).
 * We *MUST* use RT_NOEXCEPT_EX(false) on it, or the asan build doesn't fully
 * recognize it and won't emit the necessary unwind stuff (x86 tstNoCrt-1/Jmp4).
 */
#if defined(RT_OS_WINDOWS) && (defined(_MSC_VER) || defined(__clang_major__))
# undef setjmp
# undef longjmp
# define setjmp  _setjmp
extern DECL_RETURNS_TWICE(int) __cdecl _setjmp(RT_NOCRT(jmp_buf));
extern DECL_NO_RETURN(int) __cdecl     longjmp(RT_NOCRT(jmp_buf), int) RT_NOEXCEPT_EX(false);

#endif

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_nocrt_setjmp_h */

