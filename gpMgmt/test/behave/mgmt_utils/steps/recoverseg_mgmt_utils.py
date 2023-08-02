import glob
import os
import tempfile
from time import sleep
from gppylib.commands import gp
from contextlib import closing
from gppylib.commands.base import Command, ExecutionError, REMOTE, WorkerPool
from gppylib.db import dbconn
from gppylib.gparray import GpArray, ROLE_PRIMARY, ROLE_MIRROR
from gppylib.programs.clsRecoverSegment_triples import get_segments_with_running_basebackup, is_pg_rewind_running
from gppylib.operations.get_segments_in_recovery import is_seg_in_backup_mode
from test.behave_utils.utils import *
import platform, shutil
from behave import given, when, then
from gppylib.utils import writeLinesToFile

#TODO remove duplication of these functions
def _get_gpAdminLogs_directory():
    return "%s/gpAdminLogs" % os.path.expanduser("~")


def lines_matching_both(in_str, str_1, str_2):
    lines = [x.strip() for x in in_str.split('\n')]
    return [line for line in lines if line.count(str_1) and line.count(str_2)]


@given('the information of contents {contents} is saved')
@when('the information of contents {contents} is saved')
@then('the information of contents {contents} is saved')
def impl(context, contents):
    contents = set(contents.split(','))
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    context.original_seg_info = {}
    context.original_config_history_info = {}
    context.original_config_history_backout_count = 0
    for seg in all_segments:
        content = str(seg.getSegmentContentId())
        if content not in contents:
            continue
        preferred_role = seg.getSegmentPreferredRole()
        key = "{}_{}".format(content, preferred_role)
        context.original_seg_info[key] = seg

        with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
            dbid = dbconn.querySingleton(conn, "SELECT dbid FROM gp_segment_configuration WHERE content = %s AND "
                                               "preferred_role = '%s'" % (content, preferred_role))
        with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
            num_tuples = dbconn.querySingleton(conn, "SELECT count(*) FROM gp_configuration_history WHERE dbid = %d AND "
                                                     "description LIKE 'gprecoverseg: segment config for backout%%';" % dbid)
        context.original_config_history_info[key] = num_tuples
        with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
            context.original_config_history_backout_count = dbconn.querySingleton(conn, "SELECT count(*) FROM gp_configuration_history WHERE "
                                                     "description LIKE 'gprecoverseg: segment config for backout%%';")



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


#  todo ONLY implemented for a mirror; change name of step?
@given('the information of a "{seg}" segment on a remote host is saved')
@when('the information of a "{seg}" segment on a remote host is saved')
@then('the information of a "{seg}" segment on a remote host is saved')
def impl(context, seg):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    if seg == "primary":
        primary_segs = [seg for seg in gparray.getDbList()
                       if seg.isSegmentPrimary() and seg.getSegmentHostName() != platform.node()]
        context.remote_pair_primary_segdbId = primary_segs[0].getSegmentDbId()
        context.remote_pair_primary_segcid = primary_segs[0].getSegmentContentId()
        context.remote_pair_primary_host = primary_segs[0].getSegmentHostName()
        context.remote_pair_primary_datadir = primary_segs[0].getSegmentDataDirectory()
        context.remote_pair_primary_port = primary_segs[0].getSegmentPort()
        context.remote_pair_primary_address = primary_segs[0].getSegmentAddress()
    elif seg == "mirror":
        mirror_segs = [seg for seg in gparray.getDbList()
                       if seg.isSegmentMirror() and seg.getSegmentHostName() != platform.node()]
        context.remote_mirror_segdbId = mirror_segs[0].getSegmentDbId()
        context.remote_mirror_segcid = mirror_segs[0].getSegmentContentId()
        context.remote_mirror_seghost = mirror_segs[0].getSegmentHostName()
        context.remote_mirror_datadir = mirror_segs[0].getSegmentDataDirectory()
    else:
        raise Exception('Valid segment types are "primary" and "mirror"')

@given('the information of the corresponding primary segment on a remote host is saved')
@when('the information of the corresponding primary segment on a remote host is saved')
@then('the information of the corresponding primary segment on a remote host is saved')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    for seg in gparray.getDbList():
        if seg.isSegmentPrimary() and seg.getSegmentContentId() == context.remote_mirror_segcid:
            context.remote_pair_primary_segdbId = seg.getSegmentDbId()
            context.remote_pair_primary_datadir = seg.getSegmentDataDirectory()
            context.remote_pair_primary_port = seg.getSegmentPort()
            context.remote_pair_primary_host = seg.getSegmentHostName()


@given('the saved "{seg}" segment is marked down in config')
@when('the saved "{seg}" segment is marked down in config')
@then('the saved "{seg}" segment is marked down in config')
def impl(context, seg):
    if seg == "mirror":
        dbid = context.remote_mirror_segdbId
        seghost = context.remote_mirror_seghost
        datadir = context.remote_mirror_datadir
    else:
        dbid = context.remote_pair_primary_segdbId
        seghost = context.remote_pair_primary_host
        datadir = context.remote_pair_primary_datadir

    qry = """select count(*) from gp_segment_configuration where status='d' and hostname='%s' and dbid=%s""" % (seghost, dbid)
    row_count = getRows('postgres', qry)[0][0]
    if row_count != 1:
        raise Exception('Expected %s segment %s on host %s to be down, but it is running.' % (seg, datadir, seghost))

