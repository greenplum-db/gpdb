#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved. 
#
#
# THIS IMPORT MUST COME FIRST
# import mainUtils FIRST to get python version check
#
from gppylib.mainUtils import *
import os, sys

import pickle, base64

from optparse import Option, OptionGroup, OptionParser, OptionValueError

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib import gplog, gparray, pgconf
from gppylib.commands import base, gp, pg, unix
from gppylib.db import dbconn
from gppylib.utils import parseKeyColonValueLines

logger = gplog.get_default_logger()

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

    def getStatusUsingTransition(self, seg, statusRequest, currentPMPidData):
        """
        The data as from GpSegStatusProgram.getPidRunningStatus
        """
        if currentPMPidData is not None and \
            (currentPMPidData['pidValue'] == 0 or not currentPMPidData['lockFileExists'] or not currentPMPidData['netstatPortActive']):
            logger.warn("Error getting data from segment %s; it is not running" % seg.getSegmentDataDirectory())
            return None

        cmd = gp.SendFilerepTransitionStatusMessage("Check Status", statusRequest, seg.getSegmentDataDirectory(),
                                                seg.getSegmentPort())

        cmd.run()
        return cmd.unpackSuccessLine()

    def getPidStatus(self, seg, pidRunningStatus):
        """
        returns a dict containing "pid" and "error" fields.  Note that
           the "error" field may be non-None even when pid is non-zero (pid if zero indicates
           unable to determine the pid).  This can happen if the pid is there in the
           lock file but not active on the port.

        The caller can rely on this to try to differentiate between an active pid and an inactive one

        """

        lockFileExists = pidRunningStatus['lockFileExists']
        netstatPortActive = pidRunningStatus['netstatPortActive']
        pidValue = pidRunningStatus['pidValue']

        lockFileName = gp.get_lockfile_name(seg.getSegmentPort())

        error = None
        if not lockFileExists and not netstatPortActive:
            error = "No socket connection or lock file (%s) found for port %s" % (lockFileName, seg.getSegmentPort())
        elif not lockFileExists and netstatPortActive:
            error = "No lock file %s but process running on port %s" % (lockFileName, seg.getSegmentPort())
        elif lockFileExists and not netstatPortActive:
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
        (postmasterPidFileExists, tempFileExists, lockFileExists, netstatPortActive, pidValue) = \
                    gp.chk_local_db_running(seg.getSegmentDataDirectory(), seg.getSegmentPort())

        return {
            'postmasterPidFileExists' : postmasterPidFileExists,
            'tempFileExists' : tempFileExists,
            'lockFileExists' : lockFileExists,
            'netstatPortActive' : netstatPortActive,
            'pidValue' : pidValue
        }

    def __processMirrorStatusOutput(self, str):
        data = parseKeyColonValueLines(str)

        if data is None:
            return data

        # verify that all expected ones are there
        for expected in ["mode","segmentState","dataState", "postmasterState", "databaseStatus", "isFullResync",
                            "resyncNumCompleted","resyncTotalToComplete","estimatedCompletionTimeSecondsSinceEpoch",
                            "totalResyncObjectCount", "curResyncObjectCount", "changeTrackingBytesUsed"]:
            if expected not in data:
                logger.warn("Missing data key %s from str %s" % (expected, str))
                return None

        # convert some to long integers
        for toConvert in ["resyncNumCompleted","resyncTotalToComplete","estimatedCompletionTimeSecondsSinceEpoch",
                            "changeTrackingBytesUsed"]:
            value = data[toConvert]
            try:
                data[toConvert] = long(value)
            except ValueError:
                logger.warn("Invalid integer value %s from str %s" % (value, str))
                return None

        # convert some to booleans
        for toConvert in ["isFullResync"]:
            if data[toConvert] != "1" and data[toConvert] != "0":
                logger.warn("Invalid boolean str %s" % (str))
                return None
            data[toConvert] = (data[toConvert] == "1")

        return data

    def run(self):

        if self.__options.statusQueryRequests is None:
            raise ProgramArgumentValidationException("-s argument not specified")
        if self.__options.dirList is None:
            raise ProgramArgumentValidationException("-D argument not specified")

        toFetch = self.__options.statusQueryRequests.split(":")
        segments = map(gparray.Segment.initFromString, self.__options.dirList)

        output = {}
        for seg in segments:
            pidRunningStatus = self.getPidRunningStatus(seg)

            outputThisSeg = output[seg.getSegmentDbId()] = {}
            for statusRequest in toFetch:
                data = None
                if statusRequest == gp.SEGMENT_STATUS__GET_VERSION:
                    data = self.getStatusUsingTransition(seg, statusRequest, pidRunningStatus)
                    if data is not None:
                        data = data.rstrip()

                elif statusRequest == gp.SEGMENT_STATUS__GET_MIRROR_STATUS:
                    data = self.getStatusUsingTransition(seg, statusRequest, pidRunningStatus)
                    if data is not None:
                        data = self.__processMirrorStatusOutput(data)

                elif statusRequest == gp.SEGMENT_STATUS__GET_PID:
                    data = self.getPidStatus(seg, pidRunningStatus)
                
                elif statusRequest == gp.SEGMENT_STATUS__HAS_POSTMASTER_PID_FILE:
                    data = pidRunningStatus['postmasterPidFileExists']
                
                elif statusRequest == gp.SEGMENT_STATUS__HAS_LOCKFILE:
                    data = pidRunningStatus['lockFileExists']
                    
                else:
                    raise Exception("Invalid status request %s" % statusRequest )
                    
                outputThisSeg[statusRequest] = data

        status = '\nSTATUS_RESULTS:' + base64.urlsafe_b64encode(pickle.dumps(output))
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
