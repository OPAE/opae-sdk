# Copyright(c) 2017, Intel Corporation
#
# Redistribution  and  use  in source  and  binary  forms,  with  or  without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of  source code  must retain the  above copyright notice,
#  this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#  this list of conditions and the following disclaimer in the documentation
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

import os
import glob

ROOT_PATH = '/sys/class/fpga'
FPGA_DEVICE = os.path.join(ROOT_PATH, 'intel-fpga-dev.{socket_id}')
FME_DEVICE = os.path.join(FPGA_DEVICE, 'intel-fpga-fme.{socket_id}')
PORT_DEVICE = os.path.join(FPGA_DEVICE, 'intel-fpga-port.{socket_id}')


class bitmask(object):
    def __init__(self, bit_lo, bit_hi=None):
        value = 0
        for n in range(bit_lo, (bit_hi or bit_lo) + 1):
            value |= (1 << n)
        self.mask = value
        self.type = int if bit_hi else bool
        self.bit_lo = bit_lo

    def __call__(self, value):
        return self.type(self.mask & value) >> self.bit_lo


class fpga_device(object):

    def __init__(self, devpath):
        self.devpath = devpath

    @classmethod
    def enumerate(cls):
        return [fpga_device(p) for p in glob.glob(
            FPGA_DEVICE.format(socket_id='*'))]

    def children(self):
        # TODO: return FME objects
        pass


class fpga_command(object):
    commands = {}

    @classmethod
    def register_command(cls, subparsers, command_class):
        assert(
            command_class.name not in cls.commands
        ), 'Command entry already exists!!'
        subparser = subparsers.add_parser(command_class.name)
        cls.commands[command_class.name] = command_class(subparser)

    def __init__(self, parser):
        self.parser = parser
        self.args(self.parser)
        self.parser.set_defaults(func=self.run)

    def args(self, parser):
        pass

    def run(self, args):
        raise NotImplementedError

    def fme_feature_is_supported(self, path):
        return os.path.isfile(path)


def global_arguments(parser):
    parser.add_argument('-s', '--socket-id', default=0,
                        type=int, help='socket id of FPGA resource')
