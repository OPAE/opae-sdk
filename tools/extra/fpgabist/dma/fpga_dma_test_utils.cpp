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
 * \fpga_dma_test_utils.c
 * \brief DMA test utils
 */
#include <iostream>
#include <cmath>
#include "fpga_dma_test_utils.h"
#include "fpga_dma_common.h"

using namespace std;

static sem_t transfer_done;

static int err_cnt = 0;

#define UNUSED(x) (void)(x)

#define ON_ERR_GOTO(res, label, desc)\
	do {\
		if ((res) != FPGA_OK) {\
			err_cnt++;\
			fprintf(stderr, "Error %s: %s\n", (desc), fpgaErrStr(res));\
			goto label;\
		}\
	} while (0)

static void transferComplete(void *ctx, fpga_dma_transfer_status_t status) {
	UNUSED(ctx);
	UNUSED(status);
	return;
}

//Verify repeating pattern 0x00...0xFF of payload_size
static fpga_result verify_buffer(unsigned char *buf, size_t payload_size, uint16_t decim_factor) {
	size_t i,j;
	unsigned char test_word = 0;
	uint64_t byte_cnt = 1;

	while(payload_size) {
		test_word = 0x00;
		for (i = 0; i < PATTERN_LENGTH; i++) {
			for (j = 0; j < (PATTERN_WIDTH/sizeof(test_word)); j++) {
				if(!payload_size)
					goto out;
				if((*buf) != test_word) {
					printf("Invalid data at byte %zd Expected = %x Actual = %x\n",byte_cnt,test_word,(*buf));
					return FPGA_EXCEPTION;
				}
				if(byte_cnt % BEAT_SIZE == 0 &&	byte_cnt / BEAT_SIZE != 0 /*beat boundary*/ && decim_factor != 0) {
					test_word += (decim_factor * BEAT_SIZE);
				}
				++test_word;

				buf++;
				byte_cnt++;
				payload_size--;
			}
		}
	}
out:
	return FPGA_OK;
}


//Populate repeating pattern 0x00...0xFF of payload size
static void fill_buffer(unsigned char *buf, size_t payload_size) {
	size_t i,j;
	unsigned char test_word = 0;
	while(payload_size) {
		test_word = 0x00;
		for (i = 0; i < PATTERN_LENGTH; i++) {
			for (j = 0; j < (PATTERN_WIDTH/sizeof(test_word)); j++) {
				if(!payload_size)
					return;
				*buf = test_word;
				payload_size -= sizeof(test_word);
				buf++;
				test_word += 0x01;
			}
		}
	}
}

static double getBandwidth(size_t size, double seconds) {
	double throughput = (double)size/((double)seconds*1000*1000);
	return std::round(throughput);
}

// return elapsed time
static double getTime(struct timespec start, struct timespec end) {
	uint64_t diff = 1000000000L * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	return (double) diff/(double)1000000000L;
}

static fpga_result prepare_checker(fpga_handle afc_h, uint64_t size)
{
	fpga_result res;

	res = populate_pattern_checker(afc_h);
	ON_ERR_GOTO(res, out, "populate_pattern_checker");
	debug_print("populated checker\n");

	res = stop_checker(afc_h);
	ON_ERR_GOTO(res, out, "stop_checker");
	debug_print("stopped checker\n");

	res = start_checker(afc_h, size);
	ON_ERR_GOTO(res, out, "start checker");
	debug_print("started checker\n");

out:
	return res;
}

static fpga_result wait_checker(fpga_handle afc_h)
{
	fpga_result res;

	debug_print("waiting checker\n");
	res = wait_for_checker_complete(afc_h);
	ON_ERR_GOTO(res, out, "wait checker complete");

	debug_print("stopping checker\n");
	res = stop_checker(afc_h);
out:
	return res;
}


