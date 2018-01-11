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
import os

from gppylib.commands.base import Command, REMOTE
from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from time import sleep
from tinctest import TINCTestCase
from mpp.gpdb.tests.utilities.recoverseg.gprecoverseg_tests.fault.genFault import Fault
from mpp.lib.config import GPDBConfig

from mpp.lib.gprecoverseg import GpRecoverseg
from utilities.gppersistentrebuild import PTTestCase

# Environmental variable to be set priror to the gprecoverseg run.
ENV_VAR="GP_MPP_12038_INJECT_DELAY" 

class FaultInjectorTestCase (TINCTestCase):
    """
    
    @description Injects the specific faults in Primary or Mirror
    @created 2009-01-27 14:00:00
    @modified 2013-09-12 17:10:15
    @tags storage schema_topology 
    @product_version gpdb:4.2.x,gpdb:main
    """

    def test_kill_primary(self):
        """
        [feature]: Kills primary gp0 segment
        
        """
        
        newfault = Fault()
        hosts = newfault.get_segment_host(preferred_role='p',content=0) 
        if not newfault.kill_primary_gp0(hosts):
            self.fail("Could not the kill the primary process, cannot proceed further!")
        rtrycnt = 0
        while( not newfault.is_changetracking()):
            tinctest.logger.info("Waiting [%s] for DB to go into CT mode" %rtrycnt)
            rtrycnt = rtrycnt + 1

    def test_kill_mirror(self):
        """
        [feature]: Kills mirror gp0 segment 
        
        """

        newfault = Fault()
        hosts = newfault.get_segment_host(preferred_role='m',content=0)
        if not newfault.kill_mirror_gp0(hosts):
            self.fail("Could not the kill the mirror process, cannot proceed further!")
        rtrycnt = 0
        while( not newfault.is_changetracking()):
            tinctest.logger.info("Waiting [%s] for DB to go in CT mode" %rtrycnt)
            rtrycnt = rtrycnt + 1

    def test_kill_primary_group(self):
        """
        [feature]: Kill a group of primary segments
        
        """

        newfault = Fault()
        seglist = newfault.get_seginfo_for_primaries()
        seglist = seglist[:(len(seglist) + 1 ) / 2]
        for seg in seglist:
            tinctest.logger.info('Killing segment %s' % seg.getSegmentDataDirectory())
            newfault.kill_primary(seg.getSegmentHostName(), seg.getSegmentDataDirectory(), seg.getSegmentPort())
        rtrycnt = 0
        while (not newfault.is_changetracking()):
            tinctest.logger.info('Waiting [%s] for DB to go in CT mode' % rtrycnt)
            rtrycnt += 1

    def test_drop_pg_dirs_on_primary(self):
        """
        [feature]: Drops primary gp0 folder 
        
        """

        newfault = Fault()
        (host, fileLoc) = newfault.get_segment_host_fileLoc()
        newfault.drop_pg_dirs_on_primary(host, fileLoc)
        rtrycnt = 0
        max_rtrycnt = 300
        while( not newfault.is_changetracking()):
            tinctest.logger.info("Waiting [%s] for DB to go into CT mode" %rtrycnt)
            rtrycnt = rtrycnt + 1
    
    def test_use_gpfaultinjector_to_mark_segment_down(self):
        """
        [feature]: Use gpfaultinjector to mark a segment down in the configuration, but the 
        process is still running on the segment.
        
        """

        newfault = Fault()
        seginfo = newfault.get_seginfo(preferred_role='m', content=1)
        newfault.inject_using_gpfaultinjector(fault_name='filerep_consumer', fault_mode='async', fault_type='fault', segdbid=seginfo.getSegmentDbId())
        rtrycnt = 0
        while (not newfault.is_changetracking()):
            tinctest.logger.info("Waiting [%s] for DB to go into CT mode" % rtrycnt)
            rtrycnt += 1

    def test_create_symlink_for_seg(self):
        """
        [feature]: Creates a symlink to the data directory for a given segment
        
        """
        
        newfault = Fault()
        seginfo = newfault.get_seginfo(preferred_role='m', content=1)
        newfault.create_remote_symlink(seginfo.getSegmentHostName(), seginfo.getSegmentDataDirectory())
        tinctest.logger.info('Creating symlink for seg %s on host %s' % (seginfo.getSegmentDataDirectory(), seginfo.getSegmentHostName()))

    def test_remove_symlink_for_seg(self):
        """
        [feature]: Remove symlink for datadirectory and restore the orignal directory
                   for a given segment.
        
        """

        newfault = Fault()
        seginfo = newfault.get_seginfo(preferred_role='m', content=1)
        newfault.remove_remote_symlink(seginfo.getSegmentHostName(), seginfo.getSegmentDataDirectory())
        tinctest.logger.info('Removed symlinks for seg %s on host %s' % (seginfo.getSegmentDataDirectory(), seginfo.getSegmentHostName()))

    def test_corrupt_persistent_tables(self):
        """
        [feature]: corrupts PT tables for segment that has been marked down 
        
        """
        
        newfault = Fault()
        seginfo = newfault.get_seginfo(preferred_role='p', content=1)
        pt = PTTestCase('corrupt_persistent_table')
        pt.corrupt_persistent_table(seginfo.getSegmentHostName(), seginfo.getSegmentPort())
        tinctest.logger.info('Finished corruption of PT tables')

    def test_rebuild_persistent_tables(self):
        """
        [feature]: rebuilds PT tables for segment that has been marked down 
        
        """
        cmd = Command(name='Running gppersistentrebuild tool', cmdStr = 'echo "y\ny\n" | $GPHOME/sbin/gppersistentrebuild -c 1')
        cmd.run(validateAfter=True)
        tinctest.logger.info('Finished rebuild of PT tables')

    def test_shared_mem_is_cleaned(self):
        """
        [feature]: Check if the shared memory is cleaned
        
        """
        newfault = Fault()
        seginfo = newfault.get_seginfo(preferred_role='p',content=0) 
        cmd = Command('check for shared memory', cmdStr="ipcs -a", ctxt=REMOTE, remoteHost=seginfo.getSegmentHostName())
        cmd.run(validateAfter=True)
        result = cmd.get_results().stdout.split('\n')
        for r in result:
            if r and r.split()[-1] == '0':
                raise Exception('Shared memory not cleaned up for %s' % r)

    def test_wait_till_segments_in_change_tracking(self):
        """
        [feature]: Wait until segments for into change tracking
        
        """
        newfault = Fault()
        rtrycnt = 0
        while( not newfault.is_changetracking()):
            tinctest.logger.info("Waiting [%s] for DB to go in CT mode" %rtrycnt)
            rtrycnt = rtrycnt + 1

