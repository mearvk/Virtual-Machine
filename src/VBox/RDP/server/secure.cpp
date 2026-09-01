/* $Id: secure.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <iprt/bignum.h>
#include <iprt/md5.h>
#include <iprt/sha.h>
#include <iprt/crypto/key.h>
#include <iprt/crypto/misc.h>
#include <iprt/crypto/rc4.h>
#include <iprt/crypto/x509.h>

#include "secure.h"
#include "utils.h"

#define SEC_TAG_SRV_INFO        0x0c01 /* SC_CORE */
#define SEC_TAG_SRV_CRYPT       0x0c02 /* SC_SECURITY */
#define SEC_TAG_SRV_CHANNELS    0x0c03 /* SC_NET */

#define SEC_CERT_CHAIN_VERSION_1 0x00000001
#define SEC_CERT_CHAIN_VERSION_2 0x00000002

#define SEC_CERT_TEMPORARY 0x80000000

#define SEC_SIGNATURE_ALG_RSA    0x00000001

#define SEC_KEY_EXCHANGE_ALG_RSA 0x00000001

#define SEC_BB_RSA_KEY_BLOB       0x0006
#define SEC_BB_RSA_SIGNATURE_BLOB 0x0008

#define SEC_RSA1 0x31415352 /* RSA1 */


#define SEC_CLIENT_RANDOM       0x0001
#define SEC_ENCRYPT             0x0008
#define SEC_LOGON_INFO          0x0040
#define SEC_LICENCE_NEG         0x0080

/* RDP licensing constants */
#define LICENCE_TOKEN_SIZE      10
#define LICENCE_HWID_SIZE       20
#define LICENCE_SIGNATURE_SIZE  16

#define LICENCE_TAG_DEMAND      0x01
#define LICENCE_TAG_AUTHREQ     0x02
#define LICENCE_TAG_ISSUE       0x03
#define LICENCE_TAG_REISSUE     0x04
#define LICENCE_TAG_PRESENT     0x12
#define LICENCE_TAG_REQUEST     0x13
#define LICENCE_TAG_AUTHRESP    0x15
#define LICENCE_TAG_RESULT      0xff

#define LICENCE_TAG_USER        0x000f
#define LICENCE_TAG_HOST        0x0010


/**
 * Loads a private key from @a pszFilename, returning the IPRT key handle.
 */
static RTCRKEY loadPrivKey(const char *pszFilename)
{
    if (!pszFilename)
    {
        /** @todo add flag for only getting private keys! */
        RTERRINFOSTATIC ErrInfo;
        RTCRKEY hKey;
        int rc = RTCrKeyCreateFromFile(&hKey, 0, pszFilename, NULL /*pszPassword*/, RTErrInfoInitStatic(&ErrInfo));
        if (RT_SUCCESS(rc))
        {
            RTCRKEYTYPE enmType = RTCrKeyGetType(hKey);
            if (enmType == RTCRKEYTYPE_RSA_PRIVATE)
            {
                SECLOG(("SECTP: loadPrivKey loaded %s.\n", pszFilename));
                return hKey;
            }
            RTCrKeyRelease(hKey);
            SECLOG(("SECTP: loadPrivKey ERROR %s is not RSA private key: %d\n", pszFilename, enmType));
        }
        else if (RTErrInfoIsSet(&ErrInfo.Core))
        {
            SECLOG(("SECTP: loadPrivKey ERROR RTCrKeyCreateFromFile failed on %s: %Rrc - %s\n", pszFilename, rc, ErrInfo.Core.pszMsg));
        }
        else
        {
            SECLOG(("SECTP: loadPrivKey ERROR RTCrKeyCreateFromFile failed on %s: %Rrc\n", pszFilename, rc));
        }
    }
    else
    {
        SECLOG(("SECTP: loadPrivKey ERROR file name is NULL.\n"));
    }
    return NIL_RTCRKEY;
}

/**
 * Loads a certificate from @a pszFilename and into @a pCertificate.
 * @returns IPRT status code.
 */
static int loadX509(const char *pszFilename, PRTCRX509CERTIFICATE pCertificate)
{
    int rc = VERR_INVALID_POINTER;
    if (pszFilename)
    {
        RTERRINFOSTATIC ErrInfo;
        rc = RTCrX509Certificate_ReadFromFile(pCertificate, pszFilename, 0,
                                              &g_RTAsn1DefaultAllocator, RTErrInfoInitStatic(&ErrInfo));
        if (RT_SUCCESS(rc))
        {
            /** @todo add something for identifying RSA. */
            PCRTASN1OBJID pId = &pCertificate->TbsCertificate.SubjectPublicKeyInfo.Algorithm.Algorithm;
            if (   RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_RSA) == 0
                || RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_MD2_WITH_RSA) == 0
                || RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_MD4_WITH_RSA) == 0
                || RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_MD5_WITH_RSA) == 0
                || RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_SHA1_WITH_RSA) == 0
                || RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_SHA256_WITH_RSA) == 0
                || RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_SHA512_WITH_RSA) == 0
                || RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_SHA384_WITH_RSA) == 0
                || RTAsn1ObjId_CompareWithString(pId, RTCRX509ALGORITHMIDENTIFIERID_SHA224_WITH_RSA) == 0)
            {
                SECLOG(("SECTP: loadX509: loaded certificate '%s'\n", pszFilename));
                return VINF_SUCCESS;
            }
            SECLOG(("SECTP: loadX509 ERROR Certificate in '%s' is not using an RSA variant we know: %s\n", pszFilename, pId->szObjId));
            RTCrX509Certificate_Delete(pCertificate);
        }
        else if (RTErrInfoIsSet(&ErrInfo.Core))
        {
            SECLOG(("SECTP: loadX509 ERROR RTCrX509Certificate_ReadFromFile failed on %s: %Rrc - %s\n", pszFilename, rc, ErrInfo.Core.pszMsg));
        }
        else
        {
            SECLOG(("SECTP: loadX509 ERROR RTCrX509Certificate_ReadFromFile failed on %s: %Rrc\n", pszFilename, rc));
        }
    }
    else
    {
        SECLOG(("SECTP: loadX509 ERROR file name is NULL.\n"));
    }
    return rc;
}

/**
 * Returns the DER encoded length (in bytes) of @a rCert.
 */
static int lenX509(RTCRX509CERTIFICATE &rCert)
{
    uint32_t cbCert = 0;
    int rc = RTAsn1EncodePrepare(&rCert.SeqCore.Asn1Core, RTASN1ENCODE_F_DER, &cbCert, NULL);
    if (RT_SUCCESS(rc))
    {
        SECLOG(("SECTP: lenX509 cbCert = %d, p = %p.\n", cbCert, &rCert));
        return cbCert;
    }
    SECLOG(("SECTP: lenX509 ERROR %Rrc.\n", rc));
    return 0; /** @todo caller doesn't check for this! */
}

/**
 * Writes the DER encoded certificate to @a pbDst prefixed by RDPServerX509BlobHdr.
 * @returns @a pbDst on success, NULL on failure.
 */
