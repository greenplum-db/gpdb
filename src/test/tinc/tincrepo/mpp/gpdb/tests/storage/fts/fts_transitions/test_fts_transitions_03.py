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
from gppylib.commands.base import Command
import tinctest
from tinctest.lib import Gpdiff, local_path
from mpp.gpdb.tests.storage.fts.fts_transitions.fts_transitions import FTSTestCase
from mpp.lib.config import GPDBConfig
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase

class ResyncBug(MPPTestCase):

    def __init__(self, methodName):
        super(ResyncBug, self).__init__(methodName)

    def run_sql(self, filename):
        sql_file = local_path('sql/%s.sql' % filename)
        ans_file = local_path('expected/%s.ans' % filename)
        out_file = local_path('output/%s.out' % filename)
        assert PSQL.run_sql_file(sql_file, out_file), sql_file
        assert Gpdiff.are_files_equal(out_file, ans_file), out_file

    def test_resync_ct_blocks_per_query(self):
        '''Catch a bug in resync that manifests only after rebalance.
        The logic used by a resync worker to obtain changed blocks
        from CT log had a bug.  The SQL query used to obtain a batch
        of changed blocks from CT log was incorrectly using LSN to
        filter out changed blocks.  All of the following must be true
        for the bug to occur:

         * More than gp_filerep_ct_batch_size blocks of a relation
           are changed on a segment in changetracking.

         * A block with a higher number is changed earlier (lower
           LSN) than lower numbered blocks.

         * The first batch of changed blocks obtained by resync worker
           from CT log for this relation contains only lower
           (according to block number) blocks.  The higher block with
           lower LSN is not included in this batch.  Another query
           must be run against CT log to obtain this block.

         * The SQL query used to obtain next batch of changed blocks
           for this relation contains incorrect WHERE clause involving
           a filter based on LSN of previously obtained blocks.  The
           higher numbered block is missed out - not returned by the
           query as changed block for the relation.  The block is
           never shipped from primary to mirror, resulting in data
           loss.  The test aims to verify that this doesn't happen as
           the bug is now fixed.
        '''
        config = GPDBConfig()
        assert (config.is_not_insync_segments() &
                config.is_balanced_segments()), 'cluster not in-sync and balanced'

        # Create table and insert data so that adequate number of
        # blocks are occupied.
        self.run_sql('resync_bug_setup')
        # Bring down primaries and transition mirrors to
        # changetracking.
        filerep = Filerepe2e_Util()
        filerep.inject_fault(y='fault', f='segment_probe_response',
                             r='primary')
        # Trigger the fault by running a sql file.
        PSQL.run_sql_file(local_path('test_ddl.sql'))
        filerep.wait_till_change_tracking_transition()

        # Set gp_filerep_ct_batch_size = 3.
        cmd = Command('reduce resync batch size',
                      'gpconfig -c gp_filerep_ct_batch_size -v 3')
        cmd.run()
        assert cmd.get_results().rc == 0, 'gpconfig failed'
        cmd = Command('load updated config', 'gpstop -au')
        cmd.run()
        assert cmd.get_results().rc == 0, '"gpstop -au" failed'

        self.run_sql('change_blocks_in_ct')

        # Capture change tracking log contents from the segment of
        # interest for debugging, in case the test fails.
        (host, port) = GPDBConfig().get_hostandport_of_segment(0, 'p')
        assert PSQL.run_sql_file_utility_mode(
            sql_file=local_path('sql/ct_log_contents.sql'),
            out_file=local_path('output/ct_log_contents.out'),
            host=host, port=port), sql_file

        gprecover = GpRecover(GPDBConfig())
        gprecover.incremental(False)
        gprecover.wait_till_insync_transition()

        # Rebalance, so that original primary is back in the role
        gprecover = GpRecover(GPDBConfig())
        gprecover.rebalance()
        gprecover.wait_till_insync_transition()

        # Reset gp_filerep_ct_batch_size
        cmd = Command('reset resync batch size',
                      'gpconfig -r gp_filerep_ct_batch_size')
        cmd.run()
        assert cmd.get_results().rc == 0, 'gpconfig failed'
        cmd = Command('load updated config', 'gpstop -au')
        cmd.run()
        assert cmd.get_results().rc == 0, '"gpstop -au" failed'

        self.run_sql('select_after_rebalance')


class FtsTransitionsPart03(FTSTestCase):
    ''' State of FTS at different fault points
    '''
    
    def __init__(self, methodName):
        super(FtsTransitionsPart03,self).__init__(methodName)

    def test_primary_resync_postmaster_reset_with_faults(self):
        '''
        @data_provider pr_faults
        '''
        filerep_fault = self.test_data[1][0]
        fault_type = self.test_data[1][1]
        filerep_role = self.test_data[1][2]
        gpstate_role = self.test_data[1][3]
        gprecover = self.test_data[1][4]
        
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: %s " % self.test_data[0][1])
        tinctest.logger.info("\n ===============================================")
        self.primary_resync_postmaster_reset_with_faults(filerep_fault, fault_type, filerep_role, gpstate_role, gprecover)

    def test_mirror_resync_postmaster_reset_with_faults(self):

        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: test_mirror_resync_postmaster_reset_with_faults ")
        tinctest.logger.info("\n ===============================================")
        self.mirror_resync_postmaster_reset_with_faults()

@tinctest.dataProvider('pr_faults')
def test_pr_faults():

    data = {'test_27_primary_resync_postmaster_reset_checkpoint': ['checkpoint','panic', 'primary', 'mirror', 'no'],
            'test_28_primary_resync_postmaster_reset_filerep_flush': ['filerep_flush','panic','primary', 'mirror', 'no'],
            'test_29_primary_resync_postmaster_reset_filerep_consumer': ['filerep_consumer','panic', 'primary', 'mirror', 'no'],
            'test_30_mirror_resync_process_missing_failover': ['filerep_sender','error', 'mirror', 'mirror', 'yes'],
            'test_31_primary_resync_process_missing_failover': ['filerep_sender','error', 'primary', 'primary', 'yes'],
            'test_32_mirror_resync_deadlock_failover': ['filerep_sender', 'infinite_loop', 'mirror', 'mirror', 'yes'],
            'test_33_primary_resync_deadlock_failover': ['filerep_sender', 'infinite_loop', 'primary', 'primary', 'yes'],
            'test_34_primary_resync_filerep_network_failover': ['filerep_consumer', 'panic', 'mirror', 'mirror', 'yes'],
            'test_36_primary_resync_postmaster_missing_failover': ['postmaster', 'panic', 'primary', 'mirror', 'no'],
            'test_37_primary_resync_system_failover': ['filerep_resync_worker_read', 'fault', 'primary', 'mirror', 'yes'],
            'test_38_primary_resync_mirror_cannot_keepup_failover': ['filerep_receiver', 'sleep', 'primary', 'mirror', 'no'],
            'test_39_mirror_resync_filerep_network': ['filerep_receiver', 'fault', 'mirror', 'primary', 'yes'],
            'test_40_mirror_resync_system_failover': ['filerep_flush', 'fault', 'mirror', 'primary', 'yes'],
            'test_41_mirror_resync_postmaster_missing_failover': ['postmaster', 'panic', 'mirror', 'primary', 'yes'],
            'test_46_primary_resync_postmaster_reset_filerep_sender': ['filerep_sender', 'panic', 'primary', 'primary', 'yes']}
            
    return data