class GprecoversegClass(TINCTestCase):
    """
    
    @description Performs different types of Recovery process.
    @created 2009-01-27 14:00:00
    @modified 2013-09-12 17:10:15
    @tags storage schema_topology 
    @product_version gpdb:4.2.x,gpdb:main
    """

    def test_recovery_with_new_loc(self):
        """
        [feature]: Performs recovery by creating a configuration file with new segment locations 
        
        """

        newfault = Fault()
        config = GPDBConfig()
        hosts = newfault.get_segment_host()
        newfault.create_new_loc_config(hosts, orig_filename='recovery.conf', new_filename='recovery_new.conf')
        if not newfault.run_recovery_with_config(filename='recovery_new.conf'):
            self.fail("*** Incremental recovery with config file recovery_new.conf failed")
        rtrycnt = 0
        while (not config.is_not_insync_segments()):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            rtrycnt = rtrycnt + 1
               
    def test_do_incremental_recovery(self):
        """
        [feature]: Performs Incremental Recovery 
        
        """

        config = GPDBConfig()
        recoverseg = GpRecoverseg()
        tinctest.logger.info('Running Incremental gprecoverseg...')
        recoverseg.run()
        rtrycnt = 0
        while (not config.is_not_insync_segments()):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            rtrycnt = rtrycnt + 1
    
    def test_do_full_recovery(self):
        """
        [feature]: Performs Full Recovery
        
        """

        config = GPDBConfig()
        recoverseg = GpRecoverseg()
        tinctest.logger.info('Running Full gprecoverseg...')
        recoverseg.run(option = '-F')
        rtrycnt = 0
        while (not config.is_not_insync_segments()):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            rtrycnt = rtrycnt + 1
            
    def test_invalid_state_recoverseg(self):
        """
        [feature]: Sets the ENV_VAR and runs the incremental recoverseg
        
        """
        '''  '''
        # setting the ENV_VAR
        os.environ[ENV_VAR] = '1'
        recoverseg = GpRecoverseg()
        config = GPDBConfig()
        tinctest.logger.info('Running Incremental gprecoverseg...')
        recoverseg.run()
        rtrycnt = 0
        while (not config.is_not_insync_segments()):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            rtrycnt = rtrycnt + 1

    def test_incremental_recovery_skip_persistent_tables_check(self):
        """
        [feature]: Run incremental recoverseg with persistent tables check option 
        
        """

        config = GPDBConfig()
        recoverseg = GpRecoverseg()
        tinctest.logger.info('Running gprecoverseg...')
        recoverseg.run()
        self.assertNotIn('Performing persistent table check', recoverseg.stdout)
        rtrycnt = 0
        while (not config.is_not_insync_segments()):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            rtrycnt = rtrycnt + 1

    def test_full_recovery_skip_persistent_tables_check(self):
        """
        [feature]: Run recoverseg with persistent tables check option 
        
        """

        config = GPDBConfig()
        recoverseg = GpRecoverseg()
        tinctest.logger.info('Running gprecoverseg...')
        recoverseg.run(option='-F')
        self.assertNotIn('Performing persistent table check', recoverseg.stdout)
        rtrycnt = 0
        while (not config.is_not_insync_segments()):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            rtrycnt = rtrycnt + 1

    def test_incremental_recovery_with_persistent_tables_corruption(self):
        """
        [feature]: Run incremental recoverseg with persistent tables corruption 
        
        """

        recoverseg = GpRecoverseg()
        tinctest.logger.info('Running gprecoverseg...')
        try:
            recoverseg.run(option='--persistent-check', validate=False)
        except Exception as e:
            tinctest.logger.info('Encountered exception while running incremental recovery with corrupt persistent table')
        self.assertIn('Performing persistent table check', recoverseg.stdout)

    def test_full_recovery_with_persistent_tables_corruption(self):
        """
        [feature]: Run recoverseg with persistent tables corruption 
        
        """

        recoverseg = GpRecoverseg()
        tinctest.logger.info('Running gprecoverseg...')
        try:
            recoverseg.run(option='-F --persistent-check', validate=False)
        except Exception as e:
            tinctest.logger.info('Encountered exception while running full recovery with corrupt persistent table')
        self.assertIn('Performing persistent table check', recoverseg.stdout)

