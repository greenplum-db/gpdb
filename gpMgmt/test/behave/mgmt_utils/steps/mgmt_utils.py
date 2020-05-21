import codecs
import math
import fnmatch
import getpass
import glob
import json
import yaml
import os
import re
import pipes
import platform
import shutil
import socket
import tarfile
import tempfile
import thread
import json
try:
    from subprocess32 import check_output, Popen, PIPE
except:
    from subprocess import check_output, Popen, PIPE
import commands
import signal
from collections import defaultdict

import psutil
from behave import given, when, then
from datetime import datetime, timedelta
from time import sleep
from os import path

from gppylib.gparray import GpArray, ROLE_PRIMARY, ROLE_MIRROR
from gppylib.commands.gp import SegmentStart, GpStandbyStart, MasterStop
from gppylib.commands import gp
from gppylib.commands.unix import findCmdInPath, Scp
from gppylib.operations.startSegments import MIRROR_MODE_MIRRORLESS
from gppylib.operations.unix import ListRemoteFilesByPattern, CheckRemoteFile
from test.behave_utils.gpfdist_utils.gpfdist_mgmt import Gpfdist
from test.behave_utils.utils import *
from test.behave_utils.cluster_setup import TestCluster, reset_hosts
from test.behave_utils.cluster_expand import Gpexpand
from test.behave_utils.gpexpand_dml import TestDML
from gppylib.commands.base import Command, REMOTE
from gppylib import pgconf


master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
if master_data_dir is None:
    raise Exception('Please set MASTER_DATA_DIRECTORY in environment')

def show_all_installed(gphome):
    x = platform.linux_distribution()
    name = x[0].lower()
    if 'ubuntu' in name:
        return "dpkg --get-selections --admindir=%s/share/packages/database/deb | awk '{print \$1}'" % gphome
    elif 'centos' in name:
        return "rpm -qa --dbpath %s/share/packages/database" % gphome
    else:
        raise Exception('UNKNOWN platform: %s' % str(x))

def remove_native_package_command(gphome, full_gppkg_name):
    x = platform.linux_distribution()
    name = x[0].lower()
    if 'ubuntu' in name:
        return 'fakeroot dpkg --force-not-root --log=/dev/null --instdir=%s --admindir=%s/share/packages/database/deb -r %s' % (gphome, gphome, full_gppkg_name)
    elif 'centos' in name:
        return 'rpm -e %s --dbpath %s/share/packages/database' % (full_gppkg_name, gphome)
    else:
        raise Exception('UNKNOWN platform: %s' % str(x))

def remove_gppkg_archive_command(gphome, gppkg_name):
    return 'rm -f %s/share/packages/archive/%s.gppkg' % (gphome, gppkg_name)

def create_local_demo_cluster(context, extra_config='', with_mirrors='true', with_standby='true', num_primaries=None):
    stop_database_if_started(context)

    if num_primaries is None:
        num_primaries = os.getenv('NUM_PRIMARY_MIRROR_PAIRS', 3)

    os.environ['PGPORT'] = '15432'
    cmd = """
        cd ../gpAux/gpdemo &&
        export DEMO_PORT_BASE={port_base} &&
        export NUM_PRIMARY_MIRROR_PAIRS={num_primary_mirror_pairs} &&
        export WITH_STANDBY={with_standby} &&
        export WITH_MIRRORS={with_mirrors} &&
        ./demo_cluster.sh -d && ./demo_cluster.sh -c &&
        {extra_config} ./demo_cluster.sh
    """.format(port_base=os.getenv('PORT_BASE', 15432),
               num_primary_mirror_pairs=num_primaries,
               with_mirrors=with_mirrors,
               with_standby=with_standby,
               extra_config=extra_config)
    run_command(context, cmd)

    if context.ret_code != 0:
        raise Exception('%s' % context.error_message)

def _cluster_contains_standard_demo_segments():
    """
    Returns True iff a cluster contains a master, a standby, and three
    primary/mirror pairs, and each segment is in the correct role.
    """
    # We expect four pairs -- one for each demo cluster content ID. The set
    # contains a (content, role, preferred_role) tuple for each segment.
    expected_segments = set()
    for contentid in [-1, 0, 1, 2]:
        expected_segments.add( (contentid, 'p', 'p') )
        expected_segments.add( (contentid, 'm', 'm') )

    # Now check to see if the actual segments match expectations.
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()

    actual_segments = set()
    for seg in segments:
        actual_segments.add( (seg.content, seg.role, seg.preferred_role) )

    return expected_segments == actual_segments

@given('a standard local demo cluster is running')
def impl(context):
    if (check_database_is_running(context)
        and master_data_dir.endswith("demoDataDir-1")
        and _cluster_contains_standard_demo_segments()
        and are_segments_running()):
        return

    create_local_demo_cluster(context, num_primaries=3)

@given('a standard local demo cluster is created')
def impl(context):
    create_local_demo_cluster(context, num_primaries=3)

@given('create demo cluster config')
def impl(context):
    create_local_demo_cluster(context, extra_config='ONLY_PREPARE_CLUSTER_ENV=true')

@given('the cluster config is generated with HBA_HOSTNAMES "{hba_hostnames_toggle}"')
def impl(context, hba_hostnames_toggle):
    extra_config = 'env EXTRA_CONFIG="HBA_HOSTNAMES={}" ONLY_PREPARE_CLUSTER_ENV=true'.format(hba_hostnames_toggle)
    create_local_demo_cluster(context, extra_config=extra_config)

@given('the cluster config is generated with data_checksums "{checksum_toggle}"')
def impl(context, checksum_toggle):
    extra_config = 'env EXTRA_CONFIG="HEAP_CHECKSUM={}" ONLY_PREPARE_CLUSTER_ENV=true'.format(checksum_toggle)
    create_local_demo_cluster(context, extra_config=extra_config)

@given('the cluster is generated with "{num_primaries}" primaries only')
def impl(context, num_primaries):
    os.environ['PGPORT'] = '15432'
    demoDir = os.path.abspath("%s/../gpAux/gpdemo" % os.getcwd())
    os.environ['MASTER_DATA_DIRECTORY'] = "%s/datadirs/qddir/demoDataDir-1" % demoDir

    create_local_demo_cluster(context, with_mirrors='false', with_standby='false', num_primaries=num_primaries)

    context.gpexpand_mirrors_enabled = False


@given('the user runs psql with "{psql_cmd}" against database "{dbname}"')
def impl(context, dbname, psql_cmd):
    cmd = "psql -d %s %s" % (dbname, psql_cmd)

    run_command(context, cmd)

    if context.ret_code != 0:
        raise Exception('%s' % context.error_message)


@given('the user connects to "{dbname}" with named connection "{cname}"')
def impl(context, dbname, cname):
    if not hasattr(context, 'named_conns'):
        context.named_conns = {}
    if cname in context.named_conns:
        context.named_conns[cname].close()
        del context.named_conns[cname]
    context.named_conns[cname] = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)

@given('the user create a writable external table with name "{tabname}"')
def impl(conetxt, tabname):
    dbname = 'gptest'
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        sql = ("create writable external table {tabname}(a int) location "
               "('gpfdist://host.invalid:8000/file') format 'text'").format(tabname=tabname)
        dbconn.execSQL(conn, sql)
        conn.commit()

@given('the user executes "{sql}" with named connection "{cname}"')
def impl(context, cname, sql):
    conn = context.named_conns[cname]
    dbconn.execSQL(conn, sql)
    conn.commit()


@then('the user drops the named connection "{cname}"')
def impl(context, cname):
    if cname in context.named_conns:
        context.named_conns[cname].close()
        del context.named_conns[cname]


@given('the database is running')
@then('the database is running')
def impl(context):
    start_database_if_not_started(context)
    if has_exception(context):
        raise context.exception


@given('the database is initialized with checksum "{checksum_toggle}"')
def impl(context, checksum_toggle):
    is_ok = check_database_is_running(context)

    if is_ok:
        run_command(context, "gpconfig -s data_checksums")
        if context.ret_code != 0:
            raise Exception("cannot run gpconfig: %s, stdout: %s" % (context.error_message, context.stdout_message))

        try:
            # will throw
            check_stdout_msg(context, "Values on all segments are consistent")
            check_stdout_msg(context, "Master  value: %s" % checksum_toggle)
            check_stdout_msg(context, "Segment value: %s" % checksum_toggle)
        except:
            is_ok = False

    if not is_ok:
        stop_database(context)

        os.environ['PGPORT'] = '15432'
        port_base = os.getenv('PORT_BASE', 15432)

        cmd = """
        cd ../gpAux/gpdemo; \
            export DEMO_PORT_BASE={port_base} && \
            export NUM_PRIMARY_MIRROR_PAIRS={num_primary_mirror_pairs} && \
            export WITH_MIRRORS={with_mirrors} && \
            ./demo_cluster.sh -d && ./demo_cluster.sh -c && \
            env EXTRA_CONFIG="HEAP_CHECKSUM={checksum_toggle}" ./demo_cluster.sh
        """.format(port_base=port_base,
                   num_primary_mirror_pairs=os.getenv('NUM_PRIMARY_MIRROR_PAIRS', 3),
                   with_mirrors='true',
                   checksum_toggle=checksum_toggle)

        run_command(context, cmd)

        if context.ret_code != 0:
            raise Exception('%s' % context.error_message)

        if ('PGDATABASE' in os.environ):
            run_command(context, "createdb %s" % os.getenv('PGDATABASE'))


@given('the database is not running')
@when('the database is not running')
@then('the database is not running')
def impl(context):
    stop_database_if_started(context)
    if has_exception(context):
        raise context.exception


@given('database "{dbname}" exists')
@then('database "{dbname}" exists')
def impl(context, dbname):
    create_database_if_not_exists(context, dbname)


@given('database "{dbname}" is created if not exists on host "{HOST}" with port "{PORT}" with user "{USER}"')
@then('database "{dbname}" is created if not exists on host "{HOST}" with port "{PORT}" with user "{USER}"')
def impl(context, dbname, HOST, PORT, USER):
    host = os.environ.get(HOST)
    port = 0 if os.environ.get(PORT) is None else int(os.environ.get(PORT))
    user = os.environ.get(USER)
    create_database_if_not_exists(context, dbname, host, port, user)


@when('the database "{dbname}" does not exist')
@given('the database "{dbname}" does not exist')
@then('the database "{dbname}" does not exist')
def impl(context, dbname):
    drop_database_if_exists(context, dbname)


@when('the database "{dbname}" does not exist on host "{HOST}" with port "{PORT}" with user "{USER}"')
@given('the database "{dbname}" does not exist on host "{HOST}" with port "{PORT}" with user "{USER}"')
@then('the database "{dbname}" does not exist on host "{HOST}" with port "{PORT}" with user "{USER}"')
def impl(context, dbname, HOST, PORT, USER):
    host = os.environ.get(HOST)
    port = int(os.environ.get(PORT))
    user = os.environ.get(USER)
    drop_database_if_exists(context, dbname, host, port, user)


def get_segment_hostlist():
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segment_hostlist = sorted(gparray.get_hostlist(includeMaster=False))
    if not segment_hostlist:
        raise Exception('segment_hostlist was empty')
    return segment_hostlist


@given('the user truncates "{table_list}" tables in "{dbname}"')
@when('the user truncates "{table_list}" tables in "{dbname}"')
@then('the user truncates "{table_list}" tables in "{dbname}"')
def impl(context, table_list, dbname):
    if not table_list:
        raise Exception('Table list is empty')
    tables = table_list.split(',')
    for t in tables:
        truncate_table(dbname, t.strip())


@given(
    'there is a partition table "{tablename}" has external partitions of gpfdist with file "{filename}" on port "{port}" in "{dbname}" with data')
def impl(context, tablename, dbname, filename, port):
    create_database_if_not_exists(context, dbname)
    drop_table_if_exists(context, table_name=tablename, dbname=dbname)
    create_external_partition(context, tablename, dbname, port, filename)


@given('"{dbname}" does not exist')
def impl(context, dbname):
    drop_database(context, dbname)


@given('{env_var} environment variable is not set')
def impl(context, env_var):
    if not hasattr(context, 'orig_env'):
        context.orig_env = dict()
    context.orig_env[env_var] = os.environ.get(env_var)

    if env_var in os.environ:
        del os.environ[env_var]


@then('{env_var} environment variable should be restored')
def impl(context, env_var):
    if not hasattr(context, 'orig_env'):
        raise Exception('%s can not be reset' % env_var)

    if env_var not in context.orig_env:
        raise Exception('%s can not be reset.' % env_var)

    os.environ[env_var] = context.orig_env[env_var]

    del context.orig_env[env_var]


@given('the user runs "{command}"')
@when('the user runs "{command}"')
@then('the user runs "{command}"')
def impl(context, command):
    run_gpcommand(context, command)


@given('the user asynchronously sets up to end that process in {secs} seconds')
def impl(context, secs):
    command = "sleep %d; kill -9 %d" % (int(secs), context.asyncproc.pid)
    run_async_command(context, command)


@given('the user asynchronously runs "{command}" and the process is saved')
@when('the user asynchronously runs "{command}" and the process is saved')
@then('the user asynchronously runs "{command}" and the process is saved')
def impl(context, command):
    run_gpcommand_async(context, command)


@given('the async process finished with a return code of {ret_code}')
@when('the async process finished with a return code of {ret_code}')
@then('the async process finished with a return code of {ret_code}')
def impl(context, ret_code):
    rc, stdout_value, stderr_value = context.asyncproc.communicate2()
    if rc != int(ret_code):
        raise Exception("return code of the async proccess didn't match:\n"
                        "rc: %s\n"
                        "stdout: %s\n"
                        "stderr: %s" % (rc, stdout_value, stderr_value))


@given('a user runs "{command}" with gphome "{gphome}"')
@when('a user runs "{command}" with gphome "{gphome}"')
@then('a user runs "{command}" with gphome "{gphome}"')
def impl(context, command, gphome):
    masterhost = get_master_hostname()[0][0]
    cmd = Command(name='Remove archive gppkg',
                  cmdStr=command,
                  ctxt=REMOTE,
                  remoteHost=masterhost,
                  gphome=gphome)
    cmd.run()
    context.ret_code = cmd.get_return_code()


@given('the user runs command "{command}"')
@when('the user runs command "{command}"')
@then('the user runs command "{command}"')
def impl(context, command):
    run_command(context, command)
    if has_exception(context):
        raise context.exception

@given('the user runs remote command "{command}" on host "{hostname}"')
@when('the user runs remote command "{command}" on host "{hostname}"')
@then('the user runs remote command "{command}" on host "{hostname}"')
def impl(context, command, hostname):
    run_command_remote(context,
                       command,
                       hostname,
                       os.getenv("GPHOME") + '/greenplum_path.sh',
                       'export MASTER_DATA_DIRECTORY=%s' % master_data_dir)
    if has_exception(context):
        raise context.exception

