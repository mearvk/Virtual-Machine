/* $Id: udfhlp.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * IPRT - ISO 9660 and UDF Virtual Filesystem (read only).
 */

/*
 * Copyright (C) 2017-2025 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include "internal/iprt.h"
#include <iprt/fsvfs.h>

#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/asm-mem.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/crc.h>
#include <iprt/critsect.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/latin1.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/uni.h>
#include <iprt/utf16.h>
#include <iprt/formats/iso9660.h>
#include <iprt/formats/udf.h>

#include "udfhlp.h"



/**
 * UDF virtual partition read function.
 *
 * This deals with all the fun related to block mapping and such.
 *
 * @returns VBox status code.
 * @param   pVolInfo        The volume information (partition map ++).
 * @param   hVfsBacking     The backing file/device/whatever.
 * @param   idxPart         The virtual partition number.
 * @param   idxBlock        The block number.
 * @param   pvBuf           The output buffer.
 * @param   cbToRead        The number of bytes to read.
 * @param   offByteAddend   The byte offset relative to the block.
 */
DECLHIDDEN(int) RTFsUdfHlpVpRead(PCRTFSUDFVOLINFO pVolInfo, RTVFSFILE hVfsBacking,
                                 uint32_t idxPart, uint32_t idxBlock, void *pvBuf, size_t cbToRead, uint64_t offByteAddend /*=0*/)
{
    uint64_t const offByte = ((uint64_t)idxBlock << pVolInfo->cShiftBlock) + offByteAddend;

    int rc;
    if (idxPart < pVolInfo->cPartitions)
    {
        PRTFSISOVOLUDFPMAP  pPart = &pVolInfo->paPartitions[idxPart];
        switch (pPart->bType)
        {
            case RTFSUDF_PMAP_T_PLAIN:
                rc = RTVfsFileReadAt(hVfsBacking, offByte + pPart->offByteLocation, pvBuf, cbToRead, NULL);
                if (RT_SUCCESS(rc))
                {
                    Log3(("ISO/UDF: Read %#x bytes at %#RX64 (p%u/%#RX64(/%#RX64))\n",
                          cbToRead, offByte + pPart->offByteLocation, idxPart, offByte, offByte >> pVolInfo->cShiftBlock));
                    return VINF_SUCCESS;
                }
                Log(("ISO/UDF: Error reading %#x bytes at %#RX64 (p%u/%#RX64(/%#RX64)): %Rrc\n",
                     cbToRead, offByte + pPart->offByteLocation, idxPart, offByte, offByte >> pVolInfo->cShiftBlock, rc));
                break;

            default:
                AssertFailed();
                rc = VERR_ISOFS_IPE_1;
                break;
        }
    }
    else
    {
        Log(("ISO/UDF: Invalid partition index %#x (offset %#RX64), max partitions %#x\n",
             idxPart, offByte, pVolInfo->cPartitions));
        rc = VERR_ISOFS_INVALID_PARTITION_INDEX;
    }
    return rc;
}


/**
 * Converts an UDF timestamp into an IPRT timesspec.
 *
 * @param   pTimeSpec       Where to return the IRPT time.
 * @param   pUdf            The UDF timestamp.
 */
DECLHIDDEN(void) RTFsUdfHlpTimestamp2TimeSpec(PRTTIMESPEC pTimeSpec, PCUDFTIMESTAMP pUdf)
{
    /* Check the year range before we try convert anything as it's quite possible
       that this is zero. */
    if (   pUdf->iYear > 1678
        && pUdf->iYear < 2262)
    {
        RTTIME Time;
        Time.fFlags         = RTTIME_FLAGS_TYPE_UTC;
        Time.offUTC         = 0;
        Time.i32Year        = pUdf->iYear;
        Time.u8Month        = RT_MIN(RT_MAX(pUdf->uMonth, 1), 12);
        Time.u8MonthDay     = RT_MIN(RT_MAX(pUdf->uDay, 1), 31);
        Time.u8WeekDay      = UINT8_MAX;
        Time.u16YearDay     = 0;
        Time.u8Hour         = RT_MIN(pUdf->uHour, 23);
        Time.u8Minute       = RT_MIN(pUdf->uMinute, 59);
        Time.u8Second       = RT_MIN(pUdf->uSecond, 59);
        Time.u32Nanosecond  = pUdf->cCentiseconds           * UINT32_C(10000000)
                            + pUdf->cHundredsOfMicroseconds *   UINT32_C(100000)
                            + pUdf->cMicroseconds           *     UINT32_C(1000);
        RTTimeImplode(pTimeSpec, RTTimeNormalize(&Time));

        /* Only apply the UTC offset if it's within reasons. */
        if (RT_ABS(pUdf->offUtcInMin) <= 13*60)
            RTTimeSpecSubSeconds(pTimeSpec, pUdf->offUtcInMin * 60);
    }
    else
        RTTimeSpecSetNano(pTimeSpec, 0);
}


/**
 * Gathers the allocation extents from an ICB.
 *
 * @returns IPRT status code
 * @param   pVolInfo        The volume information (partition map ++).
 * @param   pbAllocDescs    Pointer to the allocation descriptor data.
 * @param   cbAllocDescs    The size of the allocation descriptor data.
 * @param   fIcbTagFlags    The ICB tag flags.
 * @param   idxDefaultPart  The default data partition.
 * @param   offAllocDescs   The default partition byte offset corresponding to
 *                          @a pbAllocDesc in case it's used as data storage
 *                          (type 3).
 * @param   cbMax           Maximum number of bytes we care to gather allocation
 *                          descriptors for.
 * @param   hVfsBacking     The backing file/device/whatever in case it's a a
 *                          very large file.
 * @param   pbBuf           The buffer, one logical block in size, for reading
 *                          more allocation descriptors for large files.
 * @param   pcExtents       Where to return the total number of extents being
 *                          returned (both in pFirstExtent and ppaExtents).
 * @param   pFirstExtent    The first extent.  Typically, we won't need more
 *                          than this one.
 * @param   papExtents      Where to return an array of additional extents.
 *                          Call RTFsUdfHlpFreeGatherExtents to free.
 */
DECLHIDDEN(int) RTFsUdfHlpGatherExtentsFromIcb(PCRTFSUDFVOLINFO pVolInfo, uint8_t const *pbAllocDescs, uint32_t cbAllocDescs,
                                               uint32_t fIcbTagFlags, uint32_t idxDefaultPart, uint64_t offAllocDescs,
                                               uint64_t cbMax, RTVFSFILE hVfsBacking, uint8_t *pbBuf,
                                               uint32_t *pcExtents, PRTFSISOEXTENT pFirstExtent, PRTFSISOEXTENT *ppaExtents)
{
    /*
     * Initialize return values for a no-extents return.
     */
    *pcExtents  = 0;
    pFirstExtent->off      = UINT64_MAX;
    pFirstExtent->cbExtent = 0;
    pFirstExtent->idxPart  = UINT32_MAX;
    *ppaExtents = NULL;

    /*
     * Figure the (minimal) size of an allocation descriptor, deal with the
     * embedded storage and invalid descriptor types.
     */
    uint32_t cbOneDesc;
    switch (fIcbTagFlags & UDF_ICB_FLAGS_AD_TYPE_MASK)
    {
        case UDF_ICB_FLAGS_AD_TYPE_EMBEDDED:
            *pcExtents             = 1;
            pFirstExtent->cbExtent = cbAllocDescs;
            pFirstExtent->off      = offAllocDescs;
            pFirstExtent->idxPart  = idxDefaultPart;
            return VINF_SUCCESS;

        case UDF_ICB_FLAGS_AD_TYPE_SHORT:       cbOneDesc = sizeof(UDFSHORTAD); break;
        case UDF_ICB_FLAGS_AD_TYPE_LONG:        cbOneDesc = sizeof(UDFLONGAD); break;
        case UDF_ICB_FLAGS_AD_TYPE_EXTENDED:    cbOneDesc = sizeof(UDFEXTAD); break;

        default:
            LogRelMax(45, ("ISO/UDF: Unknown allocation descriptor type %#x\n", fIcbTagFlags));
            return VERR_ISO_FS_UNKNOWN_AD_TYPE;
    }
    if (cbAllocDescs >= cbOneDesc)
    {
        /*
         * Loop thru the allocation descriptors.
         */
        uint64_t       offData    = 0;
        uint32_t       cExtents   = 0;
        PRTFSISOEXTENT paExtents  = NULL;
        PRTFSISOEXTENT pCurExtent = NULL;
        union
        {
            uint8_t const  *pb;
            PCUDFSHORTAD    pShort;
            PCUDFLONGAD     pLong;
            PCUDFEXTAD      pExt;
        } uPtr;
        uPtr.pb = pbAllocDescs;
        do
        {
            /*
             * Extract the information we need from the descriptor.
             */
            uint32_t idxBlock;
            uint32_t idxPart;
            uint32_t cb;
            uint8_t  uType;
            switch (fIcbTagFlags & UDF_ICB_FLAGS_AD_TYPE_MASK)
            {
                case UDF_ICB_FLAGS_AD_TYPE_SHORT:
                    uType    = uPtr.pShort->uType;
                    cb       = uPtr.pShort->cb;
                    idxBlock = uPtr.pShort->off;
                    idxPart  = idxDefaultPart;
                    cbAllocDescs -= sizeof(*uPtr.pShort);
                    uPtr.pShort++;
                    break;
                case UDF_ICB_FLAGS_AD_TYPE_LONG:
                    uType    = uPtr.pLong->uType;
                    cb       = uPtr.pLong->cb;
                    idxBlock = uPtr.pLong->Location.off;
                    idxPart  = uPtr.pLong->Location.uPartitionNo;
                    cbAllocDescs -= sizeof(*uPtr.pLong);
                    uPtr.pLong++;
                    break;
                case UDF_ICB_FLAGS_AD_TYPE_EXTENDED:
                    if (   uPtr.pExt->cbInformation > cbAllocDescs
                        || uPtr.pExt->cbInformation < sizeof(*uPtr.pExt))
                        return VERR_ISOFS_BAD_EXTAD;
                    uType    = uPtr.pExt->uType;
                    cb       = uPtr.pExt->cb;
                    idxBlock = uPtr.pExt->Location.off;
                    idxPart  = uPtr.pExt->Location.uPartitionNo;
                    cbAllocDescs -= uPtr.pExt->cbInformation;
                    uPtr.pb      += uPtr.pExt->cbInformation;
                    break;
                default:
                    AssertFailedReturn(VERR_IPE_NOT_REACHED_DEFAULT_CASE);
            }

            /* Zero length extent terminates the allocation descriptor list. */
            if (cb != 0)
            { /* likely */ }
            else
                break;

            /*
             * Check if we can extend the current extent.  This is useful since
             * the descriptors can typically only cover 1GB each.
             */
            uint64_t const off = (uint64_t)idxBlock << pVolInfo->cShiftBlock;
            if (   pCurExtent != NULL
                && (   pCurExtent->off != UINT64_MAX
                    ?     uType == UDF_AD_TYPE_RECORDED_AND_ALLOCATED
                       && pCurExtent->off + pCurExtent->cbExtent == off
                       && pCurExtent->idxPart == idxPart
                    :     uType == UDF_AD_TYPE_FREE
                       || uType == UDF_AD_TYPE_ONLY_ALLOCATED) )
            {
                pCurExtent->cbExtent += cb;
                offData += cb;
            }
            else if (uType != UDF_AD_TYPE_NEXT)
            {
                /*
                 * Allocate a new descriptor entry.
                 */
                if (cExtents == 0)
                {
                    cExtents   = 1;
                    pCurExtent = pFirstExtent;
                }
                else if (cExtents < _64K && (offData < cbMax || (cExtents < 8 && offData < 16 * _1T)))
                {
                    void *pvNew = RTMemRealloc(paExtents, cExtents * sizeof(paExtents[0]));
                    if (pvNew)
                        paExtents = (PRTFSISOEXTENT)pvNew;
                    else if (offData > cbMax)
                    {
                        Log(("RTFsUdfHlpGatherExtentsFromIcb: offData=%#RX64 > cbMax=%#RX64 - ignore out of memory\n", offData, cbMax));
                        cbAllocDescs = 0;
                        break;
                    }
                    else
                    {
                        RTMemFree(paExtents);
                        return VERR_NO_MEMORY;
                    }
                    pCurExtent = &paExtents[cExtents - 1];
                    cExtents++;
                }
                else if (offData >= cbMax)
                {
                    Log(("RTFsUdfHlpGatherExtentsFromIcb: offData=%#RX64 > cbMax=%#RX64 - stop gathering more extents\n", offData, cbMax));
                    cbAllocDescs = 0;
                    break;
                }
                else
                {
                    LogRelMax(64, ("RTFsUdfHlpGatherExtentsFromIcb: too many file fragment! offData=%#RX64\n", offData));
                    RTMemFree(paExtents);
                    return VERR_ISOFS_TOO_MANY_FILE_FRAGMENTS;
                }

                /*
                 * Initialize the new entry.
                 */
                if (uType == UDF_AD_TYPE_RECORDED_AND_ALLOCATED)
                {
                    pCurExtent->off     = off;
                    pCurExtent->idxPart = idxPart;
                }
                else
                {
                    pCurExtent->off     = UINT64_MAX;
                    pCurExtent->idxPart = UINT32_MAX;
                }
                pCurExtent->cbExtent    = cb;
                pCurExtent->uReserved   = 0;
                offData += cb;
            }
            else
            {
                /*
                 * UDF_AD_TYPE_NEXT - read in the continuation block and validate it, and restart the loop.
                 */
                Log(("RTFsUdfHlpGatherExtentsFromIcb: UDF_AD_TYPE_NEXT: %u/%#RX64/%#RX32 LB %#x\n",
                     idxPart, (uint64_t)idxBlock << pVolInfo->cShiftBlock, idxBlock, cb));
                if (offData > cbMax)
                {
                    Log(("RTFsUdfHlpGatherExtentsFromIcb: offData=%#RX64 > cbMax=%#RX64 - skip UDF_AD_TYPE_NEXT\n", offData, cbMax));
                    cbAllocDescs = 0;
                    break;
                }
                int rc;
                if (   cb <= pVolInfo->cbBlock
                    && cb > RT_UOFFSETOF(UDFALLOCATIONEXTENTDESC, u))
                {
                    rc = RTFsUdfHlpVpRead(pVolInfo, hVfsBacking, idxPart, idxBlock, pbBuf, cb);
                    if (RT_SUCCESS(rc))
                    {
                        if (cb < pVolInfo->cbBlock)
                            RT_BZERO(&pbBuf[cb], pVolInfo->cbBlock - cb);

                        PCUDFALLOCATIONEXTENTDESC pDesc = (PCUDFALLOCATIONEXTENTDESC)pbBuf;
                        rc = RTFsUdfHlpValidateDescTagAndCrc(&pDesc->Tag, pVolInfo->cbBlock, UDF_TAG_ID_ALLOCATION_EXTENT_DESC,
                                                             idxBlock, NULL);
                        if (RT_SUCCESS(rc))
                        {
                            if (   pDesc->cbAllocDescs >= cbOneDesc
                                && pDesc->cbAllocDescs >= pVolInfo->cbBlock - RT_UOFFSETOF(UDFALLOCATIONEXTENTDESC, u))
                            {
                                cbAllocDescs = pDesc->cbAllocDescs;
                                pbAllocDescs = (uint8_t const *)&pDesc->u;
                                uPtr.pb      = pbAllocDescs;
                                Log(("RTFsUdfHlpGatherExtentsFromIcb: Restarting loop with next block: cbAllocDescs=%#x\n", cbAllocDescs));
                                continue;
                            }

                            LogRelMax(45, ("ISO/UDF: Bogus UDFALLOCATIONEXTENTDESC::cbAllocDescs: %#x\n", pDesc->cbAllocDescs));
                            rc = VERR_ISOFS_BOGUS_ALLOCATION_EXTENT_LENGTH;
                        }
                    }
                }
                else
                {
                    LogRelMax(45, ("ISO/UDF: Bogus UDF_AD_TYPE_NEXT length: %#x\n", cb));
                    rc = VERR_ISOFS_BOGUS_NEXT_AD_LENGTH;
                }
                RTMemFree(paExtents);
                return rc;
            }
        } while (cbAllocDescs >= cbOneDesc);

        if (cbAllocDescs > 0)
            LogRelMax(45,("ISO/UDF: Warning! %u bytes left in allocation descriptor: %.*Rhxs\n", cbAllocDescs, cbAllocDescs, uPtr.pb));

        *pcExtents  = cExtents;
        *ppaExtents = paExtents;
    }
    else
    {
        /*
         * Zero descriptors
         */
        if (cbAllocDescs > 0)
            LogRelMax(45, ("ISO/UDF: Warning! Allocation descriptor area is shorter than one descriptor: %#u vs %#u: %.*Rhxs\n",
                           cbAllocDescs, cbOneDesc, cbAllocDescs, pbAllocDescs));
    }
    return VINF_SUCCESS;
}


/**
 * Frees the array of additional extents returned by
 * RTFsUdfHlpGatherExtentsFromIcb.
 */
DECLHIDDEN(void) RTFsUdfHlpFreeGatherExtents(PRTFSISOEXTENT paExtents)
{
    if (paExtents)
        RTMemFree(paExtents);
}


/**
 * Converts ICB flags, ICB file type and file entry permissions to an IPRT file
 * mode mask.
 *
 * @returns IPRT status ocde
 * @param   fIcbTagFlags    The ICB flags.
 * @param   bFileType       The ICB file type.
 * @param   fPermission     The file entry permission mask.
 * @param   pfAttrib        Where to return the IRPT file mode mask.
 */