static uint8_t *W_X509(uint8_t *pbDst, RTCRX509CERTIFICATE &rCert)
{
    uint32_t cbCert = lenX509(rCert);
    AssertReturn(cbCert > 0, NULL);
    *(uint32_t *)pbDst = cbCert;

    int rc = RTAsn1EncodeToBuffer(&rCert.SeqCore.Asn1Core, RTASN1ENCODE_F_DER,
                                  pbDst + sizeof(uint32_t /*RDPServerX509BlobHdr*/), cbCert, NULL);
    if (RT_SUCCESS(rc))
    {
        SECLOG(("SECTP: W_X509 cbCert = %u, p = %p.\n", cbCert, &rCert));
        return pbDst;
    }
    SECLOG(("SECTP: W_X509 ERROR cbCert = %d: %Rrc.\n", cbCert, rc));
    return NULL;
}

SECTP::SECTP ()
{
    InitMembers ();
    Reset ();
}

SECTP::~SECTP ()
{
    DestroyMembers ();
}

void SECTP::InitMembers (void)
{
    m_rsaKey       = NIL_RTCRKEY;
    int rc = RTBigNumInitZero(&m_rsaModulus, 0); AssertRC(rc);
    rc = RTBigNumInitZero(&m_rsaPrivateExponent, 0); AssertRC(rc);
    m_pub_exp      = 0x00010001;
    m_u32EncryptionLevel = ENCRYPTION_LEVEL_NONE;
    m_u32EncryptionMethod = ENCRYPTION_METHOD_NONE;
    memset (m_modulus, 0, sizeof (m_modulus));

    memset (m_sec_sign_key, 0, sizeof (m_sec_sign_key));

    m_rc4_key_len = 0;

    memset (m_sec_decrypt_key, 0, sizeof (m_sec_decrypt_key));
    memset (m_sec_encrypt_key, 0, sizeof (m_sec_encrypt_key));
    memset (m_sec_decrypt_update_key, 0, sizeof (m_sec_decrypt_update_key));
    memset (m_sec_encrypt_update_key, 0, sizeof (m_sec_encrypt_update_key));

    m_logon_received = 0;

    m_recvcount = 0;
    m_sendcount = 0;

    /** @todo Obsolete. make it configurable true. */
    m_fX509 = false;

    m_nameCertCA = "ca_cert.pem";
    m_nameCertServer = "server_cert.pem";
    m_namePrivServer = "server_priv.pem";

    RT_ZERO(m_certCA);
    RT_ZERO(m_certServer);
}

void SECTP::DestroyMembers (void)
{
    if (m_rsaKey != NIL_RTCRKEY)
    {
        RTCrKeyRelease(m_rsaKey);
        m_rsaKey = NIL_RTCRKEY;
    }
    RTBigNumDestroy(&m_rsaModulus);
    RTBigNumDestroy(&m_rsaPrivateExponent);
    RTCrX509Certificate_Delete(&m_certCA);
    RTCrX509Certificate_Delete(&m_certServer);
}


int SECTP::EnableEncryption (uint32_t fu32EncryptionMethods)
{
    /* With Standard RDP Security VRDP supports only 128 bit or nothing. */
    if (fu32EncryptionMethods & SEC_ENCRYPTION_FLAG_128BIT)
    {
        m_u32EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
        m_u32EncryptionMethod = ENCRYPTION_METHOD_128BIT;
        return VINF_VRDP_SUCCESS;
    }
    else if (fu32EncryptionMethods == 0)
    {
        m_u32EncryptionLevel = ENCRYPTION_LEVEL_NONE;
        m_u32EncryptionMethod = ENCRYPTION_METHOD_NONE;
        return VINF_VRDP_SUCCESS;
    }

    return VERR_VRDP_SEC_ENGINE_FAIL;
}

static void reverse(uint8_t *p, int len)
{
    if (len <= 1)
    {
        return;
    }

    uint8_t *s = p;
    uint8_t *e = p + len - 1;

    for (; s < e; s++, e--)
    {
        uint8_t tmp8 = *s;
        *s = *e;
        *e = tmp8;
    }
}

static int vrdp_BN_bn2bin(PCRTBIGNUM pSrcNum, uint8_t *pbDst, int cbDst)
{
    int rc = RTBigNumToBytesBigEndian(pSrcNum, pbDst, cbDst);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    SECLOG(("SECTP: the bignum length %u exceeds the buffer size %d.\n", RTBigNumByteWidth(pSrcNum), cbDst));
    return VERR_VRDP_SEC_ENGINE_FAIL;
}

int SECTP::generateRSA (void)
{
    int rc;
    if (m_fX509)
    {
        /*
         * Load everything from files.
         */
        rc = loadX509(m_nameCertCA, &m_certCA);
        if (RT_SUCCESS(rc))
        {
            rc = loadX509(m_nameCertServer, &m_certServer);
            if (RT_SUCCESS(rc))
            {
                m_rsaKey = loadPrivKey(m_namePrivServer);
                if (m_rsaKey)
                {
                    rc = RTCrKeyQueryRsaPrivateExponent(m_rsaKey, &m_rsaPrivateExponent);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTCrKeyQueryRsaModulus(m_rsaKey, &m_rsaModulus);
                        if (RT_SUCCESS(rc))
                        {
                            SECLOG(("SECTP: generateRSA successfully loaded certificates.\n"));
                            return VINF_VRDP_SUCCESS;
                        }
                    }
                }
            }
        }
    }
    else
    {
        /*
         * Generate modulus and public and private exponent for RSA
         * encryption used by client to send back it's random.
         *
         * Modulus size is SEC_MODULUS_SIZE (64) bytes (=> 512 bits).
         * Public exponent size is SEC_EXPONENT_SIZE (4) bytes.
         */
        RT_ZERO(m_modulus);
        rc = RTCrKeyCreateNewRsa(&m_rsaKey, SEC_MODULUS_SIZE * 8, m_pub_exp, 0);
        if (RT_SUCCESS(rc))
        {
            rc = RTCrKeyQueryRsaPrivateExponent(m_rsaKey, &m_rsaPrivateExponent);
            if (RT_SUCCESS(rc))
            {
                rc = RTCrKeyQueryRsaModulus(m_rsaKey, &m_rsaModulus);
                if (RT_SUCCESS(rc))
                {
                    rc = RTBigNumToBytesBigEndian(&m_rsaModulus, m_modulus, sizeof(m_modulus));
                    if (RT_SUCCESS(rc))
                    {
                        reverse(m_modulus, SEC_MODULUS_SIZE);
                        return VINF_SUCCESS;
                    }
                }
            }
        }
    }
    return VERR_VRDP_SEC_ENGINE_FAIL;
}


