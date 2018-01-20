#!/usr/bin/env python
import filecmp
import fileinput
import os
import re
import signal
import stat
import time
import glob
import shutil
import difflib

import yaml

from datetime import datetime
from gppylib.commands.base import Command, ExecutionError, REMOTE
from gppylib.commands.gp import chk_local_db_running
from gppylib.db import dbconn
from gppylib.gparray import GpArray, MODE_SYNCHRONIZED, MODE_RESYNCHRONIZATION
from pygresql import pg

PARTITION_START_DATE = '2010-01-01'
PARTITION_END_DATE = '2013-01-01'

GET_APPENDONLY_DATA_TABLE_INFO_SQL = """SELECT ALL_DATA_TABLES.oid, ALL_DATA_TABLES.schemaname, ALL_DATA_TABLES.tablename, OUTER_PG_CLASS.relname AS tupletable FROM(
SELECT ALLTABLES.oid, ALLTABLES.schemaname, ALLTABLES.tablename FROM
    (SELECT c.oid, n.nspname AS schemaname, c.relname AS tablename FROM pg_class c, pg_namespace n
    WHERE n.oid = c.relnamespace) AS ALLTABLES,
    (SELECT n.nspname AS schemaname, c.relname AS tablename
    FROM pg_class c LEFT JOIN pg_namespace n ON n.oid = c.relnamespace
    LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace
    WHERE c.relkind = 'r'::CHAR AND c.oid > 16384 AND (c.relnamespace > 16384 OR n.nspname = 'public')
    EXCEPT
    ((SELECT x.schemaname, x.partitiontablename FROM
    (SELECT DISTINCT schemaname, tablename, partitiontablename, partitionlevel FROM pg_partitions) AS X,
    (SELECT schemaname, tablename maxtable, max(partitionlevel) maxlevel FROM pg_partitions GROUP BY (tablename, schemaname))
 AS Y
    WHERE x.schemaname = y.schemaname AND x.tablename = Y.maxtable AND x.partitionlevel != Y.maxlevel)
    UNION (SELECT DISTINCT schemaname, tablename FROM pg_partitions))) AS DATATABLES
WHERE ALLTABLES.schemaname = DATATABLES.schemaname AND ALLTABLES.tablename = DATATABLES.tablename AND ALLTABLES.oid NOT IN (SELECT reloid FROM pg_exttable)
) AS ALL_DATA_TABLES, pg_appendonly, pg_class OUTER_PG_CLASS
    WHERE ALL_DATA_TABLES.oid = pg_appendonly.relid
    AND OUTER_PG_CLASS.oid = pg_appendonly.segrelid
"""

GET_ALL_AO_DATATABLES_SQL = """
    %s AND pg_appendonly.columnstore = 'f'
""" % GET_APPENDONLY_DATA_TABLE_INFO_SQL

GET_ALL_CO_DATATABLES_SQL = """
    %s AND pg_appendonly.columnstore = 't'
""" % GET_APPENDONLY_DATA_TABLE_INFO_SQL

master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
if master_data_dir is None:
    raise Exception('MASTER_DATA_DIRECTORY is not set')


def execute_sql(dbname, sql):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, sql)
        conn.commit()


def execute_sql_singleton(dbname, sql):
    result = None
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        result = dbconn.execSQLForSingleton(conn, sql)

    if result is None:
        raise Exception("error running query: %s" % sql)

    return result


def has_exception(context):
    if not hasattr(context, 'exception'):
        return False
    if context.exception:
        return True
    else:
        return False


def run_command(context, command):
    context.exception = None
    cmd = Command(name='run %s' % command, cmdStr='%s' % command)
    try:
        cmd.run(validateAfter=True)
    except ExecutionError, e:
        context.exception = e

    result = cmd.get_results()
    context.ret_code = result.rc
    context.stdout_message = result.stdout
    context.error_message = result.stderr


def run_async_command(context, command):
    context.exception = None
    cmd = Command(name='run %s' % command, cmdStr='%s' % command)
    try:
        proc = cmd.runNoWait()
    except ExecutionError, e:
        context.exception = e
    context.async_proc = proc


