import glob
import os
import tempfile

from time import sleep
from gppylib.commands.base import Command, ExecutionError, REMOTE, WorkerPool
from gppylib.db import dbconn
from gppylib.commands import gp
from gppylib.gparray import GpArray, ROLE_PRIMARY, ROLE_MIRROR
from test.behave_utils.utils import *
import platform
from behave import given, when, then


#  todo ONLY implemented for a mirror; change name of step?
@given('the information of a "{seg}" segment on a remote host is saved')
@when('the information of a "{seg}" segment on a remote host is saved')
@then('the information of a "{seg}" segment on a remote host is saved')
def impl(context, seg):
    if seg == "mirror":
        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        mirror_segs = [seg for seg in gparray.getDbList()
                       if seg.isSegmentMirror() and seg.getSegmentHostName() != platform.node()]
        context.remote_mirror_segdbId = mirror_segs[0].getSegmentDbId()
        context.remote_mirror_segcid = mirror_segs[0].getSegmentContentId()
        context.remote_mirror_seghost = mirror_segs[0].getSegmentHostName()
        context.remote_mirror_datadir = mirror_segs[0].getSegmentDataDirectory()

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
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_session_role=utility\' psql -t -h %s -p %s -c \"%s\"; " % (
        dbname, context.remote_pair_primary_host, context.remote_pair_primary_port, sql_cmd)
    cmd = Command(name='Running Remote command: %s' % psql_cmd, cmdStr = psql_cmd)
    cmd.run(validateAfter=True)
    if [cmd.get_results().stdout.strip()] not in context.stored_sql_results:
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
    psql_cmd = "PGDATABASE=\'template1\' PGOPTIONS=\'-c gp_session_role=utility\' psql -h %s -p %s -c \"%s\"; " % (host, port, sql_cmd)
    Command(name='Running Remote command: %s' % psql_cmd, cmdStr = psql_cmd).run(validateAfter=True)

@then('gprecoverseg should print "{output}" to stdout for each {segment_type}')
@when('gprecoverseg should print "{output}" to stdout for each {segment_type}')
def impl(context, output, segment_type):
    if segment_type not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % segment_type)
    role = ROLE_PRIMARY if segment_type == 'primary' else ROLE_MIRROR

    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == role and seg.content >= 0, all_segments)
    for segment in segments:
        expected = r'\(dbid {}\): {}'.format(segment.dbid, output)
        check_stdout_msg(context, expected)

@then('gprecoverseg should print "{output}" to stdout for mirrors with content {content_ids}')
def impl(context, output, content_ids):
    content_list = [int(c) for c in content_ids.split(',')]
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.content in content_list, all_segments)
    for segment in segments:
        expected = r'\(dbid {}\): {}'.format(segment.dbid, output)
        check_stdout_msg(context, expected)

@then('gpAdminLogs directory has "{expected_file}" files only for content {content_ids}')
def impl(context, expected_file, content_ids):
    content_list = [int(c) for c in content_ids.split(',')]
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentRole() == ROLE_MIRROR and
                                  seg.content in content_list, all_segments)
    seg_dbids = []
    for seg in segments:
        seg_dbids.append('dbid{}'.format(seg.dbid))

    log_dir = "%s/gpAdminLogs" % os.path.expanduser("~")
    files_found = glob.glob('%s/%s' % (log_dir, expected_file))
    if not files_found:
        raise Exception("expected %s files in %s, but not found" % (expected_file, log_dir))
    if len(files_found) != len(seg_dbids):
        raise Exception("expected {} {} files in {}, but found {}".format(len(seg_dbids), expected_file, log_dir, len(files_found)))

    for file in files_found:
        if file.split('.')[-2] not in seg_dbids:
            raise Exception("Found unexpected file {} in {}".format(file, log_dir))


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

    for segHost in host_to_seg_dbids:
        expected_files_on_host = host_to_seg_dbids[segHost]
        log_dir = "~/gpAdminLogs"
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


@then('pg_isready reports all primaries are accepting connections')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
    for seg in primary_segs:
        subprocess.check_call(['pg_isready', '-h', seg.getSegmentHostName(), '-p', str(seg.getSegmentPort())])

@then('the cluster is rebalanced')
def impl(context):
    context.execute_steps(u'''
        Then the user runs "gprecoverseg -a -s -r"
        And gprecoverseg should return a return code of 0
        And user can start transactions
        And the segments are synchronized
        ''')

# This test explicitly compares the actual before and after gparrays with what we
# expect.  While such a test is not extensible, it is easy to debug and does exactly
# what we need to right now.  Besides, the calling Scenario requires a specific cluster
# setup.  Note the before cluster is a standard CCP 5-host cluster(mdw/sdw1-4) and the
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
    expected["before"] = '''1|-1|p|p|n|u|mdw|mdw|5432|/data/gpdata/master/gpseg-1
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
    expected["one_host_down"] = '''1|-1|p|p|n|u|mdw|mdw|5432|/data/gpdata/master/gpseg-1
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
    expected["two_hosts_down"] ='''1|-1|p|p|n|u|mdw|mdw|5432|/data/gpdata/master/gpseg-1
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
        f.write(expected[before])
        f.flush()
        expected_before_gparray = GpArray.initFromFile(f.name)

    with tempfile.NamedTemporaryFile() as f:
        f.write(expected[after])
        f.flush()
        expected_after_gparray = GpArray.initFromFile(f.name)

    compare_gparray_with_recovered_host(context.saved_array[before], context.saved_array[after], expected_before_gparray, expected_after_gparray)

def compare_gparray_with_recovered_host(before_gparray, after_gparray, expected_before_gparray, expected_after_gparray):

    def _sortedSegs(gparray):
        segs_by_host = GpArray.getSegmentsByHostName(gparray.getSegDbList())
        for host in segs_by_host:
            segs_by_host[host] = sorted(segs_by_host[host], key=lambda seg: seg.getSegmentDbId())
        return segs_by_host

    before_segs = _sortedSegs(before_gparray)
    expected_before_segs = _sortedSegs(expected_before_gparray)

    after_segs = _sortedSegs(after_gparray)
    expected_after_segs  = _sortedSegs(expected_after_gparray)

    if before_segs != expected_before_segs or after_segs != expected_after_segs:
        msg = "MISMATCH\n\nactual_before:\n{}\n\nexpected_before:\n{}\n\nactual_after:\n{}\n\nexpected_after:\n{}\n".format(
            before_segs, expected_before_segs, after_segs, expected_after_segs)
        raise Exception(msg)
