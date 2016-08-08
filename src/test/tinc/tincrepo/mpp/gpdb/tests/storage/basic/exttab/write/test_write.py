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
import platform
import shutil

import tinctest
from gppylib.commands.base import Command

from mpp.models import SQLTestCase
from mpp.lib import mppUtil
from mpp.lib.config import GPDBConfig
from mpp.lib.GPFDIST import GPFDIST
from mpp.lib.PSQL import PSQL

class LegacyWETTestCase(SQLTestCase):
    '''
    @db_name wet
    @tags list_mgmt_expand
    @product_version gpdb: [4.3.5-] 
    '''
    sql_dir = 'sql'
    ans_dir = 'sql'

    @classmethod
    def setUpClass(cls):
        # we need an empty db to run the tests
        tinctest.logger.info("recreate database wet using dropdb/createdb")
        cmd = Command('recreatedb', 'dropdb wet; createdb wet')
        cmd.run(validateAfter=False)

        cls.drop_roles()

        super(LegacyWETTestCase, cls).setUpClass()

        source_dir = cls.get_source_dir()
        config = GPDBConfig()
        host, _ = config.get_hostandport_of_segment(0)
        port = mppUtil.getOpenPort(8080)
        tinctest.logger.info("gpfdist host = {0}, port = {1}".format(host, port))

        cls.config = config

        data_dir = os.path.join(source_dir, 'data')
        cls.gpfdist = GPFDIST(port, host, directory=data_dir)
        cls.gpfdist.startGpfdist()

        # WET writes into this directory.
        data_out_dir = os.path.join(cls.gpfdist.getdir(), 'output')
        shutil.rmtree(data_out_dir, ignore_errors=True)
        os.mkdir(data_out_dir)

    @classmethod
    def tearDownClass(cls):
        cls.gpfdist.killGpfdist()
        cls.drop_roles()

        super(LegacyWETTestCase, cls).tearDownClass()

    @classmethod
    def drop_roles(cls):
        '''
        Cleans up roles that might have been created in the tests.
        '''
        sql = "select rolname from pg_authid where rolname like 'ext_%'"
        lines = PSQL.run_sql_command(sql, flags='-q -t -A')
        rolnames = [n.strip() for n in lines.strip().split('\n')]
        if len(rolnames) > 0:
            sql = "drop owned by {0}; drop role {0}".format(', '.join(rolnames))
            PSQL.run_sql_command(sql, dbname='wet')

    def setUp(self):
        numcontent = self.config.get_countprimarysegments()
        if self.method_name == 'test_functional_seg2' and numcontent < 2:
            self.skip = 'not applicable for numcontent < 2'
        if self.method_name == 'test_functional_multiple_gpfdist' and numcontent > 2:
            self.skip = 'not applicable for numcontent > 2'
        super(LegacyWETTestCase, self).setUp()

    def get_substitutions(self):
        """
        Returns sustitution variables.
        """
        source_dir = self.get_source_dir()
        variables = {'@abs_srcdir@': source_dir,
                '@hostname@': self.gpfdist.gethost(),
                '@gp_port@': str(self.gpfdist.getport()),
                '@gpfdist_datadir@': self.gpfdist.getdir(),
                '@numcontent@': str(),
                '@tincrepohome@': os.environ.get('TINCREPOHOME'),
                }
        if platform.system() == 'SunOS':
            variables['@killall@'] = 'pkill'
        else:
            variables['@killall@'] = 'killall'

        return variables
