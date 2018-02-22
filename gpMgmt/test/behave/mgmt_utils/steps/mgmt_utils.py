import fnmatch
import getpass
import glob
import gzip
import json
import yaml
try:
    import pexpect
except:
    print "The pexpect module could not be imported."

import os
import platform
import shutil
import socket
import tarfile
import tempfile
import thread
import json
import csv
import subprocess
import commands
import signal
from collections import defaultdict

from datetime import datetime
from time import sleep
from behave import given, when, then

from gppylib.commands.gp import SegmentStart, GpStandbyStart
from gppylib.commands.unix import findCmdInPath
from gppylib.operations.startSegments import MIRROR_MODE_MIRRORLESS
from gppylib.operations.unix import ListRemoteFilesByPattern, CheckRemoteFile
from test.behave_utils.gpfdist_utils.gpfdist_mgmt import Gpfdist
from test.behave_utils.utils import *
from test.behave_utils.PgHba import PgHba, Entry
from gppylib.commands.base import Command, REMOTE


master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
if master_data_dir is None:
    raise Exception('Please set MASTER_DATA_DIRECTORY in environment')


@given('the cluster config is generated with data_checksums "{checksum_toggle}"')
def impl(context, checksum_toggle):
    stop_database(context)

    cmd = """
    cd ../gpAux/gpdemo; \
        export MASTER_DEMO_PORT={master_port} && \
        export DEMO_PORT_BASE={port_base} && \
        export NUM_PRIMARY_MIRROR_PAIRS={num_primary_mirror_pairs} && \
        export WITH_MIRRORS={with_mirrors} && \
        ./demo_cluster.sh -d && ./demo_cluster.sh -c && \
        env EXTRA_CONFIG="HEAP_CHECKSUM={checksum_toggle}" ONLY_PREPARE_CLUSTER_ENV=true ./demo_cluster.sh
    """.format(master_port=os.getenv('MASTER_PORT', 15432),
               port_base=os.getenv('PORT_BASE', 25432),
               num_primary_mirror_pairs=os.getenv('NUM_PRIMARY_MIRROR_PAIRS', 3),
               with_mirrors='true',
               checksum_toggle=checksum_toggle)

    run_command(context, cmd)

    if context.ret_code != 0:
        raise Exception('%s' % context.error_message)


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

        cmd = """
        cd ../gpAux/gpdemo; \
            export MASTER_DEMO_PORT={master_port} && \
            export DEMO_PORT_BASE={port_base} && \
            export NUM_PRIMARY_MIRROR_PAIRS={num_primary_mirror_pairs} && \
            export WITH_MIRRORS={with_mirrors} && \
            ./demo_cluster.sh -d && ./demo_cluster.sh -c && \
            env EXTRA_CONFIG="HEAP_CHECKSUM={checksum_toggle}" ./demo_cluster.sh
        """.format(master_port=os.getenv('MASTER_PORT', 15432),
                   port_base=os.getenv('PORT_BASE', 25432),
                   num_primary_mirror_pairs=os.getenv('NUM_PRIMARY_MIRROR_PAIRS', 3),
                   with_mirrors='true',
                   checksum_toggle=checksum_toggle)

        run_command(context, cmd)

        if context.ret_code != 0:
            raise Exception('%s' % context.error_message)


@given('the database is not running')
@when('the database is not running')
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
    port = 0 if os.environ.get(PORT) == None else int(os.environ.get(PORT))
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


@given('we have determined the first segment hostname')
def impl(context):
    segment_hostlist = get_segment_hostlist()
    context.first_segment_hostname = segment_hostlist[0]


@given('{nic} on the first segment host is {nic_status}')
@then('{nic} on the first segment host is {nic_status}')
def impl(context, nic, nic_status):
    if nic_status.strip() == 'down':
        bring_nic_down(context.first_segment_hostname, nic)
    elif nic_status.strip() == 'up':
        bring_nic_up(context.first_segment_hostname, nic)
    else:
        raise Exception('Invalid nic status in feature file %s' % nic_status)


@given('the user truncates "{table_list}" tables in "{dbname}"')
@when('the user truncates "{table_list}" tables in "{dbname}"')
@then('the user truncates "{table_list}" tables in "{dbname}"')
def impl(context, table_list, dbname):
    if not table_list:
        raise Exception('Table list is empty')
    tables = table_list.split(',')
    for t in tables:
        truncate_table(dbname, t.strip())


@given('there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data')
@when('there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data')
@then('there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data')
def impl(context, tabletype, table_name, compression_type, dbname):
    populate_regular_table_data(context, tabletype, table_name, compression_type, dbname, with_data=True)


@given(
    'there is a "{tabletype}" table "{table_name}" with index "{indexname}" compression "{compression_type}" in "{dbname}" with data')
def impl(context, tabletype, table_name, compression_type, indexname, dbname):
    create_database_if_not_exists(context, dbname)
    drop_table_if_exists(context, table_name=table_name, dbname=dbname)
    if compression_type == "None":
        create_partition(context, table_name, tabletype, dbname, compression_type=None, partition=False)
    else:
        create_partition(context, table_name, tabletype, dbname, compression_type, partition=False)
    create_indexes(context, table_name, indexname, dbname)


@given(
    'there is a "{tabletype}" partition table "{table_name}" with compression "{compression_type}" in "{dbname}" with data')
@then(
    'there is a "{tabletype}" partition table "{table_name}" with compression "{compression_type}" in "{dbname}" with data')
def impl(context, tabletype, table_name, compression_type, dbname):
    create_database_if_not_exists(context, dbname)
    drop_table_if_exists(context, table_name=table_name, dbname=dbname)
    if compression_type == "None":
        create_partition(context, tablename=table_name, storage_type=tabletype, dbname=dbname, with_data=True)
    else:
        create_partition(context, tablename=table_name, storage_type=tabletype, dbname=dbname, with_data=True,
                         compression_type=compression_type)


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


