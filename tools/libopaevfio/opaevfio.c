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
#include <sys/mman.h>

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

STATIC struct opae_vfio_sparse_info *
opae_vfio_create_sparse_info(uint32_t index, uint32_t offset, uint32_t size)
{
	struct opae_vfio_sparse_info *p = malloc(sizeof(*p));
	if (p) {
		p->next = NULL;
		p->index = index;
		p->offset = offset;
		p->size = size;
		p->ptr = MAP_FAILED;
	}
	return p;
}

STATIC void opae_vfio_destroy_sparse_info(struct opae_vfio_sparse_info *s)
{
	while (s) {
		struct opae_vfio_sparse_info *trash = s;
		s = s->next;
		if (trash->ptr != MAP_FAILED) {
			if (munmap(trash->ptr, trash->size) < 0)
				ERR("munmap failed\n");
		}
		free(trash);
	}
}

STATIC struct opae_vfio_device_region *
opae_vfio_map_sparse_device_region(uint32_t index,
				   int fd,
				   uint64_t offset,
				   size_t sz,
				   struct opae_vfio_sparse_info *slist)
{
	struct opae_vfio_device_region *r = malloc(sizeof(*r));
	if (r) {
		r->next = NULL;
		r->region_index = index;
		r->region_ptr = mmap(0, sz, PROT_READ|PROT_WRITE,
				     MAP_ANONYMOUS|MAP_PRIVATE,
				     -1, 0);
		if (r->region_ptr == MAP_FAILED) {
			free(r);
			return NULL;
		}
		r->region_size = sz;
		r->region_sparse = slist;

		while (slist) {
			slist->ptr = mmap(r->region_ptr + slist->offset,
				    slist->size, PROT_READ|PROT_WRITE,
				    MAP_FIXED|MAP_SHARED, fd,
				    offset + slist->offset);
			if (slist->ptr == MAP_FAILED)
				ERR("mmap failed\n");
			slist = slist->next;
		}
	}
	return r;
}

STATIC struct opae_vfio_device_region *
opae_vfio_map_device_region(uint32_t index,
			    int fd,
			    uint64_t offset,
			    size_t sz)
{
	struct opae_vfio_device_region *r = malloc(sizeof(*r));
	if (r) {
		r->next = NULL;
		r->region_index = index;
		r->region_ptr = mmap(0, sz, PROT_READ|PROT_WRITE,
				     MAP_SHARED, fd, (off_t)offset);
		r->region_size = sz;
		r->region_sparse = NULL;
		if (r->region_ptr == MAP_FAILED) {
			free(r);
			return NULL;
		}
	}
	return r;
}

STATIC void
opae_vfio_destroy_device_region(struct opae_vfio_device_region *r)
{
	while (r) {
		struct opae_vfio_device_region *trash = r;
		r = r->next;
		if (munmap(trash->region_ptr, trash->region_size) < 0)
			ERR("munmap failed\n");
		opae_vfio_destroy_sparse_info(trash->region_sparse);
		free(trash);
	}
}

STATIC struct opae_vfio_sparse_info *
opae_vfio_device_get_sparse_info(struct opae_vfio_device *d,
				 struct vfio_region_info *rinfo)
{
	struct opae_vfio_sparse_info *sparse_list = NULL;
	struct opae_vfio_sparse_info **slist = &sparse_list;
	uint8_t *buffer;
	struct vfio_region_info *rinfo_ptr;
	struct vfio_info_cap_header *hdr;
	struct vfio_region_info_cap_sparse_mmap *sparse_mmap;
	uint32_t i;

	if (!(rinfo->flags & VFIO_REGION_INFO_FLAG_CAPS))
		return NULL;

	buffer = malloc(rinfo->argsz);
	if (!buffer) {
		ERR("malloc failed\n");
		return NULL;
	}

	memcpy(buffer, rinfo, sizeof(*rinfo));
	rinfo_ptr = (struct vfio_region_info *) buffer;

	if (ioctl(d->device_fd, VFIO_DEVICE_GET_REGION_INFO, buffer)) {
		ERR("ioctl(%d, VFIO_DEVICE_GET_REGION_INFO, buffer)\n",
		    d->device_fd);
		goto out_free_buffer;
	}

	hdr = (struct vfio_info_cap_header *)(buffer + rinfo_ptr->cap_offset);

