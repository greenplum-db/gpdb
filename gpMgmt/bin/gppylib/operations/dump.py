import os
import shutil
import tempfile
from datetime import datetime
from gppylib import gplog
from gppylib.commands.base import Command, REMOTE, ExecutionError
from gppylib.commands.gp import Psql
from gppylib.commands.unix import getUserName, findCmdInPath, curr_platform, SUNOS
from gppylib.db import dbconn
from gppylib.db.dbconn import execSQL, execSQLForSingleton
from gppylib.gparray import GpArray
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.operations import Operation
from gppylib.operations.unix import CheckDir, CheckFile, ListFiles, ListFilesByPattern, MakeDir, RemoveFile, RemoveTree, RemoveRemoteTree
from gppylib.operations.utils import RemoteOperation, ParallelOperation
from gppylib.operations.backup_utils import backup_file_with_nbu, check_file_dumped_with_nbu, create_temp_file_from_list, execute_sql, \
                                            generate_ao_state_filename, generate_cdatabase_filename, generate_co_state_filename, generate_dirtytable_filename, \
                                            generate_filter_filename, generate_global_filename, generate_global_prefix, generate_increments_filename, \
                                            generate_master_config_filename, generate_master_dbdump_prefix, generate_master_status_prefix, generate_partition_list_filename, \
                                            generate_pgstatlastoperation_filename, generate_report_filename, generate_schema_filename, generate_seg_dbdump_prefix, \
                                            generate_seg_status_prefix, generate_segment_config_filename, get_incremental_ts_from_report_file, \
                                            get_latest_full_dump_timestamp, get_latest_full_ts_with_nbu, get_latest_report_timestamp, get_lines_from_file, \
                                            restore_file_with_nbu, validate_timestamp, verify_lines_in_file, write_lines_to_file

logger = gplog.get_default_logger()

# MPP-15307
# DUMP_DATE dictates the db_dumps/ subdirectory to which gpcrondump will dump.
# It is computed just once to ensure different pieces of logic herein operate on the same subdirectory.
TIMESTAMP = datetime.now()
TIMESTAMP_KEY = TIMESTAMP.strftime("%Y%m%d%H%M%S")
DUMP_DATE = TIMESTAMP.strftime("%Y%m%d")
FULL_DUMP_TS_WITH_NBU = None

COMPRESSION_FACTOR = 12                 # TODO: Where did 12 come from?

INJECT_GP_DUMP_FAILURE = None

GET_ALL_DATATABLES_SQL = """
SELECT ALLTABLES.oid, ALLTABLES.schemaname, ALLTABLES.tablename FROM 

    (SELECT c.oid, n.nspname AS schemaname, c.relname AS tablename FROM pg_class c, pg_namespace n 
    WHERE n.oid = c.relnamespace) as ALLTABLES,

    (SELECT n.nspname AS schemaname, c.relname AS tablename
    FROM pg_class c LEFT JOIN pg_namespace n ON n.oid = c.relnamespace
    LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace
    WHERE c.relkind = 'r'::"char" AND c.oid > 16384 AND (c.relnamespace > 16384 or n.nspname = 'public') 
    EXCEPT
    ((SELECT x.schemaname, x.partitiontablename FROM 
    (SELECT distinct schemaname, tablename, partitiontablename, partitionlevel FROM pg_partitions) as X,
    (SELECT schemaname, tablename maxtable, max(partitionlevel) maxlevel FROM pg_partitions group by (tablename, schemaname)) as Y    
    WHERE x.schemaname = y.schemaname and x.tablename = Y.maxtable and x.partitionlevel != Y.maxlevel) 
    UNION (SELECT distinct schemaname, tablename FROM pg_partitions))) as DATATABLES 

WHERE ALLTABLES.schemaname = DATATABLES.schemaname and ALLTABLES.tablename = DATATABLES.tablename AND ALLTABLES.oid not in (select reloid from pg_exttable) AND ALLTABLES.schemaname NOT LIKE 'pg_temp_%'
"""

GET_ALL_USER_TABLES_FOR_SCHEMA_SQL = """
SELECT n.nspname AS schemaname, c.relname AS tablename
    FROM pg_class c LEFT JOIN pg_namespace n ON n.oid = c.relnamespace
    LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace
    WHERE c.relkind = 'r'::"char" AND c.oid > 16384 AND n.nspname = '%s'
"""

GET_ALL_USER_TABLES_SQL = """
SELECT n.nspname AS schemaname, c.relname AS tablename
    FROM pg_class c LEFT JOIN pg_namespace n ON n.oid = c.relnamespace
    LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace
    WHERE c.relkind = 'r'::"char" AND c.oid > 16384 AND (c.relnamespace > 16384 or n.nspname = 'public')
"""

GET_APPENDONLY_DATA_TABLE_INFO_SQL = """
    SELECT ALL_DATA_TABLES.oid, ALL_DATA_TABLES.schemaname, ALL_DATA_TABLES.tablename, OUTER_PG_CLASS.relname as tupletable FROM
    (%s) as ALL_DATA_TABLES, pg_appendonly, pg_class OUTER_PG_CLASS
    WHERE ALL_DATA_TABLES.oid = pg_appendonly.relid 
    AND OUTER_PG_CLASS.oid = pg_appendonly.segrelid
""" % GET_ALL_DATATABLES_SQL

GET_ALL_AO_DATATABLES_SQL = """
    %s AND pg_appendonly.columnstore = 'f'
""" % GET_APPENDONLY_DATA_TABLE_INFO_SQL

GET_ALL_CO_DATATABLES_SQL = """
    %s AND pg_appendonly.columnstore = 't'
""" % GET_APPENDONLY_DATA_TABLE_INFO_SQL

GET_ALL_HEAP_DATATABLES_SQL = """
    %s AND ALLTABLES.oid not in (SELECT relid from pg_appendonly)
""" % GET_ALL_DATATABLES_SQL

GET_ALL_AO_CO_DATATABLES_SQL = """
    %s AND ALLTABLES.oid in (SELECT relid from pg_appendonly)
""" % GET_ALL_DATATABLES_SQL

GET_LAST_OPERATION_SQL = """
    SELECT PGN.nspname, PGC.relname, objid, staactionname, stasubtype, statime FROM pg_stat_last_operation, pg_class PGC, pg_namespace PGN
    WHERE objid = PGC.oid 
    AND PGC.relnamespace = PGN.oid
    AND staactionname IN ('CREATE', 'ALTER', 'TRUNCATE') 
    AND objid IN (SELECT oid FROM (%s) as AOCODATATABLES)
    ORDER BY objid, staactionname
""" % GET_ALL_AO_CO_DATATABLES_SQL

GET_ALL_SCHEMAS_SQL = "SELECT nspname from pg_namespace;"

def get_include_schema_list_from_exclude_schema(exclude_schema_list, catalog_schema_list, master_port, dbname):
    include_schema_list = []
    schema_list = execute_sql(GET_ALL_SCHEMAS_SQL, master_port, dbname)
    for schema in schema_list:
        if schema[0].strip() not in exclude_schema_list and schema[0].strip() not in catalog_schema_list:
            include_schema_list.append(schema[0])

    return include_schema_list

def backup_schema_file_with_ddboost(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key=None):
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    filename = generate_schema_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key, True)
    copy_file_to_dd(filename, dump_dir, timestamp_key)

def backup_report_file_with_ddboost(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key=None):
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    filename = generate_report_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key, True)
    copy_file_to_dd(filename, dump_dir, timestamp_key)

def backup_increments_file_with_ddboost(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp):
    filename = generate_increments_filename(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, True)
    copy_file_to_dd(filename, dump_dir, full_timestamp)

def copy_file_to_dd(filename, dump_dir, timestamp_key=None):
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    basefilename = os.path.basename(filename)
    cmdStr = "gpddboost --copyToDDBoost --from-file=%s --to-file=%s/%s/%s" % (filename, dump_dir, timestamp_key[0:8], basefilename)
    cmd = Command('copy file %s to DD machine' % basefilename, cmdStr)
    cmd.run(validateAfter=True)

def generate_dump_timestamp(timestamp):
    global TIMESTAMP
    global TIMESTAMP_KEY
    global DUMP_DATE

    if timestamp is not None:
        TIMESTAMP = timestamp
    else:
        TIMESTAMP = datetime.now()
    TIMESTAMP_KEY = TIMESTAMP.strftime("%Y%m%d%H%M%S")
    DUMP_DATE = TIMESTAMP.strftime("%Y%m%d")

def get_ao_partition_state(master_port, dbname):
    ao_partition_info = get_ao_partition_list(master_port, dbname)
    ao_partition_list = get_partition_state(master_port, dbname, 'pg_aoseg', ao_partition_info)
    return ao_partition_list

def get_co_partition_state(master_port, dbname):
    co_partition_info = get_co_partition_list(master_port, dbname)
    co_partition_list = get_partition_state(master_port, dbname, 'pg_aoseg', co_partition_info)
    return co_partition_list

def validate_modcount(schema, tablename, cnt):
    if not cnt:
        return
    if not cnt.isdigit():
        raise Exception("Can not convert modification count for table. Possibly exceeded  backup max tuple count of 1 quadrillion rows per table for: '%s.%s' '%s'" % (schema, tablename, cnt))
    if len(cnt) > 15:
        raise Exception("Exceeded backup max tuple count of 1 quadrillion rows per table for: '%s.%s' '%s'" % (schema, tablename, cnt))