DECLHIDDEN(int) RTFsUdfHlpIcbStuffToFileMode(uint32_t fIcbTagFlags, uint8_t bFileType, uint32_t fPermission, PRTFMODE pfAttrib)
{
    /*
     * Type:
     */
    RTFMODE fAttrib;
    switch (bFileType)
    {
        case UDF_FILE_TYPE_DIRECTORY:
            fAttrib = RTFS_TYPE_DIRECTORY | RTFS_DOS_DIRECTORY;
            break;

        case UDF_FILE_TYPE_REGULAR_FILE:
        case UDF_FILE_TYPE_REAL_TIME_FILE:
            fAttrib = RTFS_TYPE_FILE;
            break;

        case UDF_FILE_TYPE_SYMBOLIC_LINK:
            fAttrib = RTFS_TYPE_SYMLINK;
            break;

        case UDF_FILE_TYPE_BLOCK_DEVICE:
            fAttrib = RTFS_TYPE_DEV_BLOCK;
            break;
        case UDF_FILE_TYPE_CHARACTER_DEVICE:
            fAttrib = RTFS_TYPE_DEV_CHAR;
            break;

        case UDF_FILE_TYPE_FIFO:
            fAttrib = RTFS_TYPE_FIFO;
            break;

        case UDF_FILE_TYPE_SOCKET:
            fAttrib = RTFS_TYPE_SOCKET;
            break;

        case UDF_FILE_TYPE_STREAM_DIRECTORY:
        case UDF_FILE_TYPE_EXTENDED_ATTRIBUTES:
        case UDF_FILE_TYPE_TERMINAL_ENTRY:
        case UDF_FILE_TYPE_VAT:
        case UDF_FILE_TYPE_METADATA_FILE:
        case UDF_FILE_TYPE_METADATA_MIRROR_FILE:
        case UDF_FILE_TYPE_METADATA_BITMAP_FILE:
        case UDF_FILE_TYPE_NOT_SPECIFIED:
        case UDF_FILE_TYPE_INDIRECT_ENTRY:
        case UDF_FILE_TYPE_UNALLOCATED_SPACE_ENTRY:
        case UDF_FILE_TYPE_PARTITION_INTEGRITY_ENTRY:
            LogRelMax(45, ("ISO/UDF: Warning! Wrong file type: %#x\n", bFileType));
            return VERR_ISOFS_WRONG_FILE_TYPE;

        default:
            LogRelMax(45, ("ISO/UDF: Warning! Unknown file type: %#x\n", bFileType));
            return VERR_ISOFS_UNKNOWN_FILE_TYPE;
    }

    /*
     * Permissions:
     */
    if (fPermission & UDF_PERM_OTH_EXEC)
        fAttrib |= RTFS_UNIX_IXOTH;
    if (fPermission & UDF_PERM_OTH_READ)
        fAttrib |= RTFS_UNIX_IROTH;
    if (fPermission & UDF_PERM_OTH_WRITE)
        fAttrib |= RTFS_UNIX_IWOTH;

    if (fPermission & UDF_PERM_GRP_EXEC)
        fAttrib |= RTFS_UNIX_IXGRP;
    if (fPermission & UDF_PERM_GRP_READ)
        fAttrib |= RTFS_UNIX_IRGRP;
    if (fPermission & UDF_PERM_GRP_WRITE)
        fAttrib |= RTFS_UNIX_IWGRP;

    if (fPermission & UDF_PERM_USR_EXEC)
        fAttrib |= RTFS_UNIX_IXUSR;
    if (fPermission & UDF_PERM_USR_READ)
        fAttrib |= RTFS_UNIX_IRUSR;
    if (fPermission & UDF_PERM_USR_WRITE)
        fAttrib |= RTFS_UNIX_IWUSR;

    if (   !(fAttrib & (UDF_PERM_OTH_WRITE | UDF_PERM_GRP_WRITE | UDF_PERM_USR_WRITE))
        && (fAttrib & (UDF_PERM_OTH_READ | UDF_PERM_GRP_READ | UDF_PERM_USR_READ)) )
        fAttrib |= RTFS_DOS_READONLY;

    /*
     * Attributes:
     */
    if (fIcbTagFlags & UDF_ICB_FLAGS_ARCHIVE)
        fAttrib |= RTFS_DOS_ARCHIVED;
    if (fIcbTagFlags & UDF_ICB_FLAGS_SYSTEM)
        fAttrib |= RTFS_DOS_SYSTEM;
    if (fIcbTagFlags & UDF_ICB_FLAGS_ARCHIVE)
        fAttrib |= RTFS_DOS_ARCHIVED;

    if (fIcbTagFlags & UDF_ICB_FLAGS_SET_UID)
        fAttrib |= RTFS_UNIX_ISUID;
    if (fIcbTagFlags & UDF_ICB_FLAGS_SET_GID)
        fAttrib |= RTFS_UNIX_ISGID;
    if (fIcbTagFlags & UDF_ICB_FLAGS_STICKY)
        fAttrib |= RTFS_UNIX_ISTXT;

    /* Warn about weird flags. */
    if (fIcbTagFlags & UDF_ICB_FLAGS_TRANSFORMED)
        LogRelMax(45, ("ISO/UDF: Warning! UDF_ICB_FLAGS_TRANSFORMED!\n"));
    if (fIcbTagFlags & UDF_ICB_FLAGS_MULTI_VERSIONS)
        LogRelMax(45, ("ISO/UDF: Warning! UDF_ICB_FLAGS_MULTI_VERSIONS!\n"));
    if (fIcbTagFlags & UDF_ICB_FLAGS_STREAM)
        LogRelMax(45, ("ISO/UDF: Warning! UDF_ICB_FLAGS_STREAM!\n"));
    if (fIcbTagFlags & UDF_ICB_FLAGS_RESERVED_MASK)
        LogRelMax(45, ("ISO/UDF: Warning! UDF_ICB_FLAGS_RESERVED_MASK (%#x)!\n", fIcbTagFlags & UDF_ICB_FLAGS_RESERVED_MASK));

    *pfAttrib = fAttrib;
    return VINF_SUCCESS;
}

#ifdef LOG_ENABLED

/**
 * Translates UDFICBTAG::bFileType to a string for logging purposes.
 */
static const char *rtFsUdfHlpIcbFileTypeToString(uint8_t bFileType)
{
    switch (bFileType)
    {
        RT_CASE_RET_STR(UDF_FILE_TYPE_NOT_SPECIFIED);
        RT_CASE_RET_STR(UDF_FILE_TYPE_UNALLOCATED_SPACE_ENTRY);
        RT_CASE_RET_STR(UDF_FILE_TYPE_PARTITION_INTEGRITY_ENTRY);
        RT_CASE_RET_STR(UDF_FILE_TYPE_INDIRECT_ENTRY);
        RT_CASE_RET_STR(UDF_FILE_TYPE_DIRECTORY);
        RT_CASE_RET_STR(UDF_FILE_TYPE_REGULAR_FILE);
        RT_CASE_RET_STR(UDF_FILE_TYPE_BLOCK_DEVICE);
        RT_CASE_RET_STR(UDF_FILE_TYPE_CHARACTER_DEVICE);
        RT_CASE_RET_STR(UDF_FILE_TYPE_EXTENDED_ATTRIBUTES);
        RT_CASE_RET_STR(UDF_FILE_TYPE_FIFO);
        RT_CASE_RET_STR(UDF_FILE_TYPE_SOCKET);
        RT_CASE_RET_STR(UDF_FILE_TYPE_TERMINAL_ENTRY);
        RT_CASE_RET_STR(UDF_FILE_TYPE_SYMBOLIC_LINK);
        RT_CASE_RET_STR(UDF_FILE_TYPE_STREAM_DIRECTORY);
        RT_CASE_RET_STR(UDF_FILE_TYPE_VAT);
        RT_CASE_RET_STR(UDF_FILE_TYPE_REAL_TIME_FILE);
        RT_CASE_RET_STR(UDF_FILE_TYPE_METADATA_FILE);
        RT_CASE_RET_STR(UDF_FILE_TYPE_METADATA_MIRROR_FILE);
        RT_CASE_RET_STR(UDF_FILE_TYPE_METADATA_BITMAP_FILE);
        default: return "??";
    }
}


/**
 * Logs the extended embedded attributes in a file record.
 */
static void rtFsUdfHlpLogExtAttribs(uint8_t const *pbExtAttribs, size_t cbExtAttribs)
{
    PCUDFEXTATTRIBHDRDESC const pHdr = (PCUDFEXTATTRIBHDRDESC)pbExtAttribs;
    if (   cbExtAttribs >= sizeof(UDFEXTATTRIBHDRDESC)
        && pHdr->Tag.idTag == UDF_TAG_ID_EXTENDED_ATTRIB_HDR_DESC)
    {
        UDF_LOG2_MEMBER2("#06RX16", "EA.Hdr.Tag.uVersion", pHdr->Tag.uVersion);
        UDF_LOG2_MEMBER2("#06RX16", "EA.Hdr.Tag.cbDescriptorCrc", pHdr->Tag.cbDescriptorCrc);
        UDF_LOG2_MEMBER2("#010RX32", "EA.Hdr.offImplementationAttribs", pHdr->offImplementationAttribs);
        UDF_LOG2_MEMBER2("#010RX32", "EA.Hdr.offApplicationAttribs", pHdr->offApplicationAttribs);
        if (cbExtAttribs > RT_UOFFSETOF(UDFGEA, u))
        {
            PCUDFGEA const pGenEA  = (PCUDFGEA)(pHdr + 1);
            UDF_LOG2_MEMBER2("#010RX32", "EA.uAttribType", pGenEA->uAttribType);
            UDF_LOG2_MEMBER2("#04RX8",  "EA.uAttribSubtype", pGenEA->uAttribSubtype);
            if (pGenEA->abReserved[0] || pGenEA->abReserved[1], pGenEA->abReserved[2])
                UDF_LOG2_MEMBER2(".3Rhxs",  "EA.abReserved", pGenEA->abReserved);
            UDF_LOG2_MEMBER2("#010RX32", "EA.cbAttrib", pGenEA->cbAttrib);

            size_t const cbMaxEA = cbExtAttribs - RT_UOFFSETOF(UDFGEA, u);
            size_t const cbEA    = RT_MIN(RT_MAX(pGenEA->cbAttrib, RT_UOFFSETOF(UDFGEA, u)) - RT_UOFFSETOF(UDFGEA, u), cbMaxEA);
            switch (pGenEA->uAttribType)
            {
                case UDFEADATACHARSETINFO_ATTRIB_TYPE:
                    if (   cbEA < RT_UOFFSETOF(UDFEADATACHARSETINFO, abEscSeqs)
                        || cbEA - RT_UOFFSETOF(UDFEADATACHARSETINFO, abEscSeqs) < pGenEA->u.CharSetInfo.cbEscSeqs)
                        break;
                    UDF_LOG2_MEMBER2("#010RX32",  "EA.CharSetInfo.cbEscSeqs", pGenEA->u.CharSetInfo.cbEscSeqs);
                    UDF_LOG2_MEMBER2("#04RX8",  "EA.CharSetInfo.bType", pGenEA->u.CharSetInfo.bType);
                    UDF_LOG2_MEMBER2_HEX(pGenEA->u.CharSetInfo.cbEscSeqs, "EA.CharSetInfo.abEscSeqs", &pGenEA->u.CharSetInfo.abEscSeqs);
                    return;
                case UDFEADATAALTPERM_ATTRIB_TYPE:
                    if (cbEA < sizeof(pGenEA->u.AltPerm))
                        break;
                    UDF_LOG2_MEMBER2("#06RX16",  "EA.AltPerm.idOwner", pGenEA->u.AltPerm.idOwner);
                    UDF_LOG2_MEMBER2("#06RX16",  "EA.AltPerm.idGroup", pGenEA->u.AltPerm.idGroup);
                    UDF_LOG2_MEMBER2("#06RX16",  "EA.AltPerm.fPermission", pGenEA->u.AltPerm.fPermission);
                    return;
                case UDFEADATAFILETIMES_ATTRIB_TYPE:
                    if (   cbEA < RT_UOFFSETOF(UDFEADATAFILETIMES, aTimestamps)
                        || cbEA - RT_UOFFSETOF(UDFEADATAFILETIMES, aTimestamps) < pGenEA->u.FileTimes.cbTimestamps)
                        break;
                    UDF_LOG2_MEMBER2("#010RX32",  "EA.FileTimes.cbTimestamps", pGenEA->u.FileTimes.cbTimestamps);
                    UDF_LOG2_MEMBER2("#010RX32",  "EA.FileTimes.fFlags", pGenEA->u.FileTimes.fFlags);
                    for (uint32_t iTs = 0, cTs = pGenEA->u.FileTimes.cbTimestamps / sizeof(UDFTIMESTAMP); iTs < cTs; iTs++)
                        UDF_LOG2_MEMBER_TIMESTAMP2("EA.FileTimes.aTimestamps[]", pGenEA->u.FileTimes.aTimestamps[iTs]);
                    return;
                case UDFEADATAINFOTIMES_ATTRIB_TYPE:
                    if (   cbEA < RT_UOFFSETOF(UDFEADATAINFOTIMES, aTimestamps)
                        || cbEA - RT_UOFFSETOF(UDFEADATAINFOTIMES, aTimestamps) < pGenEA->u.InfoTimes.cbTimestamps)
                        break;
                    UDF_LOG2_MEMBER2("#010RX32",  "EA.InfoTimes.cbTimestamps", pGenEA->u.InfoTimes.cbTimestamps);
                    UDF_LOG2_MEMBER2("#010RX32",  "EA.InfoTimes.fFlags", pGenEA->u.InfoTimes.fFlags);
                    for (uint32_t iTs = 0, cTs = pGenEA->u.InfoTimes.cbTimestamps / sizeof(UDFTIMESTAMP); iTs < cTs; iTs++)
                        UDF_LOG2_MEMBER_TIMESTAMP2("EA.InfoTimes.aTimestamps[]", pGenEA->u.InfoTimes.aTimestamps[iTs]);
                    return;
                case UDFEADATADEVICESPEC_ATTRIB_TYPE:
                    if (cbEA < RT_UOFFSETOF(UDFEADATADEVICESPEC, abImplementationUse))
                        break;
                    UDF_LOG2_MEMBER2("#010RX32",  "EA.DeviceSpec.cbImplementationUse", pGenEA->u.DeviceSpec.cbImplementationUse);
                    UDF_LOG2_MEMBER2("#010RX32",  "EA.DeviceSpec.uMajorDeviceNo", pGenEA->u.DeviceSpec.uMajorDeviceNo);
                    UDF_LOG2_MEMBER2("#010RX32",  "EA.DeviceSpec.uMinorDeviceNo", pGenEA->u.DeviceSpec.uMinorDeviceNo);
                    if (cbEA != RT_UOFFSETOF(UDFEADATADEVICESPEC, abImplementationUse))
                        return;
                    if (cbEA - RT_UOFFSETOF(UDFEADATADEVICESPEC, abImplementationUse) < pGenEA->u.DeviceSpec.cbImplementationUse)
                        break;
                    UDF_LOG2_MEMBER2_HEX(pGenEA->u.DeviceSpec.cbImplementationUse, "EA.DeviceSpec.cbImplementationUse:",
                                         pGenEA->u.DeviceSpec.abImplementationUse);
                    return;
                case UDFEADATAIMPLUSE_ATTRIB_TYPE:
                    break;
                case UDFEADATAAPPUSE_ATTRIB_TYPE:
                    break;
            }
            UDF_LOG2_MEMBER2_HEX(cbEA, "EA.abData:", pGenEA->u.abData);
        }
        else if (cbExtAttribs > sizeof(UDFEXTATTRIBHDRDESC))
            UDF_LOG2_MEMBER2_HEX(cbExtAttribs - sizeof(*pHdr), "EA.UnknownPayload", pHdr + 1);
    }
    else
        UDF_LOG2_MEMBER2_HEX(cbExtAttribs, "abExtAttribs", pbExtAttribs);
}


/**
 * Logs the content of an extended UDF file entry.
 *
 * @returns IPRT status code
 * @param   pFileEntry      The file entry.
 * @param   cbBlock         The logicl block size.
 */
static void rtFsUdfHlpLogIcbExFileEntry(PCUDFEXFILEENTRY pFileEntry, uint32_t cbBlock)
{
    /*
     * Log it.
     */
    if (LogIs2Enabled())
    {
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32",  IcbTag.cEntiresBeforeThis);
        UDF_LOG2_MEMBER(pFileEntry, "#06RX16",  IcbTag.uStrategyType);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8",   IcbTag.abStrategyParams[0]);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8",   IcbTag.abStrategyParams[1]);
        UDF_LOG2_MEMBER(pFileEntry, "#06RX16",  IcbTag.cMaxEntries);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8",   IcbTag.bReserved);
        UDF_LOG2_MEMBER_ENUM(pFileEntry, "#04RX8", IcbTag.bFileType, rtFsUdfHlpIcbFileTypeToString);
        UDF_LOG2_MEMBER_LBADDR(pFileEntry,      IcbTag.ParentIcb);
        UDF_LOG2_MEMBER(pFileEntry, "#06RX16",  IcbTag.fFlags);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", uid);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", gid);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", fPermissions);
        UDF_LOG2_MEMBER(pFileEntry, "#06RX16", cHardlinks);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8", uRecordFormat);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8", fRecordDisplayAttribs);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", cbRecord);
        UDF_LOG2_MEMBER(pFileEntry, "#018RX64", cbData);
        UDF_LOG2_MEMBER(pFileEntry, "#018RX64", cbObject);
        UDF_LOG2_MEMBER(pFileEntry, "#018RX64", cLogicalBlocks);
        UDF_LOG2_MEMBER_TIMESTAMP(pFileEntry, AccessTime);
        UDF_LOG2_MEMBER_TIMESTAMP(pFileEntry, ModificationTime);
        UDF_LOG2_MEMBER_TIMESTAMP(pFileEntry, BirthTime);
        UDF_LOG2_MEMBER_TIMESTAMP(pFileEntry, ChangeTime);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", uCheckpoint);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", uReserved);
        UDF_LOG2_MEMBER_LONGAD(pFileEntry, ExtAttribIcb);
        UDF_LOG2_MEMBER_LONGAD(pFileEntry, StreamDirIcb);
        UDF_LOG2_MEMBER_ENTITY_ID(pFileEntry, idImplementation);
        UDF_LOG2_MEMBER(pFileEntry, "#018RX64", INodeId);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", cbExtAttribs);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", cbAllocDescs);
        uint32_t const cbExtAttribs = RT_MIN(pFileEntry->cbExtAttribs, cbBlock - RT_UOFFSETOF(UDFEXFILEENTRY, abExtAttribs));
        if (cbExtAttribs > 0)
            rtFsUdfHlpLogExtAttribs(pFileEntry->abExtAttribs, cbExtAttribs);
        if ((pFileEntry->IcbTag.fFlags & UDF_ICB_FLAGS_AD_TYPE_MASK) == UDF_ICB_FLAGS_AD_TYPE_EMBEDDED)
            Log2(("ISO/UDF:   %-32s @ICB offset %#x LB %#x/%#x%s\n",
                  "EmbeddedAD:", RT_UOFFSETOF(UDFEXFILEENTRY, abExtAttribs) + cbExtAttribs, pFileEntry->cbAllocDescs,
                  pFileEntry->cbData, pFileEntry->cbAllocDescs == pFileEntry->cbData ? "" : " - cbAllocDescs != cbData !!"));
        else if (pFileEntry->cbAllocDescs > 0)
            switch (pFileEntry->IcbTag.fFlags & UDF_ICB_FLAGS_AD_TYPE_MASK)
            {
                case UDF_ICB_FLAGS_AD_TYPE_SHORT:
                {
                    PCUDFSHORTAD paDescs = (PCUDFSHORTAD)&pFileEntry->abExtAttribs[cbExtAttribs];
                    uint32_t     cDescs  = pFileEntry->cbAllocDescs / sizeof(paDescs[0]);
                    for (uint32_t i = 0; i < cDescs; i++)
                        Log2(("ISO/UDF:   ShortAD[%u]:                      %#010RX32 LB %#010RX32; type=%u\n",
                              i, paDescs[i].off, paDescs[i].cb, paDescs[i].uType));
                    break;
                }
                case UDF_ICB_FLAGS_AD_TYPE_LONG:
                {
                    PCUDFLONGAD  paDescs = (PCUDFLONGAD)&pFileEntry->abExtAttribs[cbExtAttribs];
                    uint32_t     cDescs  = pFileEntry->cbAllocDescs / sizeof(paDescs[0]);
                    for (uint32_t i = 0; i < cDescs; i++)
                        Log2(("ISO/UDF:   LongAD[%u]:                       %#06RX16:%#010RX32 LB %#010RX32; type=%u iu=%.6Rhxs\n",
                              i, paDescs[i].Location.uPartitionNo, paDescs[i].Location.off,
                              paDescs[i].cb, paDescs[i].uType, &paDescs[i].ImplementationUse));
                    break;
                }
                default:
                    Log2(("ISO/UDF:   %-32s Type=%u\n%.*RhxD\n",
                          "abExtAttribs:", pFileEntry->IcbTag.fFlags & UDF_ICB_FLAGS_AD_TYPE_MASK,
                          pFileEntry->cbAllocDescs, &pFileEntry->abExtAttribs[cbExtAttribs]));
                    break;
            }
    }
}