@given('the user runs command "{command}" eok')
@when('the user runs command "{command}" eok')
@then('the user runs command "{command}" eok')
def impl(context, command):
    run_command(context, command)

@when('the user runs async command "{command}"')
def impl(context, command):
    run_async_command(context, command)


@given('the user runs workload under "{dir}" with connection "{dbconn}"')
@when('the user runs workload under "{dir}" with connection "{dbconn}"')
def impl(context, dir, dbconn):
    for file in os.listdir(dir):
        if file.endswith('.sql'):
            command = '%s -f %s' % (dbconn, os.path.join(dir, file))
            run_command(context, command)


@given('the user modifies the external_table.sql file "{filepath}" with host "{HOST}" and port "{port}"')
@when('the user modifies the external_table.sql file "{filepath}" with host "{HOST}" and port "{port}"')
def impl(context, filepath, HOST, port):
    host = os.environ.get(HOST)
    substr = host + ':' + port
    modify_sql_file(filepath, substr)


@given('the user starts the gpfdist on host "{HOST}" and port "{port}" in work directory "{dir}" from remote "{ctxt}"')
@then('the user starts the gpfdist on host "{HOST}" and port "{port}" in work directory "{dir}" from remote "{ctxt}"')
def impl(context, HOST, port, dir, ctxt):
    host = os.environ.get(HOST)
    remote_gphome = os.environ.get('GPHOME')
    if not dir.startswith("/"):
        dir = os.environ.get(dir)
    gp_source_file = os.path.join(remote_gphome, 'greenplum_path.sh')
    gpfdist = Gpfdist('gpfdist on host %s' % host, dir, port, os.path.join(dir, 'gpfdist.pid'), int(ctxt), host,
                      gp_source_file)
    gpfdist.startGpfdist()


@given('the user stops the gpfdist on host "{HOST}" and port "{port}" in work directory "{dir}" from remote "{ctxt}"')
@then('the user stops the gpfdist on host "{HOST}" and port "{port}" in work directory "{dir}" from remote "{ctxt}"')
def impl(context, HOST, port, dir, ctxt):
    host = os.environ.get(HOST)
    remote_gphome = os.environ.get('GPHOME')
    if not dir.startswith("/"):
        dir = os.environ.get(dir)
    gp_source_file = os.path.join(remote_gphome, 'greenplum_path.sh')
    gpfdist = Gpfdist('gpfdist on host %s' % host, dir, port, os.path.join(dir, 'gpfdist.pid'), int(ctxt), host,
                      gp_source_file)
    gpfdist.cleanupGpfdist()

@then('{command} should print "{err_msg}" error message')
def impl(context, command, err_msg):
    check_err_msg(context, err_msg)

@when('{command} should print "{out_msg}" escaped to stdout')
@then('{command} should print "{out_msg}" escaped to stdout')
@then('{command} should print a "{out_msg}" escaped warning')
def impl(context, command, out_msg):
    check_stdout_msg(context, out_msg, True)

@when('{command} should print "{out_msg}" to stdout')
@then('{command} should print "{out_msg}" to stdout')
@then('{command} should print a "{out_msg}" warning')
def impl(context, command, out_msg):
    check_stdout_msg(context, out_msg)


@then('{command} should not print "{out_msg}" to stdout')
def impl(context, command, out_msg):
    check_string_not_present_stdout(context, out_msg)


@then('{command} should print "{out_msg}" to stdout {num} times')
def impl(context, command, out_msg, num):
    msg_list = context.stdout_message.split('\n')
    msg_list = [x.strip() for x in msg_list]

    count = msg_list.count(out_msg)
    if count != int(num):
        raise Exception("Expected %s to occur %s times. Found %d" % (out_msg, num, count))


@given('{command} should return a return code of {ret_code}')
@when('{command} should return a return code of {ret_code}')
@then('{command} should return a return code of {ret_code}')
def impl(context, command, ret_code):
    check_return_code(context, ret_code)


@given('the segments are synchronized')
@when('the segments are synchronized')
@then('the segments are synchronized')
def impl(context):
    times = 60
    sleeptime = 10

    for i in range(times):
        if are_segments_synchronized():
            return
        time.sleep(sleeptime)

    raise Exception('segments are not in sync after %d seconds' % (times * sleeptime))


@then('verify that there is no table "{tablename}" in "{dbname}"')
def impl(context, tablename, dbname):
    dbname = replace_special_char_env(dbname)
    tablename = replace_special_char_env(tablename)
    if check_table_exists(context, dbname=dbname, table_name=tablename):
        raise Exception("Table '%s' still exists when it should not" % tablename)


@then('verify that there is a "{table_type}" table "{tablename}" in "{dbname}"')
def impl(context, table_type, tablename, dbname):
    if not check_table_exists(context, dbname=dbname, table_name=tablename, table_type=table_type):
        raise Exception("Table '%s' of type '%s' does not exist when expected" % (tablename, table_type))

@then('verify that there is a "{table_type}" table "{tablename}" in "{dbname}" with "{numrows}" rows')
def impl(context, table_type, tablename, dbname, numrows):
    if not check_table_exists(context, dbname=dbname, table_name=tablename, table_type=table_type):
        raise Exception("Table '%s' of type '%s' does not exist when expected" % (tablename, table_type))
        with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
            rowcount = dbconn.execSQLForSingleton(conn, "SELECT count(*) FROM %s" % tablename)
            if rowcount != numrows:
                raise Exception("Expected to find %d rows in table %s, found %d" % (numrows, tablename, rowcount))

@then(
    'data for partition table "{table_name}" with partition level "{part_level}" is distributed across all segments on "{dbname}"')
def impl(context, table_name, part_level, dbname):
    validate_part_table_data_on_segments(context, table_name, part_level, dbname)

@then('verify that table "{tname}" in "{dbname}" has "{nrows}" rows')
def impl(context, tname, dbname, nrows):
    check_row_count(context, tname, dbname, int(nrows))

@given('schema "{schema_list}" exists in "{dbname}"')
@then('schema "{schema_list}" exists in "{dbname}"')
def impl(context, schema_list, dbname):
    schemas = [s.strip() for s in schema_list.split(',')]
    for s in schemas:
        drop_schema_if_exists(context, s.strip(), dbname)
        create_schema(context, s.strip(), dbname)


@then('the temporary file "{filename}" is removed')
def impl(context, filename):
    if os.path.exists(filename):
        os.remove(filename)


def create_table_file_locally(context, filename, table_list, location=os.getcwd()):
    tables = table_list.split('|')
    file_path = os.path.join(location, filename)
    with open(file_path, 'w') as fp:
        for t in tables:
            fp.write(t + '\n')
    context.filename = file_path


@given('there is a file "{filename}" with tables "{table_list}"')
@then('there is a file "{filename}" with tables "{table_list}"')
def impl(context, filename, table_list):
    create_table_file_locally(context, filename, table_list)


@given('the row "{row_values}" is inserted into "{table}" in "{dbname}"')
def impl(context, row_values, table, dbname):
    insert_row(context, row_values, table, dbname)


@then('verify that database "{dbname}" does not exist')
def impl(context, dbname):
    with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
        sql = """SELECT datname FROM pg_database"""
        dbs = dbconn.execSQL(conn, sql)
        if dbname in dbs:
            raise Exception('Database exists when it shouldnt "%s"' % dbname)


@given('the file "{filepath}" exists under master data directory')
def impl(context, filepath):
    fullfilepath = os.path.join(master_data_dir, filepath)
    if not os.path.isdir(os.path.dirname(fullfilepath)):
        os.makedirs(os.path.dirname(fullfilepath))
    open(fullfilepath, 'a').close()

@then('the file "{filepath}" does not exist under standby master data directory')
def impl(context, filepath):
    fullfilepath = os.path.join(context.standby_data_dir, filepath)
    cmd = "ls -al %s" % fullfilepath
    try:
        run_command_remote(context,
                           cmd,
                           context.standby_hostname,
                           os.getenv("GPHOME") + '/greenplum_path.sh',
                           'export MASTER_DATA_DIRECTORY=%s' % context.standby_data_dir,
                           validateAfter=True)
    except:
        pass
    else:
        raise Exception("file '%s' should not exist in standby master data directory" % fullfilepath)

@given('results of the sql "{sql}" db "{dbname}" are stored in the context')
@when( 'results of the sql "{sql}" db "{dbname}" are stored in the context')
def impl(context, sql, dbname):
    context.stored_sql_results = []

    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        curs = dbconn.execSQL(conn, sql)
        context.stored_sql_results = curs.fetchall()


@then('validate that following rows are in the stored rows')
def impl(context):
    for row in context.table:
        found_match = False

        for stored_row in context.stored_rows:
            match_this_row = True

            for i in range(len(stored_row)):
                value = row[i]

                if isinstance(stored_row[i], bool):
                    value = str(True if row[i] == 't' else False)

                if value != str(stored_row[i]):
                    match_this_row = False
                    break

            if match_this_row:
                found_match = True
                break

        if not found_match:
            print context.stored_rows
            raise Exception("'%s' not found in stored rows" % row)


@then('validate that first column of first stored row has "{numlines}" lines of raw output')
def impl(context, numlines):
    raw_lines_count = len(context.stored_rows[0][0].splitlines())
    numlines = int(numlines)
    if raw_lines_count != numlines:
        raise Exception("Found %d of stored query result but expected %d records" % (raw_lines_count, numlines))


def get_standby_host():
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    standby_master = [seg.getSegmentHostName() for seg in segments if seg.isSegmentStandby()]
    if len(standby_master) > 0:
        return standby_master[0]
    else:
        return []


def run_gpinitstandby(context, hostname, port, standby_data_dir, options='', remote=False):
    if '-n' in options:
        cmd = "gpinitstandby -a"
    elif remote:
        #if standby_data_dir exists on $hostname, remove it
        remove_dir(hostname, standby_data_dir)
        # create the data dir on $hostname
        create_dir(hostname, os.path.dirname(standby_data_dir))
        # We do not set port nor data dir here to test gpinitstandby's ability to autogather that info
        cmd = "gpinitstandby -a -s %s" % hostname
    else:
        cmd = "gpinitstandby -a -s %s -P %s -S %s" % (hostname, port, standby_data_dir)

    run_gpcommand(context, cmd + ' ' + options)


@when('the user initializes a standby on the same host as master with same port')
def impl(context):
    hostname = get_master_hostname('postgres')[0][0]
    temp_data_dir = tempfile.mkdtemp() + "/standby_datadir"
    run_gpinitstandby(context, hostname, os.environ.get("PGPORT"), temp_data_dir)

@when('the user initializes a standby on the same host as master and the same data directory')
def impl(context):
    hostname = get_master_hostname('postgres')[0][0]
    master_port = int(os.environ.get("PGPORT"))

    cmd = "gpinitstandby -a -s %s -P %d" % (hostname, master_port + 1)
    run_gpcommand(context, cmd)

def init_standby(context, master_hostname, options, segment_hostname):
    if master_hostname != segment_hostname:
        context.standby_hostname = segment_hostname
        context.standby_port = os.environ.get("PGPORT")
        remote = True
    else:
        context.standby_hostname = master_hostname
        context.standby_port = get_open_port()
        remote = False
    # -n option assumes gpinitstandby already ran and put standby in catalog
    if "-n" not in options:
        if remote:
            context.standby_data_dir = master_data_dir
        else:
            context.standby_data_dir = tempfile.mkdtemp() + "/standby_datadir"
    run_gpinitstandby(context, context.standby_hostname, context.standby_port, context.standby_data_dir, options,
                      remote)
    context.master_hostname = master_hostname
    context.master_port = os.environ.get("PGPORT")
    context.standby_was_initialized = True

@when('running gpinitstandby on host "{master}" to create a standby on host "{standby}"')
@given('running gpinitstandby on host "{master}" to create a standby on host "{standby}"')
def impl(context, master, standby):
    # XXX This code was cribbed from init_standby and modified to support remote
    # execution.
    context.master_hostname = master
    context.standby_hostname = standby
    context.standby_port = os.environ.get("PGPORT")
    context.standby_data_dir = master_data_dir

    remove_dir(standby, context.standby_data_dir)
    create_dir(standby, os.path.dirname(context.standby_data_dir))

    # We do not set port nor data dir here to test gpinitstandby's ability to autogather that info
    cmd = "gpinitstandby -a -s %s" % standby

    run_command_remote(context,
                       cmd,
                       context.master_hostname,
                       os.getenv("GPHOME") + '/greenplum_path.sh',
                       'export MASTER_DATA_DIRECTORY=%s' % context.standby_data_dir)

    context.stdout_position = 0
    context.master_port = os.environ.get("PGPORT")
    context.standby_was_initialized = True

@when('the user runs gpinitstandby with options "{options}"')
@then('the user runs gpinitstandby with options "{options}"')
@given('the user runs gpinitstandby with options "{options}"')
def impl(context, options):
    dbname = 'postgres'
    with dbconn.connect(dbconn.DbURL(port=os.environ.get("PGPORT"), dbname=dbname), unsetSearchPath=False) as conn:
        query = """select distinct content, hostname from gp_segment_configuration order by content limit 2;"""
        cursor = dbconn.execSQL(conn, query)

    try:
        _, master_hostname = cursor.fetchone()
        _, segment_hostname = cursor.fetchone()
    except:
        raise Exception("Did not get two rows from query: %s" % query)

    # if we have two hosts, assume we're testing on a multinode cluster
    init_standby(context, master_hostname, options, segment_hostname)

@when('the user runs gpactivatestandby with options "{options}"')
@then('the user runs gpactivatestandby with options "{options}"')
def impl(context, options):
    context.execute_steps(u'''Then the user runs command "gpactivatestandby -a %s" from standby master''' % options)
    context.standby_was_activated = True

@when('the user runs command "{command}" from standby master')
@then('the user runs command "{command}" from standby master')
def impl(context, command):
    cmd = "PGPORT=%s %s" % (context.standby_port, command)
    run_command_remote(context,
                       cmd,
                       context.standby_hostname,
                       os.getenv("GPHOME") + '/greenplum_path.sh',
                       'export MASTER_DATA_DIRECTORY=%s' % context.standby_data_dir,
                       validateAfter=False)

@when('the master goes down')
@then('the master goes down')
def impl(context):
	master = MasterStop("Stopping Master", master_data_dir, mode='immediate')
	master.run()