def get_partition_state(master_port, dbname, catalog_schema, partition_info):
    """
        Reads the partition state for an AO or AOCS relation, which is the sum of
        the modication counters over all ao segment files.
        The sum might be an invalid number even when the relation contains tuples.
        The reason is that the master aoseg info tuple for segno 0 is not there
        after the CTAS. Vacuum will correct in missing state on the master.
        However, this leads to a difference in the modcount when the partition
        state is checked on the next incremantal backup.

        Thus, partition state returns 0 also when the aoseg relation is empty.
        Why is that correct?
        A table that as a modcount of 0 can only be there iff the last operation
        was a special operation (TRUNCATE, CREATE, ALTER TABLE). That is handled
        in a special way by backup. Every DML operation will increase the
        modcount by 1. Therefore it is save to assume that to relations with
        modcount 0 with the same last special operation do not have a logical
        change in them.
    """
    partition_list = list()

    dburl = dbconn.DbURL(port=master_port, dbname=dbname)
    num_sqls = 0
    with dbconn.connect(dburl) as conn:
        for (oid, schemaname, partition_name, tupletable) in partition_info:
            modcount_sql = "select to_char(coalesce(sum(modcount::bigint), 0), '999999999999999999999') from %s.%s" % (catalog_schema, tupletable)
            modcount = execSQLForSingleton(conn, modcount_sql)
            num_sqls += 1
            if num_sqls == 1000: # The choice of batch size was chosen arbitrarily
                logger.debug('Completed executing batch of 1000 tuple count SQLs')
                conn.commit()
                num_sqls = 0
            if modcount:
                modcount = modcount.strip()
            validate_modcount(schemaname, partition_name, modcount)
            partition_list.append('%s, %s, %s' %(schemaname, partition_name, modcount))

    return partition_list

def get_tables_with_dirty_metadata(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, cur_pgstatoperations,
                                   netbackup_service_host=None, netbackup_block_size=None):
    last_dump_timestamp = get_last_dump_timestamp(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, netbackup_service_host, netbackup_block_size)
    old_pgstatoperations_file = generate_pgstatlastoperation_filename(master_datadir, backup_dir, dump_dir, dump_prefix, last_dump_timestamp)
    if netbackup_service_host:
        restore_file_with_nbu(netbackup_service_host, netbackup_block_size, old_pgstatoperations_file)
    old_pgstatoperations = get_lines_from_file(old_pgstatoperations_file)
    old_pgstatoperations_dict = get_pgstatlastoperations_dict(old_pgstatoperations)
    dirty_tables = compare_metadata(old_pgstatoperations_dict, cur_pgstatoperations)
    return dirty_tables

def get_dirty_partition_tables(table_type, curr_state_partition_list, master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp,
                               netbackup_service_host=None, netbackup_block_size=None):
    last_state_partition_list = get_last_state(table_type, master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, netbackup_service_host, netbackup_block_size)
    last_state_dict = create_partition_dict(last_state_partition_list)
    curr_state_dict = create_partition_dict(curr_state_partition_list)
    return compare_dict(last_state_dict, curr_state_dict)

def get_last_state(table_type, master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, netbackup_service_host=None, netbackup_block_size=None):
    last_ts = get_last_dump_timestamp(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, netbackup_service_host, netbackup_block_size)
    last_state_filename = get_filename_from_filetype(table_type, master_datadir, backup_dir, dump_dir, dump_prefix, last_ts.strip())
    if netbackup_service_host is None:
        if not os.path.isfile(last_state_filename):
            raise Exception('%s state file does not exist: %s' % (table_type, last_state_filename))
    else:
        if check_file_dumped_with_nbu(netbackup_service_host, last_state_filename):
            if not os.path.exists(last_state_filename):
                restore_file_with_nbu(netbackup_service_host, netbackup_block_size, last_state_filename)

    return get_lines_from_file(last_state_filename)

def compare_metadata(old_pgstatoperations, cur_pgstatoperations):
    diffs = set()
    for operation in cur_pgstatoperations:
        toks = operation.split(',')
        if len(toks) != 6:
            raise Exception('Wrong number of tokens in last_operation data for current backup: "%s"' % operation)
        if (toks[2], toks[3]) not in old_pgstatoperations or old_pgstatoperations[(toks[2], toks[3])] != operation:
            tname = '%s.%s' % (toks[0], toks[1])
            diffs.add(tname)
    return diffs

def get_pgstatlastoperations_dict(last_operations):
    last_operations_dict = {}
    for operation in last_operations:
        toks = operation.split(',')
        if len(toks) != 6:
            raise Exception('Wrong number of tokens in last_operation data for last backup: "%s"' % operation)
        last_operations_dict[(toks[2], toks[3])] = operation
    return last_operations_dict

def get_last_dump_timestamp(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, netbackup_service_host=None, netbackup_block_size=None):
    increments_filename = generate_increments_filename(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp)
    if netbackup_service_host is None:
        if not os.path.isfile(increments_filename):
            return full_timestamp
    else:
        if check_file_dumped_with_nbu(netbackup_service_host, increments_filename):
            if not os.path.exists(increments_filename):
                logger.debug('Increments file %s was dumped with NBU, so restoring it now...' % increments_filename)
                restore_file_with_nbu(netbackup_service_host, netbackup_block_size, increments_filename)
        else:
            logger.debug('Increments file %s was NOT dumped with NBU, returning full timestamp as last dump timestamp' % increments_filename)
            return full_timestamp

    lines = get_lines_from_file(increments_filename)
    if not lines:
        raise Exception("increments file exists but is empty: '%s'" % increments_filename)
    timestamp = lines[-1].strip()
    if not validate_timestamp(timestamp):
        raise Exception("get_last_dump_timestamp found invalid ts in file '%s': '%s'" % (increments_filename, timestamp))
    return timestamp

def create_partition_dict(partition_list):
    table_dict = dict()
    for partition in partition_list:
        fields = partition.split(',')
        if len(fields) != 3:
            raise Exception('Invalid state file format %s' % partition)
        key = '%s.%s' % (fields[0].strip(), fields[1].strip())
        table_dict[key] = fields[2].strip()

    return table_dict

def compare_dict(last_dict, curr_dict):
    diffkeys = set()
    for k in curr_dict:
        if k not in last_dict or (curr_dict[k] != last_dict[k]):
            diffkeys.add(k)
    return diffkeys

def get_filename_from_filetype(table_type, master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key=None, ddboost=False):
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    if table_type == 'ao':
        filename = generate_ao_state_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key, ddboost)
    elif table_type == 'co':
        filename = generate_co_state_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key, ddboost)
    else:
        raise Exception('Invalid table type %s provided. Supported table types ao/co.' % table_type)

    return filename

def write_state_file(table_type, master_datadir, backup_dir, dump_dir, dump_prefix, partition_list, ddboost=False):
    filename = get_filename_from_filetype(table_type, master_datadir, backup_dir, dump_dir, dump_prefix, None, ddboost)

    write_lines_to_file(filename, partition_list)
    verify_lines_in_file(filename, partition_list)

    if ddboost:
        copy_file_to_dd(filename, dump_dir)

# return a list of dirty tables
def get_dirty_tables(master_port, dbname, master_datadir, backup_dir, dump_dir, dump_prefix, fulldump_ts,
                     ao_partition_list, co_partition_list, last_operation_data,
                     netbackup_service_host, netbackup_block_size):

    dirty_heap_tables = get_dirty_heap_tables(master_port, dbname)

    dirty_ao_tables = get_dirty_partition_tables('ao', ao_partition_list, master_datadir, backup_dir, dump_dir, dump_prefix,
                                                 fulldump_ts, netbackup_service_host, netbackup_block_size)

    dirty_co_tables = get_dirty_partition_tables('co', co_partition_list, master_datadir, backup_dir, dump_dir, dump_prefix,
                                                 fulldump_ts, netbackup_service_host, netbackup_block_size)

    dirty_metadata_set = get_tables_with_dirty_metadata(master_datadir, backup_dir, dump_dir, dump_prefix, fulldump_ts, last_operation_data,
                                                        netbackup_service_host, netbackup_block_size)

    return list(dirty_heap_tables | dirty_ao_tables | dirty_co_tables | dirty_metadata_set)

def get_dirty_heap_tables(master_port, dbname):
    dirty_tables = set()
    qresult = get_heap_partition_list(master_port, dbname)
    for row in qresult:
        if len(row) != 3:
            raise Exception("Heap tables query returned rows with unexpected number of columns %d" % len(row))
        tname = '%s.%s' % (row[1], row[2])
        dirty_tables.add(tname)
    return dirty_tables

def write_dirty_file_to_temp(dirty_tables):
    return create_temp_file_from_list(dirty_tables, 'dirty_backup_list_')

def write_dirty_file(mdd, dirty_tables, backup_dir, dump_dir, dump_prefix, timestamp_key=None, ddboost=False):
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    if dirty_tables is None:
        return None

    dirty_list_file = generate_dirtytable_filename(mdd, backup_dir, dump_dir, dump_prefix, timestamp_key, ddboost)
    write_lines_to_file(dirty_list_file, dirty_tables)

    verify_lines_in_file(dirty_list_file, dirty_tables)
    if ddboost:
        copy_file_to_dd(dirty_list_file, dump_dir)

    return dirty_list_file

def get_heap_partition_list(master_port, dbname):
    return execute_sql(GET_ALL_HEAP_DATATABLES_SQL, master_port, dbname)

def get_ao_partition_list(master_port, dbname):
    partition_list = execute_sql(GET_ALL_AO_DATATABLES_SQL, master_port, dbname)
    for line in partition_list:
        if len(line) != 4:
            raise Exception('Invalid results from query to get all AO tables: [%s]' % (','.join(line)))

    return partition_list

