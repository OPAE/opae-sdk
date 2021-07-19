#! /usr/bin/env python3
# Copyright(c) 2019, Intel Corporation
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

from __future__ import print_function
import argparse
from opae.diag.eth_group import eth_group
from .common import FpgaFinder, exception_quit, COMMON, hexint

FPGA_PHY_GROUP_ID = {'host': 1, 'line': 0}

FPGA_PHY_REG_RX_SERIALLPBKEN = 0x2E1


class FPGALPBK(COMMON):
    ports = []
    phy_number = 0
    phy_group_dev = ''
    speed = 10

    def __init__(self, args):
        self.en = args.en
        self.side = args.side
        self.type = args.type
        self.direction = args.direction
        self.argport = args.port
        self.eth_grps = args.eth_grps

    def check_args(self):
        type_dict = {
            10: {
                'line': {
                    'local': ['serial'],
                    'remote': ['precdr', 'postcdr']
                },
                'host': {
                    'remote': ['serial'],
                    'local': ['precdr', 'postcdr']
                }
            },
            25: {
                'line': {
                    'local': ['serial'],
                    'remote': []
                },
                'host': {
                    'local': [],
                    'remote': []
                }
            },
            40: {
                'line': {
                    'local': [],
                    'remote': []
                },
                'host': {
                    'local': [],
                    'remote': ['serial']
                }
            }
        }

        if self.speed not in type_dict:
            exception_quit('unknown speed {}'.format(self.speed))
        support_type = type_dict[self.speed][self.side][self.direction]
        if not support_type:
            exception_quit('fpga NOT support loopback at side [{}] '
                           'in the direction [{}]'.format(self.side,
                                                          self.direction))
        if self.type not in support_type:
            prompt_msg = ('fpga support loopback type {} at side [{}] '
                          'in the direction [{}]'.format(support_type,
                                                         self.side,
                                                         self.direction))
            exception_quit('{}\n{} is given!'.format(prompt_msg, self.type))

    # f: fpga handler
    # phy: phy index
    # reg: fpga register offset
    # idx: propose to change the register field lowest bit index
    # width: propose to change register field length
    # value: propose to change register field value
    def fpga_phy_reg_set_field(self, f, phy, reg, idx, width, value):
        self.fpga_eth_reg_set_field(f, 'phy', phy, reg, idx, width, value)

    def fpga_loopback_en(self, en):
        with open(self.phy_group_dev, 'w') as f:
            if self.speed == 10:
                if self.type == 'serial':
                    for i in self.ports:
                        self.fpga_phy_reg_set_field(
                            f, i, FPGA_PHY_REG_RX_SERIALLPBKEN, 0, 1, en)
                elif self.type == 'precdr':
                    for i in self.ports:
                        self.fpga_phy_reg_set_field(f, i, 0x137, 7, 1, en)
                        self.fpga_phy_reg_set_field(f, i, 0x13c, 7, 1, 0)
                        self.fpga_phy_reg_set_field(f, i, 0x132, 4, 2, 0)
                        self.fpga_phy_reg_set_field(f, i, 0x142, 4, 1, en)
                        self.fpga_phy_reg_set_field(f, i, 0x11d, 0, 1, en)
                elif self.type == 'postcdr':
                    for i in self.ports:
                        self.fpga_phy_reg_set_field(f, i, 0x137, 7, 1, 0)
                        self.fpga_phy_reg_set_field(f, i, 0x13c, 7, 1, en)
                        self.fpga_phy_reg_set_field(f, i, 0x132, 4, 2, en)
                        self.fpga_phy_reg_set_field(f, i, 0x142, 4, 1, 0)
                        self.fpga_phy_reg_set_field(f, i, 0x11d, 0, 1, 0)
            elif self.speed == 25:
                for i in self.ports:
                    self.fpga_eth_reg_set_field(f, 'mac', i, 0x313, 0, 4, en)
            elif self.speed == 40:
                for i in self.ports:
                    v = 0xf if en else en
                    self.fpga_eth_reg_set_field(f, 'mac', i, 0x313, 0, 4, v)
            else:
                exception_quit('unsupport speed mode {}'.format(self.speed))

    def get_port_group_info(self):
        info = self.get_eth_group_info(self.eth_grps)
        for phy_grp in info:
            if phy_grp == FPGA_PHY_GROUP_ID[self.side]:
                num, _, spd, node = info[phy_grp]
                self.phy_number = num
                self.phy_group_dev = node
                self.speed = spd

    def start(self):
        self.get_port_group_info()
        self.check_args()
        self.ports = self.get_port_list(self.argport, self.phy_number)
        self.fpga_loopback_en(self.en)

    def eth_group_loopback_en(self, en):
        for keys, values in self.eth_grps.items():
            eth_group_inst = eth_group()
            ret = eth_group_inst.eth_group_open(values[0])
            if ret != 0:
                return None

            if self.speed == 10:
                if self.type == 'serial':
                    for i in self.ports:
                        self.eth_group_phy_reg_set_field(
                            eth_group_inst,
                            i, FPGA_PHY_REG_RX_SERIALLPBKEN,
                            0, 1, en)
                elif self.type == 'precdr':
                    for i in self.ports:
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x137, 7, 1, en)
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x13c, 7, 1, 0)
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x132, 4, 2, 0)
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x142, 4, 1, en)
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x11d, 0, 1, en)
                elif self.type == 'postcdr':
                    for i in self.ports:
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x137, 7, 1, 0)
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x13c, 7, 1, en)
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x132, 4, 2, en)
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x142, 4, 1, 0)
                        self.eth_group_phy_reg_set_field(eth_group_inst,
                                                         i, 0x11d, 0, 1, 0)
            elif self.speed == 25:
                for i in self.ports:
                    self.eth_group_reg_set_field(eth_group_inst,
                                                 'mac', i, 0x313, 0, 4, en)
            elif self.speed == 40:
                for i in self.ports:
                    v = 0xf if en else en
                    self.eth_group_reg_set_field(eth_group_inst,
                                                 'mac', i, 0x313, 0, 4, v)
            else:
                exception_quit('unsupport speed mode {}'.format(self.speed))
            eth_group_inst.eth_group_close()

    def eth_group_phy_reg_set_field(self, eth_group,
                                    phy, reg, idx, width, value):
        self.eth_group_reg_set_field(eth_group,
                                     'phy', phy, reg, idx, width, value)

    def eth_group_port_info(self):
        info = self.eth_group_info(self.eth_grps)
        for phy_grp in info:
            if phy_grp == FPGA_PHY_GROUP_ID[self.side]:
                num, _, spd = info[phy_grp]
                self.phy_number = num
                self.speed = spd

    def eth_group_start(self):
        self.eth_group_port_info()
        self.check_args()
        self.ports = self.get_port_list(self.argport, self.phy_number)
        self.eth_group_loopback_en(self.en)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--segment', '-S',  type=hexint,
                        help='Segment number of PCIe device')
    parser.add_argument('--bus', '-B',  type=hexint,
                        help='Bus number of PCIe device')
    parser.add_argument('--device', '-D',  type=hexint,
                        help='Device number of PCIe device')
    parser.add_argument('--function', '-F',  type=hexint,
                        help='Function number of PCIe device')
    parser.add_argument('--direction',
                        required=True,
                        choices=['local', 'remote'],
                        help='choose loopback direction from chip view')
    parser.add_argument('--enable',
                        action='store_const',
                        dest='en',
                        const=1,
                        help='loopback enable')
    parser.add_argument('--disable',
                        action='store_const',
                        dest='en',
                        const=0,
                        help='loopback disable')
    parser.add_argument('--type',
                        choices=['serial', 'precdr', 'postcdr'],
                        help='choose loopback type')
    parser.add_argument('--port',
                        nargs='*',
                        default='all',
                        help='enable/disable loopback on specific port')
    parser.add_argument('--side',
                        required=True,
                        choices=['line', 'host'],
                        help='choose loopback on which side')
    args, left = parser.parse_known_args()

    if args.en is None:
        exception_quit('please specify --enable/--disable loopback!')
    else:
        op = 'enable' if args.en else 'disable'
        print('{} fpga loopback'.format(op))

    if not args.type:
        if ((args.side == 'line' and args.direction == 'local') or
                (args.side == 'host' and args.direction == 'remote')):
            args.type = 'serial'
        else:
            exception_quit(
                'missing argument --type [serial | precdr | postcdr]')

    f = FpgaFinder(args.segment, args.bus, args.device, args.function)
    devs = f.find()
    for d in devs:
        print('bdf: {segment:04x}:{bus:02x}:{dev:02x}.{func:x}'.format(**d))
    if len(devs) > 1:
        exception_quit('{} FPGAs are found\nplease choose '
                       'one FPGA'.format(len(devs)))
    if not devs:
        exception_quit('no FPGA found')

    args.fpga_root = devs[0].get('path')
    args.eth_grps = f.find_eth_group(args.fpga_root)
    print("args.eth_grps", args.eth_grps)
    if len(args.eth_grps) == 0:
        exception_quit("Invalid Eth group MDEV")

    lp = FPGALPBK(args)
    lp.eth_group_start()
    print('Done')


if __name__ == "__main__":
    main()
