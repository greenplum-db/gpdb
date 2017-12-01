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
from gppylib.operations.backup_utils import *
from gppylib.operations.unix import CheckFile, CheckRemoteDir, MakeRemoteDir, CheckRemotePath
from re import compile, search, sub

"""
TODO: partial restore. In 4.x, dump will only occur on primaries.
So, after a dump, dump files must be pushed to mirrors. (This is a task for gpcrondump.)
"""

""" TODO: centralize logging """
logger = gplog.get_default_logger()

WARN_MARK = '<<<<<'
# TODO: use CLI-agnostic custom exceptions instead of ExceptionNoStackTraceNeeded

def update_ao_stat_func(context, conn, ao_schema, ao_table, counter, batch_size):
    qry = "SELECT * FROM gp_update_ao_master_stats('%s.%s')" % (escape_string(escapeDoubleQuoteInSQLString(ao_schema), conn),
                                                                escape_string(escapeDoubleQuoteInSQLString(ao_table), conn))
    rows = execSQLForSingleton(conn, qry)
    if counter % batch_size == 0:
        conn.commit()

def generate_restored_tables(results, restored_tables, restored_schema, restore_all):
    restored_ao_tables = set()

    for (tbl, sch) in results:
        if restore_all:
            restored_ao_tables.add((sch, tbl))
        elif sch in restored_schema:
            restored_ao_tables.add((sch, tbl))
        else:
            tblname = '%s.%s' % (sch, tbl)
            if tblname in restored_tables:
                restored_ao_tables.add((sch, tbl))

    return restored_ao_tables

def update_ao_statistics(context, restored_tables, restored_schema=[], restore_all=False):
    # Restored schema is different from restored tables as restored schema updates all tables within that schema.
    qry = """SELECT c.relname,n.nspname
             FROM pg_class c, pg_namespace n
             WHERE c.relnamespace=n.oid
                 AND (c.relstorage='a' OR c.relstorage='c')"""

    counter = 1

    try:
        results = execute_sql(qry, context.master_port, context.target_db)
        restored_ao_tables = generate_restored_tables(results, restored_tables, restored_schema, restore_all)
        if len(restored_ao_tables) == 0:
            logger.info("No AO/CO tables restored, skipping statistics update...")
            return

        with dbconn.connect(dbconn.DbURL(port=context.master_port, dbname=context.target_db)) as conn:
            for ao_schema, ao_table in sorted(restored_ao_tables):
                update_ao_stat_func(context, conn, ao_schema, ao_table, counter, batch_size=1000)
                counter = counter + 1
            conn.commit()
    except Exception as e:
        logger.info("Error updating ao statistics after restore")
        raise e

def get_restore_tables_from_table_file(table_file):
    if not os.path.isfile(table_file):
        raise Exception('Table file does not exist "%s"' % table_file)

    return get_lines_from_file(table_file)

def get_incremental_restore_timestamps(context):
    inc_file = context.generate_filename("increments", timestamp=context.full_dump_timestamp)
    timestamps = get_lines_from_file(inc_file)
    sorted_timestamps = sorted(timestamps, key=lambda x: int(x), reverse=True)
    incremental_restore_timestamps = []
    try:
        incremental_restore_timestamps = sorted_timestamps[sorted_timestamps.index(context.timestamp):]
    except ValueError:
        pass
    return incremental_restore_timestamps

def get_partition_list(context):
    partition_list_file = context.generate_filename("partition_list")
    partition_list = get_lines_from_file(partition_list_file)
    partition_list = [split_fqn(p) for p in partition_list]
    return partition_list

def get_dirty_table_file_contents(context, timestamp):
    dirty_list_file = context.generate_filename("dirty_table", timestamp=timestamp)
    return get_lines_from_file(dirty_list_file)

def create_plan_file_contents(context, table_set_from_metadata_file, incremental_restore_timestamps, full_timestamp):
    restore_set = {}
    for ts in incremental_restore_timestamps:
        restore_set[ts] = []
        if context.netbackup_service_host:
            restore_file_with_nbu(context, "dirty_table", timestamp=ts)
        dirty_tables = get_dirty_table_file_contents(context, ts)
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

    return lines_to_write

def create_restore_plan(context):
    dump_tables = get_partition_list(context)

    table_set_from_metadata_file = [schema + '.' + table for schema, table in dump_tables]

    incremental_restore_timestamps = get_incremental_restore_timestamps(context)

    plan_file_contents = create_plan_file_contents(context, table_set_from_metadata_file,
                                                   incremental_restore_timestamps,
                                                   full_timestamp=context.full_dump_timestamp)

    plan_file = context.generate_filename("plan")

    write_to_plan_file(plan_file_contents, plan_file)

    return plan_file

def is_incremental_restore(context):
    filename = context.generate_filename("report")
    if not os.path.isfile(filename):
        logger.warn('Report file %s does not exist for restore timestamp %s' % (filename, context.timestamp))
        return False

    report_file_contents = get_lines_from_file(filename)
    if check_backup_type(report_file_contents, 'Incremental'):
        return True
    return False

def is_full_restore(context):
    filename = context.generate_filename("report")
    if not os.path.isfile(filename):
        raise Exception('Report file %s does not exist for restore timestamp %s' % (filename, context.timestamp))

    report_file_contents = get_lines_from_file(filename)
    if check_backup_type(report_file_contents, 'Full'):
        return True

    return False

def get_plan_file_contents(context):
    plan_file_items = []
    plan_file = context.generate_filename("plan")
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

