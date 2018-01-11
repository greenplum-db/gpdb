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
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin

class gpstart(StandbyRunMixin, MPPTestCase):
    def test_start(self):
        """
        Verify if gpstart stops the standby and gpstart starts it
        correctly.
        """

        # set up standby
        res = self.standby.create()
        self.assertEqual(res, 0)
        res = self.standby.start()
        self.assertTrue(res.wasSuccessful())

        # stop the whole cluster including standby
        cmd = Command("gpstop", "gpstop -a")
        cmd.run()
        self.assertEqual(cmd.get_results().rc, 0, str(cmd))

        # check if standby is shut down
        cmd = Command("pg_ctl status",
                      "pg_ctl status -D {0}".format(self.standby.datadir))
        cmd.run()
        # pg_ctl status returns 1 if it's not running
        self.assertEqual(cmd.get_results().rc, 1, str(cmd))

        # bring up the cluster.  gpstart should bring up the standby, too.
        cmd = Command("gpstart", "gpstart -a")
        cmd.run()
        self.assertTrue(cmd.get_results().rc in (0, 1), str(cmd))

        # wait for the standby to connect to the primary
        num_walsender = self.wait_for_walsender()
        self.assertEqual(num_walsender, 1)

        # make sure pg_ctl also agrees it's running
        cmd = Command("pg_ctl status",
                      "pg_ctl status -D {0}".format(self.standby.datadir))
        cmd.run()
        self.assertEqual(cmd.get_results().rc, 0, str(cmd))

