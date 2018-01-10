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


def _write_timestamp_to_json(context):
    scenario_name = context._stack[0]['scenario'].name
    timestamp = get_timestamp_from_output(context)
    if not global_timestamps.has_key(scenario_name):
        global_timestamps[scenario_name] = list()
    global_timestamps[scenario_name].append(timestamp)
    with open(timestamp_json, 'w') as outfile:
        json.dump(global_timestamps, outfile)


def _write_label_to_json(context, label_key, label):
    timestamp = get_timestamp_from_output(context)
    if not global_labels.has_key(label_key):
        global_labels[label_key] = {}
    global_labels[label_key][label] = timestamp
    with open(labels_json, 'w') as outfile:
        json.dump(global_labels, outfile)


def _read_timestamp_from_json(context):
    scenario_name = context._stack[0]['scenario'].name
    with open(timestamp_json, 'r') as infile:
        return json.load(infile)[scenario_name]


def _read_label_from_json(context, label_key):
    with open(labels_json, 'r') as infile:
        return json.load(infile)[label_key]


@given('the old database is started')
def impl(context):
    command = 'gpstop -a -M fast'
    run_gpcommand(context, command)

    new_greenplum_path = "%s/greenplum_path.sh" % os.environ['GPHOME']
    os.environ['NEW_GREENPLUM_PATH'] = new_greenplum_path
    old_greenplum_path = "/data/greenplum-db-old/greenplum_path.sh"
    command = ['bash', '-c', 'source %s && env' % old_greenplum_path]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE)
    for line in proc.stdout:
        (key, _, value) = line.partition("=")
        os.environ[key] = value.strip()
    proc.communicate()

    command = 'gpstart -a'
    run_gpcommand(context, command)

    # Wait for database to come up, to prevent race conditions in later tests
    cmd = Command(name="check if database is up", cmdStr="psql -l")
    for i in range(30):
        sleep(10)
        cmd.run()
        if cmd.get_return_code() == 0:
            return
    raise Exception("Database did not start up within 5 minutes`")


@given('the new database is started')
def impl(context):
    command = 'gpstop -a -M fast'
    run_gpcommand(context, command)

    new_greenplum_path = os.environ['NEW_GREENPLUM_PATH']
    command = ['bash', '-c', 'source %s && env' % new_greenplum_path]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE)
    for line in proc.stdout:
        (key, _, value) = line.partition("=")
        os.environ[key] = value.strip()
    proc.communicate()

    command = 'gpstart -a'
    run_gpcommand(context, command)

    # Wait for database to come up, to prevent race conditions in later tests
    cmd = Command(name="check if database is up", cmdStr="psql -l")
    for i in range(30):
        sleep(10)
        cmd.run()
        if cmd.get_return_code() == 0:
            return
    raise Exception("Database did not start up within 5 minutes`")


@given('the timestamp labels for scenario "{scenario_number}" are read from json')
def impl(context, scenario_number):
    label_key = 'timestamp_labels' + scenario_number
    global_labels[label_key] = _read_label_from_json(context, label_key)

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


@given('the database is "{version}" with dburl "{dbconn}"')
def impl(context, dbconn, version):
    command = '%s -t -q -c \'select version();\'' % (dbconn)
    (rc, out, err) = run_cmd(command)
    if not ('Greenplum Database ' + version) in out:
        print 'version %s does not match current gpdb version %s' % (version, out)


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


@given('the database "{dbname}" does not exist with connection "{dbconn}"')
@when('the database "{dbname}" does not exist with connection "{dbconn}"')
@then('the database "{dbname}" does not exist with connection "{dbconn}"')
def impl(context, dbname, dbconn):
    command = '%s -c \'drop database if exists %s;\'' % (dbconn, dbname)
    run_command(context, command)


@given('the database "{dbname}" exists with connection "{dbconn}"')
@when('the database "{dbname}" exists with connection "{dbconn}"')
@then('the database "{dbname}" exists with connection "{dbconn}"')
def impl(context, dbname, dbconn):
    command = '%s -c \'create database %s;\'' % (dbconn, dbname)
    run_command(context, command)


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


@when('an insert on "{table}" in "{dbname}" is rolled back')
def impl(context, table, dbname):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        insert_sql = """INSERT INTO %s values (1)""" % table
        dbconn.execSQL(conn, insert_sql)
        conn.rollback()


@when('a truncate on "{table}" in "{dbname}" is rolled back')
def impl(context, table, dbname):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        insert_sql = """TRUNCATE table %s""" % table
        dbconn.execSQL(conn, insert_sql)
        conn.rollback()


@when('an alter on "{table}" in "{dbname}" is rolled back')
def impl(context, table, dbname):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        insert_sql = """ALTER TABLE %s add column cnew int default 0""" % table
        dbconn.execSQL(conn, insert_sql)
        conn.rollback()


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
    'there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data "{with_data}" on host "{HOST}" with port "{PORT}" with user "{USER}"')
@when(
    'there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data "{with_data}" on host "{HOST}" with port "{PORT}" with user "{USER}"')
@then(
    'there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data "{with_data}" on host "{HOST}" with port "{PORT}" with user "{USER}"')
def impl(context, tabletype, table_name, compression_type, dbname, with_data, HOST, PORT, USER):
    host = os.environ.get(HOST)
    port = int(os.environ.get(PORT))
    user = os.environ.get(USER)
    with_data = bool(with_data)
    populate_regular_table_data(context, tabletype, table_name, compression_type, dbname, 10, with_data, host, port,
                                user)


@when('the partition table "{table_name}" in "{dbname}" is populated with similar data')
def impl(context, table_name, dbname):
    populate_partition_diff_data_same_eof(table_name, dbname)


@given('the partition table "{table_name}" in "{dbname}" is populated with same data')
def impl(context, table_name, dbname):
    populate_partition_same_data(table_name, dbname)


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


@given('there are no "{tmp_file_prefix}" tempfiles')
def impl(context, tmp_file_prefix):
    if tmp_file_prefix is not None and tmp_file_prefix:
        run_command(context, 'rm -f /tmp/%s*' % tmp_file_prefix)
    else:
        raise Exception('Invalid call to temp file removal %s' % tmp_file_prefix)


@then('{env_var} environment variable should be restored')
def impl(context, env_var):
    if not hasattr(context, 'orig_env'):
        raise Exception('%s can not be reset' % env_var)

    if env_var not in context.orig_env:
        raise Exception('%s can not be reset.' % env_var)

    os.environ[env_var] = context.orig_env[env_var]

    del context.orig_env[env_var]


@when('the table names in "{dbname}" is stored')
@then('the table names in "{dbname}" is stored')
def impl(context, dbname):
    context.table_names = get_table_names(dbname)


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


