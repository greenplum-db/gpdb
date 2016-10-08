# coding: utf-8

import os
from gppylib.db import dbconn
from gppylib.test.behave_utils.utils import run_gpcommand
from gppylib.gparray import GpArray
from gppylib.test.behave_utils.utils import get_all_hostnames_as_list
from gppylib.operations.backup_utils import Context

master_data_dir = os.environ.get('MASTER_DATA_DIRECTORY')

@given('the NetBackup "{ver}" libraries are loaded')
def impl(context, ver):
    hosts = set(get_all_hostnames_as_list(context, 'template1'))
    gphome = os.environ.get('GPHOME')
    ver = ver.replace('.', '')
    cpCmd = 'cp -f {gphome}/lib/nbu{ver}/lib/* {gphome}/lib/'.format(gphome=gphome,
                                                                     ver=ver)

    for host in hosts:
        cmd = Command(name='Copy NBU lib files',
                cmdStr=cpCmd,
                ctxt=REMOTE,
                remoteHost=host)
        cmd.run(validateAfter=True)

@when('the user runs "{cmd_str}" using netbackup with long params')
def impl(context, cmd_str):
    netbackup_service_host = ""
    netbackup_policy = ""
    netbackup_schedule = ""
    if "--netbackup-service-host" not in cmd_str:
        netbackup_service_host = " --netbackup-service-host netbackup-service-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127 "
    if "--netbackup-policy" not in cmd_str:
        netbackup_policy = " --netbackup-policy netbackup-policy-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127 "
    if "--netbackup-schedule" not in cmd_str:
        netbackup_schedule = " --netbackup-schedule netbackup-schedule-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa127 "
    bnr_tool = cmd_str.split()[0].strip()
    if bnr_tool == 'gpcrondump':
        command_str = cmd_str + netbackup_service_host + netbackup_policy + netbackup_schedule
    elif bnr_tool == 'gpdbrestore':
        command_str = cmd_str + netbackup_service_host
    elif bnr_tool == 'gp_dump':
        command_str = cmd_str + netbackup_service_host + netbackup_policy + netbackup_schedule
    elif bnr_tool == 'gp_restore':
        command_str = cmd_str + netbackup_service_host

    run_gpcommand(context, command_str)

@when('the user runs "{cmd_str}" using netbackup')
def impl(context, cmd_str):
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    if hasattr(context, 'netbackup_policy'):
        netbackup_policy = context.netbackup_policy
    if hasattr(context, 'netbackup_schedule'):
        netbackup_schedule = context.netbackup_schedule
    bnr_tool = cmd_str.split()[0].strip()
    if bnr_tool == 'gpcrondump':
        command_str = cmd_str + " --netbackup-service-host " + netbackup_service_host + " --netbackup-policy " + netbackup_policy + " --netbackup-schedule " + netbackup_schedule
    elif bnr_tool == 'gpdbrestore':
        command_str = cmd_str + " --netbackup-service-host " + netbackup_service_host
    elif bnr_tool == 'gp_dump':
        command_str = cmd_str + " --netbackup-service-host " + netbackup_service_host + " --netbackup-policy " + netbackup_policy + " --netbackup-schedule " + netbackup_schedule
    elif bnr_tool == 'gp_restore':
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
def impl(context):
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
        segment_config_filename = os.path.join(seg.getSegmentDataDirectory(), 'db_dumps', context.backup_timestamp[0:8],
                                           '%sgp_segment_config_files_0_%s_%s.tar' % (context.dump_prefix, seg.getSegmentDbId(), context.backup_timestamp))
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
    if hasattr(context, 'backup_timestamp'):
        ts = context.backup_timestamp
    if hasattr(context, 'netbackup_service_host'):
        netbackup_service_host = context.netbackup_service_host
    command = 'gp_restore -i --gp-k %s --gp-d db_dumps/%s --gp-i --gp-r db_dumps/%s --gp-l=p -d %s --gp-c -s db_dumps/%s/gp_dump_1_1_%s.gz --netbackup-service-host %s' % \
                            (ts, context.backup_subdir, context.backup_subdir, dbname, context.backup_subdir, ts, netbackup_service_host)
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
    command = Command(name='run gpcrondump in a separate thread', cmdStr=cmd)
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
        filename = os.path.join(dump_dir, "%sgp_global_1_1_%s" % (prefix, backup_timestamp))
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
            seg_config_filename = os.path.join(dump_dir, "%sgp_segment_config_files_0_%d_%s.tar" % (prefix, seg.getSegmentDbId(), backup_timestamp))
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
        filename = "%s/%sgp_cdatabase_1_1_%s" % (dump_dir, prefix, backup_timestamp)
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
