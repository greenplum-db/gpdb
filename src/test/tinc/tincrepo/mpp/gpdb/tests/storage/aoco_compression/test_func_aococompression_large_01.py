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
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.aoco_compression import GenerateSqls

class AOCOCompressionTestCase(ScenarioTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """



    @classmethod
    def setUpClass(cls):
        gensql = GenerateSqls()
        gensql.generate_sqls()


    def test_aoco_large_block(self):
        '''
        @data_provider test_types_large
        '''
        test_list1 = []
        test_list1.append("mpp.gpdb.tests.storage.aoco_compression.test_runsqls.%s" % self.test_data[1][0])
        self.test_case_scenario.append(test_list1)

    def test_validation(self):
        '''
        Check catakog and checkmirrorintegrity
        note: Seperating this out to not run as part of every test
        '''
        test_list1 = []
        test_list1.append("mpp.gpdb.tests.storage.lib.dbstate.DbStateClass.run_validation")
        self.test_case_scenario.append(test_list1)


@tinctest.dataProvider('test_types_large')
def test_data_provider():
    data = {'test_04_3_ao_create_with_row_large':['ao_create_with_row_large_2G_zlib'],
            'test_04_4_ao_create_with_row_large':['ao_create_with_row_large_2G_quick_rle'],
            'test_05_3_co_create_with_column_large':['co_create_with_column_large_2G_zlib'],
            'test_05_4_co_create_with_column_large':['co_create_with_column_large_2G_quick_rle'],
            'test_05_6_co_create_with_column_large':['co_create_with_column_large_2G_zlib_2'],
            'test_06_1_ao_create_with_row_part_large':['ao_create_with_row_part_large_1G_zlib'],
            'test_06_2_ao_create_with_row_part_large':['ao_create_with_row_part_large_1G_quick_rle'],
            'test_06_5_ao_create_with_row_part_large':['ao_create_with_row_part_large_1G_zlib_2'],
            'test_08_1_co_create_with_column_part_large':['co_create_with_column_part_large_1G_zlib'],
            'test_08_2_co_create_with_column_part_large':['co_create_with_column_part_large_1G_quick_rle'],
            'test_08_3_co_create_with_column_part_large':['co_create_with_column_part_large_1G_zlib_2'],
           }
    return data


