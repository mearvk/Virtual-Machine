/* $Id: VBoxCryptoVfs.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
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


/** @page pg_vbox_crypto_vfs    VBoxCryptoVfs - Cryptographic Support module VFS layer
 *
 * This module provides cryptographic helpers as an extension pack plugin
 * used for the full VM encryption, śpecifically the code to read and write
 * encrypted files.
 *
 *
 * @section sec_vbox_crypto_vfs_file  File format
 *
 * The encrypted file acts as a container for the raw file data to allow
 * encrypting arbitrary files. The format should support streaming as well as
 * seeking inside the payload.
 * The following shows the general layout of an encrypted file:
 *
 * File:
 * +---+-----+---------+---------+---------+    +---------+-----+
 * |   |     |         |         |         |    |         |     |
 * | H | Rnd |         |         |         |    |         | Rnd |
 * | D | Pad | Chunk 0 | Chunk 1 | Chunk 2 |....| Chunk N | Pad |
 * | R |     |         |         |         |    |         |     |
 * |   |     |         |         |         |    |         |     |
 * +---+-----+---------+---------+---------+    +---------+-----+
 *
 * Chunk:
 * +---+-----------------+
 * | I | Encrypted data  |
 * | V | +---+---------+ |
 * | / | | S |         | |
 * | G | | i |         | |
 * | C | | z | Payload | |
 * | M | | e |         | |
 * |   | +---+---------+ |
 * +---+-----------------+
 *
 * Each file consists of a small plaintext header consisting of a magic to identify
 * an encrypted file, the size of a chunk in bytes and some random bytes.
 * This is the only non encrypted part of the file.
 * The header is followed by a bunch of padding with random data to make finding the
 * start of the first encrypted chunk much more difficult.
 * The length of the random padding can be between 0 ... Chunk size and is determined by
 * the hash of the file header and the DEK used for en-/decryption. Due to the random
 * bytes in the header the size of the random padding is different for two files, even if
 * the DEK stays the same.
 * The padding is followed by the individual chunks, the number depends on the wrapped file content.
 * Each chunk has the same size. The last chunk is followed by another random padding, the padding
 * at the front and at the end add up to a full chunk size and is required in order to effectively
 * hide the start of the first chunk.
 *
 * Each chunk is prepended by the IV and GCM auth data for that particular chunk. Afterwards comes the
 * payload area containing the encrypted data. The encrypted data starts with a 32-bit size indicator,
 * indicating how much of the payload area is actually used. The MSB of the size indicator contains a flag which
 * is set if this is the last chunk in the stream. The unoccupied space is filled with random data.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/base64.h>
#include <iprt/file.h>
#include <iprt/memsafer.h>
#include <iprt/string.h>
#include <iprt/crypto/cipher.h>
#include <iprt/crypto/digest.h>
#include <iprt/crypto/misc.h>
#include <iprt/vfslowlevel.h>

#include <VBox/err.h>

#include "VBoxCryptoInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** The encrypted file header. */
#define ENCFILEHDR_MAGIC        "\177VirtualBox Encrypted File\n\0\0\0\0\0"
/** Size of a chunk. */
#define BUF_DATA_SIZE           _64K
/** Flag whether the loaded chunk is the last one in the stream. */
#define ENCFILE_CHUNK_EOS_BIT   RT_BIT_32(31)
/** Version indicator. */
#define ENCFILE_VERSION         UINT32_C(0x00010000)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Header of the encrypted file.
 */
typedef struct ENCFILEHDR
{
    /** Magic string which identifies this file as encrypted VBox file format (ENCFILEHDR_MAGIC). */
    char                        szMagic[32];
    /** Version field. */
    uint32_t                    u32Version;
    /** Encryption unit size in bytes (little endian). */
    uint32_t                    cbUnit;
    /** Some padding to a 16 byte boundary. */
    uint64_t                    u64Pad;
    /** Some random data which is used to derive the number of bytes to skip before real data starts,
     * used in conjunction with file header hashing and the DEK. */
    uint8_t                     abRnd[16];
} ENCFILEHDR, *PENCFILEHDR;
AssertCompileSize(ENCFILEHDR, 32 + 4 + 4 + 8 + 16);
AssertCompileMemberAlignment(ENCFILEHDR, u32Version, 4);
AssertCompileMemberAlignment(ENCFILEHDR, cbUnit, 4);
AssertCompileMemberAlignment(ENCFILEHDR, abRnd, 16);


/**
 * Encrypted file or I/O stream instance.
 */
typedef struct VBOXCRYPTOFILEVFS
{
    /** The crypto context handle. */
    VBOXCRYPTOCTX               hCryptoCtx;
    /** Encryption unit size. */
    size_t                      cbUnit;
    /** The size of data in the unit excluding metadata */
    size_t                      cbPayloadPerUnit;
    /** The current offset of the payload (for seeking). */
    uint64_t                    offPayloadPos;
    /** The pointer to the size indicator in the currently loaded chunk for reading/writing complete chunks. */
    uint32_t                    *pcbPayload;
    /** The buffer for the payload data for reading/writing complete chunks, derived from VBOXCRYPTOFILEVFS::pcbPayload. */
    uint8_t                     *pbPayload;
    /** Chunk number the VBOXCRYPTOFILEVFS::pbPayload contains the data from for reading/writing complete chunk. */
    uint64_t                    idChunk;
    /** The pointer to the size indicator in the incomplete chunk being appended to the file. */
    uint32_t                    *pcbPayloadAppend;
    /** The buffer for the payload data for appending, derived from VBOXCRYPTOFILEVFS::pcbPayloadAppend. */
    uint8_t                     *pbPayloadAppend;
    /** Chunk number the VBOXCRYPTOFILEVFS::pbPayloadAppend contains the data from. */
    uint64_t                    idChunkAppend;
    /** The buffer for encrypted data */
    uint8_t                     *pbEncrypted;
    /** Number of bytes available in the chunk. */
    size_t                      cbDataInChunk;
    /** Number of bytes of random data skipped after the header. */
    size_t                      cbSkipped;
    /** Size of the overall payload in bytes. */
    uint64_t                    cbPayload;
    /** Flag whether the currently loaded chunk was modified and needs writing. */
    bool                        fChunkNeedsWriting;
    /** Flag whether padding at the end of the file is required. */
    bool                        fPadEndOfFile;
    /** Flag whether the file already has padding at the end. */
    bool                        fPaddingExists;
    /** Flag whether the currently loaded chunk has the EOS flag set. */
    bool                        fChunkEos;
    /** Underlying I/O stream we do I/O from/to. */
    RTVFSIOSTREAM               hIos;
    /** The file face of hIos, if we're using an actual file. */
    RTVFSFILE                   hFile;
} VBOXCRYPTOFILEVFS;
/** Pointer to a encrypted file or I/O stream instance. */
typedef VBOXCRYPTOFILEVFS *PVBOXCRYPTOFILEVFS;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Calculates the chunk ID from the given payload offset.
 *
 * @returns Chunk ID.
 * @param   pThis               The encrypted file VFS instance.
 * @param   offPayload          The payload offset to get the chunk ID from.
 * @param   poffChunk           Where to store the offset inside the chunk the given payload offset belongs to.
 */