def validate_restore_tables_list(plan_file_contents, restore_tables, restore_schemas=None):
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
        if restore_schemas and schema_name in restore_schemas:
            continue
        else:
            comp_set.add(table)
            if not comp_set.issubset(table_set):
                invalid_tables.append(table)
                comp_set.remove(table)

    if invalid_tables != []:
        raise Exception('Invalid tables for -T option: The following tables were not found in plan file : "%s"' % (invalid_tables))

#NetBackup related functions
def restore_state_files_with_nbu(context):
    restore_file_with_nbu(context, "ao")
    restore_file_with_nbu(context, "co")
    restore_file_with_nbu(context, "last_operation")

def restore_config_files_with_nbu(context):
    restore_file_with_nbu(context, "master_config")

    primaries = context.get_current_primaries()
    for seg in primaries:
        seg_config_filename = context.generate_filename("segment_config", dbid=seg.getSegmentDbId())
        seg_host = seg.getSegmentHostName()
        restore_file_with_nbu(context, path=seg_config_filename, hostname=seg_host)

def _build_gpdbrestore_cmd_line(context, ts, table_file):
    cmd = 'gpdbrestore -t %s --table-file %s -a -v --noplan --noanalyze --noaostats --no-validate-table-name' % (ts, table_file)
    if context.backup_dir:
        cmd += " -u %s" % context.backup_dir
    if context.dump_prefix:
        cmd += " --prefix=%s" % context.dump_prefix.strip('_')
    if context.redirected_restore_db:
        cmd += " --redirect=%s" % context.redirected_restore_db
    if context.report_status_dir:
        cmd += " --report-status-dir=%s" % context.report_status_dir
    if context.ddboost:
        cmd += " --ddboost"
    if context.ddboost_storage_unit:
        cmd += " --ddboost-storage-unit=%s" % context.ddboost_storage_unit
    if context.netbackup_service_host:
        cmd += " --netbackup-service-host=%s" % context.netbackup_service_host
    if context.netbackup_block_size:
        cmd += " --netbackup-block-size=%s" % context.netbackup_block_size
    if context.change_schema:
        cmd += " --change-schema=%s" % context.change_schema

    return cmd

