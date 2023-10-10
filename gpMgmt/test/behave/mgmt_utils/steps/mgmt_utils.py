import math
import fnmatch
import glob
import json
import os
import re
import pipes
import shutil
import socket
import tempfile
import _thread
import time
from subprocess import check_output, Popen, PIPE
import subprocess
from collections import defaultdict

import psutil
from behave import given, when, then
from datetime import datetime, timedelta
from os import path
from contextlib import closing
import psycopg2
from psycopg2 import extras
from gppylib.gparray import GpArray, ROLE_PRIMARY, ROLE_MIRROR
from gppylib.commands.gp import SegmentStart, GpStandbyStart, CoordinatorStop
from gppylib.commands import gp, unix
from gppylib.commands.unix import CMD_CACHE
from gppylib.commands.pg import PgBaseBackup
from gppylib.operations.startSegments import MIRROR_MODE_MIRRORLESS
from gppylib.operations.buildMirrorSegments import get_recovery_progress_pattern
from gppylib.operations.unix import ListRemoteFilesByPattern, CheckRemoteFile
from test.behave_utils.gpfdist_utils.gpfdist_mgmt import Gpfdist
from test.behave_utils.utils import *
from test.behave_utils.cluster_setup import TestCluster, reset_hosts
from test.behave_utils.cluster_expand import Gpexpand
from test.behave_utils.gpexpand_dml import TestDML
from gppylib.commands.base import Command, REMOTE
from gppylib import pgconf
from gppylib.commands.gp import get_coordinatordatadir

coordinator_data_dir = gp.get_coordinatordatadir()
if coordinator_data_dir is None:
    raise Exception('Please set COORDINATOR_DATA_DIRECTORY in environment')

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
    Returns True iff a cluster contains a coordinator, a standby, and three
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
        and coordinator_data_dir.endswith("demoDataDir-1")
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
    os.environ['COORDINATOR_DATA_DIRECTORY'] = "%s/datadirs/qddir/demoDataDir-1" % demoDir

    create_local_demo_cluster(context, with_mirrors='false', with_standby='false', num_primaries=num_primaries)

    context.gpexpand_mirrors_enabled = False


@given('the user runs psql with "{psql_cmd}" against database "{dbname}"')
@when('the user runs psql with "{psql_cmd}" against database "{dbname}"')
@then('the user runs psql with "{psql_cmd}" against database "{dbname}"')
def impl(context, dbname, psql_cmd):
    cmd = "psql -d %s %s" % (dbname, psql_cmd)

    run_command(context, cmd)

    if context.ret_code != 0:
        raise Exception('%s' % context.error_message)


@given('the user runs sql "{query}" in "{db}" on primary segment with content {contentids}')
@when('the user runs sql "{query}" in "{db}" on primary segment with content {contentids}')
@then('the user runs sql "{query}" in "{db}" on primary segment with content {contentids}')
def impl(context, query, db, contentids):
    content_ids = [int(i) for i in contentids.split(',')]

    for content in content_ids:
        host, port = get_primary_segment_host_port_for_content(content)
        psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
            db, host, port, query)
        Command(name='Running Remote command: %s' % psql_cmd, cmdStr=psql_cmd).run(validateAfter=True)


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
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        sql = ("create writable external table {tabname}(a int) location "
               "('gpfdist://host.invalid:8000/file') format 'text'").format(tabname=tabname)
        dbconn.execSQL(conn, sql)

@given('the user create an external table with name "{tabname}" in partition table t')
def impl(conetxt, tabname):
    dbname = 'gptest'
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        sql = ("create external table {tabname}(i int, j int) location "
               "('gpfdist://host.invalid:8000/file') format 'text'").format(tabname=tabname)
        dbconn.execSQL(conn, sql)
        sql = "create table t(i int, j int) partition by list(i) (values(2018), values(1218))"
        dbconn.execSQL(conn, sql)
        sql = ("alter table t exchange partition for (2018) with table {tabname} without validation").format(tabname=tabname)
        dbconn.execSQL(conn, sql)
        conn.commit()

@given('the user create a partition table with name "{tabname}"')
def impl(conetxt, tabname):
    dbname = 'gptest'
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        sql = "create table {tabname}(i int) partition by range(i) (start(0) end(10001) every(1000)) distributed by (i)".format(tabname=tabname)
        dbconn.execSQL(conn, sql)
        sql = "INSERT INTO {tabname} SELECT generate_series(1, 10000)".format(tabname=tabname)
        dbconn.execSQL(conn, sql)
        conn.commit()


@given('the user executes "{sql}" with named connection "{cname}"')
def impl(context, cname, sql):
    conn = context.named_conns[cname]
    dbconn.execSQL(conn, sql)


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
            check_stdout_msg(context, "Coordinator value: %s" % checksum_toggle)
            check_stdout_msg(context, "Segment     value: %s" % checksum_toggle)
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
    segment_hostlist = sorted(gparray.get_hostlist(includeCoordinator=False))
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


@given('"{env_var}" environment variable is not set')
def impl(context, env_var):
    if not hasattr(context, 'orig_env'):
        context.orig_env = dict()
    context.orig_env[env_var] = os.environ.get(env_var)

    if env_var in os.environ:
        del os.environ[env_var]

@given('the environment variable "{env_var}" is set to "{val}"')
def impl(context, env_var, val):
    if not hasattr(context, 'orig_env'):
        context.orig_env = dict()

    context.orig_env[env_var] = os.environ.get(env_var)
    os.environ[env_var] = val

@then('"{env_var}" environment variable should be restored')
def impl(context, env_var):
    if not hasattr(context, 'orig_env'):
        raise Exception('%s can not be reset' % env_var)

    if env_var not in context.orig_env:
        raise Exception('%s can not be reset.' % env_var)

    if context.orig_env[env_var] is None:
        del os.environ[env_var]
    else:
        os.environ[env_var] = context.orig_env[env_var]

    del context.orig_env[env_var]


@given('all files in pg_wal directory are deleted from data directory of preferred primary of content {content_ids}')
def impl(context, content_ids):
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentPreferredRole() == ROLE_PRIMARY and
                      seg.getSegmentContentId() in [int(c) for c in content_ids.split(',')], all_segments)
    for seg in segments:
        cmd = Command(name="Remove pg_wal files",
                      cmdStr='rm -rf {}'.format(os.path.join(seg.getSegmentDataDirectory(), 'pg_wal')),
                      remoteHost=seg.getSegmentHostName(), ctxt=REMOTE)
        cmd.run(validateAfter=True)


@given('the user {action} the walsender on the {segment} on content {content_ids}')
@when('the user {action} the walsender on the {segment} on content {content_ids}')
@then('the user {action} the walsender on the {segment} on content {content_ids}')
def impl(context, action, segment, content_ids):
    if segment == 'mirror':
        role = "'m'"
    elif segment == 'primary':
        role = "'p'"
    else:
        raise Exception('segment role can only be primary or mirror')

    create_fault_query = "CREATE EXTENSION IF NOT EXISTS gp_inject_fault;"
    execute_sql('postgres', create_fault_query)

    inject_fault_query = "SELECT gp_inject_fault_infinite('wal_sender_loop', '%s', dbid) FROM gp_segment_configuration WHERE content IN (%s) AND role=%s;" % (action, content_ids, role)
    execute_sql('postgres', inject_fault_query)
    return


@given('the user skips walreceiver flushing on the {segment} on content {content}')
@then('the user skips walreceiver flushing on the {segment} on content {content}')
def impl(context, segment, content):
    if segment == 'mirror':
        role = "'m'"
    elif segment == 'primary':
        role = "'p'"
    else:
        raise Exception('segment role can only be primary or mirror')

    create_fault_query = "CREATE EXTENSION IF NOT EXISTS gp_inject_fault;"
    execute_sql('postgres', create_fault_query)

    inject_fault_query = "SELECT gp_inject_fault_infinite('walrecv_skip_flush', 'skip', dbid) FROM gp_segment_configuration WHERE content=%s AND role=%s;" % (content, role)
    execute_sql('postgres', inject_fault_query)
    return


@given('the user waits until all bytes are sent to mirror on content {content}')
@then('the user waits until all bytes are sent to mirror on content {content}')
def impl(context, content):
    host, port = get_primary_segment_host_port_for_content(content)
    query = "SELECT pg_current_wal_lsn() - sent_lsn FROM pg_stat_replication;"
    desired_result = 0
    dburl = dbconn.DbURL(hostname=host, port=port, dbname='template1')
    wait_for_desired_query_result(dburl, query, desired_result, utility=True)

    # Wait for replication state to be in 'sync'
    query = "SELECT sync_state FROM pg_stat_replication;"
    desired_result = 'sync'
    dburl = dbconn.DbURL(hostname=host, port=port, dbname='template1')
    wait_for_desired_query_result(dburl, query, desired_result, utility=True)


@given('the user just waits until recovery_progress.file is created in {logdir}')
@when('the user just waits until recovery_progress.file is created in {logdir}')
@then('the user just waits until recovery_progress.file is created in {logdir}')
def impl(context, logdir):
    attempt = 0
    num_retries = 6000
    log_dir = _get_gpAdminLogs_directory() if logdir == 'gpAdminLogs' else logdir
    recovery_progress_file = '{}/recovery_progress.file'.format(log_dir)
    while attempt < num_retries:
        attempt += 1
        if os.path.exists(recovery_progress_file):
            return
        time.sleep(0.1)
        if attempt == num_retries:
            raise Exception('Timed out after {} retries'.format(num_retries))


@given('the user waits until recovery_progress.file is created in {logdir} and verifies its format')
@when('the user waits until recovery_progress.file is created in {logdir} and verifies its format')
@then('the user waits until recovery_progress.file is created in {logdir} and verifies its format')
def impl(context, logdir):
    attempt = 0
    num_retries = 6000
    log_dir = _get_gpAdminLogs_directory() if logdir == 'gpAdminLogs' else logdir
    recovery_progress_file = '{}/recovery_progress.file'.format(log_dir)
    while attempt < num_retries:
        attempt += 1
        if os.path.exists(recovery_progress_file):
            with open(recovery_progress_file, 'r') as fp:
                context.recovery_lines = fp.readlines()
            for line in context.recovery_lines:
                recovery_type, dbid, progress = line.strip().split(':', 2)
                progress_pattern = re.compile(get_recovery_progress_pattern(recovery_type))
                # TODO: assert progress line in the actual hosts bb/rewind progress file
                if re.search(progress_pattern, progress) and dbid.isdigit() and recovery_type in ['full', 'differential', 'incremental']:
                    return
                else:
                    raise Exception('File present but incorrect format line "{}"'.format(line))
        time.sleep(0.1)
        if attempt == num_retries:
            raise Exception('Timed out after {} retries'.format(num_retries))


@then( 'verify if the {lock_file} directory is present in coordinator_data_directory')
def impl(context, lock_file):
    utility_lock_file = "%s/%s" % (gp.get_coordinatordatadir(), lock_file)
    if not os.path.exists(utility_lock_file):
        raise Exception('{0} directory does not exist'.format(utility_lock_file))
    else:
        return


@then('verify that lines from recovery_progress.file are present in segment progress files in {logdir}')
def impl(context, logdir):
    all_progress_lines_by_dbid = {}
    for line in context.recovery_lines:
        recovery_type, dbid, line_from_combined_progress_file = line.strip().split(':', 2)
        all_progress_lines_by_dbid[int(dbid)] = [recovery_type, line_from_combined_progress_file]

    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()

    log_dir = _get_gpAdminLogs_directory() if logdir == 'gpAdminLogs' else logdir
    for seg in all_segments:
        seg_dbid = seg.getSegmentDbId()
        if seg_dbid in all_progress_lines_by_dbid:
            recovery_type, line_from_combined_progress_file = all_progress_lines_by_dbid[seg_dbid]
            if recovery_type == "full":
                process_name = 'pg_basebackup'
            elif recovery_type == "differential":
                process_name = 'rsync'
            else:
                process_name = 'pg_rewind'

            seg_progress_file = '{}/{}.*.dbid{}.out'.format(log_dir, process_name, seg_dbid)
            check_cmd_str = 'grep "{}" {}'.format(line_from_combined_progress_file, seg_progress_file)
            check_cmd = Command(name='check line in segment progress file',
                          cmdStr=check_cmd_str,
                          ctxt=REMOTE,
                          remoteHost=seg.getSegmentHostName())
            check_cmd.run()
            if check_cmd.get_return_code() != 0:
                raise Exception('Expected line {} in segment progress file {} on host {} but not found.'
                                .format(line_from_combined_progress_file, seg_progress_file, seg.getSegmentHostName()))


@then('recovery_progress.file should not exist in {logdir}')
def impl(context, logdir):
    log_dir = _get_gpAdminLogs_directory() if logdir == 'gpAdminLogs' else logdir
    if os.path.exists('{}/recovery_progress.file'.format(log_dir)):
        raise Exception('recovery_progress.file is still present under {}'.format(log_dir))

def backup_bashrc():
    home_dir = os.environ.get('HOME')
    file = home_dir + '/.bashrc'
    backup_fle = home_dir + '/.bashrc.backup'
    if (os.path.isfile(file)):
        command = "cp -f %s %s" % (file, backup_fle)
        result = run_cmd(command)
        if (result[0] != 0):
            raise Exception("Error while backing up bashrc file. STDERR:%s" % (result[2]))
    return



def restore_bashrc():
    home_dir = os.environ.get('HOME')
    file = home_dir + '/.bashrc'
    backup_fle = home_dir + '/.bashrc.backup'
    if (os.path.isfile(backup_fle)):
        command = "mv -f %s %s" % (backup_fle, file)
    else:
        command = "rm -f %s" % (file)
    result = run_cmd(command)
    if (result[0] != 0):
        raise Exception("Error while restoring up bashrc file. STDERR:%s" % (result[2]))


@given('the user runs "{command}"')
@when('the user runs "{command}"')
@then('the user runs "{command}"')
def impl(context, command):
    run_gpcommand(context, command)


