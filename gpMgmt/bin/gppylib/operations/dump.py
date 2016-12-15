import shutil
from gppylib.commands.base import ExecutionError
from gppylib.commands.gp import Psql
from gppylib.commands.unix import getUserName, findCmdInPath, curr_platform, SUNOS
from gppylib.db.dbconn import execSQLForSingleton
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.utils import shellEscape
from gppylib.operations import Operation
from gppylib.operations.unix import CheckDir, CheckFile, ListFiles, ListFilesByPattern, MakeDir, RemoveFile, RemoveTree, RemoveRemoteTree
from gppylib.operations.utils import RemoteOperation, ParallelOperation
from gppylib.operations.backup_utils import *

logger = gplog.get_default_logger()

FULL_DUMP_TS_WITH_NBU = None

COMPRESSION_FACTOR = 12                 # TODO: Where did 12 come from?

INJECT_GP_DUMP_FAILURE = None

CATALOG_SCHEMA = [
    'gp_toolkit',
    'information_schema',
    'pg_aoseg',
    'pg_bitmapindex',
    'pg_catalog',
    'pg_toast'
]

IGNORE_SCHEMA = [
    'pg_temp'
]

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

# Generally used to validate that the table(s) we want to backup
GET_ALL_USER_TABLES_SQL = """
SELECT n.nspname AS schemaname, c.relname AS tablename
    FROM pg_class c LEFT JOIN pg_namespace n ON n.oid = c.relnamespace
    LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace
    WHERE c.relkind = 'r'::"char" AND c.oid > 16384 AND (c.relnamespace > 16384 or n.nspname = 'public') AND nspname NOT LIKE 'pg_temp_%'
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

GET_ALL_SCHEMAS_SQL = "SELECT nspname from pg_namespace WHERE nspname NOT LIKE 'pg_temp_%';"

def get_include_schema_list_from_exclude_schema(context, exclude_schema_list):
    """
    If schema name is already double quoted, remove it so comparing with
    schemas returned by database will be correct.
    Don't do strip, that will remove white space inside schema name
    """
    include_schema_list = []
    schema_list = execute_sql(GET_ALL_SCHEMAS_SQL, context.master_port, context.dump_database)
    for schema in schema_list:
        if schema[0] not in exclude_schema_list \
           and schema[0] not in CATALOG_SCHEMA \
           and schema[0] not in IGNORE_SCHEMA:
            include_schema_list.append(schema[0])

    return include_schema_list

def get_ao_partition_state(context):
    ao_partition_info = get_ao_partition_list(context)
    ao_partition_list = get_partition_state(context, 'pg_aoseg', ao_partition_info)
    return ao_partition_list

def get_co_partition_state(context):
    co_partition_info = get_co_partition_list(context)
    co_partition_list = get_partition_state(context, 'pg_aoseg', co_partition_info)
    return co_partition_list

def validate_modcount(schema, tablename, cnt):
    if not cnt:
        return
    if not cnt.isdigit():
        raise Exception("Can not convert modification count for table. Possibly exceeded  backup max tuple count of 1 quadrillion rows per table for: '%s.%s' '%s'" % (schema, tablename, cnt))
    if len(cnt) > 15:
        raise Exception("Exceeded backup max tuple count of 1 quadrillion rows per table for: '%s.%s' '%s'" % (schema, tablename, cnt))

def get_partition_state_tuples(context, catalog_schema, partition_info):
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

        The result is a list of tuples, of the format:
        (schema_schema, partition_name, modcount)
    """
    partition_list = list()

    dburl = dbconn.DbURL(port=context.master_port, dbname=context.dump_database)
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
            partition_list.append((schemaname, partition_name, modcount))

    return partition_list

def get_partition_state(context, catalog_schema, partition_info):
    """
    A legacy version of get_partition_state_tuples() that returns a list of strings
    instead of tuples. Should not be used in new code, because the string
    representation doesn't handle schema or table names with commas.
    """
    tuples = get_partition_state_tuples(context, catalog_schema, partition_info)

    # Don't put space after comma, which can mess up with the space in schema and table name
    return map((lambda x: '%s, %s, %s' % x), tuples)

def get_tables_with_dirty_metadata(context, cur_pgstatoperations):
    last_dump_timestamp = get_last_dump_timestamp(context)
    old_file = context.generate_filename("last_operation", timestamp=last_dump_timestamp)
    if context.netbackup_service_host:
        restore_file_with_nbu(context, path=old_file)
    old_pgstatoperations = get_lines_from_file(old_file)
    old_pgstatoperations_dict = get_pgstatlastoperations_dict(old_pgstatoperations)
    dirty_tables = compare_metadata(old_pgstatoperations_dict, cur_pgstatoperations)
    return dirty_tables

def get_dirty_partition_tables(context, table_type, curr_state_partition_list):
    last_state_partition_list = get_last_state(context, table_type)
    last_state_dict = create_partition_dict(last_state_partition_list)
    curr_state_dict = create_partition_dict(curr_state_partition_list)
    return compare_dict(last_state_dict, curr_state_dict)

def get_last_state(context, table_type):
    last_ts = get_last_dump_timestamp(context)
    last_state_filename = get_filename_from_filetype(context, table_type, last_ts.strip())
    if context.netbackup_service_host is None:
        if not os.path.isfile(last_state_filename):
            raise Exception('%s state file does not exist: %s' % (table_type, last_state_filename))
    else:
        if check_file_dumped_with_nbu(context, path=last_state_filename):
            if not os.path.exists(last_state_filename):
                restore_file_with_nbu(context, path=last_state_filename)

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

def get_last_dump_timestamp(context):
    full_dump_timestamp = get_latest_full_dump_timestamp(context)
    increments_filename = context.generate_filename("increments", timestamp=full_dump_timestamp)
    if context.netbackup_service_host is None:
        if not os.path.isfile(increments_filename):
            return full_dump_timestamp
    else:
        if check_file_dumped_with_nbu(context, path=increments_filename):
            if not os.path.exists(increments_filename):
                logger.debug('Increments file %s was dumped with NBU, so restoring it now...' % increments_filename)
                restore_file_with_nbu(context, "increments")
        else:
            logger.debug('Increments file %s was NOT dumped with NBU, returning full timestamp as last dump timestamp' % increments_filename)
            return full_dump_timestamp

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
        # As of version 4.3.8.0, gpcrondump supports spaces in schema name (field[0]) and table name (field[1])
        # so we explicitly split on "comma followed by space" instead of just commas since we can't strip() the string
        fields = partition.split(', ')
        if len(fields) != 3:
            raise Exception('Invalid state file format %s' % partition)
        if fields[2].startswith(' '):
            fields = [x for x in fields]
        key = '%s.%s' % (fields[0], fields[1])
        table_dict[key] = fields[2]

    return table_dict

def compare_dict(last_dict, curr_dict):
    diffkeys = set()
    for k in curr_dict:
        if k not in last_dict or (curr_dict[k] != last_dict[k]):
            diffkeys.add(k)
    return diffkeys

