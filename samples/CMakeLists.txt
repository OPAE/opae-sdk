## Copyright(c) 2014-2020, Intel Corporation
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
## POSSIBILITY OF SUCH DAMAGE.

#find_package(UUID REQUIRED)
#include(opae_devpkgs_cmake_install)
#include(ase_add_module)

include_directories(${OPAE_INCLUDE_DIR}
  ${OPAE_SDK_SOURCE}/libopae/src )
include_directories(base/sw)

if(BUILD_ASE_SAMPLES)
  find_package(Quartus)
  find_package(Questa)
endif()

######################################################################
# Add the samples ####################################################
######################################################################

set(OPAE_SAMPLES src/opae/samples)
if(BUILD_ASE_SAMPLES)
  # BBB libraries (MPF, DMA, etc)
  include(intel_fpga_bbb)
  Build_Intel_FPGA_BBB()

  opae_add_subdirectory(intg_xeon_nlb)
  opae_add_subdirectory(hello_afu)
  opae_add_subdirectory(hello_mpf_afu)
  if(BUILD_ASE_INTR)
    opae_add_subdirectory(hello_intr_afu)
  endif()
  # opae_add_subdirectory(hello_error_afu)
  # opae_add_subdirectory(hello_dma_afu)
endif()

######################################################################
# Client application #################################################
######################################################################
if(OPAE_BUILD_SIM)
  add_definitions(-DTEST_TIMEOUT=130000000)
endif(OPAE_BUILD_SIM)

opae_add_subdirectory(hello_fpga)
opae_add_subdirectory(hello_events)
opae_add_subdirectory(object_api)
opae_add_subdirectory(hssi)
opae_add_subdirectory(dummy_afu)
opae_add_subdirectory(fpgaperf_counter)
opae_add_subdirectory(host_exerciser)
opae_add_subdirectory(n5010-ddr-test)
opae_add_subdirectory(n5010-ctl)