class RestoreDatabase(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        if self.context.batch_default <= 0:
            raise Exception("-B <parallel processes> must be greater than 0")

        if self.context.redirected_restore_db:
            self.context.target_db = self.context.redirected_restore_db

        if (len(self.context.restore_tables) > 0 or len(self.context.restore_schemas) > 0) and self.context.truncate:
            self.truncate_restore_tables()

        if not self.context.ddboost:
            ValidateSegments(self.context).run()

        if self.context.redirected_restore_db and not self.context.drop_db:
            self.create_database_if_not_exists()
            self.create_gp_toolkit()

        if self.context.restore_stats == "only":
            self._restore_stats()
            return

        if self.context.restore_global:
            self._restore_global(self.context)
            if self.context.restore_global == "only":
                return

        if self.context.drop_db:
            self._multitry_createdb()

        """
        For full restore with table filter or for the first recurssion of the incremental restore
        we first restore the schema, expand the parent partition table name's in the restore table
        filter to include leaf partition names, and then restore data (only, using '-a' option).
        """

        full_restore_with_filter = False
        full_restore = is_full_restore(self.context)
        begin_incremental = (is_incremental_restore(self.context) and not self.context.no_plan)

        table_filter_file = self.create_filter_file() # returns None if nothing to filter
        change_schema_file = self.create_change_schema_file() # returns None if nothing to filter
        schema_level_restore_file = self.create_schema_level_file()

        if (full_restore and len(self.context.restore_tables) > 0 and not self.context.no_plan) or begin_incremental or self.context.metadata_only:
            if full_restore and not self.context.no_plan:
                full_restore_with_filter = True

            restore_line = self.create_schema_only_restore_string(table_filter_file, full_restore_with_filter, change_schema_file, schema_level_restore_file)
            logger.info("Running metadata restore")
            logger.info("Invoking commandline: %s" % restore_line)
            cmd = Command('Invoking gp_restore', restore_line)
            cmd.run(validateAfter=False)
            self._process_result(cmd)
            logger.info("Expanding parent partitions if any in table filter")
            self.context.restore_tables = expand_partition_tables(self.context, self.context.restore_tables)

        if begin_incremental:
            logger.info("Running data restore")
            self.restore_incremental_data_only()
        else:
            table_filter_file = self.create_filter_file()
            if not self.context.metadata_only:
                restore_line = self.create_standard_restore_string(table_filter_file, full_restore_with_filter, change_schema_file, schema_level_restore_file)
                logger.info('gp_restore commandline: %s: ' % restore_line)
                cmd = Command('Invoking gp_restore', restore_line)
                cmd.run(validateAfter=False)
                self._process_result(cmd)

            if full_restore_with_filter:
                restore_line = self.create_post_data_schema_only_restore_string(table_filter_file, full_restore_with_filter, change_schema_file, schema_level_restore_file)
                logger.info("Running post data restore")
                logger.info('gp_restore commandline: %s: ' % restore_line)
                cmd = Command('Invoking gp_restore', restore_line)
                cmd.run(validateAfter=False)
                self._process_result(cmd)

            restore_all=False
            if not self.context.no_ao_stats:
                logger.info("Updating AO/CO statistics on master")
                # If we don't have a filter for table and schema, then we must be doing a full restore.
                if len(self.context.restore_schemas) == 0 and len(self.context.restore_tables) == 0:
                    restore_all=True
                update_ao_statistics(self.context, self.context.restore_tables, self.context.restore_schemas, restore_all=restore_all)

        if not self.context.metadata_only:
            if (not self.context.no_analyze) and len(self.context.restore_tables) == 0 and len(self.context.restore_schemas) == 0:
                self._analyze(self.context)
            elif (not self.context.no_analyze) and (len(self.context.restore_tables) > 0 or len(self.context.restore_schemas) > 0):
                self._analyze_restore_tables()
        if self.context.restore_stats:
            self._restore_stats()

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
                remove_file_on_segments(self.context, tmp_file)

    def _analyze(self, context):
        conn = None
        logger.info('Commencing analyze of %s database, please wait' % context.target_db)
        try:
            dburl = dbconn.DbURL(port=context.master_port, dbname=context.target_db)
            conn = dbconn.connect(dburl)
            execSQL(conn, 'analyze')
            conn.commit()
        except Exception, e:
            logger.warn('Issue with analyze of %s database' % context.target_db)
        else:
            logger.info('Analyze of %s completed without error' % context.target_db)
        finally:
            if conn:
                conn.close()

    def _analyze_restore_tables(self):
        logger.info('Commencing analyze of restored tables in \'%s\' database, please wait' % self.context.target_db)
        batch_count = 0
        try:
            with dbconn.connect(dbconn.DbURL(port=self.context.master_port, dbname=self.context.target_db)) as conn:
                num_sqls = 0
                analyze_list = []
                # need to find out all tables under the schema and construct the new schema.table to analyze
                if self.context.change_schema and self.context.restore_schemas:
                    schemaname = self.context.change_schema
                    analyze_list = self.get_full_tables_in_schema(conn, schemaname)
                elif self.context.restore_schemas:
                    for schemaname in self.context.restore_schemas:
                        analyze_list.extend(self.get_full_tables_in_schema(conn, schemaname))
                else:
                    for restore_table in self.context.restore_tables:
                        schemaname, tablename = split_fqn(restore_table)
                        if self.context.change_schema:
                            schema = escapeDoubleQuoteInSQLString(self.context.change_schema)
                        else:
                            schema = escapeDoubleQuoteInSQLString(schemaname)
                        table = escapeDoubleQuoteInSQLString(tablename)
                        restore_table = '%s.%s' % (schema, table)
                        analyze_list.append(restore_table)

                for tbl in analyze_list:
                    analyze_table = "analyze " + tbl
                    try:
                        execSQL(conn, analyze_table)
                    except Exception as e:
                        raise Exception('Issue with \'ANALYZE\' of restored table \'%s\' in \'%s\' database' % (tbl, self.context.target_db))
                    else:
                        num_sqls += 1
                        if num_sqls == 1000: # The choice of batch size was choosen arbitrarily
                            batch_count +=1
                            logger.debug('Completed executing batch of 1000 tuple count SQLs')
                            conn.commit()
                            num_sqls = 0
        except Exception as e:
            logger.warn('Restore of \'%s\' database succeeded but \'ANALYZE\' of restored tables failed' % self.context.target_db)
            logger.warn('Please run ANALYZE manually on restored tables. Failure to run ANALYZE might result in poor database performance')
            raise Exception(str(e))
        else:
            logger.info('\'Analyze\' of restored tables in \'%s\' database completed without error' % self.context.target_db)
        return batch_count

    def get_full_tables_in_schema(self, conn, schemaname):
        res = []
        get_all_tables_qry = "select schemaname, tablename from pg_tables where schemaname = '%s';" % escape_string(schemaname, conn)
        relations = execSQL(conn, get_all_tables_qry)
        for relation in relations:
            schema, table = relation[0], relation[1]
            schema = escapeDoubleQuoteInSQLString(schema)
            table = escapeDoubleQuoteInSQLString(table)
            restore_table = '%s.%s' % (schema, table)
            res.append(restore_table)
        return res

    def create_schema_level_file(self):
        if not self.context.restore_schemas:
            return None

        schema_level_restore_file = create_temp_file_with_schemas(list(self.context.restore_schemas))

        addresses = get_all_segment_addresses(self.context)

        scp_file_to_hosts(addresses, schema_level_restore_file, self.context.batch_default)

        return schema_level_restore_file

    def create_change_schema_file(self):
        if not self.context.change_schema:
            return None

        schema_list = [self.context.change_schema]

        change_schema_file = create_temp_file_with_schemas(schema_list)

        addresses = get_all_segment_addresses(self.context)

        scp_file_to_hosts(addresses, change_schema_file, self.context.batch_default)

        return change_schema_file

    def create_filter_file(self):
        if not self.context.restore_tables or len(self.context.restore_tables) == 0:
            return None

        table_filter_file = create_temp_file_with_tables(self.context.restore_tables)

        addresses = get_all_segment_addresses(self.context)

        scp_file_to_hosts(addresses, table_filter_file, self.context.batch_default)

        return table_filter_file


    def restore_incremental_data_only(self):
        restore_data = False
        plan_file_items = get_plan_file_contents(self.context)
        table_files = []
        restored_tables = []

        validate_restore_tables_list(plan_file_items, self.context.restore_tables, self.context.restore_schemas)

        for (ts, table_list) in plan_file_items:
            if table_list:
                restore_data = True
                table_file = get_restore_table_list(table_list.strip('\n').split(','), self.context.restore_tables)
                if table_file is None:
                    continue
                cmd = _build_gpdbrestore_cmd_line(self.context, ts, table_file)
                logger.info('Invoking commandline: %s' % cmd)
                Command('Invoking gpdbrestore', cmd).run(validateAfter=True)
                table_files.append(table_file)
                restored_tables.extend(get_restore_tables_from_table_file(table_file))

        if not restore_data:
            raise Exception('There were no tables to restore. Check the plan file contents for restore timestamp %s' % self.context.timestamp)

        if not self.context.no_ao_stats:
            logger.info("Updating AO/CO statistics on master")
            update_ao_statistics(self.context, restored_tables)
        else:
            logger.info("noaostats enabled. Skipping update of AO/CO statistics on master.")

        for table_file in table_files:
            if table_file:
                os.remove(table_file)

        return True

    def _restore_global(self, context):
        logger.info('Commencing restore of global objects')
        global_file = context.generate_filename("global")
        if not os.path.exists(global_file):
            raise Exception('Unable to locate global file %s in dump set' % (global_file))
        Psql('Invoking global dump', filename=global_file).run(validateAfter=True)

    def _restore_stats(self):
        logger.info('Commencing restore of statistics')
        stats_filename = self.context.generate_filename("stats", directory="/tmp")
        stats_path = self.context.generate_filename("stats")
        if not os.path.exists(stats_path):
            raise Exception('Unable to locate statistics file %s in dump set' % stats_filename)

        # We need to replace existing starelid's in file to match starelid of tables in database in case they're different
        # and modify attnum's in case columns were dropped between the backup and restore.
        # First, map each schemaname.tablename to its corresponding starelid and attnums
        query = """SELECT t.schemaname || '.' || t.tablename, c.oid, a.attname, a.attnum FROM pg_class c JOIN pg_tables t ON c.relname = t.tablename JOIN pg_attribute a ON a.attrelid = c.oid
                    WHERE t.schemaname NOT IN ('pg_toast', 'pg_bitmapindex', 'pg_temp_1', 'pg_catalog', 'information_schema', 'gp_toolkit') AND a.attnum > 0;"""
        relids = {}
        attnums = {}
        rows = execute_sql(query, self.context.master_port, self.context.target_db)
        # The query returns rows of (schema.table, starelid, attname, attnum).
        # We construct maps of schema.table to starelid and schema.table.att to attnum
        for row in rows:
            if len(row) != 4:
                raise Exception("Invalid return from query: Expected 4 columns, got % columns" % (len(row)))
            relids[row[0]] = str(row[1])
            full_attname = '%s.%s' % (row[0], row[2])
            attnums[full_attname] = row[3]

        # Read in the statistics dump file, find each schemaname.tablename section, and replace the corresponding starelid
        # This section is also where we filter out tables that are not in restore_tables
        with open(stats_filename, "w") as outfile:
            with open(stats_path, "r") as infile:
                table_attribute_pattern = compile("-- Schema: (\w+), Table: (\w+), Attribute: (\w+)")
                table_pattern_pg_class = compile("-- Schema: (\w+), Table: (\w+)")
                print_toggle = True
                replace_toggle = False
                new_oid = ""
                new_attnum = ""
                for line in infile:
                    matches = search(table_attribute_pattern, line)
                    pg_class_match = search(table_pattern_pg_class, line)
                    if matches:
                        tablename = '%s.%s' % (matches.group(1), matches.group(2))
                        attname = '%s.%s' % (tablename, matches.group(3))
                        if len(self.context.restore_tables) == 0 or tablename in self.context.restore_tables:
                            try:
                                new_oid = relids[tablename]
                                new_attnum = attnums[attname]
                                print_toggle = True
                                replace_toggle = True
                            except KeyError as e:
                                if "Attribute" not in line: # Only print a warning once per table, at the tuple count restore section
                                    logger.warning("Cannot restore statistics for table %s: Table does not exist.  Skipping...", tablename)
                                print_toggle = False
                                replace_toggle = False
                        else:
                            print_toggle = False
                    elif pg_class_match:
                        tablename = '%s.%s' % (pg_class_match.group(1), pg_class_match.group(2))
                        if len(self.context.restore_tables) == 0 or tablename in self.context.restore_tables:
                            print_toggle=True
                        else:
                            print_toggle=False
                    elif replace_toggle and "DELETE FROM" in line:
                        line = re.sub("starelid = (\w+)", "starelid = %s" % new_oid, line)
                        line = re.sub("staattnum = (\w+)", "staattnum = %s" % new_attnum, line)
                    elif replace_toggle and "::oid" in line:
                        line = "    %s::oid,\n" % new_oid
                    elif replace_toggle and "::smallint" in line:
                        line = "    %s::smallint,\n" % new_attnum
                        replace_toggle = False
                    if print_toggle:
                        outfile.write(line)

        Psql('Invoking statistics restore', filename=stats_filename, database=self.context.target_db).run(validateAfter=True)

    def _multitry_createdb(self):
        no_of_trys = 600
        for _ in range(no_of_trys):
            try:
                self._process_createdb()
            except ExceptionNoStackTraceNeeded:
                time.sleep(1)
            else:
                return
        raise ExceptionNoStackTraceNeeded('Failed to drop database %s' % self.context.target_db)

    def drop_database_if_exists(self):
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.context.master_port, dbname='template1')
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_database where datname='%s';" % escape_string(self.context.target_db, conn))

            logger.info("Dropping Database %s" % self.context.target_db)
            if count == 1:
                cmd = Command(name='drop database %s' % self.context.target_db,
                              cmdStr='dropdb %s -p %s' % (checkAndAddEnclosingDoubleQuote(shellEscape(self.context.target_db)), self.context.master_port))
                cmd.run(validateAfter=True)
            logger.info("Dropped Database %s" % self.context.target_db)
        except ExecutionError, e:
            logger.exception("Could not drop database %s" % self.context.target_db)
            raise ExceptionNoStackTraceNeeded('Failed to drop database %s' % self.context.target_db)
        finally:
            if conn:
                conn.close()

    def create_database_if_not_exists(self):
        conn = None
        try:
            dburl = dbconn.DbURL(port=self.context.master_port, dbname='template1')
            conn = dbconn.connect(dburl)
            count = execSQLForSingleton(conn, "select count(*) from pg_database where datname='%s';" % escape_string(self.context.target_db, conn))

            logger.info("Creating Database %s" % self.context.target_db)
            if count == 0:
                cmd = Command(name='create database %s' % self.context.target_db,
                              cmdStr='createdb %s -p %s -T template0' % (checkAndAddEnclosingDoubleQuote(shellEscape(self.context.target_db)), self.context.master_port))
                cmd.run(validateAfter=True)
            logger.info("Created Database %s" % self.context.target_db)
        except ExecutionError, e:
            logger.exception("Could not create database %s" % self.context.target_db)
            raise ExceptionNoStackTraceNeeded('Failed to create database %s' % self.context.target_db)
        finally:
            if conn:
                conn.close()

    def check_gp_toolkit(self):
        GP_TOOLKIT_QUERY = """SELECT count(*)
                              FROM pg_class pgc, pg_namespace pgn
                              WHERE pgc.relnamespace=pgn.oid AND
                                    pgn.nspname='gp_toolkit'
                           """
        with dbconn.connect(dbconn.DbURL(dbname=self.context.target_db, port=self.context.master_port)) as conn:
            res = dbconn.execSQLForSingleton(conn, GP_TOOLKIT_QUERY)
            if res == 0:
                return False
            return True

    def create_gp_toolkit(self):
        if not self.check_gp_toolkit():
            if 'GPHOME' not in os.environ:
                logger.warn('Please set $GPHOME in your environment')
                logger.warn('Skipping creation of gp_toolkit since $GPHOME/share/postgresql/gp_toolkit.sql could not be found')
            else:
                logger.info('Creating gp_toolkit schema for database "%s"' % self.context.target_db)
                Psql(name='create gp_toolkit',
                     filename=os.path.join(os.environ['GPHOME'],
                                           'share', 'postgresql',
                                           'gp_toolkit.sql'),
                     database=self.context.target_db).run(validateAfter=True)

    def _process_createdb(self):
        self.drop_database_if_exists()
        if self.context.redirected_restore_db:
            self.create_database_if_not_exists()
        else:
            createdb_file = self.context.generate_filename("cdatabase")
            logger.info('Invoking sql file: %s' % createdb_file)
            Psql('Invoking schema dump', filename=createdb_file).run(validateAfter=True)
        self.create_gp_toolkit()

    def backup_dir_is_writable(self):
        if self.context.backup_dir and not self.context.report_status_dir:
            try:
                directory = self.context.get_backup_dir()
                check_dir_writable(directory)
            except Exception as e:
                logger.warning('Backup directory %s is not writable. Error %s' % (directory, str(e)))
                logger.warning('Since --report-status-dir option is not specified, report and status file will be written in segment data directory.')
                return False
        return True

    def create_restore_string(self, table_filter_file, full_restore_with_filter, change_schema_file=None, schema_level_restore_file=None):
        user = getpass.getuser()
        hostname = socket.gethostname()
        (gpr_path, status_path, gpd_path) = self.get_restore_line_paths()

        restore_line = "gp_restore -i -h %s -p %s -U %s --gp-d=%s --gp-i" % (hostname, self.context.master_port, user, gpd_path)
        restore_line += " --gp-k=%s --gp-l=p" % (self.context.timestamp)

        if gpr_path and status_path:
            restore_line += " --gp-r=%s" % gpr_path
            restore_line += " --status=%s" % status_path

        if self.context.use_old_filename_format:
            restore_line += " --old-format "

        if self.context.dump_prefix:
            logger.info("Adding --prefix")
            restore_line += " --prefix=%s" % self.context.dump_prefix
        if table_filter_file:
            restore_line += " --gp-f=%s" % table_filter_file
        if self.context.compress:
            restore_line += " --gp-c"
        restore_line += " -d %s" % checkAndAddEnclosingDoubleQuote(shellEscape(self.context.target_db))

        if self.context.ddboost:
            restore_line += " --ddboost"
        if self.context.ddboost_storage_unit:
            restore_line += " --ddboost-storage-unit=%s" % self.context.ddboost_storage_unit
        if self.context.netbackup_service_host:
            restore_line += " --netbackup-service-host=%s" % self.context.netbackup_service_host
        if self.context.netbackup_block_size:
            restore_line += " --netbackup-block-size=%s" % self.context.netbackup_block_size
        if change_schema_file:
            restore_line += " --change-schema-file=%s" % change_schema_file
        if schema_level_restore_file:
            restore_line += " --schema-level-file=%s" % schema_level_restore_file

        return restore_line

    def create_standard_restore_string(self, table_filter_file, full_restore_with_filter, change_schema_file=None, schema_level_restore_file=None):
        restore_line = self.create_restore_string(table_filter_file, full_restore_with_filter, change_schema_file, schema_level_restore_file)
        if self.context.no_plan or full_restore_with_filter:
            restore_line += " -a"
        if self.context.no_ao_stats:
            restore_line += " --gp-nostats"

        return restore_line

    def create_post_data_schema_only_restore_string(self, table_filter_file, full_restore_with_filter, change_schema_file=None, schema_level_restore_file=None):
        restore_line = self.create_restore_string(table_filter_file, full_restore_with_filter, change_schema_file, schema_level_restore_file)
        if full_restore_with_filter:
            restore_line += " -P"

        return restore_line

    def create_schema_only_restore_string(self, table_filter_file, full_restore_with_filter, change_schema_file=None, schema_level_restore_file=None):
        restore_line = self.create_restore_string(table_filter_file, full_restore_with_filter, change_schema_file, schema_level_restore_file)
        metadata_filename = self.context.generate_filename("metadata")
        restore_line += " -s %s" % metadata_filename
        if full_restore_with_filter:
            restore_line += " -P"

        return restore_line

    def get_restore_line_paths(self):
        (gpr_path, status_path, gpd_path) = (None, None, None)

        gpd_path = self.context.get_gpd_path()

        if self.context.report_status_dir:
            gpr_path = self.context.report_status_dir
            status_path = self.context.report_status_dir
        elif self.context.backup_dir and self.context.backup_dir_is_writable():
            gpr_path = gpd_path
            status_path = gpd_path

        if self.context.ddboost:
            gpd_path = "%s/%s" % (self.context.dump_dir, self.context.timestamp[0:8])

        return (gpr_path, status_path, gpd_path)

    def truncate_restore_tables(self):
        """
        Truncate either specific table or all tables under a schema
        """

        conn = None
        try:
            dburl = dbconn.DbURL(port=self.context.master_port, dbname=self.context.target_db)
            conn = dbconn.connect(dburl)
            truncate_list = []

            if self.context.restore_schemas:
                for schemaname in self.context.restore_schemas:
                    truncate_list.extend(self.get_full_tables_in_schema(conn, schemaname))
            else:
                for restore_table in self.context.restore_tables:
                    schemaname, tablename = split_fqn(restore_table)
                    check_table_exists_qry = """SELECT EXISTS (
                                                       SELECT 1
                                                       FROM pg_catalog.pg_class c
                                                       JOIN pg_catalog.pg_namespace n on n.oid = c.relnamespace
                                                       WHERE n.nspname = '%s' and c.relname = '%s')""" % (escape_string(schemaname, conn),
                                                                                                          escape_string(tablename, conn))
                    exists_result = execSQLForSingleton(conn, check_table_exists_qry)
                    if exists_result:
                        schema = escapeDoubleQuoteInSQLString(schemaname)
                        table = escapeDoubleQuoteInSQLString(tablename)
                        truncate_table = '%s.%s' % (schema, table)
                        truncate_list.append(truncate_table)
                    else:
                        logger.warning("Skipping truncate of %s.%s because the relation does not exist." % (self.context.target_db, restore_table))

            for table in truncate_list:
                try:
                    qry = 'Truncate %s' % table
                    execSQL(conn, qry)
                except Exception as e:
                    raise Exception("Could not truncate table %s.%s: %s" % (self.context.target_db, table, str(e).replace('\n', '')))

            conn.commit()
        except Exception as e:
            raise Exception("Failure from truncating tables, %s" % (str(e).replace('\n', '')))
        finally:
            if conn:
                conn.close()

