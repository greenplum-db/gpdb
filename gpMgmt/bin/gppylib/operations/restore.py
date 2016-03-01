import getpass
import gzip
import os
import shutil
import socket
import time

from pygresql import pg
from gppylib import gplog
from gppylib.commands.base import WorkerPool, Command, ExecutionError
from gppylib.commands.gp import Psql
from gppylib.commands.unix import Scp
from gppylib.utils import shellEscape
from gppylib.db import dbconn
from gppylib.db.dbconn import execSQL, execSQLForSingleton
from gppylib.gparray import GpArray
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.operations import Operation
from gppylib.operations.backup_utils import check_backup_type, check_dir_writable, check_file_dumped_with_nbu, create_temp_file_with_tables, \
                                            execute_sql, expand_partition_tables, generate_ao_state_filename, generate_cdatabase_filename, \
                                            generate_co_state_filename, generate_createdb_filename, generate_createdb_prefix, generate_dbdump_prefix, \
                                            generate_dirtytable_filename, generate_global_filename, generate_global_prefix, generate_increments_filename, \
                                            generate_master_config_filename, generate_master_dbdump_prefix, generate_stats_filename, generate_stats_prefix, generate_metadata_filename, \
                                            generate_partition_list_filename, generate_pgstatlastoperation_filename, generate_plan_filename, generate_report_filename, \
                                            generate_segment_config_filename, get_all_segment_addresses, get_backup_directory, get_full_timestamp_for_incremental, \
                                            get_full_timestamp_for_incremental_with_nbu, get_lines_from_file, restore_file_with_nbu, run_pool_command, scp_file_to_hosts, \
                                            verify_lines_in_file, write_lines_to_file, split_fqn, escapeDoubleQuoteInSQLString, get_dbname_from_cdatabaseline, \
                                            checkAndRemoveEnclosingDoubleQuote, checkAndAddEnclosingDoubleQuote, removeEscapingDoubleQuoteInSQLString, \
                                            create_temp_file_with_schemas, check_funny_chars_in_names, remove_file_on_segments
from gppylib.operations.unix import CheckFile, CheckRemoteDir, MakeRemoteDir, CheckRemotePath
from re import compile, search, sub

"""
TODO: partial restore. In 4.x, dump will only occur on primaries.
So, after a dump, dump files must be pushed to mirrors. (This is a task for gpcrondump.)
"""

""" TODO: centralize logging """
logger = gplog.get_default_logger()

WARN_MARK = '<<<<<'
POST_DATA_SUFFIX = '_post_data'

# TODO: use CLI-agnostic custom exceptions instead of ExceptionNoStackTraceNeeded

def update_ao_stat_func(conn, ao_schema, ao_table, counter, batch_size):
    qry = "SELECT * FROM gp_update_ao_master_stats('%s.%s')" % (pg.escape_string(escapeDoubleQuoteInSQLString(ao_schema)), 
                                                                pg.escape_string(escapeDoubleQuoteInSQLString(ao_table)))
    rows = execSQLForSingleton(conn, qry)
    if counter % batch_size == 0:
        conn.commit()

def update_ao_statistics(master_port, dbname, restored_tables):
    qry = """SELECT c.relname,n.nspname
             FROM pg_class c, pg_namespace n
             WHERE c.relnamespace=n.oid
                 AND (c.relstorage='a' OR c.relstorage='c')"""

    conn = None
    counter = 1
    restored_ao_tables = set()
    try:
        results = execute_sql(qry, master_port, dbname)
        for (tbl, sch) in results:
            tblname = '%s.%s' % (sch, tbl)
            if tblname in restored_tables:
                restored_ao_tables.add((sch, tbl))

        if len(restored_ao_tables) == 0:
            logger.info("No AO/CO tables restored, skipping statistics update...")
            return

        with dbconn.connect(dbconn.DbURL(port=master_port, dbname=dbname)) as conn:
            for (ao_schema, ao_table) in sorted(restored_ao_tables):
                update_ao_stat_func(conn, ao_schema, ao_table, counter, batch_size=1000)
                counter = counter + 1
            conn.commit()
    except Exception as e:
        logger.info("Error updating ao statistics after restore")
        raise e

def get_restore_dir(data_dir, backup_dir):
    if backup_dir is not None:
        return backup_dir
    else:
        return data_dir

def get_restore_tables_from_table_file(table_file):
    if not os.path.isfile(table_file):
        raise Exception('Table file does not exist "%s"' % table_file)

    return get_lines_from_file(table_file)

def get_incremental_restore_timestamps(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, inc_timestamp):
    inc_file = generate_increments_filename(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp)
    timestamps = get_lines_from_file(inc_file)
    sorted_timestamps = sorted(timestamps, key=lambda x: int(x), reverse=True)
    incremental_restore_timestamps = []
    try:
        incremental_restore_timestamps = sorted_timestamps[sorted_timestamps.index(inc_timestamp):]
    except ValueError:
        pass
    return incremental_restore_timestamps

def get_partition_list(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key):
    partition_list_file = generate_partition_list_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key)
    partition_list = get_lines_from_file(partition_list_file)
    partition_list = [split_fqn(p) for p in partition_list]
    return partition_list

def get_dirty_table_file_contents(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key):
    dirty_list_file = generate_dirtytable_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp_key)
    return get_lines_from_file(dirty_list_file)

def create_plan_file_contents(master_datadir, backup_dir, dump_dir, dump_prefix, table_set_from_metadata_file, incremental_restore_timestamps, full_timestamp, netbackup_service_host, netbackup_block_size):
    restore_set = {}
    for ts in incremental_restore_timestamps:
        restore_set[ts] = []
        if netbackup_service_host:
            restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_dirtytable_filename(master_datadir, backup_dir, dump_dir, dump_prefix, ts))
        dirty_tables = get_dirty_table_file_contents(master_datadir, backup_dir, dump_dir, dump_prefix, ts)
        for dt in dirty_tables:
            if dt in table_set_from_metadata_file:
                table_set_from_metadata_file.remove(dt)
                restore_set[ts].append(dt)

    restore_set[full_timestamp] = []
    if len(table_set_from_metadata_file) != 0:
        for table in table_set_from_metadata_file:
            restore_set[full_timestamp].append(table)

    return restore_set

def write_to_plan_file(plan_file_contents, plan_file):
    if plan_file is None or not plan_file:
        raise Exception('Invalid plan file %s' % str(plan_file))

    sorted_plan_file_contents = sorted(plan_file_contents, key=lambda x: int(x), reverse=True)

    lines_to_write = []
    for ts in sorted_plan_file_contents:
        tables_str = ','.join(plan_file_contents[ts])
        lines_to_write.append(ts + ':' + tables_str)

    write_lines_to_file(plan_file, lines_to_write)
    verify_lines_in_file(plan_file, lines_to_write)

    return lines_to_write

def create_restore_plan(master_datadir, backup_dir, dump_dir, dump_prefix, db_timestamp, ddboost, netbackup_service_host=None, netbackup_block_size=None):
    dump_tables = get_partition_list(master_datadir, backup_dir, dump_dir, dump_prefix, db_timestamp)

    table_set_from_metadata_file = [schema + '.' + table for schema, table in dump_tables]

    if ddboost:
        full_timestamp = get_full_timestamp_for_incremental(master_datadir, dump_dir, dump_prefix, db_timestamp)
    elif netbackup_service_host:
        full_timestamp = get_full_timestamp_for_incremental_with_nbu(dump_prefix, db_timestamp, netbackup_service_host, netbackup_block_size)
    else:
        full_timestamp = get_full_timestamp_for_incremental(get_restore_dir(master_datadir, backup_dir), dump_dir, dump_prefix, db_timestamp)

    if not full_timestamp:
        raise Exception("Could not locate fullbackup associated with ts '%s'. Either increments file or fullback is missing." % db_timestamp)

    incremental_restore_timestamps = get_incremental_restore_timestamps(master_datadir, backup_dir, dump_dir, dump_prefix, full_timestamp, db_timestamp)

    plan_file_contents = create_plan_file_contents(master_datadir, backup_dir, dump_dir, dump_prefix, table_set_from_metadata_file, incremental_restore_timestamps, full_timestamp, netbackup_service_host, netbackup_block_size)

    plan_file = generate_plan_filename(master_datadir, backup_dir, dump_dir, dump_prefix, db_timestamp)

    write_to_plan_file(plan_file_contents, plan_file)

    return plan_file

