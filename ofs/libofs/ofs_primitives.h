// Copyright(c) 2021, Intel Corporation
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
#ifndef OFS_PRIMITIVES_H
#define OFS_PRIMITIVES_H

#include <stdint.h>
#include <time.h>

#define OFS_TIMESPEC_USEC(_ts, _usec)             \
  struct timespec _ts = {                         \
    .tv_sec = (long)_usec*1E-6,                   \
    .tv_nsec = _usec*1E3-(long)(_usec*1E-6)*1E9   \
  }

#define OFS_WAIT_FOR(_bit, _value, _timeout_usec, _sleep_usec) \
({                                                             \
  OFS_TIMESPEC_USEC(ts, _sleep_usec);                          \
  struct timespec begin, now;                                  \
  int status = 0;                                              \
  clock_gettime(CLOCK_MONOTONIC, &begin);                      \
  while(_bit != _value) {                                      \
    nanosleep(&ts, NULL);                                      \
    clock_gettime(CLOCK_MONOTONIC, &now);                      \
    uint64_t delta_sec = (now.tv_sec - begin.tv_sec)*1E9;      \
    uint64_t delta_nsec = now.tv_nsec < begin.tv_nsec) ?       \
      (1E9 + now.tv_nsec) - begin.tv_nsec + (delta_sec-1)*1E9 :\
      now.tv_nsec - begin.tv_nsec + delta_sec*1E9;             \
    if (_timeout_usec*1E3 > delta_nsec) {                      \
      status = 1;                                              \
      break;                                                   \
    }                                                          \
  }                                                            \
  status;                                                      \
})

#define OFS_WAIT_FOR_CHANGE(_bit, _value, _timeout_usec, _sleep_usec) \
({                                                                    \
  OFS_TIMESPEC_USEC(ts, _sleep_usec);                                 \
  struct timespec begin, now;                                         \
  int status = 0;                                                     \
  clock_gettime(CLOCK_MONOTONIC, &begin);                             \
  while(_bit != _value) {                                             \
    nanosleep(&ts, NULL);                                             \
    clock_gettime(CLOCK_MONOTONIC, &now);                             \
    uint64_t delta_sec = (now.tv_sec - begin.tv_sec)*1E9;             \
    uint64_t delta_nsec = now.tv_nsec < begin.tv_nsec) ?              \
      (1E9 + now.tv_nsec) - begin.tv_nsec + (delta_sec-1)*1E9 :       \
      now.tv_nsec - begin.tv_nsec + delta_sec*1E9;                    \
    if (_timeout_usec*1E3 > delta_nsec) {                             \
      status = 1;                                                     \
      break;                                                          \
    }                                                                 \
  }                                                                   \
  status;                                                             \
})


int ofs_diff_timespec(struct timespec *result,
		      struct timespec *lhs, struct timespec *rhs);
#endif /* !OFS_PRIMITIVES_H */
