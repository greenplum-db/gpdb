from time import sleep
from gppylib.commands.base import Command, ExecutionError, REMOTE, WorkerPool
from gppylib.db import dbconn
from gppylib.commands import gp
from gppylib.gparray import GpArray
from gppylib.test.behave_utils.utils import run_gpcommand, getRows
import platform

@given('the information of a "{seg}" segment on a remote host is saved')
@when('the information of a "{seg}" segment on a remote host is saved')
@then('the information of a "{seg}" segment on a remote host is saved')
def impl(context, seg):
    if seg == "mirror":
        gparray = GpArray.initFromCatalog(dbconn.DbURL())
        mirror_segs = [seg for seg in gparray.getDbList() if seg.isSegmentMirror() and seg.getSegmentHostName() != platform.node()]
        context.remote_mirror_segdbId = mirror_segs[0].getSegmentDbId()
        context.remote_mirror_segcid = mirror_segs[0].getSegmentContentId()
        context.remote_mirror_segdbname = mirror_segs[0].getSegmentHostName()
        context.remote_mirror_datadir = mirror_segs[0].getSegmentDataDirectory()

@given('wait until the segment state of the corresponding primary goes in ChangeTrackingDisabled')
@when('wait until the segment state of the corresponding primary goes in ChangeTrackingDisabled')
@then('wait until the segment state of the corresponding primary goes in ChangeTrackingDisabled')
def impl(context):
    cmd = gp.SendFilerepTransitionStatusMessage(name='Get segment status',
			      msg=gp.SEGMENT_STATUS_GET_STATUS,
			      dataDir=context.remote_pair_primary_datadir,
			      port=context.remote_pair_primary_port,
			      ctxt=gp.REMOTE,
			      remoteHost=context.remote_pair_primary_host)
    # wait for segment state of the corresponding primary segment to complete its transition
    # the timeout depends on how soon the current cluster can complete this transition(eg: networking).
    max_try = 5
    while max_try > 0:
        cmd.run(validateAfter=False)
        if 'ChangeTrackingDisabled' in cmd.get_results().stderr:
            break
        sleep(10)
        max_try = max_try - 1
    if max_try == 0:
        raise Exception('Failed to inject segment id %s into change tracking disabled state' % context.remote_pair_primary_segdbId)

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

@given('user runs the command "{cmd}" with the saved "{seg}" segment option')
@when('user runs the command "{cmd}" with the saved "{seg}" segment option')
@then('user runs the command "{cmd}" with the saved "{seg}" segment option')
def impl(context, cmd, seg):
    if seg == "mirror":
        dbid = int(context.remote_mirror_segdbId)
    else:
        dbid = int(context.remote_pair_primary_segdbId)
    cmdStr = '%s -s %s' % (cmd, int(dbid))
    cmd=Command(name='user command', cmdStr=cmdStr)
    cmd.run(validateAfter=True)

@given('the saved mirror segment process is still running on that host')
@when('the saved mirror segment process is still running on that host')
@then('the saved mirror segment process is still running on that host')
def impl(context):
    cmd = """ps ux | grep "/bin/postgres \-D %s " | grep -v grep""" % (context.remote_mirror_datadir)
    cmd=Command(name='user command', cmdStr=cmd, ctxt=REMOTE, remoteHost=context.remote_mirror_segdbname)
    cmd.run(validateAfter=True)
    res = cmd.get_results()
    if not res.stdout.strip():
        raise Exception('Mirror segment "%s" not active on "%s"' % (context.remote_mirror_datadir, context.remote_mirror_segdbname))
    
@given('the saved mirror segment is marked down in config')
@when('the saved mirror segment is marked down in config')
@then('the saved mirror segment is marked down in config')
def impl(context):
    qry = """select count(*) from gp_segment_configuration where status='d' and hostname='%s' and dbid=%s""" % (context.remote_mirror_segdbname, context.remote_mirror_segdbId)
    row_count = getRows('template1', qry)[0][0]
    if row_count != 1:
        raise Exception('Expected mirror segment %s on host %s to be down, but it is running.' % (context.remote_mirror_datadir, context.remote_mirror_segdbname))