def get_filename_from_filetype(context, table_type, timestamp=None):
    if not timestamp:
        timestamp = context.timestamp
    if table_type not in ["ao", "co"]:
        raise Exception('Invalid table type %s provided. Supported table types ao/co.' % table_type)
    filename = context.generate_filename(table_type, timestamp=timestamp)

    return filename

def write_state_file(context, table_type, partition_list):
    filename = get_filename_from_filetype(context, table_type, None)

    write_lines_to_file(filename, partition_list)

    if context.ddboost:
        copy_file_to_dd(context, filename)

# return a list of dirty tables
def get_dirty_tables(context, ao_partition_list, co_partition_list, last_operation_data):
    dirty_heap_tables = get_dirty_heap_tables(context)
    dirty_ao_tables = get_dirty_partition_tables(context, 'ao', ao_partition_list)
    dirty_co_tables = get_dirty_partition_tables(context, 'co', co_partition_list)
    dirty_metadata_set = get_tables_with_dirty_metadata(context, last_operation_data)
    logger.info("%s" % list(dirty_heap_tables | dirty_ao_tables | dirty_co_tables | dirty_metadata_set))

    return list(dirty_heap_tables | dirty_ao_tables | dirty_co_tables | dirty_metadata_set)

def get_dirty_heap_tables(context):
    dirty_tables = set()
    qresult = get_heap_partition_list(context)
    for row in qresult:
        if len(row) != 3:
            raise Exception("Heap tables query returned rows with unexpected number of columns %d" % len(row))
        tname = '%s.%s' % (row[1], row[2])
        dirty_tables.add(tname)
    return dirty_tables

def write_dirty_file_to_temp(dirty_tables):
    return create_temp_file_from_list(dirty_tables, 'dirty_backup_list_')

def write_dirty_file(context, dirty_tables, timestamp=None):
    if dirty_tables is None:
        return None
    if not timestamp:
        timestamp = context.timestamp

    dirty_list_file = context.generate_filename("dirty_table", timestamp)
    write_lines_to_file(dirty_list_file, dirty_tables)

    if context.ddboost:
        copy_file_to_dd(context, dirty_list_file)

    return dirty_list_file

def get_heap_partition_list(context):
    return execute_sql(GET_ALL_HEAP_DATATABLES_SQL, context.master_port, context.dump_database)

def get_ao_partition_list(context):
    partition_list = execute_sql(GET_ALL_AO_DATATABLES_SQL, context.master_port, context.dump_database)
    for line in partition_list:
        if len(line) != 4:
            raise Exception('Invalid results from query to get all AO tables: [%s]' % (','.join(line)))

    return partition_list

def get_co_partition_list(context):
    partition_list = execute_sql(GET_ALL_CO_DATATABLES_SQL, context.master_port, context.dump_database)
    for line in partition_list:
        if len(line) != 4:
            raise Exception('Invalid results from query to get all CO tables: [%s]' % (','.join(line)))

    return partition_list

def get_partition_list(master_port, dbname):
    return execute_sql(GET_ALL_DATATABLES_SQL, master_port, dbname)

def get_user_table_list(context):
    return execute_sql(GET_ALL_USER_TABLES_SQL, context.master_port, context.dump_database)

def get_user_table_list_for_schema(context, schema):
    sql = GET_ALL_USER_TABLES_FOR_SCHEMA_SQL % pg.escape_string(schema)
    return execute_sql(sql, context.master_port, context.dump_database)

def get_last_operation_data(context):
    # oid, action, subtype, timestamp
    rows = execute_sql(GET_LAST_OPERATION_SQL, context.master_port, context.dump_database)
    data = []
    for row in rows:
        if len(row) != 6:
            raise Exception("Invalid return from query in get_last_operation_data: % cols" % (len(row)))
        line = "%s,%s,%d,%s,%s,%s" % (row[0], row[1], row[2], row[3], row[4], row[5])
        data.append(line)
    return data

def write_partition_list_file(context, timestamp=None):
    if not timestamp:
        timestamp = context.timestamp
    filter_file = get_filter_file(context)
    partition_list_file_name = context.generate_filename("partition_list", timestamp=timestamp)

    if filter_file:
        shutil.copyfile(filter_file, partition_list_file_name)
    else:
        lines_to_write = get_partition_list(context.master_port, context.dump_database)
        partition_list = []
        for line in lines_to_write:
            if len(line) != 3:
                raise Exception('Invalid results from query to get all tables: [%s]' % (','.join(line)))
            partition_list.append("%s.%s" % (line[1], line[2]))

        write_lines_to_file(partition_list_file_name, partition_list)

    if context.ddboost:
        copy_file_to_dd(context, partition_list_file_name)

def write_last_operation_file(context, rows):
    filename = context.generate_filename("last_operation")
    write_lines_to_file(filename, rows)

    if context.ddboost:
        copy_file_to_dd(context, filename)

def validate_current_timestamp(context, current=None):
    if current is None:
        current = context.timestamp
    latest = get_latest_report_timestamp(context)
    if not latest:
        return
    if latest >= current:
        raise Exception('There is a future dated backup on the system preventing new backups')

def update_filter_file(context):
    filter_filename = get_filter_file(context)
    if context.netbackup_service_host:
        restore_file_with_nbu(context, path=filter_filename)
    filter_tables = get_lines_from_file(filter_filename)
    tables_sql = "SELECT DISTINCT schemaname||'.'||tablename FROM pg_partitions"
    partitions_sql = "SELECT schemaname||'.'||partitiontablename FROM pg_partitions WHERE schemaname||'.'||tablename='%s';"
    table_list = execute_sql(tables_sql, context.master_port, context.dump_database)

    for table in table_list:
        if table[0] in filter_tables:
            partitions_list = execute_sql(partitions_sql % table[0], context.master_port, context.dump_database)
            filter_tables.extend([x[0] for x in partitions_list])

    write_lines_to_file(filter_filename, list(set(filter_tables)))
    if context.netbackup_service_host:
        backup_file_with_nbu(context, path=filter_filename)

def get_filter_file(context):
    if context.netbackup_service_host is None:
        timestamp = get_latest_full_dump_timestamp(context)
    else:
        if FULL_DUMP_TS_WITH_NBU is None:
            timestamp = get_latest_full_ts_with_nbu(context)
        else:
            timestamp = FULL_DUMP_TS_WITH_NBU
        if timestamp is None:
            raise Exception("No full backup timestamp found for given NetBackup server.")
    filter_file = context.generate_filename("filter", timestamp=timestamp)
    if context.netbackup_service_host is None:
        if os.path.isfile(filter_file):
            return filter_file
        return None
    else:
        if check_file_dumped_with_nbu(context, path=filter_file):
            if not os.path.exists(filter_file):
                restore_file_with_nbu(context, path=filter_file)
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

