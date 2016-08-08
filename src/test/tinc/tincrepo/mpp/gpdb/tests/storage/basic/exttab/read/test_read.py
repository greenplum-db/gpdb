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
from tinctest.lib.system import TINCSystem

from mpp.models import SQLTestCase
from mpp.lib import mppUtil
from mpp.lib.config import GPDBConfig
from mpp.lib.GPFDIST import GPFDIST
from mpp.lib.PSQL import PSQL

class LegacyRETTestCase(SQLTestCase):
    """
    @tags list_mgmt_expand
    @product_version gpdb: [4.3-]
    """
    sql_dir = 'sql'
    ans_dir = 'sql'

    @classmethod
    def setUpClass(cls):
        super(LegacyRETTestCase, cls).setUpClass()

        cls.split_tbl()

        source_dir = cls.get_source_dir()
        config = GPDBConfig()
        host, _ = config.get_hostandport_of_segment(0)
        port = mppUtil.getOpenPort(8080)
        tinctest.logger.info("gpfdist host = {0}, port = {1}".format(host, port))

        data_dir = os.path.join(source_dir, 'data')
        cls.gpfdist = GPFDIST(port, host, directory=data_dir)
        cls.gpfdist.startGpfdist()

        # Some test writes data into disk temporarily.
        data_out_dir = os.path.join(data_dir, 'output')
        shutil.rmtree(data_out_dir, ignore_errors=True)
        os.mkdir(data_out_dir)

    @classmethod
    def tearDownClass(cls):
        cls.gpfdist.killGpfdist()

        super(LegacyRETTestCase, cls).tearDownClass()

    @classmethod
    def split_tbl(cls):
        """
        Split data files using split command for the testing
        where gpfdist serves multiple files.
        """

        source_dir = cls.get_source_dir()
        data_dir = os.path.join(source_dir, 'data')
        for fn in os.listdir(data_dir):
            if not fn.endswith('.tbl'):
                continue
            split_dir = os.path.join(data_dir, fn + '-dir')
            if os.path.exists(split_dir):
                continue
            TINCSystem.make_dirs(split_dir, ignore_exists_error=True)
            os.system('cd {0}; split ../{1}'.format(split_dir, fn))

    def get_substitutions(self):
        """
        Returns sustitution variables.
        """
        source_dir = self.get_source_dir()
        variables = {'@gpwhich_curl@': 'curl',
                '@abs_srcdir@': source_dir,
                '@gpwhich_gunzip@': 'gunzip',
                '@hostname@': self.gpfdist.gethost(),
                '@gp_port@': str(self.gpfdist.getport()),
                '@gpfdist_datadir@': self.gpfdist.getdir(),
                }
        if platform.system() == 'SunOS':
            variables['@killall@'] = 'pkill'
        else:
            variables['@killall@'] = 'killall'

        return variables
