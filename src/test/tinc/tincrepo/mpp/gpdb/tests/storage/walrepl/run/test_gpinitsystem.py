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

import unittest2 as unittest
from mpp.models import MPPTestCase
from tinctest.lib import local_path

from gppylib.commands.base import Command
from mpp.lib.config import GPDBConfig

import os
import shutil

config = GPDBConfig()
class gpinitsystem(MPPTestCase):

    standby_dir = '/tmp/gpdata'
    standby_masterdir = os.path.join(standby_dir, 'gpseg-1')
    standby_port = 1112

    def setUp(self):
        cmd = Command('gpstop', 'gpstop -ad ' + self.standby_masterdir)
        cmd.run()
        shutil.rmtree(self.standby_dir, True)
        os.makedirs(self.standby_dir)
        hosts_path = os.path.join(self.standby_dir, 'hosts')
        cmd = Command('create hosts', 'hostname > ' + hosts_path)
        cmd.run(validateAfter=True)

    def tearDown(self):
        cmd = Command('gpstop', 'gpstop -ad ' + self.standby_masterdir)
        cmd.run()

    @unittest.skipIf(config.is_multinode(), "Test applies only to a singlenode cluster")
    def test_create_local(self):
        """
        Initialize a temporary GP cluster with a standby locally running.
        """

        init_file = local_path('gp_init_config')
        port = self.standby_port
        filespaces = 'pg_system:{0}/gpseg-standby'.format(self.standby_dir)
        cmd_str = 'gpinitsystem -ac {0} -P {1} -F {2}'.format(
                        init_file, port, filespaces)
        cmd = Command('gpinitsystem', cmd_str)
        cmd.run()
        self.assertIn(cmd.get_results().rc, (0, 1))
