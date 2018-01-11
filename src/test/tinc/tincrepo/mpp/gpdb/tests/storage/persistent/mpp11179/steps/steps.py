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

import os
from time import sleep

from tinctest.lib import run_shell_command

import tinctest
import unittest2 as unittest
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.gpdb.tests.storage.lib import Database
from tinctest.lib import local_path
from tinctest import TINCTestCase

from mpp.lib.PSQL import PSQL

class Steps(TINCTestCase):

    dbname = 'mpp11179_db'
    @classmethod
    def setUpClass(cls):
        tinctest.logger.info('Running Verification...')

    def test_runsql(self):
        tinctest.logger.info('Begin a transaction')
        cmd_str = 'gpfaultinjector -p %s -m async -s 1 -f dtm_broadcast_prepare -y reset -o 0' % os.getenv('PGPORT')
        results={'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command(cmd_str, results=results)


        PSQL.run_sql_file(local_path('drop_table.sql'), dbname=Steps.dbname)

        cmd_str = 'gpfaultinjector -p %s -m async -s 1 -f dtm_broadcast_prepare -y suspend -o 0' % os.getenv('PGPORT')
        results={'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command(cmd_str, results=results)
        if int(results['rc']) !=0:
            raise Exception('gpfaultinjector failed to suspend the fault')
        PSQL.run_sql_file(local_path('create_table.sql'), dbname=Steps.dbname)

    def test_checkpoint(self):
        tinctest.logger.info('Issue Checkpoint')
        PSQL.run_sql_file(local_path('checkpoint.sql'), dbname=Steps.dbname)

    def test_gprestart(self):
        tinctest.logger.info('Restart database after immediate shutdown')
        sleep(20)
        cmd_str = 'source %s/greenplum_path.sh;%s/bin/gpstop -air'% (os.environ['GPHOME'], os.environ['GPHOME'])
        results={'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command(cmd_str, results=results)
        if int(results['rc']) !=0:
            raise Exception('Gp-Restart failed')

    def test_gpcheckcat(self):
        tinctest.logger.info('Run Checkcat to verify persistent table consistency')
        dbstate = DbStateClass('run_validation')
        dbstate.check_catalog(alldb = False, dbname = Steps.dbname)

             
            