static fpga_result prepare_generator(fpga_handle afc_h, uint64_t size, fpga_dma_rx_ctrl_t rx_ctrl)
{
	fpga_result res;
	res = populate_pattern_generator(afc_h);
	ON_ERR_GOTO(res, out, "populating pattern generator");
	debug_print("populated generator\n");

	res = stop_generator(afc_h);
	ON_ERR_GOTO(res, out, "stopping generator");
	debug_print("stopped generator\n");

	if(rx_ctrl == RX_NO_PACKET)
		res = start_generator(afc_h, size, 0);
	else if (rx_ctrl == END_ON_EOP)
		res = start_generator(afc_h, size, 1);
	
	ON_ERR_GOTO(res, out, "starting generator");
	debug_print("started generator\n");

out:
	return res;
}

static fpga_result wait_generator(fpga_handle afc_h)
{
	fpga_result res;
	res = wait_for_generator_complete(afc_h);
	ON_ERR_GOTO(res, out, "waiting generator");
	debug_print("generator complete\n");

	res = stop_generator(afc_h);
	ON_ERR_GOTO(res, out, "stopping generator");
	debug_print("generator stopped\n");

out:
	return res;
}

struct buf_attrs {
	void *va;
	uint64_t iova;
	uint64_t wsid;
	uint64_t size;
};


static fpga_result allocate_buffer(fpga_handle afc_h, struct buf_attrs *attrs)
{
	fpga_result res;
	if(!attrs)
		return FPGA_INVALID_PARAM;

	res = fpgaPrepareBuffer(afc_h, attrs->size, (void **)&(attrs->va), &attrs->wsid, 0);
	if(res != FPGA_OK)
		return res;

	res = fpgaGetIOAddress(afc_h, attrs->wsid, &attrs->iova);
	if(res != FPGA_OK) {
		res = fpgaReleaseBuffer(afc_h, attrs->wsid);
		return res;
	}
	debug_print("Allocated test buffer of size = %ld bytes\n", attrs->size);
	return FPGA_OK;
}

static fpga_result free_buffer(fpga_handle afc_h, struct buf_attrs *attrs)
{
	if(!attrs)
		return FPGA_INVALID_PARAM;

	return fpgaReleaseBuffer(afc_h, attrs->wsid);
}


