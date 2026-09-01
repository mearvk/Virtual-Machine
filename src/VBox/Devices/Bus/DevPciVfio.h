/* $Id: DevPciVfio.h 114062 2026-05-04 08:56:49Z alexander.eichner@oracle.com $ */
/** @file
 * PCI passthrough device emulation using VFIO/IOMMUFD - Header for building on too old Linux systems.
 */

/*
 * Copyright (C) 2026 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_SRC_Bus_DevPciVfio_h
#define VBOX_INCLUDED_SRC_Bus_DevPciVfio_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/assertcompile.h>

#include <linux/ioctl.h>
#include <linux/types.h>

RT_C_DECLS_BEGIN

#define IOMMUFD_TYPE (';')

struct iommu_destroy
{
    uint32_t size;
    uint32_t id;
};
AssertCompileSize(struct iommu_destroy, 2 * sizeof(uint32_t));
#define IOMMU_DESTROY                   _IO(IOMMUFD_TYPE, 0x80)


struct iommu_ioas_alloc
{
    uint32_t size;
    uint32_t flags;
    uint32_t out_ioas_id;
};
AssertCompileSize(struct iommu_ioas_alloc, 3 * sizeof(uint32_t));
#define IOMMU_IOAS_ALLOC                _IO(IOMMUFD_TYPE, 0x81)


struct iommu_ioas_map
{
    uint32_t size;
    uint32_t flags;
    uint32_t ioas_id;
    uint32_t __reserved;
    uint64_t user_va;
    uint64_t length;
    uint64_t iova;
};
AssertCompileSize(struct iommu_ioas_map, 4 * sizeof(uint32_t) + 3 * sizeof(uint64_t));
#define IOMMU_IOAS_MAP                  _IO(IOMMUFD_TYPE, 0x85)

#define IOMMU_IOAS_MAP_FIXED_IOVA       RT_BIT_32(0)
#define IOMMU_IOAS_MAP_WRITEABLE        RT_BIT_32(1)
#define IOMMU_IOAS_MAP_READABLE         RT_BIT_32(2)


#define VFIO_API_VERSION                0
#define VFIO_TYPE1_IOMMU                1


#define VFIO_TYPE (';')
#define VFIO_BASE 100

#define VFIO_GET_API_VERSION            _IO(VFIO_TYPE, VFIO_BASE + 0)
#define VFIO_CHECK_EXTENSION            _IO(VFIO_TYPE, VFIO_BASE + 1)
#define VFIO_SET_IOMMU                  _IO(VFIO_TYPE, VFIO_BASE + 2)


struct vfio_group_status
{
    uint32_t argsz;
    uint32_t flags;
};
AssertCompileSize(struct vfio_group_status, 2 * sizeof(uint32_t));
#define VFIO_GROUP_GET_STATUS           _IO(VFIO_TYPE, VFIO_BASE + 3)

#define VFIO_GROUP_FLAGS_VIABLE         RT_BIT_32(0)
#define VFIO_GROUP_FLAGS_CONTAINER_SET  RT_BIT_32(1)

#define VFIO_GROUP_SET_CONTAINER        _IO(VFIO_TYPE, VFIO_BASE + 4)
#define VFIO_GROUP_GET_DEVICE_FD        _IO(VFIO_TYPE, VFIO_BASE + 6)

struct vfio_device_info
{
    uint32_t argsz;
    uint32_t flags;
    uint32_t num_regions;
    uint32_t num_irqs;
    uint32_t cap_offset;
    uint32_t pad;
};
AssertCompileSize(struct vfio_device_info, 6 * sizeof(uint32_t));
#define VFIO_DEVICE_GET_INFO            _IO(VFIO_TYPE, VFIO_BASE + 7)

#define VFIO_DEVICE_FLAGS_RESET     RT_BIT_32(0)
#define VFIO_DEVICE_FLAGS_PCI       RT_BIT_32(1)


struct vfio_region_info
{
    uint32_t argsz;
    uint32_t flags;
    uint32_t index;
    uint32_t cap_offset;
    uint64_t size;
    uint64_t offset;
};
AssertCompileSize(struct vfio_region_info, 4 * sizeof(uint32_t) + 2 * sizeof(uint64_t));
#define VFIO_DEVICE_GET_REGION_INFO     _IO(VFIO_TYPE, VFIO_BASE + 8)

#define VFIO_REGION_INFO_FLAG_READ  RT_BIT_32(0)
#define VFIO_REGION_INFO_FLAG_WRITE RT_BIT_32(1)
#define VFIO_REGION_INFO_FLAG_MMAP  RT_BIT_32(2)
#define VFIO_REGION_INFO_FLAG_CAPS  RT_BIT_32(3)


struct vfio_info_cap_header
{
    uint16_t id;
    uint16_t version;
    uint32_t next;
};
AssertCompileSize(struct vfio_info_cap_header, 2 * sizeof(uint16_t) + sizeof(uint32_t));

#define VFIO_REGION_INFO_CAP_MSIX_MAPPABLE  3

struct vfio_irq_info
{
    uint32_t argsz;
    uint32_t flags;
    uint32_t index;
    uint32_t count;
};
AssertCompileSize(struct vfio_irq_info, 4 * sizeof(uint32_t));
#define VFIO_DEVICE_GET_IRQ_INFO        _IO(VFIO_TYPE, VFIO_BASE + 9)

#define VFIO_IRQ_INFO_EVENTFD           RT_BIT_32(0)
#define VFIO_IRQ_INFO_MASKABLE          RT_BIT_32(1)
#define VFIO_IRQ_INFO_AUTOMASKED        RT_BIT_32(2)
#define VFIO_IRQ_INFO_NORESIZE          RT_BIT_32(3)


struct vfio_irq_set
{
    uint32_t argsz;
    uint32_t flags;
    uint32_t index;
    uint32_t start;
    uint32_t count;
#if 0 /* Fails with clang, not actually accessed. */
    RT_FLEXIBLE_ARRAY_EXTENSION
    uint8_t  data[RT_FLEXIBLE_ARRAY];
