# coding: utf-8

import os
from collections import defaultdict
from gppylib.db import dbconn
from gppylib.commands.base import Command, REMOTE, WorkerPool
from gppylib.gparray import GpArray
from test.behave_utils.utils import get_all_hostnames_as_list, run_command, run_gpcommand
from gppylib.operations.backup_utils import Context
from datetime import datetime
import json
import yaml

master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')
old_format_indicator = '/tmp/is_old.file'

def _read_timestamp_from_json(context):
    scenario_name = context._stack[0]['scenario'].name
    with open(timestamp_json, 'r') as infile:
        return json.load(infile)[scenario_name]

def parse_netbackup_params():
    current_path = os.path.realpath(__file__)
    current_dir = os.path.dirname(current_path)
    netbackup_yaml_file_path = os.path.join(current_dir, 'data/netbackup_behave_config.yaml')
    try:
        nbufile = open(netbackup_yaml_file_path, 'r')
    except IOError,e:
        raise Exception("Unable to open file %s: %s" % (netbackup_yaml_file_path, e))
    try:
        nbudata = yaml.load(nbufile.read())
    except yaml.YAMLError, exc:
        raise Exception("Error reading file %s: %s" % (netbackup_yaml_file_path, exc))
    finally:
        nbufile.close()

    if len(nbudata) == 0:
        raise Exception("The load of the config file %s failed.\
         No configuration information to continue testing operation." % netbackup_yaml_file_path)
    else:
        return nbudata

@given('the netbackup params have been parsed')
def impl(context):
    NETBACKUPDICT = defaultdict(dict)
    NETBACKUPDICT['NETBACKUPINFO'] = parse_netbackup_params()
    context.netbackup_service_host = NETBACKUPDICT['NETBACKUPINFO']['NETBACKUP_PARAMS']['NETBACKUP_SERVICE_HOST']
    context.netbackup_policy = NETBACKUPDICT['NETBACKUPINFO']['NETBACKUP_PARAMS']['NETBACKUP_POLICY']
    context.netbackup_schedule = NETBACKUPDICT['NETBACKUPINFO']['NETBACKUP_PARAMS']['NETBACKUP_SCHEDULE']


def _copy_nbu_lib_files(context, ver, gphome):
    ver = ver.replace('.', '')
    hosts = set(get_all_hostnames_as_list(context, 'template1'))
    cpCmd = 'cp -f {gphome}/lib/nbu{ver}/lib/* {gphome}/lib/'.format(gphome=gphome,
                                                                     ver=ver)
    for host in hosts:
        cmd = Command(name='Copy NBU lib files',
                cmdStr=cpCmd,
                ctxt=REMOTE,
                remoteHost=host)
        cmd.run(validateAfter=True)

@given('the NetBackup "{ver}" libraries are loaded')
def impl(context, ver):
    gphome = os.environ.get('GPHOME')
    _copy_nbu_lib_files(context=context, ver=ver, gphome=gphome)

@given('the NetBackup "{ver}" libraries are loaded for GPHOME "{gphome}"')
def impl(context, ver, gphome):
    _copy_nbu_lib_files(context=context, ver=ver, gphome=gphome)

@when('the user runs gpcrondump with -k option on database "{dbname}" using netbackup')
def impl(context, dbname):
    datetime_fmt = datetime.now().strftime("%Y%m%d%H%M%S")
    cmd_str = "gpcrondump -a -x %s -k %s --netbackup-block-size 2048" % (dbname, datetime_fmt)
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if hasattr(context, 'netbackup_policy'):
        netbackup_policy = context.netbackup_policy
    if hasattr(context, 'netbackup_schedule'):
        netbackup_schedule = context.netbackup_schedule
    command_str = cmd_str + " --netbackup-service-host " + netbackup_service_host + " --netbackup-policy " + netbackup_policy + " --netbackup-schedule " + netbackup_schedule
    run_gpcommand(context, command_str)

