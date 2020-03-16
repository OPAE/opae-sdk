#!/usr/bin/env python3

from __future__ import absolute_import
import argparse
from array import array


def reverse_bits(x, n):
    result = 0
    for i in range(n):
        if (x >> i) & 1:
            result |= 1 << (n - 1 - i)
    return result


def reverse_bits_in_file(ifile, ofile):
    bit_rev = array('B')
    for i in range(0, 256):
        bit_rev.append(reverse_bits(i, 8))

    while True:
        ichunk = ifile.read(4096)
        if not ichunk:
            break

        if isinstance(ichunk, str):
            ochunk = ''
            for b in ichunk:
                ochunk += chr(bit_rev[ord(b)])
            ofile.write(ochunk)
        elif isinstance(ichunk, bytes):
            ochunk = []
            for b in ichunk:
                ochunk.append(bit_rev[b])
            ofile.write(bytes(ochunk))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('ifile', type=argparse.FileType('rb'),
                        help='input file to be reversed')
    parser.add_argument('ofile', type=argparse.FileType('wb'),
                        help='output file')

    args = parser.parse_args()

    reverse_bits_in_file(args.ifile, args.ofile)


if __name__ == '__main__':
    main()
