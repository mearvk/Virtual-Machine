/* $Id: VDFilterCrypt.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VDFilter - Transparent encryption/decryption support of data.
 */

/*
 * Copyright (C) 2014-2025 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_VD
#include <VBox/vd-plugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/memsafer.h>
#include <iprt/crypto/cipher.h>
#include <iprt/crypto/misc.h>

#include "VDKeyStore.h"
#include "VDBackends.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Supported algorithms
 */
typedef enum VDFILTERCRYPTALGO
{
    /** Invalid algorithm. */
    VDFILTERCRYPTALGO_INVALID = 0,
    /** Simple XOR "encryption", only used for testing. DO NOT USE IN PRODUCTION! */
    VDFILTERCRYPTALGO_XOR,
    /** AES XTS 128bit with plain64 IV. */
    VDFILTERCRYPTALGO_AES_XTS128_PLAIN64,
    /** AES XTS 256bit with plain64 IV. */
    VDFILTERCRYPTALGO_AES_XTS256_PLAIN64,
    /** 32bit hack. */
    VDFILTERCRYPTALGO_32BIT_HACK= 0x7fffffff
} VDFILTERCRYPTALGO;

/**
 * Mapping entry between string repesentation of algorithm and enum.
 */
typedef struct VDFILTERCRYPTALGOMAPPING
{
    /** String represenation of the algorithm. */
    const char        *pszName;
    /** Enum value. */
    VDFILTERCRYPTALGO  enmAlgorithm;
    /** Key size. */
    size_t             cbKey;
} VDFILTERCRYPTALGOMAPPING;
/** Pointer to the algorithm mapping entry. */
typedef VDFILTERCRYPTALGOMAPPING *PVDFILTERCRYPTALGOMAPPING;

/**
 * Crypto filter instance data.
 */
typedef struct VDFILTERCRYPT
{
    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE               pVDIfsDisk;
    /** Pointer to the per-filter VD interface list. */
    PVDINTERFACE               pVDIfsFilter;
    /** Error interface. */
    PVDINTERFACEERROR          pIfError;
    /** I/O interface. */
    PVDINTERFACEIOINT          pIfIo;
    /** Config interface. */
    PVDINTERFACECONFIG         pIfConfig;
    /** Crypto interface used for key retrieval. */
    PVDINTERFACECRYPTO         pIfCrypto;
    /** Algorithm to use for encryption. */
    VDFILTERCRYPTALGO          enmAlgorithm;
    /** */
    PVDFILTERCRYPTALGOMAPPING  pAlgorithmDesc;
    /** The algorithm use for the current cipher (hCipher/pCipher). */
    VDFILTERCRYPTALGO          enmAlgorithmCipher;
    /** Handle to the current cipher, NIL if not instantiated. */
    RTCRCIPHER                 hCipher;
    /** ID of the key for retrieval. */
    char                      *pszKeyId;
    /** Pointer to the key material. */
    const uint8_t             *pbKey;
    /** Size of the key material in bytes. */
    size_t                     cbKey;
    /** Size of the raw data buffer. */
    size_t                     cbRawData;
    /** Temporary storage for one data unit. */
    uint8_t                   *pbRawData;
} VDFILTERCRYPT;
/** Pointer to a crypto filter instance. */
typedef VDFILTERCRYPT *PVDFILTERCRYPT;


/*********************************************************************************************************************************
*   Static Variables                                                                                                             *
*********************************************************************************************************************************/

/** Description of all accepted config parameters. */
static const VDCONFIGINFO s_cryptConfigInfo[] =
{
    { "Algorithm",            NULL,                                      VDCFGVALUETYPE_STRING,  0 },
    { "KeyId",                NULL,                                      VDCFGVALUETYPE_STRING,  0 },
    { "CreateKeyStore",       NULL,                                      VDCFGVALUETYPE_INTEGER, 0 },
    { "KeyStore",             NULL,                                      VDCFGVALUETYPE_STRING,  0 },
    { NULL,                   NULL,                                      VDCFGVALUETYPE_INTEGER, 0 }
};