@when('the standby master goes down')
def impl(context):
	master = MasterStop("Stopping Master Standby", context.standby_data_dir, mode='immediate', ctxt=REMOTE,
                        remoteHost=context.standby_hostname)
	master.run(validateAfter=True)

@when('the master goes down on "{host}"')
def impl(context, host):
    master = MasterStop("Stopping Master Standby", master_data_dir, mode='immediate', ctxt=REMOTE,
                        remoteHost=host)
    master.run(validateAfter=True)

@then('clean up and revert back to original master')
def impl(context):
    # TODO: think about preserving the master data directory for debugging
    shutil.rmtree(master_data_dir, ignore_errors=True)

    if context.master_hostname != context.standby_hostname:
        # We do not set port nor data dir here to test gpinitstandby's ability to autogather that info
        cmd = "gpinitstandby -a -s %s" % context.master_hostname
    else:
        cmd = "gpinitstandby -a -s %s -P %s -S %s" % (context.master_hostname, context.master_port, master_data_dir)

    context.execute_steps(u'''Then the user runs command "%s" from standby master''' % cmd)

    master = MasterStop("Stopping current master", context.standby_data_dir, mode='immediate', ctxt=REMOTE,
                        remoteHost=context.standby_hostname)
    master.run()

    cmd = "gpactivatestandby -a -d %s" % master_data_dir
    run_gpcommand(context, cmd)

# from https://stackoverflow.com/questions/2838244/get-open-tcp-port-in-python/2838309#2838309
def get_open_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("",0))
    s.listen(1)
    port = s.getsockname()[1]
    s.close()
    return port


@given('"{path}" has its permissions set to "{perm}"')
def impl(context, path, perm):
    path = os.path.expandvars(path)
    if not os.path.exists(path):
        raise Exception('Path does not exist! "%s"' % path)
    old_permissions = os.stat(path).st_mode  # keep it as a number that has a meaningful representation in octal
    test_permissions = int(perm, 8)          # accept string input with octal semantics and convert to a raw number
    os.chmod(path, test_permissions)
    context.path_for_which_to_restore_the_permissions = path
    context.permissions_to_restore_path_to = old_permissions


@then('rely on environment.py to restore path permissions')
def impl(context):
    print "go look in environment.py to see how it uses the path and permissions on context to make sure it's cleaned up"


@when('the user runs pg_controldata against the standby data directory')
def impl(context):
    cmd = "pg_controldata " + context.standby_data_dir
    run_command_remote(context,
                       cmd,
                       context.standby_hostname,
                       os.getenv("GPHOME") + '/greenplum_path.sh',
                       'export MASTER_DATA_DIRECTORY=%s' % context.standby_data_dir)


def _process_exists(pid, host):
    """
    Returns True if a process of the given PID exists on the given host, and
    False otherwise. If host is None, this check is done locally instead of
    remotely.
    """
    if host is None:
        # Local case is easy.
        return psutil.pid_exists(pid)

    # Remote case.
    cmd = Command(name="check for pid %d" % pid,
                  cmdStr="ps -p %d > /dev/null" % pid,
                  ctxt=REMOTE,
                  remoteHost=host)

    cmd.run()
    return cmd.get_return_code() == 0


@given('user stops all {segment_type} processes')
@when('user stops all {segment_type} processes')
@then('user stops all {segment_type} processes')
def stop_segments(context, segment_type):
    if segment_type not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % segment_type)

    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    role = ROLE_PRIMARY if segment_type == 'primary' else ROLE_MIRROR

    segments = filter(lambda seg: seg.getSegmentRole() == role and seg.content != -1, gparray.getDbList())
    for seg in segments:
        # For demo_cluster tests that run on the CI gives the error 'bash: pg_ctl: command not found'
        # Thus, need to add pg_ctl to the path when ssh'ing to a demo cluster.
        subprocess.check_call(['ssh', seg.getSegmentHostName(),
                               'source %s/greenplum_path.sh && pg_ctl stop -m fast -D %s -w' % (
                                   pipes.quote(os.environ.get("GPHOME")), pipes.quote(seg.getSegmentDataDirectory()))
                               ])


@given('user can start transactions')
@when('user can start transactions')
@then('user can start transactions')
def impl(context):
    wait_for_unblocked_transactions(context)


@given('the environment variable "{var}" is set to "{val}"')
def impl(context, var, val):
    context.env_var = os.environ.get(var)
    os.environ[var] = val


@given('below sql is executed in "{dbname}" db')
@when('below sql is executed in "{dbname}" db')
def impl(context, dbname):
    sql = context.text
    execute_sql(dbname, sql)


@when('sql "{sql}" is executed in "{dbname}" db')
@then('sql "{sql}" is executed in "{dbname}" db')
def impl(context, sql, dbname):
    execute_sql(dbname, sql)


@when('execute following sql in db "{dbname}" and store result in the context')
def impl(context, dbname):
    context.stored_rows = []

    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        curs = dbconn.execSQL(conn, context.text)
        context.stored_rows = curs.fetchall()


@when('execute sql "{sql}" in db "{dbname}" and store result in the context')
def impl(context, sql, dbname):
    context.stored_rows = []

    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        curs = dbconn.execSQL(conn, sql)
        context.stored_rows = curs.fetchall()


@then('validate that "{message}" is in the stored rows')
def impl(context, message):
    for row in context.stored_rows:
        for column in row:
            if message in column:
                return

    print context.stored_rows
    print message
    raise Exception("'%s' not found in stored rows" % message)


@then('verify that file "{filename}" exists under "{path}"')
def impl(context, filename, path):
    fullpath = "%s/%s" % (path, filename)
    fullpath = os.path.expandvars(fullpath)

    if not os.path.exists(fullpath):
        raise Exception('file "%s" is not exist' % fullpath)


@given('waiting "{second}" seconds')
@when('waiting "{second}" seconds')
@then('waiting "{second}" seconds')
def impl(context, second):
    time.sleep(float(second))


def get_opened_files(filename, pidfile):
    cmd = "PATH=$PATH:/usr/bin:/usr/sbin lsof -p `cat %s` | grep %s | wc -l" % (
    pidfile, filename)
    return commands.getstatusoutput(cmd)


@when('table "{tablename}" is dropped in "{dbname}"')
@then('table "{tablename}" is dropped in "{dbname}"')
@given('table "{tablename}" is dropped in "{dbname}"')
def impl(context, tablename, dbname):
    drop_table_if_exists(context, table_name=tablename, dbname=dbname)


@given('all the segments are running')
@when('all the segments are running')
@then('all the segments are running')
def impl(context):
    if not are_segments_running():
        raise Exception("all segments are not currently running")

    return


@given('the "{seg}" segment information is saved')
@when('the "{seg}" segment information is saved')
@then('the "{seg}" segment information is saved')
def impl(context, seg):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    if seg == "primary":
        primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
        context.pseg = primary_segs[0]
        context.pseg_data_dir = context.pseg.getSegmentDataDirectory()
        context.pseg_hostname = context.pseg.getSegmentHostName()
        context.pseg_dbid = context.pseg.getSegmentDbId()
    elif seg == "mirror":
        mirror_segs = [seg for seg in gparray.getDbList() if seg.isSegmentMirror()]
        context.mseg = mirror_segs[0]
        context.mseg_hostname = context.mseg.getSegmentHostName()
        context.mseg_dbid = context.mseg.getSegmentDbId()
        context.mseg_data_dir = context.mseg.getSegmentDataDirectory()


