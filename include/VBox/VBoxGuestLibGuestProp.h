/* $Id: VBoxGuestLibGuestProp.h $ */
/** @file
 * VBoxGuestLib - Guest Properties Interface (both user & kernel mode).
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

#ifndef VBOX_INCLUDED_VBoxGuestLibGuestProp_h
#define VBOX_INCLUDED_VBoxGuestLibGuestProp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/VBoxGuestLib.h>
#include <VBox/shflsvc.h>

RT_C_DECLS_BEGIN


/** @addtogroup grp_vboxguest_lib
 * @{
 */

/** @name Guest properties
 * @{ */
typedef struct VBGLGSTPROPCLIENT
{
    HGCMCLIENTID idClient;
#ifdef IN_RING0
    VBGLHGCMHANDLE handle;
#endif
} VBGLGSTPROPCLIENT;
typedef VBGLGSTPROPCLIENT *PVBGLGSTPROPCLIENT;

/** @todo Docs. */
typedef struct VBGLGUESTPROPENUM VBGLGUESTPROPENUM;
/** @todo Docs. */
typedef VBGLGUESTPROPENUM *PVBGLGUESTPROPENUM;

DECLVBGL(int)  VbglGuestPropConnect(PVBGLGSTPROPCLIENT pClient);
DECLVBGL(int)  VbglGuestPropDisconnect(PVBGLGSTPROPCLIENT pClient);

DECLVBGL(bool) VbglGuestPropExist(PVBGLGSTPROPCLIENT pClient, const char *pszPropName);
DECLVBGL(int)  VbglGuestPropWrite(PVBGLGSTPROPCLIENT pClient, const char *pszName, const char *pszValue, const char *pszFlags);
DECLVBGL(int)  VbglGuestPropWriteValue(PVBGLGSTPROPCLIENT pClient, const char *pszName, const char *pszValue);
DECLVBGL(int)  VbglGuestPropWriteValueV(PVBGLGSTPROPCLIENT pClient, const char *pszName, const char *pszValueFormat, va_list va);
DECLVBGL(int)  VbglGuestPropWriteValueF(PVBGLGSTPROPCLIENT pClient, const char *pszName, const char *pszValueFormat, ...);
DECLVBGL(int)  VbglGuestPropRead(PVBGLGSTPROPCLIENT pClient, const char *pszName, void *pvBuf, uint32_t cbBuf,
                                 char **ppszValue, uint64_t *pu64Timestamp, char **ppszFlags, uint32_t *pcbBufActual);
DECLVBGL(int)  VbglGuestPropReadEx(PVBGLGSTPROPCLIENT pClient,
                                   const char *pszPropName, char **ppszValue, char **ppszFlags, uint64_t *puTimestamp);
DECLVBGL(int)  VbglGuestPropReadValue(PVBGLGSTPROPCLIENT pClient, const char *pszName, char *pszValue, uint32_t cchValue,
                                      uint32_t *pcchValueActual);
DECLVBGL(int)  VbglGuestPropReadValueAlloc(PVBGLGSTPROPCLIENT pClient, const char *pszName, char **ppszValue);
DECLVBGL(void) VbglGuestPropReadValueFree(char *pszValue);
DECLVBGL(int)  VbglGuestPropEnumRaw(PVBGLGSTPROPCLIENT pClient, const char *pszzPatterns, char *pcBuf, uint32_t cbBuf,
                                    uint32_t *pcbBufActual);
DECLVBGL(int)  VbglGuestPropEnum(PVBGLGSTPROPCLIENT pClient, char const * const *papszPatterns, uint32_t cPatterns,
                                 PVBGLGUESTPROPENUM *ppHandle, char const **ppszName, char const **ppszValue,
                                 uint64_t *pu64Timestamp, char const **ppszFlags);
DECLVBGL(int)  VbglGuestPropEnumNext(PVBGLGUESTPROPENUM pHandle, char const **ppszName, char const **ppszValue,
                                     uint64_t *pu64Timestamp, char const **ppszFlags);
DECLVBGL(void) VbglGuestPropEnumFree(PVBGLGUESTPROPENUM pHandle);
DECLVBGL(int)  VbglGuestPropDelete(PVBGLGSTPROPCLIENT pClient, const char *pszName);
DECLVBGL(int)  VbglGuestPropDelSet(PVBGLGSTPROPCLIENT pClient, const char * const *papszPatterns, uint32_t cPatterns);
DECLVBGL(int)  VbglGuestPropWait(PVBGLGSTPROPCLIENT pClient, const char *pszPatterns, void *pvBuf, uint32_t cbBuf,
                                 uint64_t u64Timestamp, uint32_t cMillies, char ** ppszName, char **ppszValue,
                                 uint64_t *pu64Timestamp, char **ppszFlags, uint32_t *pcbBufActual, bool *pfWasDeleted);

/** @}  */

/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_VBoxGuestLibGuestProp_h */

