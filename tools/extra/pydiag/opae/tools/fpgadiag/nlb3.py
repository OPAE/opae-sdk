# Copyright(c) 2017, Intel Corporation
#
# Redistribution  and  use  in source  and  binary  forms,  with  or  without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of  source code  must retain the  above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name  of Intel Corporation  nor the names of its contributors
#   may be used to  endorse or promote  products derived  from this  software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
# IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
# LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
# CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
# SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
# INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
# CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
import nlb
from diagtest import diagtest
from opae import fpga
from opae.utils import cl_align, CACHELINE_BYTES

COOL_CACHE_LINES = 1024
COOL_CACHE_SIZE = CACHELINE_BYTES*COOL_CACHE_LINES


class nlb3(diagtest):
    guid = "F7DF405C-BD7A-CF72-22F1-44B0B93ACD18"

    def add_arguments(self, parser):
        super(nlb3, self).add_arguments(parser)
        parser.add_argument('-a', '--strided-access',
                            type=int, default=1,
                            help='where 1 <= value <= 64')
        parser.add_argument('-H', '--warm-fpga-cache',
                            action='store_true', default=False,
                            help='Attempt to prime the fpga cache with hits')
        parser.add_argument('-M', '--cool-fpga-cache',
                            action='store_true', default=False,
                            help='Attempt to prime the fpga cache with misses')
        parser.add_argument('-C', '--cool-cpu-cache',
                            action='store_true', default=False,
                            help='Attempt to prime the cpu cache with misses')

    def setup(self):
        result = super(nlb3, self).setup()
        if result:
            if self.args.warm_fpga_cache and self.args.cool_fpga_cache:
                self.logger.error(
                    "cannot do cool-fpga-cache and warm-fpga-cache together")
                return False
            if self.args.mode == self.MODE_TRPUT:
                if self.args.warm_fpga_cache or self.args.cool_fpga_cache:
                    self.logger.error(
                        "priming fpga cache is invalid for this mode")
                    return False
            if self.args.strided_access not in range(1, 65):
                self.logger.error("strided access not within valid range")
                return False
        return result

    def buffer_size(self):
        return self.args.end*self.args.strided_access*CACHELINE_BYTES

    def setup_buffers(self, handle, dsm, src, dst):
        if self.args.warm_fpga_cache:
            self.warm_fpga_cache(handle, dsm, src, dst)
        elif self.args.cool_fpga_cache:
            self.cool_fpga_cache(handle, dsm)
        else:
            src.fill(0xcafe)

    def warm_fpga_cache(self, handle, dsm, src, dst):
        dsm.fill(0)
        ctl = nlb.CTL(width=32)
        cfg = nlb.CFG(width=32)
        self.logger.info("warming fpga cache for mode: %s", self.args.mode)
        handle.write_csr64(self.DSM_ADDR, dsm.io_address())
        ctl.write(handle, reset=0)
        ctl.write(handle, reset=1)
        handle.write_csr64(self.SRC_ADDR, cl_align(src.io_address()))
        handle.write_csr64(self.DST_ADDR, cl_align(dst.io_address()))
        handle.write_csr32(self.NUM_LINES, src.size()/CACHELINE_BYTES)
        cfg_offset = cfg.offset()
        if self.args.mode == "read":
            cfg_value = cfg.value(mode=int(self.modes("read")),
                                  rd_chsel=int(self.rd_chsel("vl0")))
        else:
            cfg_value = cfg.value(mode=int(self.modes("write")),
                                  rd_chsel=int(self.wr_chsel("vl0")))
        handle.write_csr32(cfg_offset, cfg_value)
        handle.write_csr32(ctl.offset(), ctl.value(reset=1, start=1))
        if not self.wait_for_dsm(dsm):
            raise RuntimeError("DSM Complete timeout during warm fpga cache")
        handle.write_csr32(ctl.offset(), ctl.value(stop=1, reset=1, start=1))
        self.logger.info("fpga cache warmed")

    def cool_fpga_cache(self, handle, dsm):
        ice = fpga.allocate_shared_buffer(handle, COOL_CACHE_SIZE)
        dsm.fill(0)
        ctl = nlb.CTL()
        cfg = nlb.CFG()
        self.logger.info("cooling fpga cache for mode: %s", self.args.mode)
        handle.write_csr64(self.DSM_ADDR, dsm.io_address())
        ctl.write(handle, reset=0)
        ctl.write(handle, reset=1)
        handle.write_csr64(self.SRC_ADDR, cl_align(ice.io_address()))
        handle.write_csr32(self.NUM_LINES, COOL_CACHE_LINES)
        cfg_offset = cfg.offset()
        cfg_value = cfg.value(mode=int(self.modes["read"]),
                              rd_chsel=int(self.rd_chsel("vl0")),
                              cache_hint=int(self.cache_hint("rdline-I")))
        handle.write_csr32(cfg_offset, cfg_value)
        handle.write_csr32(ctl.offset(), ctl.value(reset=1, start=1))
        if not self.wait_for_dsm(dsm):
            raise RuntimeError("DSM Complete timeout during cool fpga cache")
        ctl.write(handle, stop=1, reset=1, start=1)
        self.logger.info("fpga cache cooled")