/**
 * Logs the content of an UDF file entry.
 *
 * @returns IPRT status code
 * @param   pFileEntry      The file entry.
 * @param   cbBlock         The logicl block size.
 */
static void rtFsUdfHlpLogIcbFileEntry(PCUDFFILEENTRY pFileEntry, uint32_t cbBlock)
{
    /*
     * Log it.
     */
    if (LogIs2Enabled())
    {
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32",  IcbTag.cEntiresBeforeThis);
        UDF_LOG2_MEMBER(pFileEntry, "#06RX16",  IcbTag.uStrategyType);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8",   IcbTag.abStrategyParams[0]);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8",   IcbTag.abStrategyParams[1]);
        UDF_LOG2_MEMBER(pFileEntry, "#06RX16",  IcbTag.cMaxEntries);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8",   IcbTag.bReserved);
        UDF_LOG2_MEMBER_ENUM(pFileEntry, "#04RX8", IcbTag.bFileType, rtFsUdfHlpIcbFileTypeToString);
        UDF_LOG2_MEMBER_LBADDR(pFileEntry,      IcbTag.ParentIcb);
        UDF_LOG2_MEMBER(pFileEntry, "#06RX16",  IcbTag.fFlags);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", uid);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", gid);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", fPermissions);
        UDF_LOG2_MEMBER(pFileEntry, "#06RX16", cHardlinks);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8", uRecordFormat);
        UDF_LOG2_MEMBER(pFileEntry, "#04RX8", fRecordDisplayAttribs);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", cbRecord);
        UDF_LOG2_MEMBER(pFileEntry, "#018RX64", cbData);
        UDF_LOG2_MEMBER(pFileEntry, "#018RX64", cLogicalBlocks);
        UDF_LOG2_MEMBER_TIMESTAMP(pFileEntry, AccessTime);
        UDF_LOG2_MEMBER_TIMESTAMP(pFileEntry, ModificationTime);
        UDF_LOG2_MEMBER_TIMESTAMP(pFileEntry, ChangeTime);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", uCheckpoint);
        UDF_LOG2_MEMBER_LONGAD(pFileEntry, ExtAttribIcb);
        UDF_LOG2_MEMBER_ENTITY_ID(pFileEntry, idImplementation);
        UDF_LOG2_MEMBER(pFileEntry, "#018RX64", INodeId);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", cbExtAttribs);
        UDF_LOG2_MEMBER(pFileEntry, "#010RX32", cbAllocDescs);
        uint32_t const cbExtAttribs = RT_MIN(pFileEntry->cbExtAttribs, cbBlock - RT_UOFFSETOF(UDFFILEENTRY, abExtAttribs));
        if (cbExtAttribs > 0)
            rtFsUdfHlpLogExtAttribs(pFileEntry->abExtAttribs, cbExtAttribs);
        if ((pFileEntry->IcbTag.fFlags & UDF_ICB_FLAGS_AD_TYPE_MASK) == UDF_ICB_FLAGS_AD_TYPE_EMBEDDED)
            Log2(("ISO/UDF:   %-32s @ICB offset %#x LB %#x/%#x%s\n",
                  "EmbeddedAD:", RT_UOFFSETOF(UDFFILEENTRY, abExtAttribs) + cbExtAttribs, pFileEntry->cbAllocDescs,
                  pFileEntry->cbData, pFileEntry->cbAllocDescs == pFileEntry->cbData ? "" : " - cbAllocDescs != cbData !!"));
        else if (pFileEntry->cbAllocDescs > 0)
            switch (pFileEntry->IcbTag.fFlags & UDF_ICB_FLAGS_AD_TYPE_MASK)
            {
                case UDF_ICB_FLAGS_AD_TYPE_SHORT:
                {
                    PCUDFSHORTAD paDescs = (PCUDFSHORTAD)&pFileEntry->abExtAttribs[cbExtAttribs];
                    uint32_t     cDescs  = pFileEntry->cbAllocDescs / sizeof(paDescs[0]);
                    for (uint32_t i = 0; i < cDescs; i++)
                        Log2(("ISO/UDF:   ShortAD[%u]:                      %#010RX32 LB %#010RX32; type=%u\n",
                              i, paDescs[i].off, paDescs[i].cb, paDescs[i].uType));
                    break;
                }
                case UDF_ICB_FLAGS_AD_TYPE_LONG:
                {
                    PCUDFLONGAD  paDescs = (PCUDFLONGAD)&pFileEntry->abExtAttribs[cbExtAttribs];
                    uint32_t     cDescs  = pFileEntry->cbAllocDescs / sizeof(paDescs[0]);
                    for (uint32_t i = 0; i < cDescs; i++)
                        Log2(("ISO/UDF:   LongAD[%u]:                       %#06RX16:%#010RX32 LB %#010RX32; type=%u iu=%.6Rhxs\n",
                              i, paDescs[i].Location.uPartitionNo, paDescs[i].Location.off,
                              paDescs[i].cb, paDescs[i].uType, &paDescs[i].ImplementationUse));
                    break;
                }
                default:
                    Log2(("ISO/UDF:   %-32s Type=%u\n%.*RhxD\n",
                          "abExtAttribs:", pFileEntry->IcbTag.fFlags & UDF_ICB_FLAGS_AD_TYPE_MASK,
                          pFileEntry->cbAllocDescs, &pFileEntry->abExtAttribs[cbExtAttribs]));
                    break;
            }
    }
}

#endif /* LOG_ENABLED */

/**
 * Recursive helper for rtFsIsoCore_InitFromUdfIcbAndFileIdDesc.
 *
 * @returns IRPT status code.
 * @param   pVolInfo        The volume information (partition map ++).
 * @param   hVfsBacking     The backing file/device/whatever.
 * @param   pbBuf           The buffer, one logical block in size.
 * @param   cNestings       The number of recursive nestings (should be zero).
 * @param   pfnFileEntry    Callback for direct ICBs with a file entry.
 * @param   pfnExFileEntry  Callback for direct ICB with an extended file entry.
 * @param   pvUser          Callback user parameters.
 * @param   pcProcessed     Variable to update when we've processed something
 *                          useful.
 * @param   pcIndirections  Variable tracing the number of indirections we've
 *                          taken during the processing.  This is used to
 *                          prevent us from looping forever on a bad chain
 * @param   AllocDesc       The ICB allocation descriptor.  (This is a stack
 *                          copy so we can safely reuse the buffer.)
 */
DECLHIDDEN(int) RTFsUdfReadIcbRecursive(PCRTFSUDFVOLINFO pVolInfo, RTVFSFILE hVfsBacking, uint8_t *pbBuf, uint32_t cNestings,
                                        PFNFSUDFREADICBFILENTRY pfnFileEntry, PFNFSUDFREADICBEXFILENTRY pfnExFileEntry,
                                        void *pvUser, uint32_t *pcProcessed, uint32_t *pcIndirections, UDFLONGAD AllocDesc)
{
    if (cNestings >= 8)
        return VERR_ISOFS_TOO_DEEP_ICB_RECURSION;

    for (;;)
    {
        if (*pcIndirections >= 32)
            return VERR_ISOFS_TOO_MANY_ICB_INDIRECTIONS;

        /*
         * Check the basic validity of the allocation descriptor.
         */
        if (   AllocDesc.uType == UDF_AD_TYPE_RECORDED_AND_ALLOCATED
            && AllocDesc.cb >= sizeof(UDFICBTAG) )
        { /* likely */ }
        else if (AllocDesc.uType != UDF_AD_TYPE_RECORDED_AND_ALLOCATED)
        {
            Log(("ISO/UDF: ICB has alloc type %d!\n", AllocDesc.uType));
            return VINF_SUCCESS;
        }
        else
        {
            LogRelMax(45, ("ISO/UDF: ICB is too small: %u bytes\n", AllocDesc.cb));
            return AllocDesc.cb == 0 ? VINF_SUCCESS : VERR_ISOFS_ICB_ENTRY_TOO_SMALL;
        }

        /*
         * Process it block by block.
         */
        uint32_t cBlocks = (AllocDesc.cb + pVolInfo->cbBlock - 1) >> pVolInfo->cShiftBlock;
        for (uint32_t idxBlock = 0; ; idxBlock++)
        {
            /*
             * Read a block
             */
            size_t cbToRead = RT_MIN(pVolInfo->cbBlock, AllocDesc.cb);
            int rc = RTFsUdfHlpVpRead(pVolInfo, hVfsBacking, AllocDesc.Location.uPartitionNo,
                                      AllocDesc.Location.off + idxBlock, pbBuf, cbToRead);
            if (RT_FAILURE(rc))
                return rc;
            if (cbToRead < pVolInfo->cbBlock)
                RT_BZERO(&pbBuf[cbToRead], pVolInfo->cbBlock - cbToRead);

            /*
             * Verify the TAG.
             */
            PUDFICBHDR pHdr = (PUDFICBHDR)pbBuf;
            rc = RTFsUdfHlpValidateDescTagAndCrc(&pHdr->Tag, pVolInfo->cbBlock, UINT16_MAX,
                                                 AllocDesc.Location.off + idxBlock, NULL);
            if (RT_FAILURE(rc))
                return rc;

            /*
             * Do specific processing.
             */
            if (pHdr->Tag.idTag == UDF_TAG_ID_FILE_ENTRY)
            {
                PCUDFFILEENTRY const pFileEntry = (PCUDFFILEENTRY)pHdr;
#ifdef LOG_ENABLED
                rtFsUdfHlpLogIcbFileEntry(pFileEntry, pVolInfo->cbBlock);
#endif
                /* Basic sanity checking. */
                if (   (uint64_t)RT_UOFFSETOF(UDFFILEENTRY, abExtAttribs) + pFileEntry->cbExtAttribs + pFileEntry->cbAllocDescs
                       <= pVolInfo->cbBlock
                    && (pFileEntry->cbExtAttribs & 3) == 0
                    && (   (pFileEntry->cbAllocDescs & 3) == 0
                        || (pFileEntry->IcbTag.fFlags & UDF_ICB_FLAGS_AD_TYPE_MASK) == UDF_ICB_FLAGS_AD_TYPE_EMBEDDED))

                {
                    if (   pFileEntry->uRecordFormat
                        || pFileEntry->fRecordDisplayAttribs
                        || pFileEntry->cbRecord)
                        LogRelMax(45, ("ISO/UDF: uRecordFormat=%#x fRecordDisplayAttribs=%#x cbRecord=%#x\n",
                                       pFileEntry->uRecordFormat, pFileEntry->fRecordDisplayAttribs, pFileEntry->cbRecord));

                    rc = pfnFileEntry(pVolInfo, pFileEntry, AllocDesc.Location.uPartitionNo, pvUser);
                    if (RT_FAILURE(rc))
                        return rc;
                    *pcProcessed += 1;
                }
                else
                {
                    LogRelMax(45, ("ISO/UDF: File entry (ICB) is bad size values: cbAllocDesc=%#x cbExtAttribs=%#x (cbBlock=%#x)\n",
                                   pFileEntry->cbAllocDescs, pFileEntry->cbExtAttribs, pVolInfo->cbBlock));
                    return VERR_ISOFS_BAD_FILE_ENTRY;
                }
            }
            else if (pHdr->Tag.idTag == UDF_TAG_ID_EXTENDED_FILE_ENTRY)
            {
                PCUDFEXFILEENTRY const pFileEntry = (PCUDFEXFILEENTRY)pHdr;
#ifdef LOG_ENABLED
                rtFsUdfHlpLogIcbExFileEntry(pFileEntry, pVolInfo->cbBlock);
#endif
                /* Basic sanity checking. */
                if (   (uint64_t)RT_UOFFSETOF(UDFFILEENTRY, abExtAttribs) + pFileEntry->cbExtAttribs + pFileEntry->cbAllocDescs
                       <= pVolInfo->cbBlock
                    && (pFileEntry->cbExtAttribs & 3) == 0
                    && (   (pFileEntry->cbAllocDescs & 3) == 0
                        || (pFileEntry->IcbTag.fFlags & UDF_ICB_FLAGS_AD_TYPE_MASK) == UDF_ICB_FLAGS_AD_TYPE_EMBEDDED))

                {
                    if (   pFileEntry->uRecordFormat
                        || pFileEntry->fRecordDisplayAttribs
                        || pFileEntry->cbRecord)
                        LogRelMax(45, ("ISO/UDF: uRecordFormat=%#x fRecordDisplayAttribs=%#x cbRecord=%#x\n",
                                       pFileEntry->uRecordFormat, pFileEntry->fRecordDisplayAttribs, pFileEntry->cbRecord));

                    rc = pfnExFileEntry(pVolInfo, pFileEntry, AllocDesc.Location.uPartitionNo, pvUser);
                    if (RT_FAILURE(rc))
                        return rc;
                    *pcProcessed += 1;
                }
                else
                {
                    LogRelMax(45, ("ISO/UDF: File entry (ICB) is bad size values: cbAllocDesc=%#x cbExtAttribs=%#x (cbBlock=%#x)\n",
                                   pFileEntry->cbAllocDescs, pFileEntry->cbExtAttribs, pVolInfo->cbBlock));
                    return VERR_ISOFS_BAD_FILE_ENTRY;
                }
            }
            else if (pHdr->Tag.idTag == UDF_TAG_ID_INDIRECT_ENTRY)
            {
                PUDFINDIRECTENTRY pIndir = (PUDFINDIRECTENTRY)pHdr;
                *pcIndirections += 1;
                if (pIndir->IndirectIcb.cb != 0)
                {
                    if (idxBlock + 1 == cBlocks)
                    {
                        AllocDesc = pIndir->IndirectIcb;
                        Log2(("ISO/UDF: ICB: Indirect entry - looping: %x:%#010RX32 LB %#x; uType=%d\n",
                              AllocDesc.Location.uPartitionNo, AllocDesc.Location.off, AllocDesc.cb, AllocDesc.uType));
                        break;
                    }
                    Log2(("ISO/UDF: ICB: Indirect entry - recursing: %x:%#010RX32 LB %#x; uType=%d\n",
                          pIndir->IndirectIcb.Location.uPartitionNo, pIndir->IndirectIcb.Location.off,
                          pIndir->IndirectIcb.cb, pIndir->IndirectIcb.uType));
                    rc = RTFsUdfReadIcbRecursive(pVolInfo, hVfsBacking, pbBuf, cNestings + 1, pfnFileEntry, pfnExFileEntry,
                                                 pvUser, pcProcessed, pcIndirections, pIndir->IndirectIcb);
                    if (RT_FAILURE(rc))
                        return rc;
                }
                else
                    Log(("ISO/UDF: zero length indirect entry\n"));
            }
            else if (pHdr->Tag.idTag == UDF_TAG_ID_TERMINAL_ENTRY)
            {
                Log2(("ISO/UDF: Terminal ICB entry\n"));
                return VINF_SUCCESS;
            }
            else if (pHdr->Tag.idTag == UDF_TAG_ID_UNALLOCATED_SPACE_ENTRY)
            {
                Log2(("ISO/UDF: Unallocated space entry: skipping\n"));
                /* Ignore since we don't do writing (UDFUNALLOCATEDSPACEENTRY) */
            }
            else
            {
                LogRelMax(90, ("ISO/UDF: Unknown ICB type %#x\n", pHdr->Tag.idTag));
                return VERR_ISOFS_UNSUPPORTED_ICB;
            }

            /*
             * Advance.
             */
            if (idxBlock + 1 >= cBlocks)
                return VINF_SUCCESS;
        }

        /* If we get here, we've jumped thru an indirect entry. */
    }
    /* never reached */
}


