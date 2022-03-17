from gppylib import gplog
from gppylib.commands.base import Command
from gppylib.commands import base
from gppylib.gparray import STATUS_DOWN

logger = gplog.get_default_logger()


def get_unreachable_segment_hosts(hosts, num_workers):
    if not hosts:
        return []

    pool = base.WorkerPool(numWorkers=num_workers)
    try:
        for host in set(hosts):
            cmd = Command(name='check %s is up' % host, cmdStr="ssh %s 'echo %s'" % (host, host))
            pool.addCommand(cmd)
        pool.join()
    finally:
        pool.haltWork()
        pool.joinWorkers()

    # There's no good way to map a CommandResult back to its originating Command.
    # To determine reachable hosts parse the stdout of the successful commands.
    reachable_hosts = set()
    for item in pool.getCompletedItems():
        result = item.get_results()
        if result.rc == 0:
            # Remove login(banner) messages
            host = result.stdout.strip().split('\n')[-1]
            reachable_hosts.add(host)

    unreachable_hosts = list(set(hosts).difference(reachable_hosts))
    unreachable_hosts.sort()
    if len(unreachable_hosts) > 0:
        logger.warning("One or more hosts are not reachable via SSH.")
        for host in sorted(unreachable_hosts):
            logger.warning("Host %s is unreachable" % host)

    return unreachable_hosts


def update_unreachable_flag_for_segments(gparray, unreachable_hosts):
    for i, segmentPair in enumerate(gparray.segmentPairs):
        if segmentPair.primaryDB and segmentPair.primaryDB.getSegmentHostName() in unreachable_hosts:
            gparray.segmentPairs[i].primaryDB.unreachable = True

        if segmentPair.mirrorDB and segmentPair.mirrorDB.getSegmentHostName() in unreachable_hosts:
            gparray.segmentPairs[i].mirrorDB.unreachable = True


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