@given('the user puts cluster on "{HOST}" "{PORT}" "{USER}" in "{transition}"')
@when('the user puts cluster on "{HOST}" "{PORT}" "{USER}" in "{transition}"')
@then('the user puts cluster on "{HOST}" "{PORT}" "{USER}" in "{transition}"')
def impl(context, HOST, PORT, USER, transition):
    host = os.environ.get(HOST)
    user = os.environ.get(USER)
    port = os.environ.get(PORT)
    source_file = os.path.join(os.environ.get('GPHOME'), 'greenplum_path.sh')
    master_dd = os.environ.get('MASTER_DATA_DIRECTORY')
    export_mdd = 'export MASTER_DATA_DIRECTORY=%s;export PGPORT=%s' % (master_dd, port)
    # reset all fault inject entry if exists
    command = 'gpfaultinjector -f all -m async -y reset -r primary -H ALL'
    run_command_remote(context, command, host, source_file, export_mdd)
    command = 'gpfaultinjector -f all -m async -y resume -r primary -H ALL'
    run_command_remote(context, command, host, source_file, export_mdd)
    trigger_transition = "psql -d template1 -h %s -U %s -p %s -c 'drop table if exists trigger;'" % (host, user, port)
    if transition == 'ct':
        command = 'gpfaultinjector -f filerep_consumer -m async -y fault -r primary -H ALL'
        run_command_remote(context, command, host, source_file, export_mdd)
        run_command(context, trigger_transition)
        wait_till_change_tracking_transition(host, port, user)
    if transition == 'resync':
        command = 'gpfaultinjector -f filerep_consumer -m async -y fault -r primary -H ALL'
        run_command_remote(context, command, host, source_file, export_mdd)
        run_command(context, trigger_transition)
        wait_till_change_tracking_transition(host, port, user)
        command = 'gpfaultinjector -f filerep_resync -m async -y suspend -r primary -H ALL'
        run_command_remote(context, command, host, source_file, export_mdd)
        run_command_remote(context, 'gprecoverseg -a', host, source_file, export_mdd)
        wait_till_resync_transition(host, port, user)
    if transition == 'sync':
        run_command_remote(context, 'gpstop -air', host, source_file, export_mdd)
        run_command_remote(context, 'gprecoverseg -a', host, source_file, export_mdd)
        wait_till_insync_transition(host, port, user)
        run_command_remote(context, 'gprecoverseg -ar', host, source_file, export_mdd)


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


def run_valgrind_command(context, command, suppressions_file):
    current_path = os.path.realpath(__file__)
    current_dir = os.path.dirname(current_path)
    cmd_text = "valgrind --suppressions=%s/%s %s" % (current_dir, suppressions_file, command)
    run_command(context, cmd_text)
    for line in context.error_message.splitlines():
        if 'ERROR SUMMARY' in line:
            if '0 errors from 0 contexts' not in line:
                raise Exception('Output: %s' % context.error_message)
            else:
                return
    raise Exception('Could not find "ERROR SUMMARY" in %s' % context.error_message)


@when('the timestamp key is stored')
def impl(context):
    stdout = context.stdout_message
    for line in stdout.splitlines():
        if '--gp-k' in line:
            pat = re.compile('.* --gp-k=([0-9]{14}).*')
            m = pat.search(line)
            if not m:
                raise Exception('Timestamp key not found')
            context.timestamp_key = m.group(1)
            return


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


@given('{command} should not return a return code of {ret_code}')
@when('{command} should not return a return code of {ret_code}')
@then('{command} should not return a return code of {ret_code}')
def impl(context, command, ret_code):
    check_not_return_code(context, ret_code)


@then('an "{ex_type}" should be raised')
def impl(context, ex_type):
    if not context.exception:
        raise Exception('An exception was expected but was not thrown')
    typ = context.exception.__class__.__name__
    if typ != ex_type:
        raise Exception('got exception of type "%s" but expected type "%s"' % (typ, ex_type))


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


def validate_timestamp(ts):
    try:
        int_ts = int(ts)
    except Exception as e:
        raise Exception('Timestamp is not valid %s' % ts)

    if len(ts) != 14:
        raise Exception('Timestamp is invalid %s' % ts)


def get_timestamp_from_output(context):
    ts = None
    stdout = context.stdout_message
    for line in stdout.splitlines():
        if 'Timestamp key = ' in line:
            log_msg, delim, timestamp = line.partition('=')
            ts = timestamp.strip()
            validate_timestamp(ts)
            return ts

    raise Exception('Timestamp not found %s' % stdout)


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


@then('run post verifying workload under "{dirname}"')
def impl(context, dirname):
    for filename in os.listdir(dirname):
        if filename.endswith('.sql'):
            filename_prefix = os.path.splitext(filename)[0]
            ans_file_path = os.path.join(dirname, filename_prefix + '.ans')
            out_file_path = os.path.join(dirname, filename_prefix + '.out')
            diff_file_path = os.path.join(dirname, filename_prefix + '.diff')

            # run the command to get the data from the destination system, locally
            dbconn = 'psql -d template1 -p $GPTRANSFER_DEST_PORT -U $GPTRANSFER_DEST_USER -h $GPTRANSFER_DEST_HOST'
            command = '%s -f %s > %s' % (dbconn, os.path.join(dirname, filename), out_file_path)
            run_command(context, command)

            gpdiff_cmd = 'gpdiff.pl -w  -I NOTICE: -I HINT: -I CONTEXT: -I GP_IGNORE: --gpd_init=test/behave/mgmt_utils/steps/data/global_init_file %s %s > %s' % (
            ans_file_path, out_file_path, diff_file_path)
            run_command(context, gpdiff_cmd)
    for filename in os.listdir(dirname):
        full_filename_path = os.path.join(dirname, filename)
        if filename.endswith('.diff') and os.path.getsize(full_filename_path) > 0:
            with open(full_filename_path, 'r') as diff_file:
                diff_file_contents = diff_file.read()
                # if there is some difference generated into the diff file, raise expception
                raise Exception(
                    "Found difference between source and destination system, see %s. \n Diff contents: \n %s" % (
                        full_filename_path, diff_file_contents))


@then('verify that the incremental file in "{location}" has all the stored timestamps')
def impl(context, location):
    check_increments_file_for_list(context, location)


@then('verify that the incremental file has all the stored timestamps')
def impl(context):
    check_increments_file_for_list(context, master_data_dir)


@then('verify that there is no table "{tablename}" in "{dbname}"')
def impl(context, tablename, dbname):
    dbname = replace_special_char_env(dbname)
    tablename = replace_special_char_env(tablename)
    if check_table_exists(context, dbname=dbname, table_name=tablename):
        raise Exception("Table '%s' still exists when it should not" % tablename)


@then('verify that there is no view "{viewname}" in "{dbname}"')
def impl(context, viewname, dbname):
    if check_table_exists(context, dbname=dbname, table_name=viewname, table_type='view'):
        raise Exception("View '%s' still exists when it should not" % viewname)


@then('verify that there is no procedural language "{planguage}" in "{dbname}"')
def impl(context, planguage, dbname):
    if check_pl_exists(context, dbname=dbname, lan_name=planguage):
        raise Exception("Procedural Language '%s' still exists when it should not" % planguage)


@then('verify that there is a constraint "{conname}" in "{dbname}"')
def impl(context, conname, dbname):
    if not check_constraint_exists(context, dbname=dbname, conname=conname):
        raise Exception("Constraint '%s' does not exist when it should" % conname)


@then('verify that there is a rule "{rulename}" in "{dbname}"')
def impl(context, rulename, dbname):
    if not check_rule_exists(context, dbname=dbname, rulename=rulename):
        raise Exception("Rule '%s' does not exist when it should" % rulename)


@then('verify that there is a trigger "{triggername}" in "{dbname}"')
def impl(context, triggername, dbname):
    if not check_trigger_exists(context, dbname=dbname, triggername=triggername):
        raise Exception("Trigger '%s' does not exist when it should" % triggername)


@then('verify that there is an index "{indexname}" in "{dbname}"')
def impl(context, indexname, dbname):
    if not check_index_exists(context, dbname=dbname, indexname=indexname):
        raise Exception("Index '%s' does not exist when it should" % indexname)


@then('verify that there is a "{table_type}" table "{tablename}" in "{dbname}"')
def impl(context, table_type, tablename, dbname):
    if not check_table_exists(context, dbname=dbname, table_name=tablename, table_type=table_type):
        raise Exception("Table '%s' of type '%s' does not exist when expected" % (tablename, table_type))


@then(
    'verify that there is partition "{partition}" of "{table_type}" partition table "{tablename}" in "{dbname}" in "{schemaname}"')
def impl(context, partition, table_type, tablename, dbname, schemaname):
    if not check_partition_table_exists(context, dbname=dbname, schemaname=schemaname, table_name=tablename,
                                        table_type=table_type, part_level=1, part_number=partition):
        raise Exception("Partition %s for table '%s' of type '%s' does not exist when expected" % (
        partition, tablename, table_type))


