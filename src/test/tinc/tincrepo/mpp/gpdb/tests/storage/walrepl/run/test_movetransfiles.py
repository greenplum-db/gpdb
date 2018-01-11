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

from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from mpp.lib.gpfilespace import Gpfilespace, GPfilespaceException

from gppylib.commands.base import Command
from gppylib.gparray import GpArray

import os
import shutil
import socket

class MoveTransFiles(MPPTestCase):
    transfiles_are_moved = False
    tempfiles_are_moved = False

    fsname = 'temp_fs'
    fsdict = {'pg_system': '/tmp/standby', fsname: '/tmp/standby_fs'}

    def setUp(self):
        self.remove_standby(validateAfter=False)

    def tearDown(self):

        # Remove standby first.
        self.remove_standby(validateAfter=False)

        # If we moved trans/tempfilespace, reset it before dropping
        # user filespace.
        gpfilespace = Gpfilespace()
        if gpfilespace.exists(self.fsname):
            if self.transfiles_are_moved:
                gpfilespace.move_transdefault()
            if self.tempfiles_are_moved:
                gpfilespace.move_tempdefault()
            PSQL.run_sql_command('drop filespace temp_fs')

    def format_filespace_arg(self, fsdict):
        """
        Create -F format for gpinitstandby.  The fsdict should look like
        {'pg_system': '/path/to/standby'}
        """

        buf = []
        for key, val in fsdict.items():
            buf.append('{fsname}:{fslocation}'.format(fsname=key,
                                                      fslocation=val))
        return ','.join(buf)

    def create_local_standby(self, fsdict, validateAfter=True):
        """
        Create local standby, we should clean dir in fsdict first
        """

        port = os.environ.get('PGPORT', 5432)
        port = str(int(port) + 1)
        for key, val in fsdict.items():
            shutil.rmtree(val, True)
        filespace = self.format_filespace_arg(fsdict)
        hostname = socket.gethostname()
        cmd_str = ' '.join(['gpinitstandby', '-a',
                            '-s', hostname,
                            '-P', port,
                            '-F', filespace])
        gpinitstandby = Command('gpinitstandby', cmd_str)
        gpinitstandby.run(validateAfter=validateAfter)
        return gpinitstandby

    def remove_standby(self, validateAfter=True):
        """
        Remove standby.  It's ok to run this even if standby is not configured.
        """

        cmd_str = ' '.join(['gpinitstandby', '-a', '-r'])
        gpinitstandby = Command('gpinitstandby', cmd_str)
        gpinitstandby.run(validateAfter=validateAfter)
        return gpinitstandby

    def test_standby_is_configured(self):
        """
        Create standby then move transfilespace.
        """

        gpfilespace = Gpfilespace()
        gpfilespace.create_filespace(self.fsname)
        cmd = self.create_local_standby(self.fsdict)
        self.assertIn(cmd.get_results().rc, (0, 1), 'could not create standby')

        gpfilespace.movetransfiles_localfilespace(self.fsname)

        self.transfiles_are_moved = True

    def test_transfiles_are_moved(self):
        """
        Move transfilespace then create standby.
        """

        gpfilespace = Gpfilespace()
        gpfilespace.create_filespace(self.fsname)
        gpfilespace.movetransfiles_localfilespace(self.fsname)
        self.transfiles_are_moved = True

        cmd = self.create_local_standby(self.fsdict)
        self.assertIn(cmd.get_results().rc, (0, 1),
                      'could not create standby with movetransfilespace')
        cmd = self.remove_standby()
        self.assertIn(cmd.get_results().rc, (0, 1),
                      'could not remove standby with movetransfilespace')

    def test_tempfiles_are_moved(self):
        """
        Move tempfilespace then create standby.
        """

        gpfilespace = Gpfilespace()
        gpfilespace.create_filespace(self.fsname)
        gpfilespace.movetempfiles_localfilespace(self.fsname)
        self.tempfiles_are_moved = True

        cmd = self.create_local_standby(self.fsdict)
        self.assertIn(cmd.get_results().rc, (0, 1),
                      'could not create standby with movetempfilespace')
        cmd = self.remove_standby()
        self.assertIn(cmd.get_results().rc, (0, 1),
                      'could not remove standby with movetempfilespace')