/** Mapping of algorithm names to enum. */
static VDFILTERCRYPTALGOMAPPING s_aCryptAlgoMapping[] =
{
    /* pszName              enmAlgorithm                          cbKey */
    { "XOR",                VDFILTERCRYPTALGO_XOR,                16},
    { "AES-XTS128-PLAIN64", VDFILTERCRYPTALGO_AES_XTS128_PLAIN64, 32},
    { "AES-XTS256-PLAIN64", VDFILTERCRYPTALGO_AES_XTS256_PLAIN64, 64}
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static PVDFILTERCRYPTALGOMAPPING cryptAlgorithmQueryFromString(const char *pszAlgorithm)
{
    PVDFILTERCRYPTALGOMAPPING pMapping = NULL;

    for (unsigned i = 0; i < RT_ELEMENTS(s_aCryptAlgoMapping); i++)
    {
        if (!RTStrCmp(s_aCryptAlgoMapping[i].pszName, pszAlgorithm))
        {
            pMapping = &s_aCryptAlgoMapping[i];
            break;
        }
    }

    return pMapping;
}

/**
 * Queries the used algorithm from the config and maps it to the proper enum.
 *
 * @returns VBox status code.
 * @param   pFilter    The filter instance.
 */
static int cryptAlgorithmQueryFromConfig(PVDFILTERCRYPT pFilter)
{
    int rc;
    char *pszAlgorithm = NULL;

    AssertPtr(pFilter->pIfConfig);

    rc = VDCFGQueryStringAlloc(pFilter->pIfConfig, "Algorithm", &pszAlgorithm);
    if (RT_SUCCESS(rc))
    {
        PVDFILTERCRYPTALGOMAPPING pMapping = cryptAlgorithmQueryFromString(pszAlgorithm);
        if (pMapping)
        {
            pFilter->enmAlgorithm = pMapping->enmAlgorithm;
            pFilter->pAlgorithmDesc = pMapping;
        }
        else
            rc = VERR_NOT_FOUND;

        RTMemFree(pszAlgorithm);
    }

    return rc;
}

/**
 * Frees the filter instance.
 *
 * @param   pFilter    The filter instance.
 */
static void cryptFree(PVDFILTERCRYPT pFilter)
{
    if (pFilter)
    {
        if (pFilter->pbKey)
        {
            if (pFilter->pszKeyId)
            {
                int rc = vdIfCryptoKeyRelease(pFilter->pIfCrypto, pFilter->pszKeyId);
                AssertRC(rc);
            }
            else
                RTMemSaferFree((void *)pFilter->pbKey, pFilter->cbKey);

            pFilter->pbKey = NULL;
            pFilter->cbKey = 0;
        }

        if (pFilter->pszKeyId)
        {
            RTMemFree(pFilter->pszKeyId);
            pFilter->pszKeyId = NULL;
        }

        if (pFilter->pbRawData)
        {
            RTMemWipeThoroughly(pFilter->pbRawData, pFilter->cbRawData,
                                10 /* cMinPasses - total of 13 with the three RTMemSaferFree does next */);
            RTMemSaferFree(pFilter->pbRawData, pFilter->cbRawData);
        }

        if (pFilter->hCipher != NIL_RTCRCIPHER)
        {
            uint32_t cRefs = RTCrCipherRelease(pFilter->hCipher);
            Assert(cRefs == 0); RT_NOREF(cRefs);
            pFilter->hCipher = NIL_RTCRCIPHER;
        }

        RTMemFree(pFilter);
    }
}

/**
 * Ensures that the buffer for temporary data meets the given size.
 *
 * @returns VBox status code.
 * @param   pFilter    The filter instance.
 * @param   cbDataUnit Size of one data unit in bytes.
 */
static int cryptFilterEnsureTemporaryDataBufferSize(PVDFILTERCRYPT pFilter, size_t cbDataUnit)
{
    int rc = VINF_SUCCESS;

    if (pFilter->cbRawData < cbDataUnit)
    {
        if (pFilter->pbRawData)
        {
            RTMemWipeThoroughly(pFilter->pbRawData, pFilter->cbRawData,
                                10 /* cMinPasses - total of 13 with the three RTMemSaferFree does next */);
            RTMemSaferFree(pFilter->pbRawData, pFilter->cbRawData);
        }

        pFilter->pbRawData = (uint8_t *)RTMemSaferAllocZ(cbDataUnit);
        if (pFilter->pbRawData)
            pFilter->cbRawData = cbDataUnit;
        else
        {
            pFilter->cbRawData = 0;
            pFilter->pbRawData = NULL;
            rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

/**
 * Encryption/Decryption worker for the XOR algorithm.
 *
 * @returns VBox status code.
 * @param   pFilter    The filter instance to use.
 * @param   pIoCtx     I/O context.
 * @param   cb         Number of bytes.
 */
static int cryptFilterEncDecXor(PVDFILTERCRYPT pFilter, PVDIOCTX pIoCtx, size_t cb)
{
    int rc = VINF_SUCCESS;

    /* Crude, simple and slow but enough for testing. */
    while (cb)
    {
        uint8_t *pbData;
        RTSGSEG Segment;
        unsigned cSegments = 1;
        size_t cbData;

        cbData = vdIfIoIntIoCtxSegArrayCreate(pFilter->pIfIo, pIoCtx, &Segment, &cSegments, cb);
        Assert(cbData != 0 && cSegments == 1 && Segment.cbSeg == cbData);

        pbData = (uint8_t *)Segment.pvSeg;

        for (unsigned i = 0; i < cbData; i++)
            pbData[i] ^= pFilter->pbKey[i % pFilter->cbKey];

        cb -= cbData;
    }

    return rc;
}

/**
 * Decryption worker for the AES-XTS algorithm.
 *
 * @returns VBox status code.
 * @param   pFilter    The filter instance data.
 * @param   pbIn       Pointer to the encrypted data.
 * @param   pbOut      Where to store the decrypted data.
 * @param   cbDecrypt  How many bytes to decrypt.
 * @param   pbIv       The initialization vector to use for decryption.
 * @param   cbDataUnit Size of one data unit in bytes.
 */
static int cryptFilterXtsDecryptWorker(PVDFILTERCRYPT pFilter,
                                       const uint8_t *pbIn, uint8_t *pbOut, size_t cbDecrypt,
                                       const uint8_t *pbIv, size_t cbDataUnit)
{
    size_t cbDecrypted = 0;
    int rc = RTCrCipherDecrypt(pFilter->hCipher, pFilter->pbKey, pFilter->cbKey, pbIv, 16,
                               pbIn, cbDecrypt,
                               pbOut, cbDataUnit, &cbDecrypted);
    if (RT_SUCCESS(rc))
    {
        Assert(cbDataUnit == cbDecrypted);
        return VINF_SUCCESS;
    }
    return vdIfError(pFilter->pIfError, VERR_INVALID_STATE, RT_SRC_POS, N_("Crypt: Decryption error %Rrc"), rc);
}

/**
 * Encryption worker for the AES-XTS algorithm.
 *
 * @returns VBox status code.
 * @param   pFilter    The filter instance data.
 * @param   pbIn       Pointer to the raw data.
 * @param   pbOut      Where to store the encrypted data.
 * @param   cbEncrypt  How many bytes to encrypt.
 * @param   pbIv       The initialization vector to use for encryption.
 * @param   cbDataUnit Size of one data unit in bytes.
 */
static int cryptFilterXtsEncryptWorker(PVDFILTERCRYPT pFilter,
                                       const uint8_t *pbIn, uint8_t *pbOut, size_t cbEncrypt,
                                       const uint8_t *pbIv, size_t cbDataUnit)
{
    size_t cbEncrypted = 0;
    int rc = RTCrCipherEncrypt(pFilter->hCipher, pFilter->pbKey, pFilter->cbKey, pbIv, 16,
                               pbIn, cbEncrypt,
                               pbOut, cbDataUnit, &cbEncrypted);
    if (RT_SUCCESS(rc))
    {
        Assert(cbDataUnit == cbEncrypted);
        return VINF_SUCCESS;
    }
    return vdIfError(pFilter->pIfError, VERR_INVALID_STATE, RT_SRC_POS, N_("Crypt: Encryption error %Rrc"), rc);
}

/**
 * Main encryption/decryption worker for the AES-XTS cipher variants.
 *
 * @returns VBox status code.
 * @param   pFilter    The filter instance data.
 * @param   pIoCtx     The I/O context used.
 * @param   uOffset    The start offset to use.
 * @param   cb         How many bytes to encrypt/decrypt.
 * @param   cbDataUnit Size of one data unit in bytes.
 * @param   fEncrypt   true if the data should be encrypted, false for decryption.
 *
 * @remarks VD guarentees we will be here alone and do not have to deal with any concurrency issues.
 */
static int cryptFilterEncDecXts(PVDFILTERCRYPT pFilter,
                                PVDIOCTX pIoCtx, uint64_t uOffset, size_t cb,
                                size_t cbDataUnit, bool fEncrypt)
{
    int rc = VINF_SUCCESS;
    uint64_t uSect = uOffset / cbDataUnit;

    /*
     * Ensure sufficient temporary size buffer.
     */
    rc = cryptFilterEnsureTemporaryDataBufferSize(pFilter, cbDataUnit);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Make sure we've got the cipher.
     * Note! This could probably be done elsewhere, just playing safe/lazy now.
     */
    if (   pFilter->enmAlgorithm != pFilter->enmAlgorithmCipher
        || pFilter->hCipher == NIL_RTCRCIPHER)
    {
        switch (pFilter->enmAlgorithm)
        {
            case VDFILTERCRYPTALGO_AES_XTS128_PLAIN64:
                rc = RTCrCipherOpenByType(&pFilter->hCipher, RTCRCIPHERTYPE_XTS_AES_128, 0);
                break;
            case VDFILTERCRYPTALGO_AES_XTS256_PLAIN64:
                rc = RTCrCipherOpenByType(&pFilter->hCipher, RTCRCIPHERTYPE_XTS_AES_256, 0);
                break;
            default:
                AssertLogRelMsgFailedReturn(("enmAlgorithm=%d", pFilter->enmAlgorithm), VERR_INVALID_STATE);
        }
        AssertRCReturnStmt(rc, pFilter->hCipher = NIL_RTCRCIPHER, rc);
        pFilter->enmAlgorithmCipher = pFilter->enmAlgorithm;
    }

    /*
     * Work the data.
     */
    while (cb)
    {
        uint8_t abIv[16];
        unsigned cSegments = 1;
        RTSGSEG Segment;
        uint8_t *pbData;

        /** @todo r=bird: it appears to me like we're working directly on the input buffer when encrypting.
         * This is rather problematic if the input isn't writable.  FormatFAT runs/ran into this when
         * writing zero sectors.
         */
        size_t const cbData = vdIfIoIntIoCtxSegArrayCreate(pFilter->pIfIo, pIoCtx, &Segment, &cSegments, cbDataUnit);
        Assert(cbData == cbDataUnit && cSegments == 1 && Segment.cbSeg == cbData); RT_NOREF(cbData);

        memcpy(pFilter->pbRawData, Segment.pvSeg, cbDataUnit);
        pbData = (uint8_t *)Segment.pvSeg;

        /* Create the plain IV. */
        RT_BZERO(abIv, sizeof(abIv));
        for (unsigned i = 0; i < sizeof(uint64_t); i++)
            abIv[i] = (uint8_t)((uSect >> (i * 8)) & 0xff);

        if (fEncrypt)
            rc = cryptFilterXtsEncryptWorker(pFilter, pFilter->pbRawData,
                                             pbData, cbDataUnit, &abIv[0], cbDataUnit);
        else
            rc = cryptFilterXtsDecryptWorker(pFilter, pFilter->pbRawData,
                                             pbData, cbDataUnit, &abIv[0], cbDataUnit);

        cb -= cbDataUnit;
        uSect++;
    }

    return rc;
}

/**
 * Creates a new DEK depending on the configured cipher.
 *
 * @returns VBox status code.
 * @param   pFilter    The filter instance data.
 */
static int cryptKeyStoreDekCreate(PVDFILTERCRYPT pFilter)
{
    pFilter->cbKey = pFilter->pAlgorithmDesc->cbKey;

    int rc = RTMemSaferAllocZEx((void **)&pFilter->pbKey, pFilter->cbKey, 0 /* fFlags */);
    if (RT_SUCCESS(rc))
    {
        rc = RTCrRandBytes((void *)pFilter->pbKey, pFilter->cbKey);
        if (RT_SUCCESS(rc))
            return rc;
        rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to generate enough random bytes for a new DEK (%Rrc)"), rc);
    }

    return rc;
}

/**
 * Creates a new key store which is protected by the given password.
 *
 * @returns VBox status code.
 * @param   pFilter     The filter instance data.
 * @param   pszPassword The password to protect the key store with.
 */
static int cryptKeyStoreCreate(PVDFILTERCRYPT pFilter, const char *pszPassword)
{
    int rc = cryptAlgorithmQueryFromConfig(pFilter);
    if (RT_SUCCESS(rc))
    {
        rc = cryptKeyStoreDekCreate(pFilter);
        if (RT_SUCCESS(rc))
        {
            char *pszStoreEnc = NULL;

            rc = vdKeyStoreCreate(pszPassword, pFilter->pbKey, pFilter->cbKey, pFilter->pAlgorithmDesc->pszName,
                                  &pszStoreEnc);
            if (RT_SUCCESS(rc))
            {
                rc = vdIfCryptoKeyStoreSave(pFilter->pIfCrypto, pszStoreEnc, strlen(pszStoreEnc) + 1);
                if (RT_SUCCESS(rc))
                    rc = vdIfCryptoKeyStoreReturnParameters(pFilter->pIfCrypto, pFilter->pAlgorithmDesc->pszName,
                                                            pFilter->pbKey, pFilter->cbKey);
                else
                    rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to save key store"));

                RTMemFree(pszStoreEnc);
            }
            else
                rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to create key store"));
        }
    }
    else
        rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to parse the used algorithm"));

    return rc;
}

/**
 * Decrypts an existing key store with the given password, checking whether the password is correct.
 *
 * @returns VBox status code.
 * @param   pFilter     The filter instance data.
 * @param   pszPassword The password to unlock the key store.
 */
static int cryptKeyStoreDecrypt(PVDFILTERCRYPT pFilter, const char *pszPassword)
{
    char *pszKeyStore = NULL;
    char *pszCipher = NULL;
    int rc = VDCFGQueryStringAlloc(pFilter->pIfConfig, "KeyStore", &pszKeyStore);
    if (RT_SUCCESS(rc))
    {
        rc = vdKeyStoreGetDekFromEncoded(pszKeyStore, pszPassword,
                                         (uint8_t **)&pFilter->pbKey, &pFilter->cbKey, &pszCipher);
        if (RT_SUCCESS(rc))
        {
            PVDFILTERCRYPTALGOMAPPING pMapping = cryptAlgorithmQueryFromString(pszCipher);
            if (pMapping)
            {
                pFilter->enmAlgorithm = pMapping->enmAlgorithm;
                pFilter->pAlgorithmDesc = pMapping;

                /* Notify about key store parameters if requested. */
                rc = vdIfCryptoKeyStoreReturnParameters(pFilter->pIfCrypto, pszCipher,
                                                        pFilter->pbKey, pFilter->cbKey);
            }
            else
                rc = vdIfError(pFilter->pIfError, VERR_NOT_FOUND, RT_SRC_POS, N_("Crypt: The requested cipher found in the key store is not supported by this version of the plugin"));

            if (RT_FAILURE(rc))
            {
                RTMemSaferFree((void *)pFilter->pbKey, pFilter->cbKey);
                pFilter->pbKey = NULL;
                pFilter->cbKey = 0;
            }

            RTStrFree(pszCipher);
        }
        else
            rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to decrypt the key store with the given password"));

        RTMemFree(pszKeyStore);
    }
    else
        rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to read key store data from config"));

    return rc;
}

/**
 * Queries the DEK directly using VDINTERFACECRYPTO::pfnKeyRetain().
 *
 * @returns VBox status code.
 * @param   pFilter    The filter instance data.
 */
static int cryptDekQueryDirectly(PVDFILTERCRYPT pFilter)
{
    int rc = cryptAlgorithmQueryFromConfig(pFilter);
    if (RT_SUCCESS(rc))
    {
        /* Retrieve the key. */
        /** @todo Change to key retrieval during read and writes, measure performance impact. */
        rc = vdIfCryptoKeyRetain(pFilter->pIfCrypto, pFilter->pszKeyId, &pFilter->pbKey, &pFilter->cbKey);
        if (RT_SUCCESS(rc))
        {
            if (pFilter->pAlgorithmDesc->cbKey == pFilter->cbKey)
                return VINF_SUCCESS;

            rc = vdIfError(pFilter->pIfError, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("Crypt: Wrong length for passed key"));
        }
        else
            rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to retrieve key from store"));

    }
    else
        rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to parse the used algorithm"));

    return rc;
}

/**
 * @copydoc VDFILTERBACKEND::pfnCreate
 */
static DECLCALLBACK(int) cryptCreate(PVDINTERFACE pVDIfsDisk, uint32_t fFlags,
                                     PVDINTERFACE pVDIfsFilter, void **ppvBackendData)
{
    RT_NOREF1(pVDIfsDisk);
    int rc = VINF_SUCCESS;
    PVDFILTERCRYPT pFilter = (PVDFILTERCRYPT)RTMemAllocZ(sizeof(VDFILTERCRYPT));
    if (!pFilter)
        return VERR_NO_MEMORY;

    pFilter->pVDIfsFilter = pVDIfsFilter;
    pFilter->enmAlgorithmCipher = VDFILTERCRYPTALGO_INVALID;
    pFilter->hCipher = NIL_RTCRCIPHER;

    pFilter->pIfError  = VDIfErrorGet(pFilter->pVDIfsDisk);
    pFilter->pIfIo     = VDIfIoIntGet(pFilter->pVDIfsFilter);
    pFilter->pIfCrypto = VDIfCryptoGet(pFilter->pVDIfsFilter);
    AssertPtrReturnStmt(pFilter->pIfIo, RTMemFree(pFilter), VERR_INVALID_PARAMETER);
    AssertPtrReturnStmt(pFilter->pIfCrypto, RTMemFree(pFilter), VERR_INVALID_PARAMETER);

    /* Get configuration interface. */
    pFilter->pIfConfig = VDIfConfigGet(pFilter->pVDIfsFilter);
    if (pFilter->pIfConfig)
    {
        if (VDCFGAreKeysValid(pFilter->pIfConfig,
                              "Algorithm\0"
                              "KeyId\0"
                              "CreateKeyStore\0"
                              "KeyStore\0"))
        {
            /*
             * The logic is a bit complicated because there are various ways to supply the DEK.
             * If the "KeyStore" key exists it must contain the key store as base64 encoded
             * data and the password to unlock it is gathered from VDINTERFACECRYPTO::pfnKeyStoreGetPassword().
             * If the "CreateKeyStore" boolean is set a new key store is created protected by the password
             * returned through VDINTERFACECRYPTO::pfnKeyStoreGetPassword().
             * If "CreateKeyStore" is not existing the DEK will be directly obtained via
             * VDINTERFACECRYPTO::pfnKeyRetain() to stay backwards compatible for now.
             */
            rc = VDCFGQueryStringAlloc(pFilter->pIfConfig, "KeyId", &pFilter->pszKeyId);
            if (RT_SUCCESS(rc))
            {
                if (VDCFGIsKeyExisting(pFilter->pIfConfig, "KeyStore"))
                {
                    const char *pszPassword = NULL;

                    if (!(fFlags & VD_FILTER_FLAGS_INFO))
                        rc = vdIfCryptoKeyStorePasswordRetain(pFilter->pIfCrypto, pFilter->pszKeyId, &pszPassword);

                    if (RT_SUCCESS(rc))
                    {
                        rc = cryptKeyStoreDecrypt(pFilter, pszPassword);

                        if (!(fFlags & VD_FILTER_FLAGS_INFO))
                        {
                            int rc2 = vdIfCryptoKeyStorePasswordRelease(pFilter->pIfCrypto, pFilter->pszKeyId);
                            AssertRC(rc2);
                        }
                    }
                    else
                        rc =  vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to retrieve password to access/create the key store"));

                    RTMemFree(pFilter->pszKeyId);
                    pFilter->pszKeyId = NULL;
                }
                else
                {
                    bool fCreateKeyStore = false;

                    rc = VDCFGQueryBool(pFilter->pIfConfig, "CreateKeyStore", &fCreateKeyStore);
                    if (RT_SUCCESS(rc))
                    {
                        const char *pszPassword = NULL;

                        rc = vdIfCryptoKeyStorePasswordRetain(pFilter->pIfCrypto, pFilter->pszKeyId, &pszPassword);
                        if (RT_SUCCESS(rc))
                        {
                            if (fCreateKeyStore)
                                rc = cryptKeyStoreCreate(pFilter, pszPassword);
                            else
                                rc = cryptKeyStoreDecrypt(pFilter, pszPassword);

                            int rc2 = vdIfCryptoKeyStorePasswordRelease(pFilter->pIfCrypto, pFilter->pszKeyId);
                            AssertRC(rc2);
                        }
                        else
                            rc =  vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to retrieve password to access/create the key store"));

                        RTMemFree(pFilter->pszKeyId);
                        pFilter->pszKeyId = NULL;
                    }
                    else if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                    {
                        /* Get DEK directly and don't bother with the key store at all. */
                        rc = cryptDekQueryDirectly(pFilter);
                    }
                    else
                        rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to query \"CreateKeyStore\" from config"));
                }

                if (RT_SUCCESS(rc))
                {
                    *ppvBackendData = pFilter;
                    return VINF_SUCCESS;
                }
            }
            else
                rc = vdIfError(pFilter->pIfError, rc, RT_SRC_POS, N_("Crypt: Failed to get the key identifier"));
        }
        else
            rc = vdIfError(pFilter->pIfError, VERR_VD_UNKNOWN_CFG_VALUES, RT_SRC_POS, N_("Crypt: configuration error: unknown configuration keys present"));
    }
    else
        rc = vdIfError(pFilter->pIfError, VERR_VD_UNKNOWN_INTERFACE,
                       RT_SRC_POS, N_("Crypt: configuration interface missing"));

    cryptFree(pFilter);
    return rc;
}

/**
 * @copydoc VDFILTERBACKEND::pfnDestroy
 */
static DECLCALLBACK(int) cryptDestroy(void *pvBackendData)
{
    PVDFILTERCRYPT pFilter = (PVDFILTERCRYPT)pvBackendData;

    cryptFree(pFilter);
    return VINF_SUCCESS;
}

/**
 * @copydoc VDFILTERBACKEND::pfnFilterRead
 */
static DECLCALLBACK(int) cryptFilterRead(void *pvBackendData, uint64_t uOffset, size_t cbRead,
                                         PVDIOCTX pIoCtx)
{
    PVDFILTERCRYPT pFilter = (PVDFILTERCRYPT)pvBackendData;
    size_t cbDataUnit = vdIfIoIntIoCtxGetDataUnitSize(pFilter->pIfIo, pIoCtx);

    AssertReturn(cbDataUnit, VERR_INVALID_STATE);
    AssertReturn(!(uOffset % cbDataUnit), VERR_INVALID_PARAMETER);
    AssertReturn(!(cbRead % cbDataUnit), VERR_INVALID_PARAMETER);

    if (pFilter->enmAlgorithm != VDFILTERCRYPTALGO_XOR)
        return cryptFilterEncDecXts(pFilter, pIoCtx, uOffset, cbRead, cbDataUnit, false /* fEncrypt */);
    return cryptFilterEncDecXor(pFilter, pIoCtx, cbRead);
}

/**
 * @copydoc VDFILTERBACKEND::pfnFilterWrite
 */
static DECLCALLBACK(int) cryptFilterWrite(void *pvBackendData, uint64_t uOffset, size_t cbWrite,
                                          PVDIOCTX pIoCtx)
{
    PVDFILTERCRYPT pFilter = (PVDFILTERCRYPT)pvBackendData;
    size_t cbDataUnit = vdIfIoIntIoCtxGetDataUnitSize(pFilter->pIfIo, pIoCtx);

    AssertReturn(cbDataUnit, VERR_INVALID_STATE);
    AssertReturn(!(uOffset % cbDataUnit), VERR_INVALID_PARAMETER);
    AssertReturn(!(cbWrite % cbDataUnit), VERR_INVALID_PARAMETER);

    if (pFilter->enmAlgorithm != VDFILTERCRYPTALGO_XOR)
        return cryptFilterEncDecXts(pFilter, pIoCtx, uOffset, cbWrite, cbDataUnit, true /* fEncrypt */);
    return cryptFilterEncDecXor(pFilter, pIoCtx, cbWrite);
}


/**
 * Filter plugin interface.
 */
const VDFILTERBACKEND g_VDFilterCrypt =
{
    /* u32Version */
    VD_FLTBACKEND_VERSION,
    /* pszBackendName */
    "CRYPT",
    /* paConfigInfo */
    s_cryptConfigInfo,
    /* pfnCreate */
    cryptCreate,
    /* pfnDestroy */
    cryptDestroy,
    /* pfnFilterRead */
    cryptFilterRead,
    /* pfnFilterWrite */
    cryptFilterWrite,
    /* u32VersionEnd */
    VD_FLTBACKEND_VERSION
};

