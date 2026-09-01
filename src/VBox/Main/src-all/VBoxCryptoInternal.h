/** @file
 * VirtualBox - Cryptographic support functions Interface.
 */

/*
 * Copyright (C) 2022-2025 Oracle and/or its affiliates.
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

#ifndef MAIN_INCLUDED_SRC_src_all_VBoxCryptoInternal_h
#define MAIN_INCLUDED_SRC_src_all_VBoxCryptoInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/vfs.h>
#include <VBox/types.h>
#include <VBox/VBoxCryptoIf.h>

RT_C_DECLS_BEGIN

DECLHIDDEN(int) vboxCryptoCtxCalculatePaddingSplit(VBOXCRYPTOCTX hCryptoCtx, size_t cbUnit, const void *pvData, size_t cbData,
                                                   size_t *poffSplit);

DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxLoad(const char *pszStoredCtx, const char *pszPassword, PVBOXCRYPTOCTX phCryptoCtx);
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxDestroy(VBOXCRYPTOCTX hCryptoCtx);
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxQueryEncryptedSize(VBOXCRYPTOCTX hCryptoCtx, size_t cbPlainText, size_t *pcbEncrypted);
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxQueryDecryptedSize(VBOXCRYPTOCTX hCryptoCtx, size_t cbEncrypted, size_t *pcbPlaintext);
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxEncrypt(VBOXCRYPTOCTX hCryptoCtx, bool fPartial, void const *pvIV, size_t cbIV,
                                               void const *pvPlainText, size_t cbPlainText,
                                               void const *pvAuthData, size_t cbAuthData,
                                               void *pvEncrypted, size_t cbEncrypted,
                                               size_t *pcbEncrypted);
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxDecrypt(VBOXCRYPTOCTX hCryptoCtx, bool fPartial,
                                               void const *pvEncrypted, size_t cbEncrypted,
                                               void const *pvAuthData, size_t cbAuthData,
                                               void *pvPlainText, size_t cbPlainText, size_t *pcbPlainText);

DECL_HIDDEN_CALLBACK(int) vboxCryptoFileFromVfsFile(RTVFSFILE hVfsFile, const char *pszKeyStore, const char *pszPassword,
                                                    PRTVFSFILE phVfsFile);
DECL_HIDDEN_CALLBACK(int) vboxCryptoIoStrmFromVfsIoStrmEncrypt(RTVFSIOSTREAM hVfsIosDst, const char *pszKeyStore,
                                                               const char *pszPassword, PRTVFSIOSTREAM phVfsIosCrypt);
DECL_HIDDEN_CALLBACK(int) vboxCryptoIoStrmFromVfsIoStrmDecrypt(RTVFSIOSTREAM hVfsIosIn, const char *pszKeyStore,
                                                               const char *pszPassword, PRTVFSIOSTREAM phVfsIosOut);

RT_C_DECLS_END

#endif /* !MAIN_INCLUDED_SRC_src_all_VBoxCryptoInternal_h */

