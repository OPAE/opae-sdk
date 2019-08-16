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
import tempfile
import time
import sys
import unittest
import io

from opae.admin.utils.progress import progress

if sys.version_info[0] == 3:
    temp_cfg = {'encoding': 'utf-8'}
else:
    temp_cfg = {}

class test_progress(unittest.TestCase):
    TEST_SIZE = 4096*100

    def test_notty(self):
        """test_notty
           Given the progress object time is set to a valid time and out stream
           The tool runs successfully with output stream to stdout
        """
        with tempfile.NamedTemporaryFile(mode='w+',
                                         **temp_cfg) as stream:
            with progress(time=10, stream=stream) as prg:
                for _ in range(100):
                    prg.tick()
                    time.sleep(0.1)
            stream.seek(0)
            print(stream.read())

    def test_time0(self):
        """test_time0
           Given the progress object time is set to a valid time
           The tool runs successfully with output stream to stdout
        """
        with progress(time=0) as prg:
            for _ in range(10):
                prg.tick()
                time.sleep(0.1)

    def test_noop(self):
        """test_noop
           Given the progress object null is set to True
           The progress reporting tool becomes a 'noop' method
        """
        with progress(time=0, null=True) as prg:
            for _ in range(10):
                prg.tick()
                time.sleep(0.1)

    def test_stream_readonly(self):
        """test_stream_readonly
           Given the progress object with read-only stream
           The tool prints Error but runs successfully and redirect stream to stdout
        """
        stream = io.IOBase()
        with progress(time=10, stream=stream) as prg:
            for _ in range(100):
                prg.tick()
                time.sleep(0.1)

    def test_time_set_none(self):
        """test_time_set_none
           Given the progress object time is set to none
           The tool fails with RunTimeError
        """
        with progress(time=None) as prg:
            for _ in range(10):
                with self.assertRaises(RuntimeError):
                    prg.tick()
                    time.sleep(0.1)

    def test_bytes_set_none(self):
        """test_bytes_set_none
           Given the progress object bytes is set to none
           The tool fails with RunTimeError
        """
        with progress(bytes=None) as prg:
            for _ in range(10):
                with self.assertRaises(RuntimeError):
                    prg.update(self.TEST_SIZE)
                    time.sleep(0.1)