@when('the user sets banner on host')
def impl(context):
    file = '~/.bashrc'
    command = "echo 'echo \"banner test\"' >> %s; source %s" % (file, file)
    result = run_cmd(command)
    if(result[0] != 0):
        raise Exception("Error while updating the bashrc file:%s. STDERR:"% (file))

@when('the user sets multi-line banner on host')
def impl(context):
    file = '~/.bashrc'
    command = "echo 'echo -e \"banner test1\\nbanner test2\\nbanner test-3\\nbanner test4\"' >> %s; source %s" % (file, file)
    result = run_cmd(command)
    if(result[0] != 0):
        raise Exception("Error while updating the bashrc file:%s. STDERR:"% (file))

@when('the user sets banner with separator token on host')
def impl(context):
    file = '~/.bashrc'
    token = 'GP_DELIMITER_FOR_IGNORING_BASH_BANNER'
    command = "echo 'echo -e \"banner test1\\nbanner %s test2\\nbanner test-3\\nbanner test4\\nbanner test5 %s\"' >> %s; source %s" % (token, token, file, file)
    result = run_cmd(command)
    if(result[0] != 0):
        raise Exception("Error while updating the bashrc file:%s. STDERR:"% (file))

@given('source gp_bash_functions and run simple echo')
@then('source gp_bash_functions and run simple echo')
@when('source gp_bash_functions and run simple echo')
def impl(context):
    gp_bash_functions = os.getenv("GPHOME") + '/bin/lib/gp_bash_functions.sh'
    message = 'Hello World. This is a simple command output'
    command = "source %s; REMOTE_EXECUTE_AND_GET_OUTPUT localhost \"echo %s\"" %(gp_bash_functions, message)
    result = run_cmd(command)
    if(result[0] != 0):
        raise Exception ("Expected error code is 0. Command returned error code:%s.\nStderr:%s\n" % (result[0], result[2]))
    if(result[1].strip() != message):
        raise Exception ("Expected output is: [%s] while received output is: [%s] Return code:%s" %(message, result[1], result[0]))

@given('source gp_bash_functions and run complex command')
@then('source gp_bash_functions and run complex command')
@when('source gp_bash_functions and run complex command')
def impl(context):
    gp_bash_functions = os.getenv("GPHOME") + '/bin/lib/gp_bash_functions.sh'
    message = 'Hello World. This is a simple command output'
    command = "source %s; REMOTE_EXECUTE_AND_GET_OUTPUT localhost \"echo %s; hostname | wc -w | xargs\"" %(gp_bash_functions, message)
    result = run_cmd(command)
    if(result[0] != 0):
        raise Exception ("Expected error code is 0. Command returned error code:%s.\nStderr:%s\n" % (result[0], result[2]))

    message = message + "\n1"
    if(result[1].strip() != message):
        raise Exception ("Expected output is: [%s] while received output is:[%s] Return code:%s" %(message, result[1], result[0]))

@given('source gp_bash_functions and run echo with separator token')
@then('source gp_bash_functions and run echo with separator token')
@when('source gp_bash_functions and run echo with separator token')
def impl(context):
    gp_bash_functions = os.getenv("GPHOME") + '/bin/lib/gp_bash_functions.sh'
    message = 'Hello World. This is a simple command output'
    token = 'GP_DELIMITER_FOR_IGNORING_BASH_BANNER'
    command = "source %s; REMOTE_EXECUTE_AND_GET_OUTPUT localhost \"echo %s; echo %s; echo %s\"" %(gp_bash_functions, message, token, message)
    result = run_cmd(command)
    if(result[0] != 0):
        raise Exception ("Expected error code is 0. Command returned error code:%s.\nStderr:%s\n" % (result[0], result[2]))

    message = "%s\n%s\n%s" %(message, token, message)
    if(result[1].strip() != message):
        raise Exception ("Expected output is: [%s] while received output is:[%s] Return code:%s" %(message, result[1], result[0]))

@given('the user asynchronously sets up to end {process_name} process in {secs} seconds')
@when('the user asynchronously sets up to end {process_name} process in {secs} seconds')
def impl(context, process_name, secs):
    if process_name == 'that':
        command = "sleep %d; kill -9 %d" % (int(secs), context.asyncproc.pid)
    else:
        command = "sleep %d; ps ux | grep %s | awk '{print $2}' | xargs kill" % (int(secs), process_name)
    run_async_command(context, command)

@when('the user asynchronously sets up to end {process_name} process when {log_msg} is printed in the logs')
def impl(context, process_name, log_msg):
    command = "while sleep 0.1; " \
              "do if grep -E --quiet %s  ~/gpAdminLogs/%s*log ; " \
              "then ps ux | grep bin/%s |awk '{print $2}' | xargs kill ;break 2; " \
              "fi; done" % (log_msg, process_name, process_name)
    run_async_command(context, command)

@then('the user asynchronously sets up to end {kill_process_name} process when {log_msg} is printed in the {logfile_name} logs')
def impl(context, kill_process_name, log_msg, logfile_name):
    command = "while sleep 0.1; " \
              "do if grep -E --quiet %s  ~/gpAdminLogs/%s*log ; " \
              "then ps ux | grep bin/%s |awk '{print $2}' | xargs kill -2 ;break 2; " \
              "fi; done" % (log_msg, logfile_name, kill_process_name)
    run_async_command(context, command)

@given('the user asynchronously sets up to end {process_name} process with {signal_name}')
@when('the user asynchronously sets up to end {process_name} process with {signal_name}')
@then('the user asynchronously sets up to end {process_name} process with {signal_name}')
def impl(context, process_name, signal_name):
    try:
        sig = getattr(signal, signal_name)
    except:
        raise Exception("Unknown signal: {0}".format(signal_name))

    command = "ps ux | grep bin/{0} | awk '{{print $2}}' | xargs kill -{1}".format(process_name, sig.value)
    run_async_command(context, command)

@when('the user asynchronously sets up to end gpcreateseg process when it starts')
def impl(context):
    # We keep trying to find the gpcreateseg process using ps,grep
    # and when we find it, we want to kill it only after the trap for ERROR_EXIT is setup (hence the sleep 1)
    command = """timeout 10m
    bash -c "while sleep 0.1;
    do if ps ux | grep [g]pcreateseg ;
    then sleep 1 && ps ux | grep [g]pcreateseg |awk '{print \$2}' | xargs kill ;
    break 2; fi; done" """
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


@when('the user waits until saved async process is completed')
@then('the user waits until saved async process is completed')
def impl(context):
    context.asyncproc.communicate2()


@when('the user waits until {process_name} process is completed')
def impl(context, process_name):
    wait_process_command = "while ps ux | grep %s | grep -v grep; do sleep 0.1; done;" % process_name
    run_cmd(wait_process_command)

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
                       'export COORDINATOR_DATA_DIRECTORY=%s' % coordinator_data_dir)
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

@then('{command} {state} print "{err_msg}" error message')
def impl(context, command, state, err_msg):
    if state == "should not":
        check_string_not_present_err_msg(context, err_msg)
    elif state == "should":
        check_err_msg(context, err_msg)

@when('{command} should print "{out_msg}" escaped to stdout')
@then('{command} should print "{out_msg}" escaped to stdout')
@then('{command} should print a "{out_msg}" escaped warning')
def impl(context, command, out_msg):
    check_stdout_msg(context, out_msg, True)

@when('{command} should print "{out_msg}" to stdout')
@then('{command} should print "{out_msg}" to stdout')
@then('{command} should print a "{out_msg}" warning')
@when('{command} should print a "{out_msg}" warning')
def impl(context, command, out_msg):
    check_stdout_msg(context, out_msg)


@then('{command} should not print "{out_msg}" to stdout')
def impl(context, command, out_msg):
    check_string_not_present_stdout(context, out_msg)


@then('{command} should print "{out_msg}" to stdout {num} times')
def impl(context, command, out_msg, num):
    msg_list = context.stdout_message.split('\n')
    msg_list = [x.strip() for x in msg_list]

    count = 0
    for line in msg_list:
        if out_msg in line:
            count += 1
    if count != int(num):
        raise Exception("Expected %s to occur %s times. Found %d. stdout: %s" % (out_msg, num, count, msg_list))


@given('the user records the current timestamp in log_timestamp table')
@when('the user records the current timestamp in log_timestamp table')
@then('the user records the current timestamp in log_timestamp table')
def impl(context):
    sql = "CREATE TABLE log_timestamp AS SELECT CURRENT_TIMESTAMP;"
    rc, output, error = run_cmd("psql -d template1 -c \'%s\'" %sql)
    if rc:
        raise Exception(error)


@then('the user drops log_timestamp table')
def impl(context):
    rc, output, error = run_cmd("psql -d template1 -c \"DROP TABLE log_timestamp;\"")
    if rc:
        raise Exception(error)


@then('the pg_log files on primary segments should not contain "{msg}"')
def impl(context, msg):

    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    conn = dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)

    for seg in segments:
        if seg.isSegmentPrimary():
            segname = "seg"+str(seg.content)
            sql = "select * from gp_toolkit.__gp_log_segment_ext where logsegment='%s' and logtime > (select * from log_timestamp) and logmessage like '%s'" %(segname, msg)
            try:
                cursor = dbconn.query(conn, sql)
                if cursor.fetchone():
                    raise Exception("Fatal message exists in pg_log file on primary segment %s" %segname)
            finally:
                pass
    conn.close()


def lines_matching_both(in_str, str_1, str_2):
    lines = [x.strip() for x in in_str.split('\n')]
    return [line for line in lines if line.count(str_1) and line.count(str_2)]


@then('check if {command} ran "{called_command}" {num} times with args "{args}"')
def impl(context, command, called_command, num, args):
    run_cmd_out = "Running Command: %s" % called_command
    matches = lines_matching_both(context.stdout_message, run_cmd_out, args)

    if len(matches) != int(num):
        raise Exception("Expected %s to occur with %s args %s times. Found %d. \n %s"
                        % (called_command, args, num, len(matches), context.stdout_message))


@then('{command} should only spawn up to {num} workers in WorkerPool')
def impl(context, command, num):
    workerPool_out = "WorkerPool() initialized with"
    matches = lines_matching_both(context.stdout_message, workerPool_out, command)

    for matched_line in matches:
        iw_re = re.search('initialized with (\d+) workers', matched_line)
        init_workers = int(iw_re.group(1))
        if init_workers > int(num):
            raise Exception("Expected Workerpool for %s to be initialized with %d workers. Found %d. \n %s"
                            % (command, num, init_workers, context.stdout_message))


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


@then('the segments are synchronized for content {content_ids}')
def impl(context, content_ids):
    if content_ids == 'None':
        return
    times = 60
    sleeptime = 10
    content_ids_to_check = [int(c) for c in content_ids.split(',')]
    for i in range(times):
        if are_segments_synchronized_for_content_ids(content_ids_to_check):
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
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)
    try:
        rowcount = dbconn.querySingleton(conn, "SELECT count(*) FROM %s" % tablename)
        if rowcount != int(numrows):
            raise Exception("Expected to find %d rows in table %s, found %d" % (int(numrows), tablename, rowcount))
    finally:
        conn.close()

@then(
    'data for partition table "{table_name}" with leaf partition distributed across all segments on "{dbname}"')
def impl(context, table_name, dbname):
    validate_part_table_data_on_segments(context, table_name, dbname)

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
    conn = dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)
    try:
        sql = """SELECT datname FROM pg_database"""
        dbs = dbconn.query(conn, sql)
        if dbname in dbs:
            raise Exception('Database exists when it shouldnt "%s"' % dbname)
    finally:
        conn.close()


@given('the file "{filepath}" exists under coordinator data directory')
def impl(context, filepath):
    fullfilepath = os.path.join(coordinator_data_dir, filepath)
    if not os.path.isdir(os.path.dirname(fullfilepath)):
        os.makedirs(os.path.dirname(fullfilepath))
    open(fullfilepath, 'a').close()

@then('the file "{filepath}" does not exist under standby coordinator data directory')
def impl(context, filepath):
    fullfilepath = os.path.join(context.standby_data_dir, filepath)
    cmd = "ls -al %s" % fullfilepath
    try:
        run_command_remote(context,
                           cmd,
                           context.standby_hostname,
                           os.getenv("GPHOME") + '/greenplum_path.sh',
                           'export COORDINATOR_DATA_DIRECTORY=%s' % context.standby_data_dir,
                           validateAfter=True)
    except:
        pass
    else:
        raise Exception("file '%s' should not exist in standby coordinator data directory" % fullfilepath)

@given('results of the sql "{sql}" db "{dbname}" are stored in the context')
@when( 'results of the sql "{sql}" db "{dbname}" are stored in the context')
def impl(context, sql, dbname):
    context.stored_sql_results = []

    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)
    try:
        curs = dbconn.query(conn, sql)
        context.stored_sql_results = curs.fetchall()
    finally:
        conn.close()

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
            print(context.stored_rows)
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
    standby_coordinator = [seg.getSegmentHostName() for seg in segments if seg.isSegmentStandby()]
    if len(standby_coordinator) > 0:
        return standby_coordinator[0]
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


@when('the user initializes a standby on the same host as coordinator with same port')
def impl(context):
    hostname = get_coordinator_hostname('postgres')[0][0]
    temp_data_dir = tempfile.mkdtemp() + "/standby_datadir"
    run_gpinitstandby(context, hostname, os.environ.get("PGPORT"), temp_data_dir)

@when('the user initializes a standby on the same host as coordinator and the same data directory')
def impl(context):
    hostname = get_coordinator_hostname('postgres')[0][0]
    coordinator_port = int(os.environ.get("PGPORT"))

    cmd = "gpinitstandby -a -s %s -P %d" % (hostname, coordinator_port + 1)
    run_gpcommand(context, cmd)

def init_standby(context, coordinator_hostname, options, segment_hostname):
    remote = (coordinator_hostname != segment_hostname)
    # -n option assumes gpinitstandby already ran and put standby in catalog
    if "-n" not in options:
        if remote:
            context.standby_data_dir = coordinator_data_dir
        else:
            context.standby_data_dir = tempfile.mkdtemp() + "/standby_datadir"
    run_gpinitstandby(context, context.standby_hostname, context.standby_port, context.standby_data_dir, options,
                      remote)
    context.coordinator_hostname = coordinator_hostname
    context.coordinator_port = os.environ.get("PGPORT")
    context.standby_was_initialized = True

