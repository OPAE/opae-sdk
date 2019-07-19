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

"""Program PAC firmware
"""

import argparse
import os
import sys
import struct
import re
import fcntl
import array
import binascii
import string
import json
import uuid
import subprocess
import time
import signal
import logging
from opae.admin.fpga import fpga

DEFAULT_BDF = 'ssss:bb:dd.f'

VALID_GBS_GUID = uuid.UUID('31303076-5342-47b7-4147-50466e6f6558')

BLOCK0_STATIC_REGION = 0
BLOCK0_BMC = 1
BLOCK0_GBS = 2

BLOCK0_CONTYPE_MASK = 0xff

IOCTL_IFPGA_SECURE_UPDATE_START = 0xb900
IOCTL_IFPGA_SECURE_UPDATE_WRITE_BLK = 0xb901
IOCTL_IFPGA_SECURE_UPDATE_DATA_SENT = 0xb902
IOCTL_IFPGA_SECURE_UPDATE_GET_STATUS = 0xb903
IOCTL_IFPGA_SECURE_UPDATE_CANCEL = 0xb904

IFPGA_SEC_PROGRESS_TO_STR = {
    0: "IFPGA_PROG_IDLE",
    1: "IFPGA_PROG_PREPARE",
    2: "IFPGA_PROG_SLEEPING",
    3: "IFPGA_PROG_READY",
    4: "IFPGA_PROG_AUTHENTICATING",
    5: "IFPGA_PROG_COPYING",
    6: "IFPGA_PROG_UPDATE_CANCEL",
    7: "IFPGA_PROG_RSU_DONE",
    8: "IFPGA_PROG_CANCEL_DONE"
}

IFPGA_SEC_STATUS_TO_STR = {
    0: "IFPGA_STAT_NORMAL",
    1: "IFPGA_STAT_TIMEOUT",
    2: "IFPGA_STAT_AUTH_FAIL",
    3: "IFPGA_STAT_COPY_FAIL",
    4: "IFPGA_STAT_FATAL",
    5: "IFPGA_STAT_PKVL_REJECT",
    6: "IFPGA_STAT_NON_INC",
    0x80: "IFPGA_STAT_NIOS_OK",
    0x81: "IFPGA_STAT_USER_OK",
    0x82: "IFPGA_STAT_FACTORY_OK",
    0x83: "IFPGA_STAT_USER_FAIL",
    0x84: "IFPGA_STAT_FACTORY_FAIL",
    0x85: "IFPGA_STAT_NIOS_FLASH_ERR",
    0x86: "IFPGA_STAT_FPGA_FLASH_ERR"
}

LOG = logging.getLogger()


def parse_args():
    """Parses command line arguments
    """
    parser = argparse.ArgumentParser()

    file_help = 'self-describing, signed file to update PAC card.'
    parser.add_argument('file', type=argparse.FileType('rb'), help=file_help)

    bdf_help = 'bdf of device to program (e.g. 04:00.0 or 0000:04:00.0).' \
               ' Optional when one device in system.'
    parser.add_argument('bdf', nargs='?', default=DEFAULT_BDF, help=bdf_help)

    parser.add_argument('-v', '--verbose', default=False,
                        action='store_true', help='display verbose output')

    return parser.parse_args()


