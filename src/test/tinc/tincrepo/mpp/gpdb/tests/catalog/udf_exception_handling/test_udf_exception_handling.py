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
import glob
import sys
import tinctest
from tinctest.lib import local_path, Gpdiff
from mpp.models import MPPTestCase
from tinctest.models.scenario import ScenarioTestCase
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.catalog.udf_exception_handling.udf import UDFExceptionHandling


class UDFTestCase(ScenarioTestCase, MPPTestCase):
    ''' Testing exception handling in subtransactions '''
    
    def __init__(self, methodName):
        super(UDFTestCase,self).__init__(methodName)
        self.udf_obj=UDFExceptionHandling()

    @classmethod
    def setUpClass(cls):
        super(UDFTestCase, cls).setUpClass()
        '''
        Create UDF to exercise subtransactions upfront. They will be used in the tests .
        '''
        base_dir = os.path.dirname(sys.modules[cls.__module__].__file__)
        udf_name = ['test_excep','test_protocol_allseg','setup_sql_exceptionhandling']
        for uname in udf_name:
            udfsql=''
            udfsql= os.path.join(os.path.dirname(sys.modules[cls.__module__].__file__), "sql")+'/'+uname+'.sql'
            udfans= os.path.join(os.path.dirname(sys.modules[cls.__module__].__file__), "expected")+'/'+uname+'.ans'
            udfout= os.path.join(os.path.dirname(sys.modules[cls.__module__].__file__), "sql")+'/'+uname+'.out'
            tinctest.logger.info( '\n Creating UDF :  %s' % udfsql )
            res=PSQL.run_sql_file(sql_file = udfsql,out_file=udfout)
            init_file=os.path.join( base_dir, "sql",'init_file')
            result = Gpdiff.are_files_equal(udfout, udfans, match_sub =[init_file])
            assert result, 'Gpdiff are not equal' 

            

    def test_UDF_exception(self):
        ''' 
        
        @param debug_dtm_action_segment 
        @param debug_dtm_action_target 
        @param debug_dtm_action_protocol  or debug_dtm_action_sql_command_tag
        @param debug_dtm_action 
        @param debug_dtm_action_nestinglevel
        @description: This tests the Exception Handling of GPDB PL/PgSQL UDF 
                      It exercises ;
                         1. PROTOCOL or SQL type of dtm_action_target
                         2. Various levels of sub-transactions
                         3. dtm_action_protocol(PROTOCOL): subtransaction_begin, subtransaction_rollback or subtransaction_release 
                            or
                            debug_dtm_action_sql_command_tag(SQL) : 'MPPEXEC UPDATE'
                         4. dtm_action: fail_begin_command, fail_end_command or panic_begin_comand
        
        debug_dtm_action: Using this can specify what action to be triggered/simulated and at what point like 
                          error / panic / delay and at start or end command after receiving by the segment.

        debug_dtm_action_segment: Using this can specify segment number to trigger the specified dtm_action.

        debug_dtm_action_target: Allows to set target for specified dtm_action should it be DTM protocol 
                                 command or SQL command from master to segment.

        debug_dtm_action_protocol: Allows to specify sub-type of DTM protocol for which to perform specified 
                                   dtm_action (like prepare, abort_no_prepared, commit_prepared, abort_prepared, 
                                   subtransaction_begin, subtransaction_release, subtransaction_rollback, etc...
        debug_dtm_action_sql_command_tag: If debug_dtm_action_target is sql then this parameter can be used to set the type of sql
                                          that should trigger the exeception. Ex: 'MPPEXEC UPDATE'

        debug_dtm_action_nestinglevel: This allows to optional specify at which specific depth level in transaction 
                                       to take the specified dtm_action. This apples only to target with protocol and not SQL.
        STEPS:
        @data_provider data_types_provider
        '''
        tinctest.logger.info("\n ===============================================")
        tinctest.logger.info("\n Starting New Test: %s%s " % (self.test_data[0][0], self.test_data[0][1] ))
        debug_dtm_action_segment = self.test_data[1][0]
        debug_dtm_action_target = self.test_data[1][1]
        debug_dtm_action_protocol=''
        debug_dtm_action_sql_command_tag=''
        if debug_dtm_action_target == 'protocol':
            debug_dtm_action_protocol = self.test_data[1][2]
        elif debug_dtm_action_target == 'sql':
            debug_dtm_action_sql_command_tag = self.test_data[1][2]
        debug_dtm_action = self.test_data[1][3]
        debug_dtm_action_nestinglevel = self.test_data[1][4]
        
        tinctest.logger.info( '\ndebug_dtm_action_segment: %s' % debug_dtm_action_segment)
        tinctest.logger.info( 'debug_dtm_action_target: %s' % debug_dtm_action_target)
        tinctest.logger.info( 'debug_dtm_action_protocol: %s ' % debug_dtm_action_protocol)
        tinctest.logger.info( 'debug_dtm_action_sql_command_tag: %s ' % debug_dtm_action_sql_command_tag)
        tinctest.logger.info( 'debug_dtm_action: %s ' % debug_dtm_action)
        tinctest.logger.info( 'debug_dtm_action_nestinglevel: %s ' % debug_dtm_action_nestinglevel)
        tinctest.logger.info("\n ===============================================")

        self.udf_obj.reset_protocol_conf()
        if debug_dtm_action_target == 'protocol':
            self.udf_obj.set_protocol_conf(debug_dtm_action_segment, debug_dtm_action_target, debug_dtm_action_protocol,debug_dtm_action,debug_dtm_action_nestinglevel)
            self.udf_obj.run_test(debug_dtm_action_segment, debug_dtm_action_target, debug_dtm_action_protocol,debug_dtm_action,debug_dtm_action_nestinglevel)
        if debug_dtm_action_target == 'sql':
            self.udf_obj.set_sql_conf(debug_dtm_action_segment, debug_dtm_action_target, debug_dtm_action_sql_command_tag,debug_dtm_action,debug_dtm_action_nestinglevel)
            self.udf_obj.run_test(debug_dtm_action_segment, debug_dtm_action_target, debug_dtm_action_sql_command_tag,debug_dtm_action,debug_dtm_action_nestinglevel)
        
