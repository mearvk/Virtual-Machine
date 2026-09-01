/* $Id: VBoxServicePropCache.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBoxServicePropCache - Guest property cache.
 *
 * This is used to reduce host calls setting the same data and to clean up
 * properly when the service shuts down.
 */

/*
 * Copyright (C) 2010-2025 Oracle and/or its affiliates.
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
#include <iprt/assert.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestPropertySvc.h> /* For GUEST_PROP_MAX_VALUE_LEN */

#include "VBoxServiceInternal.h"


/**
 * Searches a property within a property cache.
 *
 * @returns A pointer to the found property cache entry on success, or NULL if not found.
 * @param   pCache          The property cache.
 * @param   pszName         Name of property to search for. Case sensitive.
 */
static PVBOXSERVICEVEPROPCACHEENTRY
vgsvcPropCacheFindInternalLocked(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName)
{
    /** @todo This is a O(n) lookup, maybe improve this later to O(1) using a
     *        map.
     *  r=bird: Use a string space (RTstrSpace*). That is O(log n) in its current
     *        implementation (AVL tree). However, this is not important at the
     *        moment. */
    PVBOXSERVICEVEPROPCACHEENTRY pNodeIt;
    RTListForEach(&pCache->NodeHead, pNodeIt, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc)
    {
        if (strcmp(pNodeIt->pszName, pszName) == 0)
            return pNodeIt;
    }
    return NULL;
}


/**
 * Inserts (appends) a property into a property cache.
 *
 * Caller must first make sure the name isn't in the cache already.
 *
 * @returns A pointer to the inserted property cache entry on success, or NULL on failure.
 * @param   pCache          The property cache.
 * @param   pszName         Name of property to insert. Case sensitive.
 */
static PVBOXSERVICEVEPROPCACHEENTRY vgsvcPropCacheInsertEntryInternalLocked(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName)
{
    PVBOXSERVICEVEPROPCACHEENTRY pNode = (PVBOXSERVICEVEPROPCACHEENTRY)RTMemAlloc(sizeof(VBOXSERVICEVEPROPCACHEENTRY));
    if (pNode)
    {
        /* The entry defaults to temporary w/ deletion-on-service-termination
           and TRANSRESET.  If anything else is desired, declare it using
           VGSvcPropCacheDeclareEntry or use VGSvcPropCacheUpdateEx for updating. */
        pNode->fFlags        = VGSVCPROPCACHE_FLAGS_TEMPORARY | VGSVCPROPCACHE_FLAGS_TRANSIENT;
        pNode->pszValueReset = NULL;
        pNode->pszValue      = NULL;
        pNode->pszName       = RTStrDup(pszName);
        pNode->fNotUpdated   = false;
        AssertPtrReturnStmt(pNode->pszName, RTMemFree(pNode), NULL);

        RTListAppend(&pCache->NodeHead, &pNode->NodeSucc);
    }
    return pNode;
}


/**
 * Writes a new value to a property.
 *
 * @returns VBox status code.
 * @param   pClient         The guest property client session info.
 * @param   pszName         Name of property to write value for. Case sensitive.
 * @param   fFlags          Property cache flags of type VGSVCPROPCACHE_FLAGS_XXX.
 * @param   pszValue        The value to write, NULL to delete.
 */
static int vgsvcPropCacheWriteProp(PVBGLGSTPROPCLIENT pClient, const char *pszName, uint32_t fFlags, const char *pszValue)
{
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

    int rc;
    if (pszValue != NULL)
    {
        if (fFlags & VGSVCPROPCACHE_FLAGS_TRANSIENT)
        {
            /*
             * Because a value can be temporary we have to make sure it also
             * gets deleted when the property cache did not have the chance to
             * gracefully clean it up (due to a hard VM reset etc), so set this
             * guest property using the TRANSRESET flag..
             */
            rc = VbglGuestPropWrite(pClient, pszName, pszValue, "TRANSRESET");
            if (rc == VERR_PARSE_ERROR)
            {
                /* Host does not support the "TRANSRESET" flag, so only
                 * use the "TRANSIENT" flag -- better than nothing :-). */
                rc = VbglGuestPropWrite(pClient, pszName, pszValue, "TRANSIENT");
                /** @todo r=bird: Remember that the host doesn't support this. */
            }
        }
        else
            rc = VbglGuestPropWriteValue(pClient, pszName, pszValue); /* no flags */
    }
    else
        rc = VbglGuestPropWriteValue(pClient, pszName, NULL);
    return rc;
}


