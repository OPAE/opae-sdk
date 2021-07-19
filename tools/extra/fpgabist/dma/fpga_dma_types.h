// Copyright(c) 2017, Intel Corporation
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
 * \fpga_dma_types.h
 * \brief FPGA DMA BBB Types
 *
 */

#ifndef __FPGA_DMA_TYPES_H__
#define __FPGA_DMA_TYPES_H__

#include <opae/fpga.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	bool eop_arrived;
	size_t bytes_transferred;
} fpga_dma_transfer_status_t;

// Callback for asynchronous DMA transfers
typedef void (*fpga_dma_transfer_cb)(void *context, fpga_dma_transfer_status_t status);

// Supported DMA transfers
typedef enum {
	HOST_MM_TO_FPGA_ST = 0, // host to AFU sreaming
	FPGA_ST_TO_HOST_MM,     // AFU to host streaming
	FPGA_MM_TO_FPGA_ST,     // local mem to AFU streaming
	FPGA_ST_TO_FPGA_MM,     // AFU to local mem streaming
	HOST_MM_TO_FPGA_MM,     // host memory to fpga memory
	FPGA_MM_TO_HOST_MM,     // fpga memory to host memory
	FPGA_MAX_TRANSFER_TYPE
} fpga_dma_transfer_type_t;

// Supported TX control values
typedef enum {
	TX_NO_PACKET = 0, // deterministic length transfer
	GENERATE_SOP,
	GENERATE_EOP,
	GENERATE_SOP_AND_EOP,
	FPGA_MAX_TX_CTRL
} fpga_dma_tx_ctrl_t;

// Supported RX control values
typedef enum {
	RX_NO_PACKET = 0, // deterministic length transfer
	END_ON_EOP,
	FPGA_MAX_RX_CTRL
} fpga_dma_rx_ctrl_t;

// Channel types
typedef enum {
	TX_ST = 0,
	RX_ST,	
	MM
} fpga_dma_channel_type_t;

// Opaque object that describes a DMA transfer
typedef struct fpga_dma_transfer *fpga_dma_transfer_t;

// Opaque object that describes DMA channel
typedef struct fpga_dma_handle *fpga_dma_handle_t;


#ifdef __cplusplus
}
#endif

#endif // __FPGA_DMA_TYPES_H__
