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
import tinctest

from tinctest.lib import local_path
from tinctest import TINCTestCase

from mpp.lib.PSQL import PSQL

from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from gppylib.db import dbconn


class Verification(TINCTestCase):

    dbname = 'mpp18816_db'
    @classmethod
    def setUpClass(cls):
        tinctest.logger.info('Running Verification...')

    def run_SQLQuery(self, exec_sql, dbname = 'template1'):
        with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
            curs = dbconn.execSQL(conn, exec_sql)
            results = curs.fetchall()
        return results

    def test_checklog(self):
        ''' Select from gp_toolkit log message to see if the concurrent test run resulted in PANIC messages'''
        log_sql = "select logseverity, logstate, substring(logmessage from 0 for 60) from gp_toolkit.__gp_log_master_ext where logmessage \
                   like '%Unrecognized DTX transaction context%' or logmessage like '%proclock table corrupted%' or logseverity = 'PANIC' ;"
        result = self.run_SQLQuery(log_sql, dbname = Verification.dbname)
        for (logsev, logstate, logmsg) in result:
            if (logsev.strip() == 'PANIC' or 'Unrecognized DTX transaction context' in logmsg or 'proclock table corrupted' in logmsg ):
                raise Exception('Master log shows PANIC or other error messages: Please check the master_log')
        tinctest.logger.info('No PANIC messages found in logs')
    

    def test_gpcheckcat(self):
        dbstate = DbStateClass('run_validation')
        dbstate.check_catalog(alldb = False, dbname = Verification.dbname)
        
    
