import os
import socket

from gppylib import gplog
from gppylib.commands.base import Command
from gppylib.commands import base, gp, unix

logger = gplog.get_default_logger()


def config_primaries_for_replication(gpArray, hba_hostnames, contents_to_update=None):
    logger.info("Starting to modify pg_hba.conf on primary segments to allow replication connections")

    try:
        for segmentPair in gpArray.getSegmentList():
            # We cannot update the pg_hba.conf which uses ssh for hosts that are unreachable.
            if segmentPair.primaryDB.unreachable or segmentPair.mirrorDB.unreachable:
                continue
            if contents_to_update and not segmentPair.primaryDB.getSegmentContentId() in contents_to_update:
                continue

            # Start with an empty string so that the later .join prepends a newline to the first entry
            entries = ['']
            segment_pair_ips = []
            # Add the samehost replication entry to support single-host development
            entries.append('host  replication {username} samehost trust'.format(username=unix.getUserName()))
            if hba_hostnames:
                mirror_hostname, _, _ = socket.gethostbyaddr(segmentPair.mirrorDB.getSegmentHostName())
                entries.append("host all {username} {hostname} trust".format(username=unix.getUserName(), hostname=mirror_hostname))
                entries.append("host replication {username} {hostname} trust".format(username=unix.getUserName(), hostname=mirror_hostname))
                primary_hostname, _, _ = socket.gethostbyaddr(segmentPair.primaryDB.getSegmentHostName())
                if mirror_hostname != primary_hostname:
                    entries.append("host replication {username} {hostname} trust".format(username=unix.getUserName(), hostname=primary_hostname))
            else:
                mirror_hostname = segmentPair.mirrorDB.getSegmentHostName()
                segment_pair_ips = gp.IfAddrs.list_addrs(mirror_hostname)
                for ip in segment_pair_ips:
                    cidr_suffix = '/128' if ':' in ip else '/32'
                    cidr = ip + cidr_suffix
                    hba_line_entry = "host all {username} {cidr} trust".format(username=unix.getUserName(), cidr=cidr)
                    entries.append(hba_line_entry)
                primary_hostname = segmentPair.primaryDB.getSegmentHostName()
                if mirror_hostname != primary_hostname:
                    segment_pair_ips.extend(gp.IfAddrs.list_addrs(primary_hostname))
                for ip in segment_pair_ips:
                    cidr_suffix = '/128' if ':' in ip else '/32'
                    cidr = ip + cidr_suffix
                    hba_line_entry = "host replication {username} {cidr} trust".format(username=unix.getUserName(), cidr=cidr)
                    entries.append(hba_line_entry)
            cmdStr = ". {gphome}/greenplum_path.sh; echo '{entries}' >> {datadir}/pg_hba.conf; pg_ctl -D {datadir} reload".format(
                gphome=os.environ["GPHOME"],
                entries="\n".join(entries),
                datadir=segmentPair.primaryDB.datadir)
            logger.debug(cmdStr)
            cmd = Command(name="append to pg_hba.conf", cmdStr=cmdStr, ctxt=base.REMOTE, remoteHost=segmentPair.primaryDB.hostname)
            cmd.run(validateAfter=True)

    except Exception as e:
        logger.error("Failed while modifying pg_hba.conf on primary segments to allow replication connections: %s" % str(e))
        raise

    else:
        logger.info("Successfully modified pg_hba.conf on primary segments to allow replication connections")
