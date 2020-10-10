// Copyright(c) 2020, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "opaevfio.h"

#define __SHORT_FILE__                                    \
({                                                        \
	const char *file = __FILE__;                      \
	const char *p = file;                             \
	while (*p)                                        \
		++p;                                      \
	while ((p > file) && ('/' != *p) && ('\\' != *p)) \
		--p;                                      \
	if (p > file)                                     \
		++p;                                      \
	p;                                                \
})

#define ERR(format, ...)                                \
fprintf(stderr, "%s:%u:%s() **ERROR** [%s] : " format , \
	__SHORT_FILE__, __LINE__, __func__, strerror(errno), ##__VA_ARGS__)


STATIC int opae_vfio_device_get_sparse_info(struct opae_vfio_device *d,
					    struct vfio_region_info *rinfo)
{
	(void) d;
	(void) rinfo;

	
	return 0;
}

STATIC void opae_vfio_device_destroy(struct opae_vfio_device *d)
{
	if (d->device_fd >= 0) {
		close(d->device_fd);
		d->device_fd = -1;
	}
}

STATIC int opae_vfio_device_init(struct opae_vfio_device *d,
				 int group_fd,
				 const char *pciaddr)
{
	struct vfio_region_info region_info;
	struct vfio_device_info device_info;
	uint32_t i;

	d->device_fd = ioctl(group_fd, VFIO_GROUP_GET_DEVICE_FD, pciaddr);
	if (d->device_fd < 0) {
		ERR("ioctl(%d, VFIO_GROUP_GET_DEVICE_FD, \"%s\")\n",
		    group_fd, pciaddr);
		return 1;
	}

	memset(&region_info, 0, sizeof(region_info));
	region_info.argsz = sizeof(region_info);
	region_info.index = VFIO_PCI_CONFIG_REGION_INDEX;

	if (ioctl(d->device_fd, VFIO_DEVICE_GET_REGION_INFO, &region_info)) {
		ERR("ioctl(%d, VFIO_DEVICE_GET_REGION_INFO, &region_info)\n",
		    d->device_fd);
		return 2;
	}

	d->device_config_offset = region_info.offset;

	memset(&device_info, 0, sizeof(device_info));
	device_info.argsz = sizeof(device_info);

	if (ioctl(d->device_fd, VFIO_DEVICE_GET_INFO, &device_info)) {
		ERR("ioctl(%d, VFIO_DEVICE_GET_INFO, &device_info)\n",
		    d->device_fd);
		return 3;
	}

	d->device_num_regions = device_info.num_regions;

	for (i = 0 ; i < d->device_num_regions ; ++i) {
		memset(&region_info, 0, sizeof(region_info));
		region_info.argsz = sizeof(region_info);
		region_info.index = i;

		if (ioctl(d->device_fd,
			  VFIO_DEVICE_GET_REGION_INFO,
			  &region_info))
			continue;

		if (region_info.flags & VFIO_REGION_INFO_FLAG_MMAP) {

			if (!opae_vfio_device_get_sparse_info(d,
							      &region_info)) {



			}
		}

	}

	return 0;
}

STATIC void opae_vfio_group_destroy(struct opae_vfio_group *g)
{
	if (g->group_fd >= 0) {
		if (ioctl(g->group_fd, VFIO_GROUP_UNSET_CONTAINER)) {
			ERR("ioctl(%d, VFIO_GROUP_UNSET_CONTAINER)\n",
			    g->group_fd);
		}
		close(g->group_fd);
		g->group_fd = -1;
	}

	if (g->group_device) {
		free(g->group_device);
		g->group_device = NULL;
	}
}

STATIC int opae_vfio_group_init(struct opae_vfio_group *g,
				const char *device)
{
	struct vfio_group_status group_status;
	int res = 0;

	g->group_device = strdup(device);

	g->group_fd = open(g->group_device, O_RDWR);
	if (g->group_fd < 0) {
		ERR("open(\"%s\", O_RDWR)\n", g->group_device);
		res = 1;
		goto out_destroy;
	}

	memset(&group_status, 0, sizeof(group_status));
	group_status.argsz = sizeof(group_status);

	if (ioctl(g->group_fd, VFIO_GROUP_GET_STATUS, &group_status)) {
		ERR("ioctl(%d, VFIO_GROUP_GET_STATUS, &group_status)\n",
		    g->group_fd);
		res = 2;
		goto out_destroy;
	}

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		ERR("VFIO group not viable\n");
		res = 3;
		goto out_destroy;
	}

	return 0;

out_destroy:
	opae_vfio_group_destroy(g);
	return res;
}

STATIC void opae_vfio_container_destroy(struct opae_vfio_container *c)
{
	struct opae_vfio_iova_range *range;

	opae_vfio_device_destroy(&c->device);
	opae_vfio_group_destroy(&c->group);

	range = c->cont_ranges;
	while (range) {
		struct opae_vfio_iova_range *trash = range;
		range = range->next;
		free(trash);
	}

	if (c->cont_fd >= 0) {
		close(c->cont_fd);
		c->cont_fd = -1;
	}

	if (c->cont_device) {
		free(c->cont_device);
		c->cont_device = NULL;
	}

	if (c->cont_pciaddr) {
		free(c->cont_pciaddr);
		c->cont_pciaddr = NULL;
	}
}