@then('{command} should print "{out_msg}" to stdout')
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


@given('database "{dbname}" health check should pass on table "{tablename}"')
@when('database "{dbname}" health check should pass on table "{tablename}"')
@then('database "{dbname}" health check should pass on table "{tablename}"')
def impl(context, dbname, tablename):
    drop_database_if_exists(context, dbname)
    create_database(context, dbname)
    create_int_table(context, tablename, dbname=dbname)
    drop_database(context, dbname)


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


@when('at least one segment is resynchronized')
@then('at least one segment is resynchronized')
@given('at least one segment is resynchronized')
def impl(context):
    times = 30
    sleeptime = 10

    for i in range(times):
        if is_any_segment_resynchronized():
            return
        time.sleep(sleeptime)

    raise Exception('segments are not in resync after %d seconds' % (times * sleeptime))


@then('verify data integrity of database "{dbname}" between source and destination system, work-dir "{dirname}"')
def impl(context, dbname, dirname):
    dbconn_src = 'psql -p $GPTRANSFER_SOURCE_PORT -h $GPTRANSFER_SOURCE_HOST -U $GPTRANSFER_SOURCE_USER -d %s' % dbname
    dbconn_dest = 'psql -p $GPTRANSFER_DEST_PORT -h $GPTRANSFER_DEST_HOST -U $GPTRANSFER_DEST_USER -d %s' % dbname
    for filename in os.listdir(dirname):
        if filename.endswith('.sql'):
            filename_prefix = os.path.splitext(filename)[0]
            ans_file_path = os.path.join(dirname, filename_prefix + '.ans')
            out_file_path = os.path.join(dirname, filename_prefix + '.out')
            diff_file_path = os.path.join(dirname, filename_prefix + '.diff')
            # run the command to get the exact data from the source system
            command = '%s -f %s > %s' % (dbconn_src, os.path.join(dirname, filename), ans_file_path)
            run_command(context, command)

            # run the command to get the data from the destination system, locally
            command = '%s -f %s > %s' % (dbconn_dest, os.path.join(dirname, filename), out_file_path)
            run_command(context, command)

            gpdiff_cmd = 'gpdiff.pl -w -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE: --gpd_init=test/behave/mgmt_utils/steps/data/global_init_file %s %s > %s' % (
            ans_file_path, out_file_path, diff_file_path)
            run_command(context, gpdiff_cmd)
            if context.ret_code != 0:
                with open(diff_file_path, 'r') as diff_file:
                    diff_file_contents = diff_file.read()
                    raise Exception(
                        "Found difference between source and destination system, see %s. \n Diff contents: \n %s" % (
                        diff_file_path, diff_file_contents))


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


@then(
    'data for partition table "{table_name}" with partition level "{part_level}" is distributed across all segments on "{dbname}"')
def impl(context, table_name, part_level, dbname):
    validate_part_table_data_on_segments(context, table_name, part_level, dbname)


@then('verify that table "{tname}" in "{dbname}" has "{nrows}" rows')
def impl(context, tname, dbname, nrows):
    check_row_count(tname, dbname, int(nrows))


@then(
    'verify that table "{src_tname}" in database "{src_dbname}" of source system has same data with table "{dest_tname}" in database "{dest_dbname}" of destination system with options "{options}"')
def impl(context, src_tname, src_dbname, dest_tname, dest_dbname, options):
    match_table_select(context, src_tname, src_dbname, dest_tname, dest_dbname, options)


@then(
    'verify that table "{src_tname}" in database "{src_dbname}" of source system has same data with table "{dest_tname}" in database "{dest_dbname}" of destination system with order by "{orderby}"')
def impl(context, src_tname, src_dbname, dest_tname, dest_dbname, orderby):
    match_table_select(context, src_tname, src_dbname, dest_tname, dest_dbname, orderby)

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


@then('the temporary table file "{filename}" is removed')
def impl(context, filename):
    table_file = 'test/behave/mgmt_utils/steps/data/gptransfer/%s' % filename
    if os.path.exists(table_file):
        os.remove(table_file)


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
    with dbconn.connect(dbconn.DbURL(dbname='template1')) as conn:
        sql = """SELECT datname FROM pg_database"""
        dbs = dbconn.execSQL(conn, sql)
        if dbname in dbs:
            raise Exception('Database exists when it shouldnt "%s"' % dbname)


def get_host_and_port():
    if 'PGPORT' not in os.environ:
        raise Exception('PGPORT needs to be set in the environment')
    port = os.environ['PGPORT']
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    master_host = None
    for seg in gparray.getDbList():
        if seg.isSegmentMaster():
            master_host = seg.getSegmentAddress()

    if master_host is None:
        raise Exception('Unable to determine the master hostname')

    return (master_host, port)


@when('the performance timer is started')
def impl(context):
    context.performance_timer = time.time()


@then('the performance timer should be less then "{num_seconds}" seconds')
def impl(context, num_seconds):
    max_seconds = float(num_seconds)
    current_time = time.time()
    elapsed = current_time - context.performance_timer
    if elapsed > max_seconds:
        raise Exception(
            "Performance timer ran for %.1f seconds but had a max limit of %.1f seconds" % (elapsed, max_seconds))
    print "Elapsed time was %.1f seconds" % elapsed


@given('the file "{filename}" is removed from the system')
@when('the file "{filename}" is removed from the system')
@then('the file "{filename}" is removed from the system')
def impl(context, filename):
    os.remove(filename)


@given('the client program "{program_name}" is present under {parent_dir} in "{sub_dir}"')
def impl(context, program_name, parent_dir, sub_dir):
    if parent_dir == 'CWD':
        parent_dir = os.getcwd()
    program_path = '%s/%s/%s' % (parent_dir, sub_dir, program_name)

    print program_path
    if not os.path.isfile(program_path):
        raise Exception('gpfdist client progream does not exist: %s' % (program_path))


