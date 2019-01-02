// Copyright(c) 2017-2018, Intel Corporation
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
/*
 * opae_ioctl.c
 */

#include <stddef.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <opae/fpga.h>
#include <safe_string/safe_string.h>
#include "common_int.h"
#include "opae_ioctl.h"
#include "intel-fpga.h"

fpga_result opae_ioctl(int fd, int request, ...)
{
	fpga_result res = FPGA_OK;
	va_list argp;
	va_start(argp, request);
	void *msg = va_arg(argp, void *);
	errno = 0;
	if (ioctl(fd, request, msg) != 0) {
		OPAE_MSG("error executing ioctl: %s", strerror(errno));
		switch (errno) {
		case EINVAL:
			res = FPGA_INVALID_PARAM;
			break;
		case ENOTSUP:
			res = FPGA_NOT_SUPPORTED;
			break;
		default:
			// other errors could be
			// EBADF - fd is bad file descriptor
			// EFAULT - argp references an inaccessible
			// memory area
			// ENOTTY - fd is not associated with a char.
			// special device
			res = FPGA_EXCEPTION;
			break;
		}
	}
	va_end(argp);

	return res;
}

int opae_get_fme_info(int fd, opae_fme_info *info)
{
	ASSERT_NOT_NULL(info);
	struct fpga_fme_info fme_info = {.argsz = sizeof(fme_info), .flags = 0};
	int res = opae_ioctl(fd, FPGA_FME_GET_INFO, &fme_info);
	if (!res) {
		info->flags = fme_info.flags;
		info->capability = fme_info.capability;
	}
	return res;
}

int opae_get_port_info(int fd, opae_port_info *info)
{
	ASSERT_NOT_NULL(info);
	struct fpga_port_info pinfo = {.argsz = sizeof(pinfo), .flags = 0};
	int res = opae_ioctl(fd, FPGA_PORT_GET_INFO, &pinfo);
	if (!res) {
		info->flags = pinfo.flags;
		info->capability = pinfo.capability;
		info->num_regions = pinfo.num_regions;
		info->num_umsgs = pinfo.num_umsgs;
		info->num_uafu_irqs = pinfo.num_uafu_irqs;
	}
	return res;


}

int opae_get_port_region_info(int fd, uint32_t index,
			      opae_port_region_info *info)
{
	ASSERT_NOT_NULL(info);
	struct fpga_port_region_info rinfo = {.argsz = sizeof(rinfo),
					      .padding = 0,
					      .index = index};
	int res = opae_ioctl(fd, FPGA_PORT_GET_REGION_INFO, &rinfo);
	if (!res) {
		info->flags = rinfo.flags;
		info->size = rinfo.size;
		info->offset = rinfo.offset;
	}
	return res;

}

int opae_port_map(int fd, void *addr, uint64_t len, uint64_t *io_addr)
{
	int res = 0;
	int req = 0;
	void *msg = NULL;
	/* Set ioctl fpga_port_dma_map struct parameters */
	struct fpga_port_dma_map dma_map = {.argsz = sizeof(dma_map),
					    .flags = 0,
					    .user_addr = (__u64)addr,
					    .length = (__u64)len,
					    .iova = 0};
	ASSERT_NOT_NULL(io_addr);
	/* Dispatch ioctl command */
	req = FPGA_PORT_DMA_MAP;
	msg = &dma_map;

	res = opae_ioctl(fd, req, msg);
	if (!res) {
		*io_addr = dma_map.iova;
	}
	return res;
}

int opae_port_unmap(int fd, uint64_t io_addr)
{
	/* Set ioctl fpga_port_dma_unmap struct parameters */
	struct fpga_port_dma_unmap dma_unmap = {
		.argsz = sizeof(dma_unmap), .flags = 0, .iova = io_addr};

	/* Dispatch ioctl command */
	return opae_ioctl(fd, FPGA_PORT_DMA_UNMAP, &dma_unmap);
}

int opae_port_umsg_cfg(int fd, uint32_t flags, uint32_t hint_bitmap)
{
	if (flags) {
		OPAE_MSG("flags currently not supported in FPGA_PORT_UMSG_SET_MODE");
	}

	struct fpga_port_umsg_cfg umsg_cfg = {.argsz = sizeof(umsg_cfg),
					      .flags = 0,
					      .hint_bitmap = hint_bitmap};
	return opae_ioctl(fd, FPGA_PORT_UMSG_SET_MODE, &umsg_cfg);
}

