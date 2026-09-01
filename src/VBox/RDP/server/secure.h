/* $Id: secure.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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

#ifndef VRDP_INCLUDED_SRC_secure_h
#define VRDP_INCLUDED_SRC_secure_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/bignum.h>
#include <iprt/crypto/x509.h>
#include <iprt/crypto/rc4.h>

#include "mcs.h"

#define SEC_TAG_CLI_INFO        0xc001
#define SEC_TAG_CLI_CRYPT       0xc002
#define SEC_TAG_CLI_CHANNELS    0xc003
#define SEC_TAG_CLI_CLUSTER     0xc004
#define SEC_TAG_CLI_MONITOR     0xc005 /* TS_UD_CS_MONITOR */

#define SEC_ENCRYPTION_FLAG_40BIT  0x00000001
#define SEC_ENCRYPTION_FLAG_128BIT 0x00000002
#define SEC_ENCRYPTION_FLAG_56BIT  0x00000008
#define SEC_ENCRYPTION_FLAG_FIPS   0x00000010

/* encryptionMethod field of Server Security Data (TS_UD_SC_SEC1) */
#define ENCRYPTION_METHOD_NONE   0x00000000
#define ENCRYPTION_METHOD_40BIT  0x00000001
#define ENCRYPTION_METHOD_128BIT 0x00000002
#define ENCRYPTION_METHOD_56BIT  0x00000008
#define ENCRYPTION_METHOD_FIPS   0x00000010

/* encryptionLevel field of Server Security Data (TS_UD_SC_SEC1) */
#define ENCRYPTION_LEVEL_NONE              0x00000000
#define ENCRYPTION_LEVEL_LOW               0x00000001
#define ENCRYPTION_LEVEL_CLIENT_COMPATIBLE 0x00000002
#define ENCRYPTION_LEVEL_HIGH              0x00000003
#define ENCRYPTION_LEVEL_FIPS              0x00000004


#define SEC_RANDOM_SIZE         32 /* MUST be 32 bytes. */
#define SEC_MODULUS_SIZE        64
#define SEC_PADDING_SIZE        8
#define SEC_EXPONENT_SIZE       4

class SECTP
{
    private:
        MCSTP m_mcstp;

        enum VRDP_SEC_Status {
           VRDP_SEC_Status_RecvConnectInitial,
           VRDP_SEC_Status_SEC
        };

        enum VRDP_SEC_Status m_enmStatus;

        bool m_fCryptLevelActive;

        // RSA variables used to establish connection to client.
        RTCRKEY m_rsaKey;
        RTBIGNUM m_rsaModulus;
        RTBIGNUM m_rsaPrivateExponent;

        // RSA public exponent. Used to encrypt client_random by client.
        uint32_t m_pub_exp;
        // RSA modulus
        uint8_t m_modulus[SEC_MODULUS_SIZE];

        /* Encryption level and method selected by the server.
         * MS server uses 2 and 2.
         */
        uint32_t m_u32EncryptionLevel; /* ENCRYPTION_LEVEL_* */
        uint32_t m_u32EncryptionMethod; /* ENCRYPTION_METHOD_* */

        RTCRRC4KEY m_rc4_decrypt_key;
        RTCRRC4KEY m_rc4_encrypt_key;

        uint8_t m_sec_sign_key[64];
        unsigned m_rc4_key_len;
        uint8_t m_sec_decrypt_key[16];
        uint8_t m_sec_encrypt_key[16];
        uint8_t m_sec_decrypt_update_key[16];
        uint8_t m_sec_encrypt_update_key[16];

        unsigned m_logon_received: 1; // indicates if logon information has been received

        /* How many packets were encrypted. After every 4096 packet RC4 keys are updated. */
        unsigned m_recvcount;
        unsigned m_sendcount;

        /* Whether X509 certificates are used. RDP5 encryption. */
        bool m_fX509;

        const char *m_nameCertCA;
        const char *m_nameCertServer;
        const char *m_namePrivServer;

        RTCRX509CERTIFICATE m_certCA;
        RTCRX509CERTIFICATE m_certServer;

        int generateRSA (void);
        int decryptClientRandom (const uint8_t *crypted_random);

        uint8_t m_server_random[SEC_RANDOM_SIZE];
        uint8_t m_client_random[SEC_RANDOM_SIZE];

        int generateKeys(void);

        void updateKey (uint8_t * key, uint8_t * update_key);

        int outCryptInfo (VRDPInputCtx *pInputCtx);

        void InitMembers (void);
        void DestroyMembers (void);

        int Decrypt (VRDPInputCtx *pInputCtx);

        int signKey (uint8_t *sig, uint8_t *data, uint32_t datalen);

    public:
        SECTP ();
        ~SECTP ();

        int EnableEncryption (uint32_t fu32EncryptionMethods);
        void SetRdesktop (bool fRdesktop) { m_fCryptLevelActive = fRdesktop; }

        int SendConnectResponse (VRDPInputCtx *pInputCtx, uint8_t u8Result, uint8_t u8Channels);
        void Disconnect (VRDPInputCtx *pInputCtx);
        int SendLicense (VRDPInputCtx *pInputCtx);
        int Recv (VRDPInputCtx *pInputCtx);
        int Send (uint8_t u8Version, VRDPCtx *pCtx, uint32_t u32SubstituteFlags);
        void Reset (void);
};

#endif /* !VRDP_INCLUDED_SRC_secure_h */

