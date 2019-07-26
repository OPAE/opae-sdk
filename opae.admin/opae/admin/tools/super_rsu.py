#! /usr/bin/env python
# Copyright(c) 2019, Intel Corporation
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
import glob
import json
import logging
import logging.handlers
import operator
import os
import re
import signal
import subprocess
import sys
import tempfile
import time
import xml.etree.cElementTree as ET

from contextlib import contextmanager
from ctypes import c_uint64, LittleEndianStructure, Union
from datetime import datetime, timedelta
from threading import Thread
from uuid import UUID

PCI_ADDRESS_PATTERN = (r'(?P<pci_address>'
                       r'(?P<segment>[\da-f]{4}):(?P<bdf>(?P<bus>[\da-f]{2}):'
                       r'(?P<device>[\da-f]{2})\.(?P<function>\d)))')
PCI_ADDRESS_RE = re.compile(PCI_ADDRESS_PATTERN, re.IGNORECASE)

BMC_SENSOR_PATTERN = (r'^\(\s*(?P<num>\d+)\)\s*(?P<name>[\w \.]+)\s*:\s*'
                      r'(?P<value>[\d\.]+)\s+(?P<units>\w+)$')
BMC_SENSOR_RE = re.compile(BMC_SENSOR_PATTERN, re.MULTILINE)

VERSION_OP_PATTERN = (r'(?P<type>\w+[-\w]+)\s*(?P<op>(?:(?:[><!=])?=)|[<>])\s*'
                      r'(?P<version>.*)')
VERSION_OP_RE = re.compile(VERSION_OP_PATTERN)

VERSION_PATTERN = (r'(?P<major>\d+)(?:\.(?P<minor>\d+)(?:\.(?P<patch>\d+))?)?'
                   r'(?:\s+(?P<rest>.*))?')
VERSION_RE = re.compile(VERSION_PATTERN)

BMC_NORMAL_RANGES = {
    'FPGA Die Temperature': (1.0, 85.0),
    'Board Power': (1.0, 150.0)
}

TIMEDELTA_PATTERN = r'(?P<number>\d+)?(?P<fraction>\.\d+)?(?P<units>[dhms])'
TIMEDELTA_RE = re.compile(TIMEDELTA_PATTERN)

CLASS_ROOT = '/sys/class/fpga'

AER_OFF = 0xFFFFFFFF
AER_ON = 0x00000000

DRY_RUN = False

FLASH_TIMEOUT = 10.0

NULL_CMD = 'sleep 11'

NVM_CFG_HEAD = '''
CURRENT FAMILY: 1.0.0
CONFIG VERSION: 1.14.0

;release 19.3/19.4 to release 20.0

'''

NVM_CFG_DEVICE = '''
BEGIN DEVICE
DEVICENAME: XXV710
VENDOR: {VENDOR}
DEVICE: {DEVICE}
NVM IMAGE: {FILENAME}
SKIP OROM: TRUE
EEPID: {EEPID}
REPLACES: {REPLACES}
;Family identifies 1ea696de-c026-45ba-bfed-d1022f611d90
RESET TYPE: POWER
END DEVICE
'''

OPAE_SHARE = '/usr/share/opae'
NVMUPDATE_EXE = 'nvmupdate64e'
OPAE_NVMUPDATE_EXE = os.path.join(OPAE_SHARE, 'bin', NVMUPDATE_EXE)

LOG = logging.getLogger(__name__)
TRACE = logging.DEBUG - 5
logging.addLevelName(TRACE, 'TRACE')
RSU_TMP_LOG = '/tmp/super-rsu.log'

SECURE_UPDATE_VERSION = 4


def parse_version(ver_str):
    m = VERSION_RE.match(ver_str)
    if m:
        return m.groups()
    return ver_str


def parse_timedelta(inp):
    data = {'d': 0.0, 'h': 0.0, 'm': 0.0, 's': 0.0}
    for m in TIMEDELTA_RE.finditer(inp):
        number = m.group('number')
        fraction = m.group('fraction')
        value = 0.0
        if number:
            value += int(number)
        if fraction:
            value += float(fraction)
        units = m.group('units')
        data[units] += value

    if sum(data.values()) == 0.0:
        LOG.warn("did not find positive values in timedelta string: '%s'", inp)
        return 0.0

    td = timedelta(days=data['d'],
                   hours=data['h'],
                   minutes=data['m'],
                   seconds=data['s'])
    return td.total_seconds()


def sys_exit(code, msg=None):
    LOG.info("%s exiting with code '%s'", os.path.basename(__file__), code)
    if msg is not None:
        print(msg)
    sys.exit(code)


def trace(msg, *args, **kwargs):
    LOG.log(TRACE, msg, *args, **kwargs)