@when('the user runs "{cmd_str}" using netbackup')
def impl(context, cmd_str):
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if hasattr(context, 'netbackup_policy'):
        netbackup_policy = context.netbackup_policy
    if hasattr(context, 'netbackup_schedule'):
        netbackup_schedule = context.netbackup_schedule
    if 'gpcrondump' in cmd_str:
        command_str = cmd_str + " --netbackup-service-host " + netbackup_service_host + " --netbackup-policy " + netbackup_policy + " --netbackup-schedule " + netbackup_schedule
    elif 'gpdbrestore' in cmd_str:
        command_str = cmd_str + " --netbackup-service-host " + netbackup_service_host
    elif 'gp_dump' in cmd_str:
        command_str = cmd_str + " --netbackup-service-host " + netbackup_service_host + " --netbackup-policy " + netbackup_policy + " --netbackup-schedule " + netbackup_schedule
    elif 'gp_restore' in cmd_str:
        command_str = cmd_str + " --netbackup-service-host " + netbackup_service_host

    run_gpcommand(context, command_str)

@when('the user runs backup command "{cmd}" using netbackup')
def impl(context, cmd):
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if hasattr(context, 'netbackup_policy'):
        netbackup_policy = context.netbackup_policy
    if hasattr(context, 'netbackup_schedule'):
        netbackup_schedule = context.netbackup_schedule

    command_str = cmd + " --netbackup-service-host " + netbackup_service_host + " --netbackup-policy " + netbackup_policy + " --netbackup-schedule " + netbackup_schedule
    run_command(context, command_str)

@when('the user runs restore command "{cmd}" using netbackup')
def impl(context, cmd):
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host

    command_str = cmd + " --netbackup-service-host " + netbackup_service_host
    run_command(context, command_str)
@when('the user runs gpdbrestore with the stored json timestamp using netbackup')
@then('the user runs gpdbrestore with the stored json timestamp using netbackup')
def impl(context):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    #context.backup_timestamp = _read_timestamp_from_json(context)[-1]
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gpdbrestore -e -t %s -a --netbackup-service-host %s' % (context.backup_timestamp, netbackup_service_host)
    run_gpcommand(context, command)

@when('the user runs gpdbrestore with the stored json timestamp and options "{options}" using netbackup')
@then('the user runs gpdbrestore with the stored json timestamp and options "{options}" using netbackup')
def impl(context, options):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    #context.backup_timestamp = _read_timestamp_from_json(context)[-1]
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gpdbrestore -e -t %s -a %s --netbackup-service-host %s' % (context.backup_timestamp, options, netbackup_service_host)
    run_gpcommand(context, command)

@when('the user runs gpdbrestore with the stored json timestamp and options "{options}" without -e option using netbackup')
@then('the user runs gpdbrestore with the stored json timestamp and options "{options}" without -e option using netbackup')
def impl(context, options):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    #context.backup_timestamp = _read_timestamp_from_json(context)[-1]
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gpdbrestore -t %s -a %s --netbackup-service-host %s' % (context.backup_timestamp, options, netbackup_service_host)
    run_gpcommand(context, command)

@when('the user runs gpdbrestore with the stored timestamp using netbackup')
def impl(context):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gpdbrestore -e -a -t ' + ts + " --netbackup-service-host " + netbackup_service_host
    run_gpcommand(context, command)

@when('the user runs gpdbrestore with the stored timestamp and options "{options}" using netbackup')
def impl(context, options):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if options == '-b':
        command = 'gpdbrestore -e -b %s -a --netbackup-service-host %s' % (ts[0:8], netbackup_service_host)
    else:
        command = 'gpdbrestore -e -t %s %s -a --netbackup-service-host %s' % (ts, options, netbackup_service_host)
    run_gpcommand(context, command)

@when('the user runs gpdbrestore with the stored timestamp and options "{options}" without -e option using netbackup')
def impl(context, options):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if options == '-b':
        command = 'gpdbrestore -b %s -a --netbackup-service-host %s' % (context.backup_timestamp[0:8], netbackup_service_host)
    else:
        command = 'gpdbrestore -t %s %s -a --netbackup-service-host %s' % (context.backup_timestamp, options, netbackup_service_host)
    run_gpcommand(context, command)

@when('the user runs gpdbrestore with "{opt}" option in path "{path}" using netbackup')
def impl(context, opt, path):
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gpdbrestore -e -a %s localhost:%s/db_dumps/%s --netbackup-service-host %s' % (opt, path, context.backup_subdir, netbackup_service_host)
    run_gpcommand(context, command)