@then(
    'verify that there is partition "{partition}" of mixed partition table "{tablename}" with storage_type "{storage_type}"  in "{dbname}" in "{schemaname}"')
@then(
    'verify that there is partition "{partition}" in partition level "{partitionlevel}" of mixed partition table "{tablename}" with storage_type "{storage_type}"  in "{dbname}" in "{schemaname}"')
def impl(context, partition, tablename, storage_type, dbname, schemaname, partitionlevel=1):
    part_t = get_partition_names(schemaname, tablename, dbname, partitionlevel, partition)
    partname = part_t[0][0].strip()
    validate_storage_type(context, partname, storage_type, dbname)


@given('there is a function "{functionname}" in "{dbname}"')
def impl(context, functionname, dbname):
    SQL = """CREATE FUNCTION %s(a integer, b integer)
    RETURNS integer AS $$
        if a > b:
            return a
        return b
    $$ LANGUAGE plpythonu;""" % functionname
    execute_sql(dbname, SQL)


@then('verify that storage_types of the partition table "{tablename}" are as expected in "{dbname}"')
def impl(context, tablename, dbname):
    validate_mixed_partition_storage_types(context, tablename, dbname)


@then(
    'data for partition table "{table_name}" with partition level "{part_level}" is distributed across all segments on "{dbname}"')
def impl(context, table_name, part_level, dbname):
    validate_part_table_data_on_segments(context, table_name, part_level, dbname)


@then('data for table "{table_name}" is distributed across all segments on "{dbname}"')
def impl(context, table_name, dbname):
    validate_table_data_on_segments(context, table_name, dbname)


@then('verify that tables "{table_list}" in "{dbname}" has no rows')
def impl(context, table_list, dbname):
    tables = [t.strip() for t in table_list.split(',')]
    for t in tables:
        check_empty_table(t, dbname)


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


@then('verify that partitioned tables "{table_list}" in "{dbname}" have {num_parts} partitions')
@then(
    'verify that partitioned tables "{table_list}" in "{dbname}" have {num_parts} partitions in partition level "{partitionlevel}"')
def impl(context, table_list, dbname, num_parts, partitionlevel=1):
    num_parts = int(num_parts.strip())
    tables = [t.strip() for t in table_list.split(',')]
    for t in tables:
        names = get_partition_tablenames(t, dbname, partitionlevel)
        if len(names) != num_parts:
            raise Exception("%s.%s should have %d partitions but has %d" % (dbname, t, num_parts, len(names)))


# raise exception if tname does not have X empty partitions
def check_x_empty_parts(dbname, tname, x):
    num_empty = 0
    parts = get_partition_tablenames(tname, dbname)
    for part in parts:
        p = part[0]
        try:
            check_empty_table(p, dbname)
            num_empty += 1
        except Exception as e:
            print e

    if num_empty != x:
        raise Exception("%s.%s has %d empty partitions and should have %d" % (dbname, tname, num_empty, x))


@then('there are no report files in the master data directory')
def impl(context):
    cleanup_report_files(context, master_data_dir)


@when('verify that partitioned tables "{table_list}" in "{dbname}" has {num_parts} empty partitions')
@then('verify that partitioned tables "{table_list}" in "{dbname}" has {num_parts} empty partitions')
def impl(context, table_list, dbname, num_parts):
    expected_num_parts = int(num_parts.strip())
    tables = [t.strip() for t in table_list.split(',')]
    for t in tables:
        check_x_empty_parts(dbname, t, expected_num_parts)


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


@given('there is a fake pg_aoseg table named "{table}" in "{dbname}"')
def impl(context, table, dbname):
    create_fake_pg_aoseg_table(context, table, dbname)


@then('verify that the "{file_type}" file in "{file_dir}" dir contains "{text_find}"')
def impl(context, file_type, file_dir, text_find):
    verify_file_contents(context, file_type, file_dir, text_find)


@then('verify that the "{file_type}" file in "{file_dir}" dir does not contain "{text_find}"')
def impl(context, file_type, file_dir, text_find):
    verify_file_contents(context, file_type, file_dir, text_find, should_contain=False)


@then('verify there are no "{tmp_file_prefix}" tempfiles')
def impl(context, tmp_file_prefix):
    if tmp_file_prefix is not None and tmp_file_prefix:
        if glob.glob('/tmp/%s*' % tmp_file_prefix):
            raise Exception('Found temp %s files where they should not be present' % tmp_file_prefix)
    else:
        raise Exception('Invalid call to temp file removal %s' % tmp_file_prefix)


@then('tables names should be identical to stored table names in "{dbname}" except "{fq_table_name}"')
def impl(context, dbname, fq_table_name):
    table_names = sorted(get_table_names(dbname))
    stored_table_names = sorted(context.table_names)
    if fq_table_name != "":
        stored_table_names.remove(fq_table_name.strip().split('.'))
    compare_table_lists(table_names, stored_table_names, dbname)


@then('tables names should be identical to stored table names in "{dbname}"')
def impl(context, dbname):
    table_names = sorted(get_table_names(dbname))
    stored_table_names = sorted(context.table_names)
    compare_table_lists(table_names, stored_table_names, dbname)


@then('tables names in database "{dbname}" should be identical to stored table names in file "{table_file_name}"')
def impl(context, dbname, table_file_name):
    table_names = sorted(get_table_names(dbname))
    stored_table_file_path = os.path.join('test/behave/mgmt_utils/steps/data', table_file_name)
    stored_table_file = open(stored_table_file_path, 'r')
    reader = csv.reader(stored_table_file)
    stored_table_names = list(reader)

    compare_table_lists(table_names, stored_table_names, dbname)


@then('tables in "{dbname}" should not contain any data')
def impl(context, dbname):
    for table in context.table_names:
        table_name = "%s.%s" % (table[0], table[1])
        check_empty_table(table_name, dbname)


@then('verify that the data of "{expected_count}" tables in "{dbname}" is validated after restore')
def impl(context, dbname, expected_count):
    validate_db_data(context, dbname, int(expected_count))


@then(
    'verify that the data of "{expected_count}" tables in "{dbname}" is validated after restore from "{backedup_dbname}"')
def impl(context, dbname, expected_count, backedup_dbname):
    validate_db_data(context, dbname, int(expected_count), backedup_dbname=backedup_dbname)


@then('pg_stat_last_operation registers the truncate for tables "{table_list}" in "{dbname}" in schema "{schema}"')
def impl(context, table_list, dbname, schema):
    if not table_list:
        raise Exception('Empty table list')
    tables = table_list.split(',')
    for t in tables:
        table_oid = get_table_oid(context, dbname, schema, t.strip())
        verify_truncate_in_pg_stat_last_operation(context, dbname, table_oid)


@then(
    'pg_stat_last_operation does not register the truncate for tables "{table_list}" in "{dbname}" in schema "{schema}"')
def impl(context, table_list, dbname, schema):
    if not table_list:
        raise Exception('Empty table list')
    tables = table_list.split(',')
    for t in tables:
        table_oid = get_table_oid(context, dbname, schema, t.strip())
        verify_truncate_not_in_pg_stat_last_operation(context, dbname, table_oid)


@given('the numbers "{lownum}" to "{highnum}" are inserted into "{tablename}" tables in "{dbname}"')
@when('the numbers "{lownum}" to "{highnum}" are inserted into "{tablename}" tables in "{dbname}"')
def impl(context, lownum, highnum, tablename, dbname):
    insert_numbers(dbname, tablename, lownum, highnum)


@when('the user adds column "{cname}" with type "{ctype}" and default "{defval}" to "{tname}" table in "{dbname}"')
def impl(context, cname, ctype, defval, tname, dbname):
    sql = "ALTER table %s ADD COLUMN %s %s DEFAULT %s" % (tname, cname, ctype, defval)
    execute_sql(dbname, sql)


