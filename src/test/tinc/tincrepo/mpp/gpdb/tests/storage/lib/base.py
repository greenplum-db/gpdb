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
import tinctest
from tinctest.lib import run_shell_command
from mpp.lib.PSQL import PSQL
from mpp.lib.filerep_util import Filerepe2e_Util
from mpp.lib.gprecoverseg import GpRecover
from mpp.lib.gpstop import GpStop
from mpp.models import MPPTestCase

@tinctest.skipLoading('scenario')
class BaseClass(MPPTestCase):
    '''
    Base Class for Storage test-suites 
    '''

    def __init__(self,methodName):
        self.filereputil = Filerepe2e_Util()
        self.gprecover = GpRecover()
        super(BaseClass,self).__init__(methodName)
        

    def inject_fault(self, fault_name, type, role='mirror', port=None, occurence=None, sleeptime=None, seg_id=None):
        ''' Reset the fault and then issue the fault with the given type'''
        self.filereputil.inject_fault(f=fault_name, y='reset', r=role, p=port , o=occurence, sleeptime=sleeptime, seg_id=seg_id)
        self.filereputil.inject_fault(f=fault_name, y=type, r=role, p=port , o=occurence, sleeptime=sleeptime, seg_id=seg_id)
        tinctest.logger.info('Successfully injected fault_name : %s fault_type : %s  occurence : %s ' % (fault_name, type, occurence))
   
    def reset_fault(self, fault_name, role='mirror', port=None, occurence=None, sleeptime=None, seg_id=None):
        ''' Reset the fault '''
        self.filereputil.inject_fault(f=fault_name, y='reset', r=role, p=port , o=occurence, sleeptime=sleeptime, seg_id=seg_id)
        tinctest.logger.info('Successfully reset fault_name : %s fault_type : %s  occurence : %s ' % (fault_name, type, occurence))

    def check_fault_status(self, fault_name, seg_id=None, role=None):
        status = self.filereputil.check_fault_status(fault_name = fault_name, status ='triggered', max_cycle=20, role=role, seg_id=seg_id)
        self.assertTrue(status, 'The fault is not triggered in the time expected')

    def incremental_recoverseg(self):
        self.gprecover.incremental()

    def wait_till_change_tracking(self):
        self.filereputil.wait_till_change_tracking_transition()

    def run_sql_in_background(self, sql_cmd):
        PSQL.run_sql_command(sql_cmd, background=True)

    def wait_till_insync(self):
        self.gprecover.wait_till_insync_transition()

    def set_gpconfig(self, param, value, restart_for_config=True):
        ''' Set the configuration parameter using gpconfig '''
        command = "gpconfig -c %s -v \"\'%s\'\" --skipvalidation" % (param, value)
        rc = run_shell_command(command)
        if not rc:
            raise Exception('Unable to set the configuration parameter %s ' % param)

        gpstop = GpStop()
        if restart_for_config:
            gpstop.run_gpstop_cmd(restart=True)
        else:
            gpstop.run_gpstop_cmd(reload=True)

    def reset_gpconfig(self,param, restart_for_config=True):
        ''' Reset the configuration parameter '''
        command = "gpconfig -r %s " % (param)
        rc = run_shell_command(command)
        if not rc:
            raise Exception('Unable to reset the configuration parameter %s ' % param)
        gpstop = GpStop()
        if restart_for_config:
            gpstop.run_gpstop_cmd(restart=True)
        else:
            gpstop.run_gpstop_cmd(reload=True)
