/** @file
 * VBoxGuestLib - HGCM Inline Helper Functions.
 */

/*
 * Copyright (C) 2006-2025 Oracle and/or its affiliates.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef VBOX_INCLUDED_VBoxGuestLibHGCMInline_h
#define VBOX_INCLUDED_VBoxGuestLibHGCMInline_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/VMMDevCoreTypes.h>
#include <iprt/errcore.h>


DECLINLINE(void) VbglHGCMParmUInt32Set(HGCMFunctionParameter *pParm, uint32_t u32)
{
    pParm->type = VMMDevHGCMParmType_32bit;
    pParm->u.value64 = 0; /* init unused bits to 0 */
    pParm->u.value32 = u32;
}


DECLINLINE(int) VbglHGCMParmUInt32Get(HGCMFunctionParameter *pParm, uint32_t *pu32)
{
    if (pParm->type == VMMDevHGCMParmType_32bit)
    {
        *pu32 = pParm->u.value32;
        return VINF_SUCCESS;
    }
    *pu32 = UINT32_MAX; /* shut up gcc */
    return VERR_WRONG_PARAMETER_TYPE;
}


DECLINLINE(void) VbglHGCMParmUInt64Set(HGCMFunctionParameter *pParm, uint64_t u64)
{
    pParm->type      = VMMDevHGCMParmType_64bit;
    pParm->u.value64 = u64;
}


DECLINLINE(int) VbglHGCMParmUInt64Get(HGCMFunctionParameter *pParm, uint64_t *pu64)
{
    if (pParm->type == VMMDevHGCMParmType_64bit)
    {
        *pu64 = pParm->u.value64;
        return VINF_SUCCESS;
    }
    *pu64 = UINT64_MAX; /* shut up gcc */
    return VERR_WRONG_PARAMETER_TYPE;
}


DECLINLINE(void) VbglHGCMParmPtrSet(HGCMFunctionParameter *pParm, void *pv, uint32_t cb)
{
    pParm->type                    = VMMDevHGCMParmType_LinAddr;
    pParm->u.Pointer.size          = cb;
    pParm->u.Pointer.u.linearAddr  = (uintptr_t)pv;
}


#ifdef VBOX_VBGLR3_XFREE86
/* Rather than try to resolve all the header file conflicts, I will just
   prototype what we need here.
   Note! Other places needs the typedef, so always define it. */
RT_C_DECLS_BEGIN
typedef unsigned long xf86size_t;
xf86size_t xf86strlen(const char *);
RT_C_DECLS_END
#endif /* VBOX_VBGLR3_XFREE86 */

#ifdef IPRT_INCLUDED_string_h

DECLINLINE(void) VbglHGCMParmPtrSetString(HGCMFunctionParameter *pParm, const char *psz)
{
    pParm->type                    = VMMDevHGCMParmType_LinAddr_In;
# ifdef VBOX_VBGLR3_XFREE86
    pParm->u.Pointer.size          = (uint32_t)xf86strlen(psz) + 1;
# else
    pParm->u.Pointer.size          = (uint32_t)strlen(psz) + 1;
# endif
    pParm->u.Pointer.u.linearAddr  = (uintptr_t)psz;
}

#endif /* IPRT_INCLUDED_string_h */

#endif /* !VBOX_INCLUDED_VBoxGuestLibHGCMInline_h */
