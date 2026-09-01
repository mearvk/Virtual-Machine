/* $Id: udfhlp.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * IPRT - UDF Virtual Filesystem Common Code Header.
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

#ifndef IPRT_INCLUDED_SRC_common_fs_udfhlp_h
#define IPRT_INCLUDED_SRC_common_fs_udfhlp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "isoextent.h"


/** Check if an entity ID field equals the given ID string. */
#define UDF_ENTITY_ID_EQUALS(a_pEntityId, a_szId)  \
    ( memcmp(&(a_pEntityId)->achIdentifier[0], a_szId, RT_MIN(sizeof(a_szId), sizeof(a_pEntityId)->achIdentifier)) == 0 )
/** Checks if a character set indicator indicates OSTA compressed unicode. */
#define UDF_IS_CHAR_SET_OSTA(a_pCharSet) \
    (   (a_pCharSet)->uType == UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE \
     && memcmp((a_pCharSet)->abInfo, UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE_INFO, \
               sizeof(UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE_INFO)) == 0 )


/** @name UDF structure logging macros
 * @{ */
#define UDF_LOG2_MEMBER(a_pStruct, a_szFmt, a_Member) UDF_LOG2_MEMBER2(a_szFmt, #a_Member, (a_pStruct)->a_Member)
#define UDF_LOG2_MEMBER2(a_szFmt, a_szMember, a_MemberExpr) \
    Log2(("ISO/UDF:   %-32s %" a_szFmt "\n", a_szMember ":", a_MemberExpr))
#define UDF_LOG2_MEMBER2_HEX(a_cbMember, a_szMember, a_pvMember) \
    Log2(((a_cbMember) <= 16 ? "ISO/UDF:   %-32s %.*Rhxs\n" : "ISO/UDF:   %-32s\n%.*RhxD\n", \
          a_szMember ":", (a_cbMember), a_pvMember))
