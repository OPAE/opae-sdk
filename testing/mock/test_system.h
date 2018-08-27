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
#ifndef _TEST_SYSTEM_H
#define _TEST_SYSTEM_H

#include <dirent.h>
#include <map>
#include <string>
#include <vector>

typedef struct stat stat_t;

namespace opae {
namespace testing {

constexpr size_t KiB(size_t n) { return n * 1024; }
constexpr size_t MiB(size_t n) { return n * 1024 * KiB(1); }

class mock_object {
 public:
  enum type_t { sysfs_attr = 0, fme, afu };
  mock_object(const std::string &devpath, const std::string &sysclass,
              uint32_t device_id, type_t type = sysfs_attr);

  virtual int ioctl(int request, va_list arg) {
    (void)request;
    (void)arg;
    throw std::logic_error("not implemented");
    return 0;
  }

  std::string sysclass() const { return sysclass_; }
  uint32_t device_id() const { return device_id_; }

 private:
  std::string devpath_;
  std::string sysclass_;
  uint32_t device_id_;
  type_t type_;
};

class mock_fme : public mock_object {
 public:
  mock_fme(const std::string &devpath, const std::string &sysclass,
           uint32_t device_id)
      : mock_object(devpath, sysclass, device_id, fme) {}
  virtual int ioctl(int request, va_list argp) override;
};

class mock_port : public mock_object {
 public:
  mock_port(const std::string &devpath, const std::string &sysclass,
            uint32_t device_id)
      : mock_object(devpath, sysclass, device_id, fme) {}
  virtual int ioctl(int request, va_list argp) override;
};

struct test_device {
  const char *fme_guid;
  const char *afu_guid;
  uint16_t segment;
  uint8_t bus;
  uint8_t device;
  uint8_t function;
  uint8_t socket_id;
  uint32_t num_slots;
  uint64_t fme_object_id;
  uint64_t port_object_id;
  uint16_t vendor_id;
  uint32_t device_id;
  uint32_t fme_num_errors;
  uint32_t port_num_errors;
  static test_device unknown();
};

struct test_platform {
  const char *mock_sysfs;
  std::vector<test_device> devices;
  static test_platform get(const std::string &key);
  static bool exists(const std::string &key);
  static std::vector<std::string> keys(bool sorted = false);
};

class test_system {
 public:
  static test_system *instance();

  void set_root(const char *root);
  std::string get_sysfs_path(const std::string &src);

  void initialize();
  void finalize();
  std::string prepare_syfs(const test_platform &platform);

  int open(const std::string &path, int flags);
  int open(const std::string &path, int flags, mode_t m);

  int close(int fd);
  int ioctl(int fd, unsigned long request, va_list argp);

  DIR *opendir(const char *name);
  ssize_t readlink(const char *path, char *buf, size_t bufsize);
  int xstat(int ver, const char *path, stat_t *buf);
  int lstat(int ver, const char *path, stat_t *buf);

 private:
  test_system();
  std::string root_;
  std::map<int, mock_object *> fds_;
  static test_system *instance_;

  typedef int (*open_func)(const char *pathname, int flags);
  typedef int (*open_create_func)(const char *pathname, int flags, mode_t mode);
  typedef int (*close_func)(int fd);
  typedef int (*ioctl_func)(int fd, unsigned long request, char *argp);
  typedef DIR *(*opendir_func)(const char *name);
  typedef ssize_t (*readlink_func)(const char *pathname, char *buf,
                                   size_t bufsiz);
  typedef int (*__xstat_func)(int ver, const char *pathname, struct stat *buf);

  open_func open_;
  open_create_func open_create_;
  close_func close_;
  ioctl_func ioctl_;
  opendir_func opendir_;
  readlink_func readlink_;
  __xstat_func xstat_;
  __xstat_func lstat_;
};

}  // end of namespace testing
}  // end of namespace opae

#endif /* !_TEST_SYSTEM_H */
