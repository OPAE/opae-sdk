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

/**
 * \fpga_dma.h
 * \brief FPGA DMA BBB API Header
 *
 */

#ifndef __FPGA_DMA_H__
#define __FPGA_DMA_H__

#include <opae/fpga.h>
#include <opae/types.h>
#include <opae/dma_types.h>

#ifdef __cplusplus
extern "C" {
#endif

fpga_result fpgaDmaEnumerate(fpga_handle fpga,
			     fpga_dma_token *dma_tokens,
			     uint32_t *num_tokens);

fpga_result fpgaDmaOpen(fpga_dma_handle *dma_handle,
			fpga_dma_token dma_token,
			int flags);

fpga_result fpgaDmaClose(fpga_dma_handle dma_handle);

fpga_result fpgaDmaTokenDestroy(fpga_dma_token *dma_token);

fpga_result fpgaDmaTransferSync(fpga_dma_handle dma_handle,
				const fpga_dma_transfer dma_transfer);

fpga_result fpgaDmaTransferAsync(fpga_dma_handle dma_handle,
				 const fpga_dma_transfer dma_transfer,
				 int *fd);

fpga_result fpgaDMATransferInit(fpga_dma_transfer *dma_transfer);

fpga_result fpgaDMATransferDestroy(fpga_dma_transfer dma_transfer);

fpga_result fpgaDMATransferSetSrc(fpga_dma_transfer dma_transfer,
				  uint64_t src);

fpga_result fpgaDMATransferSetDst(fpga_dma_transfer dma_transfer,
				  uint64_t dst);

fpga_result fpgaDMATransferSetLen(fpga_dma_transfer dma_transfer,
				  uint64_t len);

fpga_result fpgaDMATransferSetTransferType(fpga_dma_transfer dma_transfer,
					   fpga_dma_transfer_type type);

fpga_result fpgaDMATransferSetRxControl(fpga_dma_transfer dma_transfer,
					fpga_dma_rx_ctrl rx_ctrl);

fpga_result fpgaDMATransferSetTxControl(fpga_dma_transfer dma_transfer,
					fpga_dma_tx_ctrl tx_ctrl);

fpga_result fpgaDMATransferSetTransferCallback(fpga_dma_transfer dma_transfer,
					       fpga_dma_transfer_cb cb,
					       void* ctxt);
fpga_result fpgaDMAGetBytesTransferred(fpga_dma_transfer dma_transfer,
				       size_t *rx_bytes);

fpga_result fpgaDMATransferCheckEopArrived(fpga_dma_transfer transfer,
					   bool *eop_arrived);

#ifdef __cplusplus
}
#endif
#endif // __FPGA_DMA_H__