def is_incremental_restore(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp, ddboost=False):

    filename = generate_report_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp, ddboost)
    if not os.path.isfile(filename):
        logger.warn('Report file %s does not exist for restore timestamp %s' % (filename, timestamp))
        return False

    report_file_contents = get_lines_from_file(filename)
    if check_backup_type(report_file_contents, 'Incremental'):
        return True
    return False

def is_full_restore(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp, ddboost=False):

    filename = generate_report_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp, ddboost)
    if not os.path.isfile(filename):
        raise Exception('Report file %s does not exist for restore timestamp %s' % (filename, timestamp))

    report_file_contents = get_lines_from_file(filename)
    if check_backup_type(report_file_contents, 'Full'):
        return True

    return False

def is_begin_incremental_run(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp, noplan, ddboost=False):
    if is_incremental_restore(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp, ddboost) and not noplan:
        return True
    else:
        return False

def get_plan_file_contents(master_datadir, backup_dir, timestamp, dump_dir, dump_prefix):
    plan_file_items = []
    plan_file = generate_plan_filename(master_datadir, backup_dir, dump_dir, dump_prefix, timestamp)
    if not os.path.isfile(plan_file):
        raise Exception('Plan file %s does not exist' % plan_file)
    plan_file_lines = get_lines_from_file(plan_file)
    if len(plan_file_lines) <= 0:
        raise Exception('Plan file %s has no contents' % plan_file)

    for line in plan_file_lines:
        if ':' not in line:
            raise Exception('Invalid plan file format')
        # timestamp is of length 14, don't split by ':' in case table name contains ':'
        # don't strip white space on table_list, schema and table name may contain white space
        ts, table_list = line[:14], line[15:]
        plan_file_items.append((ts.strip(), table_list))
    return plan_file_items

def get_restore_table_list(table_list, restore_tables):
    restore_table_set = set()
    restore_list = []

    if restore_tables is None or len(restore_tables) == 0:
        restore_list = table_list
    else:
        for restore_table in restore_tables:
            schema, table = split_fqn(restore_table)
            restore_table_set.add((schema, table))
        for tbl in table_list:
            schema, table = split_fqn(tbl)
            if (schema, table) in restore_table_set:
                restore_list.append(tbl)

    if restore_list == []:
        return None
    return create_temp_file_with_tables(restore_list)

def validate_restore_tables_list(plan_file_contents, restore_tables, schema_level_restore_list=None):
    """
    Check if the tables in plan file match any of the restore tables.

    For schema level restore, check if table schema in plan file match
    any member of schema list.
    """
    if restore_tables is None:
        return

    table_set = set()
    comp_set = set()

    for ts, table in plan_file_contents:
        tables = table.split(',')
        for table in tables:
            table_set.add(table)

    invalid_tables = []
    for table in restore_tables:
        schema_name, table_name = split_fqn(table)
        if schema_level_restore_list and schema_name in schema_level_restore_list:
            continue
        else:
            comp_set.add(table)
            if not comp_set.issubset(table_set):
                invalid_tables.append(table)
                comp_set.remove(table)

    if invalid_tables != []:
        raise Exception('Invalid tables for -T option: The following tables were not found in plan file : "%s"' % (invalid_tables))

#NetBackup related functions
def restore_state_files_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_ao_state_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp))
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_co_state_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp))
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_pgstatlastoperation_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp))

def restore_report_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_report_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp))

def restore_cdatabase_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_cdatabase_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp))

def restore_config_files_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, master_port, netbackup_service_host, netbackup_block_size):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    if master_port is None:
        raise Exception('Master port is None.')
    use_dir = get_backup_directory(master_datadir, backup_dir, dump_dir, restore_timestamp)
    master_config_filename = os.path.join(use_dir, "%s" % generate_master_config_filename(dump_prefix, restore_timestamp))
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, master_config_filename)

    gparray = GpArray.initFromCatalog(dbconn.DbURL(port=master_port), utility=True)
    segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
    for seg in segs:
        use_dir = get_backup_directory(seg.getSegmentDataDirectory(), backup_dir, dump_dir, restore_timestamp)
        seg_config_filename = os.path.join(use_dir, "%s" % generate_segment_config_filename(dump_prefix, seg.getSegmentDbId(), restore_timestamp))
        seg_host = seg.getSegmentHostName()
        restore_file_with_nbu(netbackup_service_host, netbackup_block_size, seg_config_filename, seg_host)

def restore_global_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_global_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp[0:8], restore_timestamp))

def restore_statistics_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_stats_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp[0:8], restore_timestamp))

def restore_partition_list_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_partition_list_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp))