@when('the user runs gp_restore with the the stored timestamp and subdir in "{dbname}" using netbackup')
def impl(context, dbname):
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gp_restore -i --gp-k %s --gp-d db_dumps/%s --gp-i --gp-r db_dumps/%s --gp-l=p -d %s --gp-c --netbackup-service-host %s' % (context.backup_timestamp, context.backup_subdir, context.backup_subdir, dbname, netbackup_service_host)
    run_gpcommand(context, command)

@then('verify that the config files are backed up with the stored timestamp using netbackup')
def impl(context, use_old_format = False):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if not hasattr(context, "dump_prefix"):
        context.dump_prefix = ''

    master_config_filename = os.path.join(master_data_dir, 'db_dumps', context.backup_timestamp[0:8],
                               '%sgp_master_config_files_%s.tar' % (context.dump_prefix, ts))

    command_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, master_config_filename)
    cmd = Command('Validate master config file', command_str)
    cmd.run(validateAfter=True)
    results = cmd.get_results().stdout.strip()
    if results != master_config_filename:
            raise Exception('Expected Master Config file: %s and found: %s. Master Config file was not backup up to NetBackup server' % (master_config_filename, results))

    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]

    for seg in primary_segs:
        first_digit = 0 if use_old_format else seg.getSegmentContentId()
        segment_config_filename = os.path.join(seg.getSegmentDataDirectory(), 'db_dumps', context.backup_timestamp[0:8],
                                           '%sgp_segment_config_files_%d_%s_%s.tar' % (context.dump_prefix, first_digit, seg.getSegmentDbId(), context.backup_timestamp))
        command_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, segment_config_filename)
        cmd = Command('Validate segment config file', command_str, ctxt=REMOTE, remoteHost = seg.getSegmentHostName())
        cmd.run(validateAfter=True)
        results = cmd.get_results().stdout.strip()
        if results != segment_config_filename:
            raise Exception('Expected Segment Config file: %s and found: %s. Segment Config file was not backup up to NetBackup server' % (segment_config_filename, results))

@when('the user runs gpdbrestore with the stored timestamp to print the backup set with options "{options}" using netbackup')
def impl(context, options):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gpdbrestore -t %s %s --list-backup --netbackup-service-host %s' % (ts, options, netbackup_service_host)
    run_gpcommand(context, command)

@when('the user runs gp_restore with the the stored timestamp and subdir in "{dbname}" and bypasses ao stats using netbackup')
def impl(context, dbname):
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gp_restore -i --gp-k %s --gp-d db_dumps/%s --gp-i --gp-r db_dumps/%s --gp-l=p -d %s --gp-c --gp-nostats --netbackup-service-host %s' % (ts, context.backup_subdir, context.backup_subdir, dbname, netbackup_service_host)
    run_gpcommand(context, command)

@when('the user runs gp_restore with the the stored timestamp and subdir for metadata only in "{dbname}" using netbackup')
def impl(context, dbname):
    # only the old-to-new-format netbackup test creates the indicator file
    use_old_format = os.path.isfile(old_format_indicator)
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    basename = "gp_dump_%s_1_" % ("1" if use_old_format else "-1")
    command = 'gp_restore -i --gp-k %s --gp-d db_dumps/%s --gp-i --gp-r db_dumps/%s --gp-l=p -d %s --gp-c -s db_dumps/%s/%s%s.gz --netbackup-service-host %s' % \
                            (ts, context.backup_subdir, context.backup_subdir, dbname, context.backup_subdir, basename, ts, netbackup_service_host)
    if use_old_format:
        command += ' --old-format'
    run_gpcommand(context, command)

@when('the user runs gpdbrestore with the backup list stored timestamp and options "{options}" using netbackup')
def impl(context, options):
    if hasattr(context, 'backup_timestamp_list'):
        ts = context.backup_timestamp_list.pop(0)
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host

    if options == '-b':
        command = 'gpdbrestore -e -b %s -a --netbackup-service-host %s' % (ts[0:8], netbackup_service_host)
    else:
        command = 'gpdbrestore -e -t %s %s -a --netbackup-service-host %s' % (ts, options, netbackup_service_host)
    run_gpcommand(context, command)

