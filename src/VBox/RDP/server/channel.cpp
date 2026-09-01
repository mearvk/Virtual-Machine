/* $Id: channel.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * VBox Remote Desktop Protocol - RDP channel.
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

#include "vrdp.h"
#include "utils.h"

#define VRDP_CH_MAX_DFG_BUFFER_KEPT (_64K)

VRDPChannel::VRDPChannel ()
    :
    m_buf (NULL),
    m_length (0),
    m_size (0)
{
    int rc = VRDPLock::Create("Channel", &m_pChannelLock);
    AssertRC (rc);
}

VRDPChannel::~VRDPChannel ()
{
    VRDPMemFree (m_buf);

    VRDPLock::Delete(&m_pChannelLock);
}

bool VRDPChannel::ChannelLock (void)
{
    return RT_SUCCESS(VRDPLock::Lock(m_pChannelLock));
}

void VRDPChannel::ChannelUnlock (void)
{
    VRDPLock::Unlock(m_pChannelLock);
}

void VRDPChannel::DefragmentationBegin (uint32_t length)
{
    Assert (length);

    CHANNELLOG(("DefragmentationBegin: length = %d\n", length));

    /* Reset the current data length. */
    m_length = 0;

    if (length <= m_size)
    {
        /* There is already allocated buffer that can be reused. */
        /* do nothing */;
        CHANNELLOG(("DefragmentationBegin: using existing buffer = %p, size %d\n", m_buf, m_size));
    }
    else
    {
        /* Reallocate the buffer. */
        if (m_size)
        {
            VRDPMemFree (m_buf);
            m_size = 0;
        }

        m_buf = (uint8_t *)VRDPMemAlloc (length);

        Assert (m_buf);

        if (m_buf)
        {
            m_size = length;
        }

        CHANNELLOG(("DefragmentationBegin: allocated buffer = %p, size %d\n", m_buf, m_size));
    }
}

void VRDPChannel::DefragmentationAdd (VRDPInputCtx *pInputCtx)
{
    uint32_t u32BytesLeft = m_size - m_length;
    uint32_t u32BytesToAdd = pInputCtx->BytesToRead ();

    CHANNELLOG(("DefragmentationAdd: buffer %p, len = %d, size = %d, u32BytesLeft = %d u32BytesToAdd = %d\n", m_buf, m_length, m_size, u32BytesLeft, u32BytesToAdd));

    Assert(u32BytesLeft >= u32BytesToAdd);

    uint32_t u32BytesToCopy = RT_MIN (u32BytesLeft, u32BytesToAdd);

    if (u32BytesToCopy > 0)
    {
        const uint8_t *pu8 = pInputCtx->Read (u32BytesToCopy);
        Assert (pu8);

        memcpy (m_buf + m_length, pu8, u32BytesToCopy);

        m_length += u32BytesToCopy;
    }
}

const uint8_t *VRDPChannel::DefragmentationEnd (uint32_t *pu32Length)
{
    CHANNELLOG(("DefragmentationEnd: buffer = %p, len %d, size %d\n", m_buf, m_length, m_size));

    *pu32Length = m_length;

    return m_buf;
}

void VRDPChannel::DefragmentationRelease (void)
{
    /* Notification that the current defragmentation buffer is not needed anymore. */
    if (m_size > VRDP_CH_MAX_DFG_BUFFER_KEPT)
    {
        CHANNELLOG(("DefragmentationRelease: releasing buffer = %p, len %d, size %d\n", m_buf, m_length, m_size));
        VRDPMemFree (m_buf);
        m_buf = NULL;
        m_size = 0;
        m_length = 0;
    }
}