def filter_dirty_tables(context, dirty_tables):
    if context.netbackup_service_host is None:
        timestamp = get_latest_full_dump_timestamp(context)
    else:
        if FULL_DUMP_TS_WITH_NBU is None:
            timestamp = get_latest_full_ts_with_nbu(context)
        else:
            timestamp = FULL_DUMP_TS_WITH_NBU

    schema_filename = context.generate_filename("schema", timestamp=timestamp)
    filter_file = get_filter_file(context)
    if filter_file:
        tables_to_filter = get_lines_from_file(filter_file)
        dirty_copy = dirty_tables[:]
        for table in dirty_copy:
            if table not in tables_to_filter:
                if os.path.exists(schema_filename):
                    schemas_to_filter = get_lines_from_file(schema_filename)
                    table_schema = split_fqn(table)[0]
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

def backup_state_files_with_nbu(context):
    logger.debug("Inside backup_state_files_with_nbu\n")

    backup_file_with_nbu(context, "ao")
    backup_file_with_nbu(context, "co")
    backup_file_with_nbu(context, "last_operation")

def backup_config_files_with_nbu(context):
    backup_file_with_nbu(context, "master_config")

    #backing up segment config files
    gparray = GpArray.initFromCatalog(dbconn.DbURL(port=context.master_port), utility=True)
    segments = gparray.getSegmentList()
    for segment in segments:
        seg = segment.get_active_primary()
        context.master_datadir = seg.getSegmentDataDirectory()
        host = seg.getSegmentHostName()
        backup_file_with_nbu(context, "segment_config", dbid=segment.get_primary_dbid(), hostname=host)