def decode_gbs_header(infile):
    """Reads and decodes a Green BitStream header.

    infile - a valid, open file object specified on the command line.

    Returns a dictionary of the broken out fields for the header.
    """
    orig_pos = infile.tell()

    infile.seek(0, os.SEEK_END)
    file_size = infile.tell()

    infile.seek(0, os.SEEK_SET)

    hdr_size = 20

    if file_size < hdr_size:
        LOG.warning('%s does not meet the minimum '
                    'file size requirement of %d',
                    infile.name, hdr_size)
        infile.seek(orig_pos, os.SEEK_SET)
        return None

    hdr = infile.read(hdr_size)

    # offset  size  name
    # 0x000     16  valid_gbs_guid
    # 0x010      4  metadata_length

    valid_gbs_guid = uuid.UUID(bytes=[b for b in reversed(hdr[:16])])

    if valid_gbs_guid != VALID_GBS_GUID:
        infile.seek(orig_pos, os.SEEK_SET)
        return None

    unpacked_hdr = struct.unpack_from('I'*4 + 'I', hdr)

    valid_gbs_guid_str = str(valid_gbs_guid).replace('-', '')
    valid_gbs_bytes = [valid_gbs_guid_str[i:i+2] for i in range(0, 32, 2)]
    pretty_valid_gbs = ''
    for a_byte in reversed(valid_gbs_bytes):
        a_char = chr(int(a_byte, 16))
        if a_char in string.printable:
            pretty_valid_gbs = pretty_valid_gbs + a_char
        else:
            pretty_valid_gbs = pretty_valid_gbs + '?'

    LOG.debug('GBS guid: %s', valid_gbs_guid)
    LOG.debug('pretty GBS guid: %s', pretty_valid_gbs)

    metadata_length = unpacked_hdr[4]
    hdr_size += metadata_length

    if file_size < hdr_size:
        LOG.warning('%s does not meet the minimum '
                    'file size requirement of %d',
                    infile.name, hdr_size)
        infile.seek(orig_pos, os.SEEK_SET)
        return None

    metadata = infile.read(metadata_length)
    LOG.debug('metadata: %s', metadata)

    try:
        json_obj = json.loads(metadata)
    except ValueError as exc:
        LOG.info(exc)
        infile.seek(orig_pos, os.SEEK_SET)
        return None

    if 'afu-image' not in json_obj:
        LOG.error('No "afu-image" key in JSON')
        infile.seek(orig_pos, os.SEEK_SET)
        return None

    afu_image = json_obj['afu-image']

    if 'interface-uuid' not in afu_image:
        LOG.error('No "interface-uuid" key in JSON')
        infile.seek(orig_pos, os.SEEK_SET)
        return None

    interface_uuid = afu_image['interface-uuid']
    LOG.debug('interface-uuid: %s', interface_uuid)

    return {'valid_gbs_guid': valid_gbs_guid,
            'pretty_gbs_guid': pretty_valid_gbs,
            'metadata': metadata,
            'interface-uuid': interface_uuid}


def do_partial_reconf(addr, filename):
    """Call out to fpgaconf for Partial Reconfiguration.

    addr - the canonical ssss:bb:dd.f of the device to PR.
    filename - the GBS file path.

    returns a 2-tuple of the process exit status and a message.
    """
    conf_args = ['fpgaconf', '--segment', '0x' + addr[:4],
                 '--bus', '0x' + addr[5:7],
                 '--device', '0x' + addr[8:10],
                 '--function', '0x' + addr[11:], filename]

    LOG.debug('command: %s', ' '.join(conf_args))

    try:
        output = subprocess.check_output(conf_args)
    except subprocess.CalledProcessError as exc:
        return (exc.returncode,
                exc.output + '\nPartial Reconfiguration failed')

    return (0, output + '\nPartial Reconfiguration OK')


def decode_auth_block0(infile):
    """Reads and decodes an authentication block 0.

    infile - a valid, open file object specified on the command line.

    Returns a dictionary of the broken out fields for block0.
    """
    orig_pos = infile.tell()

    infile.seek(0, os.SEEK_END)
    file_size = infile.tell()

    block0_size = 128
    block0_magic = 0xb6eafd19

    #                          block1 payload
    min_file_size = block0_size + 896 + 128

    if file_size < min_file_size:
        LOG.warning('%s does not meet the minimum file size '
                    'requirement of %d',
                    infile.name, min_file_size)
        infile.seek(orig_pos, os.SEEK_SET)
        return None

    infile.seek(0, os.SEEK_SET)

    block0 = infile.read(block0_size)
    infile.seek(orig_pos, os.SEEK_SET)

    # offset  size  name
    # 0x000      4  Magic
    # 0x004      4  ConLen
    # 0x008      4  ConType
    # 0x00c      4  Reserved
    # 0x010     32  Hash256
    # 0x030     48  Hash384
    # 0x060     32  Reserved

    unpacked_block0 = struct.unpack_from('I'*4 + '32s' + '48s', block0)

    if unpacked_block0[0] != block0_magic:
        return None

    h256 = binascii.hexlify(unpacked_block0[4])
    h384 = binascii.hexlify(unpacked_block0[5])

    infile.seek(0, os.SEEK_SET)

    res = {'Magic': unpacked_block0[0],
           'ConLen': unpacked_block0[1],
           'ConType': unpacked_block0[2],
           'Hash256': h256,
           'Hash384': h384}

    LOG.debug('hash256: %s', res['Hash256'])
    LOG.debug('hash384: %s', res['Hash384'])

    contype = res['ConType'] & BLOCK0_CONTYPE_MASK

    if contype == BLOCK0_STATIC_REGION:
        LOG.debug('file type: Static Region')
    elif contype == BLOCK0_BMC:
        LOG.debug('file type: BMC Image')
    elif contype == BLOCK0_GBS:
        LOG.debug('file type: Green BitStream')

    return res