@then('the saved "{seg}" segment is eventually marked down in config')
def impl(context, seg):
    if seg == "mirror":
        dbid = context.remote_mirror_segdbId
        seghost = context.remote_mirror_seghost
        datadir = context.remote_mirror_datadir
    else:
        dbid = context.remote_pair_primary_segdbId
        seghost = context.remote_pair_primary_host
        datadir = context.remote_pair_primary_datadir

    timeout = 300
    for i in range(timeout):
        qry = """select count(*) from gp_segment_configuration where status='d' and hostname='%s' and dbid=%s""" % (seghost, dbid)
        row_count = getRows('postgres', qry)[0][0]
        if row_count == 1:
            return
        sleep(1)

    raise Exception('Expected %s segment %s on host %s to be down, but it is still running after %d seconds.' % (seg, datadir, seghost, timeout))

@when('user kills a "{seg}" process with the saved information')
def impl(context, seg):
    if seg == "mirror":
        datadir = context.remote_mirror_datadir
        seghost = context.remote_mirror_seghost
    elif seg == "primary":
        datadir = context.remote_pair_primary_datadir
        seghost = context.remote_pair_primary_host
    else:
        raise Exception("Got invalid segment type: %s" % seg)

    datadir_grep = '[' + datadir[0] + ']' + datadir[1:]
    cmdStr = "ps ux | grep %s | awk '{print $2}' | xargs kill" % datadir_grep

    subprocess.check_call(['ssh', seghost, cmdStr])

@then('the saved primary segment reports the same value for sql "{sql_cmd}" db "{dbname}" as was saved')
def impl(context, sql_cmd, dbname):
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_role=utility\' psql -t -h %s -p %s -c \"%s\"; " % (
        dbname, context.remote_pair_primary_host, context.remote_pair_primary_port, sql_cmd)
    cmd = Command(name='Running Remote command: %s' % psql_cmd, cmdStr = psql_cmd)
    cmd.run(validateAfter=True)
    if cmd.get_results().stdout.strip() not in context.stored_sql_results[0]:
        raise Exception("cmd results do not match\n expected: '%s'\n received: '%s'" % (
            context.stored_sql_results, cmd.get_results().stdout.strip()))

def isSegmentUp(context, dbid):
    qry = """select count(*) from gp_segment_configuration where status='d' and dbid=%s""" % dbid
    row_count = getRows('template1', qry)[0][0]
    if row_count == 0:
        return True
    else:
        return False

def getPrimaryDbIdFromCid(context, cid):
    dbid_from_cid_sql = "SELECT dbid FROM gp_segment_configuration WHERE content=%s and role='p';" % cid
    result = getRow('template1', dbid_from_cid_sql)
    return result[0]

def getMirrorDbIdFromCid(context, cid):
    dbid_from_cid_sql = "SELECT dbid FROM gp_segment_configuration WHERE content=%s and role='m';" % cid
    result = getRow('template1', dbid_from_cid_sql)
    return result[0]

def runCommandOnRemoteSegment(context, cid, sql_cmd):
    local_cmd = 'psql template1 -t -c "SELECT port,hostname FROM gp_segment_configuration WHERE content=%s and role=\'p\';"' % cid
    run_command(context, local_cmd)
    port, host = context.stdout_message.split("|")
    port = port.strip()
    host = host.strip()
    psql_cmd = "PGDATABASE=\'template1\' PGOPTIONS=\'-c gp_role=utility\' psql -h %s -p %s -c \"%s\"; " % (host, port, sql_cmd)
    Command(name='Running Remote command: %s' % psql_cmd, cmdStr = psql_cmd).run(validateAfter=True)

@then('{utility} should print "{output}" to stdout for each {segment_type}')
@when('{utility} should print "{output}" to stdout for each {segment_type}')
def impl(context, utility, output, segment_type):
    if segment_type not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % segment_type)
    role = ROLE_PRIMARY if segment_type == 'primary' else ROLE_MIRROR

    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == role and seg.content >= 0, all_segments)
    for segment in segments:
        expected = r'\(dbid {}\): {}'.format(segment.dbid, output)
        check_stdout_msg(context, expected)

@then('{utility} should print "{recovery_type}" errors to {output} for content {content_ids}')
@when('{utility} should print "{recovery_type}" errors to {output} for content {content_ids}')
def impl(context, utility, recovery_type, output, content_ids):
    if content_ids == "None":
        return
    if recovery_type not in ("incremental", "full", "differential","start"):
        raise Exception("Expected recovery_type to be 'incremental', 'full' or 'start, but found '%s'." % recovery_type)
    content_list = [int(c) for c in content_ids.split(',')]

    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.content in content_list, all_segments)

    for segment in segments:
        if recovery_type == 'incremental' or recovery_type == 'full':
            expected = r'hostname: {}; port: {}; logfile: {}/gpAdminLogs/pg_{}.\d{{8}}_\d{{6}}.dbid{}.out; recoverytype: {}'.format(
                segment.getSegmentHostName(), segment.getSegmentPort(), os.path.expanduser("~"),
                'rewind' if recovery_type == 'incremental' else 'basebackup', segment.getSegmentDbId(), recovery_type)
        if recovery_type == 'differential':
            expected = r'hostname: {}; port: {}; logfile: {}/gpAdminLogs/rsync.\d{{8}}_\d{{6}}.dbid{}.out; recoverytype: {}'.format(
                segment.getSegmentHostName(), segment.getSegmentPort(), os.path.expanduser("~"),segment.getSegmentDbId(), recovery_type)
        elif recovery_type == 'start':
            expected = r'hostname: {}; port: {}; datadir: {}'.format(segment.getSegmentHostName(), segment.getSegmentPort(),
                                                                     segment.getSegmentDataDirectory())
        if output == "logfile":
            context.execute_steps('''
            Then {0} should print "{1}" regex to logfile
            '''.format(utility, expected))
            return

        check_stdout_msg(context, expected)


