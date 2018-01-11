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

import os, string, sys
import tinctest
from gppylib.commands.base import Command, REMOTE
from tinctest.lib import local_path, run_shell_command#, Gpdiff

from mpp.lib.PSQL import PSQL
from time import sleep
from mpp.lib.config import GPDBConfig


class Filerepe2e_Util():
    """ 
        Util class for filerep
    """ 

    def wait_till_change_tracking_transition(self,num_seg=None):
        """
        PURPOSE:
            Poll till change tracking state achieved: Wait till all segments transition to change tracking state
        @num_seg : Excepted number of segments down. If not given checks for all segments
        @return:
            True [if success] False [if state not in ct for more than 600 secs]
            number of nodes not in ct    
         
        """
        gpcfg = GPDBConfig() 
        if num_seg is None:
            num_seg = gpcfg.get_countprimarysegments()
        num_cl = gpcfg.count_of_nodes_in_mode('c')
        count = 0
        while(int(num_cl) < num_seg):
            tinctest.logger.info("waiting for DB to go into change tracking")
            sleep(10)
            num_cl = gpcfg.count_of_nodes_in_mode('c')
            count = count + 1
            if (count > 80):
               raise Exception("Timed out: cluster not in change tracking")
        return (True,num_cl)

    def inject_fault(self, y = None, f = None, r ='mirror', seg_id = None, H='ALL', m ='async', sleeptime = None,
                     o =None, p=None, outfile=None, table = None, database = None):
        '''
        PURPOSE : 
            Inject the fault using gpfaultinjector
        @param 
            y : suspend/resume/reset/panic/fault
            f : Name of the faulti
            outfile : output of the command is placed in this file
            rest_of_them : same as in gpfaultinjector help
        '''
        if (not y) or (not f) :
            raise Exception("Need a value for type and name to continue")
        
        if(not os.getenv('MASTER_DATA_DIRECTORY')):
             raise Exception('MASTER_DATA_DIRECTORY environment variable is not set.')
        
    
        fault_cmd = "gpfaultinjector  -f %s -m %s -y %s " % (f, m, y )
        if seg_id :
            fault_cmd = fault_cmd + " -s %s" % seg_id
        if sleeptime :
            fault_cmd = fault_cmd + " -z %s" % sleeptime
        if o != None:
            fault_cmd = fault_cmd + " -o %s" % o
        if p :
            fault_cmd = fault_cmd + " -p %s" % p
        if seg_id is None :
            fault_cmd = fault_cmd + " -H %s -r %s" % (H, r)
        if sleeptime :
            fault_cmd = fault_cmd + " --sleep_time_s %s " % sleeptime
        if table :
            fault_cmd = fault_cmd + " -t %s " % table
        if database :
            fault_cmd = fault_cmd + " -D %s " % database
        if outfile !=  None:
            fault_cmd = fault_cmd + ">" +outfile 
            
        cmd = Command('fault_command', fault_cmd)
        cmd.run()
        result = cmd.get_results()
        if result.rc != 0 and  y != 'status':
            ok = False
            out = result.stderr
        else:
            ok =  True
            out = result.stdout
        
        if not ok and y != 'status':
            raise Exception("Cmd %s Failed to inject fault %s to %s" % (fault_cmd, f,y))
        else:
            tinctest.logger.info('Injected fault %s ' % fault_cmd)
            return (ok,out)
       
    
    def check_fault_status(self,fault_name = None, status = None, max_cycle=20, role='primary', seg_id=None, num_times_hit = None):
        ''' 
        Check whether a fault is triggered. Poll till the fault is triggered
        @param name : Fault name
        @param status : Status to be checked - triggered/completed
        @param seg_id : db_id of the segment
        '''
        if (not fault_name) or (not status) :
            self.fail("Need a value for fault_name and status to continue")
    
        poll =0
        while(poll < max_cycle):
            (ok, out) = self.inject_fault(f=fault_name, y='status', r=role, seg_id=seg_id)
            poll +=1
            for line in out.splitlines():
                if line.find(fault_name) > 0 and line.find(status) > 0 :
                    if num_times_hit and line.find("num times hit:'%d'" % num_times_hit) < 0 :
                        tinctest.logger.info('Fault not hit num of times %d line %s ' % (num_times_hit,line))
                        continue
                    if num_times_hit:
                        tinctest.logger.info('Fault %s is %s num_times_hit %d' % (fault_name,status, num_times_hit))
                    poll = 0 
                    return True
            #sleep a while before start polling again
            sleep(10)
        return False