int SECTP::decryptClientRandom (const uint8_t *crypted_random)
{
    Assert(m_rsaKey != NIL_RTCRKEY);

    /* The decrypted client random will be placed in the buffer. */
    uint8_t random[SEC_RANDOM_SIZE];

    /* The encrypted random from the client. */
    uint8_t inv_crypted_random[SEC_MODULUS_SIZE];
    memcpy(inv_crypted_random, crypted_random, SEC_MODULUS_SIZE);
    reverse(inv_crypted_random, SEC_MODULUS_SIZE); /** @todo Test RTBIGNUMINIT_F_LITTLE_ENDIAN */

    hexdump(inv_crypted_random, SEC_MODULUS_SIZE);

    RTBIGNUM x;
    int rc = RTBigNumInit(&x, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_UNSIGNED, inv_crypted_random, SEC_MODULUS_SIZE);
    if (RT_SUCCESS(rc))
    {
        RTBIGNUM y;
        rc = RTBigNumInitZero(&y, 0);
        if (RT_SUCCESS(rc))
        {
            rc = RTBigNumModExp(&y, &x, &m_rsaPrivateExponent, &m_rsaModulus);
            if (RT_SUCCESS(rc))
            {
                rc = RTBigNumToBytesBigEndian(&y, random, sizeof(random));
                if (RT_SUCCESS(rc))
                {
                    if (m_fX509)
                        hexdump(random, sizeof(random));
                    reverse(random, sizeof(random));
                    memcpy(m_client_random, random, sizeof(random));
                    if (m_fX509)
                        hexdump(m_client_random, sizeof(random));
                }
            }
            RTBigNumDestroy(&y);
        }
        RTBigNumDestroy(&x);
    }
    return RT_SUCCESS(rc) ? VINF_VRDP_SUCCESS : VERR_VRDP_SEC_ENGINE_FAIL;
}

/*
 * 48-byte transformation used to generate master secret (6.1) and key material (6.2.2).
 * Both SHA1 and MD5 algorithms are used.
 */
static void
sec_hash_48(uint8_t * out, uint8_t * in, uint8_t * salt1, uint8_t * salt2, uint8_t salt)
{
    int i;

    for (i = 0; i < 3; i++)
    {
        RTSHA1CONTEXT Sha1Ctx;
        RTSha1Init(&Sha1Ctx);
        uint8_t const iPadding = salt + i;
        uint8_t abPad[4] = { iPadding, iPadding, iPadding, iPadding };
        RTSha1Update(&Sha1Ctx, abPad, i + 1);
        RTSha1Update(&Sha1Ctx, in, 48);
        RTSha1Update(&Sha1Ctx, salt1, 32);
        RTSha1Update(&Sha1Ctx, salt2, 32);
        uint8_t abShaHash[RTSHA1_HASH_SIZE];
        RTSha1Final(&Sha1Ctx, abShaHash);

        RTMD5CONTEXT Md5Ctx;
        RTMd5Init(&Md5Ctx);
        RTMd5Update(&Md5Ctx, in, 48);
        RTMd5Update(&Md5Ctx, abShaHash, sizeof(abShaHash));
        RTMd5Final(&out[i * 16], &Md5Ctx);
    }
}

/*
 * 16-byte transformation used to generate export keys (6.2.2).
 */
static void
sec_hash_16(uint8_t * out, uint8_t * in, uint8_t * salt1, uint8_t * salt2)
{
    RTMD5CONTEXT Md5Ctx;
    RTMd5Init(&Md5Ctx);
    RTMd5Update(&Md5Ctx, in, 16);
    RTMd5Update(&Md5Ctx, salt1, 32);
    RTMd5Update(&Md5Ctx, salt2, 32);
    RTMd5Final(out, &Md5Ctx);
}

/* Reduce key entropy from 64 to 40 bits */
static void
sec_make_40bit(uint8_t * key)
{
    key[0] = 0xd1;
    key[1] = 0x26;
    key[2] = 0x9e;
}



int SECTP::generateKeys(void)
{
    uint8_t pre_master_secret[48];
    uint8_t master_secret[48];
    uint8_t key_block[48];

    /* Construct pre-master secret */
    memcpy(pre_master_secret, m_client_random, 24);
    memcpy(pre_master_secret + 24, m_server_random, 24);

    /* Generate master secret and then key material */
    sec_hash_48(master_secret, pre_master_secret, m_client_random, m_server_random, 'A');
    sec_hash_48(key_block, master_secret, m_client_random, m_server_random, 'X');

    /* First 16 bytes of key material is MAC secret */
    memcpy(m_sec_sign_key, key_block, 16);

    /* Generate export keys from next two blocks of 16 bytes */
    sec_hash_16(m_sec_encrypt_key, &key_block[16], m_client_random, m_server_random);
    sec_hash_16(m_sec_decrypt_key, &key_block[32], m_client_random, m_server_random);

    if (m_u32EncryptionMethod == ENCRYPTION_METHOD_40BIT)
    {
        SECLOG(("40-bit encryption enabled\n"));
        sec_make_40bit(m_sec_sign_key);
        sec_make_40bit(m_sec_encrypt_key);
        sec_make_40bit(m_sec_decrypt_key);
        m_rc4_key_len = 8;
    }
    else if (m_u32EncryptionMethod == ENCRYPTION_METHOD_128BIT)
    {
        SECLOG(("128-bit encryption enabled\n"));
        m_rc4_key_len = 16;
    }
    else
    {
        return VERR_VRDP_SEC_ENGINE_FAIL;
    }

    /* Save initial RC4 keys as update keys */
    memcpy(m_sec_encrypt_update_key, m_sec_encrypt_key, 16);
    memcpy(m_sec_decrypt_update_key, m_sec_decrypt_key, 16);

    /* Initialise RC4 state arrays */
    RTCrRc4SetKey(&m_rc4_encrypt_key, m_rc4_key_len, m_sec_encrypt_key);
    RTCrRc4SetKey(&m_rc4_decrypt_key, m_rc4_key_len, m_sec_decrypt_key);

    return VINF_VRDP_SUCCESS;
}

static uint8_t pad_54[40] = {
    54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
    54, 54, 54,
    54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
    54, 54, 54
};

static uint8_t pad_92[48] = {
    92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
    92, 92, 92, 92, 92, 92, 92,
    92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
    92, 92, 92, 92, 92, 92, 92
};


void SECTP::updateKey (uint8_t * key, uint8_t * update_key)
{
    RTSHA1CONTEXT Sha1Ctx;
    RTSha1Init(&Sha1Ctx);
    RTSha1Update(&Sha1Ctx, update_key, m_rc4_key_len);
    RTSha1Update(&Sha1Ctx, pad_54, sizeof(pad_54));
    RTSha1Update(&Sha1Ctx, key, m_rc4_key_len);
    uint8_t abShaHash[RTSHA1_HASH_SIZE];
    RTSha1Final(&Sha1Ctx, abShaHash);

    RTMD5CONTEXT Md5Ctx;
    RTMd5Init(&Md5Ctx);
    RTMd5Update(&Md5Ctx, update_key, m_rc4_key_len);
    RTMd5Update(&Md5Ctx, pad_92, sizeof(pad_92));
    RTMd5Update(&Md5Ctx, abShaHash, sizeof(abShaHash));
    RTMd5Final(key, &Md5Ctx);

    RTCRRC4KEY update;
    RTCrRc4SetKey(&update, m_rc4_key_len, key);
    RTCrRc4(&update, m_rc4_key_len, key, key);

    if (m_rc4_key_len == 8)
        sec_make_40bit(key);
}


