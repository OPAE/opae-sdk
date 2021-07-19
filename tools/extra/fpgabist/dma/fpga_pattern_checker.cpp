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
 * \fpga_pattern_checker.c
 * \brief  Pattern Checker
 */
#include "fpga_dma_common.h"
#include "fpga_pattern_checker.h"
#include <math.h>
#include <unistd.h>
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
/* Helper functions for Pattern Checker  */
//Populate repeating pattern 0x00...0xFF checker of PATTERN_LENGTH*PATTERN_WIDTH
fpga_result populate_pattern_checker(fpga_handle fpga_h) {
	size_t i, j;
	if(!fpga_h) 
		return FPGA_INVALID_PARAM;

	fpga_result res = FPGA_OK;
	uint64_t custom_checker_addr = (uint64_t)M2S_PATTERN_CHECKER_MEMORY_SLAVE;
	uint32_t test_word = 0x03020100;
	for (i = 0; i < PATTERN_LENGTH; i++) {
		for (j = 0; j < (PATTERN_WIDTH/sizeof(test_word)); j++) {
			res = fpgaWriteMMIO32(fpga_h, 0, custom_checker_addr, test_word);
			if(res != FPGA_OK)
				return res;
			custom_checker_addr += sizeof(test_word);
			if(test_word == 0xfffefdfc){
				test_word = 0x03020100;
				continue;
			}
			test_word += 0x04040404;
		}
	}
	return res;
}

fpga_result checker_copy_to_mmio(fpga_handle fpga_h, uint32_t *checker_ctrl_addr, int len) {
	int i=0;
	fpga_result res = FPGA_OK;
	if(len % DWORD_BYTES != 0) 
		return FPGA_INVALID_PARAM;
	uint64_t checker_csr = (uint64_t)M2S_PATTERN_CHECKER_CSR;
	for(i = 0; i < len/DWORD_BYTES; i++) {
		res = fpgaWriteMMIO32(fpga_h, 0, checker_csr, *checker_ctrl_addr);
		if(res != FPGA_OK)
			return res;
		checker_ctrl_addr += 1;
		checker_csr += DWORD_BYTES;
	}

	return FPGA_OK;
}
// Write to the pattern checker registers
// Set Payload Length(Represented in terms of 64B elements)
// Set Pattern Length (Represented in terms of 64B elements)
// Set Pattern Position
// Set the control bits
fpga_result start_checker(fpga_handle fpga_h, uint64_t transfer_len) {
	fpga_result res = FPGA_OK;
	pattern_checker_control_t checker_ctrl;
	pattern_checker_status_t status ={0};

	memset(&checker_ctrl, 0, sizeof(pattern_checker_control_t));

	checker_ctrl.payload_len = ceil(transfer_len/(double)PATTERN_WIDTH);
	checker_ctrl.pattern_len = PATTERN_LENGTH;
	checker_ctrl.pattern_pos = 0;
	checker_ctrl.control.pkt_data_only_en = 0;
	checker_ctrl.control.stop_on_failure_en = 0;
	// start the checker
	checker_ctrl.control.go = 1;

	do {
		res = fpgaReadMMIO32(fpga_h, 0, M2S_PATTERN_CHECKER_CSR+offsetof(pattern_checker_control_t, status), &status.reg);
		ON_ERR_GOTO(res, out, "fpgaReadMMIO32");
	}while(status.st.busy);
	
	// Write to Registers using MMIO Api's
	res = checker_copy_to_mmio(fpga_h, (uint32_t*)&checker_ctrl, (sizeof(checker_ctrl)-sizeof(checker_ctrl.status)));
out:	
	return res;
}

fpga_result wait_for_checker_complete(fpga_handle fpga_h) {
	fpga_result res = FPGA_OK;
	pattern_checker_status_t status ={0};
	int wait_time = 5; //seconds, checker times out after max wait time
	
	do {
		res = fpgaReadMMIO32(fpga_h, 0, M2S_PATTERN_CHECKER_CSR+offsetof(pattern_checker_control_t, status), &status.reg);
		ON_ERR_GOTO(res, out, "fpgaReadMMIO32");
		sleep(1);
		wait_time--;
		debug_print("Wait time = %d\n", wait_time);
	} while(status.st.complete != 1 && wait_time);
	
	if(wait_time == 0) {
		res = FPGA_EXCEPTION;
		FPGA_DMA_ERR("checker timed out\n");
	} else if(status.st.err == 0) {
		res = FPGA_OK;
		debug_print("checker verified success!\n");
	} else {
		res = FPGA_EXCEPTION;
		FPGA_DMA_ERR("data verification error\n");
	}

out:
	return res;
}
// Checker should be stopped before populating the Checker registers and starting pattern checker
fpga_result stop_checker(fpga_handle fpga_h) {
	fpga_result res = FPGA_OK;

	res = fpgaWriteMMIO32(fpga_h, 0, M2S_PATTERN_CHECKER_CSR+offsetof(pattern_checker_control_t, control), 0x0);
	ON_ERR_GOTO(res, out, "fpgaWriteMMIO32");
out:
	return res;
}