class ValidateTimestamp(Operation):
    def __init__(self, context):
        self.context = context

    def validate_metadata_file(self):
        report_file = self.context.generate_filename("report")
        self.context.use_old_filename_format = self.context.is_timestamp_in_old_format()
        self.context.compress, self.context.target_db = self.context.get_compress_and_dbname_from_report_file(report_file)

        if self.context.ddboost:
            # We pass in segment_dir='' because we don't want it included in our path for ddboost
            ddboost_parent_dir = self.context.get_backup_dir(segment_dir='')
            expected_metadata_file = self.context.generate_filename("metadata", directory=ddboost_parent_dir)
            try:
                metadata_contents = get_lines_from_dd_file(expected_metadata_file, self.context.ddboost_storage_unit)
            except:
                raise ExceptionNoStackTraceNeeded('Unable to find %s. Skipping restore.' % expected_metadata_file)
        else:
            expected_metadata_file = self.context.generate_filename("metadata")
            if self.context.netbackup_service_host:
                logger.info('Backup for given timestamp was performed using NetBackup. Querying NetBackup server to check for the dump file.')
                restore_file_with_nbu(self.context, "metadata")
            if not os.path.exists(expected_metadata_file):
                 raise ExceptionNoStackTraceNeeded('Unable to find %s. Skipping restore.' % expected_metadata_file)

    def validate_timestamp_format(self):
        if not self.context.timestamp:
            raise Exception('Timestamp must not be None.')
        else:
            # timestamp has to be a string of 14 digits(YYYYMMDDHHMMSS)
            timestamp_pattern = compile(r'\d{14}')
            if not search(timestamp_pattern, self.context.timestamp):
                raise Exception('Invalid timestamp specified, please specify in the following format: YYYYMMDDHHMMSS.')

    def execute(self):
        self.validate_timestamp_format()
        self.validate_metadata_file()
        return self.context.target_db