#endif
};
AssertCompileSize(struct vfio_irq_set, 5 * sizeof(uint32_t));
#define VFIO_DEVICE_SET_IRQS            _IO(VFIO_TYPE, VFIO_BASE + 10)

#define VFIO_IRQ_SET_DATA_NONE      RT_BIT_32(0)
#define VFIO_IRQ_SET_DATA_BOOL      RT_BIT_32(1)
#define VFIO_IRQ_SET_DATA_EVENTFD   RT_BIT_32(2)
#define VFIO_IRQ_SET_ACTION_MASK    RT_BIT_32(3)
#define VFIO_IRQ_SET_ACTION_UNMASK  RT_BIT_32(4)
#define VFIO_IRQ_SET_ACTION_TRIGGER RT_BIT_32(5)


#define VFIO_DEVICE_RESET               _IO(VFIO_TYPE, VFIO_BASE + 11)

struct vfio_iommu_type1_dma_map
{
    uint32_t argsz;
    uint32_t flags;
    uint64_t vaddr;
    uint64_t iova;
    uint64_t size;
};
AssertCompileSize(struct vfio_iommu_type1_dma_map, 2 * sizeof(uint32_t) + 3 * sizeof(uint64_t));
#define VFIO_IOMMU_MAP_DMA              _IO(VFIO_TYPE, VFIO_BASE + 13)

#define VFIO_DMA_MAP_FLAG_READ          RT_BIT_32(0)
#define VFIO_DMA_MAP_FLAG_WRITE         RT_BIT_32(1)
#define VFIO_DMA_MAP_FLAG_VADDR         RT_BIT_32(2)


#define VFIO_PCI_BAR0_REGION_INDEX      0
#define VFIO_PCI_BAR1_REGION_INDEX      1
#define VFIO_PCI_BAR2_REGION_INDEX      2
#define VFIO_PCI_BAR3_REGION_INDEX      3
#define VFIO_PCI_BAR4_REGION_INDEX      4
#define VFIO_PCI_BAR5_REGION_INDEX      5
#define VFIO_PCI_ROM_REGION_INDEX       6
#define VFIO_PCI_CONFIG_REGION_INDEX    7
#define VFIO_PCI_VGA_REGION_INDEX       8


#define VFIO_PCI_INTX_IRQ_INDEX         0
#define VFIO_PCI_MSI_IRQ_INDEX          1
#define VFIO_PCI_MSIX_IRQ_INDEX         2
#define VFIO_PCI_ERR_IRQ_INDEX          3
#define VFIO_PCI_REQ_IRQ_INDEX          4


struct vfio_device_bind_iommufd
{
    uint32_t argsz;
    uint32_t flags;
    uint32_t iommufd;
    uint32_t out_devid;
    uint64_t token_uuid_ptr;
};
AssertCompileSize(struct vfio_device_bind_iommufd, 4 * sizeof(uint32_t) + sizeof(uint64_t));
#define VFIO_DEVICE_BIND_IOMMUFD        _IO(VFIO_TYPE, VFIO_BASE + 18)


struct vfio_device_attach_iommufd_pt
{
    uint32_t argsz;
    uint32_t flags;
    uint32_t pt_id;
    uint32_t pasid;
};
AssertCompileSize(struct vfio_device_attach_iommufd_pt, 4 * sizeof(uint32_t));
#define VFIO_DEVICE_ATTACH_IOMMUFD_PT   _IO(VFIO_TYPE, VFIO_BASE + 19)

#define VFIO_DEVICE_ATTACH_PASID        RT_BIT_32(0)


struct vfio_device_detach_iommufd_pt
{
    uint32_t argsz;
    uint32_t flags;
    uint32_t pasid;
};
AssertCompileSize(struct vfio_device_detach_iommufd_pt, 3 * sizeof(uint32_t));
#define VFIO_DEVICE_DETACH_IOMMUFD_PT   _IO(VFIO_TYPE, VFIO_BASE + 20)

#define VFIO_DEVICE_DETACH_PASID        RT_BIT_32(0)

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_Bus_DevPciVfio_h */