class DumpDatabase(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        self.context.exclude_dump_tables = ValidateDumpDatabase(self.context).run()

        # create filter file based on the include_dump_tables_file before we do formating of contents
        if self.context.dump_prefix and not self.context.incremental:
            self.create_filter_file()

        # Format sql strings for all schema and table names
        self.context.include_dump_tables_file = formatSQLString(rel_file=self.context.include_dump_tables_file, isTableName=True)
        self.context.exclude_dump_tables_file = formatSQLString(rel_file=self.context.exclude_dump_tables_file, isTableName=True)
        self.context.schema_file = formatSQLString(rel_file=self.context.schema_file, isTableName=False)

        if self.context.incremental and self.context.dump_prefix and get_filter_file(self.context):
            filtered_dump_line = self.create_filtered_dump_string()
            (start, end, rc) = self.perform_dump('Dump process', filtered_dump_line)
        else:
            dump_line = self.create_dump_string()
            (start, end, rc) = self.perform_dump('Dump process', dump_line)
        return self.create_dump_outcome(start, end, rc)

    def perform_dump(self, title, dump_line):
        logger.info("%s command line %s" % (title, dump_line))
        logger.info("Starting %s" % title)
        start = self.context.timestamp_object
        cmd = Command('Invoking gp_dump', dump_line)
        cmd.run()
        rc = cmd.get_results().rc
        if INJECT_GP_DUMP_FAILURE:
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
        filter_name = self.context.generate_filename("filter")
        if self.context.include_dump_tables_file:
            shutil.copyfile(self.context.include_dump_tables_file, filter_name)
            if self.context.netbackup_service_host:
                backup_file_with_nbu(self.context, path=filter_name)
        elif self.context.exclude_dump_tables_file:
            filters = get_lines_from_file(self.context.exclude_dump_tables_file)
            partitions = get_user_table_list(self.context)
            tables = []
            for p in partitions:
                tablename = '%s.%s' % (p[0], p[1])
                if tablename not in filters:
                    tables.append(tablename)
            write_lines_to_file(filter_name, tables)
            if self.context.netbackup_service_host:
                backup_file_with_nbu(self.context, path=filter_name)
        logger.info('Creating filter file: %s' % filter_name)

    def create_dump_outcome(self, start, end, rc):
        return {'timestamp_start': start.strftime("%Y%m%d%H%M%S"),
                'time_start': start.strftime("%H:%M:%S"),
                'time_end': end.strftime("%H:%M:%S"),
                'exit_status': rc}

    def create_filtered_dump_string(self):
        filter_filename = get_filter_file(self.context)
        dump_string = self.create_dump_string()
        dump_string += ' --incremental-filter=%s' % filter_filename
        return dump_string

    def create_dump_and_report_path(self, context):
        dump_path = None
        report_path = None
        if context.backup_dir:
            dump_path = report_path = context.get_backup_dir()
        else:
            dump_path = context.get_gpd_path()
            report_path = context.get_backup_dir()
            if not context.incremental:
                report_path = context.get_backup_dir()
        return (dump_path, report_path)

    def create_dump_string(self):
        (dump_path, report_path) = self.create_dump_and_report_path(self.context)

        dump_line = "gp_dump -p %d -U %s --gp-d=%s --gp-r=%s --gp-s=p --gp-k=%s --no-lock" % (self.context.master_port, getUserName(), dump_path, report_path, self.context.timestamp)
        if self.context.clear_catalog_dumps:
            dump_line += " -c"
        if self.context.compress:
            logger.info("Adding compression parameter")
            dump_line += " --gp-c"
        if self.context.encoding:
            logger.info("Adding encoding %s" % self.context.encoding)
            dump_line += " --encoding=%s" % self.context.encoding
        if self.context.dump_prefix:
            logger.info("Adding --prefix")
            dump_line += " --prefix=%s" % self.context.dump_prefix

        logger.info('Adding --no-expand-children')
        dump_line += " --no-expand-children"

        """
        AK: Some ridiculous escaping here. I apologize.
        These options get passed-through gp_dump to gp_dump_agent.
        Commented out lines use escaping that would be reasonable, if gp_dump escaped properly.
        """
        if self.context.dump_schema:
            logger.info("Adding schema name %s" % self.context.dump_schema)
            dump_line += " -n \"\\\"%s\\\"\"" % self.context.dump_schema
            #dump_line += " -n \"%s\"" % self.context.dump_schema
        db_name = shellEscape(self.context.dump_database)
        dump_line += " %s" % checkAndAddEnclosingDoubleQuote(db_name)
        for dump_table in self.context.include_dump_tables:
            schema, table = dump_table.split('.')
            dump_line += " --table=\"\\\"%s\\\"\".\"\\\"%s\\\"\"" % (schema, table)
            #dump_line += " --table=\"%s\".\"%s\"" % (schema, table)
        for dump_table in self.context.exclude_dump_tables:
            schema, table = dump_table.split('.')
            dump_line += " --exclude-table=\"\\\"%s\\\"\".\"\\\"%s\\\"\"" % (schema, table)
            #dump_line += " --exclude-table=\"%s\".\"%s\"" % (schema, table)
        if self.context.include_dump_tables_file and not self.context.schema_file:
            dump_line += " --table-file=%s" % self.context.include_dump_tables_file
        if self.context.exclude_dump_tables_file:
            dump_line += " --exclude-table-file=%s" % self.context.exclude_dump_tables_file
        if self.context.schema_file:
            dump_line += " --schema-file=%s" % self.context.schema_file
        for opt in self.context.output_options:
            dump_line += " %s" % opt

        if self.context.ddboost:
            dump_line += " --ddboost"
        if self.context.ddboost_storage_unit:
            dump_line += " --ddboost-storage-unit=%s" % self.context.ddboost_storage_unit
        if self.context.incremental:
            logger.info("Adding --incremental")
            dump_line += " --incremental"
        if self.context.netbackup_service_host:
            logger.info("Adding NetBackup params")
            dump_line += " --netbackup-service-host=%s --netbackup-policy=%s --netbackup-schedule=%s" % (self.context.netbackup_service_host, self.context.netbackup_policy, self.context.netbackup_schedule)
        if self.context.netbackup_block_size:
            dump_line += " --netbackup-block-size=%s" % self.context.netbackup_block_size
        if self.context.netbackup_keyword:
            dump_line += " --netbackup-keyword=%s" % self.context.netbackup_keyword

        return dump_line

class CreateIncrementsFile(Operation):

    def __init__(self, context):
        self.orig_lines_in_file = []
        full_dump_timestamp = get_latest_full_dump_timestamp(context)
        self.increments_filename = context.generate_filename("increments", timestamp=full_dump_timestamp)
        self.context = context

    def execute(self):
        if os.path.isfile(self.increments_filename):
            CreateIncrementsFile.validate_increments_file(self.context, self.increments_filename)
            self.orig_lines_in_file = get_lines_from_file(self.increments_filename)

        with open(self.increments_filename, 'a+') as fd:
            fd.write('%s\n' % self.context.timestamp)

        newlines_in_file = get_lines_from_file(self.increments_filename)

        if len(newlines_in_file) < 1:
            raise Exception("File not written to: %s" % self.increments_filename)

        if newlines_in_file[-1].strip() != self.context.timestamp:
            raise Exception("Timestamp '%s' not written to: %s" % (self.context.timestamp, self.increments_filename))

        # remove the last line and the contents should be the same as before
        newlines_in_file = newlines_in_file[0:-1]

        if self.orig_lines_in_file != newlines_in_file:
            raise Exception("trouble adding timestamp '%s' to file '%s'" % (self.context.timestamp, self.increments_filename))

        return len(newlines_in_file) + 1

    @staticmethod
    def validate_increments_file(context, inc_file_name):

        tstamps = get_lines_from_file(inc_file_name)
        for ts in tstamps:
            ts = ts.strip()
            if not ts:
                continue
            fn = context.generate_filename("report", ts)
            ts_in_rpt = None
            try:
                ts_in_rpt = get_incremental_ts_from_report_file(context, fn)
            except Exception as e:
                logger.error(str(e))

            if not ts_in_rpt:
                raise Exception("Timestamp '%s' from increments file '%s' is not a valid increment" % (ts, inc_file_name))

class PostDumpDatabase(Operation):
    def __init__(self, context, timestamp_start):
        self.context = context
        self.timestamp_start = timestamp_start

    def execute(self):
        report_dir = self.context.get_backup_dir()
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

        if self.context.ddboost:
            return {'exit_status': 0,               # feign success with exit_status = 0
                    'timestamp': timestamp}

        if self.context.netbackup_service_host:
            return {'exit_status': 0,               # feign success with exit_status = 0
                    'timestamp': timestamp}

        # Check master dumps
        status_file = self.context.generate_filename("status", timestamp=timestamp)
        dump_file = self.context.generate_filename("metadata", timestamp=timestamp)
        try:
            PostDumpSegment(self.context, status_file, dump_file).run()
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
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.context.master_port), utility=True)
        segments = gparray.getSegmentList()
        mdd = self.context.master_datadir
        for segment in segments:
            seg = segment.get_active_primary()
            self.context.master_datadir = seg.getSegmentDataDirectory()
            # If we're backing up the mirror of a failed-over primary, use the primary's dbid, not the mirror's
            status_file = self.context.generate_filename("status", segment.get_primary_dbid(), timestamp)
            dump_file = self.context.generate_filename("dump", segment.get_primary_dbid(), timestamp)
            operations.append(RemoteOperation(PostDumpSegment(self.context, status_file, dump_file), seg.getSegmentHostName()))
        self.context.master_datadir = mdd

        ParallelOperation(operations, self.context.batch_default).run()

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
    def __init__(self, context, status_file, dump_file):
        self.context = context
        self.status_file = status_file
        self.dump_file = dump_file

    def execute(self):
        # Ensure that status file exists
        if not CheckFile(self.status_file).run():
            logger.error('Could not locate status file: %s' % self.status_file)
            raise NoStatusFile()
        # Ensure that status file indicates successful dump
        completed = False
        error_found = False
        with open(self.status_file, 'r') as f:
            for line in f:
                if line.find("Finished successfully") != -1:
                    completed = True
                elif line.find("ERROR:") != -1 or line.find("[ERROR]") != -1:
                    error_found = True
        if error_found:
            if completed:
                logger.warn("Status report file indicates dump completed with errors: %s" % self.status_file)
            else:
                logger.error("Status report file indicates dump incomplete with errors: %s" % self.status_file)
            with open(self.status_file, 'r') as f:
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
    def __init__(self, context):
        self.context = context

    def execute(self):
        ValidateDatabaseExists(self.context).run()

        dump_schemas = []
        if self.context.dump_schema:
            dump_schemas = self.context.dump_schema
        elif self.context.schema_file:
            dump_schemas = get_lines_from_file(self.context.schema_file)

        for schema in dump_schemas:
            ValidateSchemaExists(self.context, schema).run()

        ValidateCluster(self.context).run()

        ValidateAllDumpDirs(self.context).run()

        if not self.context.incremental:
            self.context.exclude_dump_tables = ValidateDumpTargets(self.context).run()

        if self.context.free_space_percent:
            logger.info('Validating disk space')
            ValidateDiskSpace(self.context).run()

        return self.context.exclude_dump_tables

class ValidateDiskSpace(Operation):
    # TODO: this doesn't take into account that multiple segments may be dumping to the same logical disk.
    def __init__(self, context):
        self.context = context

    def execute(self):
        operations = []
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.context.master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            operations.append(RemoteOperation(ValidateSegDiskSpace(self.context,
                                                                   datadir = seg.getSegmentDataDirectory(),
                                                                   segport = seg.getSegmentPort()),
                                              seg.getSegmentHostName()))

        ParallelOperation(operations, self.context.batch_default).run()

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
    def __init__(self, context, datadir, segport):
        self.context = context
        self.datadir = datadir
        self.segport = segport

    def execute(self):
        needed_space = 0
        dburl = dbconn.DbURL(dbname=self.context.dump_database, port=self.segport)
        conn = None
        try:
            conn = dbconn.connect(dburl, utility=True)
            if self.context.include_dump_tables:
                for dump_table in self.context.include_dump_tables:
                    needed_space += execSQLForSingleton(conn, "SELECT pg_relation_size('%s')/1024;" % pg.escape_string(dump_table))
            else:
                needed_space = execSQLForSingleton(conn, "SELECT pg_database_size('%s')/1024;" % pg.escape_string(self.context.dump_database))
        finally:
            if conn:
                conn.close()
        if self.context.compress:
            needed_space = needed_space / COMPRESSION_FACTOR

        # get free available space
        stat_res = os.statvfs(self.datadir)
        free_space = (stat_res.f_bavail * stat_res.f_frsize) / 1024

        if free_space == 0 or (free_space - needed_space) / free_space < self.context.free_space_percent / 100:
            logger.error("Disk space: [Need: %dK, Free %dK]" % (needed_space, free_space))
            raise NotEnoughDiskSpace(free_space, needed_space)
        logger.info("Disk space: [Need: %dK, Free %dK]" % (needed_space, free_space))

