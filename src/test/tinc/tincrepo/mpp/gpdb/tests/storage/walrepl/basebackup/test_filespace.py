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

from tinctest import logger
from tinctest.lib import run_shell_command

from mpp.lib.PSQL import PSQL
from tinctest.lib import local_path
from mpp.models import MPPTestCase

from gppylib.db import dbconn
from gppylib.gparray import GpArray

from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin
from mpp.gpdb.tests.storage.walrepl.lib.pqwrap import *
from mpp.gpdb.tests.storage.walrepl.lib import PreprocessFileMixin, cleanupFilespaces
from mpp.gpdb.tests.storage.walrepl.lib.standby import Standby

import os
import shutil
import subprocess

class basebackup_filespace(StandbyRunMixin, MPPTestCase,
                           PreprocessFileMixin):

    def setUp(self):
        """
        Override
        """

        # Removing standby as these tests will fail if there is a standby enabled
        # Ignore any failures
        run_shell_command(cmdstr='gpinitstandby -ra', cmdname='remove standby')

        self.createdb(self.db_name)
        self.standby = Standby(self.standby_datadir, self.standby_port)

    def tearDown(self):
        """
        Override
        """
    
        try:
            dburl = dbconn.DbURL()
            self.standby.remove_catalog_standby(dburl)
        except Exception:
            pass
        cleanupFilespaces(dbname=self.db_name)
      
       
    def test_filespace(self):
        """
        pg_basebackup should work with user filespace.

        @tags sanity
        """

        # Add standby entry in catalog before regisering filespace.
        fsbase = os.path.join(self.fsprefix, 'fs')
        shutil.rmtree(fsbase, True)
        os.makedirs(fsbase)
        shutil.rmtree(self.standby_datadir, True)
        dburl = dbconn.DbURL()
        gparray = GpArray.initFromCatalog(dburl, utility=True)
        if gparray.standbyMaster:
            self.standby.remove_catalog_standby(dburl)
        self.standby.add_catalog_standby(dburl, gparray)

        #self.preprocess_file(local_path('filespace.sql.in'))
        sql_file = local_path('filespace.sql')
        from mpp.lib.gpfilespace import Gpfilespace
        gpfile = Gpfilespace()
        gpfile.create_filespace('fs_walrepl_a')
        result = PSQL.run_sql_file(sql_file, dbname=self.db_name)
        self.assertTrue(result)
        subprocess.check_call(['pg_basebackup', '-D', self.standby_datadir])

        #fsdir = os.path.join(self.fsprefix, 'fs', 'gpdb1')
        fsdir = os.path.join(os.path.split(self.standby_datadir)[0], 'fs_walrepl_a','mirror','pg_system')
        self.assertTrue(os.path.isdir(fsdir),
                        '{0} does not dir'.format(fsdir))
        # TODO: check relfiles...
