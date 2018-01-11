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

class UDPICPacketControlTestCases(UDPICTestCases):
    #common_sql = 'common/'
    hostlist = [] 
    hoststr = ''
    sql_prefix = ''
    log_str = ''
    # indicate gp_interconnect_fc_method is set or not in sql/case 
    is_fc_method_skip = 0
    gp_interconnect_fc_method = 'gp_interconnect_fc_method'
    cluster_platform = ''
    
    
    @classmethod
    def setUpClass(cls):
        cmdsql = 'SELECT hostname FROM pg_catalog.gp_segment_configuration WHERE content > -1 AND status = \'u\' GROUP BY hostname ORDER by hostname;'
        ret = PSQL.run_sql_command(cmdsql)
        hostlist = ret.split('\n') 
        if (len(hostlist) < 5):
            raise AssertionError('Get segment host list failed') 
        for i in range(3, len(hostlist)):
            if (hostlist[i].find('(') >= 0):
                break
            cls.hostlist.append(hostlist[i].strip()) 
        for host in cls.hostlist:
            cls.hoststr = cls.hoststr + ' -h ' + host
        if(os.path.exists(local_path('log/'))==False):
            os.mkdir(local_path('log/'))
        cls.log_str='gpssh ' + cls.hoststr + ' \"sudo cat /proc/ickmlog\"' + '>>' + local_path('log/')

        # these cases not run with 4.2.x 
        # raise SkipTest('Test does not apply to the deployed GPDB version.')
        cmdsql = 'SELECT version();'
        ret = PSQL.run_sql_command(cmdsql)
        #if ret.find('Greenplum Database 4.2 build') >= 0:
        #    cls.is_version_skip = 1
        
    def __init__(self, methodName):
        super(UDPICPacketControlTestCases, self).__init__(methodName)
        self.infer_metadata()
        #print self.match_metadata("gpdb_version", gpversion.GpVersion('(Greenplum Database 4.2 build 38471)'))
        (cur_platform,version, state) = platform.linux_distribution()
        self.cluster_platform = cur_platform

    def infer_metadata(self):
        intended_docstring = ""
        sql_prefix_list = str(self._testMethodName).split('_')
        self.sql_prefix = sql_prefix_list[0] + '_' + sql_prefix_list[1] + '_' + sql_prefix_list[2]
        sql_file = local_path(self.common_sql + self.sql_prefix + '.sql')
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

    
    def test_drop_data_1_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        #result = runShellCommand('gpssh' + self.hoststr +  ' \"sudo dmesg -c\"') 
        #self.assertTrue(result)
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=1 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log' )
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_2_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        #result = runShellCommand('gpssh' + self.hoststr +  ' \"sudo dmesg -c\"') 
        #self.assertTrue(result)
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=2 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log' )
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)


    def test_drop_data_2_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=2 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_1_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=1 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_multi_odd_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=1,3,5,7,9,11 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_multi_even_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=2,4,6,8 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_multi_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=2,3,4,7,8 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_multi_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=2,3,4,7,8 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)
        self.assertTrue(test_ret)

    def test_drop_data_multi_odd_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=1,3,5,7,9,11 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)
        self.assertTrue(test_ret)



    def test_drop_data_multi_even_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 seq_array=2,4,6,8 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)


    def test_drop_data_percent_10(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 percent=1000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_percent_50(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 percent=5000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_percent_80(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x101 percent=8000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)


    def test_drop_data_ack_1_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 seq_array=1 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_ack_2_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 seq_array=2 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)


    def test_drop_data_ack_2_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 seq_array=2 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_ack_1_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 seq_array=1 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_ack_multi_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 seq_array=2,3,4,7,8 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_ack_multi_even_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 seq_array=2,4,6,8 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_ack_multi_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 seq_array=2,3,4,7,8 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_ack_multi_even_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 seq_array=2,4,6,8 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_ack_percent_10(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")

        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 percent=1000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_ack_percent_50(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")

        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x102 percent=5000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_eos_1(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x108 drop_times=1\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_eos_3(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x108 drop_times=3\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_eos_percent_50(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x108 percent=5000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_drop_data_all_percent_10(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko drop_all=1 percent=1000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)


    '''
    def test_drop_data_all_percent_20(self):
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko drop_all=1 percent=2000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)
    '''

    
    def test_drop_data_all_percent_30(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko drop_all=1 percent=3000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)


    def test_drop_data_all_percent_50(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko drop_all=1 percent=5000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)


    def test_drop_data_all_percent_70(self):
        if (self.cluster_platform.lower().find('red hat enterprise linux server') < 0):
            self.skipTest('Test only applies to RHEL platform.')
        if (self.is_version_skip):
            self.skipTest('Test does not apply to the deployed GPDB version.')
        if (self.is_fc_method_skip):
            self.skipTest("GUC " + self.gp_interconnect_fc_method + " not defined")
        runShellCommand('gpssh ' + self.hoststr +  ' \"export PATH=$PATH:/sbin; \
                                               sudo rmmod ickm.ko\"')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko drop_all=1 percent=7000\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    '''
    def test_disorder_fuc_muti_5(self):
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x401 seq_array=3,5,7,9,11  timeout=5\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_disorder_fuc_muti_10(self):
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x401 seq_array=2,4,6,8,12 timeout=10\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_disorder_fuc_1_10(self):
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x401 seq_array=1 timeout=10\"') 
        self.assertTrue(result)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        dis_check=checkDIS(local_path('log/')+self._testMethodName + '.log',(2,))
        self.assertFalse(dis_check)
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_disorder_fuc_2_2_3_5(self):
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x401 seq_array=3  timeout=5\"') 
        self.assertTrue(result)
        setGUC(local_path(self.common_sql + self.sql_prefix),gp_interconnect_queue_depth=2,gp_interconnect_snd_queue_depth=2)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        dis_check=checkDIS(local_path('log/')+self._testMethodName + '.log',(4,))
        setGUC(local_path(self.common_sql + self.sql_prefix),gp_interconnect_queue_depth=4,gp_interconnect_snd_queue_depth=2)
        self.assertTrue(dis_check)
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_disorder_fuc_2_2_2_5(self):
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x401 seq_array=2  timeout=5\"') 
        self.assertTrue(result)
        setGUC(local_path(self.common_sql + self.sql_prefix),gp_interconnect_queue_depth=2,gp_interconnect_snd_queue_depth=2)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        dis_check=checkDIS(local_path('log/')+self._testMethodName + '.log',(3,))
        setGUC(local_path(self.common_sql + self.sql_prefix),gp_interconnect_queue_depth=4,gp_interconnect_snd_queue_depth=2)
        self.assertFalse(dis_check)
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)

    def test_disorder_fuc_2_2_4_5(self):
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo insmod ickm.ko ict_type=0x401 seq_array=4  timeout=5\"') 
        self.assertTrue(result)
        setGUC(local_path(self.common_sql + self.sql_prefix),gp_interconnect_queue_depth=2,gp_interconnect_snd_queue_depth=2)
        test_ret = self.run_sql_file(local_path(self.common_sql + self.sql_prefix + '.sql'),
                                local_path(self.common_sql + self.sql_prefix + '.ans'))
        ret_log = runShellCommand(self.log_str + self._testMethodName + '.log')
        result = runShellCommand('gpssh' + self.hoststr +  ' \"export PATH=$PATH:/sbin;sudo rmmod ickm.ko \"') 
        dis_check=checkDIS(local_path('log/')+self._testMethodName + '.log',(5,))
        setGUC(local_path(self.common_sql + self.sql_prefix),gp_interconnect_queue_depth=4,gp_interconnect_snd_queue_depth=2)
        self.assertFalse(dis_check)
        self.assertTrue(result)
        self.assertTrue(ret_log)

        self.assertTrue(test_ret)
    '''

class UDPICPacketControlCapacityTestCases(UDPICPacketControlTestCases):


    def __init__(self, methodName):
        super(UDPICPacketControlCapacityTestCases, self).__init__(methodName)
        self.infer_metadata()
        #print self.match_metadata("gpdb_version", gpversion.GpVersion('(Greenplum Database 4.2 build 38471)'))
        (cur_platform,version, state) = platform.linux_distribution()
        self.cluster_platform = cur_platform

        '''
        cmdsql = 'SELECT version();'
        ret = PSQL.run_sql_command(cmdsql)
        if ret.find('Greenplum Database 4.2 build') >= 0:
            self.skipTest('Test does not apply to the deployed GPDB version.')
        '''

        #check GUC 
        self.is_fc_method_skip = 1 
        out = self.checkGUC(self.gp_interconnect_fc_method)
        for i in range(0, len(out)):
            if out[i].find('LOSS') >=0 or out[i].find('CAPACITY') >=0:
                self.is_fc_method_skip = 0 

    def infer_metadata(self):
        intended_docstring = ""
        sql_prefix_list = str(self._testMethodName).split('_')
        self.sql_prefix = sql_prefix_list[0] + '_' + sql_prefix_list[1] + '_' + sql_prefix_list[2]+'_capacity'
        sql_file = local_path(self.common_sql + self.sql_prefix + '.sql')
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