class NotEnoughDiskSpace(Exception):
    def __init__(self, free_space, needed_space):
        self.free_space, self.needed_space = free_space, needed_space
        Exception.__init__(self, free_space, needed_space)

class ValidateGpToolkit(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        dburl = dbconn.DbURL(dbname=self.context.database, port=self.context.master_port)
        conn = None
        try:
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_class, pg_namespace where pg_namespace.nspname = 'gp_toolkit' and pg_class.relnamespace = pg_namespace.oid")
        finally:
            if conn:
                conn.close()
        if count > 0:
            logger.debug("gp_toolkit exists within database %s." % self.context.database)
            return
        logger.info("gp_toolkit not found. Installing...")
        Psql('Installing gp_toolkit',
             filename='$GPHOME/share/postgresql/gp_toolkit.sql',
             database=self.context.dump_database,
             port=self.context.master_port).run(validateAfter=True)

class ValidateAllDumpDirs(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        directory = self.context.backup_dir if self.context.backup_dir else self.context.master_datadir
        try:
            ValidateDumpDirs(directory, self.context).run()
        except DumpDirCreateFailed, e:
            raise ExceptionNoStackTraceNeeded('Could not create %s on master. Cannot continue.' % directory)
        except DumpDirNotWritable, e:
            raise ExceptionNoStackTraceNeeded('Could not write to %s on master. Cannot continue.' % directory)
        else:
            logger.info('Checked %s on master' % directory)

        # Check backup target on segments (either master_datadir or backup_dir, if present)
        operations = []
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.context.master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            directory = self.context.backup_dir if self.context.backup_dir else seg.getSegmentDataDirectory()
            operations.append(RemoteOperation(ValidateDumpDirs(directory, self.context), seg.getSegmentHostName()))

        ParallelOperation(operations, self.context.batch_default).run()

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
    def __init__(self, directory, context):
        self.directory = directory
        self.context = context

    def execute(self):
        path = os.path.join(self.directory, self.context.dump_dir, self.context.db_date_dir)
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
    def __init__(self, context):
        self.context = context

    def execute(self):
        if ((len(self.context.include_dump_tables) > 0 or (self.context.include_dump_tables_file)) and
            (len(self.context.exclude_dump_tables) > 0 or (self.context.exclude_dump_tables_file))):
            raise ExceptionNoStackTraceNeeded("Cannot use -t/--table-file and -T/--exclude-table-file options at same time")
        elif len(self.context.include_dump_tables) > 0 or self.context.include_dump_tables_file:
            logger.info("Configuring for single-database, include-table dump")
            ValidateIncludeTargets(self.context).run()
        elif len(self.context.exclude_dump_tables) > 0 or self.context.exclude_dump_tables_file:
            logger.info("Configuring for single-database, exclude-table dump")
            self.context.exclude_dump_tables = ValidateExcludeTargets(self.context).run()
        else:
            logger.info("Configuring for single database dump")
        return self.context.exclude_dump_tables

class ValidateIncludeTargets(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        dump_tables = []
        for dump_table in self.context.include_dump_tables:
            dump_tables.append(dump_table)

        if self.context.include_dump_tables_file:
            include_file = open(self.context.include_dump_tables_file, 'rU')
            if not include_file:
                raise ExceptionNoStackTraceNeeded("Can't open file %s" % self.context.include_dump_tables_file)
            for line in include_file:
                dump_tables.append(line.strip('\n'))
            include_file.close()

        for dump_table in dump_tables:
            if '.' not in dump_table:
                raise ExceptionNoStackTraceNeeded("No schema name supplied for table %s" % dump_table)
            if dump_table.startswith('pg_temp_'):
                continue
            schema, table = split_fqn(dump_table)
            exists = CheckTableExists(self.context, schema, table).run()
            if not exists:
                raise ExceptionNoStackTraceNeeded("Table %s does not exist in %s database" % (dump_table, self.context.dump_database))
            if self.context.dump_schema:
                for dump_schema in self.context.dump_schema:
                    if dump_schema != schema:
                        raise ExceptionNoStackTraceNeeded("Schema name %s not same as schema on %s" % (dump_schema, dump_table))

class ValidateExcludeTargets(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        rebuild_excludes = []

        dump_tables = []
        for dump_table in self.context.exclude_dump_tables:
            dump_tables.append(dump_table)

        if self.context.exclude_dump_tables_file:
            exclude_file = open(self.context.exclude_dump_tables_file, 'rU')
            if not exclude_file:
                raise ExceptionNoStackTraceNeeded("Can't open file %s" % self.context.exclude_dump_tables_file)
            for line in exclude_file:
                dump_tables.append(line.strip('\n'))
            exclude_file.close()

        for dump_table in dump_tables:
            if '.' not in dump_table:
                raise ExceptionNoStackTraceNeeded("No schema name supplied for exclude table %s" % dump_table)
            schema, table = split_fqn(dump_table)
            exists = CheckTableExists(self.context, schema, table).run()
            if exists:
                if self.context.dump_schema:
                    for dump_schema in self.context.dump_schema:
                        if dump_schema != schema:
                            logger.info("Adding table %s to exclude list" % dump_table)
                            rebuild_excludes.append(dump_table)
                        else:
                            logger.warn("Schema dump request and exclude table %s in that schema, ignoring" % dump_table)
            else:
                logger.warn("Exclude table %s does not exist in %s database, ignoring" % (dump_table, self.context.dump_database))
        if len(rebuild_excludes) == 0:
            logger.warn("All exclude table names have been removed due to issues, see log file")
        return self.context.exclude_dump_tables

class ValidateDatabaseExists(Operation):
    """ TODO: move this to gppylib.operations.common? """
    def __init__(self, context):
        self.context = context

    def execute(self):
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.context.master_port)
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_database where datname='%s';" % pg.escape_string(self.context.dump_database))
            if count == 0:
                raise ExceptionNoStackTraceNeeded("Database %s does not exist." % self.context.dump_database)
        finally:
            if conn:
                conn.close()

class ValidateSchemaExists(Operation):
    """ TODO: move this to gppylib.operations.common? """
    def __init__(self, context, schema):
        self.context = context
        self.schema = schema

    def execute(self):
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.context.master_port, dbname=self.context.dump_database)
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_namespace where nspname='%s';" % pg.escape_string(self.schema))
            if count == 0:
                raise ExceptionNoStackTraceNeeded("Schema %s does not exist in database %s." % (self.schema, self.context.dump_database))
        finally:
            if conn:
                conn.close()

class CheckTableExists(Operation):
    all_tables = None
    def __init__(self, context, schema, table):
        self.schema = schema
        self.table = table
        if CheckTableExists.all_tables is None:
            CheckTableExists.all_tables = set()
            for (schema, table) in get_user_table_list(context):
                CheckTableExists.all_tables.add((schema, table))

    def execute(self):
        if (self.schema, self.table) in CheckTableExists.all_tables:
            return True
        return False


class ValidateCluster(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.context.master_port), utility=True)
        failed_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True) and seg.isSegmentDown()]
        if len(failed_segs) != 0:
            logger.warn("Failed primary segment instances detected")
            failed_dbids = [seg.getSegmentDbid() for seg in failed_segs]
            raise ExceptionNoStackTraceNeeded("Detected failed segment(s) with dbid=%s" % ",".join(failed_dbids))

