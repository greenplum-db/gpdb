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

import os
import time
import unittest2 as unittest
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from mpp.lib.gpdbverify import GpdbVerify

class GpdbVerifyRegressionTests(unittest.TestCase):

    def __init__(self, methodName):
        self.gpv = GpdbVerify()
        super(GpdbVerifyRegressionTests, self).__init__(methodName)
    
    def setUp(self):
        PSQL.run_sql_command('create database gptest;', dbname='postgres')

    def tearDown(self):
        PSQL.run_sql_command('drop database gptest', dbname='postgres')
     
    def test_gpcheckcat(self):
        (a,b,c,d) = self.gpv.gpcheckcat()
        self.assertIn(a,(0,1,2))

    def test_gpcheckmirrorseg(self):
        (res,fix_file) = self.gpv.gpcheckmirrorseg()
        self.assertIn(res, (True,False))

    def test_check_db_is_running(self):
        self.assertTrue(self.gpv.check_db_is_running())

    def test_run_repairscript(self):
        repair_script = local_path('gpcheckcat_repair')
        res = self.gpv.run_repair_script(repair_script)
        self.assertIn(res, (True,False))

    def test_ignore_extra_m(self):
        fix_file = local_path('fix_file')
        res = self.gpv.ignore_extra_m(fix_file)
        self.assertIn(res, (True,False))
     
    def test_cleanup_old_file(self):
        old_time = int(time.strftime("%Y%m%d%H%M%S")) - 1005000 
        old_file = local_path('checkmirrorsegoutput_%s' % old_time)
        open(old_file,'w')
        self.gpv.cleanup_day_old_out_files(local_path(''))
        self.assertFalse(os.path.isfile(old_file))
        
    def test_not_cleanup_todays_file(self):
        new_file = local_path('checkmirrorsegoutput_%s' % time.strftime("%Y%m%d%H%M%S"))
        open(new_file,'w')
        self.gpv.cleanup_day_old_out_files(local_path(''))
        self.assertTrue(os.path.isfile(new_file))
        
