#!/usr/bin/env python
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

Functional tests for MPP-16428 - Improve plan caching in PL/pgsql. 

Tests that require plperl will first install plperl gppkg if 
gppkg is not installed.
"""

import os
import sys
import inspect
import traceback
import tinctest
from tinctest.lib import Gpdiff
from mpp.lib.PSQL import PSQL
from mpp.lib.gpdbSystem import GpdbSystem
from tinctest.lib import local_path, run_shell_command    
from mpp.gpdb.tests.package.procedural_language import ProceduralLanguage
from mpp.gpdb.tests.package.procedural_language.plpgsql_caching.functional import Plpg
from mpp.lib.gppkg.gppkg import Gppkg
from mpp.models import MPPTestCase

cmd = 'gpssh --version'
res = {'rc':0, 'stderr':'', 'stdout':''}
run_shell_command (cmd, 'check product version', res)
product_version = res['stdout'].split('gpssh version ')[1].split(' build ')[0]
plpg = Plpg()

class PlPgsqlCachingTest(MPPTestCase):
    """
    @summary: Test class for MPP-16428 - Improve plan caching in PL/pgsql. Extends ProceduralLanguageTestCase
    
    """
    def __init__(self, methodName):
        self.pl = ProceduralLanguage()
        self.gpdb = GpdbSystem()
        super(PlPgsqlCachingTest, self).__init__(methodName)

    @classmethod
    def setUpClass(cls):
        sql_files = ['negative_cachedplan_cmdutility_drop.sql', 'recursive_functions_droprecreatetable.sql', 'reexec_functioninwhere_droprecreateao.sql']
        ans_files = ['negative_cachedplan_cmdutility_drop.ans', 'recursive_functions_droprecreatetable.ans', 'reexec_functioninwhere_droprecreateao.ans']
        if product_version.startswith('4.2'):
            plpg.pre_process(sql_files, ans_files)

    def setUp(self):
        self.do_test_fixture('setup')

    def tearDown(self):
        self.do_test_fixture('teardown')
     
    def do_test_fixture(self, fixture):
        """
        @summary: Runs a setup or teardown routine
        
        @param fixture: Set to either 'setup' or 'teardown'. Used to determine sql file suffix.
        """
        testcase1 = inspect.stack()[1][3] 
        testcase = self.id().split(".")[2]
        init_file = local_path('init_file') 
        init_file_list = []
        init_file_list.append(init_file)
        if fixture == 'setup':
            sqlfile = local_path(testcase + "_setup.sql")
            outfile = local_path(testcase + "_setup.out")
            ansfile = local_path(testcase + "_setup.ans")
        elif fixture == 'teardown':
            sqlfile = local_path(testcase + "_teardown.sql")
            outfile = local_path(testcase + "_teardown.out")
            ansfile = local_path(testcase + "_teardown.ans")
        else:
            raise Exception("do_test_fixture(): Invalid value for fixture. Acceptable values are 'setup' or 'teardown'")
        
        # check if setup sql file exists
        if os.path.isfile(sqlfile):
            # if exists, run setup sql, and validate result
            PSQL.run_sql_file(sql_file = sqlfile, out_file = outfile)            
            Gpdiff.are_files_equal(outfile, ansfile, match_sub=init_file_list)
        else:
            pass
    
    def install_plperl(self):
        """
        @summary: Installs plperl gppkg and creates plperl language in database if it doesn't exist
        
        """
        gppkg = Gppkg()
        gppkg.gppkg_install(product_version, 'plperl')
        if self.pl.language_in_db('plperl') == False:
            self.pl.create_language_in_db('plperl')
            
    def check_plperl_env(self):
        """
        @summary: Checks if environment is suitable for plperl test since it requires gppkg, which is only supported on RHEL and SuSE
        
        """
        if self.pl.gppkg_os.find('rhel') < 0 and self.pl.gppkg_os.find('suse') < 0:
            self.skipTest('TEST SKIPPED: Test requires plperl. gppkg is only supported on RHEL and SuSE. Skipping test')


    def create_language_plpython(self):
        """
        @summary: Creates plpython language in database if it doesn't exist
        
        """

        if self.pl.language_in_db("plpythonu") == False:
            tinctest.logger.info("plperl language doesn't exist in database. Creating language...")
            self.pl.create_language_in_db("plpythonu")


    def do_test(self, timeout=0, sqlfile=None, host=None, port=None, username=None, password=None, flags='-a', ans_version=False):
        """
        @summary: Run a test case
        
        @param timeout: Number of seconds to run sql file before timing out
        @param sqlfile: The path to sql file (relative to TEST.py directory)
        @param host: The GPDB master host name to use to connect to database
        @param port: The GPDB port used to make connections to the database
        @param username: The database username to use to connect to the database
        @param password: The password for the database user used to connect to database
        """
        (gpdb_version, build) = self.gpdb.GetGpdbVersion()
        if sqlfile is None:
            testcase = inspect.stack()[1][3]
            filename = testcase.split('test_')[1]
            sql_file = local_path(filename +".sql")
            out_file = local_path(filename + ".out")
            ans_file = local_path(filename + ".ans")
        else:
            sql_file = local_path(sqlfile)
            out_file = local_path(sqlfile.split('.')[0] + '.out')
            ans_file = local_path(sqlfile.split('.')[0] + '.ans')
        if ans_version:
            (gpdb_version, _) = self.gpdb.GetGpdbVersion()
            if gpdb_version.startswith('4.3'):
                ans_file = ans_file+'.4.3'

        init_file = local_path('init_file')
        init_file_list = []
        init_file_list.append(init_file)

        # run psql on file, and check result
        PSQL.run_sql_file(sql_file=sql_file, out_file=out_file, timeout=timeout, host=host, port=port, username=username, password=password,flags=flags)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file, match_sub=init_file_list))

    def test_reexec_functioninselect_droprecreateheap(self):
        """plpgsql caching: Re-execute function in select list. Function drops and recreates heap table."""
        self.do_test()
    
    def test_reexec_functioninwhere_droprecreateao(self):
        """plpgsql caching: Re-execute function in where clause list. Function drops and recreates AO/CO compressed table."""
        self.do_test()
        
    def test_cursors_droprecreate_tablereferencedbycursor(self):
        """plpgsql caching: Curors - Re-execute function that drops and recreates a heap table referenced by a cursor"""
        self.do_test()
        
    def test_cursors_forloop_droprecreatetable(self):
        """plpgsql caching: Cursors - Execute a function that has a for loop which drops and recreates a table at each iteration"""
        self.do_test()
        
    def test_temptables_droprecreate_temptable(self):
        """plpgsql caching: Temp Tables - Execute function that drops and recreates temp table."""
        self.do_test()
        
    def test_nestedfunc_1level_droprecreate_function(self):
        """plpgsql caching: Nested Functions - Drop-recreate a nested function in between function calls."""
        self.do_test()
        
    def test_nestedfunc_3levels_droprecreate_table(self):
        """plpgsql caching: Nested Functions - 3 levels of function calls, drop-recreate a table reference by one of the functions in between function calls."""
        self.do_test()
        
    def test_nestedfunc_3levels_redefinefunc(self):
        """plpgsql caching: Nested Functions - 3 levels of function calls, change contents of nested function in between function calls."""
        self.do_test()
    
    def test_reexec_function_splitpartition(self):
        """plpgsql caching: Partitioned Tables - Re-execute function after splitting partitioned table referenced in function."""
        self.do_test()
    
    def test_recursive_functions_droprecreatetable(self):
        """plpgsql caching: Recursive Functions - Drop and recreate AO table referenced in recursive plpgsql function"""
        self.do_test()
        
    def test_views_redefine_view(self):
        """plpgsql caching: Views - Change definition of view referenced in plpgsql function in between function calls."""
        result = PSQL.run_sql_command("show optimizer", dbname="postgres", flags='-q -t') 
        optimizer = result.strip()

        if optimizer == "on":
            self.do_test()
        else:
            self.skipTest("MPP-20715: Function involving view gives wrong results with planner")
        
    def test_masteronly_redefine_view(self):
        """plpgsql caching: Master Only - Change definition of view referenced in plpgsql function in between function calls."""
        self.do_test()
        
    def test_negative_cachedplan_segmentquery(self):
        """plpgsql caching: Negative Test - Cache is not cleared. Run function query that is only compiled and executed on segments"""
        self.do_test()
        
    def test_negative_cachedplan_cmdutility_drop(self):
        """plpgsql caching: Negative Test - Cache is not cleared. Run function that runs a CMD_UTILITY statement - drop table"""
        self.do_test()
        
    def test_negative_cachedplan_cmdutility_create(self):
        """plpgsql caching: Negative Test - Cache is not cleared. Run function that runs a CMD_UTILITY statement - create table"""
        self.do_test()
    
    def test_negative_plperl_split_between_prepexec(self):
        """plpgsql caching: Negative Test - Non-plpgsql function: plperl. Split partition in between prepare and execute functions"""
        self.check_plperl_env()
        self.install_plperl()
        self.do_test()

    def test_negative_nocaching_plperl_drop_between_exec(self):
        """plpgsql caching: Negative Test - Non-plpgsql function: plperl. No caching test. Re-execute function after drop and recreate table."""
        self.check_plperl_env()
        self.install_plperl()
        self.do_test()

    def test_negative_nocaching_plperl_drop_recreate(self):
        """plpgsql caching: Negative Test - Non-plpgsql function: plperl. No caching test. Re-execute function after drop and recreate table in function."""
        self.check_plperl_env()
        self.install_plperl()
        self.do_test()

    def test_negative_plpython_drop_between_preexec(self):
        """plpgsql caching: Negative Test - Non-plpgsql function: plpython. Drop and recreate table in between prepare and execute functions"""
        self.create_language_plpython()
        self.do_test()

    def test_negative_nocaching_plpython_drop_between_exec(self):
        """plpgsql caching: Negative Test - Non-plpgsql function: plpython. No caching test. Re-execute function after drop and recreate table."""
        self.create_language_plpython()
        self.do_test()

    def test_negative_unsupported_drop_between_prepexec(self):
        """plpgsql caching: Negative Test - Unsupported: Drop and recreate table in between PREPARE and EXECUTE"""
        self.do_test(ans_version=True)
       
