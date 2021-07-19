// Copyright(c) 2018, Intel Corporation
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

/**
 * \fpga_pattern_checker.h
 * \brief Pattern Checker
 */

#ifndef __PATTERN_CHECKER_H__
#define __PATTERN_CHECKER_H__
#include <opae/fpga.h>

// Pattern checker slave port
#define M2S_PATTERN_CHECKER_MEMORY_SLAVE  0x1000
#define M2S_PATTERN_CHECKER_CSR   0x3000
// Single pattern is represented as 64Bytes
#define PATTERN_WIDTH 64
// No. of Patterns
#define PATTERN_LENGTH 32
#define QWORD_BYTES 8
#define DWORD_BYTES 4

/*
Pattern Checker Register map:

|-------------------------------------------------------------------------------------------------------------------------------------------------------|
|  Address   |   Access Type   |                                                            Bits                                                        |
|            |                 |------------------------------------------------------------------------------------------------------------------------|
|            |                 |            31..24            |            23..16            |            15..8            |            7..0            |
|-------------------------------------------------------------------------------------------------------------------------------------------------------|
|     0      |       r/w       |                                                       Payload Length                                                   |
|     4      |       r/w       |                        Pattern Position                                             Pattern Length                     |
|     8      |       r/w       |                                                          Control                                                       |
|     12     |      r/wclr     |                                                          Status                                                        |
|-------------------------------------------------------------------------------------------------------------------------------------------------------|


Address 0  --> Bits 31:0 are used store the payload length.  This field should only be written to while the checker is stopped.  This field is ignored if the
               infinite length test enable bit is set in the control register.
Address 4  --> Bits 15:0 is used to store pattern length, bits 31:16 used to store a new position in the pattern.  The position will update as the checker is
               operating.
               These fields should only be written to while the checker core is stopped.
Address 8  --> Bit 0 (infinite length test enable)is used to instruct the checker to ignore the payload length field and check the pattern until stopped.
               Bit 1 (checking complete IRQ mask) is the checking complete interrupt mask bit.
               Bit 2 (failure detected IRQ mask) is the checking failure interrupt mask bit.
               Bit 3 (accept only packet data enable) is used to instruct the checker to ignore any data received before SOP and after EOP.
               Bit 4 (stop on failure detection enable) is used to stop the checker when a failure is detected
               Bit 31 (checker enable) is used to start the checker core so that it begins receiving data.  This field must be set at the same time or after all the
                      other fields bits are programmed.  This field must be cleared before modifying other fields.  This field is cleared automatically when checking
                      completes.
Address 12 --> Bit 0 (checker operating) when set the checker is operational.
               Bit 1 (checker complete) is set when the checker completes the test.
               Bit 2 (error detected) is set when the checker detects an error during the test.
               Bit 3 (irq) is set when the IRQ fires, write a 1 to this bit to clear the interrupt
               Bit 4 (error count overflow) when the error counter overflows this bit is set.  This bit is cleared when the checker is started.
               Bits 31:16 (error count) each time an error is detected this counter is incremented.  This counter is cleared when the checker is started.
*/

typedef union {
	uint32_t reg;
	struct {
		uint32_t reserved1:3;
		uint32_t pkt_data_only_en:1;
		uint32_t stop_on_failure_en:1;
		uint32_t reserved2:26;
		uint32_t go:1;
	} ;
} pattern_checker_ctrl_t;

typedef union {
	uint32_t reg;
	struct {
		uint32_t busy:1;
		uint32_t complete:1;
		uint32_t err:1;
		uint32_t irq:1;
		uint32_t err_cnt_overflow:1;
		uint32_t reserved:11;
		uint32_t err_cnt:16;
	} st;
} pattern_checker_status_t;

typedef struct __attribute__((__packed__,__aligned__(32))) {
	//0x0
	uint32_t payload_len;
	//0x4
	uint16_t pattern_len;
	uint16_t pattern_pos;
	//0x8
	pattern_checker_ctrl_t control;
	//0xC
	pattern_checker_status_t status;
} pattern_checker_control_t;

fpga_result populate_pattern_checker(fpga_handle fpga_h);
fpga_result checker_copy_to_mmio(fpga_handle fpga_h, uint32_t *checker_ctrl_addr, int len);
fpga_result start_checker(fpga_handle fpga_h, uint64_t transfer_len);
fpga_result wait_for_checker_complete(fpga_handle fpga_h);
fpga_result stop_checker(fpga_handle fpga_h);

#endif //__PATTERN_CHECKER_H__
