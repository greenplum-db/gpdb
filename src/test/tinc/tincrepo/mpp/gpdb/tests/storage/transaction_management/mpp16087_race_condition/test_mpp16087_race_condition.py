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

import datetime

import tinctest
import unittest2 as unittest

from gppylib.commands.base import Command
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.models import SQLConcurrencyTestCase

class catalog_consistency(SQLConcurrencyTestCase):
    """
    
    @description FATAL failure execution handles already committed transactions properly
    @created 2013-04-19 00:00:00
    @modified 2013-04-19 00:00:00
    @tags transaction checkpoint MPP-17817 MPP-17925 MPP-17926 MPP-17927 MPP-17928 schedule_transaction
    @product_version gpdb: [4.1.2.5- main]
    """
    dbname = "mpp16087"

    @classmethod
    def setUpClass(cls):
        super(catalog_consistency, cls).setUpClass()
        tinctest.logger.info('run sql concurrently')
        cmd = Command(name='Create the database', cmdStr='createdb %s' % (catalog_consistency.dbname))
        cmd.run()

    @classmethod
    def tearDownClass(cls):
        cmd = Command(name='Drop the database', cmdStr='dropdb %s' % (catalog_consistency.dbname))
        cmd.run()
        super(catalog_consistency, cls).tearDownClass()

    def run_test(self):
        sql_file = self.sql_file
        ans_file = self.ans_file
        # overriding the runtests so that we can pass skipvalidation
        now = datetime.datetime.now()
        timestamp = '%s%s%s%s%s%s%s'%(now.year,now.month,now.day,now.hour,now.minute,now.second,now.microsecond)
        out_file = sql_file.replace('.sql', timestamp + '.out')
        result = PSQL.run_sql_file(sql_file,dbname=catalog_consistency.dbname, out_file = out_file)
        return True

    def test_verify_catalog(self):
        dbstate = DbStateClass('run_validation')
        dbstate.check_catalog(alldb=False, dbname=catalog_consistency.dbname)

