/* $Id: VDKeyStore.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VDKeyStore.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/base64.h>
#include <iprt/string.h>
#include <iprt/memsafer.h>
#include <iprt/time.h>
#include <iprt/crypto/cipher.h>
#include <iprt/crypto/digest.h>
#include <iprt/crypto/misc.h>

#include <VBox/err.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Key store structure.
 *
 * @note Everything is stored little endian.
 */
#pragma pack(1)
typedef struct VDKeyStore
{
    /** Magic value. */
    uint32_t u32Magic;
    /** Version of the header */
    uint16_t u16Version;
    /** Cipher string */
    char     szCipher[32];
    /** Key derivation function used. */
    char     szKeyDeriv[32];
    /** Key size in bytes.
     * @remarks This is misaligned because of u16Version and pack(1). Great! */
    uint32_t cbKey;
    /** The DEK digest for verification of the password. */
    uint8_t  abDekDigest[32];
    /** Size of the DEK digest.
     * @remarks Misaligned member. */
    uint32_t cbDekDigest;
    /** Salt for the DEK digest. */
    uint8_t  abDekDigestSalt[32];
    /** Iterations count of the DEK digest.
     * @remarks Misaligned member. */
    uint32_t cDekDigestIterations;
    /** Salt for the DEK. */
    uint8_t  abDekSalt[32];
    /** iterations count for the DEK.
     * @remarks Misaligned member */
    uint32_t cDekIterations;
    /** Size of the encrypted key in bytes.
     * @remarks Misaligned member. */
    uint32_t cbDekEnc;
    /** The encrypted DEK. */
    uint8_t  abDekEnc[64];
} VDKeyStore;
#pragma pack()
AssertCompileSize(VDKeyStore, 250);
typedef VDKeyStore *PVDKeyStore;

/** Key store magic (ENCS). */
#define VD_KEYSTORE_MAGIC   UINT32_C(0x454e4353)
/** Version identifier. */
#define VD_KEYSTORE_VERSION UINT16_C(0x0100)

/** Minimum amount of iterations for PBKDF2. */
#define VD_KEYSTORE_ITERATIONS_MIN     20000
/** Maximum time for the PBKDF2 function to take in milliseconds. */
#define VD_KEYSTORE_PBKDF2_COMPUTE_MAX   250


/**
 * Benchmarks the system to get a good iteration count without stalling too much when
 * trying to decrypt the DEK.
 *
 * @returns The iteration count to use while the deadline is still met.
 *          0 on failure.
 * @param   enmDigestType The message digest type to use.
 * @param   cbInput       Size of the input in bytes (password) to benchmark with.
 * @param   cbResult      The size of the resulting key to benchmark with.
 * @param   msCompute     Deadline the computation should take with the returned
 *                        iteration count. Serves only as a guideline, the returned value
 *                        might result into a bit less or a little more compute time.
 */
static uint32_t vdKeyStoreIterationCountBenchmark(RTDIGESTTYPE enmDigestType, size_t cbInput,
                                                  size_t cbResult, RTMSINTERVAL msCompute)
{
    uint32_t cIterations = 0;
    char *pbInput = (char  *)RTMemAllocZ(cbInput);
    uint8_t *pbOutput = (uint8_t *)RTMemAllocZ(cbResult);

    if (pbInput && pbOutput)
    {
        uint8_t abSalt[32] = { 0 };
        uint64_t tsStart = RTTimeMilliTS();

        while (RTTimeMilliTS() < tsStart + msCompute)
        {
            int rc = RTCrPkcs5Pbkdf2Hmac(pbInput, cbInput, &abSalt[0],
                                         sizeof(abSalt), VD_KEYSTORE_ITERATIONS_MIN,
                                         enmDigestType, cbResult, pbOutput);
            if (RT_SUCCESS(rc))
            { /* likely */ }
            else
            {
                cIterations = 0;
                break;
            }

            cIterations += VD_KEYSTORE_ITERATIONS_MIN;
        }

        if (cIterations)
            cIterations = RT_MAX(VD_KEYSTORE_ITERATIONS_MIN, cIterations);
    }

    RTMemFree(pbInput);
    RTMemFree(pbOutput);

    return cIterations;
}

