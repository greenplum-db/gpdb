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
import os

from mpp.models import SQLTestCase
from mpp.lib.config import GPDBConfig
from mpp.lib.gpfilespace import Gpfilespace
from mpp.lib.PSQL import PSQL
from tinctest.lib import local_path,Gpdiff

class GPFilespaceTablespaceTest(SQLTestCase):
    """
    
    @description This contains several test cases for possible ways of manipulating objects. This testcase specifically deals with filespace objects. For more details refer QA task - QA-143
    @created 2009-01-27 14:00:00
    @modified 2013-10-17 17:10:15
    @tags ddl schema_topology
    """

    sql_dir = 'sqls/ddls/filespace_ddls'
    ans_dir = 'sqls/ddls/filespace_ddls'
    out_dir = 'sqls/ddls/filespace_ddls'

    @classmethod 
    def get_out_dir(cls):
        # If the sqls are located in a different directory than the source file, create an output
        # directory at the same level as the sql dir
        if cls.get_source_dir() == cls.get_sql_dir():
            out_dir = os.path.join(cls.get_sql_dir(), 'output/GPFilespaceTablespaceTest/')
        else:
            out_dir = os.path.join(cls.get_sql_dir(), '../output/GPFilespaceTablespaceTest/')

        if not os.path.exists(out_dir):
            os.makedirs(out_dir)
        return out_dir

    
    @classmethod
    def setUpClass(cls):
        super(GPFilespaceTablespaceTest, cls).setUpClass()
        tinctest.logger.info("*** Running the pre-requisite sql files drop.sql and setup.sql")
        PSQL.run_sql_file(local_path('sqls/setup/drop.sql'))
        #separating dropping of filsepaces
        PSQL.run_sql_file(local_path('sqls/setup/drop_filespaces.sql'))
        PSQL.run_sql_file(local_path('sqls/setup/create.sql'))
        tinctest.logger.info("Starting the Filespace Tablespace test.. ")
        config = GPDBConfig()
        filespace = Gpfilespace()       
        filespace_name = 'cdbfast_fs_'
        if config.is_not_insync_segments():
            tinctest.logger.info("***** Creating filespaces...")
            filespace.create_filespace(filespace_name+'sch1')
            filespace.create_filespace(filespace_name+'sch2')
            filespace.create_filespace(filespace_name+'sch3')
