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
import sys
import glob
from time import sleep
import tinctest
from tinctest.lib  import local_path
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL
from tinctest.lib import local_path, Gpdiff
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop
from mpp.lib.config import GPDBConfig
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.gpdbverify import GpdbVerify
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.lib.dbstate import DbStateClass
from mpp.gpdb.tests.storage.lib.common_utils import *


class GPDBStorageBaseTestCase():
    '''
    Base Class for Storage test-suits like Crash Recovery, 
    Pg_Two_Phase, sub_transaction
    '''

    def __init__(self, config=None):
        if config is not None:
            self.config = config
        else:
            self.config = GPDBConfig()

        self.filereputil = Filerepe2e_Util()
        self.gprecover = GpRecover(self.config)
        self.gpstop = GpStop()
        self.gpstart = GpStart()
        self.gpfile = Gpfilespace(self.config)
        self.gpverify = GpdbVerify(config=self.config)
        self.dbstate = DbStateClass('run_validation', self.config)
        self.port = os.getenv('PGPORT')

    def invoke_fault(self, fault_name, type, role='mirror', port=None, occurence=None, sleeptime=None, seg_id=None):
        ''' Reset the fault and then issue the fault with the given type'''
        self.filereputil.inject_fault(f=fault_name, y='reset', r=role, p=port , o=occurence, sleeptime=sleeptime, seg_id=seg_id)
        self.filereputil.inject_fault(f=fault_name, y=type, r=role, p=port , o=occurence, sleeptime=sleeptime, seg_id=seg_id)
        tinctest.logger.info('Successfully injected fault_name : %s fault_type : %s  occurence : %s ' % (fault_name, type, occurence))

    def start_db(self):
        '''Gpstart '''
        rc = self.gpstart.run_gpstart_cmd()
        if not rc:
            raise Exception('Failed to start the cluster')
        tinctest.logger.info('Started the cluster successfully')

    def stop_db(self):
        ''' Gpstop and dont check for rc '''
        cmd = Command('Gpstop_a', 'gpstop -a')
        tinctest.logger.info('Executing command: gpstop -a')
        cmd.run()


    def get_trigger_status(self, trigger_count,max_cnt=50):
        '''Compare the pg_stat_activity count with the total number of trigger_sqls executed '''
        psql_count=0
        for i in range(1,trigger_count):
            psql_count = PSQL.run_sql_command('select count(*) from pg_stat_activity;', flags='-q -t', dbname='postgres')
            sleep(1) 
        tinctest.logger.info('Count of trigger sqls %s And it should be %s' % (psql_count, trigger_count))
        if psql_count < trigger_count :
            tinctest.logger.info('coming to the if loop in get_trigger_status')
            return False
        return True


    def check_trigger_sql_hang(self, test_dir):
        '''
        @param ddl_type : create/drop
        @param fault_type : commit/abort/end_prepare_two_phase_sleep
        @description : Return the status of the trigger sqls: whether they are waiting on the fault 
        Since gpfaultinjector has no way to check if all the sqls are triggered, we are using 
        a count(*) on pg_stat_activity and compare the total number of trigger_sqls
        '''
        trigger_dir = local_path('%s_tests/trigger_sql/' % (test_dir))
        trigger_count = len(glob.glob1(trigger_dir,"*.ans"))
        return self.get_trigger_status(trigger_count)


    def get_items_list(test_file):
        ''' Get file contents to a list '''
        with open(test_file, 'r') as f:
             test_list = [line.strip() for line in f]
        return test_list

    def validate_sql(filename):
        ''' Compare the out and ans files '''
        out_file = local_path(filename.replace(".sql", ".out"))
        ans_file = local_path(filename.replace('.sql' , '.ans'))
        assert Gpdiff.are_files_equal(out_file, ans_file)

    def run_sql(filename, verify=True):
        ''' Run the provided sql and validate it '''
        out_file = local_path(filename.replace(".sql", ".out"))
        PSQL.run_sql_file(sql_file = filename, out_file = out_file)
        if verify == True:
           validate_sql(filename)