/**
 * Returns the appropriate openssl digest engine as specified in the key store.
 *
 * @return  The digest type.
 * @param   pKeyStore    The key store.
 */
static RTDIGESTTYPE vdKeyStoreGetDigest(PVDKeyStore pKeyStore)
{
    if (!strncmp(&pKeyStore->szKeyDeriv[0], "PBKDF2-SHA1", sizeof(pKeyStore->szKeyDeriv)))
        return RTDIGESTTYPE_SHA1;
    if (!strncmp(&pKeyStore->szKeyDeriv[0], "PBKDF2-SHA256", sizeof(pKeyStore->szKeyDeriv)))
        return RTDIGESTTYPE_SHA256;
    if (!strncmp(&pKeyStore->szKeyDeriv[0], "PBKDF2-SHA512", sizeof(pKeyStore->szKeyDeriv)))
        return RTDIGESTTYPE_SHA512;
    return RTDIGESTTYPE_INVALID;
}

/**
 * Returns the appropriate IPRT cipher type as specified in the key store.
 *
 * @return  IPRT cipher type on success.
 * @param   pKeyStore    The key store.
 */
static RTCRCIPHERTYPE vdKeyStoreGetCipher(PVDKeyStore pKeyStore)
{
    if (!strncmp(&pKeyStore->szCipher[0], "AES-XTS128-PLAIN64", sizeof(pKeyStore->szCipher)))
        return RTCRCIPHERTYPE_XTS_AES_128;
    if (!strncmp(&pKeyStore->szCipher[0], "AES-XTS256-PLAIN64", sizeof(pKeyStore->szCipher)))
        return RTCRCIPHERTYPE_XTS_AES_256;
    return RTCRCIPHERTYPE_INVALID;
}

/**
 * Derives a key from the given password.
 *
 * @return  IPRT status code.
 * @param   pszPassword    The password to derive the key from.
 * @param   pKeyStore      The key store containing the deriviation parameters.
 * @param   ppbDerivKey    Where to store the derived key on success. Must be freed with
 *                         RTMemSaferFree().
 */
static int vdKeyStoreDeriveKeyFromPassword(const char *pszPassword, PVDKeyStore pKeyStore,
                                           uint8_t **ppbDerivKey)
{
    /* Allocate enough memory for the derived key. */
    void *pvDerivKey = NULL;
    int rc = RTMemSaferAllocZEx(&pvDerivKey, pKeyStore->cbKey, 0 /* fFlags */);
    if (RT_SUCCESS(rc))
    {
        /* Do the key deriviation. */
        rc = RTCrPkcs5Pbkdf2Hmac(pszPassword, strlen(pszPassword), &pKeyStore->abDekSalt[0],
                                 sizeof(pKeyStore->abDekSalt), pKeyStore->cDekIterations,
                                 vdKeyStoreGetDigest(pKeyStore), pKeyStore->cbKey, pvDerivKey);
        if (RT_SUCCESS(rc))
            *ppbDerivKey = (uint8_t *)pvDerivKey;
        else
            RTMemSaferFree(pvDerivKey, pKeyStore->cbKey);
    }

    return rc;
}

/**
 * Decrypts the DEK in the given key store with the given key.
 *
 * @return  IPRT status code.
 * @param   pKeyStore          The key store containing the encrpted DEK.
 * @param   pbKey              The key to decrypt the DEK with.
 * @param   ppbDekDecrypted    Where to store the decrypted DEK on success.
 *                             Must be freed with RTMemSaferFree().
 */
