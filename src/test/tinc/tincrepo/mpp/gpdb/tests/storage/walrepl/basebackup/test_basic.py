"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from mpp.models import MPPTestCase

import os
import shutil
import subprocess
import unittest2 as unittest

from tinctest.lib import run_shell_command

class simple(MPPTestCase):
    DESTDIR = 'base'

    @classmethod
    def setUpClass(cls):
        super(simple, cls).setUpClass()
        # Removing standby as these tests will fail if there is a standby enabled
        # Ignore any failures
        run_shell_command(cmdstr='gpinitstandby -ra', cmdname='remove standby')
        

    def check_dir(self, name, dir=DESTDIR):
        return os.path.isdir(os.path.join(dir, name))

    def setUp(self):
        shutil.rmtree(simple.DESTDIR, True)

    def test_simple(self):
        """
        @tags sanity
        """ 

        subprocess.check_call(['pg_basebackup', '-D', simple.DESTDIR])
        self.assertTrue(self.check_dir('pg_xlog'))

    def test_includewal(self):
        """
        @tags sanity
        """

        subprocess.check_call(['pg_basebackup', '-x', '-D', simple.DESTDIR])
        self.assertTrue(self.check_dir('pg_xlog'))

    @unittest.skip('max_walsender is now 1.  -X option does not work')
    def test_stream(self):
        subprocess.check_call(['pg_basebackup', '-X', 'stream', '-D', simple.DESTDIR])
        self.assertTrue(self.check_dir('pg_xlog'))

    def test_recoveryconf(self):
        """
        @tags sanity
        """

        subprocess.check_call(['pg_basebackup', '-x', '-R', '-D', simple.DESTDIR])
        recovery_conf = os.path.join(simple.DESTDIR, 'recovery.conf')
        self.assertTrue(os.path.exists(recovery_conf))

    def test_back_to_masterdd(self):
        """
        @tags sanity
        """

        dest_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        with self.assertRaises(subprocess.CalledProcessError) as c:
             subprocess.check_call(['pg_basebackup', '-x', '-D', dest_dir])
        self.assertEqual(c.exception.returncode, 1)
        self.assertTrue(self.check_dir(dest_dir,'pg_xlog'))

    def test_exclude_option(self):
        """
        @tags sanity
        """

        subprocess.check_call(['pg_basebackup', '-E', './pg_log', '-D',
                               simple.DESTDIR])
        pg_log = os.path.join(simple.DESTDIR, 'pg_log')
        self.assertFalse(os.path.exists(pg_log))
