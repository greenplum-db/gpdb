"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

import os
import tinctest
import unittest2 as unittest
from tinctest.lib import Gpdiff
from tinctest.lib import local_path, run_shell_command
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.lib.gppkg.gppkg import Gppkg

cmd = 'gpssh --version'
res = {'rc':0, 'stderr':'', 'stdout':''}
run_shell_command (cmd, 'check product version', res)
product_version = res['stdout'].split('gpssh version ')[1].split(' build ')[0]

class SpiMPPTestCase(MPPTestCase):

    def __init__(self, methodName):
        self.dbname = os.environ.get('PGDATABASE')
        super(SpiMPPTestCase, self).__init__(methodName)

    @classmethod
    def setUpClass(self):
        super(SpiMPPTestCase, self).setUpClass()
        gppkg = Gppkg()
        gppkg.gppkg_install(product_version, 'plperl')
        setup_file = local_path('setup.sql')
        PSQL.run_sql_file(setup_file)

    def test_01(self):
        "SPI: plpgsql"
        sql_file = local_path('query01.sql')
        out_file = local_path('query01.out') 
        ans_file = local_path('query01.ans')
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))

    def test_02(self):
        "SPI: plperl"
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command('psql -d %s -c \'create language plperl;\''%self.dbname, 'check if plperl.so exists', res)
        if res['rc']:
            stderr = res['stderr'].strip().split('\n')
            for line in stderr:
                # simple return if plperl.so didn't exist
                if line.find('No such file or directory') != -1:
                    tinctest.logger.info("plperl.so not found, skipping test!")
                    return
        sql_file = local_path('query02.sql')
        out_file = local_path('query02.out')
        ans_file = local_path('query02.ans')
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))

    def test_03(self):
        "SPI: cursor"
        optimizer_status = self.check_orca_status()
        if optimizer_status:
            ans_file = local_path('query03.ans.orca')
        else:
            ans_file = local_path('query03.ans')
        sql_file = local_path('query03.sql')
        out_file = local_path('query03.out')
        PSQL.run_sql_file(sql_file = sql_file, out_file = out_file)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))

    def check_orca_status(self):
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command('gpconfig -s optimizer', 'check if orca on or off', res)
        lines = res['stdout'].strip().split('\n')
        if 'Master  value: on' in lines and 'Segment value: on' in lines:
            return True
        else:
            return False
