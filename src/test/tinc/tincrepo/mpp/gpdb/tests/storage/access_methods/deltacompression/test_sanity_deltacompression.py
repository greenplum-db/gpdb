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

import os
import tinctest
from mpp.models import SQLTestCase
from mpp.models import MPPTestCase
from tinctest.models.scenario import ScenarioTestCase
from mpp.gpdb.tests.storage.lib.gp_filedump import GpfileTestCase
'''
Test for Delta Compression
'''
@tinctest.skipLoading('scenario')
class DeltaCompressionTestCase(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    
    @dbname deltagp
    @product_version gpdb: [4.3.3.0-]
    """

    sql_dir = 'sql_gp/'
    ans_dir = 'expected/'
    out_dir = 'output/'


@tinctest.skipLoading('scenario')
class DeltaHelper(MPPTestCase):
    
    def gp_filedump_check(self, checksum=False):
        gpfile = GpfileTestCase()
        dbname=os.environ.get('PGDATABASE')
        if checksum == True:
            flag = " -M "
        else:
            flag = " "
        table_list = {"delta_1_byte":"deltas_size: 1","delta_2_byte":"deltas_size: 2",
                      "delta_3_byte":"deltas_size: 3","delta_4_byte":"deltas_size: 4",
                      "delta_neg_1_byte":"deltas_size: 1","delta_neg_2_byte":"deltas_size: 2",
                      "delta_neg_3_byte":"deltas_size: 3","delta_neg_4_byte":"deltas_size: 4"}

        (host, db_path) = gpfile.get_host_and_db_path(dbname)
        # Cases where we expect a delta
        for tablename in table_list:
            file_list = gpfile.get_relfile_list(dbname, tablename, db_path, host)
            for i in range(1, len(file_list)):
                tinctest.logger.info('Running compression check for table %s relfilenode %s' % (tablename, file_list[i]))
                has_delta = gpfile.check_in_filedump(db_path, host, file_list[i], 'HAS_DELTA_COMPRESSION', flag)
                is_byte = gpfile.check_in_filedump(db_path, host, file_list[i], table_list[tablename], flag)
                if not has_delta or not is_byte:
                    raise Exception ("Delta Compression not applied to relation with relfilenode: %s " % file_list[i])

        # Cases where we are not expecting a delta
        tablename = 'delta_more_4_byte'
        file_list = gpfile.get_relfile_list(dbname, tablename, db_path, host)
        for i in range(1, len(file_list)):
            tinctest.logger.info('Running compression check for table %s relfilenode %s' % (tablename, file_list[i]))
            delta = gpfile.check_in_filedump(db_path, host, file_list[i], 'HAS_DELTA_COMPRESSION', flag)
            if delta:
                raise Exception("Delta Compression not expected for %s" % file_list[i])


class DeltaScenarioTestCase(ScenarioTestCase, MPPTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """

    def test_delta_byte_check_post_434(self):
        '''
        @product_version gpdb: [4.3.4.0-]
        '''
        test_list_1 = []
        test_list_1.append(('mpp.gpdb.tests.storage.access_methods.deltacompression.test_sanity_deltacompression.DeltaCompressionTestCase'))
        test_list_1.append(('mpp.gpdb.tests.storage.access_methods.deltacompression.test_sanity_deltacompression.DeltaHelper.gp_filedump_check', {'checksum' : True}))
        self.test_case_scenario.append(test_list_1, serial=True)
        
