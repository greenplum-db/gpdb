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
from tinctest.lib import local_path, Gpdiff
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command
from mpp.lib.gpstart import GpStart
from mpp.lib.gpstop import GpStop
from mpp.models import MPPTestCase
from mpp.lib.gpConfig import GpConfig


class UDFExceptionHandling(MPPTestCase):
    
    def __init__(self):
        self.gpstart = GpStart()
        self.gpstop = GpStop()
        self.config = GpConfig()
        self.port = os.getenv('PGPORT')
        self.gphome = (os.getenv('GPHOME'))
        self.base_dir = os.path.dirname(sys.modules[self.__class__.__module__].__file__)


    def get_sql_files(self, sql_file_name):
        sql_file = os.path.join( self.base_dir, "sql", sql_file_name + ".sql");    
        return  sql_file

    def validate_sql(self, ans_file, out_file):
        ''' Compare the out and ans files '''
        init_file=os.path.join( self.base_dir, "sql",'init_file')
        result1 = Gpdiff.are_files_equal(out_file, ans_file, match_sub =[init_file])
        self.assertTrue(result1 ,'Gpdiff.are_files_equal')        

    def run_sql(self, filename, out_file):
        ''' Run the provided sql and validate it '''
        out_file = local_path(filename.replace(".sql", ".out"))
        PSQL.run_sql_file(filename,out_file=out_file)

    def set_protocol_conf( self, debug_dtm_action_segment, debug_dtm_action_target, debug_dtm_action_protocol,debug_dtm_action,debug_dtm_action_nestinglevel): 
        tinctest.logger.info('Configuring debug config parameters') 
        self.set_guc('debug_dtm_action_segment', debug_dtm_action_segment)
        tinctest.logger.info('Configuring debug_dtm_action_segment = %s ' % debug_dtm_action_segment)
        self.set_guc('debug_dtm_action_target', debug_dtm_action_target)
        tinctest.logger.info('Configuring debug_dtm_action_target = %s ' % debug_dtm_action_target)
        self.set_guc('debug_dtm_action_protocol', debug_dtm_action_protocol)
        tinctest.logger.info('Configuring debug_dtm_action_protocol = %s ' % debug_dtm_action_protocol)
        self.set_guc('debug_dtm_action', debug_dtm_action)
        tinctest.logger.info('Configuring debug_dtm_action = %s ' % debug_dtm_action)
        self.set_guc('debug_dtm_action_nestinglevel', debug_dtm_action_nestinglevel)
        tinctest.logger.info('Configuring debug_dtm_action_nestinglevel = %s ' % debug_dtm_action_nestinglevel)
        
    def set_sql_conf( self, debug_dtm_action_segment, debug_dtm_action_target, debug_dtm_action_sql_command_tag,debug_dtm_action,debug_dtm_action_nestinglevel): 
        tinctest.logger.info('Configuring debug config parameters') 
        self.set_guc('debug_dtm_action_segment', debug_dtm_action_segment)
        tinctest.logger.info('Configuring debug_dtm_action_segment = %s ' % debug_dtm_action_segment)
        self.set_guc('debug_dtm_action_target', debug_dtm_action_target)
        tinctest.logger.info('Configuring debug_dtm_action_target = %s ' % debug_dtm_action_target)
        self.set_guc('debug_dtm_action_sql_command_tag', debug_dtm_action_sql_command_tag)
        tinctest.logger.info('Configuring debug_dtm_action_sql_command_tag = %s ' % debug_dtm_action_sql_command_tag)
        self.set_guc('debug_dtm_action', debug_dtm_action)
        tinctest.logger.info('Configuring debug_dtm_action = %s ' % debug_dtm_action)
        self.set_guc('debug_dtm_action_nestinglevel', debug_dtm_action_nestinglevel)
        tinctest.logger.info('Configuring debug_dtm_action_nestinglevel = %s ' % debug_dtm_action_nestinglevel)

    def set_guc(self, guc_name, guc_value):
        # Set the guc value
        tinctest.logger.info('Configuring ' + guc_name +' ...')
        cmd_str='source ' + self.gphome+ '/greenplum_path.sh;gpconfig -c ' + guc_name + ' -v ' +guc_value +' --skipvalidation'
        cmd=Command("gpconfig " ,cmd_str) 
        cmd.run()
        self.assertTrue(int(cmd.get_results().rc) == 0,cmd_str)
        # Load the new value to the db
        tinctest.logger.info('gpstop -u to reload config files...')
        cmd_str2='source '+ self.gphome+ '/greenplum_path.sh;gpstop -u'
        cmd = Command("gpstop -u", cmd_str2)
        cmd.run()
        self.assertTrue(int(cmd.get_results().rc) == 0,cmd_str2)
 


    def reset_protocol_conf( self):
        # Unset the guc value
        tinctest.logger.info('Reset debug config parameters...Begin') 
        self.set_guc('debug_dtm_action_segment', '0')
        self.set_guc('debug_dtm_action_target', 'none')
        self.set_guc('debug_dtm_action_protocol', 'none')
        self.set_guc('debug_dtm_action', 'none')
        self.set_guc('debug_dtm_action_nestinglevel','0')
        self.set_guc('debug_dtm_action_sql_command_tag','none')
        tinctest.logger.info('Reset debug config parameters...End') 

    
    def get_output_file(self, debug_dtm_action_segment,debug_dtm_action_target, debug_dtm_action_protocol, debug_dtm_action, debug_dtm_action_nestinglevel):
        file_name='_seg'+debug_dtm_action_segment
        if debug_dtm_action_target == 'protocol':
           # debug_dtm_action_protocol
           if debug_dtm_action_protocol == 'subtransaction_begin':
              file_name=file_name+'_subtxtbeg'
           elif debug_dtm_action_protocol == 'subtransaction_rollback':
              file_name=file_name+'_subtxtrolbk'
           else:
              file_name=file_name+'_subtxtrelse'
        if debug_dtm_action_target == 'sql':
           file_name=file_name+'_mppexec' 
        # debug_dtm_action           
        if debug_dtm_action == 'fail_begin_command':
            file_name=file_name+'_failbeg'
        elif debug_dtm_action == 'fail_end_command':
            file_name=file_name+'_failend'
        elif debug_dtm_action == 'panic_begin_command':
            file_name=file_name+'_panicbeg'
        # debug_dtm_action_nestinglevel
        file_name = file_name+'_lvl'+debug_dtm_action_nestinglevel 
        tinctest.logger.info('Test ans and out file name : %s ' % file_name)
        return file_name


    def run_test(self, debug_dtm_action_segment, debug_dtm_action_target, debug_dtm_action_protocol, debug_dtm_action, debug_dtm_action_nestinglevel):
        file_name =''
        file_name = 'protocol'+self.get_output_file(debug_dtm_action_segment,debug_dtm_action_target,debug_dtm_action_protocol, debug_dtm_action, debug_dtm_action_nestinglevel) 
        test_name = 'udf_exception_handling_'+debug_dtm_action_target+'_seg'+ debug_dtm_action_segment
        sql_file = self.get_sql_files(test_name)
        out_file =  self.base_dir+ "/sql/"+test_name+'.out'
        out_file2 =  self.base_dir+ "/sql/"+file_name+'.out'
        ans_file =  self.base_dir+ "/expected/"+file_name+'.ans'
        tinctest.logger.info( 'sql-file == %s \n' % sql_file)
        tinctest.logger.info( 'out-file == %s \n' % out_file)
        tinctest.logger.info( 'ans-file == %s \n' % ans_file)
        self.run_sql(sql_file, out_file=out_file)
        cmd_str='cp ' + out_file + ' ' + out_file2
        cmd=Command("bak outfile " ,cmd_str) 
        cmd.run()
        self.assertTrue(int(cmd.get_results().rc) == 0,cmd_str)
        self.validate_sql(ans_file,out_file2)
        