@tinctest.dataProvider('data_types_provider')
def test_data_provider():
    data = {
             '01_protocol_seg0_subtxnbegin_failbegcmd_nstlvl0': ['0','protocol','subtransaction_begin','fail_begin_command','0'],
             '02_protocol_seg0_subtxnbegin_failbegcmd_nstlvl3': ['0','protocol','subtransaction_begin','fail_begin_command','3'],
             '03_protocol_seg0_subtxnbegin_failbegcmd_nstlvl4': ['0','protocol','subtransaction_begin','fail_begin_command','4'],

             '04_protocol_seg0_subtxnrollbk_failbegcmd_nstlvl0': ['0','protocol','subtransaction_rollback','fail_begin_command','0'],
             '05_protocol_seg0_subtxnrollbk_failbegcmd_nstlvl3': ['0','protocol','subtransaction_rollback','fail_begin_command','3'],
             '06_protocol_seg0_subtxnrollbk_failbegcmd_nstlvl4': ['0','protocol','subtransaction_rollback','fail_begin_command','4'],

             '07_protocol_seg0_subtxnrelse_failbegcmd_nstlvl0': ['0','protocol','subtransaction_release','fail_begin_command','0'], 
             '08_protocol_seg0_subtxnrelse_failbegcmd_nstlvl4': ['0','protocol','subtransaction_release','fail_begin_command','4'],
             '09_protocol_seg0_subtxnrelse_failbegcmd_nstlvl5': ['0','protocol','subtransaction_release','fail_begin_command','5'],

             '10_protocol_seg0_subtxnbegin_failendcmd_nstlvl0': ['0','protocol','subtransaction_begin','fail_end_command','0'],
             '11_protocol_seg0_subtxnbegin_failendcmd_nstlvl3': ['0','protocol','subtransaction_begin','fail_end_command','3'],
             '12_protocol_seg0_subtxnbegin_failendcmd_nstlvl4': ['0','protocol','subtransaction_begin','fail_end_command','4'],

             '13_protocol_seg0_subtxnrollbk_failendcmd_nstlvl0': ['0','protocol','subtransaction_rollback','fail_end_command','0'],
             '14_protocol_seg0_subtxnrollbk_failendcmd_nstlvl3': ['0','protocol','subtransaction_rollback','fail_end_command','3'],
             '15_protocol_seg0_subtxnrollbk_failendcmd_nstlvl4': ['0','protocol','subtransaction_rollback','fail_end_command','4'],

             '16_protocol_seg0_subtxnrelse_failendcmd_nstlvl0': ['0','protocol','subtransaction_release','fail_end_command','0'],
             '17_protocol_seg0_subtxnrelse_failendcmd_nstlvl4': ['0','protocol','subtransaction_release','fail_end_command','4'],
             '18_protocol_seg0_subtxnrelse_failendcmd_nstlvl5': ['0','protocol','subtransaction_release','fail_end_command','5'],

             '19_protocol_seg0_subtxnbegin_panicbegcmd_nstlvl0': ['0','protocol','subtransaction_begin','panic_begin_command','0'],
             '20_protocol_seg0_subtxnbegin_panicbegcmd_nstlvl3': ['0','protocol','subtransaction_begin','panic_begin_command','3'],
             '21_protocol_seg0_subtxnbegin_panicbegcmd_nstlvl4': ['0','protocol','subtransaction_begin','panic_begin_command','4'],

             '22_protocol_seg0_subtxnrollbk_panicbegcmd_nstlvl0': ['0','protocol','subtransaction_rollback','panic_begin_command','0'],
             '23_protocol_seg0_subtxnrollbk_panicbegcmd_nstlvl3': ['0','protocol','subtransaction_rollback','panic_begin_command','3'],
             '24_protocol_seg0_subtxnrollbk_panicbegcmd_nstlvl4': ['0','protocol','subtransaction_rollback','panic_begin_command','4'],

             '25_protocol_seg0_subtxnrelse_panicbegcmd_nstlvl0': ['0','protocol','subtransaction_release','panic_begin_command','0'], 
             '26_protocol_seg0_subtxnrelse_panicbegcmd_nstlvl4': ['0','protocol','subtransaction_release','panic_begin_command','4'],
             '27_protocol_seg0_subtxnrelse_panicbegcmd_nstlvl5': ['0','protocol','subtransaction_release','panic_begin_command','5'],

             '28_protocol_seg1_subtxnbegin_panicbegcmd_nstlvl0': ['1','protocol','subtransaction_begin','panic_begin_command','0'],
             '29_protocol_seg1_subtxnbegin_panicbegcmd_nstlvl3': ['1','protocol','subtransaction_begin','panic_begin_command','3'],
             '30_protocol_seg1_subtxnbegin_panicbegcmd_nstlvl4': ['1','protocol','subtransaction_begin','panic_begin_command','4'],

             '31_protocol_seg1_subtxnrollbk_panicbegcmd_nstlvl0': ['1','protocol','subtransaction_rollback','panic_begin_command','0'],
             '32_protocol_seg1_subtxnrollbk_panicbegcmd_nstlvl3': ['1','protocol','subtransaction_rollback','panic_begin_command','3'],
             '33_protocol_seg1_subtxnrollbk_panicbegcmd_nstlvl4': ['1','protocol','subtransaction_rollback','panic_begin_command','4'],

             '34_protocol_seg1_subtxnrelse_panicbegcmd_nstlvl0': ['1','protocol','subtransaction_release','panic_begin_command','0'], 
             '35_protocol_seg1_subtxnrelse_panicbegcmd_nstlvl4': ['1','protocol','subtransaction_release','panic_begin_command','4'],
             '36_protocol_seg1_subtxnrelse_panicbegcmd_nstlvl5': ['1','protocol','subtransaction_release','panic_begin_command','5'],
             '37_sql_seg0_subtxnbegin_failbegcmd_nstlvl0': ['0','sql','"\'MPPEXEC UPDATE\'"','fail_begin_command','0'],
             '38_sql_seg0_subtxnbegin_failbegcmd_nstlvl0': ['0','sql','"\'MPPEXEC UPDATE\'"','fail_end_command','0']
            }
    return data