/**
 * Generic UDF object read function.
 *
 * This deals with extent mappings as well as virtual partition related block
 * mapping and such.
 *
 * @returns VBox status code.
 * @param   pVolInfo        The volume information (partition map ++).
 * @param   hVfsBacking     The backing file/device/whatever.
 * @param   cbObject        The actual size of the object being read.
 * @param   cExtents        Number of allocation extents.  Must be at least 1.
 * @param   pFirstExtent    Pointer to the first allocation extent.
 * @param   paFurtherExtents Additional allocation extents.
 * @param   offRead         The offset to start reading at.
 * @param   pvBuf           The output buffer.
 * @param   cbToRead        The number of bytes to read.
 * @param   pcbRead         Where to return the number of bytes read.
 * @param   poffPosMov      Where to return the number of bytes to move the read
 *                          position.  Optional.  (Essentially same as pcbRead
 *                          except without the behavior change.)
 */
DECLHIDDEN(int) RTFsUdfHlpReadObject(PCRTFSUDFVOLINFO pVolInfo, RTVFSFILE hVfsBacking, uint64_t cbObject,
                                     uint32_t cExtents, PCRTFSISOEXTENT pFirstExtent, PCRTFSISOEXTENT paFurtherExtents,
                                     uint64_t offRead, void *pvBuf, size_t cbToRead, size_t *pcbRead, size_t *poffPosMov)
{
    Assert(cExtents > 0);

    /*
     * Check for EOF.
     */
    if (offRead >= cbObject)
    {
        if (poffPosMov)
            *poffPosMov = 0;
        if (pcbRead)
        {
            *pcbRead = 0;
            return VINF_EOF;
        }
        return VERR_EOF;
    }
    int rcRet = VINF_SUCCESS;
    if (   cbToRead           > cbObject
        || offRead + cbToRead > cbObject)
    {
        if (!pcbRead)
        {
            if (poffPosMov)
                *poffPosMov = 0;
            return VERR_EOF;
        }
        cbToRead = cbObject - offRead;
        rcRet = VINF_EOF;
    }

    uint64_t cbActual = 0;

    /*
     * Don't bother looking up the extent if we're not going to
     * read anything from it.
     */
    if (cbToRead > 0)
    {
        /*
         * Locate the first extent.
         */
        uint64_t        offExtent  = 0;
        uint32_t        iExtent    = 0;
        PCRTFSISOEXTENT pCurExtent = pFirstExtent;
        if (offRead < pCurExtent->cbExtent)
        { /* likely */ }
        else
            do
            {
                offExtent += pCurExtent->cbExtent;
                pCurExtent = &paFurtherExtents[iExtent++];
                if (iExtent >= cExtents)
                {
                    memset(pvBuf, 0, cbToRead);

                    if (pcbRead)
                        *pcbRead = cbToRead;
                    if (poffPosMov)
                        *poffPosMov = cbToRead;
                    return rcRet;
                }
            } while (offExtent < offRead);
        Assert(offRead - offExtent < pCurExtent->cbExtent);

        /*
         * Do the reading part.
         */
        for (;;)
        {
            uint64_t offIntoExtent = offRead - offExtent;
            size_t   cbThisRead = pCurExtent->cbExtent - offIntoExtent;
            if (cbThisRead > cbToRead)
                cbThisRead = cbToRead;

            if (pCurExtent->off == UINT64_MAX)
                RT_BZERO(pvBuf, cbThisRead);
            else
            {
                int rc2;
                if (pCurExtent->idxPart == UINT32_MAX)
                    rc2 = RTVfsFileReadAt(hVfsBacking, pCurExtent->off + offIntoExtent, pvBuf, cbThisRead, NULL);
                else
                {
                    if (pCurExtent->idxPart < pVolInfo->cPartitions)
                    {
                        PRTFSISOVOLUDFPMAP const pPart = &pVolInfo->paPartitions[pCurExtent->idxPart];
                        switch (pPart->bType)
                        {
                            case RTFSUDF_PMAP_T_PLAIN:
                                rc2 = RTVfsFileReadAt(hVfsBacking, pPart->offByteLocation + pCurExtent->off + offIntoExtent,
                                                      pvBuf, cbThisRead, NULL);
                                break;

                            default:
                                AssertFailed();
                                rc2 = VERR_ISOFS_IPE_1;
                                break;
                        }
                    }
                    else
                    {
                        Log(("ISO/UDF: Invalid partition index %#x (offset %#RX64), max partitions %#x; iExtent=%#x\n",
                             pCurExtent->idxPart, pCurExtent->off + offIntoExtent, pVolInfo->cPartitions, iExtent));
                        rc2 = VERR_ISOFS_INVALID_PARTITION_INDEX;
                    }
                }
                if (RT_FAILURE(rc2))
                {
                    rcRet = rc2;
                    break;
                }
            }

            /*
             * Advance the buffer position and check if we're done (probable).
             */
            cbActual += cbThisRead;
            cbToRead -= cbThisRead;
            if (!cbToRead)
                break;
            pvBuf = (uint8_t *)pvBuf + cbThisRead;

            /*
             * Advance to the next extent.
             */
            offExtent += pCurExtent->cbExtent;
            pCurExtent = &paFurtherExtents[iExtent++];
            if (iExtent >= cExtents)
            {
                memset(pvBuf, 0, cbToRead);
                cbActual += cbToRead;
                break;
            }
        }
    }
    else
        Assert(rcRet == VINF_SUCCESS);

    if (poffPosMov)
        *poffPosMov = cbActual;
    if (pcbRead)
        *pcbRead = cbActual;
    return rcRet;
}

#if 0 /* maybe later */

/**
 * Locates a directory entry in a directory.
 *
 * @returns IPRT status code.
 * @retval  VERR_FILE_NOT_FOUND if not found.
 * @param   pThis           The directory to search.
 * @param   pszEntry        The entry to look for.
 * @param   ppFid           Where to return the pointer to the file ID entry.
 *                          (Points to the directory content.)
 */
static int rtFsIsoDir_FindEntryUdf(PRTFSISODIRSHRD pThis, const char *pszEntry, PCUDFFILEIDDESC *ppFid)
{
    Assert(pThis->Core.pVol->enmType == RTFSISOVOLTYPE_UDF);
    *ppFid = NULL;

    /*
     * Recode the entry name as 8-bit (if possible) and 16-bit strings.
     * This also disposes of entries that definitely are too long.
     */
    size_t   cb8Bit;
    bool     fSimple;
    size_t   cb16Bit;
    size_t   cwc16Bit;
    uint8_t  ab8Bit[255];
    RTUTF16  wsz16Bit[255];

    /* 16-bit */
    PRTUTF16  pwsz16Bit = wsz16Bit;
    int rc = RTStrToUtf16BigEx(pszEntry, RTSTR_MAX, &pwsz16Bit, RT_ELEMENTS(wsz16Bit), &cwc16Bit);
    if (RT_SUCCESS(rc))
        cb16Bit = 1 + cwc16Bit * sizeof(RTUTF16);
    else
        return rc == VERR_BUFFER_OVERFLOW ? VERR_FILENAME_TOO_LONG : rc;

    /* 8-bit (can't possibly overflow) */
    fSimple = true;
    cb8Bit = 0;
    const char *pszSrc = pszEntry;
    for (;;)
    {
        RTUNICP uc;
        int rc2 = RTStrGetCpEx(&pszSrc, &uc);
        AssertRCReturn(rc2, rc2);
        if (uc <= 0x7f)
        {
            if (uc)
                ab8Bit[cb8Bit++] = (uint8_t)uc;
            else
                break;
        }
        else if (uc <= 0xff)
        {
            ab8Bit[cb8Bit++] = (uint8_t)uc;
            fSimple = false;
        }
        else
        {
            cb8Bit = UINT32_MAX / 2;
            break;
        }
    }
    Assert(cb8Bit <= sizeof(ab8Bit) || cb8Bit == UINT32_MAX / 2);
    cb8Bit++;

    /*
     * Scan the directory content.
     */
    uint32_t        offDesc = 0;
    uint32_t const  cbDir   = pThis->Core.cbObject;
    while (offDesc + RT_UOFFSETOF(UDFFILEIDDESC, abImplementationUse) <= cbDir)
    {
        PCUDFFILEIDDESC pFid  = (PCUDFFILEIDDESC)&pThis->pbDir[offDesc];
        uint32_t const  cbFid = UDFFILEIDDESC_GET_SIZE(pFid);
        if (   offDesc + cbFid <= cbDir
            && pFid->Tag.idTag == UDF_TAG_ID_FILE_ID_DESC)
        { /* likely */ }
        else
            break;

        uint8_t const *pbName = UDFFILEIDDESC_2_NAME(pFid);
        if (*pbName == 16)
        {
            if (cb16Bit == pFid->cbName)
            {
                if (RTUtf16BigNICmp((PCRTUTF16)(&pbName[1]), wsz16Bit, cwc16Bit) == 0)
                {
                    *ppFid = pFid;
                    return VINF_SUCCESS;
                }
            }
        }
        else if (*pbName == 8)
        {
            if (   cb8Bit != UINT16_MAX
                && cb8Bit == pFid->cbName)
            {
                if (fSimple)
                {
                    if (RTStrNICmp((const char *)&pbName[1], (const char *)ab8Bit, cb8Bit - 1) == 0)
                    {
                        *ppFid = pFid;
                        return VINF_SUCCESS;
                    }
                }
                else
                {
                    size_t cch = cb8Bit - 1;
                    size_t off;
                    for (off = 0; off < cch; off++)
                    {
                        RTUNICP uc1 = ab8Bit[off];
                        RTUNICP uc2 = pbName[off + 1];
                        if (   uc1 == uc2
                            || RTUniCpToLower(uc1) == RTUniCpToLower(uc2)
                            || RTUniCpToUpper(uc1) == RTUniCpToUpper(uc2))
                        { /* matches */ }
                        else
                            break;
                    }
                    if (off == cch)
                    {
                        *ppFid = pFid;
                        return VINF_SUCCESS;
                    }
                }
            }
        }

        /* advance */
        offDesc += cbFid;
    }

    return VERR_FILE_NOT_FOUND;
}



/**
 * The UDF worker for rtFsIsoDir_ReadDir
 */