class ValidateSegments(Operation):
    def __init__(self, context):
        self.context = context

    def execute(self):
        """ TODO: Improve with grouping by host and ParallelOperation dispatch. """
        self.context.use_old_filename_format = self.context.is_timestamp_in_old_format()
        primaries = self.context.get_current_primaries()
        for seg in primaries:
            if seg.isSegmentDown():
                """ Why must every Segment function have the word Segment in it ?! """
                raise ExceptionNoStackTraceNeeded("Host %s dir %s dbid %d marked as invalid" % (seg.getSegmentHostName(), seg.getSegmentDataDirectory(), seg.getSegmentDbId()))

            if self.context.netbackup_service_host is None:
                user_or_default_dir = self.context.get_backup_dir(segment_dir=seg.getSegmentDataDirectory())
                remote_file = get_filename_for_content(self.context, "dump", seg.getSegmentContentId(),
                                                       user_or_default_dir, seg.getSegmentHostName())
                if not remote_file:
                    raise ExceptionNoStackTraceNeeded("No dump file on %s in %s" % (seg.getSegmentHostName(), user_or_default_dir))

def check_table_name_format_and_duplicate(table_list, restore_schemas=None):
    """
    verify table list, and schema list, resolve duplicates and overlaps
    """

    restore_table_list = []
    table_set = set()

    # validate special characters
    check_funny_chars_in_names(restore_schemas, is_full_qualified_name = False)
    check_funny_chars_in_names(table_list)

    # validate schemas
    if restore_schemas:
        restore_schemas = list(set(restore_schemas))

    for restore_table in table_list:
        if '.' not in restore_table:
            raise Exception("No schema name supplied for %s, removing from list of tables to restore" % restore_table)
        schema, table = split_fqn(restore_table)
        # schema level restore will be handled before specific table restore, treat as duplicate
        if not ((restore_schemas and schema in restore_schemas) or (schema, table) in table_set):
            table_set.add((schema, table))
            restore_table_list.append(restore_table)

    return restore_table_list, restore_schemas