#if 0 /* unused */
/**
 * Writes a new value to a property, using a format value.
 *
 * @returns VBox status code.
 * @param   pClient         The guest property client session info.
 * @param   pszName         Name of property to write value for. Case sensitive.
 * @param   fFlags          Property cache flags of type VGSVCPROPCACHE_FLAGS_XXX.
 * @param   pszValueFormat  Format string of value to write.
 */
static int vgsvcPropCacheWritePropF(PVBGLGSTPROPCLIENT pClient, const char *pszName, uint32_t fFlags,
                                    const char *pszValueFormat, ...)
{
    int rc;
    if (pszValueFormat != NULL)
    {
        va_list va;
        va_start(va, pszValueFormat);

        char *pszValue;
        if (RTStrAPrintfV(&pszValue, pszValueFormat, va) >= 0)
        {
            rc = vgsvcPropCacheWriteProp(pClient, pszName, fFlags, pszValue);
            RTStrFree(pszValue);
        }
        else
            rc = VERR_NO_MEMORY;
        va_end(va);
    }
    else
        rc = VbglGuestPropWriteValue(pClient, pszName, NULL);
    return rc;
}
#endif


/**
 * Initializes a property cache.
 *
 * @returns VBox status code.
 * @param   pCache          Pointer to the cache.
 * @param   pClient         The guest property client session info.
 */
int VGSvcPropCacheInit(PVBOXSERVICEVEPROPCACHE pCache, PVBGLGSTPROPCLIENT pClient)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    Assert(pCache->pClient == NULL);

    RTListInit(&pCache->NodeHead);
    int rc = RTCritSectInit(&pCache->CritSect);
    if (RT_SUCCESS(rc))
        pCache->pClient = pClient;
    return rc;
}


/**
 * Core of VGSvcPropCacheUpdateEntry shared with VGSvcPropCacheUpdateEx.
 */