@then('check if gprecoverseg ran gpsegrecovery.py {num} times with the expected args')
def impl(context, num):
    gprecoverseg_output = context.stdout_message

    era_cmd = "grep 'era =' {}/log/gp_era | sed 's/^.* // | tr -d '\n'".format(coordinator_data_dir)
    run_command(context, era_cmd)
    era = context.stdout_message

    expected_command = "Running Command: $GPHOME/sbin/gpsegrecovery.py"
    expected_args = "-l {} -v -b 64 --force-overwrite --era={}".format(_get_gpAdminLogs_directory(), era)
    matches = lines_matching_both(gprecoverseg_output, expected_command, expected_args)

    if len(matches) != int(num):
        raise Exception("Expected gpsegrecovery.py to occur with %s args %s times. Found %d. \n %s"
                        % (expected_args, num, len(matches), gprecoverseg_output))


@then('check if gprecoverseg ran gpsegsetuprecovery.py {num} times with the expected args')
def impl(context, num):
    gprecoverseg_output = context.stdout_message

    expected_command = "Running Command: $GPHOME/sbin/gpsegsetuprecovery.py"
    expected_args = "-l {} -v -b 64 --force-overwrite".format(_get_gpAdminLogs_directory())
    matches = lines_matching_both(gprecoverseg_output, expected_command, expected_args)

    if len(matches) != int(num):
        raise Exception("Expected gpsegrecovery.py to occur with %s args %s times. Found %d. \n %s"
                        % (expected_args, num, len(matches), gprecoverseg_output))


@then('check if {recovery_type} recovery was successful for mirrors with content {content_ids}')
def impl(context, recovery_type, content_ids):
    if recovery_type == 'incremental':
        print_msg = 'pg_rewind: Done!'
    elif recovery_type == 'full':
        print_msg = 'pg_basebackup: base backup completed'
    else:
        raise Exception("Expected recovery_type to be 'incremental', 'full' but found '%s'." % recovery_type)
    context.execute_steps('''
    Then gprecoverseg should print "{print_msg}" to stdout for mirrors with content {content_ids}
    And gprecoverseg should print "Initiating segment recovery." to stdout
    And verify that mirror on content {content_ids} is up
    And the segments are synchronized for content {content_ids}
    '''.format(print_msg=print_msg, content_ids=content_ids))


@then('check if {recovery_type} recovery failed for mirrors with content {content_ids} for {utility}')
def recovery_fail_check(context, recovery_type, content_ids, utility):
    return_code = 1
    if utility == 'gpmovemirrors':
        return_code = 3

    if recovery_type == 'incremental':
        print_msg = 'pg_rewind: fatal'
        logfile_name = 'pg_rewind*'
    elif recovery_type == 'full':
        print_msg = 'pg_basebackup: error: could not access directory' #TODO also assert for the directory location here
        logfile_name = 'pg_basebackup*'
    elif recovery_type == 'differential':
        print_msg = 'rsync error:'
        logfile_name = 'rsync*'
    else:
        raise Exception("Expected recovery_type to be 'incremental', 'full', 'differential' but found '%s'." % recovery_type)
    context.execute_steps('''
    Then gprecoverseg should return a return code of {return_code}
    And user can start transactions
    And gprecoverseg should print "Initiating segment recovery." to stdout
    And gprecoverseg should print "{print_msg}" to stdout for mirrors with content {content_ids}
    And gprecoverseg should print "Failed to recover the following segments" to stdout
    And gprecoverseg should print "{recovery_type}" errors to stdout for content {content_ids}
    And gpAdminLogs directory has "{logfile_name}" files on respective hosts only for content {content_ids}
    And verify that mirror on content {content_ids} is down
    And gprecoverseg should not print "Segments successfully recovered" to stdout
    '''.format(return_code=return_code, print_msg=print_msg, content_ids=content_ids, recovery_type=recovery_type,
               logfile_name=logfile_name))

    if recovery_type == 'differential':
        context.execute_steps('''Then gprecoverseg should print "gprecoverseg differential recovery failed. Please check the gpsegrecovery.py log file and rsync log file for more details." to stdout
        ''')

    else:
        context.execute_steps('''
                    Then gprecoverseg should print "gprecoverseg failed. Please check the output" to stdout''')


@when('check if start failed for contents {content_ids} during full recovery for {utility}')
@then('check if start failed for contents {content_ids} during full recovery for {utility}')
def start_fail_check(context, content_ids, utility):
    return_code = 1
    if utility == 'gpmovemirrors':
        return_code = 3

    context.execute_steps('''
    Then gprecoverseg should return a return code of {return_code}
    And gprecoverseg should print "Initiating segment recovery." to stdout
    And gprecoverseg should print "pg_basebackup: base backup completed" to stdout for mirrors with content {content_ids}
    And gprecoverseg should print "Failed to start the following segments" to stdout
    And gprecoverseg should print "start" errors to stdout for content {content_ids}
    And verify that mirror on content {content_ids} is down
    And gprecoverseg should print "gprecoverseg failed. Please check the output" to stdout
    And gprecoverseg should not print "Segments successfully recovered" to stdout
    '''.format(return_code=return_code, content_ids=content_ids))