@when('the user runs client program "{program_name}" from "{subdir}" under {parent_dir}')
def impl(context, program_name, subdir, parent_dir):
    if parent_dir == 'CWD':
        parent_dir = os.getcwd()

    command_line = "python %s/%s/%s" % (parent_dir, subdir, program_name)
    run_command(context, command_line)


@given('the "{process_name}" process is killed')
@then('the "{process_name}" process is killed')
@when('the "{process_name}" process is killed')
def impl(context, process_name):
    run_command(context, 'pkill %s' % process_name)


@then('the client program should print "{msg}" to stdout with value in range {min_val} to {max_val}')
def impl(context, msg, min_val, max_val):
    stdout = context.stdout_message

    for line in stdout:
        if msg in line:
            val = re.finadall(r'\d+', line)
            if not val:
                raise Exception('Expected a numeric digit after message: %s' % msg)
            if len(val) > 1:
                raise Exception('Expected one numeric digit after message: %s' % msg)

            if val[0] < min_val or val[0] > max_val:
                raise Exception('Value not in acceptable range %s' % val[0])


@given('the directory "{dirname}" exists')
def impl(context, dirname):
    if not os.path.isdir(dirname):
        os.mkdir(dirname)
        if not os.path.isdir(dirname):
            raise Exception("directory '%s' not created" % dirname)


@then('the directory "{dirname}" does not exist')
def impl(context, dirname):
    if os.path.isdir(dirname):
        shutil.rmtree(dirname, ignore_errors=True)
    if os.path.isdir(dirname):
        raise Exception("directory '%s' not removed" % dirname)


@given('the directory "{dirname}" exists in current working directory')
def impl(context, dirname):
    dirname = os.path.join(os.getcwd(), dirname)
    if os.path.isdir(dirname):
        shutil.rmtree(dirname, ignore_errors=True)
        if os.path.isdir(dirname):
            raise Exception("directory '%s' not removed" % dirname)
    os.mkdir(dirname)
    if not os.path.isdir(dirname):
        raise Exception("directory '%s' not created" % dirname)


@given('the file "{filename}" exists under "{directory}" in current working directory')
def impl(context, filename, directory):
    directory = os.path.join(os.getcwd(), directory)
    if not os.path.isdir(directory):
        raise Exception("directory '%s' not exists" % directory)
    filepath = os.path.join(directory, filename)
    open(filepath, 'a').close()
    if not os.path.exists(filepath):
        raise Exception("file '%s' not created" % filepath)


@given('the directory "{dirname}" does not exist in current working directory')
def impl(context, dirname):
    dirname = os.path.join(os.getcwd(), dirname)
    if os.path.isdir(dirname):
        shutil.rmtree(dirname, ignore_errors=True)
    if os.path.isdir(dirname):
        raise Exception("directory '%s' not removed" % dirname)


@when('the data line "{dataline}" is appened to "{fname}" in cwd')
@then('the data line "{dataline}" is appened to "{fname}" in cwd')
def impl(context, dataline, fname):
    fname = os.path.join(os.getcwd(), fname)
    with open(fname, 'a') as fd:
        fd.write("%s\n" % dataline)


@when('a "{readwrite}" external table "{tname}" is created on file "{fname}" in "{dbname}"')
def impl(context, readwrite, tname, fname, dbname):
    hostname = socket.gethostname()

    sql = """CREATE %s EXTERNAL TABLE
            %s (name varchar(255), id int)
            LOCATION ('gpfdist://%s:8088/%s')
            FORMAT 'text'
            (DELIMITER '|');
            """ % (readwrite, tname, hostname, fname)

    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, sql)
        conn.commit()


@given('the external table "{tname}" does not exist in "{dbname}"')
def impl(context, tname, dbname):
    drop_external_table_if_exists(context, table_name=tname, dbname=dbname)


@given('results of the sql "{sql}" db "{dbname}" are stored in the context')
@when( 'results of the sql "{sql}" db "{dbname}" are stored in the context')
def impl(context, sql, dbname):
    context.stored_sql_results = []

    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
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


@given('user does not have ssh permissions')
def impl(context):
    user_home = os.environ.get('HOME')
    authorized_keys_file = '%s/.ssh/authorized_keys' % user_home
    if os.path.exists(os.path.abspath(authorized_keys_file)):
        shutil.move(authorized_keys_file, '%s.bk' % authorized_keys_file)


@then('user has ssh permissions')
def impl(context):
    user_home = os.environ.get('HOME')
    authorized_keys_backup_file = '%s/.ssh/authorized_keys.bk' % user_home
    if os.path.exists(authorized_keys_backup_file):
        shutil.move(authorized_keys_backup_file, authorized_keys_backup_file[:-3])


def delete_data_dir(host):
    cmd = Command(name='remove data directories',
                  cmdStr='rm -rf %s' % master_data_dir,
                  ctxt=REMOTE,
                  remoteHost=host)
    cmd.run(validateAfter=True)


@when('the user initializes a standby on the same host as master')
def impl(context):
    hostname = get_master_hostname('template1')[0][0]
    port_guaranteed_open = get_open_port()
    temp_data_dir = tempfile.mkdtemp() + "/standby_datadir"
    cmd = "gpinitstandby -a -s %s -P %d -F %s" % (hostname, port_guaranteed_open, temp_data_dir)
    run_gpcommand(context, cmd)
    context.standby_data_dir = temp_data_dir
    context.standby_was_initialized = True


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
    run_gpcommand(context, cmd)


@when('the user initializes standby master on "{hostname}"')
def impl(context, hostname):
    create_standby(context, hostname)