STATIC int opae_vfio_container_init(struct opae_vfio_container *c,
				    const char *device,
				    const char *pciaddr)
{
	int res = 0;
	pthread_mutexattr_t mattr;
	int cont_fd;

	memset(c, 0, sizeof(*c));
	c->cont_fd = -1;
	c->group.group_fd = -1;
	c->device.device_fd = -1;

	if (pthread_mutexattr_init(&mattr)) {
		ERR("pthread_mutexattr_init()\n");
		return 1;
	}

	if (pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE)) {
		ERR("pthread_mutexattr_settype()\n");
		res = 2;
		goto out_destroy_attr;
	}

	if (pthread_mutex_init(&c->lock, &mattr)) {
		ERR("pthread_mutex_init()\n");
		res = 3;
		goto out_destroy_attr;
	}

	c->cont_device = strdup("/dev/vfio/vfio");
	c->cont_pciaddr = strdup(pciaddr);
	c->cont_fd = open(c->cont_device, O_RDWR);
	if (c->cont_fd < 0) {
		ERR("open(\"%s\")\n", c->cont_device);
		free(c->cont_device);
		c->cont_device = NULL;
		free(c->cont_pciaddr);
		c->cont_pciaddr = NULL;
		res = 4;
		goto out_destroy_attr;
	}

	if (ioctl(c->cont_fd, VFIO_GET_API_VERSION) != VFIO_API_VERSION) {
		ERR("ioctl(%d, VFIO_GET_API_VERSION)\n", c->cont_fd);
		res = 5;
		goto out_destroy_container;
	}

	if (!ioctl(c->cont_fd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)) {
		ERR("ioctl(%d, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU)\n",
		    c->cont_fd);
		res = 6;
		goto out_destroy_container;
	}

	res = opae_vfio_group_init(&c->group, device);
	if (res)
		goto out_destroy_container;

	cont_fd = c->cont_fd;
	if (ioctl(c->group.group_fd, VFIO_GROUP_SET_CONTAINER, &cont_fd)) {
		ERR("ioctl(%d, VFIO_GROUP_SET_CONTAINER, &cont_fd)\n",
		    c->group.group_fd);
		res = 7;
		goto out_destroy_container;
	}

	if (ioctl(c->cont_fd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU) < 0) {
		ERR("ioctl(%d, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU)\n",
		    c->cont_fd);
		res = 8;
		goto out_destroy_container;
	}

	res = opae_vfio_device_init(&c->device, c->group.group_fd, pciaddr);
	if (res)
		goto out_destroy_container;





	if (pthread_mutexattr_destroy(&mattr)) {
		ERR("pthread_mutexattr_destroy()\n");
		return 7;
	}

	return 0;

out_destroy_container:
	opae_vfio_container_destroy(c);

out_destroy_attr:
	if (pthread_mutexattr_destroy(&mattr)) {
		ERR("pthread_mutexattr_destroy()\n");
		res = 9;
	}

	return res;
}

#if 0
STATIC int opae_vfio_open_discover(struct opae_vfio_container *c)
{
	int res = 0;
	struct vfio_iommu_type1_info iommu_info;
	uint8_t *info_buf;
	struct vfio_iommu_type1_info *info_ptr;
	struct vfio_info_cap_header *hdr;
	struct opae_vfio_iova_range **range_ptr = &c->cont_ranges;

	memset(&iommu_info, 0, sizeof(iommu_info));
	iommu_info.argsz = sizeof(iommu_info);

	if (ioctl(c->cont_fd, VFIO_IOMMU_GET_INFO, &iommu_info)) {
		ERR("ioctl( , VFIO_IOMMU_GET_INFO, &iommu_info\n");
		return 1;
	}

	// Now that we have the size, create a temp buffer large enough..
	info_buf = calloc(1, iommu_info.argsz);
	if (!info_buf) {
		ERR("calloc(1, iommu_info.argsz)\n");
		return 2;
	}

	info_ptr = (struct vfio_iommu_type1_info *)info_buf;
	info_ptr->argsz = iommu_info.argsz;

	if (ioctl(c->cont_fd, VFIO_IOMMU_GET_INFO, info_ptr)) {
		ERR("ioctl( , VFIO_IOMMU_GET_INFO, info_ptr)\n");
		res = 3;
		goto out_free;
	}

	hdr = (struct vfio_info_cap_header *) (info_buf + info_ptr->cap_offset);

	while (1) {
		if (hdr->id == VFIO_IOMMU_TYPE1_INFO_CAP_IOVA_RANGE) {
			struct vfio_iommu_type1_info_cap_iova_range *io_range =
				(struct vfio_iommu_type1_info_cap_iova_range *) hdr;
			uint32_t i;

			for (i = 0 ; i < io_range->nr_iovas ; ++i) {
				*range_ptr = calloc(1, sizeof(struct opae_vfio_iova_range));
				if (!*range_ptr) {
					ERR("calloc()\n");
					res = 4;
					goto out_destroy_ranges;
				}

				(*range_ptr)->start = io_range->iova_ranges[i].start;
				(*range_ptr)->end = io_range->iova_ranges[i].end;
				(*range_ptr)->next_ptr = (*range_ptr)->start;

				range_ptr = &(*range_ptr)->next;
			}
		}

		if (!hdr->next)
			break;

		hdr = (struct vfio_info_cap_header *) (info_buf + hdr->next);
	}

	goto out_free;

out_destroy_ranges:
	range_ptr = &c->cont_ranges;
	while (*range_ptr) {
		struct opae_vfio_iova_range *trash = *range_ptr;
		range_ptr = &(*range_ptr)->next;
		free(trash);
	}

out_free:
	free(info_buf);
	return res;
}
#endif

int opae_vfio_open(struct opae_vfio_container *c,
		   const char *device,
		   const char *pciaddr)
{
	int res = 0;

	if (!c || !device || !pciaddr) {
		ERR("NULL param\n");
		return 1;
	}

	res = opae_vfio_container_init(c, device, pciaddr);
	if (res)
		return res;




	return res;
}

void opae_vfio_close(struct opae_vfio_container *c)
{
	opae_vfio_container_destroy(c);
}
