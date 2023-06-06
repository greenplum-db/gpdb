#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved.
#
# import mainUtils FIRST to get python version check
# THIS IMPORT SHOULD COME FIRST
from gppylib.mainUtils import *

from optparse import OptionGroup
import sys, os
import re
import collections
import pgdb
from contextlib import closing
from gppylib import gparray, gplog
from gppylib.commands import base, gp
from gppylib.db import dbconn
from gppylib.gpparseopts import OptParser, OptChecker
from gppylib.operations.startSegments import *
from gppylib.operations.buildMirrorSegments import *
from gppylib.operations.buildMirrorSegments import get_recovery_progress_file, get_recovery_progress_pattern
from gppylib.system import configurationInterface as configInterface
from gppylib.system.environment import GpCoordinatorEnvironment
from gppylib.utils import TableLogger

logger = gplog.get_default_logger()

class FieldDefinition:
    """
    Represent a field of our data.  Note that we could infer columnName from name, but we would like
              for columnName to be more stable than "name"
    """
    def __init__(self, name, columnName, columnType, shortLabel=None):
        self.__name = name
        self.__columnName = columnName
        self.__columnType = columnType
        self.__shortLabel = shortLabel if shortLabel is not None else name

    def getName(self): return self.__name
    def getColumnName(self): return self.__columnName
    def getColumnType(self): return self.__columnType
    def getShortLabel(self): return self.__shortLabel

    #
    # __str__ needs to return naem -- we use this for output in some cases right now
    #
    def __str__(self): return self.__name

CATEGORY__SEGMENT_INFO = "Segment Info"
VALUE__HOSTNAME = FieldDefinition("Hostname", "hostname", "text")
VALUE__ADDRESS = FieldDefinition("Address", "address", "text")
VALUE__DATADIR = FieldDefinition("Datadir", "datadir", "text")
VALUE__PORT = FieldDefinition("Port", "port", "int")

CATEGORY__MIRRORING_INFO = "Mirroring Info"
VALUE__CURRENT_ROLE = FieldDefinition("Current role", "role", "text") # can't use current_role as name -- it's a reserved word
VALUE__PREFERRED_ROLE = FieldDefinition("Preferred role", "preferred_role", "text")
VALUE__MIRROR_STATUS = FieldDefinition("Mirror status", "mirror_status", "text")
VALUE__MIRROR_RECOVERY_START = FieldDefinition("Mirror recovery start", "mirror_recovery_start", "text")

CATEGORY__ERROR_GETTING_SEGMENT_STATUS = "Error Getting Segment Status"
VALUE__ERROR_GETTING_SEGMENT_STATUS = FieldDefinition("Error Getting Segment Status", "error_getting_status", "text")

CATEGORY__REPLICATION_INFO = "Replication Info"
VALUE__REPL_SENT_LSN = FieldDefinition("WAL Sent Location", "sent_lsn", "text")
VALUE__REPL_FLUSH_LSN = FieldDefinition("WAL Flush Location", "flush_lsn", "text")
VALUE__REPL_REPLAY_LSN = FieldDefinition("WAL Replay Location", "replay_lsn", "text")
VALUE__REPL_CURRENT_LSN = FieldDefinition("Current write location", "current_wal_lsn", "text")
VALUE__REPL_SENT_LEFT = FieldDefinition("Bytes remaining to send to mirror", "sent_left", "int")
VALUE__REPL_FLUSH_LEFT = FieldDefinition("Bytes received but remain to flush", "flush_left", "int")
VALUE__REPL_REPLAY_LEFT = FieldDefinition("Bytes received but remain to replay", "replay_left", "int")
VALUE__REPL_SYNC_REMAINING_BYTES = FieldDefinition("WAL sync remaining bytes", "wal_sync_bytes", "int")

VALUE_RECOVERY_COMPLETED_BYTES = FieldDefinition("Completed bytes (kB)", "recovery_completed_bytes", "int")
VALUE_RECOVERY_TOTAL_BYTES = FieldDefinition("Total bytes (kB)", "recovery_total_bytes", "int")
VALUE_RECOVERY_PERCENTAGE = FieldDefinition("Percentage completed", "recovery_percentage", "int")
VALUE_RECOVERY_TYPE = FieldDefinition("Recovery type", "recovery_type", "int")

CATEGORY__STATUS = "Status"
VALUE__COORDINATOR_REPORTS_STATUS = FieldDefinition("Configuration reports status as", "status_in_config", "text", "Config status")
VALUE__MIRROR_SEGMENT_STATUS = FieldDefinition("Segment status", "segment_status", "text") # must not be same name as VALUE__SEGMENT_STATUS
VALUE__NONMIRROR_DATABASE_STATUS = FieldDefinition("Database status", "database_status", "text")
VALUE__ACTIVE_PID = FieldDefinition("PID", "active_pid", "text") # int would be better, but we print error messages here sometimes

# these are not in a category, used for other logging
VALUE__SEGMENT_STATUS = FieldDefinition("Instance status", "instance_status", "text", "Status")
VALUE__DBID = FieldDefinition("dbid", "dbid", "int")
VALUE__CONTENTID = FieldDefinition("contentid", "contentid", "int")
VALUE__HAS_DATABASE_STATUS_WARNING = FieldDefinition("Has database status warning", "has_status_warning", "bool")
VALUE__VERSION_STRING = FieldDefinition("Version", "version", "text")

VALUE__POSTMASTER_PID_FILE_EXISTS = FieldDefinition("File postmaster.pid (boolean)", "postmaster_pid_file_exists", "bool")
VALUE__POSTMASTER_PID_VALUE_INT = FieldDefinition("PID from postmaster.pid file (int)", "postmaster_pid", "int", "pid file PID")
VALUE__LOCK_FILES_EXIST = FieldDefinition("Lock files in /tmp (boolean)", "lock_files_exist", "bool", "local files exist")
VALUE__ACTIVE_PID_INT = FieldDefinition("Active PID (int)", "active_pid", "int")

VALUE__POSTMASTER_PID_FILE = FieldDefinition("File postmaster.pid", "postmaster_pid_file_exists", "text", "pid file exists") # boolean would be nice
VALUE__POSTMASTER_PID_VALUE = FieldDefinition("PID from postmaster.pid file", "postmaster_pid", "text", "pid file PID") # int would be better, but we print error messages here sometimes
VALUE__LOCK_FILES= FieldDefinition("Lock files in /tmp", "lock_files_exist", "text", "local files exist") # boolean would be nice