def run_cmd(command):
    cmd = Command(name='run %s' % command, cmdStr='%s' % command)
    try:
        cmd.run(validateAfter=True)
    except ExecutionError, e:
        print 'caught exception %s' % e

    result = cmd.get_results()
    return (result.rc, result.stdout, result.stderr)


def run_command_remote(context, command, host, source_file, export_mdd):
    cmd = Command(name='run command %s' % command,
                  cmdStr='gpssh -h %s -e \'source %s; %s; %s\'' % (host, source_file, export_mdd, command))
    cmd.run(validateAfter=True)
    result = cmd.get_results()
    context.ret_code = result.rc
    context.stdout_message = result.stdout
    context.error_message = result.stderr


def run_gpcommand(context, command, cmd_prefix=''):
    context.exception = None
    cmd = Command(name='run %s' % command, cmdStr='$GPHOME/bin/%s' % (command))
    if cmd_prefix:
        cmd = Command(name='run %s' % command, cmdStr='%s;$GPHOME/bin/%s' % (cmd_prefix, command))
    try:
        cmd.run(validateAfter=True)
    except ExecutionError, e:
        context.exception = e

    result = cmd.get_results()
    context.ret_code = result.rc
    context.stdout_message = result.stdout
    context.error_message = result.stderr


def run_gpcommand_async(context, command):
    cmd = Command(name='run %s' % command, cmdStr='$GPHOME/bin/%s' % (command))
    context.asyncproc = cmd.runNoWait()


def check_stdout_msg(context, msg):
    pat = re.compile(msg)
    if not pat.search(context.stdout_message):
        err_str = "Expected stdout string '%s' and found: '%s'" % (msg, context.stdout_message)
        raise Exception(err_str)


def check_string_not_present_stdout(context, msg):
    pat = re.compile(msg)
    if pat.search(context.stdout_message):
        err_str = "Did not expect stdout string '%s' but found: '%s'" % (msg, context.stdout_message)
        raise Exception(err_str)


def check_err_msg(context, err_msg):
    if not hasattr(context, 'exception'):
        raise Exception('An exception was not raised and it was expected')
    pat = re.compile(err_msg)
    if not pat.search(context.error_message):
        err_str = "Expected error string '%s' and found: '%s'" % (err_msg, context.error_message)
        raise Exception(err_str)


def check_return_code(context, ret_code):
    if context.ret_code != int(ret_code):
        emsg = ""
        if context.error_message:
            emsg += "STDERR:\n%s\n" % context.error_message
        if context.stdout_message:
            emsg += "STDOUT:\n%s\n" % context.stdout_message
        raise Exception("expected return code '%s' does not equal actual return code '%s' \n%s" % (ret_code, context.ret_code, emsg))


def check_not_return_code(context, ret_code):
    if context.ret_code == int(ret_code):
        emsg = ""
        if context.error_message:
            emsg += context.error_message
        raise Exception("return code unexpectedly equals '%s' %s" % (ret_code, emsg))


def check_database_is_running(context):
    if not 'PGPORT' in os.environ:
        raise Exception('PGPORT should be set')

    pgport = int(os.environ['PGPORT'])

    running_status = chk_local_db_running(master_data_dir, pgport)
    gpdb_running = running_status[0] and running_status[1] and running_status[2] and running_status[3]

    return gpdb_running


def start_database_if_not_started(context):
    if not check_database_is_running(context):
        start_database(context)


def start_database(context):
    run_gpcommand(context, 'gpstart -a')
    if context.exception:
        raise context.exception


def stop_database_if_started(context):
    if check_database_is_running(context):
        stop_database(context)


def stop_database(context):
    run_gpcommand(context, 'gpstop -M fast -a')
    if context.exception:
        raise context.exception


def getRows(dbname, exec_sql):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        curs = dbconn.execSQL(conn, exec_sql)
        results = curs.fetchall()
    return results


def getRow(dbname, exec_sql):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        curs = dbconn.execSQL(conn, exec_sql)
        result = curs.fetchone()
    return result


def check_db_exists(dbname, host=None, port=0, user=None):
    LIST_DATABASE_SQL = 'SELECT datname FROM pg_database'

    results = []
    with dbconn.connect(dbconn.DbURL(hostname=host, username=user, port=port, dbname='template1')) as conn:
        curs = dbconn.execSQL(conn, LIST_DATABASE_SQL)
        results = curs.fetchall()

    for result in results:
        if result[0] == dbname:
            return True

    return False


