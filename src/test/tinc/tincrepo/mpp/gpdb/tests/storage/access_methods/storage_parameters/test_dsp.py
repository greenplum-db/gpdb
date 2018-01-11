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

from tinctest.lib import local_path
from mpp.models import SQLTestCase
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.access_methods.storage_parameters import DspClass

'''
Test for Default Storage Parameters
'''

class DefaultStorageParameterTestCase(SQLTestCase):
    """
    
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3.4.0-]
    """

    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'

    os.environ['PGPASSWORD'] = 'dsprolepwd'

    @classmethod
    def setUpClass(cls):
        """
        @description: Create the databases and roles to be used in the test
        """
        PSQL.run_sql_file(local_path('setup.sql'), dbname='postgres')
        
        dsp = DspClass()
        dsp.add_user()
        super(DefaultStorageParameterTestCase, cls).setUpClass()

    @classmethod
    def tearDownClass(cls):
        dsp = DspClass()
        dsp.remove_user()
        super(DefaultStorageParameterTestCase, cls).tearDownClass()
