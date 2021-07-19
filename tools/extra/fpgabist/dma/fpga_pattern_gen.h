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
 * \fpga_pattern_gen.h
 * \brief Pattern generator header
 */

#ifndef __PATTERN_GENERATOR_H__
#define __PATTERN_GENERATOR_H__
#include <opae/fpga.h>

// Pattern generator slave port
#define S2M_PATTERN_GENERATOR_MEMORY_SLAVE 0x2000
#define S2M_PATTERN_GENERATOR_CSR  0x3010

// Single pattern is represented as 64Bytes
#define PATTERN_WIDTH 64
// No. of Patterns
#define PATTERN_LENGTH 32
#define QWORD_BYTES 8
#define DWORD_BYTES 4

/*
Pattern Generator Register map:

|-------------------------------------------------------------------------------------------------------------------------------------------------------|
|  Address   |   Access Type   |                                                            Bits                                                        |
|            |                 |------------------------------------------------------------------------------------------------------------------------|
|            |                 |            31..24            |            23..16            |            15..8            |            7..0            |
|-------------------------------------------------------------------------------------------------------------------------------------------------------|
|     0      |       r/w       |                                                       Payload Length                                                   |
|     4      |       r/w       |                        Pattern Position                                             Pattern Length                     |
|     8      |       r/w       |                                                          Control                                                       |
|     12     |       N/A       |                                                          Status                                                        |
|-------------------------------------------------------------------------------------------------------------------------------------------------------|


Address 0  --> Bits 31:0 are used store the payload length as well as read it back while the generator core is operating.  This field should only be written to while
               the generator is stopped.
Address 4  --> Bits 15:0 is used to store pattern length, bits 31:16 used to store a new position in the pattern.  The position will update as the generator is operating.
               These fields should only be written to while the generator core is stopped.
Address 8  --> Bit 0 (infinite length test enable)is used to instruct the generator to ignore the payload length field and generate the pattern until stopped.
               Bit 1 (generation complete IRQ mask) is the generation complete interrupt mask bit.  This bit is only applicable to length bound pattern generation.
               Bit 2 (sop generation) when set, the generator will output SOP on the first beat of the pattern.
               Bit 3 (eop generation) when set, the generator will output EOP on the last beat of the pattern.  EOP will also be generated if the generator is stopped.
               Bit 31(generator enable) is used to start the generator core so that it begins receiving data.  This field must be set at the same time or after all the
                      other fields bits are programmed.  This field must be cleared before modifying other fields.  This field is cleared automatically when checking
                      completes.
Address 12 --> Bit 0 (generator operating) when set the generator is operational.
               Bit 1 (generator complete) is set when the generator completes generating the pattern.
               Bit 2 (irq) is set when the IRQ fires, write a 1 to this bit to clear the interrupt.

*/
typedef union {
	uint32_t reg;
	struct {
		uint32_t reserved1:2;
		uint32_t generate_sop:1;
		uint32_t generate_eop:1;
		uint32_t reserved2:4;
		uint32_t last_tf_size:8;
		uint32_t reserved3:15;
		uint32_t go:1;
	} ;
} pattern_gen_ctrl_t;

typedef union {
	uint32_t reg;
	struct {
		uint32_t busy:1;
		uint32_t complete:1;
		uint32_t irq:1;
		uint32_t reserved:29;
	} st;
} pattern_gen_status_t;

typedef struct __attribute__((__packed__,__aligned__(32))) {
	//0x0
	uint32_t payload_len;
	//0x4
	uint16_t pattern_len;
	uint16_t pattern_pos;
	//0x8
	pattern_gen_ctrl_t control;
	//0xC
	pattern_gen_status_t status;
} pattern_gen_control_t;

fpga_result populate_pattern_generator(fpga_handle fpga_h);
fpga_result generator_copy_to_mmio(fpga_handle fpga_h, uint32_t *generator_ctrl_addr, int len);
fpga_result start_generator(fpga_handle fpga_h, uint64_t transfer_len, int pkt_transfer);
fpga_result wait_for_generator_complete(fpga_handle fpga_h);
fpga_result stop_generator(fpga_handle fpga_h);
#endif //__PATTERN_GENERATOR_H__