@when('running gpinitstandby on host "{coordinator}" to create a standby on host "{standby}"')
@given('running gpinitstandby on host "{coordinator}" to create a standby on host "{standby}"')
def impl(context, coordinator, standby):
    # XXX This code was cribbed from init_standby and modified to support remote
    # execution.
    context.coordinator_hostname = coordinator
    context.standby_hostname = standby
    context.standby_port = os.environ.get("PGPORT")
    context.standby_data_dir = coordinator_data_dir

    remove_dir(standby, context.standby_data_dir)
    create_dir(standby, os.path.dirname(context.standby_data_dir))

    # We do not set port nor data dir here to test gpinitstandby's ability to autogather that info
    cmd = "gpinitstandby -a -s %s" % standby

    run_command_remote(context,
                       cmd,
                       context.coordinator_hostname,
                       os.getenv("GPHOME") + '/greenplum_path.sh',
                       'export COORDINATOR_DATA_DIRECTORY=%s' % context.standby_data_dir)

    context.stdout_position = 0
    context.coordinator_port = os.environ.get("PGPORT")
    context.standby_was_initialized = True

def get_standby_variables_and_set_on_context(context):
    dbname = 'postgres'
    with closing(dbconn.connect(dbconn.DbURL(port=os.environ.get("PGPORT"), dbname=dbname), unsetSearchPath=False)) as conn:
        query = """select distinct content, hostname from gp_segment_configuration order by content limit 2;"""
        cursor = dbconn.query(conn, query)

        try:
            _, coordinator_hostname = cursor.fetchone()
            _, segment_hostname = cursor.fetchone()
        except:
            raise Exception("Did not get two rows from query: %s" % query)

    if coordinator_hostname != segment_hostname:
        context.standby_hostname = segment_hostname
        context.standby_port = os.environ.get("PGPORT")
    else:
        context.standby_hostname = coordinator_hostname
        context.standby_port = get_open_port()

    return coordinator_hostname, segment_hostname

@when('the user runs gpinitstandby with options "{options}"')
@then('the user runs gpinitstandby with options "{options}"')
@given('the user runs gpinitstandby with options "{options}"')
def impl(context, options):
    coordinator_hostname, standby_hostname = get_standby_variables_and_set_on_context(context)
    init_standby(context, coordinator_hostname, options, standby_hostname)

def _handle_sigpipe():
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)

@when('the user runs gpinitstandby and {action} the unreachable host prompt')
def impl(context, action):
    coordinator_hostname, standby_hostname = get_standby_variables_and_set_on_context(context)
    remote = (coordinator_hostname != standby_hostname)
    context.coordinator_hostname = coordinator_hostname
    context.coordinator_port = os.environ.get("PGPORT")
    context.standby_was_initialized = True

    if action == "accepts":
        answers = "y\ny\n"
    elif action == "rejects":
        answers = "y\nn\n"
    else:
        raise Exception('Invalid action for the unreachable host prompt (valid options are "accepts" and "rejects"')

    if remote:
        context.standby_data_dir = coordinator_data_dir
        cmd = ["ssh", standby_hostname]
    else:
        context.standby_data_dir = tempfile.mkdtemp() + "/standby_datadir"
        cmd = ["bash", "-c"]

    cmd.append("printf '%s' | gpinitstandby -s %s -S %s -P %s" % (answers, standby_hostname, context.standby_data_dir, context.standby_port))

    p = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=_handle_sigpipe,
    )
    context.stdout_message, context.stderr_message = p.communicate()
    context.ret_code = p.returncode
    if context.ret_code != 0:
        context.error_message = context.stderr_message

@when('the user runs gpactivatestandby with options "{options}"')
@then('the user runs gpactivatestandby with options "{options}"')
def impl(context, options):
    context.execute_steps('''Then the user runs command "gpactivatestandby -a %s" from standby coordinator''' % options)
    context.standby_was_activated = True


@given('the user runs utility "{utility}" with coordinator data directory and "{options}"')
@when('the user runs utility "{utility}" with coordinator data directory and "{options}"')
@then('the user runs utility "{utility}" with coordinator data directory and "{options}"')
def impl(context, utility, options):
    cmd = "{} -d {} {}".format(utility, coordinator_data_dir, options)
    context.execute_steps('''then the user runs command "%s"''' % cmd )


@then('gpintsystem logs should {contain} lines about running backout script')
def impl(context, contain):
    string_to_find = 'Run command bash .*backout_gpinitsystem.* on coordinator to remove these changes$'
    command = "grep -E '{}' ~/gpAdminLogs/gpinitsystem*log".format(string_to_find)
    run_command(context, command)
    if contain == "contain":
        if has_exception(context):
            raise context.exception
        context.gpinit_backout_command = re.search('Run command(.*)on coordinator', context.stdout_message).group(1)
    elif contain == "not contain":
        if not has_exception(context):
            raise Exception("Logs contain lines about running backout script")
    else:
        raise Exception("Incorrect step name, only use 'should contain' and 'should not contain'")

@then('the user runs the gpinitsystem backout script')
def impl(context):
    command = context.gpinit_backout_command
    run_command(context, command)
    if has_exception(context):
        raise context.exception

@when('the user runs command "{command}" from standby coordinator')
@then('the user runs command "{command}" from standby coordinator')
def impl(context, command):
    cmd = "PGPORT=%s %s" % (context.standby_port, command)
    run_command_remote(context,
                       cmd,
                       context.standby_hostname,
                       os.getenv("GPHOME") + '/greenplum_path.sh',
                       'export COORDINATOR_DATA_DIRECTORY=%s' % context.standby_data_dir,
                       validateAfter=False)

@when('the coordinator goes down')
@then('the coordinator goes down')
def impl(context):
    coordinator = CoordinatorStop("Stopping Coordinator", coordinator_data_dir, mode='immediate')
    coordinator.run()

@when('the standby coordinator goes down')
def impl(context):
    coordinator = CoordinatorStop("Stopping Coordinator Standby", context.standby_data_dir, mode='immediate', ctxt=REMOTE,
                        remoteHost=context.standby_hostname)
    coordinator.run(validateAfter=True)

@when('the coordinator goes down on "{host}"')
def impl(context, host):
    coordinator = CoordinatorStop("Stopping Coordinator Standby", coordinator_data_dir, mode='immediate', ctxt=REMOTE,
                        remoteHost=host)
    coordinator.run(validateAfter=True)

@then('clean up and revert back to original coordinator')
def impl(context):
    # TODO: think about preserving the coordinator data directory for debugging
    shutil.rmtree(coordinator_data_dir, ignore_errors=True)

    if context.coordinator_hostname != context.standby_hostname:
        # We do not set port nor data dir here to test gpinitstandby's ability to autogather that info
        cmd = "gpinitstandby -a -s %s" % context.coordinator_hostname
    else:
        cmd = "gpinitstandby -a -s %s -P %s -S %s" % (context.coordinator_hostname, context.coordinator_port, coordinator_data_dir)

    context.execute_steps('''Then the user runs command "%s" from standby coordinator''' % cmd)

    coordinator = CoordinatorStop("Stopping current coordinator", context.standby_data_dir, mode='immediate', ctxt=REMOTE,
                        remoteHost=context.standby_hostname)
    coordinator.run()

    cmd = "gpactivatestandby -a -d %s" % coordinator_data_dir
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
    print("go look in environment.py to see how it uses the path and permissions on context to make sure it's cleaned up")


@when('the user runs pg_controldata against the standby data directory')
def impl(context):
    cmd = "pg_controldata " + context.standby_data_dir
    run_command_remote(context,
                       cmd,
                       context.standby_hostname,
                       os.getenv("GPHOME") + '/greenplum_path.sh',
                       'export COORDINATOR_DATA_DIRECTORY=%s' % context.standby_data_dir)


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
def stop_all_primary_or_mirror_segments(context, segment_type):
    if segment_type not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % segment_type)

    role = ROLE_PRIMARY if segment_type == 'primary' else ROLE_MIRROR
    stop_segments(context, lambda seg: seg.getSegmentRole() == role and seg.content != -1)

@given('user stops all {segment_type} processes on "{hosts}"')
@given('user stops all {segment_type} processes on "{hosts}"')
@given('user stops all {segment_type} processes on "{hosts}"')
def stop_all_primary_or_mirror_segments_on_hosts(context, segment_type, hosts):
    hosts = hosts.split(',')
    if segment_type not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % segment_type)

    role = ROLE_PRIMARY if segment_type == 'primary' else ROLE_MIRROR
    stop_segments(context, lambda seg: seg.getSegmentRole() == role and seg.content != -1 and seg.getSegmentHostName() in hosts)



@given('the {role} on content {contentID} is stopped')
@when('the {role} on content {contentID} is stopped')
@then('the {role} on content {contentID} is stopped')
def stop_segments_on_contentID(context, role, contentID):
    if role not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % role)

    role = ROLE_PRIMARY if role == 'primary' else ROLE_MIRROR
    stop_segments(context, lambda seg: seg.getSegmentRole() == role and seg.content == int(contentID))

@given('the {role} on content {contents} is stopped with the immediate flag')
@when('the {role} on content {contents} is stopped with the immediate flag')
@then('the {role} on content {contents} is stopped with the immediate flag')
def stop_segments_on_contentID(context, role, contents):
    if role not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % role)
    content_ids = [int(i) for i in contents.split(',')]

    role = ROLE_PRIMARY if role == 'primary' else ROLE_MIRROR
    stop_segments_immediate(context, lambda seg: seg.getSegmentRole() == role and seg.content in content_ids)



# where_clause is a lambda that takes a segment to select what segments to stop
def stop_segments(context, where_clause):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    segments = filter(where_clause, gparray.getDbList())
    for seg in segments:
        # For demo_cluster tests that run on the CI gives the error 'bash: pg_ctl: command not found'
        # Thus, need to add pg_ctl to the path when ssh'ing to a demo cluster.
        subprocess.check_call(['ssh', seg.getSegmentHostName(),
                               'source %s/greenplum_path.sh && pg_ctl stop -m fast -D %s -w -t 120' % (
                                   pipes.quote(os.environ.get("GPHOME")), pipes.quote(seg.getSegmentDataDirectory()))
                               ])


@given('user immediately stops all {segment_type} processes for content {content}')
@then('user immediately stops all {segment_type} processes for content {content}')
def stop_all_primary_or_mirror_segments(context, segment_type, content):
    if segment_type not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % segment_type)
    content_ids = [int(i) for i in content.split(',')]
    role = ROLE_PRIMARY if segment_type == 'primary' else ROLE_MIRROR
    stop_segments_immediate(context, lambda seg: seg.getSegmentRole() == role and seg.content in content_ids)


@given('user immediately stops all {segment_type} processes')
@when('user immediately stops all {segment_type} processes')
@then('user immediately stops all {segment_type} processes')
def stop_all_primary_or_mirror_segments(context, segment_type):
    if segment_type not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % segment_type)

    role = ROLE_PRIMARY if segment_type == 'primary' else ROLE_MIRROR
    stop_segments_immediate(context, lambda seg: seg.getSegmentRole() == role and seg.content != -1)

# where_clause is a lambda that takes a segment to select what segments to stop
def stop_segments_immediate(context, where_clause):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    segments = filter(where_clause, gparray.getDbList())
    for seg in segments:
        # For demo_cluster tests that run on the CI gives the error 'bash: pg_ctl: command not found'
        # Thus, need to add pg_ctl to the path when ssh'ing to a demo cluster.
        subprocess.check_call(['ssh', seg.getSegmentHostName(),
                               'source %s/greenplum_path.sh && pg_ctl stop -m immediate -D %s -w' % (
                                   pipes.quote(os.environ.get("GPHOME")), pipes.quote(seg.getSegmentDataDirectory()))
                               ])

@given('user can start transactions')
@when('user can start transactions')
@then('user can start transactions')
def impl(context):
    wait_for_unblocked_transactions(context, 600)

@given('below sql is executed in "{dbname}" db')
@when('below sql is executed in "{dbname}" db')
def impl(context, dbname):
    sql = context.text
    execute_sql(dbname, sql)

@given('sql "{sql}" is executed in "{dbname}" db')
@when('sql "{sql}" is executed in "{dbname}" db')
@then('sql "{sql}" is executed in "{dbname}" db')
def impl(context, sql, dbname):
    execute_sql(dbname, sql)


@when('execute following sql in db "{dbname}" and store result in the context')
def impl(context, dbname):
    context.stored_rows = []

    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        curs = dbconn.query(conn, context.text)
        context.stored_rows = curs.fetchall()


@when('execute sql "{sql}" in db "{dbname}" and store result in the context')
def impl(context, sql, dbname):
    context.stored_rows = []

    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        curs = dbconn.query(conn, sql)
        context.stored_rows = curs.fetchall()


@then('validate that "{message}" is in the stored rows')
def impl(context, message):
    for row in context.stored_rows:
        for column in row:
            if message in column:
                return

    print(context.stored_rows)
    print(message)
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
    return subprocess.getstatusoutput(cmd)


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


@given('verify that mirror on content {content_ids} is {expected_status}')
@when('verify that mirror on content {content_ids} is {expected_status}')
@then('verify that mirror on content {content_ids} is {expected_status}')
def impl(context, content_ids, expected_status):
    if content_ids == 'None':
        return
    if expected_status not in ('up', 'down'):
        raise Exception("expected_status can only be 'up' or 'down'")

    for content in content_ids.split(','):
        if expected_status == 'up' and not is_segment_running(ROLE_MIRROR, int(content)):
            raise Exception("mirror for content {} is not up".format(content))
        elif expected_status == 'down' and is_segment_running(ROLE_MIRROR, int(content)):
            raise Exception("mirror for content {} is not down".format(content))

    return