def create_standby(context, hostname):
    port = os.environ.get('PGPORT')
    if hostname.strip() == 'mdw':
        if not hasattr(context, 'master') or not hasattr(context, 'standby_host'):
            raise Exception('Expected master to be saved but was not found')
        delete_data_dir(context.master)
        cmd = Command(name='gpinitstandby',
                      cmdStr='PGPORT=%s MASTER_DATA_DIRECTORY=%s gpinitstandby -a -s %s' % (
                      port, master_data_dir, context.master),
                      ctxt=REMOTE,
                      remoteHost=context.standby_host)
        cmd.run(validateAfter=True)
        return
    elif hostname.strip() == 'smdw':
        context.master = get_master_hostname('template1')[0][0]
        context.ret_code = 0
        context.stdout_message = ''
        context.error_message = ''
        segment_hostlist = get_segment_hostlist()
        segment_hosts = [seg for seg in segment_hostlist if seg != context.master]
        context.standby_host = segment_hosts[0]
        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        segdbs = gparray.getDbList()
        for seg in segdbs:
            if seg.getSegmentHostName() == context.standby_host:
                context.standby_host_data_dir = seg.getSegmentDataDirectory()

        context.standby_was_initialized = False
        standby = get_standby_host()

        if standby:
            context.standby_was_initialized = True
            context.standby_host = standby
            return

        delete_data_dir(context.standby_host)
        cmd = "gpinitstandby -a -s %s" % context.standby_host
        run_gpcommand(context, cmd)
    else:
        raise Exception('Invalid host type specified "%s"' % hostname)


@when('the user runs the query "{query}" on "{dbname}"')
def impl(context, query, dbname):
    if query.lower().startswith('create') or query.lower().startswith('insert'):
        thread.start_new_thread(execute_sql, (dbname, query))
    else:
        thread.start_new_thread(getRows, (dbname, query))
    time.sleep(30)


@given('we have exchanged keys with the cluster')
def impl(context):
    hostlist = get_all_hostnames_as_list(context, 'template1')
    host_str = ' -h '.join(hostlist)
    cmd_str = 'gpssh-exkeys %s' % host_str
    run_gpcommand(context, cmd_str)


@then('the user runs the command "{cmd}" from standby')
@when('the user runs the command "{cmd}" from standby')
def impl(context, cmd):
    port = os.environ.get('PGPORT')
    cmd = 'PGPORT=%s MASTER_DATA_DIRECTORY=%s %s' % (port, master_data_dir, cmd)
    cmd = Command('run remote command', cmd, ctxt=REMOTE, remoteHost=context.standby_host)
    cmd.run(validateAfter=True)


@given('user kills a primary postmaster process')
@when('user kills a primary postmaster process')
@then('user kills a primary postmaster process')
def impl(context):
    if hasattr(context, 'pseg'):
        seg_data_dir = context.pseg_data_dir
        seg_host = context.pseg_hostname
        seg_port = context.pseg.getSegmentPort()
    else:
        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        for seg in gparray.getDbList():
            if seg.isSegmentPrimary():
                seg_data_dir = seg.getSegmentDataDirectory()
                seg_host = seg.getSegmentHostName()
                seg_port = seg.getSegmentPort()
                break

    pid = get_pid_for_segment(seg_data_dir, seg_host)
    if pid is None:
        raise Exception('Unable to locate segment "%s" on host "%s"' % (seg_data_dir, seg_host))

    kill_process(int(pid), seg_host, signal.SIGKILL)

    has_process_eventually_stopped(pid, seg_host)

    pid = get_pid_for_segment(seg_data_dir, seg_host)
    if pid is not None:
        raise Exception('Unable to kill postmaster with pid "%d" datadir "%s"' % (pid, seg_data_dir))

    context.killed_seg_host = seg_host
    context.killed_seg_port = seg_port


@given('user can start transactions')
@when('user can start transactions')
@then('user can start transactions')
def impl(context):
    num_retries = 150
    attempt = 0
    while attempt < num_retries:
        try:
            with dbconn.connect(dbconn.DbURL()) as conn:
                break
        except Exception as e:
            attempt += 1
            pass
        time.sleep(1)

    if attempt == num_retries:
        raise Exception('Unable to establish a connection to database !!!')


@given('the core dump directory is stored')
def impl(context):
    with open('/etc/sysctl.conf', 'r') as fp:
        for line in fp:
            if 'kernel.core_pattern' in line:
                context.core_dir = os.path.dirname(line.strip().split('=')[1])

    if not hasattr(context, 'core_dir') or not context.core_dir:
        context.core_dir = os.getcwd()


@given('the number of core files "{stage}" running "{utility}" is stored')
@then('the number of core files "{stage}" running "{utility}" is stored')
def impl(context, stage, utility):
    core_files_count = 0
    files_list = os.listdir(context.core_dir)
    for f in files_list:
        if f.startswith('core'):
            core_files_count += 1

    if stage.strip() == 'before':
        context.before_core_count = core_files_count
    elif stage.strip() == 'after':
        context.after_core_count = core_files_count
    else:
        raise Exception('Invalid stage entered: %s' % stage)


@then('the number of core files is the same')
def impl(context):
    if not hasattr(context, 'before_core_count'):
        raise Exception('Core file count not stored before operation')
    if not hasattr(context, 'after_core_count'):
        raise Exception('Core file count not stored after operation')

    if context.before_core_count != context.after_core_count:
        raise Exception('Core files count before %s does not match after %s' % (
        context.before_core_count, context.after_core_count))


@given('the gpAdminLogs directory has been backed up')
def impl(context):
    src = os.path.join(os.path.expanduser('~'), 'gpAdminLogs')
    dest = os.path.join(os.path.expanduser('~'), 'gpAdminLogs.bk')
    shutil.move(src, dest)


@given('the user does not have "{access}" permission on the home directory')
def impl(context, access):
    home_dir = os.path.expanduser('~')
    context.orig_write_permission = check_user_permissions(home_dir, 'write')
    if access == 'write':
        cmd = "sudo chmod u-w %s" % home_dir
    run_command(context, cmd)

    if check_user_permissions(home_dir, access):
        raise Exception('Unable to change "%s" permissions for the directory "%s"' % (access, home_dir))