@then('the saved state file is deleted')
def impl(context):
    run_command(context, 'rm -f %s' % context.state_file)
    if context.exception:
        raise context.exception


@then('the saved state file is corrupted')
def impl(context):
    write_lines = list()
    with open(context.state_file, "r") as fr:
        lines = fr.readlines()

    for line in lines:
        line = line.replace(",", "")
        write_lines.append(line)

    with open(context.state_file, "w") as fw:
        for line in write_lines:
            fw.write("%s\n" % line.rstrip())


@then('"{table_name}" is marked as dirty in dirty_list file')
def impl(context, table_name):
    dirty_list = get_dirty_list_filename(context)
    with open(dirty_list) as fp:
        for line in fp:
            if table_name.strip() in line.strip():
                return

    raise Exception('Expected table %s to be marked as dirty in %s' % (table_name, dirty_list))


@when('the "{table_name}" is recreated with same data in "{dbname}"')
def impl(context, table_name, dbname):
    select_sql = 'select * into public.temp from %s' % table_name
    execute_sql(dbname, select_sql)
    drop_sql = 'drop table %s' % table_name
    execute_sql(dbname, drop_sql)
    recreate_sql = 'select * into %s from public.temp' % table_name
    execute_sql(dbname, recreate_sql)


@given('the row "{row_values}" is inserted into "{table}" in "{dbname}"')
def impl(context, row_values, table, dbname):
    insert_row(context, row_values, table, dbname)


@then('an exception should be raised with "{txt}"')
def impl(context, txt):
    if not context.exception:
        raise Exception("An exception was not raised")
    output = context.exception.__str__()
    if not txt in output:
        raise Exception("Exception output is not matching: '%s'" % output)


@then('verify that database "{dbname}" does not exist')
def impl(context, dbname):
    with dbconn.connect(dbconn.DbURL(dbname='template1')) as conn:
        sql = """SELECT datname FROM pg_database"""
        dbs = dbconn.execSQL(conn, sql)
        if dbname in dbs:
            raise Exception('Database exists when it shouldnt "%s"' % dbname)


@then('there are no saved data files')
def impl(context):
    clear_all_saved_data_verify_files(context)


@then('the dump timestamp for "{db_list}" are different')
def impl(context, db_list):
    if db_list is None:
        raise Exception('Expected at least 1 database in the list, found none.')

    if not hasattr(context, 'db_timestamps'):
        raise Exception('The database timestamps need to be stored')

    db_names = db_list.strip().split(',')
    for db in db_names:
        if db.strip() not in context.db_timestamps:
            raise Exception('Could not find timestamp for database: %s' % context.db_timestamps)

    timestamp_set = set([v for v in context.db_timestamps.values()])
    if len(timestamp_set) != len(context.db_timestamps):
        raise Exception('Some databases have same timestamp: "%s"' % context.db_timestamps)


@given('there is a "{table_type}" table "{table_name}" in "{db_name}" having large number of partitions')
def impl(context, table_type, table_name, db_name):
    create_large_num_partitions(table_type, table_name, db_name)


@given('there is a "{table_type}" table "{table_name}" in "{db_name}" having "{num_partitions}" partitions')
def impl(context, table_type, table_name, db_name, num_partitions):
    if not num_partitions.strip().isdigit():
        raise Exception('Invalid number of partitions specified "%s"' % num_partitions)

    num_partitions = int(num_partitions.strip()) + 1
    create_large_num_partitions(table_type, table_name, db_name, num_partitions)


@given('there is a table-file "{filename}" with tables "{table_list}"')
@then('there is a table-file "{filename}" with tables "{table_list}"')
def impl(context, filename, table_list):
    tables = table_list.split(',')
    with open(filename, 'w') as fd:
        for table in tables:
            fd.write(table.strip() + '\n')

    if not os.path.exists(filename):
        raise Exception('Unable to create file "%s"' % filename)


def create_ext_table_file(file_location):
    with open(file_location, 'w') as fd:
        for i in range(100):
            fd.write('abc, 10, 10\n')


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


@given('there is an external table "{tablename}" in "{dbname}" with data for file "{file_location}"')
def impl(context, tablename, dbname, file_location):
    create_ext_table_file(file_location)
    host, port = get_host_and_port()
    ext_table_sql = """CREATE EXTERNAL WEB TABLE %s(name text, column1 int, column2 int) EXECUTE 'cat %s 2> /dev/null || true'
                       ON MASTER FORMAT 'CSV' (DELIMITER ',')""" % (tablename, file_location)
    execute_sql(dbname, ext_table_sql)

    verify_ext_table_creation_sql = """SELECT count(*) FROM pg_class WHERE relname = '%s'""" % tablename
    row_count = getRows(dbname, verify_ext_table_creation_sql)[0][0]
    if row_count != 1:
        raise Exception(
            'Creation of external table failed for "%s:%s, row count = %s"' % (file_location, tablename, row_count))


@then('verify that exactly "{num_tables}" tables in "{dbname}" have been restored')
def impl(context, num_tables, dbname):
    validate_num_restored_tables(context, num_tables, dbname)


@then('verify that exactly "{num_tables}" tables in "{dbname}" have been restored from "{backedup_dbname}"')
def impl(context, num_tables, dbname, backedup_dbname):
    validate_num_restored_tables(context, num_tables, dbname, backedup_dbname=backedup_dbname)


@given('there are "{table_count}" "{tabletype}" tables "{table_name}" with data in "{dbname}"')
def impl(context, table_count, tabletype, table_name, dbname):
    table_count = int(table_count)
    for i in range(1, table_count + 1):
        tablename = "%s_%s" % (table_name, i)
        create_database_if_not_exists(context, dbname)
        drop_table_if_exists(context, table_name=tablename, dbname=dbname)
        create_partition(context, tablename, tabletype, dbname, compression_type=None, partition=False)


@given('the tables "{table_list}" are in dirty hack file "{dirty_hack_file}"')
def impl(context, table_list, dirty_hack_file):
    tables = [t.strip() for t in table_list.split(',')]

    with open(dirty_hack_file, 'w') as fd:
        for t in tables:
            fd.write(t + '\n')

    if not os.path.exists(dirty_hack_file):
        raise Exception('Failed to create dirty hack file "%s"' % dirty_hack_file)


@given(
    'partition "{part_num}" of partition tables "{table_list}" in "{dbname}" in schema "{schema}" are in dirty hack file "{dirty_hack_file}"')
def impl(context, part_num, table_list, dbname, schema, dirty_hack_file):
    tables = table_list.split(',')
    with open(dirty_hack_file, 'w') as fd:
        part_num = int(part_num.strip())
        for table in tables:
            part_t = get_partition_names(schema, table.strip(), dbname, 1, part_num)
            if len(part_t) < 1 or len(part_t[0]) < 1:
                print part_t
            partition_name = part_t[0][0].strip()
            fd.write(partition_name + '\n')

    if not os.path.exists(dirty_hack_file):
        raise Exception('Failed to write to dirty hack file "%s"' % dirty_hack_file)


@then('verify that the tuple count of all appendonly tables are consistent in "{dbname}"')
def impl(context, dbname):
    ao_partition_list = get_partition_list('ao', dbname)
    verify_stats(dbname, ao_partition_list)
    co_partition_list = get_partition_list('co', dbname)
    verify_stats(dbname, co_partition_list)


@then('verify that there are no aoco stats in "{dbname}" for table "{tables}"')
def impl(context, dbname, tables):
    tables = tables.split(',')
    for t in tables:
        validate_no_aoco_stats(context, dbname, t.strip())


