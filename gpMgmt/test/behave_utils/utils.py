#!/usr/bin/env python
import fileinput
import os
import pipes
import re
import signal
import stat
import time
import glob
import shutil
try:
    import subprocess32 as subprocess
except:
    import subprocess
import difflib

import yaml

from datetime import datetime
from gppylib.commands.base import Command, ExecutionError, REMOTE
from gppylib.commands.gp import chk_local_db_running
from gppylib.db import dbconn
from gppylib.gparray import GpArray, MODE_SYNCHRONIZED
from pygresql import pg

PARTITION_START_DATE = '2010-01-01'
PARTITION_END_DATE = '2013-01-01'

master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
if master_data_dir is None:
    raise Exception('MASTER_DATA_DIRECTORY is not set')


def execute_sql(dbname, sql):
    result = None

    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        result = dbconn.execSQL(conn, sql)
        conn.commit()

    return result

def execute_sql_singleton(dbname, sql):
    result = None
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
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


def run_command_remote(context, command, host, source_file, export_mdd, validateAfter=True):
    cmd = Command(name='run command %s' % command,
                  cmdStr='gpssh -h %s -e \'source %s; %s; %s\'' % (host, source_file, export_mdd, command))
    cmd.run(validateAfter=validateAfter)
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
    context.stdout_position = 0

    return (result.rc, result.stderr, result.stdout)


def run_gpcommand_async(context, command):
    cmd = Command(name='run %s' % command, cmdStr='$GPHOME/bin/%s' % (command))
    asyncproc = cmd.runNoWait()
    if 'asyncproc' not in context:
        context.asyncproc = asyncproc


def check_stdout_msg(context, msg, escapeStr = False):
    if escapeStr:
        msg = re.escape(msg)
    pat = re.compile(msg)

    actual = context.stdout_message
    if isinstance(msg, unicode):
        actual = actual.decode('utf-8')

    if not pat.search(actual):
        err_str = "Expected stdout string '%s' and found: '%s'" % (msg, actual)
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


def check_database_is_running(context):
    if not 'PGPORT' in os.environ:
        raise Exception('PGPORT should be set')

    pgport = int(os.environ['PGPORT'])

    running_status = chk_local_db_running(os.environ.get('MASTER_DATA_DIRECTORY'), pgport)
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


def stop_primary(context, content_id):
    get_psegment_sql = 'select datadir, hostname from gp_segment_configuration where content=%i and role=\'p\';' % content_id
    with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
        cur = dbconn.execSQL(conn, get_psegment_sql)
        rows = cur.fetchall()
        seg_data_dir = rows[0][0]
        seg_host = rows[0][1]

    # For demo_cluster tests that run on the CI gives the error 'bash: pg_ctl: command not found'
    # Thus, need to add pg_ctl to the path when ssh'ing to a demo cluster.
    subprocess.check_call(['ssh', seg_host,
                           'source %s/greenplum_path.sh && pg_ctl stop -m fast -D %s' % (
                               pipes.quote(os.environ.get("GPHOME")), pipes.quote(seg_data_dir))
                           ])


def trigger_fts_probe():
    run_cmd('psql -c "select gp_request_fts_probe_scan()" postgres')


def run_gprecoverseg():
    run_cmd('gprecoverseg -a -v')


def getRows(dbname, exec_sql):
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        curs = dbconn.execSQL(conn, exec_sql)
        results = curs.fetchall()
    return results


def getRow(dbname, exec_sql):
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        curs = dbconn.execSQL(conn, exec_sql)
        result = curs.fetchone()
    return result


def check_db_exists(dbname, host=None, port=0, user=None):
    LIST_DATABASE_SQL = 'SELECT datname FROM pg_database'

    results = []
    with dbconn.connect(dbconn.DbURL(hostname=host, username=user, port=port, dbname='template1'), unsetSearchPath=False) as conn:
        curs = dbconn.execSQL(conn, LIST_DATABASE_SQL)
        results = curs.fetchall()

    for result in results:
        if result[0] == dbname:
            return True

    return False


def create_database_if_not_exists(context, dbname, host=None, port=0, user=None):
    if not check_db_exists(dbname, host, port, user):
        create_database(context, dbname, host, port, user)
    context.dbname = dbname
    context.conn = dbconn.connect(dbconn.DbURL(dbname=context.dbname), unsetSearchPath=False)

def create_database(context, dbname=None, host=None, port=0, user=None):
    LOOPS = 10
    if host is None or port == 0 or user is None:
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


def check_table_exists(context, dbname, table_name, table_type=None, host=None, port=0, user=None):
    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname), unsetSearchPath=False) as conn:
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
    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname), unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, SQL)
        conn.commit()


def drop_external_table(context, table_name, dbname, host=None, port=0, user=None):
    SQL = 'drop external table %s' % table_name
    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname), unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, SQL)
        conn.commit()

    if check_table_exists(context, table_name=table_name, dbname=dbname, table_type='external', host=host, port=port,
                          user=user):
        raise Exception('Unable to successfully drop the table %s' % table_name)