#define UDF_LOG2_MEMBER_EX(a_pStruct, a_szFmt, a_Member, a_cchIndent) \
    Log2(("ISO/UDF:   %*s%-32s %" a_szFmt "\n", a_cchIndent, "", #a_Member ":", (a_pStruct)->a_Member))
#define UDF_LOG2_MEMBER_ENUM(a_pStruct, a_szFmt, a_Member, a_fnToString) \
    Log2(("ISO/UDF:   %-32s %" a_szFmt " (%s)\n", #a_Member ":", (a_pStruct)->a_Member, a_fnToString((a_pStruct)->a_Member) ))
#define UDF_LOG2_MEMBER_ENTITY_ID_EX(a_pStruct, a_Member, a_cchIndent) \
    Log2(("ISO/UDF:   %*s%-32s '%.23s' fFlags=%#06x Suffix=%.8Rhxs\n", a_cchIndent, "", #a_Member ":", \
          (a_pStruct)->a_Member.achIdentifier, (a_pStruct)->a_Member.fFlags, &(a_pStruct)->a_Member.Suffix))
#define UDF_LOG2_MEMBER_ENTITY_ID(a_pStruct, a_Member) UDF_LOG2_MEMBER_ENTITY_ID_EX(a_pStruct, a_Member, 0)
#define UDF_LOG2_MEMBER_EXTENTAD(a_pStruct, a_Member) \
    Log2(("ISO/UDF:   %-32s sector %#010RX32 LB %#010RX32\n", #a_Member ":", (a_pStruct)->a_Member.off, (a_pStruct)->a_Member.cb))
#define UDF_LOG2_MEMBER_SHORTAD(a_pStruct, a_Member) \
    Log2(("ISO/UDF:   %-32s sector %#010RX32 LB %#010RX32 %s\n", #a_Member ":", (a_pStruct)->a_Member.off, (a_pStruct)->a_Member.cb, \
          (a_pStruct)->a_Member.uType == UDF_AD_TYPE_RECORDED_AND_ALLOCATED ? "alloced+recorded" \
          : (a_pStruct)->a_Member.uType == UDF_AD_TYPE_ONLY_ALLOCATED ? "alloced" \
          : (a_pStruct)->a_Member.uType == UDF_AD_TYPE_FREE ? "free" : "next" ))
#define UDF_LOG2_MEMBER_LONGAD(a_pStruct, a_Member) \
    Log2(("ISO/UDF:   %-32s partition %#RX16, block %#010RX32 LB %#010RX32 %s idUnique=%#010RX32 fFlags=%#RX16\n", #a_Member ":", \
          (a_pStruct)->a_Member.Location.uPartitionNo, (a_pStruct)->a_Member.Location.off, (a_pStruct)->a_Member.cb, \
          (a_pStruct)->a_Member.uType == UDF_AD_TYPE_RECORDED_AND_ALLOCATED ? "alloced+recorded" \
          : (a_pStruct)->a_Member.uType == UDF_AD_TYPE_ONLY_ALLOCATED ? "alloced" \
          : (a_pStruct)->a_Member.uType == UDF_AD_TYPE_FREE ? "free" : "next", \
          (a_pStruct)->a_Member.ImplementationUse.Fid.idUnique, (a_pStruct)->a_Member.ImplementationUse.Fid.fFlags ))
#define UDF_LOG2_MEMBER_LBADDR(a_pStruct, a_Member) \
    Log2(("ISO/UDF:   %-32s block %#010RX32 in partition %#06RX16\n", #a_Member ":", \
          (a_pStruct)->a_Member.off, (a_pStruct)->a_Member.uPartitionNo))

#define UDF_LOG2_MEMBER_TIMESTAMP(a_pStruct, a_Member) UDF_LOG2_MEMBER_TIMESTAMP2(#a_Member, (a_pStruct)->a_Member)
#define UDF_LOG2_MEMBER_TIMESTAMP2(a_szMember, a_MemberExpr) \
    Log2(("ISO/UDF:   %-32s %04d-%02u-%02u %02u:%02u:%02u.%02u%02u%02u offUtc=%d type=%#x\n", a_szMember ":", \
          (a_MemberExpr).iYear, (a_MemberExpr).uMonth, (a_MemberExpr).uDay, \
          (a_MemberExpr).uHour, (a_MemberExpr).uMinute, (a_MemberExpr).uSecond, \
          (a_MemberExpr).cCentiseconds, (a_MemberExpr).cHundredsOfMicroseconds, \
          (a_MemberExpr).cMicroseconds, (a_MemberExpr).offUtcInMin, (a_MemberExpr).fType ))
#define UDF_LOG2_MEMBER_CHARSPEC(a_pStruct, a_Member) \
    do { \
        if (   (a_pStruct)->a_Member.uType == UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE \
            && memcmp(&(a_pStruct)->a_Member.abInfo[0], UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE_INFO, \
                      sizeof(UDF_CHAR_SET_OSTA_COMPRESSED_UNICODE_INFO)) == 0) \
            Log2(("ISO/UDF:   %-32s OSTA COMPRESSED UNICODE INFO\n", #a_Member ":")); \
        else if (ASMMemIsZero(&(a_pStruct)->a_Member, sizeof((a_pStruct)->a_Member))) \
            Log2(("ISO/UDF:   %-32s all zeros\n", #a_Member ":")); \
        else \
            Log2(("ISO/UDF:   %-32s %#x info: %.63Rhxs\n", #a_Member ":", \
                  (a_pStruct)->a_Member.uType, (a_pStruct)->a_Member.abInfo)); \
    } while (0)
#define UDF_LOG2_MEMBER_DSTRING(a_pStruct, a_Member) \
    do { \
        if ((a_pStruct)->a_Member[0] == 8) \
            Log2(("ISO/UDF:   %-32s  8: '%s' len=%u (actual=%u)\n", #a_Member ":", &(a_pStruct)->a_Member[1], \
                  (a_pStruct)->a_Member[sizeof((a_pStruct)->a_Member) - 1], \
                  RTStrNLen(&(a_pStruct)->a_Member[1], sizeof((a_pStruct)->a_Member) - 2) + 1 )); \
        else if ((a_pStruct)->a_Member[0] == 16) \
        { \
            PCRTUTF16 pwszTmp = (PCRTUTF16)&(a_pStruct)->a_Member[1]; \
            char     *pszTmp  = NULL; \
            RTUtf16BigToUtf8Ex(pwszTmp, (sizeof((a_pStruct)->a_Member) - 2) / sizeof(RTUTF16), &pszTmp, 0, NULL); \
            Log2(("ISO/UDF:   %-32s 16: '%s' len=%u (actual=%u)\n", #a_Member ":", pszTmp, \
                  (a_pStruct)->a_Member[sizeof((a_pStruct)->a_Member) - 1], \
                  RTUtf16NLen(pwszTmp, (sizeof((a_pStruct)->a_Member) - 2) / sizeof(RTUTF16)) * sizeof(RTUTF16) + 1 /*??*/ )); \
            RTStrFree(pszTmp); \
        } \
        else if (ASMMemIsZero(&(a_pStruct)->a_Member[0], sizeof((a_pStruct)->a_Member))) \
            Log2(("ISO/UDF:   %-32s empty\n", #a_Member ":")); \
        else \
            Log2(("ISO/UDF:   %-32s bad: %.*Rhxs\n", #a_Member ":", sizeof((a_pStruct)->a_Member), &(a_pStruct)->a_Member[0] )); \
    } while (0)
/** @} */


/** @name RTFSUDF_PMAP_T_XXX
 * @{ */
#define RTFSUDF_PMAP_T_PLAIN        1
#define RTFSUDF_PMAP_T_VPM_15       2
#define RTFSUDF_PMAP_T_VPM_20       3
#define RTFSUDF_PMAP_T_SPM          4
#define RTFSUDF_PMAP_T_MPM          5
/** @} */

/**
 * Information about a logical UDF partition.
 *
 * This combins information from the partition descriptor, the UDFPARTMAPTYPE1
 * and the UDFPARTMAPTYPE2 structure.
 */
typedef struct RTFSISOVOLUDFPMAP
{
    /** Partition starting location as a byte offset. */
    uint64_t            offByteLocation;
    /** Partition starting location (logical sector number). */
    uint32_t            offLocation;
    /** Number of sectors. */
    uint32_t            cSectors;

    /** Partition descriptor index (for processing). */
    uint16_t            idxPartDesc;
    /** Offset info the map table. */
    uint16_t            offMapTable;
    /** Partition number (not index). */
    uint16_t            uPartitionNo;
    /** Partition number (not index). */
    uint16_t            uVolumeSeqNo;

    /** The access type (UDF_PART_ACCESS_TYPE_XXX). */
    uint32_t            uAccessType;
    /** Partition flags (UDF_PARTITION_FLAGS_XXX). */
    uint16_t            fFlags;
    /** RTFSUDF_PMAP_T_XXX. */
    uint8_t             bType;
    /** Set if Hdr is valid. */
    bool                fHaveHdr;
    /** Copy of UDFPARTITIONDESC::ContentsUse::Hdr. */
    UDFPARTITIONHDRDESC Hdr;

} RTFSISOVOLUDFPMAP;
/** Pointer to UDF logical partition information.  */
typedef RTFSISOVOLUDFPMAP *PRTFSISOVOLUDFPMAP;


/**
 * Information about an UDF volume (/ volume set).
 *
 * This combines information from the primary and logical descriptors.
 *
 * @note There is only one volume per volume set in the current UDF
 *       implementation.  So, this can be considered a volume and a volume set.
 */
typedef struct RTFSUDFVOLINFO
{
    /** The extent containing the file set descriptor. */
    UDFLONGAD           FileSetDescriptor;

    /** The root directory location (from the file set descriptor). */
    UDFLONGAD           RootDirIcb;
    /** Location of the system stream directory associated with the file set. */
    UDFLONGAD           SystemStreamDirIcb;

    /** The logical block size on this volume. */
    uint32_t            cbBlock;
    /** The log2 of cbBlock. */
    uint32_t            cShiftBlock;
    /** Flags (UDF_PVD_FLAGS_XXX). */
    uint16_t            fFlags;

    /** Number of partitions mapp in this volume. */
    uint16_t            cPartitions;
    /** Partitions in this volume. */
    PRTFSISOVOLUDFPMAP  paPartitions;

    /** The volume ID string. */
    UDFDSTRING          achLogicalVolumeID[128];
} RTFSUDFVOLINFO;
/** Pointer to info about an UDF volume. */
typedef RTFSUDFVOLINFO *PRTFSUDFVOLINFO;
/** Pointer to const info about an UDF volume. */
typedef RTFSUDFVOLINFO const *PCRTFSUDFVOLINFO;


/**
 * Info gathered from a VDS sequence.
 */
typedef struct RTFSUDFVDSINFO
{
    /** Number of entries in apPrimaryVols. */
    uint32_t                cPrimaryVols;
    /** Number of entries in apLogicalVols. */
    uint32_t                cLogicalVols;
    /** Number of entries in apPartitions. */
    uint32_t                cPartitions;
    /** Pointer to primary volume descriptors (native endian). */
    PUDFPRIMARYVOLUMEDESC   apPrimaryVols[8];
    /** Pointer to logical volume descriptors (native endian). */
    PUDFLOGICALVOLUMEDESC   apLogicalVols[8];
    /** Pointer to partition descriptors (native endian). */
    PUDFPARTITIONDESC       apPartitions[16];

    /** Created after scanning the sequence (here for cleanup purposes). */
    PRTFSISOVOLUDFPMAP      paPartMaps;
} RTFSUDFVDSINFO;
/** Pointer to VDS sequence info. */
typedef RTFSUDFVDSINFO *PRTFSUDFVDSINFO;


/**
 * VDS sequences already processed.
 *
 * The call of RTFsUdfHlpReadAndHandleUdfAvdp shall zero this structure before
 * the first call.  It requires no cleanup.
 */
typedef struct RTFSUDFHLPSEENSEQENCES
{
    /** Number of sequences we've seen thus far. */
    uint32_t cSequences;
    /** The per sequence data. */
    struct
    {
        uint64_t off;   /**< Byte offset of the sequence. */
        uint32_t cb;    /**< Size of the sequence. */
    } aSequences[8];
} RTFSUDFHLPSEENSEQENCES;
/** Pointer to VDS sequences already seen and processed. */
typedef RTFSUDFHLPSEENSEQENCES *PRTFSUDFHLPSEENSEQENCES;


/**
 * UDF Helper Context.
 */
typedef struct RTFSUDFHLPCTX
{
    /** The file, partition, or whatever backing the UDF volume. */
    RTVFSFILE           hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t            cbBacking;
    /** The size of the backing thingy in sectors (cbSector). */
    uint64_t            cBackingSectors;
    /** The sector size (in bytes). */
    uint32_t            cbSector;
    /** Max logical block size. */
    uint32_t            cbMaxLogicalBlock;
    /** Pointer to a buffer that's at least cbSector bytes large. */
    uint8_t            *pbBuf;
    /** Actual buffer size. */
    size_t              cbBuf;
    /** Pointer to an error info buffer (optional). */
    PRTERRINFO          pErrInfo;
} RTFSUDFHLPCTX;
/** Pointer to an UDF helper context. */
typedef RTFSUDFHLPCTX const *PCRTFSUDFHLPCTX;


#ifdef LOG_ENABLED
DECLHIDDEN(void) RTFsUdfHlpLogDirContent(uint8_t const *pbDir, size_t cbDir);
#endif

DECLHIDDEN(int)  RTFsUdfHlpVpRead(PCRTFSUDFVOLINFO pVolInfo, RTVFSFILE hVfsBacking,  uint32_t idxPart, uint32_t idxBlock,
                                  void *pvBuf, size_t cbToRead, uint64_t offByteAddend = 0);
DECLHIDDEN(int)  RTFsUdfHlpReadObject(PCRTFSUDFVOLINFO pVolInfo, RTVFSFILE hVfsBacking, uint64_t cbObject,
                                      uint32_t cExtents, PCRTFSISOEXTENT pFirstExtent, PCRTFSISOEXTENT paFurtherExtents,
                                      uint64_t offRead, void *pvBuf, size_t cbToRead, size_t *pcbRead, size_t *poffPosMov);

DECLHIDDEN(int)  RTFsUdfHlpValidateDescTag(PCUDFTAG pTag, uint16_t idTag, uint32_t offTag, PRTERRINFO pErrInfo);
DECLHIDDEN(int)  RTFsUdfHlpValidateDescCrc(PCUDFTAG pTag, size_t cbDesc, PRTERRINFO pErrInfo);
DECLHIDDEN(int)  RTFsUdfHlpValidateDescTagAndCrc(PCUDFTAG pTag, size_t cbDesc, uint16_t idTag, uint32_t offTag, PRTERRINFO pErrInfo);

DECLHIDDEN(int)  RTFsUdfHlpReadAndHandleUdfAvdp(PCRTFSUDFHLPCTX pCtx, uint64_t offAvdp, PRTFSUDFHLPSEENSEQENCES pSeenSequences,
                                                PRTFSUDFVOLINFO pVolInfo);
DECLHIDDEN(void) RTFsUdfHlpDestroyVolInfo(PRTFSUDFVOLINFO pVolInfo);

/**
 * RTFsUdfReadIcbRecursive callback for file entries.
 *
 * @returns IPRT status code.
 * @param   pVolInfo        The volume information (partition map ++).
 * @param   pFileEntry      The file entry.
 * @param   idxDefaultPart  The default data partition.
 * @param   pvUser          The user argument.
 */
typedef DECLCALLBACKTYPE(int, FNFSUDFREADICBFILENTRY, (PCRTFSUDFVOLINFO pVolInfo, PCUDFFILEENTRY pFileEntry,
                                                       uint16_t idxDefaultPart, void *pvUser));
/*** Pointer to a RTFsUdfReadIcbRecursive callback for file entries. */
typedef FNFSUDFREADICBFILENTRY *PFNFSUDFREADICBFILENTRY;

/**
 * RTFsUdfReadIcbRecursive callback for extended file entries.
 *
 * @returns IPRT status code.
 * @param   pVolInfo        The volume information (partition map ++).
 * @param   pFileEntry      The extended file entry.
 * @param   idxDefaultPart  The default data partition.
 * @param   pvUser          The user argument.
 */
typedef DECLCALLBACKTYPE(int, FNFSUDFREADICBEXFILENTRY, (PCRTFSUDFVOLINFO pVolInfo, PCUDFEXFILEENTRY pFileEntry,
                                                         uint16_t idxDefaultPart, void *pvUser));
/*** Pointer to a RTFsUdfReadIcbRecursive callback for extended file entries. */
typedef FNFSUDFREADICBEXFILENTRY *PFNFSUDFREADICBEXFILENTRY;

DECLHIDDEN(int)  RTFsUdfReadIcbRecursive(PCRTFSUDFVOLINFO pVolInfo, RTVFSFILE hVfsBacking, uint8_t *pbBuf, uint32_t cNestings,
                                         PFNFSUDFREADICBFILENTRY pfnFileEntry, PFNFSUDFREADICBEXFILENTRY pfnExFileEntry,
                                         void *pvUser, uint32_t *pcProcessed, uint32_t *pcIndirections, UDFLONGAD AllocDesc);
DECLHIDDEN(int)  RTFsUdfHlpIcbStuffToFileMode(uint32_t fIcbTagFlags, uint8_t bFileType, uint32_t fPermission, PRTFMODE pfAttrib);
DECLHIDDEN(void) RTFsUdfHlpTimestamp2TimeSpec(PRTTIMESPEC pTimeSpec, PCUDFTIMESTAMP pUdf);
DECLHIDDEN(int)  RTFsUdfHlpGatherExtentsFromIcb(PCRTFSUDFVOLINFO pVolInfo, uint8_t const *pbAllocDescs, uint32_t cbAllocDescs,
                                                uint32_t fIcbTagFlags, uint32_t idxDefaultPart, uint64_t offAllocDescs,
                                                uint64_t cbMax, RTVFSFILE hVfsBacking, uint8_t *pbBuf,
                                                uint32_t *pcExtents, PRTFSISOEXTENT pFirstExtent, PRTFSISOEXTENT *ppaExtents);
DECLHIDDEN(void) RTFsUdfHlpFreeGatherExtents(PRTFSISOEXTENT paExtents);

DECLHIDDEN(int)  RTFsUdfHlpDStringFieldToUtf8(const char *pachSrc, size_t cbSrc, char **ppszDst);
DECLHIDDEN(int)  RTFsUdfHlpDStringFieldToUtf8Buf(const char *pachSrc, size_t cbSrc, void *pvDst, size_t cbDst, size_t *pcbRet);


#endif /* !IPRT_INCLUDED_SRC_common_fs_udfhlp_h */