static int rtFsIsoDir_ReadDirUdf(PRTFSISODIROBJ pThis, PRTFSISODIRSHRD pShared, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry,
                                 RTFSOBJATTRADD enmAddAttr)
{
    /*
     * At offset zero we've got the '.' entry.  This has to be generated
     * manually as it's not part of the directory content.  The directory
     * offset has to be faked for this too, so offDir == 0 indicates the '.'
     * entry whereas offDir == 1 is the first file id descriptor.
     */
    if (pThis->offDir == 0)
    {
        if (*pcbDirEntry < RT_UOFFSETOF(RTDIRENTRYEX, szName) + 2)
        {
            *pcbDirEntry = RT_UOFFSETOF(RTDIRENTRYEX, szName) + 2;
            Log3(("rtFsIsoDir_ReadDirUdf: VERR_BUFFER_OVERFLOW (dot)\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        pDirEntry->cbName    = 1;
        pDirEntry->szName[0] = '.';
        pDirEntry->szName[1] = '\0';
        pDirEntry->cwcShortName = 0;
        pDirEntry->wszShortName[0] = '\0';

        int rc = rtFsIsoCore_QueryInfo(&pShared->Core, &pDirEntry->Info, enmAddAttr);

        Log3(("rtFsIsoDir_ReadDirUdf: offDir=%#07x: %s (rc=%Rrc)\n", pThis->offDir, pDirEntry->szName, rc));
        pThis->offDir = 1;
        return rc;
    }

    /*
     * Do the directory content.
     */
    while (pThis->offDir + RT_UOFFSETOF(UDFFILEIDDESC, abImplementationUse) <= pShared->cbDir + 1)
    {
        PCUDFFILEIDDESC pFid  = (PCUDFFILEIDDESC)&pShared->pbDir[pThis->offDir - 1];
        uint32_t const  cbFid = UDFFILEIDDESC_GET_SIZE(pFid);

        if (pThis->offDir + cbFid <= pShared->cbDir + 1)
        { /* likely */ }
        else
            break;

        /*
         * Do names first as they may cause overflows.
         */
        if (pFid->cbName > 1)
        {
            uint8_t const  *pbName = UDFFILEIDDESC_2_NAME(pFid);
            uint32_t        cbSrc  = pFid->cbName;
            if (*pbName == 8)
            {
                /* Figure out the UTF-8 length first. */
                bool     fSimple = true;
                uint32_t cchDst  = 0;
                for (uint32_t offSrc = 1; offSrc < cbSrc; offSrc++)
                    if (!(pbName[offSrc] & 0x80))
                        cchDst++;
                    else
                    {
                        cchDst += 2;
                        fSimple = false;
                    }

                size_t cbNeeded = RT_UOFFSETOF(RTDIRENTRYEX, szName) + cchDst + 1;
                if (*pcbDirEntry >= cbNeeded)
                {
                    if (fSimple)
                    {
                        Assert(cbSrc - 1 == cchDst);
                        memcpy(pDirEntry->szName, &pbName[1], cchDst);
                        pDirEntry->szName[cchDst] = '\0';
                    }
                    else
                    {
                        char *pszDst = pDirEntry->szName;
                        for (uint32_t offSrc = 1; offSrc < cbSrc; offSrc++)
                            pszDst = RTStrPutCp(pszDst, pbName[offSrc]);
                        *pszDst = '\0';
                        Assert((size_t)(pszDst - &pDirEntry->szName[0]) == cchDst);
                    }
                }
                else
                {
                    Log3(("rtFsIsoDir_ReadDirUdf: VERR_BUFFER_OVERFLOW - cbDst=%zu cbNeeded=%zu (8-bit)\n", *pcbDirEntry, cbNeeded));
                    *pcbDirEntry = cbNeeded;
                    return VERR_BUFFER_OVERFLOW;
                }
            }
            else
            {
                /* Let RTUtf16BigToUtf8Ex do the bounds checking. */
                char  *pszDst    = pDirEntry->szName;
                size_t cbDst     = *pcbDirEntry - RT_UOFFSETOF(RTDIRENTRYEX, szName);
                size_t cchNeeded = 0;
                int    rc;
                if (*pbName == 16)
                    rc = RTUtf16BigToUtf8Ex((PCRTUTF16)(pbName + 1), (cbSrc - 1) / sizeof(RTUTF16), &pszDst, cbDst, &cchNeeded);
                else
                    rc = VERR_INVALID_NAME;
                if (RT_SUCCESS(rc))
                    pDirEntry->cbName = (uint16_t)cchNeeded;
                else if (rc == VERR_BUFFER_OVERFLOW)
                {
                    *pcbDirEntry = RT_UOFFSETOF(RTDIRENTRYEX, szName) + cchNeeded + 1;
                    Log3(("rtFsIsoDir_ReadDirUdf: VERR_BUFFER_OVERFLOW - cbDst=%zu cchNeeded=%zu (16-bit)\n", cbDst, cchNeeded));
                    return VERR_BUFFER_OVERFLOW;
                }
                else
                {
                    LogRelMax(90, ("ISO/UDF: Malformed directory entry name at %#x: %.*Rhxs\n", pThis->offDir - 1, cbSrc, pbName));
                    ssize_t cchNeeded2 = RTStrPrintf2(pszDst, cbDst, "bad-name-%#x", pThis->offDir - 1);
                    if (cchNeeded2 >= 0)
                        pDirEntry->cbName = (uint16_t)cchNeeded2;
                    else
                    {
                        *pcbDirEntry = RT_UOFFSETOF(RTDIRENTRYEX, szName) + (size_t)-cchNeeded2;
                        return VERR_BUFFER_OVERFLOW;
                    }
                }
            }
        }
        else if (pFid->fFlags & UDF_FILE_FLAGS_PARENT)
        {
            size_t cbNeeded = RT_UOFFSETOF(RTDIRENTRYEX, szName) + 2 + 1;
            if (*pcbDirEntry < cbNeeded)
            {
                Log3(("rtFsIsoDir_ReadDirUdf: VERR_BUFFER_OVERFLOW - cbDst=%zu cbNeeded=%zu (dot-dot)\n", *pcbDirEntry, cbNeeded));
                *pcbDirEntry = cbNeeded;
                return VERR_BUFFER_OVERFLOW;
            }
            pDirEntry->cbName    = 2;
            pDirEntry->szName[0] = '.';
            pDirEntry->szName[1] = '.';
            pDirEntry->szName[2] = '\0';
        }
        else
        {
            size_t cbNeeded = RT_UOFFSETOF(RTDIRENTRYEX, szName) + 1;
            if (*pcbDirEntry < cbNeeded)
            {
                Log3(("rtFsIsoDir_ReadDirUdf: VERR_BUFFER_OVERFLOW - cbDst=%zu cbNeeded=%zu (empty)\n", *pcbDirEntry, cbNeeded));
                *pcbDirEntry = cbNeeded;
                return VERR_BUFFER_OVERFLOW;
            }
            pDirEntry->cbName    = 0;
            pDirEntry->szName[0] = '\0';
        }

        pDirEntry->cwcShortName    = 0;
        pDirEntry->wszShortName[0] = '\0';

        /*
         * To avoid duplicating code in rtFsIsoCore_InitUdf and
         * rtFsIsoCore_QueryInfo, we create a dummy RTFSISOCORE on the stack.
         */
        RTFSISOCORE TmpObj;
        RT_ZERO(TmpObj);
        int rc = rtFsIsoCore_InitFromUdfIcbAndFileIdDesc(&TmpObj, &pFid->Icb, pFid, pThis->offDir - 1, pShared->Core.pVol);
        if (RT_SUCCESS(rc))
        {
            rc = rtFsIsoCore_QueryInfo(&TmpObj, &pDirEntry->Info, enmAddAttr);
            rtFsIsoCore_Destroy(&TmpObj);
        }

        /*
         * Update.
         */
        Log3(("rtFsIsoDir_ReadDirUdf: offDir=%#07x: %s (rc=%Rrc)\n", pThis->offDir, pDirEntry->szName, rc));
        pThis->offDir += cbFid;

        return rc;
    }

    Log3(("rtFsIsoDir_ReadDirUdf: offDir=%#07x: VERR_NO_MORE_FILES\n", pThis->offDir));
    return VERR_NO_MORE_FILES;
}

#endif /* maybe later */

#ifdef LOG_ENABLED
/**
 * Logs the content of a directory.
 */
DECLHIDDEN(void) RTFsUdfHlpLogDirContent(uint8_t const *pbDir, size_t cbDir)
{
    if (LogIs2Enabled())
    {
        uint32_t offDesc = 0;
        while (offDesc + RT_UOFFSETOF(UDFFILEIDDESC, abImplementationUse) < cbDir)
        {
            PCUDFFILEIDDESC pFid  = (PCUDFFILEIDDESC)&pbDir[offDesc];
            uint32_t const  cbFid = UDFFILEIDDESC_GET_SIZE(pFid);
            if (offDesc + cbFid > cbDir)
                break;

            uint32_t    cwcName = 0;
            RTUTF16     wszName[260];
            if (pFid->cbName > 0)
            {
                uint8_t const *pbName = UDFFILEIDDESC_2_NAME(pFid);
                uint32_t       offSrc = 1;
                if (*pbName == 8)
                    while (offSrc < pFid->cbName)
                    {
                        wszName[cwcName] = pbName[offSrc];
                        cwcName++;
                        offSrc++;
                    }
                else if (*pbName == 16)
                    while (offSrc + 1 <= pFid->cbName)
                    {
                        wszName[cwcName] = RT_MAKE_U16(pbName[offSrc + 1], pbName[offSrc]);
                        cwcName++;
                        offSrc += 2;
                    }
                else
                {
                    RTUtf16CopyAscii(wszName, RT_ELEMENTS(wszName), "<bad type>");
                    cwcName = 10;
                }
            }
            else if (pFid->fFlags & UDF_FILE_FLAGS_PARENT)
            {
                wszName[0] = '.';
                wszName[1] = '.';
                cwcName    = 2;
            }
            else
            {
                RTUtf16CopyAscii(wszName, RT_ELEMENTS(wszName), "<empty>");
                cwcName = 7;
            }
            wszName[cwcName] = '\0';

            Log2(("ISO/UDF: %04x: fFlags=%#x uVer=%u Icb={%#04x:%#010RX32 LB %#06x t=%u} cbName=%#04x cbIU=%#x '%ls'\n",
                  offDesc,
                  pFid->fFlags,
                  pFid->uVersion,
                  pFid->Icb.Location.uPartitionNo,
                  pFid->Icb.Location.off,
                  pFid->Icb.cb,
                  pFid->Icb.uType,
                  pFid->cbName,
                  pFid->cbImplementationUse,
                  wszName));
            int rc = RTFsUdfHlpValidateDescTagAndCrc(&pFid->Tag, cbDir - offDesc,
                                                        UDF_TAG_ID_FILE_ID_DESC, pFid->Tag.offTag, NULL);
            if (RT_FAILURE(rc))
                Log2(("ISO/UDF:      Bad Tag: %Rrc - idTag=%#x\n", rc, pFid->Tag.idTag));
            if (pFid->cbImplementationUse > 32)
                Log2(("ISO/UDF:      impl use (%#x bytes):\n%.*RhxD\n",
                      pFid->cbImplementationUse, pFid->cbImplementationUse, pFid->abImplementationUse));
            else if (pFid->cbImplementationUse > 0)
                Log2(("ISO/UDF:      impl use (%#x bytes): %.*Rhxs\n",
                      pFid->cbImplementationUse, pFid->cbImplementationUse, pFid->abImplementationUse));

            /* advance */
            offDesc += cbFid;
        }

        if (offDesc < cbDir)
            Log2(("ISO/UDF:  warning! %#x trailing bytes in directory:\n%.*RhxD\n",
                  cbDir - offDesc, cbDir - offDesc, &pbDir[offDesc]));
    }
}
#endif /* LOG_ENABLED */


/**
 * Converts a dstring field value into a UTF-8 string buffer.
 *
 * @returns IPRT status code.
 * @retval  VERR_BUFFER_OVERFLOW if insufficient buffer space. @a *pcbRet will
 *          be set to the required buffer size.
 * @param   pachSrc             The source dstring field.
 * @param   cbSrc               The size of the source field.
 * @param   pvDst               The destination buffer.  Can be NULL if @a cbDst
 *                              is also zero.
 * @param   cbDst               The destination buffer size.
 * @param   pcbRet              The size of the converted string, in bytes,
 *                              including the terminator character.
 */
DECLHIDDEN(int) RTFsUdfHlpDStringFieldToUtf8Buf(const char *pachSrc, size_t cbSrc, void *pvDst, size_t cbDst, size_t *pcbRet)
{
    AssertReturn(cbSrc < 256, VERR_INTERNAL_ERROR_3);
    AssertReturn(cbSrc > 2, VERR_INTERNAL_ERROR_2);
    Assert(pcbRet);
    char *pszDst = (char *)pvDst;

    /*
     * The last byte is supposed to contain the total encoded length.
     */
    size_t cbSrcEncoded = (uint8_t)pachSrc[cbSrc - 1];
    AssertStmt(cbSrcEncoded < cbSrc, cbSrcEncoded = cbSrc - 1);

    if (pachSrc[0] == 8)
    {
        /*
         * Convert from unicode points 0..255 (latin-1) to UTF-8.
         */
        if (cbDst > 1)
            pszDst[cbDst - 2] = '\0';

        AssertStmt(cbSrcEncoded > 0, cbSrcEncoded = 1);
        size_t  const         cbSrcText = cbSrcEncoded - 1;
        uint8_t const * const pbSrcText = (uint8_t const *)&pachSrc[1];
        size_t                offDst    = 0;
        for (size_t offSrc = 0; offSrc < cbSrcText; offSrc++)
        {
            RTUNICP const uc = pbSrcText[offSrc++];
            if (uc < 128)
            {
                if (uc > 0)
                {
                    if (offDst < cbDst)
                        pszDst[offDst] = (char)uc;
                    offDst++;
                }
                else
                    break;
            }
            else
            {
                if (offDst + 1 < cbDst)
                {
                    pszDst[offDst]     = 0xc0 | (uc >> 6);
                    pszDst[offDst + 1] = 0x80 | (uc & 0x3f);
                }
                offDst += 2;
            }
        }

        /* terminate */
        if (offDst < cbDst)
            pszDst[offDst] = '\0';
        else if (cbDst > 0)
            pszDst[cbDst - 1] = '\0';

        *pcbRet = ++offDst;
        return offDst <= cbDst ? VINF_SUCCESS : VERR_BUFFER_OVERFLOW;
    }

    if (pachSrc[0] == 16)
    {
        /*
         * Convert UTF-16 to UTF-8.
         *
         * Note! We don't honor the specs wrt single trailing byte here. We
         *       expect all codepoints to be encoded as 2 or 4 bytes.
         */
        AssertStmt(cbSrcEncoded > 0, cbSrcEncoded = 1);
        *pcbRet = 0;
        int       rc;
        PCRTUTF16 pwszSrc = (PCRTUTF16)&pachSrc[1]; /* misaligned */
        if (cbDst > 0)
            rc = RTUtf16BigToUtf8Ex(pwszSrc, (cbSrcEncoded - 1) / sizeof(RTUTF16), &pszDst, cbDst, pcbRet);
        else
        {
            rc = RTUtf16CalcUtf8LenEx(pwszSrc, (cbSrcEncoded - 1) / sizeof(RTUTF16), pcbRet);
            if (RT_SUCCESS(rc))
                rc = VERR_BUFFER_OVERFLOW;
        }
        *pcbRet += 1;
        return rc;
    }

    /*
     * Empty string, probably...
     */
    if (cbSrcEncoded <= 1)
    {
        *pcbRet = 1;
        if (cbDst >= 1)
        {
            *pszDst = '\0';
            return VINF_SUCCESS;
        }
        return VERR_BUFFER_OVERFLOW;
    }

    *pcbRet = 0;
    return VERR_ISOFS_BOGUS_UDF_DSTRING_FIELD;
}


/**
 * Converts a dstring field value into a UTF-8 heap string.
 *
 * @returns IPRT status code.
 * @retval  VERR_NO_STR_MEMORY
 * @param   pachSrc             The source dstring field.
 * @param   cbSrc               The size of the source field.
 * @param   ppszDst             Where to return the pointer to the string on
 *                              success.  Call RTStrFree when done with it.
 */
DECLHIDDEN(int) RTFsUdfHlpDStringFieldToUtf8(const char *pachSrc, size_t cbSrc, char **ppszDst)
{
    AssertReturn(cbSrc < 256, VERR_INTERNAL_ERROR_3);
    AssertReturn(cbSrc > 2, VERR_INTERNAL_ERROR_2);
    *ppszDst = NULL;

    /*
     * The last byte is supposed to contain the total encoded length.
     */
    size_t cbSrcEncoded = (uint8_t)pachSrc[cbSrc - 1];
    AssertStmt(cbSrcEncoded < cbSrc, cbSrcEncoded = cbSrc - 1);

    if (pachSrc[0] == 8)
    {
        /*
         * Convert latin-1 to UTF-8.
         */
        AssertStmt(cbSrcEncoded > 0, cbSrcEncoded = 1);
        return RTLatin1ToUtf8Ex(&pachSrc[1], cbSrcEncoded - 1, ppszDst, 0, NULL);
    }

    if (pachSrc[0] == 16)
    {
        /*
         * Convert UTF-16 to UTF-8.
         *
         * Note! We don't honor the specs wrt single trailing byte here. We
         *       expect all codepoints to be encoded as 2 or 4 bytes.
         */
        AssertStmt(cbSrcEncoded > 0, cbSrcEncoded = 1);
        PCRTUTF16 pwszSrc = (PCRTUTF16)&pachSrc[1]; /* misaligned */
        return RTUtf16BigToUtf8Ex(pwszSrc, (cbSrcEncoded - 1) / sizeof(RTUTF16), ppszDst, 0, NULL);
    }

    /*
     * Empty string, probably...
     */
    if (cbSrcEncoded <= 1)
    {
        char *psz = RTStrAlloc(1);
        if (psz)
        {
            *psz = '\0';
            *ppszDst = psz;
            return VINF_SUCCESS;
        }
        return VERR_NO_STR_MEMORY;
    }
    return VERR_ISOFS_BOGUS_UDF_DSTRING_FIELD;
}


/**
 * Checks the descriptor tag and CRC.
 *
 * @retval  IPRT status code.
 * @retval  VERR_ISOFS_TAG_IS_ALL_ZEROS
 * @retval  VERR_MISMATCH
 * @retval  VERR_ISOFS_UNSUPPORTED_TAG_VERSION
 * @retval  VERR_ISOFS_TAG_SECTOR_MISMATCH
 * @retval  VERR_ISOFS_BAD_TAG_CHECKSUM
 *
 * @param   pTag        The tag to check.
 * @param   idTag       The expected descriptor tag ID, UINT16_MAX matches any
 *                      tag ID.
 * @param   offTag      The sector offset of the tag.
 * @param   pErrInfo    Where to return extended error info.
 */
DECLHIDDEN(int) RTFsUdfHlpValidateDescTag(PCUDFTAG pTag, uint16_t idTag, uint32_t offTag, PRTERRINFO pErrInfo)
{
    /*
     * Checksum the tag first.
     */
    const uint8_t *pbTag     = (const uint8_t *)pTag;
    uint8_t const  bChecksum = pbTag[0]
                             + pbTag[1]
                             + pbTag[2]
                             + pbTag[3]
                             + pbTag[5] /* skipping byte 4 as that's the checksum. */
                             + pbTag[6]
                             + pbTag[7]
                             + pbTag[8]
                             + pbTag[9]
                             + pbTag[10]
                             + pbTag[11]
                             + pbTag[12]
                             + pbTag[13]
                             + pbTag[14]
                             + pbTag[15];
    if (pTag->uChecksum == bChecksum)
    {
        /*
         * Do the matching.
         */
        if (   pTag->uVersion == 3
            || pTag->uVersion == 2)
        {
            if (   pTag->idTag == idTag
                || idTag == UINT16_MAX)
            {
                if (pTag->offTag == offTag)
                {
                    //Log3(("ISO/UDF: Valid descriptor %#06x at %#010RX32; cbDescriptorCrc=%#06RX32 uTagSerialNo=%#x\n",
                    //      pTag->idTag, offTag, pTag->cbDescriptorCrc, pTag->uTagSerialNo));
                    return VINF_SUCCESS;
                }

                Log(("RTFsUdfHlpValidateDescTag(,%#x,%#010RX32,): Sector mismatch: %#RX32 (%.*Rhxs)\n",
                     idTag, offTag, pTag->offTag, sizeof(*pTag), pTag));
                return RTERRINFO_LOG_SET_F(pErrInfo, VERR_ISOFS_TAG_SECTOR_MISMATCH,
                                           "Descriptor tag sector number mismatch: %#x, expected %#x (%.*Rhxs)",
                                           pTag->offTag, offTag, sizeof(*pTag), pTag);
            }
            Log(("RTFsUdfHlpValidateDescTag(,%#x,%#010RX32,): Tag ID mismatch: %#x (%.*Rhxs)\n",
                 idTag, offTag, pTag->idTag, sizeof(*pTag), pTag));
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_MISMATCH, "Descriptor tag ID mismatch: %#x, expected %#x (%.*Rhxs)",
                                       pTag->idTag, idTag, sizeof(*pTag), pTag);
        }
        if (ASMMemIsZero(pTag, sizeof(*pTag)))
        {
            Log(("RTFsUdfHlpValidateDescTag(,%#x,%#010RX32,): All zeros\n", idTag, offTag));
            return RTERRINFO_LOG_SET(pErrInfo, VERR_ISOFS_TAG_IS_ALL_ZEROS, "Descriptor is all zeros");
        }

        Log(("RTFsUdfHlpValidateDescTag(,%#x,%#010RX32,): Unsupported version: %#x (%.*Rhxs)\n",
             idTag, offTag, pTag->uVersion, sizeof(*pTag), pTag));
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_ISOFS_UNSUPPORTED_TAG_VERSION, "Unsupported descriptor tag version: %#x, expected 2 or 3 (%.*Rhxs)",
                                   pTag->uVersion, sizeof(*pTag), pTag);
    }
    Log(("RTFsUdfHlpValidateDescTag(,%#x,%#010RX32,): checksum error: %#x, calc %#x (%.*Rhxs)\n",
         idTag, offTag, pTag->uChecksum, bChecksum, sizeof(*pTag), pTag));
    return RTERRINFO_LOG_SET_F(pErrInfo, VERR_ISOFS_BAD_TAG_CHECKSUM,
                               "Descriptor tag checksum error: %#x, calculated %#x (%.*Rhxs)",
                               pTag->uChecksum, bChecksum, sizeof(*pTag), pTag);
}


/**
 * Checks the descriptor CRC.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_ISOFS_INSUFFICIENT_DATA_FOR_DESC_CRC
 * @retval  VERR_ISOFS_DESC_CRC_MISMATCH
 *
 * @param   pTag        The descriptor buffer to checksum.
 * @param   cbDesc      The size of the descriptor buffer.
 * @param   pErrInfo    Where to return extended error info.
 */
DECLHIDDEN(int) RTFsUdfHlpValidateDescCrc(PCUDFTAG pTag, size_t cbDesc, PRTERRINFO pErrInfo)
{
    if (pTag->cbDescriptorCrc + sizeof(*pTag) <= cbDesc)
    {
        uint16_t uCrc = RTCrc16Ccitt(pTag + 1, pTag->cbDescriptorCrc);
        if (pTag->uDescriptorCrc == uCrc)
            return VINF_SUCCESS;

        Log(("RTFsUdfHlpValidateDescCrc(,%#x,%#010RX32,): Descriptor CRC mismatch: expected %#x, calculated %#x (cbDescriptorCrc=%#x)\n",
             pTag->idTag, pTag->offTag, pTag->uDescriptorCrc, uCrc, pTag->cbDescriptorCrc));
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_ISOFS_DESC_CRC_MISMATCH,
                                   "Descriptor CRC mismatch: exepcted %#x, calculated %#x (cbDescriptor=%#x, idTag=%#x, offTag=%#010RX32)",
                                   pTag->uDescriptorCrc, uCrc, pTag->cbDescriptorCrc, pTag->idTag, pTag->offTag);
    }

    Log(("RTFsUdfHlpValidateDescCrc(,%#x,%#010RX32,): Insufficient data to CRC: cbDescriptorCrc=%#x cbDesc=%#zx\n",
         pTag->idTag, pTag->offTag, pTag->cbDescriptorCrc, cbDesc));
    return RTERRINFO_LOG_SET_F(pErrInfo, VERR_ISOFS_INSUFFICIENT_DATA_FOR_DESC_CRC,
                               "Insufficient data to CRC: cbDescriptorCrc=%#x cbDesc=%#zx (idTag=%#x, offTag=%#010RX32)",
                               pTag->cbDescriptorCrc, cbDesc, pTag->idTag, pTag->offTag);
}


/**
 * Checks the descriptor tag and CRC.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_ISOFS_INSUFFICIENT_DATA_FOR_DESC_CRC
 * @retval  VERR_ISOFS_TAG_IS_ALL_ZEROS
 * @retval  VERR_MISMATCH
 * @retval  VERR_ISOFS_UNSUPPORTED_TAG_VERSION
 * @retval  VERR_ISOFS_TAG_SECTOR_MISMATCH
 * @retval  VERR_ISOFS_BAD_TAG_CHECKSUM
 * @retval  VERR_ISOFS_DESC_CRC_MISMATCH
 *
 * @param   pTag        The descriptor buffer to check the tag of and to
 *                      checksum.
 * @param   cbDesc      The size of the descriptor buffer.
 * @param   idTag       The expected descriptor tag ID, UINT16_MAX
 *                      matches any tag ID.
 * @param   offTag      The sector offset of the tag.
 * @param   pErrInfo    Where to return extended error info.
 */
DECLHIDDEN(int) RTFsUdfHlpValidateDescTagAndCrc(PCUDFTAG pTag, size_t cbDesc, uint16_t idTag, uint32_t offTag, PRTERRINFO pErrInfo)
{
    int rc = RTFsUdfHlpValidateDescTag(pTag, idTag, offTag, pErrInfo);
    if (RT_SUCCESS(rc))
        rc = RTFsUdfHlpValidateDescCrc(pTag, cbDesc, pErrInfo);
    return rc;
}