static fpga_result loopback_test(fpga_handle afc_h, fpga_dma_handle_t tx_dma_h, fpga_dma_handle_t rx_dma_h, struct config *config) {
	uint64_t total_size;
	int64_t tid;
	uint64_t src;
	fpga_dma_transfer_t transfer;
	fpga_result res = FPGA_OK;
	struct timespec start, end;

	// configure loopback on
	uint64_t loopback_en = (uint64_t)0x1;
	fpgaWriteMMIO64(afc_h, 0, FPGA_DMA_TWO_TO_ONE_MUX_CSR, loopback_en);
	fpgaWriteMMIO64(afc_h, 0, FPGA_DMA_ONE_TO_TWO_MUX_CSR, loopback_en);

	// configure decimation factor and turn on decimator
	decimator_config_t dconfig = {0};
	dconfig.dc.en = 0;
	dconfig.dc.counter = 0;
	dconfig.dc.factor = config->decim_factor;
	fpgaWriteMMIO64(afc_h, 0, FPGA_DMA_DECIMATOR_CSR, dconfig.reg);
	dconfig.dc.en = 1;
	fpgaWriteMMIO64(afc_h, 0, FPGA_DMA_DECIMATOR_CSR, dconfig.reg);

	struct buf_attrs battrs_src = {
		.va = NULL,
		.iova = 0,
		.wsid = 0,
		.size = 0
	};

	struct buf_attrs battrs_dst = {
		.va = NULL,
		.iova = 0,
		.wsid = 0,
		.size = 0
	};

	battrs_src.size = config->data_size;
	res = allocate_buffer(afc_h, &battrs_src);	
	ON_ERR_GOTO(res, out, "allocating battrs_src");
	debug_print("allocated src test buffer va = %p, iova = %lx, wsid = %lx, size = %ld\n", battrs_src.va, battrs_src.iova, battrs_src.wsid, battrs_src.size);
	fill_buffer((unsigned char *)battrs_src.va, config->data_size);

	battrs_dst.size = config->data_size;
	res = allocate_buffer(afc_h, &battrs_dst);	
	ON_ERR_GOTO(res, out, "allocating battrs_dst");
	debug_print("allocated dst test buffer va = %p, iova = %lx, wsid = %lx, size = %ld\n", battrs_dst.va, battrs_dst.iova, battrs_dst.wsid, battrs_dst.size);
	memset(battrs_dst.va, 0, config->data_size);

	res = fpgaDMATransferInit(&transfer);
	ON_ERR_GOTO(res, out, "allocating transfer");
	debug_print("init transfer\n");

	// do memory to stream transfer
	total_size = config->data_size;
	tid = ceil((double)config->data_size / (double)config->payload_size);
	src = battrs_src.iova;

	clock_gettime(CLOCK_MONOTONIC, &start);
	fpga_dma_tx_ctrl_t tx_ctrl;
	if(config->transfer_type == DMA_TRANSFER_FIXED)
		tx_ctrl = TX_NO_PACKET;
	else
		tx_ctrl = GENERATE_SOP_AND_EOP;
	while(total_size > 0) {
		uint64_t transfer_bytes = MIN(total_size, config->payload_size);
		//debug_print("Transfer src=%lx, dst=%lx, bytes=%ld\n", (uint64_t)src, (uint64_t)0, transfer_bytes);

		fpgaDMATransferSetSrc(transfer, src);
		fpgaDMATransferSetDst(transfer, (uint64_t)0);
		fpgaDMATransferSetLen(transfer, transfer_bytes);
		fpgaDMATransferSetTransferType(transfer, HOST_MM_TO_FPGA_ST);
		fpgaDMATransferSetTxControl(transfer, tx_ctrl);
		// perform non-blocking transfers
		if(tid == 1)
			fpgaDMATransferSetLast(transfer, true);
		else
			fpgaDMATransferSetLast(transfer, false);
		fpgaDMATransferSetTransferCallback(transfer, transferComplete, NULL);

		res = fpgaDMATransfer(tx_dma_h, transfer);
		ON_ERR_GOTO(res, free_transfer, "transfer error");
		total_size -= transfer_bytes;
		src += transfer_bytes;
		tid--;
	}

	// do stream to memory transfer
	fpga_dma_rx_ctrl_t rx_ctrl;
	if(config->transfer_type == DMA_TRANSFER_FIXED)
		rx_ctrl = RX_NO_PACKET;
	else
		rx_ctrl = END_ON_EOP;

	int64_t required_beats;
	uint64_t expected_beats;
	expected_beats = 0;
	required_beats = ceil((double)config->data_size / (double)BEAT_SIZE); // beat = 64 bytes
	uint64_t beat_cnt;
	beat_cnt = 0;

	// calculate expected bytes based on decimation factor
	while (required_beats) {
		if(beat_cnt % (config->decim_factor + 1) == 0)
			expected_beats++;
		beat_cnt++;
		required_beats--;
	}

	uint64_t tsize;
	total_size = expected_beats * BEAT_SIZE;
	tsize = total_size;
	tid = ceil((double)total_size / (double)config->payload_size);
	uint64_t dst;
	dst = battrs_dst.iova;
	while(total_size > 0) {
		uint64_t transfer_bytes = MIN(total_size, config->payload_size);
		fpgaDMATransferSetSrc(transfer, (uint64_t)0);
		fpgaDMATransferSetDst(transfer, dst);
		fpgaDMATransferSetLen(transfer, transfer_bytes);
		fpgaDMATransferSetTransferType(transfer, FPGA_ST_TO_HOST_MM);
		fpgaDMATransferSetRxControl(transfer, rx_ctrl);
		if(tid == 1) {
			fpgaDMATransferSetLast(transfer, true);
			fpgaDMATransferSetTransferCallback(transfer, NULL, NULL);
		} else {
			fpgaDMATransferSetLast(transfer, false);
			fpgaDMATransferSetTransferCallback(transfer, transferComplete, NULL);
		}

		res = fpgaDMATransfer(rx_dma_h, transfer);
		ON_ERR_GOTO(res, free_transfer, "transfer error");
		total_size -= transfer_bytes;
		dst += transfer_bytes;
		tid--;	
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	res = verify_buffer((unsigned char *)battrs_dst.va, tsize, config->decim_factor);
	ON_ERR_GOTO(res, free_transfer, "buffer verify failed");
	std::cout << "PASS! Bandwidth = " << getBandwidth(config->data_size+tsize, getTime(start,end)) << " MB/s" << std::endl;

free_transfer:
	debug_print("destroying transfer\n");
	res = fpgaDMATransferDestroy(&transfer);
	ON_ERR_GOTO(res, out, "destroy transfer");
	debug_print("destroyed transfer\n");

out:
	if(battrs_src.va) {
		debug_print("free app buffer\n");
		free_buffer(afc_h, &battrs_src);
	}

	if(battrs_dst.va) {
		debug_print("free app buffer\n");
		free_buffer(afc_h, &battrs_dst);
	}
	return res;
}

static fpga_result non_loopback_test(fpga_handle afc_h, fpga_dma_handle_t dma_h, struct config *config) {
	fpga_dma_transfer_t transfer;
	fpga_result res = FPGA_OK;
	struct timespec start, end;
	memset((void *)&start, 0, sizeof(timespec));
	memset((void *)&end, 0, sizeof(timespec));
	//start = (struct timespec){ 0 };
	//end = (struct timespec){ 0 };

	struct buf_attrs battrs = {
		.va = NULL,
		.iova = 0,
		.wsid = 0,
		.size = 0
	};

	// configure loopback off
	if(config->direction != DMA_MTOM) {
		uint64_t loopback_en = (uint64_t)0x0;
		fpgaWriteMMIO64(afc_h, 0, FPGA_DMA_TWO_TO_ONE_MUX_CSR, loopback_en);
		fpgaWriteMMIO64(afc_h, 0, FPGA_DMA_ONE_TO_TWO_MUX_CSR, loopback_en);
	}

	battrs.size = config->data_size;
	res = allocate_buffer(afc_h, &battrs);	
	ON_ERR_GOTO(res, out, "allocating buffer");
	debug_print("allocated test buffer va = %p, iova = %lx, wsid = %lx, size = %ld\n", battrs.va, battrs.iova, battrs.wsid, battrs.size);

	res = fpgaDMATransferInit(&transfer);
	ON_ERR_GOTO(res, out, "allocating transfer");
	debug_print("init transfer\n");

	if(config->direction == DMA_MTOM) {
		fill_buffer((unsigned char *)battrs.va, config->data_size);
		debug_print("filled test buffer\n");

		clock_gettime(CLOCK_MONOTONIC, &start);
		uint64_t total_size = config->data_size;
		int64_t tid = ceil((double)config->data_size /(double)config->payload_size);
		uint64_t src = battrs.iova; // host memory addr
		uint64_t dst = config->fpga_addr; // fpga memory addr
		while(total_size > 0) {
			uint64_t transfer_bytes = MIN(total_size, config->payload_size);
			//debug_print("Transfer src=%lx, dst=%lx, bytes=%ld\n", (uint64_t)src, (uint64_t)0, transfer_bytes);

			fpgaDMATransferSetSrc(transfer, src);
			fpgaDMATransferSetDst(transfer, dst);
			fpgaDMATransferSetLen(transfer, transfer_bytes);
			fpgaDMATransferSetTransferType(transfer, HOST_MM_TO_FPGA_MM);
			// perform non-blocking transfers, except for the very last
			if(tid == 1) {
				fpgaDMATransferSetLast(transfer, true);
				fpgaDMATransferSetTransferCallback(transfer, NULL, NULL);
			} else {
				fpgaDMATransferSetTransferCallback(transfer, transferComplete, NULL);
			}

			res = fpgaDMATransfer(dma_h, transfer);
			ON_ERR_GOTO(res, free_transfer, "transfer error");
			total_size -= transfer_bytes;
			src += transfer_bytes;
			dst += transfer_bytes;
			tid--;
		}
		clock_gettime(CLOCK_MONOTONIC, &end);

		// clear recieve buffer
		memset(battrs.va, 0, battrs.size);
		fpgaDMATransferReset(transfer);
		ON_ERR_GOTO(res, free_transfer, "transfer reset error");

		clock_gettime(CLOCK_MONOTONIC, &start);
		total_size = config->data_size;
		tid = ceil((double)config->data_size / (double)config->payload_size);
		src = config->fpga_addr;
		dst = battrs.iova;
		while(total_size > 0) {
			uint64_t transfer_bytes = MIN(total_size, config->payload_size);

			fpgaDMATransferSetSrc(transfer, src);
			fpgaDMATransferSetDst(transfer, dst);
			fpgaDMATransferSetLen(transfer, transfer_bytes);
			fpgaDMATransferSetTransferType(transfer, FPGA_MM_TO_HOST_MM);
			if(tid == 1) {
				fpgaDMATransferSetLast(transfer, true);
				fpgaDMATransferSetTransferCallback(transfer, NULL, NULL);
			} else {
				fpgaDMATransferSetTransferCallback(transfer, transferComplete, NULL);
			}

			res = fpgaDMATransfer(dma_h, transfer);
			ON_ERR_GOTO(res, free_transfer, "transfer error");
			total_size -= transfer_bytes;
			dst += transfer_bytes;
			src += transfer_bytes;
			tid--;
		}
		ON_ERR_GOTO(res, free_transfer, "transfer error");
		clock_gettime(CLOCK_MONOTONIC, &end);

		res = verify_buffer((unsigned char *)battrs.va, config->data_size, 0/*decimation factor*/);
		ON_ERR_GOTO(res, free_transfer, "buffer verify failed");
	}
	if(config->direction == DMA_MTOS) {
		fill_buffer((unsigned char *)battrs.va, config->data_size);
		debug_print("filled test buffer\n");

		fpga_dma_tx_ctrl_t tx_ctrl;
		if(config->transfer_type == DMA_TRANSFER_FIXED)
			tx_ctrl = TX_NO_PACKET;
		else
			tx_ctrl = GENERATE_SOP_AND_EOP;

		#if !EMU_MODE
		res = prepare_checker(afc_h, config->data_size);
		ON_ERR_GOTO(res, free_transfer, "preparing checker");
		debug_print("checker prepared\n");
		#endif

		clock_gettime(CLOCK_MONOTONIC, &start);
		uint64_t total_size = config->data_size;
		int64_t tid = ceil(config->data_size / config->payload_size);
		uint64_t src = battrs.iova;
		while(total_size > 0) {
			uint64_t transfer_bytes = MIN(total_size, config->payload_size);
			//debug_print("Transfer src=%lx, dst=%lx, bytes=%ld\n", (uint64_t)src, (uint64_t)0, transfer_bytes);

			fpgaDMATransferSetSrc(transfer, src);
			fpgaDMATransferSetDst(transfer, (uint64_t)0);
			fpgaDMATransferSetLen(transfer, transfer_bytes);
			fpgaDMATransferSetTransferType(transfer, HOST_MM_TO_FPGA_ST);
			fpgaDMATransferSetTxControl(transfer, tx_ctrl);
			// perform non-blocking transfers, except for the very last
			if(tid == 1) {
				fpgaDMATransferSetLast(transfer, true);
				fpgaDMATransferSetTransferCallback(transfer, NULL, NULL);
			} else {
				fpgaDMATransferSetTransferCallback(transfer, transferComplete, NULL);
			}

			res = fpgaDMATransfer(dma_h, transfer);
			ON_ERR_GOTO(res, free_transfer, "transfer error");
			total_size -= transfer_bytes;
			src += transfer_bytes;
			tid--;
		}
		clock_gettime(CLOCK_MONOTONIC, &end);

		#if !EMU_MODE
		res = wait_checker(afc_h);
		ON_ERR_GOTO(res, free_transfer, "checker verify failed");
		#endif

	}
	if(config->direction == DMA_STOM) {
		fpga_dma_rx_ctrl_t rx_ctrl;
		if(config->transfer_type == DMA_TRANSFER_FIXED)
			rx_ctrl = RX_NO_PACKET;
		else
			rx_ctrl = END_ON_EOP;

		memset(battrs.va, 0, config->data_size);
		res = prepare_generator(afc_h, config->data_size , rx_ctrl);
		ON_ERR_GOTO(res, free_transfer, "preparing generator");
		debug_print("generator prepared\n");

		clock_gettime(CLOCK_MONOTONIC, &start);
		uint64_t total_size = config->data_size;
		int64_t tid = ceil(config->data_size / config->payload_size);
		uint64_t dst = battrs.iova;
		while(total_size > 0) {
			uint64_t transfer_bytes = MIN(total_size, config->payload_size);

			fpgaDMATransferSetSrc(transfer, (uint64_t)0);
			fpgaDMATransferSetDst(transfer, dst);
			fpgaDMATransferSetLen(transfer, transfer_bytes);
			fpgaDMATransferSetTransferType(transfer, FPGA_ST_TO_HOST_MM);
			fpgaDMATransferSetRxControl(transfer, rx_ctrl);
			if(tid == 1) {
				fpgaDMATransferSetLast(transfer, true);
				fpgaDMATransferSetTransferCallback(transfer, NULL, NULL);
			} else {
				fpgaDMATransferSetTransferCallback(transfer, transferComplete, NULL);
			}

			res = fpgaDMATransfer(dma_h, transfer);
			ON_ERR_GOTO(res, free_transfer, "transfer error");
			total_size -= transfer_bytes;
			dst += transfer_bytes;
			tid--;
		}
		ON_ERR_GOTO(res, free_transfer, "transfer error");
		clock_gettime(CLOCK_MONOTONIC, &end);

		res = wait_generator(afc_h);
		ON_ERR_GOTO(res, free_transfer, "wait generator");
		debug_print("generator complete\n");

		res = verify_buffer((unsigned char *)battrs.va, config->data_size, 0/*decimation factor*/);
		ON_ERR_GOTO(res, free_transfer, "buffer verify failed");
	}
	std::cout << "PASS! Bandwidth = " << getBandwidth(config->data_size, getTime(start,end)) << " MB/s" << std::endl;

free_transfer:
	if(transfer) {
		debug_print("destroying transfer\n");
		res = fpgaDMATransferDestroy(&transfer);
		ON_ERR_GOTO(res, out, "destroy transfer");
		debug_print("destroyed transfer\n");
	}
out:
	if(battrs.va) {
		debug_print("free app buffer\n");
		free_buffer(afc_h, &battrs);
	}

	return res;
}

fpga_result configure_numa(fpga_token afc_token, bool cpu_affinity, bool memory_affinity)
{
	fpga_result res = FPGA_OK;
	fpga_properties props;
	#ifndef USE_ASE
	// Set up proper affinity if requested
	if (cpu_affinity || memory_affinity) {
		unsigned dom = 0, bus = 0, dev = 0, func = 0;
		int retval;
		#if(FPGA_DMA_DEBUG)
				char str[4096];
		#endif
		res = fpgaGetProperties(afc_token, &props);
		ON_ERR_GOTO(res, out, "fpgaGetProperties");
		res = fpgaPropertiesGetBus(props, (uint8_t *) & bus);
		ON_ERR_GOTO(res, out_destroy_prop, "fpgaPropertiesGetBus");
		res = fpgaPropertiesGetDevice(props, (uint8_t *) & dev);
		ON_ERR_GOTO(res, out_destroy_prop, "fpgaPropertiesGetDevice");
		res = fpgaPropertiesGetFunction(props, (uint8_t *) & func);
		ON_ERR_GOTO(res, out_destroy_prop, "fpgaPropertiesGetFunction");

		// Find the device from the topology
		hwloc_topology_t topology;
		hwloc_topology_init(&topology);
#if HWLOC_API_VERSION >= 0x00020000
		hwloc_topology_set_io_types_filter(topology,
			HWLOC_TYPE_FILTER_KEEP_ALL);
#else
		hwloc_topology_set_flags(topology,
			HWLOC_TOPOLOGY_FLAG_IO_DEVICES);
#endif
		hwloc_topology_load(topology);
		hwloc_obj_t obj = hwloc_get_pcidev_by_busid(topology, dom, bus, dev, func);
		hwloc_obj_t obj2 = hwloc_get_non_io_ancestor_obj(topology, obj);
		#if (FPGA_DMA_DEBUG)
			hwloc_obj_type_snprintf(str, 4096, obj2, 1);
			printf("%s\n", str);
			hwloc_obj_attr_snprintf(str, 4096, obj2, " :: ", 1);
			printf("%s\n", str);
			hwloc_bitmap_taskset_snprintf(str, 4096, obj2->cpuset);
			printf("CPUSET is %s\n", str);
			hwloc_bitmap_taskset_snprintf(str, 4096, obj2->nodeset);
			printf("NODESET is %s\n", str);
		#endif
		if (memory_affinity) {
#if HWLOC_API_VERSION >= 0x00020000
			retval = hwloc_set_membind(
				topology,
				obj2->nodeset,
				HWLOC_MEMBIND_DEFAULT,
				0);
#else
			retval = hwloc_set_membind_nodeset(
				topology, obj2->nodeset,
				HWLOC_MEMBIND_BIND,
				HWLOC_MEMBIND_THREAD | HWLOC_MEMBIND_MIGRATE);
#endif
			ON_ERR_GOTO((fpga_result)retval, out_destroy_prop, "hwloc_set_membind");
		}
		if (cpu_affinity) {
			retval = hwloc_set_cpubind(topology, obj2->cpuset, HWLOC_CPUBIND_STRICT);
			ON_ERR_GOTO((fpga_result)retval, out_destroy_prop, "hwloc_set_cpubind");
		}

	}

out_destroy_prop:
	res = fpgaDestroyProperties(&props);
	#endif
out:
	return res;
}

int find_accelerator(const char *afu_id, struct config *config,
			    fpga_token *afu_tok) {
	UNUSED(afu_id);
	fpga_result res;
	fpga_guid guid;
	uint32_t num_matches = 0;
	fpga_properties filter = NULL;

	if(config->direction == DMA_MTOM) {
 		if(uuid_parse(MMDMA_AFU_ID, guid) < 0)
			return 1;
	} else {
		if(uuid_parse(STDMA_AFU_ID, guid) < 0)
			return 1;
	}

	res = fpgaGetProperties(NULL, &filter);
	ON_ERR_GOTO(res, out, "fpgaGetProperties");

	res = fpgaPropertiesSetObjectType(filter, FPGA_ACCELERATOR);
	ON_ERR_GOTO(res, out_destroy_prop, "fpgaPropertiesSetObjectType");

	res = fpgaPropertiesSetGUID(filter, guid);
	ON_ERR_GOTO(res, out_destroy_prop, "fpgaPropertiesSetGUID");

	if (CONFIG_UNINIT != config->bus) {
		res = fpgaPropertiesSetBus(filter, config->bus);
		ON_ERR_GOTO(res, out_destroy_prop, "setting bus");
	}

	if (CONFIG_UNINIT != config->device) {
		res = fpgaPropertiesSetDevice(filter, config->device);
		ON_ERR_GOTO(res, out_destroy_prop, "setting device");
	}

	if (CONFIG_UNINIT != config->function) {
		res = fpgaPropertiesSetFunction(filter, config->function);
		ON_ERR_GOTO(res, out_destroy_prop, "setting function");
	}

	res = fpgaEnumerate(&filter, 1, afu_tok, 1, &num_matches);
	ON_ERR_GOTO(res, out_destroy_prop, "fpgaEnumerate");
	
out_destroy_prop:
	res = fpgaDestroyProperties(&filter);
	ON_ERR_GOTO(res, out, "fpgaDestroyProperties");

out:
	if (num_matches > 0)
		return (int)num_matches;
	else
		return 0;
}

fpga_result do_action(struct config *config, fpga_token afc_tok)
{
	fpga_dma_handle_t dma_h = NULL;
	fpga_dma_handle_t tx_dma_h = NULL;
	fpga_dma_handle_t rx_dma_h = NULL;
	fpga_handle afc_h = NULL;
	fpga_result res;
	#ifndef USE_ASE
	volatile uint64_t *mmio_ptr = NULL;
	#endif

	sem_init(&transfer_done , 0, 0);
	res = fpgaOpen(afc_tok, &afc_h, 0);
	ON_ERR_GOTO(res, out, "fpgaOpen");
	debug_print("opened afc handle\n");

	#ifndef USE_ASE
	res = fpgaMapMMIO(afc_h, 0, (uint64_t**)&mmio_ptr);
	ON_ERR_GOTO(res, out_afc_close, "fpgaMapMMIO");
	debug_print("mapped mmio\n");
	#endif

	res = fpgaReset(afc_h);
	ON_ERR_GOTO(res, out_unmap, "fpgaReset");
	debug_print("applied afu reset\n");

	// Enumerate DMA handles
	uint64_t ch_count;
	ch_count = 0;
	res = fpgaCountDMAChannels(afc_h, &ch_count);
	ON_ERR_GOTO(res, out_unmap, "fpgaGetDMAChannels");
	if(ch_count < 1) {
		fprintf(stderr, "DMA channels not found (found %ld, expected %d\n",
			ch_count, 2);
		ON_ERR_GOTO(FPGA_INVALID_PARAM, out_unmap, "count<1");
	}

	debug_print("found %ld dma channels\n", ch_count);

	if(config->direction == DMA_MTOM) {
		res = fpgaDMAOpen(afc_h, 0, &dma_h);
		ON_ERR_GOTO(res, out_dma_close, "fpgaDMAOpen");
		debug_print("opened memory to memory channel\n");

		// Run test
		res = non_loopback_test(afc_h, dma_h, config);
		ON_ERR_GOTO(res, out_dma_close, "fpgaDMAOpen");
		debug_print("non loopback test success\n");
	} else {
		if(config->loopback == DMA_LOOPBACK_OFF) {
			if(config->direction == DMA_MTOS) {
				// Memory to stream -> Channel 0
				res = fpgaDMAOpen(afc_h, 0, &dma_h);
				ON_ERR_GOTO(res, out_dma_close, "fpgaDMAOpen");
				debug_print("opened memory to stream channel\n");
			} else {
				// Stream to memory -> Channel 1
				res = fpgaDMAOpen(afc_h, 1, &dma_h);
				ON_ERR_GOTO(res, out_dma_close, "fpgaDMAOpen");
				debug_print("opened stream to memory channel\n");
			}

			// Run test
			res = non_loopback_test(afc_h, dma_h, config);
			ON_ERR_GOTO(res, out_dma_close, "fpgaDMAOpen");
			debug_print("non loopback test success\n");
		} else {
			res = fpgaDMAOpen(afc_h, 0, &tx_dma_h);
			ON_ERR_GOTO(res, out_tx_close, "fpgaDMAOpen tx");

			res = fpgaDMAOpen(afc_h, 1, &rx_dma_h);
			ON_ERR_GOTO(res, out_rx_close, "fpgaDMAOpen rx");

			// Run test
			res = loopback_test(afc_h, tx_dma_h, rx_dma_h, config);
			ON_ERR_GOTO(res, out_rx_close, "loopback test failed");
			debug_print("loopback test success\n");
		}
	}

out_rx_close:
	if(rx_dma_h) {
		res = fpgaDMAClose(rx_dma_h);
		ON_ERR_GOTO(res, out_tx_close, "fpgaDMAClose");
		debug_print("closed rx channel\n");
	}

out_tx_close:
	if(tx_dma_h) {
		res = fpgaDMAClose(tx_dma_h);
		ON_ERR_GOTO(res, out_dma_close, "fpgaDMAClose");
		debug_print("closed tx channel\n");
	}

out_dma_close:
	if(dma_h) {
		res = fpgaDMAClose(dma_h);
		ON_ERR_GOTO(res, out_unmap, "fpgaDMAOpen");
		debug_print("closed dma channel\n");
	}

out_unmap:
	#ifndef USE_ASE
	if(afc_h) {
		res = fpgaUnmapMMIO(afc_h, 0);
		ON_ERR_GOTO(res, out_afc_close, "fpgaUnmapMMIO");
		debug_print("unmapped mmio\n");
	}
	#endif

out_afc_close:
	if (afc_h) {
		res = fpgaClose(afc_h);
		ON_ERR_GOTO(res, out, "fpgaClose");
		debug_print("closed afc\n");
	}

out:
	sem_destroy(&transfer_done);
	return res;
}
