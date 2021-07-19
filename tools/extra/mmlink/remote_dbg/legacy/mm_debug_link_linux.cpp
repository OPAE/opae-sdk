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
//****************************************************************************
/// @file  mml_debug_link_linux.cpp
/// @brief Basic AFU interaction.
/// @ingroup SigTap
/// @verbatim
//****************************************************************************

#include <fcntl.h>

#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>
#include <sys/mman.h>
#include "mm_debug_link_linux.h"

#define DRIVER_PATH "/dev/mm_debug_link"
#define B2P_EOP 0x7B

#define BASE_ADDR 4096

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

#define MM_DEBUG_LINK_DATA_WRITE        0x100
#define MM_DEBUG_LINK_WRITE_CAPACITY    0x104
#define MM_DEBUG_LINK_DATA_READ         0x108
#define MM_DEBUG_LINK_READ_CAPACITY     0x10C
#define MM_DEBUG_LINK_FIFO_WRITE_COUNT  0x120
#define MM_DEBUG_LINK_FIFO_READ_COUNT   0x140
#define MM_DEBUG_LINK_ID_ROM            0x160
#define MM_DEBUG_LINK_SIGNATURE         0x170
#define MM_DEBUG_LINK_VERSION           0x174
#define MM_DEBUG_LINK_DEBUG_RESET       0x178
#define MM_DEBUG_LINK_MGMT_INTF         0x17C

#define REMSTP_MMIO_RD_LEN              0x180
#define REMSTP_MMIO_WR_LEN              0x184
#define REMSTP_RESET                    0x188
#define LEN_8B                          0x2
#define LEN_4B                          0x1
#define LEN_1B                          0x0

//#define DEBUG_8B_4B_TRANSFERS 1 // Uncomment for 4B/8B DBG
//#define DEBUG_FLAG 1 //Uncomment to enable read/write information

using namespace std;

/*
 * The value to expect at offset MM_DEBUG_LINK_SIGNATURE, aka "SysC".
 */
#define EXPECT_SIGNATURE 0x53797343

/*
 * The maximum version this driver supports.
 */
#define MAX_SUPPORTED_VERSION 1


mm_debug_link_interface *get_mm_debug_link(void)
{
	return new mm_debug_link_linux();
}


mm_debug_link_linux::mm_debug_link_linux() {
	m_fd = -1;
	m_buf_end = 0;
	m_write_fifo_capacity = 0;
	m_write_before_any_read_rfifo_level = false;
	m_last_read_rfifo_level_empty_time = 0;
	m_read_rfifo_level_empty_interval = 1;
	map_base = NULL;
}

int mm_debug_link_linux::open(unsigned char* stpAddr)
{
	unsigned int sign, version;
	m_fd = -1;
	map_base = stpAddr;

	cout << "Remote STP : Assert Reset"    << endl << flush;
	write_mmr(REMSTP_RESET, 'w', 0x1);
	cout << "Remote STP : De-Assert Reset" << endl << flush;
	write_mmr(REMSTP_RESET, 'w', 0x0);

	sign = read_mmr<unsigned int>(MM_DEBUG_LINK_SIGNATURE);
	cout << "Read signature value " << std::hex << sign << " to hw\n" << flush;
	if ( sign != EXPECT_SIGNATURE)
	{
		cerr << "Unverified Signature\n";
		return -1;
	}

	version = read_mmr<unsigned int>(MM_DEBUG_LINK_VERSION);
	cout << "Read version value " << version << " to hw\n";
	if ( version > MAX_SUPPORTED_VERSION )
	{
		cerr << "Unsupported Version\n";
		return -1;
	}

	this->m_write_fifo_capacity = read_mmr<int>(MM_DEBUG_LINK_WRITE_CAPACITY);
	cout << "Read write fifo capacity value " << std::dec << this->m_write_fifo_capacity << " to hw\n";

	return 0;
}

