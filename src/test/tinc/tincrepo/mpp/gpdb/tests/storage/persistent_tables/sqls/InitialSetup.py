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
import fnmatch
import tinctest

from tinctest.lib import local_path
from mpp.gpdb.tests.storage.persistent_tables.sqls.generate_sqls import GenerateSqls
from mpp.lib.PSQL import PSQL

'''
Creates and runs the pre-requisite SQL files before the actual Load starts
'''

class InitialSetup():

    def createSQLFiles(self):
        tinctest.logger.info('Creating the SQL files under setup folder')
        schema = GenerateSqls()
        table_types = ('ao', 'co', 'heap')
        for table_type in table_types:
            schema.create_table_setup('table',table_type,table_type)
            schema.create_table_setup('insert_tb',table_type,table_type)
            schema.create_table_setup('insert_tb',table_type + '_part',table_type,'yes')
            schema.create_table_setup('drop_tb',table_type,table_type)

    def runSQLFiles(self):
        tinctest.logger.info('Running SQL files under the setup folder')
        for file in os.listdir(local_path('setup')):
            if fnmatch.fnmatch(file,'*_table_pre.sql'):
                PSQL.run_sql_file(local_path('setup/' + file))
