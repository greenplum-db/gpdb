#!/usr/bin/env python
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

@summary: Test class for installing pgcrypto in a given GPDB system
"""

import os
import sys
import tinctest
from tinctest.lib import run_shell_command
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from mpp.lib.gppkg.gppkg import Gppkg

class pgcrypto_installgppkg(MPPTestCase):
    
    def __init__(self, methodName):
        super(pgcrypto_installgppkg, self).__init__(methodName)

    @classmethod
    def setUpClass(self):
        """
        @summary: check if current OS is supported for gppkg.  If not, test is skipped.
        
        """
        cmd = 'gpssh --version'
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command (cmd, 'check product version', res)
        gppkg = Gppkg()
        product_version = res['stdout']
        gppkg.gppkg_install(product_version, 'pgcrypto')

    def test_zzz_functionsetup(self):
        """ run $GPHOME/share/postgresql/contrib/pgcrypto.sql to create all pgcrypto functions """
        # check if sql file exist
        gphome = os.environ.get('GPHOME')
        file_loc = os.path.join(gphome, 'share/postgresql/contrib/pgcrypto.sql')
        if not os.path.exists(file_loc):
            self.skipTest("file does not exist: $GPHOME/share/postgresql/contrib/pgcrypto.sql")
        else:
            PSQL.run_sql_file(file_loc, dbname=os.environ.get('PGDATABASE'))