static int vgsvcPropCacheUpdateDeclaration(PVBOXSERVICEVEPROPCACHEENTRY pNode, uint32_t fFlags, const char *pszValueReset)
{
    pNode->fFlags = fFlags;
    if (  pszValueReset != NULL
        ? pNode->pszValueReset == NULL || strcmp(pszValueReset, pNode->pszValueReset) != 0
        : pNode->pszValueReset != NULL)
    {
        if (pNode->pszValueReset)
        {
            RTStrFree(pNode->pszValueReset);
            pNode->pszValueReset = NULL;
        }
        if (pszValueReset)
        {
            pNode->pszValueReset = RTStrDup(pszValueReset);
            AssertReturn(pNode->pszValueReset, VERR_NO_STR_MEMORY);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Declares a cache entry, typeically a beacon, setting flags and termination
 * behaviour.
 *
 * This can also be used to modify the declaration of existing entries,
 * unlike VGSvcPropCacheUpdateEx().
 *
 * @note    Don't use this on values which may be deleted during updating, as it
 *          might cause stale (prior to service launch) values not be deleted as
 *          they should.
 *
 * @returns VBox status code.
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   fFlags          The property flags to set.
 * @param   pszValueReset   The property reset value (only applicable if
 *                          VGSVCPROPCACHE_FLAGS_TEMPORARY is set).
 */
int VGSvcPropCacheDeclareEntry(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t fFlags,
                               const char *pszValueReset /*= NULL*/)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    Assert(!pszValueReset || (fFlags & VGSVCPROPCACHE_FLAGS_TEMPORARY));

    int rc = RTCritSectEnter(&pCache->CritSect);
    AssertRCReturn(rc, rc);

    PVBOXSERVICEVEPROPCACHEENTRY pNode = vgsvcPropCacheFindInternalLocked(pCache, pszName);
    if (pNode == NULL)
        pNode = vgsvcPropCacheInsertEntryInternalLocked(pCache, pszName);
    if (pNode != NULL)
        rc = vgsvcPropCacheUpdateDeclaration(pNode, fFlags, pszValueReset);
    else
        rc = VERR_NO_MEMORY;

    RTCritSectLeave(&pCache->CritSect);
    return rc;
}


/**
 * Core of VGSvcPropCacheUpdate.
 */
static int vgsvcPropCacheUpdateNode(PVBOXSERVICEVEPROPCACHE pCache, PVBOXSERVICEVEPROPCACHEENTRY pNode,
                                    const char *pszValue, bool fNew)
{
    int rc = VINF_SUCCESS;
    if (pszValue) /* Do we have a value to check for? */
    {
        bool fUpdate = fNew;
        /* Always update this property, no matter what? */
        if (pNode->fFlags & VGSVCPROPCACHE_FLAGS_ALWAYS_UPDATE)
            fUpdate = true;
        /* Did the value change so we have to update? */
        else if (pNode->pszValue && strcmp(pNode->pszValue, pszValue) != 0)
            fUpdate = true;
        /* No value stored at the moment but we have a value now? */
        else if (pNode->pszValue == NULL)
            fUpdate = true;

        if (fUpdate)
        {
            /* Write the update. */
            rc = vgsvcPropCacheWriteProp(pCache->pClient, pNode->pszName, pNode->fFlags, pszValue);
            VGSvcVerbose(4, "[PropCache %p]: Written '%s'='%s' (flags: %x), rc=%Rrc\n",
                         pCache, pNode->pszName, pszValue, pNode->fFlags, rc);
            if (RT_SUCCESS(rc)) /* Only update the node's value on successful write. */
            {
                RTStrFree(pNode->pszValue);
                pNode->pszValue = RTStrDup(pszValue);
                if (!pNode->pszValue)
                    rc = VERR_NO_STR_MEMORY;
            }
        }
        else
            rc = VINF_NO_CHANGE; /* No update needed. */
    }
    else
    {
        /* No value specified. Deletion (or no action required). */
        if (pNode->pszValue) /* Did we have a value before? Then the value needs to be deleted. */
        {
            rc = vgsvcPropCacheWriteProp(pCache->pClient, pNode->pszName, 0, /*fFlags*/ NULL /*pszValue*/);
            VGSvcVerbose(4, "[PropCache %p]: Deleted '%s'='%s' (flags: %x), rc=%Rrc\n",
                         pCache, pNode->pszName, pNode->pszValue, pNode->fFlags, rc);
            if (RT_SUCCESS(rc)) /* Only delete property value on successful Vbgl deletion. */
            {
                /* Delete property (but do not remove from cache) if not deleted yet. */
                RTStrFree(pNode->pszValue);
                pNode->pszValue = NULL;
            }
        }
        else if (!fNew)
            rc = VINF_NO_CHANGE; /* No update needed. */
        else
        {
            rc = vgsvcPropCacheWriteProp(pCache->pClient, pNode->pszName, 0, /*fFlags*/ NULL /*pszValue*/);
            VGSvcVerbose(4, "[PropCache %p]: Deleted new entry '%s' (flags: %x), rc=%Rrc\n",
                         pCache, pNode->pszName, pNode->fFlags, rc);
        }
    }

    /* Clear the not-updated flag regardless of outcome. */
    pNode->fNotUpdated = false;

    return rc;
}


/**
 * Creates/Updates the locally cached value and writes it to the host if modified.
 *
 * @note    New entries defaults to temporary w/ deletion-on-service-termination
 *          and have the TRANSRESET flag set.  Use VGSvcPropCacheDeclareEntry or
 *          VGSvcPropCacheUpdateEx to control the flags and termination
 *          behaviour explicitly.
 *
 * @returns VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the property name or value exceeds the limit.
 * @retval  VINF_NO_CHANGE if the value is the same and nothing was written.
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   pszValue        The property value.  If this is NULL then the
 *                          property will be deleted (if possible).
 */
int VGSvcPropCacheUpdate(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValue)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

    AssertPtr(pCache->pClient);

    if (RTStrNLen(pszName, GUEST_PROP_MAX_NAME_LEN) > GUEST_PROP_MAX_NAME_LEN - 1 /* Terminator */)
        return VERR_BUFFER_OVERFLOW;

    /*
     * Lock the cache.
     */
    int rc = RTCritSectEnter(&pCache->CritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /*
         * Find the cache entry, create a new one if necessary, then update it.
         */
        PVBOXSERVICEVEPROPCACHEENTRY pNode = vgsvcPropCacheFindInternalLocked(pCache, pszName);
        bool const                   fNew  = pNode == NULL;
        if (pNode == NULL)
            pNode = vgsvcPropCacheInsertEntryInternalLocked(pCache, pszName);
        if (pNode != NULL)
            rc = vgsvcPropCacheUpdateNode(pCache, pNode, pszValue, fNew);
        else
            rc = VERR_NO_MEMORY;

        /*
         * Release cache.
         */
        RTCritSectLeave(&pCache->CritSect);
    }

    VGSvcVerbose(4, "[PropCache %p]: Updating '%s' resulted in rc=%Rrc\n", pCache, pszName, rc);
    return rc;
}


/**
 * Creates/Updates the locally cached value and writes it to the host if modified.
 *
 * @note    New entries defaults to temporary w/ deletion-on-service-termination
 *          and have the TRANSRESET flag set.  Use VGSvcPropCacheDeclareEntry or
 *          VGSvcPropCacheUpdateEx to control the flags and termination
 *          behaviour explicitly.
 *
 * @returns VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the property name or value exceeds the limit.
 * @retval  VINF_NO_CHANGE if the value is the same and nothing was written.
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   pszValueFormat  The property format string.  If this is NULL then
 *                          the property will be deleted (if possible).
 * @param   ...             Format arguments.
 */
int VGSvcPropCacheUpdateF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValueFormat, ...)
{
    if (pszValueFormat)
    {
        char    szValue[GUEST_PROP_MAX_VALUE_LEN];
        va_list va;
        va_start(va, pszValueFormat);
        ssize_t cchValue = RTStrPrintf2V(szValue, sizeof(szValue), pszValueFormat, va);
        va_end(va);
        if (cchValue >= 0)
            return VGSvcPropCacheUpdate(pCache, pszName, szValue);
        return VERR_BUFFER_OVERFLOW;
    }
    return VGSvcPropCacheUpdate(pCache, pszName, NULL);
}


