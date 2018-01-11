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
from gppylib import gparray
from gppylib.db import dbconn

from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin

class gpinitstandby(StandbyRunMixin, MPPTestCase):

    def test_initstandby_utility(self):
        """
        Verify gpinitstandby works with utility master.
        """

        # stop master and restart in utility mode.
        Command('gpstop -am', 'gpstop -am').run(validateAfter=True)
        Command('gpstart -am',
                'echo yes | gpstart -am').run(validateAfter=True)
        try:
            return self.test_initstandby()
        finally:
            # cleanup
            Command('gpstop -ar', 'gpstop -ar').run()

    def test_initstandby(self):
        """
        Verify if gpinitstandby creates and removes standby.
        All done in local.
        """

        # cleanup
        cmd = Command('gpinitstandby',
                      'gpinitstandby -ar')
        # don't care the result in case standby is not configured
        cmd.run()
        
        dburl = dbconn.DbURL()
        array = gparray.GpArray.initFromCatalog(dburl, utility=True)
        # create standby locally
        cmd = Command('gpinitstandby',
                      ' '.join(['gpinitstandby', '-a',
                         '-s', array.master.getSegmentHostName(),
                         '-P', self.standby_port,
                         '-F', 'pg_system:' + self.standby_datadir]))
        cmd.run(validateAfter=True)

        nsender = self.wait_for_walsender()
        self.assertEqual(nsender, 1, 'replication has not begun')

        # remove standby
        cmd = Command('gpinitstandby',
                      ' '.join(['gpinitstandby', '-ar']))
        cmd.run(validateAfter=True)

    def test_samehostport(self):
        """
        If the standby is being created on the same host/port,
        it should be rejected.

        @tags sanity
        """

        dburl = dbconn.DbURL()
        array = gparray.GpArray.initFromCatalog(dburl, utility=True)
        cmd = Command('gpinitstandby',
                      'gpinitstandby -a -s {0} -F {1}'.format(
                            array.master.getSegmentHostName(),
                            'pg_system:/tmp/dummydir'))
        cmd.run()
        self.assertIn('cannot create standby on the same host and port',
                        cmd.get_results().stdout)

