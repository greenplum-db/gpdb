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

from mpp.models import MPPTestCase

from gppylib.commands.base import Command
from gppylib.db import dbconn
from gppylib.gparray import GpArray

from mpp.gpdb.tests.storage.walrepl.lib import NewEnv
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

import os
import shutil
import signal

pgutil = GpUtility()
class gpactivatestandby(StandbyRunMixin, MPPTestCase):

    master_dd = os.environ.get('MASTER_DATA_DIRECTORY')
    standby_host = ''

    def setUp(self):
        # cleanup
        cmd = Command('gpinitstandby', 'gpinitstandby -ar')
        # don't care the result in case standby is not configured
        cmd.run()

    def tearDown(self):
        # cleanup
        cmd = Command('gpinitstandby', 'gpinitstandby -ar')
        cmd.run()

    def test_wrong_dataparam(self):
        """
        Test if conflicting -d option is reported.
        @tags sanity
        """

        cmd = Command(
                'gpactivatestandby',
                'gpactivatestandby -a -d invalid_directory')
        cmd.run()
        results = cmd.get_results()
        expected = 'Please make sure the command gpactivatestandby is executed on current Standby host'
        expected_regex = 'Error activating standby master: Critical required file on standby \".*\" is not present'

        self.assertIn(expected, results.stdout)
        self.assertRegexpMatches(results.stdout, expected_regex)
        self.assertEqual(results.rc, 2)

    def test_from_not_running(self):
        """
        Test gpactivatestandby -f while standby is not running.
        """

        dburl = dbconn.DbURL()
        array = GpArray.initFromCatalog(dburl, utility=True)
        self.standby_host = array.master.getSegmentHostName()
        # create standby locally
        shutil.rmtree(self.basepath, True)
        try:
            os.makedirs(self.basepath)
        except OSError, e:
            if e.errno != 17:
                raise
            pass
        cmd = Command(
                'gpinitstandby',
                ' '.join(['gpinitstandby', '-a',
                    '-s', self.standby_host,
                    '-P', self.standby_port,
                    '-F', self.standby_datadir]))
        cmd.run(validateAfter=True)

        nsender = self.wait_for_walsender()
        self.assertEqual(nsender, 1, 'replication has not begun')

        # stop standby
        cmd_str = 'pg_ctl stop -D {0}'.format(self.standby_datadir)
        cmd = Command('pg_ctl stop', cmd_str)
        cmd.run(validateAfter=True)

        datadir = os.path.abspath(self.standby_datadir)
        with NewEnv(MASTER_DATA_DIRECTORY=datadir,
                             PGPORT=self.standby_port) as env:
            cmd_str = 'pg_ctl stop -D {0}'.format(self.master_dd)
            cmd = Command('stop master before activating standby',cmd_str)
            cmd.run(validateAfter=True)
            # -f is required
            cmd = Command('gpactivatestandby', 'gpactivatestandby -af')
            cmd.run()
            # It could return 1 in case warning is emitted.
            self.assertIn(cmd.get_results().rc, (0, 1))
        pgutil.failback_to_original_master(self.master_dd, self.standby_host, self.standby_datadir,self.standby_port)