DECLINLINE(uint64_t) vboxCryptoFileGetChunkIdFromPayloadOffset(PVBOXCRYPTOFILEVFS pThis, uint64_t offPayload, uint64_t *poffChunk)
{
    if (poffChunk)
        *poffChunk = offPayload % pThis->cbPayloadPerUnit;

    return offPayload / pThis->cbPayloadPerUnit;
}


/**
 * Returns the starting offset in the underlying storage object for the given chunk ID.
 *
 * @returns Starting offset for the given chunk ID.
 * @param   pThis               The encrypted file VFS instance.
 * @param   idChunk             The chunk ID to get the starting offset of.
 */
DECLINLINE(RTFOFF) vboxCryptoFileGetOffsetForChunkId(PVBOXCRYPTOFILEVFS pThis, uint64_t idChunk)
{
    return (RTFOFF)(idChunk * pThis->cbUnit + pThis->cbSkipped + sizeof(ENCFILEHDR));
}


/**
 * Returns the number of chunks for the current file.
 *
 * @returns Number of chunks.
 * @param   pThis               The encrypted file VFS instance.
 * @param   cbFile              Raw file size, including headers, padding and metadata for each chunk.
 */
DECLINLINE(uint64_t) vboxCryptoFileGetChunkCount(PVBOXCRYPTOFILEVFS pThis, uint64_t cbFile)
{
    cbFile -= sizeof(ENCFILEHDR) + pThis->cbSkipped;
    /* If there is padding at the end it needs top be removed from the file size as well. */
    if (pThis->fPaddingExists)
        cbFile -= pThis->cbUnit - pThis->cbSkipped;

    return (cbFile + pThis->cbUnit - 1) / pThis->cbUnit;
}


/**
 * Returns the size of the payload data in bytes (stripped of all headers, padding and metadata).
 *
 * @returns Payload size in bytes.
 * @param   pThis               The encrypted file VFS instance.
 * @param   cbFile              Raw file size, including headers, padding and metadata for each chunk.
 * @param   fExcludeLastChunk   Flag whether to exclude the last chunk from the calculation.
 */
static uint64_t vboxCryptoFileGetSizeWithoutMetadata(PVBOXCRYPTOFILEVFS pThis, uint64_t cbFile, bool fExcludeLastChunk)
{
    if (cbFile < sizeof(ENCFILEHDR) + pThis->cbSkipped)
        return 0;

    uint64_t cUnits = vboxCryptoFileGetChunkCount(pThis, cbFile);
    if (fExcludeLastChunk)
        cUnits--;

    return cUnits * pThis->cbPayloadPerUnit;
}


/**
 * Encrypts a full chunk and writes it to the underlying I/O stream.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 * @param   idChunk             ID of the chunk to write.
 * @param   pcbPayload          Where to store the size of the payload in bytes.
 * @param   pbPayload           The payload data.
 * @param   cbDataInChunk       Actual size of the data in the chunk, can be less than the chunk size.
 * @param   fEos                Flag whether this is the last chunk in the stream, only random padding comes after this chunk.
 */
static int vboxCryptoFileWriteChunkWithId(PVBOXCRYPTOFILEVFS pThis, uint64_t idChunk, uint32_t *pcbPayload, uint8_t *pbPayload,
                                          size_t cbDataInChunk, bool fEos)
{
    AssertReturn(cbDataInChunk <= UINT32_MAX, VERR_INVALID_STATE);
    uint32_t cbData =   (uint32_t)cbDataInChunk
                      | (fEos ? ENCFILE_CHUNK_EOS_BIT : 0);
    *pcbPayload = RT_H2LE_U32(cbData);

    /* Clear out any free space at the end with the random bytes. */
    if (cbDataInChunk < pThis->cbPayloadPerUnit)
    {
        int vrc = RTCrRandBytes(&pbPayload[cbDataInChunk], pThis->cbPayloadPerUnit - cbDataInChunk);
        if (RT_FAILURE(vrc))
            return vrc;
    }

    size_t cbEncrypted = 0;
    int vrc = vboxCryptoCtxEncrypt(pThis->hCryptoCtx, false, NULL, 0, pcbPayload, pThis->cbPayloadPerUnit + sizeof(uint32_t),
                                   &idChunk, sizeof(idChunk),
                                   pThis->pbEncrypted, pThis->cbUnit, &cbEncrypted);
    AssertRC(vrc);
    if (RT_SUCCESS(vrc))
    {
        RTFOFF offChunk = vboxCryptoFileGetOffsetForChunkId(pThis, idChunk);

        vrc = RTVfsIoStrmWriteAt(pThis->hIos, offChunk, pThis->pbEncrypted, cbEncrypted,
                                 true /*fBlocking*/, NULL /*pcbWritten*/);
    }

    return vrc;
}


/**
 * Appends a new full chunk to the file which wasn't used before.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 * @param   fEos                Flag whether this is the last chunk in the stream, only random padding comes after this chunk.
 */
static int vboxCryptoFileAppendChunk(PVBOXCRYPTOFILEVFS pThis, bool fEos)
{
    AssertReturn(pThis->cbDataInChunk, VERR_INVALID_STATE);

    int vrc = vboxCryptoFileWriteChunkWithId(pThis, pThis->idChunkAppend, pThis->pcbPayloadAppend, pThis->pbPayloadAppend,
                                             pThis->cbDataInChunk, fEos);
    if (RT_SUCCESS(vrc))
    {
        pThis->idChunkAppend++;
        pThis->cbDataInChunk = 0;
        pThis->fPadEndOfFile = true; /* Appending anything means padding needs to be restored. */
    }

    return vrc;
}