@when('we run a sample background script to generate a pid on "{seg}" segment')
def impl(context, seg):
    if seg == "primary":
        if not hasattr(context, 'pseg_hostname'):
            raise Exception("primary seg host is not saved in the context")
        hostname = context.pseg_hostname
    elif seg == "smdw":
        if not hasattr(context, 'standby_host'):
            raise Exception("Standby host is not saved in the context")
        hostname = context.standby_host

    filename = os.path.join(os.getcwd(), './test/behave/mgmt_utils/steps/data/pid_background_script.py')

    cmd = Command(name="Remove background script on remote host", cmdStr='rm -f /tmp/pid_background_script.py',
                  remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    cmd = Command(name="Copy background script to remote host", cmdStr='scp %s %s:/tmp' % (filename, hostname))
    cmd.run(validateAfter=True)

    cmd = Command(name="Run Bg process to save pid",
                  cmdStr='sh -c "python /tmp/pid_background_script.py" &>/dev/null &', remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    cmd = Command(name="get bg pid", cmdStr="ps ux | grep pid_background_script.py | grep -v grep | awk '{print \$2}'",
                  remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)
    context.bg_pid = cmd.get_stdout()
    if not context.bg_pid:
        raise Exception("Unable to obtain the pid of the background script. Seg Host: %s, get_results: %s" %
                        (hostname, cmd.get_stdout()))


@when('the background pid is killed on "{seg}" segment')
@then('the background pid is killed on "{seg}" segment')
def impl(context, seg):
    if seg == "primary":
        if not hasattr(context, 'pseg_hostname'):
            raise Exception("primary seg host is not saved in the context")
        hostname = context.pseg_hostname
    elif seg == "smdw":
        if not hasattr(context, 'standby_host'):
            raise Exception("Standby host is not saved in the context")
        hostname = context.standby_host

    cmd = Command(name="get bg pid", cmdStr="ps ux | grep pid_background_script.py | grep -v grep | awk '{print \$2}'",
                  remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)
    pids = cmd.get_stdout().splitlines()
    for pid in pids:
        cmd = Command(name="killbg pid", cmdStr='kill -9 %s' % pid, remoteHost=hostname, ctxt=REMOTE)
        cmd.run(validateAfter=True)


@when('we generate the postmaster.pid file with the background pid on "{seg}" segment')
def impl(context, seg):
    if seg == "primary":
        if not hasattr(context, 'pseg_hostname'):
            raise Exception("primary seg host is not saved in the context")
        hostname = context.pseg_hostname
        data_dir = context.pseg_data_dir
    elif seg == "smdw":
        if not hasattr(context, 'standby_host'):
            raise Exception("Standby host is not saved in the context")
        hostname = context.standby_host
        data_dir = context.standby_host_data_dir

    pid_file = os.path.join(data_dir, 'postmaster.pid')
    pid_file_orig = pid_file + '.orig'

    cmd = Command(name="Copy pid file", cmdStr='cp %s %s' % (pid_file_orig, pid_file), remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    cpCmd = Command(name='copy pid file to master for editing', cmdStr='scp %s:%s /tmp' % (hostname, pid_file))

    cpCmd.run(validateAfter=True)

    with open('/tmp/postmaster.pid', 'r') as fr:
        lines = fr.readlines()

    lines[0] = "%s\n" % context.bg_pid

    with open('/tmp/postmaster.pid', 'w') as fw:
        fw.writelines(lines)

    cpCmd = Command(name='copy pid file to segment after editing',
                    cmdStr='scp /tmp/postmaster.pid %s:%s' % (hostname, pid_file))
    cpCmd.run(validateAfter=True)


@when('we generate the postmaster.pid file with a non running pid on the same "{seg}" segment')
def impl(context, seg):
    if seg == "primary":
        data_dir = context.pseg_data_dir
        hostname = context.pseg_hostname
    elif seg == "mirror":
        data_dir = context.mseg_data_dir
        hostname = context.mseg_hostname
    elif seg == "smdw":
        if not hasattr(context, 'standby_host'):
            raise Exception("Standby host is not saved in the context")
        hostname = context.standby_host
        data_dir = context.standby_host_data_dir

    pid_file = os.path.join(data_dir, 'postmaster.pid')
    pid_file_orig = pid_file + '.orig'

    cmd = Command(name="Copy pid file", cmdStr='cp %s %s' % (pid_file_orig, pid_file), remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    cpCmd = Command(name='copy pid file to master for editing', cmdStr='scp %s:%s /tmp' % (hostname, pid_file))

    cpCmd.run(validateAfter=True)

    # Since Command creates a short-lived SSH session, we observe the PID given
    # a throw-away remote process. Assume that the PID is unused and available on
    # the remote in the near future.
    # This pid is no longer associated with a
    # running process and won't be recycled for long enough that tests
    # have finished.
    cmd = Command(name="get non-existing pid", cmdStr="echo \$\$", remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)
    pid = cmd.get_results().stdout.strip()

    with open('/tmp/postmaster.pid', 'r') as fr:
        lines = fr.readlines()

    lines[0] = "%s\n" % pid

    with open('/tmp/postmaster.pid', 'w') as fw:
        fw.writelines(lines)

    cpCmd = Command(name='copy pid file to segment after editing',
                    cmdStr='scp /tmp/postmaster.pid %s:%s' % (hostname, pid_file))
    cpCmd.run(validateAfter=True)


@when('the user starts one "{seg}" segment')
def impl(context, seg):
    if seg == "primary":
        dbid = context.pseg_dbid
        hostname = context.pseg_hostname
        segment = context.pseg
    elif seg == "mirror":
        dbid = context.mseg_dbid
        hostname = context.mseg_hostname
        segment = context.mseg

    segStartCmd = SegmentStart(name="Starting new segment dbid %s on host %s." % (str(dbid), hostname)
                               , gpdb=segment
                               , numContentsInCluster=0  # Starting seg on it's own.
                               , era=None
                               , mirrormode=MIRROR_MODE_MIRRORLESS
                               , utilityMode=False
                               , ctxt=REMOTE
                               , remoteHost=hostname
                               , pg_ctl_wait=True
                               , timeout=300)
    segStartCmd.run(validateAfter=True)


@when('the postmaster.pid file on "{seg}" segment is saved')
def impl(context, seg):
    if seg == "primary":
        data_dir = context.pseg_data_dir
        hostname = context.pseg_hostname
    elif seg == "mirror":
        data_dir = context.mseg_data_dir
        hostname = context.mseg_hostname
    elif seg == "smdw":
        if not hasattr(context, 'standby_host'):
            raise Exception("Standby host is not saved in the context")
        hostname = context.standby_host
        data_dir = context.standby_host_data_dir

    pid_file = os.path.join(data_dir, 'postmaster.pid')
    pid_file_orig = pid_file + '.orig'

    cmd = Command(name="Copy pid file", cmdStr='cp %s %s' % (pid_file, pid_file_orig), remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)


@then('the backup pid file is deleted on "{seg}" segment')
def impl(context, seg):
    if seg == "primary":
        data_dir = context.pseg_data_dir
        hostname = context.pseg_hostname
    elif seg == "mirror":
        data_dir = context.mseg_data_dir
        hostname = context.mseg_hostname
    elif seg == "smdw":
        data_dir = context.standby_host_data_dir
        hostname = context.standby_host

    cmd = Command(name="Remove pid file", cmdStr='rm -f %s' % (os.path.join(data_dir, 'postmaster.pid.orig')),
                  remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)


@given('the standby is not initialized')
@then('the standby is not initialized')
def impl(context):
    standby = get_standby_host()
    if standby:
        context.cluster_had_standby = True
        context.standby_host = standby
        run_gpcommand(context, 'gpinitstandby -ra')

@then('verify the standby master entries in catalog')
def impl(context):
	check_segment_config_query = "SELECT * FROM gp_segment_configuration WHERE content = -1 AND role = 'm'"
	check_stat_replication_query = "SELECT * FROM pg_stat_replication"
	with dbconn.connect(dbconn.DbURL(dbname='postgres'), unsetSearchPath=False) as conn:
		segconfig = dbconn.execSQL(conn, check_segment_config_query).fetchall()
		statrep = dbconn.execSQL(conn, check_stat_replication_query).fetchall()

	context.standby_dbid = segconfig[0][0]

	if len(segconfig) != 1:
		raise Exception("gp_segment_configuration did not have standby master")

	if len(statrep) != 1:
		raise Exception("pg_stat_replication did not have standby master")

@then('verify the standby master is now acting as master')
def impl(context):
	check_segment_config_query = "SELECT * FROM gp_segment_configuration WHERE content = -1 AND role = 'p' AND preferred_role = 'p' AND dbid = %s" % context.standby_dbid
	with dbconn.connect(dbconn.DbURL(hostname=context.standby_hostname, dbname='postgres', port=context.standby_port), unsetSearchPath=False) as conn:
		segconfig = dbconn.execSQL(conn, check_segment_config_query).fetchall()

	if len(segconfig) != 1:
		raise Exception("gp_segment_configuration did not have standby master acting as new master")

@then('verify that the schema "{schema_name}" exists in "{dbname}"')
def impl(context, schema_name, dbname):
    schema_exists = check_schema_exists(context, schema_name, dbname)
    if not schema_exists:
        raise Exception("Schema '%s' does not exist in the database '%s'" % (schema_name, dbname))


@then('verify that the utility {utilname} ever does logging into the user\'s "{dirname}" directory')
def impl(context, utilname, dirname):
    absdirname = "%s/%s" % (os.path.expanduser("~"), dirname)
    if not os.path.exists(absdirname):
        raise Exception('No such directory: %s' % absdirname)
    pattern = "%s/%s_*.log" % (absdirname, utilname)
    logs_for_a_util = glob.glob(pattern)
    if not logs_for_a_util:
        raise Exception('Logs matching "%s" were not created' % pattern)


@then('verify that a log was created by {utilname} in the "{dirname}" directory')
def impl(context, utilname, dirname):
    if not os.path.exists(dirname):
        raise Exception('No such directory: %s' % dirname)
    pattern = "%s/%s_*.log" % (dirname, utilname)
    logs_for_a_util = glob.glob(pattern)
    if not logs_for_a_util:
        raise Exception('Logs matching "%s" were not created' % pattern)


@then('drop the table "{tablename}" with connection "{dbconn}"')
def impl(context, tablename, dbconn):
    command = "%s -c \'drop table if exists %s\'" % (dbconn, tablename)
    run_gpcommand(context, command)


def _get_gpAdminLogs_directory():
    return "%s/gpAdminLogs" % os.path.expanduser("~")


@given('an incomplete map file is created')
def impl(context):
    with open('/tmp/incomplete_map_file', 'w') as fd:
        fd.write('nonexistent_host,nonexistent_host')


@then('verify that function "{func_name}" exists in database "{dbname}"')
def impl(context, func_name, dbname):
    SQL = """SELECT proname FROM pg_proc WHERE proname = '%s';""" % func_name
    row_count = getRows(dbname, SQL)[0][0]
    if row_count != 'test_function':
        raise Exception('Function %s does not exist in %s"' % (func_name, dbname))

@then('verify that sequence "{seq_name}" last value is "{last_value}" in database "{dbname}"')
@when('verify that sequence "{seq_name}" last value is "{last_value}" in database "{dbname}"')
@given('verify that sequence "{seq_name}" last value is "{last_value}" in database "{dbname}"')
def impl(context, seq_name, last_value, dbname):
    SQL = """SELECT last_value FROM %s;""" % seq_name
    lv = getRows(dbname, SQL)[0][0]
    if lv != int(last_value):
        raise Exception('Sequence %s last value is not %s in %s"' % (seq_name, last_value, dbname))

@given('the user runs the command "{cmd}" in the background')
@when('the user runs the command "{cmd}" in the background')
def impl(context, cmd):
    thread.start_new_thread(run_command, (context, cmd))
    time.sleep(10)


@given('the user runs the command "{cmd}" in the background without sleep')
@when('the user runs the command "{cmd}" in the background without sleep')
def impl(context, cmd):
    thread.start_new_thread(run_command, (context, cmd))


# For any pg_hba.conf line with `host ... trust`, its address should only contain FQDN
@then('verify that the file "{filename}" contains FQDN only for trusted host')
def impl(context, filename):
    with open(filename) as fr:
        for line in fr:
            contents = line.strip()
            # for example: host all all hostname    trust
            if contents.startswith("host") and contents.endswith("trust"):
                tokens = contents.split()
                if tokens.__len__() != 5:
                    raise Exception("failed to parse pg_hba.conf line '%s'" % contents)
                hostname = tokens[3]
                if hostname.__contains__("/"):
                    # Exempt localhost. They are part of the stock config and harmless
                    net = hostname.split("/")[0]
                    if net == "127.0.0.1" or net == "::1":
                        continue
                    raise Exception("'%s' is not valid FQDN" % hostname)


# For any pg_hba.conf line with `host ... trust`, its address should only contain CIDR
@then('verify that the file "{filename}" contains CIDR only for trusted host')
def impl(context, filename):
    with open(filename) as fr:
        for line in fr:
            contents = line.strip()
            # for example: host all all hostname    trust
            if contents.startswith("host") and contents.endswith("trust"):
                tokens = contents.split()
                if tokens.__len__() != 5:
                    raise Exception("failed to parse pg_hba.conf line '%s'" % contents)
                cidr = tokens[3]
                if not cidr.__contains__("/") and cidr not in ["samenet", "samehost"]:
                    raise Exception("'%s' is not valid CIDR" % cidr)


@then('verify that the file "{filename}" contains the string "{output}"')
def impl(context, filename, output):
    contents = ''
    with open(filename) as fr:
        for line in fr:
            contents = line.strip()
    print contents
    check_stdout_msg(context, output)

@then('verify that the last line of the file "{filename}" in the master data directory contains the string "{output}" escaped')
def impl(context, filename, output):
    find_string_in_master_data_directory(context, filename, output, True)


@then('verify that the last line of the file "{filename}" in the master data directory contains the string "{output}"')
def impl(context, filename, output):
    find_string_in_master_data_directory(context, filename, output)


def find_string_in_master_data_directory(context, filename, output, escapeStr=False):
    contents = ''
    file_path = os.path.join(master_data_dir, filename)

    with codecs.open(file_path, encoding='utf-8') as f:
        for line in f:
            contents = line.strip()

    if escapeStr:
        output = re.escape(output)
    pat = re.compile(output)
    if not pat.search(contents):
        err_str = "Expected stdout string '%s' and found: '%s'" % (output, contents)
        raise Exception(err_str)


@given('verify that the file "{filename}" in the master data directory has "{some}" line starting with "{output}"')
@then('verify that the file "{filename}" in the master data directory has "{some}" line starting with "{output}"')
def impl(context, filename, some, output):
    if (some == 'some'):
        valuesShouldExist = True
    elif (some == 'no'):
        valuesShouldExist = False
    else:
        raise Exception("only 'some' and 'no' are valid inputs")
    regexStr = "%s%s" % ("^[\s]*", output)
    pat = re.compile(regexStr)
    file_path = os.path.join(master_data_dir, filename)
    with open(file_path) as fr:
        for line in fr:
            contents = line.strip()
            match = pat.search(contents)
            if not valuesShouldExist:
                if match:
                    err_str = "Expected no stdout string '%s' and found: '%s'" % (regexStr, contents)
                    raise Exception(err_str)
            else:
                if match:
                    return

    if valuesShouldExist:
        err_str = "xx Expected stdout string '%s' and found: '%s'" % (regexStr, contents)
        raise Exception(err_str)

@given('verify that the file "{filename}" in each segment data directory has "{some}" line starting with "{output}"')
@then('verify that the file "{filename}" in each segment data directory has "{some}" line starting with "{output}"')
def impl(context, filename, some, output):
    try:
        with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
            curs = dbconn.execSQL(conn, "SELECT hostname, datadir FROM gp_segment_configuration WHERE role='p' AND content > -1;")
            result = curs.fetchall()
            segment_info = [(result[s][0], result[s][1]) for s in range(len(result))]
    except Exception as e:
        raise Exception("Could not retrieve segment information: %s" % e.message)

    if (some == 'some'):
        valuesShouldExist = True
    elif (some == 'no'):
        valuesShouldExist = False
    else:
        raise Exception("only 'some' and 'no' are valid inputs")

    for info in segment_info:
        host, datadir = info
        filepath = os.path.join(datadir, filename)
        regex = "%s%s" % ("^[%s]*", output)
        cmd_str = 'ssh %s "grep -c %s %s"' % (host, regex, filepath)
        cmd = Command(name='Running remote command: %s' % cmd_str, cmdStr=cmd_str)
        cmd.run(validateAfter=False)
        try:
            val = int(cmd.get_stdout().strip())
            if not valuesShouldExist:
                if val:
                    raise Exception('File %s on host %s does start with "%s"(val error: %s)' % (filepath, host, output, val))
            else:
                if not val:
                    raise Exception('File %s on host %s does start not with "%s"(val error: %s)' % (filepath, host, output, val))
        except:
            raise Exception('File %s on host %s does start with "%s"(parse error)' % (filepath, host, output))



@then('verify that the last line of the file "{filename}" in each segment data directory contains the string "{output}"')
def impl(context, filename, output):
    segment_info = []
    try:
        with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
            curs = dbconn.execSQL(conn, "SELECT hostname, datadir FROM gp_segment_configuration WHERE role='p' AND content > -1;")
            result = curs.fetchall()
            segment_info = [(result[s][0], result[s][1]) for s in range(len(result))]
    except Exception as e:
        raise Exception("Could not retrieve segment information: %s" % e.message)

    for info in segment_info:
        host, datadir = info
        filepath = os.path.join(datadir, filename)
        cmd_str = 'ssh %s "tail -n1 %s"' % (host, filepath)
        cmd = Command(name='Running remote command: %s' % cmd_str, cmdStr=cmd_str)
        cmd.run(validateAfter=True)

        actual = cmd.get_stdout().decode('utf-8')
        if output not in actual:
            raise Exception('File %s on host %s does not contain "%s"' % (filepath, host, output))

@given('the gpfdists occupying port {port} on host "{hostfile}"')
def impl(context, port, hostfile):
    remote_gphome = os.environ.get('GPHOME')
    gp_source_file = os.path.join(remote_gphome, 'greenplum_path.sh')
    source_map_file = os.environ.get(hostfile)
    dir = '/tmp'
    ctxt = 2
    with open(source_map_file, 'r') as f:
        for line in f:
            host = line.strip().split(',')[0]
            if host in ('localhost', '127.0.0.1', socket.gethostname()):
                ctxt = 1
            gpfdist = Gpfdist('gpfdist on host %s' % host, dir, port, os.path.join('/tmp', 'gpfdist.pid'),
                              ctxt, host, gp_source_file)
            gpfdist.startGpfdist()


@then('the gpfdists running on port {port} get cleaned up from host "{hostfile}"')
def impl(context, port, hostfile):
    remote_gphome = os.environ.get('GPHOME')
    gp_source_file = os.path.join(remote_gphome, 'greenplum_path.sh')
    source_map_file = os.environ.get(hostfile)
    dir = '/tmp'
    ctxt = 2
    with open(source_map_file, 'r') as f:
        for line in f:
            host = line.strip().split(',')[0]
            if host in ('localhost', '127.0.0.1', socket.gethostname()):
                ctxt = 1
            gpfdist = Gpfdist('gpfdist on host %s' % host, dir, port, os.path.join('/tmp', 'gpfdist.pid'),
                              ctxt, host, gp_source_file)
            gpfdist.cleanupGpfdist()


@then('verify that the query "{query}" in database "{dbname}" returns "{nrows}"')
def impl(context, dbname, query, nrows):
    check_count_for_specific_query(dbname, query, int(nrows))


@then('verify that the file "{filepath}" contains "{line}"')
def impl(context, filepath, line):
    filepath = glob.glob(filepath)[0]
    if line not in open(filepath).read():
        raise Exception("The file '%s' does not contain '%s'" % (filepath, line))


@then('verify that the file "{filepath}" does not contain "{line}"')
def impl(context, filepath, line):
    filepath = glob.glob(filepath)[0]
    if line in open(filepath).read():
        raise Exception("The file '%s' does contain '%s'" % (filepath, line))


@given('database "{dbname}" is dropped and recreated')
@when('database "{dbname}" is dropped and recreated')
@then('database "{dbname}" is dropped and recreated')
def impl(context, dbname):
    drop_database_if_exists(context, dbname)
    create_database(context, dbname)


@then('validate and run gpcheckcat repair')
def impl(context):
    context.execute_steps(u'''
        Then gpcheckcat should print "repair script\(s\) generated in dir gpcheckcat.repair.*" to stdout
        Then the path "gpcheckcat.repair.*" is found in cwd "1" times
        Then run all the repair scripts in the dir "gpcheckcat.repair.*"
        And the path "gpcheckcat.repair.*" is removed from current working directory
    ''')

@given('there is a "{tabletype}" table "{tablename}" in "{dbname}" with "{numrows}" rows')
def impl(context, tabletype, tablename, dbname, numrows):
    populate_regular_table_data(context, tabletype, tablename, 'None', dbname, with_data=True, rowcount=int(numrows))


@given('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data')
@then('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data')
@when('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data')
def impl(context, tabletype, tablename, dbname):
    populate_regular_table_data(context, tabletype, tablename, 'None', dbname, with_data=True)


@given('there is a "{tabletype}" partition table "{table_name}" in "{dbname}" with data')
@then('there is a "{tabletype}" partition table "{table_name}" in "{dbname}" with data')
@when('there is a "{tabletype}" partition table "{table_name}" in "{dbname}" with data')
def impl(context, tabletype, table_name, dbname):
    create_partition(context, tablename=table_name, storage_type=tabletype, dbname=dbname, with_data=True)


@then('read pid from file "{filename}" and kill the process')
@when('read pid from file "{filename}" and kill the process')
@given('read pid from file "{filename}" and kill the process')
def impl(context, filename):
    retry = 0
    pid = None

    while retry < 5:
        try:
            with open(filename) as fr:
                pid = fr.readline().strip()
            if pid:
                break
        except:
            retry += 1
            time.sleep(retry * 0.1) # 100 millis, 200 millis, etc.

    if not pid:
        raise Exception("process id '%s' not found in the file '%s'" % (pid, filename))

    cmd = Command(name="killing pid", cmdStr='kill -9 %s' % pid)
    cmd.run(validateAfter=True)


@then('an attribute of table "{table}" in database "{dbname}" is deleted on segment with content id "{segid}"')
def impl(context, table, dbname, segid):
    local_cmd = 'psql %s -t -c "SELECT port,hostname FROM gp_segment_configuration WHERE content=%s and role=\'p\';"' % (
    dbname, segid)
    run_command(context, local_cmd)
    port, host = context.stdout_message.split("|")
    port = port.strip()
    host = host.strip()
    user = os.environ.get('USER')
    source_file = os.path.join(os.environ.get('GPHOME'), 'greenplum_path.sh')
    # Yes, the below line is ugly.  It looks much uglier when done with separate strings, given the multiple levels of escaping required.
    remote_cmd = """
ssh %s "source %s; export PGUSER=%s; export PGPORT=%s; export PGOPTIONS=\\\"-c gp_session_role=utility\\\"; psql -d %s -c \\\"SET allow_system_table_mods=true; DELETE FROM pg_attribute where attrelid=\'%s\'::regclass::oid;\\\""
""" % (host, source_file, user, port, dbname, table)
    run_command(context, remote_cmd.strip())


@then('The user runs sql "{query}" in "{dbname}" on first primary segment')
@when('The user runs sql "{query}" in "{dbname}" on first primary segment')
@given('The user runs sql "{query}" in "{dbname}" on first primary segment')
def impl(context, query, dbname):
    host, port = get_primary_segment_host_port()
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_session_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
    dbname, host, port, query)
    Command(name='Running Remote command: %s' % psql_cmd, cmdStr=psql_cmd).run(validateAfter=True)

@then('The user runs sql "{query}" in "{dbname}" on all the segments')
@when('The user runs sql "{query}" in "{dbname}" on all the segments')
@given('The user runs sql "{query}" in "{dbname}" on all the segments')
def impl(context, query, dbname):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    for seg in segments:
        host = seg.getSegmentHostName()
        if seg.isSegmentPrimary() or seg.isSegmentMaster():
            port = seg.getSegmentPort()
            psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_session_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
            dbname, host, port, query)
            Command(name='Running Remote command: %s' % psql_cmd, cmdStr=psql_cmd).run(validateAfter=True)


@then('The user runs sql file "{file}" in "{dbname}" on all the segments')
@when('The user runs sql file "{file}" in "{dbname}" on all the segments')
@given('The user runs sql file "{file}" in "{dbname}" on all the segments')
def impl(context, file, dbname):
    with open(file) as fd:
        query = fd.read().strip()
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    for seg in segments:
        host = seg.getSegmentHostName()
        if seg.isSegmentPrimary() or seg.isSegmentMaster():
            port = seg.getSegmentPort()
            psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_session_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
            dbname, host, port, query)
            Command(name='Running Remote command: %s' % psql_cmd, cmdStr=psql_cmd).run(validateAfter=True)

@then('The user runs sql "{query}" in "{dbname}" on specified segment {host}:{port} in utility mode')
@when('The user runs sql "{query}" in "{dbname}" on specified segment {host}:{port} in utility mode')
@given('The user runs sql "{query}" in "{dbname}" on specified segment {host}:{port} in utility mode')
def impl(context, query, dbname, host, port):
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_session_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
    dbname, host, port, query)
    cmd = Command(name='Running Remote command: %s' % psql_cmd, cmdStr=psql_cmd)
    cmd.run(validateAfter=True)
    context.stdout_message = cmd.get_stdout()