/* static const uint32_t sigPublicExp = 0xc0887b5b; */

/* Reversed, already suitable for BN_bin2bn. */
static const uint8_t sigModulus[SEC_MODULUS_SIZE] =
{
    0x87,0xea,0x6d,0x05,0x5f,0x09,0x93,0x20,0xbb,0x61,0xf5,0x1a,0x09,0x06,0x5e,0x6c,
    0x7d,0x5c,0xf6,0x3d,0xfe,0xbf,0xe7,0x7c,0xef,0xfe,0x3a,0x58,0x6b,0x65,0x63,0xce,
    0x95,0x45,0x52,0xf2,0x9a,0x6b,0xb7,0xd7,0xe2,0xc1,0xf5,0xef,0x87,0x20,0x88,0x3e,
    0xcb,0x5f,0xba,0x4a,0x1e,0xc1,0xbb,0x4d,0xc9,0x3e,0x43,0x72,0xbd,0x5e,0x3a,0x3d
};

static const uint8_t sigPrivateExp[SEC_MODULUS_SIZE] =
{
    0x5f,0xf3,0x3f,0xe7,0x13,0x01,0x10,0xc7,0xb3,0x9b,0x51,0x0b,0x17,0x79,0x07,0x35,
    0xdd,0x0d,0x7b,0x0a,0xb8,0x3a,0xc7,0x99,0x24,0xb1,0xdd,0x24,0x9f,0x12,0x9a,0x17,
    0x60,0x08,0xe8,0x99,0x33,0xcc,0x92,0xcf,0x94,0x49,0x67,0xe9,0xfa,0xe6,0x3e,0x24,
    0xf8,0x68,0x65,0x25,0x16,0x16,0x00,0x58,0x55,0x87,0x11,0xda,0x32,0x19,0xa7,0x87
};

int SECTP::signKey (uint8_t *sig, uint8_t *data, uint32_t datalen)
{
    /* Make RDP4 signature for our public key. */
    uint8_t hash[16];
    /* 63 bytes array:
     *     first 16 bytes: the MD5 hash;
     *     [16] = 0x00;
     *     [17...61] = 0xFF;
     *     [62] = 0x01;
     */
    uint8_t hashArray[SEC_MODULUS_SIZE - 1];


    /* MD5 hash for public key. */
    RTMd5(data, datalen, hash);

    /* Prepare the hash array. */
    memset(hashArray, 0xFF, sizeof(hashArray));
    memcpy(&hashArray[0], hash, sizeof(hash));
    hashArray[16] = 0x00;
    hashArray[62] = 0x01;
    reverse(hashArray, sizeof(hashArray)); /**< RTBigNumInit can handle little endian numbers, not sure if it has been tested yet though... */

    /* Encrypt MD5 hash with the private key. */
    RTBIGNUM HashNum;
    int rc = RTBigNumInit(&HashNum, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_UNSIGNED, hashArray, sizeof(hashArray));
    if (RT_SUCCESS(rc))
    {
        /** @todo the next two are constants and should be kept in 'this'. */
        RTBIGNUM PrivExp;
        rc = RTBigNumInit(&PrivExp, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_UNSIGNED, sigPrivateExp, SEC_MODULUS_SIZE);
        if (RT_SUCCESS(rc))
        {
            RTBIGNUM Mod;
            rc = RTBigNumInit(&Mod, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_UNSIGNED, sigModulus, SEC_MODULUS_SIZE);
            if (RT_SUCCESS(rc))
            {
                RTBIGNUM Result;
                rc = RTBigNumInitZero(&Result, 0);
                if (RT_SUCCESS(rc))
                {
                    rc = RTBigNumModExp(&Result, &HashNum, &PrivExp, &Mod);
                    if (RT_SUCCESS(rc))
                    {
                        rc = vrdp_BN_bn2bin(&Result, sig, SEC_MODULUS_SIZE);
                        if (RT_SUCCESS(rc))
                            reverse(sig, SEC_MODULUS_SIZE);
                    }
                    RTBigNumDestroy(&Result);
                }
                RTBigNumDestroy(&Mod);
            }
            RTBigNumDestroy(&PrivExp);
        }
        RTBigNumDestroy(&HashNum);
    }
    return RT_SUCCESS(rc) ? VINF_VRDP_SUCCESS : VERR_VRDP_SEC_ENGINE_FAIL;
}

#pragma pack(1)
/* TS_UD_HEADER */
typedef struct RDPSecTagHdr
{
    uint16_t u16Tag;
    uint16_t u16Size;
} RDPSecTagHdr;

/* TS_UD_SC_CORE */
typedef struct RDPServerCoreData
{
    uint32_t u32Version; /* Server version number for the RDP: 0x00080001 == 4.0, 0x00080004 == 5.0 to 7.0. */
} RDPServerCoreData;

typedef struct RDPServerCoreDataEx
{
    RDPServerCoreData data;
    /* Optional client flags in the requestedProtocols field of the RDP Negotiation Request. */
    uint32_t u32ClientRequestedProtocols;
} RDPServerCoreDataEx;

/* TS_UD_SC_NET */
typedef struct RDPServerNetworkData
{
    uint16_t u16MCSChannelId; /* I/O channel. */
    uint16_t u16ChannelCount;
    /* channelIdArray follows. */
} RDPServerNetworkData;

/* TS_UD_SC_SEC1 */
typedef struct RDPServerSecurityData
{
    uint32_t u32EncryptionMethod;
    uint32_t u32EncryptionLevel;
    uint32_t u32ServerRandomLen;
    uint32_t u32ServerCertLen;
    /* serverRandom follows. */
    /* serverCertificate follows. */
} RDPServerSecurityData;

typedef struct RDPServerCertificate
{
    uint32_t u32Version;
    /* certData follows */
} RDPServerCertificate;

/* RSA_PUBLIC_KEY */
typedef struct RDPRSAPublicKey
{
    uint32_t u32Magic;
    uint32_t u32KeyLen;   /* Size of modulus and padding in bytes. */
    uint32_t u32BitLen;   /* The number of bits in the public key modulus. */
    uint32_t u32DataLen;  /* The maximum number of bytes that can be encoded using the public key. */
    uint32_t u32PubExp;
    uint8_t  au8Modulus[SEC_MODULUS_SIZE];
    uint8_t  au8Padding[SEC_PADDING_SIZE];
} RDPRSAPublicKey;

typedef struct RDPProprietaryCertificateSignature
{
    uint8_t au8Signature[SEC_MODULUS_SIZE];
    uint8_t au8Padding[SEC_PADDING_SIZE];
} RDPProprietaryCertificateSignature;