/**
 * Creates/Updates the locally cached value and writes it to the host if
 * modified, extended version.
 *
 * @returns VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the property name or value exceeds the limit.
 * @retval  VINF_NO_CHANGE if the value is the same and nothing was written.
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   pszValue        The property value.  If this is NULL then the
 *                          property will be deleted (if possible).
 * @param   fFlags          The entry flags for new entries,
 *                          VGSVCPROPCACHE_FLAGS_XXX.
 * @param   pszValueReset   The property reset value (only applicable if
 *                          VGSVCPROPCACHE_FLAGS_TEMPORARY is set) for new
 *                          entries.
 */
int VGSvcPropCacheUpdateEx(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, const char *pszValue,
                           uint32_t fFlags, const char *pszValueReset)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    AssertPtr(pCache->pClient);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(!pszValueReset || (fFlags & VGSVCPROPCACHE_FLAGS_TEMPORARY), VERR_INVALID_PARAMETER);

    if (RTStrNLen(pszName, GUEST_PROP_MAX_NAME_LEN) > GUEST_PROP_MAX_NAME_LEN - 1 /* Terminator */)
        return VERR_BUFFER_OVERFLOW;

    /*
     * Lock the cache.
     */
    int rc = RTCritSectEnter(&pCache->CritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /*
         * Find the cache entry, create a new one if necessary, then update it.
         */
        PVBOXSERVICEVEPROPCACHEENTRY pNode = vgsvcPropCacheFindInternalLocked(pCache, pszName);
        bool const                   fNew  = pNode == NULL;
        if (pNode == NULL)
            pNode = vgsvcPropCacheInsertEntryInternalLocked(pCache, pszName);
        if (pNode != NULL)
        {
            if (!fNew) /* Only update these when new. */
            {
                Assert(pNode->fFlags == fFlags);
                Assert(!pszValueReset ? !pNode->pszValueReset : RTStrCmp(pszValueReset, pNode->pszValueReset) == 0);
                rc = vgsvcPropCacheUpdateNode(pCache, pNode, pszValue, fNew);
            }
            else
            {
                rc = vgsvcPropCacheUpdateDeclaration(pNode, fFlags, pszValueReset);
                int rc2 = vgsvcPropCacheUpdateNode(pCache, pNode, pszValue, true /*fNew*/);
                if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                    rc = rc2;
            }
        }
        else
            rc = VERR_NO_MEMORY;

        /*
         * Release cache.
         */
        RTCritSectLeave(&pCache->CritSect);
    }

    VGSvcVerbose(4, "[PropCache %p]: Updating '%s' resulted in rc=%Rrc\n", pCache, pszName, rc);
    return rc;
}


