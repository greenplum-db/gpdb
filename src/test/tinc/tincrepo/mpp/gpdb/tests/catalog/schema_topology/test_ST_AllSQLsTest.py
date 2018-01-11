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

import tinctest
import os
import getpass

from mpp.models import SQLTestCase
from mpp.lib.PSQL import PSQL
from tinctest.lib import local_path, Gpdiff
from mpp.gpdb.tests.storage.lib import Database

import re
import fileinput
from mpp.lib.datagen.dataset import DataSetDatabase

class AllSQLsTest(SQLTestCase):
    """
    
    @description This contains several test cases for possible ways of manipulating objects.
                    For more details refer QA task - QA-143
    @created 2009-01-27 14:00:00
    @modified 2013-10-17 17:10:15
    @tags ddl schema_topology
    """
            
    sql_dir = 'sqls/ddls/all'
    ans_dir = 'sqls/ddls/all'
    out_dir = 'sqls/ddls/all'

    @classmethod
    def get_out_dir(cls):
        #If the sqls are located in a different directory than the source file, create an output
        #directory at the same level as the sql dir
        if cls.get_source_dir() == cls.get_sql_dir():
            out_dir = os.path.join(cls.get_sql_dir(), 'output/AllSQLsTest/')
        else:
            out_dir = os.path.join(cls.get_sql_dir(), '../output/AllSQLsTest/')

        if not os.path.exists(out_dir):
            os.makedirs(out_dir)
        return out_dir

    @classmethod
    def setUpClass(cls):
        super(AllSQLsTest, cls).setUpClass()
        tinctest.logger.info("*** Running the pre-requisite sql files drop.sql and setup.sql")

        """
        Creating database with default name
        """
        db = Database()
        db.setupDatabase()

        PSQL.run_sql_file(local_path('sqls/setup/drop.sql'))
        PSQL.run_sql_file(local_path('sqls/setup/create.sql'))
        PSQL.run_sql_file(local_path('sqls/setup/anyconstraint.sql'))
        tinctest.logger.info('*** Starting the schema topology tests..')

    def run_sql_file(self, sql_file, out_file = None, out_dir = None, optimizer=None):
        """
        Given a sql file and an ans file, this adds the specified gucs (self.gucs) to the sql file , runs the sql
        against the test case databse (self.db_name) and verifies the output with the ans file.
        If an 'init_file' exists in the same location as the sql_file, this will be used
        while doing gpdiff.
        """
        result = True

        self.test_artifacts.append(sql_file)
        if not out_file:
            out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql','.out'))
        self.test_artifacts.append(out_file)

        tinctest.logger.info('running the sql testcase file:')
        tinctest.logger.info(sql_file)

        if (sql_file.find('hybrid_part_tbl_drop_col')>=0):
            
            default_db = getpass.getuser()
            dbase = os.getenv('PGDATABASE',default_db)
            datasetobj = DataSetDatabase(database_name = dbase)
            tinctest.logger.info('--running dataset reload')
            datasetobj.reload_dataset()

        PSQL.run_sql_file(sql_file, dbname = self.db_name, out_file = out_file)

        return out_file
