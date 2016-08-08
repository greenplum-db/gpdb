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

''' Creates a query having 50+ Joins '''
class JoinQuery():

    # Total no. of tables involved in Join Query
    no_of_tables = 10
    # Only 10 columns for each table are used currently, if more columns needed add over here
    column_mappings = {1:'c1',2:'c2',3:'c3',4:'c4',5:'c5',6:'c6',7:'c7',8:'c8',9:'c9',10:'c10'}
    # For indentation purpose, 4 white space characters are used per tab
    spaces_per_tab = 4

    def local_path(self, filename):
        '''Return the absolute path of the input file.:Overriding it here to use the absolute path instead of relative'''
        frame = inspect.stack()[1]
        source_file = inspect.getsourcefile(frame[0])
        source_dir = os.path.dirname(os.path.abspath(source_file))
        return os.path.join(source_dir, filename)

    def get_spaces_for_level(self,level):
        ''' Returns the no. of spaces needed for proper indentation based on the level of the query '''
        spaces = ""
        if level == 0:
            return spaces
        no_of_tabs = level*2 - 1
        no_of_spaces = no_of_tabs * self.spaces_per_tab
        counter = 0
        while counter < no_of_spaces:
            spaces += " "
            counter += 1
        return spaces
        
    def get_two_distinct_tables(self,tables):
        ''' Returns two distinct tables that will be further used in the Joins '''
        table_no_1 = random.randint(1,tables)
        table_no_2 = random.randint(1,(tables-1))
        if table_no_1 == table_no_2:
            table_no_2 += 1
        table1 = "T" + str(table_no_1)
        table2 = "T" + str(table_no_2) 
        return (table1,table2)
        
    
    def do_join(self,table1,is_table1_view,table2,is_table2_view,level,join_type = None):
        ''' Returns the complete Join query in the string format after performing the given join_type '''
        no_of_columns = len(self.column_mappings)
        initial_spaces = self.get_spaces_for_level(level)
        view1_stmt1 = view1_stmt2 = table1
        view2_stmt1 = view2_stmt2 = table2
        view3 =  ", "
        
        if is_table1_view:
            view1_stmt1 = "J1"
            view1_stmt2 = "(\n" +  table1 +  initial_spaces + self.get_spaces_for_level(1) + ") " + view1_stmt1 + "(c1,c2,c3,c4,c5,c6,c7,c8,c9,c10)\n"
        if is_table2_view:
            view2_stmt1 = "J2"
            view2_stmt2 = "(\n" +  table2 +  initial_spaces + self.get_spaces_for_level(1) + ") " + view2_stmt1 + "(c1,c2,c3,c4,c5,c6,c7,c8,c9,c10)"
            view3 = initial_spaces + self.get_spaces_for_level(1) + "," 
        
        stmt1 = initial_spaces + "SELECT " + view1_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + "," + view1_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + "," + view1_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + ","  + view1_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + "," + view1_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + "," + view2_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + ","  + view2_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + "," + view2_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + ","  + view2_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + ","  + view2_stmt1 + "." + self.column_mappings[random.randint(1,no_of_columns)] + "\n"
        
        stmt2 = initial_spaces + "FROM " + view1_stmt2
        
        if join_type is not None :
            stmt2 = stmt2 + initial_spaces + join_type + " " + view2_stmt2 + "\n"
            where_clause = "ON "
        else:
            stmt2 = stmt2 +  view3 + view2_stmt2 + "\n"
            where_clause = "WHERE "
            
        
        stmt3 = initial_spaces + where_clause + view1_stmt1 + "." + self.column_mappings[random.randint(1,10)] + " = " + view2_stmt1 + "." + self.column_mappings[random.randint(1,10)] + "\n"
        
        stmt4 = initial_spaces + "ORDER BY " + view1_stmt1 + "." + self.column_mappings[random.randint(1,10)] +"\n"
        
        final_query = stmt1 + stmt2 + stmt3 + stmt4
        
        return final_query
        
    def do_left_outer_join(self,table1,is_table1_view,table2,is_table2_view,level):
        ''' Returns LEFT OUTER JOIN query in string format '''
        return self.do_join(table1,is_table1_view,table2,is_table2_view,level,"LEFT OUTER JOIN")
       
        
    def do_right_outer_join(self,table1,is_table1_view,table2,is_table2_view,level):
        ''' Returns RIGHT OUTER JOIN query in string format '''
        return self.do_join(table1,is_table1_view,table2,is_table2_view,level,"RIGHT OUTER JOIN")
        
    def do_complex_join(self):   
        ''' Returns a complex JOIN query containing Inner, Left Outer and Right Outer joins '''
        stmt1 = "SELECT"
        req_columns = ['a','b','c','d','e']
        no_of_columns = len(req_columns)
        initial_spaces = self.get_spaces_for_level(1)
        stmt2 = ""
        for columns in self.column_mappings.keys():
            column = req_columns[random.randint(0,(no_of_columns-1))]
            
            (table1,table2) = self.get_two_distinct_tables(self.no_of_tables)
            (table3,table4) = self.get_two_distinct_tables(self.no_of_tables)
            (table5,table6) = self.get_two_distinct_tables(self.no_of_tables)
            
            inner_stmt1 = self.do_join(table1,False,table2,False,4)
            inner_stmt2 = self.do_join(table3,False,table4,False,4)
            
            inner_stmt3 = self.do_left_outer_join(inner_stmt1,True,table5,False,3)
            inner_stmt4 = self.do_right_outer_join(inner_stmt2,True,table6,False,3)
            
            inner_stmt5 = self.do_join(inner_stmt3,True,inner_stmt4,True,2)
        
            inner_stmt6 = "\n"+initial_spaces +"(\n"+ initial_spaces+ "SELECT " + column + "\n" + initial_spaces + "FROM\n" + initial_spaces + self.get_spaces_for_level(1) + "(\n" + inner_stmt5 + initial_spaces + self.get_spaces_for_level(1) + ")\n" + initial_spaces + "AS v1(a,b,c,d,e)\n"  + initial_spaces + "GROUP BY " + column + "\n" + initial_spaces + "Limit 1\n" + initial_spaces + ")c" +str(columns) + ","
            
            stmt2 += inner_stmt6
         
        stmt2 = stmt2[:-1]
        stmt3 = "\nFROM T8,T9\nWHERE T8.c1 = T9.c2;"
        
        final_stmt = stmt1 + stmt2 + stmt3
        
        return final_stmt
        
    def write_query_to_file(self, filename="join_query.sql", concurrency = None, iterations = None):
        ''' Writes the Join query with added concurrency and iteration values to a file '''
        filename = self.local_path(filename)
        file = open(filename,"w")

        dot_index = filename.rfind('.')
        ans_filename = filename[:dot_index+1] + "ans"
        ans_file = open(ans_filename,"w")
        
        if iterations is None:
            iterations = random.randint(10, 20)
        if concurrency is None:
            concurrency = random.randint(30,40)
        file.write("-- @concurrency %s \n" % concurrency)
        file.write("-- @iterations %s \n" % iterations)  
        
        query = self.do_complex_join()
        file.write(query)

if __name__ == "__main__":
    newJoinQuery = JoinQuery()
    newJoinQuery.write_query_to_file(concurrency=10,iterations=10)

        
            
        
    
