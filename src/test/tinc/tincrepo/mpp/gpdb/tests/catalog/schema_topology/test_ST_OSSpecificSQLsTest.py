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

import os,re

from mpp.models import SQLTestCase
from mpp.lib.PSQL import PSQL
from tinctest.lib import local_path, Gpdiff

MYD = os.path.abspath(os.path.dirname(__file__))

class OSSpecificSQLsTest(SQLTestCase):
    """
    
    @description This contains several test cases for possible ways of manipulating objects. This testcase specifically deals with OS specific SQLs -  sqls which require specific run-time changes to be made to them before execution  For more details refer QA task - QA-143
    @created 2009-01-27 14:00:00
    @modified 2013-10-17 17:10:15
    @tags ddl schema_topology
    """

    sql_dir = 'sqls/ddls/os_specific_ddls'
    ans_dir = 'sqls/ddls/os_specific_ddls'
    out_dir = 'sqls/ddls/os_specific_ddls'
    
    transformations = {
                'AS\s\'.+\',' : 'AS \''+MYD+'/'+sql_dir+'/funcs\',' ,
                'TO\s\'.+test1_file_copy\'' : 'TO \''+MYD+'/'+sql_dir+'/test1_file_copy\'' ,
                'TO\s\'.+test2_file_copy\'' : 'TO \''+MYD+'/'+sql_dir+'/test2_file_copy\'' ,
                'TO\s\'.+CO_all_types_file_copy\'' : 'TO \''+MYD+'/'+sql_dir+'/CO_all_types_file_copy\'' ,
                'FROM\s\'.+test1_file_copy\'' : 'FROM \''+MYD+'/'+sql_dir+'/test1_file_copy\'' ,
                'FROM\s\'.+test2_file_copy\'' : 'FROM \''+MYD+'/'+sql_dir+'/test2_file_copy\'' ,
                'FROM\s\'.+CO_all_types_file_copy\'' : 'FROM \''+MYD+'/'+sql_dir+'/CO_all_types_file_copy\'' 
             }
    
    os_name = os.uname()[0]
    if os_name == 'Darwin': #Mac
        transformations.__setitem__('schema_topology(\/\w+)*?\/funcs','schema_topology/Mac/funcs')
    else: #Linux/Solaris
        transformations.__setitem__('schema_topology(\/\w+)*?\/funcs','schema_topology/Linux_Solaris/funcs')

    @classmethod
    def setUpClass(cls):
        super(OSSpecificSQLsTest, cls).setUpClass()
        tinctest.logger.info("*** Running the pre-requisite sql files drop.sql and setup.sql")
        PSQL.run_sql_file(local_path('sqls/setup/drop.sql'))
        PSQL.run_sql_file(local_path('sqls/setup/create.sql'))
            
    
    @classmethod    
    def get_out_dir(cls):
        # If the sqls are located in a different directory than the source file, create an output
        # directory at the same level as the sql dir
        if cls.get_source_dir() == cls.get_sql_dir():
            out_dir = os.path.join(cls.get_sql_dir(), 'output/OSSpecificSQLsTest/')
        else:
            out_dir = os.path.join(cls.get_sql_dir(), '../output/OSSpecificSQLsTest/')

        if not os.path.exists(out_dir):
            os.makedirs(out_dir)
        return out_dir

    def perform_transformation_on_sqlfile(self,input_filename, output_filename,transforms):
        with open(input_filename, 'r') as input:
            with open(output_filename, 'w') as output:
                for line in input.readlines():
                    for key,value in transforms.iteritems():
                        line = re.sub(key,value,line)
                    output.write(line)
        return output_filename

    def run_test(self):
        sql_file = self.sql_file
        ans_file = self.ans_file
        new_sql_file = sql_file + '.t'
        new_ans_file = ans_file + '.t'
        tinctest.logger.info("**** Performing the transformations...")
        tinctest.logger.info(self.transformations)
        self.sql_file = self.perform_transformation_on_sqlfile(sql_file, new_sql_file,self.transformations)
        self.ans_file = self.perform_transformation_on_sqlfile(ans_file, new_ans_file,self.transformations)
        return super(OSSpecificSQLsTest, self).run_test()
        tinctest.logger.info('***** Running the OS specific sql files...')
