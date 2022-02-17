import tempfile

from time import sleep
from gppylib.commands.base import Command, ExecutionError, REMOTE, WorkerPool
from gppylib.db import dbconn
from gppylib.commands import gp
from gppylib.gparray import GpArray, ROLE_PRIMARY, ROLE_MIRROR
from test.behave_utils.utils import *
import glob, os, os.path, platform, shutil
from behave import given, when, then


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

@then('gprecoverseg should print "{output}" to stdout for each {segment_type}')
@when('gprecoverseg should print "{output}" to stdout for each {segment_type}')
def impl(context, output, segment_type):
    if segment_type not in ("primary", "mirror"):
        raise Exception("Expected segment_type to be 'primary' or 'mirror', but found '%s'." % segment_type)
    role = ROLE_PRIMARY if segment_type == 'primary' else ROLE_MIRROR

    # use preferred_role as that is the dbid that runs the recovery process
    all_segments = GpArray.initFromCatalog(dbconn.DbURL()).getDbList()
    segments = filter(lambda seg: seg.getSegmentPreferredRole() == role and seg.content >= 0, all_segments)
    for segment in segments:
        expected = r'\(dbid {}\): {}'.format(segment.dbid, output)
        check_stdout_msg(context, expected)

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
        f.write(expected[before].encode('utf-8'))
        f.flush()
        expected_before_gparray = GpArray.initFromFile(f.name)

    with tempfile.NamedTemporaryFile() as f:
        f.write(expected[after].encode('utf-8'))
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

@when('a gprecoverseg input file "{filename}" is created with a different data directory for content {content}')
def impl(context, filename, content):
    line = ""
    with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
        result = dbconn.query(conn, "SELECT port, hostname, datadir FROM gp_segment_configuration WHERE preferred_role='p' AND content = %s;" % content).fetchall()
        port, hostname, datadir = result[0][0], result[0][1], result[0][2]
        line = "%s|%s|%s %s|%s|/tmp/newdir" % (hostname, port, datadir, hostname, port)

    with open("/tmp/%s" % filename, "w") as fd:
        fd.write("%s\n" % line)

def find_backout_dirs():
    return glob.glob("%s/gpAdminLogs/gprecoverseg_*_backout" % os.path.expanduser("~"))

def get_current_backout_directory():
    dirs = find_backout_dirs()
    if len(dirs) == 0:
        raise Exception("No backout directory found in %s." % cdd)
    elif len(dirs) > 1:
        raise Exception("Multiple backout directories found in %s." % cdd)
    return dirs[0]

@given('a gprecoverseg backout file exists for content {content}')
def impl(context, content):
    backout_dir = get_current_backout_directory()
    if not os.path.exists("%s/revert_gprecoverseg_catalog_changes.sh" % backout_dir):
        raise Exception("The main coordinator backout script does not exist.")

    with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
        dbid = dbconn.querySingleton(conn, "SELECT dbid FROM gp_segment_configuration WHERE preferred_role='p' AND content = %s;" % content)
        if not os.path.exists("%s/backout_%s.sql" % (backout_dir, dbid)):
            raise Exception("The segment backout file for content %s does not exist." % content)

@when('the gprecoverseg backout script is run')
def impl(context):
    backout_dir = get_current_backout_directory()
    context.execute_steps(u'''
        Then the user runs command "chmod +x {backout_dir}/revert_gprecoverseg_catalog_changes.sh"
         And the user runs command "bash -c {backout_dir}/revert_gprecoverseg_catalog_changes.sh"
         And bash should return a return code of 0
         And user can start transactions
         '''.format(backout_dir=backout_dir))

@then('the primary for content {content} should have its original data directory in the system configuration')
def impl(context, content):
    with dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False) as conn:
        datadir = dbconn.querySingleton(conn, "SELECT datadir FROM gp_segment_configuration WHERE preferred_role='p' AND content = %s;" % content)
        if not datadir == context.remote_pair_primary_datadir:
            raise Exception("Expected datadir %s, got %s" % (context.remote_pair_primary_datadir, datadir))

@given('the gprecoverseg input file "{filename}" and all backout files are cleaned up')
@then('the gprecoverseg input file "{filename}" and all backout files are cleaned up')
def impl(context, filename):
    if os.path.exists(filename):
        os.remove(filename)
    dirs = find_backout_dirs()
    for backout_dir in dirs:
        if os.path.exists(backout_dir):
            shutil.rmtree(backout_dir)

@then('verify there are no gprecoverseg backout files')
def impl(context):
    dirs = find_backout_dirs()
    if len(dirs) > 0:
        raise Exception("One or more backout directories exist: %s" % dirs)

@then('the gprecoverseg lock directory is removed')
def impl(context):
    lock_dir = "%s/gprecoverseg.lock" % os.environ["COORDINATOR_DATA_DIRECTORY"]
    if os.path.exists(lock_dir):
        shutil.rmtree(lock_dir)

@then('the gp_configuration_history table should contain a backout entry for the {seg} segment for content {content}')
def impl(context, seg, content):
    role = ""
    if seg == "primary":
        role = 'p'
    elif seg == "mirror":
        role = 'm'
    else:
        raise Exception('Valid segment types are "primary" and "mirror"')

    with dbconn.connect(dbconn.DbURL(), unsetSearchPath=False) as conn:
        dbid = dbconn.querySingleton(conn, "SELECT dbid FROM gp_segment_configuration WHERE content = %s AND preferred_role = '%s'" % (content, role))
        num_tuples = dbconn.querySingleton(conn, "SELECT count(*) FROM gp_configuration_history WHERE dbid = %d AND gp_configuration_history.desc LIKE 'gprecoverseg: segment config for backout%%';" % dbid)
        if num_tuples != 1:
            raise Exception("Expected configuration history table to contain 1 entry for dbid %d, found %d" % (dbid, num_tuples))

# HACK; DELETE BEFORE MERGING
@given('coverage is installed')
def impl(context):
    with dbconn.connect(dbconn.DbURL(), unsetSearchPath=False) as conn:
        result = dbconn.query(conn, "SELECT distinct(hostname) FROM gp_segment_configuration")
        hosts = [r[0] for r in result]
        cmd = Command(name='Install coverage', cmdStr = "gpssh -h %s PIP_CACHE_DIR=${PIP_CACHE_DIR} pip3 --retries 10 install --user coverage" % (" -h ".join(hosts)))
        cmd.run(validateAfter=True)