def create_database_if_not_exists(context, dbname, host=None, port=0, user=None):
    if not check_db_exists(dbname, host, port, user):
        create_database(context, dbname, host, port, user)


def create_database(context, dbname=None, host=None, port=0, user=None):
    LOOPS = 10
    if host == None or port == 0 or user == None:
        createdb_cmd = 'createdb %s' % dbname
    else:
        createdb_cmd = 'psql -h %s -p %d -U %s -d template1 -c "create database %s"' % (host,
                                                                                        port, user, dbname)
    for i in range(LOOPS):
        context.exception = None

        run_command(context, createdb_cmd)

        if context.exception:
            time.sleep(1)
            continue

        if check_db_exists(dbname, host, port, user):
            return

        time.sleep(1)

    if context.exception:
        raise context.exception

    raise Exception("create database for '%s' failed after %d attempts" % (dbname, LOOPS))


def get_segment_hostnames(context, dbname):
    sql = "SELECT DISTINCT(hostname) FROM gp_segment_configuration WHERE content != -1;"
    return getRows(dbname, sql)


def check_partition_table_exists(context, dbname, schemaname, table_name, table_type=None, part_level=1, part_number=1):
    partitions = get_partition_names(schemaname, table_name, dbname, part_level, part_number)
    if not partitions:
        return False
    return check_table_exists(context, dbname, partitions[0][0].strip(), table_type)


def check_table_exists(context, dbname, table_name, table_type=None, host=None, port=0, user=None):
    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname)) as conn:
        if '.' in table_name:
            schemaname, tablename = table_name.split('.')
            SQL_format = """
                SELECT c.oid, c.relkind, c.relstorage, c.reloptions
                FROM pg_class c, pg_namespace n
                WHERE c.relname = '%s' AND n.nspname = '%s' AND c.relnamespace = n.oid;
                """
            SQL = SQL_format % (escape_string(tablename, conn=conn), escape_string(schemaname, conn=conn))
        else:
            SQL_format = """
                SELECT oid, relkind, relstorage, reloptions \
                FROM pg_class \
                WHERE relname = E'%s';\
                """
            SQL = SQL_format % (escape_string(table_name, conn=conn))

        table_row = None
        try:
            table_row = dbconn.execSQLForSingletonRow(conn, SQL)
        except Exception as e:
            context.exception = e
            return False

        if table_type is None:
            return True

    if table_row[2] == 'a':
        original_table_type = 'ao'
    elif table_row[2] == 'c':
        original_table_type = 'co'
    elif table_row[2] == 'h':
        original_table_type = 'heap'
    elif table_row[2] == 'x':
        original_table_type = 'external'
    elif table_row[2] == 'v':
        original_table_type = 'view'
    else:
        raise Exception('Unknown table type %s' % table_row[2])

    if original_table_type != table_type.strip():
        return False

    return True


def drop_external_table_if_exists(context, table_name, dbname):
    if check_table_exists(context, table_name=table_name, dbname=dbname, table_type='external'):
        drop_external_table(context, table_name=table_name, dbname=dbname)


def drop_table_if_exists(context, table_name, dbname, host=None, port=0, user=None):
    SQL = 'drop table if exists %s' % table_name
    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname)) as conn:
        dbconn.execSQL(conn, SQL)
        conn.commit()


def drop_external_table(context, table_name, dbname, host=None, port=0, user=None):
    SQL = 'drop external table %s' % table_name
    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname)) as conn:
        dbconn.execSQL(conn, SQL)
        conn.commit()

    if check_table_exists(context, table_name=table_name, dbname=dbname, table_type='external', host=host, port=port,
                          user=user):
        raise Exception('Unable to successfully drop the table %s' % table_name)


def drop_table(context, table_name, dbname, host=None, port=0, user=None):
    SQL = 'drop table %s' % table_name
    with dbconn.connect(dbconn.DbURL(hostname=host, username=user, port=port, dbname=dbname)) as conn:
        dbconn.execSQL(conn, SQL)
        conn.commit()

    if check_table_exists(context, table_name=table_name, dbname=dbname, host=host, port=port, user=user):
        raise Exception('Unable to successfully drop the table %s' % table_name)