def get_co_partition_list(master_port, dbname):
    partition_list = execute_sql(GET_ALL_CO_DATATABLES_SQL, master_port, dbname)
    for line in partition_list:
        if len(line) != 4:
            raise Exception('Invalid results from query to get all CO tables: [%s]' % (','.join(line)))

    return partition_list

def get_partition_list(master_port, dbname):
    return execute_sql(GET_ALL_DATATABLES_SQL, master_port, dbname)

def get_user_table_list(master_port, dbname):
    return execute_sql(GET_ALL_USER_TABLES_SQL, master_port, dbname)

def get_user_table_list_for_schema(master_port, dbname, schema):
    sql = GET_ALL_USER_TABLES_FOR_SCHEMA_SQL % schema
    return execute_sql(sql, master_port, dbname)

def get_last_operation_data(master_port, dbname):
    # oid, action, subtype, timestamp
    rows = execute_sql(GET_LAST_OPERATION_SQL, master_port, dbname)
    data = []
    for row in rows:
        if len(row) != 6:
            raise Exception("Invalid return from query in get_last_operation_data: % cols" % (len(row)))
        line = "%s,%s,%d,%s,%s,%s" % (row[0], row[1], row[2], row[3], row[4], row[5])
        data.append(line)
    return data

def write_partition_list_file(master_datadir, backup_dir, timestamp_key, master_port, dbname, dump_dir, dump_prefix,
                              ddboost=False, netbackup_service_host=None):
    filter_file = get_filter_file(dbname, master_datadir, backup_dir, dump_dir, dump_prefix, ddboost, netbackup_service_host)
    partition_list_file_name = generate_partition_list_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key)

    if filter_file:
        shutil.copyfile(filter_file, partition_list_file_name)
        verify_lines_in_file(partition_list_file_name, get_lines_from_file(filter_file))
    else:
        lines_to_write = get_partition_list(master_port, dbname)
        partition_list = []
        for line in lines_to_write:
            if len(line) != 3:
                raise Exception('Invalid results from query to get all tables: [%s]' % (','.join(line)))
            partition_list.append("%s.%s" % (line[1], line[2]))

        write_lines_to_file(partition_list_file_name, partition_list)
        verify_lines_in_file(partition_list_file_name, partition_list)

    if ddboost:
        copy_file_to_dd(partition_list_file_name, dump_dir)

def write_last_operation_file(master_datadir, backup_dir, rows, dump_dir, dump_prefix, timestamp_key=None, ddboost=False):
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    filename = generate_pgstatlastoperation_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key, ddboost)
    write_lines_to_file(filename, rows)
    verify_lines_in_file(filename, rows)

    if ddboost:
        copy_file_to_dd(filename, dump_dir)

def validate_current_timestamp(backup_dir, dump_dir, dump_prefix, current=None):
    if current is None:
        current = TIMESTAMP_KEY

    latest = get_latest_report_timestamp(backup_dir, dump_dir, dump_prefix)
    if not latest:
        return
    if latest >= current:
        raise Exception('There is a future dated backup on the system preventing new backups')

def get_backup_dir(master_datadir, backup_dir):
    if backup_dir:
        return backup_dir
    return master_datadir

def update_filter_file(dump_database, master_datadir, backup_dir, master_port, dump_dir, dump_prefix, ddboost=False, netbackup_service_host=None,
                       netbackup_policy=None, netbackup_schedule=None, netbackup_block_size=None, netbackup_keyword=None):
    filter_filename = get_filter_file(dump_database, master_datadir, backup_dir, dump_dir, dump_prefix, ddboost, netbackup_service_host, netbackup_block_size)
    if netbackup_service_host:
        restore_file_with_nbu(netbackup_service_host, netbackup_block_size, filter_filename)
    filter_tables = get_lines_from_file(filter_filename)
    tables_sql = "SELECT DISTINCT schemaname||'.'||tablename FROM pg_partitions"
    partitions_sql = "SELECT schemaname||'.'||partitiontablename FROM pg_partitions WHERE schemaname||'.'||tablename='%s';"
    table_list = execute_sql(tables_sql, master_port, dump_database)

    for table in table_list:
        if table[0] in filter_tables:
            partitions_list = execute_sql(partitions_sql % table[0], master_port, dump_database)
            filter_tables.extend([x[0] for x in partitions_list])

    write_lines_to_file(filter_filename, list(set(filter_tables)))
    if netbackup_service_host:
        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, filter_filename)

def get_filter_file(dump_database, master_datadir, backup_dir, dump_dir, dump_prefix, ddboost=False, netbackup_service_host=None, netbackup_block_size=None):
    if netbackup_service_host is None:
        timestamp = get_latest_full_dump_timestamp(dump_database, get_backup_dir(master_datadir, backup_dir), dump_dir, dump_prefix, ddboost)
    else:
        if FULL_DUMP_TS_WITH_NBU is None:
            timestamp = get_latest_full_ts_with_nbu(dump_database, get_backup_dir(master_datadir, backup_dir), dump_prefix, netbackup_service_host, netbackup_block_size)
        else:
            timestamp = FULL_DUMP_TS_WITH_NBU
        if timestamp is None:
            raise Exception("No full backup timestamp found for given NetBackup server.")
    filter_file = generate_filter_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp)
    if netbackup_service_host is None:
        if os.path.isfile(filter_file):
            return filter_file
        return None
    else:
        if check_file_dumped_with_nbu(netbackup_service_host, filter_file):
            if not os.path.exists(filter_file):
                restore_file_with_nbu(netbackup_service_host, netbackup_block_size, filter_file)
            return filter_file
        return None

def update_filter_file_with_dirty_list(filter_file, dirty_tables):
    filter_list = []
    if filter_file:
        filter_list = get_lines_from_file(filter_file)

        for table in dirty_tables:
            if table not in filter_list:
                filter_list.append(table)

        write_lines_to_file(filter_file, filter_list)

def filter_dirty_tables(dirty_tables, dump_database, master_datadir, backup_dir, dump_dir, dump_prefix, ddboost=False,
                        netbackup_service_host=None, netbackup_block_size=None):
    if netbackup_service_host is None:
        timestamp = get_latest_full_dump_timestamp(dump_database, get_backup_dir(master_datadir, backup_dir), dump_dir, dump_prefix, ddboost)
    else:
        if FULL_DUMP_TS_WITH_NBU is None:
            timestamp = get_latest_full_ts_with_nbu(dump_database, get_backup_dir(master_datadir, backup_dir), dump_prefix, netbackup_service_host, netbackup_block_size)
        else:
            timestamp = FULL_DUMP_TS_WITH_NBU

    schema_filename = generate_schema_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp, ddboost)
    filter_file = get_filter_file(dump_database, master_datadir, backup_dir, dump_dir, dump_prefix, ddboost, netbackup_service_host, netbackup_block_size)
    if filter_file:
        tables_to_filter = get_lines_from_file(filter_file)
        dirty_copy = dirty_tables[:]
        for table in dirty_copy:
            if table not in tables_to_filter:
                if os.path.exists(schema_filename):
                    schemas_to_filter = get_lines_from_file(schema_filename)
                    table_schema = table.split('.')[0].strip()
                    if table_schema not in schemas_to_filter:
                        dirty_tables.remove(table)
                else:
                    dirty_tables.remove(table)

    # Schema Level Backup :
    # Any newly added partition to the schema's in schema file is added to the
    # filter file for it to be backed up.
    # Newly added partition can be obtained from dirty table list file.
    if os.path.exists(schema_filename):
        update_filter_file_with_dirty_list(filter_file, dirty_tables)

    return dirty_tables

def backup_state_files_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, netbackup_service_host, netbackup_policy,
                                netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_state_files_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')

    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         get_filename_from_filetype('ao', master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key))
    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         get_filename_from_filetype('co', master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key))
    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         generate_pgstatlastoperation_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key))

def backup_schema_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, netbackup_service_host, netbackup_policy,
                                netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_schema_file_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         generate_schema_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key))

def backup_cdatabase_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, netbackup_service_host, netbackup_policy,
                                   netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_cdatabase_file_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         generate_cdatabase_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key))

def backup_report_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, netbackup_service_host, netbackup_policy,
                                netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_report_file_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         generate_report_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key))

def backup_global_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, netbackup_service_host, netbackup_policy, netbackup_schedule,
                                netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_global_file_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         generate_global_filename(master_datadir, backup_dir, dump_dir, dump_prefix, DUMP_DATE, timestamp_key))

def backup_config_files_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, master_port, netbackup_service_host, netbackup_policy,
                                 netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_config_files_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    #backing up master config file
    config_backup_file = generate_master_config_filename(dump_prefix, timestamp_key)
    if backup_dir is not None:
        path = os.path.join(backup_dir, 'db_dumps', DUMP_DATE, config_backup_file)
    else:
        path = os.path.join(master_datadir, dump_dir, DUMP_DATE, config_backup_file)
    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, path)

    #backing up segment config files
    gparray = GpArray.initFromCatalog(dbconn.DbURL(port=master_port), utility=True)
    primaries = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
    for seg in primaries:
        config_backup_file = generate_segment_config_filename(dump_prefix, seg.getSegmentDbId(), timestamp_key)
        if backup_dir is not None:
            path = os.path.join(backup_dir, 'db_dumps', DUMP_DATE, config_backup_file)
        else:
            path = os.path.join(seg.getSegmentDataDirectory(), dump_dir, DUMP_DATE, config_backup_file)
        host = seg.getSegmentHostName()
        backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, path, host)

