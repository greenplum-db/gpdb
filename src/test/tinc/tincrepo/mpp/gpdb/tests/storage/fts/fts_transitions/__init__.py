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
from time import sleep
import tinctest

from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from tinctest.lib import local_path, run_shell_command
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gprecoverseg import GpRecover
from gppylib.gparray import GpArray
from gppylib.db import dbconn
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.gpdb.tests.storage.lib.common_utils import Gpstate
from mpp.gpdb.tests.storage.lib.common_utils import Gpprimarymirror
from gppylib.commands.base import Command
from mpp.models import MPPTestCase
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop

class FtsTransitions(MPPTestCase):

    def __init__(self, methodName):
        self.pgport = os.environ.get('PGPORT')
        self.fileutil = Filerepe2e_Util()
        self.gpstate = Gpstate()
        self.gpprimarymirror = Gpprimarymirror()
        self.gpstart = GpStart()
        self.gpstop = GpStop()
        super(FtsTransitions,self).__init__(methodName)

    def kill_first_mirror(self):
        mirror_data_loc = self.get_default_fs_loc(role='m',content=0)
        gpconfig = GPDBConfig()
        (host, port) = gpconfig.get_hostandport_of_segment(psegmentNumber = 0, pRole = 'm')    
        cmdString = 'ps -ef|grep -v grep|grep \'%s\'|awk \'{print $2}\'|xargs kill -9'%mirror_data_loc
        remote = Command(name ='kill first mirror', cmdStr = cmdString, ctxt=2, remoteHost=host)
        remote.run() 
        tinctest.logger.info('run command %s'%cmdString)
        rc = remote.get_results().rc    
        result = remote.get_results().stdout
        tinctest.logger.info('Command returning, rc: %s, result: %s'%(rc,result))

    def kill_master_process(self, ProcName=None):
        cmdString = 'ps -ef|grep postgres| grep %s | grep \'%s\'| awk \'{print $2}\'|xargs kill -9'%(self.pgport,ProcName) 
        cmd = Command('kill process on master', cmdStr = cmdString)
        cmd.run()
        tinctest.logger.info('run command %s'%cmdString)
        rc = cmd.get_results().rc    
        result = cmd.get_results().stdout
        tinctest.logger.info('Command returning, rc: %s, result: %s'%(rc,result))


    def get_default_fs_loc(self, role='m', content=0):
        fs_sql = '''select fselocation from pg_filespace_entry
                    where fsefsoid = 3052 and fsedbid = (select dbid from gp_segment_configuration
                    where role = \'%s\' and content = %s);'''%(role,content)
        result = PSQL.run_sql_command(fs_sql, flags = '-q -t', dbname= 'template1')
        result = result.strip()
        filespace_loc = result.split('\n')
        return filespace_loc[0]
  
    def set_faults(self,fault_name, type, role='mirror', port=None, occurence=None, sleeptime=None, seg_id=None):
        ''' Reset the fault and then issue the fault with the given type'''
        self.fileutil.inject_fault(f=fault_name, y=type, r=role, p=port , o=occurence, sleeptime=sleeptime, seg_id=seg_id)

    def resume_faults(self,fault_name, role='mirror'):
        ''' Resume the fault issues '''
        self.fileutil.inject_fault(f=fault_name, y='resume', r=role)

    def corrupt_ct_logfile(self, write_string='*', corruption_offset=1, start_of_file=True):
        # Force flush CT file to disk
        PSQL.run_sql_command("CHECKPOINT", flags = '-q -t', dbname= 'postgres')

        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
        for seg in primary_segs:
            file = '%s/pg_changetracking/CT_LOG_FULL' % seg.datadir
            try:
                with open(file, "r+b") as f:
                    if start_of_file:
                        f.seek(corruption_offset, 0)
                    else:
                        f.seek(corruption_offset, 2)
                    f.write(write_string)
                    f.close()
            except Exception, e:
                pass

    def incremental_recoverseg(self, workerPool=False):
        gprecover = GpRecover(GPDBConfig())
        gprecover.incremental(workerPool)

    def full_recoverseg(self):
        gprecover = GpRecover(GPDBConfig())
        gprecover.full()

    def run_recoverseg_if_ct(self):
        gpconfig = GPDBConfig()
        num_down = gpconfig.count_of_nodes_in_mode('c')
        if (int(num_down) > 0):
            self.incremental_recoverseg()

    def wait_till_change_tracking(self):
        self.fileutil.wait_till_change_tracking_transition()

    def wait_till_insync(self):
        gprecover = GpRecover(GPDBConfig())
        gprecover.wait_till_insync_transition()

    def run_gpstate(self, type, phase):
        self.gpstate.run_gpstate(type, phase)

    def run_gpprimarymirror(self):
        self.gpprimarymirror.run_gpprimarymirror()

    def verify_gpprimarymirror_output(self, total_resync=0, cur_resync=0):
        status = self.gpprimarymirror.verify_gpprimarymirror_output(total_resync, cur_resync)
        self.assertTrue(status, 'Total and Cur resync object count mismatch')

    def run_gpstate_shell_cmd(self, options):
        self.gpstate.run_gpstate_shell_cmd(options)

    def verify_gpstate_output(self):
        status = self.gpstate.verify_gpstate_output()
        self.assertTrue(status, 'Total and Cur resync object count mismatch')

    def run_trigger_sql(self, wait_for_db=True):
        ''' Run a sql statement to trigger postmaster reset '''
        PSQL.run_sql_file(local_path('test_ddl.sql'))
        if wait_for_db:
            PSQL.wait_for_database_up()

    def run_fts_test_ddl_dml(self):
        PSQL.run_sql_file(local_path('fts_test_ddl_dml.sql'))

    def run_fts_test_ddl_dml_before_ct(self):
        PSQL.run_sql_file(local_path('fts_test_ddl_dml_before_ct.sql'))

    def run_fts_test_ddl_dml_ct(self):
        PSQL.run_sql_file(local_path('fts_test_ddl_dml_ct.sql'))

    def run_fts_test_ddl_dml_checksum_ct_recoverseg(self):
        PSQL.run_sql_file(local_path('fts_test_ddl_dml_checksum_ct_recoverseg.sql'))

    def restart_db(self):
        self.gpstop.run_gpstop_cmd(immediate = True)
        self.gpstart.run_gpstart_cmd()

    def stop_db_with_no_rc_check(self):
        ''' Gpstop and dont check for rc '''
        cmd = Command('Gpstop_a', 'gpstop -a')
        tinctest.logger.info('Executing command: gpstop -a')
        cmd.run()

    def start_db_with_no_rc_check(self):
        ''' Gpstart and dont check for rc '''
        cmd = Command('Gpstart_a', 'gpstart -a')
        tinctest.logger.info('Executing command: gpstart -a')
        cmd.run()

    def restart_db_with_no_rc_check(self):
        self.stop_db_with_no_rc_check()
        self.start_db_with_no_rc_check()

    def check_fault_status(self, fault_name, seg_id=None, role=None):
        status = self.fileutil.check_fault_status(fault_name = fault_name, status ='triggered', max_cycle=20, role=role, seg_id=seg_id)
        self.assertTrue(status, 'The fault is not triggered in the time expected')

    def cluster_state(self):
        gpconfig = GPDBConfig()
        state = gpconfig.is_not_insync_segments()
        self.assertTrue(state,'The cluster is not up and in sync')