/**
 * Creates/Updates the locally cached value and writes it to the host if modified.
 *
 * @returns VBox status code.
 * @retval  VERR_BUFFER_OVERFLOW if the property name or value exceeds the limit.
 * @retval  VINF_NO_CHANGE if the value is the same and nothing was written.
 * @param   pCache          The property cache.
 * @param   pszName         The property name.
 * @param   fFlags          The entry flags for new entries,
 *                          VGSVCPROPCACHE_FLAGS_XXX.
 * @param   pszValueReset   The property reset value (only applicable if
 *                          VGSVCPROPCACHE_FLAGS_TEMPORARY is set) for new
 *                          entries.
 * @param   pszValueFormat  The property format string.  If this is NULL then
 *                          the property will be deleted (if possible).
 * @param   ...             Format arguments.
 */
int VGSvcPropCacheUpdateExF(PVBOXSERVICEVEPROPCACHE pCache, const char *pszName, uint32_t fFlags, const char *pszValueReset,
                            const char *pszValueFormat, ...)
{
    if (pszValueFormat)
    {
        char    szValue[GUEST_PROP_MAX_VALUE_LEN];
        va_list va;
        va_start(va, pszValueFormat);
        ssize_t cchValue = RTStrPrintf2V(szValue, sizeof(szValue), pszValueFormat, va);
        va_end(va);
        if (cchValue >= 0)
            return VGSvcPropCacheUpdateEx(pCache, pszName, szValue, fFlags, pszValueReset);
        return VERR_BUFFER_OVERFLOW;
    }
    return VGSvcPropCacheUpdateEx(pCache, pszName, NULL, fFlags, pszValueReset);
}


/**
 * Marks entries starting with @a pszPath as not-updated.
 *
 * VGSvcPropCachedDeleteNotUpdated is used to delete the stale entries when the
 * updating is completed.
 *
 * @returns VBox status code.
 * @param   pCache          The property cache.
 * @param   pszPath         The path prefix to match against.
 */
int VGSvcPropCacheMarkNotUpdatedByPath(PVBOXSERVICEVEPROPCACHE pCache, const char *pszPath)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);
    size_t const cchPath = strlen(pszPath);

    int rc = RTCritSectEnter(&pCache->CritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICEVEPROPCACHEENTRY pNodeIt;
        RTListForEach(&pCache->NodeHead, pNodeIt, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc)
        {
            if (strncmp(pNodeIt->pszName, pszPath, cchPath) == 0)
                pNodeIt->fNotUpdated = true;
        }
        RTCritSectLeave(&pCache->CritSect);
    }
    return rc;
}


