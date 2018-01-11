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
import tinctest
from tinctest.lib import local_path, run_shell_command
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.access_methods.storage_parameters import DspClass
from mpp.gpdb.tests.storage.lib.base import BaseClass
from mpp.models import MPPTestCase
from mpp.models import SQLTestCase
from tinctest.models.scenario import ScenarioTestCase

'''
Test for Default Storage Parameters
'''
@tinctest.skipLoading('scenario')
class DspSystemSqls(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """
    sql_dir = 'sql_system/'
    ans_dir = 'expected/'
    out_dir = 'output/'

class DspSystemTestCase(ScenarioTestCase, MPPTestCase):
    """
    
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3.4.0-]
    """
    os.environ['PGPASSWORD'] = 'dsprolepwd'

    @classmethod
    def setUpClass(cls):
        """
        @description: Create the databases and roles to be used in the test
        """
        PSQL.run_sql_file(local_path('setup.sql'), dbname='postgres')
        dsp = DspClass()
        dsp.add_user()

    @classmethod
    def tearDownClass(cls):
        dsp = DspClass()
        dsp.remove_user()
        base = BaseClass('reset_gpconfig')
        base.reset_gpconfig('gp_default_storage_options')

    

    
    def test_dsp_system_level_guc(self):
        ''' 
        @data_provider parameter_options
        '''
        test_name = self.test_data[1][0]
        storage_option = self.test_data[1][1]
        test_list_0 = []
        test_list_0.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.set_gpconfig', ['gp_default_storage_options', storage_option]))
        test_list_0.append(('mpp.gpdb.tests.storage.access_methods.storage_parameters.test_dsp_scenario.DspSystemSqls.test_%s' % test_name))
        self.test_case_scenario.append(test_list_0, serial=True)


    def test_dsp_system_level_masteronly(self):
        '''
        Setting the gp_default_storage_options with --masteronly or -m option is not supported
        '''
        command = "gpconfig -c gp_default_storage_options -v 'appendonly=true' --masteronly "
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command (command, 'run gpconfig with masteronly', res)
        self.assertEqual(res['rc'], 1, msg='gpconfig with masteronly is expected to fail')

        command = "gpconfig -c gp_default_storage_options -v 'appendonly=true' -m 'appendonly=true'"
        run_shell_command (command, 'run gpconfig with -m', res)
        self.assertEqual(res['rc'], 1, msg='gpconfig with different seg and master values is expected to fail')

        command = "gpstop -u"
        run_shell_command (command, 'reload postgresql.conf', res)
        self.assertEqual(res['rc'], 0, msg='reloading postgresql.conf should succeed')

        command = "gpconfig -s gp_default_storage_options"
        run_shell_command (command, 'show current settings for gp_default_storage_options', res)
        self.assertNotIn("appendonly=true", res['stdout'], msg="master and segment values should show appendonly=false")

    def test_dsp_system_level_invalid_guc(self):
        test_list_0 = []
        test_list_0.append(('mpp.gpdb.tests.storage.lib.base.BaseClass.set_gpconfig', ['gp_default_storage_options', 'invalidvalue']))
        self.test_case_scenario.append(test_list_0)

@tinctest.dataProvider('parameter_options')
def test_data_provider():
    data = {'appendonly_true_orientation_column_checksum_false_compresstype_quicklz': ['system_test_1','appendonly=true,orientation=column,checksum=false,compresstype=quicklz'],
            'appendonly_true_checksum_true_compresslevel_5': ['system_test_2', 'appendonly=true,checksum=true,compresslevel=5'],
            'appendonly_true_orientation_column_compresstype_rle_type': ['system_test_3','appendonly=true,orientation=column,compresstype=rle_type'],
            'appendonly_false': ['system_test_4', 'appendonly=false'],
            'system_and_database_level':['system_test_5','orientation=column,compresstype=zlib,compresslevel=6'],
            'system_database_and_role_level':['system_test_6', 'appendonly=true,orientation=column,checksum=true'],
            'system_database_role_and_session_level': ['system_test_7', 'orientation=column,compresstype=quicklz,compresslevel=1'],
            }
    return data