def validate_tablenames_exist_in_dump_file(restore_tables, dumped_tables):
    unmatched_table_names = []
    if dumped_tables:
        dumped_table_names = [schema + '.' + table for (schema, table, _) in dumped_tables]
        for table in restore_tables:
            if table not in dumped_table_names:
                unmatched_table_names.append(table)
    else:
        raise Exception('No dumped tables to restore.')

    if len(unmatched_table_names) > 0:
        raise Exception("Tables %s not found in backup" % unmatched_table_names)

class CopyPostData(Operation):
    ''' Copy _post_data when using fake timestamp.
        The same operation can be done with/without ddboost, because
        the _post_data file is always kept on the master, not on the dd server '''
    def __init__(self, context, fake_timestamp):
        self.fake_timestamp = fake_timestamp
        self.context = context

    def execute(self):
         # Build master _post_data file:
        real_post_data = self.context.generate_filename("postdata")
        fake_post_data = self.context.generate_filename("postdata", timestamp=self.fake_timestamp)
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
    def __init__(self, context, host, path):
        self.host = host
        self.path = path
        self.context = context

    def execute(self):
        from_host, from_path = self.host, self.path
        logger.info("Commencing remote database dump file recovery process, please wait...")
        segs = [seg for seg in self.context.gparray.getDbList() if seg.isSegmentPrimary(current_role=True) or seg.isSegmentMaster()]
        self.pool = WorkerPool(numWorkers=min(len(segs), self.context.batch_default))
        for seg in segs:
            to_host = seg.getSegmentHostName()
            to_path = self.context.get_backup_dir(segment_dir=seg.getSegmentDataDirectory())

            if seg.isSegmentMaster():
                from_file = self.context.generate_filename("metadata")
                to_file = self.context.generate_filename("metadata", directory=to_path)
            else:
                from_file = self.context.generate_filename("dump", dbid=seg.getSegmentDbId())
                to_file = self.context.generate_filename("dump", dbid=seg.getSegmentDbId(), directory=to_path)

            if not CheckRemoteDir(to_path, to_host).run():
                logger.info('Creating directory %s on %s' % (to_path, to_host))
                try:
                    MakeRemoteDir(to_path, to_host).run()
                except OSError, e:
                    raise ExceptionNoStackTraceNeeded("Failed to create directory %s on %s" % (to_path, to_host))

            logger.info("Commencing remote copy from %s to %s:%s" % (from_host, to_host, to_path))
            self.pool.addCommand(Scp('Copying dump for seg %d' % seg.getSegmentDbId(),
                                     srcFile=from_file,
                                     dstFile=to_file,
                                     srcHost=from_host,
                                     dstHost=to_host))
        self.pool.addCommand(Scp('Copying schema dump',
                                 srcHost=from_host,
                                 srcFile=self.context.generate_filename("cdatabase", directory=from_path),
                                 dstFile=self.context.generate_filename("cdatabase")))

        self.pool.addCommand(Scp('Copying report file',
                                 srcHost=from_host,
                                 srcFile=self.context.generate_filename("report", directory=from_path),
                                 dstFile=self.context.generate_filename("report")))

        self.pool.addCommand(Scp('Copying post data schema dump',
                            srcHost=from_host,
                            srcFile=self.context.generate_filename("postdata", directory=from_path),
                            dstFile=self.context.generate_filename("postdata")))

        if self.context.restore_global:
            self.pool.addCommand(Scp("Copying global dump",
                                     srcHost=from_host,
                                     srcFile=self.context.generate_filename("global", directory=from_path),
                                     dstFile=self.context.generate_filename("global")))
        self.pool.join()
        self.pool.check_results()
        self.pool.haltWork()