@then('the "{filetype}" path "{file_path}" should "{cond}" exist')
def impl(context, filetype, file_path, cond):
    cond = cond.strip()
    if file_path[0] == '~':
        file_path = os.path.join(os.path.expanduser('~'), file_path[2:])

    if filetype == 'file':
        existence_check_fn = os.path.isfile
    elif filetype == 'directory':
        existence_check_fn = os.path.isdir
    else:
        raise Exception('File type should be either file or directory')

    if cond == '' and not existence_check_fn(file_path):
        raise Exception('The %s "%s" does not exist' % (filetype, file_path))
    elif cond == 'not' and existence_check_fn(file_path):
        raise Exception('The %s "%s" exist' % (filetype, file_path))


@given('the environment variable "{var}" is not set')
def impl(context, var):
    context.env_var = os.environ.get(var)
    os.environ[var] = ''


@then('the environment variable "{var}" is reset')
def impl(context, var):
    if hasattr(context, 'env_var'):
        os.environ[var] = context.env_var
    else:
        raise Exception('Environment variable %s cannot be reset because its value was not saved.' % var)


@given('the environment variable "{var}" is set to "{val}"')
def impl(context, var, val):
    context.env_var = os.environ.get(var)
    os.environ[var] = val


@when('the user runs query from the file "{filename}" on "{dbname}" without fetching results')
def impl(context, filename, dbname):
    with open(filename) as fr:
        for line in fr:
            query = line.strip()
    thread.start_new_thread(execute_sql, (dbname, query))
    time.sleep(30)


@then('the following text should be printed to stdout')
def impl(context):
    check_stdout_msg(context, context.text.strip())


@then('the text in the file "{filename}" should be printed to stdout')
def impl(context, filename):
    contents = ''
    with open(filename) as fr:
        for line in fr:
            contents = line.strip()
    print "contents: ", contents
    check_stdout_msg(context, contents)

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

    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        curs = dbconn.execSQL(conn, context.text)
        context.stored_rows = curs.fetchall()


@when('execute sql "{sql}" in db "{dbname}" and store result in the context')
def impl(context, sql, dbname):
    context.stored_rows = []

    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
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
    cmd = "if [ `uname -s` = 'SunOS' ]; then CMD=pfiles; else CMD='lsof -p'; fi && PATH=$PATH:/usr/bin:/usr/sbin $CMD `cat %s` | grep %s | wc -l" % (
    pidfile, filename)
    return commands.getstatusoutput(cmd)


@then('the file "{filename}" by process "{pidfile}" is not closed')
def impl(context, filename, pidfile):
    (ret, output) = get_opened_files(filename, pidfile)
    if int(output) == 0:
        raise Exception('file %s has been closed' % (filename))


@then('the file "{filename}" by process "{pidfile}" is closed')
def impl(context, filename, pidfile):
    (ret, output) = get_opened_files(filename, pidfile)
    if int(output) != 0:
        raise Exception('file %s has not been closed' % (filename))


@then('the file "{filename}" by process "{pidfile}" opened number is "{num}"')
def impl(context, filename, pidfile, num):
    (ret, output) = get_opened_files(filename, pidfile)
    if int(output) != int(num):
        raise Exception('file %s opened number %d is not %d' % (filename, int(output), int(num)))


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


@when('the user stops the syncmaster')
@then('the user stops the syncmaster')
def impl(context):
    host = context.gparray.standbyMaster.hostname
    # Cat is added because pgrep returns all the processes of the tree, while
    # child processes are kill when the parent is kill, which produces an error
    cmdStr = 'pgrep syncmaster | xargs -i kill {} | cat'
    run_cmd = Command('kill syncmaster', cmdStr, ctxt=REMOTE, remoteHost=host)
    run_cmd.run(validateAfter=True)
    datadir = context.gparray.standbyMaster.datadir


@when('the user starts the syncmaster')
@then('the user starts the syncmaster')
def impl(context):
    host = context.gparray.standbyMaster.hostname
    datadir = context.gparray.standbyMaster.datadir
    port = context.gparray.standbyMaster.port
    dbid = context.gparray.standbyMaster.dbid
    ncontents = context.gparray.getNumSegmentContents()
    GpStandbyStart.remote('test start syncmaster', host, datadir, port, ncontents, dbid)


@when('save the cluster configuration')
@then('save the cluster configuration')
def impl(context):
    context.gparray = GpArray.initFromCatalog(dbconn.DbURL())


@given('this test sleeps for "{secs}" seconds')
@when('this test sleeps for "{secs}" seconds')
@then('this test sleeps for "{secs}" seconds')
def impl(context, secs):
    secs = float(secs)
    time.sleep(secs)


def execute_sql_for_sec(dbname, query, sec):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, query)
        conn.commit()
        time.sleep(sec)


@given('the standby is not initialized')
@then('the standby is not initialized')
def impl(context):
    standby = get_standby_host()
    if standby:
        context.cluster_had_standby = True
        context.standby_host = standby
        run_gpcommand(context, 'gpinitstandby -ra')


@given('all the compression data from "{dbname}" is saved for verification')
def impl(context, dbname):
    partitions = get_partition_list('ao', dbname) + get_partition_list('co', dbname)
    with open('test/data/compression_{db}_backup'.format(db=dbname), 'w') as fp:
        with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
            for p in partitions:
                query = """SELECT get_ao_compression_ratio('{schema}.{table}')""".format(schema=p[1], table=p[2])
                compression_rate = dbconn.execSQLForSingleton(conn, query)
                fp.write('{schema}.{table}:{comp}\n'.format(schema=p[1], table=p[2], comp=compression_rate))


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


@given('a table is created containing rows of length "{length}" with connection "{dbconn}"')
def impl(context, length, dbconn):
    length = int(length)
    wide_row_file = 'test/behave/mgmt_utils/steps/data/gptransfer/wide_row_%s.sql' % length
    tablename = 'public.wide_row_%s' % length
    entry = "x" * length
    with open(wide_row_file, 'w') as sql_file:
        sql_file.write("CREATE TABLE %s (a integer, b text);\n" % tablename)
        for i in range(10):
            sql_file.write("INSERT INTO %s VALUES (%d, \'%s\');\n" % (tablename, i, entry))
    command = '%s -f %s' % (dbconn, wide_row_file)
    run_gpcommand(context, command)


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