def call_process(cmd, no_dry=False):
    if DRY_RUN and not no_dry:
        print(cmd)
        return '0x0' if 'setpci' in cmd else ''

    try:
        return subprocess.check_output(cmd.split(),
                                       stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as err:
        LOG.error('calling %s returned %d', cmd, err.returncode)
        LOG.debug('process output: %s', err.output)
        raise


class update_thread(Thread):
    def __init__(self, board, fn, *args, **kwargs):
        self._timeout = kwargs.pop('timeout', 0.0)
        thread_name = kwargs.pop('name', board.pci_node.bdf if board else None)
        super(update_thread, self).__init__(target=fn,
                                            name=thread_name,
                                            args=args,
                                            kwargs=kwargs)
        self._board = board

    @property
    def board(self):
        return self._board

    @property
    def timeout(self):
        return self._timeout

    def terminate(self):
        self._board.terminate_update()


class process_task(object):
    def __init__(self, cmd, **kwargs):
        self._cmd = cmd
        self._kwargs = kwargs
        self._start_time = None
        self._process = None

    @property
    def cmd(self):
        return self._cmd

    @property
    def start_time(self):
        return self._start_time

    def terminate(self, timeout=None):
        if self._process is None:
            LOG.warning('called terminate on a process that has not started')
            return
        p = self._process
        if timeout is not None:
            timeout = time.time() + timeout
            while time.time() < timeout and p.poll() is None:
                time.sleep(0.1)
        if p.poll() is None:
            LOG.warning('terminating task command: %s', self.cmd)
        p.terminate()
        _, stderr = p.communicate()

    def __call__(self):
        LOG.debug('starting task: %s', self._cmd)
        self._start_time = time.time()
        if DRY_RUN:
            print(self._cmd)
            self._process = subprocess.Popen(NULL_CMD, shell=True)
        else:
            self._process = subprocess.Popen(self._cmd.split(),
                                             stdin=subprocess.PIPE,
                                             stdout=self._kwargs.get('stdout'),
                                             stderr=self._kwargs.get('stderr'))
        return self._process


@contextmanager
def aer_disabled(*nodes):
    """aer_disabled Context manager used to disable AER on a set of pci nodes
                    The exit function will re-enable AER on the nodes
    :param *nodes(pci_node): A list of pci nodes to disable AER on
    """
    original_aer = [(node, node.aer) for node in nodes]
    for node in nodes:
        node.aer = (AER_OFF, AER_OFF)
    try:
        yield True if nodes and all(nodes) else None
    finally:
        for n, v in original_aer:
            n.aer = (AER_ON, AER_ON) if DRY_RUN else v


@contextmanager
def ignore_signals(*signals):
    handlers = {}

    for s in signals:
        try:
            signal.signal(s, signal.SIG_IGN)
        except ValueError:
            LOG.warn('signal (%s) cannot be ignored', s)
        else:
            handlers[s] = signal.getsignal(s)
    try:
        yield handlers
    finally:
        for k, v in handlers.iteritems():
            signal.signal(k, v)


def find_subdevices(node):
    """find_subdevices find a list of (vendor, device) tuples in children
                       and grandchildren

    :param node: parent node to find subdevices

    :return a set of (vendor, device) tuples
    """
    try:
        devices = [n.pci_id for n in node.children]
    except NameError:
        return set()

    for c in node.children:
        devices.extend(find_subdevices(c))
    return set(devices)


class sysfs_node(object):
    """sysfs_node is a base class representing a sysfs object in sysfs """
    def __init__(self, sysfs_path):
        self._sysfs_path = sysfs_path

    def read_node(self, *nodes):
        """read_node function to read sysfs attributes relative to self

        :param *nodes: list of subdirectory/file objects to append
        """
        path = os.path.join(self._sysfs_path, *nodes)
        if not os.path.exists(path):
            raise NameError("Could not find sysfs node: {}".format(path))
        with open(path, 'r') as fd:
            value = fd.read().strip()
        trace('read %s from %s', value, path)
        return value

    def write_node(self, value, *nodes):
        global DRY_RUN
        path = os.path.join(self._sysfs_path, *nodes)
        if not os.path.exists(path):
            raise NameError("Could not find sysfs node: {}".format(path))
        trace('writing %s to %s', value, path)
        if DRY_RUN:
            print('echo {} > {}'.format(value, path))
        else:
            with open(path, 'w') as fd:
                fd.write('{}\n'.format(value))

    @property
    def sysfs_path(self):
        return self._sysfs_path


class pci_node(sysfs_node):
    PCI_BUS_SYSFS = '/sys/bus/pci/devices'
    """pci_node is a class used to encapsulate a node on the pci bus
       that can be found in /sys/bus/pci/devices and can have a parent
       node and one or more children nodes
       This can be used to represent a PCIe tree or subtree"""
    def __init__(self, pci_address, parent=None, **kwargs):
        """__init__ initialize a pci_node object

        :param pci_address: The pci address of the node using following format
                            [segment:]bus:device.function
        :param parent(pci_node): Another pci_node object that is the parent of
                                 this node in the PCIe tree
        :param **kwargs: optional arguments
            class_node: Path to device in /sys/class/fpga(_region)
        """
        node_path = os.path.join(self.PCI_BUS_SYSFS,
                                 pci_address['pci_address'])
        super(pci_node, self).__init__(node_path)
        self._pci_address = pci_address
        self._parent = parent
        self._class_node = kwargs.get('class_node')
        self._children = []
        self._aer_cmd1 = 'setpci -s {} ECAP_AER+0x08.L'.format(
            pci_address['pci_address'])
        self._aer_cmd2 = 'setpci -s {} ECAP_AER+0x14.L'.format(
            pci_address['pci_address'])

    def __str__(self):
        return '[pci_address({}), pci_id(0x{:04x}, 0x{:04x})]'.format(
            self.pci_address, *self.pci_id)

    def __repr__(self):
        return str(self)

    def _find_children(self):
        children = []
        for f in os.listdir(self.sysfs_path):
            if not f.startswith(self.pci_address):
                m = PCI_ADDRESS_RE.match(f)
                if m:
                    children.append(pci_node(m.groupdict(), self))
        return children

    def tree(self, level=0):
        text = '{}{}\n'.format(' ' * level*4, self)
        for n in self.children:
            text += n.tree(level+1)
        return text

    @property
    def root(self):
        if self.parent is None:
            return self
        return self.parent.root

    @property
    def pci_address(self):
        """pci_address get the pci address of the node"""
        return self._pci_address['pci_address']

    @property
    def bdf(self):
        """pci_address get the pci address of the node"""
        return self._pci_address['bdf']

    @property
    def segment(self):
        """segment get the pci segment or domain of the node"""
        return self._pci_address['segment']

    @property
    def domain(self):
        """segment get the pci segment or domain of the node"""
        return self._pci_address['segment']

    @property
    def bus(self):
        """bus get the pci bus of the node"""
        return self._pci_address['bus']

    @property
    def device(self):
        """device get the pci device of the node"""
        return self._pci_address['device']

    @property
    def function(self):
        """function get the pci function of the node"""
        return self._pci_address['function']

    @property
    def parent(self):
        """parent get the parent (pci_node) of this node"""
        return self._parent

    @parent.setter
    def parent(self, value):
        """parent set the parent (pci_node) of this node

        :param value: set the parent (pci_node) to this value
        """
        self._parent = value

    @property
    def children(self):
        """children get the children pci_node objects"""
        if not self._children:
            self._children = self._find_children()
        return self._children

    @property
    def all_children(self):
        nodes = self.children
        for n in nodes:
            nodes.extend(n.all_children)
        return list(set(nodes))

    @property
    def vendor_id(self):
        return self.read_node('vendor')

    @property
    def device_id(self):
        return self.read_node('device')

    @property
    def pci_id(self):
        return (int(self.vendor_id, 16),
                int(self.device_id, 16))

    def remove(self):
        LOG.debug('removing device at %s', self.pci_address)
        self.write_node('1', 'remove')

    def rescan(self):
        LOG.debug('rescanning device at %s', self.pci_address)
        self.write_node('1', 'rescan')

    def rescan_bus(self, bus, power_on=True):
        if power_on:
            power = self.read_node('power', 'control')
            if power != 'on':
                self.write_node('on', 'power', 'control')
        LOG.debug('rescanning bus %s under %s', bus, self.pci_address)
        self.write_node('1', 'pci_bus', bus, 'rescan')
        self._children = []

    @property
    def aer(self):
        return (int(call_process(self._aer_cmd1), 16),
                int(call_process(self._aer_cmd2), 16))

    @aer.setter
    def aer(self, values):
        call_process('{}={:#08x}'.format(self._aer_cmd1, values[0]))
        call_process('{}={:#08x}'.format(self._aer_cmd2, values[1]))


class spi_bus(sysfs_node):
    def __init__(self, sysfs_path):
        super(spi_bus, self).__init__(sysfs_path)


class fpga_device(sysfs_node):
    SPI_ALIAS = 'spi:intel-max10'

    def __init__(self, sysfs_path):
        super(fpga_device, self).__init__(sysfs_path)
        self._fme = None
        self._port = None
        self._spi = None

    @property
    def fme(self):
        if self._fme is None:
            self._fme = self._find_fme()
        return self._fme

    def _find_fme(self):
        fme_pattern = os.path.join(self.sysfs_path, "*fme*")
        glob_results = glob.glob(fme_pattern)
        if len(glob_results) == 1:
            return sysfs_node(glob_results[0])
        LOG.warning("Could not find FME device")

    @property
    def port(self):
        if self._port is None:
            self._port = self._find_port()
        return self._port

    def _find_port(self):
        port_pattern = os.path.join(self.sysfs_path, "*port*")
        glob_results = glob.glob(port_pattern)
        if len(glob_results) == 1:
            return sysfs_node(glob_results[0])
        LOG.warning("Could not find PORT device")

    @property
    def spi_bus(self):
        if self._spi is None:
            self._spi = self._find_spi()
        return self._spi

    def _find_spi(self):
        for root, dirs, files in os.walk(self.fme.sysfs_path):
            basename = os.path.basename(root)
            if basename.startswith('spi') and 'modalias' in files:
                with open(os.path.join(root, 'modalias'), 'r') as fd:
                    if fd.read().strip() == self.SPI_ALIAS:
                        return spi_bus(root)
        LOG.warning('Could not find SPI bus')
        raise NameError('Could not find SPI bus')


# VERSION PARSING STRUCTS
class bitstream_id_bits(LittleEndianStructure):
    _fields_ = [
                    ("githash", c_uint64, 32),
                    ("hssi", c_uint64, 4),
                    ("reserved36", c_uint64, 12),
                    ("debug", c_uint64, 4),
                    ("patch", c_uint64, 4),
                    ("minor", c_uint64, 4),
                    ("major", c_uint64, 4),
    ]


class vc_bitstream_id_bits(LittleEndianStructure):
    _fields_ = [
                    ("githash", c_uint64, 32),
                    ("interface", c_uint64, 4),
                    ("reserved36", c_uint64, 12),
                    ("debug", c_uint64, 4),
                    ("patch", c_uint64, 4),
                    ("minor", c_uint64, 4),
                    ("major", c_uint64, 4),
    ]


class fme_version(Union):
    _fields_ = [("bits", bitstream_id_bits),
                ("value", c_uint64)]

    def __init__(self, value):
        self.value = value

    def __str__(self):
        val = '{}.{}.{}'.format(self.bits.major,
                                self.bits.minor,
                                self.bits.patch)
        return val

    def __repr__(self):
        return str(self)

    def __eq__(self, other):
        return str(self) == str(other)

    def __ne__(self, other):
        return str(self) != str(other)

    @property
    def major(self):
        return self.bits.major

    @property
    def minor(self):
        return self.bits.minor

    @property
    def patch(self):
        return self.bits.patch


class vc_fme_version(fme_version):
    _fields_ = [("bits", vc_bitstream_id_bits),
                ("value", c_uint64)]


class spi_version_bits(LittleEndianStructure):
    _fields_ = [
                    ("patch", c_uint64, 8),
                    ("minor", c_uint64, 8),
                    ("major", c_uint64, 8),
                    ("revision", c_uint64, 8),
                    ("reserved", c_uint64, 32),
    ]


class spi_version(Union):
    _fields_ = [("bits", spi_version_bits),
                ("value", c_uint64)]

    def __init__(self, value):
        self.value = value

    def __str__(self):
        val = '{}.{}.{}'.format(self.bits.major,
                                self.bits.minor,
                                self.bits.patch)
        return val

    def __repr__(self):
        rev = chr(self.bits.revision)
        val = str(self)
        if rev.isalpha():
            val += ' Revision {} '.format(rev)
        return val

    def __eq__(self, other):
        return str(self) == str(other)

    def __ne__(self, other):
        return str(self) != str(other)

    @property
    def major(self):
        return self.bits.major

    @property
    def minor(self):
        return self.bits.minor

    @property
    def patch(self):
        return self.bits.patch

    @property
    def revision(self):
        rev = chr(self.bits.revision)
        if not rev.isalpha():
            return ''
        return rev


# flashable classes
# classes here represent parts on the FPGA that can be flashed
class flashable(object):
    def __init__(self, fpga, is_factory=False, **kwargs):
        self._fpga = fpga
        self._is_factory = is_factory
        self._can_verify = kwargs.get('can_verify', True)

    @property
    def can_verify(self):
        return self._can_verify

    @property
    def image_load(self):
        return None

    def needs_flash(self, flash_info):
        return self.version != flash_info['version']

    def is_supported(self, flash_info):
        return True

    def command(self, flash_info, filename, pci_address):
        flash_type = flash_info['type']
        if flash_info.get('secure', False):
            cmd = 'fpgasupdate {} {}'.format(filename, pci_address)
        else:
            cmd = 'fpgaflash {} {} {}'.format(flash_type, filename,
                                              pci_address)
            if self.is_factory:
                cmd += ' -y'

        return cmd

    @property
    def version(self):
        raise NotImplementedError

    @property
    def is_factory(self):
        return self._is_factory


class bmc_flashable(flashable):
    @property
    def version_path(self):
        raise NotImplementedError()

    @property
    def version(self):
        value = self._fpga.spi_bus.read_node(self.version_path)
        return spi_version(int(value, 16))

    def is_supported(self, flash_info):
        current_rev = self.version.revision.lower()
        flash_rev = flash_info.get('revision', '').lower()
        if current_rev == '' or flash_rev == '':
            return True
        LOG.debug('%s - current_rev: "%s", flash_rev: "%s"',
                  flash_info['type'], current_rev, flash_rev)
        return bool(current_rev in flash_rev)


class bmc_fw(bmc_flashable):
    @property
    def version_path(self):
        return 'bmcfw_flash_ctrl/bmcfw_version'


class bmc_img(bmc_flashable):
    @property
    def version_path(self):
        return 'max10_version'


class bmc_pkg(flashable):
    def __init__(self, fpga, is_factory=False, **kwargs):
        super(bmc_pkg, self).__init__(fpga, is_factory, **kwargs)
        self._img = bmc_img(fpga, is_factory, **kwargs)
        self._fw = bmc_fw(fpga, is_factory, **kwargs)

    @property
    def version(self):
        return (self._img.version, self._fw.version)

    def is_supported(self, flash_info):
        return self._img.is_supported(flash_info) and self._fw.is_supported(
            flash_info)


class a10(flashable):
    @property
    def image_load(self):
        return int(self._fpga.spi_bus.read_node('fpga_flash_ctrl',
                                                'fpga_image_load'))

    @property
    def version(self):
        int_value = int(self._fpga.fme.read_node('bitstream_id'), 16)
        return '0x{:016x}'.format(int_value)


class dtb(a10):
    def needs_flash(self, flash_info):
        return True


class bmc_fw_pkvl(bmc_fw):
    def command(self, flash_info, filename, pci_address):
        integrated = flash_info.get('integrated', False)
        flash_cmd = 'phy_eeprom' if integrated else 'bmc_fw'
        return 'fpgaflash {} {} {}'.format(flash_cmd, filename, pci_address)


class eth_instance(object):
    TEST_MODES = ['offline', 'online', 'external_lb']

    def __init__(self, node, version, address):
        self._vendor_id = node.get('vendor')
        self._device_id = node.get('device')
        self._bus = int(node.get('bus'))
        self._device = int(node.get('dev'))
        self._function = int(node.get('func'))
        self._version = version
        self._address = address
        # for now use enp{bus}s{subdevice}f{func} as the interface name
        # TODO (RR): replace with better method to map to bdf
        self._interface = 'enp{bus}s{subdevice}f{func}'.format(**node.attrib)

    @property
    def version(self):
        return self._version

    @property
    def bus(self):
        return self._bus

    @property
    def device(self):
        return self._device

    @property
    def function(self):
        return self._function

    def check(self):
        # TODO (RR): implement ethernet testing
        return True


class nvm_updater(object):
    def __init__(self):
        self._to_update = []

    def enumerate(self, **kwargs):
        nodes = self._run_inventory(**kwargs)
        instances = []
        for node in nodes:
            module = node.find('Module[@version]')
            mac = node.find('MACAddresses/MAC[@address]')
            fail = node.find('Status[@result="fail"]')
            version = None if module is None else module.get('version')
            address = None if mac is None else mac.get('address')
            if version is None or address is None and fail:
                LOG.warn('could not get all attributes from xml: %s',
                         fail.text)
            instances.append(eth_instance(node, version, address))
        return instances

    def _run_inventory(self, **kwargs):
        f = tempfile.NamedTemporaryFile()
        LOG.debug("running nvmupdate inventory")
        try:
            call_process('{} -i -o {}'.format(OPAE_NVMUPDATE_EXE, f.name),
                         no_dry=True)
        except (subprocess.CalledProcessError, OSError):
            LOG.error('%s may not be installed', NVMUPDATE_EXE)
            return []
        tree = ET.parse(f.name)
        root = tree.getroot()
        attr = ''.join(["[@{}]".format(k)
                        for k in kwargs.keys()])
        xpath = './Instance{}'.format(attr)
        LOG.debug('searching %s', xpath)

        def filter_attrib(node):
            for k, v in kwargs.iteritems():
                if k not in node.attrib:
                    return False
                if k in ['vendor', 'device', 'bus']:
                    attrib_value = int(node.get(k), 16)
                else:
                    try:
                        attrib_value = type(v)(node.get(k))
                    except ValueError:
                        attrib_value = None
                if attrib_value != v:
                    return False
            return True

        nodes = filter(filter_attrib, root.findall(xpath))
        LOG.debug('found %s nodes in inventory', len(nodes))
        return nodes


# pac classes
class pac(object):
    boot_page = 0
    FACTORY_IMAGE = 0
    USER_IMAGE = 1

    def __init__(self, pci_node, fpga, **kwargs):
        self._pci_node = pci_node
        self._fpga = fpga
        self._other_pci = kwargs.get('other_pci', [])
        self._other_nodes = None
        self._common_root = None
        self._common_tree = None
        self._update_thread = None
        self._terminate = False
        self._flashables = {}
        self._errors = 0
        self.add_flashables(user=a10(fpga),
                            factory=a10(fpga, True),
                            factory_only=a10(fpga, True),
                            bmc_fw=bmc_fw(fpga),
                            bmc_factory=bmc_img(fpga, True),
                            bmc_img=bmc_img(fpga),
                            bmc_pkg=bmc_pkg(fpga),
                            dtb=dtb(fpga, can_verify=False))

    def add_flashables(self, **kwargs):
        for k, v in kwargs.iteritems():
            if isinstance(v, flashable):
                self._flashables[k] = v
            else:
                LOG.warning('%s is not a flashable', type(v))

    def get_flashable(self, flash_type):
        return self._flashables.get(flash_type)

    @property
    def errors(self):
        return self._errors

    @property
    def pci_node(self):
        return self._pci_node

    @pci_node.setter
    def pci_node(self, node):
        if isinstance(node, pci_node):
            self._pci_node = node
        else:
            LOG.warn('pci_node value not compatible')

    @property
    def fpga(self):
        return self._fpga

    @property
    def common_root(self):
        if self._common_root is None:
            self._common_root = self._find_common_root()
        return self._common_root

    def _find_common_root(self):
        if not self._other_pci:
            return self.pci_node.parent
        node = self.pci_node.parent
        other = set(self._other_pci)
        while node:
            devices = find_subdevices(node)
            if other <= devices:
                return node
            else:
                node = node.parent
        return self.pci_node.parent

    @property
    def other_nodes(self):
        if self._other_nodes is None:
            self._other_nodes = self._find_others()
        return self._other_nodes

    def _find_others(self):
        nodes = []
        # go to the root node
        root = self.pci_node
        while root.parent is not None:
            root = root.parent
        # now iterate over all children (and grandchildren) from the root
        # and add the nodes with a pci_id in our list of other_pci ids
        for node in root.all_children:
            if node.pci_id in self._other_pci:
                nodes.append(node)
        return nodes

    def flash_task(self, flash_dir, flash_info, args):
        if not flash_info.get('enabled', True):
            return None

        flash_type = flash_info['type']
        flash_rev = flash_info.get('revision', '')
        filename = flash_info['filename']
        force = flash_info.get('force', args.force_flash)

        if os.path.exists(os.path.join(flash_dir, filename)):
            filename = os.path.join(flash_dir, filename)

        if not os.path.exists(filename):
            LOG.error('filename %s does not exist', filename)
            return None

        to_flash = self.get_flashable(flash_type)
        if to_flash is None:
            LOG.info('flash type %s is not recognized or supported',
                     flash_type)
            return None

        if not to_flash.is_supported(flash_info):
            LOG.info('[%s] %s is not supported by current component',
                     self.pci_node.bdf, (flash_type, flash_rev))
            return None

        needs_flash = (force or to_flash.is_factory or
                       to_flash.image_load == self.FACTORY_IMAGE or
                       to_flash.needs_flash(flash_info))
        if needs_flash:
            if force:
                LOG.debug('%s is being force flashed', flash_type)
            if to_flash.is_factory:
                LOG.debug('%s is a factory type flash', flash_type)
            if to_flash.image_load == self.FACTORY_IMAGE:
                LOG.debug('%s (%s) is booted from factory bank', flash_type,
                          self.pci_node)
            if to_flash.needs_flash(flash_info) and to_flash.can_verify:
                LOG.debug('%s versions not equal (system:%s != manifest:%s)',
                          flash_type, to_flash.version, flash_info['version'])
            cmd = to_flash.command(flash_info, filename,
                                   self.pci_node.pci_address)
            return process_task(cmd, stderr=subprocess.PIPE)

        if to_flash.version == flash_info['version']:
            LOG.debug('[%s] version (%s) up to date for %s', self.pci_node.bdf,
                      to_flash.version, flash_type)

    def _update(self, tasks):
        LOG.debug('update of board at %s started', self.pci_node)

        while tasks:
            # get the next task...
            task, flashfile = tasks.pop(0)
            p = task()
            while p.poll() is None:
                if self._terminate:
                    task.terminate(FLASH_TIMEOUT)
                    # stop processing next tasks
                    tasks = []
                    break
                time.sleep(0.1)
            if p.returncode:
                LOG.warning('%s exited with code: %s', task.cmd, p.returncode)
                self._errors += 1

            LOG.debug('task completed in %s', timedelta(
                      seconds=time.time() - task.start_time))

    def reset_flash_mode(self):
        self.fpga.spi_bus.write_node("0", 'bmcimg_flash_ctrl',
                                     'bmcimg_flash_mode')

    def update(self, flash_dir, rsu_config, args):
        tasks = []
        timeout = 0.0
        is_secure = rsu_config.get('version') >= SECURE_UPDATE_VERSION
        for flash_info in rsu_config['flash']:
            flash_info['secure'] = is_secure
            task = self.flash_task(flash_dir, flash_info, args)
            if task is not None:
                task_timeout = flash_info.get('timeout', '0')
                timeout += parse_timedelta(task_timeout)
                tasks.append((task, flash_info['filename']))
        LOG.debug('[%s] update timeout set to: %s', self.pci_node.bdf, timeout)
        if tasks:
            t = update_thread(self, self._update, tasks, timeout=timeout)
            t.start()
            return t
        return None

    def rsu(self, boot_page=None):
        page = self.boot_page if boot_page is None else boot_page
        try:
            self.fpga.spi_bus.write_node(str(page),
                                         'bmcimg_flash_ctrl',
                                         'bmcimg_image_load')
        except IOError:
            LOG.debug('[%s] anticipated error writing to bmcimg_image_load',
                      self.pci_node.bdf)

    def terminate_update(self):
        self._terminate = True

    def verify(self, rsu_config, args):
        unflashed = []
        for flash_info in rsu_config['flash']:
            if flash_info.get('enabled', True):
                flash_type = flash_info['type']
                flashed = self.get_flashable(flash_type)
                if flashed is None:
                    LOG.warn('could not find flashable for entry: %s',
                             flash_info)
                    continue

                if flashed.is_factory or not flashed.can_verify:
                    # can't verify factory flashes
                    continue
                if not flashed.is_supported(flash_info):
                    # skip if not supported
                    continue

                if flashed.image_load == self.FACTORY_IMAGE:
                    LOG.error('%s (%s) booted from factory bank', flash_type,
                              self.pci_node)
                    unflashed.append(flash_info)
                # we should've flashed this, check if versions match
                elif flashed.version != flash_info['version']:
                    LOG.debug('[%s] %s (%s) not at version in spec (%s)',
                              self.pci_node.bdf, flash_info['type'],
                              flashed.version, flash_info['version'])
                    unflashed.append(flash_info)
        return len(unflashed) == 0 and not self.errors

    def _validate_bmc(self, bmc_sensors):
        cmd = 'fpgainfo bmc --bus 0x{}'.format(self.pci_node.bus)
        try:
            LOG.debug('[%s] reading sensor data', self.pci_node.bdf)
            output = call_process(cmd)
        except (subprocess.CalledProcessError, OSError):
            return os.EX_SOFTWARE
        sensors = {}
        for m in BMC_SENSOR_RE.finditer(output):
            sensors[m.group('name').strip()] = m.groupdict()
        if not sensors:
            LOG.error('no sensor data found')
            return os.EX_SOFTWARE

        errors = 0

        for (k, (lo, hi)) in bmc_sensors['ranges'].iteritems():
            sensor = sensors.get(k)
            if not sensor:
                LOG.debug("sensor '%s' not found in sensor data", k)
                errors += 1
                continue
            str_value = sensors[k]['value']
            value = float(str_value)
            LOG.debug('[%s] - %s = %s', self.pci_node.bdf,
                      k, value)
            if value < lo or value > hi:
                LOG.warn('[%s] sensor (%s) value (%s) out of range: %s',
                         self.pci_node.bdf, k, value, (lo, hi))
                errors += 1
        return errors

    def run_tests(self, rsu_config):
        err = 0
        if rsu_config.get('version', 0) <= 1:
            return os.EX_OK

        sensors_validated = False
        for sv in rsu_config.get('sensor validation', []):
            if sv['type'] == 'bmc':
                bmc = self.get_flashable('bmc_fw')
                if bmc.version.revision.lower() == sv['revision'].lower():
                    LOG.debug("[%s] validating sensors for rev '%s'",
                              self.pci_node.bdf, sv['revision'])
                    err += self._validate_bmc(sv)
                    sensors_validated = True
                    break

        if not sensors_validated:
            LOG.warn('[%s] did not validate sensors', self.pci_node.bdf)
        elif err:
            LOG.error('[%s] error validating sensors', self.pci_node.bdf)
            return os.EX_SOFTWARE
        return os.EX_OK

    @property
    def is_secure(self):
        return 1 == len(glob.glob(
            os.path.join(self.fpga.fme.sysfs_path,
                         'ifpga_sec_mgr/ifpga_sec*')))



class vc(pac):
    boot_page = 0
    FACTORY_IMAGE = 0
    USER_IMAGE = 1
    PKVL_STATUS_UP = 0x11

    def __init__(self, pci_node, fpga, **kwargs):
        super(vc, self).__init__(pci_node, fpga, **kwargs)
        # vista creek bmc fw and pkvl eeprom is integrated
        # and versioned using bmc_fw version
        # override the flashable for bmc_fw from pac class
        # with bmc_fw_pkvl which uses 'phy_eeprom' when calling fpgaflash
        self.add_flashables(bmc_fw=bmc_fw_pkvl(fpga))

    @property
    def user_version(self):
        value = int(self.fpga.fme.read_node('bitstream_id'), 16)
        return vc_fme_version(value)

    def run_tests(self, rsu_config):
        failures = super(vc, self).run_tests(rsu_config)

        expected_afu_id = rsu_config.get('afu_id')
        if expected_afu_id is not None:
            try:
                read_afu_id = self._fpga.port.read_node('afu_id')
            except ValueError:
                LOG.error('[%s] could not read afu_id from system',
                          self.pci_node.bdf)
                failures += 1
            else:
                if UUID(expected_afu_id) != UUID(read_afu_id):
                    LOG.warn('[%s] afu_id read not equal to expected one',
                             self.pci_node.bdf)
                    failures += 1

        try:
            self._fpga.spi_bus.read_node('pkvl', 'pkvl_a_version')
            self._fpga.spi_bus.read_node('pkvl', 'pkvl_b_version')
        except NameError:
            LOG.warn('error reading pkvl versions')
            failures += 1

        return failures


class dc(pac):
    boot_page = 1


def make_pac(node, fpga, **kwargs):
    pacs = {(0x8086, 0x0b30): vc, (0x8086, 0x0b2b): dc}
    pac_class = pacs.get(node.pci_id)
    if pac_class:
        return pac_class(node, fpga, **kwargs)
    LOG.error('could not find pac with id: (%x, %x)',
              node.pci_id[0], node.pci_id[1])


def discover_boards(rsu_config, args):
    """discover_boards discover FPGA devices in sysfs given a config

    :param rsu_config(dict): A dictionary describing the product to discover.
                             This config should be made up of a product name
                             and a list of files to flash. The product will be
                             mapped to a PCI identifer to match. This file will
                             be passed to 'fpgaboard' for loading flash
                             binaries.
    """
    product_name = rsu_config.get('product')
    if product_name is None:
        LOG.warning("RSU config file missing 'product' key")
        return []

    product_vendor = rsu_config.get('vendor')
    if product_vendor is None:
        LOG.warning("RSU config file missing 'vendor' key")
        return []

    product_device = rsu_config.get('device')
    if product_device is None:
        LOG.warning("RSU config file missing 'device' key")
        return []

    additional = [(int(vid, 16), int(did, 16))
                  for (vid, did) in rsu_config.get('additional_devices', [])]

    # The sysfs nodes in the fpga class path (/sys/class/fpga)
    # are symlinks to fpga/intel.*dev.* under the sysfs tree rooted at
    # a /sys/devices/pci<segment>:<bus> node. Find all sys class nodes.
    fpga_devices = glob.glob(os.path.join(CLASS_ROOT, "*"))
    boards = []
    for dev_path in fpga_devices:
        fpga = fpga_device(dev_path)
        # read the device/vendor nodes under the device node
        vendor_id = fpga.read_node('device', 'vendor')
        device_id = fpga.read_node('device', 'device')
        if vendor_id == product_vendor and device_id == product_device:
            # The fpga device can be behind many PCIe devices.
            # example: /sys/devices/pci<segment>:<bus>/(PCI_ADDRESS_PATTERN)+
            # Read the link and parse it using the PCI_ADDRESS_PATTERN regex
            link = os.readlink(dev_path)
            match_iter = PCI_ADDRESS_RE.finditer(link)
            # After parsing it, build the path in the PCIe tree as represented
            # in the sysfs tree.
            # The first match is the root of this path and has no parent.
            # Iterate over all matches in the symlink creating a new node
            # for each match, setting the parent to the previous node.
            node = None
            path = []
            for match in match_iter:
                node = pci_node(match.groupdict(),
                                parent=node,
                                class_node=fpga)
                path.append(node)
            # the last node is the fpga node
            logging.debug('found fpga device at %s -tree is\n %s',
                          node.pci_address,
                          path[0].tree())
            if args.bus is None or args.bus == node.bus:
                boards.append(make_pac(node, fpga, other_pci=additional))

    return boards


def do_rsu(boards, args, config):
    """do_rsu

    :param boards:
    :param args:
    :param config:
    """

    # roots are the root ports of the boards
    roots = []
    # targets are the devices to remove
    targets = []

    for b in boards:
        if b.errors:
            LOG.warn('[%s] %s errors detected, skipping rsu', b.errors,
                     b.pci_node.bdf)
            continue
        node = b.pci_node
        root = node.root
        while node.parent is not root:
            node = node.parent
        targets.append(node)
        roots.append(root)

    # disable AER on root devices
    with aer_disabled(*roots):
        for (t, r, b) in zip(targets, roots, boards):
            try:
                b.rsu()
            except (NameError, AttributeError) as err:
                LOG.error('%s: could not find spi bus or spi device for %s',
                          err, b.pci_node)
                continue
            except IOError:
                time.sleep(5)
            else:
                # ignore signals for now (the signals are ignored, not
                # deferred)
                with ignore_signals(signal.SIGINT, signal.SIGHUP,
                                    signal.SIGTERM, signal.SIGABRT):
                    t.remove()
                    logging.info('waiting for FPGA reconfiguration')
                    time.sleep(10)
                    r.rescan_bus('{}:{}'.format(t.segment, t.bus))
                    time.sleep(1)


def get_update_threads(boards, args, rsu_config):
    """get_update_threads Get a list of threads to manage updating flash
       components

    :param boards: A list of discovered boards
    :param args: Command line arguments
    :param rsu_config: Parsed super-rsu manifest
    """
    flash_dir = os.path.dirname(args.rsu_config.name)
    threads = []
    for b in boards:
        update_thr = b.update(flash_dir, rsu_config, args)
        if update_thr is not None:
            threads.append(update_thr)

    if 'nvmupdate' in rsu_config:
        nvm_timeout = parse_timedelta(
            rsu_config['nvmupdate'].get('timeout', '00:10:0'))
        nvm_update_thr = update_thread(None, do_nvmupdate, args, rsu_config,
                                       name='nvmupdate', timeout=nvm_timeout)
        nvm_update_thr.start()
        threads.append(nvm_update_thr)
    return threads


def update_wait(threads, args, rsu_config):
    complete = []
    terminated = []
    if rsu_config.get('version', 1) == 1:
        timeout = args.timeout
    else:
        timeout = max([t.timeout for t in threads])
    LOG.debug('max timeout set to: %s', timedelta(seconds=timeout))
    expire = time.time() + timeout
    remaining = timeout
    interrupted = False
    try:
        while time.time() < expire and threads:
            remaining = expire - time.time()
            t = threads.pop(0)
            if not t.is_alive():
                if t.board is not None:
                    complete.append(t.board)
            else:
                threads.append(t)
            delta = timedelta(seconds=remaining)
            if delta.seconds % 60 == 0:
                LOG.debug('waiting (%s) for threads: %s', delta,
                          ', '.join(sorted([t.name for t in threads])))
            time.sleep(0.5)
    except (KeyboardInterrupt, SystemExit):
        LOG.warning('update process interrupted')
    finally:
        if threads:
            LOG.warning('timed out or interrupted,'
                        'some boards not complete: %s',
                        [t.name for t in threads])
            for t in threads:
                t.terminate()
                if t.board is not None:
                    t.board.reset_flash_mode()
                terminated.append(t.board)
            interrupted = True
    return (complete, terminated, interrupted)


def do_verify(boards, args, rsu_config):
    return [b for b in boards if b.verify(rsu_config, args)]


def do_nvmverify(boards, args, rsu_config):
    nvm_cfg = rsu_config.get('nvmupdate')
    if not nvm_cfg:
        return True

    nvmu = nvm_updater()
    if int(rsu_config.get('version', 1)) <= 1:
        vendor_id = 0x8086
        device_id = 0x0d58
        total_count = 2
    else:
        vendor_id = int(nvm_cfg['vendor'], 16)
        device_id = int(nvm_cfg['device'], 16)
        total_count = nvm_cfg.get('interfaces', 1) * len(boards)
    eth_instances = nvmu.enumerate(vendor=vendor_id, device=device_id, func=0)
    eth_instances = [eth for eth in eth_instances
                     if eth.version == nvm_cfg.get('version')]
    updated_count = len(eth_instances)

    if updated_count != total_count:
        LOG.warn('Only %s/%s ETH interfaces updated',
                 updated_count, total_count)
        return False
    return True


def do_nvmupdate(args, rsu_config, **kwargs):
    flash_dir = os.path.dirname(args.rsu_config.name)
    nvm_cfg = rsu_config.get('nvmupdate')
    if not nvm_cfg:
        return 0

    nvmu = nvm_updater()
    if rsu_config['version'] == 1:
        vendor_id = 0x8086
        device_id = 0x0d58
    else:
        vendor_id = int(nvm_cfg['vendor'], 16)
        device_id = int(nvm_cfg['device'], 16)
    eth_instances = nvmu.enumerate(vendor=vendor_id, device=device_id, func=0)
    if not eth_instances:
        LOG.warn('could not enumerate ETH interfaces')
        return (os.EX_UNAVAILABLE, os.EX_UNAVAILABLE)

    need_update = []
    for eth in eth_instances:
        if eth.version is None:
            LOG.warn('Could not get version for ETH interface: %s', eth.bus)
        elif eth.version != nvm_cfg.get('version'):
            need_update.append(eth)

    if not need_update:
        LOG.debug('no eth interfaces to update')
        return 0

    old_versions = set([item.version for item in need_update])
    nvm_file = os.path.abspath(os.path.join(flash_dir, nvm_cfg['filename']))

    # generate the nvm cfg file
    with tempfile.NamedTemporaryFile('w', delete=False) as fd:
        LOG.debug('generating nvmupdate cfg: %s', fd.name)
        fd.write(NVM_CFG_HEAD)
        replaces_str = ' '.join(old_versions)
        LOG.debug("adding old versions to nvmupdate cfg: '%s'", replaces_str)
        nvmupdate_vendor = '{:X}'.format(int(nvm_cfg['vendor'], 16))
        nvmupdate_device = '{:X}'.format(int(nvm_cfg['device'], 16))
        fd.write(NVM_CFG_DEVICE.format(EEPID=nvm_cfg['version'],
                                       REPLACES=replaces_str,
                                       FILENAME=nvm_file,
                                       VENDOR=nvmupdate_vendor,
                                       DEVICE=nvmupdate_device))
        cmd = '{} -u -c {}'.format(OPAE_NVMUPDATE_EXE, fd.name)

    task = process_task(cmd)
    p = task()
    result = p.wait()
    LOG.debug('task completed in %s', timedelta(
        seconds=time.time() - task.start_time))
    if result:
        LOG.error('nvmupdate returned non-zero exit: %s', result)
    return result


def run_tests(boards, args, rsu_config):
    results = []
    for b in boards:
        result = b.run_tests(rsu_config)
        if result:
            LOG.debug('[%s] self-test failed', b.pci_node.bdf)
        results.append(result)
    if any(results):
        return os.EX_SOFTWARE
    LOG.info('all board tests pass')
    return os.EX_OK


def need_requires(boards, flash_spec, req_type, op_str, req_version):
    ops = {'=': operator.eq,
           '==': operator.eq,
           '<=': operator.le,
           '>=': operator.ge,
           '>': operator.gt,
           '<': operator.lt,
           '!=': operator.ne}
    missing = []
    if not flash_spec.get('enabled'):
        return missing
    for b in boards:
        spec_type = flash_spec['type']
        if not b.get_flashable(spec_type).is_supported(flash_spec):
            continue
        cur = b.get_flashable(req_type)
        if cur is None or cur.is_factory:
            LOG.warn('could not get component of type: %s', req_type)
            missing.append('[{}] {}'.format(b.pci_node.bdf, req_type))
        else:
            op = ops[op_str]
            cur_version = str(cur.version)
            try:
                result = op(parse_version(cur_version),
                            parse_version(req_version))
            except TypeError:
                LOG.error('could not compare versions of different type : %s',
                          '{} {} {}'.format(cur_version, op_str, req_version))
                result = False

            if not result:
                missing.append('[{}] {} {}'.format(b.pci_node.bdf,
                                                   req_type,
                                                   req_version))
                LOG.warn('[%s] %s (%s) does not meet requirement (%s)',
                         b.pci_node.bdf, req_type, cur.version,
                         req_version)
    return missing


def check_requirements(boards, args, rsu_config):
    # check if manifest version is secure updates only
    m_version = rsu_config.get('version', 0)
    non_secure = 0
    for b in boards:
        if not b.is_secure and m_version >= SECURE_UPDATE_VERSION:
            non_secure += 1
            LOG.warn('[%s] does not support secure update',
                     b.pci_node.pci_address)
    if non_secure:
        return False

    missing = []
    callables = ['fpgaflash']
    if 'nvmupdate' in rsu_config:
        if not os.path.exists(OPAE_NVMUPDATE_EXE):
            LOG.debug("missing '%s'", NVMUPDATE_EXE)
            missing.append(NVMUPDATE_EXE)

    for exe in callables:
        if subprocess.check_call(['which', exe],
                                 stdout=subprocess.PIPE):
            LOG.debug("missing '%s'", exe)
            missing.append(exe)

    flash_dir = os.path.dirname(args.rsu_config.name)
    nvm_cfg = rsu_config.get('nvmupdate')
    # copy the flash specs
    specs = list(rsu_config['flash'])
    if nvm_cfg is not None:
        specs.append(nvm_cfg)

    for item in specs:
        args.rsu_config.name
        filename = item['filename']
        flashfile = os.path.join(flash_dir, filename)
        if not os.path.exists(flashfile) and not os.path.exists(filename):
            missing.append(filename)

        for req in item.get('requires', []):
            m = VERSION_OP_RE.match(req)
            if m is None:
                LOG.error('requires spec invalid: %s', req)
                missing.append(req)
                continue
            version = m.groupdict()['version']
            op = m.groupdict()['op']
            flash_type = m.groupdict()['type']
            missing.extend(need_requires(boards,
                                         item, flash_type, op, version))

    if missing:
        LOG.warn('missing %s', ','.join(missing))
        return False
    return True


def sighandler(signum, frame):
    raise KeyboardInterrupt('interrupt signal received')


def main():
    global DRY_RUN
    import argparse
    signal.signal(signal.SIGINT, sighandler)
    signal.signal(signal.SIGTERM, sighandler)

    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('rsu_config', type=argparse.FileType('r'),
                        help='path to config file listing what components '
                             'to update and their versions')
    parser.add_argument('--bus',
                        help=argparse.SUPPRESS)
    parser.add_argument('-n', '--dry-run',
                        default=False,
                        action='store_true',
                        help="Don't perform any updates, just a dry run")
    parser.add_argument('--log-level', choices=['trace',
                                                'debug',
                                                'error',
                                                'warn',
                                                'info',
                                                'notset'], default='info',
                        help='log level to use')
    parser.add_argument('--log-file', default=RSU_TMP_LOG,
                        help='destination logfile'
                             '- default is {}'.format(RSU_TMP_LOG))
    parser.add_argument('--rsu-only', default=False, action='store_true',
                        help='only perform the RSU command')
    parser.add_argument('--with-rsu', default=False, action='store_true',
                        help='perform RSU after updating flash components'
                             '(experimental)')
    parser.add_argument('--force-flash', default=False, action='store_true',
                        help='flash all images regardless of versions matching'
                             'or not')
    parser.add_argument('--timeout', default=3600.0, type=float,
                        help=argparse.SUPPRESS)
    parser.add_argument('--verify', default=False, action='store_true',
                        help='verify if versions on system match versions in'
                             'manifest')

    args = parser.parse_args()
    DRY_RUN = args.dry_run
    logfmt = ('[%(asctime)-15s] [%(levelname)-8s] [%(threadName)-12s] - '
              '%(message)s')
    level = TRACE if args.log_level == 'trace' else logging.NOTSET
    logging.basicConfig(format=logfmt,
                        level=getattr(logging, args.log_level.upper(), level))

    if args.log_file == RSU_TMP_LOG:
        try:
            fh = logging.handlers.RotatingFileHandler(args.log_file,
                                                      backupCount=50)
            fh.doRollover()
        except IOError:
            sys.stderr.write('Could not rollover log file: {}\n'.format(
                RSU_TMP_LOG))
            fh = None
    else:
        try:
            fh = logging.FileHandler(args.log_file)
        except IOError:
            sys.stderr.write('Could not write to logfile: %s', args.log_file)
            fh = None

    if fh is not None:
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(logging.Formatter(fmt=logfmt))
        LOG.addHandler(fh)

    if not DRY_RUN and os.getuid():
        LOG.error('must run as root')
        sys_exit(os.EX_NOPERM)

    rsu_config = json.load(args.rsu_config)
    if not rsu_config:
        logging.error('invalid rsu config: %s', args.rsu_config.name)
        sys_exit(os.EX_USAGE)
    boards = discover_boards(rsu_config, args)
    if not boards:
        logging.error('Could not find boards for given product %s',
                      rsu_config['product'])
        sys_exit(os.EX_UNAVAILABLE)

    if not check_requirements(boards, args, rsu_config):
        LOG.error('missing one or more items required by rsu config')
        sys_exit(os.EX_CONFIG)

    begin = datetime.now()

    if args.verify:
        updated = do_verify(boards, args, rsu_config)
        verified = True
        need_flash = list(set(boards) - set(updated))
        if len(need_flash):
            LOG.error('%s boards not up to date: %s',
                      len(boards) - len(updated),
                      ','.join([b.pci_node.bdf for b in need_flash]))
            verified = False
        else:
            LOG.debug('all boards up to date')
        if 'nvmupdate' in rsu_config:
            nvm_verified = do_nvmverify(boards, args, rsu_config)
        else:
            nvm_verified = True

        test_result = run_tests(boards, args, rsu_config)
        total_elapsed = datetime.now() - begin
        LOG.info('%s verification completed in: %s',
                 os.path.basename(__file__), total_elapsed)
        if not verified or not nvm_verified or test_result:
            sys_exit(os.EX_SOFTWARE)
        sys_exit(os.EX_OK)

    terminated = []
    interrupted = False
    if args.rsu_only:
        to_rsu = boards
    else:
        threads = get_update_threads(boards, args, rsu_config)
        if not threads:
            LOG.info('Nothing to update')
            to_rsu = []
        else:
            to_rsu, terminated, interrupted = update_wait(threads, args,
                                                          rsu_config)
            have_errors = any([b.errors for b in to_rsu])
            if have_errors or terminated or interrupted:
                LOG.error('not all boards updated, failing now')
                total_elapsed = datetime.now() - begin
                LOG.info('%s update completed in: %s',
                         os.path.basename(__file__),
                         total_elapsed)
                sys_exit(os.EX_SOFTWARE)

    if not args.with_rsu and not args.rsu_only:
        total_elapsed = datetime.now() - begin
        count = len(to_rsu)
        if count:
            msg = '{} board{} updated. A power-cycle is required.'.format(
                count, 's' if count > 1 else '')
        else:
            msg = 'No boards updated'
        LOG.info(msg)
        LOG.info('%s completed in: %s', os.path.basename(__file__),
                 total_elapsed)
        sys_exit(os.EX_OK, msg)

    do_rsu(to_rsu, args, rsu_config)
    to_verify = discover_boards(rsu_config, args)
    if len(to_verify) != len(to_rsu):
        LOG.error('did not rediscover same number of boards')
        total_elapsed = datetime.now() - begin
        LOG.info('%s update completed in: %s', os.path.basename(__file__),
                 total_elapsed)
        sys_exit(os.EX_SOFTWARE)

    to_test = do_verify(to_verify, args, rsu_config)
    exit_code = os.EX_OK

    if len(to_test) < len(to_rsu):
        LOG.warn('not all boards updated, testing only those that were')
        exit_code = os.EX_SOFTWARE

    test_result = run_tests(to_test, args, rsu_config)
    if test_result:
        LOG.error('board self tests failed')
        exit_code = os.EX_SOFTWARE

    if 'nvmupdate' in rsu_config:
        if not do_nvmverify(boards, args, rsu_config):
            exit_code = os.EX_SOFTWARE
    total_elapsed = datetime.now() - begin
    LOG.info('%s update completed in: %s', os.path.basename(__file__),
             total_elapsed)
    sys_exit(exit_code)


if __name__ == '__main__':
    main()
