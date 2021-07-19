// Copyright(c) 2018, Intel Corporation
//
// Redistribution  and	use  in source	and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//	 this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//	 this list of conditions and the following disclaimer in the documentation
//	 and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//	 may be used to  endorse or promote  products derived  from this  software
//	 without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.	IN NO EVENT  SHALL THE COPYRIGHT OWNER	OR CONTRIBUTORS BE
// LIABLE  FOR	ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,	EXEMPLARY,	OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,	BUT  NOT LIMITED  TO,  PROCUREMENT	OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,	DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,	WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,	EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
/**
 * \fpga_dma_test_utils.h
 * \brief DMA test utils
 */

#ifndef __FPGA_DMA_TEST_UTILS_H__
#define __FPGA_DMA_TEST_UTILS_H__

#include <string.h>
#include <uuid/uuid.h>
#include <opae/fpga.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <semaphore.h>
#ifndef USE_ASE
#include <hwloc.h>
#endif
#include "fpga_dma.h"
#include "fpga_pattern_gen.h"
#include "fpga_pattern_checker.h"
#include "fpga_dma_common.h"

#define STDMA_AFU_ID				"EB59BF9D-B211-4A4E-B3E3-753CE68634BA"
#define MMDMA_AFU_ID				"331DB30C-9885-41EA-9081-F88B8F655CAA"
// Single pattern is represented as 64Bytes
#define PATTERN_WIDTH 64
// No. of Patterns
#define PATTERN_LENGTH 32
#define MIN_PAYLOAD_LEN 64
#define MAX_PAYLOAD_LEN  4 * 1024U * 1024U * 1024U
#define MAX_DECIM_FACTOR (0xFFFF)
#define CONFIG_UNINIT (0)
#define BEAT_SIZE (64) // bytes

#define FPGA_DMA_TWO_TO_ONE_MUX_CSR (0x40)
#define FPGA_DMA_ONE_TO_TWO_MUX_CSR (0x50)
#define FPGA_DMA_DECIMATOR_CSR (0x48)

#ifndef USE_ASE
//#include <hwloc.h>
#endif

#define STR_CONST_CMP(str, str_const) strncmp(str, str_const, sizeof(str_const))

enum dma_loopback {
	DMA_INVAL_LOOPBACK = 0,
	DMA_LOOPBACK_ON,
	DMA_LOOPBACK_OFF
};

enum dma_test_direction {
	DMA_INVAL_DIRECTION = 0,
	DMA_MTOS,
	DMA_STOM,
	DMA_MTOM
};

enum dma_test_transfer_type {
	DMA_INVAL_TRANSFER_TYPE = 0,
	DMA_TRANSFER_FIXED,
	DMA_TRANSFER_PACKET
};

struct config {
	int bus;
	int device;
	int function;
	int segment;
	uint64_t data_size;
	uint64_t payload_size;
	enum dma_test_direction direction;
	enum dma_test_transfer_type transfer_type;
	enum dma_loopback loopback;
	uint16_t decim_factor;
	uint64_t fpga_addr;
};

typedef union {
	uint64_t reg;
	struct {
		uint64_t en:1;
		uint64_t rsvd1:15;
		uint64_t factor:16;
		uint64_t counter:16;
		uint64_t rsvd2:16;
	} dc;
} decimator_config_t;

int find_accelerator(const char *afu_id, struct config *config, fpga_token *afu_tok);
fpga_result configure_numa(fpga_token afc_token, bool cpu_affinity, bool memory_affinity);
fpga_result do_action(struct config *config, fpga_token afc_tok);

#endif