/* PROPRIETARYSERVERCERTIFICATE */
typedef struct RDPServerProprietaryCertificate
{
    uint32_t u32Version;
    uint32_t u32SigAlgId;
    uint32_t u32KeyAlgId;
    uint16_t u16PublicKeyBlobType;
    uint16_t u16PublicKeyBlobLen;
    RDPRSAPublicKey PublicKeyBlob; /* This is a variable length field, but the server uses a fixed structure. */
    uint16_t u16SignatureBlobType;
    uint16_t u16SignatureBlobLen;
    RDPProprietaryCertificateSignature SignatureBlob;  /* Server uses a fixed structure. */
} RDPServerProprietaryCertificate;

/* CERT_BLOB */
typedef struct RDPServerX509BlobHdr
{
    uint32_t u32CertLength;
    /* abCert follows */
} RDPServerX509BlobHdr;

/* SERVER_CERTIFICATE + certificate chain. */
typedef struct RDPServerX509Certificate
{
    uint32_t u32Version;
    uint32_t u32NumCertBlobs; /* min = 2, max == 200. */
    /* CertBlobArray certificate chain follows: each blob = u32Length + au8Data[] */
    /* Padding follows: A byte array of the length 8 + 4 * NumCertBlobs */
} RDPServerX509Certificate;
#pragma pack()


int SECTP::outCryptInfo (VRDPInputCtx *pInputCtx)
{
    if (m_u32EncryptionLevel == ENCRYPTION_LEVEL_NONE)
    {
        /* Encryption is disabled. */
        uint16_t cbPacket = sizeof (RDPSecTagHdr) + sizeof (RDPServerSecurityData);

        RDPSecTagHdr *pHdr = (RDPSecTagHdr *)pInputCtx->AppendProtocolHeader(cbPacket);
        Assert (pHdr);

        RDPServerSecurityData *pServerSecurityData = (RDPServerSecurityData *)((uint8_t *)pHdr + sizeof (RDPSecTagHdr));

        pHdr->u16Tag = SEC_TAG_SRV_CRYPT;
        pHdr->u16Size = cbPacket;
        pServerSecurityData->u32EncryptionMethod = m_u32EncryptionMethod;
        pServerSecurityData->u32EncryptionLevel = m_u32EncryptionLevel;
        pServerSecurityData->u32ServerRandomLen = 0;
        pServerSecurityData->u32ServerCertLen = 0;

        return VINF_SUCCESS;
    }

    /* Generate server random. @todo move to EnableEncryption? it is a public info anyway. */
    int rc = RTCrRandBytes(m_server_random, sizeof(m_server_random));

    if (RT_FAILURE(rc))
    {
        VRDPLOGRELLIMIT(16, ("VRDP: failed to generate the server random (%Rrc).\n", rc));
        return VERR_VRDP_SEC_ENGINE_FAIL;
    }

    uint8_t *pu8ServerCertBlob = NULL; /* The server certificate data. */
    uint32_t cbServerCertBlob = 0;

    if (m_fX509)
    {
        /* Use an X.509 Server Certificate. This works only with ENCRYPTION_LEVEL_HIGH or _FIPS. */
        Assert(   m_u32EncryptionLevel == ENCRYPTION_LEVEL_HIGH
               || m_u32EncryptionLevel == ENCRYPTION_LEVEL_FIPS);

        VRDPLOGREL(("Using X.509 server certificate.\n"));

        /*
         * Write an X.509 Certificate Chain, which consists of a collection of certificates
         * concatenated together in root-certificate-first order.
         */

        /** @todo Obtain the server certificate and root certificates from somewhere on the host.
         * Use a platform specific module as the certificate provider, which uses the host infrastructure?
         */

        Assert(RTCrX509Certificate_IsPresent(&m_certCA) && RTCrX509Certificate_IsPresent(&m_certServer));

        uint32_t u32NumCerts = 2;

        cbServerCertBlob = sizeof (RDPServerX509Certificate);
        cbServerCertBlob += sizeof (RDPServerX509BlobHdr) + lenX509 (m_certCA);
        cbServerCertBlob += sizeof (RDPServerX509BlobHdr) + lenX509 (m_certServer);
        cbServerCertBlob += 8 + 4 * u32NumCerts;

        pu8ServerCertBlob = (uint8_t *)VRDPMemAllocZ(cbServerCertBlob);

        if (pu8ServerCertBlob == NULL)
        {
            return VERR_NO_MEMORY;
        }

        RDPServerX509Certificate *pCert = (RDPServerX509Certificate *)pu8ServerCertBlob;

        pCert->u32Version      = SEC_CERT_CHAIN_VERSION_2 | SEC_CERT_TEMPORARY;
        pCert->u32NumCertBlobs = u32NumCerts;

        uint8_t *pu8Write = (uint8_t *)pCert + sizeof (RDPServerX509Certificate);

        /* Write CA sertificate. */
        pu8Write = W_X509 (pu8Write, m_certCA);

        if (pu8Write == NULL)
        {
            VRDPMemFree(pu8ServerCertBlob);
            return VERR_VRDP_SEC_ENGINE_FAIL;
        }

        /* Write the server sertificate. */
        pu8Write = W_X509 (pu8Write, m_certServer);

        if (pu8Write == NULL)
        {
            VRDPMemFree(pu8ServerCertBlob);
            return VERR_VRDP_SEC_ENGINE_FAIL;
        }

        /* Padding. */
        memset (pu8Write, 0, 8 + 4 * u32NumCerts);

        Assert(pu8Write + 8 + 4 * u32NumCerts == pu8ServerCertBlob + cbServerCertBlob);
    }
    else
    {
        /* Server Proprietary Certificate */
        SECLOG(("SECTP: outCryptInfo using Server Proprietary Certificate.\n"));

        cbServerCertBlob = sizeof (RDPServerProprietaryCertificate);

        pu8ServerCertBlob = (uint8_t *)VRDPMemAllocZ(cbServerCertBlob);

        if (pu8ServerCertBlob == NULL)
        {
            return VERR_NO_MEMORY;
        }

        RDPServerProprietaryCertificate *pCert = (RDPServerProprietaryCertificate *)pu8ServerCertBlob;

        pCert->u32Version           = SEC_CERT_CHAIN_VERSION_1;
        pCert->u32SigAlgId          = SEC_SIGNATURE_ALG_RSA;
        pCert->u32KeyAlgId          = SEC_KEY_EXCHANGE_ALG_RSA;

        pCert->u16PublicKeyBlobType = SEC_BB_RSA_KEY_BLOB;
        pCert->u16PublicKeyBlobLen  = sizeof (RDPRSAPublicKey);

        pCert->PublicKeyBlob.u32Magic   = SEC_RSA1;
        pCert->PublicKeyBlob.u32KeyLen  = SEC_MODULUS_SIZE + SEC_PADDING_SIZE;
        pCert->PublicKeyBlob.u32BitLen  = 512;
        pCert->PublicKeyBlob.u32DataLen = SEC_MODULUS_SIZE - 1;
        pCert->PublicKeyBlob.u32PubExp  = m_pub_exp;
        memcpy (pCert->PublicKeyBlob.au8Modulus, m_modulus, SEC_MODULUS_SIZE);
        memset (pCert->PublicKeyBlob.au8Padding, 0, SEC_PADDING_SIZE);

        pCert->u16SignatureBlobType = SEC_BB_RSA_SIGNATURE_BLOB;
        pCert->u16SignatureBlobLen  = sizeof (RDPProprietaryCertificateSignature);

        /* Sign everything before the u16SignatureBlobType. */
        rc = signKey (pCert->SignatureBlob.au8Signature,
                      pu8ServerCertBlob,
                      RT_UOFFSETOF(RDPServerProprietaryCertificate, u16SignatureBlobType));

        if (RT_FAILURE (rc))
        {
            VRDPMemFree(pu8ServerCertBlob);
            return rc;
        }

        memset (pCert->SignatureBlob.au8Padding, 0, SEC_PADDING_SIZE);
    }

    Assert(pu8ServerCertBlob);
    Assert(cbServerCertBlob);

    /* Write:
     * [RDPServerSecurityData]
     * [serverRandom]
     * [serverCertificate]
     */
    uint32_t cbPacket = sizeof (RDPSecTagHdr) + sizeof (RDPServerSecurityData) + SEC_RANDOM_SIZE + cbServerCertBlob;

    RDPSecTagHdr *pHdr = (RDPSecTagHdr *)pInputCtx->AppendProtocolHeader(cbPacket);
    Assert (pHdr);

    RDPServerSecurityData *pServerSecurityData = (RDPServerSecurityData *)((uint8_t *)pHdr + sizeof (RDPSecTagHdr));

    uint8_t *pu8ServerRandom = (uint8_t *)pServerSecurityData + sizeof (RDPServerSecurityData);
    uint8_t *pu8ServerCertificate = pu8ServerRandom + SEC_RANDOM_SIZE;

    pHdr->u16Tag = SEC_TAG_SRV_CRYPT;
    pHdr->u16Size = (uint16_t)cbPacket;

    pServerSecurityData->u32EncryptionMethod = m_u32EncryptionMethod;
    pServerSecurityData->u32EncryptionLevel = m_u32EncryptionLevel;
    pServerSecurityData->u32ServerRandomLen = SEC_RANDOM_SIZE;
    pServerSecurityData->u32ServerCertLen = cbServerCertBlob;

    memcpy (pu8ServerRandom, m_server_random, sizeof (m_server_random));

    memcpy (pu8ServerCertificate, pu8ServerCertBlob, cbServerCertBlob);

    VRDPMemFree(pu8ServerCertBlob);

    return VINF_VRDP_SUCCESS;
}