static int vdKeyStoreDekDecryptWithKey(PVDKeyStore pKeyStore, const uint8_t *pbKey,
                                       uint8_t **ppbDekDecrypted)
{
    RTCRCIPHER hCipher;
    int rc = RTCrCipherOpenByType(&hCipher, vdKeyStoreGetCipher(pKeyStore), 0);
    if (RT_SUCCESS(rc))
    {
        void *pvDek = NULL;
        rc = RTMemSaferAllocZEx(&pvDek, pKeyStore->cbKey, 0 /* fFlags */);
        if (RT_SUCCESS(rc))
        {
            size_t  cbDecrypted = 0;
            uint8_t abIv[16];
            RT_ZERO(abIv);
            rc = RTCrCipherDecrypt(hCipher, pbKey, pKeyStore->cbKey, &abIv[0], sizeof(abIv),
                                   &pKeyStore->abDekEnc[0], pKeyStore->cbDekEnc,
                                   pvDek, pKeyStore->cbKey, &cbDecrypted);
            if (RT_SUCCESS(rc))
            {
                Assert(cbDecrypted == pKeyStore->cbKey);
                *ppbDekDecrypted = (uint8_t *)pvDek;
            }
            else
                RTMemSaferFree(pvDek, pKeyStore->cbKey);
        }
        uint32_t cRefs = RTCrCipherRelease(hCipher);
        Assert(cRefs == 0); RT_NOREF_PV(cRefs);
    }
    return rc;
}

/**
 * Checks the given DEK against the digest stored in the key store.
 *
 * @return  IPRT status code
 * @retval  VINF_SUCCESS if the DEK matches the digeststored in the key store.
 * @retval  VERR_ACCESS_DENIED if the DEK is incorrect.
 * @param   pKeyStore    The key store containing the DEk digest.
 * @param   pbDek        The DEK to check.
 */