@given('the cluster configuration has no segments where "{filter}"')
@when('the cluster configuration has no segments where "{filter}"')
@then('the cluster configuration has no segments where "{filter}"')
def impl(context, filter):
    SLEEP_PERIOD = 5
    MAX_DURATION = 300
    MAX_TRIES = MAX_DURATION // SLEEP_PERIOD

    num_tries = 0
    num_matching = 10
    while num_matching and num_tries < MAX_TRIES:
        num_tries += 1
        time.sleep(SLEEP_PERIOD)
        context.execute_steps(u'''
        Given the user runs psql with "-c 'SELECT gp_request_fts_probe_scan()'" against database "postgres"
    ''')
        with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
            sql = "SELECT count(*) FROM gp_segment_configuration WHERE %s" % filter
            num_matching = dbconn.querySingleton(conn, sql)

    if num_matching:
        raise Exception("could not achieve desired state")

    context.execute_steps(u'''
    Given the user runs psql with "-c 'BEGIN; CREATE TEMP TABLE tempt(a int); COMMIT'" against database "postgres"
    ''')


@given('the cluster configuration is saved for "{when}"')
@then('the cluster configuration is saved for "{when}"')
def impl(context, when):
    if not hasattr(context, 'saved_array'):
        context.saved_array = {}
    context.saved_array[when] = GpArray.initFromCatalog(dbconn.DbURL())


@given('we run a sample background script to generate a pid on "{seg}" segment')
@when('we run a sample background script to generate a pid on "{seg}" segment')
@then('we run a sample background script to generate a pid on "{seg}" segment')
def impl(context, seg):
    if seg == "primary":
        if not hasattr(context, 'pseg_hostname'):
            raise Exception("primary seg host is not saved in the context")
        hostname = context.pseg_hostname
    elif seg == "smdw":
        if not hasattr(context, 'standby_host'):
            raise Exception("Standby host is not saved in the context")
        hostname = context.standby_host
    elif seg == "coordinator":
        hostname = get_coordinator_hostname()[0][0]

    filename = os.path.join(os.getcwd(), './test/behave/mgmt_utils/steps/data/pid_background_script.py')

    cmd = Command(name="Remove background script on remote host", cmdStr='rm -f /tmp/pid_background_script.py',
                  remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    cmd = Command(name="Copy background script to remote host", cmdStr='rsync %s %s:/tmp' % (filename, hostname))
    cmd.run(validateAfter=True)

    cmd = Command(name="Run Bg process to save pid",
                  cmdStr='sh -c "/tmp/pid_background_script.py /tmp/bgpid" &>/dev/null &', remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    cmd = Command(name="get Bg process PID",
                  cmdStr='sleep 1; until [ -f /tmp/bgpid ]; do sleep 1; done; cat /tmp/bgpid', remoteHost=hostname, ctxt=REMOTE)
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
    elif seg == "coordinator":
        hostname = get_coordinator_hostname()[0][0]

    cmd = Command(name="killbg pid", cmdStr='kill -9 %s' % context.bg_pid, remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)

    cmd = Command(name="remove pid", cmdStr='rm -rf /tmp/bgpid', remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)


@when('{process} is killed on mirror with content {contentids}')
@then('{process} is killed on mirror with content {contentids}')
def impl(context, process, contentids):
    segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    content_ids = [int(i) for i in contentids.split(',')]
    hosts = set()
    for seg in segments:
        if seg.content in content_ids and seg.role == 'm':
            hosts.add(seg.getSegmentHostName())

    for host in hosts:
        cmd = Command(name="kill process {}".format(process), cmdStr="pkill -9 {}".format(process), remoteHost=host, ctxt=REMOTE)
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

    cpCmd = Command(name='copy pid file to coordinator for editing', cmdStr='rsync %s:%s /tmp' % (hostname, pid_file_orig))

    cpCmd.run(validateAfter=True)

    with open('/tmp/postmaster.pid.orig', 'r') as fr:
        lines = fr.readlines()

    lines[0] = "%s\n" % context.bg_pid

    with open('/tmp/postmaster.pid.orig', 'w') as fw:
        fw.writelines(lines)

    cpCmd = Command(name='copy pid file to segment after editing',
                    cmdStr='rsync /tmp/postmaster.pid.orig %s:%s' % (hostname, pid_file))
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

    cpCmd = Command(name='copy pid file to coordinator for editing', cmdStr='rsync %s:%s /tmp' % (hostname, pid_file))

    cpCmd.run(validateAfter=True)

    # Since Command creates a short-lived SSH session, we observe the PID given
    # a throw-away remote process. Assume that the PID is unused and available on
    # the remote in the near future.
    # This pid is no longer associated with a
    # running process and won't be recycled for long enough that tests
    # have finished.
    cmd = Command(name="get non-existing pid", cmdStr="echo $$", remoteHost=hostname, ctxt=REMOTE)
    cmd.run(validateAfter=True)
    pid = cmd.get_results().stdout.strip()

    with open('/tmp/postmaster.pid', 'r') as fr:
        lines = fr.readlines()

    lines[0] = "%s\n" % pid

    with open('/tmp/postmaster.pid', 'w') as fw:
        fw.writelines(lines)

    cpCmd = Command(name='copy pid file to segment after editing',
                    cmdStr='rsync /tmp/postmaster.pid %s:%s' % (hostname, pid_file))
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

@given('the catalog has a standby coordinator entry')
@then('verify the standby coordinator entries in catalog')
def impl(context):
    check_segment_config_query = "SELECT * FROM gp_segment_configuration WHERE content = -1 AND role = 'm'"
    check_stat_replication_query = "SELECT * FROM pg_stat_replication"
    with closing(dbconn.connect(dbconn.DbURL(dbname='postgres'), unsetSearchPath=False)) as conn:
        segconfig = dbconn.query(conn, check_segment_config_query).fetchall()
        statrep = dbconn.query(conn, check_stat_replication_query).fetchall()

    if len(segconfig) != 1:
        raise Exception("gp_segment_configuration did not have standby coordinator")

    if len(statrep) != 1:
        raise Exception("pg_stat_replication did not have standby coordinator")

    context.standby_dbid = segconfig[0][0]

@then('verify the standby coordinator is now acting as coordinator')
def impl(context):
    check_segment_config_query = "SELECT * FROM gp_segment_configuration WHERE content = -1 AND role = 'p' AND preferred_role = 'p' AND dbid = %s" % context.standby_dbid
    with closing(dbconn.connect(dbconn.DbURL(hostname=context.standby_hostname, dbname='postgres', port=context.standby_port), unsetSearchPath=False)) as conn:
        segconfig = dbconn.query(conn, check_segment_config_query).fetchall()
    if len(segconfig) != 1:
        raise Exception("gp_segment_configuration did not have standby coordinator acting as new coordinator")

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
    _thread.start_new_thread(run_command, (context, cmd))
    time.sleep(10)


@given('the user runs the command "{cmd}" in the background without sleep')
@when('the user runs the command "{cmd}" in the background without sleep')
def impl(context, cmd):
    _thread.start_new_thread(run_command, (context, cmd))


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
    print(contents)
    check_stdout_msg(context, output)

@then('verify that the last line of the file "{filename}" in the coordinator data directory {contain} the string "{output}"{escape}')
def impl(context, filename, contain, output, escape):
    if contain == 'should contain':
        valuesShouldExist = True
    elif contain == 'should not contain':
        valuesShouldExist = False
    else:
        raise Exception("only 'contains' and 'does not contain' are valid inputs")

    find_string_in_coordinator_data_directory(context, filename, output, valuesShouldExist, (escape == ' escaped'))

def find_string_in_coordinator_data_directory(context, filename, output, valuesShouldExist, escapeStr=False):
    contents = ''
    file_path = os.path.join(coordinator_data_dir, filename)

    with open(file_path) as f:
        for line in f:
            contents = line.strip()

    if escapeStr:
        output = re.escape(output)
    pat = re.compile(output)
    if valuesShouldExist and (not pat.search(contents)):
        err_str = "Expected stdout string '%s' and found: '%s'" % (output, contents)
        raise Exception(err_str)
    if (not valuesShouldExist) and pat.search(contents):
        err_str = "Did not expect stdout string '%s' but found: '%s'" % (output, contents)
        raise Exception(err_str)


@given('verify that the file "{filename}" in the coordinator data directory has "{some}" line starting with "{output}"')
@then('verify that the file "{filename}" in the coordinator data directory has "{some}" line starting with "{output}"')
def impl(context, filename, some, output):
    if (some == 'some'):
        valuesShouldExist = True
    elif (some == 'no'):
        valuesShouldExist = False
    else:
        raise Exception("only 'some' and 'no' are valid inputs")
    regexStr = "%s%s" % ("^[\s]*", output)
    pat = re.compile(regexStr)
    file_path = os.path.join(coordinator_data_dir, filename)
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

@given('verify that pg_hba.conf file has "{type}" entries in each segment data directories')
@then('verify that pg_hba.conf file has "{type}" entries in each segment data directories')
def impl(context, type):
    conn = dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)
    try:
        curs = dbconn.query(conn,
                            "SELECT hostname, datadir FROM gp_segment_configuration WHERE role='p' AND content > -1;")
        result = curs.fetchall()
        segment_info = [(result[s][0], result[s][1]) for s in range(len(result))]
    except Exception as e:
        raise Exception("Could not retrieve segment information: %s" % str(e))
    finally:
        conn.close()

    if (type == 'standby'):
        checkForStandby = True
    elif (type == 'replication'):
        checkForStandby = False
    else:
        raise Exception("only 'standby' and 'replication' are valid inputs")

    for info in segment_info:
        host, datadir = info
        filename = 'pg_hba.conf'
        if checkForStandby:
            standby_host = get_standby_host()
            standby_host_address = gp.IfAddrs.list_addrs(standby_host)[0]
            output = "host.*all.*" + standby_host_address + ".*trust"
        else:
            output = "host.*replication.*.*trust"

        filepath = os.path.join(datadir, filename)
        regex = "%s%s" % ("^[%s]*", output)
        cmd_str = 'ssh %s "grep -c %s %s"' % (host, regex, filepath)
        cmd = Command(name='Running remote command: %s' % cmd_str, cmdStr=cmd_str)
        cmd.run(validateAfter=False)
        try:
            val = int(cmd.get_stdout().strip())
            if not val:
                raise Exception(
                    'File %s on host %s does not have %s entry with "%s"(val error: %s)' % (filepath, host, type, output, val))
        except:
            raise Exception('File %s on host %s does not have %s entry start with "%s"(parse error)' % (filepath, host, type, output))

@given('verify that the file "{filename}" in each segment data directory has "{some}" line starting with "{output}"')
@then('verify that the file "{filename}" in each segment data directory has "{some}" line starting with "{output}"')
def impl(context, filename, some, output):
    conn = dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)
    try:
        curs = dbconn.query(conn, "SELECT hostname, datadir FROM gp_segment_configuration WHERE role='p' AND content > -1;")
        result = curs.fetchall()
        segment_info = [(result[s][0], result[s][1]) for s in range(len(result))]
    except Exception as e:
        raise Exception("Could not retrieve segment information: %s" % str(e))
    finally:
        conn.close()

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



@then('verify that the last line of the file "{filename}" in each segment data directory {contain} the string "{output}"')
def impl(context, filename, contain, output):
    if contain == 'should contain':
        valuesShouldExist = True
    elif contain == 'should not contain':
        valuesShouldExist = False
    else:
        raise Exception("only 'should contain' and 'should not contain' are valid inputs")
    segment_info = []
    conn = dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)
    try:
        curs = dbconn.query(conn, "SELECT hostname, datadir FROM gp_segment_configuration WHERE role='p' AND content > -1;")
        result = curs.fetchall()
        segment_info = [(result[s][0], result[s][1]) for s in range(len(result))]
    except Exception as e:
        raise Exception("Could not retrieve segment information: %s" % str(e))
    finally:
        conn.close()

    for info in segment_info:
        host, datadir = info
        filepath = os.path.join(datadir, filename)
        cmd_str = 'ssh %s "tail -n1 %s"' % (host, filepath)
        cmd = Command(name='Running remote command: %s' % cmd_str, cmdStr=cmd_str)
        cmd.run(validateAfter=True)

        actual = cmd.get_stdout()
        if valuesShouldExist and (output not in actual):
                raise Exception('File %s on host %s does not contain "%s"' % (filepath, host, output))
        if (not valuesShouldExist) and (output in actual):
            raise Exception('File %s on host %s contains "%s"' % (filepath, host, output))

@given('verify that the path "{filename}" in each segment data directory does not exist')
@then('verify that the path "{filename}" in each segment data directory does not exist')
def impl(context, filename):
    try:
        with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
            curs = dbconn.query(conn, "SELECT hostname, datadir FROM gp_segment_configuration WHERE content > -1;")
            result = curs.fetchall()
            segment_info = [(result[s][0], result[s][1]) for s in range(len(result))]
    except Exception as e:
        raise Exception("Could not retrieve segment information: %s" % str(e))

    for info in segment_info:
        host, datadir = info
        filepath = os.path.join(datadir, filename)
        cmd_str = 'test -d "%s" && echo 1 || echo 0' % (filepath)
        cmd = Command(name='check exists directory or not',
                      cmdStr=cmd_str,
                      ctxt=REMOTE,
                      remoteHost=host)
        cmd.run(validateAfter=False)
        try:
            val = int(cmd.get_stdout().strip())
            if val:
                raise Exception('Path %s on host %s exists (val %s) (cmd "%s")' % (filepath, host, val, cmd_str))
        except:
            raise Exception('Path %s on host %s exists (cmd "%s")' % (filepath, host, cmd_str))


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

@then('validate gpcheckcat logs contain skipping ACL and Owner tests')
def imp(context):
    dirname = 'gpAdminLogs'
    absdirname = "%s/%s" % (os.path.expanduser("~"), dirname)
    if not os.path.exists(absdirname):
        raise Exception('No such directory: %s' % absdirname)
    pattern = "%s/gpcheckcat_*.log" % (absdirname)
    logs_for_a_util = glob.glob(pattern)
    if not logs_for_a_util:
        raise Exception('Logs matching "%s" were not created' % pattern)
    rc, error, output = run_cmd("grep 'Default skipping test: acl' %s" % pattern)
    if rc:
        raise Exception("Error executing grep on gpcheckcat logs while finding ACL: %s" % error)

    rc, error, output = run_cmd("grep 'Default skipping test: owner' %s" % pattern)
    if rc:
        raise Exception("Error executing grep on gpcheckcat logs while finding Owner: %s" % error)

