#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
# Greenplum DB related utility functions

import os
from contextlib import closing

from gppylib.commands.base import Command
from gppylib.commands.pg import PgControlData
from gppylib.commands import base
from gppylib.db import dbconn
from gppylib import gplog

logger = gplog.get_default_logger()

def get_gp_prefix(masterDatadir):
    base = os.path.basename(masterDatadir)
    idx = base.rfind('-1')
    if idx == -1:
        return None
    return base[0:idx]

# returns startup recovery remaining bytes
def get_startup_recovery_remaining_bytes(hostname, port, datadir):
    """
        To get XLOG file that is being currently recovered, grep the startup process of the segment.
        the process looks like below and contains the walfilename
        gpadmin  30271 30184 90 19:04 ?        00:29:06 postgres:  7001, startup   recovering 000000030000052D0000000C
    """

    string = 'postgres:\s+[0-9]+,\s+startup\s+recovering\s+[0-9A-F]{24}'
    cmd_str = "ps -ef | grep -v grep | grep '{}' | grep -Eo '{}'".format(port, string)
    cmd = Command(name='Running Remote command: {}'.format(cmd_str), cmdStr=cmd_str, ctxt=base.REMOTE, remoteHost=hostname)
    cmd.run(validateAfter=False)
    if cmd.get_return_code() != 0:
        if cmd.get_return_code() != 1:
            logger.debug("Command {} failed. err: {}".format(cmd_str, cmd.get_stderr()))
        else:
            logger.debug("No startup recovery process running")
        return

    recovery_walfile = cmd.get_stdout().split(" ")[-1]
    if len(recovery_walfile) != 24:
        logger.debug("Could not fetch startup recovery xlog filename")
        return

    # Get the 'Minimum recovery ending location' from mirror pg_controldata
    cmd = PgControlData(name='run pg_controldata', datadir=datadir, ctxt=base.REMOTE, remoteHost=hostname)
    cmd.run(validateAfter=False)
    if cmd.get_return_code() != 0:
        logger.debug("Could not fetch 'Minimum recovery ending location' from pg_controldata")
        return
    min_recovery_ending_location = cmd.get_value('Minimum recovery ending location')  # value can be possibly None?

    try:
        with closing(dbconn.connect(dbconn.DbURL())) as conn:
            wal_segment_size_bytes = dbconn.execSQLForSingleton(conn, "SELECT ps.setting::int FROM pg_show_all_settings() "
                                                                 "ps WHERE ps.name = 'wal_segment_size';")
            min_recovery_ending_location_xlogfile = dbconn.execSQLForSingleton(conn, "select pg_xlogfile_name('{}')".format(min_recovery_ending_location))
    except Exception as e:
        logger.debug("Failed to get either wal_segment_size or walfile_name of Minimum recovery ending location, "
                     "err: {}".format(str(e)))
        return

    try:
        current_xlogfile_bytes = split_walfile_name(recovery_walfile, wal_segment_size_bytes)[1]*wal_segment_size_bytes
        final_xlogfile_bytes = split_walfile_name(min_recovery_ending_location_xlogfile, wal_segment_size_bytes)[1]*wal_segment_size_bytes
    except Exception as e:
        logger.debug("Failed to split walfile_name, err: {}".format(str(e)))
        return

    startup_recovery_remaining_bytes = final_xlogfile_bytes - current_xlogfile_bytes

    return startup_recovery_remaining_bytes


# GPDB_16_MERGE_FIXME: Use pg_split_walfile_name() when it becomes available and remove below function.
def split_walfile_name(fname, wal_segment_size):
    # Convert fname to uppercase, simulating pg_toupper behavior
    fname_upper = fname.upper()

    XLOG_FNAME_LEN = 24

    # Simulate IsXLogFileName function
    def is_xlog_file_name(fname):
        valid_chars = set("0123456789ABCDEF")
        return len(fname) == XLOG_FNAME_LEN and all(char in valid_chars for char in fname)

    if not is_xlog_file_name(fname_upper):
        raise ValueError("Invalid XLOG file name \"{}\"".format(fname))

    # Simulate the XLogFromFileName function to extract tli and segno
    # Assuming wal_segment_size is provided
    def xlog_from_file_name(fname, wal_segsz_bytes):
        tli = int(fname[:8], 16)
        log = int(fname[8:16], 16)
        seg = int(fname[16:24], 16)
        segno = log * (0x100000000 // wal_segsz_bytes) + seg
        return tli, segno

    tli, segno = xlog_from_file_name(fname_upper, wal_segment_size)

    return tli, segno
