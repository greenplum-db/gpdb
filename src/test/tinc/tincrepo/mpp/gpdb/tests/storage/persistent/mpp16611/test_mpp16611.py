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
from tinctest.models.scenario import ScenarioTestCase
from mpp.models import MPPTestCase
from mpp.models import SQLTestCase
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop

@tinctest.skipLoading('scenario')
class scenario_sql(SQLTestCase):
    sql_dir = 'sql'
    ans_dir = 'sql'

    def get_substitutions(self):
        return {'@long_comment@':
                'comment_to_force_toast,' * 1000000
                }

@tinctest.skipLoading('scenario')
class scenario_restart(MPPTestCase):
    def restart(self):
        # GpStop does not accept immedaite and restart both...
        GpStop().run_gpstop_cmd(immediate=True)
        GpStart().run_gpstart_cmd()

class mpp16611(MPPTestCase, ScenarioTestCase):
    """
    
    @description Test for the RelcacheInvalidate VS persistent table
    @created 2013-02-12 00:00:00
    @modified 2014-06-12 00:00:00
    @tags mpp16160 schedule_persistent echo
    @product_version gpdb: [4.2.5.0- main]
    """
    def test_pt_relation_cache(self):
        """

        The issue of mpp16611 is that persistent table (PT) information
        in Relation cache is lost during heap_insert after fetching it
        from gp_relation_node, if concurrent cache invalidation message
        overflows, and if the heap_insert goes saving values to toast
        table.  We should restore the PT info in that case.

        The following is the series of events.

            - Session 1 inserts a row worthy of toasting into a table.
            - After session 1 picks up the gp_relation_node information for
              this table, but prior to relation_open, Session 2 jumps in.
            - Session 2 needs to overflow shared invalidation message, which
              causes Session 1 to rebuild all relcache (and catcache) rebuild
              regardless whether the cache entries actually need to be
              updated. The overflow threshold is defined by MAXNUMMESSAGES
              in sinvaladt.h
            - Session 1 proceeds to relation_open and
              AcceptInvalidationMessages, and during RelationClearRelation,
              it rebuilds its relcache information which erroneously loses
              the gp_relation_node information.
            - Session 1 persists this incorrect gp_relation_node information
              to the xlog during XLogInsert.
            - If we then crash immediately, and rely upon xlog replay during
              crash recovery, we hit the heap_insert_redo error.  It seems
              we hit the heap_insert_redo error in segments, but in master it
              just loses the data, which this test shows.

        In this test, we force relcache refresh by gp_test_system_cache_flush_force = plain
        """

        package_name = 'mpp.gpdb.tests.storage.persistent.mpp16611'
        prefix = self.package_name + '.test_mpp16611'
        self.test_case_scenario.append(['%s.scenario_sql.test_workload' % prefix])
        self.test_case_scenario.append(['%s.scenario_sql.test_check_catalog' % prefix])
        self.test_case_scenario.append(['%s.scenario_restart.restart' % prefix])
        self.test_case_scenario.append(['%s.scenario_sql.test_check_catalog' % prefix])
