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

@summary: Test class for functional tests for PostGIS support in GPDB
"""
import sys
import os
import inspect
import re

from tinctest.lib import Gpdiff
from tinctest.lib import local_path, run_shell_command
from mpp.lib.gppkg.gppkg import Gppkg
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from mpp.lib.gpSystem import GpSystem

cmd = 'gpssh --version'
res = {'rc':0, 'stderr':'', 'stdout':''}
run_shell_command (cmd, 'check product version', res)
product_version = res['stdout'].split('gpssh version ')[1].split(' build ')[0]

class PostgisTestCase(MPPTestCase):
    """
    @summary: Test class for PostGIS support in GPDB
    
    """
    def __init__(self, methodName):
        super(PostgisTestCase, self).__init__(methodName)

    @classmethod
    def setUpClass(self):
        super(PostgisTestCase, self).setUpClass()
        gppkg = Gppkg()
        gppkg.gppkg_install(product_version, 'postgis')

    def setUp(self):
        """
        @summary: Overrides setUp for gptest to check if current OS is supported for gppkg.  If not, test is skipped.
        
        """
        system = GpSystem()
        gppkg_os = system.GetOS().lower() + system.GetOSMajorVersion()
        if gppkg_os.find('rhel') < 0 and gppkg_os.find('suse') < 0:
            self.skipTest('TEST SKIPPED: postgis is only supported on RHEL and SuSE. Skipping test.')

    def run_sql(self, sql):
        """
        @summary: Runs sql then checks to make sure no errors
        
        @return: list containing actual output and the output file
        """            
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        sql_command = 'psql -d %s -c \"%s\"'% (self.dbname, sql)
        run_shell_command(sql_command, 'run sql %s'%sql, res)
        ok = not res['rc']
        out = res['stdout']
        if not ok:
            raise Exception("*** ERROR run_sql(): Running sql [ %s ] returned error" % sql)               
        return out

    def do_test(self, timeout=0, sqlfile=None, host=None, port=None, username=None, password=None, flags='-a', usetemplate=False):
        """
        @summary: Run a PostGIS test case
        
        @param timeout: Number of seconds to run sql file before timing out
        @param sqlfile: The path to sql file (relative to TEST.py directory)
        @param host: The GPDB master host name to use to connect to database
        @param port: The GPDB port used to make connections to the database
        @param username: The database username to use to connect to the database
        @param password: The password for the database user used to connect to database
        """
        if sqlfile is None:
            testcase = inspect.stack()[1][3].split('test_')[1]
            
            #file = mkpath(testcase +".sql")
            file = local_path(testcase +".sql")
        else:
            #file = mkpath(sqlfile)
            file = local_path(sqlfile)
        # run psql on file, and check result
        #psql.runfile(file,timeout=timeout,host=host,port=port,username=username,password=password,flag=flags)
        #self.checkResult(ifile=file, optionalFlags=" -B")

        out_file = local_path(testcase + ".out")
        ans_file = local_path(testcase +".ans")
        PSQL.run_sql_file(sql_file = file, out_file = out_file)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))
            
    def test_areas(self):
        """postgis: Test case - Areas"""
        self.do_test()

    def test_roads(self):
        """postgis: Test case - Roads"""
        self.do_test()
        
    def test_datatypes_point(self):
        """postgis: Datatypes - POINT"""
        self.do_test()
        
    def test_datatypes_linestring(self):
        """postgis: Datatypes - LINESTRING"""
        self.do_test()
        
    def test_datatypes_polygon(self):
        """postgis: Datatypes - POLYGON"""
        self.do_test()
       
    def test_datatypes_multipoint(self):
        """postgis: Datatypes - MULTIPOINT"""
        self.do_test()
        
    def test_datatypes_multilinestring(self):
        """postgis: Datatypes - MULTILINESTRING"""
        self.do_test()
        
    def test_datatypes_multipolygon(self):
        """postgis: Datatypes - MULTIPOLYGON"""
        self.do_test()
        
    def test_datatypes_geometrycollection(self):
        """postgis: Datatypes - GEOMETRYCOLLECTION"""
        self.do_test()