def backup_dirty_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, netbackup_service_host, netbackup_policy,
                               netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_dirty_file_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         generate_dirtytable_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key))

def backup_increments_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, netbackup_service_host,
                                    netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_increments_file_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         generate_increments_filename(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp))

def backup_partition_list_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, netbackup_service_host, netbackup_policy,
                                        netbackup_schedule, netbackup_block_size, netbackup_keyword, timestamp_key=None):
    logger.debug("Inside backup_partition_list_file_with_nbu\n")
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if timestamp_key is None:
        timestamp_key = TIMESTAMP_KEY

    backup_file_with_nbu(netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                         generate_partition_list_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key))

class DumpDatabase(Operation):
    # TODO: very verbose constructor = room for improvement. in the parent constructor, we could use kwargs
    # to automatically take in all arguments and perhaps do some data type validation.
    def __init__(self, dump_database, dump_schema, include_dump_tables, exclude_dump_tables, include_dump_tables_file,
                 exclude_dump_tables_file, backup_dir, free_space_percent, compress, clear_catalog_dumps, encoding,
                 output_options, batch_default, master_datadir, master_port, dump_dir, dump_prefix, ddboost,
                 netbackup_service_host, netbackup_policy, netbackup_schedule, netbackup_block_size, netbackup_keyword,
                 incremental=False, include_schema_file=None):
        self.dump_database = dump_database
        self.dump_schema = dump_schema
        self.include_dump_tables = include_dump_tables
        self.exclude_dump_tables = exclude_dump_tables
        self.include_dump_tables_file = include_dump_tables_file,
        self.exclude_dump_tables_file = exclude_dump_tables_file,
        self.backup_dir = backup_dir
        self.free_space_percent = free_space_percent
        self.compress = compress
        self.clear_catalog_dumps = clear_catalog_dumps
        self.encoding = encoding
        self.output_options = output_options
        self.batch_default = batch_default
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.include_schema_file = include_schema_file
        self.ddboost = ddboost
        self.incremental = incremental
        self.netbackup_service_host = netbackup_service_host
        self.netbackup_policy = netbackup_policy
        self.netbackup_schedule = netbackup_schedule
        self.netbackup_block_size = netbackup_block_size
        self.netbackup_keyword = netbackup_keyword

    def execute(self):
        self.exclude_dump_tables = ValidateDumpDatabase(dump_database = self.dump_database,
                                                        dump_schema = self.dump_schema,
                                                        include_dump_tables = self.include_dump_tables,
                                                        exclude_dump_tables = self.exclude_dump_tables,
                                                        include_dump_tables_file = self.include_dump_tables_file[0],
                                                        exclude_dump_tables_file = self.exclude_dump_tables_file[0],
                                                        backup_dir = self.backup_dir,
                                                        free_space_percent = self.free_space_percent,
                                                        compress = self.compress,
                                                        batch_default = self.batch_default,
                                                        master_datadir = self.master_datadir,
                                                        master_port = self.master_port,
                                                        dump_dir = self.dump_dir,
                                                        incremental = self.incremental,
                                                        include_schema_file = self.include_schema_file).run()

        if self.incremental and self.dump_prefix \
                            and get_filter_file(self.dump_database, self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.ddboost, self.netbackup_service_host):
            filtered_dump_line = self.create_filtered_dump_string(getUserName(), DUMP_DATE, TIMESTAMP_KEY)
            (start, end, rc) = self.perform_dump('Dump process', filtered_dump_line)
            return self.create_dump_outcome(start, end, rc)
        dump_line = self.create_dump_string(getUserName(), DUMP_DATE, TIMESTAMP_KEY)
        (start, end, rc) = self.perform_dump('Dump process', dump_line)
        if self.dump_prefix and self.include_dump_tables_file and not self.incremental:
            self.create_filter_file()
        return self.create_dump_outcome(start, end, rc)

    def perform_dump(self, title, dump_line):
        logger.info("%s command line %s" % (title, dump_line))
        logger.info("Starting %s" % title)
        start = TIMESTAMP
        cmd = Command('Invoking gp_dump', dump_line)
        cmd.run()
        rc = cmd.get_results().rc
        if INJECT_GP_DUMP_FAILURE is not None:
            rc = INJECT_GP_DUMP_FAILURE
        if rc != 0:
            logger.warn("%s returned exit code %d" % (title, rc))
        else:
            logger.info("%s returned exit code 0" % title)
        end = datetime.now()
        return (start, end, rc)

    # If using -t, copy the filter file over the table list to be passed to the master
    # If using -T, get the intersection of the filter and the table list
    # In either case, the filter file contains the list of tables to include
    def create_filter_file(self):
        filter_name = generate_filter_filename(self.master_datadir,
                                               get_backup_dir(self.master_datadir, self.backup_dir),
                                               self.dump_dir,
                                               self.dump_prefix,
                                               TIMESTAMP_KEY)
        if self.include_dump_tables_file[0]:
            shutil.copyfile(self.include_dump_tables_file[0], filter_name)
            verify_lines_in_file(filter_name, get_lines_from_file(self.include_dump_tables_file[0]))
            if self.netbackup_service_host:
                backup_file_with_nbu(self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword, filter_name)
        elif self.exclude_dump_tables_file[0]:
            filters = get_lines_from_file(self.exclude_dump_tables_file[0])
            partitions = get_user_table_list(self.master_port, self.dump_database)
            tables = []
            for p in partitions:
                tablename = '%s.%s' % (p[0], p[1])
                if tablename not in filters:
                    tables.append(tablename)
            write_lines_to_file(filter_name, tables)
            if self.netbackup_service_host:
                backup_file_with_nbu(self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule, self.netbackup_block_size, self.netbackup_keyword, filter_name)
        logger.info('Creating filter file: %s' % filter_name)

    def create_dump_outcome(self, start, end, rc):
        return {'timestamp_start': start.strftime("%Y%m%d%H%M%S"),
                'time_start': start.strftime("%H:%M:%S"),
                'time_end': end.strftime("%H:%M:%S"),
                'exit_status': rc}

    def create_filtered_dump_string(self, user_name, dump_date, timestamp_key):
        filter_filename = get_filter_file(self.dump_database, self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.ddboost, self.netbackup_service_host)
        dump_string = self.create_dump_string(user_name, dump_date, timestamp_key)
        dump_string += ' --incremental-filter=%s' % filter_filename
        return dump_string

    def create_dump_and_report_path(self, dump_date):
        dump_path = None
        report_path = None
        if self.backup_dir is not None:
            dump_path = report_path = os.path.join(self.backup_dir, self.dump_dir, dump_date)
        else:
            dump_path = os.path.join(self.dump_dir, dump_date)
            report_path = os.path.join(self.master_datadir, self.dump_dir, dump_date)

        if self.incremental:
            if self.backup_dir:
                report_path = dump_path
            else:
                report_path = os.path.join(self.master_datadir, dump_path)

        return (dump_path, report_path)

    def create_dump_string(self, user_name, dump_date, timestamp_key):
        (dump_path, report_path) = self.create_dump_and_report_path(dump_date)

        dump_line = "gp_dump -p %d -U %s --gp-d=%s --gp-r=%s --gp-s=p --gp-k=%s --no-lock" % (self.master_port, user_name, dump_path, report_path, timestamp_key)
        if self.clear_catalog_dumps:
            dump_line += " -c"
        if self.compress:
            logger.info("Adding compression parameter")
            dump_line += " --gp-c"
        if self.encoding is not None:
            logger.info("Adding encoding %s" % self.encoding)
            dump_line += " --encoding=%s" % self.encoding
        if self.dump_prefix:
            logger.info("Adding --prefix")
            dump_line += " --prefix=%s" % self.dump_prefix

        logger.info('Adding --no-expand-children')
        dump_line += " --no-expand-children"

        """
        AK: Some ridiculous escaping here. I apologize.
        These options get passed-through gp_dump to gp_dump_agent.
        Commented out lines use escaping that would be reasonable, if gp_dump escaped properly.
        """
        if self.dump_schema:
            logger.info("Adding schema name %s" % self.dump_schema)
            dump_line += " -n \"\\\"%s\\\"\"" % self.dump_schema
            #dump_line += " -n \"%s\"" % self.dump_schema
        dump_line += " %s" % self.dump_database
        for dump_table in self.include_dump_tables:
            schema, table = dump_table.split('.')
            dump_line += " --table=\"\\\"%s\\\"\".\"\\\"%s\\\"\"" % (schema, table)
            #dump_line += " --table=\"%s\".\"%s\"" % (schema, table)
        for dump_table in self.exclude_dump_tables:
            schema, table = dump_table.split('.')
            dump_line += " --exclude-table=\"\\\"%s\\\"\".\"\\\"%s\\\"\"" % (schema, table)
            #dump_line += " --exclude-table=\"%s\".\"%s\"" % (schema, table)
        if self.include_dump_tables_file[0] is not None:
            dump_line += " --table-file=%s" % self.include_dump_tables_file
        if self.exclude_dump_tables_file[0] is not None:
            dump_line += " --exclude-table-file=%s" % self.exclude_dump_tables_file
        if self.include_schema_file is not None and not self.dump_prefix:
            dump_line += " --schema-file=%s" % self.include_schema_file
        for opt in self.output_options:
            dump_line += " %s" % opt

        if self.ddboost:
            dump_line += " --ddboost"
        if self.incremental:
            logger.info("Adding --incremental")
            dump_line += " --incremental"
        if self.netbackup_service_host is not None:
            logger.info("Adding NetBackup params")
            dump_line += " --netbackup-service-host=%s --netbackup-policy=%s --netbackup-schedule=%s" % (self.netbackup_service_host, self.netbackup_policy, self.netbackup_schedule)
        if self.netbackup_block_size is not None:
            dump_line += " --netbackup-block-size=%s" % self.netbackup_block_size
        if self.netbackup_keyword is not None:
            dump_line += " --netbackup-keyword=%s" % self.netbackup_keyword

        return dump_line