class GetDumpTablesOperation(Operation):
    def __init__(self, context):
        self.context = context
        self.grep_cmdStr = ''' | grep -e "-- Name: " -e "^\W*START (" -e "^\W*PARTITION " -e "^\W*DEFAULT PARTITION " -e "^\W*SUBPARTITION " -e "^\W*DEFAULT SUBPARTITION "'''
        self.gunzip_maybe = ' | gunzip' if self.context.compress else ''

    def extract_dumped_tables(self, lines):
        schema = ''
        owner = ''
        table = ''
        ret = []
        for line in lines:
            if line.startswith("-- Name: "):
                table, table_type, schema, owner = get_table_info(line)
                if table_type in ["TABLE", "EXTERNAL TABLE"]:
                    ret.append((schema, table, owner))
            else:
                line = line.strip()
                if (line.startswith("START (") or line.startswith("DEFAULT PARTITION ") or line.startswith("PARTITION ") or
                    line.startswith("SUBPARTITION ") or line.startswith("DEFAULT SUBPARTITION ")):

                    keyword = " WITH \(tablename=E"

                    # minus the length of keyword below as we escaped '(' with an extra back slash (\)
                    pos = get_nonquoted_keyword_index(line, keyword, "'", len(keyword) - 1)
                    if pos == -1:
                        keyword = " WITH \(tablename="
                        pos = get_nonquoted_keyword_index(line, keyword, "'", len(keyword) - 1)
                        if pos == -1:
                            continue
                    # len(keyword) plus one to not include the first single quote
                    table = line[pos + len(keyword) : line.rfind("'")]
                    # unescape table name to get the defined name in database
                    table = unescape_string(table)
                    ret.append((schema, table, owner))

        return ret