class UpdateHistoryTable(Operation):
    HISTORY_TABLE = "public.gpcrondump_history"
    def __init__(self, context, time_start, time_end, options_list, timestamp, dump_exit_status, pseudo_exit_status):
        self.context = context
        self.time_start = time_start
        self.time_end = time_end
        self.options_list = options_list
        self.timestamp = timestamp
        self.dump_exit_status = dump_exit_status
        self.pseudo_exit_status = pseudo_exit_status

    def execute(self):
        schema, table = split_fqn(UpdateHistoryTable.HISTORY_TABLE)
        exists = CheckTableExists(self.context, schema, table).run()
        if not exists:
            conn = None
            CREATE_HISTORY_TABLE = """ create table %s (rec_date timestamp, start_time char(8), end_time char(8), options text, dump_key varchar(20), dump_exit_status smallint, script_exit_status smallint, exit_text varchar(10)) distributed by (rec_date); """ % UpdateHistoryTable.HISTORY_TABLE
            try:
                dburl = dbconn.DbURL(port=self.context.master_port, dbname=self.context.dump_database)
                conn = dbconn.connect(dburl)
                execSQL(conn, CREATE_HISTORY_TABLE)
                conn.commit()
            except Exception, e:
                logger.exception("Unable to create %s in %s database" % (UpdateHistoryTable.HISTORY_TABLE, self.context.dump_database))
                return
            else:
                logger.info("Created %s in %s database" % (UpdateHistoryTable.HISTORY_TABLE, self.context.dump_database))
            finally:
                if conn:
                    conn.close()

        translate_rc_to_msg = {0: "COMPLETED", 1: "WARNING", 2: "FATAL"}
        exit_msg = translate_rc_to_msg[self.pseudo_exit_status]
        APPEND_HISTORY_TABLE = """ insert into %s values (now(), '%s', '%s', '%s', '%s', %d, %d, '%s'); """ % (UpdateHistoryTable.HISTORY_TABLE, self.time_start, self.time_end, self.options_list, self.timestamp, self.dump_exit_status, self.pseudo_exit_status, exit_msg)
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.context.master_port, dbname=self.context.dump_database)
            conn = dbconn.connect(dburl)
            execSQL(conn, APPEND_HISTORY_TABLE)
            conn.commit()
        except Exception, e:
            logger.exception("Failed to insert record into %s in %s database" % (UpdateHistoryTable.HISTORY_TABLE, self.context.dump_database))
        else:
            logger.info("Inserted dump record into %s in %s database" % (UpdateHistoryTable.HISTORY_TABLE, self.context.dump_database))
        finally:
            if conn:
                conn.close()

class DumpGlobal(Operation):
    def __init__(self, context, timestamp=None):
        self.context = context
        self.timestamp = timestamp

    def execute(self):
        logger.info("Commencing pg_catalog dump")
        Command('Dump global objects',
                self.create_pgdump_command_line()).run(validateAfter=True)

        if self.context.ddboost:
            global_backup_file = self.context.generate_filename("global", timestamp=self.timestamp)
            abspath = global_backup_file
            relpath = global_backup_file[global_backup_file.index(self.context.dump_dir):]
            logger.debug('Copying %s to DDBoost' % abspath)
            cmdStr = 'gpddboost --copyToDDBoost --from-file=%s --to-file=%s' % (abspath, relpath)
            if self.context.ddboost_storage_unit:
                cmdStr += ' --ddboost-storage-unit=%s' % self.context.ddboost_storage_unit
            cmd = Command('DDBoost copy of %s' % abspath, cmdStr)
            cmd.run(validateAfter=True)

    def create_pgdump_command_line(self):
        return "pg_dumpall -p %s -g --gp-syntax > %s" % (self.context.master_port, self.context.generate_filename("global", self.timestamp))

class DumpConfig(Operation):
    # TODO: Should we really just give up if one of the tars fails?
    # TODO: WorkerPool
    def __init__(self, context):
        self.context = context

    def execute(self):
        config_backup_file = self.context.generate_filename("master_config")
        logger.info("Dumping master config files")
        Command("Dumping master configuration files",
                "tar cf %s %s/*.conf" % (config_backup_file, self.context.master_datadir)).run(validateAfter=True)
        if self.context.ddboost:
            abspath = config_backup_file
            relpath = config_backup_file[config_backup_file.index(self.context.dump_dir):]
            logger.debug('Copying %s to DDBoost' % abspath)
            cmdStr = 'gpddboost --copyToDDBoost --from-file=%s --to-file=%s' % (abspath, relpath)
            if self.context.ddboost_storage_unit:
                cmdStr += ' --ddboost-storage-unit=%s' % self.context.ddboost_storage_unit
            cmd = Command('DDBoost copy of %s' % abspath, cmdStr)
            cmd.run(validateAfter=True)
            res = cmd.get_results()
            if res.rc != 0:
                logger.error("DDBoost command to copy master config file failed. %s" % res.printResult())
            rc = res.rc

        logger.info("Dumping segment config files")
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.context.master_port), utility=True)
        segments = gparray.getSegmentList()
        mdd = self.context.master_datadir
        for segment in segments:
            seg = segment.get_active_primary()
            self.context.master_datadir = seg.getSegmentDataDirectory()
            config_backup_file = self.context.generate_filename("segment_config", dbid=segment.get_primary_dbid())
            host = seg.getSegmentHostName()
            Command("Dumping segment config files",
                    "tar cf %s %s/*.conf" % (config_backup_file, seg.getSegmentDataDirectory()),
                    ctxt=REMOTE,
                    remoteHost=host).run(validateAfter=True)
            if self.context.ddboost:
                abspath = config_backup_file
                relpath = config_backup_file[config_backup_file.index(self.context.dump_dir):]
                logger.debug('Copying %s to DDBoost' % abspath)
                cmdStr = 'gpddboost --copyToDDBoost --from-file=%s --to-file=%s' % (abspath, relpath)
                if self.context.ddboost_storage_unit:
                    cmdStr += ' --ddboost-storage-unit=%s' % self.context.ddboost_storage_unit
                cmd = Command('DDBoost copy of %s' % abspath,
                              cmdStr,
                              ctxt=REMOTE,
                              remoteHost=host)
                cmd.run(validateAfter=True)
                res = cmd.get_results()
                if res.rc != 0:
                    logger.error("DDBoost command to copy segment config file failed. %s" % res.printResult())
                rc = rc + res.rc
        self.context.master_datadir = mdd
        if self.context.ddboost:
            return {"exit_status": rc, "timestamp": self.context.timestamp}