class CreateIncrementsFile(Operation):

    def __init__(self, dump_database, full_timestamp, timestamp, master_datadir, backup_dir, dump_dir, dump_prefix, ddboost, netbackup_service_host, netbackup_block_size):
        self.full_timestamp = full_timestamp
        self.timestamp = timestamp
        self.master_datadir = master_datadir
        self.backup_dir = backup_dir
        self.increments_filename = generate_increments_filename(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp)
        self.orig_lines_in_file = []
        self.dump_database = dump_database
        self.ddboost = ddboost
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.netbackup_service_host = netbackup_service_host
        self.netbackup_block_size = netbackup_block_size

    def execute(self):
        if os.path.isfile(self.increments_filename):
            CreateIncrementsFile.validate_increments_file(self.dump_database, self.increments_filename, self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix,
                                                          self.ddboost, self.netbackup_service_host, self.netbackup_block_size)
            self.orig_lines_in_file = get_lines_from_file(self.increments_filename)

        with open(self.increments_filename, 'a') as fd:
            fd.write('%s\n' % self.timestamp)

        newlines_in_file = get_lines_from_file(self.increments_filename)

        if len(newlines_in_file) < 1:
            raise Exception("File not written to: %s" % self.increments_filename)

        if newlines_in_file[-1].strip() != self.timestamp:
            raise Exception("Timestamp '%s' not written to: %s" % (self.timestamp, self.increments_filename))

        # remove the last line and the contents should be the same as before
        newlines_in_file = newlines_in_file[0:-1]

        if self.orig_lines_in_file != newlines_in_file:
            raise Exception("trouble adding timestamp '%s' to file '%s'" % (self.timestamp, self.increments_filename))

        return len(newlines_in_file) + 1

    @staticmethod
    def validate_increments_file(dump_database, inc_file_name, master_data_dir, backup_dir, dump_dir, dump_prefix, ddboost=False, netbackup_service_host=None, netbackup_block_size=None):

        tstamps = get_lines_from_file(inc_file_name)
        for ts in tstamps:
            ts = ts.strip()
            if not ts:
                continue
            fn = generate_report_filename(master_data_dir, backup_dir, dump_dir, dump_prefix, ts, ddboost)
            ts_in_rpt = None
            try:
                ts_in_rpt = get_incremental_ts_from_report_file(dump_database, fn, dump_prefix, ddboost, netbackup_service_host, netbackup_block_size)
            except Exception as e:
                logger.error(str(e))

            if not ts_in_rpt:
                raise Exception("Timestamp '%s' from increments file '%s' is not a valid increment" % (ts, inc_file_name))

class PostDumpDatabase(Operation):
    def __init__(self, timestamp_start, compress, backup_dir, batch_default, master_datadir, master_port, dump_dir, dump_prefix, ddboost, netbackup_service_host, incremental=False):
        self.timestamp_start = timestamp_start
        self.compress = compress
        self.backup_dir = backup_dir
        self.batch_default = batch_default
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.incremental = incremental
        self.ddboost = ddboost
        self.netbackup_service_host = netbackup_service_host

    def get_report_dir(self, dump_date):
        path = self.backup_dir if self.backup_dir is not None else self.master_datadir
        path = os.path.join(path, self.dump_dir, dump_date)
        return path

    def execute(self):
        report_dir = self.get_report_dir(DUMP_DATE)
        reports = ListFilesByPattern(report_dir, "*gp_dump_*.rpt").run()
        if not reports:
            logger.error("Could not locate a report file on master.")
            return {'exit_status': 2, 'timestamp': 'n/a'}
        reports.sort(key=lambda x: int(x.split('_')[-1].split('.')[0]))
        reports.reverse()
        report = reports[0]
        timestamp = report[-18:-4]    # last 14 digits, just before .rpt
        if int(timestamp) < int(self.timestamp_start):
            logger.error("Could not locate the newly generated report %s file on master, for timestamp %s (%s)." % (timestamp, self.timestamp_start, reports))
            return {'exit_status': 2, 'timestamp': 'n/a'}
        logger.info("Timestamp key = %s" % timestamp)

        if self.ddboost:
            return {'exit_status': 0,               # feign success with exit_status = 0
                    'timestamp': timestamp}

        if self.netbackup_service_host:
            return {'exit_status': 0,               # feign success with exit_status = 0
                    'timestamp': timestamp}

        # Check master dumps
        path = self.backup_dir if self.backup_dir is not None else self.master_datadir
        path = os.path.join(path, self.dump_dir, DUMP_DATE)
        status_file = os.path.join(path, "%s%s" % (generate_master_status_prefix(self.dump_prefix), timestamp))
        dump_file = os.path.join(path, "%s%s" % (generate_master_dbdump_prefix(self.dump_prefix), timestamp))
        if self.compress: dump_file += ".gz"
        try:
            PostDumpSegment(status_file=status_file,
                            dump_file=dump_file).run()
        except NoStatusFile, e:
            logger.warn('Status file %s not found on master' % status_file)
            return {'exit_status': 1, 'timestamp': timestamp}
        except StatusFileError, e:
            logger.warn('Status file %s on master indicates errors' % status_file)
            return {'exit_status': 1, 'timestamp': timestamp}
        except NoDumpFile, e:
            logger.warn('Dump file %s not found on master' % dump_file)
            return {'exit_status': 1, 'timestamp': timestamp}
        else:
            logger.info('Checked master status file and master dump file.')

        # Perform similar checks for primary segments
        operations = []
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            path = self.backup_dir if self.backup_dir is not None else seg.getSegmentDataDirectory()
            path = os.path.join(path, self.dump_dir, DUMP_DATE)
            status_file = os.path.join(path, "%s%d_%s" % (generate_seg_status_prefix(self.dump_prefix), seg.getSegmentDbId(), timestamp))
            dump_file = os.path.join(path, "%s%d_%s" % (generate_seg_dbdump_prefix(self.dump_prefix), seg.getSegmentDbId(), timestamp))
            if self.compress: dump_file += ".gz"
            operations.append(RemoteOperation(PostDumpSegment(status_file=status_file, dump_file=dump_file),
                                              seg.getSegmentHostName()))

        ParallelOperation(operations, self.batch_default).run()

        success = 0
        for remote in operations:
            host = remote.host
            status_file = remote.operation.status_file
            dump_file = remote.operation.dump_file
            try:
                remote.get_ret()
            except NoStatusFile, e:
                logger.warn('Status file %s not found on %s' % (status_file, host))
            except StatusFileError, e:
                logger.warn('Status file %s on %s indicates errors' % (status_file, host))
            except NoDumpFile, e:
                logger.warn('Dump file %s not found on %s' % (dump_file, host))
            else:
                success += 1

        if success < len(operations):
            logger.warn("Dump was unsuccessful. %d segment(s) failed post-dump checks." % (len(operations) - success))
            return {'exit_status': 1, 'timestamp': timestamp}
        return {'exit_status': 0, 'timestamp': timestamp}

class PostDumpSegment(Operation):
    def __init__(self, status_file, dump_file):
        self.status_file = status_file
        self.dump_file = dump_file

    def execute(self):
        # Ensure that status file exists
        if not CheckFile(self.status_file).run():
            logger.error('Could not locate status file: %s' % self.status_file)
            raise NoStatusFile()
        # Ensure that status file indicates successful dump
        with open(self.status_file, 'r') as f:
            for line in f:
                if line.find("Finished successfully") != -1:
                    break
            else:
                logger.error("Status report file indicates errors: %s" % self.status_file)
                for line in f:
                    logger.info(line)
                logger.error("Status file contents dumped to log file")
                raise StatusFileError()
        # Ensure that dump file exists
        if not os.path.exists(self.dump_file):
            logger.error("Could not locate dump file: %s" % self.dump_file)
            raise NoDumpFile()

class NoStatusFile(Exception): pass
class StatusFileError(Exception): pass
class NoDumpFile(Exception): pass

class ValidateDumpDatabase(Operation):
    def __init__(self, dump_database, dump_schema, include_dump_tables, exclude_dump_tables,
                 include_dump_tables_file, exclude_dump_tables_file, backup_dir,
                 free_space_percent, compress, batch_default, master_datadir, master_port,
                 dump_dir, incremental, include_schema_file):
        self.dump_database = dump_database
        self.dump_schema = dump_schema
        self.include_dump_tables = include_dump_tables
        self.exclude_dump_tables = exclude_dump_tables
        self.include_dump_tables_file = include_dump_tables_file
        self.exclude_dump_tables_file = exclude_dump_tables_file
        self.backup_dir = backup_dir
        self.free_space_percent = free_space_percent
        self.compress = compress
        self.batch_default = batch_default
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.incremental = incremental
        self.include_schema_file = include_schema_file

    def execute(self):
        ValidateDatabaseExists(database = self.dump_database,
                               master_port = self.master_port).run()

        dump_schemas = []
        if self.dump_schema:
            dump_schemas = self.dump_schema
        elif self.include_schema_file is not None:
            dump_schemas = get_lines_from_file(self.include_schema_file)

        for schema in dump_schemas:
            ValidateSchemaExists(database = self.dump_database,
                                 schema = schema,
                                 master_port = self.master_port).run()

        ValidateCluster(master_port = self.master_port).run()

        ValidateAllDumpDirs(backup_dir = self.backup_dir,
                            batch_default = self.batch_default,
                            master_datadir = self.master_datadir,
                            master_port = self.master_port,
                            dump_dir = self.dump_dir).run()

        if not self.incremental:
            self.exclude_dump_tables = ValidateDumpTargets(dump_database = self.dump_database,
                                                           dump_schema = self.dump_schema,
                                                           include_dump_tables = self.include_dump_tables,
                                                           exclude_dump_tables = self.exclude_dump_tables,
                                                           include_dump_tables_file = self.include_dump_tables_file,
                                                           exclude_dump_tables_file = self.exclude_dump_tables_file,
                                                           master_port = self.master_port).run()

        if self.free_space_percent is not None:
            logger.info('Validating disk space')
            ValidateDiskSpace(free_space_percent = self.free_space_percent,
                              compress = self.compress,
                              dump_database = self.dump_database,
                              include_dump_tables = self.include_dump_tables,
                              batch_default = self.batch_default,
                              master_port = self.master_port).run()

        return self.exclude_dump_tables