class GPDBdbOps(TINCTestCase):
    """
    
    @description GPDB admin operations
    @created 2009-01-27 14:00:00
    @modified 2013-09-12 17:10:15
    @tags storage schema_topology 
    @product_version gpdb:4.2.x,gpdb:main
    """

    @classmethod
    def setUpClass(cls):
        super(GPDBdbOps,cls).setUpClass()
        tinctest.logger.info('GPDB Operations')

    def gprestartdb(self):
        ''' Restarts the Database '''
        newfault = Fault()
        newfault.stop_db()
        newfault.start_db()
        sleep(30)
        
    def check_if_not_in_preferred_role(self):
        ''' Checks if the segments are in preferred role or not '''
        newfault = Fault()
        result = newfault.check_if_not_in_preferred_role()
        if result == True:
           self.fail("Segments are not in preferred roles!!!")
    
         
class SegmentConfigurations(TINCTestCase):
    """
    
    @description Checks the segment's configuration for any invalid states
    @created 2009-01-27 14:00:00
    @modified 2013-09-12 17:10:15
    @tags storage schema_topology 
    @product_version gpdb:4.2.x,gpdb:main
    """
    
    @classmethod
    def setUpClass(cls):
        super(SegmentConfigurations,cls).setUpClass()
        tinctest.logger.info('Running all the invalid state tests...')
        # Sleep introduced so that the gprecoverseg starts before the invalid state tests
        tinctest.logger.info('Sleep introduced for 15 secs...')
        sleep(15)

    def test_if_primary_down(self):
        """
        [feature]: Check for invalid state - Primary is marked down
        
        """
        sql_stmt = "SELECT 'down_segment' FROM gp_segment_configuration " \
                    "WHERE role = 'p' " \
                    "AND status = 'd'"
        out = PSQL.run_sql_command(sql_stmt)
        if len(out) == 0:
            error_msg = 'Could not connect to the sever!!'
            tinctest.logger.error(error_msg)
            self.fail(error_msg)
        out = out.count('down_segment') - 1
        if out == 0:
            tinctest.logger.info('Primary is marked down => 0 rows')
        else:
            error_msg = "%s down segments found" %out
            tinctest.logger.info(error_msg)
            self.fail(error_msg)
            
    def test_if_mirror_down_and_primary_in_CT(self):
        """
        [feature]: Check for invalid state - Mirror is down but primary is not in change tracking 
        
        """
        sql_stmt = "SELECT p.content, p.dbid AS p_dbid, m.dbid AS m_dbid, " \
                   "p.role AS p_role, m.role AS m_role, " \
                   "p.preferred_role AS p_pref_role, m.preferred_role AS m_pref_role, " \
                   "p.address AS p_address, m.address AS m_address, " \
                   "p.status AS p_status, m.status AS m_status, " \
                   "p.mode AS p_mode, m.mode AS m_mode " \
                   "FROM gp_segment_configuration p, gp_segment_configuration m " \
                   "WHERE ( (p.content = m.content) AND (p.dbid <> m.dbid) ) " \
                   "AND p.status = 'u' and m.status = 'd' " \
                   "AND p.mode <> 'c'" 
        out = PSQL.run_sql_command(sql_stmt)
        if len(out) == 0:
            error_msg = 'Could not connect to the sever!!'
            tinctest.logger.error(error_msg)
            self.fail(error_msg)
        
        out = out.split('\n')[3].find('0 rows')
        if out > 0:
            tinctest.logger.info('Mirror is down but primary is not in change tracking => 0 rows')
        else:
            error_msg = "%s down segments found" %out
            tinctest.logger.info(error_msg)
            self.fail(error_msg)
                
    def test_if_primary_in_CT_but_mirror_not_down(self):
        """
        [feature]: Check for invalid state -  Primary is in change tracking but mirror is not down
        
        """
        sql_stmt = "SELECT p.content, p.dbid AS p_dbid, m.dbid AS m_dbid, " \
                   "p.role AS p_role, m.role AS m_role, " \
                   "p.preferred_role AS p_pref_role, m.preferred_role AS m_pref_role, " \
                   "p.address AS p_address, m.address AS m_address, " \
                   "p.status AS p_status, m.status AS m_status, " \
                   "p.mode AS p_mode, m.mode AS m_mode " \
                   "FROM gp_segment_configuration p, gp_segment_configuration m " \
                   "WHERE ( (p.content = m.content) AND (p.dbid <> m.dbid) ) " \
                   "AND p.status = 'u' and p.mode = 'c' " \
                   "AND m.status <> 'd'" 
        out = PSQL.run_sql_command(sql_stmt)
        if len(out) == 0:
            error_msg = 'Could not connect to the sever!!'
            tinctest.logger.error(error_msg)
            self.fail(error_msg)
        out = out.split('\n')[3].find('0 rows')
        if out > 0:
            tinctest.logger.info('Primary is in change tracking but mirror is not down => 0 rows')
        else:
            error_msg = "%s down segments found" %out
            tinctest.logger.info(error_msg)
            self.fail(error_msg)
         
    def test_if_primary_up_resync_and_mirror_down_not_in_resync(self):
        """
        [feature]: Check for invalid state - Primary is Up/In resync, Mirror is not in resync or is marked down
        
        """
        sql_stmt = "SELECT p.content, p.dbid AS p_dbid, m.dbid AS m_dbid, " \
                   "p.role AS p_role, m.role AS m_role, " \
                   "p.preferred_role AS p_pref_role, m.preferred_role AS m_pref_role, " \
                   "p.address AS p_address, m.address AS m_address, " \
                   "p.status AS p_status, m.status AS m_status, " \
                   "p.mode AS p_mode, m.mode AS m_mode " \
                   "FROM gp_segment_configuration p, gp_segment_configuration m " \
                   "WHERE ( (p.content = m.content) AND (p.dbid <> m.dbid) ) " \
                   "AND p.status = 'u' and p.mode = 'r' " \
                   "AND ( (m.mode <> 'r') OR (m.status = 'd') )" 
        out = PSQL.run_sql_command(sql_stmt)
        if len(out) == 0:
            error_msg = 'Could not connect to the sever!!'
            tinctest.logger.error(error_msg)
            self.fail(error_msg)
        out = out.split('\n')[3].find('0 rows')
        if out > 0:
            tinctest.logger.info('Primary is Up/In resync, Mirror is not in resync or is marked down => 0 rows')
        else:
            error_msg = "%s down segments found" %out
            tinctest.logger.info(error_msg)
            self.fail(error_msg)
