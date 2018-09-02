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

import fnmatch
import os
import time
import tinctest
import re
import sys
import platform

from mpp.models import SQLTestCase
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command
#from gppylib import gpversion

'''
UDP IC  tests
'''

def runShellCommand( cmdstr, cmdname = 'shell command'):
    """
    Executes a given command string using gppylib.Command. Definite candidate for a move to
    tinctest.lib.
    @param cmdname - Name of the command
    @param cmdstr - Command string to be executed
    """
    cmd = Command(cmdname, cmdstr)
    tinctest.logger.info('Executing command: %s :  %s' %(cmdname, cmdstr))
    cmd.run()
    result = cmd.get_results()
    tinctest.logger.info('Finished command execution with return code ' + str(result.rc))
    tinctest.logger.debug('stdout: ' + result.stdout)
    tinctest.logger.debug('stderr: ' + result.stderr)
    if result.rc != 0:
        return False
    return True

def setGUC(pathname,**GUC):

        input = open(pathname+'.sql')
        lines = input.readlines()
        input.close()
        output = open(pathname+'.sql', 'w')
        for line in lines:
                found=0
                for name in GUC:
                    str='SET' + ' ' + name
                    if line.startswith(str):
                        str1=str + '=' + '%s'%GUC[name] + ';' + '\n'
                        output.write(str1)
                        found=1  
                        break
                if found:
                    continue
                else:
                    output.write(line)
        output.close()

        input = open(pathname+'.ans')
        lines = input.readlines()
        input.close()
        output = open(pathname+'.ans', 'w')
        for line in lines:
                found=0
                for name in GUC:
                    str='SET' + ' ' + name       
                    if line.startswith(str):
 
                        str1=str + '=' + '%s'%GUC[name] + ';' + '\n'
                        output.write(str1)              
                        found=1  
                        break
                if found:
                    continue
                else:
                    output.write(line)
        output.close()

def checkDIS(pathname,array):
    log=open(pathname,'r')
    lines=log.readlines()
    log.close()
    cout=0
    for value in array:
        if value:
            patern='.+DIS\s+\w+\s+%d\s+'%value
        else:
            patern='.+DIS\s+'
        r1=re.compile(patern)
        for line in lines:
            if r1.search(line):
                cout=cout+1
                break
    if cout==len(array):
        return True
    else:
        return False

class UDPICTestCases(SQLTestCase):
    """
    @product_version gpdb: [4.3-]
    """
    common_sql = 'common/'
    is_version_skip =0 
    '''
    sol: ('', '', '')
    suse: ('SUSE Linux Enterprise Server ', '10', 'x86_64')
    redhat: ('Red Hat Enterprise Linux Server', '6.2', 'Santiago')
    '''
 
    @classmethod
    def setUpClass(cls):
        pass

    def __init__(self, methodName):
        super(UDPICTestCases, self).__init__(methodName)
        self.infer_metadata()
        #self.cluster = "init"

    def infer_metadata(self):
        intended_docstring = ""
        sql_file = local_path(self.common_sql + str(self._testMethodName) + '.sql')
        with open(sql_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line.find('--') != 0:
                    break
                intended_docstring += line[2:].strip()
                intended_docstring += "\n"
                line = line[2:].strip()
                if line.find('@') != 0:
                    continue
                line = line[1:]
                (key, value) = line.split(' ', 1)
                self._metadata[key] = value

        self.gpdb_version = self._metadata.get('gpdb_version', None)

    def checkGUC(self, name):
        if (len(name) <1):
            return -1
        cmd = "show " + name
        out = PSQL.run_sql_command(cmd)
        return out.split('\n')

    def checkOutputPattern(self, outfile, pattern_arr):
        try :
            fin = open ((outfile), "r")
            flines = fin.readlines()
            fin.close()
            buf = ' '.join(flines)

            for pattern in pattern_arr:
                if  buf.find(pattern) < 0:
                    return False 
            return True 
        except IOError:
            return str(IOError)

class UDPICFullTestCases(UDPICTestCases):
    gp_udpic_fault_inject_percent = "gp_udpic_fault_inject_percent"

    @classmethod
    def setUpClass(cls):
        pass

    def __init__(self, methodName):
        super(UDPICFullTestCases, self).__init__(methodName)

        # these cases not run with 4.2.x 
        cmdsql = 'SELECT version();'
        ret = PSQL.run_sql_command(cmdsql)
        #if ret.find('Greenplum Database 4.2 build') >= 0:
        #    self.is_version_skip = 1

    def test_icudp_full(self):
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        try:
            out = self.checkGUC(self.gp_udpic_fault_inject_percent)
            self.assertTrue(len(out) > 4 and int(out[3].strip()) >= 0 )
        except:
            self.skipTest("GUC " + self.gp_udpic_fault_inject_percent + " not defined")

        start_time = time.strftime('%I:%M:%S',time.localtime(time.time()))
        tinctest.logger.debug('start time:%s ' % start_time)

        test_ret = self.run_sql_file(local_path(self.common_sql + str(self._testMethodName) + '.sql'),
                                local_path(self.common_sql + str(self._testMethodName) + '.ans'))

        self.assertTrue(test_ret)

        end_time = time.strftime('%I:%M:%S',time.localtime(time.time()))
        tinctest.logger.debug('end time:%s ' % end_time)