@then('verify that there are "{tupcount}" tuples in "{dbname}" for table "{tables}"')
def impl(context, tupcount, dbname, tables):
    tables = tables.split(',')
    for t in tables:
        validate_aoco_stats(context, dbname, t.strip(), tupcount)


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


@then('the gpfdist should print {msg} to "{filename}" under {parent_dir}')
def impl(context, msg, filename, parent_dir):
    if parent_dir == 'CWD':
        parent_dir = os.getcwd()
    filepath = '%s/%s' % (parent_dir, filename)

    with open(filepath, 'r') as fp:
        for line in fp:
            if msg in line:
                return

        raise Exception('Log file %s did not contain the message %s' % (filepath, msg))


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


@when('all rows from table "{tname}" db "{dbname}" are stored in the context')
def impl(context, tname, dbname):
    context.stored_rows = []

    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        sql = "SELECT * FROM %s" % tname
        curs = dbconn.execSQL(conn, sql)
        context.stored_rows = curs.fetchall()

@given('results of the sql "{sql}" db "{dbname}" are stored in the context')
@when( 'results of the sql "{sql}" db "{dbname}" are stored in the context')
def impl(context, sql, dbname):
    context.stored_sql_results = []

    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        curs = dbconn.execSQL(conn, sql)
        context.stored_sql_results = curs.fetchall()


@then('validate that "{dataline}" "{formatter}" seperated by "{delim}" is in the stored rows')
def impl(context, dataline, formatter, delim):
    lookfor = dataline.split(delim)
    formatter = formatter.split(delim)

    for i in range(len(formatter)):
        if formatter[i] == 'int':
            lookfor[i] = int(lookfor[i])

    if lookfor not in context.stored_rows:
        print context.stored_rows
        print lookfor
        raise Exception("'%s' not found in stored rows" % dataline)


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


@then('validate that stored rows has "{numlines}" lines of output')
def impl(context, numlines):
    num_found = len(context.stored_rows)
    numlines = int(numlines)
    if num_found != numlines:
        raise Exception("Found %d of stored query result but expected %d records" % (num_found, numlines))


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


@when('the user activates standby on the standbyhost')
@then('the user activates standby on the standbyhost')
def impl(context):
    port = os.environ.get('PGPORT')
    cmd = 'PGPORT=%s MASTER_DATA_DIRECTORY=%s gpactivatestandby -d %s -fa' % (port, master_data_dir, master_data_dir)
    cmd = Command('run remote command', cmd, ctxt=REMOTE, remoteHost=context.standby_host)
    cmd.run(validateAfter=True)


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


@when('the temp files "{filename_prefix}" are removed from the system')
@given('the temp files "{filename_prefix}" are removed from the system')
def impl(context, filename_prefix):
    hostlist = get_all_hostnames_as_list(context, 'template1')
    print hostlist
    for host in hostlist:
        cmd = Command(name='remove data directories',
                      cmdStr='rm -rf /tmp/%s*' % filename_prefix,
                      ctxt=REMOTE,
                      remoteHost=host)
        cmd.run(validateAfter=True)


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


@when('user runs "{cmd}" with sudo access')
def impl(context, cmd):
    gphome = os.environ.get('GPHOME')
    python_path = os.environ.get('PYTHONPATH')
    python_home = os.environ.get('PYTHONHOME')
    ld_library_path = os.environ.get('LD_LIBRARY_PATH')
    path = os.environ.get('PATH')
    cmd_str = """sudo GPHOME=%s
                    PATH=%s
                    PYTHONHOME=%s
                    PYTHONPATH=%s
                    LD_LIBRARY_PATH=%s %s/bin/%s""" % (
    gphome, path, python_home, python_path, ld_library_path, gphome, cmd)
    run_command(context, cmd_str)


def verify_num_files(results, expected_num_files, timestamp):
    num_files = results.stdout.strip()
    if num_files != expected_num_files:
        raise Exception(
            'Expected "%s" files with timestamp key "%s" but found "%s"' % (expected_num_files, timestamp, num_files))


def verify_timestamps_on_master(timestamp, dump_type):
    list_cmd = 'ls -l %s/db_dumps/%s/*%s* | wc -l' % (master_data_dir, timestamp[:8], timestamp)
    cmd = Command('verify timestamps on master', list_cmd)
    cmd.run(validateAfter=True)
    expected_num_files = '10' if dump_type == 'incremental' else '8'
    verify_num_files(cmd.get_results(), expected_num_files, timestamp)


def verify_timestamps_on_segments(timestamp):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    primary_segs = [segdb for segdb in gparray.getDbList() if segdb.isSegmentPrimary()]

    for seg in primary_segs:
        db_dumps_dir = os.path.join(seg.getSegmentDataDirectory(),
                                    'db_dumps',
                                    timestamp[:8])
        list_cmd = 'ls -l %s/*%s* | wc -l' % (db_dumps_dir, timestamp)
        cmd = Command('get list of dump files', list_cmd, ctxt=REMOTE, remoteHost=seg.getSegmentHostName())
        cmd.run(validateAfter=True)
        verify_num_files(cmd.get_results(), '2', timestamp)


def validate_files(file_list, pattern_list, expected_file_count):
    file_count = 0
    for pattern in pattern_list:
        pat = re.compile(pattern)
        pat_found = False
        for f in file_list:
            m = pat.search(f.strip())
            if m is not None:
                pat_found = True
                file_count += 1

        if not pat_found:
            raise Exception('Expected file not found for pattern: "%s" in file list "%s"' % (pattern, file_list))

    if file_count != expected_file_count:
        raise Exception('Expected count of %d does not match actual count %d in file list "%s"' % (
        expected_file_count, file_count, file_list))


@then('close all opened pipes')
def impl(context):
    hosts = set(get_all_hostnames_as_list(context, 'template1'))
    for h in hosts:
        find_cmd = Command('get list of pipe processes',
                           "ps -eaf | grep _pipe.py | grep -v grep | grep -v ssh",
                           ctxt=REMOTE,
                           remoteHost=h)
        find_cmd.run()
        results = find_cmd.get_stdout().split('\n')
        for process in results:
            if not process.strip():
                continue
            pid = process.split()[1].strip()
            print 'pid = %s on host %s' % (pid, h)
            cmd = Command('Kill pipe process',
                          "kill %s" % pid,
                          ctxt=REMOTE,
                          remoteHost=h)
            cmd.run(validateAfter=True)

        find_cmd.run()  # We expect failure here
        results = find_cmd.get_stdout()
        if results:
            raise Exception('Unexpected pipe processes found "%s"' % results)


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


@given('the path "{path}" exists')
def impl(context, path):
    if not os.path.exists(path):
        os.makedirs(path)


@then('the path "{path}" does not exist')
def impl(context, path):
    if os.path.exists(path):
        shutil.rmtree(path)


@when('the user runs the following query on "{dbname}" without fetching results')
def impl(context, dbname):
    query = context.text.strip()
    thread.start_new_thread(execute_sql, (dbname, query))
    time.sleep(30)


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


@when('the user runs command "{cmd}" on the "{seg_type}" segment')
def impl(context, cmd, seg_type):
    if seg_type == 'Change Tracking':
        port, host = get_change_tracking_segment_info()
    elif seg_type == 'Original':
        port, host = context.killed_seg_port, context.killed_seg_host
    else:
        raise Exception('Invalid segment type "%s" specified' % seg_type)

    cmd += ' -p %s -h %s' % (port, host)

    run_command(context, cmd)


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


@given('the directory {path} exists')
@then('the directory {path} exists')
def impl(context, path):
    if not os.path.isdir(path):
        raise Exception('Directory "%s" does not exist' % path)


