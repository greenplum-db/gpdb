from gppylib import gplog
from gppylib.commands.base import Command
from gppylib.commands import base
from gppylib.gparray import STATUS_DOWN

logger = gplog.get_default_logger()


def get_unreachable_segment_hosts(hosts_excluding_master, num_workers):
    pool = base.WorkerPool(numWorkers=num_workers)
    try:
        for host in hosts_excluding_master:
            cmd = Command(name='check %s is up' % host, cmdStr="ssh %s 'echo %s'" % (host, host))
            pool.addCommand(cmd)
        pool.join()
    finally:
        pool.haltWork()
        pool.joinWorkers()

    # There's no good way to map a CommandResult back to its originating Command so instead
    # of looping through and finding the hosts that errored out, we remove any hosts that
    # succeeded from the hosts_excluding_master and any remaining hosts will be ones that were unreachable.
    for item in pool.getCompletedItems():
        result = item.get_results()
        if result.rc == 0:
            host = result.stdout.strip()
            hosts_excluding_master.remove(host)

    if len(hosts_excluding_master) > 0:
        logger.warning("One or more hosts are not reachable via SSH.  Any segments on those hosts will be marked down")
        for host in sorted(hosts_excluding_master):
            logger.warning("Host %s is unreachable" % host)
        return hosts_excluding_master
    return None


def mark_segments_down_for_unreachable_hosts(segmentPairs, unreachable_hosts):
    # We only mark the segment down in gparray for use by later checks, as
    # setting the actual segment down in gp_segment_configuration leads to
    # an inconsistent state and may prevent the database from starting.
    for segmentPair in segmentPairs:
        for seg in [segmentPair.primaryDB, segmentPair.mirrorDB]:
            host = seg.getSegmentHostName()
            if host in unreachable_hosts:
                logger.warning("Marking segment %d down because %s is unreachable" % (seg.dbid, host))
                seg.setSegmentStatus(STATUS_DOWN)
