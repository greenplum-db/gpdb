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

from mpp.gpdb.tests.storage.lib.sql_isolation_testcase import SQLIsolationTestCase
from mpp.models import SQLConcurrencyTestCase, GpfdistSQLTestCase
from mpp.lib.gpConfig import GpConfig
from mpp.lib.gpstop import GpStop

class AppendOnlyEOFTests(SQLIsolationTestCase):
    """
    @product_version gpdb: [4.3.3.0-]
    @gucs gp_create_table_random_default_distribution=off
    """
    
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    data_dir = 'data/'
    out_dir = 'output/'


    master_value = None
    segment_value = None


    @classmethod
    def setUpClass(cls):
        super(AppendOnlyEOFTests, cls).setUpClass()
        gpconfig = GpConfig()
        (cls.master_value, cls.segment_value) = gpconfig.getParameter('max_appendonly_tables')
        tinctest.logger.debug("Original max_appendonly_tables values - Master Value: %s Segment Value: %s" %(cls.master_value, cls.segment_value))
        gpconfig.setParameter('max_appendonly_tables', '3', '3')
        GpStop().run_gpstop_cmd(restart=True)
        (master_value, segment_value) = gpconfig.getParameter('max_appendonly_tables')
        tinctest.logger.debug("Set max_appendonly_tables to Master Value: %s  Segment Value: %s " %(master_value, segment_value))
        if master_value != '3' or segment_value != '3':
            raise Exception("Failed to set max_appendonly_tables to the required values")


    @classmethod
    def tearDownClass(cls):
        gpconfig = GpConfig()
        if (not cls.master_value) or (not cls.segment_value):
            raise Exception("Original max_appendonly_tables value is None")

        gpconfig.setParameter('max_appendonly_tables', cls.master_value, cls.segment_value)
        GpStop().run_gpstop_cmd(restart=True)
        # Make sure the values are reset properly
        (master_value, segment_value) = gpconfig.getParameter('max_appendonly_tables')
        try:
            if master_value != cls.master_value or segment_value != cls.segment_value:
                raise Exception("Failed to reset max_appendonly_tables to the required values")
        finally:
            super(AppendOnlyEOFTests, cls).tearDownClass()


class AOCOEOFConcurrencyTests(GpfdistSQLTestCase, SQLConcurrencyTestCase):
    """
    @gpdiff True
    @product_version gpdb: [4.3.3.0-]
    @gucs gp_create_table_random_default_distribution=off
    """

    sql_dir = 'sql_concurrency/'
    ans_dir = 'expected_concurrency/'
    data_dir = 'data/'
    out_dir = 'output_concurrency/'    
