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

import inspect
import os
import random
import sys

import tinctest
from mpp.lib.PSQL import PSQL

class GenerateSqls():
    
    def local_path(self, filename):
        """Return the absolute path of the input file.:Overriding it here to use the absolute path instead of relative"""
        frame = inspect.stack()[1]
        source_file = inspect.getsourcefile(frame[0])
        source_dir = os.path.dirname(os.path.abspath(source_file))
        return os.path.join(source_dir, filename)
    
    def writeConcurrentcyIteration(self, filename, concurrency=None, iterations=None):  
        if iterations is None:
            iterations = random.randint(5,10)
            #iterations = 4
        if concurrency is None:
            concurrency = random.randint(5,10)
            #concurrency = 5
        filename.write("-- @concurrency %s \n" % concurrency)
        filename.write("-- @iterations %s \n" % iterations)  
        
    def column_generation(self, col_count=10):
        
        table_def = "c1 int, c2 char(10), c3 date, c4 text, c5 timestamp, c6 numeric, c7 varchar, c8 int, c9 text, c10 date"
        if col_count == 100:
            for i in range(1, 10):
                table_def == table_def + ' , ' + table_def
        return table_def
        
            
    def data_generation(self, col_count=10):
        
        insert_row = "generate_series(1,6), 'pt_test', '2013-01-01', 'persistent tables testing', '2002-11-13 03:51:15+1359', generate_series(20,24), 'persistent tables' , 2013, 'again same text pt_testing', '2013-03-25'"
        if col_count == 100 :
            for i in range(1, 10):
                insert_row == insert_row + ' , ' + insert_row
        return insert_row    
                 
    def create_tb_string(self, tablename, storage_type, compression='no', partition='no'):
        
        storage_orientation = {'ao':'row', 'co': 'column'}
        if storage_type == 'co':
            col_count = 100
        else:
            col_count = 10
            
        table_definition = self.column_generation(col_count)
        
        create_table_str = "Create table " + tablename + "(" + table_definition + ")"
        
        part_string = " Partition by range(c1) \n Subpartition by range(c6)  Subpartition Template  (default subpartition subothers, start(20) end(24) every(1)) \n (default partition others, start (1) end (6) every (1)) "
                
        compression_list = ['quicklz', 'zlib']
        if storage_type == "heap":
            create_table_str = create_table_str 
              
        elif storage_type == "co"  or storage_type == "ao" :
            create_table_str = create_table_str + " WITH(appendonly = true, orientation = " + storage_orientation[storage_type] + ") "
            if compression == "yes" :
                create_table_str = create_table_str[:-2] + ", compresstype = " + random.choice(compression_list) + ") "
        if partition == "yes" :
                create_table_str = create_table_str + part_string
                
        create_table_str = create_table_str + ";\n" 
        
        return create_table_str
            
    def create_tables(self, table_type, storage_type, compression='no', partition='no'):
        ''' Generate the DDLs for different types of tables '''
        
        if storage_type == 'co':
            col_count = 100
        else:
            col_count = 10
        # Lets have three files with random statements written to them.
        # First write concurrency and iteration to all the files 
        filename = self.local_path('%s_1.sql' % table_type)
        file1 = open(filename , "w")
        self.writeConcurrentcyIteration(file1)
        filename = self.local_path('%s_1.ans' % table_type)
        file1_ans = open(filename, "w")
        
        filename = self.local_path('%s_2.sql' % table_type)
        file2 = open(filename , "w")
        self.writeConcurrentcyIteration(file2)
        filename = self.local_path('%s_2.ans' % table_type)
        file2_ans = open(filename, "w")
        
        filename = self.local_path('%s_3.sql' % table_type)
        file3 = open(filename , "w")
        self.writeConcurrentcyIteration(file3)
        filename = self.local_path('%s_3.ans' % table_type)
        file3_ans = open(filename, "w")             
        
        for t in range(1, 201):
            tablename = '%s_table_%s' % (table_type, t)
            create_table_str = self.create_tb_string(tablename, storage_type, compression, partition)
            
            sql_file = random.choice([file1, file2, file3])
            if t % 10 == 0:
                sql_file.write('Begin; \n')
            sql_file.write('%s \n' % create_table_str)
            
            # Due to the Deadlock issue index creation part is commented out Ref. MPP-19781
            # if t % 5 == 0:
                # create_index_str = "Create index " + tablename + "_idx_" + str(t) + " on " + tablename + " (c1);\n"
                # sql_file.write('%s \n' % create_index_str)
            
            insert_str = 'Insert into %s values ( %s ); \n ' % (tablename, self.data_generation(col_count))
            sql_file.write('%s \n' % insert_str)
            
            sql_file.write("Drop table if exists %s; \n" % tablename)
            if t % 20 == 0 :
                sql_file.write('Commit;\n')
            elif t % 10 == 0 and t % 20 != 0 :
                sql_file.write('Abort; \n')
            
    def create_table_setup(self, table_prefix, table_type, storage_type, partition='no'):
        ''' This is few create tables that are a pre-req for the actual load'''
        filename = self.local_path('setup/%s_%s_pre.sql' % (table_type, table_prefix))
        sql_file = open(filename , "w")
        self.writeConcurrentcyIteration(sql_file, concurrency=1, iterations=1)
                 
        for t in range(1, 201):
            tablename = '%s_%s_%s' % (table_type, table_prefix, t)
            
            create_table_str = self.create_tb_string(tablename, storage_type) 
            sql_file.write('Drop table if exists %s; \n' % tablename)
            sql_file.write('%s \n' % create_table_str)
        
    def insert_rows(self, table_type, storage_type, partition='no'):
        #self.create_table_setup('insert_tb', table_type, storage_type, partition)
        filename = self.local_path('%s_insert_data.sql' % table_type)
        sql_file = open(filename , "w")
        #self.writeConcurrentcyIteration(sql_file, concurrency=100)
        self.writeConcurrentcyIteration(sql_file)
        filename = self.local_path('%s_insert_data.ans' % table_type)
        sql_file_ans = open(filename , "w")
        
        if storage_type == 'co':
            col_count = 100
        else:
            col_count = 10
            
        for t in range(1, 201):
            tablename = '%s_insert_tb_%s' % (table_type, t)
            if t % 3 == 0:
                insert_str = 'Begin; \n Insert into %s values ( %s ); \nCommit; \n ' % (tablename, self.data_generation(col_count))
                sql_file.write('%s \n' % insert_str)
            else:
                insert_str = 'Begin; \n Insert into %s values ( %s ); \nAbort; \n ' % (tablename, self.data_generation(col_count))
                sql_file.write('%s \n' % insert_str)
            
    def drop_recreate_table(self, table_type, storage_type):
        #self.create_table_setup('drop_tb', table_type, storage_type)
        filename = self.local_path('%s_drop_tb.sql' % table_type)
        sql_file = open(filename , "w")
        self.writeConcurrentcyIteration(sql_file)
        filename = self.local_path('%s_drop_tb.ans' % table_type)
        sql_file_ans = open(filename , "w")
        
        for t in range(1, 201):
            tablename = '%s_drop_tb_%s' % (table_type, random.randint(1, 200))
            drop_str = 'Drop table %s; \nCreate table %s (i int, t text); \n' % (tablename, tablename)
            if t % 5 == 0 and t % 10 != 0:
                sql_file.write('Begin; \n %sCommit; \n' % drop_str)
            elif t % 5 == 0  :
                sql_file.write('Begin; \n %sAbort; \n ' % drop_str)
            else :
                sql_file.write('%s \n' % drop_str)
      
    def generate_sqls(self):
        
        for storage_type in ('ao', 'co', 'heap'):
            self.create_tables(storage_type, storage_type)
            self.create_tables(storage_type + '_part', storage_type, 'no', 'yes')
            if storage_type != 'heap' :
                self.create_tables(storage_type + "_compr", storage_type, "yes")
            self.insert_rows(storage_type, storage_type)
            self.insert_rows(storage_type + '_part', storage_type, 'yes')
            self.drop_recreate_table(storage_type, storage_type)