@then('validate and run gpcheckcat repair')
def impl(context):
    context.execute_steps('''
        Then gpcheckcat should print "repair script\(s\) generated in dir gpcheckcat.repair.*" to stdout
        Then the path "gpcheckcat.repair.*" is found in cwd "1" times
        Then run all the repair scripts in the dir "gpcheckcat.repair.*"
        And the path "gpcheckcat.repair.*" is removed from current working directory
    ''')

@given('there is a "{tabletype}" table "{tablename}" in "{dbname}" with "{numrows}" rows')
def impl(context, tabletype, tablename, dbname, numrows):
    populate_regular_table_data(context, tabletype, tablename, dbname, compression_type=None, with_data=True, rowcount=int(numrows))


@given('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data')
@then('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data')
@when('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data')
def impl(context, tabletype, tablename, dbname):
    populate_regular_table_data(context, tabletype, tablename, dbname, compression_type=None, with_data=True)

@given('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data and description')
@then('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data and description')
@when('there is a "{tabletype}" table "{tablename}" in "{dbname}" with data and description')
def impl(context, tabletype, tablename, dbname):
	populate_regular_table_data(context, tabletype, tablename, dbname, compression_type=None, with_data=True, with_desc=True)


@given('there is a "{tabletype}" partition table "{table_name}" in "{dbname}" with data')
@then('there is a "{tabletype}" partition table "{table_name}" in "{dbname}" with data')
@when('there is a "{tabletype}" partition table "{table_name}" in "{dbname}" with data')
def impl(context, tabletype, table_name, dbname):
    create_partition(context, tablename=table_name, storage_type=tabletype, dbname=dbname, with_data=True)

@given('there is a view without columns in "{dbname}"')
@then('there is a view without columns in "{dbname}"')
@when('there is a view without columns in "{dbname}"')
def impl(context, dbname):
    with dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False) as conn:
        dbconn.execSQL(conn, "create view v_no_cols as select;")
        conn.commit()

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
@when('an attribute of table "{table}" in database "{dbname}" is deleted on segment with content id "{segid}"')
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
ssh %s "source %s; export PGUSER=%s; export PGPORT=%s; export PGOPTIONS=\\\"-c gp_role=utility\\\"; psql -d %s -c \\\"SET allow_system_table_mods=true; DELETE FROM pg_attribute where attrelid=\'%s\'::regclass::oid;\\\""
""" % (host, source_file, user, port, dbname, table)
    run_command(context, remote_cmd.strip())

@when('a table "{table}" in database "{dbname}" has its relnatts inflated on segment with content id "{segid}"')
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
ssh %s "source %s; export PGUSER=%s; export PGPORT=%s; export PGOPTIONS=\\\"-c gp_role=utility\\\"; psql -d %s -c \\\"SET allow_system_table_mods=true; UPDATE pg_class SET relnatts=relnatts + 2 WHERE relname=\'%s\';\\\""
""" % (host, source_file, user, port, dbname, table)
    run_command(context, remote_cmd.strip())


@then('The user runs sql "{query}" in "{dbname}" on first primary segment')
@when('The user runs sql "{query}" in "{dbname}" on first primary segment')
@given('The user runs sql "{query}" in "{dbname}" on first primary segment')
def impl(context, query, dbname):
    host, port = get_primary_segment_host_port()
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
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
        if seg.isSegmentPrimary() or seg.isSegmentCoordinator():
            port = seg.getSegmentPort()
            psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
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
        if seg.isSegmentPrimary() or seg.isSegmentCoordinator():
            port = seg.getSegmentPort()
            psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
            dbname, host, port, query)
            Command(name='Running Remote command: %s' % psql_cmd, cmdStr=psql_cmd).run(validateAfter=True)

@then('The user runs sql "{query}" in "{dbname}" on specified segment {host}:{port} in utility mode')
@when('The user runs sql "{query}" in "{dbname}" on specified segment {host}:{port} in utility mode')
@given('The user runs sql "{query}" in "{dbname}" on specified segment {host}:{port} in utility mode')
def impl(context, query, dbname, host, port):
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
    dbname, host, port, query)
    cmd = Command(name='Running Remote command: %s' % psql_cmd, cmdStr=psql_cmd)
    cmd.run(validateAfter=True)
    context.stdout_message = cmd.get_stdout()

@when('The user runs psql "{psql_cmd}" against database "{dbname}" when utility mode is set to {utility_mode}')
@then('The user runs psql "{psql_cmd}" against database "{dbname}" when utility mode is set to {utility_mode}')
@given('The user runs psql "{psql_cmd}" against database "{dbname}" when utility mode is set to {utility_mode}')
def impl(context, psql_cmd, dbname, utility_mode):
    if eval(utility_mode) == 'True':
        cmd = "PGOPTIONS=\'-c gp_role=utility\' psql -d \'{}\' {};".format(dbname, psql_cmd)
    else:
        cmd = "psql -d \'{}\' {};".format(dbname, psql_cmd)

    run_command(context, cmd)

@then('table {table_name} exists in "{dbname}" on specified segment {host}:{port}')
@when('table {table_name} exists in "{dbname}" on specified segment {host}:{port}')
@given('table {table_name} exists in "{dbname}" on specified segment {host}:{port}')
def impl(context, table_name, dbname, host, port):
    query = "SELECT COUNT(*) FROM pg_class WHERE relname = '%s'" % table_name
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_role=utility\' psql -h %s -p %s -c \"%s\"; " % (
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
    with closing(dbconn.connect(dbconn.DbURL(dbname=db_name), unsetSearchPath=False)) as conn:
        for qry in ["set allow_system_table_mods=true;", "set allow_segment_dml=true;", delete_qry]:
            dbconn.execSQL(conn, qry)

@when('the entry for the table "{user_table}" is removed from "{catalog_table}" with key "{primary_key}" in the database "{db_name}" on the first primary segment')
@given('the entry for the table "{user_table}" is removed from "{catalog_table}" with key "{primary_key}" in the database "{db_name}" on the first primary segment')
def impl(context, user_table, catalog_table, primary_key, db_name):
    host, port = get_primary_segment_host_port()
    delete_qry = "delete from %s where %s='%s'::regclass::oid;" % (catalog_table, primary_key, user_table)

    with closing(dbconn.connect(dbconn.DbURL(dbname=db_name, port=port, hostname=host), utility=True,
                        allowSystemTableMods=True, unsetSearchPath=False)) as conn:
        for qry in [delete_qry]:
            dbconn.execSQL(conn, qry)

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

    with closing(dbconn.connect(dbconn.DbURL(dbname=db_name), unsetSearchPath=False)) as conn:
        dbconn.execSQL(conn, index_qry)

@then('the file with the fake timestamp no longer exists')
def impl(context):
    if os.path.exists(context.fake_timestamp_file):
        raise Exception("expected no file at: %s" % context.fake_timestamp_file)

@given('the user runs command "{command}" on all hosts without validation')
@when('the user runs command "{command}" on all hosts without validation')
@then('the user runs command "{command}" on all hosts without validation')
def impl(context, command):
    hostlist = get_all_hostnames_as_list(context, 'template1')

    for hostname in set(hostlist):
        cmd = Command(name='running command:%s' % command,
                      cmdStr=command,
                      ctxt=REMOTE,
                      remoteHost=hostname)
        cmd.run(validateAfter=False)

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
            'gpsync',
            '-av',
            ] + host_opts + [
            os.getenv('GPHOME')+'/',
            '=:{}'.format(location),
        ])

    finally:
        # Put greenplum_path.sh back the way it was.
        subprocess.check_call([
            'mv', '{}.bak'.format(greenplum_path), greenplum_path
        ])

@given('all files in gpAdminLogs directory are deleted')
@when('all files in gpAdminLogs directory are deleted')
@then('all files in gpAdminLogs directory are deleted')
def impl(context):
    log_dir = _get_gpAdminLogs_directory()
    files_found = glob.glob('%s/*' % (log_dir))
    for file in files_found:
        os.remove(file)


@given('all files in gpAdminLogs directory are deleted on hosts {hosts}')
def impl(context, hosts):
    host_list = hosts.split(',')
    log_dir = _get_gpAdminLogs_directory()
    for host in host_list:
        rm_cmd = Command(name="remove files in gpAdminLogs",
                              cmdStr="rm -rf {}/*".format(log_dir),
                              remoteHost=host, ctxt=REMOTE)
        rm_cmd.run(validateAfter=True)

@given('all files in "{dir}" directory are deleted on all hosts in the cluster')
@then('all files in "{dir}" directory are deleted on all hosts in the cluster')
def impl(context, dir):
    host_list = GpArray.initFromCatalog(dbconn.DbURL()).getHostList()
    for host in host_list:
        rm_cmd = Command(name="remove files in {}".format(dir),
                         cmdStr="rm -rf {}/*".format(dir),
                         remoteHost=host, ctxt=REMOTE)
        rm_cmd.run(validateAfter=True)

@given('all files in gpAdminLogs directory are deleted on all hosts in the cluster')
@when('all files in gpAdminLogs directory are deleted on all hosts in the cluster')
@then('all files in gpAdminLogs directory are deleted on all hosts in the cluster')
def impl(context):
    host_list = GpArray.initFromCatalog(dbconn.DbURL()).getHostList()
    log_dir = _get_gpAdminLogs_directory()
    for host in host_list:
        rm_cmd = Command(name="remove files in gpAdminLogs",
                         cmdStr="rm -rf {}/*".format(log_dir),
                         remoteHost=host, ctxt=REMOTE)
        rm_cmd.run(validateAfter=True)


@then('gpAdminLogs directory {has} "{expected_file}" files')
def impl(context, has, expected_file):
    log_dir = _get_gpAdminLogs_directory()
    files_found = glob.glob('%s/%s' % (log_dir, expected_file))
    if files_found and (has == 'has no'):
        raise Exception("expected no %s files in %s, but found %s" % (expected_file, log_dir, files_found))
    if (not files_found) and (has == 'has'):
        raise Exception("expected %s file in %s, but not found" % (expected_file, log_dir))


@then('gpAdminLogs directory has "{expected_file}" files on respective hosts only for content {content_ids}')
def impl(context, expected_file, content_ids):
    content_list = [int(c) for c in content_ids.split(',')]
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.content in content_list, all_segments)
    host_to_seg_dbids = {}
    for seg in segments:
        segHost = seg.getSegmentHostName()
        if segHost in host_to_seg_dbids:
            host_to_seg_dbids[segHost].append('dbid{}'.format(seg.dbid))
        else:
            host_to_seg_dbids[segHost] = ['dbid{}'.format(seg.dbid)]

    for segHost, expected_files_on_host in host_to_seg_dbids.items():
        log_dir = "%s/gpAdminLogs" % os.path.expanduser("~")
        listdir_cmd = Command(name="list logfiles on host",
                              cmdStr="ls -l {}/{}".format(log_dir, expected_file),
                              remoteHost=segHost, ctxt=REMOTE)
        listdir_cmd.run(validateAfter=True)
        ls_outs = listdir_cmd.get_results().stdout.split('\n')
        files_found = [ls_line.split(' ')[-1] for ls_line in ls_outs if ls_line]

        if not files_found:
            raise Exception("expected {} files in {} on host {}, but not found".format(expected_file, log_dir, segHost))

        if len(files_found) != len(expected_files_on_host):
            raise Exception("expected {} {} files in {} on host {}, but found {}: {}"
                            .format(len(expected_files_on_host), expected_file, log_dir, segHost, len(files_found),
                                    files_found))
        for file in files_found:
            if file.split('.')[-2] not in expected_files_on_host:
                raise Exception("Found unexpected file {} in {}".format(file, log_dir))


@then('gpAdminLogs directory {has} "{expected_file}" files on all segment hosts')
def impl(context, has, expected_file):
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    all_segment_hosts = [seg.getSegmentHostName() for seg in all_segments if seg.getSegmentContentId() >= 0]

    for seg_host in all_segment_hosts:
        log_dir = "%s/gpAdminLogs" % os.path.expanduser("~")
        listdir_cmd = Command(name="list logfiles on host",
                              cmdStr="ls -l {}/{} | wc -l".format(log_dir, expected_file),
                              remoteHost=seg_host, ctxt=REMOTE)
        listdir_cmd.run(validateAfter=True)
        ls_outs = listdir_cmd.get_results().stdout
        files_found = int(ls_outs)
        if files_found > 0 and (has == 'has no'):
            raise Exception("expected no {} files in {} on host {}, but found".format(expected_file, log_dir, seg_host))
        if files_found == 0 and (has == 'has'):
            raise Exception("expected {} files in {} on host {}, but not found".format(expected_file, log_dir, seg_host))


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


@then('{command} should print "{target}" regex to logfile')
def impl(context, command, target):
    log_dir = _get_gpAdminLogs_directory()
    filename = glob.glob('%s/%s_*.log' % (log_dir, command))[0]
    contents = ''
    with open(filename) as fr:
        for line in fr:
            contents += line

    pat = re.compile(target)
    if not pat.search(contents):
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
    logfile = "%s/log/startup.log" % gp.get_coordinatordatadir()
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


def _create_cluster(context, coordinator_host, segment_host_list, hba_hostnames='0', with_mirrors=False, mirroring_configuration='group'):
    if segment_host_list == "":
        segment_host_list = []
    else:
        segment_host_list = segment_host_list.split(",")

    global coordinator_data_dir
    coordinator_data_dir = os.path.join(context.working_directory, 'data/coordinator/gpseg-1')
    os.environ['COORDINATOR_DATA_DIRECTORY'] = coordinator_data_dir

    try:
        with closing(dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)) as conn:
            count = dbconn.querySingleton(conn, "select count(*) from gp_segment_configuration where role='m';")
        if not with_mirrors and count == 0:
            print("Skipping creating a new cluster since the cluster is primary only already.")
            return
        elif with_mirrors and count > 0:
            print("Skipping creating a new cluster since the cluster has mirrors already.")
            return
    except:
        pass

    testcluster = TestCluster(hosts=[coordinator_host]+segment_host_list, base_dir=context.working_directory,hba_hostnames=hba_hostnames)
    testcluster.reset_cluster()
    testcluster.create_cluster(with_mirrors=with_mirrors, mirroring_configuration=mirroring_configuration)
    context.gpexpand_mirrors_enabled = with_mirrors

