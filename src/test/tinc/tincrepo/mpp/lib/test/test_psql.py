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

import inspect
import os
import re

import unittest2 as unittest

from mpp.lib.PSQL import PSQL, PSQLException

class PSQLCommandLineTests(unittest.TestCase):
    def test_default_sql_file_construction(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, output_to_file = False)
        expected_cmdstr = 'psql -a -f %s' %sql_file
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_default_sql_cmd_construction(self):
        sql_cmd = "select 1"
        psql = PSQL(sql_cmd = sql_cmd, output_to_file = False)
        expected_cmdstr = 'psql -a -c \"%s\"' %sql_cmd
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_sql_file_with_out_file(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.out')
        psql = PSQL(sql_file = sql_file, out_file = out_file)
        expected_cmdstr = "psql -a -f %s &> %s 2>&1" %(sql_file, out_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_sql_file_with_out_file_none(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        out_file = sql_file.replace('.sql', '.out')
        psql = PSQL(sql_file = sql_file)
        expected_cmdstr = "psql -a -f %s &> %s 2>&1" %(sql_file, out_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_sql_file_with_out_file_disabled(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.out')
        psql = PSQL(sql_file = sql_file, out_file = out_file, output_to_file = False)
        expected_cmdstr = "psql -a -f %s" %(sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_sql_cmd_with_out_file(self):
        sql_cmd = "select 1"
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.out')
        psql = PSQL(sql_cmd = sql_cmd, out_file = out_file)
        expected_cmdstr = 'psql -a -c \"%s\" &> %s 2>&1' %(sql_cmd, out_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_sql_cmd_with_out_file_none(self):
        sql_cmd = "select 1"
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.out')
        psql = PSQL(sql_cmd = sql_cmd)
        expected_cmdstr = 'psql -a -c \"%s\"' %(sql_cmd)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_sql_cmd_with_out_file_disabled(self):
        sql_cmd = "select 1"
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.out')
        psql = PSQL(sql_cmd = sql_cmd, out_file = out_file, output_to_file = False)
        expected_cmdstr = 'psql -a -c \"%s\"' %(sql_cmd)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_psql_with_dbname(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, dbname = 'gptest', output_to_file = False)
        expected_cmdstr = 'psql -d %s -a -f %s' %('gptest', sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_psql_with_host(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, host = 'localhost', output_to_file = False)
        expected_cmdstr = 'psql -h localhost -a -f %s' %(sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_psql_with_port(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, port = '5432', output_to_file = False)
        expected_cmdstr = 'psql -p 5432 -a -f %s' %(sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_psql_with_username(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, username = 'gpadmin', output_to_file = False)
        expected_cmdstr = 'psql -U gpadmin -a -f %s' %(sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_psql_with_password(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, password = 'changeme', output_to_file = False)
        expected_cmdstr = 'psql -a -f %s' %(sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)
        self.assertEquals(psql.propagate_env_map.get('PGPASSWORD'), 'changeme')

    def test_psql_with_pgoptions(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, PGOPTIONS = "-c gp_optimizer=off", output_to_file = False)
        expected_cmdstr = "PGOPTIONS=\'-c gp_optimizer=off\' psql -a -f %s" %(sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_psql_with_flags(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, flags = '-e', output_to_file = False)
        expected_cmdstr = 'psql -e -f %s' %(sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_psql_with_background(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        psql = PSQL(sql_file = sql_file, background = True, output_to_file = False)
        expected_cmdstr = 'psql -a -f %s &' %(sql_file)
        self.assertEquals(re.sub(' +', ' ', psql.cmdStr.strip()), expected_cmdstr)

    def test_psql_with_invalid_sql_file(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_not.sql')
        with self.assertRaises(PSQLException) as cm:
            psql = PSQL(sql_file = sql_file, output_to_file = False)
        
        
        
        




    
        
        
        

        