/**
 * Reads and processes the file set descriptor.
 *
 * @returns IPRT status code.
 * @param   pCtx            The UDF helper context.
 * @param   pVolInfo        Information from the VDS and where to add info from
 *                          the file set descriptor sequence.
 */
static int rtFsUdfProcessFileSetDescSeq(PCRTFSUDFHLPCTX pCtx, PRTFSUDFVOLINFO pVolInfo)
{
    /*
     * We assume there is a single file descriptor and don't bother checking what comes next.
     */
    PUDFFILESETDESC pFsd     = (PUDFFILESETDESC)pCtx->pbBuf;
    Assert(pCtx->cbBuf > sizeof(*pFsd));
    RT_ZERO(*pFsd);
    size_t const    cbToRead = RT_MIN(RT_MAX(pVolInfo->FileSetDescriptor.cb, sizeof(*pFsd)), pCtx->cbBuf);
    int rc = RTFsUdfHlpVpRead(pVolInfo, pCtx->hVfsBacking, pVolInfo->FileSetDescriptor.Location.uPartitionNo,
                              pVolInfo->FileSetDescriptor.Location.off, pFsd, cbToRead);
    if (RT_SUCCESS(rc))
    {
        rc = RTFsUdfHlpValidateDescTagAndCrc(&pFsd->Tag, cbToRead, UDF_TAG_ID_FILE_SET_DESC,
                                             pVolInfo->FileSetDescriptor.Location.off, pCtx->pErrInfo);
        if (RT_SUCCESS(rc))
        {
#ifdef LOG_ENABLED
            Log(("ISO/UDF: File set descriptor at %#RX32 (p%u/%#RX32)\n", pFsd->Tag.offTag,
                 pVolInfo->FileSetDescriptor.Location.uPartitionNo,
                 pVolInfo->FileSetDescriptor.Location.off));
            if (LogIs2Enabled())
            {
                UDF_LOG2_MEMBER_TIMESTAMP(pFsd, RecordingTimestamp);
                UDF_LOG2_MEMBER(pFsd, "#06RX16", uInterchangeLevel);
                UDF_LOG2_MEMBER(pFsd, "#06RX16", uMaxInterchangeLevel);
                UDF_LOG2_MEMBER(pFsd, "#010RX32", fCharacterSets);
                UDF_LOG2_MEMBER(pFsd, "#010RX32", fMaxCharacterSets);
                UDF_LOG2_MEMBER(pFsd, "#010RX32", uFileSetNo);
                UDF_LOG2_MEMBER(pFsd, "#010RX32", uFileSetDescNo);
                UDF_LOG2_MEMBER_CHARSPEC(pFsd, LogicalVolumeIDCharSet);
                UDF_LOG2_MEMBER_DSTRING(pFsd, achLogicalVolumeID);
                UDF_LOG2_MEMBER_CHARSPEC(pFsd, FileSetCharSet);
                UDF_LOG2_MEMBER_DSTRING(pFsd, achFileSetID);
                UDF_LOG2_MEMBER_DSTRING(pFsd, achCopyrightFile);
                UDF_LOG2_MEMBER_DSTRING(pFsd, achAbstractFile);
                UDF_LOG2_MEMBER_LONGAD(pFsd, RootDirIcb);
                UDF_LOG2_MEMBER_ENTITY_ID(pFsd, idDomain);
                UDF_LOG2_MEMBER_LONGAD(pFsd, NextExtent);
                UDF_LOG2_MEMBER_LONGAD(pFsd, SystemStreamDirIcb);
                if (!ASMMemIsZero(&pFsd->abReserved[0], sizeof(pFsd->abReserved)))
                    UDF_LOG2_MEMBER(pFsd, ".32Rhxs", abReserved);
            }
#endif

            /*
             * Do some basic sanity checking.
             */
            if (!UDF_IS_CHAR_SET_OSTA(&pFsd->FileSetCharSet))
                return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_FSD_UNSUPPORTED_CHAR_SET,
                                           "Invalid file set charset %.64Rhxs", &pFsd->FileSetCharSet);
            if (   pFsd->RootDirIcb.cb == 0
                || pFsd->RootDirIcb.uType != UDF_AD_TYPE_RECORDED_AND_ALLOCATED)
                return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_FSD_ZERO_ROOT_DIR,
                                           "Root Dir ICB location is zero or malformed: uType=%#x cb=%#x loc=%#x:%#RX32",
                                           pFsd->RootDirIcb.uType, pFsd->RootDirIcb.cb,
                                           pFsd->RootDirIcb.Location.uPartitionNo, pFsd->RootDirIcb.Location.off);
            if (   pFsd->NextExtent.cb != 0
                && pFsd->NextExtent.uType == UDF_AD_TYPE_RECORDED_AND_ALLOCATED)
                return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_FSD_NEXT_EXTENT,
                                           "NextExtent isn't zero: uType=%#x cb=%#x loc=%#x:%#RX32",
                                           pFsd->NextExtent.uType, pFsd->NextExtent.cb,
                                           pFsd->NextExtent.Location.uPartitionNo, pFsd->NextExtent.Location.off);

            /*
             * Copy the information we need.
             */
            pVolInfo->RootDirIcb         = pFsd->RootDirIcb;
            if (   pFsd->SystemStreamDirIcb.cb > 0
                && pFsd->SystemStreamDirIcb.uType == UDF_AD_TYPE_RECORDED_AND_ALLOCATED)
                pVolInfo->SystemStreamDirIcb = pFsd->SystemStreamDirIcb;
            else
                RT_ZERO(pVolInfo->SystemStreamDirIcb);
            return VINF_SUCCESS;
        }
        return rc;
    }
    return RTERRINFO_LOG_SET(pCtx->pErrInfo, rc, "Error reading file set descriptor");
}


/**
 * Check validatity and extract information from the descriptors in the VDS seq.
 *
 * @returns IPRT status code
 * @param   pCtx            The UDF helper context.
 * @param   pVdsInfo        The gathered temp info from the VDS sequence.
 * @param   pVolInfo        Where to return volume information found in the VDS.
 */
static int rtFsUdfProcessVdsSeqInfo(PCRTFSUDFHLPCTX pCtx, PRTFSUDFVDSINFO pVdsInfo, PRTFSUDFVOLINFO pVolInfo)
{
    /*
     * Check the basic descriptor counts.
     */
    PUDFPRIMARYVOLUMEDESC pPvd;
    if (pVdsInfo->cPrimaryVols == 1)
        pPvd = pVdsInfo->apPrimaryVols[0];
    else
    {
        if (pVdsInfo->cPrimaryVols == 0)
            return RTERRINFO_LOG_SET(pCtx->pErrInfo, VERR_ISOFS_NO_PVD, "No primary volume descriptor was found");
        return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_MULTIPLE_PVDS,
                                   "More than one primary volume descriptor was found: %u", pVdsInfo->cPrimaryVols);
    }

    PUDFLOGICALVOLUMEDESC pLvd;
    if (pVdsInfo->cLogicalVols == 1)
        pLvd = pVdsInfo->apLogicalVols[0];
    else
    {
        if (pVdsInfo->cLogicalVols == 0)
            return RTERRINFO_LOG_SET(pCtx->pErrInfo, VERR_ISOFS_NO_LVD, "No logical volume descriptor was found");
        return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_MULTIPLE_LVDS,
                                   "More than one logical volume descriptor was found: %u", pVdsInfo->cLogicalVols);
    }

#if 0
    if (pVdsInfo->cPartitions == 0)
        return RTERRINFO_LOG_SET(pCtx->pErrInfo, VERR_ISOFS_NO_PD, "No partition descriptors was found");
#endif

    /*
     * Check out the partition map in the logical volume descriptor.
     * Produce the mapping table while going about that.
     */
    if (pLvd->cPartitionMaps > 64)
        return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_TOO_MANY_PART_MAPS,
                                   "Too many partition maps: %u (max 64)", pLvd->cPartitionMaps);

    PRTFSISOVOLUDFPMAP paPartMaps = NULL;
    if (pLvd->cPartitionMaps > 0)
    {
        pVdsInfo->paPartMaps = paPartMaps = (PRTFSISOVOLUDFPMAP)RTMemAllocZ(sizeof(paPartMaps[0]) * pLvd->cPartitionMaps);
        if (!paPartMaps)
            return VERR_NO_MEMORY;
    }
    uint32_t cPartMaps = 0;

    if (pLvd->cbMapTable)
    {
        uint32_t off  = 0;
        while (off + sizeof(UDFPARTMAPHDR) <= pLvd->cbMapTable)
        {
            PCUDFPARTMAPHDR pHdr = (PCUDFPARTMAPHDR)&pLvd->abPartitionMaps[off];

            /*
             * Bounds checking.
             */
            if (off + pHdr->cb > pLvd->cbMapTable)
            {
                if (cPartMaps < pLvd->cbMapTable)
                    return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_MALFORMED_PART_MAP_TABLE,
                                               "Incomplete partition map entry at offset %#x: cb=%#x -> offEnd=%#x cbMapTable=%#x (type=%#x)",
                                               off, pHdr->cb, off + pHdr->cb, pLvd->cbMapTable, pHdr->bType);
                LogRel(("ISO/UDF: Warning: Incomplete partition map entry at offset %#x: cb=%#x -> offEnd=%#x cbMapTable=%#x (type=%#x)\n",
                        off, pHdr->cb, off + pHdr->cb, pLvd->cbMapTable, pHdr->bType));
                break;
            }
            if (cPartMaps >= pLvd->cPartitionMaps)
            {
                LogRel(("ISO/UDF: Warning: LVD::cPartitionMaps is %u but there are more bytes in the table. (off=%#x cb=%#x cbMapTable=%#x bType=%#x)\n",
                        cPartMaps - pLvd->cPartitionMaps, off, pHdr->cb, pLvd->cbMapTable, pHdr->bType));
                break;
            }

            /*
             * Extract relevant info out of the entry.
             */
            paPartMaps[cPartMaps].offMapTable = (uint16_t)off;
            uint16_t uPartitionNo;
            if (pHdr->bType == 1)
            {
                PCUDFPARTMAPTYPE1 pType1 = (PCUDFPARTMAPTYPE1)pHdr;
                paPartMaps[cPartMaps].uVolumeSeqNo = pType1->uVolumeSeqNo;
                paPartMaps[cPartMaps].bType        = RTFSUDF_PMAP_T_PLAIN;
                uPartitionNo = pType1->uPartitionNo;
            }
            else if (pHdr->bType == 2)
            {
                PCUDFPARTMAPTYPE2 pType2 = (PCUDFPARTMAPTYPE2)pHdr;
                if (UDF_ENTITY_ID_EQUALS(&pType2->idPartitionType, UDF_ENTITY_ID_VPM_PARTITION_TYPE))
                {
                    paPartMaps[cPartMaps].bType = pType2->idPartitionType.Suffix.Udf.uUdfRevision >= 0x200
                                                ? RTFSUDF_PMAP_T_VPM_20 : RTFSUDF_PMAP_T_VPM_15;
                    return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_VPM_NOT_SUPPORTED, "Partition type '%.23s' (%#x) not supported",
                                               pType2->idPartitionType.achIdentifier, pType2->idPartitionType.Suffix.Udf.uUdfRevision);
                }
                else if (UDF_ENTITY_ID_EQUALS(&pType2->idPartitionType, UDF_ENTITY_ID_SPM_PARTITION_TYPE))
                {
                    paPartMaps[cPartMaps].bType = RTFSUDF_PMAP_T_SPM;
                    return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_SPM_NOT_SUPPORTED, "Partition type '%.23s' (%#x) not supported",
                                               pType2->idPartitionType.achIdentifier, pType2->idPartitionType.Suffix.Udf.uUdfRevision);
                }
                else if (UDF_ENTITY_ID_EQUALS(&pType2->idPartitionType, UDF_ENTITY_ID_MPM_PARTITION_TYPE))
                {
                    paPartMaps[cPartMaps].bType = RTFSUDF_PMAP_T_MPM;
                    return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_MPM_NOT_SUPPORTED, "Partition type '%.23s' (%#x) not supported",
                                               pType2->idPartitionType.achIdentifier, pType2->idPartitionType.Suffix.Udf.uUdfRevision);
                }
                else
                    return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_UNKNOWN_PART_MAP_TYPE_ID,
                                               "Unknown partition map ID for #%u @ %#x: %.23s",
                                               cPartMaps, off, pType2->idPartitionType.achIdentifier);
#if 0 /* unreachable code */
                paPartMaps[cPartMaps].uVolumeSeqNo = pType2->uVolumeSeqNo;
                uPartitionNo = pType2->uPartitionNo;
#endif
            }
            else
                return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_UNKNOWN_PART_MAP_ENTRY_TYPE,
                                           "Unknown partition map entry type #%u @ %#x: %u", cPartMaps, off, pHdr->bType);
            paPartMaps[cPartMaps].uPartitionNo = uPartitionNo;

            /*
             * Lookup the partition number and retrieve the relevant info from the partition descriptor.
             */
            uint32_t i = pVdsInfo->cPartitions;
            while (i-- > 0)
            {
                PUDFPARTITIONDESC pPd = pVdsInfo->apPartitions[i];
                if (paPartMaps[cPartMaps].uPartitionNo == pPd->uPartitionNo)
                {
                    paPartMaps[cPartMaps].idxPartDesc     = (uint16_t)i;
                    paPartMaps[cPartMaps].cSectors        = pPd->cSectors;
                    paPartMaps[cPartMaps].offLocation     = pPd->offLocation;
                    paPartMaps[cPartMaps].offByteLocation = (uint64_t)pPd->offLocation * pCtx->cbSector;
                    paPartMaps[cPartMaps].fFlags          = pPd->fFlags;
                    paPartMaps[cPartMaps].uAccessType     = pPd->uAccessType;
                    if (!UDF_ENTITY_ID_EQUALS(&pPd->idPartitionContents, UDF_ENTITY_ID_PD_PARTITION_CONTENTS_UDF_3))
                        paPartMaps[cPartMaps].fHaveHdr    = false;
                    else
                    {
                        paPartMaps[cPartMaps].fHaveHdr    = true;
                        paPartMaps[cPartMaps].Hdr         = pPd->ContentsUse.Hdr;
                    }
                    break;
                }
            }
            if (i > pVdsInfo->cPartitions)
                return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_PARTITION_NOT_FOUND,
                                           "Partition #%u (%#x) specified by mapping entry #%u (@ %#x) was not found! (int-type %u)",
                                           uPartitionNo, uPartitionNo, cPartMaps, off, paPartMaps[cPartMaps].bType);

            /*
             * Advance.
             */
            cPartMaps++;
            off += pHdr->cb;
        }

        if (cPartMaps < pLvd->cPartitionMaps)
            return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_INCOMPLETE_PART_MAP_TABLE,
                                       "Only found %u of the %u announced partition mapping table entries",
                                       cPartMaps, pLvd->cPartitionMaps);
    }

    /* It might be theoretically possible to not use virtual partitions for
       accessing data, so just warn if there aren't any. */
    if (cPartMaps == 0)
        LogRel(("ISO/UDF: Warning: No partition maps!\n"));

    /*
     * Check out the logical volume descriptor.
     */
    if (   pLvd->cbLogicalBlock < pCtx->cbSector
        || pLvd->cbLogicalBlock > pCtx->cbMaxLogicalBlock
        || (pLvd->cbLogicalBlock % pCtx->cbSector) != 0)
        return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_UNSUPPORTED_LOGICAL_BLOCK_SIZE,
                                   "Logical block size of %#x is not supported with a sector size of %#x",
                                   pLvd->cbLogicalBlock, pCtx->cbSector);

    if (!UDF_ENTITY_ID_EQUALS(&pLvd->idDomain, UDF_ENTITY_ID_LVD_DOMAIN))
        return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_BAD_LVD_DOMAIN_ID,
                                   "Unsupported domain ID in logical volume descriptor: '%.23s'", pLvd->idDomain.achIdentifier);

    if (   pLvd->ContentsUse.FileSetDescriptor.uType != UDF_AD_TYPE_RECORDED_AND_ALLOCATED
        || pLvd->ContentsUse.FileSetDescriptor.cb    == 0
        || pLvd->ContentsUse.FileSetDescriptor.Location.uPartitionNo >= cPartMaps)
        return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_BAD_LVD_FILE_SET_DESC_LOCATION,
                                   "Malformed file set descriptor location (type=%u cb=%#x part=%#x)",
                                   pLvd->ContentsUse.FileSetDescriptor.uType,
                                   pLvd->ContentsUse.FileSetDescriptor.cb,
                                   pLvd->ContentsUse.FileSetDescriptor.Location.uPartitionNo);

    bool fLvdHaveVolId = !ASMMemIsZero(pLvd->achLogicalVolumeID, sizeof(pLvd->achLogicalVolumeID));
    if (   fLvdHaveVolId
        && !UDF_IS_CHAR_SET_OSTA(&pLvd->DescCharSet))
        return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_BAD_LVD_DESC_CHAR_SET,
                                   "Logical volume ID is not using OSTA compressed unicode");

    /*
     * We can ignore much, if not all of the primary volume descriptor.
     */

    /*
     * We're good. So copy over the data.
     */
    pVolInfo->FileSetDescriptor = pLvd->ContentsUse.FileSetDescriptor;
    pVolInfo->cbBlock           = pLvd->cbLogicalBlock;
    pVolInfo->cShiftBlock       = 9;
    while (pVolInfo->cbBlock != RT_BIT_32(pVolInfo->cShiftBlock))
        pVolInfo->cShiftBlock++;
    pVolInfo->fFlags            = pPvd->fFlags;
    pVolInfo->cPartitions       = cPartMaps;
    pVolInfo->paPartitions      = paPartMaps;
    pVdsInfo->paPartMaps = NULL;
    if (fLvdHaveVolId)
        memcpy(pVolInfo->achLogicalVolumeID, pLvd->achLogicalVolumeID, sizeof(pVolInfo->achLogicalVolumeID));
    else
        RT_ZERO(pVolInfo->achLogicalVolumeID);

    return VINF_SUCCESS;
}


/**
 * Processes a primary volume descriptor in the VDS (UDF).
 *
 * @returns IPRT status code.
 * @param   pCtx            The UDF helper context.
 * @param   pVdsInfo        Where to gather temp info from the VDS sequence.
 * @param   pDesc           The descriptor.
 */