@then('check if full recovery failed for mirrors with hostname {host}')
def impl(context, host):
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.getSegmentHostName() == host, all_segments)
    content_ids_on_host = [seg.content for seg in segments]
    content_id_str = ','.join(str(i) for i in content_ids_on_host)
    recovery_fail_check(context, recovery_type='full', content_ids=content_id_str)

@then('check if moving the mirrors from {original_host} to {new_host} failed {user_termination} user termination')
def impl(context, original_host, new_host, user_termination):
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.getSegmentHostName() == original_host, all_segments)
    content_ids_on_host = [seg.content for seg in segments]
    content_id_str = ','.join(str(i) for i in content_ids_on_host)

    context.execute_steps('''
    Then gprecoverseg should return a return code of 1
    And user can start transactions
    And gprecoverseg should print "Initiating segment recovery." to stdout
    And gprecoverseg should print "Failed to recover the following segments" to stdout
    And verify that mirror on content {content_ids} is down
    And gprecoverseg should print "gprecoverseg failed. Please check the output" to stdout
    And gprecoverseg should not print "Segments successfully recovered" to stdout
    '''.format(content_ids=content_id_str))

    if user_termination == "without":
        context.execute_steps('''
        Then gprecoverseg should print "pg_basebackup: error: could not access directory" to stdout for mirrors with content {content_ids}
        '''.format(content_ids=content_id_str))

    #TODO add this step
    #And gpAdminLogs directory has "pg_basebackup*" files on {new_host} only for content {content_ids}

    for segment in segments:
        #TODO replace with actual port name
        expected = r'hostname: {}; port: \d{{5}}; logfile: {}/gpAdminLogs/pg_basebackup.\d{{8}}_\d{{6}}.dbid{}.out; ' \
                   r'recoverytype: full'.format(new_host, os.path.expanduser("~"), segment.getSegmentDbId())
        check_stdout_msg(context, expected)



@then('check if start failed for full recovery for mirrors with hostname {host}')
def impl(context, host):
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.getSegmentHostName() == host, all_segments)
    content_ids_on_host = [seg.content for seg in segments]
    content_id_str = ','.join(str(i) for i in content_ids_on_host)
    start_fail_check(context, content_ids=content_id_str, utility='gprecoverseg')


@then('gprecoverseg should print "{output}" to stdout for mirrors with content {content_ids}')
def impl(context, output, content_ids):
    if content_ids == 'None':
        return
    content_list = [int(c) for c in content_ids.split(',')]
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.content in content_list, all_segments)
    for segment in segments:
        expected = r'\(dbid {}\): {}'.format(segment.dbid, output)
        check_stdout_msg(context, expected)


@then('pg_isready reports all primaries are accepting connections')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
    for seg in primary_segs:
        subprocess.check_call(['pg_isready', '-h', seg.getSegmentHostName(), '-p', str(seg.getSegmentPort())])


@then('the "{before}" and "{after}" cluster configuration matches for gprecoverseg newhost')
def impl(context, before, after):
    if not hasattr(context,'saved_array') or (before not in context.saved_array) or (after not in context.saved_array):
        raise Exception("before_array or after_array not saved prior to call")

    compare_gparray_with_expected(context.saved_array[before], context.saved_array[after])