@then('table {table_name} exists in "{dbname}" on specified segment {host}:{port}')
@when('table {table_name} exists in "{dbname}" on specified segment {host}:{port}')
@given('table {table_name} exists in "{dbname}" on specified segment {host}:{port}')
def impl(context, table_name, dbname, host, port):
    query = "SELECT COUNT(*) FROM pg_class WHERE relname = '%s'" % table_name
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_session_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
    dbname, host, port, query)
    cmd = Command(name='Running Remote command: %s' % psql_cmd, cmdStr=psql_cmd)
    cmd.run(validateAfter=True)
    keyword = "1 row"
    if keyword not in cmd.get_stdout():
        raise Exception(context.stdout_message)


@then('The path "{path}" is removed from current working directory')
@when('The path "{path}" is removed from current working directory')
@given('The path "{path}" is removed from current working directory')
def impl(context, path):
    remove_local_path(path)


@given('the path "{path}" is found in cwd "{num}" times')
@then('the path "{path}" is found in cwd "{num}" times')
@when('the path "{path}" is found in cwd "{num}" times')
def impl(context, path, num):
    result = validate_local_path(path)
    if result != int(num):
        raise Exception("expected %s items but found %s items in path %s" % (num, result, path))


@when('the user runs all the repair scripts in the dir "{dir}"')
@then('run all the repair scripts in the dir "{dir}"')
def impl(context, dir):
    bash_files = glob.glob("%s/*.sh" % dir)
    for file in bash_files:
        run_command(context, "bash %s" % file)

        if context.ret_code != 0:
            raise Exception("Error running repair script %s: %s" % (file, context.stdout_message))

@when(
    'the entry for the table "{user_table}" is removed from "{catalog_table}" with key "{primary_key}" in the database "{db_name}"')
def impl(context, user_table, catalog_table, primary_key, db_name):
    delete_qry = "delete from %s where %s='%s'::regclass::oid;" % (catalog_table, primary_key, user_table)
    with dbconn.connect(dbconn.DbURL(dbname=db_name), unsetSearchPath=False) as conn:
        for qry in ["set allow_system_table_mods=true;", "set allow_segment_dml=true;", delete_qry]:
            dbconn.execSQL(conn, qry)
            conn.commit()


@when('the entry for the table "{user_table}" is removed from "{catalog_table}" with key "{primary_key}" in the database "{db_name}" on the first primary segment')
@given('the entry for the table "{user_table}" is removed from "{catalog_table}" with key "{primary_key}" in the database "{db_name}" on the first primary segment')
def impl(context, user_table, catalog_table, primary_key, db_name):
    host, port = get_primary_segment_host_port()
    delete_qry = "delete from %s where %s='%s'::regclass::oid;" % (catalog_table, primary_key, user_table)

    with dbconn.connect(dbconn.DbURL(dbname=db_name, port=port, hostname=host), utility=True,
                        allowSystemTableMods=True, unsetSearchPath=False) as conn:
        for qry in [delete_qry]:
            dbconn.execSQL(conn, qry)
            conn.commit()


@given('the timestamps in the repair dir are consistent')
@when('the timestamps in the repair dir are consistent')
@then('the timestamps in the repair dir are consistent')
def impl(_):
    repair_regex = "gpcheckcat.repair.*"
    timestamp = ""
    repair_dir = ""
    for file in os.listdir('.'):
        if fnmatch.fnmatch(file, repair_regex):
            repair_dir = file
            timestamp = repair_dir.split('.')[2]

    if not timestamp:
        raise Exception("Timestamp was not found")

    for file in os.listdir(repair_dir):
        if not timestamp in file:
            raise Exception("file found containing inconsistent timestamp")

@when('wait until the process "{proc}" goes down')
@then('wait until the process "{proc}" goes down')
@given('wait until the process "{proc}" goes down')
def impl(context, proc):
    is_stopped = has_process_eventually_stopped(proc)
    context.ret_code = 0 if is_stopped else 1
    if not is_stopped:
        context.error_message = 'The process %s is still running after waiting' % proc
    check_return_code(context, 0)


@when('wait until the process "{proc}" is up')
@then('wait until the process "{proc}" is up')
@given('wait until the process "{proc}" is up')
def impl(context, proc):
    cmd = Command(name='pgrep for %s' % proc, cmdStr="pgrep %s" % proc)
    start_time = current_time = datetime.now()
    while (current_time - start_time).seconds < 120:
        cmd.run()
        if cmd.get_return_code() > 1:
            raise Exception("unexpected problem with gprep, return code: %s" % cmd.get_return_code())
        if cmd.get_return_code() != 1:  # 0 means match
            break
        time.sleep(2)
        current_time = datetime.now()
    context.ret_code = cmd.get_return_code()
    context.error_message = ''
    if context.ret_code > 1:
        context.error_message = 'pgrep internal error'
    check_return_code(context, 0)  # 0 means one or more processes were matched


@given('the user creates an index for table "{table_name}" in database "{db_name}"')
@when('the user creates an index for table "{table_name}" in database "{db_name}"')
@then('the user creates an index for table "{table_name}" in database "{db_name}"')
def impl(context, table_name, db_name):
    index_qry = "create table {0}(i int primary key, j varchar); create index test_index on index_table using bitmap(j)".format(
        table_name)

    with dbconn.connect(dbconn.DbURL(dbname=db_name), unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, index_qry)
        conn.commit()

@then('the file with the fake timestamp no longer exists')
def impl(context):
    if os.path.exists(context.fake_timestamp_file):
        raise Exception("expected no file at: %s" % context.fake_timestamp_file)

@then('"{gppkg_name}" gppkg files exist on all hosts')
def impl(context, gppkg_name):
    remote_gphome = os.environ.get('GPHOME')
    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    hostlist = get_all_hostnames_as_list(context, 'template1')

    # We can assume the GPDB is installed at the same location for all hosts
    command_list_all = show_all_installed(remote_gphome)

    for hostname in set(hostlist):
        cmd = Command(name='check if internal gppkg is installed',
                      cmdStr=command_list_all,
                      ctxt=REMOTE,
                      remoteHost=hostname)
        cmd.run(validateAfter=True)

        if not gppkg_name in cmd.get_stdout():
            raise Exception( '"%s" gppkg is not installed on host: %s. \nInstalled packages: %s' % (gppkg_name, hostname, cmd.get_stdout()))


@given('"{gppkg_name}" gppkg files do not exist on any hosts')
@when('"{gppkg_name}" gppkg files do not exist on any hosts')
@then('"{gppkg_name}" gppkg files do not exist on any hosts')
def impl(context, gppkg_name):
    remote_gphome = os.environ.get('GPHOME')
    hostlist = get_all_hostnames_as_list(context, 'template1')

    # We can assume the GPDB is installed at the same location for all hosts
    command_list_all = show_all_installed(remote_gphome)

    for hostname in set(hostlist):
        cmd = Command(name='check if internal gppkg is installed',
                      cmdStr=command_list_all,
                      ctxt=REMOTE,
                      remoteHost=hostname)
        cmd.run(validateAfter=True)

        if gppkg_name in cmd.get_stdout():
            raise Exception( '"%s" gppkg is installed on host: %s. \nInstalled packages: %s' % (gppkg_name, hostname, cmd.get_stdout()))


def _remove_gppkg_from_host(context, gppkg_name, is_master_host):
    remote_gphome = os.environ.get('GPHOME')

    if is_master_host:
        hostname = get_master_hostname()[0][0] # returns a list of list
    else:
        hostlist = get_segment_hostlist()
        if not hostlist:
            raise Exception("Current GPDB setup is not a multi-host cluster.")

        # Let's just pick whatever is the first host in the list, it shouldn't
        # matter which one we remove from
        hostname = hostlist[0]

    command_list_all = show_all_installed(remote_gphome)
    cmd = Command(name='get all from the host',
                  cmdStr=command_list_all,
                  ctxt=REMOTE,
                  remoteHost=hostname)
    cmd.run(validateAfter=True)
    installed_gppkgs = cmd.get_stdout_lines()
    if not installed_gppkgs:
        raise Exception("Found no packages installed")

    full_gppkg_name = next((gppkg for gppkg in installed_gppkgs if gppkg_name in gppkg), None)
    if not full_gppkg_name:
        raise Exception("Found no matches for gppkg '%s'\n"
                        "gppkgs installed:\n%s" % (gppkg_name, installed_gppkgs))

    remove_command = remove_native_package_command(remote_gphome, full_gppkg_name)
    cmd = Command(name='Cleanly remove from the remove host',
                  cmdStr=remove_command,
                  ctxt=REMOTE,
                  remoteHost=hostname)
    cmd.run(validateAfter=True)

    remove_archive_gppkg = remove_gppkg_archive_command(remote_gphome, gppkg_name)
    cmd = Command(name='Remove archive gppkg',
                  cmdStr=remove_archive_gppkg,
                  ctxt=REMOTE,
                  remoteHost=hostname)
    cmd.run(validateAfter=True)


@when('gppkg "{gppkg_name}" is removed from a segment host')
def impl(context, gppkg_name):
    _remove_gppkg_from_host(context, gppkg_name, is_master_host=False)


@when('gppkg "{gppkg_name}" is removed from master host')
def impl(context, gppkg_name):
    _remove_gppkg_from_host(context, gppkg_name, is_master_host=True)


@given('a gphome copy is created at {location} on all hosts')
def impl(context, location):
    """
    Copies the contents of GPHOME from the local machine into a different
    directory location for all hosts in the cluster.
    """
    gphome = os.environ["GPHOME"]
    greenplum_path = path.join(gphome, 'greenplum_path.sh')

    # First replace the GPHOME envvar in greenplum_path.sh.
    subprocess.check_call([
        'sed',
        '-i.bak', # we use this backup later
        '-e', r's|^GPHOME=.*$|GPHOME={}|'.format(location),
        greenplum_path,
    ])

    try:
        # Now copy all the files over.
        hosts = set(get_all_hostnames_as_list(context, 'template1'))

        host_opts = []
        for host in hosts:
            host_opts.extend(['-h', host])

        subprocess.check_call([
            'gpscp',
            '-rv',
            ] + host_opts + [
            os.getenv('GPHOME'),
            '=:{}'.format(location),
        ])

    finally:
        # Put greenplum_path.sh back the way it was.
        subprocess.check_call([
            'mv', '{}.bak'.format(greenplum_path), greenplum_path
        ])


@then('gpAdminLogs directory has no "{expected_file}" files')
def impl(context, expected_file):
    log_dir = _get_gpAdminLogs_directory()
    files_found = glob.glob('%s/%s' % (log_dir, expected_file))
    if files_found:
        raise Exception("expected no %s files in %s, but found %s" % (expected_file, log_dir, files_found))