//cmd: kmk VBoxRT && kmk_redirect -E VBOX_LOG_DEST="nofile stderr" -E VBOX_LOG="rt_fs=~0" -E VBOX_LOG_FLAGS="unbuffered enabled" -- e:\vbox\svn\trunk\out\win.amd64\debug\bin\tools\RTLs.exe :iprtvfs:file(open,d:\Downloads\en_windows_10_enterprise_version_1703_updated_march_2017_x64_dvd_10189290.iso,r):vfs(isofs):/ -la
static int rtFsUdfProcessPrimaryVolDesc(PCRTFSUDFHLPCTX pCtx, PRTFSUDFVDSINFO pVdsInfo, PCUDFPRIMARYVOLUMEDESC pDesc)
{
#ifdef LOG_ENABLED
    Log(("ISO/UDF: Primary volume descriptor at sector %#RX32\n", pDesc->Tag.offTag));
    if (LogIs2Enabled())
    {
        UDF_LOG2_MEMBER(pDesc, "#010RX32", uVolumeDescSeqNo);
        UDF_LOG2_MEMBER(pDesc, "#010RX32", uPrimaryVolumeDescNo);
        UDF_LOG2_MEMBER_DSTRING(pDesc, achVolumeID);
        UDF_LOG2_MEMBER(pDesc, "#06RX16", uVolumeSeqNo);
        UDF_LOG2_MEMBER(pDesc, "#06RX16", uVolumeSeqNo);
        UDF_LOG2_MEMBER(pDesc, "#06RX16", uMaxVolumeSeqNo);
        UDF_LOG2_MEMBER(pDesc, "#06RX16", uInterchangeLevel);
        UDF_LOG2_MEMBER(pDesc, "#06RX16", uMaxInterchangeLevel);
        UDF_LOG2_MEMBER(pDesc, "#010RX32", fCharacterSets);
        UDF_LOG2_MEMBER(pDesc, "#010RX32", fMaxCharacterSets);
        UDF_LOG2_MEMBER_DSTRING(pDesc, achVolumeSetID);
        UDF_LOG2_MEMBER_CHARSPEC(pDesc, DescCharSet);
        UDF_LOG2_MEMBER_CHARSPEC(pDesc, ExplanatoryCharSet);
        UDF_LOG2_MEMBER_EXTENTAD(pDesc, VolumeAbstract);
        UDF_LOG2_MEMBER_EXTENTAD(pDesc, VolumeCopyrightNotice);
        UDF_LOG2_MEMBER_ENTITY_ID(pDesc, idApplication);
        UDF_LOG2_MEMBER_TIMESTAMP(pDesc, RecordingTimestamp);
        UDF_LOG2_MEMBER_ENTITY_ID(pDesc, idImplementation);
        if (!ASMMemIsZero(&pDesc->abImplementationUse, sizeof(pDesc->abImplementationUse)))
            Log2(("ISO/UDF:   %-32s %.64Rhxs\n", "abReserved[64]:", &pDesc->abImplementationUse[0]));
        UDF_LOG2_MEMBER(pDesc, "#010RX32", offPredecessorVolDescSeq);
        UDF_LOG2_MEMBER(pDesc, "#06RX16", fFlags);
        if (!ASMMemIsZero(&pDesc->abReserved, sizeof(pDesc->abReserved)))
            Log2(("ISO/UDF:   %-32s %.22Rhxs\n", "abReserved[22]:", &pDesc->abReserved[0]));
    }
#endif

    /*
     * Check if this is a new revision of an existing primary volume descriptor.
     */
    PUDFPRIMARYVOLUMEDESC pEndianConvert = NULL;
    uint32_t i = pVdsInfo->cPrimaryVols;
    while (i--> 0)
    {
        if (   memcmp(pDesc->achVolumeID, pVdsInfo->apPrimaryVols[i]->achVolumeID, sizeof(pDesc->achVolumeID)) == 0
            && memcmp(&pDesc->DescCharSet, &pVdsInfo->apPrimaryVols[i]->DescCharSet, sizeof(pDesc->DescCharSet)) == 0)
        {
            if (RT_LE2H_U32(pDesc->uVolumeDescSeqNo) >= pVdsInfo->apPrimaryVols[i]->uVolumeDescSeqNo)
            {
                Log(("ISO/UDF: Primary descriptor prevails over previous! (%u >= %u)\n",
                     RT_LE2H_U32(pDesc->uVolumeDescSeqNo), pVdsInfo->apPartitions[i]->uVolumeDescSeqNo));
                pEndianConvert = pVdsInfo->apPrimaryVols[i];
                memcpy(pEndianConvert, pDesc, sizeof(*pDesc));
            }
            else
                Log(("ISO/UDF: Primary descriptor has lower sequence number than the previous! (%u < %u)\n",
                     RT_LE2H_U32(pDesc->uVolumeDescSeqNo), pVdsInfo->apPartitions[i]->uVolumeDescSeqNo));
            break;
        }
    }
    if (i >= pVdsInfo->cPrimaryVols)
    {
        /*
         * It wasn't. Append it.
         */
        i = pVdsInfo->cPrimaryVols;
        if (i < RT_ELEMENTS(pVdsInfo->apPrimaryVols))
        {
            pVdsInfo->apPrimaryVols[i] = pEndianConvert = (PUDFPRIMARYVOLUMEDESC)RTMemDup(pDesc, sizeof(*pDesc));
            if (pEndianConvert)
                pVdsInfo->cPrimaryVols = i + 1;
            else
                return VERR_NO_MEMORY;
            Log2(("ISO/UDF: ++New primary descriptor.\n"));
        }
        else
            return RTERRINFO_LOG_SET(pCtx->pErrInfo, VERR_ISOFS_TOO_MANY_PVDS, "Encountered too many primary volume descriptors");
    }

#ifdef RT_BIG_ENDIAN
    /*
     * Do endian conversion of the descriptor.
     */
    if (pEndianConvert)
    {
        AssertFailed();
    }
#else
    RT_NOREF(pEndianConvert);
#endif
    return VINF_SUCCESS;
}


/**
 * Processes an logical volume descriptor in the VDS (UDF).
 *
 * @returns IPRT status code.
 * @param   pVdsInfo    Where to gather temp info from the VDS sequence.
 * @param   pDesc       The descriptor.
 * @param   cbSector    The sector size (UDF defines the logical and physical
 *                      sector size to be the same).
 */
static int rtFsUdfProcessLogicalVolumeDesc(PCRTFSUDFHLPCTX pCtx, PRTFSUDFVDSINFO pVdsInfo, PCUDFLOGICALVOLUMEDESC pDesc)
{
#ifdef LOG_ENABLED
    Log(("ISO/UDF: Logical volume descriptor at sector %#RX32\n", pDesc->Tag.offTag));
    if (LogIs2Enabled())
    {
        UDF_LOG2_MEMBER(pDesc, "#010RX32", uVolumeDescSeqNo);
        UDF_LOG2_MEMBER_CHARSPEC(pDesc, DescCharSet);
        UDF_LOG2_MEMBER_DSTRING(pDesc, achLogicalVolumeID);
        UDF_LOG2_MEMBER(pDesc, "#010RX32", cbLogicalBlock);
        UDF_LOG2_MEMBER_ENTITY_ID(pDesc, idDomain);
        if (UDF_ENTITY_ID_EQUALS(&pDesc->idDomain, UDF_ENTITY_ID_LVD_DOMAIN))
            UDF_LOG2_MEMBER_LONGAD(pDesc, ContentsUse.FileSetDescriptor);
        else if (!ASMMemIsZero(&pDesc->ContentsUse.ab[0], sizeof(pDesc->ContentsUse.ab)))
            Log2(("ISO/UDF:   %-32s %.16Rhxs\n", "ContentsUse.ab[16]:", &pDesc->ContentsUse.ab[0]));
        UDF_LOG2_MEMBER(pDesc, "#010RX32", cbMapTable);
        UDF_LOG2_MEMBER(pDesc, "#010RX32", cPartitionMaps);
        UDF_LOG2_MEMBER_ENTITY_ID(pDesc, idImplementation);
        if (!ASMMemIsZero(&pDesc->ImplementationUse.ab[0], sizeof(pDesc->ImplementationUse.ab)))
            Log2(("ISO/UDF:   %-32s\n%.128RhxD\n", "ImplementationUse.ab[128]:", &pDesc->ImplementationUse.ab[0]));
        UDF_LOG2_MEMBER_EXTENTAD(pDesc, IntegritySeqExtent);
        if (pDesc->cbMapTable)
        {
            Log2(("ISO/UDF:   %-32s\n", "abPartitionMaps"));
            uint32_t iMap = 0;
            uint32_t off  = 0;
            while (off + sizeof(UDFPARTMAPHDR) <= pDesc->cbMapTable)
            {
                PCUDFPARTMAPHDR pHdr = (PCUDFPARTMAPHDR)&pDesc->abPartitionMaps[off];
                Log2(("ISO/UDF:     %02u @ %#05x: type %u, length %u\n", iMap, off, pHdr->bType, pHdr->cb));
                if (off + pHdr->cb > pDesc->cbMapTable)
                {
                    Log2(("ISO/UDF:                 BAD! Entry is %d bytes too long!\n", off + pHdr->cb - pDesc->cbMapTable));
                    break;
                }
                if (pHdr->bType == 1)
                {
                    PCUDFPARTMAPTYPE1 pType1 = (PCUDFPARTMAPTYPE1)pHdr;
                    UDF_LOG2_MEMBER_EX(pType1, "#06RX16", uVolumeSeqNo, 5);
                    UDF_LOG2_MEMBER_EX(pType1, "#06RX16", uPartitionNo, 5);
                }
                else if (pHdr->bType == 2)
                {
                    PCUDFPARTMAPTYPE2 pType2 = (PCUDFPARTMAPTYPE2)pHdr;
                    UDF_LOG2_MEMBER_ENTITY_ID_EX(pType2, idPartitionType, 5);
                    UDF_LOG2_MEMBER_EX(pType2, "#06RX16", uVolumeSeqNo, 5);
                    UDF_LOG2_MEMBER_EX(pType2, "#06RX16", uPartitionNo, 5);
                    if (UDF_ENTITY_ID_EQUALS(&pType2->idPartitionType, UDF_ENTITY_ID_SPM_PARTITION_TYPE))
                    {
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#06RX16", Spm.cBlocksPerPacket, 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#04RX8", Spm.cSparingTables, 5);
                        if (pType2->u.Spm.bReserved2)
                            UDF_LOG2_MEMBER_EX(&pType2->u, "#04RX8", Spm.bReserved2, 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Spm.cbSparingTable, 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Spm.aoffSparingTables[0], 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Spm.aoffSparingTables[1], 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Spm.aoffSparingTables[2], 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Spm.aoffSparingTables[3], 5);
                    }
                    else if (UDF_ENTITY_ID_EQUALS(&pType2->idPartitionType, UDF_ENTITY_ID_MPM_PARTITION_TYPE))
                    {
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Mpm.offMetadataFile, 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Mpm.offMetadataMirrorFile, 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Mpm.offMetadataBitmapFile, 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#010RX32", Mpm.cBlocksAllocationUnit, 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#06RX16", Mpm.cBlocksAlignmentUnit, 5);
                        UDF_LOG2_MEMBER_EX(&pType2->u, "#04RX8", Mpm.fFlags, 5);
                        if (!ASMMemIsZero(pType2->u.Mpm.abReserved2, sizeof(pType2->u.Mpm.abReserved2)))
                            UDF_LOG2_MEMBER_EX(&pType2->u, ".5Rhxs", Mpm.abReserved2, 5);
                    }
                }
                else
                    Log2(("ISO/UDF:                 BAD! Unknown type!\n"));

                /* advance */
                off += pHdr->cb;
                iMap++;
            }
        }
    }
#endif

    /*
     * Check if this is a newer revision of an existing primary volume descriptor.
     */
    size_t cbDesc = (size_t)pDesc->cbMapTable + RT_UOFFSETOF(UDFLOGICALVOLUMEDESC, abPartitionMaps);
    if (   pDesc->cbMapTable >= (UINT32_MAX >> 1)
        || cbDesc > pCtx->cbSector)
    {
        Log(("ISO/UDF: Logical volume descriptor is too big: %#zx (cbSector=%#x)\n", cbDesc, pCtx->cbSector));
        return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_TOO_BIG_PARTMAP_IN_LVD,
                                   "Logical volume descriptor is too big: %#zx (cbSector=%#x)\n", cbDesc, pCtx->cbSector);
    }

    PUDFLOGICALVOLUMEDESC pEndianConvert = NULL;
    uint32_t i = pVdsInfo->cLogicalVols;
    while (i--> 0)
        if (   memcmp(pDesc->achLogicalVolumeID, pVdsInfo->apLogicalVols[i]->achLogicalVolumeID,
                      sizeof(pDesc->achLogicalVolumeID)) == 0
            && memcmp(&pDesc->DescCharSet, &pVdsInfo->apLogicalVols[i]->DescCharSet,
                      sizeof(pDesc->DescCharSet)) == 0)
        {
            if (RT_LE2H_U32(pDesc->uVolumeDescSeqNo) >= pVdsInfo->apLogicalVols[i]->uVolumeDescSeqNo)
            {
                Log(("ISO/UDF: Logical descriptor prevails over previous! (%u >= %u)\n",
                     RT_LE2H_U32(pDesc->uVolumeDescSeqNo), pVdsInfo->apLogicalVols[i]->uVolumeDescSeqNo));
                pEndianConvert = (PUDFLOGICALVOLUMEDESC)RTMemDup(pDesc, cbDesc);
                if (!pEndianConvert)
                    return VERR_NO_MEMORY;
                RTMemFree(pVdsInfo->apLogicalVols[i]);
                pVdsInfo->apLogicalVols[i] = pEndianConvert;
            }
            else
                Log(("ISO/UDF: Logical descriptor has lower sequence number than the previous! (%u >= %u)\n",
                     RT_LE2H_U32(pDesc->uVolumeDescSeqNo), pVdsInfo->apLogicalVols[i]->uVolumeDescSeqNo));
            break;
        }
    if (i >= pVdsInfo->cLogicalVols)
    {
        /*
         * It wasn't. Append it.
         */
        i = pVdsInfo->cLogicalVols;
        if (i < RT_ELEMENTS(pVdsInfo->apLogicalVols))
        {
            pVdsInfo->apLogicalVols[i] = pEndianConvert = (PUDFLOGICALVOLUMEDESC)RTMemDup(pDesc, cbDesc);
            if (pEndianConvert)
                pVdsInfo->cLogicalVols = i + 1;
            else
                return VERR_NO_MEMORY;
            Log2(("ISO/UDF: ++New logical volume descriptor.\n"));
        }
        else
            return RTERRINFO_LOG_SET(pCtx->pErrInfo, VERR_ISOFS_TOO_MANY_LVDS, "Too many logical volume descriptors");
    }

#ifdef RT_BIG_ENDIAN
    /*
     * Do endian conversion of the descriptor.
     */
    if (pEndianConvert)
    {
        AssertFailed();
    }
#else
    RT_NOREF(pEndianConvert);
#endif
    return VINF_SUCCESS;
}


/**
 * Processes an partition descriptor in the VDS (UDF).
 *
 * @returns IPRT status code.
 * @param   pCtx            The UDF helper context.
 * @param   pVdsInfo        Where to gather temp info from the VDS sequence.
 * @param   pDesc           The descriptor.
 */
static int rtFsUdfProcessPartitionDesc(PCRTFSUDFHLPCTX pCtx, PRTFSUDFVDSINFO pVdsInfo, PCUDFPARTITIONDESC pDesc)
{
#ifdef LOG_ENABLED
    Log(("ISO/UDF: Partition descriptor at sector %#RX32\n", pDesc->Tag.offTag));
    if (LogIs2Enabled())
    {
        UDF_LOG2_MEMBER(pDesc, "#010RX32", uVolumeDescSeqNo);
        UDF_LOG2_MEMBER(pDesc, "#06RX16", fFlags);
        UDF_LOG2_MEMBER(pDesc, "#06RX16", uPartitionNo);
        UDF_LOG2_MEMBER_ENTITY_ID(pDesc, idPartitionContents);
        if (UDF_ENTITY_ID_EQUALS(&pDesc->idPartitionContents, UDF_ENTITY_ID_PD_PARTITION_CONTENTS_UDF_3))
        {
            UDF_LOG2_MEMBER_SHORTAD(&pDesc->ContentsUse, Hdr.UnallocatedSpaceTable);
            UDF_LOG2_MEMBER_SHORTAD(&pDesc->ContentsUse, Hdr.UnallocatedSpaceBitmap);
            UDF_LOG2_MEMBER_SHORTAD(&pDesc->ContentsUse, Hdr.PartitionIntegrityTable);
            UDF_LOG2_MEMBER_SHORTAD(&pDesc->ContentsUse, Hdr.FreedSpaceTable);
            UDF_LOG2_MEMBER_SHORTAD(&pDesc->ContentsUse, Hdr.FreedSpaceBitmap);
            if (!ASMMemIsZero(&pDesc->ContentsUse.Hdr.abReserved[0], sizeof(pDesc->ContentsUse.Hdr.abReserved)))
                Log2(("ISO/UDF:   %-32s\n%.88RhxD\n", "Hdr.abReserved[88]:", &pDesc->ContentsUse.Hdr.abReserved[0]));
        }
        else if (!ASMMemIsZero(&pDesc->ContentsUse.ab[0], sizeof(pDesc->ContentsUse.ab)))
            Log2(("ISO/UDF:   %-32s\n%.128RhxD\n", "ContentsUse.ab[128]:", &pDesc->ContentsUse.ab[0]));
        UDF_LOG2_MEMBER(pDesc, "#010RX32", uAccessType);
        UDF_LOG2_MEMBER(pDesc, "#010RX32", offLocation);
        UDF_LOG2_MEMBER(pDesc, "#010RX32", cSectors);
        UDF_LOG2_MEMBER_ENTITY_ID(pDesc, idImplementation);
        if (!ASMMemIsZero(&pDesc->ImplementationUse.ab[0], sizeof(pDesc->ImplementationUse.ab)))
            Log2(("ISO/UDF:   %-32s\n%.128RhxD\n", "ImplementationUse.ab[128]:", &pDesc->ImplementationUse.ab[0]));

        if (!ASMMemIsZero(&pDesc->abReserved[0], sizeof(pDesc->abReserved)))
            Log2(("ISO/UDF:   %-32s\n%.156RhxD\n", "ImplementationUse.ab[156]:", &pDesc->abReserved[0]));
    }
#endif

    /*
     * Check if this is a newer revision of an existing primary volume descriptor.
     */
    PUDFPARTITIONDESC pEndianConvert = NULL;
    uint32_t i = pVdsInfo->cPartitions;
    while (i--> 0)
        if (pDesc->uPartitionNo == pVdsInfo->apPartitions[i]->uPartitionNo)
        {
            if (RT_LE2H_U32(pDesc->uVolumeDescSeqNo) >= pVdsInfo->apPartitions[i]->uVolumeDescSeqNo)
            {
                Log(("ISO/UDF: Partition descriptor for part %#u prevails over previous! (%u >= %u)\n",
                     pDesc->uPartitionNo, RT_LE2H_U32(pDesc->uVolumeDescSeqNo), pVdsInfo->apPartitions[i]->uVolumeDescSeqNo));
                pEndianConvert = pVdsInfo->apPartitions[i];
                memcpy(pEndianConvert, pDesc, sizeof(*pDesc));
            }
            else
                Log(("ISO/UDF: Partition descriptor for part %#u has a lower sequence number than the previous! (%u < %u)\n",
                     pDesc->uPartitionNo, RT_LE2H_U32(pDesc->uVolumeDescSeqNo), pVdsInfo->apPartitions[i]->uVolumeDescSeqNo));
            break;
        }
    if (i >= pVdsInfo->cPartitions)
    {
        /*
         * It wasn't. Append it.
         */
        i = pVdsInfo->cPartitions;
        if (i < RT_ELEMENTS(pVdsInfo->apPartitions))
        {
            pVdsInfo->apPartitions[i] = pEndianConvert = (PUDFPARTITIONDESC)RTMemDup(pDesc, sizeof(*pDesc));
            if (pEndianConvert)
                pVdsInfo->cPartitions = i + 1;
            else
                return VERR_NO_MEMORY;
            Log2(("ISO/UDF: ++New partition descriptor.\n"));
        }
        else
            return RTERRINFO_LOG_SET(pCtx->pErrInfo, VERR_ISOFS_TOO_MANY_PDS, "Too many physical volume descriptors");
    }

#ifdef RT_BIG_ENDIAN
    /*
     * Do endian conversion of the descriptor.
     */
    if (pEndianConvert)
    {
        AssertFailed();
    }
#else
    RT_NOREF(pEndianConvert);
#endif
    return VINF_SUCCESS;
}


