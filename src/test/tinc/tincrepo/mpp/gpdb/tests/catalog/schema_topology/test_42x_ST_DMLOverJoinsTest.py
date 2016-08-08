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

import tinctest

import os
import inspect

from mpp.models import SQLTestCase
from tinctest.lib import local_path, Gpdiff
from mpp.lib.PSQL import PSQL

class DMLOverJoinsTest(SQLTestCase):
    """
    
    @description This contains several test cases for possible ways of manipulating objects. This test case specifically deals with Joins. For more details refer QA task - QA-143
    @created 2009-01-27 14:00:00
    @modified 2013-10-17 17:10:15
    @tags ddl schema_topology
    """

    sql_dir = 'sqls/ddls/joins'
    ans_dir = 'sqls/ddls/joins'
    out_dir = 'sqls/ddls/joins'
        
    @classmethod
    def setUpClass(cls):
        super(DMLOverJoinsTest, cls).setUpClass()
        tinctest.logger.info("*** Running the pre-requisite sql files drop.sql and setup.sql")
        PSQL.run_sql_file(local_path('sqls/setup/drop.sql'))
        PSQL.run_sql_file(local_path('sqls/setup/create.sql'))
        tinctest.logger.info("Starting the join test.. ")

    def count_segs(self):
        """ Counts the no. of segments from the cluster configuration """
        cmd_str = "SELECT COUNT(*) FROM gp_segment_configuration WHERE content <> -1 and preferred_role = 'p'"
        out = PSQL.run_sql_command(cmd_str).split("\n")[3].strip()
        return int(out)
        
    def local_path(self, filename):
        """
        Return the absolute path of the input file.:Overriding it here to use the absolute path instead of relative"""
        frame = inspect.stack()[1]
        source_file = inspect.getsourcefile(frame[0])
        source_dir = os.path.dirname(os.path.abspath(source_file))
        return os.path.join(source_dir, filename)

    @classmethod
    def get_out_dir(cls):
        # If the sqls are located in a different directory than the source file, create an output
        # directory at the same level as the sql dir
        if cls.get_source_dir() == cls.get_sql_dir():
            out_dir = os.path.join(cls.get_sql_dir(), 'output/DMLOverJoinsTest/')
        else:
            out_dir = os.path.join(cls.get_sql_dir(), '../output/DMLOverJoinsTest/')

        if not os.path.exists(out_dir):
            os.makedirs(out_dir)
        return out_dir
        
    def verify_out_file(self, out_file, ans_file):
        if ans_file is not None:
            # ramans2: Modified test case to pick different answer files depending on the # segments in the cluster
            ans_file = self.local_path(ans_file+".%s" %self.count_segs())
            self.test_artifacts.append(ans_file)
            # Check if an init file exists in the same location as the sql file
            init_files = []
            init_file_path = os.path.join(self.get_sql_dir(), 'init_file')
            if os.path.exists(init_file_path):
                init_files.append(init_file_path)
            result = Gpdiff.are_files_equal(out_file, ans_file, match_sub = init_files)
            if result == False:
                self.test_artifacts.append(out_file.replace('.out', '.diff'))
        return result

