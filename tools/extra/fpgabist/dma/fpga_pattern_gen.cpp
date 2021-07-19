// Copyright(c) 2018, Intel Corporation
//
// Redistribution  and  use  in source and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//  this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//  may be used to  endorse or promote  products derived  from this  software
//  without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMEdesc.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING, BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,   WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,   EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * \fpga_pattern_gen.c
 * \brief  Pattern Generator
 */

#include "fpga_pattern_gen.h"
#include <math.h>
/*
 * macro for checking return codes
 */
#define ON_ERR_GOTO(res, label, desc)\
	do {\
		if ((res) != FPGA_OK) {\
			err_cnt++;\
			fprintf(stderr, "Error %s: %s\n", (desc), fpgaErrStr(res));\
			goto label;\
		}\
	} while (0)
static int err_cnt = 0;

// Populate repeating pattern 0x00...0xFF of PATTERN_LENGTH*PATTERN_WIDTH  
fpga_result populate_pattern_generator(fpga_handle fpga_h) {
	size_t i, j;
	if(!fpga_h) {
		return FPGA_INVALID_PARAM;
	}
	fpga_result res = FPGA_OK;
	uint64_t custom_generator_mem_addr = (uint64_t)S2M_PATTERN_GENERATOR_MEMORY_SLAVE;
	uint32_t test_word = 0x03020100;
	for (i = 0; i < PATTERN_LENGTH; i++) {
		for (j = 0; j < (PATTERN_WIDTH/sizeof(test_word)); j++) {
			res = fpgaWriteMMIO32(fpga_h, 0, custom_generator_mem_addr, test_word);
			if(res != FPGA_OK)
				return res;
			custom_generator_mem_addr += sizeof(test_word);
			if(test_word == 0xfffefdfc) {
				test_word = 0x03020100;
				continue;
			}
			test_word += 0x04040404;
		}
	}
	return res;
}

fpga_result generator_copy_to_mmio(fpga_handle fpga_h, uint32_t *generator_ctrl_addr, int len) {
	int i=0;
	fpga_result res = FPGA_OK;
	if(len % DWORD_BYTES != 0) return FPGA_INVALID_PARAM;
	uint64_t generator_csr = (uint64_t)S2M_PATTERN_GENERATOR_CSR;
	for(i = 0; i < len/DWORD_BYTES; i++) {
		res = fpgaWriteMMIO32(fpga_h, 0, generator_csr, *generator_ctrl_addr);
		if(res != FPGA_OK)
			return res;
		generator_ctrl_addr += 1;
		generator_csr += DWORD_BYTES;
	}

	return FPGA_OK;
}
// Write to the pattern generator registers
// Set Payload Length(Represented in terms of 64B elements)
// Set Pattern Length (Represented in terms of 64B elements)
// Set Pattern Position
// Set the control bits
fpga_result start_generator(fpga_handle fpga_h, uint64_t transfer_len, int pkt_transfer) {
	fpga_result res = FPGA_OK;
	pattern_gen_control_t generator_ctrl;
	pattern_gen_status_t status ={0};

	memset(&generator_ctrl, 0, sizeof(pattern_gen_control_t));

	generator_ctrl.payload_len = ceil(transfer_len/(double)PATTERN_WIDTH);
	generator_ctrl.pattern_len = PATTERN_LENGTH;
	generator_ctrl.pattern_pos = 0;
	if(pkt_transfer) {
		generator_ctrl.control.generate_sop = 1;
		generator_ctrl.control.generate_eop = 1;
	} else {
		generator_ctrl.control.generate_sop = 0;
		generator_ctrl.control.generate_eop = 0;
	}
	//Set to no. of valid bytes in last transfer
	//last_tf_size = transfer_len % 64
	generator_ctrl.control.last_tf_size = transfer_len%64;
	generator_ctrl.control.go = 1;

	do {
		res = fpgaReadMMIO32(fpga_h, 0, S2M_PATTERN_GENERATOR_CSR+offsetof(pattern_gen_control_t, status), &status.reg);
		ON_ERR_GOTO(res, out, "fpgaReadMMIO32");
	}while(status.st.busy);

	res = generator_copy_to_mmio(fpga_h, (uint32_t*)&generator_ctrl, (sizeof(generator_ctrl)-sizeof(generator_ctrl.status)));
	ON_ERR_GOTO(res, out, "generator_copy_to_mmio");
out:
	return res;
}


fpga_result wait_for_generator_complete(fpga_handle fpga_h) {
	fpga_result res = FPGA_OK;
	pattern_gen_status_t status ={0};

	do {
		res = fpgaReadMMIO32(fpga_h, 0, S2M_PATTERN_GENERATOR_CSR+offsetof(pattern_gen_control_t, status), &status.reg);
		ON_ERR_GOTO(res, out, "fpgaReadMMIO32");
	} while(status.st.complete != 1);
#ifdef DEBUG
	printf("S2M Generator:Finished sending data\n");
#endif
out:
	return res;
}

fpga_result stop_generator(fpga_handle fpga_h) {
	fpga_result res = FPGA_OK;

	res = fpgaWriteMMIO32(fpga_h, 0, S2M_PATTERN_GENERATOR_CSR+offsetof(pattern_gen_control_t, control), 0x0);
	ON_ERR_GOTO(res, out, "fpgaWriteMMIO32");
out:
	return res;
}
/* END of Helper functions for Pattern Generator */