static int vdKeyStoreCheckDekAgainstDigest(PVDKeyStore pKeyStore, const uint8_t *pbDek)
{
    /* Query key derivation function. */
    int rc;
    RTDIGESTTYPE enmDigestType = vdKeyStoreGetDigest(pKeyStore);
    if (enmDigestType != RTDIGESTTYPE_INVALID)
    {
        /* Allocate buffer for holding the digest. */
        size_t const cbDekDigest = RTCrDigestTypeToHashSize(enmDigestType);
        uint8_t     *pbDekDigest = (uint8_t *)RTMemTmpAllocZ(cbDekDigest);
        if (pbDekDigest)
        {
            /* Do the magic and compare outcome. */
            rc = RTCrPkcs5Pbkdf2Hmac(pbDek, pKeyStore->cbKey, &pKeyStore->abDekDigestSalt[0],
                                     sizeof(pKeyStore->abDekDigestSalt), pKeyStore->cDekDigestIterations,
                                     vdKeyStoreGetDigest(pKeyStore), pKeyStore->cbDekDigest, pbDekDigest);
            if (   RT_SUCCESS(rc)
                && memcmp(pbDekDigest, pKeyStore->abDekDigest, cbDekDigest) == 0)
                rc = VINF_SUCCESS;
            else
                rc = VERR_VD_PASSWORD_INCORRECT;
            RTMemTmpFree(pbDekDigest);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_INVALID_PARAMETER;
    return rc;
}

/**
 * Generate a digest of the given DEK and store in the given key store.
 *
 * @return  IPRT status code.
 * @param   pKeyStore    The key store to store the digest in.
 * @param   pbDek        The DEK to generate the digest from.
 */
static int vdKeyStoreDekDigestGenerate(PVDKeyStore pKeyStore, const uint8_t *pbDek)
{
    /* Query key derivation function. */
    int rc;
    RTDIGESTTYPE enmDigestType = vdKeyStoreGetDigest(pKeyStore);
    if (enmDigestType != RTDIGESTTYPE_INVALID)
    {
        /* Create salt. */
        rc = RTCrRandBytes(&pKeyStore->abDekDigestSalt[0], sizeof(pKeyStore->abDekDigestSalt));
        if (RT_SUCCESS(rc))
        {
            pKeyStore->cDekDigestIterations = VD_KEYSTORE_ITERATIONS_MIN;

            /* Generate digest. */
            uint32_t const cbDekDigest = RTCrDigestTypeToHashSize(enmDigestType);
            rc = RTCrPkcs5Pbkdf2Hmac(pbDek, pKeyStore->cbKey, &pKeyStore->abDekDigestSalt[0],
                                     sizeof(pKeyStore->abDekDigestSalt), pKeyStore->cDekDigestIterations,
                                     enmDigestType, cbDekDigest, &pKeyStore->abDekDigest[0]);
            if (RT_SUCCESS(rc))
            {
                pKeyStore->cbDekDigest = cbDekDigest;
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_ACCESS_DENIED; /** @todo Better status code. */
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;
    return rc;
}

/**
 * Encrypt the given DEK with the given key and store it into the key store.
 *
 * @return  IPRT status code.
 * @param   pKeyStore    The key store to store the encrypted DEK in.
 * @param   pbKey        The key to encrypt the DEK with.
 * @param   pbDek        The DEK to encrypt.
 */
static int vdKeyStoreDekEncryptWithKey(PVDKeyStore pKeyStore, const uint8_t *pbKey,
                                       const uint8_t *pbDek)
{
    RTCRCIPHER hCipher;
    int rc = RTCrCipherOpenByType(&hCipher, vdKeyStoreGetCipher(pKeyStore), 0);
    if (RT_SUCCESS(rc))
    {
        size_t  cbEncrypted = 0;
        uint8_t abIv[16];
        RT_ZERO(abIv);
        rc = RTCrCipherEncrypt(hCipher, pbKey, pKeyStore->cbKey, &abIv[0], sizeof(abIv),
                               pbDek, pKeyStore->cbKey,
                               &pKeyStore->abDekEnc[0], sizeof(pKeyStore->abDekEnc), &cbEncrypted);
        if (RT_SUCCESS(rc))
            pKeyStore->cbDekEnc = (uint32_t)cbEncrypted;

        uint32_t cRefs = RTCrCipherRelease(hCipher);
        Assert(cRefs == 0); RT_NOREF_PV(cRefs);
    }
    return rc;
}

/**
 * Encodes the given key store in a base64 string.
 *
 * @return IPRT status code.
 * @param  pKeyStore    The key store to encode.
 * @param  ppszEnc      Where to store the encoded key store on success.
 *                      Must be freed with RTMemFree().
 */
static int vdKeyStoreEncode(PVDKeyStore pKeyStore, char **ppszEnc)
{
    pKeyStore->u32Magic             = RT_H2LE_U32(pKeyStore->u32Magic);
    pKeyStore->u16Version           = RT_H2LE_U16(pKeyStore->u16Version);
    pKeyStore->cbKey                = RT_H2LE_U32(pKeyStore->cbKey);
    pKeyStore->cbDekDigest          = RT_H2LE_U32(pKeyStore->cbDekDigest);
    pKeyStore->cDekDigestIterations = RT_H2LE_U32(pKeyStore->cDekDigestIterations);
    pKeyStore->cDekIterations       = RT_H2LE_U32(pKeyStore->cDekIterations);
    pKeyStore->cbDekEnc             = RT_H2LE_U32(pKeyStore->cbDekEnc);

    size_t cbEncoded = RTBase64EncodedLength(sizeof(*pKeyStore)) + 1;
    char *pszEnc = (char *)RTMemAllocZ(cbEncoded);
    if (!pszEnc)
        return VERR_NO_MEMORY;

    int rc = RTBase64Encode(pKeyStore, sizeof(*pKeyStore), pszEnc, cbEncoded, NULL);
    if (RT_SUCCESS(rc))
        *ppszEnc = pszEnc;
    else
        RTMemFree(pszEnc);
    return rc;
}

DECLHIDDEN(int) vdKeyStoreGetDekFromEncoded(const char *pszEnc, const char *pszPassword,
                                            uint8_t **ppbDek, size_t *pcbDek, char **ppszCipher)
{
    VDKeyStore KeyStore;

    /* Convert to binary data and host endianess. */
    int rc = RTBase64Decode(pszEnc, &KeyStore, sizeof(VDKeyStore), NULL, NULL);
    if (RT_FAILURE(rc))
        return rc;

    KeyStore.u32Magic             = RT_LE2H_U32(KeyStore.u32Magic);
    KeyStore.u16Version           = RT_LE2H_U16(KeyStore.u16Version);
    KeyStore.cbKey                = RT_LE2H_U32(KeyStore.cbKey);
    KeyStore.cbDekDigest          = RT_LE2H_U32(KeyStore.cbDekDigest);
    KeyStore.cDekDigestIterations = RT_LE2H_U32(KeyStore.cDekDigestIterations);
    KeyStore.cDekIterations       = RT_LE2H_U32(KeyStore.cDekIterations);
    KeyStore.cbDekEnc             = RT_LE2H_U32(KeyStore.cbDekEnc);
    if (   KeyStore.u32Magic != VD_KEYSTORE_MAGIC
        || KeyStore.u16Version != VD_KEYSTORE_VERSION)
        return VERR_INVALID_MAGIC;

    /* A validation checks. */
    if (   KeyStore.cbKey > _1M
        || KeyStore.cbDekDigest > sizeof(KeyStore.abDekDigest)
        || KeyStore.cbDekEnc > sizeof(KeyStore.abDekEnc))
        return VERR_INVALID_STATE;

    char *pszCipher = RTStrDupN(&KeyStore.szCipher[0], sizeof(KeyStore.szCipher));
    if (!pszCipher)
        return VERR_NO_STR_MEMORY;

    if (!pszPassword)
    {
        *ppszCipher = pszCipher;
        return VINF_SUCCESS;
    }

    uint8_t *pbDerivKey = NULL;
    rc = vdKeyStoreDeriveKeyFromPassword(pszPassword, &KeyStore, &pbDerivKey);
    if (RT_SUCCESS(rc))
    {
        /* Use the derived key to decrypt the DEK. */
        uint8_t *pbDekDecrypted = NULL;
        rc = vdKeyStoreDekDecryptWithKey(&KeyStore, pbDerivKey, &pbDekDecrypted);
        if (RT_SUCCESS(rc))
        {
            /* Check the decrypted key with the digest. */
            rc = vdKeyStoreCheckDekAgainstDigest(&KeyStore, pbDekDecrypted);
            if (RT_SUCCESS(rc))
            {
                *pcbDek = KeyStore.cbKey;
                *ppbDek = pbDekDecrypted;
                *ppszCipher = pszCipher;
            }
            else
                RTMemSaferFree(pbDekDecrypted, KeyStore.cbKey);
        }
    }

    if (pbDerivKey)
        RTMemSaferFree(pbDerivKey, KeyStore.cbKey);

    if (RT_FAILURE(rc))
        RTStrFree(pszCipher);

    return rc;
}

DECLHIDDEN(int) vdKeyStoreCreate(const char *pszPassword, const uint8_t *pbDek, size_t cbDek,
                                 const char *pszCipher, char **ppszEnc)
{
    VDKeyStore KeyStore;
    RT_ZERO(KeyStore);

    KeyStore.u32Magic       = VD_KEYSTORE_MAGIC;
    KeyStore.u16Version     = VD_KEYSTORE_VERSION;

    /* Generate the salt for the DEK encryption. */
    int rc = RTCrRandBytes(&KeyStore.abDekSalt[0], sizeof(KeyStore.abDekSalt));
    if (RT_SUCCESS(rc))
    {
        rc = RTStrCopy(&KeyStore.szCipher[0], sizeof(KeyStore.szCipher), pszCipher);
        if (RT_SUCCESS(rc))
        {
            KeyStore.cbKey = (uint32_t)cbDek;
            Assert(KeyStore.cbKey == cbDek);
            strcpy(&KeyStore.szKeyDeriv[0], "PBKDF2-SHA256");
            KeyStore.cDekIterations = vdKeyStoreIterationCountBenchmark(vdKeyStoreGetDigest(&KeyStore), strlen(pszPassword),
                                                                        cbDek, VD_KEYSTORE_PBKDF2_COMPUTE_MAX);
            if (KeyStore.cDekIterations > 0)
            {
                rc = vdKeyStoreDekDigestGenerate(&KeyStore, pbDek);
                if (RT_SUCCESS(rc))
                {
                    uint8_t *pbDerivKey = NULL;
                    rc = vdKeyStoreDeriveKeyFromPassword(pszPassword, &KeyStore, &pbDerivKey);
                    if (RT_SUCCESS(rc))
                    {
                        rc = vdKeyStoreDekEncryptWithKey(&KeyStore, pbDerivKey, pbDek);
                        if (RT_SUCCESS(rc))
                            rc = vdKeyStoreEncode(&KeyStore, ppszEnc);

                        RTMemSaferFree(pbDerivKey, KeyStore.cbKey);
                    }
                }
            }
            else
                rc = VERR_INVALID_STATE;
        }
    }

    return rc;
}

