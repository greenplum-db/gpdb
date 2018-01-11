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
from tinctest.lib import run_shell_command, local_path, Gpdiff
from mpp.lib.PSQL import PSQL

MYD = os.path.abspath(os.path.dirname(__file__))
GPHOME = os.environ['GPHOME']

class EnhancedTableFunctionTest(SQLTestCase):
    """

    @description This contains several test cases for possible ways of manipulating objects. This test case specifically deals with enhanced table functionality. For more details refer QA task - QA-143
    @created 2009-01-27 14:00:00
    @modified 2013-10-17 17:10:15
    @tags ddl schema_topology
    """


    sql_dir = 'sqls/ddls/enhanced_tables'
    ans_dir = 'sqls/ddls/enhanced_tables'
    out_dir = 'sqls/ddls/enhanced_tables'

    @classmethod
    def setUpClass(cls):
        super(EnhancedTableFunctionTest, cls).setUpClass()

        tinctest.logger.info("*** Running the pre-requisite sql files drop.sql and setup.sql")
        PSQL.run_sql_file(local_path('sqls/setup/drop.sql'))
        PSQL.run_sql_file(local_path('sqls/setup/create.sql'))
        tinctest.logger.info("*** Starting the Enhaced table test")

    def getMultinodeHosts(self):
        """
        Returns distinct no. of nodes for a mult-node cluster environment, else returns None
        """
        cmd = "SELECT DISTINCT(hostname) FROM gp_segment_configuration"
        hosts = PSQL.run_sql_command(cmd).split('\n')[3:-3]
        if len(hosts) > 1:
            return hosts
        else:
            return None

    @classmethod
    def get_out_dir(cls):
        # If the sqls are located in a different directory than the source file, create an output
        # directory at the same level as the sql dir
        if cls.get_source_dir() == cls.get_sql_dir():
            out_dir = os.path.join(cls.get_sql_dir(), 'output/EnhancedTableFunctionTest/')
        else:
            out_dir = os.path.join(cls.get_sql_dir(), '../output/EnhancedTableFunctionTest/')

        if not os.path.exists(out_dir):
            os.makedirs(out_dir)
        return out_dir
    
    def setUp(self):
        # compile tablefunc_demo.c and install the tablefunc_demo.so 
        cmdMakeInstall = 'cd '+MYD+'/%s/data && make && make install' %self.sql_dir
        ok = run_shell_command(cmdMakeInstall)
        # Current make file works for linux, but not for Solaris or OSX.
        # If compilation fails or installation fails, force system quit: os._exit(1)
        if not ok:
            tinctest.logger.error("***** make command failed!! Executed Command : %s"%cmdMakeInstall)
            self.fail("ERROR: make command failed!!")
        sharedObj = GPHOME+'/lib/postgresql/tabfunc_demo.so'
        if not os.path.isfile(sharedObj):
            tinctest.logger.error("***** Shared object '%s' does not exist!!"%sharedObj)
            self.fail("ERROR: Shared object '%s' does not exist!!"%sharedObj)

        # For multinode cluster, need to copy shared object tabfunc_demo.so to all primary segments
        hosts = self.getMultinodeHosts()
        if hosts is not None:
            for host in hosts:
                cmd_str = "scp "+GPHOME+"/lib/postgresql/tabfunc_demo.so "+host.strip()+":"+GPHOME+"/lib/postgresql"
                ok = run_shell_command(cmd_str)
                if not ok:
                     tinctest.logger.error('***** Could not copy shared object to primary segment: '+cmd_str)
                     self.fail('Could not copy shared object to primary segment: '+cmd_str)
