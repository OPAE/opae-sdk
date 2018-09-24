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
#include <fstream>
#include "gtest/gtest.h"
#include "test_system.h"
#include "types_int.h"
#include "xfpga.h"

using namespace opae::testing;

const std::string DATA =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

class sysobject_p : public ::testing::TestWithParam<std::string> {
 protected:
  sysobject_p()
      : tmpsysfs_("mocksys-XXXXXX"), tokens_{nullptr}, handle_(nullptr) {}

  virtual void SetUp() override {
    ASSERT_TRUE(test_platform::exists(GetParam()));
    ASSERT_EQ(xfpga_fpgaGetProperties(nullptr, &dev_filter_), FPGA_OK);
    ASSERT_EQ(fpgaPropertiesSetObjectType(dev_filter_, FPGA_DEVICE), FPGA_OK);
    ASSERT_EQ(xfpga_fpgaGetProperties(nullptr, &acc_filter_), FPGA_OK);
    ASSERT_EQ(fpgaPropertiesSetObjectType(acc_filter_, FPGA_ACCELERATOR),
              FPGA_OK);
    platform_ = test_platform::get(GetParam());
    system_ = test_system::instance();
    system_->initialize();
    tmpsysfs_ = system_->prepare_syfs(platform_);
    invalid_device_ = test_device::unknown();
  }

  virtual void TearDown() override {
    for (auto t : tokens_) {
      if (t) {
        EXPECT_EQ(xfpga_fpgaDestroyToken(&t), FPGA_OK);
      }
    }
    if (handle_) {
      EXPECT_EQ(xfpga_fpgaClose(handle_), FPGA_OK);
      handle_ = nullptr;
    }
    system_->finalize();
  }

  std::string tmpsysfs_;
  test_platform platform_;
  test_device invalid_device_;
  test_system *system_;
  std::array<fpga_token, 2> tokens_;
  fpga_handle handle_;
  fpga_properties dev_filter_;
  fpga_properties acc_filter_;
};

TEST_P(sysobject_p, xfpga_fpgaTokenGetObject) {
  uint32_t num_matches = 0;
  ASSERT_EQ(xfpga_fpgaEnumerate(&dev_filter_, 1, tokens_.data(), tokens_.size(),
                                &num_matches),
            FPGA_OK);
  ASSERT_GT(num_matches, 0);
  const char *name = "bitstream_id";
  fpga_object object;
  int flags = 0;
  EXPECT_EQ(xfpga_fpgaTokenGetObject(tokens_[0], name, &object, flags),
            FPGA_OK);
  uint64_t bitstream_id = 0;
  EXPECT_EQ(xfpga_fpgaObjectRead64(object, &bitstream_id, FPGA_OBJECT_TEXT),
            FPGA_OK);
  EXPECT_EQ(bitstream_id, platform_.devices[0].bbs_id);
  EXPECT_EQ(xfpga_fpgaTokenGetObject(tokens_[0], "invalid_name", &object, 0),
            FPGA_NOT_FOUND);
  EXPECT_EQ(xfpga_fpgaDestroyObject(&object), FPGA_OK);
}

TEST_P(sysobject_p, xfpga_fpgaHandleGetObject) {
  uint32_t num_matches = 0;
  ASSERT_EQ(xfpga_fpgaEnumerate(&dev_filter_, 1, tokens_.data(), tokens_.size(),
                                &num_matches),
            FPGA_OK);
  ASSERT_GT(num_matches, 0);
  ASSERT_EQ(xfpga_fpgaOpen(tokens_[0], &handle_, 0), FPGA_OK);
  const char *name = "bitstream_id";
  fpga_object object;
  int flags = 0;
  ASSERT_EQ(xfpga_fpgaHandleGetObject(handle_, name, &object, flags), FPGA_OK);
  uint64_t bitstream_id = 0;
  EXPECT_EQ(xfpga_fpgaObjectRead64(object, &bitstream_id, FPGA_OBJECT_TEXT),
            FPGA_OK);
  EXPECT_EQ(bitstream_id, platform_.devices[0].bbs_id);
  EXPECT_EQ(xfpga_fpgaHandleGetObject(handle_, "invalid_name", &object, 0),
            FPGA_NOT_FOUND);
  EXPECT_EQ(xfpga_fpgaDestroyObject(&object), FPGA_OK);
}