@given(
    'there is a table "{table_name}" dependent on function "{func_name}" in database "{dbname}" on the source system')
def impl(context, table_name, func_name, dbname):
    dbconn = 'psql -d %s -p $GPTRANSFER_SOURCE_PORT -U $GPTRANSFER_SOURCE_USER -h $GPTRANSFER_SOURCE_HOST' % dbname
    SQL = """CREATE TABLE %s (num integer); CREATE FUNCTION %s (integer) RETURNS integer AS 'select abs(\$1);' LANGUAGE SQL IMMUTABLE; CREATE INDEX test_index ON %s (%s(num))""" % (
    table_name, func_name, table_name, func_name)
    command = '%s -c "%s"' % (dbconn, SQL)
    run_command(context, command)


@then('verify that function "{func_name}" exists in database "{dbname}"')
def impl(context, func_name, dbname):
    SQL = """SELECT proname FROM pg_proc WHERE proname = '%s';""" % func_name
    row_count = getRows(dbname, SQL)[0][0]
    if row_count != 'test_function':
        raise Exception('Function %s does not exist in %s"' % (func_name, dbname))


@given('the user runs the command "{cmd}" in the background')
@when('the user runs the command "{cmd}" in the background')
def impl(context, cmd):
    thread.start_new_thread(run_command, (context, cmd))
    time.sleep(10)


@given('the user runs the command "{cmd}" in the background without sleep')
@when('the user runs the command "{cmd}" in the background without sleep')
def impl(context, cmd):
    thread.start_new_thread(run_command, (context, cmd))


@then('verify that the file "{filename}" contains the string "{output}"')
def impl(context, filename, output):
    contents = ''
    with open(filename) as fr:
        for line in fr:
            contents = line.strip()
    print contents
    check_stdout_msg(context, output)


@then('verify that the last line of the file "{filename}" in the master data directory contains the string "{output}"')
def impl(context, filename, output):
    contents = ''
    file_path = os.path.join(master_data_dir, filename)
    with open(file_path) as fr:
        for line in fr:
            contents = line.strip()
    pat = re.compile(output)
    if not pat.search(contents):
        err_str = "Expected stdout string '%s' and found: '%s'" % (output, contents)
        raise Exception(err_str)


@then('the user waits for "{process_name}" to finish running')
def impl(context, process_name):
    run_command(context, "ps ux | grep `which %s` | grep -v grep | awk '{print $2}' | xargs" % process_name)
    pids = context.stdout_message.split()
    while len(pids) > 0:
        for pid in pids:
            try:
                os.kill(int(pid), 0)
            except OSError:
                pids.remove(pid)
        time.sleep(10)


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


@then('verify that gptransfer is in order of "{filepath}" when partition transfer is "{is_partition_transfer}"')
def impl(context, filepath, is_partition_transfer):
    with open(filepath) as f:
        table = f.read().splitlines()
        if is_partition_transfer != "None":
            table = [x.split(',')[0] for x in table]

    split_message = re.findall("Starting transfer of.*\n", context.stdout_message)

    if len(split_message) == 0 and len(table) != 0:
        raise Exception("There were no tables transfered")

    counter_table = 0
    counter_split = 0
    found = 0

    while counter_table < len(table) and counter_split < len(split_message):
        for i in range(counter_split, len(split_message)):
            pat = table[counter_table] + " to"
            prog = re.compile(pat)
            res = prog.search(split_message[i])
            if not res:
                counter_table += 1
                break
            else:
                found += 1
                counter_split += 1

    if found != len(split_message):
        raise Exception("expected to find %s tables in order and only found %s in order" % (len(split_message), found))


@given('database "{dbname}" is dropped and recreated')
@when('database "{dbname}" is dropped and recreated')
@then('database "{dbname}" is dropped and recreated')
def impl(context, dbname):
    drop_database_if_exists(context, dbname)
    create_database(context, dbname)


def execute_sql_until_stopped(context, dbname, query):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, query)
        conn.commit()
        while True:
            if hasattr(context, 'background_query_lock'):
                break
            time.sleep(1)


@then('validate and run gpcheckcat repair')
def impl(context):
    context.execute_steps(u'''
        Then gpcheckcat should print "repair script\(s\) generated in dir gpcheckcat.repair.*" to stdout
        Then the path "gpcheckcat.repair.*" is found in cwd "1" times
        Then run all the repair scripts in the dir "gpcheckcat.repair.*"
        And the path "gpcheckcat.repair.*" is removed from current working directory
    ''')


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
ssh %s "source %s; export PGUSER=%s; export PGPORT=%s; export PGOPTIONS=\\\"-c gp_session_role=utility\\\"; psql -d %s -c \\\"SET allow_system_table_mods=\'dml\'; DELETE FROM pg_attribute where attrelid=\'%s\'::regclass::oid;\\\""
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


@then('run all the repair scripts in the dir "{dir}"')
def impl(context, dir):
    command = "cd {0} ; for i in *.sh ; do bash $i; done".format(dir)
    run_command(context, command)


@when(
    'the entry for the table "{user_table}" is removed from "{catalog_table}" with key "{primary_key}" in the database "{db_name}"')
def impl(context, user_table, catalog_table, primary_key, db_name):
    delete_qry = "delete from %s where %s='%s'::regclass::oid;" % (catalog_table, primary_key, user_table)
    with dbconn.connect(dbconn.DbURL(dbname=db_name)) as conn:
        for qry in ["set allow_system_table_mods='dml';", "set allow_segment_dml=true;", delete_qry]:
            dbconn.execSQL(conn, qry)
            conn.commit()


