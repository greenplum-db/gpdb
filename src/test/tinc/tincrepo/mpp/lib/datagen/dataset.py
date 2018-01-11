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
from gppylib.commands.base import Command, CommandResult, REMOTE

from mpp.lib.datagen import TINCTestDatabase

from mpp.lib.PSQL import PSQL, PSQLException

class DataSetDatabase(TINCTestDatabase):
    def __init__(self, database_name = "dataset_0.01_heap", storage_type = 'heap', partition = False):
        self.db_name = database_name
        self.storage_type = storage_type
        self.partition = partition
        super(DataSetDatabase, self).__init__(database_name)

    def setUp(self):
        if super(DataSetDatabase, self).setUp():
            return True
        #call reload_dataset
        if self.storage_type == 'heap':
            self.reload_dataset()
        elif self.storage_type == 'ao':
            self.reload_dataset_AO()
        elif self.storage_type == 'co':
            self.reload_dataset_CO()
        else:
            self.reload_dataset()

        return False

    def _form_dataset_location_string(self, table_prefix):
        try:
            num_hosts = (PSQL.run_sql_command("select count(distinct hostname) from gp_segment_configuration;", flags='')).split('\n')[2].strip()
            hostname = (PSQL.run_sql_command("select distinct hostname from gp_segment_configuration where content=0 and role='p' limit 1", flags='')).split('\n')[2].strip()
            if '(0 rows)' in hostname:
                raise Exception
        except:
            raise Exception('Could not query hostname of master')

        dataset_dir = os.path.dirname(os.path.abspath(__file__)) + '/datasets'
        dataset_location = ''' location ('file://%s/%s/%s.csv') format 'csv' (delimiter '|')''' % ( hostname, dataset_dir, table_prefix )
        if num_hosts > 1:
            mkdir_command = "ssh %s 'mkdir -p %s'" %(hostname, dataset_dir)
            os.system(mkdir_command)
            scp_command = "scp %s/%s.csv %s:%s/%s.csv" %(dataset_dir, table_prefix, hostname, dataset_dir, table_prefix)
            os.system(scp_command)

        return dataset_location

    def reload_table(self, pTableName, pTableDefinition, withClause = "", preExecuteCmd = "",
                     createExternalTable = True, pTablePartitionDefinition = ''):
        '''
        Drop a table; re-create it; and re-load the data into it.
        @param pTableName: The name of the table.
            pTableDefinition: The list of column names and data types,
                enclosed in parentheses, for example:
                    "(x int, y float)"
        @param withClause: This is an optional string that may contain a "WITH"
                clause such as "WITH (APPENDONLY=True, ORIENTATION='column')".
                It could also contain other clauses, like "DISTRIBUTED BY...".
        @param preExecuteCmd: This is an optional string that will be executed
                prior to the CREATE TABLE statement.  For example, this string
                might contain "SET SEARCH_PATH TO AO_SCHEMA;" to set the
                search path to include the schema named "AO_SCHEMA".
        @param createExternalTable: set this to False if you have already created
                the external table and don't want to create it again.
        '''

        ok = True

        if withClause == None:
            withClause = ""

        if preExecuteCmd == None:
            preExecuteCmd = ""
        if len(preExecuteCmd) > 0 and preExecuteCmd[-1].strip() != ";":
            preExecuteCmd = preExecuteCmd + ";"

        res = {'rc':0, 'stderr':'', 'stdout':''}

        pDataSetLocation = self._form_dataset_location_string(pTableName.lower())
        if createExternalTable:
            cmd = 'drop external table if exists e_' + pTableName
            out = PSQL.run_sql_command(cmd , dbname = self.db_name)
            cmd = "create external table e_" + pTableName + \
                      pTableDefinition + " " + pDataSetLocation
            cmd = "".join( cmd.split( '\n' ) )
            out = PSQL.run_sql_command(cmd , dbname = self.db_name, results=res)
            if res['rc']:
                raise PSQLException(res['stderr'])

        cmd = 'drop table if exists ' + pTableName + ' cascade'
        out = PSQL.run_sql_command(preExecuteCmd + cmd , dbname = self.db_name, results=res)
        if res['rc']:
            raise PSQLException(res['stderr'])

        cmd = 'CREATE TABLE ' + pTableName + pTableDefinition
        cmd = "".join(cmd.split( '\n' ))
        cmd = cmd + withClause + ' ' + pTablePartitionDefinition
        out = PSQL.run_sql_command(preExecuteCmd + cmd, dbname = self.db_name, results=res)
        if res['rc']:
            raise PSQLException(res['stderr'])

        cmd = 'insert into ' + pTableName + ' select * from e_' + pTableName
        out = PSQL.run_sql_command(preExecuteCmd + cmd, dbname = self.db_name, results=res)
        if res['rc']:
            raise PSQLException(res['stderr'])

        cmd = 'analyse ' + pTableName
        out = PSQL.run_sql_command(preExecuteCmd + cmd, dbname = self.db_name, results=res)
        if res['rc']:
            raise PSQLException(res['stderr'])

    def reload_Nation(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads Nation Table
        '''

        tableName = "nation"
        tableDefinition = '''(N_NATIONKEY  INTEGER ,
                           N_NAME       CHAR(25) ,
                           N_REGIONKEY  INTEGER ,
                           N_COMMENT    VARCHAR(152))'''

        self.reload_table( tableName, tableDefinition, withClause,
                           preExecuteCmd, createExternalTable )


    def reload_Region(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads Region Table
        '''

        tableName = "region"
        tableDefinition = '''( R_REGIONKEY  INTEGER ,
                                R_NAME       CHAR(25) ,
                                R_COMMENT    VARCHAR(152))'''

        self.reload_table( tableName, tableDefinition, withClause,
                           preExecuteCmd, createExternalTable )

    def reload_Part(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads Part Table
        '''

        tableName = "Part"
        tableDefinition = '''(P_PARTKEY     INTEGER ,
                              P_NAME        VARCHAR(55) ,
                              P_MFGR        CHAR(25) ,
                              P_BRAND       CHAR(10) ,
                              P_TYPE        VARCHAR(25) ,
                              P_SIZE        INTEGER ,
                              P_CONTAINER   CHAR(10) ,
                              P_RETAILPRICE DECIMAL(15,2) ,
                              P_COMMENT     VARCHAR(23) )'''

        self.reload_table(tableName, tableDefinition, withClause,
                          preExecuteCmd, createExternalTable)


    def reload_Supplier(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads Supplier Table
        '''

        tableName = "Supplier"

        tableDefinition = '''(S_SUPPKEY     INTEGER ,
                                 S_NAME        CHAR(25) ,
                                 S_ADDRESS     VARCHAR(40) ,
                                 S_NATIONKEY   INTEGER ,
                                 S_PHONE       CHAR(15) ,
                                 S_ACCTBAL     DECIMAL(15,2) ,
                                 S_COMMENT     VARCHAR(101) )'''

        self.reload_table(tableName, tableDefinition, withClause,
                          preExecuteCmd, createExternalTable)


    def reload_Partsupp(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads Part Supply Table
        '''

        tableName = "Partsupp"
        tableDefinition = '''(   PS_PARTKEY     INTEGER ,
                                 PS_SUPPKEY     INTEGER ,
                                 PS_AVAILQTY    INTEGER ,
                                 PS_SUPPLYCOST  DECIMAL(15,2)  ,
                                 PS_COMMENT     VARCHAR(199) )'''

        self.reload_table(tableName, tableDefinition, withClause,
                          preExecuteCmd, createExternalTable)


    def reload_Customer(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads Customer Table
        '''

        tableName = "Customer"
        tableDefinition = ''' (  C_CUSTKEY     INTEGER ,
                                 C_NAME        VARCHAR(25) ,
                                 C_ADDRESS     VARCHAR(40) ,
                                 C_NATIONKEY   INTEGER ,
                                 C_PHONE       CHAR(15) ,
                                 C_ACCTBAL     DECIMAL(15,2) ,
                                 C_MKTSEGMENT  CHAR(10) ,
                                 C_COMMENT     VARCHAR(117) )'''

        self.reload_table(tableName, tableDefinition, withClause,
                          preExecuteCmd, createExternalTable)


    def reload_Orders(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads Orders Table
        '''
        pTablePartitionDefinition = ''
        tableName = "Orders"
        tableDefinition = '''( O_ORDERKEY       INTEGER ,
                               O_CUSTKEY        INTEGER ,
                               O_ORDERSTATUS    CHAR(1) ,
                               O_TOTALPRICE     DECIMAL(15,2) ,
                               O_ORDERDATE      DATE ,
                               O_ORDERPRIORITY  CHAR(15) ,
                               O_CLERK          CHAR(15) ,
                               O_SHIPPRIORITY   INTEGER ,
                               O_COMMENT        VARCHAR(79) )'''
        if self.partition:
            pTablePartitionDefinition += " PARTITION BY RANGE(o_orderdate) (partition p1 start('1992-01-01') end('1998-08-03') every(interval '1 month'))"

        self.reload_table(tableName, tableDefinition, withClause,
                          preExecuteCmd, createExternalTable, pTablePartitionDefinition)


    def reload_Lineitem( self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads Lineitem Table
        '''

        tableName = "Lineitem"
        pTablePartitionDefinition = ''
        tableDefinition = '''( L_ORDERKEY    INTEGER ,
                                  L_PARTKEY     INTEGER ,
                                  L_SUPPKEY     INTEGER ,
                                  L_LINENUMBER  INTEGER ,
                                  L_QUANTITY    DECIMAL(15,2) ,
                                  L_EXTENDEDPRICE  DECIMAL(15,2) ,
                                  L_DISCOUNT    DECIMAL(15,2) ,
                                  L_TAX         DECIMAL(15,2) ,
                                  L_RETURNFLAG  CHAR(1) ,
                                  L_LINESTATUS  CHAR(1) ,
                                  L_SHIPDATE    DATE ,
                                  L_COMMITDATE  DATE ,
                                  L_RECEIPTDATE DATE ,
                                  L_SHIPINSTRUCT CHAR(25) ,
                                  L_SHIPMODE     CHAR(10) ,
                                  L_COMMENT      VARCHAR(44) )'''
        if self.partition:
            pTablePartitionDefinition += "PARTITION by range(l_shipdate) (partition p1 start('1992-01-01') end('1998-12-02') every(interval '1 month'))"

        self.reload_table(tableName, tableDefinition, withClause,
                          preExecuteCmd, createExternalTable, pTablePartitionDefinition)


    def reload_dataset_AO(self, withClause = "WITH (checksum=true,appendonly=true,compresstype=quicklz,compresslevel=1)", preExecuteCmd = "",
     createExternalTable = True, forceCreateTable = False):
        '''
        Create Append-only dataset database
        @param withClause: append only definition
        '''
        self.reload_dataset(withClause, preExecuteCmd, createExternalTable, forceCreateTable)

    def reload_dataset_CO(self, withClause = "WITH (checksum=false,appendonly=true,orientation = column,blocksize=49152,compresslevel=5)", preExecuteCmd = "",
     createExternalTable = True, forceCreateTable = False):
        '''
        Create Column Append-only dataset database
        @param withClause: append only definition
        '''
        self.reload_dataset(withClause, preExecuteCmd, createExternalTable, forceCreateTable)

    def reload_dataset(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True, forceCreateTable = False):
        '''
        Drop/create/reload the dataset database
        @param withClause: This is an optional string that may contain a "WITH"
                clause such as "WITH (APPENDONLY=True, ORIENTATION='column')".
                It could also contain other clauses, like "DISTRIBUTED BY...".
        @param preExecuteCmd: This is an optional string that will be executed
                prior to the CREATE TABLE statement.  For example, this string
                might contain "SET SEARCH_PATH TO AO_SCHEMA;" to set the
                search path to include the schema named "AO_SCHEMA".
        @param createExternalTable: set this to False if you have already created
                the external table and don't want to create it again.
        @param forceCreateTable: Set to True if you want to force re-creation
                of the table even if it already exists and data hasn't changed.
                (You might do this if you want to drop a heap table and
                re-create it as an AO table, for example.)
        @note: Uses PSQL to load dataset, problem with using pyODB causes SIGSEGV
        The results are different so it will always drop and load data. And since it gives SIGSEGV
        no data is loaded. We need to find a better way to do this.
        '''

        if withClause == None:
            withClause = ""

        if preExecuteCmd == None:
            preExecuteCmd = ""
        if len(preExecuteCmd) > 0 and preExecuteCmd[-1] != ";":
            preExecuteCmd = preExecuteCmd + ";"

        self.reload_Nation(withClause, preExecuteCmd, createExternalTable)
        self.reload_Region(withClause, preExecuteCmd, createExternalTable)
        self.reload_Part(withClause, preExecuteCmd, createExternalTable)
        self.reload_Supplier(withClause, preExecuteCmd, createExternalTable)
        self.reload_Partsupp(withClause, preExecuteCmd, createExternalTable)
        self.reload_Customer(withClause, preExecuteCmd, createExternalTable)
        self.reload_Orders(withClause, preExecuteCmd, createExternalTable)
        self.reload_Lineitem(withClause, preExecuteCmd, createExternalTable)


if __name__ == '__main__':
    pass