class GetDDboostDumpTablesOperation(GetDumpTablesOperation):
    def __init__(self, context):
        self.context = context
        super(GetDDboostDumpTablesOperation, self).__init__(context)

    def execute(self):
        # We want to make sure that directory is empty so that we can grab the ddboost directory value
        # with the timestamp
        ddboost_parent_dir = self.context.get_backup_dir(segment_dir='')
        ddboost_cmdStr = 'gpddboost --readFile --from-file=%s' % self.context.generate_filename("dump", directory=ddboost_parent_dir)

        if self.context.ddboost_storage_unit:
            ddboost_cmdStr += ' --ddboost-storage-unit=%s' % self.context.ddboost_storage_unit

        cmdStr = ddboost_cmdStr + self.gunzip_maybe + self.grep_cmdStr
        cmd = Command('DDBoost copy of master dump file', cmdStr)

        cmd.run(validateAfter=True)
        line_list = cmd.get_results().stdout.splitlines()

        ret = self.extract_dumped_tables(line_list)
        return ret

class GetNetBackupDumpTablesOperation(GetDumpTablesOperation):
    def __init__(self, context):
        self.context = context
        super(GetNetBackupDumpTablesOperation, self).__init__(context)

    def execute(self):
        nbu_cmdStr = 'gp_bsa_restore_agent --netbackup-service-host %s --netbackup-filename %s' % (self.context.netbackup_service_host, self.context.generate_filename("dump"))
        cmdStr = nbu_cmdStr + self.gunzip_maybe + self.grep_cmdStr

        cmd = Command('NetBackup copy of master dump file', cmdStr)
        cmd.run(validateAfter=True)
        line_list = cmd.get_results().stdout.splitlines()

        ret = self.extract_dumped_tables(line_list)
        return ret

class GetLocalDumpTablesOperation(GetDumpTablesOperation):
    def __init__(self, context):
        self.context = context
        super(GetLocalDumpTablesOperation, self).__init__(context)

    def execute(self):
        f = None
        try:
            dump_file = self.context.generate_filename("dump")
            if self.context.compress:
                f = gzip.open(dump_file, 'r')
            else:
                f = open(dump_file, 'r')

            lines = f.readlines()
            ret = self.extract_dumped_tables(lines)
            return ret
        finally:
            if f:
                f.close()

class GetRemoteDumpTablesOperation(GetDumpTablesOperation):
    def __init__(self, context, remote_host):
        self.context = context
        self.host = remote_host
        super(GetRemoteDumpTablesOperation, self).__init__(context)

    def execute(self):
        cat_cmdStr = 'cat %s' % self.context.generate_filename("dump")
        get_remote_dump_tables = '''ssh %s %s%s''' % (self.host, cat_cmdStr, self.grep_cmdStr)

        cmd = Command('Get remote copy of dumped tables', get_remote_dump_tables)
        cmd.run(validateAfter=True)
        line_list = cmd.get_results().stdout.splitlines()

        return self.extract_dumped_tables(line_list)

class GetDumpTables():
    def __init__(self, context, remote_host=None):
        self.context = context
        self.remote_hostname = remote_host

    def get_dump_tables(self):
        if self.context.ddboost:
            get_dump_table_cmd = GetDDboostDumpTablesOperation(self.context)
        elif self.context.netbackup_service_host:
            get_dump_table_cmd = GetNetBackupDumpTablesOperation(self.context)
        elif self.remote_hostname:
            get_dump_table_cmd = GetRemoteDumpTablesOperation(self.context, self.remote_hostname)
        else:
            get_dump_table_cmd = GetLocalDumpTablesOperation(self.context)

        return get_dump_table_cmd.run()
