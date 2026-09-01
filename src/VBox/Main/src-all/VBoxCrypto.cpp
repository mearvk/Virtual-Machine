/* $Id: VBoxCrypto.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VirtualBox Cryptographic support module (for full VM encryption).
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/base64.h>
#include <iprt/memsafer.h>
#include <iprt/string.h>
#include <iprt/crypto/cipher.h>
#include <iprt/crypto/digest.h>
#include <iprt/crypto/misc.h>

#include <VBox/err.h>

#include "VBoxCryptoInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/**
 * Key store structure.
 *
 * @note Everything is stored little endian.
 */
typedef struct VBoxKeyStore
{
    /** Magic value. */
    uint32_t u32Magic;
    /** Version of the header */
    uint16_t u16Version;
    /** Padding. */
    uint16_t u16Padding;
    /** Cipher string */
    char     szCipher[32];
    /** Key derivation function used. */
    char     szKeyDeriv[32];
    /** Key size in bytes. */
    uint32_t cbKey;
    /** The DEK digest for verification of the password. */
    uint8_t  abDekDigest[32];
    /** Size of the DEK digest. */
    uint32_t cbDekDigest;
    /** Salt for the DEK digest. */
    uint8_t  abDekDigestSalt[32];
    /** Iterations count of the DEK digest. */
    uint32_t cDekDigestIterations;
    /** Salt for the DEK. */
    uint8_t  abDekSalt[32];
    /** iterations count for the DEK. */
    uint32_t cDekIterations;
    /** Size of the encrypted key in bytes. */
    uint32_t cbDekEnc;
    /** The encrypted DEK. */
    uint8_t  abDekEnc[64];
} VBoxKeyStore;
AssertCompileSize(VBoxKeyStore, 252);
AssertCompileMemberAlignment(VBoxKeyStore, szCipher, 8);
AssertCompileMemberAlignment(VBoxKeyStore, cbKey, 4);
AssertCompileMemberAlignment(VBoxKeyStore, abDekSalt, 4);
AssertCompileMemberAlignment(VBoxKeyStore, cDekIterations, 4);
typedef VBoxKeyStore *PVBoxKeyStore;

/** Key store magic (ENCS). */
#define VBOX_KEYSTORE_MAGIC   UINT32_C(0x454e4353)
/** Version identifier. */
/** @todo The version is at 2 here because version 1 key stores (used solely for disk encryption) have a misaligned layout
 * but will be handled here later as well. */
#define VBOX_KEYSTORE_VERSION UINT16_C(0x0200)

/** Minimum amount of iterations for PBKDF2. */
#define VBOX_KEYSTORE_ITERATIONS_MIN     20000
/** Maximum time for the PBKDF2 function to take in milliseconds. */
#define VBOX_KEYSTORE_PBKDF2_COMPUTE_MAX   250


/**
 * Supported algorithms
 */
typedef enum VBOXCRYPTALGO
{
    /** Invalid algorithm. */
    VBOXCRYPTALGO_INVALID = 0,
    /** Simple XOR "encryption", only used for testing. DO NOT USE IN PRODUCTION! */
    VBOXCRYPTALGO_XOR,
    /** AES GCM 128bit. */
    VBOXCRYPTALGO_AES_GCM128,
    /** AES GCM 256bit. */
    VBOXCRYPTALGO_AES_GCM256,
    /** AES CTR 128bit. */
    VBOXCRYPTALGO_AES_CTR128,
    /** AES CTR 256bit. */
    VBOXCRYPTALGO_AES_CTR256,
    /** 32bit hack. */
    VBOXCRYPTALGO_32BIT_HACK = 0x7fffffff
} VBOXCRYPTALGO, *PVBOXCRYPTALGO;

typedef enum VBOXCRYPTALGOMODE
{
    /* Invalid mode */
    VBOXCRYPTALGOMODE_INVALID = 0,
    /* Pseudo mode for XOR */
    VBOXCRYPTALGOMODE_XOR,
    /* GCM mode */
    VBOXCRYPTALGOMODE_GCM,
    /* CTR mode */
    VBOXCRYPTALGOMODE_CTR

} VBOXCRYPTALGOMODE;

/**
 * Mapping entry between string repesentation of algorithm and enum.
 */
typedef struct VBOXCRYPTALGOMAPPING
{
    /** String represenation of the algorithm. */
    const char *pszName;
    /** Enum value. */
    VBOXCRYPTALGO enmAlgorithm;
    /** Algorithm Mode */
    VBOXCRYPTALGOMODE enmMode;
    /** Cipher type enum value. */
    RTCRCIPHERTYPE enmCipherType;
    /** Key size. */
    size_t cbKey;
} VBOXCRYPTALGOMAPPING;
typedef VBOXCRYPTALGOMAPPING *PVBOXCRYPTALGOMAPPING;
typedef const VBOXCRYPTALGOMAPPING *PCVBOXCRYPTALGOMAPPING;

/**
 * Crypting execution context.
 */
typedef struct VBOXCRYPTOCTXINT
{
    PCVBOXCRYPTALGOMAPPING pCipherAlgo;
    char *pszEncStore;
    uint8_t *pbKey;
    size_t cbKey;
    RTCRCIPHER hCipher;
    RTCRCIPHERCTX pCipherCtx;
} VBOXCRYPTOCTXINT;
/** Pointer to a cryptographic context. */
typedef VBOXCRYPTOCTXINT *PVBOXCRYPTOCTXINT;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) vboxCryptoKeyStoreGetDekFromEncoded(const char *pszEnc, const char *pszPassword,
                                                             uint8_t **ppbDek, size_t *pcbDek, char **ppszCipher);
static DECLCALLBACK(int) vboxCryptoKeyStoreCreate(const char *pszPassword, const uint8_t *pbDek, size_t cbDek,
                                                  const char *pszCipher, char **ppszEnc);


/** Mapping of algorithm names to enum. */
static const VBOXCRYPTALGOMAPPING s_aCryptAlgoMapping[] =
{
    /* pszName      enmAlgorithm              enmMode                enmCipherType               cbKey */
    {"XOR",        VBOXCRYPTALGO_XOR,        VBOXCRYPTALGOMODE_XOR, RTCRCIPHERTYPE_INVALID,     16},
    {"AES-GCM128", VBOXCRYPTALGO_AES_GCM128, VBOXCRYPTALGOMODE_GCM, RTCRCIPHERTYPE_GCM_AES_128, 16},
    {"AES-GCM256", VBOXCRYPTALGO_AES_GCM256, VBOXCRYPTALGOMODE_GCM, RTCRCIPHERTYPE_GCM_AES_256, 32},
    {"AES-CTR128", VBOXCRYPTALGO_AES_CTR128, VBOXCRYPTALGOMODE_CTR, RTCRCIPHERTYPE_CTR_AES_128, 16},
    {"AES-CTR256", VBOXCRYPTALGO_AES_CTR256, VBOXCRYPTALGOMODE_CTR, RTCRCIPHERTYPE_CTR_AES_256, 32},
};