class ValidateDiskSpace(Operation):
    # TODO: this doesn't take into account that multiple segments may be dumping to the same logical disk.
    def __init__(self, free_space_percent, compress, dump_database, include_dump_tables, batch_default, master_port):
        self.free_space_percent = free_space_percent
        self.compress = compress
        self.dump_database = dump_database
        self.include_dump_tables = include_dump_tables
        self.batch_default = batch_default
        self.master_port = master_port

    def execute(self):
        operations = []
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            operations.append(RemoteOperation(ValidateSegDiskSpace(free_space_percent = self.free_space_percent,
                                                                   compress = self.compress,
                                                                   dump_database = self.dump_database,
                                                                   include_dump_tables = self.include_dump_tables,
                                                                   datadir = seg.getSegmentDataDirectory(),
                                                                   segport = seg.getSegmentPort()),
                                              seg.getSegmentHostName()))

        ParallelOperation(operations, self.batch_default).run()

        success = 0
        for remote in operations:
            host = remote.host
            try:
                remote.get_ret()
            except NotEnoughDiskSpace, e:
                logger.error("%s has insufficient disk space. [Need: %dK, Free %dK]" % (host, e.needed_space, e.free_space))
            else:
                success += 1
        if success < len(operations):
            raise ExceptionNoStackTraceNeeded("Cannot continue. %d segment(s) failed disk space checks" % (len(operations) - success))

class ValidateSegDiskSpace(Operation):
    # TODO: this estimation of needed space needs work. it doesn't include schemas or exclusion tables.
    def __init__(self, free_space_percent, compress, dump_database, include_dump_tables, datadir, segport):
        self.free_space_percent = free_space_percent
        self.compress = compress
        self.dump_database = dump_database
        self.include_dump_tables = include_dump_tables
        self.datadir = datadir
        self.segport = segport

    def execute(self):
        needed_space = 0
        dburl = dbconn.DbURL(dbname=self.dump_database, port=self.segport)
        conn = None
        try:
            conn = dbconn.connect(dburl, utility=True)
            if self.include_dump_tables:
                for dump_table in self.include_dump_tables:
                    needed_space += execSQLForSingleton(conn, "SELECT pg_relation_size('%s')/1024;" % dump_table)
            else:
                needed_space = execSQLForSingleton(conn, "SELECT pg_database_size('%s')/1024;" % self.dump_database)
        finally:
            if conn is not None:
                conn.close()
        if self.compress:
            needed_space = needed_space / COMPRESSION_FACTOR

        # get free available space
        stat_res = os.statvfs(self.datadir)
        free_space = (stat_res.f_bavail * stat_res.f_frsize) / 1024

        if free_space == 0 or (free_space - needed_space) / free_space < self.free_space_percent / 100:
            logger.error("Disk space: [Need: %dK, Free %dK]" % (needed_space, free_space))
            raise NotEnoughDiskSpace(free_space, needed_space)
        logger.info("Disk space: [Need: %dK, Free %dK]" % (needed_space, free_space))

class NotEnoughDiskSpace(Exception):
    def __init__(self, free_space, needed_space):
        self.free_space, self.needed_space = free_space, needed_space
        Exception.__init__(self, free_space, needed_space)

class ValidateGpToolkit(Operation):
    def __init__(self, database, master_port):
        self.database = database
        self.master_port = master_port

    def execute(self):
        dburl = dbconn.DbURL(dbname=self.database, port=self.master_port)
        conn = None
        try:
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_class, pg_namespace where pg_namespace.nspname = 'gp_toolkit' and pg_class.relnamespace = pg_namespace.oid")
        finally:
            if conn is not None:
                conn.close()
        if count > 0:
            logger.debug("gp_toolkit exists within database %s." % self.database)
            return
        logger.info("gp_toolkit not found. Installing...")
        Psql('Installing gp_toolkit',
             filename='$GPHOME/share/postgresql/gp_toolkit.sql',
             database=self.database,
             port=self.master_port).run(validateAfter=True)

class ValidateAllDumpDirs(Operation):
    def __init__(self, backup_dir, batch_default, master_datadir, master_port, dump_dir):
        self.backup_dir = backup_dir
        self.batch_default = batch_default
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.dump_dir = dump_dir

    def execute(self):
        directory = self.backup_dir if self.backup_dir is not None else self.master_datadir
        try:
            ValidateDumpDirs(directory, self.dump_dir).run()
        except DumpDirCreateFailed, e:
            raise ExceptionNoStackTraceNeeded('Could not create %s on master. Cannot continue.' % directory)
        except DumpDirNotWritable, e:
            raise ExceptionNoStackTraceNeeded('Could not write to %s on master. Cannot continue.' % directory)
        else:
            logger.info('Checked %s on master' % directory)

        # Check backup target on segments (either master_datadir or backup_dir, if present)
        operations = []
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            directory = self.backup_dir if self.backup_dir is not None else seg.getSegmentDataDirectory()
            operations.append(RemoteOperation(ValidateDumpDirs(directory, self.dump_dir), seg.getSegmentHostName()))

        ParallelOperation(operations, self.batch_default).run()

        success = 0
        for remote in operations:
            directory = remote.operation.directory
            host = remote.host
            try:
                remote.get_ret()
            except DumpDirCreateFailed, e:
                logger.error("Could not create %s on %s." % (directory, host))
            except DumpDirNotWritable, e:
                logger.error("Could not write to %s on %s." % (directory, host))
            else:
                success += 1

        if success < len(operations):
            raise ExceptionNoStackTraceNeeded("Cannot continue. %d segment(s) failed directory checks" % (len(operations) - success))

class ValidateDumpDirs(Operation):
    def __init__(self, directory, dump_dir):
        self.directory = directory
        self.dump_dir = dump_dir

    def execute(self):
        path = os.path.join(self.directory, self.dump_dir, DUMP_DATE)
        exists = CheckDir(path).run()
        if exists:
            logger.info("Directory %s exists" % path)
        else:
            logger.info("Directory %s not found, will try to create" % path)
            try:
                MakeDir(path).run()
            except OSError, e:
                logger.exception("Could not create directory %s" % path)
                raise DumpDirCreateFailed()
            else:
                logger.info("Created %s" % path)
        try:
            with tempfile.TemporaryFile(dir=path) as f:
                pass
        except Exception, e:
            logger.exception("Cannot write to %s" % path)
            raise DumpDirNotWritable()

class DumpDirCreateFailed(Exception): pass
class DumpDirNotWritable(Exception): pass

class ValidateDumpTargets(Operation):
    def __init__(self, dump_database, dump_schema, include_dump_tables, exclude_dump_tables,
                 include_dump_tables_file, exclude_dump_tables_file, master_port):
        self.dump_database = dump_database
        self.dump_schema = dump_schema
        self.include_dump_tables = include_dump_tables
        self.exclude_dump_tables = exclude_dump_tables
        self.include_dump_tables_file = include_dump_tables_file
        self.exclude_dump_tables_file = exclude_dump_tables_file
        self.master_port = master_port

    def execute(self):
        if ((len(self.include_dump_tables) > 0 or (self.include_dump_tables_file is not None)) and
            (len(self.exclude_dump_tables) > 0 or (self.exclude_dump_tables_file is not None))):
            raise ExceptionNoStackTraceNeeded("Cannot use -t/--table-file and -T/--exclude-table-file options at same time")
        elif len(self.include_dump_tables) > 0 or self.include_dump_tables_file is not None:
            logger.info("Configuring for single-database, include-table dump")
            ValidateIncludeTargets(dump_database = self.dump_database,
                                   dump_schema = self.dump_schema,
                                   include_dump_tables = self.include_dump_tables,
                                   include_dump_tables_file = self.include_dump_tables_file,
                                   master_port = self.master_port).run()
        elif len(self.exclude_dump_tables) > 0 or self.exclude_dump_tables_file is not None:
            logger.info("Configuring for single-database, exclude-table dump")
            self.exclude_dump_tables = ValidateExcludeTargets(dump_database = self.dump_database,
                                                              dump_schema = self.dump_schema,
                                                              exclude_dump_tables = self.exclude_dump_tables,
                                                              exclude_dump_tables_file = self.exclude_dump_tables_file,
                                                              master_port = self.master_port).run()
        else:
            logger.info("Configuring for single database dump")
        return self.exclude_dump_tables