@given('verify that {filetype} file has been backed up using nebBackup')
@when('verify that {filetype} file has been backed up using netbackup')
@then('verify that {filetype} file has been backed up using netbackup')
def impl(context, filetype):
    backup_utils = Context()
    if hasattr(context, 'netbackup_service_host'):
        backup_utils.netbackup_service_host = context.netbackup_service_host
    if hasattr(context, 'backup_timestamp'):
        backup_utils.timestamp = context.backup_timestamp
    if hasattr(context, 'backup_dir'):
        backup_utils.backup_dir = context.backup_dir
    else:
        backup_dir = None

    if filetype not in ['config', 'state']:
        filename = backup_utils.generate_filename(filetype)
        cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (backup_utils.netbackup_service_host, filename)
        cmd = Command("Querying NetBackup server for %s file" % filetype, cmd_str)
        cmd.run(validateAfter=True)
        if cmd.get_results().stdout.strip() != filename:
            raise Exception('File %s was not backup up to NetBackup server %s successfully' % (filename, backup_utils.netbackup_service_host))

    if filetype == 'config':
        master_port = os.environ.get('PGPORT')
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port = master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            backup_utils.master_datadir = seg.getSegmentDataDirectory()
            seg_config_filename = backup_utils.generate_filename('segment_config', dbid=seg.getSegmentDbId())
            seg_host = seg.getSegmentHostName()
            cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (backup_utils.netbackup_service_host, seg_config_filename)
            cmd = Command("Querying NetBackup server for segment config file", cmd_str, ctxt=REMOTE, remoteHost=seg_host)
            cmd.run(validateAfter=True)
            if cmd.get_results().stdout.strip() != seg_config_filename:
                raise Exception('Segment config file %s was not backup up to NetBackup server %s successfully' % (seg_config_filename, netbackup_service_host))

    elif filetype == 'state':
        for type in ['ao', 'co', 'last_operation']:
            filename = backup_utils.generate_filename(type)
            cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (backup_utils.netbackup_service_host, filename)
            cmd = Command("Querying NetBackup server for %s file" % type, cmd_str)
            cmd.run(validateAfter=True)
            if cmd.get_results().stdout.strip() != filename:
                raise Exception('The %s file %s was not backup up to NetBackup server %s successfully' % (type, filename, netbackup_service_host))

@when('the user runs the "{cmd}" in a worker pool "{poolname}" using netbackup')
def impl(context, cmd, poolname):
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if hasattr(context, 'netbackup_policy'):
        netbackup_policy = context.netbackup_policy
    if hasattr(context, 'netbackup_schedule'):
        netbackup_schedule = context.netbackup_schedule
    cmd = cmd + " --netbackup-service-host " + netbackup_service_host + " --netbackup-policy " + netbackup_policy + " --netbackup-schedule " + netbackup_schedule
    command = Command(name='run command in a separate thread', cmdStr=cmd)
    pool = WorkerPool(numWorkers=1)
    pool.addCommand(command)
    if not hasattr(context, 'pool'):
        context.pool = {}
    context.pool[poolname] = pool
    context.cmd = cmd

@then('the timestamps for database dumps are stored in a list')
def impl(context):
    context.ts_list = get_timestamps_from_output(context)

def get_timestamps_from_output(context):
    ts_list = []
    stdout = context.stdout_message
    for line in stdout.splitlines():
        if 'Timestamp key = ' in line:
            log_msg, delim, timestamp = line.partition('=')
            ts = timestamp.strip()
            validate_timestamp(ts)
            ts_list.append(ts)

    if ts_list is not []:
        return ts_list
    else:
        raise Exception('Timestamp not found %s' % stdout)

@when('the user runs gpdbrestore for the database "{dbname}" with the stored timestamp using netbackup')
def impl(context, dbname):
    if hasattr(context, 'db_timestamps'):
        db_timestamps = context.db_timestamps
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host

    ts = db_timestamps[dbname]

    command = 'gpdbrestore -e -a -t ' + ts + " --netbackup-service-host " + netbackup_service_host
    run_gpcommand(context, command)

