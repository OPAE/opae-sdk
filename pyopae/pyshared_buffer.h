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
#pragma once
#include <Python.h>

#include <opae/cxx/core/shared_buffer.h>
#include <pybind11/pybind11.h>
#include "pyhandle.h"

const char *shared_buffer_doc();

const char *shared_buffer_doc_allocate();
opae::fpga::types::shared_buffer::ptr_t shared_buffer_allocate(
    opae::fpga::types::handle::ptr_t hndl, size_t size);
const char *shared_buffer_doc_size();

const char *shared_buffer_doc_wsid();

const char *shared_buffer_doc_iova();

const char *shared_buffer_doc_fill();

const char *shared_buffer_doc_compare();

const char *shared_buffer_doc_getitem();
uint8_t shared_buffer_getitem(opae::fpga::types::shared_buffer::ptr_t buf,
                              uint32_t offset);

const char *shared_buffer_doc_getslice();
pybind11::list shared_buffer_getslice(
    opae::fpga::types::shared_buffer::ptr_t buf, pybind11::slice slice);