def canonicalize_bdf(bdf):
    """Verifies the given PCIe address.

    bdf - a string representing the PCIe address. It must be of
          the form bb:dd.f or ssss:bb:dd.f.

    returns None if bdf does not have the proper form. Otherwise
    returns the canonical form as a string.
    """
    abbrev_pcie_addr_pattern = r'(?P<bus>[\da-f]{2}):' \
                               r'(?P<device>[\da-f]{2})\.' \
                               r'(?P<function>\d)'
    pcie_addr_pattern = r'(?P<segment>[\da-f]{4}):' + abbrev_pcie_addr_pattern

    abbrev_regex = re.compile(abbrev_pcie_addr_pattern, re.IGNORECASE)

    match = abbrev_regex.match(bdf)
    if match:
        return '0000:' + bdf
    else:
        regex = re.compile(pcie_addr_pattern, re.IGNORECASE)
        match = regex.match(bdf)
        if match:
            return bdf

    return None


def fw_update_status(fd_dev):
    """Issues the IOCTL_IFPGA_SECURE_UPDATE_GET_STATUS and returns the result

    fd_dev - an integer file descriptor to the os.open()'ed secure
             device file.

    returns a 2-tuple of the progress and status as strings.
    """
    buf = array.array('B', '\x00' * 32)
    sizeof_ifpga_secure_status = 16

    # offset size name
    # 0x000     4 argsz
    # 0x004     4 flags
    # 0x008     4 progress
    # 0x00c     4 status

    # set argsz and flags
    struct.pack_into('II', buf, 0, sizeof_ifpga_secure_status, 0)

    fcntl.ioctl(fd_dev, IOCTL_IFPGA_SECURE_UPDATE_GET_STATUS,
                buf, True)

    _, _, progress, status = struct.unpack_from('IIII', buf)

    prog_str = '<unknown>' if progress not in IFPGA_SEC_PROGRESS_TO_STR \
               else IFPGA_SEC_PROGRESS_TO_STR[progress]

    stat_str = '<unknown>' if status not in IFPGA_SEC_STATUS_TO_STR \
               else IFPGA_SEC_STATUS_TO_STR[status]

    LOG.debug('%s / %s', prog_str, stat_str)

    return (prog_str, stat_str)


def fw_write_block(fd_dev, offset, size, addr):
    """Write firmware block to staging area.

    fd_dev - an integer file descriptor to the os.open()'ed secure
             device file.
    offset - the offset into the staging area to write.
    size - the size of the memory buffer in bytes.
    addr - the user virtual address of the buffer.
    """
    buf = array.array('B', '\x00' * 32)
    sizeof_ifpga_secure_write = 24

    # offset size name
    # 0x000     4 argsz
    # 0x004     4 flags
    # 0x008     4 offset
    # 0x00c     4 size
    # 0x010     8 buf

    struct.pack_into('IIIIQ', buf, 0, sizeof_ifpga_secure_write, 0,
                     offset, size, addr)

    fcntl.ioctl(fd_dev, IOCTL_IFPGA_SECURE_UPDATE_WRITE_BLK,
                buf, True)


class SecureUpdateError(Exception):
    """Secure update exception
    """
    def __init__(self, arg):
        super(SecureUpdateError, self).__init__(self)
        self.errno, self.strerror = arg