def check_schema_exists(context, schema_name, dbname):
    schema_check_sql = "select * from pg_namespace where nspname='%s';" % schema_name
    if len(getRows(dbname, schema_check_sql)) < 1:
        return False
    return True


def drop_schema_if_exists(context, schema_name, dbname):
    if check_schema_exists(context, schema_name, dbname):
        drop_schema(context, schema_name, dbname)


def drop_schema(context, schema_name, dbname):
    SQL = 'drop schema %s cascade' % schema_name
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, SQL)
        conn.commit()
    if check_schema_exists(context, schema_name, dbname):
        raise Exception('Unable to successfully drop the schema %s' % schema_name)


def get_table_names(dbname):
    sql = """
            SELECT n.nspname AS schemaname, c.relname AS tablename\
            FROM pg_class c\
            LEFT JOIN pg_namespace n ON n.oid = c.relnamespace\
            LEFT JOIN pg_tablespace t ON t.oid = c.reltablespace\
            WHERE c.relkind = 'r'::CHAR AND c.oid > 16384 AND (c.relnamespace > 16384 OR n.nspname = 'public')
                  AND n.nspname NOT LIKE 'pg_temp_%'
          """

    return getRows(dbname, sql)


def get_partition_tablenames(tablename, dbname, part_level=1):
    child_part_sql = "select partitiontablename from pg_partitions where tablename='%s' and partitionlevel=%s;" % (
    tablename, part_level)
    rows = getRows(dbname, child_part_sql)
    return rows


def get_partition_names(schemaname, tablename, dbname, part_level, part_number):
    part_num_sql = """select partitionschemaname || '.' || partitiontablename from pg_partitions
                             where schemaname='%s' and tablename='%s'
                             and partitionlevel=%s and partitionposition=%s;""" % (
    schemaname, tablename, part_level, part_number)
    rows = getRows(dbname, part_num_sql)
    return rows


def validate_part_table_data_on_segments(context, tablename, part_level, dbname):
    rows = get_partition_tablenames(tablename, dbname, part_level)
    for part_tablename in rows:
        seg_data_sql = "select gp_segment_id, count(*) from gp_dist_random('%s') group by gp_segment_id;" % \
                       part_tablename[0]
        rows = getRows(dbname, seg_data_sql)
        for row in rows:
            if row[1] == '0':
                raise Exception('Data not present in segment %s' % row[0])


def create_external_partition(context, tablename, dbname, port, filename):
    table_definition = 'Column1 int, Column2 varchar(20), Column3 date'
    create_table_str = "Create table %s (%s) Distributed randomly \
                        Partition by range(Column3) ( \
                        partition p_1  start(date '2010-01-01') end(date '2011-01-01') with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1), \
                        partition p_2  start(date '2011-01-01') end(date '2012-01-01') with (appendonly=true, orientation=row, compresstype=zlib, compresslevel=1), \
                        partition s_3  start(date '2012-01-01') end(date '2013-01-01') with (appendonly=true, orientation=column), \
                        partition s_4  start(date '2013-01-01') end(date '2014-01-01') with (appendonly=true, orientation=row), \
                        partition s_5  start(date '2014-01-01') end(date '2015-01-01') ) \
                        ;" % (tablename, table_definition)

    master_hostname = get_master_hostname();
    create_ext_table_str = "Create readable external table %s_ret (%s) \
                            location ('gpfdist://%s:%s/%s') \
                            format 'csv' encoding 'utf-8' \
                            log errors segment reject limit 1000 \
                            ;" % (tablename, table_definition, master_hostname[0][0].strip(), port, filename)

    alter_table_str = "Alter table %s exchange partition p_2 \
                       with table %s_ret without validation \
                       ;" % (tablename, tablename)

    drop_table_str = "Drop table %s_ret;" % (tablename)

    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, create_table_str)
        dbconn.execSQL(conn, create_ext_table_str)
        dbconn.execSQL(conn, alter_table_str)
        dbconn.execSQL(conn, drop_table_str)
        conn.commit()

    populate_partition(tablename, '2010-01-01', dbname, 0, 100)