/**
 * Writes the currently loaded chunk (not the appending one) if it was modified.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 */
static int vboxCryptoFileWriteModifiedChunk(PVBOXCRYPTOFILEVFS pThis)
{
    /* Fast path if there is nothing to do. */
    if (!pThis->fChunkNeedsWriting)
        return VINF_SUCCESS;

    int vrc = vboxCryptoFileWriteChunkWithId(pThis, pThis->idChunk, pThis->pcbPayload, pThis->pbPayload,
                                             pThis->cbPayloadPerUnit, pThis->fChunkEos);
    if (RT_SUCCESS(vrc))
        pThis->fChunkNeedsWriting = false;

    return vrc;
}


/**
 * Reads the chunk with the given ID from the underlying I/O stream and decrypts it.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 * @param   idChunk             ID of the chunk to read.
 */
static int vboxCryptoFileReadChunkById(PVBOXCRYPTOFILEVFS pThis, uint64_t idChunk)
{
    /* Write the currently chunk out if it was modified. */
    int vrc = vboxCryptoFileWriteModifiedChunk(pThis);
    if (RT_FAILURE(vrc))
        return vrc;

    RTFOFF offChunk = vboxCryptoFileGetOffsetForChunkId(pThis, idChunk);

    size_t cbRead = 0;
    vrc = RTVfsIoStrmReadAt(pThis->hIos, offChunk, pThis->pbEncrypted, pThis->cbUnit,
                            true /*fBlocking*/, &cbRead);
    if (RT_SUCCESS(vrc))
    {
        size_t cbDecrypted = 0;
        vrc = vboxCryptoCtxDecrypt(pThis->hCryptoCtx, false, pThis->pbEncrypted, cbRead,
                                   &idChunk, sizeof(idChunk),
                                   pThis->pcbPayload, pThis->cbPayloadPerUnit + sizeof(uint32_t), &cbDecrypted);
        AssertRC(vrc);
        if (RT_FAILURE(vrc))
            return vrc;
        if (cbDecrypted != pThis->cbPayloadPerUnit + sizeof(uint32_t))
            return VERR_PARSE_ERROR;

        /* Validate the size indicator and set the EOS flag accordingly. */
        uint32_t cbDataInChunk = RT_LE2H_U32(*pThis->pcbPayload);
        pThis->fChunkEos = RT_BOOL(cbDataInChunk & ENCFILE_CHUNK_EOS_BIT);
        cbDataInChunk &= ~ENCFILE_CHUNK_EOS_BIT;

        if (RT_UNLIKELY(cbDataInChunk > pThis->cbPayloadPerUnit))
            return VERR_PARSE_ERROR;

        pThis->cbDataInChunk = cbDataInChunk;
        pThis->idChunk       = idChunk;
    }

    return vrc;
}


/**
 * Returns the chunk data for the given chunk ID.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 * @param   idChunk             ID of the chunk to query the buffer for.
 * @param   fWrite              Flag whether the buffer is queried for write (true) or read (false).
 * @param   ppbChunk            Where to store the start of the chunk buffer on success.
 * @param   pcbDataInChunk      Where to store the number of bytes in the chunk buffer on success.
 * @param   pfEos               Where to return the status of the End-Of-Stream flag on success.
 */
static int vboxCryptoFileQueryChunkBuf(PVBOXCRYPTOFILEVFS pThis, uint64_t idChunk, bool fWrite,
                                       uint8_t **ppbChunk, size_t *pcbDataInChunk,
                                       bool *pfEos)
{
    /* We need to write out any full append buffer first if a new chunk is appended. */
    if (   idChunk == pThis->idChunkAppend + 1
        && pThis->cbDataInChunk == pThis->cbPayloadPerUnit
        && fWrite)
    {
        int vrc = vboxCryptoFileAppendChunk(pThis, false /*fEos*/);
        if (RT_FAILURE(vrc))
            return vrc;

        Assert(idChunk == pThis->idChunkAppend);
    }

    /* Check whether the requested chunk is in the appending chunk buffer and return that. */
    if (idChunk == pThis->idChunkAppend)
    {
        /* Allocating the append buffer lazily. */
        if (!pThis->pbPayloadAppend)
        {
            pThis->pcbPayloadAppend = (uint32_t *)RTMemPageAllocZ(pThis->cbUnit);
            if (RT_UNLIKELY(!pThis->pcbPayloadAppend))
                return VERR_NO_MEMORY;

            pThis->pbPayloadAppend = (uint8_t *)(pThis->pcbPayloadAppend + 1);
        }

        *ppbChunk       = pThis->pbPayloadAppend;
        *pcbDataInChunk = pThis->cbDataInChunk;
        *pfEos          = true;
        return VINF_SUCCESS;
    }

    /* Try to read in the chunk if it differs from the currently loaded one. */
    int vrc = VINF_SUCCESS;
    if (idChunk != pThis->idChunk)
        vrc = vboxCryptoFileReadChunkById(pThis, idChunk);
    if (RT_SUCCESS(vrc))
    {
        *ppbChunk       = pThis->pbPayload;
        *pcbDataInChunk = pThis->cbPayloadPerUnit;
        *pfEos          = pThis->fChunkEos;
    }
    else
        pThis->idChunk = UINT64_MAX;

    return vrc;
}


/**
 * Determines the payload size for an existing file or I/O stream.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 * @param   cbFile              Size of the underlying file in bytes.
 */