@when('the entry for the table "{user_table}" is removed from "{catalog_table}" with key "{primary_key}" in the database "{db_name}" on the first primary segment')
@given('the entry for the table "{user_table}" is removed from "{catalog_table}" with key "{primary_key}" in the database "{db_name}" on the first primary segment')
def impl(context, user_table, catalog_table, primary_key, db_name):
    host, port = get_primary_segment_host_port()
    delete_qry = "delete from %s where %s='%s'::regclass::oid;" % (catalog_table, primary_key, user_table)

    with dbconn.connect(dbconn.DbURL(dbname=db_name, port=port, hostname=host), utility=True,
                        allowSystemTableMods='dml') as conn:
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

@when('user temporarily moves the data directory of the killed mirror')
@then('user temporarily moves the data directory of the killed mirror')
def impl(context):
    rmStr = "mv %s{,.bk}" % context.mirror_datadir
    cmd = Command(name='Move mirror data directory', cmdStr=rmStr)
    cmd.run(validateAfter=True)


@when('user returns the data directory to the default location of the killed mirror')
@then('user returns the data directory to the default location of the killed mirror')
def impl(context):
    rmStr = "mv %s{.bk,}" % context.mirror_datadir
    cmd = Command(name='Move mirror data directory', cmdStr=rmStr)
    cmd.run(validateAfter=True)


@when('wait until the mirror is down')
@then('wait until the mirror is down')
@given('wait until the mirror is down')
def impl(context):
    qry = "select status from gp_segment_configuration where dbid='%s' and status='d' " % context.mirror_segdbId
    start_time = current_time = datetime.now()
    while (current_time - start_time).seconds < 120:
        row_count = len(getRows('template1', qry))
        if row_count == 1:
            break
        time.sleep(5)
        current_time = datetime.now()


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


@when('wait until the results from boolean sql "{sql}" is "{boolean}"')
@then('wait until the results from boolean sql "{sql}" is "{boolean}"')
@given('wait until the results from boolean sql "{sql}" is "{boolean}"')
def impl(context, sql, boolean):
    cmd = Command(name='psql', cmdStr='psql --tuples-only -d gpperfmon -c "%s"' % sql)
    start_time = current_time = datetime.now()
    result = None
    while (current_time - start_time).seconds < 120:
        cmd.run()
        if cmd.get_return_code() != 0:
            break
        result = cmd.get_stdout()
        if _str2bool(result) == _str2bool(boolean):
            break
        time.sleep(2)
        current_time = datetime.now()

    if cmd.get_return_code() != 0:
        context.ret_code = cmd.get_return_code()
        context.error_message = 'psql internal error: %s' % cmd.get_stderr()
        check_return_code(context, 0)
    else:
        if _str2bool(result) != _str2bool(boolean):
            raise Exception("sql output '%s' is not same as '%s'" % (result, boolean))


def _str2bool(string):
    return string.lower().strip() in ['t', 'true', '1', 'yes', 'y']


@given('the information of a "{seg}" segment on any host is saved')
@when('the information of a "{seg}" segment on any host is saved')
@then('the information of a "{seg}" segment on any host is saved')
def impl(context, seg):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    if seg == "mirror":
        to_save_segs = [seg for seg in gparray.getDbList() if seg.isSegmentMirror()]
        context.mirror_segdbId = to_save_segs[0].getSegmentDbId()
        context.mirror_segcid = to_save_segs[0].getSegmentContentId()
        context.mirror_segdbname = to_save_segs[0].getSegmentHostName()
        context.mirror_datadir = to_save_segs[0].getSegmentDataDirectory()
        context.mirror_port = to_save_segs[0].getSegmentPort()
    elif seg == "primary":
        to_save_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
    elif seg == "master":
        to_save_segs = [seg for seg in gparray.getDbList() if seg.isSegmentMaster()]

    context.saved_segcid = to_save_segs[0].getSegmentContentId()


@given('the user creates an index for table "{table_name}" in database "{db_name}"')
@when('the user creates an index for table "{table_name}" in database "{db_name}"')
@then('the user creates an index for table "{table_name}" in database "{db_name}"')
def impl(context, table_name, db_name):
    index_qry = "create table {0}(i int primary key, j varchar); create index test_index on index_table using bitmap(j)".format(
        table_name)

    with dbconn.connect(dbconn.DbURL(dbname=db_name)) as conn:
        dbconn.execSQL(conn, index_qry)
        conn.commit()


@given('verify that mirror_existence_state of segment "{segc_id}" is "{mirror_existence_state}"')
@when('verify that mirror_existence_state of segment "{segc_id}" is "{mirror_existence_state}"')
@then('verify that mirror_existence_state of segment "{segc_id}" is "{mirror_existence_state}"')
def impl(context, segc_id, mirror_existence_state):
    with dbconn.connect(dbconn.DbURL(dbname='template1')) as conn:
        sql = """SELECT mirror_existence_state from gp_dist_random('gp_persistent_relation_node') where gp_segment_id=%s group by 1;""" % segc_id
        cluster_state = dbconn.execSQL(conn, sql).fetchone()
        if cluster_state[0] != int(mirror_existence_state):
            raise Exception("mirror_existence_state of segment %s is %s. Expected %s." % (
            segc_id, cluster_state[0], mirror_existence_state))


@given('the gptransfer test is initialized')
def impl(context):
    context.execute_steps(u'''
        Given the database is running
        And the database "gptest" does not exist
        And the database "gptransfer_destdb" does not exist
        And the database "gptransfer_testdb1" does not exist
        And the database "gptransfer_testdb3" does not exist
        And the database "gptransfer_testdb4" does not exist
        And the database "gptransfer_testdb5" does not exist
    ''')