class ValidateIncludeTargets(Operation):
    def __init__(self, dump_database, dump_schema, include_dump_tables, include_dump_tables_file, master_port):
        self.dump_database = dump_database
        self.dump_schema = dump_schema
        self.include_dump_tables = include_dump_tables
        self.include_dump_tables_file = include_dump_tables_file
        self.master_port = master_port

    def execute(self):
        dump_tables = []
        for dump_table in self.include_dump_tables:
            dump_tables.append(dump_table)

        if self.include_dump_tables_file is not None:
            include_file = open(self.include_dump_tables_file, 'rU')
            if not include_file:
                raise ExceptionNoStackTraceNeeded("Can't open file %s" % self.include_dump_tables_file)
            for line in include_file:
                dump_tables.append(line.strip('\n'))
            include_file.close()

        for dump_table in dump_tables:
            if '.' not in dump_table:
                raise ExceptionNoStackTraceNeeded("No schema name supplied for table %s" % dump_table)
            schema, table = dump_table.split('.')
            exists = CheckTableExists(schema = schema,
                                      table = table,
                                      database = self.dump_database,
                                      master_port = self.master_port).run()
            if not exists:
                raise ExceptionNoStackTraceNeeded("Table %s does not exist in %s database" % (dump_table, self.dump_database))
            if self.dump_schema:
                for dump_schema in self.dump_schema:
                    if dump_schema != schema:
                        raise ExceptionNoStackTraceNeeded("Schema name %s not same as schema on %s" % (dump_schema, dump_table))

class ValidateExcludeTargets(Operation):
    def __init__(self, dump_database, dump_schema, exclude_dump_tables, exclude_dump_tables_file, master_port):
        self.dump_database = dump_database
        self.dump_schema = dump_schema
        self.exclude_dump_tables = exclude_dump_tables
        self.exclude_dump_tables_file = exclude_dump_tables_file
        self.master_port = master_port

    def execute(self):
        rebuild_excludes = []

        dump_tables = []
        for dump_table in self.exclude_dump_tables:
            dump_tables.append(dump_table)

        if self.exclude_dump_tables_file is not None:
            exclude_file = open(self.exclude_dump_tables_file, 'rU')
            if not exclude_file:
                raise ExceptionNoStackTraceNeeded("Can't open file %s" % self.exclude_dump_tables_file)
            for line in exclude_file:
                dump_tables.append(line.strip('\n'))
            exclude_file.close()

        for dump_table in dump_tables:
            if '.' not in dump_table:
                raise ExceptionNoStackTraceNeeded("No schema name supplied for exclude table %s" % dump_table)
            schema, table = dump_table.split('.')
            exists = CheckTableExists(schema = schema,
                                      table = table,
                                      database = self.dump_database,
                                      master_port = self.master_port).run()
            if exists:
                if self.dump_schema:
                    for dump_schema in self.dump_schema:
                        if dump_schema != schema:
                            logger.info("Adding table %s to exclude list" % dump_table)
                            rebuild_excludes.append(dump_table)
                        else:
                            logger.warn("Schema dump request and exclude table %s not in that schema, ignoring" % dump_table)
            else:
                logger.warn("Exclude table %s does not exist in %s database, ignoring" % (dump_table, self.dump_database))
        if len(rebuild_excludes) == 0:
            logger.warn("All exclude table names have been removed due to issues, see log file")
        return self.exclude_dump_tables

class ValidateDatabaseExists(Operation):
    """ TODO: move this to gppylib.operations.common? """
    def __init__(self, database, master_port):
        self.master_port = master_port
        self.database = database

    def execute(self):
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.master_port)
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_database where datname='%s';" % self.database)
            if count == 0:
                raise ExceptionNoStackTraceNeeded("Database %s does not exist." % self.database)
        finally:
            if conn is not None:
                conn.close()

class ValidateSchemaExists(Operation):
    """ TODO: move this to gppylib.operations.common? """
    def __init__(self, database, schema, master_port):
        self.database = database
        self.schema = schema
        self.master_port = master_port

    def execute(self):
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.master_port, dbname=self.database)
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_namespace where nspname='%s';" % self.schema)
            if count == 0:
                raise ExceptionNoStackTraceNeeded("Schema %s does not exist in database %s." % (self.schema, self.database))
        finally:
            if conn is not None:
                conn.close()

class CheckTableExists(Operation):
    all_tables = None
    def __init__(self, database, schema, table, master_port):
        self.database = database
        self.schema = schema
        self.table = table
        self.master_port = master_port
        if CheckTableExists.all_tables is None:
            CheckTableExists.all_tables = set()
            for (schema, table) in get_user_table_list(self.master_port, self.database):
                CheckTableExists.all_tables.add((schema, table))

    def execute(self):
        if (self.schema, self.table) in CheckTableExists.all_tables:
            return True
        return False

class ValidateCluster(Operation):
    def __init__(self, master_port):
        self.master_port = master_port

    def execute(self):
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
        failed_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True) and seg.isSegmentDown()]
        if len(failed_segs) != 0:
            logger.warn("Failed primary segment instances detected")
            failed_dbids = [seg.getSegmentDbid() for seg in failed_segs]
            raise ExceptionNoStackTraceNeeded("Detected failed segment(s) with dbid=%s" % ",".join(failed_dbids))

class UpdateHistoryTable(Operation):
    HISTORY_TABLE = "public.gpcrondump_history"
    def __init__(self, dump_database, time_start, time_end, options_list, timestamp, dump_exit_status, pseudo_exit_status, master_port):
        self.dump_database = dump_database
        self.time_start = time_start
        self.time_end = time_end
        self.options_list = options_list
        self.timestamp = timestamp
        self.dump_exit_status = dump_exit_status
        self.pseudo_exit_status = pseudo_exit_status
        self.master_port = master_port

    def execute(self):
        schema, table = UpdateHistoryTable.HISTORY_TABLE.split('.')
        exists = CheckTableExists(database = self.dump_database,
                                  schema = schema,
                                  table = table,
                                  master_port = self.master_port).run()
        if not exists:
            conn = None
            CREATE_HISTORY_TABLE = """ create table %s (rec_date timestamp, start_time char(8), end_time char(8), options text, dump_key varchar(20), dump_exit_status smallint, script_exit_status smallint, exit_text varchar(10)) distributed by (rec_date); """ % UpdateHistoryTable.HISTORY_TABLE
            try:
                dburl = dbconn.DbURL(port=self.master_port, dbname=self.dump_database)
                conn = dbconn.connect(dburl)
                execSQL(conn, CREATE_HISTORY_TABLE)
                conn.commit()
            except Exception, e:
                logger.exception("Unable to create %s in %s database" % (UpdateHistoryTable.HISTORY_TABLE, self.dump_database))
                return
            else:
                logger.info("Created %s in %s database" % (UpdateHistoryTable.HISTORY_TABLE, self.dump_database))
            finally:
                if conn is not None:
                    conn.close()

        translate_rc_to_msg = {0: "COMPLETED", 1: "WARNING", 2: "FATAL"}
        exit_msg = translate_rc_to_msg[self.pseudo_exit_status]
        APPEND_HISTORY_TABLE = """ insert into %s values (now(), '%s', '%s', '%s', '%s', %d, %d, '%s'); """ % (UpdateHistoryTable.HISTORY_TABLE, self.time_start, self.time_end, self.options_list, self.timestamp, self.dump_exit_status, self.pseudo_exit_status, exit_msg)
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.master_port, dbname=self.dump_database)
            conn = dbconn.connect(dburl)
            execSQL(conn, APPEND_HISTORY_TABLE)
            conn.commit()
        except Exception, e:
            logger.exception("Failed to insert record into %s in %s database" % (UpdateHistoryTable.HISTORY_TABLE, self.dump_database))
        else:
            logger.info("Inserted dump record into %s in %s database" % (UpdateHistoryTable.HISTORY_TABLE, self.dump_database))
        finally:
            if conn is not None:
                conn.close()

class DumpGlobal(Operation):
    def __init__(self, timestamp, master_datadir, master_port, backup_dir, dump_dir, dump_prefix, ddboost):
        self.timestamp = timestamp
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.backup_dir = backup_dir
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.ddboost = ddboost
        self.global_filename = generate_global_filename(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, DUMP_DATE, self.timestamp)

    def execute(self):
        logger.info("Commencing pg_catalog dump")
        Command('Dump global objects',
                self.create_pgdump_command_line()).run(validateAfter=True)

        if self.ddboost:
            abspath = self.global_filename
            relpath = os.path.join(self.dump_dir, DUMP_DATE, "%s%s" % (generate_global_prefix(self.dump_prefix), self.timestamp))
            logger.debug('Copying %s to DDBoost' % abspath)
            cmd = Command('DDBoost copy of %s' % abspath,
                          'gpddboost --copyToDDBoost --from-file=%s --to-file=%s' % (abspath, relpath))
            cmd.run(validateAfter=True)

    def create_pgdump_command_line(self):
        return "pg_dumpall -p %s -g --gp-syntax > %s" % (self.master_port, self.global_filename)