# This test explicitly compares the actual before and after gparrays with what we
# expect.  While such a test is not extensible, it is easy to debug and does exactly
# what we need to right now.  Besides, the calling Scenario requires a specific cluster
# setup.  Note the before cluster is a standard CCP 5-host cluster(cdw/sdw1-4) and the
# after cluster is a result of a call to `gprecoverseg -p` after hosts have been shutdown.
# This causes the shutdown primaries to fail over to the mirrors, at which point there are 4
# contents in the cluster with no mirrors.  The `gprecoverseg -p` call creates those 4
# mirrors on new hosts.  It does not leave the cluster in its original state.
@then('the "{before}" and "{after}" cluster configuration matches with the expected for gprecoverseg newhost')
def impl(context, before, after):
    if not hasattr(context,'saved_array') or (before not in context.saved_array) or \
            (after not in context.saved_array):
        raise Exception("before_array or after_array not saved prior to call")

    expected = {}

    # this is the expected configuration coming into the Scenario(e.g. the original CCP cluster)
    expected["before"] = '''1|-1|p|p|n|u|cdw|mdw|5432|/data/gpdata/coordinator/gpseg-1
2|0|p|p|s|u|sdw1|sdw1|20000|/data/gpdata/primary/gpseg0
3|1|p|p|s|u|sdw1|sdw1|20001|/data/gpdata/primary/gpseg1
16|6|m|m|s|u|sdw1|sdw1|21000|/data/gpdata/mirror/gpseg6
17|7|m|m|s|u|sdw1|sdw1|21001|/data/gpdata/mirror/gpseg7
10|0|m|m|s|u|sdw2|sdw2|21000|/data/gpdata/mirror/gpseg0
11|1|m|m|s|u|sdw2|sdw2|21001|/data/gpdata/mirror/gpseg1
4|2|p|p|s|u|sdw2|sdw2|20000|/data/gpdata/primary/gpseg2
5|3|p|p|s|u|sdw2|sdw2|20001|/data/gpdata/primary/gpseg3
12|2|m|m|s|u|sdw3|sdw3|21000|/data/gpdata/mirror/gpseg2
13|3|m|m|s|u|sdw3|sdw3|21001|/data/gpdata/mirror/gpseg3
6|4|p|p|s|u|sdw3|sdw3|20000|/data/gpdata/primary/gpseg4
7|5|p|p|s|u|sdw3|sdw3|20001|/data/gpdata/primary/gpseg5
14|4|m|m|s|u|sdw4|sdw4|21000|/data/gpdata/mirror/gpseg4
15|5|m|m|s|u|sdw4|sdw4|21001|/data/gpdata/mirror/gpseg5
8|6|p|p|s|u|sdw4|sdw4|20000|/data/gpdata/primary/gpseg6
9|7|p|p|s|u|sdw4|sdw4|20001|/data/gpdata/primary/gpseg7
'''
    expected["after_recreation"] = expected["before"]

    # this is the expected configuration after "gprecoverseg -p sdw5" after "sdw1" goes down
    expected["one_host_down"] = '''1|-1|p|p|n|u|mdw|mdw|5432|/data/gpdata/coordinator/gpseg-1
10|0|p|m|s|u|sdw2|sdw2|21000|/data/gpdata/mirror/gpseg0
11|1|p|m|s|u|sdw2|sdw2|21001|/data/gpdata/mirror/gpseg1
4|2|p|p|s|u|sdw2|sdw2|20000|/data/gpdata/primary/gpseg2
5|3|p|p|s|u|sdw2|sdw2|20001|/data/gpdata/primary/gpseg3
12|2|m|m|s|u|sdw3|sdw3|21000|/data/gpdata/mirror/gpseg2
13|3|m|m|s|u|sdw3|sdw3|21001|/data/gpdata/mirror/gpseg3
6|4|p|p|s|u|sdw3|sdw3|20000|/data/gpdata/primary/gpseg4
7|5|p|p|s|u|sdw3|sdw3|20001|/data/gpdata/primary/gpseg5
14|4|m|m|s|u|sdw4|sdw4|21000|/data/gpdata/mirror/gpseg4
15|5|m|m|s|u|sdw4|sdw4|21001|/data/gpdata/mirror/gpseg5
8|6|p|p|s|u|sdw4|sdw4|20000|/data/gpdata/primary/gpseg6
9|7|p|p|s|u|sdw4|sdw4|20001|/data/gpdata/primary/gpseg7
2|0|m|p|s|u|sdw5|sdw5|20000|/data/gpdata/primary/gpseg0
3|1|m|p|s|u|sdw5|sdw5|20001|/data/gpdata/primary/gpseg1
16|6|m|m|s|u|sdw5|sdw5|20002|/data/gpdata/mirror/gpseg6
17|7|m|m|s|u|sdw5|sdw5|20003|/data/gpdata/mirror/gpseg7
'''

    # this is the expected configuration after "gprecoverseg -p sdw5,sdw6" after "sdw1,sdw3" go down
    expected["two_hosts_down"] ='''1|-1|p|p|n|u|mdw|mdw|5432|/data/gpdata/coordinator/gpseg-1
10|0|p|m|s|u|sdw2|sdw2|21000|/data/gpdata/mirror/gpseg0
11|1|p|m|s|u|sdw2|sdw2|21001|/data/gpdata/mirror/gpseg1
4|2|p|p|s|u|sdw2|sdw2|20000|/data/gpdata/primary/gpseg2
5|3|p|p|s|u|sdw2|sdw2|20001|/data/gpdata/primary/gpseg3
14|4|p|m|s|u|sdw4|sdw4|21000|/data/gpdata/mirror/gpseg4
15|5|p|m|s|u|sdw4|sdw4|21001|/data/gpdata/mirror/gpseg5
8|6|p|p|s|u|sdw4|sdw4|20000|/data/gpdata/primary/gpseg6
9|7|p|p|s|u|sdw4|sdw4|20001|/data/gpdata/primary/gpseg7
2|0|m|p|s|u|sdw5|sdw5|20000|/data/gpdata/primary/gpseg0
3|1|m|p|s|u|sdw5|sdw5|20001|/data/gpdata/primary/gpseg1
16|6|m|m|s|u|sdw5|sdw5|20002|/data/gpdata/mirror/gpseg6
17|7|m|m|s|u|sdw5|sdw5|20003|/data/gpdata/mirror/gpseg7
12|2|m|m|s|u|sdw6|sdw6|20000|/data/gpdata/mirror/gpseg2
13|3|m|m|s|u|sdw6|sdw6|20001|/data/gpdata/mirror/gpseg3
6|4|m|p|s|u|sdw6|sdw6|20002|/data/gpdata/primary/gpseg4
7|5|m|p|s|u|sdw6|sdw6|20003|/data/gpdata/primary/gpseg5
'''

    if (before not in expected) or (after not in expected):
        raise Exception("before_array or after_array has no expected array...")

    with tempfile.NamedTemporaryFile() as f:
        f.write(expected[before].encode('utf-8'))
        f.flush()
        expected_before_gparray = GpArray.initFromFile(f.name)

    with tempfile.NamedTemporaryFile() as f:
        f.write(expected[after].encode('utf-8'))
        f.flush()
        expected_after_gparray = GpArray.initFromFile(f.name)

    compare_gparray_with_expected(context.saved_array[before],  expected_before_gparray, array_name=before)
    compare_gparray_with_expected(context.saved_array[after], expected_after_gparray, array_name=after)