def restore_increments_file_with_nbu(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    full_ts = get_full_timestamp_for_incremental_with_nbu(dump_prefix, restore_timestamp, netbackup_service_host, netbackup_block_size)
    if full_ts is not None:
        restore_file_with_nbu(netbackup_service_host, netbackup_block_size, generate_increments_filename(master_datadir, backup_dir, dump_dir, dump_prefix, full_ts))
    else:
        raise Exception('Unable to locate full timestamp for given incremental timestamp "%s" using NetBackup' % restore_timestamp)

def config_files_dumped(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    use_dir = get_backup_directory(master_datadir, backup_dir, dump_dir, restore_timestamp)
    master_config_filename = os.path.join(use_dir, "%s" % generate_master_config_filename(dump_prefix, restore_timestamp))
    return check_file_dumped_with_nbu(netbackup_service_host, master_config_filename)

def global_file_dumped(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    global_filename = generate_global_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp[0:8], restore_timestamp)
    return check_file_dumped_with_nbu(netbackup_service_host, global_filename)

def statistics_file_dumped(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp, netbackup_service_host):
    if (master_datadir is None) and (backup_dir is None):
        raise Exception('Master data directory and backup directory are both none.')
    if restore_timestamp is None:
        raise Exception('Restore timestamp is None.')
    if netbackup_service_host is None:
        raise Exception('Netbackup service hostname is None.')
    statistics_filename = generate_stats_filename(master_datadir, backup_dir, dump_dir, dump_prefix, restore_timestamp[0:8], restore_timestamp)
    return check_file_dumped_with_nbu(netbackup_service_host, statistics_filename)

def _build_gpdbrestore_cmd_line(ts, table_file, backup_dir, redirected_restore_db, report_status_dir, dump_prefix, ddboost=False, netbackup_service_host=None,
                                netbackup_block_size=None, change_schema=None, schema_level_restore_file=None):
    cmd = 'gpdbrestore -t %s --table-file %s -a -v --noplan --noanalyze --noaostats' % (ts, table_file)
    if backup_dir is not None:
        cmd += " -u %s" % backup_dir
    if dump_prefix:
        cmd += " --prefix=%s" % dump_prefix.strip('_')
    if redirected_restore_db:
        cmd += " --redirect=%s" % checkAndAddEnclosingDoubleQuote(shellEscape(redirected_restore_db))
    if report_status_dir:
        cmd += " --report-status-dir=%s" % report_status_dir
    if ddboost:
        cmd += " --ddboost"
    if netbackup_service_host:
        cmd += " --netbackup-service-host=%s" % netbackup_service_host
    if netbackup_block_size:
        cmd += " --netbackup-block-size=%s" % netbackup_block_size
    if change_schema:
        cmd += " --change-schema=%s" % checkAndAddEnclosingDoubleQuote(shellEscape(change_schema))
    if schema_level_restore_file:
        cmd += " --schema-level-file=%s" % schema_level_restore_file

    return cmd

def truncate_restore_tables(restore_tables, master_port, dbname, schema_level_restore_list=None):
    """
    Truncate either specific table or all tables under a schema
    """

    try:
        dburl = dbconn.DbURL(port=master_port, dbname=dbname)
        conn = dbconn.connect(dburl)
        truncate_list = []

        if schema_level_restore_list:
            for schemaname in schema_level_restore_list:
                truncate_list.extend(self.get_full_tables_in_schema(conn, schemaname))
        else:
            for restore_table in restore_tables:
                schemaname, tablename = split_fqn(restore_table)
                check_table_exists_qry = """SELECT EXISTS (
                                                   SELECT 1
                                                   FROM pg_catalog.pg_class c
                                                   JOIN pg_catalog.pg_namespace n on n.oid = c.relnamespace
                                                   WHERE n.nspname = E'%s' and c.relname = E'%s')""" % (pg.escape_string(schemaname),
                                                                                                      pg.escape_string(tablename))
                exists_result = execSQLForSingleton(conn, check_table_exists_qry)
                if exists_result:
                    schema = escapeDoubleQuoteInSQLString(schemaname)
                    table = escapeDoubleQuoteInSQLString(tablename)
                    truncate_table = '%s.%s' % (schema, table)
                    truncate_list.append(truncate_table)
                else:
                    logger.warning("Skipping truncate of %s.%s because the relation does not exist." % (dbname, restore_table))

        for t in truncate_list:
            try:
                qry = 'Truncate %s' % t
                execSQL(conn, qry)
            except Exception as e:
                raise Exception("Could not truncate table %s.%s: %s" % (dbname, t, str(e).replace('\n', '')))

        conn.commit()
    except Exception as e:
        raise Exception("Failure from truncating tables, %s" % (str(e).replace('\n', '')))

class RestoreDatabase(Operation):
    def __init__(self, restore_timestamp, no_analyze, drop_db, restore_global, master_datadir, backup_dir,
                 master_port, dump_dir, dump_prefix, no_plan, restore_tables, batch_default, no_ao_stats,
                 redirected_restore_db, report_status_dir, restore_stats, metadata_only, ddboost,
                 netbackup_service_host, netbackup_block_size, change_schema, schema_level_restore_list):
        self.restore_timestamp = restore_timestamp
        self.no_analyze = no_analyze
        self.drop_db = drop_db
        self.restore_global = restore_global
        self.master_datadir = master_datadir
        self.backup_dir = backup_dir
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.no_plan = no_plan
        self.restore_tables = restore_tables
        self.batch_default = batch_default
        self.no_ao_stats = no_ao_stats
        self.redirected_restore_db = redirected_restore_db
        self.report_status_dir = report_status_dir
        self.restore_stats = restore_stats
        self.metadata_only = metadata_only
        self.ddboost = ddboost
        self.netbackup_service_host = netbackup_service_host
        self.netbackup_block_size = netbackup_block_size
        self.change_schema = change_schema
        self.schema_level_restore_list = schema_level_restore_list

    def execute(self):
        (restore_timestamp, restore_db, compress) = ValidateRestoreDatabase(restore_timestamp = self.restore_timestamp,
                                                                            master_datadir = self.master_datadir,
                                                                            backup_dir = self.backup_dir,
                                                                            master_port = self.master_port,
                                                                            dump_dir = self.dump_dir,
                                                                            dump_prefix = self.dump_prefix,
                                                                            ddboost = self.ddboost,
                                                                            netbackup_service_host = self.netbackup_service_host).run()

        if self.redirected_restore_db and not self.drop_db:
            restore_db = self.redirected_restore_db
            self.create_database_if_not_exists(self.master_port, self.redirected_restore_db)
            self.create_gp_toolkit(self.redirected_restore_db)
        elif self.redirected_restore_db:
            restore_db = self.redirected_restore_db

        if self.restore_stats == "only":
            self._restore_stats(restore_timestamp, self.master_datadir, self.backup_dir, self.master_port, restore_db, self.restore_tables)
            return

        if self.drop_db:
            self._multitry_createdb(restore_timestamp,
                                    restore_db,
                                    self.redirected_restore_db,
                                    self.master_datadir,
                                    self.backup_dir,
                                    self.master_port)

        if self.restore_global:
            self._restore_global(restore_timestamp, self.master_datadir, self.backup_dir)
            if self.restore_global == "only":
                return

        """
        For full restore with table filter or for the first recurssion of the incremental restore
        we first restore the schema, expand the parent partition table name's in the restore table
        filter to include leaf partition names, and then restore data (only, using '-a' option).
        """

        full_restore_with_filter = False
        full_restore = is_full_restore(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.restore_timestamp, self.ddboost)
        begin_incremental = is_begin_incremental_run(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.restore_timestamp, self.no_plan, self.ddboost)

        table_filter_file = self.create_filter_file() # returns None if nothing to filter
        change_schema_file = self.create_change_schema_file() # returns None if nothing to filter
        schema_level_restore_file = self.create_schema_level_file()

        if (full_restore and len(self.restore_tables) > 0 and not self.no_plan) or begin_incremental or self.metadata_only:
            if full_restore and not self.no_plan:
                full_restore_with_filter = True

            metadata_file = generate_metadata_filename(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.restore_timestamp)
            restore_line = self._build_schema_only_restore_line(restore_timestamp,
                                                                restore_db, compress,
                                                                self.master_port,
                                                                metadata_file,
                                                                table_filter_file,
                                                                full_restore_with_filter,
                                                                change_schema_file,
                                                                schema_level_restore_file)
            logger.info("Running metadata restore")
            logger.info("Invoking commandline: %s" % restore_line)
            cmd = Command('Invoking gp_restore', restore_line)
            cmd.run(validateAfter=False)
            self._process_result(cmd)
            logger.info("Expanding parent partitions if any in table filter")
            self.restore_tables = expand_partition_tables(restore_db, self.restore_tables)

        if begin_incremental:
            logger.info("Running data restore")
            self.restore_incremental_data_only(restore_db)
        else:
            table_filter_file = self.create_filter_file() # returns None if nothing to filter

            if not self.metadata_only:
                restore_line = self._build_restore_line(restore_timestamp,
                                                        restore_db, compress,
                                                        self.master_port,
                                                        self.no_plan, table_filter_file,
                                                        self.no_ao_stats, full_restore_with_filter,
                                                        change_schema_file, schema_level_restore_file)
                logger.info('gp_restore commandline: %s: ' % restore_line)
                cmd = Command('Invoking gp_restore', restore_line)
                cmd.run(validateAfter=False)
                self._process_result(cmd)

            if full_restore_with_filter:
                restore_line = self._build_post_data_schema_only_restore_line(restore_timestamp,
                                                                              restore_db, compress,
                                                                              self.master_port,
                                                                              table_filter_file,
                                                                              full_restore_with_filter,
                                                                              change_schema_file, schema_level_restore_file)
                logger.info("Running post data restore")
                logger.info('gp_restore commandline: %s: ' % restore_line)
                cmd = Command('Invoking gp_restore', restore_line)
                cmd.run(validateAfter=False)
                self._process_result(cmd)

        if not self.metadata_only:
            if (not self.no_analyze) and (len(self.restore_tables) == 0):
                self._analyze(restore_db, self.master_port)
            elif (not self.no_analyze) and len(self.restore_tables) > 0:
                self._analyze_restore_tables(restore_db, self.restore_tables, self.change_schema)
        if self.restore_stats:
            self._restore_stats(restore_timestamp, self.master_datadir, self.backup_dir, self.master_port, restore_db, self.restore_tables)

        self.tmp_files = [table_filter_file, change_schema_file, schema_level_restore_file]
        self.cleanup_files_on_segments()

    def _process_result(self, cmd):
        res = cmd.get_results()
        if res.rc == 0:
            logger.info("gpdbrestore finished successfully")
        elif res.rc == 2:
            logger.warn("gpdbrestore finished but ERRORS were found, please check the restore report file for details")
        else:
            raise Exception('gpdbrestore finished unsuccessfully')

    def cleanup_files_on_segments(self):
        for tmp_file in self.tmp_files:
            if tmp_file and os.path.isfile(tmp_file):
                os.remove(tmp_file)
                remove_file_on_segments(self.master_port, tmp_file, self.batch_default)

    def _analyze(self, restore_db, master_port):
        conn = None
        logger.info('Commencing analyze of %s database, please wait' % restore_db)
        try:
            dburl = dbconn.DbURL(port=master_port, dbname=restore_db)
            conn = dbconn.connect(dburl)
            execSQL(conn, 'analyze')
            conn.commit()
        except Exception, e:
            logger.warn('Issue with analyze of %s database' % restore_db)
        else:
            logger.info('Analyze of %s completed without error' % restore_db)
        finally:
            if conn is not None:
                conn.close()

    def _analyze_restore_tables(self, restore_db, restore_tables, change_schema):
        logger.info('Commencing analyze of restored tables in \'%s\' database, please wait' % restore_db)
        batch_count = 0
        try:
            with dbconn.connect(dbconn.DbURL(dbname=restore_db, port=self.master_port)) as conn:
                num_sqls = 0
                analyze_list = []
                # need to find out all tables under the schema and construct the new schema.table to analyze
                if change_schema and self.schema_level_restore_list:
                    schemaname = change_schema
                    analyze_list = self.get_full_tables_in_schema(conn, schemaname)
                elif self.schema_level_restore_list:
                    for schemaname in self.schema_level_restore_list:
                        analyze_list.extend(self.get_full_tables_in_schema(conn, schemaname))
                else:
                    for restore_table in restore_tables:
                        schemaname, tablename = split_fqn(restore_table)
                        if change_schema:
                            schema = escapeDoubleQuoteInSQLString(change_schema)
                        else:
                            schema = escapeDoubleQuoteInSQLString(schemaname)
                        table = escapeDoubleQuoteInSQLString(tablename)
                        restore_table = '%s.%s' % (schema, table)
                        analyze_list.append(restore_table)

                for tbl in analyze_list:
                    analyze_table = "analyze " + tbl
                    logger.info('analyze table statement is %s' % analyze_table)
                    try:
                        execSQL(conn, analyze_table)
                    except Exception as e:
                        raise Exception('Issue with \'ANALYZE\' of restored table \'%s\' in \'%s\' database' % (restore_table, restore_db))
                    else:
                        num_sqls += 1
                        if num_sqls == 1000: # The choice of batch size was choosen arbitrarily
                            batch_count +=1
                            logger.debug('Completed executing batch of 1000 tuple count SQLs')
                            conn.commit()
                            num_sqls = 0
        except Exception as e:
            logger.warn('Restore of \'%s\' database succeeded but \'ANALYZE\' of restored tables failed' % restore_db)
            logger.warn('Please run ANALYZE manually on restored tables. Failure to run ANALYZE might result in poor database performance')
            raise Exception(str(e))
        else:
            logger.info('\'Analyze\' of restored tables in \'%s\' database completed without error' % restore_db)
        return batch_count

    def get_full_tables_in_schema(self, conn, schemaname):
        res = []
        get_all_tables_qry = 'select \'"\' || schemaname || \'"\', \'"\' || tablename || \'"\'from pg_tables where schemaname = \'%s\';' % pg.escape_string(schemaname)
        relations = execSQL(conn, get_all_tables_qry)
        for relation in relations:
            schema, table = relation[0], relation[1]
            schema = escapeDoubleQuoteInSQLString(schema)
            table = escapeDoubleQuoteInSQLString(table)
            restore_table = '%s.%s' % (schema, table)
            res.append(restore_table)
        return res

    def create_schema_level_file(self):
        if not self.schema_level_restore_list:
            return None

        schema_level_restore_file = create_temp_file_with_schemas(list(self.schema_level_restore_list))

        addresses = get_all_segment_addresses(self.master_port)

        scp_file_to_hosts(addresses, schema_level_restore_file, self.batch_default)

        return schema_level_restore_file

    def create_change_schema_file(self):
        if not self.change_schema:
            return None

        schema_list = [self.change_schema]

        change_schema_file = create_temp_file_with_schemas(schema_list)

        addresses = get_all_segment_addresses(self.master_port)

        scp_file_to_hosts(addresses, change_schema_file, self.batch_default)

        return change_schema_file

    def create_filter_file(self):
        if not self.restore_tables or len(self.restore_tables) == 0:
            return None

        table_filter_file = create_temp_file_with_tables(self.restore_tables)

        addresses = get_all_segment_addresses(self.master_port)

        scp_file_to_hosts(addresses, table_filter_file, self.batch_default)

        return table_filter_file


    def restore_incremental_data_only(self, restore_db):
        restore_data = False
        plan_file_items = get_plan_file_contents(self.master_datadir, self.backup_dir,
                                                 self.restore_timestamp, self.dump_dir,
                                                 self.dump_prefix)
        table_file = None
        table_files = []
        restored_tables = []

        validate_restore_tables_list(plan_file_items, self.restore_tables, self.schema_level_restore_list)

        for (ts, table_list) in plan_file_items:
            if table_list:
                restore_data = True
                table_file = get_restore_table_list(table_list.strip('\n').split(','), self.restore_tables)
                if table_file is None:
                    continue
                cmd = _build_gpdbrestore_cmd_line(ts, table_file, self.backup_dir,
                                                  self.redirected_restore_db,
                                                  self.report_status_dir, self.dump_prefix,
                                                  self.ddboost, self.netbackup_service_host,
                                                  self.netbackup_block_size, self.change_schema)
                logger.info('Invoking commandline: %s' % cmd)
                Command('Invoking gpdbrestore', cmd).run(validateAfter=True)
                table_files.append(table_file)
                restored_tables.extend(get_restore_tables_from_table_file(table_file))

        if not restore_data:
            raise Exception('There were no tables to restore. Check the plan file contents for restore timestamp %s' % self.restore_timestamp)

        if not self.no_ao_stats:
            logger.info("Updating AO/CO statistics on master")
            update_ao_statistics(self.master_port, restore_db, restored_tables)
        else:
            logger.info("noaostats enabled. Skipping update of AO/CO statistics on master.")

        for table_file in table_files:
            if table_file:
                os.remove(table_file)

        return True

    def _restore_global(self, restore_timestamp, master_datadir, backup_dir):
        logger.info('Commencing restore of global objects')
        global_file = os.path.join(get_restore_dir(master_datadir, backup_dir), self.dump_dir, restore_timestamp[0:8], "%s%s" % (generate_global_prefix(self.dump_prefix), restore_timestamp))
        if not os.path.exists(global_file):
            raise Exception('Unable to locate global file %s%s in dump set' % (generate_global_prefix(self.dump_prefix), restore_timestamp))
        Psql('Invoking global dump', filename=global_file).run(validateAfter=True)

    def _restore_stats(self, restore_timestamp, master_datadir, backup_dir, master_port, restore_db, restore_tables):
        logger.info('Commencing restore of statistics')
        stats_filename = "%s%s" % (generate_stats_prefix(self.dump_prefix), restore_timestamp)
        stats_path = os.path.join(get_restore_dir(master_datadir, backup_dir), self.dump_dir, restore_timestamp[0:8], stats_filename)
        if not os.path.exists(stats_path):
            raise Exception('Unable to locate statistics file %s in dump set' % stats_filename)

        # We need to replace existing starelid's in file to match starelid of tables in database in case they're different
        # First, map each schemaname.tablename to its corresponding starelid
        query = """SELECT t.schemaname || '.' || t.tablename, c.oid FROM pg_class c join pg_tables t ON c.relname = t.tablename
                         WHERE t.schemaname NOT IN ('pg_toast', 'pg_bitmapindex', 'pg_temp_1', 'pg_catalog', 'information_schema', 'gp_toolkit')"""
        relids = {}
        rows = execute_sql(query, master_port, restore_db)
        file = []
        for row in rows:
            if len(row) != 2:
                raise Exception("Invalid return from query: Expected 2 columns, got % columns" % (len(row)))
            relids[row[0]] = str(row[1])

        # Read in the statistics dump file, find each schemaname.tablename section, and replace the corresponding starelid
        # This section is also where we filter out tables that are not in restore_tables
        with open("/tmp/%s" % stats_filename, "w") as outfile:
            with open(stats_path, "r") as infile:
                table_pattern = compile("-- Schema: (\w+), Table: (\w+)")
                print_toggle = True
                starelid_toggle = False
                new_oid = ""
                for line in infile:
                    matches = search(table_pattern, line)
                    if matches:
                        tablename = '%s.%s' % (matches.group(1), matches.group(2))
                        if len(restore_tables) == 0 or tablename in restore_tables:
                            try:
                                new_oid = relids[tablename]
                                print_toggle = True
                                starelid_toggle = True
                            except KeyError as e:
                                if "Attribute" not in line: # Only print a warning once per table, at the tuple count restore section
                                    logger.warning("Cannot restore statistics for table %s: Table does not exist.  Skipping...", tablename)
                                print_toggle = False
                                starelid_toggle = False
                        else:
                            print_toggle = False
                    if starelid_toggle and "::oid" in line:
                        line = "    %s::oid,\n" % new_oid
                        starelid_toggle = False
                    if print_toggle:
                        outfile.write(line)

        Psql('Invoking statistics restore', filename="/tmp/%s" % stats_filename, database=restore_db).run(validateAfter=True)

    def _multitry_createdb(self, restore_timestamp, restore_db, redirected_restore_db, master_datadir, backup_dir, master_port):
        no_of_trys = 600
        for _ in range(no_of_trys):
            try:
                self._process_createdb(restore_timestamp, restore_db, redirected_restore_db, master_datadir, backup_dir, master_port)
            except ExceptionNoStackTraceNeeded:
                time.sleep(1)
            else:
                return
        raise ExceptionNoStackTraceNeeded('Failed to drop database %s' % restore_db)

    def drop_database_if_exists(self, master_port, restore_db):
        conn = None
        try:
            dburl = dbconn.DbURL(port=master_port, dbname='template1')
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_database where datname='%s';" % pg.escape_string(restore_db))

            logger.info("Dropping Database %s" % restore_db)
            if count == 1:
                cmd = Command(name='drop database %s' % restore_db, 
                              cmdStr='dropdb %s -p %s' % (checkAndAddEnclosingDoubleQuote(shellEscape(restore_db)), master_port))
                cmd.run(validateAfter=True)
            logger.info("Dropped Database %s" % restore_db)
        except ExecutionError, e:
            logger.exception("Could not drop database %s" % restore_db)
            raise ExceptionNoStackTraceNeeded('Failed to drop database %s' % restore_db)
        finally:
            conn.close()

    def create_database_if_not_exists(self, master_port, restore_db):
        """
        restore_db: true database name (non escaped or quoted)
        """
        conn = None
        try:
            dburl = dbconn.DbURL(port=master_port)
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_database where datname='%s';" % pg.escape_string(restore_db))

            logger.info("Creating Database %s" % restore_db)
            if count == 0:
                cmd = Command(name='create database %s' % restore_db,
                              cmdStr='createdb %s -p %s -T template0' % (checkAndAddEnclosingDoubleQuote(shellEscape(restore_db)), master_port))
                cmd.run(validateAfter=True)
            logger.info("Created Database %s" % restore_db)
        except ExecutionError, e:
            logger.exception("Could not create database %s" % restore_db)
            raise ExceptionNoStackTraceNeeded('Failed to create database %s' % restore_db)
        finally:
            conn.close()

    def check_gp_toolkit(self, restore_db):
        GP_TOOLKIT_QUERY = """SELECT count(*)
                              FROM pg_class pgc, pg_namespace pgn
                              WHERE pgc.relnamespace=pgn.oid AND
                                    pgn.nspname='gp_toolkit'
                           """
        with dbconn.connect(dbconn.DbURL(dbname=restore_db, port=self.master_port)) as conn:
            res = dbconn.execSQLForSingleton(conn, GP_TOOLKIT_QUERY)
            if res == 0:
                return False
            return True

    def create_gp_toolkit(self, restore_db):
        if not self.check_gp_toolkit(restore_db):
            if 'GPHOME' not in os.environ:
                logger.warn('Please set $GPHOME in your environment')
                logger.warn('Skipping creation of gp_toolkit since $GPHOME/share/postgresql/gp_toolkit.sql could not be found')
            else:
                logger.info('Creating gp_toolkit schema for database "%s"' % restore_db)
                Psql(name='create gp_toolkit',
                     filename=os.path.join(os.environ['GPHOME'],
                                           'share', 'postgresql',
                                           'gp_toolkit.sql'),
                     database=restore_db).run(validateAfter=True)

    def _process_createdb(self, restore_timestamp, restore_db, redirected_restore_db, master_datadir, backup_dir, master_port):
        if redirected_restore_db:
            self.drop_database_if_exists(master_port, redirected_restore_db)
            self.create_database_if_not_exists(master_port, redirected_restore_db)
        else:
            self.drop_database_if_exists(master_port, restore_db)
            createdb_file = generate_createdb_filename(master_datadir, backup_dir, self.dump_dir, self.dump_prefix, restore_timestamp)
            logger.info('Invoking sql file: %s' % createdb_file)
            Psql('Invoking schema dump', filename=createdb_file).run(validateAfter=True)
        self.create_gp_toolkit(restore_db)

    def backup_dir_is_writable(self):
        if self.backup_dir and not self.report_status_dir:
            try:
                path = os.path.join(self.dump_dir, self.restore_timestamp[0:8])
                directory = os.path.join(self.backup_dir, path)
                check_dir_writable(directory)
            except Exception as e:
                logger.warning('Backup directory %s is not writable. Error %s' % (directory, str(e)))
                logger.warning('Since --report-status-dir option is not specified, report and status file will be written in segment data directory.')
                return False
        return True

    def _build_restore_line(self, restore_timestamp, restore_db, compress, master_port, no_plan,
                            table_filter_file, no_stats, full_restore_with_filter, change_schema_file, schema_level_restore_file=None):

        user = getpass.getuser()
        hostname = socket.gethostname()    # TODO: can this just be localhost? bash was using `hostname`
        path = os.path.join(self.dump_dir, restore_timestamp[0:8])
        if self.backup_dir is not None:
            path = os.path.join(self.backup_dir, path)
        restore_line = "gp_restore -i -h %s -p %s -U %s --gp-i" % (hostname, master_port, user)
        if self.dump_prefix:
            logger.info("Adding --prefix")
            restore_line += " --prefix=%s" % self.dump_prefix
        restore_line += " --gp-k=%s --gp-l=p" % (restore_timestamp)
        restore_line += " --gp-d=%s" % path

        if self.report_status_dir:
            restore_line += " --gp-r=%s" % self.report_status_dir
            restore_line += " --status=%s" % self.report_status_dir
        elif self.backup_dir and self.backup_dir_is_writable():
            restore_line += " --gp-r=%s" % path
            restore_line += " --status=%s" % path
        # else
        # gp-r is not set, restore.c sets it to MASTER_DATA_DIRECTORY if not specified.
        # status file is not set, cdbbackup.c sets it to SEGMENT_DATA_DIRECTORY if not specified.

        if table_filter_file:
            restore_line += " --gp-f=%s" % table_filter_file
        if compress:
            restore_line += " --gp-c"
        restore_line += " -d %s" % checkAndAddEnclosingDoubleQuote(shellEscape(restore_db))

        # Restore only data if no_plan or full_restore_with_filter is True
        if no_plan or full_restore_with_filter:
            restore_line += " -a"
        if no_stats:
            restore_line += " --gp-nostats"
        if self.ddboost:
            restore_line += " --ddboost"
        if self.netbackup_service_host:
            restore_line += " --netbackup-service-host=%s" % self.netbackup_service_host
        if self.netbackup_block_size:
            restore_line += " --netbackup-block-size=%s" % self.netbackup_block_size
        if change_schema_file:
            restore_line += " --change-schema-file=%s" % change_schema_file
        if schema_level_restore_file:
            restore_line += " --schema-level-file=%s" % schema_level_restore_file

        return restore_line

    def _build_post_data_schema_only_restore_line(self, restore_timestamp, restore_db, compress, master_port,
                                                  table_filter_file, full_restore_with_filter, 
                                                  change_schema_file=None, schema_level_restore_file=None):
        user = getpass.getuser()
        hostname = socket.gethostname()    # TODO: can this just be localhost? bash was using `hostname`
        path = os.path.join(self.dump_dir, restore_timestamp[0:8])
        if self.backup_dir is not None:
            path = os.path.join(self.backup_dir, path)
        restore_line = "gp_restore -i -h %s -p %s -U %s --gp-d=%s --gp-i" % (hostname, master_port, user, path)
        restore_line += " --gp-k=%s --gp-l=p" % (restore_timestamp)

        if full_restore_with_filter:
            restore_line += " -P"
        if self.report_status_dir:
            restore_line += " --gp-r=%s" % self.report_status_dir
            restore_line += " --status=%s" % self.report_status_dir
        elif self.backup_dir and self.backup_dir_is_writable():
            restore_line += " --gp-r=%s" % path
            restore_line += " --status=%s" % path
        # else
        # gp-r is not set, restore.c sets it to MASTER_DATA_DIRECTORY if not specified.
        # status file is not set, cdbbackup.c sets it to SEGMENT_DATA_DIRECTORY if not specified.

        if self.dump_prefix:
            logger.info("Adding --prefix")
            restore_line += " --prefix=%s" % self.dump_prefix
        if table_filter_file:
            restore_line += " --gp-f=%s" % table_filter_file
        if change_schema_file:
            restore_line += " --change-schema-file=%s" % change_schema_file
        if schema_level_restore_file:
            restore_line += " --schema-level-file=%s" % schema_level_restore_file
        if compress:
            restore_line += " --gp-c"
        restore_line += " -d %s" % checkAndAddEnclosingDoubleQuote(shellEscape(restore_db))

        if self.ddboost:
            restore_line += " --ddboost"
        if self.netbackup_service_host:
            restore_line += " --netbackup-service-host=%s" % self.netbackup_service_host
        if self.netbackup_block_size:
            restore_line += " --netbackup-block-size=%s" % self.netbackup_block_size

        return restore_line

    def _build_schema_only_restore_line(self, restore_timestamp, restore_db, compress, master_port,
                                        metadata_filename, table_filter_file, full_restore_with_filter, 
                                        change_schema_file=None, schema_level_restore_file=None):
        user = getpass.getuser()
        hostname = socket.gethostname()    # TODO: can this just be localhost? bash was using `hostname`
        (gpr_path, status_path, gpd_path) = self.get_restore_line_paths(restore_timestamp[0:8])

        restore_line = "gp_restore -i -h %s -p %s -U %s --gp-i" % (hostname, master_port, user)
        restore_line += " --gp-k=%s --gp-l=p -s %s" % (restore_timestamp, metadata_filename)

        if full_restore_with_filter:
            restore_line += " -P"
        if gpr_path is not None and status_path is not None:
            restore_line += " --gp-r=%s" % gpr_path
            restore_line += " --status=%s" % status_path
        # else
        # gp-r is not set, restore.c sets it to MASTER_DATA_DIRECTORY if not specified.
        # status file is not set, cdbbackup.c sets it to SEGMENT_DATA_DIRECTORY if not specified.

        restore_line += " --gp-d=%s" % gpd_path

        if self.dump_prefix:
            logger.info("Adding --prefix")
            restore_line += " --prefix=%s" % self.dump_prefix
        if table_filter_file:
            restore_line += " --gp-f=%s" % table_filter_file
        if compress:
            restore_line += " --gp-c"
        restore_line += " -d %s" % checkAndAddEnclosingDoubleQuote(shellEscape(restore_db))

        if self.ddboost:
            restore_line += " --ddboost"
        if self.netbackup_service_host:
            restore_line += " --netbackup-service-host=%s" % self.netbackup_service_host
        if self.netbackup_block_size:
            restore_line += " --netbackup-block-size=%s" % self.netbackup_block_size
        if change_schema_file:
            restore_line += " --change-schema-file=%s" % change_schema_file
        if schema_level_restore_file:
            restore_line += " --schema-level-file=%s" % schema_level_restore_file

        return restore_line

    def get_restore_line_paths(self, timestamp):
        (gpr_path, status_path, gpd_path) = (None, None, None)

        gpd_path = os.path.join(self.dump_dir, timestamp)
        if self.backup_dir is not None:
            gpd_path = os.path.join(self.backup_dir, gpd_path)

        if self.report_status_dir:
            gpr_path = self.report_status_dir
            status_path = self.report_status_dir
        elif self.backup_dir and self.backup_dir_is_writable():
            gpr_path = gpd_path
            status_path = gpd_path

        if self.ddboost:
            gpd_path = "%s/%s" % (self.dump_dir, timestamp)

        return (gpr_path, status_path, gpd_path)

class ValidateRestoreDatabase(Operation):
    """ TODO: add other checks. check for _process_createdb? """
    def __init__(self, restore_timestamp, master_datadir, backup_dir, master_port, dump_dir, dump_prefix, ddboost, netbackup_service_host):
        self.restore_timestamp = restore_timestamp
        self.master_datadir = master_datadir
        self.backup_dir = backup_dir
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.ddboost = ddboost
        self.netbackup_service_host = netbackup_service_host

    def execute(self):
        (restore_timestamp, restore_db, compress) = ValidateTimestamp(self.restore_timestamp, self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.netbackup_service_host, self.ddboost).run()
        if not self.ddboost:
            ValidateSegments(restore_timestamp, compress, self.master_port, self.backup_dir, self.dump_dir, self.dump_prefix, self.netbackup_service_host).run()
        return (restore_timestamp, restore_db, compress)

class ValidateTimestamp(Operation):
    def __init__(self, candidate_timestamp, master_datadir, backup_dir, dump_dir, dump_prefix, netbackup_service_host, ddboost=False):
        self.master_datadir = master_datadir
        self.backup_dir = backup_dir
        self.candidate_timestamp = candidate_timestamp
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.netbackup_service_host = netbackup_service_host
        self.ddboost = ddboost

    def validate_compressed_file(self, compressed_file):
        if self.netbackup_service_host:
            logger.info('Backup for given timestamp was performed using NetBackup. Querying NetBackup server to check for the dump file.')
            compress = check_file_dumped_with_nbu(self.netbackup_service_host, compressed_file)
        else:
            compress = os.path.exists(compressed_file)
            if not compress:
                uncompressed_file = compressed_file[:compressed_file.index('.gz')]
                if not os.path.exists(uncompressed_file):
                    raise ExceptionNoStackTraceNeeded('Unable to find {ucfile} or {ucfile}.gz. Skipping restore.'
                                                      .format(ucfile=uncompressed_file))
        return compress

    def execute(self):
        path = os.path.join(get_restore_dir(self.master_datadir, self.backup_dir), self.dump_dir, self.candidate_timestamp[0:8])
        createdb_file = generate_createdb_filename(self.master_datadir, self.backup_dir, self.dump_dir, self.dump_prefix, self.candidate_timestamp, self.ddboost)
        if not CheckFile(createdb_file).run():
            raise ExceptionNoStackTraceNeeded("Dump file '%s' does not exist on Master" % createdb_file)
        restore_db = GetDbName(createdb_file).run()
        if not self.ddboost:
            compressed_file = os.path.join(path, "%s%s.gz" % (generate_master_dbdump_prefix(self.dump_prefix), self.candidate_timestamp))
            compress = self.validate_compressed_file(compressed_file)
        else:
            compressed_file = os.path.join(path, "%sgp_dump_1_1_%s%s.gz" % (self.dump_prefix, self.candidate_timestamp, POST_DATA_SUFFIX))
            compress = CheckFile(compressed_file).run()
        return (self.candidate_timestamp, restore_db, compress)

class ValidateSegments(Operation):
    def __init__(self, restore_timestamp, compress, master_port, backup_dir, dump_dir, dump_prefix, netbackup_service_host):
        self.restore_timestamp = restore_timestamp
        self.compress = compress
        self.master_port = master_port
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.backup_dir = backup_dir
        self.netbackup_service_host = netbackup_service_host

    def execute(self):
        """ TODO: Improve with grouping by host and ParallelOperation dispatch. """
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
        primaries = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in primaries:
            if seg.isSegmentDown():
                """ Why must every Segment function have the word Segment in it ?! """
                raise ExceptionNoStackTraceNeeded("Host %s dir %s dbid %d marked as invalid" % (seg.getSegmentHostName(), seg.getSegmentDataDirectory(), seg.getSegmentDbId()))

            if self.netbackup_service_host is None:
                path = os.path.join(get_restore_dir(seg.getSegmentDataDirectory(), self.backup_dir), self.dump_dir, self.restore_timestamp[0:8])
                host = seg.getSegmentHostName()
                path = os.path.join(path, "%s0_%d_%s" % (generate_dbdump_prefix(self.dump_prefix), seg.getSegmentDbId(), self.restore_timestamp))
                if self.compress:
                    path += ".gz"
                exists = CheckRemotePath(path, host).run()
                if not exists:
                    raise ExceptionNoStackTraceNeeded("No dump file on %s at %s" % (seg.getSegmentHostName(), path))

def validate_tablenames(table_list, master_data_dir, backup_dir, dump_dir, dump_prefix, timestamp, schema_level_restore_list=None):
    """
    verify table list, and schema list, resolve duplicates and overlaps
    """

    restore_table_list = []
    table_set = set()

    # validate special characters
    check_funny_chars_in_names(schema_level_restore_list, is_full_qualified_name = False)
    check_funny_chars_in_names(table_list)

    # validate schemas
    if schema_level_restore_list:
        schema_level_restore_list = list(set(schema_level_restore_list))

    for restore_table in table_list:
        if '.' not in restore_table:
            raise Exception("No schema name supplied for %s, removing from list of tables to restore" % restore_table)
        schema, table = split_fqn(restore_table)
        # schema level restore will be handled before specific table restore, treat as duplicate
        if not ((schema_level_restore_list and schema in schema_level_restore_list) or (schema, table) in table_set):
            table_set.add((schema, table))
            restore_table_list.append(restore_table)

    # validate tables
    filename = generate_metadata_filename(master_data_dir, backup_dir, dump_dir, dump_prefix, timestamp)

    dumped_tables = []
    fd = gzip.open(filename, 'r')
    try:
        for line in fd:
            pattern = "-- Name: (.+?); Type: (.+?); Schema: (.+?);"
            match = search(pattern, line)
            if match is None:
                continue
            name, type, schema = match.group(1), match.group(2), match.group(3)
            if type == "TABLE":
                schema = pg.escape_string(schema)
                name = pg.escape_string(name)
                dumped_tables.append('%s.%s' % (schema, name))
    finally:
        fd.close()
    for table in restore_table_list:
        if table not in dumped_tables:
            raise Exception("Table %s not found in backup" % table)

    return restore_table_list, schema_level_restore_list 

class ValidateRestoreTables(Operation):
    def __init__(self, restore_tables, restore_db, master_port):
        self.restore_tables = restore_tables
        self.restore_db = restore_db
        self.master_port = master_port

    def execute(self):
        existing_tables = []
        table_counts = []
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.master_port, dbname=self.restore_db)
            conn = dbconn.connect(dburl)
            for restore_table in self.restore_tables:
                schema, table = split_fqn(restore_table)
                count = execSQLForSingleton(conn, "select count(*) from pg_class, pg_namespace where pg_class.relname = '%s' and pg_class.relnamespace = pg_namespace.oid and pg_namespace.nspname = '%s'" % (table, schema))
                if count == 0:
                    logger.warn("Table %s does not exist in database %s, removing from list of tables to restore" % (table, self.restore_db))
                    continue

                count = execSQLForSingleton(conn, "select count(*) from %s.%s" % (schema, table))
                if count > 0:
                    logger.warn('Table %s has %d records %s' % (restore_table, count, WARN_MARK))
                existing_tables.append(restore_table)
                table_counts.append((restore_table, count))
        finally:
            if conn is not None:
                conn.close()

        if len(existing_tables) == 0:
            raise ExceptionNoStackTraceNeeded("Have no tables to restore")
        logger.info("Have %d tables to restore, will continue" % len(existing_tables))

        return (existing_tables, table_counts)

class CopyPostData(Operation):
    ''' Copy _post_data when using fake timestamp.
        The same operation can be done with/without ddboost, because
        the _post_data file is always kept on the master, not on the dd server '''
    def __init__(self, restore_timestamp, fake_timestamp, compress, master_datadir, dump_dir, dump_prefix, backup_dir):
        self.restore_timestamp = restore_timestamp
        self.fake_timestamp = fake_timestamp
        self.compress = compress
        self.master_datadir = master_datadir
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.backup_dir = backup_dir

    def execute(self):
         # Build master _post_data file:
        restore_dir = get_restore_dir(self.master_datadir, self.backup_dir)
        real_post_data = os.path.join(restore_dir, self.dump_dir, self.restore_timestamp[0:8], "%s%s%s" % (generate_master_dbdump_prefix(self.dump_prefix), self.restore_timestamp, POST_DATA_SUFFIX))
        fake_post_data = os.path.join(restore_dir, self.dump_dir, self.fake_timestamp[0:8], "%s%s%s" % (generate_master_dbdump_prefix(self.dump_prefix), self.fake_timestamp, POST_DATA_SUFFIX))
        if self.compress:
            real_post_data = real_post_data + ".gz"
            fake_post_data = fake_post_data + ".gz"
        shutil.copy(real_post_data, fake_post_data)

class GetDbName(Operation):
    def __init__(self, createdb_file):
        self.createdb_file = createdb_file

    def execute(self):
        f = open(self.createdb_file, 'r')
        # assumption: 'CREATE DATABASE' line will reside within the first 50 lines of the gp_cdatabase_1_1_* file
        for _ in range(0, 50):
            line = f.readline()
            if not line:
                break
            if line.startswith("CREATE DATABASE"):
                restore_db = get_dbname_from_cdatabaseline(line)
                if restore_db is None:
                    raise Exception('Expected database name after CREATE DATABASE in line "%s" of file "%s"' % (line, self.createdb_file))
                return removeEscapingDoubleQuoteInSQLString(checkAndRemoveEnclosingDoubleQuote(restore_db), forceDoubleQuote=False)
        else:
            raise GetDbName.DbNameGiveUp()
        raise GetDbName.DbNameNotFound()

    class DbNameNotFound(Exception): pass
    class DbNameGiveUp(Exception): pass

class RecoverRemoteDumps(Operation):
    def __init__(self, host, path, restore_timestamp, compress, restore_global, batch_default, master_datadir, master_port, dump_dir, dump_prefix):
        self.host = host
        self.path = path
        self.restore_timestamp = restore_timestamp
        self.compress = compress
        self.restore_global = restore_global
        self.batch_default = batch_default
        self.master_datadir = master_datadir
        self.master_port = master_port
        self.pool = None
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix

    def execute(self):
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port=self.master_port), utility=True)
        from_host, from_path = self.host, self.path
        logger.info("Commencing remote database dump file recovery process, please wait...")
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True) or seg.isSegmentMaster()]
        self.pool = WorkerPool(numWorkers=min(len(segs), self.batch_default))
        for seg in segs:
            if seg.isSegmentMaster():
                file = '%s%s' % (generate_master_dbdump_prefix(self.dump_prefix), self.restore_timestamp)
            else:
                file = '%s0_%d_%s' % (generate_dbdump_prefix(self.dump_prefix), seg.getSegmentDbId(), self.restore_timestamp)
            if self.compress:
                file += '.gz'

            to_host = seg.getSegmentHostName()
            to_path = os.path.join(seg.getSegmentDataDirectory(), self.dump_dir, self.restore_timestamp[0:8])
            if not CheckRemoteDir(to_path, to_host).run():
                logger.info('Creating directory %s on %s' % (to_path, to_host))
                try:
                    MakeRemoteDir(to_path, to_host).run()
                except OSError, e:
                    raise ExceptionNoStackTraceNeeded("Failed to create directory %s on %s" % (to_path, to_host))

            logger.info("Commencing remote copy from %s to %s:%s" % (from_host, to_host, to_path))
            self.pool.addCommand(Scp('Copying dump for seg %d' % seg.getSegmentDbId(),
                                     srcFile=os.path.join(from_path, file),
                                     dstFile=os.path.join(to_path, file),
                                     srcHost=from_host,
                                     dstHost=to_host))
        createdb_file = '%s%s' % (generate_createdb_prefix(self.dump_prefix), self.restore_timestamp)
        to_path = os.path.join(self.master_datadir, self.dump_dir, self.restore_timestamp[0:8])
        self.pool.addCommand(Scp('Copying schema dump',
                                 srcHost=from_host,
                                 srcFile=os.path.join(from_path, createdb_file),
                                 dstFile=generate_createdb_filename(self.master_datadir, None, self.dump_dir, self.dump_prefix, self.restore_timestamp)))

        report_file = "%s%s.rpt" % (generate_dbdump_prefix(self.dump_prefix), self.restore_timestamp)
        self.pool.addCommand(Scp('Copying report file',
                                 srcHost=from_host,
                                 srcFile=os.path.join(from_path, report_file),
                                 dstFile=os.path.join(to_path, report_file)))

        post_data_file = "%s%s%s" % (generate_master_dbdump_prefix(self.dump_prefix), self.restore_timestamp, POST_DATA_SUFFIX)
        if self.compress:
            post_data_file += ".gz"
        self.pool.addCommand(Scp('Copying post data schema dump',
                            srcHost=from_host,
                            srcFile=os.path.join(from_path, post_data_file),
                            dstFile=os.path.join(to_path, post_data_file)))
        if self.restore_global:
            global_file = "%s%s" % (generate_global_prefix(self.dump_prefix), self.restore_timestamp)
            self.pool.addCommand(Scp("Copying global dump",
                                     srcHost=from_host,
                                     srcFile=os.path.join(from_path, global_file),
                                     dstFile=os.path.join(to_path, global_file)))
        self.pool.join()
        self.pool.check_results()