@then('a cluster is created with no mirrors on "{coordinator_host}" and "{segment_host_list}"')
@given('a cluster is created with no mirrors on "{coordinator_host}" and "{segment_host_list}"')
def impl(context, coordinator_host, segment_host_list):
    _create_cluster(context, coordinator_host, segment_host_list, with_mirrors=False)

@given('with HBA_HOSTNAMES "{hba_hostnames}" a cluster is created with no mirrors on "{coordinator_host}" and "{segment_host_list}"')
@when('with HBA_HOSTNAMES "{hba_hostnames}" a cluster is created with no mirrors on "{coordinator_host}" and "{segment_host_list}"')
@when('with HBA_HOSTNAMES "{hba_hostnames}" a cross-subnet cluster without a standby is created with no mirrors on "{coordinator_host}" and "{segment_host_list}"')
def impl(context, coordinator_host, segment_host_list, hba_hostnames):
    _create_cluster(context, coordinator_host, segment_host_list, hba_hostnames, with_mirrors=False)

@given('a cross-subnet cluster without a standby is created with mirrors on "{coordinator_host}" and "{segment_host_list}"')
@given('a cluster is created with mirrors on "{coordinator_host}" and "{segment_host_list}"')
def impl(context, coordinator_host, segment_host_list):
    _create_cluster(context, coordinator_host, segment_host_list, with_mirrors=True, mirroring_configuration='group')

@given('a cluster is created with "{mirroring_configuration}" segment mirroring on "{coordinator_host}" and "{segment_host_list}"')
def impl(context, mirroring_configuration, coordinator_host, segment_host_list):
    _create_cluster(context, coordinator_host, segment_host_list, with_mirrors=True, mirroring_configuration=mirroring_configuration)

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
    list(map(os.remove, glob.glob("gpexpand_inputfile*")))

@given('there are no gpexpand tablespace input configuration files')
def impl(context):
    list(map(os.remove, glob.glob("{}/*.ts".format(context.working_directory))))
    if len(glob.glob('{}/*.ts'.format(context.working_directory))) != 0:
        raise Exception("expected no gpexpand tablespace input configuration files")

@then('verify if a gpexpand tablespace input configuration file is created')
def impl(context):
    if len(glob.glob('{}/*.ts'.format(context.working_directory))) != 1:
        raise Exception("expected gpexpand tablespace input configuration file to be created")

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
@when('the user runs gpexpand with a static inputfile for a two-node cluster with mirrors')
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

@given('the coordinator pid has been saved')
def impl(context):
    data_dir = os.path.join(context.working_directory,
                            'data/coordinator/gpseg-1')
    context.coordinator_pid = gp.get_postmaster_pid_locally(data_dir)

@then('verify that the coordinator pid has not been changed')
def impl(context):
    data_dir = os.path.join(context.working_directory,
                            'data/coordinator/gpseg-1')
    current_coordinator_pid = gp.get_postmaster_pid_locally(data_dir)
    if context.coordinator_pid == current_coordinator_pid:
        return

    raise Exception("The coordinator pid has been changed.\nprevious: %s\ncurrent: %s" % (context.coordinator_pid, current_coordinator_pid))

@then('the numsegments of table "{tabname}" is {numsegments}')
def impl(context, tabname, numsegments):
    dbname = 'gptest'
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = "select numsegments from gp_distribution_policy where localoid = '{tabname}'::regclass".format(tabname=tabname)
        ns = dbconn.querySingleton(conn, query)
    if ns == int(numsegments):
        return

    raise Exception("The numsegments of the writable external table {tabname} is {ns} (expected to be {numsegments})".format(tabname=tabname,
                                                                                                                             ns=str(ns),
                                                                                                                             numsegments=str(numsegments)))

@given('the number of segments have been saved')
@then('the number of segments have been saved')
def impl(context):
    dbname = 'gptest'
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = """SELECT count(*) from gp_segment_configuration where -1 < content"""
        context.start_data_segments = dbconn.querySingleton(conn, query)

@given('the gp_segment_configuration have been saved')
@when('the gp_segment_configuration have been saved')
@then('the gp_segment_configuration have been saved')
def impl(context):
    dbname = 'gptest'
    gp_segment_conf_backup = {}
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = """SELECT count(*) from gp_segment_configuration where -1 < content"""
        segment_count = int(dbconn.querySingleton(conn, query))
        query = """SELECT * from gp_segment_configuration where -1 < content order by dbid"""
        cursor = dbconn.query(conn, query)
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
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = """SELECT count(*) from gp_segment_configuration where -1 < content"""
        segment_count = int(dbconn.querySingleton(conn, query))
        query = """SELECT * from gp_segment_configuration where -1 < content order by dbid"""
        cursor = dbconn.query(conn, query)
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
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = """CREATE TABLE %s(a INT)""" % table_name
        dbconn.execSQL(conn, query)

@given('a long-run read-only transaction exists on {table_name}')
def impl(context, table_name):
    dbname = 'gptest'
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)
    context.long_run_select_only_conn = conn
    cursor = conn.cursor()
    context.long_run_select_only_cursor = cursor

    # Start a readonly transaction.
    cursor.execute("BEGIN")

    query = """SELECT gp_segment_id, * from %s order by 1, 2""" % table_name
    cursor.execute(query)
    data_result = cursor.fetchall()

    context.long_run_select_only_data_result = data_result

    query = """SELECT txid_current()"""
    cursor.execute(query)
    xid = cursor.fetchone()[0]
    context.long_run_select_only_xid = xid

@then('verify that long-run read-only transaction still exists on {table_name}')
def impl(context, table_name):
    dbname = 'gptest'
    cursor = context.long_run_select_only_cursor

    query = """SELECT gp_segment_id, * from %s order by 1, 2""" % table_name
    cursor.execute(query)
    data_result = cursor.fetchall()

    query = """SELECT txid_current()"""
    cursor.execute(query)
    xid = cursor.fetchone()[0]

    if (xid != context.long_run_select_only_xid or
        data_result != context.long_run_select_only_data_result):
        error_str = "Incorrect xid or select result of long run read-only transaction: \
                xid(before {}, after {}), result(before {}, after {})"
        raise Exception(error_str.format(context.long_run_select_only_xid, xid, context.long_run_select_only_data_result, data_result))

@given('a long-run transaction starts')
def impl(context):
    dbname = 'gptest'
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)
    context.long_run_conn = conn

    cursor = conn.cursor()
    context.long_run_cursor = cursor

    cursor.execute("BEGIN")

    query = """SELECT txid_current()"""
    cursor.execute(query)
    xid = cursor.fetchone()[0]
    context.long_run_xid = xid

@then('verify that long-run transaction aborted for changing the catalog by creating table {table_name}')
def impl(context, table_name):
    cursor = context.long_run_cursor

    query = """SELECT txid_current()"""
    cursor.execute(query)
    xid = cursor.fetchone()[0]
    if context.long_run_xid != xid:
        raise Exception("Incorrect xid of long run transaction: before %s, after %s" %
                        (context.long_run_xid, xid));

    query = """CREATE TABLE %s (a INT)""" % table_name
    try:
        cursor.execute(query)
    except Exception as e:
        key_msg = "cluster is expanded from"
        if key_msg not in str(e):
            raise Exception("transaction not abort correctly, errmsg:%s" % str(e))
    else:
        raise Exception("transaction not abort")

@when('verify that the cluster has {num_of_segments} new segments')
@then('verify that the cluster has {num_of_segments} new segments')
def impl(context, num_of_segments):
    dbname = 'gptest'
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = """SELECT dbid, content, role, preferred_role, mode, status, port, hostname, address, datadir from gp_segment_configuration;"""
        rows = dbconn.query(conn, query).fetchall()
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

@when('verify that {table_name} catalog table is present on new segments')
@then('verify that {table_name} catalog table is present on new segments')
def impl(context, table_name):
    dbname = 'gptest'
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = """SELECT count(*) from gp_segment_configuration where -1 < content and role='p'"""
        no_of_segments = dbconn.querySingleton(conn, query)

        query = """select count(distinct(gp_segment_id)) from gp_dist_random('%s')""" % table_name
        no_segments_table_present = dbconn.querySingleton(conn, query)

    if no_of_segments != no_segments_table_present:
        raise Exception("Table %s is not present on newly expanded segments" % table_name)


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
def make_temp_dir(context, tmp_base_dir, mode=''):
    if not tmp_base_dir:
        raise Exception("tmp_base_dir cannot be empty")
    if not os.path.exists(tmp_base_dir):
        os.mkdir(tmp_base_dir)
    context.temp_base_dir = tempfile.mkdtemp(dir=tmp_base_dir)
    if mode:
        os.chmod(path.normpath(path.join(tmp_base_dir, context.temp_base_dir)),
                 int(mode, 8))

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
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        for i in range(3):
            query = """drop table if exists expansiontest%s""" % (i)
            dbconn.execSQL(conn, query)
            query = """create table expansiontest%s(a int)""" % (i)
            dbconn.execSQL(conn, query)

@then('the tables have finished expanding')
def impl(context):
    dbname = 'postgres'
    conn = dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)
    try:
        query = """select fq_name from gpexpand.status_detail WHERE expansion_finished IS NULL"""
        cursor = dbconn.query(conn, query)

        row = cursor.fetchone()
        if row:
            raise Exception("table %s has not finished expanding" % row[0])
    finally:
        conn.close()

@given('an FTS probe is triggered')
@when('an FTS probe is triggered')
@then('an FTS probe is triggered')
def impl(context):
    with closing(dbconn.connect(dbconn.DbURL(dbname='postgres'), unsetSearchPath=False)) as conn:
        dbconn.querySingleton(conn, "SELECT gp_request_fts_probe_scan()")

@then('verify that gpstart on original coordinator fails due to lower Timeline ID')
def step_impl(context):
    ''' This assumes that gpstart still checks for Timeline ID if a standby coordinator is present '''
    context.execute_steps('''
                            When the user runs "gpstart -a"
                            Then gpstart should return a return code of 2
                            And gpstart should print "Standby activated, this node no more can act as coordinator." to stdout
                            ''')

@then('verify gpstate with options "{options}" output is correct')
def step_impl(context, options):
    if '-f' in options:
        if context.standby_hostname not in context.stdout_message or \
                context.standby_data_dir not in context.stdout_message or \
                str(context.standby_port) not in context.stdout_message:
            raise Exception("gpstate -f output is missing expected standby coordinator information")
    elif '-s' in options:
        if context.standby_hostname not in context.stdout_message or \
                context.standby_data_dir not in context.stdout_message or \
                str(context.standby_port) not in context.stdout_message:
            raise Exception("gpstate -s output is missing expected coordinator information")
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
        conn = dbconn.connect(dbconn.DbURL(hostname=context.standby_hostname, port=context.standby_port, dbname=dbname), unsetSearchPath=False)
        try:
            query = """select datadir, port from pg_catalog.gp_segment_configuration where role='m' and content <> -1;"""
            cursor = dbconn.query(conn, query)

            for i in range(cursor.rowcount):
                datadir, port = cursor.fetchone()
                if datadir not in context.stdout_message or \
                    str(port) not in context.stdout_message:
                        raise Exception("gpstate -m output missing expected mirror info, datadir %s port %d" %(datadir, port))
        finally:
            conn.close()
    else:
        raise Exception("no verification for gpstate option given")

@given('ensure the standby directory does not exist')
def impl(context):
    run_command(context, 'rm -rf $COORDINATOR_DATA_DIRECTORY/newstandby')
    run_command(context, 'rm -rf /tmp/gpinitsystemtest && mkdir /tmp/gpinitsystemtest')

@when('initialize a cluster with standby using "{config_file}"')
def impl(context, config_file):
    run_gpcommand(context, 'gpinitsystem -a -I %s -l /tmp/gpinitsystemtest -s localhost -P 21100 -S $COORDINATOR_DATA_DIRECTORY/newstandby -h ../gpAux/gpdemo/hostfile' % config_file)
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
@given('check segment conf: postgresql.conf')
def step_impl(context):
    query = "select dbid, port, hostname, datadir from gp_segment_configuration where content >= 0"
    conn = dbconn.connect(dbconn.DbURL(dbname='postgres'), unsetSearchPath=False)
    try:
        segments = dbconn.query(conn, query).fetchall()
        for segment in segments:
            dbid = "'%s'" % segment[0]
            port = "'%s'" % segment[1]
            hostname = segment[2]
            datadir = segment[3]

            ## check postgresql.conf
            remote_postgresql_conf = "%s/%s" % (datadir, 'postgresql.conf')
            local_conf_copy = os.path.join(gp.get_coordinatordatadir(), "%s.%s" % ('postgresql.conf', hostname))
            cmd = Command(name="Copy remote conf to local to diff",
                        cmdStr='rsync %s:%s %s' % (hostname, remote_postgresql_conf, local_conf_copy))
            cmd.run(validateAfter=True)

            dic = pgconf.readfile(filename=local_conf_copy)
            if str(dic['port']) != port:
                raise Exception("port value in postgresql.conf of %s is incorrect. Expected:%s, given:%s" %
                                (hostname, port, dic['port']))
    finally:
        if conn:
            conn.close()

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
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        for dml, job in context.dml_jobs:
            code, message = job.reverify(conn)
            if not code:
                raise Exception(message)


@given('the "{table_name}" table row count in "{dbname}" is saved')
def impl(context, table_name, dbname):
    if 'table_row_count' not in context:
        context.table_row_count = {}
    if table_name not in context.table_row_count:
        context.table_row_count[table_name] = _get_row_count_per_segment(table_name, dbname)

@given('distribution information from table "{table}" with data in "{dbname}" is saved')
def impl(context, table, dbname):
    context.pre_redistribution_row_count = _get_row_count_per_segment(table, dbname)
    context.pre_redistribution_dist_policy = _get_dist_policy_per_partition(table, dbname)