def compare_gparray_with_expected(actual_gparray, expected_gparray, array_name = ''):

    def _sortedSegs(gparray):
        segs_by_host = GpArray.getSegmentsByHostName(gparray.getSegDbList())
        for host in segs_by_host:
            segs_by_host[host] = sorted(segs_by_host[host], key=lambda seg: seg.getSegmentDbId())
        return segs_by_host

    actual_segs = _sortedSegs(actual_gparray)
    expected_segs = _sortedSegs(expected_gparray)

    if actual_segs != expected_segs:
        msg = "MISMATCH {}\n\nactual_array:\n{}\n\nexpected_array:\n{}\n".format(
            array_name, actual_segs, expected_segs)
        raise Exception(msg)


@when('a gprecoverseg input file "{filename}" is created with a different data directory for content {content}')
def impl(context, filename, content):
    line = ""
    with closing(dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)) as conn:
        result = dbconn.query(conn, "SELECT port, hostname, datadir FROM gp_segment_configuration WHERE preferred_role='p' AND content = %s;" % content).fetchall()
        port, hostname, datadir = result[0][0], result[0][1], result[0][2]
        line = "%s|%s|%s %s|%s|/tmp/newdir" % (hostname, port, datadir, hostname, port)

    with open("/tmp/%s" % filename, "w") as fd:
        fd.write("%s\n" % line)



@given('the user waits until mirror on content {content_ids} is {expected_status}')
@when('the user waits until mirror on content {content_ids} is {expected_status}')
@then('the user waits until mirror on content {content_ids} is {expected_status}')
def impl(context, content_ids, expected_status):
    contents = content_ids.split(',')
    for content in contents:
        query = "SELECT gp_request_fts_probe_scan(); SELECT status FROM gp_segment_configuration where role = 'm' and content = {};".format(content)
        wait_for_desired_query_result(dbconn.DbURL(), query, 'u' if expected_status == 'up' else 'd')


@then('the contents {contents} should have their original data directory in the system configuration')
def impl(context, contents):
    contents = contents.split(',')
    for content in contents:
        with closing(dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)) as conn:
            for role in [ROLE_PRIMARY, ROLE_MIRROR]:
                actual_datadir = dbconn.querySingleton(conn, "SELECT datadir FROM gp_segment_configuration WHERE preferred_role='{}' AND "
                                                      "content = {};".format(role, content))
                expected_datadir = context.original_seg_info["{}_{}".format(content, role)].getSegmentDataDirectory()
                if not expected_datadir == actual_datadir:
                    raise Exception("Expected datadir {}, got {} for content {}".format(expected_datadir, actual_datadir, content))


@given('the gprecoverseg input file "{filename}" is cleaned up')
@then('the gprecoverseg input file "{filename}" is cleaned up')
def impl(context, filename):
    if os.path.exists(filename):
        os.remove(filename)


@given('verify there are no recovery backout files')
@then('verify there are no recovery backout files')
@when('verify there are no recovery backout files')
def impl(context):
    dirs = glob.glob("{}/gpAdminLogs/*backout*".format(os.path.expanduser("~")))
    if len(dirs) > 0:
        raise Exception("One or more backout directories exist: %s" % dirs)

@given('the gprecoverseg lock directory is removed')
@when('the gprecoverseg lock directory is removed')
@then('the gprecoverseg lock directory is removed')
def impl(context):
    lock_dir = "%s/gprecoverseg.lock" % os.environ["COORDINATOR_DATA_DIRECTORY"]
    if os.path.exists(lock_dir):
        shutil.rmtree(lock_dir)

@then('the gp_configuration_history table should contain a backout entry for the {seg} segment for contents {contents}')
def impl(context, seg, contents):
    role = ""
    if seg == "primary":
        role = 'p'
    elif seg == "mirror":
        role = 'm'
    else:
        raise Exception('Valid segment types are "primary" and "mirror"')

    contents = contents.split(',')
    for content in contents:
        with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
            dbid = dbconn.querySingleton(conn, "SELECT dbid FROM gp_segment_configuration WHERE content = %s AND preferred_role = '%s'" % (content, role))
        with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
            actual_tuples = dbconn.querySingleton(conn, "SELECT count(*) FROM gp_configuration_history WHERE dbid = %d AND description LIKE 'gprecoverseg: segment config for backout%%';" % dbid)

        original_tuples = context.original_config_history_info["{}_{}".format(content, role)]
        if actual_tuples != original_tuples + 1: # Running the backout script should have inserted exactly 1 entry
            raise Exception("Expected configuration history table for dbid {} to contain {} backout entries, found {}".format
                            (dbid, original_tuples + 1, actual_tuples))


@then('the gp_configuration_history table should contain {expected_additional_entries} additional backout entries')
def impl(context, expected_additional_entries):
    with closing(dbconn.connect(dbconn.DbURL(), unsetSearchPath=False)) as conn:
        actual_backout_entries = int(dbconn.querySingleton(conn, "SELECT count(*) FROM gp_configuration_history WHERE "
                                                             "description LIKE "
                                                             "'gprecoverseg: segment config for backout%%';"))
    expected_total_entries = int(context.original_config_history_backout_count) + int(expected_additional_entries)
    if actual_backout_entries != expected_total_entries:
        raise Exception("Expected configuration history table to have {} backout entries, found {}".format(
            context.original_config_history_backout_count + expected_additional_entries, actual_backout_entries))