class DumpConfig(Operation):
    # TODO: Should we really just give up if one of the tars fails?
    # TODO: WorkerPool
    def __init__(self, backup_dir, master_datadir, master_port, dump_dir, dump_prefix, ddboost):
        self.backup_dir = backup_dir
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.ddboost = ddboost

    def execute(self):
        timestamp = TIMESTAMP_KEY
        config_backup_file = generate_master_config_filename(self.dump_prefix, timestamp)
        if self.backup_dir is not None:
            path = os.path.join(self.backup_dir, 'db_dumps', DUMP_DATE, config_backup_file)
        else:
            path = os.path.join(self.master_datadir, self.dump_dir, DUMP_DATE, config_backup_file)
        logger.info("Dumping master config files")
        Command("Dumping master configuration files",
                "tar cf %s %s/*.conf" % (path, self.master_datadir)).run(validateAfter=True)
        if self.ddboost:
            abspath = path
            relpath = os.path.join(self.dump_dir, DUMP_DATE, config_backup_file)
            logger.debug('Copying %s to DDBoost' % abspath)
            cmd = Command('DDBoost copy of %s' % abspath,
                          'gpddboost --copyToDDBoost --from-file=%s --to-file=%s' % (abspath, relpath))
            cmd.run(validateAfter=True)
            res = cmd.get_results()
            if res.rc != 0:
                logger.error("DDBoost command to copy master config file failed. %s" % res.printResult())
            rc = res.rc

        logger.info("Dumping segment config files")
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
        primaries = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in primaries:
            config_backup_file = generate_segment_config_filename(self.dump_prefix, seg.getSegmentDbId(), timestamp)
            if self.backup_dir is not None:
                path = os.path.join(self.backup_dir, 'db_dumps', DUMP_DATE, config_backup_file)
            else:
                path = os.path.join(seg.getSegmentDataDirectory(), self.dump_dir, DUMP_DATE, config_backup_file)
            host = seg.getSegmentHostName()
            Command("Dumping segment config files",
                    "tar cf %s %s/*.conf" % (path, seg.getSegmentDataDirectory()),
                    ctxt=REMOTE,
                    remoteHost=host).run(validateAfter=True)
            if self.ddboost:
                abspath = path
                relpath = os.path.join(self.dump_dir, DUMP_DATE, config_backup_file)
                logger.debug('Copying %s to DDBoost' % abspath)
                cmd = Command('DDBoost copy of %s' % abspath,
                              'gpddboost --copyToDDBoost --from-file=%s --to-file=%s' % (abspath, relpath),
                              ctxt=REMOTE,
                              remoteHost=host)
                cmd.run(validateAfter=True)
                res = cmd.get_results()
                if res.rc != 0:
                    logger.error("DDBoost command to copy segment config file failed. %s" % res.printResult())
                rc = rc + res.rc
        if self.ddboost:
            return {"exit_status": rc, "timestamp": timestamp}

class DeleteCurrentDump(Operation):
    def __init__(self, timestamp, master_datadir, master_port, dump_dir, ddboost):
        self.timestamp = timestamp
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.ddboost = ddboost

    def execute(self):
        try:
            DeleteCurrentSegDump(self.timestamp, self.master_datadir, self.dump_dir).run()
        except OSError, e:
            logger.warn("Error encountered during deletion of %s on master" % self.timestamp)
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            try:
                RemoteOperation(DeleteCurrentSegDump(self.timestamp, seg.getSegmentDataDirectory(), self.dump_dir),
                                seg.getSegmentHostName()).run()
            except OSError, e:
                logger.warn("Error encountered during deletion of %s on %s" % (self.timestamp, seg.getSegmentHostName()))

        if self.ddboost:
            relpath = os.path.join(self.dump_dir, DUMP_DATE)
            logger.debug('Listing %s on DDBoost to locate dump files with timestamp %s' % (relpath, self.timestamp))
            cmd = Command('DDBoost list dump files',
                          'gpddboost --listDirectory --dir=%s' % relpath)
            cmd.run(validateAfter=True)
            for line in cmd.get_results().stdout.splitlines():
                line = line.strip()
                if self.timestamp in line:
                    abspath = os.path.join(relpath, line)
                    logger.debug('Deleting %s from DDBoost' % abspath)
                    cmd = Command('DDBoost delete of %s' % abspath,
                                  'gpddboost --del-file=%s' % abspath)
                    cmd.run(validateAfter=True)

class DeleteCurrentSegDump(Operation):
    """ TODO: Improve with grouping by host. """
    def __init__(self, timestamp, datadir, dump_dir):
        self.timestamp = timestamp
        self.datadir = datadir
        self.dump_dir = dump_dir

    def execute(self):
        path = os.path.join(self.datadir, self.dump_dir, DUMP_DATE)
        filenames = ListFilesByPattern(path, "*%s*" % self.timestamp).run()
        for filename in filenames:
            RemoveFile(os.path.join(path, filename)).run()

class DeleteOldestDumps(Operation):
    # TODO: This Operation isn't consuming backup_dir. Should it?
    def __init__(self, master_datadir, master_port, dump_dir, ddboost):
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.ddboost = ddboost

    def execute(self):
        dburl = dbconn.DbURL(port=self.master_port)
        if self.ddboost:
            cmd = Command('List directories in DDBoost db_dumps dir',
                          'gpddboost --listDir --dir=%s/ | grep ^[0-9]' % self.dump_dir)
            cmd.run(validateAfter=False)
            rc = cmd.get_results().rc
            if rc != 0:
                logger.info("Cannot find old backup sets to remove on DDboost")
                return
            old_dates = cmd.get_results().stdout.splitlines()
        else:
            old_dates = ListFiles(os.path.join(self.master_datadir, 'db_dumps')).run()
        try:
            old_dates.remove(DUMP_DATE)
        except ValueError:            # DUMP_DATE was not found in old_dates
            pass
        if len(old_dates) == 0:
            logger.info("No old backup sets to remove")
            return
        old_dates.sort()
        old_date = old_dates[0]

        # Remove the directories on DDBoost only. This will avoid the problem
        # where we might accidently end up deleting local backup files, but
        # the intention was to delete only the files on DDboost.
        if self.ddboost:
            logger.info("Preparing to remove dump %s from DDBoost" % old_date)
            cmd = Command('DDBoost cleanup',
                          'gpddboost --del-dir=%s' % os.path.join(self.dump_dir, old_date))
            cmd.run(validateAfter=False)
            rc = cmd.get_results().rc
            if rc != 0:
                logger.info("Error encountered during deletion of %s on DDBoost" % os.path.join(self.dump_dir, old_date))
                logger.debug(cmd.get_results().stdout)
                logger.debug(cmd.get_results().stderr)
        else:
            logger.info("Preparing to remove dump %s from all hosts" % old_date)
            path = os.path.join(self.master_datadir, 'db_dumps', old_date)

            try:
                RemoveTree(path).run()
            except OSError, e:
                logger.warn("Error encountered during deletion of %s" % path)
            gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
            primaries = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
            for seg in primaries:
                path = os.path.join(seg.getSegmentDataDirectory(), 'db_dumps', old_date)
                try:
                    RemoveRemoteTree(path, seg.getSegmentHostName()).run()
                except ExecutionError, e:
                    logger.warn("Error encountered during deletion of %s on %s" % (path, seg.getSegmentHostName()))

        return old_date

class VacuumDatabase(Operation):
    # TODO: move this to gppylib.operations.common?
    def __init__(self, database, master_port):
        self.database = database
        self.master_port = master_port

    def execute(self):
        conn = None
        logger.info('Commencing vacuum of %s database, please wait' % self.database)
        try:
            dburl = dbconn.DbURL(port=self.master_port, dbname=self.database)
            conn = dbconn.connect(dburl)
            cursor = conn.cursor()
            cursor.execute("commit")                          # hack to move drop stmt out of implied transaction
            cursor.execute("vacuum")
            cursor.close()
        except Exception, e:
            logger.exception('Error encountered with vacuum of %s database' % self.database)
        else:
            logger.info('Vacuum of %s completed without error' % self.database)
        finally:
            if conn is not None:
                conn.close()

class MailDumpEvent(Operation):
    def __init__(self, subject, message, sender=None):
        self.subject = subject
        self.message = message
        self.sender = sender or None

    def execute(self):
        if "HOME" not in os.environ or "GPHOME" not in os.environ:
            logger.warn("Could not find mail_contacts file. Set $HOME and $GPHOME.")
            return
        mail_file = os.path.join(os.environ["GPHOME"], "bin", "mail_contacts")
        home_mail_file = os.path.join(os.environ["HOME"], "mail_contacts")
        contacts_file = None
        if CheckFile(home_mail_file).run():
            contacts_file = home_mail_file
        elif CheckFile(mail_file).run():
            contacts_file = mail_file
        else:
            logger.warn("Found neither %s nor %s" % (mail_file, home_mail_file))
            logger.warn("Unable to send dump email notification")
            logger.info("To enable email notification, create %s or %s containing required email addresses" % (mail_file, home_mail_file))
            return
        to_addrs = None
        with open(contacts_file, 'r') as f:
            to_addrs = [line.strip() for line in f]
        MailEvent(subject = self.subject,
                  message = self.message,
                  to_addrs = to_addrs,
                  sender = self.sender).run()

class MailEvent(Operation):
    # TODO: move this to gppylib.operations.common?
    def __init__(self, subject, message, to_addrs, sender=None):
        if isinstance(to_addrs, str):
            to_addrs = [to_addrs]
        self.subject = subject
        self.message = message
        self.to_addrs = to_addrs
        self.sender = sender or None

    def execute(self):
        logger.info("Sending mail to %s" % ",".join(self.to_addrs))
        cmd = "/bin/mailx" if curr_platform == SUNOS else findCmdInPath('mail')
        if self.sender is None:
            command_str = 'echo "%s" | %s -s "%s" %s' % (self.message, cmd, self.subject, " ".join(self.to_addrs))
        else:
            command_str = 'echo "%s" | %s -s "%s" %s -- -f %s' % (self.message, cmd, self.subject, " ".join(self.to_addrs), self.sender)
        logger.debug("Email command string= %s" % command_str)
        Command('Sending email', command_str).run(validateAfter=True)
