#!/usr/bin/cmake -P
## Copyright(c) 2021, Intel Corporation
##
## Redistribution  and  use  in source  and  binary  forms,  with  or  without
## modification, are permitted provided that the following conditions are met:
##
## * Redistributions of  source code  must retain the  above copyright notice,
##   this list of conditions and the following disclaimer.
## * Redistributions in binary form must reproduce the above copyright notice,
##   this list of conditions and the following disclaimer in the documentation
##   and/or other materials provided with the distribution.
## * Neither the name  of Intel Corporation  nor the names of its contributors
##   may be used to  endorse or promote  products derived  from this  software
##   without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
## IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
## LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
## CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
## SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
## INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
## CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE

find_package(PkgConfig)
pkg_search_module(CAP libcap)

find_path(LIBCAP_INCLUDE_DIRS
  NAMES sys/capability.h
  HINTS ${PC_LIBCAP_INCLUDEDIR}
        ${PC_LIBCAP_INCLUDE_DIRS}
        /usr/local/include
        /usr/include
        ${CMAKE_EXTRA_INCLUDES})

find_library(LIBCAP_LIBRARIES
  NAMES cap
  HINTS ${PC_LIBCAP_LIBDIR}
        ${PC_LIBCAP_LIBRARY_DIRS}
        /usr/local/lib
        /usr/lib
        /lib
        /usr/lib/x86_64-linux-gnu
        ${CMAKE_EXTRA_LIBS})

if(LIBCAP_LIBRARIES AND LIBCAP_INCLUDE_DIRS)
  set(LIBCAP_FOUND true)
endif(LIBCAP_LIBRARIES AND LIBCAP_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libcap DEFAULT_MSG
  LIBCAP_INCLUDE_DIRS LIBCAP_LIBRARIES)