class DeleteCurrentDump(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        try:
            DeleteCurrentSegDump(self.context, self.context.master_datadir).run()
        except OSError, e:
            logger.warn("Error encountered during deletion of %s on master" % self.context.timestamp)
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.context.master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            try:
                RemoteOperation(DeleteCurrentSegDump(self.context.timestamp, seg.getSegmentDataDirectory()),
                                seg.getSegmentHostName()).run()
            except OSError, e:
                logger.warn("Error encountered during deletion of %s on %s" % (self.context.timestamp, seg.getSegmentHostName()))

        if self.context.ddboost:
            relpath = os.path.join(self.context.dump_dir, self.context.db_date_dir)
            logger.debug('Listing %s on DDBoost to locate dump files with timestamp %s' % (relpath, self.context.timestamp))
            cmdStr = 'gpddboost --listDirectory --dir=%s' % relpath
            if self.context.ddboost_storage_unit:
                cmdStr += ' --ddboost-storage-unit=%s' % self.context.ddboost_storage_unit
            cmd = Command('DDBoost list dump files', cmdStr)
            cmd.run(validateAfter=True)
            for line in cmd.get_results().stdout.splitlines():
                line = line.strip()
                if self.context.timestamp in line:
                    abspath = os.path.join(relpath, line)
                    logger.debug('Deleting %s from DDBoost' % abspath)
                    cmdStr = 'gpddboost --del-file=%s' % abspath
                    if self.context.ddboost_storage_unit:
                        cmdStr += ' --ddboost-storage-unit=%s' % self.context.ddboost_storage_unit
                    cmd = Command('DDBoost delete of %s' % abspath, cmdStr)
                    cmd.run(validateAfter=True)

class DeleteCurrentSegDump(Operation):
    """ TODO: Improve with grouping by host. """
    def __init__(self, context, datadir):
        self.context = context
        self.datadir = datadir

    def execute(self):
        path = os.path.join(self.datadir, self.context.dump_dir, self.context.db_date_dir)
        filenames = ListFilesByPattern(path, "*%s*" % self.context.timestamp).run()
        for filename in filenames:
            RemoveFile(os.path.join(path, filename)).run()

class DeleteOldestDumps(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        dburl = dbconn.DbURL(port=self.context.master_port)
        if self.context.ddboost:
            cmdStr = 'gpddboost'
            if self.context.ddboost_storage_unit:
                cmdStr += ' --ddboost-storage-unit %s' % self.context.ddboost_storage_unit
            cmdStr += ' --listDir --dir=%s/ | grep ^[0-9] ' % self.context.dump_dir
            cmd = Command('List directories in DDBoost db_dumps dir', cmdStr)
            cmd.run(validateAfter=False)
            rc = cmd.get_results().rc
            if rc != 0:
                logger.info("Cannot find old backup sets to remove on DDboost")
                return
            old_dates = cmd.get_results().stdout.splitlines()
        else:
            old_dates = ListFiles(os.path.join(self.context.master_datadir, 'db_dumps')).run()

        try:
            old_dates.remove(self.context.db_date_dir)
        except ValueError:            # db_date_dir was not found in old_dates
            pass

        if len(old_dates) == 0:
            logger.info("No old backup sets to remove")
            return

        delete_old_dates = []
        if self.context.cleanup_total:
            if len(old_dates) < int(self.context.cleanup_total):
                logger.warning("Unable to delete %s backups.  Only have %d backups." % (self.context.cleanup_total, len(old_dates)))
                return

            old_dates.sort()
            delete_old_dates = delete_old_dates + old_dates[0:int(self.context.cleanup_total)]

        if self.context.cleanup_date and self.context.cleanup_date not in delete_old_dates:
            if self.context.cleanup_date not in old_dates:
                logger.warning("Timestamp dump %s does not exist." % self.context.cleanup_date)
                return
            delete_old_dates.append(self.context.cleanup_date)

        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.context.master_port), utility=True)
        primaries = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for old_date in delete_old_dates:
            # Remove the directories on DDBoost only. This will avoid the problem
            # where we might accidently end up deleting local backup files, but
            # the intention was to delete only the files on DDboost.
            if self.context.ddboost:
                logger.info("Preparing to remove dump %s from DDBoost" % old_date)
                cmdStr = 'gpddboost --del-dir=%s' % os.path.join(self.context.dump_dir, old_date)
                if self.context.ddboost_storage_unit:
                    cmdStr += ' --ddboost-storage-unit %s' % self.context.ddboost_storage_unit
                cmd = Command('DDBoost cleanup', cmdStr)
                cmd.run(validateAfter=False)
                rc = cmd.get_results().rc
                if rc != 0:
                    logger.info("Error encountered during deletion of %s on DDBoost" % os.path.join(self.context.dump_dir, old_date))
                    logger.debug(cmd.get_results().stdout)
                    logger.debug(cmd.get_results().stderr)
            else:
                logger.info("Preparing to remove dump %s from all hosts" % old_date)
                path = os.path.join(self.context.master_datadir, 'db_dumps', old_date)

                try:
                    RemoveTree(path).run()
                except OSError, e:
                    logger.warn("Error encountered during deletion of %s" % path)

                for seg in primaries:
                    path = os.path.join(seg.getSegmentDataDirectory(), 'db_dumps', old_date)
                    try:
                        RemoveRemoteTree(path, seg.getSegmentHostName()).run()
                    except ExecutionError, e:
                        logger.warn("Error encountered during deletion of %s on %s" % (path, seg.getSegmentHostName()))

        return delete_old_dates

class VacuumDatabase(Operation):
    # TODO: move this to gppylib.operations.common?
    def __init__(self, context):
        self.context = context

    def execute(self):
        conn = None
        logger.info('Commencing vacuum of %s database, please wait' % self.context.dump_database)
        try:
            dburl = dbconn.DbURL(port=self.context.master_port, dbname=self.context.dump_database)
            conn = dbconn.connect(dburl)
            cursor = conn.cursor()
            cursor.execute("commit")                          # hack to move drop stmt out of implied transaction
            cursor.execute("vacuum")
            cursor.close()
        except Exception, e:
            logger.exception('Error encountered with vacuum of %s database' % self.context.dump_database)
        else:
            logger.info('Vacuum of %s completed without error' % self.context.dump_database)
        finally:
            if conn:
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