@then('distribution information from table "{table}" with data in "{dbname}" is verified against saved data')
def impl(context, table, dbname):
    pre_distribution_row_count = context.pre_redistribution_row_count
    pre_redistribution_dist_policy = context.pre_redistribution_dist_policy
    post_distribution_row_count = _get_row_count_per_segment(table, dbname)
    post_distribution_dist_policy = _get_dist_policy_per_partition(table, dbname)

    if len(pre_distribution_row_count) >= len(post_distribution_row_count):
        raise Exception("Failed to redistribute table. Expected to have more than %d segments, got %d segments" % (len(pre_distribution_row_count), len(post_distribution_row_count)))

    post_distribution_num_segments = 0
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = "SELECT count(DISTINCT content) FROM gp_segment_configuration WHERE content != -1;"
        cursor = dbconn.query(conn, query)
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

    for i in range(len(post_distribution_dist_policy)):
        if(post_distribution_dist_policy[i][0] == pre_redistribution_dist_policy[i][0] or \
           post_distribution_dist_policy[i][1] != pre_redistribution_dist_policy[i][1] or \
           post_distribution_dist_policy[i][2] != pre_redistribution_dist_policy[i][2]):
            raise Exception("""Redistributed policy does not match pre-redistribution policy.
            before expanded: %s, after expanded: %s""" % (",".join(map(str, pre_redistribution_dist_policy[i])), \
            ",".join(map(str, post_distribution_dist_policy[i]))))


@then('the row count from table "{table_name}" in "{dbname}" is verified against the saved data')
def impl(context, table_name, dbname):
    saved_row_count = context.table_row_count[table_name]
    current_row_count = _get_row_count_per_segment(table_name, dbname)

    if saved_row_count != current_row_count:
        raise Exception("%s table in %s has %d rows, expected %d rows." % (table_name, dbname, current_row_count, saved_row_count))


@then('distribution information from table "{table1}" and "{table2}" in "{dbname}" are the same')
def impl(context, table1, table2, dbname):
    distribution_row_count_tbl1 = _get_row_count_per_segment(table1, dbname)
    distribution_row_count_tbl2 = _get_row_count_per_segment(table2, dbname)
    if distribution_row_count_tbl1 != distribution_row_count_tbl2:
        raise Exception("%s and %s have different distribution. Row count of %s is %s and row count of %s is %s" % (table1, table2, table1, distribution_row_count_tbl1, table2, distribution_row_count_tbl2)) 

def _get_row_count_per_segment(table, dbname):
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = "SELECT gp_segment_id,COUNT(*) FROM %s GROUP BY gp_segment_id ORDER BY gp_segment_id;" % table
        cursor = dbconn.query(conn, query)
        rows = cursor.fetchall()
    return [row[1] for row in rows] # indices are the gp segment id's, so no need to store them explicitly

def _get_dist_policy_per_partition(table, dbname):
    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        query = "select * from gp_distribution_policy where localoid::regclass::text like '%s%%' order by localoid;" % table
        cursor = dbconn.query(conn, query)
        rows = cursor.fetchall()
    return [row[2:5] for row in rows] # we only need numsegments、distkey、distclass

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

    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
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
        with closing(dbconn.connect(dbURL, allowSystemTableMods=True, utility=utility, unsetSearchPath=False)) as conn:
            dbconn.execSQL(conn, sql)

@given('the database "{dbname}" is broken with "{broken}" orphaned toast tables')
def impl(context, dbname, broken):
    break_orphaned_toast_tables(context, dbname, broken)

@given('the database "{dbname}" has a table that is orphaned in multiple ways')
def impl(context, dbname):
    drop_database_if_exists(context, dbname)
    create_database(context, dbname)

    coordinator = dbconn.DbURL(dbname=dbname)
    gparray = GpArray.initFromCatalog(coordinator)

    primary0 = gparray.segmentPairs[0].primaryDB
    primary1 = gparray.segmentPairs[1].primaryDB

    seg0 = dbconn.DbURL(dbname=dbname, hostname=primary0.hostname, port=primary0.port)
    seg1 = dbconn.DbURL(dbname=dbname, hostname=primary1.hostname, port=primary1.port)

    with closing(dbconn.connect(coordinator, allowSystemTableMods=True, unsetSearchPath=False)) as conn:
        dbconn.execSQL(conn, """
            DROP TABLE IF EXISTS borked;
            CREATE TABLE borked (a text);
        """)

    with closing(dbconn.connect(seg0, utility=True, allowSystemTableMods=True, unsetSearchPath=False)) as conn:
        dbconn.execSQL(conn, """
            DELETE FROM pg_depend WHERE refobjid = 'borked'::regclass;
        """)

    with closing(dbconn.connect(seg1, utility=True, allowSystemTableMods=True, unsetSearchPath=False)) as conn:
        dbconn.execSQL(conn, """
            UPDATE pg_class SET reltoastrelid = 0 WHERE oid = 'borked'::regclass;
        """)

@then('verify status file and gp_segment_configuration backup file exist on standby')
def impl(context):
    status_file = 'gpexpand.status'
    gp_segment_configuration_backup = 'gpexpand.gp_segment_configuration'

    query = "select hostname, datadir from gp_segment_configuration where content = -1 order by dbid"
    conn = dbconn.connect(dbconn.DbURL(dbname='postgres'), unsetSearchPath=False)
    res = dbconn.query(conn, query).fetchall()
    coordinator = res[0]
    standby = res[1]

    coordinator_datadir = coordinator[1]
    standby_host = standby[0]
    standby_datadir = standby[1]

    standby_remote_statusfile = "%s:%s/%s" % (standby_host, standby_datadir, status_file)
    standby_local_statusfile = "%s/%s.standby" % (coordinator_datadir, status_file)
    standby_remote_gp_segment_configuration_file = "%s:%s/%s" % \
            (standby_host, standby_datadir, gp_segment_configuration_backup)
    standby_local_gp_segment_configuration_file = "%s/%s.standby" % \
            (coordinator_datadir, gp_segment_configuration_backup)

    cmd = Command(name="Copy standby file to coordinator", cmdStr='rsync %s %s' % \
            (standby_remote_statusfile, standby_local_statusfile))
    cmd.run(validateAfter=True)
    cmd = Command(name="Copy standby file to coordinator", cmdStr='rsync %s %s' % \
            (standby_remote_gp_segment_configuration_file, standby_local_gp_segment_configuration_file))
    cmd.run(validateAfter=True)

    if not os.path.exists(standby_local_statusfile):
        raise Exception('file "%s" is not exist' % standby_remote_statusfile)
    if not os.path.exists(standby_local_gp_segment_configuration_file):
        raise Exception('file "%s" is not exist' % standby_remote_gp_segment_configuration_file)


@when('the user runs {command} and selects {input}')
@then('the user runs {command} and selects {input}')
def impl(context, command, input):
    if input == "no mode but presses enter":
        input = os.linesep
    p = Popen(command.split(), stdout=PIPE, stdin=PIPE, stderr=PIPE)
    stdout, stderr = p.communicate(input=input.encode())

    p.stdin.close()

    context.ret_code = p.returncode
    context.stdout_message = stdout.decode()
    context.error_message = stderr.decode()

@when('the user runs {command}, selects {input} and interrupt the process')
def impl(context, command, input):
    p = Popen(command.split(), stdout=PIPE, stdin=PIPE, stderr=PIPE)
    p.stdin.write(input.encode())
    p.stdin.flush()
    time.sleep(120)
    # interrupt the process.
    p.terminate()
    p.communicate(input=input.encode())


def are_on_different_subnets(primary_hostname, mirror_hostname):
    primary_broadcast = check_output(['ssh', '-n', primary_hostname, "/sbin/ip addr show | grep 'inet .* brd' | awk '{ print $4 }'"])
    mirror_broadcast = check_output(['ssh', '-n', mirror_hostname,  "/sbin/ip addr show | grep 'inet .* brd' | awk '{ print $4 }'"])
    if not primary_broadcast:
        raise Exception("primary hostname %s has no broadcast address" % primary_hostname)
    if not mirror_broadcast:
        raise Exception("mirror hostname %s has no broadcast address" % mirror_hostname)

    return primary_broadcast != mirror_broadcast

@then('the primaries and mirrors {including} coordinatorStandby are on different subnets')
def impl(context, including):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())

    if including == "including":
        if not gparray.standbyCoordinator:
            raise Exception("no standby found for coordinator")
        if not are_on_different_subnets(gparray.coordinator.hostname, gparray.standbyCoordinator.hostname):
            raise Exception("coordinator %s and its standby %s are on same the subnet" % (gparray.coordinator, gparray.standbyCoordinator))

    for segPair in gparray.segmentPairs:
        if not segPair.mirrorDB:
            raise Exception("no mirror found for segPair: %s" % segPair)
        if not are_on_different_subnets(segPair.primaryDB.hostname, segPair.mirrorDB.hostname):
            raise Exception("segmentPair on same subnet: %s" % segPair)

@then('content {content} is {desired_state}')
def impl(context, content, desired_state):
    acceptable_states = ["balanced", "unbalanced"]
    if desired_state not in acceptable_states:
        raise Exception("expected desired state to be one of %s", acceptable_states)

    role_operator = "=" if desired_state == "balanced" else "<>"
    with closing(dbconn.connect(dbconn.DbURL(dbname="template1"), unsetSearchPath=False)) as conn:
        rows = dbconn.query(conn, "SELECT role, preferred_role FROM gp_segment_configuration WHERE content = %s and preferred_role %s role" % (content, role_operator)).fetchall()

    if len(rows) == 0:
        raise Exception("Expected content %s to be %s." % (content, desired_state))

@given('the system locale is saved')
def impl(context):
    if "LANG" in os.environ:
        context.system_locale = os.environ["LANG"]
        return

    cmd = Command(name='Get system locale', cmdStr='locale -a | head -1')
    cmd.run(validateAfter=True)
    context.system_locale = cmd.get_stdout()

@then('the database locales are saved')
def impl(context):
    with closing(dbconn.connect(dbconn.DbURL(), cursorFactory=psycopg2.extras.NamedTupleCursor)) as conn:
        rows = dbconn.query(conn, "SELECT name, setting FROM pg_settings WHERE name LIKE 'lc_%'").fetchall()
        context.database_locales = {row.name: row.setting for row in rows}

def check_locales(database_locales, locale_names, expected):
    locale_names = locale_names.split(',')
    for name in locale_names:
        if name not in database_locales:
            raise Exception("Locale %s is invalid" % name)
        locale = database_locales[name]
        if locale != expected:
            raise Exception("Expected %s to be %s, but it was %s" % (name, expected, locale))

def get_en_utf_locale():
    cmd = Command(name='Get installed US UTF locale', cmdStr='locale -a | grep -i "en[_-]..\.utf.*8" | head -1')
    cmd.run(validateAfter=True)
    locale = cmd.get_stdout()
    if locale == "":
        raise Exception("This test requires at least one English UTF-8 locale to be installed on this system")
    return locale

@then('the database locales "{locale_names}" match the locale "{expected}"')
def step_impl(context, locale_names, expected):
    check_locales(context.database_locales, locale_names, expected)

@then('the database locales "{locale_names}" match the system locale')
def step_impl(context, locale_names):
    check_locales(context.database_locales, locale_names, context.system_locale)

@then('the database locales "{locale_names}" match the installed UTF locale')
def step_impl(context, locale_names):
    locale = get_en_utf_locale()
    check_locales(context.database_locales, locale_names, locale)

@when('a demo cluster is created using gpinitsystem args "{args}"')
def impl(context, args):
    context.execute_steps('''
    Given the user runs command "rm -rf ../gpAux/gpdemo/datadirs/*"
      And the user runs command "mkdir ../gpAux/gpdemo/datadirs/qddir; mkdir ../gpAux/gpdemo/datadirs/dbfast1; mkdir ../gpAux/gpdemo/datadirs/dbfast2; mkdir ../gpAux/gpdemo/datadirs/dbfast3"
      And the user runs command "mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror1; mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror2; mkdir ../gpAux/gpdemo/datadirs/dbfast_mirror3"
      And the user runs command "rm -rf /tmp/gpinitsystemtest && mkdir /tmp/gpinitsystemtest"
     When the user runs "gpinitsystem -a %s -c ../gpAux/gpdemo/clusterConfigFile -l /tmp/gpinitsystemtest -P 21100 -h ../gpAux/gpdemo/hostfile"
    ''' % args)

@when('a demo cluster is created using the installed UTF locale')
def impl(context):
    locale = get_en_utf_locale()
    context.execute_steps('''When a demo cluster is created using gpinitsystem args "--lc-ctype=%s"''' % locale)

@given('the user asynchronously runs pg_basebackup with {segment} of content {contentid} as source and the process is saved')
@when('the user asynchronously runs pg_basebackup with {segment} of content {contentid} as source and the process is saved')
@then('the user asynchronously runs pg_basebackup with {segment} of content {contentid} as source and the process is saved')
def impl(context, segment, contentid):
    if segment == 'mirror':
        role = 'm'
    elif segment == 'primary':
        role = 'p'

    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()

    basebackup_target = all_segments[0]
    basebackup_source = all_segments[0]
    for seg in all_segments:
        if role == seg.role and str(seg.content) == contentid:
            basebackup_source = seg
        elif str(seg.content) == contentid:
            basebackup_target = seg

    make_temp_dir(context, '/tmp')

    cmd = PgBaseBackup(target_datadir=context.temp_base_dir,
                       source_host=basebackup_source.getSegmentHostName(),
                       source_port=str(basebackup_source.getSegmentPort()),
                       create_slot=True,
                       replication_slot_name="replication_slot",
                       forceoverwrite=True,
                       target_gp_dbid=basebackup_target.getSegmentDbId())
    asyncproc = cmd.runNoWait()
    context.asyncproc = asyncproc


