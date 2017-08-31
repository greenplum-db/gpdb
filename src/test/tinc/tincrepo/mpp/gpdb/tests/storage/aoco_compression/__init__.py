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
import string
import tinctest
from tinctest.lib import local_path, run_shell_command
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
    
class GenerateSqls(MPPTestCase):
    
    def __init__(self):
        self.compress_type_list = ["quicklz","rle_type", "zlib"]
        self.block_size_list = ["8192", "32768", "65536", "1048576", "2097152"]
        self.compress_level_list = [1, 2, 3, 4, 5, 6, 7, 8, 9]   
        self.all_columns = "a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42"
        self.alter_comprtype = {"quicklz":"zlib","rle_type":"quicklz","zlib":"rle_type"}
        
    def compare_data_with_uncompressed_table(self,tablename, sqlfile, part_table="No"):
        ''' Part of Sql file generation: Compare data with Uncompressed table'''
                       
        ## Select number of rows from the uncompressed table
        sqlfile.write("--\n-- Select number of rows from the uncompressed table \n--\n")
        sqlfile.write("SELECT count(*) as count_uncompressed from  " + tablename + "_uncompr ;" + "\n") 
        
        ## Select number of rows from the compressed table
        sqlfile.write("--\n-- Select number of rows from the compressed table \n--\n")
        sqlfile.write("SELECT count(*) as count_compressed from  " + tablename + ";" + "\n")   
        
        ## Select number of rows from the compressed table
                    
        ## Select number of rows using a FULL outer join on all the columns of the two tables: Count should match with above result if the all the rows uncompressed correctly:
        sqlfile.write("--\n-- Select number of rows using a FULL outer join on all the columns of the two tables \n")
        sqlfile.write("-- Count should match with above result if the all the rows uncompressed correctly: \n--\n")
                       
        join_string = "Select count(*) as count_join from " + tablename + " t1 full outer join " + tablename + "_uncompr t2 on t1.id=t2.id and "
        clm_list = self.get_column_list()
         
        clm_excl = ['a20', 'a21', 'a25', 'a26', 'a28', 'a31']    
        for c in range (len(clm_list)):
            clm = clm_list[c]
            if clm in clm_excl : # point and polygon has no operators
                continue;
            join_string = join_string + "t1." + clm + "=t2." + clm + " and "        
        
        join_string = join_string[:-4] + ";"
        sqlfile.write(join_string + "\n")     
                        
        ### Truncate the table
        sqlfile.write("--\n-- Truncate the table \n--\n")
        sqlfile.write("TRUNCATE table " + tablename + ";" + "\n")
                       
        ### Insert data again
        sqlfile.write("--\n-- Insert data again \n--\n")                    
        sqlfile.write("insert into " + tablename + " select * from " + tablename + "_uncompr order by a1;\n\n")
                       
        #get_ao_compression_ratio
        sqlfile.write("--\n-- Compression ratio\n--\n")
        if part_table == "No":
            sqlfile.write("select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "'); \n\n")
                                    
    def validation_sqls(self,tablename, sqlfile):
        ''' Generate sqls for validation'''
        #### Validation using psql utility ### 
        sqlfile.write("\d+ " + tablename + "\n\n") 
        #get_ao_compression_ratio
        sqlfile.write("--\n-- Compression ratio\n--\n")
        sqlfile.write("select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('" + tablename + "'); \n\n")
            
        ## Select from pg_attribute_encoding to see the table entry
        if 'with' not in tablename:
            sqlfile.write ("--Select from pg_attribute_encoding to see the table entry \n")
            sqlfile.write ("select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = '" + tablename + "' and c.oid=e.attrelid  order by relname, attnum limit 3; \n")
            
        ###Compare the selected data with that of an uncompressed table to see if the data in two tables are same when selected.
        sqlfile.write("--\n-- Compare data with uncompressed table\n--\n")
        self.compare_data_with_uncompressed_table(tablename, sqlfile)
        
    def generate_copy_files(self):
        ''' Generate the data files to be copied to the tables '''
        # Create base tables with the inserts
        PSQL.run_sql_file(local_path('create_base_tables.sql'), out_file=local_path('create_base_tables.out'))
        # Copy the rows to data files
        for tablename in ('base_small', 'base_large'):
            copy_file = local_path('data/copy_%s' % tablename)
            cp_out_cmd = "Copy %s To '%s' DELIMITER AS '|'" % (tablename, copy_file)
            out = PSQL.run_sql_command(cp_out_cmd, flags = '-q -t')
            if 'COPY 0' in out:
                raise Exception ("Copy did not work for tablename %s " % tablename)
            else:
                tinctest.logger.info('Created copy file for %s' % tablename)
            
                   
    def insert_data(self,tablename, sqlfile, block_size,compress_type):
        ''' Part of sql file creation: Insert data'''
        if block_size in ('8192', '32768', '65536'):
            copy_file = local_path('data/copy_base_small')
        elif block_size in ('1048576', '2097152'):
            copy_file = local_path('data/copy_base_large')
        
        copy_string = "COPY " + tablename  + "(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42)  FROM '"  + copy_file + "' DELIMITER AS '|' ;"
        sqlfile.write(copy_string + "\n\n")
                
    def get_compresslevel_list(self,compress_type):        
        ''' Returns a list of compresslevel for the given compresstype'''    
        if (compress_type == "quicklz"):
            compress_lvl_list = [1]
        elif (compress_type == "rle_type"):
            compress_lvl_list = [1,2,3,4]
        else:
            compress_lvl_list = self.compress_level_list 
        return compress_lvl_list    
    
    def get_table_definition(self):
        listfilename = "column_list"      
        list_file = open(local_path(listfilename), "r")
        tabledefinition = "(id SERIAL,"
            
        for line in list_file:
            tabledefinition = tabledefinition + line.strip('\n') + ","
        tabledefinition = tabledefinition[:-1] 
        list_file.close()
            
        return tabledefinition
    
    
    def get_column_list(self):
            
        column_list = ["a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "a10", "a11", "a12", "a13", "a14", "a15", "a16", "a17", "a18", "a19", "a20", "a21", "a22", "a23", "a24", "a25", "a26", "a27", "a28", "a29", "a30", "a31", "a32", "a33", "a34", "a35", "a36", "a37", "a38", "a39", "a40", "a41", "a42"]
        return column_list
                 
                 
    def alter_column_tests(self,tablename, sqlfile):
        ''' Alter column test cases '''
        # Alter type of a column
        sqlfile.write("--Alter table alter type of a column \n")
        sqlfile.write("Alter table " + tablename + " Alter column a3 TYPE int4; \n")
        sqlfile.write("--Insert data to the table, select count(*)\n")     
        sqlfile.write("Insert into " + tablename + "(" + self.all_columns + ") select " + self.all_columns + " from " + tablename + " where id =10;\n")
        sqlfile.write("Select count(*) from " + tablename + "; \n\n")  
                    
        #Drop  a column
        sqlfile.write("--Alter table drop a column \n")
        sqlfile.write("Alter table " + tablename + " Drop column a12; \n")
        sqlfile.write("Insert into " + tablename + "(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from " + tablename + " where id =10;\n")
        sqlfile.write("Select count(*) from " + tablename + "; \n\n") 
                    
        #Rename a column
        sqlfile.write("--Alter table rename a column \n")
        sqlfile.write("Alter table " + tablename + " Rename column a13 TO after_rename_a13; \n")           
                    
        sqlfile.write("--Insert data to the table, select count(*)\n")     
        sqlfile.write("Insert into " + tablename + "(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from " + tablename + " where id =10;\n")
        sqlfile.write("Select count(*) from " + tablename + "; \n\n")    
                     
        #Add a column
        sqlfile.write("--Alter table add a column \n")
        sqlfile.write("Alter table " + tablename + " Add column a12 text default 'new column'; \n")           
                    
        sqlfile.write("--Insert data to the table, select count(*)\n")     
        sqlfile.write("Insert into " + tablename + "(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from " + tablename + " where id =10;\n")
        sqlfile.write("Select count(*) from " + tablename + "; \n\n")    
    
        
    def split_and_exchange(self,tablename, sqlfile, part_level, part_type, encoding_type, orientation, tabledefinition):
        '''Exchange and split partition cases '''

         # Exchange partition

        sqlfile.write("--Alter table Exchange Partition \n--Create a table to use in exchange partition \n")
        exchange_part = tablename + "_exch"
        if orientation == "column" :
            storage_string = " WITH (appendonly=true, orientation=column, compresstype=zlib) "
        else :
            storage_string = " WITH (appendonly=true, orientation=row, compresstype=zlib) "

        exchange_table_str = "Drop Table if exists "+ exchange_part +"; \n CREATE TABLE " + exchange_part + tabledefinition + ")" + storage_string + " distributed randomly;\n"

        sqlfile.write(exchange_table_str + " \n")

        sqlfile.write("Insert into " + exchange_part + "(" + self.all_columns + ") select " + self.all_columns + " from " + tablename + " where  a1=10 and a2!='C';\n\n")

        if part_level == "sub_part":
            if part_type == "range":
                sqlfile.write("Alter table " + tablename + " alter partition FOR (RANK(1)) exchange partition sp1 with table " + exchange_part + ";\n")
                sqlfile.write("\d+ " + tablename + "_1_prt_1_2_prt_sp1\n\n")
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_1_2_prt_sp1'); \n\n")   
            else:
                if encoding_type == "with":
                    pname = "p1"
                else:
                    pname = "p2"
                sqlfile.write("Alter table " + tablename + " alter partition "+ pname +" exchange partition FOR (RANK(1)) with table " + exchange_part + ";\n")
                sqlfile.write("\d+ " + tablename + "_1_prt_"+ pname + "_2_prt_2\n\n")
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_"+ pname +"_2_prt_2'); \n\n")
        else:
            if part_type == "range":
                sqlfile.write("Alter table " + tablename + " exchange partition FOR (RANK(1)) with table " + exchange_part + ";\n")
                sqlfile.write("\d+ " + tablename + "_1_prt_1\n\n")
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_1'); \n\n")
            else:
                sqlfile.write("Alter table " + tablename + " exchange partition p1 with table " + exchange_part + ";\n")
                sqlfile.write("\d+ " + tablename + "_1_prt_p1\n\n")
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_p1'); \n\n")

            sqlfile.write("\d+ " + tablename + "_1_prt_df_p\n\n")

         #split partition

        if part_level == "sub_part":
            if part_type == "list":
                if encoding_type == "with":
                    pname = "p2"
                else:
                    pname = "p1"
                sqlfile.write("--Alter table Split Partition \n Alter table " + tablename + " alter partition "+ pname +" split partition FOR (RANK(4)) at(4000) into (partition splita,partition splitb) ;\n")
                sqlfile.write("\d+ " + tablename + "_1_prt_"+ pname +"_2_prt_splita \n\n")
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_"+ pname +"_2_prt_splita'); \n\n")

        else:
            if part_type == "range":
                sqlfile.write("--Alter table Split Partition \n Alter table " + tablename + " split partition FOR (RANK(2)) at(1050) into (partition splitc,partition splitd) ;\n")
                sqlfile.write("\d+ " + tablename + "_1_prt_splitd \n\n")
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_splitd'); \n\n")

        sqlfile.write("Select count(*) from " + tablename + "; \n\n")

        
    def alter_partition_tests(self,tablename, sqlfile, part_level, part_type, encoding_type, orientation):
        ''' Testcases for Altering aprtitions '''    
        
        tabledefinition = self.get_table_definition() 
        if encoding_type =='with':
            co_str = " WITH (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1)"
        else :
            co_str = ""
        # Add partition
        sqlfile.write("\n--Alter table Add Partition \n")
        if part_type == "range":
            if part_level == "sub_part":
                sqlfile.write("alter table " + tablename + " add partition new_p start(5050) end (6051)" + co_str +";\n\n")
                sqlfile.write("--Validation with psql utility \n  \d+ " + tablename + "_1_prt_new_p_2_prt_sp1\n\n")
                
                
                sqlfile.write("alter table " + tablename + " add default partition df_p ;\n\n")
                sqlfile.write("--Validation with psql utility \n  \d+ " + tablename + "_1_prt_df_p_2_prt_sp2\n\n")
            else:                 
                sqlfile.write("alter table " + tablename + " add partition new_p start(5050) end (5061)" + co_str +";\n\n")                
                sqlfile.write("alter table " + tablename + " add default partition df_p;\n\n")
                
        else:
            if part_level == "sub_part":
                sqlfile.write("alter table " + tablename + " add partition new_p values('C') " + co_str +";\n\n")
                sqlfile.write("--Validation with psql utility \n  \d+ " + tablename + "_1_prt_new_p_2_prt_3\n\n")
                                
                sqlfile.write("alter table " + tablename + " add default partition df_p ;\n\n")
                sqlfile.write("--Validation with psql utility \n  \d+ " + tablename + "_1_prt_df_p_2_prt_2\n\n")
            else:
                sqlfile.write("alter table " + tablename + " add partition new_p values('C')" + co_str +";\n\n")                                  
                sqlfile.write("alter table " + tablename + " add default partition df_p;\n\n")
         
        
        sqlfile.write("-- Insert data \n")
        sqlfile.write("Insert into " + tablename + "(" + self.all_columns + ") values(generate_series(1,5000),'C',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; \n\n")     
        sqlfile.write("Insert into " + tablename + "(" + self.all_columns + ") values(generate_series(5061,6050),'F',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; \n\n")     
        sqlfile.write("Insert into " + tablename + "(" + self.all_columns + ") values(generate_series(5051,6050),'M',2011,'t','a','dfjjjjjj','2001-12-24 02:26:11','hghgh',333,'2011-10-11','Tddd','sss','1234.56',323453,4454,7845,'0011','2005-07-16 01:51:15+1359','2001-12-13 01:51:15','((1,2),(0,3),(2,1))','((2,3)(4,5))','08:00:2b:01:02:03','1-2','dfdf','((2,3)(4,5))','(6,7)',11.222,'((4,5),7)',32,3214,'(1,0,2,3)','2010-02-21',43564,'$1,000.00','192.168.1','126.1.3.4','12:30:45','ggg','1','0',12,23) ; \n\n")     
        if part_type == "range":
            if part_level == "sub_part":         
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_new_p_2_prt_sp1'); \n\n")
            else:
                if encoding_type == "with" : #due to MPP-17780
                    sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_new_p'); \n\n")
        else:
            if part_level == "sub_part":
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_new_p_2_prt_3'); \n\n")
            else:
                if encoding_type == "with" : #due to MPP-17780
                    sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_new_p'); \n\n")
                
        if encoding_type == "with": # only adding for with clause . split partition limits concurrent run of tests due to duplicate oid issue
            self.split_and_exchange(tablename, sqlfile, part_level, part_type, encoding_type, orientation, tabledefinition)
            
        # Drop partition
        sqlfile.write("--Alter table Drop Partition \n")
        sqlfile.write("alter table " + tablename + " drop partition new_p;\n\n") 
        
        # Drop the default partition
        sqlfile.write("-- Drop the default partition \n")
        sqlfile.write("alter table " + tablename + " drop default partition;\n\n") 
        
        
    def generate_sql_files_part(self,sqlfile, tablename, create_table_string, block_size,compress_type, part_level, part_type, encoding_type, orientation): 
            
        tabledefinition = self.get_table_definition() 
        sqlfile.write("--\n-- Drop table if exists\n--\n")
        sqlfile.write("DROP TABLE if exists " + tablename + " cascade;\n\n")
        sqlfile.write("DROP TABLE if exists " + tablename + "_uncompr cascade;\n\n")
           
        
        sqlfile.write("--\n-- Create table\n--\n")
        sqlfile.write(create_table_string + "\n\n") 
                                    
        ### Create Indexes ###
                        
        index_name1 = tablename + "_idx_bitmap"
        index_string1 = "CREATE INDEX " + index_name1 + " ON " + tablename + " USING bitmap (a1);"
        index_name2 = tablename + "_idx_btree"
        index_string2 = "CREATE INDEX " + index_name2 + " ON " + tablename + "(a9);"
        sqlfile.write("-- \n-- Create Indexes\n--\n")
        sqlfile.write(index_string1 + "\n\n") 
        sqlfile.write(index_string2 + "\n\n")
                        
        #### Insert data ###
        sqlfile.write("--\n-- Insert data to the table\n--\n")  
        self.insert_data(tablename, sqlfile, block_size,compress_type)     
                                      
        ### Create an uncompressed table of same definition  for comparing data ###
         
        if part_type == "range":
            if part_level == "sub_part":
                uncompressed_table_string = "CREATE TABLE " + tablename + "_uncompr" + tabledefinition + ") WITH (appendonly=true, orientation=" + orientation + ") distributed randomly Partition by range(a1) Subpartition by list(a2) subpartition template ( subpartition sp1 values('M') , subpartition sp2 values('F') ) (start(1)  end(5000) every(1000)) ;"
            else:
                uncompressed_table_string = "CREATE TABLE " + tablename + "_uncompr" + tabledefinition + ") WITH (appendonly=true, orientation=" + orientation + ") distributed randomly Partition by range(a1) (start(1)  end(5000) every(1000)) ;"
        else:
            if part_level == "sub_part":
                uncompressed_table_string = "CREATE TABLE " + tablename + "_uncompr" + tabledefinition + ") WITH (appendonly=true, orientation=" + orientation + ") distributed randomly Partition by list(a2)  Subpartition by range(a1) subpartition template (start(1)  end(5000) every(1000)) (default partition p1 , partition p2 values ('M') );"
            else:
                uncompressed_table_string = "CREATE TABLE " + tablename + "_uncompr" + tabledefinition + ") WITH (appendonly=true, orientation=" + orientation + ") distributed randomly Partition by list(a2) (default partition p1 , partition p2 values ('M') );"
               
        sqlfile.write("\n--Create Uncompressed table of same schema definition" + "\n\n")
        sqlfile.write(uncompressed_table_string + "\n\n")  
                       
        #### Insert to uncompressed table ###
        sqlfile.write("--\n-- Insert to uncompressed table\n--\n")
        self.insert_data(tablename + "_uncompr", sqlfile, block_size,compress_type) 
                        
        sqlfile.write("--\n-- ********Validation******* \n--\n")
        
        #### Validation using psql utility ### 
        if part_type == "range":
            if part_level == "sub_part":
                sqlfile.write("\d+ " + tablename + "_1_prt_1_2_prt_sp2\n\n") 
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_1_2_prt_sp2'); \n\n")
            else:
                sqlfile.write("\d+ " + tablename + "_1_prt_1\n\n") 
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_1'); \n\n")
        else:
            if part_level == "sub_part":
                sqlfile.write("\d+ " + tablename + "_1_prt_p1_2_prt_2 \n\n") 
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_p1_2_prt_2'); \n\n")
            else:
                sqlfile.write("\d+ " + tablename + "_1_prt_p2\n\n")  
                sqlfile.write("--\n-- Compression ratio\n--\n select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('" + tablename + "_1_prt_p2'); \n\n") 
          
        ## Select from pg_partition_encoding to see the table entry in case of a sub partition
        if part_level == "sub_part":
            sqlfile.write ("--Select from pg_attribute_encoding to see the table entry \n") 
            sqlfile.write ("select parencattnum, parencattoptions from pg_partition_encoding e, pg_partition p, pg_class c  where c.relname = '" + tablename + "' and c.oid = p.parrelid and p.oid = e.parencoid order by parencattnum limit 3; \n")
        ###Compare the selected data with that of an uncompressed table to see if the data in two tables are same when selected.
        sqlfile.write("--\n-- Compare data with uncompressed table\n--\n")
        self.compare_data_with_uncompressed_table(tablename, sqlfile, part_table="Yes")  
        self.alter_partition_tests(tablename, sqlfile, part_level, part_type, encoding_type, orientation)
        # Alter column tests
        self.alter_column_tests(tablename, sqlfile)    
        
        
        
    def generate_sql_files(self,sqlfile, tablename, create_table_string, block_size,compress_type): 
            
        tabledefinition = self.get_table_definition() 
        sqlfile.write("--\n-- Drop table if exists\n--\n")
        sqlfile.write("DROP TABLE if exists " + tablename + " cascade;\n\n")
        sqlfile.write("DROP TABLE if exists " + tablename + "_uncompr cascade;\n\n") 
        
        sqlfile.write("--\n-- Create table\n--\n")
        sqlfile.write(create_table_string + "\n\n") 
                                    
        ### Create Indexes ###
                        
        index_name1 = tablename + "_idx_bitmap"
        index_string1 = "CREATE INDEX " + index_name1 + " ON " + tablename + " USING bitmap (a1);"
        index_name2 = tablename + "_idx_btree"
        index_string2 = "CREATE INDEX " + index_name2 + " ON " + tablename + "(a9);"
        sqlfile.write("-- \n-- Create Indexes\n--\n")
        sqlfile.write(index_string1 + "\n\n") 
        sqlfile.write(index_string2 + "\n\n")
                        
        #### Insert data ###
        sqlfile.write("--\n-- Insert data to the table\n--\n")  
        self.insert_data(tablename, sqlfile, block_size,compress_type)     
                       
        ### Create an uncompressed table of same definition  for comparing data ###
        
        if "AO" in string.upper(tablename):
            uncompressed_table_string = "CREATE TABLE " + tablename + "_uncompr" + tabledefinition + ") WITH (appendonly=true, orientation=row) distributed randomly;"
        else:
            uncompressed_table_string = "CREATE TABLE " + tablename + "_uncompr" + tabledefinition + ") WITH (appendonly=true, orientation=column) distributed randomly;"    
        sqlfile.write("\n--Create Uncompressed table of same schema definition" + "\n\n")
        sqlfile.write(uncompressed_table_string + "\n\n")  
                       
        #### Insert to uncompressed table ###
        sqlfile.write("--\n-- Insert to uncompressed table\n--\n")
        self.insert_data(tablename + "_uncompr", sqlfile, block_size,compress_type) 
                        
        sqlfile.write("--\n-- ********Validation******* \n--\n")
        self.validation_sqls(tablename, sqlfile)     
        
                        
    def create_table(self,encoding_type="storage_directive", orientation="column", reference_type="column"):
        ''' Generate Sql files for create table with different encoding types: storage_directive,WITH clause,column_reference
            All the data types are having same encoding'''
        listfilename = "column_list" 
        test_listname = ""   
        tabledefinition = self.get_table_definition() 
        column_list = self.get_column_list()
        count = 1
           
        if orientation == "row":
            tb_prefix = "ao_"
        else:
            tb_prefix = "co_" 
            
        for compress_type in self.compress_type_list:
            for block_size in self.block_size_list:
                compress_lvl_list = self.get_compresslevel_list(compress_type)
                for compress_level in compress_lvl_list:
                    create_table_string = ""
                    tablename = ""
                        
                    if encoding_type == "storage_directive" :
                        tablename = tb_prefix + "crtb_stg_dir_" + compress_type + "_" + block_size + "_" + str(compress_level)
                        column_str = ""
                        test_listname = "create_" + encoding_type 
                        list_file = open(local_path(listfilename), "r")
                        create_table_string = create_table_string + "CREATE TABLE " + tablename + " (id SERIAL," + "\n" + "\t"
                            
                        for line in list_file:
                            column_str = column_str + line.strip('\n') + " ENCODING (" + "compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")," + "\n"
                           
                        column_str = column_str[:-2]
                        create_table_string = create_table_string + " " + column_str + ") WITH (appendonly=true, orientation=column) distributed randomly;"  
                        list_file.close()
                            
                    elif encoding_type == "with":
                        if orientation == "row" and compress_type == "rle_type":
                            continue;
                        test_listname = "create_" + encoding_type + "_" + orientation 
                        tablename = tb_prefix + "crtb_with_" + orientation + "_" + compress_type + "_" + block_size + "_" + str(compress_level)
                        create_table_string = create_table_string + "CREATE TABLE " + tablename + " \n" + "\t" + tabledefinition + " )" + "\n" 
                        create_table_string = create_table_string + " WITH (appendonly=true, orientation=" + orientation + ",compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ") distributed randomly;"  
                       
                    elif encoding_type == "column_reference":
                        test_listname = "create_" + encoding_type + "_" + reference_type   
                        tablename = tb_prefix + "crtb_col_ref_" + reference_type + "_" + compress_type + "_" + block_size + "_" + str(compress_level)
                        create_table_string = "CREATE TABLE " + tablename + "\n" + "\t" + tabledefinition + " " + "\n"
                        column_str = ""
                            
                        if reference_type == "column":
                            for column_name in column_list:
                                column_str = column_str + ", COLUMN " + column_name + " ENCODING (" + "compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")" + "\n"
                            create_table_string = create_table_string + column_str + ") WITH (appendonly=true, orientation=column) distributed randomly;"                          
                            
                        else:
                            column_str = ", DEFAULT COLUMN ENCODING (" + "compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")"
                            create_table_string = create_table_string + column_str + " ) WITH (appendonly=true, orientation=column) distributed randomly;"
                                     
                    sqlfilename = tablename + ".sql" 
                    ### Generate the sql file    
                    if block_size in ('8192', '32768', '65536'):
                        sql_dir = tb_prefix + test_listname + '/small'
                    elif block_size == '1048576':
                        if compress_type == 'zlib':
                            if compress_level in (1,2,3,4,5):
                                sql_dir = tb_prefix + test_listname + '/large_1G_zlib'
                            else:
                                sql_dir = tb_prefix + test_listname + '/large_1G_zlib_2'
                        else:
                            sql_dir = tb_prefix + test_listname + '/large_1G_quick_rle'
                    else:
                        if compress_type == 'zlib':
                            if compress_level in (1,2,3,4,5):
                                sql_dir = tb_prefix + test_listname + '/large_2G_zlib'
                            else:
                                sql_dir = tb_prefix + test_listname + '/large_2G_zlib_2'
                        else:
                            sql_dir = tb_prefix + test_listname + '/large_2G_quick_rle'

                    sqlfile1 = open(local_path(sql_dir + '/' +sqlfilename), "w")
                        
                    self.generate_sql_files(sqlfile1, tablename, create_table_string, block_size,compress_type)
                         
                    # Alter column tests
                    self.alter_column_tests(tablename, sqlfile1)
                                    
                    # Drop the table
                    sqlfile1.write("--Drop table \n")
                    sqlfile1.write("DROP table " + tablename + "; \n\n")
                    
                    # Create the table
                    sqlfile1.write("--Create table again and insert data \n")
                    sqlfile1.write(create_table_string + "\n")
                    self.insert_data(tablename, sqlfile1, block_size,compress_type)
                    # Drop a column
                    sqlfile1.write("--Alter table drop a column \n")
                    sqlfile1.write("Alter table " + tablename + " Drop column a12; \n")
                    
                    #Create a CTAS table
                    create_ctas_string = "CREATE TABLE " + tablename + "_ctas  WITH (appendonly=true, orientation=column) AS Select * from " + tablename + ";\n\n"
                    sqlfile1.write("--Create CTAS table \n\n Drop table if exists " + tablename + "_ctas ;\n")
                    sqlfile1.write("--Create a CTAS table \n")
                    sqlfile1.write(create_ctas_string)
                    count += 1     
                
                            
    def create_partition_table(self,encoding_type="column_reference", orientation="column", reference_type="column", part_level="part"):
        ''' Generate sql files for create table with different encoding types: WITH clause,column_reference: 
            Each file create different types of partition tables with the corresponding compression encoding'''
        test_listname = ""   
        tabledefinition = self.get_table_definition() 
        count = 1
        if orientation == "row":
            tb_prefix = "ao_"
        else:
            tb_prefix = "co_"
        part_str1 = ""    
        part_str2 = ""             
        if part_level == "part":
            part_str1 = "Partition by range(a1) (start(1)  end(5000) every(1000)" 
            part_str2 = "Partition by list(a2) (partition p1 values ('M'), partition p2 values ('F') "
        else:
            part_str1 = " Partition by range(a1) Subpartition by list(a2) subpartition template ( default subpartition df_sp, subpartition sp1 values('M') , subpartition sp2 values('F') " 
            part_str2 = " Partition by list(a2) Subpartition by range(a1) subpartition template (default subpartition df_sp, start(1)  end(5000) every(1000)" 
                
        for compress_type in self.compress_type_list:
            for block_size in self.block_size_list:
                compress_lvl_list = self.get_compresslevel_list(compress_type)
                for compress_level in compress_lvl_list:
                    create_table_string1 = ""
                    create_table_string2 = ""
                    tablename = "" 
                    nonpart_column_str = " COLUMN a5 ENCODING (" + "compresstype=" + self.alter_comprtype[compress_type] + ",compresslevel=1, blocksize=" + block_size + ")"    
                    if encoding_type == "with":
                        if orientation == "row" and compress_type == "rle_type":
                            continue;
                        if part_level == 'sub_part' and  block_size in ("1048576", "2097152"): #out of memeory issue with large blocksize and partitions
                            continue;
                        test_listname = "create_with_" + orientation + "_" + part_level
                        
                        tablename = tb_prefix + "wt_" + part_level + compress_type + block_size + "_" + str(compress_level)
                        create_table_string1 = "CREATE TABLE " + tablename + " \n" + "\t" + tabledefinition + " )" + "\n" 
                        create_table_string2 = "CREATE TABLE " + tablename + "_2 \n" + "\t" + tabledefinition + " )" + "\n" 
                        if part_level == "part":
                            create_table_string1 = create_table_string1 + " WITH (appendonly=true, orientation=" + orientation + ") distributed randomly " + part_str1 + " \n WITH (appendonly=true, orientation=" + orientation + ",compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + "));"
                            create_table_string2 = create_table_string2 + " WITH (appendonly=true, orientation=" + orientation + ") distributed randomly " + part_str2 + " \n WITH (appendonly=true, orientation=" + orientation + ",compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + "));"
                        else:
                            create_table_string1 = create_table_string1 + " WITH (appendonly=true, orientation=" + orientation + ") distributed randomly " + part_str1 + " \n WITH (appendonly=true, orientation=" + orientation + ",compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")) (start(1)  end(5000) every(1000) );"
                            create_table_string2 = create_table_string2 + " WITH (appendonly=true, orientation=" + orientation + ") distributed randomly " + part_str2 + " \n WITH (appendonly=true, orientation=" + orientation + ",compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")) (partition p1 values ('M'), partition p2 values ('F'));"
                        
                    elif encoding_type == "column_reference":
                        if part_level == 'sub_part' and  block_size in ("1048576", "2097152"): #out of memeory issue with large blocksize and partitions
                            continue;
                        test_listname = "create_" + encoding_type + "_" + reference_type + "_" + part_level   
                        tablename = tb_prefix + "cr_" + part_level + compress_type + block_size + "_" + str(compress_level)
                        create_table_string1 = "CREATE TABLE " + tablename + " \n" + "\t" + tabledefinition + " )  WITH (appendonly=true, orientation=column) distributed randomly \n"
                        create_table_string2 = "CREATE TABLE " + tablename + "_2 \n" + "\t" + tabledefinition + " )  WITH (appendonly=true, orientation=column) distributed randomly \n"
                        column_str = " ENCODING (" + "compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")" 
                        default_str = " DEFAULT COLUMN ENCODING (" + "compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")"
                        if part_level == "part":
                            create_table_string1 = create_table_string1 + part_str1 + " ,\n COLUMN  a1" + column_str + ",\n" + nonpart_column_str + ",\n" + default_str + ");"
                            create_table_string2 = create_table_string2 + part_str2 + " ,\n COLUMN  a2" + column_str + ",\n" + nonpart_column_str + ", \n" + default_str + ");"
                        else:
                             
                            create_table_string1 = create_table_string1 + part_str1 + " , \n COLUMN a2 " + column_str + ", \n COLUMN a1 encoding (compresstype = " + self.alter_comprtype[compress_type] + "),\n" + nonpart_column_str + ",\n" + default_str + ") (start(1) end(5000) every(1000));"
                            create_table_string2 = create_table_string2 + part_str2 + ", \n COLUMN a2 " + column_str + ", \n COLUMN a1 encoding (compresstype = " + self.alter_comprtype[compress_type] + "),\n" + nonpart_column_str + ",\n" + default_str + ") (partition p1 values('F'), partition p2 values ('M'));"
                                                 
                    sqlfilename = tablename + ".sql" 
                    ### Generate the sql file    
                    if block_size in ('8192', '32768', '65536'):
                        sql_dir = tb_prefix + test_listname + '/small'
                    elif block_size == '1048576':
                        if compress_type == 'zlib':
                            if compress_level in (1,2,3,4,5):
                                sql_dir = tb_prefix + test_listname + '/large_1G_zlib'
                            else:
                                sql_dir = tb_prefix + test_listname + '/large_1G_zlib_2'
                        else:
                            sql_dir = tb_prefix + test_listname + '/large_1G_quick_rle'
                    else:
                        if compress_type == 'zlib':
                            if compress_level in (1,2,3,4,5):
                                sql_dir = tb_prefix + test_listname + '/large_2G_zlib'
                            else:
                                sql_dir = tb_prefix + test_listname + '/large_2G_zlib_2'
                        else:
                            sql_dir = tb_prefix + test_listname + '/large_2G_quick_rle'

                    sqlfile6 = open(local_path(sql_dir + '/' +sqlfilename), "w")
                        
                    self.generate_sql_files_part(sqlfile6, tablename, create_table_string1, block_size,compress_type, part_level, "range", encoding_type, orientation)
                    self.generate_sql_files_part(sqlfile6, tablename + "_2", create_table_string2, block_size,compress_type, part_level, "list", encoding_type, orientation)
                    count += 1     
         
            
            
    def create_table_columns_with_different_encoding_type(self,storage_directive="no", column_reference="no"):
        ''' Generate sql files for create table with columns having different storage_directive encoding pr column level'''
        listfilename = "column_list"
        tabledefinition = self.get_table_definition()
        column_list = self.get_column_list()
        test_listname = ""
        count = 1
        
        for compress_lvl in self.compress_level_list:
            tablename = ""
            create_table_string = ""
            column_ref_str = ""
            storage_dir_str = ""
            list_file = open(local_path(listfilename), "r")
            clm_list = list_file.readlines()
            for i in range(len(clm_list)):
                if(i % 4 == 1):
                    compress_type = self.compress_type_list[0]
                    compress_level = 1
                    block_size = self.block_size_list[1]
                elif(i % 4 == 2):
                    compress_type = self.compress_type_list[1]
                    if (compress_lvl < 5):
                        compress_level = compress_lvl
                    else:    
                        compress_level = 1
                    block_size = self.block_size_list[2]
                else:
                    compress_type = self.compress_type_list[2]
                    compress_level = compress_lvl
                    block_size = self.block_size_list[3]
    
                if column_reference == "yes" and (i % 3 <> 3):
                        column_ref_str = column_ref_str + ", COLUMN " + column_list[i] + " ENCODING (" + "compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")" + "\n"
                if storage_directive == "yes" :
                    if (i % 3 <> 3):
                        storage_dir_str = storage_dir_str + clm_list[i].strip('\n') + " ENCODING (" + "compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ")," + "\n"
                    else:
                        storage_dir_str = storage_dir_str + clm_list[i].strip('\n') + ", \n"
    
            storage_dir_str = storage_dir_str[:-2]
            column_ref_str = column_ref_str[:-1]
            list_file.close()
            if  storage_directive == "yes" and column_reference == "yes" :
                test_listname = "create_col_with_storage_directive_and_col_ref"
                tablename = "co_" + "crtb_with_strg_dir_and_col_ref_" + str(compress_lvl)
                create_table_string = "CREATE TABLE " + tablename + " ( \n id SERIAL," + storage_dir_str + column_ref_str + ",DEFAULT COLUMN ENCODING (compresstype=quicklz,blocksize=8192)) WITH (appendonly=true, orientation=column) distributed randomly;"
            elif column_reference == "yes":
                tablename = "co_" + "crtb_col_with_diff_col_ref_" + str(compress_lvl)
                test_listname = "create_col_with_diff_column_reference"
                create_table_string = "CREATE TABLE " + tablename + "\n" + "\t" + tabledefinition + " " + "\n" + column_ref_str + ") WITH (appendonly=true, orientation=column) distributed randomly;"
            elif  storage_directive == "yes":
                tablename = "co_" + "crtb_col_with_diff_strg_dir_" + str(compress_lvl)
                test_listname = "create_col_with_diff_storage_directive"
                create_table_string = "CREATE TABLE " + tablename + " ( \n id SERIAL," + storage_dir_str + ") WITH (appendonly=true, orientation=column) distributed randomly;"
            sqlfilename = tablename + ".sql"

            sql_dir = test_listname
            sqlfile3 = open(local_path(sql_dir + '/' +sqlfilename), "w")
                    
            self.generate_sql_files(sqlfile3, tablename, create_table_string, block_size,compress_type)
                
            # Alter column tests
            self.alter_column_tests(tablename, sqlfile3)
                
            count += 1     
       
       
    def alter_table(self):
        ''' Alter table add column with encoding and alter table alter column encoding'''
        listfilename = "column_list_with_default"  
        tabledefinition = self.get_table_definition()
        count = 1
        create_table_string = ""
        for block_size in ("8192", "32768", "65536"): 
            for compress_type in self.compress_type_list:
                compress_lvl_list = self.get_compresslevel_list(compress_type)
                for compress_lvl in compress_lvl_list:
                    tablename = "co_" + "alter_table_add_" + compress_type + "_" + block_size + "_" + str(compress_lvl)
                    create_table_string = "CREATE TABLE " + tablename + " ( \n id SERIAL, DEFAULT COLUMN ENCODING (compresstype=" + self.alter_comprtype[compress_type] + ",blocksize=8192,compresslevel=1)) WITH (appendonly=true, orientation=column) distributed randomly ;"
                    sqlfilename = tablename + ".sql"   
                    
                    sql_dir = 'co_alter_table_add'
                    sqlfile4 = open(local_path(sql_dir + '/' +sqlfilename), "w")
                    
                    sqlfile4.write("--\n  -- Drop table if exists\n --\n")
                    sqlfile4.write("DROP TABLE if exists " + tablename + ";\n\n")
                    sqlfile4.write("DROP TABLE if exists " + tablename + "_uncompr; \n\n")
                    sqlfile4.write("--\n  -- Create table\n --\n")
                    sqlfile4.write(create_table_string + "\n")
                    
                    list_file = open(local_path(listfilename), "r")
                    clm_list = list_file.readlines()
                    for i in range(len(clm_list)):
                        alter_str = "Alter table " + tablename + " ADD COLUMN " + clm_list[i].strip('\n') + " ENCODING (" + "compresstype=" + compress_type + ",compresslevel=" + str(compress_lvl) + ",blocksize=" + block_size + ");" + "\n"
                        sqlfile4.write(alter_str + "\n")  
                        
                    #### Insert data ###
                    sqlfile4.write("--\n  -- Insert data to the table\n --\n")  
                    self.insert_data(tablename, sqlfile4, block_size,compress_type)    
                      
                    ### Alter table SET distributed by 
                    sqlfile4.write("--\n--Alter table set distributed by \n") 
                    sqlfile4.write("ALTER table " + tablename + " set with ( reorganize='true') distributed by (a1);\n")   
                      
                    ### Create an uncompressed table of same definition  for comparing data ###
                    sqlfile4.write("-- Create Uncompressed table of same schema definition" + "\n")
                    
                    uncompressed_table_string = "CREATE TABLE " + tablename + "_uncompr " + tabledefinition + ")" + " WITH (appendonly=true, orientation=column) distributed randomly;"
                    sqlfile4.write(uncompressed_table_string + "\n\n")  
                       
                    #### Insert to uncompressed table ###
                    sqlfile4.write("--\n-- Insert to uncompressed table\n --\n")
                    self.insert_data(tablename + "_uncompr", sqlfile4, block_size,compress_type) 
                           
                    ###Validation
                    self.validation_sqls(tablename, sqlfile4) 
                    
                    # Alter column tests
                    self.alter_column_tests(tablename, sqlfile4)
                    
                    count += 1     
      
            
    def alter_type(self):
        '''Alter the different data types with encoding: Create a table using these types '''                  
              
        listfilename = "datatype_list"  
        tabledefinition = self.get_table_definition()
        test_listname = "alter_type" 
        count = 1
        create_table_string = ""
        for compress_type in self.compress_type_list:
            for block_size in ("8192", "32768", "65536"):
                compress_lvl_list = self.get_compresslevel_list(compress_type)
                for compress_level in compress_lvl_list:
                    tablename = "co_" + "alter_type_" + compress_type + "_" + block_size + "_" + str(compress_level)
                    list_file = open(local_path(listfilename), "r")
                    sqlfilename = tablename + ".sql"   

                    sql_dir = 'co_alter_type'
                    sqlfile5 = open(local_path(sql_dir + '/' +sqlfilename), "w")

                    for data_type in list_file:
                        data_type = data_type.strip('\n')
                        alter_str = "ALTER TYPE " + data_type + " SET DEFAULT ENCODING (compresstype=" + compress_type + ",compresslevel=" + str(compress_level) + ",blocksize=" + block_size + ");" + "\n"
                        sqlfile5.write(alter_str + "\n")  
                        sqlfile5.write("select typoptions from pg_type_encoding where typid='" + data_type + " '::regtype;\n\n")
                        
                        
                    create_table_string = "CREATE TABLE " + tablename + "\n" + "\t" + tabledefinition + ") WITH (appendonly=true, orientation=column) distributed randomly;" + "\n"
                        
                    ### Create table
                    sqlfile5.write("--\n  -- Drop table if exists\n --\n")
                    sqlfile5.write("DROP TABLE if exists " + tablename + ";\n\n")
                    sqlfile5.write("DROP TABLE if exists " + tablename + "_uncompr; \n\n")
                    sqlfile5.write("-- Create table " + "\n")
                    sqlfile5.write(create_table_string + "\n\n")     
                    #### Insert data ###
                    sqlfile5.write("--\n  -- Insert data to the table\n --\n")  
                    self.insert_data(tablename, sqlfile5, block_size,compress_type)    
                       
                       
                    ### Create an uncompressed table of same definition  for comparing data ###
                    sqlfile5.write("-- Create Uncompressed table of same schema definition" + "\n")
                    sqlfile5.write("-- First Alter the data types back to compression=None" + "\n")
                    list_file2 = open(local_path(listfilename), "r")
                    for data_type in list_file2:
                        alter2_str = "ALTER TYPE " + data_type.strip('\n') + " SET DEFAULT ENCODING (compresstype=none,compresslevel=0);" + "\n"
                        sqlfile5.write(alter2_str + "\n")  
                        sqlfile5.write("select typoptions from pg_type_encoding where typid='" + data_type + " '::regtype;\n\n")
                    
                    uncompressed_table_string = "CREATE TABLE " + tablename + "_uncompr " + tabledefinition + ")" + " WITH (appendonly=true, orientation=column) distributed randomly;"
                    sqlfile5.write(uncompressed_table_string + "\n\n")  
                       
                    #### Insert to uncompressed table ###
                    sqlfile5.write("--\n-- Insert to uncompressed table\n --\n")
                    self.insert_data(tablename + "_uncompr", sqlfile5, block_size,compress_type) 
                           
                    ###Validation
                    self.validation_sqls(tablename, sqlfile5) 
                    #Alter type drop column
                    sqlfile5.write("--Alter table drop a column \n")
                    sqlfile5.write("Alter table " + tablename + " Drop column a12; \n")
                    #Alter type add column with encoding
                    sqlfile5.write("--Alter table add a column \n")
                    sqlfile5.write("Alter table " + tablename + " Add column a12 text default 'default value' encoding (compresstype=" + self.alter_comprtype[compress_type] + ",compresslevel=1,blocksize=32768); \n")
                    count += 1     
                   
    
    
    def create_type(self):
        '''Create different data types with encoding: Create a table using these types '''                  
              
        datatype_list = ["int", "char", "text", "varchar", "date", "timestamp"]  
        type_func_in = {"int":"int4in", "char":"charin", "text":"textin", "date":"date_in", "varchar":"varcharin", "timestamp":"timestamp_in"}
        type_func_out = {"int":"int4out", "char":"charout", "text":"textout", "date":"date_out", "varchar":"varcharout", "timestamp":"timestamp_out"}
        type_default = {"int":55, "char":" 'asd' ", "text":" 'hfkdshfkjsdhflkshadfkhsadflkh' ", "date":" '2001-12-11' ", "varchar":" 'ajhgdjagdjasdkjashk' ", "timestamp":" '2001-12-24 02:26:11' "}
        
        
        test_listname = "create_type" 
        count = 1
        create_table_string = ""
        for blocksize in self.block_size_list:
            for compress_type in self.compress_type_list:
                compress_lvl_list = self.get_compresslevel_list(compress_type)
                for compress_level in compress_lvl_list:
                    tablename = "co_" + "create_type_" + compress_type + "_" + blocksize + "_" + str(compress_level)
                    sqlfilename = tablename + ".sql"   

                    sql_dir = 'co_create_type'
                    sqlfile5 = open(local_path(sql_dir + '/' +sqlfilename), "w")

                    for dtype in datatype_list:
                        data_type = dtype + "_" + compress_type
                        sqlfile5.write("DROP type if exists " + data_type + " cascade ; \n\n")
                        shell_type_str = "CREATE type " + data_type + ";"
                        sqlfile5.write(shell_type_str + "\n")
                    
                        function_in_str = "CREATE FUNCTION " + data_type + "_in(cstring) \n RETURNS " + data_type + "\n AS '" + type_func_in[dtype] + "' \n LANGUAGE internal IMMUTABLE STRICT; \n"
                        function_out_str = "CREATE FUNCTION " + data_type + "_out(" + data_type + ") \n RETURNS cstring" "\n AS '" + type_func_out[dtype] + "' \n LANGUAGE internal IMMUTABLE STRICT; \n"
                        sqlfile5.write(function_in_str + "\n")
                        sqlfile5.write(function_out_str + "\n")
                    
                        compress_str = " compresstype=" + compress_type + ",\n blocksize=" + blocksize + ",\n compresslevel=" + str(compress_level) 
                        if dtype == "varchar" or dtype == "text":
                            create_type_str = "CREATE TYPE " + data_type + "( \n input = " + data_type + "_in ,\n output = " + data_type + "_out ,\n internallength = variable, \n default =" + str(type_default[dtype]) + ", \n"
                        else:
                            create_type_str = "CREATE TYPE " + data_type + "( \n input = " + data_type + "_in ,\n output = " + data_type + "_out ,\n internallength = 4, \n default =" + str(type_default[dtype]) + ", \n passedbyvalue, \n"
                        create_type_str = create_type_str + compress_str + ");\n"
                        sqlfile5.write(create_type_str + "\n")  
                    
                        #Drop and recreate the type
                        sqlfile5.write("--Drop and recreate the data type \n\n ")
                        sqlfile5.write("Drop type if exists " + data_type + " cascade;\n\n")
                        sqlfile5.write(function_in_str + "\n\n")
                        sqlfile5.write(function_out_str + "\n\n")
                        sqlfile5.write(create_type_str + "\n\n")    
                        sqlfile5.write("select typoptions from pg_type_encoding where typid='" + data_type + " '::regtype;\n\n")  
                        
                    create_table_string = "CREATE TABLE " + tablename + "\n" + "\t (id serial,  a1 int_" + compress_type + ", a2 char_" + compress_type + ", a3 text_" + compress_type + ", a4 date_" + compress_type + ", a5 varchar_" + compress_type + ", a6 timestamp_" + compress_type + " ) WITH (appendonly=true, orientation=column) distributed randomly;" + "\n"
                        
                    ### Create table
                    sqlfile5.write("DROP table if exists " + tablename + "; \n")
                    sqlfile5.write("-- Create table " + "\n")
                    sqlfile5.write(create_table_string + "\n\n")     
            
                    sqlfile5.write("\d+ " + tablename + "\n\n")
                    #### Insert data ###
                    sqlfile5.write("INSERT into " + tablename + " DEFAULT VALUES ; \n")
                    sqlfile5.write("Select * from " + tablename + ";\n\n")
                    sqlfile5.write("Insert into " + tablename + " select * from " + tablename + "; \n" + "Insert into " + tablename + " select * from " + tablename + "; \n")
                    sqlfile5.write("Insert into " + tablename + " select * from " + tablename + "; \n" + "Insert into " + tablename + " select * from " + tablename + "; \n")
                    sqlfile5.write("Insert into " + tablename + " select * from " + tablename + "; \n" + "Insert into " + tablename + " select * from " + tablename + "; \n\n")
                    sqlfile5.write("Select * from " + tablename + ";\n\n")
             
                    
                    #Drop  a column
                    sqlfile5.write("--Alter table drop a column \n")
                    sqlfile5.write("Alter table " + tablename + " Drop column a2; \n")
    
                    sqlfile5.write("Insert into " + tablename + "(a1,a3,a4,a5,a6)  select a1,a3,a4,a5,a6 from "+ tablename + " ;\n")
                    sqlfile5.write("Select count(*) from " + tablename + "; \n\n") 
                    
                    #Rename a column
                    sqlfile5.write("--Alter table rename a column \n")
                    sqlfile5.write("Alter table " + tablename + " Rename column a3 TO after_rename_a3; \n")           
                    
                    sqlfile5.write("--Insert data to the table, select count(*)\n")     
                    sqlfile5.write("Insert into " + tablename + "(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from "+ tablename + " ;\n")
                    sqlfile5.write("Select count(*) from " + tablename + "; \n\n")  
            
                    #Alter type to new encoding
                    sqlfile5.write("Alter type int_" + compress_type + " set default encoding (compresstype="+ self.alter_comprtype[compress_type] +",compresslevel=1);\n\n")
                    sqlfile5.write("--Add a column \n  Alter table "+ tablename + " Add column new_cl int_"+ compress_type + " default '5'; \n\n")
                    sqlfile5.write("\d+ " + tablename +"\n\n")
                    sqlfile5.write("Insert into " + tablename + "(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from "+ tablename + " ;\n")
                    sqlfile5.write("Select count(*) from " + tablename + "; \n\n")  
            
                    count += 1     
        
        
    def generate_sqls(self):
        self.create_table("storage_directive", "column", "column")
        self.create_table("column_reference", "column", "default")
        self.create_table("column_reference", "column", "column")
        self.create_table("with", "row", "column")
        self.create_table("with", "column", "column")
        self.create_partition_table("with", "row", "column", "part")
        self.create_partition_table("with", "row", "column", "sub_part")
        self.create_partition_table("with", "column", "column", "part")
        self.create_partition_table("with", "column", "column", "sub_part")
        self.create_partition_table("column_reference", "column", "column", "part")
        self.create_partition_table("column_reference", "column", "column", "sub_part")
        self.create_table_columns_with_different_encoding_type("no", "yes")
        self.create_table_columns_with_different_encoding_type("yes", "no")
        self.create_table_columns_with_different_encoding_type("yes","yes")
        self.alter_table()
        self.alter_type()
        self.create_type() 
        tinctest.logger.info('Done with generating the sqls... Lets create the copy files for inserting data ...')
        self.generate_copy_files()