@given('"{filepath}" is copied to the install directory')
def impl(context, filepath):
    gphome = os.getenv("GPHOME")
    if not gphome:
        raise Exception("GPHOME must be set")
    shutil.copy(filepath, os.path.join(gphome, "bin"))


@then('{command} should print "{target}" to logfile')
def impl(context, command, target):
    log_dir = _get_gpAdminLogs_directory()
    filename = glob.glob('%s/%s_*.log' % (log_dir, command))[0]
    contents = ''
    with open(filename) as fr:
        for line in fr:
            contents += line
    if target not in contents:
        raise Exception("cannot find %s in %s" % (target, filename))

@given('verify that a role "{role_name}" exists in database "{dbname}"')
@then('verify that a role "{role_name}" exists in database "{dbname}"')
def impl(context, role_name, dbname):
    query = "select rolname from pg_roles where rolname = '%s'" % role_name
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)
    try:
        result = getRows(dbname, query)[0][0]
        if result != role_name:
            raise Exception("Role %s does not exist in database %s." % (role_name, dbname))
    except:
        raise Exception("Role %s does not exist in database %s." % (role_name, dbname))

@given('the system timezone is saved')
def impl(context):
    cmd = Command(name='Get system timezone',
                  cmdStr='date +"%Z"')
    cmd.run(validateAfter=True)
    context.system_timezone = cmd.get_stdout()

@then('the database timezone is saved')
def impl(context):
    cmd = Command(name='Get database timezone',
                  cmdStr='psql -d template1 -c "show time zone" -t')
    cmd.run(validateAfter=True)
    tz = cmd.get_stdout()
    cmd = Command(name='Get abbreviated database timezone',
                  cmdStr='psql -d template1 -c "select abbrev from pg_timezone_names where name=\'%s\';" -t' % tz)
    cmd.run(validateAfter=True)
    context.database_timezone = cmd.get_stdout()

@then('the database timezone matches the system timezone')
def step_impl(context):
    if context.database_timezone != context.system_timezone:
        raise Exception("Expected database timezone to be %s, but it was %s" % (context.system_timezone, context.database_timezone))

@then('the database timezone matches "{abbreviated_timezone}"')
def step_impl(context, abbreviated_timezone):
    if context.database_timezone != abbreviated_timezone:
        raise Exception("Expected database timezone to be %s, but it was %s" % (abbreviated_timezone, context.database_timezone))

@then('the startup timezone is saved')
def step_impl(context):
    logfile = "%s/pg_log/startup.log" % os.getenv("MASTER_DATA_DIRECTORY")
    timezone = ""
    with open(logfile) as l:
        first_line = l.readline()
        timestamp = first_line.split(",")[0]
        timezone = timestamp[-3:]
    if timezone == "":
        raise Exception("Could not find timezone information in startup.log")
    context.startup_timezone = timezone

@then('the startup timezone matches the system timezone')
def step_impl(context):
    if context.startup_timezone != context.system_timezone:
        raise Exception("Expected timezone in startup.log to be %s, but it was %s" % (context.system_timezone, context.startup_timezone))

@then('the startup timezone matches "{abbreviated_timezone}"')
def step_impl(context, abbreviated_timezone):
    if context.startup_timezone != abbreviated_timezone:
        raise Exception("Expected timezone in startup.log to be %s, but it was %s" % (abbreviated_timezone, context.startup_timezone))

@given("a working directory of the test as '{working_directory}' with mode '{mode}'")
def impl(context, working_directory, mode):
    _create_working_directory(context, working_directory, mode)

@given("a working directory of the test as '{working_directory}'")
def impl(context, working_directory):
    _create_working_directory(context, working_directory)

def _create_working_directory(context, working_directory, mode=''):
    context.working_directory = working_directory
    # Don't fail if directory already exists, which can occur for the first scenario
    shutil.rmtree(context.working_directory, ignore_errors=True)
    if (mode != ''):
        os.mkdir(context.working_directory, int(mode,8))
    else:
        os.mkdir(context.working_directory)


def _create_cluster(context, master_host, segment_host_list, hba_hostnames='0', with_mirrors=False, mirroring_configuration='group'):
    if segment_host_list == "":
        segment_host_list = []
    else:
        segment_host_list = segment_host_list.split(",")

    global master_data_dir
    master_data_dir = os.path.join(context.working_directory, 'data/master/gpseg-1')
    os.environ['MASTER_DATA_DIRECTORY'] = master_data_dir

    try:
        with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
            curs = dbconn.execSQL(conn, "select count(*) from gp_segment_configuration where role='m';")
            count = curs.fetchall()[0][0]
            if not with_mirrors and count == 0:
                print "Skipping creating a new cluster since the cluster is primary only already."
                return
            elif with_mirrors and count > 0:
                print "Skipping creating a new cluster since the cluster has mirrors already."
                return
    except:
        pass

    testcluster = TestCluster(hosts=[master_host]+segment_host_list, base_dir=context.working_directory,hba_hostnames=hba_hostnames)
    testcluster.reset_cluster()
    testcluster.create_cluster(with_mirrors=with_mirrors, mirroring_configuration=mirroring_configuration)
    context.gpexpand_mirrors_enabled = with_mirrors

@then('a cluster is created with no mirrors on "{master_host}" and "{segment_host_list}"')
@given('a cluster is created with no mirrors on "{master_host}" and "{segment_host_list}"')
def impl(context, master_host, segment_host_list):
    _create_cluster(context, master_host, segment_host_list, with_mirrors=False)

@given('with HBA_HOSTNAMES "{hba_hostnames}" a cluster is created with no mirrors on "{master_host}" and "{segment_host_list}"')
@when('with HBA_HOSTNAMES "{hba_hostnames}" a cluster is created with no mirrors on "{master_host}" and "{segment_host_list}"')
@when('with HBA_HOSTNAMES "{hba_hostnames}" a cross-subnet cluster without a standby is created with no mirrors on "{master_host}" and "{segment_host_list}"')
def impl(context, master_host, segment_host_list, hba_hostnames):
    _create_cluster(context, master_host, segment_host_list, hba_hostnames, with_mirrors=False)

@given('a cross-subnet cluster without a standby is created with mirrors on "{master_host}" and "{segment_host_list}"')
@given('a cluster is created with mirrors on "{master_host}" and "{segment_host_list}"')
def impl(context, master_host, segment_host_list):
    _create_cluster(context, master_host, segment_host_list, with_mirrors=True, mirroring_configuration='group')

@given('a cluster is created with "{mirroring_configuration}" segment mirroring on "{master_host}" and "{segment_host_list}"')
def impl(context, mirroring_configuration, master_host, segment_host_list):
    _create_cluster(context, master_host, segment_host_list, with_mirrors=True, mirroring_configuration=mirroring_configuration)

@given('the user runs gpexpand interview to add {num_of_segments} new segment and {num_of_hosts} new host "{hostnames}"')
@when('the user runs gpexpand interview to add {num_of_segments} new segment and {num_of_hosts} new host "{hostnames}"')
def impl(context, num_of_segments, num_of_hosts, hostnames):
    num_of_segments = int(num_of_segments)
    num_of_hosts = int(num_of_hosts)

    hosts = []
    if num_of_hosts > 0:
        hosts = hostnames.split(',')
        if num_of_hosts != len(hosts):
            raise Exception("Incorrect amount of hosts. number of hosts:%s\nhostnames: %s" % (num_of_hosts, hosts))

    base_dir = "/tmp"
    if hasattr(context, "temp_base_dir"):
        base_dir = context.temp_base_dir
    elif hasattr(context, "working_directory"):
        base_dir = context.working_directory
    primary_dir = os.path.join(base_dir, 'data', 'primary')
    mirror_dir = ''
    if context.gpexpand_mirrors_enabled:
        mirror_dir = os.path.join(base_dir, 'data', 'mirror')

    directory_pairs = []
    # we need to create the tuples for the interview to work.
    for i in range(0, num_of_segments):
        directory_pairs.append((primary_dir,mirror_dir))

    gpexpand = Gpexpand(context, working_directory=context.working_directory)
    output, returncode = gpexpand.do_interview(hosts=hosts,
                                               num_of_segments=num_of_segments,
                                               directory_pairs=directory_pairs,
                                               has_mirrors=context.gpexpand_mirrors_enabled)
    if returncode != 0:
        raise Exception("*****An error occured*****:\n %s" % output)

@given('there are no gpexpand_inputfiles')
def impl(context):
    map(os.remove, glob.glob("gpexpand_inputfile*"))

@when('the user runs gpexpand with the latest gpexpand_inputfile with additional parameters {additional_params}')
def impl(context, additional_params=''):
    gpexpand = Gpexpand(context, working_directory=context.working_directory)
    ret_code, std_err, std_out = gpexpand.initialize_segments(additional_params)
    if ret_code != 0:
        raise Exception("gpexpand exited with return code: %d.\nstderr=%s\nstdout=%s" % (ret_code, std_err, std_out))

@when('the user runs gpexpand with the latest gpexpand_inputfile without ret code check')
def impl(context):
    gpexpand = Gpexpand(context, working_directory=context.working_directory)
    gpexpand.initialize_segments()

@when('the user runs gpexpand to redistribute with duration "{duration}"')
def impl(context, duration):
    _gpexpand_redistribute(context, duration)

@when('the user runs gpexpand to redistribute with the --end flag')
def impl(context):
    _gpexpand_redistribute(context, endtime=True)

@when('the user runs gpexpand to redistribute')
def impl(context):
    _gpexpand_redistribute(context)

def _gpexpand_redistribute(context, duration=False, endtime=False):
    gpexpand = Gpexpand(context, working_directory=context.working_directory)
    context.command = gpexpand
    ret_code, std_err, std_out = gpexpand.redistribute(duration, endtime)
    if duration or endtime:
        if ret_code != 0:
            # gpexpand exited on time, it's expected
            return
        else:
            raise Exception("gpexpand didn't stop at duration / endtime.\nstderr=%s\nstdout=%s" % (std_err, std_out))
    if ret_code != 0:
        raise Exception("gpexpand exited with return code: %d.\nstderr=%s\nstdout=%s" % (ret_code, std_err, std_out))

@given('expanded preferred primary on segment "{segment_id}" has failed')
def step_impl(context, segment_id):
    stop_primary(context, int(segment_id))
    wait_for_unblocked_transactions(context)

@given('the user runs gpexpand with a static inputfile for a two-node cluster with mirrors')
def impl(context):
    inputfile_contents = """
sdw1|sdw1|20502|/tmp/gpexpand_behave/two_nodes/data/primary/gpseg2|6|2|p
sdw2|sdw2|21502|/tmp/gpexpand_behave/two_nodes/data/mirror/gpseg2|8|2|m
sdw2|sdw2|20503|/tmp/gpexpand_behave/two_nodes/data/primary/gpseg3|7|3|p
sdw1|sdw1|21503|/tmp/gpexpand_behave/two_nodes/data/mirror/gpseg3|9|3|m"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    inputfile_name = "%s/gpexpand_inputfile_%s" % (context.working_directory, timestamp)
    with open(inputfile_name, 'w') as fd:
        fd.write(inputfile_contents)

    gpexpand = Gpexpand(context, working_directory=context.working_directory)
    ret_code, std_err, std_out = gpexpand.initialize_segments()
    if ret_code != 0:
        raise Exception("gpexpand exited with return code: %d.\nstderr=%s\nstdout=%s" % (ret_code, std_err, std_out))

@when('the user runs gpexpand with a static inputfile for a single-node cluster with mirrors')
def impl(context):
    inputfile_contents = """sdw1|sdw1|20502|/tmp/gpexpand_behave/data/primary/gpseg2|6|2|p
sdw1|sdw1|21502|/tmp/gpexpand_behave/data/mirror/gpseg2|8|2|m
sdw1|sdw1|20503|/tmp/gpexpand_behave/data/primary/gpseg3|7|3|p
sdw1|sdw1|21503|/tmp/gpexpand_behave/data/mirror/gpseg3|9|3|m"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    inputfile_name = "%s/gpexpand_inputfile_%s" % (context.working_directory, timestamp)
    with open(inputfile_name, 'w') as fd:
        fd.write(inputfile_contents)

    gpexpand = Gpexpand(context, working_directory=context.working_directory)
    ret_code, std_err, std_out = gpexpand.initialize_segments()
    if ret_code != 0:
        raise Exception("gpexpand exited with return code: %d.\nstderr=%s\nstdout=%s" % (ret_code, std_err, std_out))