def drop_table(context, table_name, dbname, host=None, port=0, user=None):
    SQL = 'drop table %s' % table_name
    with dbconn.connect(dbconn.DbURL(hostname=host, username=user, port=port, dbname=dbname), unsetSearchPath=False) as conn:
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
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, SQL)
        conn.commit()
    if check_schema_exists(context, schema_name, dbname):
        raise Exception('Unable to successfully drop the schema %s' % schema_name)


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

    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
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

    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname), unsetSearchPath=False) as conn:
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

    with dbconn.connect(dbconn.DbURL(hostname=host, port=port, username=user, dbname=dbname), unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, insert_sql_str)
        conn.commit()


def create_indexes(context, table_name, indexname, dbname):
    btree_index_sql = "create index btree_%s on %s using btree(column1);" % (indexname, table_name)
    bitmap_index_sql = "create index bitmap_%s on %s using bitmap(column3);" % (indexname, table_name)
    index_sql = btree_index_sql + bitmap_index_sql
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
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
        with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
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
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, CREATE_TABLE_SQL)
        conn.commit()

        result = dbconn.execSQLForSingleton(conn, SELECT_TABLE_SQL)
        if result != NROW:
            raise Exception('Integer table creation was not successful. Expected %d does not match %d' % (NROW, result))


def drop_database(context, dbname, host=None, port=0, user=None):
    LOOPS = 10
    if host is None or port == 0 or user is None:
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


def are_segments_synchronized():
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    for seg in segments:
        if seg.mode != MODE_SYNCHRONIZED and not seg.isSegmentMaster(True):
            return False
    return True


def check_row_count(context, tablename, dbname, nrows):
    NUM_ROWS_QUERY = 'select count(*) from %s' % tablename
    # We want to bubble up the exception so that if table does not exist, the test fails
    if hasattr(context, 'standby_was_activated') and context.standby_was_activated is True:
        dburl = dbconn.DbURL(dbname=dbname, port=context.standby_port, hostname=context.standby_hostname)
    else:
        dburl = dbconn.DbURL(dbname=dbname)

    with dbconn.connect(dburl, unsetSearchPath=False) as conn:
        result = dbconn.execSQLForSingleton(conn, NUM_ROWS_QUERY)
    if result != nrows:
        raise Exception('%d rows in table %s.%s, expected row count = %d' % (result, dbname, tablename, nrows))


def get_master_hostname(dbname='template1'):
    master_hostname_sql = "SELECT DISTINCT hostname FROM gp_segment_configuration WHERE content=-1 AND role='p'"
    return getRows(dbname, master_hostname_sql)


def get_hosts(dbname='template1'):
    get_hosts_sql = "SELECT DISTINCT hostname FROM gp_segment_configuration WHERE role='p';"
    return getRows(dbname, get_hosts_sql)


def truncate_table(dbname, tablename):
    TRUNCATE_SQL = 'TRUNCATE %s' % tablename
    execute_sql(dbname, TRUNCATE_SQL)


def insert_row(context, row_values, table, dbname):
    sql = """INSERT INTO %s values(%s)""" % (table, row_values)
    execute_sql(dbname, sql)


def get_all_hostnames_as_list(context, dbname):
    hosts = []
    segs = get_segment_hostnames(context, dbname)
    for seg in segs:
        hosts.append(seg[0].strip())

    masters = get_master_hostname(dbname)
    for master in masters:
        hosts.append(master[0].strip())

    return hosts

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
    result = True
    for seg in segments:
        if seg.status != 'u':
            print "segment is not up - %s" % seg
            result = False
    return result


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
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        result = dbconn.execSQLForSingleton(conn, NUM_ROWS_QUERY)
    if result != nrows:
        raise Exception('%d rows in query: %s. Expected row count = %d' % (result, query, nrows))


def get_primary_segment_host_port():
    """
    return host, port of primary segment (dbid 2)
    """
    FIRST_PRIMARY_DBID = 2
    get_psegment_sql = 'select hostname, port from gp_segment_configuration where dbid=%i;' % FIRST_PRIMARY_DBID
    with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
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


def wait_for_unblocked_transactions(context, num_retries=150):
    """
    Tries once a second to successfully commit a transaction to the database
    running on PGHOST/PGPORT. Raises an Exception after failing <num_retries>
    times.
    """
    attempt = 0
    while attempt < num_retries:
        try:
            with dbconn.connect(dbconn.DbURL(), unsetSearchPath=False) as conn:
                # Cursor.execute() will issue an implicit BEGIN for us.
                # Empty block of 'BEGIN' and 'END' won't start a distributed transaction,
                # execute a DDL query to start a distributed transaction.
                conn.cursor().execute('CREATE TEMP TABLE temp_test(a int)')
                conn.cursor().execute('COMMIT')
                break
        except Exception as e:
            attempt += 1
            pass
        time.sleep(1)

    if attempt == num_retries:
        raise Exception('Unable to establish a connection to database !!!')
