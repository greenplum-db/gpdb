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
from mpp.lib.PSQL import PSQL
from mpp.lib.GPFDIST import GPFDISTError
from tinctest.lib import local_path,run_shell_command
from mpp.models import SQLTestCase
from mpp.gpdb.tests.package.metadata_track import MDT
from mpp.lib.gppkg.gppkg import Gppkg

mdt = MDT()
class MDTSQLTestCase(SQLTestCase):
    """ 
    @optimizer_mode off
    @tags gppkg
    """

    sql_dir = 'sql/'
    ans_dir = 'expected'
    out_dir = 'output/'

    @classmethod
    def setUpClass(cls):
        """
        Checking if plperl package installed, otherwise install the package
        """
        super(MDTSQLTestCase, cls).setUpClass()
        mdt.pre_process_sql()
        mdt.pre_process_ans()
        mdt.setup_gpfdist()

        cmd = 'gpssh --version'
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command(cmd, 'check product version', res)
        gppkg = Gppkg()
        product_version = res['stdout']
        gppkg.gppkg_install(product_version, 'plperl')

        setup_user = 'create role mdt_user superuser login;'
        setup_db = 'create database mdt_db;'
        setup_sql = local_path('sql/setup/setup.sql')
        setup_output = local_path('output/setup/setup.out')
        PSQL.run_sql_command(sql_cmd=setup_user, dbname=os.environ.get('PGDATABASE'))
        PSQL.run_sql_command(sql_cmd=setup_db, dbname=os.environ.get('PGDATABASE'), username='mdt_user')
        PSQL.run_sql_file(sql_file = setup_sql, out_file=setup_output, dbname='mdt_db', username='mdt_user')

    @classmethod
    def tearDownClass(cls):
        try:
            mdt.cleanup_gpfdist()
        except GPFDISTError:
            tinctest.logger.error("Unable to clenup gpfdist process")
