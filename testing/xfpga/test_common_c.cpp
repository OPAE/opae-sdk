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
#ifdef __cplusplus
extern "C" {
#endif
#include <opae/utils.h>
const char * xfpga_fpgaErrStr(fpga_result);

#ifdef __cplusplus
}
#endif

#include "test_system.h"
#include "gtest/gtest.h"
#include "xfpga.h"

using namespace opae::testing;

/**
 * @test       common_01
 *
 * @brief      Verifies the string returned by fpgaErrStr() for each
 *             fpga_result enumeration value.
 */
TEST(LibopaecErrorCommonALL, common_01) {
  EXPECT_STREQ("success",                 xfpga_fpgaErrStr(FPGA_OK));
  EXPECT_STREQ("invalid parameter",       xfpga_fpgaErrStr(FPGA_INVALID_PARAM));
  EXPECT_STREQ("resource busy",           xfpga_fpgaErrStr(FPGA_BUSY));
  EXPECT_STREQ("exception",               xfpga_fpgaErrStr(FPGA_EXCEPTION));
  EXPECT_STREQ("not found",               xfpga_fpgaErrStr(FPGA_NOT_FOUND));
  EXPECT_STREQ("no memory",               xfpga_fpgaErrStr(FPGA_NO_MEMORY));
  EXPECT_STREQ("not supported",           xfpga_fpgaErrStr(FPGA_NOT_SUPPORTED));
  EXPECT_STREQ("no driver available",     xfpga_fpgaErrStr(FPGA_NO_DRIVER));
  EXPECT_STREQ("no fpga daemon running",  xfpga_fpgaErrStr(FPGA_NO_DAEMON));
  EXPECT_STREQ("insufficient privileges", xfpga_fpgaErrStr(FPGA_NO_ACCESS));
  EXPECT_STREQ("reconfiguration error",   xfpga_fpgaErrStr(FPGA_RECONF_ERROR));
}