int SECTP::SendConnectResponse (VRDPInputCtx *pInputCtx, uint8_t u8Result, uint8_t u8Channels)
{
    /** @todo SEC_TAG_ construction should be probably moved to VRDPTP. */

    /* Constructing the packet from tail to begin.

     * The level of server response consists of:
     *  * T.124 ConferenceCreateResponse
     *  * SEC_TAG_SRV_INFO
     *  * SEC_TAG_SRV_CHANNELS
     *  * SEC_TAG_SRV_CRYPT
     */

    unsigned i;

    /*
     * Start with SEC_TAG_SRV_CRYPT
     */
    int rc = outCryptInfo (pInputCtx);

    if (RT_FAILURE (rc))
    {
        return rc;
    }

    /*
     * SEC_TAG_SRV_CHANNELS
     * 03 0C, 10 00, EB 03 03    00 EC 03 ED 03 EE 03 00 00
     */
    uint16_t cbPacket = sizeof (RDPSecTagHdr) + sizeof (RDPServerNetworkData) + sizeof (uint16_t) * u8Channels;

    /* Align to 4 bytes. Since sizeof (RDPSecTagHdr) == 4 and sizeof (RDPServerNetworkData) == 4 then
     * alignment is needed only if u8Channels is an odd number.
     *
     * Workaround for FlashRDP client: the client does not take into account the variable padding,
     * and expects the padding for the zero channel packet. Since FlashRDP is probably the only
     * client, which has no channels, the workaround was implemented:
     * if (channels == 0) { add padding }
     */
    if (   (u8Channels & 1) != 0
        || u8Channels == 0) /* FlashRDP workaround */
    {
        cbPacket += 2;
    }
    Assert((cbPacket % 4 == 0) || (u8Channels == 0));

    RDPSecTagHdr *pHdr = (RDPSecTagHdr *)pInputCtx->AppendProtocolHeader(cbPacket);
    Assert (pHdr);

    RDPServerNetworkData *pServerNetworkData = (RDPServerNetworkData *)((uint8_t *)pHdr + sizeof (RDPSecTagHdr));

    uint16_t *paChannelIdArray = (uint16_t *)((uint8_t *)pServerNetworkData + sizeof (RDPServerNetworkData));

    pHdr->u16Tag = SEC_TAG_SRV_CHANNELS;
    pHdr->u16Size= cbPacket;
    pServerNetworkData->u16MCSChannelId = MCS_GLOBAL_CHANNEL;
    pServerNetworkData->u16ChannelCount = u8Channels;

    /** @todo A channel value of 0 indicates that the channel was not allocated. */
    for (i = 0; i < u8Channels; i++)
    {
        paChannelIdArray[i] = MCS_GLOBAL_CHANNEL + i + 1;
    }

    /* Optional pad. */
    if (   (u8Channels & 1) != 0
        || u8Channels == 0) /* FlashRDP workaround */
    {
        paChannelIdArray[u8Channels] = 0;
    }

    /*
     * SEC_TAG_SRV_INFO
     */
    VRDPData *pData = pInputCtx->Data();
    if (pData->RDPNegotiationRequest())
    {
        cbPacket = sizeof (RDPSecTagHdr) + sizeof (RDPServerCoreDataEx);
    }
    else
    {
        cbPacket = sizeof (RDPSecTagHdr) + sizeof (RDPServerCoreData);
    }

    pHdr = (RDPSecTagHdr *)pInputCtx->AppendProtocolHeader(cbPacket);
    Assert (pHdr);

    RDPServerCoreData *pServerCoreData = (RDPServerCoreData *)((uint8_t *)pHdr + sizeof (RDPSecTagHdr));

    pHdr->u16Tag = SEC_TAG_SRV_INFO;
    pHdr->u16Size= cbPacket;
    pServerCoreData->u32Version = 0x00080004;
    if (pData->RDPNegotiationRequest())
    {
        RDPServerCoreDataEx *pServerCoreDataEx = (RDPServerCoreDataEx *)pServerCoreData;
        pServerCoreDataEx->u32ClientRequestedProtocols = pData->RDPNegotiationRequestedProtocols();
    }

    uint32_t u32CryptInfoLength = pInputCtx->SizeOfWrite ();

    /*
     * T.124 ConferenceCreateResponse.
     */
    uint8_t *pu8Write = pInputCtx->AppendProtocolHeader (23); /* Seems to be fixed size. */

    Assert (pu8Write);

    *pu8Write++ = 0;    // unknown
    *pu8Write++ = 5;    // unknown
    *pu8Write++ = 0;    // unknown
    *pu8Write++ = 0x14; // byte length of the req without 3 leading bytes?

    *pu8Write++ = 0x7c; // unknown
    *pu8Write++ = 0;    // unknown
    *pu8Write++ = 1;    // unknown

    *pu8Write++ = 0x2a; // the high bit is checked, and if set, the following byte is skipped. Some length?

    *pu8Write++ = 0x14; // unknown
    *pu8Write++ = 0x76; // unknown
    *pu8Write++ = 0x0a; // unknown
    *pu8Write++ = 0x01; // unknown
    *pu8Write++ = 0x01; // unknown
    *pu8Write++ = 0x00; // unknown
    *pu8Write++ = 0x01; // unknown
    *pu8Write++ = 0xc0; // unknown
    *pu8Write++ = 0x00; // unknown

    *(uint32_t *)pu8Write = 0x6e44634d; /* 'McDn' */
    pu8Write += 4;

    /* Length of remaining data. <= 7f or  0x8000 | (len & 0x3fff)  */
    u32CryptInfoLength |= 0x8000;

    *pu8Write++ = (uint8_t)RT_HIBYTE(u32CryptInfoLength);
    *pu8Write++ = (uint8_t)RT_LOBYTE(u32CryptInfoLength);

    pInputCtx->DumpWrite ();

    return m_mcstp.SendConnectResponse (pInputCtx, u8Result, u8Channels);
}