	if (hdr->id != VFIO_REGION_INFO_CAP_SPARSE_MMAP)
		goto out_free_buffer;

	sparse_mmap = (struct vfio_region_info_cap_sparse_mmap *) hdr;

	for (i = 0 ; i < sparse_mmap->nr_areas ; ++i) {
		struct opae_vfio_sparse_info *node =
		opae_vfio_create_sparse_info(i,
					     sparse_mmap->areas[i].offset,
					     sparse_mmap->areas[i].size);
		if (!node) {
			ERR("malloc failed");
			goto out_free_buffer;
		}
		*slist = node;
		slist = &node->next;
	}

out_free_buffer:
	free(buffer);

	return sparse_list;
}

STATIC void opae_vfio_device_destroy(struct opae_vfio_device *d)
{
	opae_vfio_destroy_device_region(d->regions);
	d->regions = NULL;

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
	struct opae_vfio_device_region **rlist = &d->regions;

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
		struct opae_vfio_sparse_info *sparse_list = NULL;

		memset(&region_info, 0, sizeof(region_info));
		region_info.argsz = sizeof(region_info);
		region_info.index = i;

		if (ioctl(d->device_fd,
			  VFIO_DEVICE_GET_REGION_INFO,
			  &region_info))
			continue;

		if (region_info.flags & VFIO_REGION_INFO_FLAG_MMAP) {
			struct opae_vfio_device_region *region = NULL;

			sparse_list = opae_vfio_device_get_sparse_info(
					d, &region_info);

			if (sparse_list) {
				// map_sparse
				region = opae_vfio_map_sparse_device_region(
						i, d->device_fd,
						region_info.offset,
						region_info.size,
						sparse_list);
			} else {
				// map
				region = opae_vfio_map_device_region(i,
					d->device_fd,
					region_info.offset,
					region_info.size);
			}

			if (region) {
				*rlist = region;
				rlist = &region->next;
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

STATIC void opae_vfio_destroy_iova_range(struct opae_vfio_iova_range *r)
{
	while (r) {
		struct opae_vfio_iova_range *trash = r;
		r = r->next;
		free(trash);
	}
}

STATIC void
opae_vfio_destroy_buffer(int , struct opae_vfio_buffer * );

STATIC void opae_vfio_container_destroy(struct opae_vfio_container *c)
{
	opae_vfio_device_destroy(&c->device);
	opae_vfio_group_destroy(&c->group);
	opae_vfio_destroy_iova_range(c->cont_ranges);
	c->cont_ranges = NULL;
	opae_vfio_destroy_buffer(c->cont_fd, c->cont_buffers);
	c->cont_buffers = NULL;

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

	if (pthread_mutex_unlock(&c->lock))
		ERR("pthread_mutex_unlock() failed\n");

	if (pthread_mutex_destroy(&c->lock))
		ERR("pthread_mutex_destroy() failed\n");
}

STATIC struct opae_vfio_iova_range *
opae_vfio_create_iova_range(uint64_t start, uint64_t end)
{
	struct opae_vfio_iova_range *r;
	r = malloc(sizeof(*r));
	if (r) {
		r->start = start;
		r->end = end;
		r->next_ptr = r->start;
		r->next = NULL;
	}
	return r;
}

STATIC struct opae_vfio_iova_range *
opae_vfio_iova_discover(struct opae_vfio_container *c)
{
	struct opae_vfio_iova_range *iova_list = NULL;
	struct opae_vfio_iova_range **ilist = &iova_list;
	struct vfio_iommu_type1_info iommu_info;
	uint8_t *info_buf;
	struct vfio_iommu_type1_info *info_ptr;
	struct vfio_info_cap_header *hdr;

	memset(&iommu_info, 0, sizeof(iommu_info));
	iommu_info.argsz = sizeof(iommu_info);

	if (ioctl(c->cont_fd, VFIO_IOMMU_GET_INFO, &iommu_info)) {
		ERR("ioctl(%d, VFIO_IOMMU_GET_INFO, &iommu_info\n",
		    c->cont_fd);
		return NULL;
	}

	info_buf = calloc(1, iommu_info.argsz);
	if (!info_buf) {
		ERR("calloc(1, iommu_info.argsz)\n");
		return NULL;
	}

	info_ptr = (struct vfio_iommu_type1_info *) info_buf;
	info_ptr->argsz = iommu_info.argsz;

	if (ioctl(c->cont_fd, VFIO_IOMMU_GET_INFO, info_ptr)) {
		ERR("ioctl(%d, VFIO_IOMMU_GET_INFO, info_ptr)\n",
		    c->cont_fd);
		goto out_free;
	}

	hdr = (struct vfio_info_cap_header *) (info_buf + info_ptr->cap_offset);

	while (1) {
		if (hdr->id == VFIO_IOMMU_TYPE1_INFO_CAP_IOVA_RANGE) {
			struct vfio_iommu_type1_info_cap_iova_range *io_range =
				(struct vfio_iommu_type1_info_cap_iova_range *) hdr;
			uint32_t i;

			for (i = 0 ; i < io_range->nr_iovas ; ++i) {
				struct opae_vfio_iova_range *node;

				node = opae_vfio_create_iova_range(
						io_range->iova_ranges[i].start,
						io_range->iova_ranges[i].end);

				if (node) {
					*ilist = node;
					ilist = &node->next;
				}
			}
		}

		if (!hdr->next)
			break;

		hdr = (struct vfio_info_cap_header *) (info_buf + hdr->next);
	}

out_free:
	free(info_buf);
	return iova_list;
}

STATIC int opae_vfio_iova_reserve(struct opae_vfio_container *c,
				  uint64_t *size,
				  uint64_t *iova)
{
	uint64_t page_size;
	struct opae_vfio_iova_range *range;

	page_size = sysconf(_SC_PAGE_SIZE);
	*size = page_size + ((*size - 1) & ~(page_size - 1));

	for (range = c->cont_ranges ; range ; range = range->next) {
		if (range->next_ptr + *size <= range->end) {
			*iova = range->next_ptr;
			range->next_ptr += *size;
			return 0;
		}
	}

	return 2;
}

STATIC struct opae_vfio_buffer *
opae_vfio_create_buffer(uint8_t *vaddr,
			size_t size,
			uint64_t iova)
{
	struct opae_vfio_buffer *b;
	b = malloc(sizeof(*b));
	if (b) {
		b->buffer_ptr = vaddr;
		b->buffer_size = size;
		b->buffer_iova = iova;
		b->next = NULL;
	}
	return b;
}

STATIC void
opae_vfio_destroy_buffer(int fd, struct opae_vfio_buffer *b)
{
	struct vfio_iommu_type1_dma_unmap dma_unmap;

	while (b) {
		struct opae_vfio_buffer *trash = b;
		b = b->next;

		memset(&dma_unmap, 0, sizeof(dma_unmap));
		dma_unmap.argsz = sizeof(dma_unmap);
		dma_unmap.iova = trash->buffer_iova;
		dma_unmap.size = trash->buffer_size;

		if (ioctl(fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap) < 0)
			ERR("ioctl(%d, VFIO_IOMMU_UNMAP_DMA, &dma_unmap)\n",
			    fd);

		if (munmap(trash->buffer_ptr, trash->buffer_size) < 0)
			ERR("munmap(0x%p, %lu) failed\n",
			    trash->buffer_ptr, trash->buffer_size);

		free(trash);
	}
}

int opae_vfio_buffer_allocate(struct opae_vfio_container *c,
			      size_t *size,
			      uint8_t **buf,
			      uint64_t *iova)
{
	int res = 0;
	uint64_t ioaddr = 0;
	uint8_t *vaddr;
	struct vfio_iommu_type1_dma_map dma_map;
	struct vfio_iommu_type1_dma_unmap dma_unmap;
	struct opae_vfio_buffer *node;

	if (!c || !size) {
		ERR("NULL param\n");
		return 1;
	}

	if (!*size) {
		ERR("size must be > 0\n");
		return 2;
	}

	if (pthread_mutex_lock(&c->lock)) {
		ERR("pthread_mutex_lock() failed\n");
		return 3;
	}

	if (opae_vfio_iova_reserve(c, size, &ioaddr)) {
		pthread_mutex_unlock(&c->lock);
		return 4;
	}

	vaddr = mmap(0, *size, PROT_READ|PROT_WRITE,
		     MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, 0, 0);
	if (vaddr == MAP_FAILED) {
		ERR("mmap() failed\n");
		pthread_mutex_unlock(&c->lock);
		return 5;
	}

	memset(&dma_map, 0, sizeof(dma_map));

	dma_map.argsz = sizeof(dma_map);
	dma_map.vaddr = (uint64_t) vaddr;
	dma_map.size = *size;
	dma_map.iova = ioaddr;
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ|VFIO_DMA_MAP_FLAG_WRITE;

	if (ioctl(c->cont_fd, VFIO_IOMMU_MAP_DMA, &dma_map) < 0) {
		ERR("ioctl(%d, VFIO_IOMMU_MAP_DMA, &dma_map)\n",
		    c->cont_fd);
		res = 5;
		goto out_munmap;
	}

	node = opae_vfio_create_buffer(vaddr, *size, ioaddr);
	if (!node) {
		ERR("malloc failed\n");
		res = 6;
		goto out_unmap_ioctl;
	}

	node->next = c->cont_buffers;
	c->cont_buffers = node;

	if (buf)
		*buf = vaddr;
	if (iova)
		*iova = ioaddr;

	if (pthread_mutex_unlock(&c->lock))
		ERR("pthread_mutex_unlock() failed\n");

	return 0;

out_unmap_ioctl:
	memset(&dma_unmap, 0, sizeof(dma_unmap));
	dma_unmap.argsz = sizeof(dma_unmap);
	dma_unmap.flags = VFIO_DMA_MAP_FLAG_READ|VFIO_DMA_MAP_FLAG_WRITE;
	dma_unmap.iova = ioaddr;
	dma_unmap.size = *size;
	ioctl(c->cont_fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
out_munmap:
	munmap(vaddr, *size);
	if (pthread_mutex_unlock(&c->lock))
		ERR("pthread_mutex_unlock() failed\n");
	return res;
}

int opae_vfio_buffer_free(struct opae_vfio_container *c,
			  uint8_t *buf)
{
	struct opae_vfio_buffer *b;
	struct opae_vfio_buffer *prev = NULL;
	int res = 0;

	if (!c) {
		ERR("NULL param\n");
		return 1;
	}

	if (pthread_mutex_lock(&c->lock)) {
		ERR("pthread_mutex_lock() failed\n");
		return 2;
	}

	for (b = c->cont_buffers ; b ; prev = b, b = b->next) {
		if (b->buffer_ptr == buf) {
			if (!prev) { // b == c->cont_buffers
				c->cont_buffers = b->next;
			} else {
				prev->next = b->next;
			}
			b->next = NULL;
			opae_vfio_destroy_buffer(c->cont_fd, b);
			goto out_unlock;
		}
	}

	res = 3;

out_unlock:
	if (pthread_mutex_unlock(&c->lock))
		ERR("pthread_mutex_unlock() failed\n");

	return res;
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
		res = 4;
		goto out_destroy_container;
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

	c->cont_ranges = opae_vfio_iova_discover(c);

	if (pthread_mutexattr_destroy(&mattr)) {
		ERR("pthread_mutexattr_destroy()\n");
		return 9;
	}

	return 0;

out_destroy_container:
	pthread_mutex_lock(&c->lock);
	opae_vfio_container_destroy(c);

out_destroy_attr:
	if (pthread_mutexattr_destroy(&mattr)) {
		ERR("pthread_mutexattr_destroy()\n");
		res = 10;
	}

	return res;
}

int opae_vfio_open(struct opae_vfio_container *c,
		   const char *device,
		   const char *pciaddr)
{
	if (!c || !device || !pciaddr) {
		ERR("NULL param\n");
		return 1;
	}

	return opae_vfio_container_init(c, device, pciaddr);
}

int opae_vfio_region_get(struct opae_vfio_container *c,
			 uint32_t index,
			 uint8_t **ptr,
			 size_t *size)
{
	struct opae_vfio_device_region *region;

	if (!c) {
		ERR("NULL param\n");
		return 1;
	}

	for (region = c->device.regions ; region ; region = region->next)
	{
		if (index == region->region_index) {
			if (ptr)
				*ptr = region->region_ptr;
			if (size)
				*size = region->region_size;
			return 0;
		}
	}

	return 2;
}

void opae_vfio_close(struct opae_vfio_container *c)
{
	if (!c) {
		ERR("NULL param\n");
		return;
	}

	if (pthread_mutex_lock(&c->lock)) {
		ERR("pthread_mutex_lock() failed\n");
		return;
	}

	opae_vfio_container_destroy(c);
}
