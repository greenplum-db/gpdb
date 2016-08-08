"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

from mpp.lib.PSQL import PSQL
from mpp.lib.gpdbverify import GpdbVerify
from mpp.lib.config import GPDBConfig
from mpp.models import MPPTestCase

class DbStateClass(MPPTestCase):


    def __init__(self,methodName,config=None):
        if config is not None:
            self.config = config
        else:
            self.config = GPDBConfig()
        self.gpverify = GpdbVerify(config=self.config)
        super(DbStateClass,self).__init__(methodName)

    def check_system(self):
        ''' 
        @summary: Check whether the system is up and sync. Exit out if not 
        '''
        cmd ="select count(*) from gp_segment_configuration where content<> -1 ;"
        count_all = PSQL.run_sql_command(cmd, flags ='-q -t', dbname='postgres')
        cmd ="select count(*) from gp_segment_configuration where content<> -1 and mode = 's' and status = 'u';"
        count_up_and_sync = PSQL.run_sql_command(cmd, flags ='-q -t', dbname='postgres')
        if count_all.strip() != count_up_and_sync.strip() :
            tinctest.logger.info('Exiting the test. The cluster is not in up/sync ............')
            os._exit(1)
        else:
            tinctest.logger.info("\n Starting New Test: System is up and in sync .........")

    def check_catalog(self,dbname=None, alldb=True, online=False, testname=None, outputFile=None, run_repair=True, host=None, port=None, expected_rc=0):
        '''1. Run gpcheckcat 2. Run repairscript if present 3. Run gpcheckcat again. Repeat this 5 times'''
        errorCode = 1
        if run_repair:
            i = 0
            while i < 5:
                (errorCode, hasError, gpcheckcat_output, repairScriptDir) =  self.gpverify.gpcheckcat(dbname=dbname, alldb=alldb, online=online, testname=testname, outputFile=outputFile, host=host, port=port)
                tinctest.logger.info(" %s Gpcheckcat iteration . ErrorCode: %s " % (i,errorCode))
                if errorCode == 0:
                    break
                else:
                    self.gpverify.run_repair_script(repairScriptDir,dbname=dbname, alldb=alldb, online=online, testname=testname, outputFile=outputFile, host=host, port=port)
                    i = i+1
        else:
            (errorCode, hasError, gpcheckcat_output, repairScriptDir) =  self.gpverify.gpcheckcat(dbname=dbname, alldb=alldb, online=online, testname=testname, outputFile=outputFile, host=host, port=port)
        if errorCode != expected_rc:
            raise Exception('GpCheckcat failed, %s != %s '% (errorCode, expected_rc))

    def check_mirrorintegrity(self, master=False):
        '''Runs checkmirrorintegrity(default), check_mastermirrorintegrity(when master=True) '''
        (checkmirror, fix_outfile) = self.gpverify.gpcheckmirrorseg(master=master)
        if not checkmirror:
           self.fail('Checkmirrorseg failed. Fix file location : %s' %fix_outfile)
        tinctest.logger.info('Successfully completed integrity check')

    def run_validation(self):
        '''
        1. gpcheckcat
        2. checkmirrorintegrity
        3. check_mastermirrorintegrity 
        ''' 
        self.check_catalog()
        self.check_mirrorintegrity()
        if self.config.has_master_mirror():
            self.check_mirrorintegrity(master=True)