void SECTP::Disconnect (VRDPInputCtx *pInputCtx)
{
    if (pInputCtx)
    {
        m_mcstp.Disconnect (pInputCtx);
    }
}

int SECTP::Decrypt (VRDPInputCtx *pInputCtx)
{
    if (m_u32EncryptionLevel == ENCRYPTION_LEVEL_NONE)
    {
        /* Encryption is disabled, do nothing. */
        return VINF_SUCCESS;
    }

    /* Read 8 bytes signature. */
    const uint8_t *pu8 = pInputCtx->Read (8);

    if (pu8 == NULL)
    {
        SECLOG(("Failed to read signature!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    if (m_recvcount == 4096)
    {
        SECLOG(("update key recv\n\n"));
        updateKey (m_sec_decrypt_key, m_sec_decrypt_update_key);
        RTCrRc4SetKey(&m_rc4_decrypt_key, m_rc4_key_len, m_sec_decrypt_key);
        m_recvcount = 0;
    }

    /* How many bytes to decrypt. */
    uint16_t cb = pInputCtx->BytesToRead ();

    if (cb == 0)
    {
        SECLOG(("Encrypted data size is 0!!!\n"));
        return VERR_VRDP_PROTOCOL_ERROR;
    }

    /* Advance the pointer to data, and decrypt the buffer. */
    pu8 += 8;

    TESTLOG(("cnt %d, l %d\n", m_recvcount, cb));

    RTCrRc4(&m_rc4_decrypt_key, cb, pu8, (uint8_t *)pu8);
    m_recvcount++;

    return VINF_SUCCESS;
}

#pragma pack(1)
typedef struct _SECLicenseResult
{
    uint32_t u32Flags;    /// @todo flags
    uint8_t u8Tag;        // 0xFF
    uint8_t u8Version;    // version
    uint16_t u16Length;   // length
    uint32_t u32Unknown1; /// @todo unknown
    uint32_t u32Unknown2; /// @todo unknown
    uint32_t u32Unknown3; /// @todo unknown
} SECLicenseResult;
#pragma pack(1)

int SECTP::SendLicense (VRDPInputCtx *pInputCtx)
{
    // This tag is only that MS server sends.
    // however rdesktop will not work if only this
    // was send and encryption is not enabled.
    // This is either a bug in rdesktop or
    // license must be ussied if no encryption is used (seems unlikely).

    SECLicenseResult *pLicenseResult = (SECLicenseResult *)pInputCtx->AppendProtocolHeader (sizeof (SECLicenseResult));
    Assert (pLicenseResult);

    pLicenseResult->u32Flags    = 0x00100280;
    pLicenseResult->u8Tag       = LICENCE_TAG_RESULT;
    pLicenseResult->u8Version   = 3;
    pLicenseResult->u16Length   = 16;
    pLicenseResult->u32Unknown1 = 7;
    pLicenseResult->u32Unknown2 = 2;
    pLicenseResult->u32Unknown3 = 4;

    return m_mcstp.Send (3, pInputCtx);
}

int SECTP::Recv (VRDPInputCtx *pInputCtx)
{
    int rc = m_mcstp.Recv (pInputCtx);

    if (rc != VINF_VRDP_PROCESS_PDU)
    {
        return rc;
    }

    const uint8_t *pu8;

    switch (m_enmStatus)
    {
        case VRDP_SEC_Status_RecvConnectInitial:
        {
            SECLOG(("VRDP_SEC_Status_RecvConnectInitial\n"));

            rc = generateRSA ();

            if (RT_FAILURE (rc))
            {
                return rc;
            }

            /* No data at this layer. */
            m_enmStatus = VRDP_SEC_Status_SEC;

            return VINF_VRDP_PROCESS_PDU;
        }

        case VRDP_SEC_Status_SEC:
        {
            if (!m_fCryptLevelActive)
            {
                /* Encryption is not enabled. */
                return VINF_VRDP_PROCESS_PDU;
            }

            uint8_t u8Version = pInputCtx->PacketVersion ();

            switch (u8Version)
            {
                case 3:
                {
                    /* Read the SEC_FLAGS. */
                    const uint32_t *pu32SecFlags = (const uint32_t *)pInputCtx->Read (4);

                    if (pu32SecFlags == NULL)
                    {
                        SECLOG(("Failed to read SEC_FLAGS!!!\n"));
                        return VERR_VRDP_PROTOCOL_ERROR;
                    }

                    SECLOG(("SEC_FLAGS: 0x%08X\n", *pu32SecFlags));

                    if (*pu32SecFlags & SEC_CLIENT_RANDOM)
                    {
                        /* Receive client random. */
                        SECLOG(("SEC_CLIENT_RANDOM\n"));

                        /* Read the length. */
                        const uint32_t *pu32Length = (const uint32_t *)pInputCtx->Read (4);

                        if (pu32Length == NULL)
                        {
                            SECLOG(("Failed to read length!!!\n"));
                            return VERR_VRDP_PROTOCOL_ERROR;
                        }

                        SECLOG(("Length: %d, bytes left %d\n", *pu32Length, pInputCtx->BytesToRead ()));

                        if (   *pu32Length != SEC_MODULUS_SIZE + SEC_PADDING_SIZE
                            || *pu32Length != pInputCtx->BytesToRead ())
                        {
                            SECLOG(("Not supported client random length!!!\n"));
                            return VERR_VRDP_SEC_ENGINE_FAIL;
                        }

                        /* Read the client random. */
                        pu8 = pInputCtx->Read (*pu32Length);

                        if (pu8 == NULL)
                        {
                            SECLOG(("Failed to read client random!!!\n"));
                            AssertFailed ();
                            return VERR_VRDP_PROTOCOL_ERROR;
                        }

                        rc = decryptClientRandom (pu8);

                        SECLOG(("decryptClientRandom rc = %d\n", rc));

                        if (RT_FAILURE(rc))
                        {
                            return rc;
                        }

                        rc = generateKeys ();

                        SECLOG(("generateKeys rc = %d\n", rc));

                        if (RT_FAILURE (rc))
                        {
                            return rc;
                        }

                        /* No need to further process the pdu. */
                        return VINF_SUCCESS;
                    }

                    if (*pu32SecFlags & SEC_ENCRYPT)
                    {
                        rc = Decrypt (pInputCtx);

                        if (RT_FAILURE (rc))
                        {
                            return rc;
                        }
                    }

                    if (*pu32SecFlags & SEC_LOGON_INFO)
                    {
                        SECLOG(("SEC_LOGON_INFO\n"));

                        if (m_u32EncryptionLevel == ENCRYPTION_LEVEL_NONE)
                        {
                            /** @todo */
                            /* Encryption is not enabled and the logon info
                             * that has the SEC_FLAGS field has just been
                             * received. There will be no more SEC_FLAGS in
                             * packets, if the client is MS RDP.
                             *
                             * rdesktop <= 1.5 seems to have a bug and continues to send
                             * the secure flags until it receives LICENSE_ISSUE packet.
                             * Since VRDP does not send the packet, continue to process
                             * and send the secure flags by setting m_fCryptLevelActive true
                             * in SetRdesktop method.
                             */
                            m_fCryptLevelActive = false;
                        }
                    }

                    return VINF_VRDP_PROCESS_PDU;
                }

                case VRDP_CTX_VER_NULL:
                {
                    AssertFailed ();
                    return VERR_VRDP_PROTOCOL_ERROR;
                }

                default:
                {
                    /* Possibly RDP5. */
                    if (u8Version & 0x80)
                    {
                        /* Compressed packet. */
                        rc = Decrypt (pInputCtx);

                        if (RT_FAILURE (rc))
                        {
                            return rc;
                        }
                    }

                    return VINF_VRDP_PROCESS_PDU;
                }
            }
        }

        default:
        {
            AssertFailed ();
        } break;
    }

    return VERR_VRDP_PROTOCOL_ERROR;

}

static void signPacket (void *pvSignature, unsigned cbSignature,
                        uint8_t *pu8Key, unsigned cbKey,
                        uint32_t cbData, const void *pvData)
{
    /* Generate a MAC hash (5.2.3.1), using a combination of SHA1 and MD5 */
    RTSHA1CONTEXT Sha1Ctx;
    RTSha1Init(&Sha1Ctx);
    RTSha1Update(&Sha1Ctx, pu8Key,  cbKey);
    RTSha1Update(&Sha1Ctx, pad_54,  sizeof(pad_54));
    RTSha1Update(&Sha1Ctx, &cbData, sizeof(uint32_t));
    RTSha1Update(&Sha1Ctx, pvData,  cbData);
    uint8_t abSha1Hash[RTSHA1_HASH_SIZE];
    RTSha1Final(&Sha1Ctx, abSha1Hash);

    RTMD5CONTEXT Md5Ctx;
    RTMd5Init(&Md5Ctx);
    RTMd5Update(&Md5Ctx, pu8Key, cbKey);
    RTMd5Update(&Md5Ctx, pad_92, sizeof(pad_92));
    RTMd5Update(&Md5Ctx, abSha1Hash, sizeof(abSha1Hash));
    uint8_t abMd5Hash[RTMD5_HASH_SIZE];
    RTMd5Final(abMd5Hash, &Md5Ctx);

    Assert(cbSignature <= sizeof(abMd5Hash));
    memcpy(pvSignature, abMd5Hash, cbSignature);
}

int SECTP::Send (uint8_t u8Version, VRDPCtx *pCtx, uint32_t u32SubstituteFlags)
{
    if (m_enmStatus != VRDP_SEC_Status_SEC)
    {
        /* The protocol is not initialized. */
        return VERR_NOT_SUPPORTED;
    }

    if (m_u32EncryptionLevel == ENCRYPTION_LEVEL_NONE)
    {
        if (m_fCryptLevelActive)
        {
            if (u8Version == 3)
            {
                /* Workaround for rdesktop and VRDP without license stuff. */
                uint8_t *pu8Write = pCtx->AppendProtocolHeader (4);
                Assert (pu8Write);

                *(uint32_t *)pu8Write = u32SubstituteFlags;
            }
        }
        else
        {
            /* Encryption is disabled, do nothing. */
        }
    }
    else
    {
        /* Encryption is enabled. Output 32 bit flags and 8 bytes signature. */
        uint16_t u16Length = pCtx->SizeOfWrite ();

        if (u8Version == 3)
        {
            uint8_t *pu8Write = pCtx->AppendProtocolHeader (12);
            Assert (pu8Write);

            *(uint32_t *)pu8Write = u32SubstituteFlags ? u32SubstituteFlags : SEC_ENCRYPT;

            signPacket (pu8Write + 4, 8,
                        m_sec_sign_key, m_rc4_key_len,
                        u16Length, pu8Write + 12);

            if (m_sendcount == 4096)
            {
                SECLOG(("update key send v3\n\n"));
                updateKey (m_sec_encrypt_key, m_sec_encrypt_update_key);
                RTCrRc4SetKey(&m_rc4_encrypt_key, m_rc4_key_len, m_sec_encrypt_key);
                m_sendcount = 0;
            }

            m_sendcount++;

            RTCrRc4(&m_rc4_encrypt_key, u16Length, pu8Write + 12, pu8Write + 12);
        }
        else
        {
            /* Mark the packet as encrypted */
            u8Version |= 0x80;

            uint8_t *pu8Write = pCtx->AppendProtocolHeader (8);
            Assert (pu8Write);

            signPacket (pu8Write, 8,
                        m_sec_sign_key, m_rc4_key_len,
                        u16Length, pu8Write + 8);

            if (m_sendcount == 4096)
            {
                SECLOG(("update key send rdp5\n\n"));
                updateKey (m_sec_encrypt_key, m_sec_encrypt_update_key);
                RTCrRc4SetKey(&m_rc4_encrypt_key, m_rc4_key_len, m_sec_encrypt_key);
                m_sendcount = 0;
            }

            m_sendcount++;

            RTCrRc4(&m_rc4_encrypt_key, u16Length, pu8Write + 8, pu8Write + 8);
        }
    }

    return m_mcstp.Send (u8Version, pCtx);
}

void SECTP::Reset (void)
{
    DestroyMembers ();
    InitMembers ();

    m_fCryptLevelActive = true;

    m_mcstp.Reset ();

    m_enmStatus = VRDP_SEC_Status_RecvConnectInitial;
}

