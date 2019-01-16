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

#ifdef __cplusplus

extern "C" {
#endif
#include "token_list_int.h"

#ifdef __cplusplus
}
#endif
#include "gtest/gtest.h"
#include "sysfs_int.h"
#include "test_system.h"
#include "types_int.h"

extern pthread_mutex_t global_lock;

using namespace opae::testing;

class token_list_c_p : public ::testing::TestWithParam<std::string> {
 protected:
  token_list_c_p() {}
  virtual ~token_list_c_p() {}

  virtual void SetUp() override {
    ASSERT_TRUE(test_platform::exists(GetParam()));
    platform_ = test_platform::get(GetParam());
    system_ = test_system::instance();
    system_->initialize();
    system_->prepare_syfs(platform_);
    ASSERT_EQ(fpgaInitialize(nullptr), FPGA_OK);

    if (sysfs_region_count() > 0) {
      const sysfs_fpga_region* region = sysfs_get_region(0);
      ASSERT_NE(region, nullptr);
      if (region->fme) {
        sysfs_fme = std::string(region->fme->res_path);

        dev_fme = std::string("/dev/") + std::string(region->fme->res_name);
      }
      if (region->port) {
        sysfs_port = std::string(region->port->res_path);

        dev_port = std::string("/dev/") + std::string(region->port->res_name);
      }
    }
  }
  virtual void TearDown() override {
    fpgaFinalize();
    token_cleanup();
    system_->finalize();
  }

  test_platform platform_;
  test_system* system_;
  std::string sysfs_fme;
  std::string dev_fme;
  std::string sysfs_port;
  std::string dev_port;
};

TEST_P(token_list_c_p, simple_case) {
  auto fme = token_add(sysfs_fme.c_str(), dev_fme.c_str());
  ASSERT_NE(fme, nullptr);
  auto port = token_add(sysfs_port.c_str(), dev_port.c_str());
  ASSERT_NE(port, nullptr);

  auto parent = token_get_parent(port);
  EXPECT_EQ(parent, fme);

  parent = token_get_parent(fme);
  EXPECT_EQ(nullptr, parent);
}

TEST_P(token_list_c_p, invalid_mutex) {
  pthread_mutex_destroy(&global_lock);
  auto fme = token_add(sysfs_fme.c_str(), dev_fme.c_str());
  EXPECT_EQ(fme, nullptr);
  pthread_mutex_init(&global_lock, NULL);

  auto port = token_add(sysfs_port.c_str(), dev_port.c_str());
  ASSERT_NE(port, nullptr);

  pthread_mutex_destroy(&global_lock);
  auto parent = token_get_parent(port);
  EXPECT_EQ(nullptr, parent);
  pthread_mutex_init(&global_lock, NULL);

  pthread_mutex_destroy(&global_lock);
  token_cleanup();
  pthread_mutex_init(&global_lock, NULL);
  parent = token_get_parent(port);
  EXPECT_EQ(parent, fme);
}

TEST_P(token_list_c_p, invalid_paths) {
  // paths missing dot
  std::string sysfs_fme_invlaid =
      "/sys/class/fpga/intel-fpga-dev/intel-fpga-fme";
  std::string dev_fme_invlaid = "/dev/intel-fpga-fme";
  std::string sysfs_port_invlaid =
      "/sys/class/fpga/intel-fpga-dev/intel-fpga-port";
  std::string dev_port_invlaid = "/dev/intel-fpga-port";
  auto fme = token_add(sysfs_fme_invlaid.c_str(), dev_fme_invlaid.c_str());
  EXPECT_EQ(fme, nullptr);

  // paths with dot, but non-decimal character afterwards
  sysfs_fme_invlaid += ".z";
  sysfs_port_invlaid += ".z";
  fme = token_add(sysfs_fme_invlaid.c_str(), dev_fme_invlaid.c_str());
  EXPECT_EQ(fme, nullptr);

  // get a parent of a bogus token
  auto port = new _fpga_token;
  std::copy(sysfs_port_invlaid.begin(), sysfs_port_invlaid.end(),
            &port->sysfspath[0]);
  auto parent = token_get_parent(port);
  EXPECT_EQ(parent, nullptr);
  delete port;

  // invalidate malloc

  test_system::instance()->invalidate_malloc();
  fme = token_add(sysfs_fme.c_str(), dev_fme.c_str());
  ASSERT_EQ(fme, nullptr);
}

INSTANTIATE_TEST_CASE_P(token_list_c, token_list_c_p,
                        ::testing::ValuesIn(test_platform::keys(true)));
