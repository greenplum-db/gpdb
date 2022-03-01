import os
import re
import socket

from gppylib import gplog
from gppylib.commands.base import Command, WorkerPool, REMOTE
from gppylib.commands import base, gp, unix
from gppylib.operations.initstandby import create_standby_pg_hba_entries

logger = gplog.get_default_logger()


class SegUpdateHba(Command):
    def __init__(self, entries, pghbaconf_dir, ctxt=REMOTE, remoteHost=None):
        entries_str = '\n'.join(entries)
        command_name = 'Update pg_hba.conf'
        cmdStr = "$GPHOME/sbin/seg_update_pg_hba.py --data-dir %s --entries '%s'" \
                 % (os.path.join(pghbaconf_dir), entries_str)

        Command.__init__(self, command_name, cmdStr, ctxt, remoteHost)


def create_entries(primary_hostname, mirror_hostname, hba_hostnames):
    # Start with an empty string so that the later .join prepends a newline to the first entry
    entries = ['']
    # Add the samehost replication entry to support single-host development
    entries.append('host replication {username} samehost trust'.format(username=unix.getUserName()))
    if hba_hostnames:
        mirror_hostname, _, _ = socket.gethostbyaddr(mirror_hostname)
        entries.append("host all {username} {hostname} trust".format(username=unix.getUserName(), hostname=mirror_hostname))
        entries.append("host replication {username} {hostname} trust".format(username=unix.getUserName(), hostname=mirror_hostname))
        primary_hostname, _, _ = socket.gethostbyaddr(primary_hostname)
        if mirror_hostname != primary_hostname:
            entries.append("host replication {username} {hostname} trust".format(username=unix.getUserName(), hostname=primary_hostname))
    else:
        segment_pair_ips = gp.IfAddrs.list_addrs(mirror_hostname)
        for ip in segment_pair_ips:
            cidr_suffix = '/128' if ':' in ip else '/32'
            cidr = ip + cidr_suffix
            hba_line_entry = "host all {username} {cidr} trust".format(username=unix.getUserName(), cidr=cidr)
            entries.append(hba_line_entry)
        if mirror_hostname != primary_hostname:
            segment_pair_ips.extend(gp.IfAddrs.list_addrs(primary_hostname))
        for ip in segment_pair_ips:
            cidr_suffix = '/128' if ':' in ip else '/32'
            cidr = ip + cidr_suffix
            hba_line_entry = "host replication {username} {cidr} trust".format(username=unix.getUserName(), cidr=cidr)
            entries.append(hba_line_entry)
    return entries

def update_on_segments(update_cmds, batch_size):

    num_workers = min(batch_size, len(update_cmds))
    pool = WorkerPool(num_workers)
    for uc in update_cmds:
        pool.addCommand(uc)
    try:
        pool.join()
    except Exception as e:
        pool.haltWork()
        pool.joinWorkers()
    failure = False
    for cmd in pool.getCompletedItems():
        r = cmd.get_results()
        if not cmd.was_successful():
           logger.error("Unable to update pg_hba conf on primary segment: " + str(r))
           failure = True

    pool.haltWork()
    pool.joinWorkers()
    if failure:
        logger.error("Unable to update pg_hba.conf on the primary segments")
        raise Exception("Unable to update pg_hba.conf on the primary segments.")


def update_pg_hba_on_segments_for_standby(gpArray, standby_host, hba_hostnames,
                                          batch_size):
    logger.info("Starting to create new pg_hba.conf on primary segments")
    update_cmds = []
    unreachable_seg_hosts = []

    standby_pg_hba_entries = create_standby_pg_hba_entries(standby_host, hba_hostnames)

    for segmentPair in gpArray.getSegmentList():
        # We cannot update the pg_hba.conf which uses ssh for hosts that are unreachable.
        primary_hostname = segmentPair.primaryDB.getSegmentHostName()
        if segmentPair.primaryDB.unreachable:
            unreachable_seg_hosts.append(primary_hostname)
            continue

        update_cmds.append(SegUpdateHba(standby_pg_hba_entries, segmentPair.primaryDB.datadir,
                                        remoteHost=primary_hostname))

    if unreachable_seg_hosts:
        logger.warning("Not updating pg_hba.conf for segments on unreachable hosts: %s."
                       "You can manually update pg_hba.conf once you make the hosts reachable."
                       % ', '.join(unreachable_seg_hosts))

    if not update_cmds:
        logger.info("None of the reachable segments require update to pg_hba.conf")
        return

    update_on_segments(update_cmds, batch_size)

    logger.info("Successfully modified pg_hba.conf on primary segments to allow standby connection")


def update_pg_hba_on_segments(gpArray, hba_hostnames, batch_size,
                              contents_to_update=None):
    logger.info("Starting to create new pg_hba.conf on primary segments")
    update_cmds = []
    unreachable_seg_hosts = []

    for segmentPair in gpArray.getSegmentList():

        if contents_to_update and not segmentPair.primaryDB.getSegmentContentId() in contents_to_update:
            continue

        # We cannot update the pg_hba.conf which uses ssh for hosts that are unreachable.
        if segmentPair.primaryDB.unreachable or not segmentPair.mirrorDB or segmentPair.mirrorDB.unreachable:
            unreachable_seg_hosts.append(segmentPair.primaryDB.getSegmentHostName())
            continue

        primary_hostname = segmentPair.primaryDB.getSegmentHostName()
        mirror_hostname = segmentPair.mirrorDB.getSegmentHostName()

        entries = create_entries(primary_hostname, mirror_hostname, hba_hostnames)

        update_cmds.append(SegUpdateHba(entries, segmentPair.primaryDB.datadir,
                                        remoteHost=primary_hostname))

    if unreachable_seg_hosts:
        logger.warning("Not updating pg_hba.conf for segments on unreachable hosts: %s."
                       "You can manually update pg_hba.conf once you make the hosts reachable."
                       % ', '.join(unreachable_seg_hosts))

    if not update_cmds:
        logger.info("None of the reachable segments require update to pg_hba.conf")
        return

    update_on_segments(update_cmds, batch_size)

    logger.info("Successfully modified pg_hba.conf on primary segments to allow replication connections")


def update_pg_hba_for_new_mirrors(PgHbaEntriesToUpdate, hba_hostnames, batch_size):
    update_cmds = []
    for primary_datadir, primary_hostname, newMirror_hostname in PgHbaEntriesToUpdate:
        entries = create_entries(primary_hostname, newMirror_hostname, hba_hostnames)
        update_cmds.append(SegUpdateHba(entries, primary_datadir,
                                        remoteHost=primary_hostname))
        logger.info("Updating pg_hba.conf entries on primary %s:%s with new mirror %s information"
                    % (primary_hostname, primary_datadir, newMirror_hostname))

    if not update_cmds:
        logger.info("None of the reachable segments require update to pg_hba.conf")
        return

    update_on_segments(update_cmds, batch_size)
