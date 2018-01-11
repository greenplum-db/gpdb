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
from tinctest import TINCTestLoader
from tinctest.lib import Gpdiff, collect_gpdb_logs
from tinctest.runner import TINCTextTestResult
import datetime
import inspect
import new
import os
import re
import sys
import shutil
import tinctest
import unittest2 as unittest

from mpp.models import SQLTestCase
from mpp.lib.datagen.databases import __databases__
from mpp.lib.PSQL import PSQL

class SQLTAQOTestCase(SQLTestCase):
    
    def __init__(self, methodName, sql_file=None, db_name = None):
        # taqo score
        self._taqo_score = -1.0
        self._taqo_output = None
        self._taqo_correctness_result = False
        super(SQLTAQOTestCase, self).__init__(methodName, sql_file, db_name)
        self._current_dir = None
        
    def setUp(self):
        #Setup the database by calling out the super class
        super(SQLTAQOTestCase, self).setUp()
        source_file = sys.modules[self.__module__].__file__
        self._current_dir = os.path.dirname(source_file)
        
    def run_test(self):
        """
        The method that subclasses should override to execute a sql test case differently.
        This encapsulates the execution mechanism of SQLTestCase. Given a base sql file and
        an ans file, runs all the sql files for the test case.

        Note that this also runs the other part sqls that make up the test case. For eg: if the
        base sql is query1.sql, the part sqls are of the form query1_part*.sql in the same location
        as the base sql.
        """
        sql_file = self.sql_file
        ans_file = self.ans_file
        # call taqo2.jar to execute the test case
        taqo_config_file = os.path.join(self._current_dir, 'taqo_config.properties')
        taqo_jar_file = os.path.join(self._current_dir, 'taqo2.jar')
        output_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql','.out'))
#        print('DEBUG: taqo_config_file: '+taqo_config_file)
#        print('DEBUG: taqo_jar_file: '+taqo_jar_file)
#        print('DEBUG: output_file: '+output_file)
        
        if not os.path.exists(taqo_config_file):
            tinctest.logger.info('Fail to find taqo_config.properties. Abort the testcase.')
            return False
        
        if not os.path.exists(taqo_jar_file):
            tinctest.logger.info('Fail to find taqo2.jar. Abort the testcase.')
            return False
    
        command = 'cd %s; java -jar %s -c %s -s %s -o %s'%(self._current_dir, taqo_jar_file, taqo_config_file, sql_file, output_file)
#        print ("Execute command: "+command)
        os.system(command)
        
        self._parse_taqo_output(output_file)                
        # Currently, use correctness result to judge succeed or failure of a test case
        return self._taqo_correctness_result
           
        
    def _parse_taqo_output(self, output_file):
        lines = [line.strip() for line in open(output_file)]
        self._taqo_output = '\n'.join(lines)
        for line in lines:
            if line.find('Spearman')!=-1:
                self._taqo_score = float(line[line.index(':')+1:].strip())
            elif line.find('Correctness testing')!=-1:
                if line.find('!=')!=-1:
                    # collect minidump
                    iter_out_file = line[line.index('!=')+2:].strip()
                    self._collect_minidump(iter_out_file)
                elif line.find('succeed')!=-1:
                    self._taqo_correctness_result = True
                elif line.find('failed')!=-1:
                    self._taqo_correctness_result = False
                

            
    def _collect_minidump(self, iter_out_file):
        iter_sql_file = iter_out_file.replace('.out','.sql')
        print ("DEBUG: "+iter_sql_file)
        minidump_sql_file = iter_sql_file.replace('.sql','_minidump.sql')
        print ("DEBUG: "+minidump_sql_file)
        minidump_out_file = minidump_sql_file.replace('.sql','.out')
        minidump_mdp_file = minidump_sql_file.replace('.sql','.mdp')
        
        print ("Check ITER_SQL_FILE exist: "+str(os.path.exists(iter_sql_file)))
        
        with open(minidump_sql_file, 'w') as o:
            with open(iter_sql_file, 'r') as f:
                for line in f:
                    o.write(line)
                    if line.find('SET gp_optimizer=on')!=-1:
                        o.write('SET gp_opt_minidump = on;\n')
                    elif line.find('--end_ignore')!=-1:
                        o.write('EXPLAIN \n')
        
        # Go to $MASTER_DATA_DIRECTORY/minidumps remove the existing minidumps
        mdd = os.path.join(os.environ['MASTER_DATA_DIRECTORY'],'minidumps')
        # check mdd exist
        if os.path.exists(mdd):
            # remove existing minidumps
            for f in os.listdir(mdd):
                if fnmatch.fnmatch(f, 'Minidump_*.mdp'):
                    os.remove(os.path.join(mdd, f))
        
        # Run minidump_sql_file and copy the generated minidump to out dir. 
        PSQL.run_sql_file(minidump_sql_file, dbname = self.db_name, out_file = minidump_out_file)
        if os.path.exists(mdd):
            # remove existing minidumps
            for f in os.listdir(mdd):
                if fnmatch.fnmatch(f, 'Minidump_*.mdp'):
                    shutil.copyfile(os.path.join(mdd, f), minidump_mdp_file)
                    break;
        
                    
        
        
                    
            
    def defaultTestResult(self, stream=None, descriptions=None, verbosity=None):
        if stream and descriptions and verbosity:
            return SQLTAQOTestCaseResult(stream, descriptions, verbosity)
        else:
            return unittest.TestResult()
        
class SQLTAQOTestCaseResult(TINCTextTestResult):
    def __init__(self, stream, descriptions, verbosity):
        super(SQLTAQOTestCaseResult, self).__init__(stream, descriptions, verbosity)
        
    def addSuccess(self, test):
        self.value = test._taqo_score
        super(SQLTAQOTestCaseResult, self).addSuccess(test)
        self.result_detail['taqo_output'] = test._taqo_output
        self.result_detail['correctness_testing_result'] = test._taqo_correctness_result
#        self._debug_print()
        
    def addFailure(self, test, err):
        self.value = test._taqo_score
        super(SQLTAQOTestCaseResult, self).addFailure(test, err)
        self.result_detail['taqo_output'] = test._taqo_output
        self.result_detail['correctness_testing_result'] = test._taqo_correctness_result
#        self._debug_print()
        
    def _debug_print(self):
        s1 = "\n=====\ntaqo score: %s\ntaqo output:%s\ncorrectness %s\n====\n"%(self.value,
                self.result_detail['taqo_output'],self.result_detail['correctness_testing_result'])
        print s1
