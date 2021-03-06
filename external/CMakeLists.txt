## Copyright(c) 2020-2021, Intel Corporation
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

cmake_minimum_required(VERSION  3.10)

if(NOT CLI11_FOUND)
    find_package(CLI11 1.8)
endif(NOT CLI11_FOUND)

if(NOT CLI11_FOUND)
    set(CLI11_URL
        https://github.com/CLIUtils/CLI11.git
        CACHE STRING "URL for CLI11 Project")
    set(CLI11_TAG
        v1.9.1
        CACHE STRING "Tag for CLI11")

    opae_external_project_add(PROJECT_NAME CLI11
                              GIT_URL ${CLI11_URL}
                              GIT_TAG ${CLI11_TAG}
                              PRESERVE_REPOS ${OPAE_PRESERVE_REPOS}
                              NO_ADD_SUBDIRECTORY
                              DEFER
    )
    set(CLI11_INCLUDE_DIRS ${CLI11_ROOT}/include
        CACHE STRING "Path to external CLI11" FORCE)
    set(CLI11_LIBRARIES ""
        CACHE STRING "Do not link against CLI11 library, using header-only" FORCE)
    set(CLI11_DEFINITIONS ""
        CACHE STRING "Do not link against CLI11 library, using header-only" FORCE)
    set(CLI11_FOUND TRUE CACHE BOOL "CLI11 found as external")
endif(NOT CLI11_FOUND)

if(NOT spdlog_FOUND)
    find_package(spdlog 1.8)
endif(NOT spdlog_FOUND)

if(NOT spdlog_FOUND)
    set(SPDLOG_URL
        https://github.com/gabime/spdlog.git
        CACHE STRING "URL for spdlog Project")
    set(SPDLOG_TAG
        v1.8.0
        CACHE STRING "Tag for spdlog")

    opae_external_project_add(PROJECT_NAME spdlog
                              GIT_URL ${SPDLOG_URL}
                              GIT_TAG ${SPDLOG_TAG}
                              PRESERVE_REPOS ${OPAE_PRESERVE_REPOS}
                              NO_ADD_SUBDIRECTORY
                              DEFER
    )
    set(spdlog_INCLUDE_DIRS ${spdlog_ROOT}/include
        CACHE STRING "Path to external spdlog" FORCE)
    set(spdlog_LIBRARIES ""
        CACHE STRING "Do not link against spdlog library, using header-only" FORCE)
    set(spdlog_DEFINITIONS ""
        CACHE STRING "Do not link against spdlog library, using header-only" FORCE)
    set(spdlog_FOUND TRUE CACHE BOOL "spdlog found as external")
else(NOT spdlog_FOUND)
    if(spdlog_DEFINITIONS)
        string(REGEX MATCH "-DSPDLOG_FMT_EXTERNAL" have_fmt ${spdlog_DEFINITIONS})
        if (have_fmt)
            find_package(fmt)
            set(fmt_LIBRARIES "fmt::fmt" CACHE STRING "fmt library link interface")
        endif (have_fmt)
    endif(spdlog_DEFINITIONS)
endif(NOT spdlog_FOUND)

if(OPAE_BUILD_LEGACY)
    opae_external_project_add(PROJECT_NAME opae-legacy
                              GIT_URL https://github.com/OPAE/opae-legacy.git
                              GIT_TAG ${OPAE_LEGACY_TAG}
                              PRESERVE_REPOS ${OPAE_PRESERVE_REPOS})

endif(OPAE_BUILD_LEGACY)