/**
 * Processes an implementation use descriptor in the VDS (UDF).
 *
 * @returns IPRT status code.
 * @param   pVdsInfo    Where to gather temp info from the VDS sequence.
 * @param   pDesc       The descriptor.
 */
static int rtFsUdfProcessImplUseVolDesc(PRTFSUDFVDSINFO pVdsInfo, PCUDFIMPLEMENTATIONUSEVOLUMEDESC pDesc)
{
#ifdef LOG_ENABLED
    Log(("ISO/UDF: Implementation use volume descriptor at sector %#RX32\n", pDesc->Tag.offTag));
    if (LogIs2Enabled())
    {
        UDF_LOG2_MEMBER(pDesc, "#010RX32", uVolumeDescSeqNo);
        UDF_LOG2_MEMBER_ENTITY_ID(pDesc, idImplementation);
        if (UDF_ENTITY_ID_EQUALS(&pDesc->idImplementation, UDF_ENTITY_ID_IUVD_IMPLEMENTATION))
        {
            UDF_LOG2_MEMBER_CHARSPEC(&pDesc->ImplementationUse, Lvi.Charset);
            UDF_LOG2_MEMBER_DSTRING(&pDesc->ImplementationUse, Lvi.achVolumeID);
            UDF_LOG2_MEMBER_DSTRING(&pDesc->ImplementationUse, Lvi.achInfo1);
            UDF_LOG2_MEMBER_DSTRING(&pDesc->ImplementationUse, Lvi.achInfo2);
            UDF_LOG2_MEMBER_DSTRING(&pDesc->ImplementationUse, Lvi.achInfo3);
            UDF_LOG2_MEMBER_ENTITY_ID(&pDesc->ImplementationUse, Lvi.idImplementation);
            if (!ASMMemIsZero(&pDesc->ImplementationUse.Lvi.abUse[0], sizeof(pDesc->ImplementationUse.Lvi.abUse)))
                Log2(("ISO/UDF:   %-32s\n%.128RhxD\n", "Lvi.abUse[128]:", &pDesc->ImplementationUse.Lvi.abUse[0]));
        }
        else if (!ASMMemIsZero(&pDesc->ImplementationUse.ab[0], sizeof(pDesc->ImplementationUse.ab)))
            Log2(("ISO/UDF:   %-32s\n%.460RhxD\n", "ImplementationUse.ab[460]:", &pDesc->ImplementationUse.ab[0]));
    }
#endif

    RT_NOREF(pVdsInfo, pDesc);
    return VINF_SUCCESS;
}


/**
 * Process a VDS sequence, recursively dealing with volume descriptor pointers.
 *
 * This function only gathers information from the sequence, handling the
 * prevailing descriptor fun.
 *
 * @returns IPRT status code.
 * @param   pCtx            The UDF helper context.
 * @param   pVdsInfo        Where to gather temp info from the VDS sequence.
 * @param   offSeq          The byte offset of the sequence.
 * @param   cbSeq           The length of the sequence.
 * @param   cNestings       The VDS nesting depth.
 * @param   pVolInfo        Where to return volume information found in the VDS.
 */
static int rtFsUdfReadAndProcessVdsSeq(PCRTFSUDFHLPCTX pCtx, PRTFSUDFVDSINFO pVdsInfo, uint64_t offSeq, uint32_t cbSeq,
                                       uint32_t cNestings, PRTFSUDFVOLINFO pVolInfo)
{
    Assert(pCtx->cbBuf >= pCtx->cbSector);

    /*
     * Check nesting depth.
     */
    if (cNestings > 5)
        return RTERRINFO_LOG_SET(pCtx->pErrInfo, VERR_TOO_MUCH_DATA, "The volume descriptor sequence (VDS) is nested too deeply.");


    /*
     * Do the processing sector by sector to keep things simple.
     */
    uint32_t offInSeq = 0;
    while (offInSeq < cbSeq)
    {
        int rc;

        /*
         * Read the next sector.  Zero pad if less that a sector.
         */
        Assert((offInSeq & (pCtx->cbSector - 1)) == 0);
        rc = RTVfsFileReadAt(pCtx->hVfsBacking, offSeq + offInSeq, pCtx->pbBuf, pCtx->cbSector, NULL);
        if (RT_FAILURE(rc))
            return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, rc, "Error reading VDS content at %RX64 (LB %#x): %Rrc",
                                       offSeq + offInSeq, pCtx->cbSector, rc);
        if (cbSeq - offInSeq < pCtx->cbSector)
            memset(&pCtx->pbBuf[cbSeq - offInSeq], 0, pCtx->cbSector - (cbSeq - offInSeq));

        /*
         * Check tag.
         */
        PCUDFTAG pTag = (PCUDFTAG)pCtx->pbBuf;
        rc = RTFsUdfHlpValidateDescTagAndCrc(pTag, pCtx->cbSector, UINT16_MAX, (offSeq + offInSeq) / pCtx->cbSector,
                                             pCtx->pErrInfo);
        if (   RT_SUCCESS(rc)
            || (   rc == VERR_ISOFS_INSUFFICIENT_DATA_FOR_DESC_CRC
                && (   pTag->idTag == UDF_TAG_ID_LOGICAL_VOLUME_INTEGRITY_DESC
                    || pTag->idTag == UDF_TAG_ID_LOGICAL_VOLUME_DESC
                    || pTag->idTag == UDF_TAG_ID_UNALLOCATED_SPACE_DESC
                   )
               )
           )
        {
            switch (pTag->idTag)
            {
                case UDF_TAG_ID_PRIMARY_VOL_DESC:
                    rc = rtFsUdfProcessPrimaryVolDesc(pCtx, pVdsInfo, (PCUDFPRIMARYVOLUMEDESC)pTag);
                    break;

                case UDF_TAG_ID_IMPLEMENTATION_USE_VOLUME_DESC:
                    rc = rtFsUdfProcessImplUseVolDesc(pVdsInfo, (PCUDFIMPLEMENTATIONUSEVOLUMEDESC)pTag);
                    break;

                case UDF_TAG_ID_PARTITION_DESC:
                    rc = rtFsUdfProcessPartitionDesc(pCtx, pVdsInfo, (PCUDFPARTITIONDESC)pTag);
                    break;

                case UDF_TAG_ID_LOGICAL_VOLUME_DESC:
                    if (rc != VERR_ISOFS_INSUFFICIENT_DATA_FOR_DESC_CRC)
                        rc = rtFsUdfProcessLogicalVolumeDesc(pCtx, pVdsInfo, (PCUDFLOGICALVOLUMEDESC)pTag);
                    else
                        rc = VERR_ISOFS_TOO_BIG_PARTMAP_IN_LVD;
                    break;

                case UDF_TAG_ID_LOGICAL_VOLUME_INTEGRITY_DESC:
                    Log(("ISO/UDF: Ignoring logical volume integrity descriptor at offset %#RX64.\n", offSeq + offInSeq));
                    rc = VINF_SUCCESS;
                    break;

                case UDF_TAG_ID_UNALLOCATED_SPACE_DESC:
                    Log(("ISO/UDF: Ignoring unallocated space descriptor at offset %#RX64.\n", offSeq + offInSeq));
                    rc = VINF_SUCCESS;
                    break;

                case UDF_TAG_ID_ANCHOR_VOLUME_DESC_PTR:
                    Log(("ISO/UDF: Ignoring AVDP in VDS (at offset %#RX64).\n", offSeq + offInSeq));
                    rc = VINF_SUCCESS;
                    break;

                case UDF_TAG_ID_VOLUME_DESC_PTR:
                {
                    PCUDFVOLUMEDESCPTR pVdp = (PCUDFVOLUMEDESCPTR)pTag;
                    Log(("ISO/UDF: Processing volume descriptor pointer at offset %#RX64: %#x LB %#x (seq %#x); cNestings=%d\n",
                         offSeq + offInSeq, pVdp->NextVolumeDescSeq.off, pVdp->NextVolumeDescSeq.cb,
                         pVdp->uVolumeDescSeqNo, cNestings));
                    rc = rtFsUdfReadAndProcessVdsSeq(pCtx, pVdsInfo, (uint64_t)pVdp->NextVolumeDescSeq.off * pCtx->cbSector,
                                                     pVdp->NextVolumeDescSeq.cb, cNestings + 1, pVolInfo);
                    break;
                }

                case UDF_TAG_ID_TERMINATING_DESC:
                    Log(("ISO/UDF: Terminating descriptor at offset %#RX64\n", offSeq + offInSeq));
                    return VINF_SUCCESS;

                default:
                    return RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_ISOFS_UNEXPECTED_VDS_DESC,
                                               "Unexpected/unknown VDS descriptor %#x at byte offset %#RX64",
                                               pCtx->cbSector, offSeq + offInSeq);
            }
            if (RT_FAILURE(rc))
                return rc;
        }
        /* The descriptor sequence is usually zero padded to 16 sectors.  Just
           ignore zero descriptors. */
        else if (rc != VERR_ISOFS_TAG_IS_ALL_ZEROS)
            return rc;

        /*
         * Advance.
         */
        offInSeq += pCtx->cbSector;
    }

    return VINF_SUCCESS;
}



/**
 * Processes a volume descriptor sequence (VDS) and file set descriptor sequence
 * (FSDS).
 *
 * @returns IPRT status code.
 * @param   pCtx            The UDF helper context.
 * @param   offSeq          The byte offset of the sequence.
 * @param   cbSeq           The length of the sequence.
 * @param   pSeenSequences  Structure where to keep track of VDSes we've already
 *                          processed, to avoid redoing one that we don't
 *                          understand.
 * @param   pVolInfo        Where to return volume information found in the VDS.
 */
static int rtFsUdfReadAndProcessVdsAndFsds(PCRTFSUDFHLPCTX pCtx, uint64_t offSeq, uint32_t cbSeq,
                                           PRTFSUDFHLPSEENSEQENCES pSeenSequences, PRTFSUDFVOLINFO pVolInfo)
{
    /*
     * Skip if already seen.
     */
    uint32_t i = pSeenSequences->cSequences;
    while (i-- > 0)
        if (   pSeenSequences->aSequences[i].off == offSeq
            && pSeenSequences->aSequences[i].cb  == cbSeq)
            return VERR_NOT_FOUND;

    /* Not seen, so add it. */
    AssertReturn(pSeenSequences->cSequences + 1 <= RT_ELEMENTS(pSeenSequences->aSequences), VERR_INTERNAL_ERROR_3);
    pSeenSequences->aSequences[pSeenSequences->cSequences].cb = cbSeq;
    pSeenSequences->aSequences[pSeenSequences->cSequences].off = offSeq;
    pSeenSequences->cSequences++;

    LogFlow(("ISO/UDF: Processing anchor volume descriptor sequence at offset %#RX64 LB %#RX32\n", offSeq, cbSeq));

    /*
     * Gather relevant descriptor info from the VDS then process it and on
     * success copy it into the instance.
     *
     * The processing has to be done in a different function because there may
     * be links to sub-sequences that needs to be processed.  We do this by
     * recursing and check that we don't go to deep.
     */
    RTFSUDFVDSINFO VdsInfo = {0};

    int rc = rtFsUdfReadAndProcessVdsSeq(pCtx, &VdsInfo, offSeq, cbSeq, 0 /*cNestings*/, pVolInfo);
    if (RT_SUCCESS(rc))
    {
        rc = rtFsUdfProcessVdsSeqInfo(pCtx, &VdsInfo, pVolInfo);
        if (RT_SUCCESS(rc))
            rc = rtFsUdfProcessFileSetDescSeq(pCtx, pVolInfo);
    }

    /*
     * Clean up info.
     */
    i = VdsInfo.cPrimaryVols;
    while (i-- > 0)
        RTMemFree(VdsInfo.apPrimaryVols[i]);

    i = VdsInfo.cLogicalVols;
    while (i-- > 0)
        RTMemFree(VdsInfo.apLogicalVols[i]);

    i = VdsInfo.cPartitions;
    while (i-- > 0)
        RTMemFree(VdsInfo.apPartitions[i]);

    RTMemFree(VdsInfo.paPartMaps);

    return rc;
}


DECLHIDDEN(int) RTFsUdfHlpReadAndHandleUdfAvdp(PCRTFSUDFHLPCTX pCtx, uint64_t offAvdp, PRTFSUDFHLPSEENSEQENCES pSeenSequences,
                                               PRTFSUDFVOLINFO pVolInfo)
{
    AssertReturn(pCtx->cbSector          >= 512, VERR_INVALID_PARAMETER);
    AssertReturn(pCtx->cbMaxLogicalBlock >= pCtx->cbSector, VERR_INVALID_PARAMETER);
    AssertReturn(pCtx->cbBuf             >= pCtx->cbMaxLogicalBlock, VERR_INVALID_PARAMETER);

    /*
     * Try read the descriptor and validate its tag.
     */
    PUDFANCHORVOLUMEDESCPTR const pAvdp = (PUDFANCHORVOLUMEDESCPTR)pCtx->pbBuf;
    int rc = RTVfsFileReadAt(pCtx->hVfsBacking, offAvdp, pAvdp, pCtx->cbSector, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = RTFsUdfHlpValidateDescTagAndCrc(&pAvdp->Tag, pCtx->cbSector, UDF_TAG_ID_ANCHOR_VOLUME_DESC_PTR,
                                             offAvdp / pCtx->cbSector, pCtx->pErrInfo);
        if (RT_SUCCESS(rc))
        {
            UDFEXTENTAD const aVds[2] = { pAvdp->MainVolumeDescSeq, pAvdp->ReserveVolumeDescSeq };
            Log2(("ISO/UDF: AVDP: MainVolumeDescSeq=%#RX32 LB %#RX32, ReserveVolumeDescSeq=%#RX32 LB %#RX32\n",
                  aVds[0].off, aVds[0].cb, aVds[1].off, aVds[1].cb));

            for (unsigned i = 0; i < RT_ELEMENTS(aVds); i++)
            {
                /* Try the sequence if it looks sane. */
                if (   aVds[i].off < pCtx->cBackingSectors
                    &&     (uint64_t)aVds[i].off
                         + ((uint64_t)aVds[i].cb + pCtx->cbSector - 1) / pCtx->cbSector
                       <= pCtx->cBackingSectors)
                {
                    rc = rtFsUdfReadAndProcessVdsAndFsds(pCtx, (uint64_t)aVds[i].off * pCtx->cbSector, aVds[i].cb,
                                                         pSeenSequences, pVolInfo);
                    if (RT_SUCCESS(rc))
                        return rc;
                }
                else if (RT_SUCCESS(rc))
                    rc = RTERRINFO_LOG_SET_F(pCtx->pErrInfo, VERR_NOT_FOUND,
                                             "%sVolumeDescSeq is out of bounds: sector %#RX32 LB %#RX32 bytes, image is %#RX64 sectors",
                                             i ? "Reserve" : "Main", aVds[i].off, aVds[i].cb, pCtx->cBackingSectors);
            }
        }
    }
    else
        rc = RTERRINFO_LOG_SET_F(pCtx->pErrInfo, rc,
                                 "Error reading sector at offset %#RX64 (anchor volume descriptor pointer): %Rrc", offAvdp, rc);

    return rc;
}


/**
 * Cleans up after RTFsUdfHlpReadAndHandleUdfAvdp.
 */
DECLHIDDEN(void) RTFsUdfHlpDestroyVolInfo(PRTFSUDFVOLINFO pVolInfo)
{
    if (pVolInfo->paPartitions)
        RTMemFree(pVolInfo->paPartitions);
    pVolInfo->paPartitions = NULL;
}