@given('gpperfmon is configured and running in qamode')
@then('gpperfmon is configured and running in qamode')
def impl(context):
    target_line = 'qamode = 1'
    gpperfmon_config_file = "%s/gpperfmon/conf/gpperfmon.conf" % os.getenv("MASTER_DATA_DIRECTORY")
    if not check_db_exists("gpperfmon", "localhost"):
        context.execute_steps(u'''
                              When the user runs "gpperfmon_install --port 15432 --enable --password foo"
                              Then gpperfmon_install should return a return code of 0
                              ''')

    if not file_contains_line(gpperfmon_config_file, target_line):
        context.execute_steps(u'''
                              When the user runs command "echo 'qamode = 1' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
                              Then echo should return a return code of 0
                              When the user runs command "echo 'verbose = 1' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
                              Then echo should return a return code of 0
                              When the user runs command "echo 'min_query_time = 0' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
                              Then echo should return a return code of 0
                              When the user runs command "echo 'quantum = 10' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
                              Then echo should return a return code of 0
                              When the user runs command "echo 'harvest_interval = 5' >> $MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf"
                              Then echo should return a return code of 0
                              ''')

    if not is_process_running("gpsmon"):
        context.execute_steps(u'''
                              When the database is not running
                              Then wait until the process "postgres" goes down
                              When the user runs "gpstart -a"
                              Then gpstart should return a return code of 0
                              And verify that a role "gpmon" exists in database "gpperfmon"
                              And verify that the last line of the file "postgresql.conf" in the master data directory contains the string "gpperfmon_log_alert_level=warning"
                              And verify that there is a "heap" table "database_history" in "gpperfmon"
                              Then wait until the process "gpmmon" is up
                              And wait until the process "gpsmon" is up
                              ''')


@given('the setting "{variable_name}" is NOT set in the configuration file "{path_to_file}"')
@when('the setting "{variable_name}" is NOT set in the configuration file "{path_to_file}"')
def impl(context, variable_name, path_to_file):
    path = os.path.join(os.getenv("MASTER_DATA_DIRECTORY"), path_to_file)
    temp_file = "/tmp/gpperfmon_temp_config"
    with open(path) as oldfile, open(temp_file, 'w') as newfile:
        for line in oldfile:
            if variable_name not in line:
                newfile.write(line)
    shutil.move(temp_file, path)


@given('the setting "{setting_string}" is placed in the configuration file "{path_to_file}"')
@when('the setting "{setting_string}" is placed in the configuration file "{path_to_file}"')
def impl(context, setting_string, path_to_file):
    path = os.path.join(os.getenv("MASTER_DATA_DIRECTORY"), path_to_file)
    with open(path, 'a') as f:
        f.write(setting_string)
        f.write("\n")


@given('the latest gpperfmon gpdb-alert log is copied to a file with a fake (earlier) timestamp')
@when('the latest gpperfmon gpdb-alert log is copied to a file with a fake (earlier) timestamp')
def impl(context):
    gpdb_alert_file_path_src = sorted(glob.glob(os.path.join(os.getenv("MASTER_DATA_DIRECTORY"),
                                    "gpperfmon",
                                       "logs",
                                       "gpdb-alert*")))[-1]
    # typical filename would be gpdb-alert-2017-04-26_155335.csv
    # setting the timestamp to a string that starts with `-` (em-dash)
    #   will be sorted (based on ascii) before numeric timestamps
    #   without colliding with a real timestamp
    dest = re.sub(r"_\d{6}\.csv$", "_-takeme.csv", gpdb_alert_file_path_src)

    # Let's wait until there's actually something in the file before actually
    # doing a copy of the log...
    for _ in range(60):
        if os.stat(gpdb_alert_file_path_src).st_size != 0:
            shutil.copy(gpdb_alert_file_path_src, dest)
            context.fake_timestamp_file = dest
            return
        sleep(1)

    raise Exception("File: %s is empty" % gpdb_alert_file_path_src)



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
    rpm_command_list_all = 'rpm -qa --dbpath %s/share/packages/database' % remote_gphome

    for hostname in set(hostlist):
        cmd = Command(name='check if internal rpm gppkg is installed',
                      cmdStr=rpm_command_list_all,
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
    rpm_command_list_all = 'rpm -qa --dbpath %s/share/packages/database' % remote_gphome

    for hostname in set(hostlist):
        cmd = Command(name='check if internal rpm gppkg is installed',
                      cmdStr=rpm_command_list_all,
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

    rpm_command_list_all = 'rpm -qa --dbpath %s/share/packages/database' % remote_gphome
    cmd = Command(name='get all rpm from the host',
                  cmdStr=rpm_command_list_all,
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

    rpm_remove_command = 'rpm -e %s --dbpath %s/share/packages/database' % (full_gppkg_name, remote_gphome)
    cmd = Command(name='Cleanly remove from the remove host',
                  cmdStr=rpm_remove_command,
                  ctxt=REMOTE,
                  remoteHost=hostname)
    cmd.run(validateAfter=True)

    remove_archive_gppgk = 'rm -f %s/share/packages/archive/%s.gppkg' % (remote_gphome, gppkg_name)
    cmd = Command(name='Remove archive gppkg',
                  cmdStr=remove_archive_gppgk,
                  ctxt=REMOTE,
                  remoteHost=hostname)
    cmd.run(validateAfter=True)


@when('gppkg "{gppkg_name}" is removed from a segment host')
def impl(context, gppkg_name):
    _remove_gppkg_from_host(context, gppkg_name, is_master_host=False)


@when('gppkg "{gppkg_name}" is removed from master host')
def impl(context, gppkg_name):
    _remove_gppkg_from_host(context, gppkg_name, is_master_host=True)


@given('gpAdminLogs directory has no "{prefix}" files')
def impl(context, prefix):
    log_dir = _get_gpAdminLogs_directory()
    items = glob.glob('%s/%s_*.log' % (log_dir, prefix))
    for item in items:
        os.remove(item)


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
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname))
    try:
        result = getRows(dbname, query)[0][0]
        if result != role_name:
            raise Exception("Role %s does not exist in database %s." % (role_name, dbname))
    except:
        raise Exception("Role %s does not exist in database %s." % (role_name, dbname))