@then('{file} should be found in tarball with prefix "{prefix}" within directory {path}')
def impl(context, file, prefix, path):
    ## look for subdirectory created during collection
    collection_dirlist = os.listdir(path)

    if len(collection_dirlist) > 1:
        raise Exception('more then one data collection directory found.')
    if len(collection_dirlist) == 0:
        raise Exception('Collection directory was not found')

    ## get a listing of files in the subdirectory and make sure there is only one tarball found
    tarpath = os.path.join(path, collection_dirlist[0])
    collection_filelist = os.listdir(tarpath)

    if len(collection_filelist) > 1:
        raise Exception('Too many files found in "%s"' % tarpath)
    if len(collection_filelist) == 0:
        raise Exception('No files found in "%s"' % tarpath)

    ## Expand tarball with prefix "GP_LOG_COLLECTION_" and search for given file within collection
    if prefix in collection_filelist[0]:

        ## extract the root tarball
        tar = tarfile.open(os.path.join(tarpath, collection_filelist[0]), "r:")
        tar.extractall(tarpath)
        tar.close()

        FOUND = False
        for tfile in os.listdir(tarpath):
            if prefix in tfile:
                continue

            ## Find any tar file that contain given file
            segtar = tarfile.open(os.path.join(tarpath, tfile), "r:gz")
            for tarinfo in segtar:
                if tarinfo.isreg() and file in tarinfo.name:
                    FOUND = True
                    segtar.close()
                    break  ## break segtar loop
        if FOUND:
            return  ## we found the file so pass the test case
        else:
            ## the file should have been found in the first segment tarball
            raise Exception('Unable to find "%s" in "%s" tar file' % (file, collection_filelist[0]))
    else:
        raise Exception('tarball with prefix "%s" was not found' % prefix)


@given('the directory {path} is removed or does not exist')
@when('the directory {path} is removed or does not exist')
@then('the directory {path} is removed or does not exist')
def impl(context, path):
    if '*' in path:
        raise Exception('Wildcard not supported')

    if path[0] == '~':
        path = os.path.join(os.path.expanduser('~'), path[2:])

    run_command(context, 'rm -rf %s' % path)


@given('the user runs sbin command "{cmd}"')
@when('the user runs sbin command "{cmd}"')
@then('the user runs sbin command "{cmd}"')
def impl(context, cmd):
    gpsbin = os.path.join(os.environ.get('GPHOME'), 'sbin')

    if not os.path.isdir(gpsbin):
        raise Exception('ERROR: GPHOME not set in environment')

    cmd = gpsbin + "/" + cmd  ## don't us os.path join because command might have arguments
    run_command(context, cmd)


@given('the OS type is not "{os_type}"')
def impl(context, os_type):
    assert platform.system() != os_type


@then('{file1} and {file2} should exist and have a new mtime')
def impl(context, file1, file2):
    gphome = os.environ.get('GPHOME')

    if not os.path.isfile(os.path.join(gphome, file1)) or not os.path.isfile(os.path.join(gphome, file2)):
        raise Exception(
            'installation of ' + context.utility + ' failed because one or more files do not exist in ' + os.path.join(
                gphome, file1))

    file1_mtime = os.path.getmtime(os.path.join(gphome, file1))
    file2_mtime = os.path.getmtime(os.path.join(gphome, file2))

    if file1_mtime < context.currenttime or file2_mtime < context.currenttime:
        raise Exception('one or both file mtime was not updated')

    os.chdir(context.cwd)
    run_command(context, 'rm -rf %s' % context.path)


@given('you are about to run a database query')
@when('you are about to run a database query')
@then('you are about to run a database query')
def impl(context):
    context.sessionID = []


@given('the user ran this query "{query}" against database "{dbname}" for table "{table}" and it hangs')
@when('the user ran this query "{query}" against database "{dbname}" for table "{table}" and it hangs')
def impl(context, query, dbname, table):
    get_sessionID_query = "SELECT sess_id FROM pg_stat_activity WHERE current_query ~ '" + table + "' AND current_query !~ 'pg_stat_activity'"

    if not check_db_exists(dbname):
        raise Exception('The database ' + dbname + 'Does not exist')

    thread.start_new_thread(getRows, (dbname, query))
    time.sleep(15)
    context.sessionIDRow = getRows(dbname, get_sessionID_query)

    if len(context.sessionIDRow) == 0:
        raise Exception('Was not able to determine the session ID')

    context.sessionID.append(context.sessionIDRow[0][0])


@then('user runs "{command}" against the queries session ID')
def impl(context, command):
    ## build the session_list
    session_list = None
    for session in context.sessionID:
        if not session_list:
            session_list = str(session)
        else:
            session_list = session_list + ',' + str(session)

    command += " " + session_list
    run_gpcommand(context, command)


@then('{file} file with queries sessionIDs should be found within directory {path}')
def impl(context, file, path):
    ######################################################################################
    ## This function needs to be modified.. changes are pending hung_analyzer revisions ##
    ######################################################################################

    ## look for subdirectory created during collection
    collection_dirlist = os.listdir(path)

    if len(collection_dirlist) > 1:
        raise Exception(
            'more then one data collection directory found.  Possibly left over from a previous run of hung analyzer')
    if len(collection_dirlist) == 0:
        raise Exception('Collection directory was not found')

    ## Make sure we have a core file for each session
    sessions_found = 0
    for rootdir, dirs, files in os.walk(os.path.join(path, collection_dirlist[0])):
        for session in context.sessionID:
            for f in files:
                core_prefix = file + str(session) + '.'
                if core_prefix in f:
                    sessions_found += 1
                    break

    if sessions_found == 0:
        raise Exception('No core files were found in collection')

    if sessions_found != len(context.sessionID):
        raise Exception('Only ' + str(sessions_found) + ' core files were found out of ' + str(len(context.sessionID)))


@then('{file} file should be found within directory {path}')
def impl(context, file, path):
    ## look for subdirectory created during collection
    collection_dirlist = os.listdir(path)

    if len(collection_dirlist) > 1:
        raise Exception(
            'more then one data collection directory found.  Possibly left over from a previous run of hung analyzer')
    if len(collection_dirlist) == 0:
        raise Exception('Collection directory was not found')

    ## get a listing of files and dirs and prune until file is found
    for rootdir, dirs, files in os.walk(os.path.join(path, collection_dirlist[0])):
        for f in files:
            if file in f:
                return

    raise Exception('File was not found in :' + path)


@then('database is restarted to kill the hung query')
def impl(context):
    try:
        stop_database_if_started(context)
    except Exception as e:
        context.exception = None
        pass  ## capture the thread dieing from our hung query

    if check_database_is_running(context):
        raise Exception('Failed to stop the database')

    start_database_if_not_started(context)
    if not check_database_is_running():
        raise Exception('Failed to start the database')


@then('partition "{partitionnum}" is added to partition table "{tablename}" in "{dbname}"')
def impl(context, partitionnum, tablename, dbname):
    add_partition(context, partitionnum, tablename, dbname)


@then('partition "{partitionnum}" is dropped from partition table "{tablename}" in "{dbname}"')
def impl(context, partitionnum, tablename, dbname):
    drop_partition(context, partitionnum, tablename, dbname)


@when('table "{tablename}" is dropped in "{dbname}"')
@then('table "{tablename}" is dropped in "{dbname}"')
@given('table "{tablename}" is dropped in "{dbname}"')
def impl(context, tablename, dbname):
    drop_table_if_exists(context, table_name=tablename, dbname=dbname)


def create_trigger_function(dbname, trigger_func_name, tablename):
    trigger_func_sql = """
    CREATE OR REPLACE FUNCTION %s() RETURNS TRIGGER AS $$
    BEGIN
    INSERT INTO %s VALUES(2001, 'backup', '2100-08-23');
    END;
    $$ LANGUAGE plpgsql
    """ % (trigger_func_name, tablename)
    execute_sql(dbname, trigger_func_sql)