@given('verify that {filetype} file with prefix "{prefix}" under subdir "{subdir}" has been backed up using netbackup')
@when('verify that {filetype} file with prefix "{prefix}" under subdir "{subdir}" has been backed up using netbackup')
@then('verify that {filetype} file with prefix "{prefix}" under subdir "{subdir}" has been backed up using netbackup')
def impl(context, filetype, prefix, subdir):
    # only the old-to-new-format netbackup test creates the indicator file
    use_old_format = os.path.isfile(old_format_indicator)
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if hasattr(context, 'backup_timestamp'):
        backup_timestamp = context.backup_timestamp
    subdir = subdir.strip()
    if len(subdir) > 0:
        dump_dir = os.path.join(subdir, 'db_dumps', '%s' % (backup_timestamp[0:8]))
    else:
        dump_dir = os.path.join(master_data_dir, 'db_dumps', '%s' % (backup_timestamp[0:8]))

    prefix = prefix.strip()
    if len(prefix) > 0:
        prefix = prefix + '_'

    if filetype == 'report':
        filename =  "%s/%sgp_dump_%s.rpt" % (dump_dir, prefix, backup_timestamp)
        cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, filename)
        cmd = Command("Querying NetBackup server for report file", cmd_str)
        cmd.run(validateAfter=True)
        if cmd.get_results().stdout.strip() != filename:
            raise Exception('Report file %s was not backup up to NetBackup server %s successfully' % (filename, netbackup_service_host))

    elif filetype == 'global':
        basename = "gp_global_%s_1_" % ("1" if use_old_format else "-1")
        filename = os.path.join(dump_dir, "%s%s%s" % (prefix, basename, backup_timestamp))
        cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, filename)
        cmd = Command("Querying NetBackup server for global file", cmd_str)
        cmd.run(validateAfter=True)
        if cmd.get_results().stdout.strip() != filename:
            raise Exception('Global file %s was not backup up to NetBackup server %s successfully' % (filename, netbackup_service_host))

    elif filetype == 'config':
        master_config_filename = os.path.join(dump_dir, "%sgp_master_config_files_%s.tar" % (prefix, backup_timestamp))
        cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, master_config_filename)
        cmd = Command("Querying NetBackup server for master config file", cmd_str)
        cmd.run(validateAfter=True)
        if cmd.get_results().stdout.strip() != master_config_filename:
            raise Exception('Master config file %s was not backup up to NetBackup server %s successfully' % (master_config_filename, netbackup_service_host))

        master_port = os.environ.get('PGPORT')
        gparray = GpArray.initFromCatalog(dbconn.DbURL(port = master_port), utility=True)
        segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary(current_role=True)]
        for seg in segs:
            seg_dir = seg.getSegmentDataDirectory()
            dump_dir = os.path.join(seg_dir, 'db_dumps', '%s' % (backup_timestamp[0:8]))
            first_digit = 0 if use_old_format else seg.getSegmentContentId()
            seg_config_filename = os.path.join(dump_dir, "%sgp_segment_config_files_%d_%d_%s.tar" % (prefix, first_digit, seg.getSegmentDbId(), backup_timestamp))
            seg_host = seg.getSegmentHostName()
            cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, seg_config_filename)
            cmd = Command("Querying NetBackup server for segment config file", cmd_str, ctxt=REMOTE, remoteHost=seg_host)
            cmd.run(validateAfter=True)
            if cmd.get_results().stdout.strip() != seg_config_filename:
                raise Exception('Segment config file %s was not backup up to NetBackup server %s successfully' % (seg_config_filename, netbackup_service_host))

    elif filetype == 'state':
        filename = "%s/%sgp_dump_%s_ao_state_file" % (dump_dir, prefix, backup_timestamp)
        cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, filename)
        cmd = Command("Querying NetBackup server for AO state file", cmd_str)
        cmd.run(validateAfter=True)
        if cmd.get_results().stdout.strip() != filename:
            raise Exception('AO state file %s was not backup up to NetBackup server %s successfully' % (filename, netbackup_service_host))

        filename = "%s/%sgp_dump_%s_co_state_file" % (dump_dir, prefix, backup_timestamp)
        cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, filename)
        cmd = Command("Querying NetBackup server for CO state file", cmd_str)
        cmd.run(validateAfter=True)
        if cmd.get_results().stdout.strip() != filename:
            raise Exception('CO state file %s was not backup up to NetBackup server %s successfully' % (filename, netbackup_service_host))

        filename = "%s/%sgp_dump_%s_last_operation" % (dump_dir, prefix, backup_timestamp)
        cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, filename)
        cmd = Command("Querying NetBackup server for last operation state file", cmd_str)
        cmd.run(validateAfter=True)
        if cmd.get_results().stdout.strip() != filename:
            raise Exception('Last operation state file %s was not backup up to NetBackup server %s successfully' % (filename, netbackup_service_host))

    elif filetype == 'cdatabase':
        basename = "gp_cdatabase_%s_1_" % ("1" if use_old_format else "-1")
        filename = "%s/%s%s%s" % (dump_dir, prefix, basename, backup_timestamp)
        cmd_str = "gp_bsa_query_agent --netbackup-service-host %s --netbackup-filename %s" % (netbackup_service_host, filename)
        cmd = Command("Querying NetBackup server for cdatabase file", cmd_str)
        cmd.run(validateAfter=True)
        if cmd.get_results().stdout.strip() != filename:
            raise Exception('Cdatabase file %s was not backup up to NetBackup server %s successfully' % (filename, netbackup_service_host))