@given('gp_stat_replication table has pg_basebackup entry for content {contentid}')
@when('gp_stat_replication table has pg_basebackup entry for content {contentid}')
@then('gp_stat_replication table has pg_basebackup entry for content {contentid}')
def impl(context, contentid):
    sql = "select gp_segment_id from gp_stat_replication where application_name = 'pg_basebackup'"

    try:
        with closing(dbconn.connect(dbconn.DbURL())) as conn:
            res = dbconn.query(conn, sql)
            rows = res.fetchall()
    except Exception as e:
        raise Exception("Failed to query gp_stat_replication: %s" % str(e))

    segments_with_running_basebackup = {str(row[0]) for row in rows}

    if str(contentid) not in segments_with_running_basebackup:
        raise Exception("pg_basebackup entry was not found for content %s in gp_stat_replication" % contentid)

@given('create a gpcheckperf input host file')
def impl(context):
    cmd = Command(name='create input host file', cmdStr='echo sdw1 > /tmp/hostfile1;echo cdw >> /tmp/hostfile1;')
    cmd.run(validateAfter=True)

@given('backup /etc/hosts file and update hostname entry for localhost')
def impl(context):
     # Backup current /etc/hosts file
     cmd = Command(name='backup the hosts file', cmdStr='sudo cp /etc/hosts /tmp/hosts_orig')
     cmd.run(validateAfter=True)
     # Get the host-name
     cmd = Command(name='get hostname', cmdStr='hostname')
     cmd.run(validateAfter=True)
     hostname = cmd.get_stdout()
     # Update entry in current /etc/hosts file to add new host-address
     cmd = Command(name='update hostlist with new hostname', cmdStr="sudo sed 's/%s/%s__1 %s/g' </etc/hosts >> /tmp/hosts; sudo cp -f /tmp/hosts /etc/hosts;rm /tmp/hosts"
                                                        %(hostname, hostname, hostname))
     cmd.run(validateAfter=True)

@then('restore /etc/hosts file and cleanup hostlist file')
@when('restore /etc/hosts file and cleanup hostlist file')
def impl(context):
    cmd = "sudo mv -f /tmp/hosts_orig /etc/hosts; rm -f /tmp/clusterConfigFile-1; rm -f /tmp/hostfile--1"
    context.execute_steps(u'''Then the user runs command "%s"''' % cmd)


@given('update /etc/hosts file with address for the localhost')
def impt(context):
    hostname = context.hostname
    # Backup current /etc/hosts file
    cmd = Command(name='backup the hosts file', cmdStr='sudo cp /etc/hosts /tmp/hosts_orig')
    cmd.run(validateAfter=True)
    # Update the address
    cmdStr = "echo \"127.0.0.1 {}\" | sudo tee -a /etc/hosts".format(hostname)
    cmd = Command(name="update /etc/hosts file with hostname entry", cmdStr=cmdStr)
    cmd.run(validateAfter=True)

@given('update hostlist file with updated host-address')
def impl(context):
     cmd = Command(name='get hostname', cmdStr='hostname')
     cmd.run(validateAfter=True)
     hostname = cmd.get_stdout()
     # Update entry in hostfile to replace with address
     cmd = Command(name='update temp hosts file', cmdStr= "sed 's/%s/%s__1/g' < ../gpAux/gpdemo/hostfile >> /tmp/hostfile--1" % (hostname, hostname))
     cmd.run(validateAfter=True)

@given('update clusterConfig file with new port and host-address')
def impl(context):
     cmd = Command(name='get hostname', cmdStr='hostname')
     cmd.run(validateAfter=True)
     hostname = cmd.get_stdout()

     # Create a copy of config file
     cmd = Command(name='create a copy of config file',
                   cmdStr= "cp ../gpAux/gpdemo/clusterConfigFile /tmp/clusterConfigFile-1;")
     cmd.run(validateAfter=True)

     # Update hostfile location
     cmd = Command(name='update coordinator hostname in config file',
                   cmdStr= "sed 's/MACHINE_LIST_FILE=.*/MACHINE_LIST_FILE=\/tmp\/hostfile--1/g' -i /tmp/clusterConfigFile-1")
     cmd.run(validateAfter=True)


@then('verify that cluster config has host-name populated correctly')
def impl(context):
     cmd = Command(name='get hostname', cmdStr='hostname')
     cmd.run(validateAfter=True)
     hostname_orig = cmd.get_stdout().strip()
     hostname_new = "{}__1".format(hostname_orig)
     # Verift host-address not populated in the config
     with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
         sql = "SELECT count(*) FROM gp_segment_configuration WHERE hostname='%s'" % hostname_new
         num_matching = dbconn.querySingleton(conn, sql)
         if(num_matching != 0):
             raise Exception("Found entries in gp_segment_configuration is host-address popoulated as host-name")
     # Verify correct host-name is populated in the config
     with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
         sql = "SELECT count( distinct hostname) FROM gp_segment_configuration WHERE hostname='%s'" % hostname_orig
         num_matching = dbconn.querySingleton(conn, sql)
         if(num_matching != 1):
             raise Exception("Found no entries in gp_segment_configuration is host-address popoulated as host-name")

@given('update the private keys for the new host address')
def impl(context):
     cmd = Command(name='get hostname', cmdStr='hostname')
     cmd.run(validateAfter=True)
     hostname = "{}__1".format(cmd.get_stdout().strip())
     cmd_str = "rm -f ~/.ssh/id_rsa ~/.ssh/id_rsa.pub ~/.ssh/known_hosts; $GPHOME/bin/gpssh-exkeys -h {}".format(hostname)
     cmd = Command(name='update ssh private keys', cmdStr=cmd_str)
     cmd.run(validateAfter=True)

@then('verify replication slot {slot} is available on all the segments')
@when('verify replication slot {slot} is available on all the segments')
@given('verify replication slot {slot} is available on all the segments')
def impl(context, slot):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()
    dbname = "template1"
    query = "SELECT count(*) FROM pg_catalog.pg_replication_slots WHERE slot_name = '{}' and active = 't'".format(slot)

    for seg in segments:
        if seg.isSegmentPrimary(current_role=True):
            host = seg.getSegmentHostName()
            port = seg.getSegmentPort()
            with closing(dbconn.connect(dbconn.DbURL(dbname=dbname, port=port, hostname=host),
                                        utility=True, unsetSearchPath=False)) as conn:
                result = dbconn.querySingleton(conn, query)
                if result == 0:
                    raise Exception("Slot either does not exist or is inactive for host:{}, port:{}".format(host, port))


@given('user waits until gp_stat_replication table has no pg_basebackup entries for content {contentids}')
@when('user waits until gp_stat_replication table has no pg_basebackup entries for content {contentids}')
@then('user waits until gp_stat_replication table has no pg_basebackup entries for content {contentids}')
def impl(context, contentids):
     retries = 600
     content_ids = contentids.split(',')
     content_ids = ', '.join(c for c in content_ids)
     sql = "select count(*) from gp_stat_replication where application_name = 'pg_basebackup' and gp_segment_id in (%s)" %(content_ids)
     no_basebackup = False

     for i in range(retries):
         try:
             with closing(dbconn.connect(dbconn.DbURL())) as conn:
                 res = dbconn.querySingleton(conn, sql)
         except Exception as e:
             raise Exception("Failed to query gp_stat_replication: %s" % str(e))
         if res == 0:
             no_basebackup = True
             break
         time.sleep(1)

     if not no_basebackup:
         raise Exception("pg_basebackup entry was found for contents %s in gp_stat_replication after %d retries" % (contentids, retries))


@given("create event trigger function")
def impl(context):
    dbname = "gptest"
    query = "create or replace function notcie_ddl() returns event_trigger as $$ begin raise notice 'command % is executed.', tg_tag; end $$ language plpgsql"

    with closing(dbconn.connect(dbconn.DbURL(dbname=dbname), unsetSearchPath=False)) as conn:
        dbconn.execSQL(conn, query)


@given('running postgres processes are saved in context')
@when('running postgres processes are saved in context')
@then('running postgres processes are saved in context')
def impl(context):

    # Store the pids in a dictionary where key will be the hostname and the
    # value will be the pids of all the postgres processes running on that host
    host_to_pid_map = dict()
    segs = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    for seg in segs:
        pids = gp.get_postgres_segment_processes(seg.datadir, seg.hostname)
        if seg.hostname not in host_to_pid_map:
            host_to_pid_map[seg.hostname] = pids
        else:
            host_to_pid_map[seg.hostname].extend(pids)

    context.host_to_pid_map = host_to_pid_map


@given('verify no postgres process is running on all hosts')
@when('verify no postgres process is running on all hosts')
@then('verify no postgres process is running on all hosts')
def impl(context):
    host_to_pid_map = context.host_to_pid_map

    for host in host_to_pid_map:
        for pid in host_to_pid_map[host]:
            if unix.check_pid_on_remotehost(pid, host):
                raise Exception("Postgres process {0} not killed on {1}.".format(pid, host))


@then('the database segments are in execute mode')
def impl(context):
    # Get all primary segments details
    # For mirror segments, there's no way to distinguish if in execute mode or utility mode
    with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
        sql = "SELECT dbid, hostname, port  FROM gp_segment_configuration WHERE content > -1 and status = 'u' and role = 'p'"
        rows = dbconn.query(conn, sql).fetchall()

        if len(rows) <= 0:
            raise Exception("Found no entries in gp_segment_configuration table")
    # Check for each segment if the process is in
    for row in rows:
        dbid = row[0]
        hostname = row[1].strip()
        portnum = row[2]
        cmd = "psql -d template1 -p {0} -h {1} -c \";\"".format(portnum, hostname)
        run_command(context, cmd)
        # If node is in execute mode, psql should return value 2 and the print the following error message:
        # For a primary segment: "FATAL:  connections to primary segments are not allowed"
        # For a mirror segment, always prints: "FATAL:  the database system is in recovery mode"
        if context.ret_code == 2 and \
           "FATAL:  connections to primary segments are not allowed" in context.error_message:
            continue
        else:
            raise Exception("segment process not running in execute mode for DBID:{0}".format(dbid))


@when('the user would run "{command}" and terminate the process with SIGINT and selects "{input}" {delay} delay')
def impl(context, command, input, delay):
    p = Popen(command.split(), stdout=PIPE, stdin=PIPE, stderr=PIPE)

    context.execute_steps('''Then verify that pg_basebackup is running for content 0''')
    p.send_signal(signal.SIGINT)

    if delay == "with":
        context.execute_steps('''When the user waits until mirror on content 0 is up''')

    p.stdin.write(input.encode("utf-8"))
    p.stdin.flush()

    if input == "n":
        context.execute_steps('''Then the user reset the walsender on the primary on content 0''')

    stdout, stderr = p.communicate()
    context.ret_code = p.returncode
    context.stdout_message = stdout.decode()
    context.error_message = stderr.decode()


@when('the user would run "{command}" and terminate the process for host "{hostname}" with SIGTERM')
def impl(context, command, hostname):
    p = Popen(command.split(), stdout=PIPE, stdin=PIPE, stderr=PIPE)

    context.execute_steps('''
        Then the user just waits until recovery_progress.file is created in gpAdminLogs
        Then verify that pg_basebackup is running for host {}
        '''.format(hostname))
    p.send_signal(signal.SIGTERM)

    stdout, stderr = p.communicate()
    context.ret_code = p.returncode
    context.stdout_message = stdout.decode()
    context.error_message = stderr.decode()


@given('user creates a new executable rsync script which inserts data into table and runs checkpoint along with doing '
       'rsync')
def impl(context):

    rsync_script = """
cat >/usr/local/bin/rsync <<EOL
#!/usr/bin/env bash

arguments="\$@"

# Insert data into table and run checkpoint just before syncing pg_control
if [[ "\$arguments" == *"pg_wal"* ]]
then
    ssh cdw "psql -c 'INSERT INTO test_recoverseg SELECT generate_series(1, 1000)' -d postgres -p 5432 -h cdw"

    # run checkpoint
    ssh cdw "psql -c "CHECKPOINT" -d postgres -p 5432 -h cdw"
fi

/usr/bin/rsync \$arguments
EOL
"""
    clear_cmd_cache_script = """
cat >/tmp/clear_cmd_cache.py <<EOL
#!/usr/bin/env python3

# clear the cmd cache
global CMD_CACHE
CMD_CACHE = {}
EOL
"""

    with closing(dbconn.connect(dbconn.DbURL(port=os.environ.get("PGPORT")), unsetSearchPath=False)) as conn:
        query = "select distinct hostname from gp_segment_configuration where status='d';"
        result = dbconn.query(conn, query).fetchall()
        host_list = [result[s][0] for s in range(len(result))]
        context.hosts_with_rsync_bash = host_list

    cmd = Command(name='create a file for new rsync script',
                  cmdStr="sudo touch /usr/local/bin/rsync;sudo chmod 777 /usr/local/bin/rsync")
    cmd.run(validateAfter=True)

    cmd = Command(name='update rsync bash script', cmdStr=rsync_script)
    cmd.run(validateAfter=True)

    for host in host_list:
        cmd = Command(name='create a file for new rsync script', cmdStr="sudo touch /usr/local/bin/rsync;sudo chmod 777 /usr/local/bin/rsync", remoteHost=host, ctxt=REMOTE)
        cmd.run(validateAfter=True)

        cmd = Command(name='update rsync bash script', cmdStr="scp /usr/local/bin/rsync {}:/usr/local/bin/rsync".format(host))
        cmd.run(validateAfter=True)

        cmd = Command(name='create script to clear cmd_cache', cmdStr=clear_cmd_cache_script,
                      remoteHost=host, ctxt=REMOTE)
        cmd.run(validateAfter=True)

        cmd = Command(name='run script to clear cmd_cache', cmdStr="chmod +x /tmp/clear_cmd_cache.py;/tmp/clear_cmd_cache.py",
                      remoteHost=host, ctxt=REMOTE)
        cmd.run(validateAfter=True)


@then('the row count of table {table} in "{dbname}" should be {count}')
def impl(context, table, dbname, count):
    current_row_count = _get_row_count_per_segment(table, dbname)
    if int(count) != sum(current_row_count):
        raise Exception(
            "%s table in %s has %d rows, expected %d rows." % (table, dbname, sum(current_row_count), int(count)))