TEST_P(sysobject_p, xfpga_fpgaObjectGetObject) {
  uint32_t num_matches = 0;
  ASSERT_EQ(xfpga_fpgaEnumerate(&dev_filter_, 1, tokens_.data(), tokens_.size(),
                                &num_matches),
            FPGA_OK);
  ASSERT_GT(num_matches, 0);
  fpga_object err_object, object;
  int flags = 0;
  const char *name = "errors";
  EXPECT_EQ(xfpga_fpgaTokenGetObject(tokens_[0], name, &err_object, flags),
            FPGA_OK);
  EXPECT_EQ(xfpga_fpgaObjectGetObject(err_object, nullptr, "bbs_errors",
                                      &object, flags),
            FPGA_OK);
  uint64_t bbs_errors = 0;
  EXPECT_EQ(xfpga_fpgaObjectRead64(object, &bbs_errors, FPGA_OBJECT_TEXT),
            FPGA_OK);
  EXPECT_EQ(xfpga_fpgaDestroyObject(&object), FPGA_OK);
  EXPECT_EQ(xfpga_fpgaDestroyObject(&err_object), FPGA_OK);
}

TEST_P(sysobject_p, xfpga_fpgaDestroyObject) {
  EXPECT_EQ(xfpga_fpgaDestroyObject(NULL), FPGA_INVALID_PARAM);
}

TEST_P(sysobject_p, xfpga_fpgaObjectRead) {
  uint32_t num_matches = 0;
  ASSERT_EQ(xfpga_fpgaEnumerate(&dev_filter_, 1, tokens_.data(), tokens_.size(),
                                &num_matches),
            FPGA_OK);
  ASSERT_GT(num_matches, 0);
  _fpga_token *tk = static_cast<_fpga_token *>(tokens_[0]);
  std::string syspath(tk->sysfspath);
  syspath += "/testdata";
  auto fp = system_->register_file(syspath);
  ASSERT_NE(fp, nullptr) << strerror(errno);
  fwrite(DATA.c_str(), DATA.size(), 1, fp);
  fflush(fp);
  fpga_object object;
  int flags = 0;
  EXPECT_EQ(xfpga_fpgaTokenGetObject(tokens_[0], "testdata", &object, flags),
            FPGA_OK);
  std::vector<uint8_t> buffer(DATA.size());
  EXPECT_EQ(xfpga_fpgaObjectRead(object, buffer.data(), 0, DATA.size() + 1, 0),
            FPGA_INVALID_PARAM);
  EXPECT_EQ(xfpga_fpgaObjectRead(object, buffer.data(), 0, 10, FPGA_OBJECT_SYNC), 
            FPGA_OK);
  buffer[10] = '\0';
  EXPECT_STREQ(reinterpret_cast<const char *>(buffer.data()),
               DATA.substr(0, 10).c_str());
  rewind(fp);
  std::string c0c0str = "0xc0c0cafe\n";
  uint64_t value = 0;
  fwrite(c0c0str.c_str(), c0c0str.size(), 1, fp);
  fflush(fp);
  EXPECT_EQ(xfpga_fpgaObjectRead64(object, &value,
                                   FPGA_OBJECT_TEXT | FPGA_OBJECT_SYNC),
            FPGA_OK);
  EXPECT_EQ(value, 0xc0c0cafe);
  EXPECT_EQ(xfpga_fpgaDestroyObject(&object), FPGA_OK);
}

TEST_P(sysobject_p, xfpga_fpgaObjectWrite64) {
  uint32_t num_matches = 0;
  ASSERT_EQ(xfpga_fpgaEnumerate(&dev_filter_, 1, tokens_.data(), tokens_.size(),
                                &num_matches),
            FPGA_OK);
  ASSERT_GT(num_matches, 0);
  ASSERT_EQ(xfpga_fpgaOpen(tokens_[0], &handle_, 0), FPGA_OK);
  _fpga_handle *h = static_cast<_fpga_handle *>(handle_);
  _fpga_token *tok = static_cast<_fpga_token *>(h->token);
  std::string syspath(tok->sysfspath);
  syspath += "/testdata";
  auto fp = system_->register_file(syspath);
  fpga_object object;
  ASSERT_EQ(xfpga_fpgaHandleGetObject(handle_, "testdata", &object, 0),
            FPGA_OK);
  EXPECT_EQ(xfpga_fpgaObjectWrite64(object, 0xc0c0cafe, 0), FPGA_OK);
  EXPECT_EQ(xfpga_fpgaObjectWrite64(object, 0xc0c0cafe, FPGA_OBJECT_TEXT),
            FPGA_OK);

  _fpga_object *obj = static_cast<_fpga_object *>(object);
  char *path = obj->path;
  std::string invalid_path = "test";

  char *inv_path = new char[invalid_path.length() + 1];
  strcpy(inv_path, invalid_path.c_str());

  obj->path = inv_path;

  EXPECT_EQ(xfpga_fpgaObjectWrite64(object, 0xc0c0cafe, 0), FPGA_EXCEPTION);

  obj->path = path;
  delete [] inv_path;

  rewind(fp);
  std::vector<char> buffer(256);
  fread(buffer.data(), buffer.size(), 1, fp);
  EXPECT_EQ(xfpga_fpgaDestroyObject(&object), FPGA_OK);
}

INSTANTIATE_TEST_CASE_P(sysobject_c, sysobject_p,
                        ::testing::ValuesIn(test_platform::keys(true)));
