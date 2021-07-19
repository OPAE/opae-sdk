#! /usr/bin/env python3
# Copyright(c) 2021, Intel Corporation
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
import re
import sys
from argparse import ArgumentParser, FileType
from opae.admin.sysfs import pcie_device, pci_node


PCI_ADDRESS_PATTERN = (r'^(?P<pci_address>'
                       r'(?:(?P<segment>[\da-f]{4}):)?'
                       r'(?P<bdf>(?P<bus>[\da-f]{2}):'
                       r'(?P<device>[\da-f]{2})\.(?P<function>[0-7]{1})))$')
PCI_ADDRESS_RE = re.compile(PCI_ADDRESS_PATTERN, re.IGNORECASE)

PCI_VENDOR_DEVICE_PATTERN = r'^(?P<vendor>[\da-f]{4})?:(?P<device>[\da-f]{4})?$'
PCI_VENDOR_DEVICE_RE = re.compile(PCI_VENDOR_DEVICE_PATTERN, re.IGNORECASE)


def pci_devices(inp):
    m = PCI_ADDRESS_RE.match(inp)
    if m:
        d = m.groupdict()
        pci_address =  '{}:{}'.format(d.get('segment') or '0000', d['bdf'])
        pci_devices = pcie_device.enum([{'pci_node.pci_address': pci_address}])
        return pci_devices
    m = PCI_VENDOR_DEVICE_RE.match(inp)
    if m:
        d = m.groupdict()
        vendor_str = d.get('vendor')
        device_str = d.get('device')
        if not vendor_str and not device_str:
            raise SystemExit('must specify vendor or device')
        f = {}
        if vendor_str:
            f['pci_node.vendor_id'] = int(vendor_str, 16)
        if device_str:
            f['pci_node.device_id'] = int(device_str, 16)
        pci_devices = pcie_device.enum([f])
        return pci_devices

    raise ValueError('wrong pci address format: {}'.format(inp))


class pci_op(object):
    def __init__(self, op):
        if op not in dir(pci_node):
            raise KeyError(f'{op} is not a valid operation')
        self.op = op

    def __call__(self, pci_device, args, *other):
        if not args.other_endpoints:
            getattr(pci_device.pci_node, self.op)()
        else:
            for p in pci_device.pci_node.root.endpoints:
                if p.pci_address != args.device:
                    getattr(p, self.op)(*other)


class pci_prop(object):
    def __init__(self, op, arg_type=str):
        if op not in dir(pci_node):
            raise KeyError(f'{op} is not a valid operation')
        self.op = op
        self.arg_type = arg_type

    def __call__(self, pci_device, args, value):
        if not args.other_endpoints:
            try:
                setattr(pci_device.pci_node, self.op, self.arg_type(value))
            except ValueError:
                print(f'Could not set {self.op} to {value} for {pci_device}')
        else:
            for p in pci_device.pci_node.root.endpoints:
                if p.pci_address != args.device:
                    try:
                        setattr(p, self.op, self.arg_type(value))
                    except ValueError:
                        print(f'Could not set {self.op} to {value} for {p}')


def rescan(pci_device, args, *rest):
    pci_device.pci_node.pci_bus.node('rescan').value = 1


class aer(object):
    def __init__(self):
        self.parser = ArgumentParser('pci_device [device] aer')
        self.parser.add_argument('action', choices=['dump', 'mask', 'clear'], nargs='?',
                                 help='AER related operation to perform')
    def __call__(self, device, args, *rest):
        myargs, rest = self.parser.parse_known_args(rest)
        action = myargs.action or 'dump'
        getattr(self, action)(device, *rest)


    def dump(self, device):
        for key in ['aer_dev_correctable', 'aer_dev_fatal', 'aer_dev_nonfatal']:
            if device.pci_node.have_node(key):
                print('-' * 64)
                label = f'{key.lstrip("aer_dev").upper()} ({device})'
                print(f'|{label:^62}|')
                print('-' * 64)
                for line in device.node(key).value.split('\n'):
                    k,v = line.split()
                    print(f'|{k:>30}: {v:30}|')
                print('-' * 64)
                print()

    def mask(self, device, *args):
        parser = ArgumentParser('pci_device [device] aer mask')
        parser.add_argument('values', nargs='*', default=[])
        parser.add_argument('--root-port', action='store_true',
                            default=False)
        parser.add_argument('-o', '--output', type=FileType('w'),
                            default=sys.stdout)
        parser.add_argument('-i', '--input', type=FileType('r'))

        my_args = parser.parse_args(args)
        inp = my_args.input
        if inp:
            values = my_args.input.readline().split()
        else:
            values = my_args.values
        if not values or values[0] in ['show', 'print']:
            v0, v1 = device.pci_node.aer
            my_args.output.write(f'0x{v0:08x} 0x{v1:08x}\n')
        elif values[0] == 'all':
            device.pci_node.aer = (0xFFFFFFFF, 0xFFFFFFFF)
        elif values[0] == 'off':
            device.pci_node.aer = (0, 0)
        elif len(values) == 2:
            device.pci_node.aer = [int(v, 0) for v in values]
        else:
            raise SystemExit('incorrect values for setting AER')


def topology(pci_device, args):
    for line in pci_device.pci_node.root.tree().split('\n'):
        if pci_device.pci_node.pci_address in line:
            line = '\033[92m{}\033[00m' .format(line)
        elif '(pcieport)' not in line:
            line = '\033[96m{}\033[00m' .format(line)
        print(line)


def main():
    actions = {'unbind': pci_op('unbind'),
               'rescan': rescan,
               'remove': pci_op('remove'),
               'vf': pci_prop('sriov_numvfs', int),
               'aer': aer(),
               'topology': topology}

    parser = ArgumentParser()
    parser.add_argument('devices', type=pci_devices,
                        metavar='device-filter',
                        help=('pcie filter of device '
                              '([segment:]bus:device.function)'
                              ' or [vendor]:[device]'))
    parser.add_argument('action', choices=sorted(actions.keys()),
                        nargs='?',
                        help='action to perform on device')
    parser.add_argument('-E', '--other-endpoints', action='store_true',
                        default=False,
                        help='perform action on peer pcie devices')
    args, rest = parser.parse_known_args()


    if not args.devices:
        raise SystemExit(f'{args.device} not found')

    for dev in args.devices:
        actions[args.action or 'topology'](dev, args, *rest)


if __name__ == '__main__':
    main()