def create_trigger(dbname, trigger_func_name, trigger_name, tablename):
    SQL = """
    CREATE TRIGGER %s AFTER INSERT OR UPDATE OR DELETE ON %s FOR EACH STATEMENT EXECUTE PROCEDURE %s();
    """ % (trigger_name, tablename, trigger_func_name)
    execute_sql(dbname, SQL)


@given(
    'there is a trigger "{trigger_name}" on table "{tablename}" in "{dbname}" based on function "{trigger_func_name}"')
def impl(context, trigger_name, tablename, dbname, trigger_func_name):
    create_trigger_function(dbname, trigger_func_name, tablename)
    create_trigger(dbname, trigger_func_name, trigger_name, tablename)


@then('there is a trigger function "{trigger_func_name}" on table "{tablename}" in "{dbname}"')
def impl(context, trigger_func_name, tablename, dbname):
    create_trigger_function(dbname, trigger_func_name, tablename)


@when('the index "{index_name}" in "{dbname}" is dropped')
def impl(context, index_name, dbname):
    drop_index_sql = """DROP INDEX %s""" % index_name
    execute_sql(dbname, drop_index_sql)


@when('the trigger "{trigger_name}" on table "{tablename}" in "{dbname}" is dropped')
def impl(context, trigger_name, tablename, dbname):
    drop_trigger_sql = """DROP TRIGGER %s ON %s""" % (trigger_name, tablename)
    execute_sql(dbname, drop_trigger_sql)


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
                               , noWait=False
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


@given('the user creates an init config file "{to_file}" without mirrors')
@when('the user creates an init config file "{to_file}" without mirrors')
@then('the user creates an init config file "{to_file}" without mirrors')
def impl(context, to_file):
    write_lines = []
    BLDWRAP_TOP = os.environ.get('BLDWRAP_TOP')
    # this file is the output of the pulse system, where gpinit has been run
    from_file = BLDWRAP_TOP + '/sys_mgmt_test/test/general/cluster_conf.out'
    with open(from_file) as fr:
        lines = fr.readlines()
        for line in lines:
            if not line.startswith('REPLICATION_PORT_BASE') and not line.startswith(
                    'MIRROR_REPLICATION_PORT_BASE') and not line.startswith('MIRROR_PORT_BASE') and not line.startswith(
                    'declare -a MIRROR_DATA_DIRECTORY'):
                write_lines.append(line)

    with open(to_file, 'w+') as fw:
        fw.writelines(write_lines)


@given('the user creates mirrors config file "{to_file}"')
@when('the user creates mirrors config file "{to_file}"')
@then('the user creates mirrors config file "{to_file}"')
def impl(context, to_file):
    data_dirs = []
    BLDWRAP_TOP = os.environ.get('BLDWRAP_TOP')
    from_file = BLDWRAP_TOP + '/sys_mgmt_test/test/general/cluster_conf.out'
    with open(from_file) as fr:
        lines = fr.readlines()
        for line in lines:
            if line.startswith('declare -a MIRROR_DATA_DIRECTORY'):
                data_dirs = line.split('(')[-1].strip().strip(')').split()
                break

    if not data_dirs:
        raise Exception("Could not find MIRROR_DATA_DIRECTORY in config file %s" % from_file)

    with open(to_file, 'w+') as fw:
        for dir in data_dirs:
            fw.write(dir.strip(')') + '\n')


@given('the standby hostname is saved')
@when('the standby hostname is saved')
@then('the standby hostname is saved')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    primary_segs = [seg for seg in gparray.getDbList() if (seg.isSegmentPrimary() and not seg.isSegmentMaster())]
    context.standby = primary_segs[0].getSegmentHostName()


@given('user runs the init command "{cmd}" with the saved standby host')
@when('user runs the init command "{cmd}" with the saved standby host')
@then('user runs the init command "{cmd}" with the saved standby host')
def impl(context, cmd):
    run_cmd = cmd + '-s %s' % context.standby
    run_cmd.run(validateAfter=True)


@given('there is a sequence "{seq_name}" in "{dbname}"')
def impl(context, seq_name, dbname):
    sequence_sql = 'CREATE SEQUENCE %s' % seq_name
    execute_sql(dbname, sequence_sql)


@when('the user removes the "{cmd}" command on standby')
@then('the user removes the "{cmd}" command on standby')
def impl(context, cmd):
    cmdStr = 'chmod u+rw ~/.bashrc && cp ~/.bashrc ~/.bashrc.backup'
    run_cmd = Command('run remote command', cmdStr, ctxt=REMOTE, remoteHost=context.standby_host)
    run_cmd.run(validateAfter=True)
    cmdStr = """echo >>~/.bashrc && echo "shopt -s expand_aliases" >>~/.bashrc && echo "alias %s='no_command'" >>~/.bashrc""" % cmd
    run_cmd = Command('run remote command', cmdStr, ctxt=REMOTE, remoteHost=context.standby_host)
    run_cmd.run(validateAfter=True)


@when('the user restores the "{cmd}" command on the standby')
@then('the user restores the "{cmd}" command on the standby')
def impl(context, cmd):
    cmdStr = 'cp ~/.bashrc.backup ~/.bashrc'
    run_cmd = Command('run remote command', cmdStr, ctxt=REMOTE, remoteHost=context.standby_host)
    run_cmd.run(validateAfter=True)


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


@given(
    'partition "{partition}" of partition table "{schema_parent}.{table_name}" is assumed to be in schema "{schema_child}" in database "{dbname}"')
@when(
    'partition "{partition}" of partition table "{schema_parent}.{table_name}" is assumed to be in schema "{schema_child}" in database "{dbname}"')
@then(
    'partition "{partition}" of partition table "{schema_parent}.{table_name}" is assumed to be in schema "{schema_child}" in database "{dbname}"')
def impl(context, partition, schema_parent, table_name, schema_child, dbname):
    part_t = get_partition_names(schema_parent.strip(), table_name.strip(), dbname.strip(), 1, partition)
    if len(part_t) < 1 or len(part_t[0]) < 1:
        print part_t
    a_partition_name = part_t[0][0].strip()
    alter_sql = """ALTER TABLE %s SET SCHEMA %s""" % (a_partition_name, schema_child)
    execute_sql(dbname, alter_sql)


@given('this test sleeps for "{secs}" seconds')
@when('this test sleeps for "{secs}" seconds')
@then('this test sleeps for "{secs}" seconds')
def impl(context, secs):
    secs = float(secs)
    time.sleep(secs)


@then('verify that there are no duplicates in column "{columnname}" of table "{tablename}" in "{dbname}"')
def impl(context, columnname, tablename, dbname):
    duplicate_sql = 'SELECT %s, COUNT(*) FROM %s GROUP BY %s HAVING COUNT(*) > 1' % (columnname, tablename, columnname)
    rows = getRows(dbname, duplicate_sql)
    if rows:
        raise Exception(
            'Found duplicate rows in the column "%s" for table "%s" in database "%s"' % (columnname, tablename, dbname))


def execute_sql_for_sec(dbname, query, sec):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, query)
        conn.commit()
        time.sleep(sec)


@given('the user runs the query "{query}" on "{dbname}" for "{sec}" seconds')
@when('the user runs the query "{query}" on "{dbname}" for "{sec}" seconds')
@then('the user runs the query "{query}" on "{dbname}" for "{sec}" seconds')
def impl(context, query, dbname, sec):
    if query.lower().startswith('create') or query.lower().startswith('insert'):
        thread.start_new_thread(execute_sql_for_sec, (dbname, query, float(sec)))
    else:
        thread.start_new_thread(getRows, (dbname, query))
    time.sleep(30)


@given('verify that the contents of the files "{expected_filepath}" and "{result_filepath}" are identical')
@when('verify that the contents of the files "{expected_filepath}" and "{result_filepath}" are identical')
@then('verify that the contents of the files "{expected_filepath}" and "{result_filepath}" are identical')
def impl(context, expected_filepath, result_filepath):
    diff_files(expected_filepath, result_filepath)