def update_fw(fd_dev, infile):
    """Writes firmware to secure device.

    fd_dev - an integer file descriptor to the os.open()'ed secure
             device file.

    infile - an open file object whose current position is set to
             the beginning of the payload to send to the device.

    returns a 2-tuple of the process exit status and a message.
    """

    def poll(fd_dev, interval=0.1,
             while_prog=None, until_prog=None,
             while_stat=None, until_stat=None,
             exc_prog=None, exc_stat=None):
        """Poll on secure update status
        """
        while True:
            p_s = fw_update_status(fd_dev)

            if exc_prog and p_s[0] in exc_prog:
                LOG.debug(
                    'except poll [ %s in %s ]',
                    p_s[0], str(exc_prog))
                raise SecureUpdateError(
                    (1, 'Secure update failed: {}'.format(p_s)))

            if exc_stat and p_s[1] in exc_stat:
                LOG.debug(
                    'except poll [ %s in %s ]',
                    p_s[1], str(exc_stat))
                raise SecureUpdateError(
                    (1, 'Secure update failed: {}'.format(p_s)))

            if while_prog and p_s[0] not in while_prog:
                LOG.debug(
                    'break poll [ %s not in %s ]',
                    p_s[0], str(while_prog))
                break

            if until_prog and p_s[0] in until_prog:
                LOG.debug(
                    'break poll [ %s in %s ]',
                    p_s[0], str(until_prog))
                break

            if while_stat and p_s[1] not in while_stat:
                LOG.debug(
                    'break poll [ %s not in %s ]',
                    p_s[1], str(while_stat))
                break

            if until_stat and p_s[1] in until_stat:
                LOG.debug(
                    'break poll [ %s in %s ]',
                    p_s[1], str(until_stat))
                break

            time.sleep(interval)

        return p_s

    error_status = ["IFPGA_STAT_TIMEOUT",
                    "IFPGA_STAT_AUTH_FAIL",
                    "IFPGA_STAT_COPY_FAIL",
                    "IFPGA_STAT_FATAL",
                    "IFPGA_STAT_PKVL_REJECT",
                    "IFPGA_STAT_NON_INC",
                    "IFPGA_STAT_USER_FAIL",
                    "IFPGA_STAT_FACTORY_FAIL",
                    "IFPGA_STAT_NIOS_FLASH_ERR",
                    "IFPGA_STAT_FPGA_FLASH_ERR"]

    not_normal_status = ["IFPGA_STAT_TIMEOUT",
                         "IFPGA_STAT_AUTH_FAIL",
                         "IFPGA_STAT_COPY_FAIL",
                         "IFPGA_STAT_FATAL",
                         "IFPGA_STAT_PKVL_REJECT",
                         "IFPGA_STAT_NON_INC",
                         "IFPGA_STAT_NIOS_OK",
                         "IFPGA_STAT_USER_OK",
                         "IFPGA_STAT_FACTORY_OK",
                         "IFPGA_STAT_USER_FAIL",
                         "IFPGA_STAT_FACTORY_FAIL",
                         "IFPGA_STAT_NIOS_FLASH_ERR",
                         "IFPGA_STAT_FPGA_FLASH_ERR"]
    block_size = 4096
    offset = 0

    orig_pos = infile.tell()
    infile.seek(0, os.SEEK_END)
    payload_size = infile.tell() - orig_pos
    infile.seek(orig_pos, os.SEEK_SET)

    LOG.debug('payload size: %d', payload_size)

    try:
        fcntl.ioctl(fd_dev, IOCTL_IFPGA_SECURE_UPDATE_START)
    except IOError as exc:
        return exc.errno, exc.strerror
    LOG.debug('IOCTL ==> SECURE_UPDATE_START')

    poll(fd_dev, interval=0.1,
         until_prog=["IFPGA_PROG_READY"],
         exc_stat=error_status)

    poll(fd_dev, interval=0.1,
         while_prog=["IFPGA_PROG_PREPARE"],
         exc_stat=error_status)

    to_transfer = block_size if block_size <= payload_size \
        else payload_size

    while to_transfer:
        buf = array.array('B')
        buf.fromfile(infile, to_transfer)

        buf_addr, buf_len = buf.buffer_info()
        LOG.debug('buf virt: 0x{:x} offset: {}'.format(buf_addr, offset))

        if buf_len != to_transfer:
            to_transfer = buf_len

        try:
            fw_write_block(fd_dev, offset, to_transfer, buf_addr)
        except IOError as exc:
            return exc.errno, exc.strerror
        LOG.debug('IOCTL ==> SECURE_UPDATE_WRITE_BLK')

        payload_size -= to_transfer
        offset += to_transfer
        to_transfer = block_size if block_size <= payload_size \
            else payload_size

        poll(fd_dev, interval=0.01,
             until_prog=["IFPGA_PROG_READY"],
             exc_prog=["IFPGA_PROG_IDLE"],
             exc_stat=not_normal_status)

    try:
        fcntl.ioctl(fd_dev, IOCTL_IFPGA_SECURE_UPDATE_DATA_SENT)
    except IOError as exc:
        return exc.errno, exc.strerror
    LOG.debug('IOCTL ==> SECURE_UPDATE_DATA_SENT')

    poll(fd_dev, interval=0.1,
         until_prog=["IFPGA_PROG_READY"],
         exc_prog=["IFPGA_PROG_IDLE"],
         exc_stat=not_normal_status)

    poll(fd_dev, interval=0.1,
         while_prog=["IFPGA_PROG_READY"],
         exc_prog=["IFPGA_PROG_IDLE"],
         exc_stat=not_normal_status)

    poll(fd_dev, interval=0.1,
         while_prog=["IFPGA_PROG_AUTHENTICATING"],
         exc_prog=["IFPGA_PROG_IDLE"],
         exc_stat=not_normal_status)

    poll(fd_dev, interval=0.1,
         while_prog=["IFPGA_PROG_COPYING"],
         exc_prog=["IFPGA_PROG_IDLE"],
         exc_stat=not_normal_status)

    poll(fd_dev, interval=0.1,
         while_prog=["IFPGA_PROG_RSU_DONE"],
         exc_prog=["IFPGA_PROG_IDLE"],
         exc_stat=not_normal_status)

    return 0, 'Secure update OK'