@when('the user runs gpexpand with a static inputfile for a single-node cluster with mirrors without ret code check')
def impl(context):
    inputfile_contents = """sdw1|sdw1|20502|/data/gpdata/gpexpand/data/primary/gpseg2|7|2|p
sdw1|sdw1|21502|/data/gpdata/gpexpand/data/mirror/gpseg2|8|2|m"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    inputfile_name = "%s/gpexpand_inputfile_%s" % (context.working_directory, timestamp)
    with open(inputfile_name, 'w') as fd:
        fd.write(inputfile_contents)

    gpexpand = Gpexpand(context, working_directory=context.working_directory)
    gpexpand.initialize_segments()

@given('the master pid has been saved')
def impl(context):
    data_dir = os.path.join(context.working_directory,
                            'data/master/gpseg-1')
    context.master_pid = gp.get_postmaster_pid_locally(data_dir)

@then('verify that the master pid has not been changed')
def impl(context):
    data_dir = os.path.join(context.working_directory,
                            'data/master/gpseg-1')
    current_master_pid = gp.get_postmaster_pid_locally(data_dir)
    if context.master_pid == current_master_pid:
        return

    raise Exception("The master pid has been changed.\nprevious: %s\ncurrent: %s" % (context.master_pid, current_master_pid))

@then('the numsegments of table "{tabname}" is {numsegments}')
def impl(context, tabname, numsegments):
    dbname = 'gptest'
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = "select numsegments from gp_distribution_policy where localoid = '{tabname}'::regclass".format(tabname=tabname)
        ns = dbconn.execSQLForSingleton(conn, query)

    if ns == int(numsegments):
        return

    raise Exception("The numsegments of the writable external table {tabname} is {ns} (expected to be {numsegments})".format(tabname=tabname,
                                                                                                                             ns=str(ns),
                                                                                                                             numsegments=str(numsegments)))

@given('the number of segments have been saved')
@then('the number of segments have been saved')
def impl(context):
    dbname = 'gptest'
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = """SELECT count(*) from gp_segment_configuration where -1 < content"""
        context.start_data_segments = dbconn.execSQLForSingleton(conn, query)

@given('the gp_segment_configuration have been saved')
@when('the gp_segment_configuration have been saved')
@then('the gp_segment_configuration have been saved')
def impl(context):
    dbname = 'gptest'
    gp_segment_conf_backup = {}
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = """SELECT count(*) from gp_segment_configuration where -1 < content"""
        segment_count = int(dbconn.execSQLForSingleton(conn, query))
        query = """SELECT * from gp_segment_configuration where -1 < content order by dbid"""
        cursor = dbconn.execSQL(conn, query)
        for i in range(0, segment_count):
            dbid, content, role, preferred_role, mode, status,\
            port, hostname, address, datadir = cursor.fetchone();
            gp_segment_conf_backup[dbid] = {}
            gp_segment_conf_backup[dbid]['content'] = content
            gp_segment_conf_backup[dbid]['role'] = role
            gp_segment_conf_backup[dbid]['preferred_role'] = preferred_role
            gp_segment_conf_backup[dbid]['mode'] = mode
            gp_segment_conf_backup[dbid]['status'] = status
            gp_segment_conf_backup[dbid]['port'] = port
            gp_segment_conf_backup[dbid]['hostname'] = hostname
            gp_segment_conf_backup[dbid]['address'] = address
            gp_segment_conf_backup[dbid]['datadir'] = datadir
    context.gp_segment_conf_backup = gp_segment_conf_backup

@given('verify the gp_segment_configuration has been restored')
@when('verify the gp_segment_configuration has been restored')
@then('verify the gp_segment_configuration has been restored')
def impl(context):
    dbname = 'gptest'
    gp_segment_conf_backup = {}
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = """SELECT count(*) from gp_segment_configuration where -1 < content"""
        segment_count = int(dbconn.execSQLForSingleton(conn, query))
        query = """SELECT * from gp_segment_configuration where -1 < content order by dbid"""
        cursor = dbconn.execSQL(conn, query)
        for i in range(0, segment_count):
            dbid, content, role, preferred_role, mode, status,\
            port, hostname, address, datadir = cursor.fetchone();
            gp_segment_conf_backup[dbid] = {}
            gp_segment_conf_backup[dbid]['content'] = content
            gp_segment_conf_backup[dbid]['role'] = role
            gp_segment_conf_backup[dbid]['preferred_role'] = preferred_role
            gp_segment_conf_backup[dbid]['mode'] = mode
            gp_segment_conf_backup[dbid]['status'] = status
            gp_segment_conf_backup[dbid]['port'] = port
            gp_segment_conf_backup[dbid]['hostname'] = hostname
            gp_segment_conf_backup[dbid]['address'] = address
            gp_segment_conf_backup[dbid]['datadir'] = datadir
    if context.gp_segment_conf_backup != gp_segment_conf_backup:
        raise Exception("gp_segment_configuration has not been restored")

@given('user has created {table_name} table')
def impl(context, table_name):
    dbname = 'gptest'
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = """CREATE TABLE %s(a INT)""" % table_name
        dbconn.execSQL(conn, query)
        conn.commit()

@given('a long-run read-only transaction exists on {table_name}')
def impl(context, table_name):
    dbname = 'gptest'
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)
    context.long_run_select_only_conn = conn

    query = """SELECT gp_segment_id, * from %s order by 1, 2""" % table_name
    data_result = dbconn.execSQL(conn, query).fetchall()
    context.long_run_select_only_data_result = data_result

    query = """SELECT txid_current()"""
    xid = dbconn.execSQLForSingleton(conn, query)
    context.long_run_select_only_xid = xid

@then('verify that long-run read-only transaction still exists on {table_name}')
def impl(context, table_name):
    dbname = 'gptest'
    conn = context.long_run_select_only_conn

    query = """SELECT gp_segment_id, * from %s order by 1, 2""" % table_name
    data_result = dbconn.execSQL(conn, query).fetchall()

    query = """SELECT txid_current()"""
    xid = dbconn.execSQLForSingleton(conn, query)

    if (xid != context.long_run_select_only_xid or
        data_result != context.long_run_select_only_data_result):
        error_str = "Incorrect xid or select result of long run read-only transaction: \
                xid(before %s, after %), result(before %s, after %s)"
        raise Exception(error_str % (context.long_run_select_only_xid, xid, context.long_run_select_only_data_result, data_result))

@given('a long-run transaction starts')
def impl(context):
    dbname = 'gptest'
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)
    context.long_run_conn = conn

    query = """SELECT txid_current()"""
    xid = dbconn.execSQLForSingleton(conn, query)
    context.long_run_xid = xid

@then('verify that long-run transaction aborted for changing the catalog by creating table {table_name}')
def impl(context, table_name):
    dbname = 'gptest'
    conn = context.long_run_conn

    query = """SELECT txid_current()"""
    xid = dbconn.execSQLForSingleton(conn, query)
    if context.long_run_xid != xid:
        raise Exception("Incorrect xid of long run transaction: before %s, after %s" %
                        (context.long_run_xid, xid));

    query = """CREATE TABLE %s (a INT)""" % table_name
    try:
        data_result = dbconn.execSQL(conn, query)
    except Exception, msg:
        key_msg = "FATAL:  cluster is expaneded"
        if key_msg not in msg.__str__():
            raise Exception("transaction not abort correctly, errmsg:%s" % msg)
    else:
        raise Exception("transaction not abort, result:%s" % data_result)

@when('verify that the cluster has {num_of_segments} new segments')
@then('verify that the cluster has {num_of_segments} new segments')
def impl(context, num_of_segments):
    dbname = 'gptest'
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = """SELECT dbid, content, role, preferred_role, mode, status, port, hostname, address, datadir from gp_segment_configuration;"""
        rows = dbconn.execSQL(conn, query).fetchall()
        end_data_segments = 0
        for row in rows:
            content = row[1]
            status = row[5]
            if content > -1 and status == 'u':
                end_data_segments += 1

    if int(num_of_segments) == int(end_data_segments - context.start_data_segments):
        return

    raise Exception("Incorrect amount of segments.\nprevious: %s\ncurrent:"
            "%s\ndump of gp_segment_configuration: %s" %
            (context.start_data_segments, end_data_segments, rows))

@given('the cluster is setup for an expansion on hosts "{hostnames}"')
def impl(context, hostnames):
    hosts = hostnames.split(",")
    base_dir = "/tmp"
    if hasattr(context, "temp_base_dir"):
        base_dir = context.temp_base_dir
    elif hasattr(context, "working_directory"):
        base_dir = context.working_directory
    for host in hosts:
        cmd = Command(name='create data directories for expansion',
                      cmdStr="mkdir -p %s/data/primary; mkdir -p %s/data/mirror" % (base_dir, base_dir),
                      ctxt=REMOTE,
                      remoteHost=host)
        cmd.run(validateAfter=True)

@given("a temporary directory under '{tmp_base_dir}' with mode '{mode}' is created")
@given('a temporary directory under "{tmp_base_dir}" to expand into')
def make_temp_dir(context,tmp_base_dir, mode=''):
    if not tmp_base_dir:
        raise Exception("tmp_base_dir cannot be empty")
    if not os.path.exists(tmp_base_dir):
        os.mkdir(tmp_base_dir)
    context.temp_base_dir = tempfile.mkdtemp(dir=tmp_base_dir)
    if mode:
        os.chmod(path.normpath(path.join(tmp_base_dir, context.temp_base_dir)),
                 int(mode,8))

@given('the new host "{hostnames}" is ready to go')
def impl(context, hostnames):
    hosts = hostnames.split(',')
    if hasattr(context, "working_directory"):
        reset_hosts(hosts, context.working_directory)
    if hasattr(context, "temp_base_dir"):
        reset_hosts(hosts, context.temp_base_dir)


@given('user has created expansiontest tables')
@then('user has created expansiontest tables')
def impl(context):
    dbname = 'gptest'
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        for i in range(3):
            query = """drop table if exists expansiontest%s""" % (i)
            dbconn.execSQL(conn, query)
            query = """create table expansiontest%s(a int)""" % (i)
            dbconn.execSQL(conn, query)
        conn.commit()

@then('the tables have finished expanding')
def impl(context):
    dbname = 'postgres'
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = """select fq_name from gpexpand.status_detail WHERE expansion_finished IS NULL"""
        cursor = dbconn.execSQL(conn, query)

        row = cursor.fetchone()
        if row:
            raise Exception("table %s has not finished expanding" % row[0])

@given('an FTS probe is triggered')
@when('an FTS probe is triggered')
def impl(context):
    with dbconn.connect(dbconn.DbURL(dbname='postgres'), unsetSearchPath=False) as conn:
        dbconn.execSQLForSingleton(conn, "SELECT gp_request_fts_probe_scan()")

@then('verify that gpstart on original master fails due to lower Timeline ID')
def step_impl(context):
    ''' This assumes that gpstart still checks for Timeline ID if a standby master is present '''
    context.execute_steps(u'''
                            When the user runs "gpstart -a"
                            Then gpstart should return a return code of 2
                            And gpstart should print "Standby activated, this node no more can act as master." to stdout
                            ''')

@then('verify gpstate with options "{options}" output is correct')
def step_impl(context, options):
    if '-f' in options:
        if context.standby_hostname not in context.stdout_message or \
                context.standby_data_dir not in context.stdout_message or \
                str(context.standby_port) not in context.stdout_message:
            raise Exception("gpstate -f output is missing expected standby master information")
    elif '-s' in options:
        if context.standby_hostname not in context.stdout_message or \
                context.standby_data_dir not in context.stdout_message or \
                str(context.standby_port) not in context.stdout_message:
            raise Exception("gpstate -s output is missing expected master information")
    elif '-Q' in options:
        for stdout_line in context.stdout_message.split('\n'):
            if 'up segments, from configuration table' in stdout_line:
                segments_up = int(re.match(".*of up segments, from configuration table\s+=\s+([0-9]+)", stdout_line).group(1))
                if segments_up <= 1:
                    raise Exception("gpstate -Q output does not match expectations of more than one segment up")

            if 'down segments, from configuration table' in stdout_line:
                segments_down = int(re.match(".*of down segments, from configuration table\s+=\s+([0-9]+)", stdout_line).group(1))
                if segments_down != 0:
                    raise Exception("gpstate -Q output does not match expectations of all segments up")
                break ## down segments comes after up segments, so we can break here
    elif '-m' in options:
        dbname = 'postgres'
        with dbconn.connect(dbconn.DbURL(hostname=context.standby_hostname, port=context.standby_port, dbname=dbname), unsetSearchPath=False) as conn:
            query = """select datadir, port from pg_catalog.gp_segment_configuration where role='m' and content <> -1;"""
            cursor = dbconn.execSQL(conn, query)

        for i in range(cursor.rowcount):
            datadir, port = cursor.fetchone()
            if datadir not in context.stdout_message or \
                str(port) not in context.stdout_message:
                    raise Exception("gpstate -m output missing expected mirror info, datadir %s port %d" %(datadir, port))
    else:
        raise Exception("no verification for gpstate option given")

@given('ensure the standby directory does not exist')
def impl(context):
    run_command(context, 'rm -rf $MASTER_DATA_DIRECTORY/newstandby')
    run_command(context, 'rm -rf /tmp/gpinitsystemtest && mkdir /tmp/gpinitsystemtest')

@when('initialize a cluster with standby using "{config_file}"')
def impl(context, config_file):
    run_gpcommand(context, 'gpinitsystem -a -I %s -l /tmp/gpinitsystemtest -s localhost -P 21100 -S $MASTER_DATA_DIRECTORY/newstandby -h ../gpAux/gpdemo/hostfile' % config_file)
    check_return_code(context, 0)

@when('initialize a cluster using "{config_file}"')
def impl(context, config_file):
    run_gpcommand(context, 'gpinitsystem -a -I %s -l /tmp/' % config_file)
    check_return_code(context, 0)

@when('generate cluster config file "{config_file}"')
def impl(context, config_file):
    run_gpcommand(context, 'gpinitsystem -a -c ../gpAux/gpdemo/clusterConfigFile -O %s' % config_file)
    check_return_code(context, 0)

@when('check segment conf: postgresql.conf')
@then('check segment conf: postgresql.conf')
def step_impl(context):
    query = "select dbid, port, hostname, datadir from gp_segment_configuration where content >= 0"
    conn = dbconn.connect(dbconn.DbURL(dbname='postgres'), unsetSearchPath=False)
    segments = dbconn.execSQL(conn, query).fetchall()
    for segment in segments:
        dbid = "'%s'" % segment[0]
        port = "'%s'" % segment[1]
        hostname = segment[2]
        datadir = segment[3]

        ## check postgresql.conf
        remote_postgresql_conf = "%s/%s" % (datadir, 'postgresql.conf')
        local_conf_copy = os.path.join(os.getenv("MASTER_DATA_DIRECTORY"), "%s.%s" % ('postgresql.conf', hostname))
        cmd = Command(name="Copy remote conf to local to diff",
                    cmdStr='scp %s:%s %s' % (hostname, remote_postgresql_conf, local_conf_copy))
        cmd.run(validateAfter=True)

        dic = pgconf.readfile(filename=local_conf_copy)
        if str(dic['port']) != port:
            raise Exception("port value in postgresql.conf of %s is incorrect. Expected:%s, given:%s" %
                            (hostname, port, dic['port']))

@given('the transactions are started for dml')
def impl(context):
    dbname = 'gptest'
    context.dml_jobs = []
    for dml in ['insert', 'update', 'delete']:
        job = TestDML.create(dbname, dml)
        job.start()
        context.dml_jobs.append((dml, job))

@then('verify the dml results and commit')
def impl(context):
    dbname = 'gptest'

    for dml, job in context.dml_jobs:
        code, message = job.stop()
        if not code:
            raise Exception(message)

@then('verify the dml results again in a new transaction')
def impl(context):
    dbname = 'gptest'
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)

    for dml, job in context.dml_jobs:
        code, message = job.reverify(conn)
        if not code:
            raise Exception(message)
@given('distribution information from table "{table}" with data in "{dbname}" is saved')
def impl(context, table, dbname):
    context.pre_redistribution_row_count = _get_row_count_per_segment(table, dbname)

@then('distribution information from table "{table}" with data in "{dbname}" is verified against saved data')
def impl(context, table, dbname):
    pre_distribution_row_count = context.pre_redistribution_row_count
    post_distribution_row_count = _get_row_count_per_segment(table, dbname)

    if len(pre_distribution_row_count) >= len(post_distribution_row_count):
        raise Exception("Failed to redistribute table. Expected to have more than %d segments, got %d segments" % (len(pre_distribution_row_count), len(post_distribution_row_count)))

    post_distribution_num_segments = 0
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = "SELECT count(DISTINCT content) FROM gp_segment_configuration WHERE content != -1;"
        cursor = dbconn.execSQL(conn, query)
        post_distribution_num_segments = cursor.fetchone()[0]

    if len(post_distribution_row_count) != post_distribution_num_segments:
        raise Exception("Failed to redistribute table %s. Expected table to have data on %d segments, but found %d segments" % (table, post_distribution_num_segments, len(post_distribution_row_count)))

    if sum(pre_distribution_row_count) != sum(post_distribution_row_count):
        raise Exception("Redistributed data does not match pre-redistribution data. Actual: %d, Expected: %d" % (sum(post_distribution_row_count), sum(pre_distribution_row_count)))

    mean = sum(post_distribution_row_count) / len(post_distribution_row_count)
    variance = sum(pow(row_count - mean, 2) for row_count in post_distribution_row_count) / len(post_distribution_row_count)
    std_deviation = math.sqrt(variance)
    std_error = std_deviation / math.sqrt(len(post_distribution_row_count))
    relative_std_error = std_error / mean
    tolerance = 0.01
    if relative_std_error > tolerance:
        raise Exception("Unexpected variance for redistributed data in table %s. Relative standard error %f exceeded tolerance factor of %f." %
                (table, relative_std_error, tolerance))

def _get_row_count_per_segment(table, dbname):
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        query = "SELECT gp_segment_id,COUNT(i) FROM %s GROUP BY gp_segment_id;" % table
        cursor = dbconn.execSQL(conn, query)
        rows = cursor.fetchall()
        return [row[1] for row in rows] # indices are the gp segment id's, so no need to store them explicitly

@given('set fault inject "{fault}"')
@then('set fault inject "{fault}"')
@when('set fault inject "{fault}"')
def impl(context, fault):
    os.environ['GPMGMT_FAULT_POINT'] = fault

@given('unset fault inject')
@then('unset fault inject')
@when('unset fault inject')
def impl(context):
    os.environ['GPMGMT_FAULT_POINT'] = ""

@given('run rollback')
@then('run rollback')
@when('run rollback')
def impl(context):
    gpexpand = Gpexpand(context, working_directory=context.working_directory)
    ret_code, std_err, std_out = gpexpand.rollback()
    if ret_code != 0:
        raise Exception("rollback exited with return code: %d.\nstderr=%s\nstdout=%s" % (ret_code, std_err, std_out))

@given('create database schema table with special character')
@then('create database schema table with special character')
def impl(context):
    dbname = ' a b."\'\\\\'
    escape_dbname = dbname.replace('\\', '\\\\').replace('"', '\\"')
    createdb_cmd = "createdb \"%s\"" % escape_dbname
    run_command(context, createdb_cmd)

    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        #special char table
        query = 'create table " a b.""\'\\\\"(c1 int);'
        dbconn.execSQL(conn, query)
        query = 'create schema " a b.""\'\\\\";'
        dbconn.execSQL(conn, query)
        #special char schema and table
        query = 'create table " a b.""\'\\\\"." a b.""\'\\\\"(c1 int);'
        dbconn.execSQL(conn, query)

        #special char partition table
        query = """