@when('a gprecoverseg input file "{filename}" is created with added parameter hostname to recover the failed segment on new host')
def impl(context, filename):
    with closing(dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)) as conn:
        failed_port, failed_hostname, failed_datadir, failed_address = context.remote_pair_primary_port, \
            context.remote_pair_primary_host, context.remote_pair_primary_datadir, context.remote_pair_primary_address
        result = dbconn.query(conn,"SELECT hostname FROM gp_segment_configuration WHERE preferred_role='p' and status = 'u' and content != -1;").fetchall()
        failover_port, failover_hostname, failover_datadir = 23000, result[0][0], failed_datadir

        failover_host_address = get_host_address(failover_hostname)
        context.recovery_host_address = failover_host_address
        context.recovery_host_name = failover_hostname

        line = "{0}|{1}|{2}|{3} {4}|{5}|{6}|{7}" .format(failed_hostname, failed_address, failed_port, failed_datadir,
                                            failover_hostname, failover_host_address, failover_port, failover_datadir)

    with open("/tmp/%s" % filename, "w") as fd:
        fd.write("%s\n" % line)


@when('check hostname and address updated on segment configuration with the saved information')
def impl(context):
    with closing(dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)) as conn:
        result = dbconn.queryRow(conn, "SELECT content, hostname, address FROM gp_segment_configuration WHERE dbid = {};" .format(context.remote_pair_primary_segdbId))
        content, hostname, address = result[0], result[1], result[2]

        if address != context.recovery_host_address or hostname != context.recovery_host_name:
            raise Exception(
                'Host name and address could not updated on segment configuration for dbId {0}'.format(context.remote_pair_primary_segdbId))


@when('a gprecoverseg input file "{filename}" is created with hostname parameter to recover the failed segment on same host')
def impl(context, filename):
    port, hostname, datadir, address = context.remote_pair_primary_port, context.remote_pair_primary_host,\
                              context.remote_pair_primary_datadir, context.remote_pair_primary_address

    host_address = get_host_address(hostname)
    context.recovery_host_address = host_address
    context.recovery_host_name = hostname

    line = "{0}|{1}|{2}|{3} {4}|{5}|{6}|/tmp/newdir" .format(hostname, address, port, datadir, hostname,
                                                             host_address, port)

    with open("/tmp/%s" % filename, "w") as fd:
        fd.write("%s\n" % line)


@when('a gprecoverseg input file "{filename}" is created with hostname parameter matches with segment configuration table for incremental recovery of failed segment')
def impl(context, filename):
    port, hostname, datadir, address = context.remote_pair_primary_port, context.remote_pair_primary_host, \
        context.remote_pair_primary_datadir, context.remote_pair_primary_address
    context.recovery_host_address = address
    context.recovery_host_name = hostname

    line = "{0}|{1}|{2}|{3}" .format(hostname, address, port, datadir)

    with open("/tmp/%s" % filename, "w") as fd:
        fd.write("%s\n" % line)


@when('a gprecoverseg input file "{filename}" is created with invalid format for inplace full recovery of failed segment')
def impl(context, filename):
    port, hostname, datadir, address = context.remote_pair_primary_port, context.remote_pair_primary_host,\
                              context.remote_pair_primary_datadir, context.remote_pair_primary_address

    host_address = get_host_address(hostname)

    line = "{0}|{1}|{2}|{3} {4}|{5}|/tmp/newdir" .format(hostname, address, port, datadir, host_address, port)

    with open("/tmp/%s" % filename, "w") as fd:
        fd.write("%s\n" % line)


@when('a gprecoverseg input file "{filename}" created with invalid failover hostname for full recovery of failed segment')
def impl(context, filename):
    port, hostname, datadir, address = context.remote_pair_primary_port, context.remote_pair_primary_host,\
                              context.remote_pair_primary_datadir, context.remote_pair_primary_address

    host_address = get_host_address(hostname)

    line = "{0}|{1}|{2}|{3} {4}_1|{5}|{6}|/tmp/newdir" .format(hostname, address, port, datadir, hostname, host_address, port)

    with open("/tmp/%s" % filename, "w") as fd:
        fd.write("%s\n" % line)


@when('a gprecoverseg input file "{filename}" is created with and without parameter hostname to recover all the failed segments')
def impl(context, filename):
    lines = []
    with closing(dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False)) as conn:
        rows = dbconn.query(conn,"SELECT port, hostname, datadir, content, address FROM gp_segment_configuration WHERE  status = 'd' and content != -1;").fetchall()
    for i, row in enumerate(rows):
        output_str = ""
        hostname = row[1]
        host_address = get_host_address(hostname)
        port = row[0]
        address = row[4]
        datadir = row[2]
        content = row[3]

        if content == 0:
            output_str += "{0}|{1}|{2}".format(address, port, datadir)
        elif content == 1:
            output_str += "{0}|{1}|{2} {3}|{4}|/tmp/newdir{5}".format(address, port, datadir, address, port, i)
        else:
            output_str += "{0}|{1}|{2}|{3} {4}|{5}|{6}|/tmp/newdir{7}".format(hostname, address, port, datadir,
                                                                              hostname, host_address, port, i)

        lines.append(output_str)
    writeLinesToFile("/tmp/%s" % filename, lines)

@when('a gprecoverseg input file "{filename}" is created with invalid hostname parameter that does not matches with the segment configuration table hostname')
def impl(context, filename):
    port, hostname, datadir, address = context.remote_pair_primary_port, context.remote_pair_primary_host, \
        context.remote_pair_primary_datadir, context.remote_pair_primary_address

    line = "{0}|{1}|{2}|{3}" .format("invalid_hostname", address, port, datadir)

    with open("/tmp/%s" % filename, "w") as fd:
        fd.write("%s\n" % line)