int opae_port_umsg_set_base_addr(int fd, uint32_t flags, uint64_t io_addr)
{
	if (flags) {
		OPAE_MSG("flags currently not supported in FPGA_PORT_UMSG_SET_BASE_ADDR");
	}

	struct fpga_port_umsg_base_addr baseaddr = {
		.argsz = sizeof(baseaddr), .flags = 0, .iova = io_addr};
	return opae_ioctl(fd, FPGA_PORT_UMSG_SET_BASE_ADDR, &baseaddr);
}

int opae_port_umsg_enable(int fd)
{
	return opae_ioctl(fd, FPGA_PORT_UMSG_ENABLE, NULL);
}

int opae_port_umsg_disable(int fd)
{
	return opae_ioctl(fd, FPGA_PORT_UMSG_DISABLE, NULL);
}

int opae_fme_set_err_irq(int fd, uint32_t flags, int32_t evtfd)
{
	if (flags) {
		OPAE_MSG(
			"flags currently not supported in FPGA_FME_ERR_SET_IRQ");
	}

	struct fpga_fme_err_irq_set irq = {
		.argsz = sizeof(irq), .flags = flags, .evtfd = evtfd};
	return opae_ioctl(fd, FPGA_FME_ERR_SET_IRQ, &irq);
}

int opae_port_set_err_irq(int fd, uint32_t flags, int32_t evtfd)
{
	if (flags) {
		OPAE_MSG(
			"flags currently not supported in FPGA_FME_ERR_SET_IRQ");
	}

	struct fpga_port_err_irq_set irq = {
		.argsz = sizeof(irq), .flags = flags, .evtfd = evtfd};
	return opae_ioctl(fd, FPGA_PORT_ERR_SET_IRQ, &irq);
}

int opae_port_set_user_irq(int fd, uint32_t flags, uint32_t start, uint32_t count, int32_t *eventfd)
{
	uint32_t sz = sizeof(struct fpga_port_uafu_irq_set) + count*sizeof(int32_t);
	struct fpga_port_uafu_irq_set *irq = NULL;
	errno_t err = 0;
	int res = 0;

	ASSERT_NOT_NULL(eventfd);
	if (!count) {
		OPAE_ERR("set_user irq with emtpy count");
		return FPGA_INVALID_PARAM;
	}

	if (flags) {
		OPAE_MSG(
			"flags currently not supported in FPGA_FME_ERR_SET_IRQ");
	}
	irq = malloc(sz);

	if (!irq) {
		OPAE_ERR("Could not allocate memory for irq request");
		return FPGA_NO_MEMORY;
	}

	irq->argsz = sz;
	irq->flags = 0;
	irq->start = start;
	irq->count = count;
	err = memcpy32_s((uint32_t *)irq->evtfd, count, (uint32_t *)eventfd, count);
	if (err) {
		res = FPGA_INVALID_PARAM;
		goto out_free;
	}

	res = opae_ioctl(fd, FPGA_PORT_UAFU_SET_IRQ, irq);

out_free:
	free(irq);
	return res;
}

int opae_fme_port_assign(int fd, uint32_t flags, uint32_t port_id)
{
	struct fpga_fme_port_assign assign = {
		.argsz = sizeof(assign), .flags = 0, .port_id = port_id};
	if (flags) {
		OPAE_MSG(
			"flags currently not supported in FPGA_FME_PORT_ASSIGN");
	}
	return opae_ioctl(fd, FPGA_FME_PORT_ASSIGN, &assign);
}

int opae_fme_port_release(int fd, uint32_t flags, uint32_t port_id)
{
	struct fpga_fme_port_assign assign = {
		.argsz = sizeof(assign), .flags = 0, .port_id = port_id};
	if (flags) {
		OPAE_MSG(
			"flags currently not supported in FPGA_FME_PORT_RELEASE");
	}
	return opae_ioctl(fd, FPGA_FME_PORT_RELEASE, &assign);
}

int opae_fme_port_pr(int fd, uint32_t flags, uint32_t port_id, uint32_t sz,
		     uint64_t addr, uint64_t *status)
{
	struct fpga_fme_port_pr port_pr = {.argsz = sizeof(port_pr),
					   .flags = 0,
					   .port_id = port_id,
					   .buffer_size = sz,
					   .buffer_address = addr};
	int res = FPGA_OK;
	if (flags) {
		OPAE_MSG("flags currently not supported in FPGA_FME_PORT_PR");
	}
	ASSERT_NOT_NULL(status);
	res = opae_ioctl(fd, FPGA_FME_PORT_PR, &port_pr);
	*status = port_pr.status;
	return res;
}

int opae_fme_port_reset(int fd)
{
	return opae_ioctl(fd, FPGA_PORT_RESET, NULL);
}