class GetDumpTables(Operation):
    def __init__(self, restore_timestamp, master_datadir, backup_dir, dump_dir, dump_prefix, ddboost, netbackup_service_host):
        self.master_datadir = master_datadir
        self.restore_timestamp = restore_timestamp
        self.dump_dir = dump_dir
        self.dump_prefix = dump_prefix
        self.backup_dir = backup_dir
        self.ddboost = ddboost
        self.netbackup_service_host = netbackup_service_host

    def execute(self):
        (restore_timestamp, restore_db, compress) = ValidateTimestamp(master_datadir = self.master_datadir,
                                                                      backup_dir = self.backup_dir,
                                                                      candidate_timestamp = self.restore_timestamp,
                                                                      dump_dir = self.dump_dir,
                                                                      dump_prefix = self.dump_prefix,
                                                                      netbackup_service_host = self.netbackup_service_host,
                                                                      ddboost = self.ddboost).run()
        dump_file = os.path.join(get_restore_dir(self.master_datadir, self.backup_dir), self.dump_dir, restore_timestamp[0:8], "%s%s" % (generate_master_dbdump_prefix(self.dump_prefix), restore_timestamp))
        if compress:
            dump_file += '.gz'

        if self.ddboost:
            from_file = os.path.join(self.dump_dir, restore_timestamp[0:8], "%s%s" % (generate_master_dbdump_prefix(self.dump_prefix), restore_timestamp))
            if compress:
                from_file += '.gz'
            ret = []
            schema = ''
            owner = ''
            if compress:
                cmd = Command('DDBoost copy of master dump file',
                              'gpddboost --readFile --from-file=%s | gunzip | grep -e "SET search_path = " -e "-- Data for Name: " -e "COPY "'
                              % (from_file))
            else:
                cmd = Command('DDBoost copy of master dump file',
                              'gpddboost --readFile --from-file=%s | grep -e "SET search_path = " -e "-- Data for Name: " -e "COPY "'
                              % (from_file))

            # TODO: The code below is duplicated from the code for local restore
            #       We need to refactor this. Probably use the same String IO interfaces
            #       to extract lines in both the cases.
            cmd.run(validateAfter=True)
            line_list = cmd.get_results().stdout.splitlines()
            for line in line_list:
                if line.startswith("SET search_path = "):
                    line = line[len("SET search_path = ") : ]
                    if ", pg_catalog;" in line:
                        schema = line[ : line.index(", pg_catalog;")]
                    else:
                        schema = "pg_catalog"
                elif line.startswith("-- Data for Name: "):
                    owner = line[line.index("; Owner: ") + 9 : ].rstrip()
                elif line.startswith("COPY "):
                    table = line[5:]
                    if table.rstrip().endswith(") FROM stdin;"):
                        if table.startswith("\""):
                            table = table[: table.index("\" (") + 1]
                        else:
                            table = table[: table.index(" (")]
                    else:
                        table = table[: table.index(" FROM stdin;")]
                    table = table.rstrip()
                    ret.append((schema, table, owner))
            return ret
        else:
            f = None
            schema = ''
            owner = ''
            ret = []
            try:
                if compress:
                    f = gzip.open(dump_file, 'r')
                else:
                    f = open(dump_file, 'r')

                while True:
                    line = f.readline()
                    if not line:
                        break
                    if line.startswith("SET search_path = "):
                        line = line[len("SET search_path = ") : ]
                        if ", pg_catalog;" in line:
                            schema = line[ : line.index(", pg_catalog;")]
                        else:
                            schema = "pg_catalog"
                    elif line.startswith("-- Data for Name: "):
                        owner = line[line.index("; Owner: ") + 9 : ].rstrip()
                    elif line.startswith("COPY "):
                        table = line[5:]
                        if table.rstrip().endswith(") FROM stdin;"):
                            if table.startswith("\""):
                                table = table[: table.index("\" (") + 1]
                            else:
                                table = table[: table.index(" (")]
                        else:
                            table = table[: table.index(" FROM stdin;")]
                        table = table.rstrip()
                        ret.append((schema, table, owner))
            finally:
                if f is not None:
                    f.close()
            return ret