class GpStateData:
    """
    Store key-value pairs of unpacked data for each segment in the cluster

    Also provides categories on top of this

    To add new values:
    1) add CATEGORY_* and VALUE* constants as appropriate
    2) update self.__categories and self.__entriesByCategories below
    3) call .addValue from the code that loads the values (search for it down below)

    """
    def __init__(self ):
        self.__segmentData = []
        self.__segmentDbIdToSegmentData = {}
        self.__dbIdIsProbablyDown = {}
        self.__contentsWithUpSegments = {}
        self.__currentSegmentData = None
        self.__categories = [
                    CATEGORY__SEGMENT_INFO,
                    CATEGORY__MIRRORING_INFO,
                    CATEGORY__ERROR_GETTING_SEGMENT_STATUS,
                    CATEGORY__REPLICATION_INFO,
                    CATEGORY__STATUS]
        self.__entriesByCategory = {}

        self.__entriesByCategory[CATEGORY__SEGMENT_INFO] = \
                [VALUE__HOSTNAME,
                VALUE__ADDRESS,
                VALUE__DATADIR,
                VALUE__PORT]

        self.__entriesByCategory[CATEGORY__MIRRORING_INFO] = \
                [VALUE__CURRENT_ROLE,
                VALUE__PREFERRED_ROLE,
                VALUE__MIRROR_STATUS,
                VALUE__MIRROR_RECOVERY_START]

        self.__entriesByCategory[CATEGORY__ERROR_GETTING_SEGMENT_STATUS] = \
                [VALUE__ERROR_GETTING_SEGMENT_STATUS]

        self.__entriesByCategory[CATEGORY__REPLICATION_INFO] = [
            VALUE__REPL_SENT_LSN,
            VALUE__REPL_FLUSH_LSN,
            VALUE__REPL_REPLAY_LSN,
            VALUE__REPL_CURRENT_LSN,
            VALUE__REPL_SENT_LEFT,
            VALUE__REPL_FLUSH_LEFT,
            VALUE__REPL_REPLAY_LEFT,
            VALUE__REPL_SYNC_REMAINING_BYTES,
        ]

        self.__entriesByCategory[CATEGORY__STATUS] = \
                [VALUE__ACTIVE_PID,
                VALUE__COORDINATOR_REPORTS_STATUS,
                VALUE__MIRROR_SEGMENT_STATUS,
                VALUE__NONMIRROR_DATABASE_STATUS]

        self.__allValues = {}
        for k in [VALUE__SEGMENT_STATUS, VALUE__DBID, VALUE__CONTENTID,
                    VALUE__HAS_DATABASE_STATUS_WARNING,
                    VALUE__VERSION_STRING, VALUE__POSTMASTER_PID_FILE_EXISTS, VALUE__LOCK_FILES_EXIST,
                    VALUE__ACTIVE_PID_INT, VALUE__POSTMASTER_PID_VALUE_INT,
                    VALUE__POSTMASTER_PID_FILE, VALUE__POSTMASTER_PID_VALUE, VALUE__LOCK_FILES,
                    VALUE_RECOVERY_COMPLETED_BYTES, VALUE_RECOVERY_TOTAL_BYTES, VALUE_RECOVERY_PERCENTAGE,
                    VALUE_RECOVERY_TYPE
                    ]:
            self.__allValues[k] = True

        for values in list(self.__entriesByCategory.values()):
            for v in values:
                self.__allValues[v] = True

    def beginSegment(self, segment):
        self.__currentSegmentData = {}
        self.__currentSegmentData["values"] = {}
        self.__currentSegmentData["isWarning"] = {}

        self.__segmentData.append(self.__currentSegmentData)
        self.__segmentDbIdToSegmentData[segment.getSegmentDbId()] = self.__currentSegmentData

    def switchSegment(self, segment):
        dbid = segment.getSegmentDbId()
        self.__currentSegmentData = self.__segmentDbIdToSegmentData[dbid]

    def addValue(self, key, value, isWarning=False):
        self.__currentSegmentData["values"][key] = value
        self.__currentSegmentData["isWarning"][key] = isWarning

        assert key in self.__allValues;

    def isClusterProbablyDown(self, gpArray):
        """
          approximate whether or not the cluster has a problem and need to review
          we could beef this up -- for example, the mirror is only useful
          if we are in resync mode
        """
        for seg in gpArray.getSegDbList():
            if seg.getSegmentContentId() not in self.__contentsWithUpSegments:
                return True
        return False

    def setSegmentProbablyDown(self, seg, peerPrimary, isThisSegmentProbablyDown):
        """
        Mark whether this segment is probably down (based on isThisSegmentProbablyDown)

        @param peerPrimary: if this is a mirror in file replication mode, this will be its primary
        """
        if isThisSegmentProbablyDown:
            self.__dbIdIsProbablyDown[seg.getSegmentDbId()] = True
        else:
            #
            # a segment is "good to use" for the cluster only if it's a primary, or a mirror whose
            #  primary says that they are in sync (primary not in changetracking or resync)
            #
            isGoodToUse = seg.isSegmentPrimary(current_role=True) or peerPrimary.isSegmentModeSynchronized()
            if isGoodToUse:
                self.__contentsWithUpSegments[seg.getSegmentContentId()] = True

    def isSegmentProbablyDown(self, seg):
        return seg.getSegmentDbId() in self.__dbIdIsProbablyDown

    def addSegmentToTableLogger(self, tabLog, segment, suppressCategories={}):
        """
        @param suppressCategories map from [categoryName-> true value] for category names that should be suppressed
        """
        for cat in self.__categories:
            if not suppressCategories.get(cat):
                keys = self.__entriesByCategory[cat]
                self.addSectionToTableLogger(tabLog, segment, cat, keys)

    def getStrValue(self, segment, key, defaultIfNone=""):
        data = self.__segmentDbIdToSegmentData[segment.getSegmentDbId()]
        valuesMap = data["values"]
        val = valuesMap.get(key)
        if val is None:
            val = defaultIfNone
        else:
            val = str(val)
        return val

    def addSectionToTableLogger(self, tabLog, segment, sectionHeader, keys, categoryIndent="", indent="   "):
        data = self.__segmentDbIdToSegmentData[segment.getSegmentDbId()]
        valuesMap = data["values"]
        isWarningMap = data["isWarning"]

        hasValue = False
        for k in keys:
            if k in valuesMap:
                hasValue = True
                break
        if not hasValue:
            #
            # skip sections for which we have no values!
            #
            return

        tabLog.info([categoryIndent + sectionHeader])
        for k in keys:
            if k in valuesMap:
                val = valuesMap[k]
                if val is None:
                    val = ""
                else:
                    val = str(val)
                tabLog.infoOrWarn(isWarningMap[k], ["%s%s" %(indent, k), "= %s" % val])


def replication_state_to_string(state):
    if state == 'backup':
        return 'Copying files from primary'
    else:
        return state.capitalize()


