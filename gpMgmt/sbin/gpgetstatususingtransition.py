#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved. 
#
#
# THIS IMPORT MUST COME FIRST
# import mainUtils FIRST to get python version check
#
from gppylib.mainUtils import *
import os, sys

import json
import re

from optparse import Option, OptionGroup, OptionParser, OptionValueError

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib import gplog, gparray, pgconf
from gppylib.commands import base, gp, pg, unix
from gppylib.db import dbconn

logger = gplog.get_default_logger()

# Return codes for PQping(), exported by libpq and returned from pg_isready.
PQPING_OK = 0
PQPING_REJECT = 1
PQPING_NO_RESPONSE = 2
PQPING_NO_ATTEMPT = 3
PQPING_MIRROR_READY = 64

# When attempting to connect to a mirror, the postmaster will report the
# software version after a short prefix. This is that prefix, which must match
# that in <postmaster/postmaster.h>.
POSTMASTER_MIRROR_VERSION_DETAIL_MSG = "- VERSION:"

def _get_segment_status(segment):
    cmd = base.Command('pg_isready for segment',
                       "PGOPTIONS=\"-c gp_role=utility\" pg_isready -q -h %s -p %d -d %s" % (segment.hostname, segment.port, gp.PGDATABASE_FOR_COMMON_USE))
    cmd.run()

    rc = cmd.get_return_code()

    if rc == PQPING_OK:
        if segment.role == gparray.ROLE_PRIMARY:
            return 'Up'
        elif segment.role == gparray.ROLE_MIRROR:
            return 'Acting as Primary'
    elif rc == PQPING_REJECT:
        return 'Rejecting Connections'
    elif rc == PQPING_NO_RESPONSE:
        return 'Down'
    elif rc == PQPING_MIRROR_READY:
        if segment.role == gparray.ROLE_PRIMARY:
            return 'Acting as Mirror'
        elif segment.role == gparray.ROLE_MIRROR:
            return 'Up'

    return None

# Used by _get_segment_version() to find the version string for a mirror.
_version_regex = re.compile(r'%s (.*)' % POSTMASTER_MIRROR_VERSION_DETAIL_MSG)

def _get_segment_version(seg):
    try:
        if seg.role == gparray.ROLE_PRIMARY:
            dburl = dbconn.DbURL(hostname=seg.hostname, port=seg.port, dbname="template1")
            conn = dbconn.connect(dburl, utility=True)
            return dbconn.querySingleton(conn, "select version()")

        if seg.role == gparray.ROLE_MIRROR:
            cmd = base.Command("Try connecting to mirror",
                               "psql -h %s -p %s template1 -c 'select 1'"
                               %(seg.hostname, seg.port))
            cmd.run(validateAfter=False)
            if cmd.results.rc == 0:
                raise RuntimeError("Connection to mirror succeeded unexpectedly")

            stderr = cmd.results.stderr.splitlines()
            for line in stderr:
                match = _version_regex.match(line)
                if match:
                    return match.group(1)

            raise RuntimeError("Unexpected error from mirror connection: %s" % cmd.results.stderr)

        logger.error("Invalid role '%s' for dbid %d", seg.role, seg.dbid)
        return None

    except Exception as ex:
        logger.error("Could not get segment version for dbid %d", seg.dbid, exc_info=ex)
        return None

