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

extern "C" {

#include <json-c/json.h>
#include <uuid/uuid.h>

#include "fpgad/log.h"
#include "fpgad/srv.h"
#include "fpgad/errtable.h"
#include "fpgad/ap6.h"

struct client_event_registry *register_event(int conn_socket, int fd,
	                                     fpga_event_type e, const char *device);

void unregister_all_events(void);

void evt_notify_error(const struct fpga_err *e);

void evt_notify_ap6_and_null(const struct fpga_err *e);

}

#include <config.h>
#include <opae/fpga.h>

#include <array>
#include <cstdlib>
#include <unistd.h>
#include <sys/eventfd.h>
#include <poll.h>
#include "gtest/gtest.h"
#include "test_system.h"

using namespace opae::testing;

class fpgad_evt_c_p : public ::testing::TestWithParam<std::string> {
 protected:
  fpgad_evt_c_p()
      : tmpfpgad_log_("tmpfpgad-XXXXXX.log") {}

  virtual void SetUp() override {
    tmpfpgad_log_ = mkstemp(const_cast<char *>(tmpfpgad_log_.c_str()));
    std::string platform_key = GetParam();
    ASSERT_TRUE(test_platform::exists(platform_key));
    platform_ = test_platform::get(platform_key);
    system_ = test_system::instance();
    system_->initialize();
    system_->prepare_syfs(platform_);

    open_log(tmpfpgad_log_.c_str());
    int i;
    for (i = 0 ; i < MAX_SOCKETS ; ++i)
      sem_init(&ap6_sem[i], 0, 0);
  }

  bool do_poll(int fd)
  {
    struct pollfd pollfd;

    pollfd.fd = fd;
    pollfd.events = POLL_IN;

    int res = poll(&pollfd, 1, 1000);

    return (res == 1) && ((pollfd.revents & POLLIN) != 0);
  }

  virtual void TearDown() override {
    unregister_all_events();

    int i;
    for (i = 0 ; i < MAX_SOCKETS ; ++i)
      sem_destroy(&ap6_sem[i]);

    close_log();
    system_->finalize();
  }

  std::string tmpfpgad_log_;
  test_platform platform_;
  test_system *system_;
};

/**
 * @test       notify_err
 * @brief      Test: evt_notify_error
 * @details    evt_notify_error signals the event fd for each<br>
 *             FPGA_EVENT_ERROR registration in event_registry_list.<br>
 */
TEST_P(fpgad_evt_c_p, notify_err) {
  int conn_sockets[3] = { 0, 1, 2 };
  int evt_fds[3]      = { -1, -1, eventfd(0, 0) };
  const char *devs[3] = { "deva", "devb", "devc" }; 

  EXPECT_NE(nullptr, register_event(conn_sockets[2], evt_fds[2], FPGA_EVENT_ERROR, devs[2]));
  EXPECT_NE(nullptr, register_event(conn_sockets[1], evt_fds[1], FPGA_EVENT_ERROR, devs[1]));
  EXPECT_NE(nullptr, register_event(conn_sockets[0], evt_fds[0], FPGA_EVENT_ERROR, devs[0]));

  struct fpga_err err = {
    .socket = -1,
    .sysfsfile = "devc",
    .reg_field = "reg_field",
    .lowbit = 0,
    .highbit = 1,
    .occurred = true,
    .callback = nullptr
  };

  evt_notify_error(&err);

  ASSERT_TRUE(do_poll(evt_fds[2]));

  close(evt_fds[2]);
}

/**
 * @test       notify_ap6
 * @brief      Test: evt_notify_ap6_and_null
 * @details    evt_notify_ap6_and_null signals the event fd for each<br>
 *             FPGA_EVENT_POWER_THERMAL registration in event_registry_list.<br>
 */
TEST_P(fpgad_evt_c_p, notify_ap6) {
  int conn_sockets[3] = { 0, 1, 2 };
  int evt_fds[3]      = { -1, eventfd(0, 0), -1 };
  const char *devs[3] = { "deva", "devb", "devc" }; 

  EXPECT_NE(nullptr, register_event(conn_sockets[2], evt_fds[2], FPGA_EVENT_POWER_THERMAL, devs[2]));
  EXPECT_NE(nullptr, register_event(conn_sockets[1], evt_fds[1], FPGA_EVENT_POWER_THERMAL, devs[1]));
  EXPECT_NE(nullptr, register_event(conn_sockets[0], evt_fds[0], FPGA_EVENT_POWER_THERMAL, devs[0]));

  struct fpga_err err = {
    .socket = 0,
    .sysfsfile = "devb",
    .reg_field = "reg_field",
    .lowbit = 0,
    .highbit = 1,
    .occurred = true,
    .callback = nullptr
  };

  evt_notify_ap6_and_null(&err);

  ASSERT_TRUE(do_poll(evt_fds[1]));

  close(evt_fds[1]);
}

INSTANTIATE_TEST_CASE_P(fpgad_evt_c, fpgad_evt_c_p,
                        ::testing::ValuesIn(test_platform::keys()));
