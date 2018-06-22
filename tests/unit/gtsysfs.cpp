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

#ifdef __cplusplus

extern "C" {
#endif
#include <opae/enum.h>
#include <opae/properties.h>
#include "sysfs_int.h"

#ifdef __cplusplus
}
#endif

#include "common_test.h"
#include "gtest/gtest.h"
#include "types_int.h"

#define DECLARE_GUID(var, ...) uint8_t var[16] = {__VA_ARGS__};

using namespace common_test;


/**
* @test    fpga_sysfs_01
* @brief   Tests: sysfs_deviceid_from_path
* @details sysfs_deviceid_from_path giver device id
*          Then the return device id <br>
*/
TEST(LibopaecsysfsCommonMOCKHW, fpga_sysfs_01) {

	uint64_t deviceid;
	
	//Valid path
	fpga_result result = sysfs_deviceid_from_path("/sys/class/fpga/intel-fpga-dev.0/intel-fpga-fme.0", &deviceid);
	ASSERT_EQ(result, FPGA_OK);

	//NULL input
	result = sysfs_deviceid_from_path("/sys/class/fpga/intel-fpga-dev.0/intel-fpga-fme.0", NULL);
	ASSERT_NE(result, FPGA_OK);

	//NULL input
	result = sysfs_deviceid_from_path(NULL, NULL);
	ASSERT_NE(result, FPGA_OK);

	//Invalid path
	result = sysfs_deviceid_from_path("/sys/class/fpga/intel-fpga-dev.0/intel-fpga.0", &deviceid);
	ASSERT_NE(result, FPGA_OK);

	result = sysfs_deviceid_from_path("/sys/class/fpga/intel-fpga-dev.20/intel-fpga-fme", &deviceid);
	ASSERT_NE(result, FPGA_OK);

	result = sysfs_deviceid_from_path("/sys/class/fpga/intel-fpga-dev.0/intel-fpga-fme.20", &deviceid);
	ASSERT_NE(result, FPGA_OK);

	result = sysfs_deviceid_from_path("/sys/class/fpga/intel-fpga-dev/intel-fpga-fme", &deviceid);
	ASSERT_NE(result, FPGA_OK);

}

/**
* @test    fpga_sysfs_02
* @brief   Tests: sysfs_read_int,sysfs_read_u32
*          sysfs_read_u32_pair,sysfs_read_u64
*          sysfs_read_u64,sysfs_write_u64
*..........get_port_sysfs,sysfs_read_guid
*..........get_fpga_deviceid
*/
TEST(LibopaecsysfsCommonMOCKHW, fpga_sysfs_02) {

	fpga_result result;

	//Invalid input path
	result = sysfs_read_int("", NULL);
	EXPECT_NE(result, FPGA_OK);

	//Invalid input parameters 
	result = sysfs_read_u32(NULL, NULL);
	EXPECT_NE(result, FPGA_OK);

	//Invalid input parameters
	result = sysfs_read_u32_pair(NULL, NULL, NULL, '\0');
	EXPECT_NE(result, FPGA_OK);

	//Invalid input parameters
	result = sysfs_read_u32_pair(NULL, NULL, NULL, 'a');
	EXPECT_NE(result, FPGA_OK);

	uint32_t u1;
	uint32_t u2;
	result = sysfs_read_u32_pair("/tmp/class/fpga/intel-fpga-dev.0/intel-fpga-fme.0/socket_id", &u1, &u2, '\0');
	EXPECT_NE(result, FPGA_OK);

	result = sysfs_read_u32_pair("/tmp/class/fpga/intel-fpga-dev.0/intel-fpga-fme.0", &u1, &u2, 'a');
	EXPECT_NE(result, FPGA_OK);

	//Invalid input parameters
	result = sysfs_read_u64(NULL, NULL);
	EXPECT_NE(result, FPGA_OK);

	uint64_t u64;
	result = sysfs_read_u64("/tmp/class/fpga/intel-fpga-dev.0/intel-fpga-fme.0", &u64);
	EXPECT_NE(result, FPGA_OK);

	result = sysfs_read_u64("/tmp/class/fpga/intel-fpga-dev.0/intel-fpga-fme.0/socket_id", &u64);
	EXPECT_EQ(result, FPGA_OK);

	//Invalid input parameters
	result = sysfs_write_u64(NULL, 0);
	EXPECT_NE(result, FPGA_OK);

	result = sysfs_write_u64("/tmp/class/fpga/intel-fpga-dev.0/intel-fpga-fme.0", 0x100);
	EXPECT_NE(result, FPGA_OK);

	result = sysfs_write_u64("/tmp/class/fpga/intel-fpga-dev.0/intel-fpga-fme.0/socket_id", 0x100);
	EXPECT_EQ(result, FPGA_OK);

	//Invalid input parameters
	fpga_guid guid;
	result = sysfs_read_guid(NULL, NULL);
	EXPECT_NE(result, FPGA_OK);

	result = sysfs_read_guid("/sys/class/fpga/intel-fpga-dev.0/intel-fpga.0/", guid);
	EXPECT_NE(result, FPGA_OK);

	//Invalid input parameters
	result = get_port_sysfs(NULL, NULL);
	EXPECT_NE(result, FPGA_OK);

	//Invalid handle
	result = get_port_sysfs((void*) 0x123, NULL);
	EXPECT_NE(result, FPGA_OK);

	//Invalid path parameters
	result = get_fpga_deviceid(NULL, NULL);
	EXPECT_NE(result, FPGA_OK);

	//Invalid sturct handle 
	result = get_fpga_deviceid((void*) 0x123, NULL);
	EXPECT_NE(result, FPGA_OK);

	struct _fpga_handle _handle;
	uint64_t deviceid;
	//Invalid sturct handle token
	result = get_fpga_deviceid((void*) &_handle, &deviceid);
	EXPECT_NE(result, FPGA_OK);
}