#-------------------------------------------------------------------------
class GpSystemStateProgram:

    #
    # Constructor:
    #
    # @param options the options as returned by the options parser
    #
    def __init__(self, options):
        self.__options = options
        self.__pool = None

    def __appendSegmentTripletToArray(self, segment, line):
        """
        returns line

        @param the line to which to append the triplet of address/datadir/port
        """
        line.append(segment.getSegmentAddress())
        line.append(segment.getSegmentDataDirectory())
        line.append(str(segment.getSegmentPort()))
        return line

    def __getMirrorType(self, gpArray):

        if gpArray.hasMirrors:
            if gpArray.guessIsSpreadMirror():
                return "Spread"
            else:
                return "Group"
        else:
            return "No Mirror"

    def __showClusterConfig(self, gpEnv, gpArray):
        """
        Returns the exitCode

        """
        if gpArray.hasMirrors:
            logger.info("-------------------------------------------------------------" )
            logger.info("-Current GPDB mirror list and status" )
            logger.info("-Type = %s" % self.__getMirrorType(gpArray) )
            logger.info("-------------------------------------------------------------" )

            primarySegments = [ seg for seg in gpArray.getSegDbList() if seg.isSegmentPrimary(False) ]
            mirrorSegments = [ seg for seg in gpArray.getSegDbList() if seg.isSegmentMirror(False) ]
            contentIdToMirror = GpArray.getSegmentsByContentId(mirrorSegments)

            tabLog = TableLogger().setWarnWithArrows(True)
            tabLog.info(["Status", "Data State", "Primary", "Datadir", "Port", "Mirror", "Datadir", "Port"])
            numUnsynchronized = 0
            numMirrorsActingAsPrimaries = 0
            for primary in primarySegments:
                mirror = contentIdToMirror[primary.getSegmentContentId()][0]

                doWarn = False
                status = ""
                if primary.isSegmentMirror(True):
                    actingPrimary = mirror
                    actingMirror = primary

                    actMirrorStatus = "Available" if actingMirror.isSegmentUp() else "Failed"
                    status = "Mirror Active, Primary %s" % (actMirrorStatus)

                    numMirrorsActingAsPrimaries += 1
                else:
                    actingPrimary = primary
                    actingMirror = mirror

                    actMirrorStatus = "Available" if actingMirror.isSegmentUp() else "Failed"
                    status = "Primary Active, Mirror %s" % (actMirrorStatus)

                if not actingPrimary.isSegmentModeSynchronized():
                    doWarn = True
                    numUnsynchronized += 1

                dataStatus = gparray.getDataModeLabel(actingPrimary.getSegmentMode())
                line = [status, dataStatus]
                self.__appendSegmentTripletToArray(primary, line)
                self.__appendSegmentTripletToArray(mirror, line)

                tabLog.infoOrWarn(doWarn, line)
            tabLog.outputTable()

            logger.info("-------------------------------------------------------------" )
            if numMirrorsActingAsPrimaries > 0:
                logger.warn( "%s segment(s) configured as mirror(s) are acting as primaries" % numMirrorsActingAsPrimaries )
            if numUnsynchronized > 0:
                logger.warn("%s primary segment(s) are not synchronized" % numUnsynchronized)

        else:
            logger.info("-------------------------------------------------------------" )
            logger.info("-Primary list [Mirror not used]")
            logger.info("-------------------------------------------------------------" )

            tabLog = TableLogger().setWarnWithArrows(True)
            tabLog.info(["Primary", "Datadir", "Port"])
            for seg in [ seg for seg in gpArray.getSegDbList()]:
                tabLog.info(self.__appendSegmentTripletToArray(seg, []))
            tabLog.outputTable()
            logger.info("-------------------------------------------------------------" )

        return 0

    def _showMirrorList(self,gpEnv, gpArray):
        """
        Returns the exitCode
        """
        exitCode = 0
        if gpArray.hasMirrors:
            tabLog = TableLogger().setWarnWithArrows(True)
            tabLog.info(["Mirror","Datadir", "Port", "Status", "Data Status", ""])

            # based off the bash version of -m "mirror list" option,
            #    the mirror list prints information about defined mirrors only
            mirrorSegments = [ seg for seg in gpArray.getSegDbList() if seg.isSegmentMirror(False) ]
            numMirrorsActingAsPrimaries = 0
            numFailedMirrors = 0
            numUnsynchronizedMirrors = 0
            for seg in mirrorSegments:
                doWarn = False
                status = ""
                dataStatus = gparray.getDataModeLabel(seg.getSegmentMode())
                if seg.isSegmentPrimary(True):
                    status = "Acting as Primary"

                    if not seg.isSegmentModeSynchronized():
                        numUnsynchronizedMirrors += 1

                    numMirrorsActingAsPrimaries += 1
                elif seg.isSegmentUp():
                    status = "Passive"
                else:
                    status = "Failed"
                    dataStatus = ""
                    doWarn = True
                    numFailedMirrors += 1

                if doWarn:
                    exitCode = 1

                line = self.__appendSegmentTripletToArray(seg, [])
                line.extend([status, dataStatus])

                tabLog.infoOrWarn(doWarn, line)

            logger.info("-------------------------------------------------------------" )
            logger.info("-Current GPDB mirror list and status" )
            logger.info("-Type = %s" % self.__getMirrorType(gpArray) )
            logger.info("-------------------------------------------------------------" )

            tabLog.outputTable()

            logger.info("-------------------------------------------------------------" )
            if numMirrorsActingAsPrimaries > 0:
                logger.warn( "%s segment(s) configured as mirror(s) are acting as primaries" % numMirrorsActingAsPrimaries )
            if numFailedMirrors > 0:
                logger.warn( "%s segment(s) configured as mirror(s) have failed" % numFailedMirrors )
            if numUnsynchronizedMirrors > 0:
                logger.warn( "%s mirror segment(s) acting as primaries are not synchronized" % numUnsynchronizedMirrors)

        else:
            logger.warn("-------------------------------------------------------------" )
            logger.warn( "Mirror not used")
            logger.warn("-------------------------------------------------------------" )

        return exitCode

    def __appendStandbySummary(self, hostNameToResults, standby, tabLog):
        """
        Log information about the configured standby and its current status
        """
        if standby is None:
            tabLog.info(["Coordinator standby", "= No coordinator standby configured"])
        else:
            tabLog.info(["Coordinator standby", "= %s" % standby.getSegmentHostName()])

            (standbyStatusFetchWarning, outputFromStandbyCmd) = hostNameToResults[standby.getSegmentHostName()]
            standbyData = outputFromStandbyCmd[standby.getSegmentDbId()] if standbyStatusFetchWarning is None else None

            if standbyStatusFetchWarning is not None:
                tabLog.warn(["Standby coordinator state", "= Status could not be determined: %s" % standbyStatusFetchWarning])

            elif standbyData[gp.SEGMENT_STATUS__HAS_POSTMASTER_PID_FILE] and \
                    standbyData[gp.SEGMENT_STATUS__GET_PID]['pid'] > 0 and \
                    standbyData[gp.SEGMENT_STATUS__GET_PID]['error'] is None:
                tabLog.info(["Standby coordinator state", "= Standby host passive"])

            else:
                tabLog.warn(["Standby coordinator state", "= Standby host DOWN"])

    def __showStatusStatistics(self, gpEnv, gpArray):
        """
        Print high-level numeric stats about the cluster

        returns the exit code
        """
        hostNameToResults = self.__fetchAllSegmentData(gpArray)

        logger.info("Greenplum instance status summary")

        # coordinator summary info
        tabLog = TableLogger().setWarnWithArrows(True)

        tabLog.addSeparator()
        tabLog.info(["Coordinator instance", "= Active"])

        self.__appendStandbySummary(hostNameToResults, gpArray.standbyCoordinator, tabLog)

        tabLog.info(["Total segment instance count from metadata", "= %s" % len(gpArray.getSegDbList())])
        tabLog.addSeparator()

        # primary and mirror segment info
        for whichType in ["Primary", "Mirror"]:
            tabLog.info(["%s Segment Status" % whichType])
            tabLog.addSeparator()

            if whichType == "Primary":
                segs = [seg for seg in gpArray.getSegDbList() if seg.isSegmentPrimary(current_role=False)]
            else:
                segs = [seg for seg in gpArray.getSegDbList() if seg.isSegmentMirror(current_role=False)]
                if not segs:
                    tabLog.info(["Mirrors not configured on this array"])
                    tabLog.addSeparator()
                    continue

            numPostmasterPidFilesMissing = 0
            numPostmasterProcessesMissing = 0
            numLockFilesMissing = 0
            numPostmasterPidsMissing = 0
            for seg in segs:
                (statusFetchWarning, outputFromCmd) = hostNameToResults[seg.getSegmentHostName()]
                if statusFetchWarning is not None:
                    # I guess if we can't contact the segment that we can do this?
                    # or should add a new error row instead to account for this?
                    numPostmasterPidFilesMissing += 1
                    numLockFilesMissing += 1
                    numPostmasterProcessesMissing += 1
                    numPostmasterPidsMissing += 1
                else:
                    segmentData = outputFromCmd[seg.getSegmentDbId()]
                    if not segmentData[gp.SEGMENT_STATUS__HAS_LOCKFILE]:
                        numLockFilesMissing += 1
                    if not segmentData[gp.SEGMENT_STATUS__HAS_POSTMASTER_PID_FILE]:
                        numPostmasterPidFilesMissing += 1

                    # note: this (which I think matches old behavior fairly closely)
                    #        doesn't seem entirely correct -- we are checking whether netstat is
                    #        there, but not really checking that the process is running on that port?
                    if segmentData[gp.SEGMENT_STATUS__GET_PID] is None or \
                            segmentData[gp.SEGMENT_STATUS__GET_PID]['pid'] == 0:
                        numPostmasterPidsMissing += 1
                        numPostmasterProcessesMissing += 1
                    elif segmentData[gp.SEGMENT_STATUS__GET_PID]['error'] is not None:
                        numPostmasterProcessesMissing += 1

            numSegments = len(segs)
            numValidAtCoordinator = len([seg for seg in segs if seg.isSegmentUp()])
            numFailuresAtCoordinator = len([seg for seg in segs if seg.isSegmentDown()])
            numPostmasterPidFilesFound = numSegments - numPostmasterPidFilesMissing
            numLockFilesFound = numSegments - numLockFilesMissing
            numPostmasterPidsFound = numSegments - numPostmasterPidsMissing
            numPostmasterProcessesFound = numSegments - numPostmasterProcessesMissing

            # print stuff
            tabLog.info(["Total %s segments" % whichType.lower(), "= %d" % numSegments])
            tabLog.info(["Total %s segment valid (at coordinator)" % whichType.lower(), "= %d" % numValidAtCoordinator])
            tabLog.infoOrWarn(numFailuresAtCoordinator > 0,
                      ["Total %s segment failures (at coordinator)" % whichType.lower(), "= %d" % numFailuresAtCoordinator])

            tabLog.infoOrWarn(numPostmasterPidFilesMissing > 0,
                      ["Total number of postmaster.pid files missing", "= %d" % numPostmasterPidFilesMissing])
            tabLog.info( ["Total number of postmaster.pid files found", "= %d" % numPostmasterPidFilesFound])

            tabLog.infoOrWarn(numPostmasterPidsMissing > 0,
                      ["Total number of postmaster.pid PIDs missing", "= %d" % numPostmasterPidsMissing])
            tabLog.info( ["Total number of postmaster.pid PIDs found", "= %d" % numPostmasterPidsFound])

            tabLog.infoOrWarn(numLockFilesMissing > 0,
                        ["Total number of /tmp lock files missing", "= %d" % numLockFilesMissing])
            tabLog.info( ["Total number of /tmp lock files found", "= %d" % numLockFilesFound])

            tabLog.infoOrWarn(numPostmasterProcessesMissing > 0,
                        ["Total number postmaster processes missing", "= %d" % numPostmasterProcessesMissing])
            tabLog.info( ["Total number postmaster processes found", "= %d" % numPostmasterProcessesFound])

            if whichType == "Mirror":
                numMirrorsActive = len([seg for seg in segs if seg.isSegmentPrimary(current_role=True)])
                numMirrorsPassive = numSegments - numMirrorsActive
                tabLog.infoOrWarn(numMirrorsActive > 0,
                            ["Total number mirror segments acting as primary segments", "= %d" % numMirrorsActive])
                tabLog.info( ["Total number mirror segments acting as mirror segments", "= %d" % numMirrorsPassive])

            tabLog.addSeparator()
        self.__showExpandStatusSummary(gpEnv, tabLog, showPostSep=True)
        tabLog.outputTable()

    def __fetchAllSegmentData(self, gpArray):
        """
        returns a dict mapping hostName to the GpGetSgementStatusValues decoded result
        """
        logger.info("Gathering data from segments...")
        segmentsByHost = GpArray.getSegmentsByHostName(gpArray.getDbList())
        hostNameToCmd = {}
        for hostName, segments in segmentsByHost.items():
            cmd = gp.GpGetSegmentStatusValues("get segment version status", segments,
                              [gp.SEGMENT_STATUS__GET_VERSION,
                                gp.SEGMENT_STATUS__GET_PID,
                                gp.SEGMENT_STATUS__HAS_LOCKFILE,
                                gp.SEGMENT_STATUS__HAS_POSTMASTER_PID_FILE,
                                gp.SEGMENT_STATUS__GET_MIRROR_STATUS
                                ],
                               verbose=logging_is_verbose(),
                               ctxt=base.REMOTE,
                               remoteHost=segments[0].getSegmentAddress())
            hostNameToCmd[hostName] = cmd
            self.__pool.addCommand(cmd)
        self.__poolWait()

        hostNameToResults = {}
        for hostName, cmd in hostNameToCmd.items():
            hostNameToResults[hostName] = cmd.decodeResults()
        return hostNameToResults

    def __showSummaryOfSegmentsWhichRequireAttention(self, gpEnv, gpArray):
        """
        Prints out the current status of the cluster.

        @param gpEnv the GpCoordinatorEnvironment object
        @param gpArray the array to display

        returns the exit code
        """
        exitCode = 0
        if not gpArray.hasMirrors:
            logger.info("Physical mirroring is not configured")
            return 1

        mirrorSegments = [ seg for seg in gpArray.getSegDbList() if seg.isSegmentMirror(current_role=True) ]
        contentIdToMirror = GpArray.getSegmentsByContentId(mirrorSegments)

        hostNameToResults = self.__fetchAllSegmentData(gpArray)
        data = self.__buildGpStateData(gpArray, hostNameToResults)

        def logSegments(segments, logAsPairs, additionalFieldsToLog=[]):
            """
            helper function for logging a list of primaries, with their mirrors

            @param logAsPairs if True, then segments should be primaries only, and we will log corresponding mirror datadir/port
            @param additionalFieldsToLog should be a list of FieldDefinition objects
            """
            tabLog = TableLogger().setWarnWithArrows(True)
            for segment in segments:
                if tabLog.getNumLines() == 0:
                    header = ["Current Primary" if logAsPairs else "Segment", "Port"]
                    header.extend([f.getShortLabel() for f in additionalFieldsToLog])
                    if logAsPairs:
                        header.extend(["Mirror", "Port"])
                    tabLog.info(header)

                line = []
                line.extend([segment.getSegmentAddress(), str(segment.getSegmentPort())])
                for key in additionalFieldsToLog:
                    line.append(data.getStrValue(segment, key))
                if logAsPairs:
                    mirror = contentIdToMirror[segment.getSegmentContentId()][0]
                    line.extend([mirror.getSegmentAddress(), str(mirror.getSegmentPort())])
                tabLog.info(line)
            tabLog.outputTable()


        logger.info("----------------------------------------------------")
        logger.info("Segment Mirroring Status Report")


        # segment pairs that are in wrong roles
        primariesInWrongRole = [s for s in gpArray.getSegDbList() if s.isSegmentPrimary(current_role=True) and \
                                                    not s.isSegmentPrimary(current_role=False)]
        if primariesInWrongRole:
            logger.info("----------------------------------------------------")
            logger.info("Segments with Primary and Mirror Roles Switched")
            logSegments(primariesInWrongRole, logAsPairs=True)
            exitCode = 1
        else:
            pass # logger.info( "No segment pairs with switched roles")

        # segments that are not synchronized
        unsync_segs = self._get_unsync_segs_add_wal_remaining_bytes(data, gpArray)
        if unsync_segs:
            logger.info("----------------------------------------------------")
            logger.info("Unsynchronized Segment Pairs")
            logSegments(unsync_segs, True, [VALUE__REPL_SYNC_REMAINING_BYTES])
            exitCode = 1
        else:
            pass # logger.info( "No segment pairs are in resynchronization")

        # segments that are down
        segmentsThatAreDown = [s for s in gpArray.getSegDbList() if data.isSegmentProbablyDown(s)]
        if segmentsThatAreDown:
            logger.info("----------------------------------------------------")
            logger.info("Downed Segments (may include segments where status could not be retrieved)")
            logSegments(segmentsThatAreDown, False, [VALUE__COORDINATOR_REPORTS_STATUS, VALUE__SEGMENT_STATUS])
            exitCode = 1
        else:
            pass # logger.info( "No segments are down")

        self.__addClusterDownWarning(gpArray, data)

        # Check if gprecoverseg process is running. We do not want to
        # show the progress if the gprecoverseg process is not running.
        if gp.is_gprecoverseg_running():
            recovery_progress_file = get_recovery_progress_file(gplog)
            segments_under_recovery = self._parse_recovery_progress_data(data, recovery_progress_file, gpArray)

            if segments_under_recovery:
                logger.info("----------------------------------------------------")
                logger.info("Segments in recovery")
                logSegments(segments_under_recovery, False, [VALUE_RECOVERY_TYPE, VALUE_RECOVERY_COMPLETED_BYTES, VALUE_RECOVERY_TOTAL_BYTES,
                                                            VALUE_RECOVERY_PERCENTAGE])
                exitCode = 1

        # final output -- no errors, then log this message
        if exitCode == 0:
            logger.info("----------------------------------------------------")
            logger.info("All segments are running normally")

        return exitCode

    def __addClusterDownWarning(self, gpArray, gpStateData):
        if gpStateData.isClusterProbablyDown(gpArray):
            logger.warn("*****************************************************" )
            logger.warn("DATABASE IS PROBABLY UNAVAILABLE" )
            logger.warn("Review Instance Status in log file or screen output for more information" )
            logger.warn("*****************************************************" )

    def __getSegmentStatusColumns(self):
        return [
                VALUE__DBID,
                VALUE__CONTENTID,

                VALUE__HOSTNAME,
                VALUE__ADDRESS,
                VALUE__DATADIR,
                VALUE__PORT,

                VALUE__CURRENT_ROLE,
                VALUE__PREFERRED_ROLE,
                VALUE__MIRROR_STATUS,
                VALUE__MIRROR_RECOVERY_START,

                VALUE__COORDINATOR_REPORTS_STATUS,
                VALUE__SEGMENT_STATUS,
                VALUE__HAS_DATABASE_STATUS_WARNING,

                VALUE__ERROR_GETTING_SEGMENT_STATUS,

                VALUE__POSTMASTER_PID_FILE_EXISTS,
                VALUE__POSTMASTER_PID_VALUE_INT,
                VALUE__LOCK_FILES_EXIST,
                VALUE__ACTIVE_PID_INT,

                VALUE__VERSION_STRING
                ]

    def __segmentStatusPipeSeparatedForTableUse(self, gpEnv, gpArray):
        """
        Print out the current status of the cluster (not including coordinator+standby) as a pipe separate list

        @param gpEnv the GpCoordinatorEnvironment object
        @param gpArray the array to display

        returns the exit code
        """
        hostNameToResults = self.__fetchAllSegmentData(gpArray)
        data = self.__buildGpStateData(gpArray, hostNameToResults)

        fields = self.__getSegmentStatusColumns()
        rows = [] # [[f.getName() for f in fields]]
        for seg in gpArray.getSegDbList():
            row = []
            for key in fields:
                row.append(data.getStrValue(seg, key, ""))
            rows.append(row)

        # output rows and fieldNames!
        self.__writePipeSeparated(rows, printToLogger=False)
        return 0


    def __showExpandProgress(self, gpEnv, st):
        st.get_progress()

        # group uncompleted tables by dbname
        uncompleted = collections.defaultdict(int)
        for dbname, _, _ in st.uncompleted:
            uncompleted[dbname] += 1

        tabLog = TableLogger().setWarnWithArrows(True)
        tabLog.addSeparator()
        tabLog.outputTable()
        tabLog = None

        if uncompleted:
            tabLog = TableLogger().setWarnWithArrows(True)
            logger.info("Number of tables to be redistributed")
            tabLog.info(["  Database", "Count of Tables to redistribute"])
            for dbname, count in uncompleted.items():
                tabLog.info(["  %s" % dbname, "%d" % count])
            tabLog.addSeparator()
            tabLog.outputTable()

        if st.inprogress:
            tabLog = TableLogger().setWarnWithArrows(True)
            logger.info("Active redistributions = %d" % len(st.inprogress))
            tabLog.info(["  Action", "Database", "Table"])
            for dbname, fq_name, _ in st.inprogress:
                tabLog.info(["  Redistribute", "%s" % dbname, "%s" % fq_name])
            tabLog.addSeparator()
            tabLog.outputTable()

    def __showExpandStatus(self, gpEnv):
        st = gp.get_gpexpand_status()

        if st.phase == 0:
            logger.info("Cluster Expansion State = No Expansion Detected")
        elif st.phase == 1:
            logger.info("Cluster Expansion State = Replicating Meta Data")
            logger.info("  Some database tools and functionality")
            logger.info("  are disabled during this process")
        else:
            assert st.phase == 2
            st.get_progress()

            if st.status == "SETUP DONE" or st.status == "EXPANSION STOPPED":
                logger.info("Cluster Expansion State = Data Distribution - Paused")
                self.__showExpandProgress(gpEnv, st)
            elif st.status == "EXPANSION STARTED":
                logger.info("Cluster Expansion State = Data Distribution - Active")
                self.__showExpandProgress(gpEnv, st)
            elif st.status == "EXPANSION COMPLETE":
                logger.info("Cluster Expansion State = Expansion Complete")
            else:
                # unknown phase2 status
                logger.info("Cluster Expansion State = Data Distribution - Unknown")

        return 0

    def __showExpandStatusSummary(self, gpEnv, tabLog, showPreSep=False, showPostSep=False):
        st = gp.get_gpexpand_status()

        if st.phase == 0:
            # gpexpand is not detected
            return

        if showPreSep:
            tabLog.addSeparator()

        tabLog.info(["Cluster Expansion", "= In Progress"])

        if showPostSep:
            tabLog.addSeparator()


    def __printSampleExternalTableSqlForSegmentStatus(self, gpEnv):
        scriptName = "%s/gpstate --segmentStatusPipeSeparatedForTableUse -q -d %s" % \
                        (sys.path[0], gpEnv.getCoordinatorDataDir()) # todo: ideally, would escape here
        columns = ["%s %s" % (f.getColumnName(), f.getColumnType()) for f in self.__getSegmentStatusColumns()]

        sql = "\nDROP EXTERNAL TABLE IF EXISTS gpstate_segment_status;\n\n\nCREATE EXTERNAL WEB TABLE gpstate_segment_status\n" \
              "(%s)\nEXECUTE '%s' ON COORDINATOR\nFORMAT 'TEXT' (DELIMITER '|' NULL AS '');\n" % \
               (", ".join(columns), scriptName )

        print(sql)

        return 0

    def __writePipeSeparated(self, rows, printToLogger=True):
        for row in rows:
            escapedRow = [s.replace("|", "_") for s in row]  # todo: can we escape it better?
            str = "|".join(escapedRow)
            if printToLogger:
                logger.info(str)
            else:
                print(str)

    def __showStatus(self, gpEnv, gpArray):
        """
        Prints out the current status of the cluster.

        @param gpEnv the GpCoordinatorEnvironment object
        @param gpArray the array to display

        returns the exit code
        """
        hasWarnings = False
        hostNameToResults = self.__fetchAllSegmentData(gpArray)

        #
        # fetch data about coordinator
        #
        coordinator = gpArray.coordinator

        dbUrl = dbconn.DbURL(port=gpEnv.getCoordinatorPort(), dbname='template1' )
        conn = dbconn.connect(dbUrl, utility=True)
        initDbVersion = dbconn.queryRow(conn, "select productversion from gp_version_at_initdb limit 1;")[0]
        pgVersion = dbconn.queryRow(conn, "show server_version;")[0]
        conn.close()

        try:
            # note: this is how old gpstate did this but ... can we do it without requiring a non-utility-mode
            #  connection?  non-utility-mode connections can take a long time to quit out if there
            #  are segment failures and you need to wait for the prober (and this would print
            #  role as "utility" even though it's really a failed-dispatcher.
            #
            # for now, we use Verbose=True so we don't try any statements on the connection during connect
            conn = dbconn.connect(dbUrl, utility=False, verbose=True)
            conn.close()
            qdRole = "dispatch"
        except Exception:
            qdRole = "utility" # unable to connect in non-utility, but we've been able to connect in utility so...
        #
        # print output about coordinator
        #
        (statusFetchWarning, outputFromCoordinatorCmd) = hostNameToResults[coordinator.getSegmentHostName()]
        coordinatorData = outputFromCoordinatorCmd[coordinator.getSegmentDbId()] if statusFetchWarning is None else None
        data = self.__buildGpStateData(gpArray, hostNameToResults)

        logger.info( "----------------------------------------------------" )
        logger.info("-Coordinator Configuration & Status")
        logger.info( "----------------------------------------------------" )

        self.__addClusterDownWarning(gpArray, data)

        tabLog = TableLogger().setWarnWithArrows(True)
        tabLog.info(["Coordinator host", "= %s" % coordinator.getSegmentHostName()])
        if statusFetchWarning is None:
            pidData = coordinatorData[gp.SEGMENT_STATUS__GET_PID]
            tabLog.info(["Coordinator postgres process ID", "= %s" % pidData['pid']])
        else:
            tabLog.warn(["Coordinator port", "= Error fetching data: %s" % statusFetchWarning])
        tabLog.info(["Coordinator data directory", "= %s" % coordinator.getSegmentDataDirectory()])
        tabLog.info(["Coordinator port", "= %d" % coordinator.getSegmentPort()])

        tabLog.info(["Coordinator current role", "= %s" % qdRole])
        tabLog.info(["Greenplum initsystem version", "= %s" % initDbVersion])

        if statusFetchWarning is None:
            if coordinatorData[gp.SEGMENT_STATUS__GET_VERSION] is None:
                tabLog.warn(["Greenplum current version", "= Unknown"])
            else:
                tabLog.info(["Greenplum current version", "= %s" % coordinatorData[gp.SEGMENT_STATUS__GET_VERSION]])
        else:
            tabLog.warn(["Greenplum current version", "= Error fetching data: %s" % statusFetchWarning])
        tabLog.info(["Postgres version", "= %s" % pgVersion])

        self.__appendStandbySummary(hostNameToResults, gpArray.standbyCoordinator, tabLog)
        tabLog.outputTable()
        hasWarnings = hasWarnings or tabLog.hasWarnings()

        #
        # Output about segments
        #
        logger.info("----------------------------------------------------")
        logger.info("Segment Instance Status Report")

        tabLog = TableLogger().setWarnWithArrows(True)
        categoriesToIgnoreWithoutMirroring = {CATEGORY__MIRRORING_INFO:True}
        for seg in gpArray.getSegDbList():
            tabLog.addSeparator()
            toSuppress = {} if gpArray.hasMirrors else categoriesToIgnoreWithoutMirroring
            data.addSegmentToTableLogger(tabLog, seg, toSuppress)
        self.__showExpandStatusSummary(gpEnv, tabLog, showPreSep=True, showPostSep=True)
        tabLog.outputTable()
        hasWarnings = hasWarnings or tabLog.hasWarnings()

        self.__addClusterDownWarning(gpArray, data)

        if hasWarnings:
            logger.warn("*****************************************************" )
            logger.warn("Warnings have been generated during status processing" )
            logger.warn("Check log file or review screen output" )
            logger.warn("*****************************************************" )

        return 1 if hasWarnings else 0

    @staticmethod
    def _parse_recovery_progress_data(data, recovery_progress_file, gpArray):
        """
        helper function for adding recovery segments progress to GpstateData
        :param gpArray: gpArray instance
        :param recovery_progress_file: progress file written by gprecoverseg on the coordinator
        returns recovery_progress_segs
        """
        if not os.path.exists(recovery_progress_file):
            return []

        recovery_progress_by_dbid = {}
        with open(recovery_progress_file, 'r') as fp:
            for line in fp:
                recovery_type, dbid, progress = line.strip().split(':',2)
                pattern = re.compile(get_recovery_progress_pattern())
                if re.search(pattern, progress):
                    bytes, units, precentage_str = progress.strip().split(' ',2)
                    completed_bytes, total_bytes = bytes.split('/')
                    percentage = re.search(r'(\d+\%)', precentage_str).group()
                    recovery_progress_by_dbid[int(dbid)] = [recovery_type, completed_bytes, total_bytes, percentage]

        # Now the catalog update happens before we run recovery,
        # so now when we query gpArray here, it will have new address/port for the recovering segments
        recovery_progress_segs = []
        for seg in gpArray.getSegDbList():
            dbid = seg.getSegmentDbId()
            if dbid in recovery_progress_by_dbid.keys():
                data.switchSegment(seg)
                recovery_progress_segs.append(seg)
                recovery_type, completed_bytes, total_bytes, percentage = recovery_progress_by_dbid[dbid]
                data.addValue(VALUE_RECOVERY_TYPE, recovery_type)
                data.addValue(VALUE_RECOVERY_COMPLETED_BYTES, completed_bytes)
                data.addValue(VALUE_RECOVERY_TOTAL_BYTES, total_bytes)
                data.addValue(VALUE_RECOVERY_PERCENTAGE, percentage)

        return recovery_progress_segs

    @staticmethod
    def _get_unsync_segs_add_wal_remaining_bytes(data, gpArray):
        """
        helper function for adding WAL sync remaining bytes to GpstateData and get unsync segments
        :param gpArray: gpArray instance
        returns list of primary segments of pairs which aren't in sync state
        """
        unsync_segs = []
        primaries = [s for s in gpArray.getSegDbList() if s.isSegmentPrimary(current_role=True)]
        for s in primaries:
            try:
                data.switchSegment(s)
                url = dbconn.DbURL(hostname=s.hostname, port=s.port, dbname='template1')
                conn = dbconn.connect(url, utility=True)
                with closing(conn) as conn:
                    cursor = dbconn.query(conn,
                                          "SELECT pg_wal_lsn_diff(pg_current_wal_flush_lsn(), sent_lsn)"
                                          ",sync_state FROM pg_stat_replication")
                    rows = cursor.fetchall()
                    cursor.close()
                    if rows:
                        # wal connection is active.
                        if rows[0][1] != 'sync':
                            # walsender is in 'catchup' state
                            wal_sync_bytes_out = rows[0][0]
                            unsync_segs.append(s)
                            data.addValue(VALUE__REPL_SYNC_REMAINING_BYTES, wal_sync_bytes_out)
                    else:
                        # no return value from pg_stat_replication, there isn't a replication connection
                        wal_sync_bytes_out = 'Unknown'
                        unsync_segs.append(s)
                        data.addValue(VALUE__REPL_SYNC_REMAINING_BYTES, wal_sync_bytes_out)
            except pgdb.InternalError:
                logger.warning('could not query segment {} ({}:{})'.format(
                    s.dbid, s.hostname, s.port
                ))
        return unsync_segs

    @staticmethod
    def _add_replication_info(data, primary, mirror):
        """
        Adds WAL replication information for a segment to GpStateData.

        If segment is a mirror, peer should be set to its corresponding primary.
        Otherwise, peer is ignored.
        """
        # Preload the mirror's replication info with Unknowns so that we'll
        # still have usable UI information on an early exit from this function.
        GpSystemStateProgram._set_mirror_replication_values(data, mirror)

        # Even though this information is considered part of the mirror's state,
        # we have to connect to the primary to get it.
        if not primary.isSegmentUp():
            return

        # Query pg_stat_replication for the info we want.
        rows = []
        rewind_start_time = None
        rewinding = False
        current_wal_lsn = None
        try:
            url = dbconn.DbURL(hostname=primary.hostname, port=primary.port, dbname='template1')
            conn = dbconn.connect(url, utility=True)

            with closing(conn) as conn:
                cursor = dbconn.query(conn,
                    "SELECT application_name, state, sent_lsn, "
                           "flush_lsn, "
                           "sent_lsn - flush_lsn AS flush_left, "
                           "replay_lsn, "
                           "sent_lsn - replay_lsn AS replay_left, "
                           "backend_start, "
                           "pg_current_wal_lsn() - sent_lsn AS sent_left "
                    "FROM pg_stat_replication;"
                )

                rows = cursor.fetchall()
                cursor.close()

                # We need this separately since pg_stat_replication may not return a value if WAL connection is down
                current_wal_lsn_cursor = dbconn.query(conn, "SELECT pg_current_wal_lsn();")
                current_wal_lsn_row = current_wal_lsn_cursor.fetchall()
                if current_wal_lsn_row:
                    current_wal_lsn = current_wal_lsn_row[0][0]
                current_wal_lsn_cursor.close()

                if mirror.isSegmentDown():
                    cursor = dbconn.query(conn,
                        "SELECT backend_start "
                        "FROM pg_stat_activity "
                        "WHERE application_name = '%s'" % gp.RECOVERY_REWIND_APPNAME
                    )

                    if cursor.rowcount > 0:
                        rewinding = True

                        if cursor.rowcount > 1:
                            logger.warning('pg_stat_activity has more than one rewinding mirror')

                        stat_activity_row = cursor.fetchone()
                        rewind_start_time = stat_activity_row[0]

                    cursor.close()

        except pgdb.InternalError:
            logger.warning('could not query segment {} ({}:{})'.format(
                    primary.dbid, primary.hostname, primary.port
            ))
            return

        # Successfully queried pg_stat_replication. If there are any backup
        # or pg_rewind connections, mention them in the primary status.
        state = None
        start_time = None
        backup_connections = [r for r in rows if r[1] == 'backup']

        if backup_connections:
            row = backup_connections[0]
            state = replication_state_to_string(row[1])
            start_time = row[7]
        elif rewinding:
            state = "Rewinding history to match primary timeline"
            start_time = rewind_start_time

        data.switchSegment(primary)
        if state:
            data.addValue(VALUE__MIRROR_STATUS, state)
        if start_time:
            data.addValue(VALUE__MIRROR_RECOVERY_START, start_time)

        data.addValue(VALUE__REPL_CURRENT_LSN, current_wal_lsn if current_wal_lsn else 'Unknown', isWarning=(not current_wal_lsn))

        # Set SENT_LEFT to unknown, and if we find a valid WAL connection, set it to correct value
        data.addValue(VALUE__REPL_SENT_LEFT, 'Unknown', isWarning=True)

        # Now fill in the information for the standby connection. There should
        # be exactly one such entry; otherwise we bail.
        standby_connections = [r for r in rows if r[0] == 'gp_walreceiver']
        if not standby_connections:
            logger.warning('pg_stat_replication shows no standby connections')
            return
        elif len(standby_connections) > 1:
            logger.warning('pg_stat_replication shows more than one standby connection')
            return

        row = standby_connections[0]

        sent_left = row[8]
        if sent_left is not None:
            data.addValue(VALUE__REPL_SENT_LEFT, sent_left, isWarning=False)

        GpSystemStateProgram._set_mirror_replication_values(data, mirror,
            state=row[1],
            sent_lsn=row[2],
            flush_lsn=row[3],
            flush_left=row[4],
            replay_lsn=row[5],
            replay_left=row[6],
        )

    @staticmethod
    def _set_mirror_replication_values(data, mirror, **kwargs):
        """
        Fill GpStateData with replication information for a mirror. We have to
        sanely handle not only the case where we couldn't connect, but also
        cases where pg_stat_replication is incomplete or contains unexpected
        data, so any or all of the keyword arguments may be None.

        This is tightly coupled to _add_replication_info()'s SQL query; see that
        implementation for the kwargs that may be passed.
        """
        data.switchSegment(mirror)

        state = kwargs.pop('state', None)
        sent_lsn = kwargs.pop('sent_lsn', None)
        flush_lsn = kwargs.pop('flush_lsn', None)
        flush_left = kwargs.pop('flush_left', None)
        replay_lsn = kwargs.pop('replay_lsn', None)
        replay_left = kwargs.pop('replay_left', None)

        if kwargs:
            raise TypeError('unexpected keyword argument {!r}'.format(list(kwargs.keys())[0]))

        if state:
            # Sharp eyes will notice that we may have already set the
            # replication status in the output at this point, but that was a
            # broad "synchronized vs. not synchronized" determination; we can do
            # better if we have access to pg_stat_replication.
            data.addValue(VALUE__MIRROR_STATUS, replication_state_to_string(state))

        mirror_wal_vars = [(VALUE__REPL_SENT_LSN, sent_lsn),
                (VALUE__REPL_FLUSH_LSN, flush_lsn),
                (VALUE__REPL_FLUSH_LEFT, flush_left),
                (VALUE__REPL_REPLAY_LSN, replay_lsn),
                (VALUE__REPL_REPLAY_LEFT, replay_left)]
        for key, val in mirror_wal_vars:
            data.addValue(key, val if val is not None else 'Unknown', isWarning=(val is None))

    def __buildGpStateData(self, gpArray, hostNameToResults):
        data = GpStateData()
        primaryByContentId = GpArray.getSegmentsByContentId(\
                                [s for s in gpArray.getSegDbList() if s.isSegmentPrimary(current_role=True)])

        segments = gpArray.getSegDbList()

        # Create segment entries in the state data.
        for seg in segments:
            data.beginSegment(seg)
            data.addValue(VALUE__DBID, seg.getSegmentDbId())
            data.addValue(VALUE__CONTENTID, seg.getSegmentContentId())
            data.addValue(VALUE__HOSTNAME, seg.getSegmentHostName())
            data.addValue(VALUE__ADDRESS, seg.getSegmentAddress())
            data.addValue(VALUE__DATADIR, seg.getSegmentDataDirectory())
            data.addValue(VALUE__PORT, seg.getSegmentPort())
            data.addValue(VALUE__CURRENT_ROLE, "Primary" if seg.isSegmentPrimary(current_role=True) else "Mirror")
            data.addValue(VALUE__PREFERRED_ROLE, "Primary" if seg.isSegmentPrimary(current_role=False) else "Mirror")

            if gpArray.hasMirrors:
                data.addValue(VALUE__MIRROR_STATUS, gparray.getDataModeLabel(seg.getSegmentMode()))
            else:
                data.addValue(VALUE__MIRROR_STATUS, "Physical replication not configured")

        # Add replication info on a per-pair basis.
        if gpArray.hasMirrors:
            for pair in gpArray.segmentPairs:
                primary, mirror = pair.primaryDB, pair.mirrorDB
                data.switchSegment(mirror)
                self._add_replication_info(data, primary, mirror)

        for seg in segments:
            data.switchSegment(seg)

            peerPrimary = None
            if gpArray.hasMirrors and seg.isSegmentMirror(current_role=True):
                peerPrimary = primaryByContentId[seg.getSegmentContentId()][0]

            (statusFetchWarning, outputFromCmd) = hostNameToResults[seg.getSegmentHostName()]
            if statusFetchWarning is not None:
                segmentData = None
                data.addValue(VALUE__ERROR_GETTING_SEGMENT_STATUS, statusFetchWarning)
            else:
                segmentData = outputFromCmd[seg.getSegmentDbId()]

                #
                # Able to fetch from that segment, proceed with PID status
                #
                pidData = segmentData[gp.SEGMENT_STATUS__GET_PID]

                found = segmentData[gp.SEGMENT_STATUS__HAS_POSTMASTER_PID_FILE]
                data.addValue(VALUE__POSTMASTER_PID_FILE, "Found" if found else "Missing", isWarning=not found)
                data.addValue(VALUE__POSTMASTER_PID_FILE_EXISTS, "t" if found else "f", isWarning=not found)

                # PID from postmaster.pid
                pidValueForSql = "" if pidData["pid"] == 0 else str(pidData["pid"])
                data.addValue(VALUE__POSTMASTER_PID_VALUE, pidData["pid"], pidData['pid'] == 0)
                data.addValue(VALUE__POSTMASTER_PID_VALUE_INT, pidValueForSql, pidData['pid'] == 0)

                # has lock file
                found = segmentData[gp.SEGMENT_STATUS__HAS_LOCKFILE]
                data.addValue(VALUE__LOCK_FILES, "Found" if found else "Missing", isWarning=not found)
                data.addValue(VALUE__LOCK_FILES_EXIST, "t" if found else "f", isWarning=not found)

                if pidData['error'] is None:
                    data.addValue(VALUE__ACTIVE_PID, abs(pidData["pid"]))
                    data.addValue(VALUE__ACTIVE_PID_INT, pidValueForSql)
                else:
                    data.addValue(VALUE__ACTIVE_PID, "Not found", True)
                    data.addValue(VALUE__ACTIVE_PID_INT, "", True)

                data.addValue(VALUE__VERSION_STRING, segmentData[gp.SEGMENT_STATUS__GET_VERSION])
            data.addValue(VALUE__COORDINATOR_REPORTS_STATUS, "Up" if seg.isSegmentUp() else "Down", seg.isSegmentDown())

            databaseStatus = None
            databaseStatusIsWarning = False

            if seg.isSegmentDown():
                databaseStatus = "Down in configuration"
                databaseStatusIsWarning = True
            elif segmentData is None:
                databaseStatus = "Unknown -- unable to load segment status"
                databaseStatusIsWarning = True
            elif segmentData[gp.SEGMENT_STATUS__GET_PID]['error'] is not None:
                databaseStatus = "Process error -- database process may be down"
                databaseStatusIsWarning = True
            elif segmentData[gp.SEGMENT_STATUS__GET_MIRROR_STATUS] is None:
                databaseStatus = "Unknown -- unable to load segment status"
                databaseStatusIsWarning = True
            else:
                databaseStatus = segmentData[gp.SEGMENT_STATUS__GET_MIRROR_STATUS]["databaseStatus"]
                databaseStatusIsWarning = databaseStatus != "Up"

            if seg.isSegmentMirror(current_role=True):
                data.addValue(VALUE__MIRROR_SEGMENT_STATUS, databaseStatus, databaseStatusIsWarning)
            else:
                data.addValue(VALUE__NONMIRROR_DATABASE_STATUS, databaseStatus, databaseStatusIsWarning)
            data.addValue(VALUE__SEGMENT_STATUS, databaseStatus, databaseStatusIsWarning)
            data.addValue(VALUE__HAS_DATABASE_STATUS_WARNING, "t" if databaseStatusIsWarning else "f", databaseStatusIsWarning)

            data.setSegmentProbablyDown(seg, peerPrimary, databaseStatusIsWarning)
        return data

    def __showQuickStatus(self, gpEnv, gpArray):

        exitCode = 0

        logger.info("-Quick Greenplum database status from Coordinator instance only")
        logger.info( "----------------------------------------------------------")

        segments = [seg for seg in gpArray.getDbList() if seg.isSegmentQE()]
        upSegments = [seg for seg in segments if seg.isSegmentUp()]
        downSegments = [seg for seg in segments if seg.isSegmentDown()]

        logger.info("# of up segments, from configuration table     = %s" % (len(upSegments)))
        if len(downSegments) > 0:
            exitCode = 1

            logger.info("# of down segments, from configuration table   = %s" % (len(downSegments)))

            tabLog = TableLogger().setWarnWithArrows(True)
            tabLog.info(["Down Segment", "Datadir", "Port"])
            for seg in downSegments:
                tabLog.info(self.__appendSegmentTripletToArray(seg, []))
            tabLog.outputTable()

        logger.info( "----------------------------------------------------------")

        return exitCode

    def __showPortInfo(self, gpEnv, gpArray):

        logger.info("-Coordinator segment instance  %s  port = %d" % (gpEnv.getCoordinatorDataDir(), gpEnv.getCoordinatorPort()))
        logger.info("-Segment instance port assignments")
        logger.info("----------------------------------")

        tabLog = TableLogger().setWarnWithArrows(True)
        tabLog.info([ "Host", "Datadir", "Port"])
        for seg in gpArray.getSegDbList():
            tabLog.info(self.__appendSegmentTripletToArray(seg, []))
        tabLog.outputTable()

    def __showStandbyCoordinatorInformation(self, gpEnv, gpArray):

        standby = gpArray.standbyCoordinator

        #
        # print standby configuration/status
        #
        if standby is None:
            logger.info("Standby coordinator instance not configured")
        else:
            cmd = gp.GpGetSegmentStatusValues("get standby segment version status", [standby],
                               [gp.SEGMENT_STATUS__GET_PID], verbose=logging_is_verbose(), ctxt=base.REMOTE,
                               remoteHost=standby.getSegmentAddress())
            cmd.run()

            # fetch standby pid
            (standbyPidFetchWarning, outputFromCmd) = cmd.decodeResults()
            if standbyPidFetchWarning is None:
                pidData = outputFromCmd[standby.getSegmentDbId()][gp.SEGMENT_STATUS__GET_PID]
            else:
                pidData = {}
                pidData['pid'] = 0
                pidData['error'] = None

            # Print output!
            logger.info("Standby coordinator details" )
            logger.info("----------------------" )
            tabLog = TableLogger().setWarnWithArrows(True)
            tabLog.info(["Standby address", "= %s" % standby.getSegmentAddress()])
            tabLog.info(["Standby data directory", "= %s" % standby.getSegmentDataDirectory()])
            tabLog.info(["Standby port", "= %s" % standby.getSegmentPort()])
            if standbyPidFetchWarning is not None:
                tabLog.warn(["Standby PID", "= %s" % standbyPidFetchWarning ])
                tabLog.warn(["Standby status", "= Status could not be determined"])
            elif pidData['pid'] == 0:
                tabLog.warn(["Standby PID", "= 0"])
                tabLog.warn(["Standby status", "= Standby process not running"])
            else:
                if pidData['error'] is not None:
                    #
                    # we got a pid value but had some kind of error -- so possibly the PID
                    #   is not actually active on its port.  Print the error
                    #
                    tabLog.warn(["Standby PID", "= %s" % pidData['pid'], "%s" % pidData['error']])
                    tabLog.warn(["Standby status", "= Status could not be determined" ])
                else:
                    tabLog.info(["Standby PID", "= %s" % pidData['pid']])
                    tabLog.info(["Standby status", "= Standby host passive" ])
            tabLog.outputTable()

        #
        # now print pg_stat_replication
        #
        logger.info("-------------------------------------------------------------" )
        logger.info("-pg_stat_replication" )
        logger.info("-------------------------------------------------------------" )

        dbUrl = dbconn.DbURL(port=gpEnv.getCoordinatorPort(), dbname='template1')
        conn = dbconn.connect(dbUrl, utility=True)
        sql = "SELECT state, sync_state, sent_lsn, flush_lsn, replay_lsn FROM pg_stat_replication"
        cur = dbconn.query(conn, sql)
        if cur.rowcount == 1:
            row = cur.fetchall()[0]
            logger.info("-WAL Sender State: %s" % row[0])
            logger.info("-Sync state: %s" % row[1])
            logger.info("-Sent Location: %s" % row[2])
            logger.info("-Flush Location: %s" % row[3])
            logger.info("-Replay Location: %s" % row[4])
        elif cur.rowcount > 1:
            logger.warning("pg_stat_replication shows more than 1 row.")
        else:
            logger.info("No entries found.")

        logger.info("-------------------------------------------------------------" )

        # done printing pg_stat_replication table

    def __poolWait(self):
        if self.__options.quiet:
            self.__pool.join()
        else:
            base.join_and_indicate_progress(self.__pool)

    def __showVersionInfo(self, gpEnv, gpArray):

        exitCode = 0

        logger.info("Loading version information")
        segmentsAndCoordinator = [seg for seg in gpArray.getDbList()]
        upSegmentsAndCoordinator = [seg for seg in segmentsAndCoordinator if seg.isSegmentUp()]

        # fetch from hosts
        segmentsByHost = GpArray.getSegmentsByHostName(upSegmentsAndCoordinator)
        for hostName, segments in segmentsByHost.items():
            cmd = gp.GpGetSegmentStatusValues("get segment version status", segments,
                               [gp.SEGMENT_STATUS__GET_VERSION],
                               verbose=logging_is_verbose(),
                               ctxt=base.REMOTE,
                               remoteHost=segments[0].getSegmentAddress())
            self.__pool.addCommand(cmd)

        self.__poolWait()

        # group output
        dbIdToVersion = {}
        uniqueVersions = {}
        for cmd in self.__pool.getCompletedItems():
            (warning, outputFromCmd) = cmd.decodeResults()
            if warning is None:
                for seg in cmd.dblist:
                    version = outputFromCmd[seg.getSegmentDbId()][gp.SEGMENT_STATUS__GET_VERSION]
                    if version is not None:
                        dbIdToVersion[seg.getSegmentDbId()] = version
                        uniqueVersions[version] = True
            else:
                logger.warn(warning)

        # print the list of all segments and warnings about trouble
        tabLog = TableLogger().setWarnWithArrows(True)
        tabLog.info(["Host","Datadir", "Port", "Version", ""])
        for seg in segmentsAndCoordinator:
            line = self.__appendSegmentTripletToArray(seg, [])
            version = dbIdToVersion.get(seg.getSegmentDbId())
            if version is None:
                line.append("unable to retrieve version")
                tabLog.warn(line)
            else:
                line.append(version)
                tabLog.info(line)
        tabLog.outputTable()

        if len(uniqueVersions) > 1:
            logger.warn("Versions for some segments do not match.  Review table above for details.")

        hadFailures = len(dbIdToVersion) != len(segmentsAndCoordinator)
        if hadFailures:
            logger.warn("Unable to retrieve version data from all segments.  Review table above for details.")

        if len(uniqueVersions) == 1 and not hadFailures:
            # if we got data from all segments then we are confident they are all the same version
            logger.info("All segments are running the same software version")

        self.__pool.empty_completed_items()

        return exitCode

    def run(self):

        # check that only one option is set
        numSet = (1 if self.__options.showMirrorList else 0) + \
                 (1 if self.__options.showClusterConfig else 0) + \
                 (1 if self.__options.showQuickStatus else 0) + \
                 (1 if self.__options.showStatus else 0) + \
                 (1 if self.__options.showStatusStatistics else 0) + \
                 (1 if self.__options.segmentStatusPipeSeparatedForTableUse else 0) + \
                 (1 if self.__options.printSampleExternalTableSqlForSegmentStatus else 0) + \
                 (1 if self.__options.showPortInformation else 0) + \
                 (1 if self.__options.showStandbyCoordinatorInformation else 0) + \
                 (1 if self.__options.showSummaryOfSegmentsWhichRequireAttention else 0) + \
                 (1 if self.__options.showVersionInfo else 0)
        if numSet > 1:
            raise ProgramArgumentValidationException("Too many output options specified")

        if self.__options.parallelDegree < 1 or self.__options.parallelDegree > 64:
            raise ProgramArgumentValidationException("Invalid parallelDegree provided with -B argument: %d" % self.__options.parallelDegree)

        self.__pool = base.WorkerPool(self.__options.parallelDegree)

        # load config
        gpEnv = GpCoordinatorEnvironment(self.__options.coordinatorDataDirectory, True, self.__options.timeout, self.__options.retries)
        confProvider = configInterface.getConfigurationProvider().initializeProvider(gpEnv.getCoordinatorPort())
        gpArray = confProvider.loadSystemConfig(useUtilityMode=True)

        # do it!
        if self.__options.showMirrorList:
            exitCode = self._showMirrorList(gpEnv, gpArray)
        elif self.__options.showClusterConfig:
            exitCode = self.__showClusterConfig(gpEnv, gpArray)
        elif self.__options.showQuickStatus:
            exitCode = self.__showQuickStatus(gpEnv, gpArray)
        elif self.__options.showStatus:
            exitCode = self.__showStatus(gpEnv, gpArray)
        elif self.__options.showVersionInfo:
            exitCode = self.__showVersionInfo(gpEnv, gpArray)
        elif self.__options.showSummaryOfSegmentsWhichRequireAttention:
            exitCode = self.__showSummaryOfSegmentsWhichRequireAttention(gpEnv, gpArray)
        elif self.__options.printSampleExternalTableSqlForSegmentStatus:
            exitCode = self.__printSampleExternalTableSqlForSegmentStatus(gpEnv)
        elif self.__options.showStandbyCoordinatorInformation:
            exitCode = self.__showStandbyCoordinatorInformation(gpEnv, gpArray)
        elif self.__options.showPortInformation:
            exitCode = self.__showPortInfo(gpEnv, gpArray)
        elif self.__options.segmentStatusPipeSeparatedForTableUse:
            exitCode = self.__segmentStatusPipeSeparatedForTableUse(gpEnv, gpArray)
        elif self.__options.showExpandStatus:
            exitCode = self.__showExpandStatus(gpEnv)
        else:
            # self.__options.showStatusStatistics OR default:
            exitCode = self.__showStatusStatistics(gpEnv, gpArray)

        return exitCode

    def cleanup(self):
        if self.__pool:
            self.__pool.haltWork()

    #-------------------------------------------------------------------------
    @staticmethod
    def createParser():

        description = ("Display system state")
        help = [""]

        parser = OptParser(option_class=OptChecker,
                    description=' '.join(description.split()),
                    version='%prog version $Revision$')
        parser.setHelp(help)

        addStandardLoggingAndHelpOptions(parser, True)

        addTo = OptionGroup(parser, "Connection Options")
        parser.add_option_group(addTo)
        addCoordinatorDirectoryOptionForSingleClusterProgram(addTo)

        addTo = OptionGroup(parser, "Output Options")
        parser.add_option_group(addTo)
        addTo.add_option('-m', None, default=False, action='store_true',
                            dest="showMirrorList",
                            metavar="<showMirrorList>",
                            help="Show mirror list from configuration")
        addTo.add_option('-c', None, default=False, action='store_true',
                            dest="showClusterConfig",
                            metavar="<showClusterConfig>",
                            help="Show cluster configuration")
        addTo.add_option("-Q", None, default=False, action="store_true",
                            dest="showQuickStatus",
                            metavar="<showQuickStatus>",
                            help="Show quick status")
        addTo.add_option("-s", None, default=False, action="store_true",
                            dest="showStatus",
                            metavar="<showStatus>",
                            help="Show status")
        addTo.add_option("-i", None, default=False, action="store_true",
                            dest="showVersionInfo",
                            metavar="<showVersionInfo>",
                            help="Show version information")
        addTo.add_option("-p", None, default=False, action="store_true",
                            dest="showPortInformation",
                            metavar="<showPortInformation>",
                            help="Show port information")
        addTo.add_option("-f", None, default=False, action="store_true",
                         dest="showStandbyCoordinatorInformation",
                         metavar="<showStandbyCoordinatorInformation>",
                         help="Show standby coordinator information")
        addTo.add_option("-b", None, default=False, action="store_true",
                         dest="showStatusStatistics",
                         metavar="<showStatusStatistics>",
                         help="Show status statistics")
        addTo.add_option("-e", None, default=False, action="store_true",
                         dest="showSummaryOfSegmentsWhichRequireAttention",
                         metavar="<showSummaryOfSegmentsWhichRequireAttention>",
                         help="Show summary of segments needing attention")
        addTo.add_option("-x", None, default=False, action="store_true",
                         dest="showExpandStatus",
                         metavar="<showExpandStatus>",
                         help="Show gpexpand status")

        #
        # two experimental options for exposing segment status as a queryable web table
        #
        addTo.add_option("--segmentStatusPipeSeparatedForTableUse", None, default=False, action="store_true",
                         dest="segmentStatusPipeSeparatedForTableUse",
                         metavar="<segmentStatusPipeSeparatedForTableUse>",
                         help="Show status as pipe separated output")
        addTo.add_option("--printSampleExternalTableSql", None, default=False, action="store_true",
                         dest="printSampleExternalTableSqlForSegmentStatus",
                         metavar="<printSampleExternalTableSqlForSegmentStatus>",
                         help="Print sample sql that can be run to create an external table on stop of gpstate --segmentStatusPipeSeparatedForTableUse")

        addTo = OptionGroup(parser, "Other Options")
        parser.add_option_group(addTo)
        addTo.add_option("-B", None, type="int", default=16,
                            dest="parallelDegree",
                            metavar="<parallelDegree>",
                            help="Max # of workers to use querying segments for status.  [default: %default]")
        addTo.add_option("--timeout", None, type="int", default=None,
                            dest="timeout",
                            metavar="<timeout>",
                            help="Database connection timeout. [default: %default]")
        addTo.add_option("--retries", None, type="int", default=None,
                            dest="retries",
                            metavar="<retries>",
                            help="Database connection retries. [default: %default]")

        parser.set_defaults()
        return parser

    @staticmethod
    def createProgram(options, args):
        if len(args) > 0 :
            raise ProgramArgumentValidationException(\
                            "too many arguments: only options may be specified", True)
        return GpSystemStateProgram(options)
