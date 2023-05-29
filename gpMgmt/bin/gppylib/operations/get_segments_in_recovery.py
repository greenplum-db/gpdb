#!/usr/bin/env python3

from gppylib import gplog
from gppylib.db.catalog import RemoteQueryCommand

logger = gplog.get_default_logger()


def is_seg_in_backup_mode(hostname, port):
    """
    To check if segment is already in backup mode. If yes, then differential recovery might be running already
    to recover it's mirror. And in that case the mirror should be skipped from being recovered again.
    Also if source is in backup already, pg_start_backup() will throw exception which is a mandatory operation to
    perform during differential recovery.
    Parameters:
        hostname: host name of source server
        port: port of source server
    Returns:
         boolean: true if backup is in progress for the segment
    """
    logger.debug(
        "Checking if backup is already in progress for the source server with host {} and port {}".format(
            hostname, port))

    sql = "SELECT pg_is_in_backup()"
    try:
        query_cmd = RemoteQueryCommand("pg_is_in_backup", sql, hostname, port)
        query_cmd.run()
        res = query_cmd.get_results()

    except Exception as e:
        raise Exception("Failed to query pg_is_in_backup() for segment with hostname {}, port {}, error: {}".format(
            hostname, str(port), str(e)))

    return res[0][0]