/**
 * Encrypts the given data in either XTS-128 or XTS-256 mode.
 *
 * @returns VBox status code.
 * @param   pThis               The cryptographic context instance.
 * @param   pvData              The data to encrypt.
 * @param   cbData              Number of bytes to encrypt.
 * @param   pvEncrypted         Where to store the encrypted data.
 * @param   cbBufEncrypted      Number of bytes the encrpyted data buffer can hold.
 */
static int vboxCryptoCtxEncryptAesXts(PVBOXCRYPTOCTXINT pThis, const void *pvData, size_t cbData,
                                      void *pvEncrypted, size_t cbBufEncrypted)
{
    AssertReturn(pThis->cbKey == 32 || pThis->cbKey == 64, VERR_INTERNAL_ERROR);

    RTCRCIPHER hCipher;
    int vrc = RTCrCipherOpenByType(&hCipher,
                                     pThis->cbKey == 32
                                   ? RTCRCIPHERTYPE_XTS_AES_128
                                   : RTCRCIPHERTYPE_XTS_AES_256,
                                   0);
    if (RT_SUCCESS(vrc))
    {
        size_t  cbEncrypted = 0;
        uint8_t abIv[16];
        RT_ZERO(abIv);
        vrc = RTCrCipherEncrypt(hCipher, pThis->pbKey, pThis->cbKey, &abIv[0], sizeof(abIv),
                                pvData, cbData,
                                pvEncrypted, cbBufEncrypted, &cbEncrypted);
        if (RT_SUCCESS(vrc))
            AssertReturn(cbEncrypted == cbBufEncrypted, VERR_INTERNAL_ERROR);

        uint32_t cRefs = RTCrCipherRelease(hCipher);
        Assert(cRefs == 0); RT_NOREF_PV(cRefs);
    }

    return vrc;
}


/**
 * Calculates the split at which the padding for an encrypted file should occur.
 *
 * @returns VBox status code.
 * @param   hCryptoCtx          The cryptographic context handle.
 * @param   cbUnit              Size of a data unit in bytes (the calculated split will be between 0 and cbUnit).
 * @param   pvData              The data to use for calculating the split.
 * @param   cbData              Size of the data in bytes.
 * @param   poffSplit           Where to store the offset inside 0...cbUnit for the padding split.
 *
 * @note This doesn't really belong here because it is an implementation detail for encrypted files but I didn't want to
 *       offer more primitive APIs to the file encryption code or make internals of the context public.
 */
DECLHIDDEN(int) vboxCryptoCtxCalculatePaddingSplit(VBOXCRYPTOCTX hCryptoCtx, size_t cbUnit, const void *pvData, size_t cbData,
                                                   size_t *poffSplit)
{
    PVBOXCRYPTOCTXINT pThis = hCryptoCtx;

    RTCRDIGEST hDigest;
    int vrc = RTCrDigestCreateByType(&hDigest, RTDIGESTTYPE_SHA256);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTCrDigestUpdate(hDigest, pvData, cbData);
        if (RT_SUCCESS(vrc))
        {
            uint8_t abHash[256 / 8];
            AssertReturn(sizeof(abHash) == RTCrDigestGetHashSize(hDigest), VERR_INTERNAL_ERROR);

            vrc = RTCrDigestFinal(hDigest, &abHash[0], sizeof(abHash));
            if (RT_SUCCESS(vrc))
            {
                /* Encrypt the hash in XTS mode with the DEK. */
                uint16_t abHashEncrypted[sizeof(abHash) / sizeof(uint16_t)];

                vrc = vboxCryptoCtxEncryptAesXts(pThis, &abHash[0], sizeof(abHash),
                                                 &abHashEncrypted[0], sizeof(abHashEncrypted));
                if (RT_SUCCESS(vrc))
                {
                    uint16_t cbSplit = 0;
                    for (uint16_t i = 0; i < RT_ELEMENTS(abHashEncrypted); i++)
                        cbSplit ^= abHashEncrypted[i];

                    *poffSplit = cbSplit % cbUnit;
                }
            }
        }
        RTCrDigestRelease(hDigest);
    }

    return vrc;
}