def create_partition(context, tablename, storage_type, dbname, compression_type=None, partition=True, rowcount=1094,
                     with_data=True, host=None, port=0, user=None):
    interval = '1 year'

    table_definition = 'Column1 int, Column2 varchar(20), Column3 date'
    create_table_str = "Create table " + tablename + "(" + table_definition + ")"
    storage_type_dict = {'ao': 'row', 'co': 'column'}

    part_table = " Distributed Randomly Partition by list(Column2) \
                    Subpartition by range(Column3) Subpartition Template \
                    (start (date '%s') end (date '%s') every (interval '%s')) \
                    (Partition p1 values('backup') , Partition p2 values('restore')) " \
                 % (PARTITION_START_DATE, PARTITION_END_DATE, interval)

    if storage_type == "heap":
        create_table_str = create_table_str
        if partition:
            create_table_str = create_table_str + part_table

    elif storage_type == "ao" or storage_type == "co":
        create_table_str = create_table_str + " WITH(appendonly = true, orientation = %s) " % storage_type_dict[
            storage_type]
        if compression_type is not None:
            create_table_str = create_table_str[:-2] + ", compresstype = " + compression_type + ") "
        if partition:
            create_table_str = create_table_str + part_table

    create_table_str = create_table_str + ";"

    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname)) as conn:
        dbconn.execSQL(conn, create_table_str)
        conn.commit()

    if with_data:
        populate_partition(tablename, PARTITION_START_DATE, dbname, 0, rowcount, host, port, user)


# same data size as populate partition, but different values
def populate_partition(tablename, start_date, dbname, data_offset, rowcount=1094, host=None, port=0, user=None):
    insert_sql_str = "insert into %s select i+%d, 'backup', i + date '%s' from generate_series(0,%d) as i" % (
    tablename, data_offset, start_date, rowcount)
    insert_sql_str += "; insert into %s select i+%d, 'restore', i + date '%s' from generate_series(0,%d) as i" % (
    tablename, data_offset, start_date, rowcount)

    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname)) as conn:
        dbconn.execSQL(conn, insert_sql_str)
        conn.commit()


def create_indexes(context, table_name, indexname, dbname):
    btree_index_sql = "create index btree_%s on %s using btree(column1);" % (indexname, table_name)
    bitmap_index_sql = "create index bitmap_%s on %s using bitmap(column3);" % (indexname, table_name)
    index_sql = btree_index_sql + bitmap_index_sql
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, index_sql)
        conn.commit()
    validate_index(context, table_name, dbname)


def validate_index(context, table_name, dbname):
    index_sql = "select count(indexrelid::regclass) from pg_index, pg_class where indrelid = '%s'::regclass group by indexrelid;" % table_name
    rows = getRows(dbname, index_sql)
    if len(rows) != 2:
        raise Exception('Index creation was not successful. Expected 2 rows does not match %d rows' % len(rows))


def create_schema(context, schema_name, dbname):
    if not check_schema_exists(context, schema_name, dbname):
        schema_sql = "create schema %s" % schema_name
        with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
            dbconn.execSQL(conn, schema_sql)
            conn.commit()


def create_int_table(context, table_name, table_type='heap', dbname='testdb'):
    CREATE_TABLE_SQL = None
    NROW = 1000

    table_type = table_type.upper()
    if table_type == 'AO':
        CREATE_TABLE_SQL = 'create table %s WITH(APPENDONLY=TRUE) as select generate_series(1,%d) as c1' % (
        table_name, NROW)
    elif table_type == 'CO':
        CREATE_TABLE_SQL = 'create table %s WITH(APPENDONLY=TRUE, orientation=column) as select generate_series(1, %d) as c1' % (
        table_name, NROW)
    elif table_type == 'HEAP':
        CREATE_TABLE_SQL = 'create table %s as select generate_series(1, %d) as c1' % (table_name, NROW)

    if CREATE_TABLE_SQL is None:
        raise Exception('Invalid table type specified')

    SELECT_TABLE_SQL = 'select count(*) from %s' % table_name
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, CREATE_TABLE_SQL)
        conn.commit()

        result = dbconn.execSQLForSingleton(conn, SELECT_TABLE_SQL)
        if result != NROW:
            raise Exception('Integer table creation was not successful. Expected %d does not match %d' % (NROW, result))