#
# todo: the file containing this should be renamed since it gets more status than just from transition
#
class GpSegStatusProgram:
    """

    Program to fetch status from the a segment(s).

    Multiple pieces of status information can be fetched in a single request by
         passing in multiple status request options on the command line

    """

    def __init__(self, options):
        self.__options = options
        self.__pool = None

    def getPidStatus(self, seg, pidRunningStatus):
        """
        returns a dict containing "pid" and "error" fields.  Note that
           the "error" field may be non-None even when pid is non-zero (pid if zero indicates
           unable to determine the pid).  This can happen if the pid is there in the
           lock file but not active on the port.

        The caller can rely on this to try to differentiate between an active pid and an inactive one

        """

        lockFileExists = pidRunningStatus['lockFileExists']
        ssPortActive = pidRunningStatus['ssPortActive']
        pidValue = pidRunningStatus['pidValue']

        lockFileName = gp.get_lockfile_name(seg.getSegmentPort())

        error = None
        if not lockFileExists and not ssPortActive:
            error = "No socket connection or lock file (%s) found for port %s" % (lockFileName, seg.getSegmentPort())
        elif not lockFileExists and ssPortActive:
            error = "No lock file %s but process running on port %s" % (lockFileName, seg.getSegmentPort())
        elif lockFileExists and not ssPortActive:
            error = "Have lock file %s but no process running on port %s" % (lockFileName, seg.getSegmentPort())
        else:
            if pidValue == 0:
                error = "Have lock file and process is active, but did not get a pid value" # this could be an assert?

        res = {}
        res['pid'] = pidValue
        res['error'] = error
        return res


    def getPidRunningStatus(self, seg):
        """
        Get an object containing various information about the postmaster pid's status
        """
        (postmasterPidFileExists, tempFileExists, lockFileExists, ssPortActive, pidValue) = \
                    gp.chk_local_db_running(seg.getSegmentDataDirectory(), seg.getSegmentPort())

        return {
            'postmasterPidFileExists' : postmasterPidFileExists,
            'tempFileExists' : tempFileExists,
            'lockFileExists' : lockFileExists,
            'ssPortActive' : ssPortActive,
            'pidValue' : pidValue
        }

    def run(self):

        if self.__options.statusQueryRequests is None:
            raise ProgramArgumentValidationException("-s argument not specified")
        if self.__options.dirList is None:
            raise ProgramArgumentValidationException("-D argument not specified")

        toFetch = self.__options.statusQueryRequests.split(":")
        segments = list(map(gparray.Segment.initFromString, self.__options.dirList))

        output = {}
        for seg in segments:
            pidRunningStatus = self.getPidRunningStatus(seg)

            outputThisSeg = output[seg.getSegmentDbId()] = {}
            for statusRequest in toFetch:
                data = None
                if statusRequest == gp.SEGMENT_STATUS__GET_VERSION:
                    data = _get_segment_version(seg)

                elif statusRequest == gp.SEGMENT_STATUS__GET_MIRROR_STATUS:
                    data = _get_segment_status(seg)
                    if data is not None:
                        data = {'databaseStatus': data}

                elif statusRequest == gp.SEGMENT_STATUS__GET_PID:
                    data = self.getPidStatus(seg, pidRunningStatus)
                
                elif statusRequest == gp.SEGMENT_STATUS__HAS_POSTMASTER_PID_FILE:
                    data = pidRunningStatus['postmasterPidFileExists']
                
                elif statusRequest == gp.SEGMENT_STATUS__HAS_LOCKFILE:
                    data = pidRunningStatus['lockFileExists']
                    
                else:
                    raise Exception("Invalid status request %s" % statusRequest )
                    
                outputThisSeg[statusRequest] = data

        status = '\nSTATUS_RESULTS:' + json.dumps(output)
        logger.info(status)

    def cleanup(self):
        if self.__pool:
            self.__pool.haltWork()

    @staticmethod
    def createParser():
        parser = OptParser(option_class=OptChecker,
                           description="Gets status from segments on a single host "
                                            "using a transition message.  Internal-use only.",
                           version='%prog version $Revision: #1 $')
        parser.setHelp([])

        addStandardLoggingAndHelpOptions(parser, True)

        addTo = parser
        addTo.add_option("-s", None, type="string",
                         dest="statusQueryRequests",
                         metavar="<statusQueryRequests>",
                         help="Status Query Message")
        addTo.add_option("-D", "--dblist", type="string", action="append",
                         dest="dirList",
                         metavar="<dirList>",
                         help="Directory List")

        parser.set_defaults()
        return parser

    @staticmethod
    def createProgram(options, args):
        if len(args) > 0 :
            raise ProgramArgumentValidationException(\
                            "too many arguments: only options may be specified", True)
        return GpSegStatusProgram(options)

#-------------------------------------------------------------------------
if __name__ == '__main__':
    mainOptions = { 'setNonuserOnToolLogger':True}
    simple_main( GpSegStatusProgram.createParser, GpSegStatusProgram.createProgram, mainOptions)