static PCVBOXCRYPTALGOMAPPING vboxCryptoQueryAlgorithmMapping(const char *pszAlgorithm)
{
    PCVBOXCRYPTALGOMAPPING pMapping = NULL;

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
 * Creates a new DEK depending on the configured cipher.
 *
 * @returns VBox status code.
 * @param   pThis               The cryptographic context.
 */
static int vboxCryptoKeyStoreDekCreate(PVBOXCRYPTOCTXINT pThis)
{
    pThis->cbKey = pThis->pCipherAlgo->cbKey;

    int vrc = RTMemSaferAllocZEx((void **)&pThis->pbKey, pThis->cbKey, 0 /* fFlags */);
    if (RT_SUCCESS(vrc))
        vrc = RTCrRandBytes((void *)pThis->pbKey, pThis->cbKey);

    return vrc;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxCreate}
 */
static DECLCALLBACK(int) vboxCryptoCtxCreate(const char *pszCipher, const char *pszPassword, PVBOXCRYPTOCTX phCryptoCtx)
{
    AssertPtrReturn(pszCipher, VERR_INVALID_PARAMETER);
    AssertReturn(*pszCipher != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPassword, VERR_INVALID_PARAMETER);
    AssertReturn(*pszPassword != '\0', VERR_INVALID_PARAMETER);
    AssertReturn(phCryptoCtx, VERR_INVALID_PARAMETER);

    PCVBOXCRYPTALGOMAPPING pCipherAlgo = vboxCryptoQueryAlgorithmMapping(pszCipher);
    if (!pCipherAlgo)
        return VERR_INVALID_PARAMETER;

    int vrc = VINF_SUCCESS;
    PVBOXCRYPTOCTXINT pThis = (PVBOXCRYPTOCTXINT)RTMemAllocZ(sizeof(*pThis));
    if (RT_LIKELY(pThis))
    {
        pThis->pCipherAlgo = pCipherAlgo;
        vrc = vboxCryptoKeyStoreDekCreate(pThis);
        if (RT_SUCCESS(vrc))
        {
            vrc = vboxCryptoKeyStoreCreate(pszPassword, pThis->pbKey, pThis->cbKey, pszCipher, &pThis->pszEncStore);
            if (RT_SUCCESS(vrc))
            {
                *phCryptoCtx = pThis;
                return VINF_SUCCESS;
            }

            RTMemSaferFree(pThis->pbKey, pThis->cbKey);
        }

        RTMemFree(pThis);
    }
    else
        vrc = VERR_NO_MEMORY;

    return vrc;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxLoad}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxLoad(const char *pszStoredCtx, const char *pszPassword, PVBOXCRYPTOCTX phCryptoCtx)
{
    AssertPtrReturn(pszStoredCtx, VERR_INVALID_PARAMETER);
    AssertReturn(*pszStoredCtx != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPassword, VERR_INVALID_PARAMETER);
    AssertReturn(*pszPassword != '\0', VERR_INVALID_PARAMETER);
    AssertReturn(phCryptoCtx, VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;
    PVBOXCRYPTOCTXINT pThis = (PVBOXCRYPTOCTXINT)RTMemAllocZ(sizeof(*pThis));
    if (RT_LIKELY(pThis))
    {
        pThis->pszEncStore = RTStrDup(pszStoredCtx);
        if (RT_LIKELY(pThis->pszEncStore))
        {
            char *pszCipher = NULL;
            vrc = vboxCryptoKeyStoreGetDekFromEncoded(pszStoredCtx, pszPassword, &pThis->pbKey, &pThis->cbKey, &pszCipher);
            if (RT_SUCCESS(vrc))
            {
                pThis->pCipherAlgo = vboxCryptoQueryAlgorithmMapping(pszCipher);
                if (RT_LIKELY(pThis->pCipherAlgo))
                {
                    *phCryptoCtx = pThis;
                    return VINF_SUCCESS;
                }
                else
                    vrc = VERR_NOT_SUPPORTED;
                RTStrFree(pszCipher);
            }

            RTStrFree(pThis->pszEncStore);
            pThis->pszEncStore = NULL;
        }
        else
            vrc = VERR_NO_MEMORY;

        RTMemFree(pThis);
    }
    else
        vrc = VERR_NO_MEMORY;

    return vrc;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxDestroy}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxDestroy(VBOXCRYPTOCTX hCryptoCtx)
{
    PVBOXCRYPTOCTXINT pThis = hCryptoCtx;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    if (pThis->pbKey)
        RTMemSaferFree(pThis->pbKey, pThis->cbKey);
    if (pThis->pszEncStore)
        RTStrFree(pThis->pszEncStore);

    RTMemFree(pThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxSave}
 */
static DECLCALLBACK(int) vboxCryptoCtxSave(VBOXCRYPTOCTX hCryptoCtx, char **ppszStoredCtx)
{
    PVBOXCRYPTOCTXINT pThis = hCryptoCtx;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(ppszStoredCtx, VERR_INVALID_PARAMETER);

    *ppszStoredCtx = RTStrDup(pThis->pszEncStore);
    return *ppszStoredCtx != NULL ? VINF_SUCCESS : VERR_NO_MEMORY;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxPasswordChange}
 */
static DECLCALLBACK(int) vboxCryptoCtxPasswordChange(VBOXCRYPTOCTX hCryptoCtx, const char *pszPassword)
{
    PVBOXCRYPTOCTXINT pThis = hCryptoCtx;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pszPassword, VERR_INVALID_PARAMETER);
    AssertReturn(*pszPassword != '\0', VERR_INVALID_PARAMETER);

    const char *pszCipher = pThis->pCipherAlgo->pszName;
    char *pszOldEncStore = pThis->pszEncStore;
    int vrc = vboxCryptoKeyStoreCreate(pszPassword, pThis->pbKey, pThis->cbKey, pszCipher, &pThis->pszEncStore);

    if (RT_SUCCESS(vrc) && pszOldEncStore)
        RTStrFree(pszOldEncStore);

    return vrc;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxQueryEncryptedSize}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxQueryEncryptedSize(VBOXCRYPTOCTX hCryptoCtx, size_t cbPlainText, size_t *pcbEncrypted)
{
    PVBOXCRYPTOCTXINT pThis = hCryptoCtx;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(cbPlainText > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbEncrypted, VERR_INVALID_POINTER);

    switch (pThis->pCipherAlgo->enmMode)
    {
        case VBOXCRYPTALGOMODE_GCM:
            *pcbEncrypted = cbPlainText + 16 /* AES-GCM tag */ + 12 /* IV */;
            break;
        case VBOXCRYPTALGOMODE_CTR:
            *pcbEncrypted = cbPlainText + 16 /* IV */;
            break;
        default:
            *pcbEncrypted = cbPlainText;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxQueryDecryptedSize}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxQueryDecryptedSize(VBOXCRYPTOCTX hCryptoCtx, size_t cbEncrypted, size_t *pcbPlaintext)
{
    PVBOXCRYPTOCTXINT pThis = hCryptoCtx;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(cbEncrypted > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbPlaintext, VERR_INVALID_POINTER);

    switch (pThis->pCipherAlgo->enmMode)
    {
        case VBOXCRYPTALGOMODE_GCM:
            *pcbPlaintext = cbEncrypted < 16 + 12 ? 0 : cbEncrypted - 16 /* AES-GCM tag */ - 12 /* IV */;
            break;
        case VBOXCRYPTALGOMODE_CTR:
            *pcbPlaintext = cbEncrypted < 16 ? 0 : cbEncrypted - 16 /* IV */;
            break;
        default:
            *pcbPlaintext = cbEncrypted;
    }

    return VINF_SUCCESS;
}


static int vboxCryptoCtxXor(PVBOXCRYPTOCTXINT pThis, const void *pvIn, size_t cbIn, void *pvOut, size_t cbOut)
{
    AssertReturn(cbIn == cbOut, VERR_INVALID_PARAMETER);

    const uint8_t *pbIn = (const uint8_t *)pvIn;
    uint8_t *pbOut = (uint8_t *)pvOut;

    for (size_t i = 0; i < cbIn; i++)
        pbOut[i] = pbIn[i] ^ pThis->pbKey[i % pThis->cbKey];

    return VINF_SUCCESS;
}


static int vboxCryptoCtxEncryptAes(PVBOXCRYPTOCTXINT pThis, bool fPartial, void const *pvIV, size_t cbIV,
                                   void const *pvPlainText, size_t cbPlainText, void const *pvAuthData, size_t cbAuthData,
                                   void *pvEncrypted, size_t cbEncrypted, size_t *pcbEncrypted)
{
    size_t cbEncryptedReq;
    int vrc = vboxCryptoCtxQueryEncryptedSize(pThis, cbPlainText, &cbEncryptedReq);
    AssertRCReturn(vrc, vrc);

    /** @todo Review! */
    if (   (   (!fPartial || (fPartial && pThis->hCipher == NIL_RTCRCIPHER))
            && cbEncrypted < cbEncryptedReq)
        || (fPartial && pThis->hCipher != NIL_RTCRCIPHER && cbEncrypted < cbPlainText))
        return VERR_INVALID_PARAMETER;

    if (fPartial && pThis->pCipherAlgo->enmMode == VBOXCRYPTALGOMODE_GCM)
        return VERR_INVALID_PARAMETER;

    if (!fPartial && (pvPlainText == NULL || cbPlainText == 0))
        return VERR_INVALID_PARAMETER;

    if (   pvIV != NULL
        && (   (pThis->pCipherAlgo->enmMode == VBOXCRYPTALGOMODE_CTR && cbIV != 16)
            || (pThis->pCipherAlgo->enmMode == VBOXCRYPTALGOMODE_GCM && cbIV != 12)))
        return VERR_INVALID_PARAMETER;

    if (fPartial || pThis->hCipher != NIL_RTCRCIPHER)
    {
        size_t cbEncryptedIV = 0;
        uint8_t *pbEncrypted = (uint8_t *)pvEncrypted;
        if (pThis->hCipher == NIL_RTCRCIPHER)
        {
            vrc = RTCrCipherOpenByType(&pThis->hCipher, pThis->pCipherAlgo->enmCipherType, 0);
            AssertRCReturnStmt(vrc, pThis->hCipher = NIL_RTCRCIPHER, vrc);

            /* CTR mode: <IV 16 bytes><data> */
            if (pvIV != NULL)
                memcpy(pvEncrypted, pvIV, 16);
            else
                vrc = RTCrRandBytes(pvEncrypted, 16);

            if (RT_SUCCESS(vrc))
                vrc = RTCrCipherCtxEncryptInit(pThis->hCipher, (const void *)pThis->pbKey, pThis->cbKey,
                                               pvEncrypted, 16, NULL, 0, &pThis->pCipherCtx);
            if (RT_FAILURE(vrc))
            {
                RTCrCipherCtxFree(pThis->pCipherCtx);
                pThis->pCipherCtx = NIL_RTCRCIPHERCTX;
                RTCrCipherRelease(pThis->hCipher);
                pThis->hCipher = NIL_RTCRCIPHER;
            }
            else
            {
                pbEncrypted = &pbEncrypted[16];
                cbEncrypted -= 16;
                cbEncryptedIV = 16;
            }
        }
        size_t cbEncrypted1 = 0;
        /* Initialization without encrypting is allowed in partial mode */
        if (RT_SUCCESS(vrc) && pvPlainText && cbPlainText > 0)
            vrc = RTCrCipherCtxEncryptProcess(pThis->pCipherCtx, pvPlainText, cbPlainText,
                                              (void *)pbEncrypted, cbEncrypted, &cbEncrypted1);
        size_t cbEncrypted2 = 0;
        if (RT_SUCCESS(vrc) && !fPartial)
        {
            vrc = RTCrCipherCtxEncryptFinish(pThis->pCipherCtx, (void *)&pbEncrypted[cbEncrypted1],
                                             &cbEncrypted2, NULL, 0, NULL);
            if (RT_SUCCESS(vrc))
            {
                RTCrCipherCtxFree(pThis->pCipherCtx);
                pThis->pCipherCtx = NIL_RTCRCIPHERCTX;
                RTCrCipherRelease(pThis->hCipher);
                pThis->hCipher = NIL_RTCRCIPHER;
            }
        }
        if (RT_SUCCESS(vrc) && pcbEncrypted)
            *pcbEncrypted = cbEncryptedIV + cbEncrypted1 + cbEncrypted2;
    }
    else
    {
        RTCRCIPHER hCipher = NULL;
        vrc = RTCrCipherOpenByType(&hCipher, pThis->pCipherAlgo->enmCipherType, 0);
        AssertRCReturnStmt(vrc, hCipher = NIL_RTCRCIPHER, vrc);

        uint8_t *pbEncrypted = (uint8_t *)pvEncrypted;
        /*
         * GCM mode: <IV 12 bytes><TAG 16 bytes><data>
         * CTR mode: <IV 16 bytes><data>
         */
        cbIV = 12;
        size_t cbTag = 16;
        size_t idxEnc = cbIV + cbTag;
        void *pTag = (void *)&pbEncrypted[12];
        const void *pvAuth = pvAuthData;
        size_t cbAuth = cbAuthData;
        if (pThis->pCipherAlgo->enmMode == VBOXCRYPTALGOMODE_CTR)
        {
            cbIV = 16;
            idxEnc = cbIV;
            pTag = NULL;
            cbTag = 0;
            pvAuth = NULL;
            cbAuth = 0;
        }
        if (pvIV != NULL)
            memcpy(pvEncrypted, pvIV, cbIV);
        else
            vrc = RTCrRandBytes(pvEncrypted, cbIV);
        if (RT_SUCCESS(vrc))
        {
            size_t cbEnc = 0;
            size_t cbTag1 = 0;
            vrc = RTCrCipherEncryptEx(hCipher, (const void *)pThis->pbKey, pThis->cbKey,
                                     pvEncrypted, cbIV, pvAuth, cbAuth, pvPlainText, cbPlainText,
                                     (void *)&pbEncrypted[idxEnc], cbEncrypted - idxEnc, &cbEnc,
                                     pTag, cbTag, &cbTag1);
            if (RT_SUCCESS(vrc))
            {
                if (cbTag1 != cbTag)
                    vrc = VERR_INTERNAL_ERROR;
                else
                    *pcbEncrypted = cbEnc + idxEnc;
            }
        }
        RTCrCipherRelease(hCipher);
        pThis->hCipher = NIL_RTCRCIPHER;
    }
    return vrc;
}


static int vboxCryptoCtxDecryptAes(PVBOXCRYPTOCTXINT pThis, bool fPartial, const void *pvEncrypted,
                                   size_t cbEncrypted, void const *pvAuthData, size_t cbAuthData,
                                   void *pvPlainText, size_t cbPlainText, size_t *pcbPlainText)
{
    size_t cbDecryptedReq;
    int vrc = vboxCryptoCtxQueryDecryptedSize(pThis, cbPlainText, &cbDecryptedReq);
    AssertRCReturn(vrc, vrc);

    /** @todo Review! */
    if (   (   (!fPartial || (fPartial && pThis->hCipher == NIL_RTCRCIPHER))
            && cbPlainText < cbDecryptedReq)
        || (fPartial && pThis->hCipher != NIL_RTCRCIPHER && cbPlainText < cbEncrypted))
        return VERR_INVALID_PARAMETER;

    if (fPartial && pThis->pCipherAlgo->enmMode == VBOXCRYPTALGOMODE_GCM)
        return VERR_INVALID_PARAMETER;

    if (pvEncrypted == NULL || cbEncrypted == 0)
        return VERR_INVALID_PARAMETER;

    if (fPartial || pThis->hCipher != NIL_RTCRCIPHER)
    {
        uint8_t *pbEncrypted = (uint8_t *)pvEncrypted;
        if (pThis->hCipher == NIL_RTCRCIPHER)
        {
            vrc = RTCrCipherOpenByType(&pThis->hCipher, pThis->pCipherAlgo->enmCipherType, 0);
            AssertRCReturnStmt(vrc, pThis->hCipher = NIL_RTCRCIPHER, vrc);

            /* CTR mode: <IV 16 bytes><data> */

            if (RT_SUCCESS(vrc))
                vrc = RTCrCipherCtxDecryptInit(pThis->hCipher, (const void *)pThis->pbKey, pThis->cbKey,
                                               pvEncrypted, 16, NULL, 0, NULL, 0, &pThis->pCipherCtx);
            if (RT_FAILURE(vrc))
            {
                RTCrCipherCtxFree(pThis->pCipherCtx);
                pThis->pCipherCtx = NIL_RTCRCIPHERCTX;
                RTCrCipherRelease(pThis->hCipher);
                pThis->hCipher = NIL_RTCRCIPHER;
            }
            else
            {
                pbEncrypted = &pbEncrypted[16];
                cbEncrypted -= 16;
            }
        }
        size_t cbDecrypted1 = 0;
        if (RT_SUCCESS(vrc))
            vrc = RTCrCipherCtxDecryptProcess(pThis->pCipherCtx, (void *)pbEncrypted, cbEncrypted,
                                              pvPlainText, cbPlainText, &cbDecrypted1);
        size_t cbDecrypted2 = 0;
        uint8_t *pbPlainText = (uint8_t *)pvPlainText;
        if (RT_SUCCESS(vrc) && !fPartial)
        {
            vrc = RTCrCipherCtxDecryptFinish(pThis->pCipherCtx, (void *)&pbPlainText[cbDecrypted1], &cbDecrypted2);
            if (RT_SUCCESS(vrc))
            {
                RTCrCipherCtxFree(pThis->pCipherCtx);
                pThis->pCipherCtx = NIL_RTCRCIPHERCTX;
                RTCrCipherRelease(pThis->hCipher);
                pThis->hCipher = NIL_RTCRCIPHER;
            }
        }
        if (RT_SUCCESS(vrc) && pcbPlainText)
            *pcbPlainText = cbDecrypted1 + cbDecrypted2;
    }
    else
    {
        RTCRCIPHER hCipher = NULL;
        vrc = RTCrCipherOpenByType(&hCipher, pThis->pCipherAlgo->enmCipherType, 0);
        AssertRCReturnStmt(vrc, hCipher = NIL_RTCRCIPHER, vrc);

        /*
         * GCM mode: <IV 12 bytes><TAG 16 bytes><data>
         * CTR mode: <IV 16 bytes><data>
         */
        uint8_t *pbEncrypted = (uint8_t *)pvEncrypted;

        size_t cbIV = 12;
        size_t cbTag = 16;
        size_t idxEnc = cbIV + cbTag;
        void *pTag = (void *)&pbEncrypted[12];
        const void *pvAuth = pvAuthData;
        size_t cbAuth = cbAuthData;
        if (pThis->pCipherAlgo->enmMode == VBOXCRYPTALGOMODE_CTR)
        {
            cbIV = 16;
            idxEnc = cbIV;
            pTag = NULL;
            cbTag = 0;
            pvAuth = NULL;
            cbAuth = 0;
        }
        size_t cbDec = 0;
        vrc = RTCrCipherDecryptEx(hCipher, (const void *)pThis->pbKey, pThis->cbKey,
                                 pvEncrypted, cbIV, pvAuth, cbAuth,
                                 pTag, cbTag, (void *)&pbEncrypted[idxEnc], cbEncrypted - idxEnc,
                                 pvPlainText, cbPlainText, &cbDec);
        if (RT_SUCCESS(vrc))
            *pcbPlainText = cbDec;
        RTCrCipherRelease(hCipher);
        pThis->hCipher = NIL_RTCRCIPHER;
    }
    return vrc;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxEncrypt}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxEncrypt(VBOXCRYPTOCTX hCryptoCtx, bool fPartial, void const *pvIV, size_t cbIV,
                                               void const *pvPlainText, size_t cbPlainText,
                                               void const *pvAuthData, size_t cbAuthData,
                                               void *pvEncrypted, size_t cbEncrypted,
                                               size_t *pcbEncrypted)
{
    PVBOXCRYPTOCTXINT pThis = hCryptoCtx;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvEncrypted, VERR_INVALID_PARAMETER);
    AssertReturn(cbEncrypted, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbEncrypted, VERR_INVALID_PARAMETER);

    switch (pThis->pCipherAlgo->enmAlgorithm)
    {
        case VBOXCRYPTALGO_XOR:
        {
            int vrc = vboxCryptoCtxXor(pThis, pvPlainText, cbPlainText, pvEncrypted, cbEncrypted);
            if (RT_SUCCESS(vrc))
                *pcbEncrypted = cbEncrypted;
            return vrc;
        }
        case VBOXCRYPTALGO_AES_GCM128:
        case VBOXCRYPTALGO_AES_GCM256:
        case VBOXCRYPTALGO_AES_CTR128:
        case VBOXCRYPTALGO_AES_CTR256:
            return vboxCryptoCtxEncryptAes(pThis, fPartial, pvIV, cbIV, pvPlainText,
                                           cbPlainText, pvAuthData, cbAuthData,
                                           pvEncrypted, cbEncrypted, pcbEncrypted);
        default:
            break;
    }

    AssertFailed();
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoCtxDecrypt}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoCtxDecrypt(VBOXCRYPTOCTX hCryptoCtx, bool fPartial,
                                               void const *pvEncrypted, size_t cbEncrypted,
                                               void const *pvAuthData, size_t cbAuthData,
                                               void *pvPlainText, size_t cbPlainText, size_t *pcbPlainText)
{
    PVBOXCRYPTOCTXINT pThis = hCryptoCtx;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvPlainText, VERR_INVALID_PARAMETER);
    AssertReturn(cbPlainText, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbPlainText, VERR_INVALID_PARAMETER);

    switch (pThis->pCipherAlgo->enmAlgorithm)
    {
        case VBOXCRYPTALGO_XOR:
        {
            int vrc = vboxCryptoCtxXor(pThis, pvEncrypted, cbEncrypted, pvPlainText, cbPlainText);
            if (RT_SUCCESS(vrc))
                *pcbPlainText = cbEncrypted;
            return vrc;
        }
        case VBOXCRYPTALGO_AES_GCM128:
        case VBOXCRYPTALGO_AES_GCM256:
        case VBOXCRYPTALGO_AES_CTR128:
        case VBOXCRYPTALGO_AES_CTR256:
            return vboxCryptoCtxDecryptAes(pThis, fPartial, pvEncrypted, cbEncrypted,
                                           pvAuthData, cbAuthData, pvPlainText, cbPlainText,
                                           pcbPlainText);
        default:
            break;
    }

    AssertFailed();
    return VERR_NOT_SUPPORTED;
}



/*
 *
 *  V B o x   K e y s t o r e   I n t e r f a c e
 *  V B o x   K e y s t o r e   I n t e r f a c e
 *  V B o x   K e y s t o r e   I n t e r f a c e
 *
 */

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
static uint32_t vboxCryptoKeyStoreIterationCountBenchmark(RTDIGESTTYPE enmDigestType, size_t cbInput,
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
            int vrc = RTCrPkcs5Pbkdf2Hmac(pbInput, cbInput, &abSalt[0],
                                          sizeof(abSalt), VBOX_KEYSTORE_ITERATIONS_MIN,
                                          enmDigestType, cbResult, pbOutput);
            if (RT_SUCCESS(vrc))
            { /* likely */ }
            else
            {
                cIterations = 0;
                break;
            }

            cIterations += VBOX_KEYSTORE_ITERATIONS_MIN;
        }

        if (cIterations)
            cIterations = RT_MAX(VBOX_KEYSTORE_ITERATIONS_MIN, cIterations);
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
static RTDIGESTTYPE vboxCryptoKeyStoreGetDigest(PVBoxKeyStore pKeyStore)
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
static RTCRCIPHERTYPE vboxCryptoKeyStoreGetCipherForDek(PVBoxKeyStore pKeyStore)
{
    /*
     * The DEK is always encrypted in XTS mode because of the limited space
     * for it there.
     */
    if (   !strncmp(&pKeyStore->szCipher[0], "AES-XTS128-PLAIN64", sizeof(pKeyStore->szCipher))
        || !strncmp(&pKeyStore->szCipher[0], "AES-GCM128", sizeof(pKeyStore->szCipher))
        || !strncmp(&pKeyStore->szCipher[0], "AES-CTR128", sizeof(pKeyStore->szCipher)))
        return RTCRCIPHERTYPE_XTS_AES_128;

    if (   !strncmp(&pKeyStore->szCipher[0], "AES-XTS256-PLAIN64", sizeof(pKeyStore->szCipher))
        || !strncmp(&pKeyStore->szCipher[0], "AES-GCM256", sizeof(pKeyStore->szCipher))
        || !strncmp(&pKeyStore->szCipher[0], "AES-CTR256", sizeof(pKeyStore->szCipher)))
        return RTCRCIPHERTYPE_XTS_AES_256;

    return RTCRCIPHERTYPE_INVALID;
}


/**
 * Returns the key size of derived key for DEK encoding based on IPRT cipher type.
 *
 * @return  Key size on success.
 * @param   enmCipher    The IPRT cipher type.
 */
static size_t vboxCryptoKeyStoreGetKeySizeForCipher(RTCRCIPHERTYPE enmCipher)
{
    switch (enmCipher)
    {
        case RTCRCIPHERTYPE_XTS_AES_128: return 32;
        case RTCRCIPHERTYPE_XTS_AES_256: return 64;
        default:                         return 64;
    }
}


/**
 * Derives a key from the given password.
 *
 * @return  IPRT status code.
 * @param   pszPassword    The password to derive the key from.
 * @param   pKeyStore      The key store containing the deriviation parameters.
 * @param   ppbDerivKey    Where to store the derived key on success. Must be freed with
 *                         RTMemSaferFree().
 * @param   pcbDerivKey    Where to store the size of the derived key on success;
 */
static int vboxCryptoKeyStoreDeriveKeyFromPassword(const char *pszPassword, PVBoxKeyStore pKeyStore,
                                             uint8_t **ppbDerivKey, size_t *pcbDerivKey)
{
    /* Allocate enough memory for the derived key. */
    void *pvDerivKey = NULL;
    size_t cbDerivKey = vboxCryptoKeyStoreGetKeySizeForCipher(vboxCryptoKeyStoreGetCipherForDek(pKeyStore));
    int vrc = RTMemSaferAllocZEx(&pvDerivKey, cbDerivKey, 0 /* fFlags */);
    if (RT_SUCCESS(vrc))
    {
        /* Do the key deriviation. */
        vrc = RTCrPkcs5Pbkdf2Hmac(pszPassword, strlen(pszPassword), &pKeyStore->abDekSalt[0],
                                  sizeof(pKeyStore->abDekSalt), pKeyStore->cDekIterations,
                                  vboxCryptoKeyStoreGetDigest(pKeyStore), cbDerivKey, pvDerivKey);
        if (RT_SUCCESS(vrc))
        {
            *ppbDerivKey = (uint8_t *)pvDerivKey;
            *pcbDerivKey = cbDerivKey;
        }
        else
            RTMemSaferFree(pvDerivKey, cbDerivKey);
    }

    return vrc;
}


/**
 * Decrypts the DEK in the given key store with the given key.
 *
 * @return  IPRT status code.
 * @param   pKeyStore          The key store containing the encrpted DEK.
 * @param   pbKey              The key to decrypt the DEK with.
 * @param   cbKey              The key size to decrypt the DEK with.
 * @param   ppbDekDecrypted    Where to store the decrypted DEK on success.
 *                             Must be freed with RTMemSaferFree().
 */
static int vboxCryptoKeyStoreDekDecryptWithKey(PVBoxKeyStore pKeyStore, const uint8_t *pbKey,
                                               const size_t cbKey, uint8_t **ppbDekDecrypted)
{
    RTCRCIPHER hCipher;
    int vrc = RTCrCipherOpenByType(&hCipher, vboxCryptoKeyStoreGetCipherForDek(pKeyStore), 0);
    if (RT_SUCCESS(vrc))
    {
        void *pvDek = NULL;
        vrc = RTMemSaferAllocZEx(&pvDek, pKeyStore->cbKey, 0 /* fFlags */);
        if (RT_SUCCESS(vrc))
        {
            size_t  cbDecrypted = 0;
            uint8_t abIv[16];
            RT_ZERO(abIv);
            vrc = RTCrCipherDecrypt(hCipher, pbKey, cbKey, &abIv[0], sizeof(abIv),
                                    &pKeyStore->abDekEnc[0], pKeyStore->cbDekEnc,
                                    pvDek, pKeyStore->cbKey, &cbDecrypted);
            if (RT_SUCCESS(vrc))
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
    return vrc;
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
static int vboxCryptoKeyStoreCheckDekAgainstDigest(PVBoxKeyStore pKeyStore, const uint8_t *pbDek)
{
    /* Query key derivation function. */
    int vrc;
    RTDIGESTTYPE enmDigestType = vboxCryptoKeyStoreGetDigest(pKeyStore);
    if (enmDigestType != RTDIGESTTYPE_INVALID)
    {
        /* Allocate buffer for holding the digest. */
        size_t const cbDekDigest = RTCrDigestTypeToHashSize(enmDigestType);
        uint8_t     *pbDekDigest = (uint8_t *)RTMemTmpAllocZ(cbDekDigest);
        if (pbDekDigest)
        {
            /* Do the magic and compare outcome. */
            vrc = RTCrPkcs5Pbkdf2Hmac(pbDek, pKeyStore->cbKey, &pKeyStore->abDekDigestSalt[0],
                                      sizeof(pKeyStore->abDekDigestSalt), pKeyStore->cDekDigestIterations,
                                      enmDigestType, pKeyStore->cbDekDigest, pbDekDigest);
            if (   RT_SUCCESS(vrc)
                && memcmp(pbDekDigest, pKeyStore->abDekDigest, cbDekDigest) == 0)
                vrc = VINF_SUCCESS;
            else
                vrc = VERR_VD_PASSWORD_INCORRECT;
            RTMemTmpFree(pbDekDigest);
        }
        else
            vrc = VERR_NO_MEMORY;
    }
    else
        vrc = VERR_INVALID_PARAMETER;
    return vrc;
}


/**
 * Generate a digest of the given DEK and store in the given key store.
 *
 * @return  IPRT status code.
 * @param   pKeyStore    The key store to store the digest in.
 * @param   pbDek        The DEK to generate the digest from.
 */
static int vboxCryptoKeyStoreDekDigestGenerate(PVBoxKeyStore pKeyStore, const uint8_t *pbDek)
{
    /* Query key derivation function. */
    int vrc;
    RTDIGESTTYPE enmDigestType = vboxCryptoKeyStoreGetDigest(pKeyStore);
    if (enmDigestType != RTDIGESTTYPE_INVALID)
    {
        /* Create salt. */
        vrc = RTCrRandBytes(&pKeyStore->abDekDigestSalt[0], sizeof(pKeyStore->abDekDigestSalt));
        if (RT_SUCCESS(vrc))
        {
            pKeyStore->cDekDigestIterations = VBOX_KEYSTORE_ITERATIONS_MIN;

            /* Generate digest. */
            uint32_t const cbDekDigest = RTCrDigestTypeToHashSize(enmDigestType);
            vrc = RTCrPkcs5Pbkdf2Hmac(pbDek, pKeyStore->cbKey, &pKeyStore->abDekDigestSalt[0],
                                      sizeof(pKeyStore->abDekDigestSalt), pKeyStore->cDekDigestIterations,
                                      enmDigestType, cbDekDigest, &pKeyStore->abDekDigest[0]);
            if (RT_SUCCESS(vrc))
            {
                pKeyStore->cbDekDigest = cbDekDigest;
                vrc = VINF_SUCCESS;
            }
            else
                vrc = VERR_ACCESS_DENIED; /** @todo Better status code. */
        }
    }
    else
        vrc = VERR_INVALID_PARAMETER;
    return vrc;
}


/**
 * Encrypt the given DEK with the given key and store it into the key store.
 *
 * @return  IPRT status code.
 * @param   pKeyStore    The key store to store the encrypted DEK in.
 * @param   pbKey        The key to encrypt the DEK with.
 * @param   cbKey        The key size to encrypt the DEK with.
 * @param   pbDek        The DEK to encrypt.
 */
static int vboxCryptoKeyStoreDekEncryptWithKey(PVBoxKeyStore pKeyStore, const uint8_t *pbKey,
                                               const size_t cbKey, const uint8_t *pbDek)
{
    RTCRCIPHER hCipher;
    int vrc = RTCrCipherOpenByType(&hCipher, vboxCryptoKeyStoreGetCipherForDek(pKeyStore), 0);
    if (RT_SUCCESS(vrc))
    {
        size_t  cbEncrypted = 0;
        uint8_t abIv[16];
        RT_ZERO(abIv);
        vrc = RTCrCipherEncrypt(hCipher, pbKey, cbKey, &abIv[0], sizeof(abIv),
                                pbDek, pKeyStore->cbKey,
                                &pKeyStore->abDekEnc[0], sizeof(pKeyStore->abDekEnc), &cbEncrypted);
        if (RT_SUCCESS(vrc))
            pKeyStore->cbDekEnc = (uint32_t)cbEncrypted;

        uint32_t cRefs = RTCrCipherRelease(hCipher);
        Assert(cRefs == 0); RT_NOREF_PV(cRefs);
    }
    return vrc;
}


/**
 * Encodes the given key store in a base64 string.
 *
 * @return IPRT status code.
 * @param  pKeyStore    The key store to encode.
 * @param  ppszEnc      Where to store the encoded key store on success.
 *                      Must be freed with RTMemFree().
 */
static int vboxCryptoKeyStoreEncode(PVBoxKeyStore pKeyStore, char **ppszEnc)
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

    int vrc = RTBase64Encode(pKeyStore, sizeof(*pKeyStore), pszEnc, cbEncoded, NULL);
    if (RT_SUCCESS(vrc))
        *ppszEnc = pszEnc;
    else
        RTMemFree(pszEnc);
    return vrc;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoKeyStoreGetDekFromEncoded}
 */
static DECLCALLBACK(int) vboxCryptoKeyStoreGetDekFromEncoded(const char *pszEnc, const char *pszPassword,
                                                             uint8_t **ppbKey, size_t *pcbKey, char **ppszCipher)
{
    VBoxKeyStore KeyStore;

    /* Convert to binary data and host endianess. */
    int vrc = RTBase64Decode(pszEnc, &KeyStore, sizeof(VBoxKeyStore), NULL, NULL);
    if (RT_FAILURE(vrc))
        return vrc;

    KeyStore.u32Magic             = RT_LE2H_U32(KeyStore.u32Magic);
    KeyStore.u16Version           = RT_LE2H_U16(KeyStore.u16Version);
    KeyStore.cbKey                = RT_LE2H_U32(KeyStore.cbKey);
    KeyStore.cbDekDigest          = RT_LE2H_U32(KeyStore.cbDekDigest);
    KeyStore.cDekDigestIterations = RT_LE2H_U32(KeyStore.cDekDigestIterations);
    KeyStore.cDekIterations       = RT_LE2H_U32(KeyStore.cDekIterations);
    KeyStore.cbDekEnc             = RT_LE2H_U32(KeyStore.cbDekEnc);
    if (   KeyStore.u32Magic != VBOX_KEYSTORE_MAGIC
        || KeyStore.u16Version != VBOX_KEYSTORE_VERSION)
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
    size_t   cbDerivKey = 0;
    vrc = vboxCryptoKeyStoreDeriveKeyFromPassword(pszPassword, &KeyStore, &pbDerivKey, &cbDerivKey);
    if (RT_SUCCESS(vrc))
    {
        /* Use the derived key to decrypt the DEK. */
        uint8_t *pbDekDecrypted = NULL;
        vrc = vboxCryptoKeyStoreDekDecryptWithKey(&KeyStore, pbDerivKey, cbDerivKey, &pbDekDecrypted);
        if (RT_SUCCESS(vrc))
        {
            /* Check the decrypted key with the digest. */
            vrc = vboxCryptoKeyStoreCheckDekAgainstDigest(&KeyStore, pbDekDecrypted);
            if (RT_SUCCESS(vrc))
            {
                *pcbKey = KeyStore.cbKey;
                *ppbKey = pbDekDecrypted;
                *ppszCipher = pszCipher;
            }
            else
                RTMemSaferFree(pbDekDecrypted, KeyStore.cbKey);
        }
    }

    if (pbDerivKey)
        RTMemSaferFree(pbDerivKey, cbDerivKey);

    if (RT_FAILURE(vrc))
        RTStrFree(pszCipher);

    return vrc;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoKeyStoreCreate}
 */
static DECLCALLBACK(int) vboxCryptoKeyStoreCreate(const char *pszPassword, const uint8_t *pbKey, size_t cbKey,
                                                  const char *pszCipher, char **ppszEnc)
{
    VBoxKeyStore KeyStore;
    RT_ZERO(KeyStore);

    KeyStore.u32Magic       = VBOX_KEYSTORE_MAGIC;
    KeyStore.u16Version     = VBOX_KEYSTORE_VERSION;

    /* Generate the salt for the DEK encryption. */
    int vrc = RTCrRandBytes(&KeyStore.abDekSalt[0], sizeof(KeyStore.abDekSalt));
    if (RT_SUCCESS(vrc))
    {
        vrc = RTStrCopy(&KeyStore.szCipher[0], sizeof(KeyStore.szCipher), pszCipher);
        if (RT_SUCCESS(vrc))
        {
            KeyStore.cbKey = (uint32_t)cbKey;
            Assert(KeyStore.cbKey == cbKey);
            strcpy(&KeyStore.szKeyDeriv[0], "PBKDF2-SHA256");
            KeyStore.cDekIterations = vboxCryptoKeyStoreIterationCountBenchmark(vboxCryptoKeyStoreGetDigest(&KeyStore),
                                                                                strlen(pszPassword),
                                                                                cbKey, VBOX_KEYSTORE_PBKDF2_COMPUTE_MAX);
            if (KeyStore.cDekIterations > 0)
            {
                vrc = vboxCryptoKeyStoreDekDigestGenerate(&KeyStore, pbKey);
                if (RT_SUCCESS(vrc))
                {
                    uint8_t *pbDerivKey = NULL;
                    size_t   cbDerivKey = 0;
                    vrc = vboxCryptoKeyStoreDeriveKeyFromPassword(pszPassword, &KeyStore, &pbDerivKey, &cbDerivKey);
                    if (RT_SUCCESS(vrc))
                    {
                        vrc = vboxCryptoKeyStoreDekEncryptWithKey(&KeyStore, pbDerivKey, cbDerivKey, pbKey);
                        if (RT_SUCCESS(vrc))
                            vrc = vboxCryptoKeyStoreEncode(&KeyStore, ppszEnc);

                        RTMemSaferFree(pbDerivKey, cbDerivKey);
                    }
                }
            }
            else
                vrc = VERR_INVALID_STATE;
        }
    }

    return vrc;
}



/**
 * The cryptographic support callback table.
 */
DECL_HIDDEN_CONST(VBOXCRYPTOIF) g_VBoxCryptoIf =
{
    /* .u32Magic = */                               VBOXCRYPTOIF_MAGIC,
    /* .u32Version = */                             VBOXCRYPTOIF_VERSION,
    /* .pszDesc = */                                "Puel Extension Pack cryptographic module for full VM encryption",
    /* .pfnCryptoCtxCreate = */                     vboxCryptoCtxCreate,
    /* .pfnCryptoCtxLoad = */                       vboxCryptoCtxLoad,
    /* .pfnCryptoCtxDestroy = */                    vboxCryptoCtxDestroy,
    /* .pfnCryptoCtxSave = */                       vboxCryptoCtxSave,
    /* .pfnCryptoCtxPasswordChange = */             vboxCryptoCtxPasswordChange,
    /* .pfnCryptoCtxQueryEncryptedSize = */         vboxCryptoCtxQueryEncryptedSize,
    /* .pfnCryptoCtxQueryDecryptedSize = */         vboxCryptoCtxQueryDecryptedSize,
    /* .pfnCryptoCtxEncrypt = */                    vboxCryptoCtxEncrypt,
    /* .pfnCryptoCtxDecrypt = */                    vboxCryptoCtxDecrypt,
    /* .pfnCryptoFileFromVfsFile = */               vboxCryptoFileFromVfsFile,
    /* .pfnCryptoIoStrmFromVfsIoStrmEncrypt = */    vboxCryptoIoStrmFromVfsIoStrmEncrypt,
    /* .pfnCryptoIoStrmFromVfsIoStrmDecrypt = */    vboxCryptoIoStrmFromVfsIoStrmDecrypt,
    /* .pfnCryptoKeyStoreGetDekFromEncoded = */     vboxCryptoKeyStoreGetDekFromEncoded,
    /* .pfnCryptoKeyStoreCreate = */                vboxCryptoKeyStoreCreate,
    /* .pfnReserved1 = */                           NULL,
    /* .pfnReserved2 = */                           NULL,
    /* .pfnReserved3 = */                           NULL,
    /* .pfnReserved4 = */                           NULL,
    /* .pfnReserved5 = */                           NULL,
    /* .pfnReserved6 = */                           NULL,
    /* .uReserved7 = */                             0,
    /* .u32EndMarker = */                           VBOXCRYPTOIF_VERSION
};
