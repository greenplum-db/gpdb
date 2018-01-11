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

from gppylib.commands.base import Command

import tinctest
from tinctest.lib import local_path
from tinctest.case import TINCTestCase

from mpp.lib.PSQL import PSQL

class ReindexPgClass(TINCTestCase):
    """Test recovery after reindex of pg_class indexes.
    """

    def __init__(self, methodName):
        super(ReindexPgClass, self).__init__(methodName)
        self.dbname = None

    def test_reindex_pg_class(self):
        tinctest.logger.info("create checkpoint")
        results = {'rc':0, 'stdout':'', 'stderr':''}
        PSQL.run_sql_command("checkpoint", results=results, dbname=self.dbname)
        assert results['rc'] == 0, results['stderr']

        tinctest.logger.info("inject fault to skip checkpoints")
        cmd = Command("skip checkpoint on primaries",
                      "gpfaultinjector -f checkpoint -m async -y skip -o 0"
                      " -H ALL -r primary")
        cmd.run(validateAfter=True)
        tinctest.logger.info(cmd.get_results().printResult())

        cmd = Command("skip checkpoint on master",
                      "gpfaultinjector -f checkpoint -m async -y skip -o 0 -s 1")
        cmd.run(validateAfter=True)
        tinctest.logger.info(cmd.get_results().printResult())

        tinctest.logger.info("reindex pg_class indexes")
        assert PSQL.run_sql_file(local_path('reindex_pg_class.sql'),
                                 dbname=self.dbname)

        tinctest.logger.info("shutdown immediate")
        cmd = Command("shutdown immediate", "gpstop -ai")
        cmd.run(validateAfter=True)
        tinctest.logger.info(cmd.get_results().printResult())

        tinctest.logger.info("trigger recovery")
        cmd = Command("restart the cluster", "gpstart -a")
        cmd.run(validateAfter=True)
        tinctest.logger.info(cmd.get_results().printResult())

        tinctest.logger.info("validate recovery succeeded")
        results = {'rc':0, 'stdout':'', 'stderr':''}
        PSQL.run_sql_command("DROP TABLE reindex_pg_class_test",
                             results=results, dbname=self.dbname)
        assert results['rc'] == 0, results['stderr']

    def test_reindex_pg_class_template1(self):
        """Test that relcache does not contain stale refilenodes after
        crash recovery.
        """
        self.dbname = "template1"
        self.test_reindex_pg_class()