void mm_debug_link_linux::write_mmr(off_t target,
				int access_type,
				uint64_t write_val)
{
#ifdef DEBUG_8B_4B_TRANSFERS
	cout << hex <<"WRITING : "<< write_val << dec << endl;
#endif
	void *virt_addr;
	/* Map one page */

	virt_addr = (void *) (map_base + target);

	switch(access_type) {
	case 'b':
                *((uint8_t *) virt_addr) = write_val;
                break;
	case 'h':
                *((uint16_t *) virt_addr) = write_val;
                break;
	case 'w':
                *((uint32_t *) virt_addr) = write_val;
                break;
	case 'q':
                *((uint64_t *) virt_addr) = write_val;
                break;
	default:
                cerr << "Illegal data type '" << access_type << "'.\n";
		break;
        }
}

bool mm_debug_link_linux::can_read_data()
{
	bool ret  = this->m_write_before_any_read_rfifo_level;

	if ( !ret )
	{
		clock_t cur_time = ::clock();
		clock_t duration = cur_time - this->m_last_read_rfifo_level_empty_time;
		if ( duration < 0 )
		{
			duration = -duration;
		}
		if ( duration >= this->m_read_rfifo_level_empty_interval )
		{
			ret = true;
		}
	}

	return ret;
}

ssize_t mm_debug_link_linux::read()
{
	uint8_t num_bytes;

	num_bytes = read_mmr<uint8_t>(MM_DEBUG_LINK_FIFO_READ_COUNT);

	// Reset the timer record
	if ( (this->m_write_before_any_read_rfifo_level ||  // when this is the first read after write
	      num_bytes > 0) )                               // when something is available to read
	{
		this->m_write_before_any_read_rfifo_level = false;
		this->m_read_rfifo_level_empty_interval = 1;     // Increase the read fifo level polling freq. in anticipation of more read data availability.
	}

	if (num_bytes > 0 )
	{
		if ( num_bytes > (mm_debug_link_linux::BUFSIZE - m_buf_end) )
		{
			num_bytes = mm_debug_link_linux::BUFSIZE - m_buf_end;
		}

#ifdef DEBUG_FLAG
		cout << "Read " << num_bytes << " bytes\n";
#endif

/*
  ==========================================================================================================================
  At this point, num_bytes has the No. of bytes available to read from the FPGA
  Default implementation: Tries to pull 1B at a time.

  The Objective is to increase link utilization (1/8) to (8/8):
  -------------------------------------------------------------
  The interface on HW to SLD HUB Controller system still supports 1B reads/ writes only
  The solution is to avoid 1B ping-pong and communicate to remote STP soft logic on HW with No. of bytes to read (say N)
  The HW should read so many bytes (N) from the SLD HUB Cont Sys and return a packed read response. (little endian 64b max payload)
  A register (REMSTP_MMIO_RD_LEN) is defined @ PORT DFH offset 0x4180 - Default value is 0. Possible values are 0/1/2.
  Only SW can modify this register. SW should always retain the last value written to this register
  SW always does 8B/4B/1B MMIO reads to MM_DEBUG_LINK_DATA_READ register (depending on the current value of N)
  RemoteSTP logic on HW translates this to REMSTP_MMIO_RD_LEN (N) number of 1B reads from SLD HUB Cont sys endpoint
  HW returns a 1B/4B/8B read value. Only lower REMSTP_MMIO_RD_LEN bytes are valid
  SW should drop the remaining upper bytes of the returned response and update the mem-mapped pointer.
  SW is responsible for credit control on the HW read FIFO
  i.e. SW should update REMSTP_MMIO_RD_LEN register based on num_bytes available to read
  Leaving back entries in the FIFO/ popping an empty FIFO is FATAL

  Similarly, on the Write Path number of bytes to be written to HW write FIFO is packed into 4B/8B writes whenever possible
  A register (REMSTP_MMIO_WR_LEN) is defined @ PORT DFH offset 0x4184 - Default value is 0. Possible values are 0/1/2. (M say)
  RemoteSTP logic on HW will replay 1B writes M times into SLD HUB controller system w/o SLD endpoint.

  Encodings for REMSTP_MMIO_WR_LEN & REMSTP_MMIO_RD_LEN
  -----------------------------------------------------

  -------------------------
  | Encoding  | Rd/Wr Len |
  -------------------------
  | 2'b00     | 1         |
  | 2'b01     | 4         |
  | 2'b10     | 8         |
  | 2'b11     | Rsvd      |
  -------------------------

  Performance impact:
  -------------------
  1)
  OLD - Time to read 8bytes              = 8 * 1 MMIO Read latency from device
  NEW - Time to read 8bytes (best case)  = 1 MMIO Read latency from device
  NEW - Time to read 8bytes (worst case) = 1 MMIO Write latency to device + 1 MMIO Read latency from device

  2) Through efficient utilization of MMIO data width, the amount of degradation seen on available AFU BW when using remoteSTP could be lowered

  NOTE:
  -----
  MMIO reads to REMSTP_MMIO_RD_LEN or REMSTP_MMIO_WR_LEN is NOT supported
*/

		uint8_t  num_8B_reads, num_4B_reads, num_1B_reads, remaining_bytes;
		num_8B_reads    = num_bytes/8;
		remaining_bytes = num_bytes%8;
		num_4B_reads    = remaining_bytes/4;
		remaining_bytes = remaining_bytes%4;
		num_1B_reads    = remaining_bytes;

#ifdef DEBUG_8B_4B_TRANSFERS
		cout << dec;
		cout << "DBG_READ : Total_Bytes = " << (unsigned) num_bytes << " ; 8_bytes = "
		     << (unsigned) num_8B_reads << " ; 4_bytes = " << (unsigned) num_4B_reads << " ; 1_bytes = " << (unsigned) num_1B_reads << endl << flush;
#endif

		// SW should update HW control (REMSTP_MMIO_RD_LEN) and use only REMSTP_MMIO_RD_LEN bytes returned
		if (num_8B_reads > 0)
		{
			// Change REMSTP_MMIO_RD_LEN to 8B
			write_mmr( REMSTP_MMIO_RD_LEN, 'w', LEN_8B);
			for ( unsigned char  i = 0; i < num_8B_reads; ++i )
			{
				volatile uint64_t *p = reinterpret_cast<volatile uint64_t *>(this->m_buf +
                                                                                             this->m_buf_end +
                                                                                             (i*8));
				*p = read_mmr<uint64_t>(MM_DEBUG_LINK_DATA_READ);
#ifdef DEBUG_8B_4B_TRANSFERS
				cout << "DBG_READ_8B : Iteration "<< (unsigned) i << "; READ VALUE : " << hex;
				for (unsigned char j=0; j<8; j++)
					cout << (int) *( this->m_buf + this->m_buf_end + (i*8) + j ) << flush;
				cout << dec << endl << flush;
#endif
			}
		}

		if (num_4B_reads > 0)
		{
			// Change REMSTP_MMIO_RD_LEN to 4B
			write_mmr( REMSTP_MMIO_RD_LEN, 'w', LEN_4B);
			for ( unsigned char  i = 0; i < num_4B_reads; ++i )
			{
				volatile uint32_t *p = reinterpret_cast<volatile uint32_t *>(this->m_buf +
                                                                                             this->m_buf_end +
                                                                                             (num_8B_reads*8) +
                                                                                             (i*4));
				*p = read_mmr<uint32_t>(MM_DEBUG_LINK_DATA_READ);
#ifdef DEBUG_8B_4B_TRANSFERS
				cout << "DBG_READ_4B : Iteration "<< (int) i << "; READ VALUE : " << hex;
				for (unsigned char j=0; j<4; j++)
					cout << (int) *( this->m_buf + this->m_buf_end + ( (num_8B_reads*8) + (i*4) + j)) << flush;
				cout << dec << endl << flush;
#endif
			}
		}

		if (num_1B_reads > 0)
		{
			// Change REMSTP_MMIO_RD_LEN to 1B
			write_mmr( REMSTP_MMIO_RD_LEN, 'w', LEN_1B);
			for ( unsigned char i = 0; i < num_1B_reads; ++i )
			{
				volatile uint8_t *p = reinterpret_cast<volatile uint8_t *>(this->m_buf + this->m_buf_end +
                                                                                           (num_8B_reads*8) +
                                                                                           (num_4B_reads*4) +
                                                                                           i);
				*p = read_mmr<uint8_t>(MM_DEBUG_LINK_DATA_READ);
#ifdef DEBUG_8B_4B_TRANSFERS
				cout << "DBG_READ_1B : Iteration "<< (int) i << "; READ VALUE : "
				     << hex << (int) *( this->m_buf + this->m_buf_end + ( (num_8B_reads*8) + (num_4B_reads*4) +i)) << endl << flush << dec;
#endif
			}
		}
		// ==========================================================================================================================

		unsigned int x;
		for ( unsigned char i = 0; i < num_bytes; ++i )
		{
			x = this->m_buf[this->m_buf_end + i];

#ifdef DEBUG_FLAG
			cout << setfill('0') << setw(2) << std::hex << x << " ";
#else
			UNUSED_PARAM(x);
#endif
		}
#ifdef DEBUG_FLAG
		cout << std::dec << "\n";
#endif

		this->m_buf_end += num_bytes;
	}
	else
	{
		//printf( "%s %s(): error read hw read buffer level\n", __FILE__, __FUNCTION__ );
		num_bytes = 0;

		this->m_last_read_rfifo_level_empty_time = ::clock();

		//Throttle the read rfifo level polling freq.  up to 10 sec.
		this->m_read_rfifo_level_empty_interval *= 2;
		if ( this->m_read_rfifo_level_empty_interval >= 10 * CLOCKS_PER_SEC )
		{
			this->m_read_rfifo_level_empty_interval = 10 * CLOCKS_PER_SEC;
		}
	}

	return num_bytes;
}