def sig_handler(signum, frame):
    """raise exception for SIGTERM
    """
    raise SecureUpdateError((1, 'Caught SIGTERM'))


def main():
    args = parse_args()

    LOG.setLevel(logging.NOTSET)
    log_fmt = ('[%(asctime)-15s] [%(levelname)-8s] '
               '%(message)s')
    log_hndlr = logging.StreamHandler(sys.stdout)
    log_hndlr.setFormatter(logging.Formatter(log_fmt))

    if args.verbose:
        log_hndlr.setLevel(logging.DEBUG)
    else:
        log_hndlr.setLevel(logging.INFO)

    LOG.addHandler(log_hndlr)

    signal.signal(signal.SIGTERM, sig_handler)

    LOG.debug('fw file: %s', args.file.name)
    LOG.debug('addr: %s', args.bdf)

    stat = 1
    mesg = 'Secure update failed'
    gbs_hdr = None

    blk0 = decode_auth_block0(args.file)
    if blk0 is None:
        gbs_hdr = decode_gbs_header(args.file)
        if gbs_hdr is None:
            LOG.error('Unknown file format in %s', args.file.name)
            sys.exit(1)

    pac = None
    if args.bdf == DEFAULT_BDF:
        # Address wasn't specified. Enumerate all PAC's.
        if gbs_hdr:
            pr_guid = gbs_hdr['interface-uuid'].replace('-', '')
            pacs = fpga.enum([{'fme.pr_interface_id': pr_guid}])
        else:
            pacs = fpga.enum()

        if not pacs:
            LOG.error('No suitable PAC found.')
            sys.exit(1)
        elif len(pacs) > 1:
            LOG.error('More than one PAC found. '
                      'Please specify an %s', DEFAULT_BDF)
            sys.exit(1)
        pac = pacs[0]
    else:
        # Enumerate for a specific device.
        canon_bdf = canonicalize_bdf(args.bdf)
        if not canon_bdf:
            LOG.error('%s is not a valid PCIe address. '
                      'Use %s', args.bdf, DEFAULT_BDF)
            sys.exit(1)

        pacs = fpga.enum([{'pci_node.pci_address': canon_bdf}])
        if not pacs:
            LOG.error('Failed to find PAC at %s', canon_bdf)
            sys.exit(1)

        pac = pacs[0]

        if gbs_hdr:
            # verify the PR interface uuid
            fme = pac.fme
            if fme:
                pr_guid = gbs_hdr['interface-uuid'].replace('-', '')
                if fme.pr_interface_id.replace('-', '') != pr_guid:
                    LOG.error('PR interface uuid mismatch.')
                    sys.exit(1)

    if gbs_hdr is not None:
        args.file.close()
        stat, mesg = do_partial_reconf(pac.pci_node.pci_address,
                                       args.file.name)
    elif blk0 is not None:
        sec_dev = pac.secure_dev
        if not sec_dev:
            LOG.error('Failed to find secure '
                      'device for PAC %s', pac.pci_node.pci_address)
            sys.exit(1)

        LOG.debug('Found secure device for PAC '
                  '%s : %s', pac.pci_node.pci_address, sec_dev.devpath)

        try:
            with sec_dev as descr:
                stat, mesg = update_fw(descr, args.file)
        except SecureUpdateError as exc:
            stat, mesg = exc.errno, exc.strerror
        except KeyboardInterrupt as exc:
            with sec_dev as descr:
                fcntl.ioctl(descr, IOCTL_IFPGA_SECURE_UPDATE_CANCEL)
            stat, mesg = 1, 'Interrupted'

    if stat:
        LOG.error(mesg)
    else:
        LOG.info(mesg)
    sys.exit(stat)


if __name__ == '__main__':
    main()