class DumpStats(Operation):
    def __init__(self, context):
        self.stats_filename = context.generate_filename("stats")
        self.context = context

    def execute(self):
        logger.info("Commencing pg_statistic dump")

        include_tables = self.get_include_tables_from_context()

        self.write_stats_file_header()

        dburl = dbconn.DbURL(port=self.context.master_port, dbname=self.context.dump_database)
        conn = dbconn.connect(dburl)

        for table in sorted(include_tables):
            self.dump_table(table, conn)

        conn.close()

        if self.context.ddboost:
            stats_backup_file = self.context.generate_filename("stats")
            abspath = stats_backup_file
            relpath = stats_backup_file[stats_backup_file.index(self.context.dump_dir):]
            logger.debug('Copying %s to DDBoost' % abspath)
            cmdStr = 'gpddboost'
            if self.context.ddboost_storage_unit:
                cmdStr +=  ' --ddboost-storage-unit=%s' % self.context.ddboost_storage_unit
            cmdStr += ' --copyToDDBoost --from-file=%s --to-file=%s' % (abspath, relpath)
            cmd = Command('DDBoost copy of %s' % abspath, cmdStr)
            cmd.run(validateAfter=True)

    def write_stats_file_header(self):
        with open(self.stats_filename, "w") as outfile:
            outfile.write("""--
-- Allow system table modifications
--
set allow_system_table_mods="DML";

""")

    def get_include_tables_from_context(self):
        include_tables = []
        if self.context.exclude_dump_tables_file:
            exclude_tables = get_lines_from_file(self.context.exclude_dump_tables_file)
            user_tables = get_user_table_list(self.context)
            tables = []
            for table in user_tables:
                tables.append("%s.%s" % (table[0], table[1]))
            include_tables = list(set(tables) - set(exclude_tables))
        elif self.context.include_dump_tables_file:
            include_tables = get_lines_from_file(self.context.include_dump_tables_file)
        elif self.context.schema_file:
            include_schemas = get_lines_from_file(self.context.schema_file)
            for schema in include_schemas:
                user_tables = get_user_table_list_for_schema(self.context, schema)
                tables = []
                for table in user_tables:
                    tables.append("%s.%s" % (table[0], table[1]))
                include_tables.extend(tables)
        else:
            user_tables = get_user_table_list(self.context)
            for table in user_tables:
                include_tables.append("%s.%s" % (table[0], table[1]))
        return include_tables

    def dump_table(self, table, conn):
        schemaname, tablename = table.split(".")
        schemaname = checkAndRemoveEnclosingDoubleQuote(schemaname)
        tablename = checkAndRemoveEnclosingDoubleQuote(tablename)
        tuples_query = """SELECT pgc.relname, pgn.nspname, pgc.relpages, pgc.reltuples
                          FROM pg_class pgc, pg_namespace pgn
                          WHERE pgc.relnamespace = pgn.oid
                              and pgn.nspname = E'%s'
                              and pgc.relname = E'%s'""" % (schemaname, tablename)
        stats_query = """SELECT pgc.relname, pgn.nspname, pga.attname, pgt.typname, pgs.*
                         FROM pg_class pgc, pg_statistic pgs, pg_namespace pgn, pg_attribute pga, pg_type pgt
                         WHERE pgc.relnamespace = pgn.oid
                             and pgn.nspname = E'%s'
                             and pgc.relname = E'%s'
                             and pgc.oid = pgs.starelid
                             and pga.attrelid = pgc.oid
                             and pga.attnum = pgs.staattnum
                             and pga.atttypid = pgt.oid""" % (schemaname, tablename)

        self.dump_tuples(tuples_query, conn)
        self.dump_stats(stats_query, conn)

    def dump_tuples(self, query, conn):
        rows = execute_sql_with_connection(query, conn)
        for row in rows:
            if len(row) != 4:
                raise Exception("Invalid return from query: Expected 4 columns, got % columns" % (len(row)))
            self.print_tuples(row)

    def dump_stats(self, query, conn):
        rows = execute_sql_with_connection(query, conn)
        for row in rows:
            if len(row) != 25:
                raise Exception("Invalid return from query: Expected 25 columns, got % columns" % (len(row)))
            self.print_stats(row)

    def print_tuples(self, row):
        relname, relnsp, relpages, reltup = row
        with open(self.stats_filename, "a") as outfile:
            outfile.write("""--
-- Schema: %s, Table: %s
--
UPDATE pg_class
SET
    relpages = %s::int,
    reltuples = %d::real
WHERE relname = '%s' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = '%s');

""" % (relnsp, relname, relpages, int(reltup), relname, relnsp))

    def print_stats(self, row):
        relname, nspname, attname, typname, starelid, staattnum, stanullfrac, stawidth, stadistinct, stakind1, stakind2, \
            stakind3, stakind4, staop1, staop2, staop3, staop4, stanumbers1, stanumbers2, stanumbers3, stanumbers4, stavalues1, \
            stavalues2, stavalues3, stavalues4 = row
        smallints = [stakind1, stakind2, stakind3, stakind4]
        oids = [staop1, staop2, staop3, staop4]
        reals = [stanumbers1, stanumbers2, stanumbers3, stanumbers4]
        anyarrays = [stavalues1, stavalues2, stavalues3, stavalues4]

        with open(self.stats_filename, "a") as outfile:
            outfile.write("""--
-- Schema: %s, Table: %s, Attribute: %s
--
INSERT INTO pg_statistic VALUES (
    %d::oid,
    %d::smallint,
    %f::real,
    %d::integer,
    %f::real,
""" % (nspname, relname, attname, starelid, staattnum, stanullfrac, stawidth, stadistinct))

            # If a typname starts with exactly one it describes an array type
            # We can't restore statistics of array columns, so we'll zero and NULL everything out
            if typname[0] == '_' and typname[1] != '_':
                arrayformat = """    0::smallint,
            0::smallint,
            0::smallint,
            0::smallint,
            0::oid,
            0::oid,
            0::oid,
            0::oid,
            NULL::real[],
            NULL::real[],
            NULL::real[],
            NULL::real[],
            NULL,
            NULL,
            NULL,
            NULL
        );"""
                outfile.write(arrayformat)
            else:
                for s in smallints:
                    outfile.write("    %d::smallint,\n" % s)
                for o in oids:
                    outfile.write("    %d::oid,\n" % o)
                for r in reals:
                    if r is None:
                        outfile.write("    NULL::real[],\n")
                    else:
                        strR = str(r)
                        outfile.write("    '{%s}'::real[],\n" % strR[1:len(strR)-1])
                for l in range(len(anyarrays)):
                    a = anyarrays[l]
                    if a is None:
                        outfile.write("    NULL::%s[]" % typname)
                    else:
                        strA = str(a)
                        outfile.write("    '{%s}'::%s[]" % (strA[1:len(strA)-1], typname))
                    if l < len(anyarrays)-1:
                        outfile.write(",")
                    outfile.write("\n")
                outfile.write(");")
            outfile.write("\n\n")