CREATE TABLE \" a b.'\"\"\\\\\" (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)

  SUBPARTITION BY RANGE (month)
    SUBPARTITION TEMPLATE (
       START (1) END (13) EVERY (4),
       DEFAULT SUBPARTITION other_months )
( START (2008) END (2016) EVERY (1),
  DEFAULT PARTITION outlying_years);
"""
        dbconn.execSQL(conn, query)
        #special char schema and partition table
        query = """
CREATE TABLE \" a b.\"\"'\\\\\".\" a b.'\"\"\\\\\" (id int, year int, month int, day int,
region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)

  SUBPARTITION BY RANGE (month)
    SUBPARTITION TEMPLATE (
       START (1) END (13) EVERY (4),
       DEFAULT SUBPARTITION other_months )
( START (2008) END (2016) EVERY (1),
  DEFAULT PARTITION outlying_years);
"""
        dbconn.execSQL(conn, query)
        conn.commit()

@given('the database "{dbname}" is broken with "{broken}" orphaned toast tables only on segments with content IDs "{contentIDs}"')
def break_orphaned_toast_tables(context, dbname, broken, contentIDs=None):
    drop_database_if_exists(context, dbname)
    create_database(context, dbname)

    sql = ''
    if broken == 'bad reference':
        sql = '''
DROP TABLE IF EXISTS bad_reference;
CREATE TABLE bad_reference (a text);
UPDATE pg_class SET reltoastrelid = 0 WHERE relname = 'bad_reference';'''
    if broken == 'mismatched non-cyclic':
        sql = '''
DROP TABLE IF EXISTS mismatch_one;
CREATE TABLE mismatch_one (a text);

DROP TABLE IF EXISTS mismatch_two;
CREATE TABLE mismatch_two (a text);

DROP TABLE IF EXISTS mismatch_three;
CREATE TABLE mismatch_three (a text);

-- 1 -> 2 -> 3
UPDATE pg_class SET reltoastrelid = (SELECT reltoastrelid FROM pg_class WHERE relname = 'mismatch_two') WHERE relname = 'mismatch_one';
UPDATE pg_class SET reltoastrelid = (SELECT reltoastrelid FROM pg_class WHERE relname = 'mismatch_three') WHERE relname = 'mismatch_two';'''
    if broken == 'mismatched cyclic':
        sql = '''
DROP TABLE IF EXISTS mismatch_fixed;
CREATE TABLE mismatch_fixed (a text);

DROP TABLE IF EXISTS mismatch_one;
CREATE TABLE mismatch_one (a text);

DROP TABLE IF EXISTS mismatch_two;
CREATE TABLE mismatch_two (a text);

DROP TABLE IF EXISTS mismatch_three;
CREATE TABLE mismatch_three (a text);

-- fixed -> 1 -> 2 -> 3 -> 1
UPDATE pg_class SET reltoastrelid = (SELECT reltoastrelid FROM pg_class WHERE relname = 'mismatch_one') WHERE relname = 'mismatch_fixed'; -- "save" the reltoastrelid
UPDATE pg_class SET reltoastrelid = (SELECT reltoastrelid FROM pg_class WHERE relname = 'mismatch_two') WHERE relname = 'mismatch_one';
UPDATE pg_class SET reltoastrelid = (SELECT reltoastrelid FROM pg_class WHERE relname = 'mismatch_three') WHERE relname = 'mismatch_two';
UPDATE pg_class SET reltoastrelid = (SELECT reltoastrelid FROM pg_class WHERE relname = 'mismatch_fixed') WHERE relname = 'mismatch_three';'''
    if broken == "bad dependency":
        sql = '''
DROP TABLE IF EXISTS bad_dependency;
CREATE TABLE bad_dependency (a text);

DELETE FROM pg_depend WHERE refobjid = 'bad_dependency'::regclass;'''
    if broken == "double orphan - no parent":
        sql = '''
DROP TABLE IF EXISTS double_orphan_no_parent;
CREATE TABLE double_orphan_no_parent (a text);

DELETE FROM pg_depend WHERE refobjid = 'double_orphan_no_parent'::regclass;
DROP TABLE double_orphan_no_parent;'''
    if broken == "double orphan - valid parent":
        sql = '''
DROP TABLE IF EXISTS double_orphan_valid_parent;
CREATE TABLE double_orphan_valid_parent (a text);

-- save double_orphan_valid_parent toast table oid
CREATE TEMP TABLE first_orphan_toast AS
    SELECT oid, relname FROM pg_class WHERE oid = (SELECT reltoastrelid FROM pg_class WHERE oid = 'double_orphan_valid_parent'::regclass);

-- create a orphan toast table
DELETE FROM pg_depend WHERE objid = (SELECT oid FROM first_orphan_toast);

DROP TABLE double_orphan_valid_parent;

-- recreate double_orphan_valid_parent table to create a second valid toast table
CREATE TABLE double_orphan_valid_parent (a text);

-- save the second toast table oid from recreating double_orphan_valid_parent
CREATE TEMP TABLE second_orphan_toast AS
    SELECT oid, relname FROM pg_class WHERE oid = (SELECT reltoastrelid FROM pg_class WHERE oid = 'double_orphan_valid_parent'::regclass);

-- swap the first_orphan_toast table with a temp name
UPDATE pg_class SET relname = (SELECT relname || '_temp' FROM second_orphan_toast)
    WHERE oid = (SELECT oid FROM first_orphan_toast);

-- swap second_orphan_toast table with the original name of valid_parent toast table
UPDATE pg_class SET relname = (SELECT relname FROM first_orphan_toast)
     WHERE oid = (SELECT oid FROM second_orphan_toast);

-- swap the temp name with the first_orphan_toast table
UPDATE pg_class SET relname = (SELECT relname FROM second_orphan_toast)
     WHERE oid = (SELECT oid FROM first_orphan_toast);'''
    if broken == "double orphan - invalid parent":
        sql = '''
DROP TABLE IF EXISTS double_orphan_invalid_parent;
CREATE TABLE double_orphan_invalid_parent (a text);

DELETE FROM pg_depend
    WHERE objid = (SELECT reltoastrelid FROM pg_class WHERE relname = 'double_orphan_invalid_parent')
    AND refobjid = (SELECT oid FROM pg_class where relname = 'double_orphan_invalid_parent');

UPDATE pg_class SET reltoastrelid = 0 WHERE relname = 'double_orphan_invalid_parent';'''

    dbURLs = [dbconn.DbURL(dbname=dbname)]

    if contentIDs:
        dbURLs = []

        seg_config_sql = '''SELECT port,hostname FROM gp_segment_configuration WHERE role='p' AND content IN (%s);''' % contentIDs
        for port, hostname in getRows(dbname, seg_config_sql):
            dbURLs.append(dbconn.DbURL(dbname=dbname, hostname=hostname, port=port))

    for dbURL in dbURLs:
        utility = True if contentIDs else False
        with dbconn.connect(dbURL, allowSystemTableMods=True, utility=utility, unsetSearchPath=False) as conn:
            dbconn.execSQL(conn, sql)
            conn.commit()

@given('the database "{dbname}" is broken with "{broken}" orphaned toast tables')
def impl(context, dbname, broken):
    break_orphaned_toast_tables(context, dbname, broken)

@given('the database "{dbname}" has a table that is orphaned in multiple ways')
def impl(context, dbname):
    drop_database_if_exists(context, dbname)
    create_database(context, dbname)

    master = dbconn.DbURL(dbname=dbname)
    gparray = GpArray.initFromCatalog(master)

    primary0 = gparray.segmentPairs[0].primaryDB
    primary1 = gparray.segmentPairs[1].primaryDB

    seg0 = dbconn.DbURL(dbname=dbname, hostname=primary0.hostname, port=primary0.port)
    seg1 = dbconn.DbURL(dbname=dbname, hostname=primary1.hostname, port=primary1.port)

    with dbconn.connect(master, allowSystemTableMods=True, unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, """
            DROP TABLE IF EXISTS borked;
            CREATE TABLE borked (a text);
        """)
        conn.commit()

    with dbconn.connect(seg0, utility=True, allowSystemTableMods=True, unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, """
            DELETE FROM pg_depend WHERE refobjid = 'borked'::regclass;
        """)
        conn.commit()

    with dbconn.connect(seg1, utility=True, allowSystemTableMods=True, unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, """
            UPDATE pg_class SET reltoastrelid = 0 WHERE oid = 'borked'::regclass;
        """)
        conn.commit()

@then('verify status file and gp_segment_configuration backup file exist on standby')
def impl(context):
    status_file = 'gpexpand.status'
    gp_segment_configuration_backup = 'gpexpand.gp_segment_configuration'

    query = "select hostname, datadir from gp_segment_configuration where content = -1 order by dbid"
    conn = dbconn.connect(dbconn.DbURL(dbname='postgres'), unsetSearchPath=False)
    res = dbconn.execSQL(conn, query).fetchall()
    master = res[0]
    standby = res[1]

    master_datadir = master[1]
    standby_host = standby[0]
    standby_datadir = standby[1]

    standby_remote_statusfile = "%s:%s/%s" % (standby_host, standby_datadir, status_file)
    standby_local_statusfile = "%s/%s.standby" % (master_datadir, status_file)
    standby_remote_gp_segment_configuration_file = "%s:%s/%s" % \
            (standby_host, standby_datadir, gp_segment_configuration_backup)
    standby_local_gp_segment_configuration_file = "%s/%s.standby" % \
            (master_datadir, gp_segment_configuration_backup)

    cmd = Command(name="Copy standby file to master", cmdStr='scp %s %s' % \
            (standby_remote_statusfile, standby_local_statusfile))
    cmd.run(validateAfter=True)
    cmd = Command(name="Copy standby file to master", cmdStr='scp %s %s' % \
            (standby_remote_gp_segment_configuration_file, standby_local_gp_segment_configuration_file))
    cmd.run(validateAfter=True)

    if not os.path.exists(standby_local_statusfile):
        raise Exception('file "%s" is not exist' % standby_remote_statusfile)
    if not os.path.exists(standby_local_gp_segment_configuration_file):
        raise Exception('file "%s" is not exist' % standby_remote_gp_segment_configuration_file)


@when('the user runs {command} and selects {input}')
def impl(context, command, input):
    p = Popen(command.split(), stdout=PIPE, stdin=PIPE, stderr=PIPE)
    stdout, stderr = p.communicate(input=input)

    p.stdin.close()

    context.ret_code = p.returncode
    context.stdout_message = stdout
    context.error_message = stderr

def are_on_different_subnets(primary_hostname, mirror_hostname):
    primary_broadcast = check_output(['ssh', '-n', primary_hostname, "/sbin/ip addr show eth0 | grep 'inet .* brd' | awk '{ print $4 }'"])
    mirror_broadcast = check_output(['ssh', '-n', mirror_hostname,  "/sbin/ip addr show eth0 | grep 'inet .* brd' | awk '{ print $4 }'"])
    if not primary_broadcast:
        raise Exception("primary hostname %s has no broadcast address" % primary_hostname)
    if not mirror_broadcast:
        raise Exception("mirror hostname %s has no broadcast address" % mirror_hostname)

    return primary_broadcast != mirror_broadcast

@then('the primaries and mirrors {including} masterStandby are on different subnets')
def impl(context, including):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    if including == "including":
        if not gparray.standbyMaster:
            raise Exception("no standby found for master")
        if not are_on_different_subnets(gparray.master.hostname, gparray.standbyMaster.hostname):
            raise Exception("master %s and its standby %s are on same the subnet" % (gparray.master, gparray.standbyMaster))

    for segPair in gparray.segmentPairs:
        if not segPair.mirrorDB:
            raise Exception("no mirror found for segPair: %s" % segPair)
        if not are_on_different_subnets(segPair.primaryDB.hostname, segPair.mirrorDB.hostname):
            raise Exception("segmentPair on same subnet: %s" % segPair)