static int vboxCryptoFileDeterminePayloadSize(PVBOXCRYPTOFILEVFS pThis, uint64_t cbFile)
{
    if (pThis->hFile != NIL_RTVFSFILE)
    {
        /*
         * Try to seek to the last chunk, read it and determine the amount of data there because it might not
         * be completely filled. Works only with seekable streams though.
         */
        size_t cbPadding = pThis->cbUnit - pThis->cbSkipped; /* Need to skip the padding at the end. */
        int vrc = RTVfsFileSeek(pThis->hFile, -(RTFOFF)(cbPadding + pThis->cbUnit), RTFILE_SEEK_END, NULL /*poffActual*/);
        if (RT_SUCCESS(vrc))
        {
            /* Allocate append buffer and read the data into it. */
            Assert(!pThis->pcbPayloadAppend);

            pThis->pcbPayloadAppend = (uint32_t *)RTMemPageAllocZ(pThis->cbUnit);
            if (RT_LIKELY(pThis->pcbPayloadAppend))
            {
                pThis->pbPayloadAppend = (uint8_t *)(pThis->pcbPayloadAppend + 1);

                /* Read into the buffer and decrypt. */
                size_t cbRead = 0;
                vrc = RTVfsFileRead(pThis->hFile, pThis->pbEncrypted, pThis->cbUnit, &cbRead);
                if (   RT_SUCCESS(vrc)
                    && cbRead == pThis->cbUnit)
                {
                    /* Seek back */
                    vrc = RTVfsFileSeek(pThis->hFile, (RTFOFF)(sizeof(ENCFILEHDR) + pThis->cbSkipped), RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                    AssertRCReturn(vrc, vrc);

                    size_t cbDecrypted = 0;
                    uint64_t idChunk = vboxCryptoFileGetChunkCount(pThis, cbFile) - 1;
                    vrc = vboxCryptoCtxDecrypt(pThis->hCryptoCtx, false, pThis->pbEncrypted, cbRead,
                                               &idChunk, sizeof(idChunk),
                                               pThis->pcbPayloadAppend, pThis->cbPayloadPerUnit + sizeof(uint32_t), &cbDecrypted);
                    AssertRC(vrc);
                    if (RT_FAILURE(vrc))
                        return vrc;
                    if (cbDecrypted != pThis->cbPayloadPerUnit + sizeof(uint32_t))
                        return VERR_PARSE_ERROR;

                    /* Validate the size indicator and set the EOS flag accordingly. */
                    uint32_t cbDataInChunk = RT_LE2H_U32(*pThis->pcbPayloadAppend);
                    if (RT_UNLIKELY(!(cbDataInChunk & ENCFILE_CHUNK_EOS_BIT)))
                        return VERR_PARSE_ERROR;

                    cbDataInChunk &= ~ENCFILE_CHUNK_EOS_BIT;
                    if (RT_UNLIKELY(cbDataInChunk > pThis->cbPayloadPerUnit))
                        return VERR_PARSE_ERROR;

                    pThis->cbDataInChunk = cbDataInChunk;
                    pThis->idChunkAppend = idChunk;
                    pThis->cbPayload = vboxCryptoFileGetSizeWithoutMetadata(pThis, cbFile, true /*fExcludeLastChunk*/);
                    pThis->cbPayload += cbDataInChunk; /* Add the amount of data in the last chunk. */
                    return VINF_SUCCESS;
                }
                else if (   RT_SUCCESS(vrc)
                         && cbRead != pThis->cbUnit)
                    return VERR_PARSE_ERROR; /* The file got truncated/corrupted. */
            }
            /* else: No memory, fall through. */
        }
    }

    /*
     * Fallback for I/O streams and non seekable files. Reporting is incorrect but there is nothing we can do
     * about it.
     */
    pThis->cbPayload = vboxCryptoFileGetSizeWithoutMetadata(pThis, cbFile, false /*fExcludeLastChunk*/);
    return VINF_SUCCESS;
}


/**
 * Pads the encrypted file with random data.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 * @param   cbPadding           Number of bytes to pad with.
 */
static int vboxCryptoFilePadWithRandomData(PVBOXCRYPTOFILEVFS pThis, uint64_t cbPadding)
{
    int vrc = VINF_SUCCESS;
    uint8_t abRnd[_1K];
    size_t cbPaddingLeft = cbPadding;
    while (   cbPaddingLeft
           && RT_SUCCESS(vrc))
    {
        size_t cbThisPad = RT_MIN(cbPaddingLeft, sizeof(abRnd));
        vrc = RTCrRandBytes(&abRnd[0], sizeof(abRnd));
        if (RT_SUCCESS(vrc))
            vrc = RTVfsIoStrmWrite(pThis->hIos, &abRnd[0], cbThisPad, true /*fBlocking*/, NULL /*pcbWritten*/);
        cbPaddingLeft -= cbThisPad;
    }

    return vrc;
}


/**
 * Reads and checks the encrypted header.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 */
static int vboxCryptoFileEncryptedHdrCheck(PVBOXCRYPTOFILEVFS pThis)
{
    uint8_t abBuf[512]; /**< For the header and random data which we might skip. */

    int vrc = RTVfsIoStrmRead(pThis->hIos, &abBuf[0], sizeof(ENCFILEHDR), true /*fBlocking*/, NULL /*pcbRead*/);
    if (RT_SUCCESS(vrc))
    {
        PENCFILEHDR pHdr = (PENCFILEHDR)&abBuf[0];

        if (!memcmp(&pHdr->szMagic[0], ENCFILEHDR_MAGIC, sizeof(pHdr->szMagic)))
        {
            if (RT_LE2H_U32(pHdr->u32Version) == ENCFILE_VERSION)
            {
                pThis->cbUnit = RT_LE2H_U32(pHdr->cbUnit);
                if (pThis->cbUnit <= _1M)
                {
                    vrc = vboxCryptoCtxCalculatePaddingSplit(pThis->hCryptoCtx, pThis->cbUnit, pHdr, sizeof(*pHdr),
                                                             &pThis->cbSkipped);
                    if (RT_SUCCESS(vrc))
                    {
                        /* Read and skip the random data in case the underlying I/O stream doesn't support seeking. */
                        size_t cbSkipLeft = pThis->cbSkipped;
                        while (   RT_SUCCESS(vrc)
                               && cbSkipLeft)
                        {
                            size_t cbThisSkip = RT_MIN(sizeof(abBuf), cbSkipLeft);

                            vrc = RTVfsIoStrmRead(pThis->hIos, &abBuf[0], cbThisSkip, true /*fBlocking*/, NULL /*pcbRead*/);
                            cbSkipLeft -= cbThisSkip;
                        }

                        if (RT_SUCCESS(vrc))
                            vrc = vboxCryptoCtxQueryDecryptedSize(pThis->hCryptoCtx, pThis->cbUnit, &pThis->cbPayloadPerUnit);
                    }
                }
                else
                    vrc = VERR_PARSE_ERROR;
            }
            else
                vrc = VERR_NOT_SUPPORTED;
        }
        else
            vrc = VERR_INVALID_MAGIC;
    }

    return vrc;
}


/**
 * Writes the encrypted header to the underlying I/O stream.
 *
 * @returns VBox status code.
 * @param   pThis               The encrypted file VFS instance.
 */
static int vboxCryptoFileEncryptedHdrWrite(PVBOXCRYPTOFILEVFS pThis)
{
    int vrc = vboxCryptoCtxQueryEncryptedSize(pThis->hCryptoCtx, BUF_DATA_SIZE, &pThis->cbUnit);
    if (RT_SUCCESS(vrc))
        vrc = vboxCryptoCtxQueryDecryptedSize(pThis->hCryptoCtx, pThis->cbUnit, &pThis->cbPayloadPerUnit);
    if (RT_SUCCESS(vrc))
    {
        ENCFILEHDR Hdr; RT_ZERO(Hdr);

        memcpy(&Hdr.szMagic, ENCFILEHDR_MAGIC, RT_MIN(sizeof(Hdr.szMagic), sizeof(ENCFILEHDR_MAGIC)));
        Hdr.cbUnit     = RT_H2LE_U32((uint32_t)pThis->cbUnit);
        Hdr.u32Version = RT_H2LE_U32(ENCFILE_VERSION);

        /*
         * Fill the remainder of the header with random data so we get different padding splits even when the
         * DEK is the same.
         */
        vrc = RTCrRandBytes(&Hdr.abRnd[0], sizeof(Hdr.abRnd));
        if (RT_SUCCESS(vrc))
        {
            /*
             * Calculate the padding split to hide where the first data block starts in the file.
             */
            vrc = vboxCryptoCtxCalculatePaddingSplit(pThis->hCryptoCtx, pThis->cbUnit, &Hdr, sizeof(Hdr),
                                                     &pThis->cbSkipped);
            if (RT_SUCCESS(vrc))
            {
                /* Write the header and random data for the padding. */
                vrc = RTVfsIoStrmWrite(pThis->hIos, &Hdr, sizeof(Hdr), true /*fBlocking*/, NULL /*pcbWritten*/);
                if (RT_SUCCESS(vrc))
                {
                    vrc = vboxCryptoFilePadWithRandomData(pThis, pThis->cbSkipped);
                    if (RT_SUCCESS(vrc))
                    {
                        pThis->fPadEndOfFile = true;
                        pThis->idChunkAppend = 0;
                        return VINF_SUCCESS;
                    }
                }
            }
        }
        else
            vrc = VERR_NO_MEMORY;
    }

    return vrc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_Close(void *pvThis)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;

    /* Write out any modified chunks. */
    int vrc = vboxCryptoFileWriteModifiedChunk(pThis);

    /* Append any pending chunk now. */
    if (   RT_SUCCESS(vrc)
        && pThis->cbDataInChunk)
        vrc = vboxCryptoFileAppendChunk(pThis, true /*fEos*/);

    /*
     * Pad the end of the file with random data to make figuring out the chunk boundaries
     * much more difficult.
     */
    if (   RT_SUCCESS(vrc)
        && pThis->fPadEndOfFile)
        vrc = vboxCryptoFilePadWithRandomData(pThis, pThis->cbUnit - pThis->cbSkipped);

    /* Release references to the underlying storage object. */
    RTVfsIoStrmRelease(pThis->hIos);
    pThis->hIos  = NIL_RTVFSIOSTREAM;
    RTVfsFileRelease(pThis->hFile);
    pThis->hFile = NIL_RTVFSFILE;

    /* Destroy crypto context and free resources. */
    vboxCryptoCtxDestroy(pThis->hCryptoCtx);
    RTMemPageFree(pThis->pcbPayload, pThis->cbUnit);
    RTMemPageFree(pThis->pbEncrypted, pThis->cbUnit);
    pThis->pcbPayload  = NULL;
    pThis->pbPayload   = NULL;
    pThis->pbEncrypted = NULL;

    if (pThis->cbDataInChunk)
    {
        RTMemPageFree(pThis->pcbPayloadAppend, pThis->cbUnit);
        pThis->pcbPayloadAppend = NULL;
        pThis->pbPayloadAppend  = NULL;
    }
    return vrc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;
    int vrc = RTVfsIoStrmQueryInfo(pThis->hIos, pObjInfo, enmAddAttr);
    if (RT_SUCCESS(vrc))
    {
        /* Adjust the file size to exclude all the metadata. */
        pObjInfo->cbObject = (RTFOFF)pThis->cbPayload;
    }

    return vrc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_Read(void *pvThis, RTFOFF off, PRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;

    Assert(pSgBuf->cSegs == 1); /* Caller deals with multiple SGs. */
    RT_NOREF(fBlocking);

    uint64_t offUnsigned = off < 0 ? pThis->offPayloadPos : (uint64_t)off;
    uint64_t offChunk = 0;
    uint64_t idChunk = vboxCryptoFileGetChunkIdFromPayloadOffset(pThis, offUnsigned, &offChunk);

#if 0 /** @todo r=bird: can't decode comment. */
    /* Read in the first chunk if it is not matching. */
    int vrc = VINF_SUCCESS;
    size_t cbLeftToRead = pSgBuf->paSegs[0].cbSeg;
    if (RT_SUCCESS(rc))
    {
        /* Read in all the data. */
        uint8_t *pbDst = (uint8_t *)pSgBuf->paSegs[0].pvSeg;
        for (;;)
#endif

    /* Read in all the data. */
    size_t       cbLeftToRead     = 0;
    uint8_t     *pbDst            = (uint8_t *)RTSgBufGetCurrentSegment(pSgBuf, ~(size_t)0, &cbLeftToRead);
    size_t const cbInitialRequest = cbLeftToRead;
    int          vrc              = VINF_SUCCESS;
    for (;;)
    {
        size_t cbData;
        bool fEos = false;
        uint8_t *pbSrc = NULL;

        vrc = vboxCryptoFileQueryChunkBuf(pThis, idChunk, false /*fWrite*/, &pbSrc, &cbData, &fEos);
        if (RT_FAILURE(vrc))
            break;

        size_t cbThisRead = RT_MIN(cbLeftToRead, cbData - offChunk);
        memcpy(pbDst, &pbSrc[offChunk], cbThisRead);

        cbLeftToRead -= cbThisRead;
        offUnsigned  += cbThisRead;
        pbDst        += cbThisRead;
        offChunk      = 0;
        if (!cbLeftToRead)
            break;

        if (fEos)
        {
            vrc = VERR_EOF;
            break;
        }

        /* Go to the next chunk. */
        idChunk++;
    }

    if (pcbRead)
        *pcbRead = cbInitialRequest - cbLeftToRead;
    RTSgBufAdvance(pSgBuf, cbInitialRequest - cbLeftToRead);

    if (   RT_FAILURE(vrc)
        && pcbRead)
    {
        if (*pcbRead == 0 && vrc == VERR_EOF)
            vrc = VINF_EOF;
        else
            vrc = VINF_SUCCESS;
    }

    pThis->offPayloadPos = offUnsigned;
    return vrc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_Write(void *pvThis, RTFOFF off, PRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;

    Assert(pSgBuf->cSegs == 1); /* Caller deals with multiple SGs. */
    RT_NOREF(fBlocking);

    uint64_t offUnsigned = off < 0 ? pThis->offPayloadPos : (uint64_t)off;
    uint64_t offChunk = 0;
    uint64_t idChunk = vboxCryptoFileGetChunkIdFromPayloadOffset(pThis, offUnsigned, &offChunk);

    size_t         cbLeftToWrite    = 0;
    uint8_t const *pbSrc            = (uint8_t const *)RTSgBufGetCurrentSegment(pSgBuf, ~(size_t)0, &cbLeftToWrite);
    size_t const   cbInitialRequest = cbLeftToWrite;
    int            vrc              = VINF_SUCCESS;

    for (;;)
    {
        size_t cbData;
        bool fEos = false;
        uint8_t *pbDst = NULL;

        vrc = vboxCryptoFileQueryChunkBuf(pThis, idChunk, true /*fWrite*/, &pbDst, &cbData, &fEos);
        if (RT_FAILURE(vrc))
            break;

        size_t cbThisWrite = RT_MIN(cbLeftToWrite, pThis->cbPayloadPerUnit - offChunk);
        memcpy(&pbDst[offChunk], pbSrc, cbThisWrite);

        cbLeftToWrite -= cbThisWrite;
        offUnsigned   += cbThisWrite;
        offChunk      += cbThisWrite;
        pbSrc         += cbThisWrite;

        /* When writing to the append buffer write it out if it is full and there is more to write. */
        if (pbDst == pThis->pbPayloadAppend)
        {
            pThis->cbPayload    += offChunk > pThis->cbDataInChunk ? offChunk - pThis->cbDataInChunk : 0;
            pThis->cbDataInChunk = RT_MAX((size_t)offChunk, pThis->cbDataInChunk);

            if (   pThis->cbDataInChunk == pThis->cbPayloadPerUnit
                && cbLeftToWrite)
            {
                vrc = vboxCryptoFileAppendChunk(pThis, false /*fEos*/);
                if (RT_FAILURE(vrc))
                    break;
            }
        }
        else
        {
            Assert(pbDst == pThis->pbPayload);

            /* Modifying the regular buffer means it needs to be written out before a new one can be loaded. */
            pThis->fChunkNeedsWriting = true;
        }

        if (!cbLeftToWrite)
            break;

        offChunk = 0;
        idChunk++;
    }

    size_t const cbWritten = cbInitialRequest - cbLeftToWrite;
    if (pcbWritten)
        *pcbWritten = cbWritten;
    if (   RT_FAILURE(vrc)
        && pcbWritten)
    {
        RTSgBufAdvance(pSgBuf, cbWritten);
        vrc = VINF_SUCCESS;
    }

    pThis->offPayloadPos = offUnsigned;
    return vrc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_Flush(void *pvThis)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;

    /** @todo Flush any unencrypted data block if full. */

    return RTVfsIoStrmFlush(pThis->hIos);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                                   uint32_t *pfRetEvents)
{
    RT_NOREF(pvThis, fEvents, cMillies, fIntr, pfRetEvents);

    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_Tell(void *pvThis, PRTFOFF poffActual)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;

    *poffActual = (RTFOFF)pThis->offPayloadPos;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetMode}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;
    AssertReturn(pThis->hFile != NIL_RTVFSFILE, VERR_NOT_SUPPORTED);

    RT_NOREF_PV(fMode); RT_NOREF_PV(fMask);
    int vrc = VERR_NOT_SUPPORTED;

    return vrc;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                                    PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;
    AssertReturn(pThis->hFile != NIL_RTVFSFILE, VERR_NOT_SUPPORTED);

    RT_NOREF_PV(pAccessTime); RT_NOREF_PV(pModificationTime); RT_NOREF_PV(pChangeTime); RT_NOREF_PV(pBirthTime);
    /// @todo int vrc = RTVfsFileSetTimes(pThis->hFile, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    int vrc = VERR_NOT_SUPPORTED;

    return vrc;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;
    AssertReturn(pThis->hFile != NIL_RTVFSFILE, VERR_NOT_SUPPORTED);

    RT_NOREF_PV(uid); RT_NOREF_PV(gid);
    /// @todo int vrc = RTVfsFileSetOwner(pThis->hFile, uid, gid);
    int vrc = VERR_NOT_SUPPORTED;

    return vrc;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;
    AssertReturn(pThis->hFile != NIL_RTVFSFILE, VERR_NOT_SUPPORTED);

    /*
     * Seek relative to which position.
     */
    uint64_t offWrt;
    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
            offWrt = 0;
            break;

        case RTFILE_SEEK_CURRENT:
            offWrt = pThis->offPayloadPos;
            break;

        case RTFILE_SEEK_END:
        {
            int vrc = RTVfsFileQuerySize(pThis->hFile, &offWrt);
            if (RT_FAILURE(vrc))
                return vrc;

            offWrt = pThis->cbPayload;
            break;
        }

        default:
            return VERR_INTERNAL_ERROR_5;
    }

    /*
     * Calc new position, take care to stay within RTFOFF type bounds.
     */
    uint64_t offNew;
    if (offSeek == 0)
        offNew = offWrt;
    else if (offSeek > 0)
    {
        offNew = offWrt + (uint64_t)offSeek;
        if (   offNew < offWrt
            || offNew > RTFOFF_MAX)
            offNew = RTFOFF_MAX;
    }
    else if ((uint64_t)-offSeek < offWrt)
        offNew = offWrt + (uint64_t)offSeek;
    else
        offNew = 0;

    /*
     * Update the state and set return value.
     */
    if (pThis->offPayloadPos != offNew)
    {
        /* Invalidate the current chunk data. */
        pThis->idChunk       = UINT64_MAX;
        /** @todo What about half written chunks? */
        pThis->offPayloadPos = offNew;
    }


    *poffActual = (RTFOFF)offNew;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;
    AssertReturn(pThis->hFile != NIL_RTVFSFILE, VERR_NOT_SUPPORTED);

    *pcbFile = pThis->cbPayload;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSetSize}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_SetSize(void *pvThis, uint64_t cbFile, uint32_t fFlags)
{
    RT_NOREF(pvThis, cbFile, fFlags);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQueryMaxSize}
 */
static DECLCALLBACK(int) vboxCryptoFileVfs_QueryMaxSize(void *pvThis, uint64_t *pcbMax)
{
    PVBOXCRYPTOFILEVFS pThis = (PVBOXCRYPTOFILEVFS)pvThis;
    AssertReturn(pThis->hFile != NIL_RTVFSFILE, VERR_NOT_SUPPORTED);

    return RTVfsFileQueryMaxSize(pThis->hFile, pcbMax);
}


/**
 * VBox encrypted I/O stream operations.
 */
DECL_HIDDEN_CONST(const RTVFSIOSTREAMOPS) g_VBoxCryptoVfsIosOps =
{ /* Stream */
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "VBox encrypted I/O stream",
        vboxCryptoFileVfs_Close,
        vboxCryptoFileVfs_QueryInfo,
        NULL,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    RTVFSIOSTREAMOPS_FEAT_NO_SG,
    vboxCryptoFileVfs_Read,
    vboxCryptoFileVfs_Write,
    vboxCryptoFileVfs_Flush,
    vboxCryptoFileVfs_PollOne,
    vboxCryptoFileVfs_Tell,
    NULL /*Skip*/,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION,
};


/**
 * VBox encrypted file operations.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_VBoxCryptoVfsFileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "VBox encrypted file",
            vboxCryptoFileVfs_Close,
            vboxCryptoFileVfs_QueryInfo,
            NULL,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        RTVFSIOSTREAMOPS_FEAT_NO_SG,
        vboxCryptoFileVfs_Read,
        vboxCryptoFileVfs_Write,
        vboxCryptoFileVfs_Flush,
        vboxCryptoFileVfs_PollOne,
        vboxCryptoFileVfs_Tell,
        NULL /*Skip*/,
        NULL /*ZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    /*RTVFSIOFILEOPS_FEAT_NO_AT_OFFSET*/ 0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_UOFFSETOF(RTVFSFILEOPS, ObjSet) - RT_UOFFSETOF(RTVFSFILEOPS, Stream.Obj),
        vboxCryptoFileVfs_SetMode,
        vboxCryptoFileVfs_SetTimes,
        vboxCryptoFileVfs_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    vboxCryptoFileVfs_Seek,
    vboxCryptoFileVfs_QuerySize,
    vboxCryptoFileVfs_SetSize,
    vboxCryptoFileVfs_QueryMaxSize,
    RTVFSFILEOPS_VERSION
};


/**
 * Creates a new crypto file instance for reading and/or writing.
 *
 * @returns VBox status code.
 * @param   hVfsIosSrc          The I/O stream handle of the underlying storage object.
 * @param   hVfsFileSrc         The VFS file handle of the underlying storage object if it is a file.
 * @param   cbFile              Size of the underlying I/O stream/file in bytes. If it is > 0 there should be
 *                              a valid header already. If 0 a new header is created and written out immediately.
 * @param   pszKeyStore         The keystore data containing the encrypted DEK.
 * @param   pszPassword         The password the file is encrypted with.
 * @param   phVfsIos            Where to store the handle to the created I/O stream on success, optional.
 * @param   phVfsFile           Where to store the handle to the created VFS file on success, optional.
 */
static int vboxCryptoFileCreateInstance(RTVFSIOSTREAM hVfsIosSrc, RTVFSFILE hVfsFileSrc, uint64_t cbFile,
                                        const char *pszKeyStore, const char *pszPassword,
                                        PRTVFSIOSTREAM phVfsIos, PRTVFSFILE phVfsFile)
{
    VBOXCRYPTOCTX hCryptoCtx;
    int vrc = vboxCryptoCtxLoad(pszKeyStore, pszPassword, &hCryptoCtx);
    if (RT_SUCCESS(vrc))
    {
        /*
         * Create a file or I/O stream instance.
         *
         * @todo Fix file open flags.
         */
        RTVFSFILE          hVfsFileCrypto = NIL_RTVFSFILE;
        RTVFSIOSTREAM      hVfsIosCrypto  = NIL_RTVFSIOSTREAM;
        PVBOXCRYPTOFILEVFS pThis;
        if (hVfsFileSrc != NIL_RTVFSFILE)
            vrc = RTVfsNewFile(&g_VBoxCryptoVfsFileOps, sizeof(*pThis), RTFILE_O_READWRITE, NIL_RTVFS, NIL_RTVFSLOCK,
                               &hVfsFileCrypto, (void **)&pThis);
        else
            vrc = RTVfsNewIoStream(&g_VBoxCryptoVfsIosOps, sizeof(*pThis), RTFILE_O_READWRITE, NIL_RTVFS, NIL_RTVFSLOCK,
                                   &hVfsIosCrypto, (void **)&pThis);
        if (RT_SUCCESS(vrc))
        {
            pThis->hCryptoCtx           = hCryptoCtx;
            pThis->hFile                = hVfsFileSrc;
            pThis->hIos                 = hVfsIosSrc;
            pThis->fChunkNeedsWriting   = false;
            pThis->fPaddingExists       = cbFile > 0;
            pThis->idChunk              = UINT64_MAX;
            pThis->idChunkAppend        = UINT64_MAX;

            if (cbFile)
                vrc = vboxCryptoFileEncryptedHdrCheck(pThis);
            else
                vrc = vboxCryptoFileEncryptedHdrWrite(pThis);

            /* Allocate the required data buffers. */
            if (RT_SUCCESS(vrc))
            {
                pThis->cbPayloadPerUnit -= sizeof(uint32_t); /* For the size indicator. */
                pThis->pcbPayload = (uint32_t *)RTMemPageAllocZ(pThis->cbUnit);
                if (RT_LIKELY(pThis->pcbPayload))
                {
                    pThis->pbPayload = (uint8_t *)(pThis->pcbPayload + 1);
                    pThis->pbEncrypted = (uint8_t *)RTMemPageAllocZ(pThis->cbUnit);
                    if (RT_LIKELY(pThis->pbEncrypted))
                    {
                        /* Try to determine the correct payload size of an existing stream. */
                        if (   RT_SUCCESS(vrc)
                            && cbFile)
                            vrc = vboxCryptoFileDeterminePayloadSize(pThis, cbFile);

                        /* Read in the first chunk if there is data available. */
                        if (   RT_SUCCESS(vrc)
                            && cbFile)
                            vrc = vboxCryptoFileReadChunkById(pThis, 0 /*idChunk*/);

                        if (RT_SUCCESS(vrc))
                        {
                            /*
                             * We're good.
                             */
                            if (phVfsFile)
                                *phVfsFile = hVfsFileCrypto;
                            else if (hVfsFileCrypto == NIL_RTVFSFILE)
                                *phVfsIos = hVfsIosCrypto;
                            else
                            {
                                *phVfsIos = RTVfsFileToIoStream(hVfsFileCrypto);
                                RTVfsFileRelease(hVfsFileCrypto);
                                AssertReturn(*phVfsIos != NIL_RTVFSIOSTREAM, VERR_INTERNAL_ERROR_5);
                            }
                            return VINF_SUCCESS;
                        }

                        RTMemPageFree(pThis->pbEncrypted, pThis->cbUnit);
                        pThis->pbEncrypted = NULL;
                    }

                    RTMemPageFree(pThis->pcbPayload, pThis->cbUnit);
                    pThis->pcbPayload = NULL;
                    pThis->pbPayload  = NULL;
                }
                else
                    vrc = VERR_NO_MEMORY;
            }
        }

        int vrc2 = vboxCryptoCtxDestroy(hCryptoCtx);
        AssertRC(vrc2); RT_NOREF(vrc2);
    }

    return vrc;
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoFileFromVfsFile}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoFileFromVfsFile(RTVFSFILE hVfsFile, const char *pszKeyStore, const char *pszPassword,
                                                    PRTVFSFILE phVfsFile)
{
    AssertReturn(hVfsFile != NIL_RTVFSFILE, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszKeyStore, VERR_INVALID_POINTER);
    AssertReturn(*pszKeyStore != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPassword, VERR_INVALID_POINTER);
    AssertReturn(*pszPassword != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(phVfsFile, VERR_INVALID_POINTER);
    *phVfsFile = NIL_RTVFSFILE;

    /*
     * Retain the input file and cast it to an I/O stream.
     */
    RTVFSIOSTREAM hVfsIos = RTVfsFileToIoStream(hVfsFile);
    AssertReturn(hVfsIos != NIL_RTVFSIOSTREAM, VERR_INVALID_HANDLE);
    uint32_t cRefs = RTVfsFileRetain(hVfsFile);
    AssertReturnStmt(cRefs != UINT32_MAX, RTVfsIoStrmRelease(hVfsIos), VERR_INVALID_HANDLE);

    uint64_t cbFile = 0;
    int vrc = RTVfsFileQuerySize(hVfsFile, &cbFile);
    AssertRCReturn(vrc, vrc);

    /*
     * Do the job. (This always consumes the above retained references.)
     */
    return vboxCryptoFileCreateInstance(hVfsIos, hVfsFile, cbFile, pszKeyStore, pszPassword,
                                        NULL /*phVfsIos*/, phVfsFile);
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoIoStrmFromVfsIoStrmEncrypt}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoIoStrmFromVfsIoStrmEncrypt(RTVFSIOSTREAM hVfsIosDst, const char *pszKeyStore,
                                                               const char *pszPassword, PRTVFSIOSTREAM phVfsIosCrypt)
{
    AssertReturn(hVfsIosDst != NIL_RTVFSIOSTREAM, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszKeyStore, VERR_INVALID_POINTER);
    AssertReturn(*pszKeyStore != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPassword, VERR_INVALID_POINTER);
    AssertReturn(*pszPassword != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(phVfsIosCrypt, VERR_INVALID_POINTER);
    *phVfsIosCrypt = NIL_RTVFSIOSTREAM;

    /*
     * Retain the input stream, trying to obtain a file handle too so we can
     * fully mirror it.
     */
    uint32_t cRefs = RTVfsIoStrmRetain(hVfsIosDst);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);
    RTVFSFILE hVfsFile = RTVfsIoStrmToFile(hVfsIosDst);

    /*
     * Do the job. (This always consumes the above retained references.)
     */
    return vboxCryptoFileCreateInstance(hVfsIosDst, hVfsFile, 0 /*cbObject*/, pszKeyStore, pszPassword,
                                        phVfsIosCrypt, NULL /*phVfsFile*/);
}


/**
 * @interface_method_impl{VBOXCRYPTOIF,pfnCryptoIoStrmFromVfsIoStrmDecrypt}
 */
DECL_HIDDEN_CALLBACK(int) vboxCryptoIoStrmFromVfsIoStrmDecrypt(RTVFSIOSTREAM hVfsIosIn, const char *pszKeyStore,
                                                               const char *pszPassword, PRTVFSIOSTREAM phVfsIosOut)
{
    AssertReturn(hVfsIosIn != NIL_RTVFSIOSTREAM, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszKeyStore, VERR_INVALID_POINTER);
    AssertReturn(*pszKeyStore != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPassword, VERR_INVALID_POINTER);
    AssertReturn(*pszPassword != '\0', VERR_INVALID_PARAMETER);
    AssertPtrReturn(phVfsIosOut, VERR_INVALID_POINTER);
    *phVfsIosOut = NIL_RTVFSIOSTREAM;

    /*
     * Retain the input stream, trying to obtain a file handle too so we can
     * fully mirror it.
     */
    uint32_t cRefs = RTVfsIoStrmRetain(hVfsIosIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);
    RTVFSFILE hVfsFile = RTVfsIoStrmToFile(hVfsIosIn);

    RTFSOBJINFO ObjInfo;
    int vrc = RTVfsIoStrmQueryInfo(hVfsIosIn, &ObjInfo, RTFSOBJATTRADD_UNIX);
    if (RT_FAILURE(vrc))
        return vrc;

    /*
     * Do the job. (This always consumes the above retained references.)
     */
    return vboxCryptoFileCreateInstance(hVfsIosIn, hVfsFile, (uint64_t)ObjInfo.cbObject, pszKeyStore, pszPassword,
                                        phVfsIosOut, NULL /*phVfsFile*/);
}
