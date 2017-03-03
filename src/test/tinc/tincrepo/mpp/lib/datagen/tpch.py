from gppylib.commands.base import Command, CommandResult, REMOTE

from mpp.lib.datagen import TINCTestDatabase

from mpp.lib.PSQL import PSQL, PSQLException

class TPCHDatabase(TINCTestDatabase):
    def __init__(self, database_name = "tpch_0.01_heap", scale_factor = 0.01, storage_type = 'heap', partition = False):
        self.db_name = database_name
        self.scale_factor = scale_factor
        self.storage_type = storage_type
        self.partition = partition
        self.primary_segments_count = -1
        super(TPCHDatabase, self).__init__(database_name)

    def setUp(self):
        if super(TPCHDatabase, self).setUp():
            return True
        #call reload_tpch
        if self.storage_type == 'heap':
            self.reload_TPCH()
        elif self.storage_type == 'ao':
            self.reload_AOTPCH()
        elif self.storage_type == 'co':
            self.reload_COTPCH()
        elif self.storage_type == 'parquet':
            self.reload_ParquetTPCH()
        else:
            self.reload_TPCH()

        return False

    def _form_dbgen_command_string(self, table_prefix):
        if self.primary_segments_count < 0:
            #Find number of primary segments
            self.primary_segments_count = (PSQL.run_sql_command("select 'primary_segment' from gp_segment_configuration \
                                                                where content >= 0 and role = 'p'", flags='')).count('primary_segment')
        db_gen_command = ''' execute 'bash -c \\"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T %s -s %f -N %d -n \$((GP_SEGMENT_ID + 1))\\"'
                on %d format 'text' (delimiter '|')''' % ( table_prefix, self.scale_factor, self.primary_segments_count, self.primary_segments_count)
        return db_gen_command
        
    def reload_table(self, pTableName, pTableDefinition, pDbGenCmd,
     withClause = "", preExecuteCmd = "", createExternalTable = True, pTablePartitionDefinition = ''):
        '''
        Drop a table; re-create it; and re-load the data into it.
        @param pTableName: The name of the table.
            pTableDefinition: The list of column names and data types, 
                enclosed in parentheses, for example:
                    "(x int, y float)"
        @param pDbGenCmd: A command to generate data to insert into the table.  
                This typically looks like:
                    dbGenCmd = "execute 'bash -c \\"\$GPHOME/bin/dbgen -b \
                    $GPHOME/bin/dists.dss -T r -s 1 \\"'
                    on 1 format 'text' (delimiter '|')"
                I don't really understand this; I copied it blindly from an 
                earlier version of gp.test.  I assume that the dbgen program 
                is being called to generate data for the external tables.
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

        if createExternalTable:
            cmd = 'drop external web table if exists e_' + pTableName
            out = PSQL.run_sql_command(cmd , dbname = self.db_name)
            cmd = "create external web table e_" + pTableName + \
                      pTableDefinition + " " + pDbGenCmd
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
        Reloads TPCH Nation
        '''

        tableName = "nation"
        tableDefinition = '''(N_NATIONKEY  INTEGER ,
                           N_NAME       CHAR(25) ,
                           N_REGIONKEY  INTEGER ,
                           N_COMMENT    VARCHAR(152))'''

        # Always use a fixed scale factor of 1
        # for Region and Nation as required by TPC-H standards.
        dbGenCmd = '''execute 'bash -c \\"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T n -s 1 \\"'
                on 1 format 'text' (delimiter '|')'''

        self.reload_table( tableName, tableDefinition, dbGenCmd,
         withClause, preExecuteCmd, createExternalTable )


    def reload_Region(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads TPCH Region
        '''

        tableName = "region"
        tableDefinition = '''( R_REGIONKEY  INTEGER ,
                                R_NAME       CHAR(25) ,
                                R_COMMENT    VARCHAR(152))'''
        # Always use a fixed scale factor of 1
        # for Region and Nation as required by TPC-H standards.
        dbGenCmd = '''execute 'bash -c \\"\$GPHOME/bin/dbgen -b \$GPHOME/bin/dists.dss -T r -s 1 \\"'
                on 1 format 'text' (delimiter '|')'''

        self.reload_table( tableName, tableDefinition, dbGenCmd,
         withClause, preExecuteCmd, createExternalTable )
        
    def reload_Part(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads TPCH Part
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

        dbGenCmd = self._form_dbgen_command_string('P')
        
        self.reload_table(tableName, tableDefinition, dbGenCmd,
         withClause, preExecuteCmd, createExternalTable)

 
    def reload_Supplier(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads TPCH Supplier
        '''
        
        tableName = "Supplier"

        tableDefinition = '''(S_SUPPKEY     INTEGER ,
                                 S_NAME        CHAR(25) ,
                                 S_ADDRESS     VARCHAR(40) ,
                                 S_NATIONKEY   INTEGER ,
                                 S_PHONE       CHAR(15) ,
                                 S_ACCTBAL     DECIMAL(15,2) ,
                                 S_COMMENT     VARCHAR(101) )'''

        dbGenCmd = self._form_dbgen_command_string('s')
        
        self.reload_table(tableName, tableDefinition, dbGenCmd,
         withClause, preExecuteCmd, createExternalTable)
        
    
    def reload_Partsupp(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads TPCH Part Supply
        '''

        tableName = "Partsupp"
        tableDefinition = '''(   PS_PARTKEY     INTEGER ,
                                 PS_SUPPKEY     INTEGER ,
                                 PS_AVAILQTY    INTEGER ,
                                 PS_SUPPLYCOST  DECIMAL(15,2)  ,
                                 PS_COMMENT     VARCHAR(199) )'''

        dbGenCmd = self._form_dbgen_command_string('S')
        
        self.reload_table(tableName, tableDefinition, dbGenCmd,
         withClause, preExecuteCmd, createExternalTable)


    def reload_Customer(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads TPCH Customer
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

        dbGenCmd = self._form_dbgen_command_string('c')
        
        self.reload_table(tableName, tableDefinition, dbGenCmd,
         withClause, preExecuteCmd, createExternalTable)

    
    def reload_Orders(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads TPCH Orders
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

        dbGenCmd = self._form_dbgen_command_string('O')
        
        self.reload_table(tableName, tableDefinition, dbGenCmd,
         withClause, preExecuteCmd, createExternalTable, pTablePartitionDefinition)

    
    def reload_Lineitem( self, withClause = "", preExecuteCmd = "",
     createExternalTable = True):
        '''
        Reloads TPCH Lineitem
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

        dbGenCmd = self._form_dbgen_command_string('L')
                      
        self.reload_table(tableName, tableDefinition, dbGenCmd,
         withClause, preExecuteCmd, createExternalTable, pTablePartitionDefinition)


    def reload_AOTPCH(self, withClause = "WITH (checksum=true,appendonly=true,compresstype=quicklz,compresslevel=1)", preExecuteCmd = "",
     createExternalTable = True, forceCreateTable = False):
        '''
        Create Append-only TPCH1 database
        @param withClause: append only definition
        @param scaleFactor: scale factor, default = 1
        '''
        self.reload_TPCH(withClause, preExecuteCmd, createExternalTable, forceCreateTable)

    def reload_COTPCH(self, withClause = "WITH (checksum=false,appendonly=true,orientation = column,blocksize=49152,compresslevel=5)", preExecuteCmd = "",
     createExternalTable = True, forceCreateTable = False):
        '''
        Create Column Append-only TPCH1 database
        @param withClause: append only definition
        @param scaleFactor: scale factor, default = 1
        '''
        self.reload_TPCH(withClause, preExecuteCmd, createExternalTable, forceCreateTable)

    def reload_ParquetTPCH(self, withClause = "WITH (appendonly=true,orientation=parquet)", preExecuteCmd = "",createExternalTable = True, forceCreateTable = False):
        '''
        Create Parquet TPCH database
        @param withClause: parquet definition
        @param scaleFactor: scale factor, default = 0.01
        '''
        self.reload_TPCH(withClause, preExecuteCmd, createExternalTable, forceCreateTable)
 
    def reload_TPCH(self, withClause = "", preExecuteCmd = "",
     createExternalTable = True, forceCreateTable = False):
        '''
        Drop/create/reload the TPCH1 database
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
        @change: 2010-05-20 mgilkey
                Added withClause, preExecuteCmd, and createExternalTable.
        @note: Uses PSQL to load TPCH1 data, problem with using pyODB causes SIGSEGV
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
    print (PSQL.run_sql_command("select 'primary_segment' from gp_segment_configuration \
                                 where content >= 0 and role = 'p'", flags='')).count('primary_segment')
