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

import inspect
import os
import random
import sys
import tinctest

from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.persistent_tables.sqls.generate_sqls import GenerateSqls

'''Creates the partition table sqls'''
class GeneratePartitionSqls(GenerateSqls):
           
    def create_tb_string(self, tablename, storage_type, partitions, compression='no'):
        '''Generates the create table string'''
        storage_orientation = {'ao':'row', 'co': 'column'}
        if storage_type == 'co':
            col_count = 100
        else:
            col_count = 10
            
        table_definition = self.column_generation(col_count)
        
        drop_table_str = "DROP TABLE IF EXISTS %s;\n" % tablename
        
        create_table_str = drop_table_str + "Create table " + tablename + "(" + table_definition + ")"

        part_string = "\nPARTITION BY RANGE(c1) ( \n start (1) end (%s) every (1) \n );" % partitions
        
        compression_list = ['quicklz', 'zlib']
        
        if storage_type == "co"  or storage_type == "ao" :
            create_table_str = create_table_str + " WITH(appendonly = true, orientation = " + storage_orientation[storage_type] + ") "
            if compression == "yes" :
                create_table_str = create_table_str[:-2] + ", compresstype = " + random.choice(compression_list) + ") "
                
        create_table_str = create_table_str + part_string
        
        return create_table_str
        
    def create_tables(self, table_type, storage_type, no_of_tables, no_of_partitions,compression='no'):
        ''' Generate the DDLs for different types of tables '''
        
        no_of_partitions = no_of_partitions + 1
        
        if storage_type == 'co':
            col_count = 100
        else:
            col_count = 10
       
        filename = self.local_path('%s.sql' % table_type)
        file = open(filename , "w")
        self.writeConcurrentcyIteration(file,1,1)
        file.write("BEGIN;\n")
        filename = self.local_path('%s.ans' % table_type)
        file_ans = open(filename, "w")
        
        for t in range(1, no_of_tables+1):
            tablename = '%s_table_%s' % (table_type, t)
            create_table_str = self.create_tb_string(tablename, storage_type, no_of_partitions, compression)
            file.write('%s \n' % create_table_str)
            

    def generate_sqls(self, no_of_tables = 1 , no_of_partitions = 50):
        
        for storage_type in ('ao', 'co', 'heap'):
            self.create_tables(storage_type, storage_type,no_of_tables,no_of_partitions,'no')
            if storage_type != 'heap' :
                self.create_tables(storage_type + "_compr", storage_type, no_of_tables,no_of_partitions,"yes")