@when('all backup files under "{dir}" for stored dump timestamp are removed')
def impl(context, dir):
    if hasattr(context, 'backup_timestamp'):
        backup_timestamp = context.backup_timestamp
    else:
        raise Exception('No dump timestamp was stored from gpcrondump')
    dump_date = backup_timestamp[0:8]

    if dir.strip():
        dump_dir = dir.strip()
    else:
        dump_dir = master_data_dir

    backup_files_dir = os.path.join(dump_dir, 'db_dumps', dump_date)
    rm_cmd_str = "rm -rf %s/*" % backup_files_dir
    rm_cmd = Command("Remove files from dump dir location", rm_cmd_str)
    rm_cmd.run(validateAfter=True)

def verify_num_files_with_nbu(results, expected_num_files, timestamp):
    num_files = results.stdout.strip()
    if num_files != expected_num_files:
        raise Exception('Expected "%s" files with timestamp key "%s" but found "%s"' % (expected_num_files, timestamp,num_files))

def verify_timestamps_on_master_with_nbu(timestamp, dump_type):
    list_cmd = 'ls -l %s/db_dumps/%s/*%s* | wc -l' % (master_data_dir, timestamp[:8], timestamp)
    cmd = Command('verify timestamps on master', list_cmd)
    cmd.run(validateAfter=True)
    expected_num_files = '8' if dump_type == 'incremental' else '6'
    verify_num_files_with_nbu(cmd.get_results(), expected_num_files, timestamp)

def verify_timestamps_on_segments_with_nbu(timestamp):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    primary_segs = [segdb for segdb in gparray.getDbList() if segdb.isSegmentPrimary()]

    for seg in primary_segs:
        db_dumps_dir = os.path.join(seg.getSegmentDataDirectory(),
                                    'db_dumps',
                                    timestamp[:8])
        list_cmd = 'ls -l %s/*%s* | wc -l' % (db_dumps_dir, timestamp)
        cmd = Command('get list of dump files', list_cmd, ctxt=REMOTE, remoteHost=seg.getSegmentHostName())
        cmd.run(validateAfter=True)
        verify_num_files_with_nbu(cmd.get_results(), '1', timestamp)

@then('verify that "{dump_type}" dump files using netbackup have stored timestamp in their filename')
def impl(context, dump_type):
    if dump_type.strip().lower() != 'full' and dump_type.strip().lower() != 'incremental':
        raise Exception('Invalid dump type "%s"' % dump_type)

    verify_timestamps_on_master_with_nbu(context.backup_timestamp, dump_type.strip().lower())
    verify_timestamps_on_segments_with_nbu(context.backup_timestamp)

@given('all netbackup objects containing "{substr}" are deleted')
@when('all netbackup objects containing "{substr}" are deleted')
@then('all netbackup objects containing "{substr}" are deleted')
def impl(context, substr):
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host

    del_cmd_str = "gp_bsa_delete_agent --netbackup-service-host=%s --netbackup-delete-objects=*%s*" % (netbackup_service_host, substr)
    cmd = Command('Delete the list of objects matching regex on NetBackup server', del_cmd_str)
    cmd.run(validateAfter=True)