/**
 * Deletes stale entries that have been explictily marked not-updated.
 *
 * This should be called near the very end of the property updating.
 *
 * @returns VBox status code.
 * @param   pCache          The property cache.
 */
int VGSvcPropCachedDeleteNotUpdated(PVBOXSERVICEVEPROPCACHE pCache)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pCache->CritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICEVEPROPCACHEENTRY pNodeIt;
        RTListForEach(&pCache->NodeHead, pNodeIt, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc)
        {
            if (pNodeIt->fNotUpdated)
            {
                if (pNodeIt->pszValue)
                {
                    int rc2 = vgsvcPropCacheWriteProp(pCache->pClient, pNodeIt->pszName, 0, /*fFlags*/ NULL /*pszValue*/);
                    VGSvcVerbose(4, "[PropCache %p]: Deleted '%s'='%s' (stale, flags: %x), rc=%Rrc\n",
                                 pCache, pNodeIt->pszName, pNodeIt->pszValue, pNodeIt->fFlags, rc2);
                    if (RT_SUCCESS(rc2)) /* Only delete property value on successful Vbgl deletion. */
                    {
                        /* Delete property (but do not remove from cache) if not deleted yet. */
                        RTStrFree(pNodeIt->pszValue);
                        pNodeIt->pszValue = NULL;
                        pNodeIt->fNotUpdated = false;
                    }
                    else if (RT_SUCCESS(rc))
                        rc = rc2;
                }
                else
                    pNodeIt->fNotUpdated = false;
            }
        }
        RTCritSectLeave(&pCache->CritSect);
    }
    return rc;
}


/**
 * Flushes the cache by writing every item regardless of its state.
 *
 * @returns VBox status code.
 * @param   pCache          The property cache.
 */
int VGSvcPropCacheFlush(PVBOXSERVICEVEPROPCACHE pCache)
{
    AssertPtrReturn(pCache, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pCache->CritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICEVEPROPCACHEENTRY pNodeIt;
        RTListForEach(&pCache->NodeHead, pNodeIt, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc)
        {
            int rc2 = vgsvcPropCacheWriteProp(pCache->pClient, pNodeIt->pszName, pNodeIt->fFlags, pNodeIt->pszValue);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
        }
        RTCritSectLeave(&pCache->CritSect);
    }
    return rc;
}


/**
 * Terminates the property cache, deleting/resetting all temporary properties.
 *
 * @param   pCache          The property cache.
 */
void VGSvcPropCacheTerm(PVBOXSERVICEVEPROPCACHE pCache)
{
    AssertPtrReturnVoid(pCache);
    AssertReturnVoid(pCache->pClient);

    /* Lock the cache. */
    int rc = RTCritSectEnter(&pCache->CritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /* Destroy all the entries, writing the reset value for the temporary ones. */
        PVBOXSERVICEVEPROPCACHEENTRY pNode, pNextNode;
        RTListForEachSafe(&pCache->NodeHead, pNode, pNextNode, VBOXSERVICEVEPROPCACHEENTRY, NodeSucc)
        {
            AssertPtr(pNode->pszName);
            RTListNodeRemove(&pNode->NodeSucc);

            if (pNode->fFlags & VGSVCPROPCACHE_FLAGS_TEMPORARY)
                vgsvcPropCacheWriteProp(pCache->pClient, pNode->pszName, pNode->fFlags, pNode->pszValueReset);

            RTStrFree(pNode->pszName);
            pNode->pszName       = NULL;
            RTStrFree(pNode->pszValue);
            pNode->pszValue      = NULL;
            RTStrFree(pNode->pszValueReset);
            pNode->pszValueReset = NULL;
            pNode->fFlags        = 0;
            RTMemFree(pNode);
        }

        RTCritSectLeave(&pCache->CritSect);
    }

    /* Destroy critical section. */
    RTCritSectDelete(&pCache->CritSect);
    pCache->pClient = NULL;
}

