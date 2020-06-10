from time import sleep
from gppylib.commands.base import Command, ExecutionError, REMOTE, WorkerPool
from gppylib.db import dbconn
from gppylib.commands import gp
from gppylib.gparray import GpArray
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
    psql_cmd = "PGDATABASE=\'%s\' PGOPTIONS=\'-c gp_role=utility\' psql -t -h %s -p %s -c \"%s\"; " % (
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
    psql_cmd = "PGDATABASE=\'template1\' PGOPTIONS=\'-c gp_role=utility\' psql -h %s -p %s -c \"%s\"; " % (host, port, sql_cmd)
    Command(name='Running Remote command: %s' % psql_cmd, cmdStr = psql_cmd).run(validateAfter=True)

@then('gprecoverseg should print "{output}" to stdout for each mirror')
def impl(context, output):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    segments = gparray.getDbList()

    for segment in segments:
        if segment.isSegmentMirror():
            expected = r'\(dbid {}\): {}'.format(segment.dbid, output)
            check_stdout_msg(context, expected)

@then('pg_isready reports all primaries are accepting connections')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    primary_segs = [seg for seg in gparray.getDbList() if seg.isSegmentPrimary()]
    for seg in primary_segs:
        subprocess.check_call(['pg_isready', '-h', seg.getSegmentHostName(), '-p', str(seg.getSegmentPort())])
