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

import fnmatch
import os
import sys
import tinctest
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from gppylib.db import dbconn
from gppylib.commands.base import Command, ExecutionError
from tinctest.lib import Gpdiff

class IndexScanTestCase(MPPTestCase):

    """
    @product_version gpdb: [4.3.5.0-]
    """

    def __init__(self, methodName):
        super(IndexScanTestCase, self).__init__(methodName)
        self.dbname = os.environ['PGDATABASE']

    @classmethod
    def setUpClass(cls):
        base_dir = os.path.dirname(sys.modules[cls.__module__].__file__)

        #Create the output directory
        output_dir = os.path.join(base_dir, 'output')
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        sql_dir = os.path.join(base_dir, 'sql')

        #Create the readindex function
        sql_file = os.path.join(sql_dir, 'create_function.sql')
        out_file = os.path.join(output_dir, 'create_function.out')
        ans_file = os.path.join(base_dir, 'expected', 'create_function.ans')
        PSQL.run_sql_file(sql_file, out_file=out_file)
        if not Gpdiff.are_files_equal(out_file, ans_file):
            raise Exception('Unable to create readindex function')

    def _get_host_and_port_for_table(self, tablename):
        HOST_AND_PORT_QUERY = """
        SELECT hostname, port
        FROM gp_segment_configuration
        WHERE role = 'p' AND content = (SELECT DISTINCT(gp_segment_id)
                                        FROM %s)
        """ % tablename
        with dbconn.connect(dbconn.DbURL(dbname=self.dbname)) as conn:
            host, port = dbconn.execSQLForSingletonRow(conn, HOST_AND_PORT_QUERY)
        return host, port

    def run_test(self, data_file, sql_file, out_file, ans_file, table):
        base_dir = os.path.dirname(sys.modules[self.__module__].__file__)
        out_file = os.path.join(base_dir, 'output', out_file)
        ans_file = os.path.join(base_dir, 'expected', ans_file)
        sql_file = os.path.join(base_dir, 'sql', sql_file)

        data_out_file = os.path.join(base_dir, 'output', data_file.strip('.sql') + '.out')
        data_ans_file = os.path.join(base_dir, 'expected', data_file.strip('.sql') + '.ans')
        data_file = os.path.join(base_dir, 'sql', data_file) 

        PSQL.run_sql_file(data_file, out_file=data_out_file)
        self.assertTrue(Gpdiff.are_files_equal(data_out_file, data_ans_file))

        host, port = self._get_host_and_port_for_table(table)
        PSQL.run_sql_file_utility_mode(sql_file, host=host, port=port, out_file=out_file)
        self.assertTrue(Gpdiff.are_files_equal(out_file, ans_file))

    def test_index_scan_heap(self):
        self.run_test(data_file='heap_index.sql', sql_file='read_heap_index.sql', out_file='read_heap_index.out',
                      ans_file='read_heap_index.ans', table='heap_t1')

    def test_index_scan_ao(self):
        self.run_test(data_file='ao_index.sql', sql_file='read_ao_index.sql', out_file='read_ao_index.out',
                      ans_file='read_ao_index.ans', table='ao_t1')

    def test_index_scan_co(self):
        self.run_test(data_file='co_index.sql', sql_file='read_co_index.sql', out_file='read_co_index.out',
                      ans_file='read_co_index.ans', table='co_t1')
