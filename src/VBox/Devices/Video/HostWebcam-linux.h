/*$Id: HostWebcam-linux.h 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $*/
/** @file
 * ???
 */

/*
 * Copyright (C) 2013-2025 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_SRC_Video_HostWebcam_linux_h
#define VBOX_INCLUDED_SRC_Video_HostWebcam_linux_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <linux/videodev2.h>

#ifndef VIDIOC_ENUM_FRAMESIZES /* introduced in 2.6.19 */

#define VIDIOC_ENUM_FRAMESIZES  _IOWR('V', 74, struct v4l2_frmsizeenum)
#define VIDIOC_ENUM_FRAMEINTERVALS _IOWR('V', 75, struct v4l2_frmivalenum)

enum v4l2_frmsizetypes
{
    V4L2_FRMSIZE_TYPE_DISCRETE       = 1,
    V4L2_FRMSIZE_TYPE_CONTINUOUS     = 2,
    V4L2_FRMSIZE_TYPE_STEPWISE       = 3
};

struct v4l2_frmsize_discrete
{
    uint32_t  width;
    uint32_t  height;
};

struct v4l2_frmsize_stepwise
{
    uint32_t  min_width;
    uint32_t  max_width;
    uint32_t  step_width;
    uint32_t  min_height;
    uint32_t  max_height;
    uint32_t  step_height;
};

struct v4l2_frmsizeenum
{
    uint32_t  index;
    uint32_t  pixel_format;
    uint32_t  type;

    union
    {
        struct v4l2_frmsize_discrete discrete;
        struct v4l2_frmsize_stepwise stepwise;
    };

    uint32_t  reserved[2];
};

enum v4l2_frmivaltypes
{
    V4L2_FRMIVAL_TYPE_DISCRETE       = 1,
    V4L2_FRMIVAL_TYPE_CONTINUOUS     = 2,
    V4L2_FRMIVAL_TYPE_STEPWISE       = 3
};

struct v4l2_frmival_stepwise
{
    struct v4l2_fract   min;
    struct v4l2_fract   max;
    struct v4l2_fract   step;
};

struct v4l2_frmivalenum
{
    uint32_t  index;
    uint32_t  pixel_format;
    uint32_t  width;
    uint32_t  height;
    uint32_t  type;

    union
    {
        struct v4l2_fract            discrete;
        struct v4l2_frmival_stepwise stepwise;
    };

    uint32_t   reserved[2];
};

#endif /* !VIDIOC_ENUM_FRAMESIZES */

#ifndef V4L2_CAP_META_CAPTURE /* Since kernel 4.12.0. */
# define V4L2_CAP_META_CAPTURE (0x00800000)
#endif

#ifndef KERNEL_VERSION
# define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

/* In kernel 4.12.0, V4L stack introduced V4L2_CAP_META_CAPTURE marco.
 * Devices which report this capability do not provide actual image data.
 * Therefore, such devices need to be filtered out when discovering all
 * the available V4L devices in system.
 *
 * Original struct @v4l2_capability was extended with field @device_caps
 * in kernel 3.4.0. This field reflects actual capabilities of /dev/videoX
 * and required to skip V4L META devices when iterating over the list.
 *
 * To filter META devices, we only use @device_caps if device reports
 * V4L API version >= 4.12.0 in @version field. According to kernel
 * documentation, starting from kernel 3.1 @version field
 * in struct v4l2_capability should generally match to kernel version.
 *
 * https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/querycap.html.
 */
struct v4l2_capability_3_4_0
{
    uint8_t    driver[16];
    uint8_t    card[32];
    uint8_t    bus_info[32];
    uint32_t   version;
    uint32_t   capabilities;
    uint32_t   device_caps;
    uint32_t   reserved[3];
};

/* This macro represents LINUX_VERSION_CODE macro which corresponds
 * to 4.12.0 kernel. Field @device_caps of struct v4l2_capability_3_4 should
 * only be used if its @version >= VBOX_WITH_V4L2_CAP_META_CAPTURE.
 */
#define VBOX_WITH_V4L2_CAP_META_CAPTURE KERNEL_VERSION(4,12,0)

#endif /* !VBOX_INCLUDED_SRC_Video_HostWebcam_linux_h */

