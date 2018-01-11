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

from mpp.lib.gplog import GpLog, GpLogException, _DEFAULT_OUT_FILE
from mpp.lib.PSQL import PSQL

class GpLogRegressionTests(unittest.TestCase):

    def test_gather_log_to_default_file(self):
        if os.path.exists(_DEFAULT_OUT_FILE):
            os.remove(_DEFAULT_OUT_FILE)
        self.assertFalse(os.path.exists(_DEFAULT_OUT_FILE))
        start_time = time.time()
        PSQL.run_sql_command("select pg_sleep(2)")
        end_time = time.time()
        GpLog.gather_log(start_time=start_time, end_time=end_time)
        self.assertTrue(os.path.exists(_DEFAULT_OUT_FILE))
        self.assertTrue(os.path.getsize(_DEFAULT_OUT_FILE) > 0)

    def test_gather_log_out_file(self):
        out_file = '/tmp/cluster2.logs'
        if os.path.exists(out_file):
            os.remove(out_file)
        self.assertFalse(os.path.exists(out_file))
        start_time = time.time()
        time.sleep(2)
        end_time = time.time()
        GpLog.gather_log(start_time=start_time, end_time=end_time, out_file=out_file)
        self.assertTrue(os.path.exists(out_file))
        self.assertTrue(os.path.getsize(out_file) > 0)

    def test_check_log(self):
        start_time = time.time()
        PSQL.run_sql_command("SELECT * from some_table_that_does_not_exist_to_generate_errors_in_logs")
        time.sleep(2)
        end_time = time.time()
        self.assertTrue(GpLog.check_log_for_errors(start_time, end_time))
        
        
        
    