ssize_t mm_debug_link_linux::write(const void *buf, size_t count)
{
	uint8_t num_bytes;
	unsigned int x;

	num_bytes = read_mmr<uint8_t>(MM_DEBUG_LINK_FIFO_WRITE_COUNT);

	this->m_write_before_any_read_rfifo_level = true;     // Set this to kick off any possible read activity even if write FIFO is full to avoid potential deadlock.

	if ( num_bytes < this->m_write_fifo_capacity )
	{
		num_bytes = this->m_write_fifo_capacity - num_bytes;
		if ( count < num_bytes )
		{
			num_bytes = count;
		}

		count = 0;

		// ==========================================================================================================================
		uint8_t      num_8B_writes, num_4B_writes, num_1B_writes, remaining_bytes;
		num_8B_writes   = num_bytes/8;
		remaining_bytes = num_bytes%8;
		num_4B_writes   = remaining_bytes/4;
		remaining_bytes = remaining_bytes%4;
		num_1B_writes   = remaining_bytes;

#ifdef DEBUG_8B_4B_TRANSFERS
		cout << dec << endl;
		cout << "DBG_WRITE : Total_Bytes = " << (unsigned) num_bytes << " ; 8_bytes = " << (unsigned) num_8B_writes
		     << " ; 4_bytes = " << (unsigned) num_4B_writes << " ; 1_bytes = " << (unsigned) num_1B_writes << endl << flush;
#endif

		// SW should update HW control (REMSTP_MMIO_WR_LEN) and use only REMSTP_MMIO_WR_LEN bytes returned
		if (num_8B_writes > 0)
		{
			// Change REMSTP_MMIO_WR_LEN to 8B
			write_mmr( REMSTP_MMIO_WR_LEN, 'w', LEN_8B);
			for ( size_t i = 0; i < num_8B_writes; ++i )
			{
#ifdef DEBUG_8B_4B_TRANSFERS
				cout << "DBG_WRITE_8B : Iteration "<< i << "; ";
#endif
				write_mmr( MM_DEBUG_LINK_DATA_WRITE, 'q', *( (uint64_t *)buf + i ) );
				count+=8;
			}
		}

		if (num_4B_writes > 0)
		{
			// Change REMSTP_MMIO_WR_LEN to 4B
			write_mmr( REMSTP_MMIO_WR_LEN, 'w', LEN_4B);
			for ( size_t i = 0; i < num_4B_writes; ++i )
			{
#ifdef DEBUG_8B_4B_TRANSFERS
				cout << "DBG_WRITE_4B : Iteration "<< i << "; ";
#endif
				write_mmr( MM_DEBUG_LINK_DATA_WRITE, 'w', *( (uint32_t *)buf + ( (num_8B_writes*2) + i ) ) );
				count+=4;
			}
		}

		if (num_1B_writes > 0)
		{
			// Change REMSTP_MMIO_WR_LEN to 1B
			write_mmr( REMSTP_MMIO_WR_LEN, 'w', LEN_1B);
			for ( size_t i = 0; i < num_1B_writes; ++i )
			{
#ifdef DEBUG_8B_4B_TRANSFERS
				cout << "DBG_WRITE_1B : Iteration "<< i << "; ";
#endif
				write_mmr( MM_DEBUG_LINK_DATA_WRITE, 'b', *( (unsigned char *)buf + ( (num_8B_writes*8) + (num_4B_writes*4) + (i) ) ) );
				++count;
			}
		}
		// ==========================================================================================================================

		num_bytes = count;
#ifdef DEBUG_FLAG
		cout << "Wrote " << num_bytes << " bytes\n";
#endif
		for ( int i = 0; i < num_bytes; ++i )
		{
			x = *((unsigned char *)buf + i);
#ifdef DEBUG_FLAG
			cout << setfill('0') << setw(2) << std::hex << x << " ";
#else
			UNUSED_PARAM(x);
#endif
		}
#ifdef DEBUG_FLAG
		cout << std::dec << "\n" ;
#endif
	}
	else
	{
		//cerr << "Error write hw write buffer level\n";
		num_bytes = 0;
	}

	return num_bytes;
}

