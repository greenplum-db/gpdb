from time import sleep
from gppylib.commands.base import Command, ExecutionError, REMOTE, WorkerPool
from gppylib.db import dbconn
from gppylib.commands import gp
from gppylib.gparray import GpArray
from gppylib.test.behave_utils.utils import run_gpcommand, getRows, getRow
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

@given('the mirror with content id "{cid}" is marked down in config')
@when('the mirror with content id "{cid}" is marked down in config')
@then('the mirror with content id "{cid}" is marked down in config')
def impl(context, cid):
    qry = """select count(*) from gp_segment_configuration where status='d' and content='%s' and role='m'""" % (cid)
    row_count = getRows('template1', qry)[0][0]
    if row_count != 1:
        raise Exception('Expected mirror segment cid %s to be down, but it is up.' % cid)

@given('user runs the command "{cmd}" on segment "{cid}"')
@when('user runs the command "{cmd}" on segment "{cid}"')
@then('user runs the command "{cmd}" on segment "{cid}"')
def impl(context, cmd, cid):
    dbid = getPrimaryDbIdFromCid(context, cid)
    cmdStr = '%s -s %s' % (cmd, int(dbid))
    cmd=Command(name='user command', cmdStr=cmdStr)
    cmd.run(validateAfter=True)

@given('segment with content "{cid}" has persistent tables that were rebuilt with mirrors disabled')
@when('segment with content "{cid}" has persistent tables that were rebuilt with mirrors disabled')
@then('segment with content "{cid}" has persistent tables that were rebuilt with mirrors disabled')
def impl(context, cid):
    mirror_state = '1' # 1 indicates mirrors are disabled
    add_persistent_query = '''select
    gp_add_persistent_relation_node_entry(NULL,tablespace_oid, database_oid,
    relfilenode_oid, segment_file_num, relation_storage_manager,
    persistent_state,create_mirror_data_loss_tracking_session_num, '%s',
    mirror_data_synchronization_state,
    mirror_bufpool_marked_for_scan_incremental_resync,
    mirror_bufpool_resync_changed_page_count, mirror_bufpool_resync_ckpt_loc,
    mirror_bufpool_resync_ckpt_block_num, mirror_append_only_loss_eof,
    mirror_append_only_new_eof, relation_bufpool_kind, parent_xid,
    persistent_serial_num, previous_free_tid) from (select
    ctid,tablespace_oid,database_oid,relfilenode_oid,
    segment_file_num,relation_storage_manager,
    persistent_state,create_mirror_data_loss_tracking_session_num,
    mirror_existence_state, mirror_data_synchronization_state,
    mirror_bufpool_marked_for_scan_incremental_resync,
    mirror_bufpool_resync_changed_page_count, mirror_bufpool_resync_ckpt_loc,
    mirror_bufpool_resync_ckpt_block_num, mirror_append_only_loss_eof,
    mirror_append_only_new_eof, relation_bufpool_kind, parent_xid,
    persistent_serial_num, previous_free_tid from gp_persistent_relation_node
    limit 1) as test; ''' % mirror_state
    runCommandOnRemoteSegment(context, cid, add_persistent_query)

@given('verify that segment with content "{cid}" is not recovered')
@when('verify that segment with content "{cid}" is not recovered')
@then('verify that segment with content "{cid}" is not recovered')
def impl(context, cid):
    primary_dbid = getPrimaryDbIdFromCid(context, cid)
    mirror_dbid = getMirrorDbIdFromCid(context, cid)
    output_msg ="Segments with dbid %s not recovered; persistent mirroring state is disabled." % primary_dbid
    check_stdout_msg(context, output_msg)
    if isSegmentUp(context, mirror_dbid):
        raise Exception('Expected mirror segment with dbid %s to be down, but it is up.' % mirror_dbid)

@given('verify that segment with content "{cid}" is recovered')
@when('verify that segment with content "{cid}" is recovered')
@then('verify that segment with content "{cid}" is recovered')
def impl(context, cid):
    primary_dbid = getPrimaryDbIdFromCid(context, cid)
    mirror_dbid = getMirrorDbIdFromCid(context, cid)
    output_msg ="Segments with dbid %s not recovered; persistent rebuild mirroring state is disabled." % primary_dbid
    check_string_not_present_stdout(context, output_msg)
    if not isSegmentUp(context, mirror_dbid):
        raise Exception('Expected mirror segment with dbid %s to be up, but it is down.' % mirror_dbid)

@given('delete extra tid persistent table entries on cid "{cid}"')
@when('delete extra tid persistent table entries on cid "{cid}"')
@then('delete extra tid persistent table entries on cid "{cid}"')
def impl(context, cid):
    remove_extra_tid_entry_sql = '''
    select gp_delete_persistent_relation_node_entry(ctid) from (select ctid from gp_persistent_relation_node where mirror_existence_state=1) as ctid;
    '''
    runCommandOnRemoteSegment(context, cid, remove_extra_tid_entry_sql)

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
