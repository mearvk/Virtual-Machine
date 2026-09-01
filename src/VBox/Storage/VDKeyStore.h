/* $Id: VDKeyStore.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VD - Simple keystore handling for encrypted media.
 */

/*
 * Copyright (C) 2015-2025 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_SRC_Storage_VDKeyStore_h
#define VBOX_INCLUDED_SRC_Storage_VDKeyStore_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>

/**
 * Return the encryption parameters and DEK from the base64 encoded key store data.
 *
 * @returns IPRT status code.
 * @param   pszEnc         The base64 encoded key store data.
 * @param   pszPassword    The password to use for key decryption.
 *                         If the password is NULL only the cipher is returned.
 * @param   ppbKey         Where to store the DEK on success.
 *                         Must be freed with RTMemSaferFree().
 * @param   pcbKey         Where to store the DEK size in bytes on success.
 * @param   ppszCipher     Where to store the used cipher for the decrypted DEK.
 *                         Must be freed with RTStrFree().
 */
DECLHIDDEN(int) vdKeyStoreGetDekFromEncoded(const char *pszEnc, const char *pszPassword,
                                            uint8_t **ppbKey, size_t *pcbKey, char **ppszCipher);

/**
 * Stores the given DEK in a key store protected by the given password.
 *
 * @returns IPRT status code.
 * @param   pszPassword    The password to protect the DEK.
 * @param   pbKey          The DEK to protect.
 * @param   cbKey          Size of the DEK to protect.
 * @param   pszCipher      The cipher string associated with the DEK.
 * @param   ppszEnc        Where to store the base64 encoded key store data on success.
 *                         Must be freed with RTMemFree().
 */
DECLHIDDEN(int) vdKeyStoreCreate(const char *pszPassword, const uint8_t *pbKey, size_t cbKey,
                                 const char *pszCipher, char **ppszEnc);

#endif /* !VBOX_INCLUDED_SRC_Storage_VDKeyStore_h */