void mm_debug_link_linux::close(void)
{

	if(munmap((void *) map_base, MAP_SIZE) == -1){
		cerr << "Unmap error\n";
	}

	if (m_fd != -1){
		::close(m_fd);
	}
	m_fd = -1;
}

void mm_debug_link_linux::write_ident(int val)
{
	write_mmr(MM_DEBUG_LINK_ID_ROM, 'b', val);
	cout << "Write mixer value " << val << " to hw\n";
}

void mm_debug_link_linux::reset(bool val)
{
	unsigned int reset_val = val ? 1 : 0;
	write_mmr(MM_DEBUG_LINK_DEBUG_RESET, 'w', val);
	cout << "Write reset value " << reset_val << " to hw\n";
}

void mm_debug_link_linux::ident(int id[4])
{
	for ( int i = 0; i < 4; i++ )
	{
		id[i] = read_mmr<int>(MM_DEBUG_LINK_ID_ROM + i * 4);
	}
}

void mm_debug_link_linux::enable(int channel, bool state)
{
	int encoded_cmd = (channel << 8) | (state ? 1 : 0);
	write_mmr(MM_DEBUG_LINK_MGMT_INTF, 'w', encoded_cmd);
	cout << "Enable channel " << encoded_cmd << " to hw\n";

}

bool mm_debug_link_linux::flush_request(void)
{
	bool should_flush = false;
	if (m_buf_end == BUFSIZE)
		// Full buffer? Send.
		should_flush = true;
	else if (memchr(m_buf, B2P_EOP, m_buf_end - 1))
		// Buffer contains eop? Send.
		// If the eop character occurs in the very last buffer byte, there's no packet here -
		// we need at least one more byte.
		// Interesting corner case: it's not strictly true that one more byte after EOP indicates
		// the end of a packet - that byte after EOP might be the escape character. In this case,
		// we flush even though it's not necessarily a complete packet. This probably has negligible
		// impact on performance.
		should_flush = true;

	if ( m_buf_end > 0 )
	{
		should_flush = true;
	}
	return should_flush;
}