@given('the standby is not initialized')
@then('the standby is not initialized')
def impl(context):
    standby = get_standby_host()
    if standby:
        context.cluster_had_standby = True
        context.standby_host = standby
        run_gpcommand(context, 'gpinitstandby -ra')


@when('user can "{can_ssh}" ssh locally on standby')
@then('user can "{can_ssh}" ssh locally on standby')
def impl(context, can_ssh):
    if not hasattr(context, 'standby_host'):
        raise Exception('Standby host not stored in context !')
    if can_ssh.strip() == 'not':
        cmdStr = 'mv ~/.ssh/authorized_keys ~/.ssh/authorized_keys.bk'
    else:
        cmdStr = 'mv ~/.ssh/authorized_keys.bk ~/.ssh/authorized_keys'
    cmd = Command(name='disable ssh locally', cmdStr=cmdStr,
                  ctxt=REMOTE, remoteHost=context.standby_host)
    cmd.run(validateAfter=True)


@given('all the compression data from "{dbname}" is saved for verification')
def impl(context, dbname):
    partitions = get_partition_list('ao', dbname) + get_partition_list('co', dbname)
    with open('test/data/compression_{db}_backup'.format(db=dbname), 'w') as fp:
        with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
            for p in partitions:
                query = """SELECT get_ao_compression_ratio('{schema}.{table}')""".format(schema=p[1], table=p[2])
                compression_rate = dbconn.execSQLForSingleton(conn, query)
                fp.write('{schema}.{table}:{comp}\n'.format(schema=p[1], table=p[2], comp=compression_rate))


@given('the schemas "{schema_list}" do not exist in "{dbname}"')
@then('the schemas "{schema_list}" do not exist in "{dbname}"')
def impl(context, schema_list, dbname):
    schemas = [s.strip() for s in schema_list.split(',')]
    for s in schemas:
        drop_schema_if_exists(context, s.strip(), dbname)


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


@then(
    'the function-dependent table "{table_name}" and the function "{func_name}" in database "{dbname}" are dropped on the source system')
def impl(context, table_name, func_name, dbname):
    dbconn = 'psql -d %s -p $GPTRANSFER_SOURCE_PORT -U $GPTRANSFER_SOURCE_USER -h $GPTRANSFER_SOURCE_HOST' % dbname
    SQL = """DROP TABLE %s; DROP FUNCTION %s(integer);""" % (table_name, func_name)
    command = '%s -c "%s"' % (dbconn, SQL)
    run_command(context, command)


@then('verify that function "{func_name}" exists in database "{dbname}"')
def impl(context, func_name, dbname):
    SQL = """SELECT proname FROM pg_proc WHERE proname = '%s';""" % func_name
    row_count = getRows(dbname, SQL)[0][0]
    if row_count != 'test_function':
        raise Exception('Function %s does not exist in %s"' % (func_name, dbname))


@when('the user runs the query "{query}" in database "{dbname}" and sends the output to "{filename}"')
def impl(context, query, dbname, filename):
    cmd = "psql -d %s -p $GPTRANSFER_DEST_PORT -U $GPTRANSFER_DEST_USER -c '\copy (%s) to %s'" % (
    dbname, query, filename)
    thread.start_new_thread(run_gpcommand, (context, cmd))
    time.sleep(10)


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


@when('verify that db_dumps directory does not exist in master or segments')
@then('verify that db_dumps directory does not exist in master or segments')
def impl(context):
    check_dump_dir_exists(context, 'template1')


@given('the database "{dbname}" is analyzed')
def impl(context, dbname):
    analyze_database(context, dbname)


@when('the user deletes rows from the table "{table_name}" of database "{dbname}" where "{column_name}" is "{info}"')
@then('the user deletes rows from the table "{table_name}" of database "{dbname}" where "{column_name}" is "{info}"')
def impl(context, dbname, table_name, column_name, info):
    delete_rows_from_table(context, dbname, table_name, column_name, info)


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


@given('the user runs the query "{query}" on "{dbname}" in the background until stopped')
@when('the user runs the query "{query}" on "{dbname}" in the background until stopped')
@then('the user runs the query "{query}" on "{dbname}" in the background until stopped')
def impl(context, query, dbname):
    thread.start_new_thread(execute_sql_until_stopped, (context, dbname, query))


def execute_sql_until_stopped(context, dbname, query):
    with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
        dbconn.execSQL(conn, query)
        conn.commit()
        while True:
            if hasattr(context, 'background_query_lock'):
                break
            time.sleep(1)


@when('the user stops all background queries')
@then('the user stops all background queries')
def impl(context):
    context.background_query_lock = True


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


@given(
    'there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data and {rowcount} rows')
@when(
    'there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data and {rowcount} rows')
@then(
    'there is a "{tabletype}" table "{table_name}" with compression "{compression_type}" in "{dbname}" with data and {rowcount} rows')
def impl(context, tabletype, table_name, compression_type, dbname, rowcount):
    populate_regular_table_data(context, tabletype, table_name, compression_type, dbname, int(rowcount))


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


@when('user kills a mirror process with the saved information')
def impl(context):
    cmdStr = "ps ux | grep '[m]irror process' | grep %s  | awk '{print $2}'" % context.mirror_port
    cmd = Command(name='get mirror pid: %s' % cmdStr, cmdStr=cmdStr)
    cmd.run()
    pid = cmd.get_stdout_lines()[0]
    kill_process(int(pid), context.mirror_segdbname, sig=signal.SIGABRT)


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


@when('run gppersistent_rebuild with the saved content id')
@then('run gppersistent_rebuild with the saved content id')
def impl(context):
    cmdStr = "echo -e 'y\ny\n' | $GPHOME/sbin/gppersistentrebuild -c %s" % context.saved_segcid
    cmd = Command(name='Run gppersistentrebuild', cmdStr=cmdStr)
    cmd.run(validateAfter=True)
    context.ret_code = cmd.get_return_code()


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


@given('a role "{role_name}" is created')
@when('a role "{role_name}" is created')
@then('a role "{role_name}" is created')
def impl(context, role_name):
    with dbconn.connect(dbconn.DbURL(dbname='template1')) as conn:
        pghba = PgHba()
        new_entry = Entry(entry_type='local',
                          database='all',
                          user=role_name,
                          authmethod="password")
        pghba.add_entry(new_entry)
        pghba.write()

        dbconn.execSQL(conn, "Drop role if exists dsp_role")

        dbconn.execSQL(conn, "Create role %s with login password 'dsprolepwd'" % role_name)
        dbconn.execSQL(conn, "select pg_reload_conf()")
        conn.commit()


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
                              And verify that the last line of the file "postgresql.conf" in the master data directory contains the string "gpperfmon_log_alert_level='warning'"
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

@then('store the vacuum timestamp for verification in database "{dbname}"')
def impl(context, dbname):
    sleep(2)
    res = execute_sql_singleton(dbname, 'select last_vacuum from pg_stat_all_tables where last_vacuum is not null order by last_vacuum desc limit 1')
    context.vacuum_timestamp = res

@then('verify that vacuum has been run in database "{dbname}"')
def impl(context, dbname):
    sleep(2)
    res = execute_sql_singleton(dbname, 'select last_vacuum from pg_stat_all_tables where last_vacuum is not null order by last_vacuum desc limit 1')
    if res == context.vacuum_timestamp:
        raise Exception ("Vacuum did not occur as expected. The last_vacuum timestamp %s has not changed" % context.vacuum_timestamp)

@then('the "{utility}" log file should exist under "{directory}"')
def impl(context, utility, directory):
    filepath = glob.glob(os.path.join(directory, utility+"*"))
    if not os.path.isfile(filepath[0]):
        err_str = "The output file '%s' does not exist.\n" % filepath
        raise Exception(err_str)

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
