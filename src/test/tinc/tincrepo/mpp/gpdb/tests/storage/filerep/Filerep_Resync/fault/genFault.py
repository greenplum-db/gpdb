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
import unittest2 as unittest

from time import sleep
from tinctest.lib import local_path
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from subprocess import Popen, PIPE

'''
Creates Faults for the scenario
'''
class Fault(Command):
    def __init__(self, cmd = None):
        Command.__init__(self, 'Running fault command', cmd)

    def _run_sys_cmd(self, cmd_str, validate = False):
        '''helper function to run a sys cmd'''
        tinctest.logger.info("execute:" +cmd_str)
        cmd = Fault(cmd_str)
        cmd.run(validateAfter = validate)
        return cmd.get_results()
  
    def get_host_port_mapping(self,role):
        """ 
        Returns a dictionary having key as hostname and value as a list of port nos.
        For e.g {'vm9':['22001','22000'] , 'vm10':{'42000','42001'}...}
        """        
        config = GPDBConfig()
        no_of_segments = config.get_countprimarysegments()
        hosts_dict = {}
        counter = 0
        while counter < no_of_segments:
            (host,port) = config.get_hostandport_of_segment(counter,role)
            if hosts_dict.has_key(host):
                hosts_dict[host].append(port)
            else:
                hosts_dict[host] = list()
                hosts_dict[host].append(port)
            counter += 1
        return hosts_dict
        
    def kill_processes_with_role(self,role = 'm'):
        '''kills all the data segment processes'''
        hosts_dict = self.get_host_port_mapping(role)
        # Kill all the postgres intsances of the concerned segment role running
        # on a particular host using the port nos.
        for host in hosts_dict:
            ports_list = hosts_dict[host]
            # Create a structure ('port1'|'port2'|...) from the list of ports
            # this is further passed as pattern to the grep expression.
            segment_ports = "("
            for port in ports_list[:-1]:
                segment_ports += port + "|" 
            segment_ports = segment_ports + ports_list[len(ports_list)-1] + ")"
            
            sys_cmd = "gpssh -h %s  -e ' ps aux | egrep '\\\''postgres -D.*-p %s'\\\'' | awk '\\\''{print \"kill -9 \"$2}'\\\'' | sh' "%(host,segment_ports)
            tinctest.logger.info("kill process command : %s"%sys_cmd)
            result = self._run_sys_cmd(sys_cmd)
        
    def run_recovery(self):
        '''Runs the incremental recovery'''
        tinctest.logger.info('Invoking gprecoverseg to bring up the mirrors')
        cmd_str = "gprecoverseg -a"
        result = self._run_sys_cmd(cmd_str, True)
        '''sleep introduced so that the gpdb tables are updated before querying'''
        tinctest.logger.info('Delaying next step for 30 secs..')
        sleep(30)
        return result.stdout
        
    def are_mirrors_up(self):
        '''Checks if the mirrors are up or not after recovery'''
        tinctest.logger.info('Checking if the mirrors are up or not')
        cmd_str = "select 'down_segment' from gp_segment_configuration where preferred_role = 'm' and status = 'd'"
        out = PSQL.run_sql_command(cmd_str).count('down_segment') - 1
        tinctest.logger.info(str(out)+' down segments found.')
        if out > 0:
            return False
        else:
            return True