def drop_database(context, dbname, host=None, port=0, user=None):
    LOOPS = 10
    if host == None or port == 0 or user == None:
        dropdb_cmd = 'dropdb %s' % dbname
    else:
        dropdb_cmd = 'psql -h %s -p %d -U %s -d template1 -c "drop database %s"' % (host,
                                                                                    port, user, dbname)
    for i in range(LOOPS):
        context.exception = None

        run_gpcommand(context, dropdb_cmd)

        if context.exception:
            time.sleep(1)
            continue

        if not check_db_exists(dbname):
            return

        time.sleep(1)

    if context.exception:
        raise context.exception

    raise Exception('db exists after dropping: %s' % dbname)


def drop_database_if_exists(context, dbname=None, host=None, port=0, user=None):
    if check_db_exists(dbname, host=host, port=port, user=user):
        drop_database(context, dbname, host=host, port=port, user=user)


def run_on_all_segs(context, dbname, query):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]

    for seg in primary_segs:
        with dbconn.connect(dbconn.DbURL(dbname=dbname, hostname=seg.getSegmentHostName(), port=seg.getSegmentPort()),
                            utility=True) as conn:
            dbconn.execSQL(conn, query)
            conn.commit()


def get_nic_up(hostname, nic):
    address = hostname + '-cm'
    cmd = Command(name='ifconfig nic', cmdStr='sudo /sbin/ifconfig %s' % nic, remoteHost=address, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    return 'UP' in cmd.get_results().stdout


def bring_nic_down(hostname, nic):
    address = hostname + '-cm'
    cmd = Command(name='bring down nic', cmdStr='sudo /sbin/ifdown %s' % nic, remoteHost=address, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    if get_nic_up(hostname, nic):
        raise Exception('Unable to bring down nic %s on host %s' % (nic, hostname))


def bring_nic_up(hostname, nic):
    address = hostname + '-cm'
    cmd = Command(name='bring up nic', cmdStr='sudo /sbin/ifup %s' % nic, remoteHost=address, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    if not get_nic_up(hostname, nic):
        raise Exception('Unable to bring up nic %s on host %s' % (nic, hostname))


def are_segments_synchronized():
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    for seg in segments:
        if seg.mode != MODE_SYNCHRONIZED and not seg.isSegmentMaster(True):
            return False
    return True


def is_any_segment_resynchronized():
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    for seg in segments:
        if seg.mode == MODE_RESYNCHRONIZATION:
            return True
    return False


def check_row_count(tablename, dbname, nrows):
    NUM_ROWS_QUERY = 'select count(*) from %s' % tablename
    # We want to bubble up the exception so that if table does not exist, the test fails
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        result = dbconn.execSQLForSingleton(conn, NUM_ROWS_QUERY)
    if result != nrows:
        raise Exception('%d rows in table %s.%s, expected row count = %d' % (result, dbname, tablename, nrows))


def match_table_select(context, src_tablename, src_dbname, dest_tablename, dest_dbname, orderby=None, options=''):
    if orderby != None:
        dest_tbl_qry = 'psql -d %s -c \'select * from %s order by %s\' %s' % (
        dest_dbname, dest_tablename, orderby, options)
        src_tbl_qry = '''psql -p %s -h %s -U %s -d %s -c \'select * from %s order by %s\' %s''' % (
            os.environ.get('GPTRANSFER_SOURCE_PORT'),
            os.environ.get('GPTRANSFER_SOURCE_HOST'),
            os.environ.get('GPTRANSFER_SOURCE_USER'),
            src_dbname, src_tablename, orderby, options)
    else:
        dest_tbl_qry = 'psql -d %s -c \'select * from %s\' %s' % (dest_dbname, dest_tablename, options)
        src_tbl_qry = '''psql -p %s -h %s -U %s -d %s -c \'select * from %s\' %s''' % (
            os.environ.get('GPTRANSFER_SOURCE_PORT'),
            os.environ.get('GPTRANSFER_SOURCE_HOST'),
            os.environ.get('GPTRANSFER_SOURCE_USER'),
            src_dbname, src_tablename, options)

    (_, dest_content, _) = run_cmd(dest_tbl_qry)
    (_, src_content, _) = run_cmd(src_tbl_qry)
    if src_content != dest_content:
        raise Exception('''table %s in database %s of source system does not match rows with table %s in database %s of destination system.\n
                         destination table content:\n%s\n
                         source table content:\n%s\n''' % (
            src_tablename, src_dbname, dest_tablename, dest_dbname, dest_content, src_content))


def get_master_hostname(dbname='template1'):
    master_hostname_sql = "SELECT DISTINCT hostname FROM gp_segment_configuration WHERE content=-1 AND role='p'"
    return getRows(dbname, master_hostname_sql)


def get_hosts(dbname='template1'):
    get_hosts_sql = "SELECT DISTINCT hostname FROM gp_segment_configuration WHERE role='p';"
    return getRows(dbname, get_hosts_sql)


def truncate_table(dbname, tablename):
    TRUNCATE_SQL = 'TRUNCATE %s' % tablename
    execute_sql(dbname, TRUNCATE_SQL)


def get_table_oid(context, dbname, schema, tablename):
    OID_SQL = """SELECT c.oid
                 FROM pg_class c, pg_namespace n
                 WHERE c.relnamespace = n.oid AND c.relname = '%s' AND n.nspname = '%s'""" % (tablename, schema)

    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        oid = dbconn.execSQLForSingleton(conn, OID_SQL)

    return oid


def insert_row(context, row_values, table, dbname):
    sql = """INSERT INTO %s values(%s)""" % (table, row_values)
    execute_sql(dbname, sql)


def get_partition_list(partition_type, dbname):
    if partition_type == 'ao':
        sql = GET_ALL_AO_DATATABLES_SQL
    elif partition_type == 'co':
        sql = GET_ALL_CO_DATATABLES_SQL

    partition_list = getRows(dbname, sql)
    for line in partition_list:
        if len(line) != 4:
            raise Exception('Invalid results from query to get all AO tables: [%s]' % (','.join(line)))
    return partition_list


def get_all_hostnames_as_list(context, dbname):
    hosts = []
    segs = get_segment_hostnames(context, dbname)
    for seg in segs:
        hosts.append(seg[0].strip())

    masters = get_master_hostname(dbname)
    for master in masters:
        hosts.append(master[0].strip())

    return hosts


def get_pid_for_segment(seg_data_dir, seg_host):
    cmd = Command(name='get list of postmaster processes',
                  cmdStr='ps -eaf | grep %s' % seg_data_dir,
                  ctxt=REMOTE,
                  remoteHost=seg_host)
    cmd.run(validateAfter=True)

    pid = None
    results = cmd.get_results().stdout.strip().split('\n')
    for res in results:
        if 'grep' not in res:
            pid = res.split()[1]

    if pid is None:
        return None

    return int(pid)


def install_gppkg(context):
    if 'GPPKG_PATH' not in os.environ:
        raise Exception('GPPKG_PATH needs to be set in the environment to install gppkg')
    if 'GPPKG_NAME' not in os.environ:
        raise Exception('GPPKG_NAME needs to be set in the environment to install gppkg')

    gppkg_path = os.environ['GPPKG_PATH']
    gppkg_name = os.environ['GPPKG_NAME']
    command = "gppkg --install %s/%s.gppkg" % (gppkg_path, gppkg_name)
    run_command(context, command)
    print "Install gppkg command: '%s', stdout: '%s', stderr: '%s'" % (
    command, context.stdout_message, context.error_message)


def kill_process(pid, host=None, sig=signal.SIGTERM):
    if host is not None:
        cmd = Command('kill process on a given host',
                      cmdStr='kill -%d %d' % (sig, pid),
                      ctxt=REMOTE,
                      remoteHost=host)
        cmd.run(validateAfter=True)
    else:
        os.kill(pid, sig)

def has_process_eventually_stopped(proc, host=None):
    start_time = current_time = datetime.now()
    is_running = False
    while (current_time - start_time).seconds < 120:
        is_running = is_process_running(proc, host)
        if not is_running:
            break
        time.sleep(2)
        current_time = datetime.now()
    return not is_running


def check_user_permissions(file_name, access_mode):
    st = os.stat(file_name)
    if access_mode == 'write':
        return bool(st.st_mode & stat.S_IWUSR)
    elif access_mode == 'read':
        return bool(st.st_mode & stat.S_IRUSR)
    elif access_mode == 'execute':
        return bool(st.st_mode & stat.S_IXUSR)
    else:
        raise Exception('Invalid mode specified, should be read, write or execute only')


def are_segments_running():
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    for seg in segments:
        if seg.status != 'u':
            return False
    return True


def modify_sql_file(file, hostport):
    if os.path.isfile(file):
        for line in fileinput.FileInput(file, inplace=1):
            if line.find("gpfdist") >= 0:
                line = re.sub('(\d+)\.(\d+)\.(\d+)\.(\d+)\:(\d+)', hostport, line)
            print str(re.sub('\n', '', line))


def remove_dir(host, directory):
    cmd = 'gpssh -h %s -e \'rm -rf %s\'' % (host, directory)
    run_cmd(cmd)


def create_dir(host, directory):
    cmd = 'gpssh -h %s -e \'mkdir -p %s\'' % (host, directory)
    run_cmd(cmd)


def check_count_for_specific_query(dbname, query, nrows):
    NUM_ROWS_QUERY = '%s' % query
    # We want to bubble up the exception so that if table does not exist, the test fails
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        result = dbconn.execSQLForSingleton(conn, NUM_ROWS_QUERY)
    if result != nrows:
        raise Exception('%d rows in query: %s. Expected row count = %d' % (result, query, nrows))


def get_primary_segment_host_port():
    """
    return host, port of primary segment (dbid 2)
    """
    FIRST_PRIMARY_DBID = 2
    get_psegment_sql = 'select hostname, port from gp_segment_configuration where dbid=%i;' % FIRST_PRIMARY_DBID
    with dbconn.connect(dbconn.DbURL(dbname='template1')) as conn:
        cur = dbconn.execSQL(conn, get_psegment_sql)
        rows = cur.fetchall()
        primary_seg_host = rows[0][0]
        primary_seg_port = rows[0][1]
    return primary_seg_host, primary_seg_port


def remove_local_path(dirname):
    list = glob.glob(os.path.join(os.path.curdir, dirname))
    for dir in list:
        shutil.rmtree(dir, ignore_errors=True)


def validate_local_path(path):
    list = glob.glob(os.path.join(os.path.curdir, path))
    return len(list)


def populate_regular_table_data(context, tabletype, table_name, compression_type, dbname, rowcount=1094,
                                with_data=False, host=None, port=0, user=None):
    create_database_if_not_exists(context, dbname, host=host, port=port, user=user)
    drop_table_if_exists(context, table_name=table_name, dbname=dbname, host=host, port=port, user=user)
    if compression_type == "None":
        create_partition(context, table_name, tabletype, dbname, compression_type=None, partition=False,
                         rowcount=rowcount, with_data=with_data, host=host, port=port, user=user)
    else:
        create_partition(context, table_name, tabletype, dbname, compression_type, partition=False,
                         rowcount=rowcount, with_data=with_data, host=host, port=port, user=user)


def is_process_running(proc_name, host=None):
    if host is not None:
        cmd = Command(name='pgrep for %s' % proc_name,
                      cmdStr="pgrep %s" % proc_name,
                      ctxt=REMOTE,
                      remoteHost=host)
    else:
        cmd = Command(name='pgrep for %s' % proc_name,
                      cmdStr="pgrep %s" % proc_name)
    cmd.run()
    if cmd.get_return_code() > 1:
        raise Exception("unexpected problem with pgrep, return code: %s" % cmd.get_return_code())
    return cmd.get_return_code() == 0


def file_contains_line(filepath, target_line):
    with open(filepath, 'r') as myfile:
        return target_line in myfile.read().splitlines()

def replace_special_char_env(str):
    for var in ["SP_CHAR_DB", "SP_CHAR_SCHEMA", "SP_CHAR_AO", "SP_CHAR_CO", "SP_CHAR_HEAP"]:
        if var in os.environ:
            str = str.replace("$%s" % var, os.environ[var])
    return str


def escape_string(string, conn):
    return pg.DB(db=conn).escape_string(string)
