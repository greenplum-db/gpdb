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
import platform
import shutil

import tinctest

from mpp.models import SQLTestCase
from mpp.lib import mppUtil
from mpp.lib.config import GPDBConfig
from mpp.lib.GPFDIST import GPFDIST

'''
Class to generate and run the sqls
'''
@tinctest.skipLoading('scenario') 
class co_create_storage_directive_small(SQLTestCase):
    """
    @description: Create tables with storage_directive
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'co_create_storage_directive/small/'
    ans_dir = 'expected/co_create_storage_directive/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_storage_directive_large_1G_zlib(SQLTestCase):
    """
    @description: Create tables with storage_directive
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'co_create_storage_directive/large_1G_zlib/'
    ans_dir = 'expected/co_create_storage_directive/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_storage_directive_large_1G_zlib_2(SQLTestCase):
    """
    @description: Create tables with storage_directive
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_storage_directive/large_1G_zlib_2/'
    ans_dir = 'expected/co_create_storage_directive/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_storage_directive_large_1G_quick_rle(SQLTestCase):
    """
    @description: Create tables with storage_directive
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_storage_directive/large_1G_quick_rle/'
    ans_dir = 'expected/co_create_storage_directive/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_storage_directive_large_2G_zlib(SQLTestCase):
    """
    @description: Create tables with storage_directive
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_storage_directive/large_2G_zlib/'
    ans_dir = 'expected/co_create_storage_directive/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_storage_directive_large_2G_zlib_2(SQLTestCase):
    """
    @description: Create tables with storage_directive
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_storage_directive/large_2G_zlib_2/'
    ans_dir = 'expected/co_create_storage_directive/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_storage_directive_large_2G_quick_rle(SQLTestCase):
    """
    @description: Create tables with storage_directive
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_storage_directive/large_2G_quick_rle/'
    ans_dir = 'expected/co_create_storage_directive/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_small(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'ao_create_with_row/small/'
    ans_dir = 'expected/ao_create_with_row/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_large_1G_zlib(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    sql_dir = 'ao_create_with_row/large_1G_zlib/'
    ans_dir = 'expected/ao_create_with_row/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_large_1G_zlib_2(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    sql_dir = 'ao_create_with_row/large_1G_zlib_2/'
    ans_dir = 'expected/ao_create_with_row/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_large_1G_quick_rle(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    sql_dir = 'ao_create_with_row/large_1G_quick_rle/'
    ans_dir = 'expected/ao_create_with_row/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_large_2G_zlib(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    sql_dir = 'ao_create_with_row/large_2G_zlib/'
    ans_dir = 'expected/ao_create_with_row/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_large_2G_zlib_2(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    sql_dir = 'ao_create_with_row/large_2G_zlib_2/'
    ans_dir = 'expected/ao_create_with_row/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_large_2G_quick_rle(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    sql_dir = 'ao_create_with_row/large_2G_quick_rle/'
    ans_dir = 'expected/ao_create_with_row/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_small(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause -CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'co_create_with_column/small/'
    ans_dir = 'expected/co_create_with_column/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_large_1G_zlib(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause -CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column/large_1G_zlib/'
    ans_dir = 'expected/co_create_with_column/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_large_1G_zlib_2(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause -CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column/large_1G_zlib_2/'
    ans_dir = 'expected/co_create_with_column/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_large_1G_quick_rle(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause -CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column/large_1G_quick_rle/'
    ans_dir = 'expected/co_create_with_column/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_large_2G_zlib(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause -CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column/large_2G_zlib/'
    ans_dir = 'expected/co_create_with_column/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_large_2G_zlib_2(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause -CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column/large_2G_zlib_2/'
    ans_dir = 'expected/co_create_with_column/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_large_2G_quick_rle(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause -CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column/large_2G_quick_rle/'
    ans_dir = 'expected/co_create_with_column/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_part_small(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'ao_create_with_row_part/small/'
    ans_dir = 'expected/ao_create_with_row_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_part_large_1G_zlib(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'ao_create_with_row_part/large_1G_zlib/'
    ans_dir = 'expected/ao_create_with_row_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_part_large_1G_zlib_2(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'ao_create_with_row_part/large_1G_zlib_2/'
    ans_dir = 'expected/ao_create_with_row_part/'
    out_dir = 'output/'


@tinctest.skipLoading('scenario') 
class ao_create_with_row_part_large_1G_quick_rle(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'ao_create_with_row_part/large_1G_quick_rle/'
    ans_dir = 'expected/ao_create_with_row_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_part_large_2G_zlib(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'ao_create_with_row_part/large_2G_zlib/'
    ans_dir = 'expected/ao_create_with_row_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_part_large_2G_zlib_2(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'ao_create_with_row_part/large_2G_zlib_2/'
    ans_dir = 'expected/ao_create_with_row_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_part_large_2G_quick_rle(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'ao_create_with_row_part/large_2G_quick_rle/'
    ans_dir = 'expected/ao_create_with_row_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class ao_create_with_row_sub_part_small(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at sub-partition level - AO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'ao_create_with_row_sub_part/small/'
    ans_dir = 'expected/ao_create_with_row_sub_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_part_small(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'co_create_with_column_part/small/'
    ans_dir = 'expected/co_create_with_column_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_part_large_1G_zlib(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column_part/large_1G_zlib/'
    ans_dir = 'expected/co_create_with_column_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_part_large_1G_zlib_2(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column_part/large_1G_zlib_2/'
    ans_dir = 'expected/co_create_with_column_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_part_large_1G_quick_rle(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column_part/large_1G_quick_rle/'
    ans_dir = 'expected/co_create_with_column_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_part_large_2G_zlib(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column_part/large_2G_zlib/'
    ans_dir = 'expected/co_create_with_column_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_part_large_2G_zlib_2(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column_part/large_2G_zlib_2/'
    ans_dir = 'expected/co_create_with_column_part/'
    out_dir = 'output/'


@tinctest.skipLoading('scenario') 
class co_create_with_column_part_large_2G_quick_rle(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at partition level - CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_create_with_column_part/large_2G_quick_rle/'
    ans_dir = 'expected/co_create_with_column_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_with_column_sub_part_small(SQLTestCase):
    """
    @description: Create tables with compression attributes in with clause at sub-partition level - CO tables
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'co_create_with_column_sub_part/small/'
    ans_dir = 'expected/co_create_with_column_sub_part/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class create_col_with_diff_column_reference(SQLTestCase):
    """
    @description: Create table with each column having different
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
                  compresstypes in column reference
    """
    
    sql_dir = 'create_col_with_diff_column_reference/'
    ans_dir = 'expected/create_col_with_diff_column_reference/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class create_col_with_diff_storage_directive(SQLTestCase):
    """
    @description: Create table with each column having different 
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
                  compresstypes in storage_directive
    """
    
    sql_dir = 'create_col_with_diff_storage_directive/'
    ans_dir = 'expected/create_col_with_diff_storage_directive/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class create_col_with_storage_directive_and_col_ref(SQLTestCase):
    """
    @description: Create table with storage_directive and column_reference
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'create_col_with_storage_directive_and_col_ref/'
    ans_dir = 'expected/create_col_with_storage_directive_and_col_ref/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_alter_table_add(SQLTestCase):
    """
    @description: Alter table add new column with compression
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'co_alter_table_add/'
    ans_dir = 'expected/co_alter_table_add/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_alter_type(SQLTestCase):
    """
    @description: Alter a built in datatype to use compression
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'co_alter_type/'
    ans_dir = 'expected/co_alter_type/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class co_create_type(SQLTestCase):
    """
    @description: Create type with compresstypes
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    
    sql_dir = 'co_create_type/'
    ans_dir = 'expected/co_create_type/'
    out_dir = 'output/'

@tinctest.skipLoading('scenario') 
class other_tests(SQLTestCase):
    """
    @description: Other AO and CO related tests
    @gucs gp_create_table_random_default_distribution=off
    @product_version gpdb: [4.3-]
    """
    sql_dir = 'other_tests/'
    ans_dir = 'expected/other_tests/'
    out_dir = 'output/'

    @classmethod
    def setUpClass(cls):
        super(other_tests, cls).setUpClass()
        source_dir = cls.get_source_dir()
        config = GPDBConfig()
        host, _ = config.get_hostandport_of_segment(0)
        port = mppUtil.getOpenPort(8080)
        tinctest.logger.info("gpfdist host = {0}, port = {1}".format(host, port))

        data_dir = os.path.join(source_dir, 'data')
        cls.gpfdist = GPFDIST(port, host, directory=data_dir)
        cls.gpfdist.startGpfdist()

        data_out_dir = os.path.join(data_dir, 'output')
        shutil.rmtree(data_out_dir, ignore_errors=True)
        os.mkdir(data_out_dir)

    @classmethod
    def tearDownClass(cls):
        cls.gpfdist.killGpfdist()
        super(other_tests, cls).tearDownClass()