def get_host_address(hostname):
    cmd = Command("get the address of the host", cmdStr="hostname -I", ctxt=REMOTE, remoteHost=hostname)
    cmd.run(validateAfter=True)
    host_address = cmd.get_stdout().strip().split(' ')
    return host_address[0]



@then('pg_hba file on primary of mirrors on "{newhost}" with "{contents}" contains no replication entries for "{oldhost}"')
@when('pg_hba file on primary of mirrors on "{newhost}" with "{contents}" contains no replication entries for "{oldhost}"')
def impl(context, newhost, contents, oldhost):
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()

    for seg in all_segments:
        if newhost != "none" and seg.mirrorDB.getSegmentHostName() != newhost:
            continue
        if contents != "all":
            for content_id in contents.split(','):
                if seg.mirrorDB.getSegmentContentId() != int(content_id):
                    continue
                check_entry_present(context, seg, oldhost)
        else:
            check_entry_present(context, seg, oldhost)

def check_entry_present(context, seg, oldhost):
    for host in oldhost.split(','):
        search_ip_addr = context.host_ip_list[host]
        dbname = "template1"
        ip_address = "','".join(search_ip_addr)
        query = "SELECT count(*) FROM pg_hba_file_rules WHERE database='{{replication}}' AND (address='{0}' OR address IN ('{1}'))".format(
            host, ip_address)
        phost = seg.primaryDB.getSegmentHostName()
        port = seg.primaryDB.getSegmentPort()

        with closing(dbconn.connect(dbconn.DbURL(dbname=dbname, port=port, hostname=phost),
                                    utility=True, unsetSearchPath=False)) as conn:
            result = dbconn.querySingleton(conn, query)
            if result != 0:
                raise Exception("{0} replication entry for {1}, {2} still existing in pg_hba.conf of {3}:{4}"
                                .format(result, host, search_ip_addr,phost, port))


@then('verify that only replication connection primary has is to {new_mirror}')
@when('verify that only replication connection primary has is to {new_mirror}')
@given('verify that only replication connection primary has is to {new_mirror}')
def impl(context, new_mirror):
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getSegmentList()

    for seg in all_segments:
        if seg.mirrorDB.getSegmentHostName() != new_mirror:
            continue

        dbname = "template1"
        search_ip_addr = context.host_ip_list[new_mirror]
        ip_address = "','".join(search_ip_addr)
        query = """
        SELECT
          CASE
            WHEN
              (SELECT COUNT(*) FROM gp_stat_replication WHERE client_addr IN ('{1}')) =
              (SELECT COUNT(*) FROM gp_stat_replication)
            THEN TRUE
            ELSE FALSE
        END;""".format(ip_address)

        phost = seg.primaryDB.getSegmentHostName()
        port = seg.primaryDB.getSegmentPort()
        with closing(dbconn.connect(dbconn.DbURL(dbname=dbname, port=port, hostname=phost),
                                    utility=True, unsetSearchPath=False)) as conn:
            result = dbconn.querySingleton(conn, query)
            if result != 't':
                raise Exception("{} replication connections are not updated.".format(phost))


@given('saving host IP address of "{host}"')
@then('saving host IP address of "{host}"')
@when('saving host IP address of "{host}"')
def impl(context, host):
    context.host_ip_list = {}
    for host_name in host.split(','):
        if_addrs = gp.IfAddrs.list_addrs(host_name)
        context.host_ip_list[host_name] = if_addrs



@then('verify that pg_basebackup {action} running for content {content_ids}')
def impl(context, action, content_ids):
    attempt = 0
    num_retries = 600
    content_ids_to_check = [int(c) for c in content_ids.split(',')]

    while attempt < num_retries:
        attempt += 1
        content_ids_running_basebackup = get_segments_with_running_basebackup()

        if action == "is not":
            if not any(content in content_ids_running_basebackup for content in content_ids_to_check):
                return

        if action == "is":
            if all(content in content_ids_running_basebackup for content in content_ids_to_check):
                return

        time.sleep(0.1)
        if attempt == num_retries:
            raise Exception('Timed out after {} retries'.format(num_retries))


@then('verify that pg_rewind {action} running for content {content_ids}')
def impl(context, action, content_ids):
    qry = "SELECT hostname, port FROM gp_segment_configuration WHERE status='u' AND role='p' AND content IN ({0})".format(content_ids)
    rows = getRows('postgres', qry)

    attempt = 0
    num_retries = 600
    while attempt < num_retries:
        attempt += 1

        if action == "is not":
            if not any(is_pg_rewind_running(row[0], row[1]) for row in rows):
                return

        if action == "is":
            if all(is_pg_rewind_running(row[0], row[1]) for row in rows):
                return

        time.sleep(0.1)
        if attempt == num_retries:
            raise Exception('Timed out after {} retries'.format(num_retries))


@then('verify that differential {action} running for content {content_ids}')
def impl(context, action, content_ids):
    qry = "SELECT hostname, port FROM gp_segment_configuration WHERE status='u' AND role='p' AND content IN ({0})".format(content_ids)
    rows = getRows('postgres', qry)

    attempt = 0
    num_retries = 600
    while attempt < num_retries:
        attempt += 1

        if action == "is not":
            if not any(is_seg_in_backup_mode(row[0], row[1]) for row in rows):
                return

        if action == "is":
            if all(is_seg_in_backup_mode(row[0], row[1]) for row in rows):
                return

        time.sleep(0.1)
        if attempt == num_retries:
            raise Exception('Timed out after {} retries'.format(num_retries))
