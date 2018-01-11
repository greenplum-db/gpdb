#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2010. All Rights Reserved.
#
#
# Used to inject faults into the file replication code
#

#
# THIS IMPORT MUST COME FIRST
#
# import mainUtils FIRST to get python version check
from gppylib.mainUtils import *

from optparse import Option, OptionGroup, OptionParser, OptionValueError, SUPPRESS_USAGE

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib.utils import toNonNoneString
from gppylib import gplog
from gppylib import gparray
from gppylib.commands import base
from gppylib.commands import unix
from gppylib.commands import gp
from gppylib.commands import pg
from gppylib.db import catalog
from gppylib.db import dbconn
from gppylib.system import configurationInterface, fileSystemInterface, osInterface
from gppylib import pgconf
from gppylib.testold.testUtils import testOutput
from gppylib.system.environment import GpMasterEnvironment

logger = gplog.get_default_logger()

#-------------------------------------------------------------------------
class GpInjectFaultProgram:
    #
    # Constructor:
    #
    # @param options the options as returned by the options parser
    #
    def __init__(self, options):
        self.options = options

    #
    # Build the fault transition message.  Fault options themselves will NOT be validated by the
    #   client -- the server will do that when we send the fault
    #
    def buildMessage(self) :

        # note that we don't validate these strings -- if they contain newlines
        # (and so mess up the transition protocol) then the server will error
        result = ["faultInject"]
        result.append( toNonNoneString(self.options.faultName))
        result.append( toNonNoneString(self.options.type))
        result.append( toNonNoneString(self.options.ddlStatement))
        result.append( toNonNoneString(self.options.databaseName))
        result.append( toNonNoneString(self.options.tableName))
        result.append( toNonNoneString(self.options.numOccurrences))
        result.append( toNonNoneString(self.options.sleepTimeSeconds))
        return '\n'.join(result)

    #
    # build a message that will get status of the fault
    #
    def buildGetStatusMessage(self) :
        # note that we don't validate this string then the server may error
        result = ["getFaultInjectStatus"]
        result.append( toNonNoneString(self.options.faultName))
        return '\n'.join(result)

    #
    # return True if the segment matches the given role, False otherwise
    #
    def isMatchingRole(self, role, segment):
        segmentRole = segment.getSegmentRole()
        if role == "primary":
            return segmentRole == 'p'
        elif role == "mirror":
            return segmentRole == 'm'
        elif role == "primary_mirror":
            return segmentRole == 'm' or segmentRole == 'p'
        else:
            raise ProgramArgumentValidationException("Invalid role specified: %s" % role)

    #
    # load the segments and filter to the ones we should target
    #
    def loadTargetSegments(self) :

        targetHost = self.options.targetHost
        targetRole = self.options.targetRole
        targetDbId = self.options.targetDbId

        if targetHost is None and targetDbId is None:
            raise ProgramArgumentValidationException(\
                            "neither --host nor --seg_dbid specified.  " \
                            "Exactly one should be specified.")
        if targetHost is not None and targetDbId is not None:
            raise ProgramArgumentValidationException(\
                            "both --host nor --seg_dbid specified.  " \
                            "Exactly one should be specified.")
        if targetHost is not None and targetRole is None:
            raise ProgramArgumentValidationException(\
                            "--role not specified when --host is specified.  " \
                            "Role is required when targeting a host.")
        if targetDbId is not None and targetRole is not None:
            raise ProgramArgumentValidationException(\
                            "--role specified when --seg_dbid is specified.  " \
                            "Role should not be specified when targeting a single dbid.")

        #
        # load from master db
        #
        masterPort = self.options.masterPort
        if masterPort is None:
            gpEnv = GpMasterEnvironment(self.options.masterDataDirectory, False, verbose=False)
            masterPort = gpEnv.getMasterPort()
        conf = configurationInterface.getConfigurationProvider().initializeProvider(masterPort)
        gpArray = conf.loadSystemConfig(useUtilityMode=True, verbose=False)
        segments = gpArray.getDbList()
        
        #
        # prune gpArray according to filter settings
        #
        segments = [seg for seg in segments if seg.isSegmentQE()]
        if targetHost is not None and targetHost != "ALL":
            segments = [seg for seg in segments if seg.getSegmentHostName() == targetHost]

        if targetDbId is not None:
            segments = gpArray.getDbList()
            dbId = int(targetDbId)
            segments = [seg for seg in segments if seg.getSegmentDbId() == dbId]

        if targetRole is not None:
            segments = [seg for seg in segments if self.isMatchingRole(targetRole, seg)]

        # only DOWN segments remaining?  Error out
        downSegments = [seg for seg in segments if seg.getSegmentStatus() != 'u']
        if len(downSegments) > 0:
            downSegStr = "\n     Down Segment: "
            raise ExceptionNoStackTraceNeeded(
                "Unable to inject fault.  At least one segment is marked as down in the database.%s%s" %
                (downSegStr, downSegStr.join([str(downSeg) for downSeg in downSegments])))

        return segments

    # return True for sync, False for async
    def getAndValidateIsSyncSetting(self):
        syncMode = self.options.syncMode
        if syncMode == "sync":
            return True
        elif syncMode == "async":
            return False
        raise ExceptionNoStackTraceNeeded( "Invalid -m, --mode option %s" % syncMode)

    #
    # write string to a temporary file that will be deleted on completion
    #
    def writeToTempFile(self, str):
        inputFile = fileSystemInterface.getFileSystemProvider().createNamedTemporaryFile()
        inputFile.write(str)
        inputFile.flush()
        return inputFile

    def injectFaults(self, segments, messageText):

        inputFile = self.writeToTempFile(messageText)
        testOutput("Injecting fault on %d segment(s)" % len(segments))

        # run the command in serial to each target
        for segment in segments :
            logger.info("Injecting fault on content=%d:dbid=%d:mode=%s:status=%s", 
                segment.getSegmentContentId(), 
                segment.getSegmentDbId(), 
                segment.getSegmentMode(), 
                segment.getSegmentStatus())
            # if there is an error then an exception is raised by command execution
            cmd = gp.SendFilerepTransitionMessage("Fault Injector", inputFile.name, \
                                        segment.getSegmentPort(), base.LOCAL, segment.getSegmentHostName())
            cmd.run(validateAfter=False)


            # validate ourselves
            if cmd.results.rc != 0:
                raise ExceptionNoStackTraceNeeded("Injection Failed: %s" % cmd.results.stderr)
            elif self.options.type == "status":
                # server side prints nice success messages on status...so print it
                str = cmd.results.stderr
                if str.startswith("Success: "):
                    str = str.replace("Success: ", "", 1)
                str = str.replace("\n", "")
                logger.info("%s", str)
        inputFile.close()

    def waitForFaults(self, segments, statusQueryText ):
        inputFile = self.writeToTempFile(statusQueryText)
        segments = [seg for seg in segments]
        sleepTimeSec = 0.115610199
        sleepTimeMultipler = 1.5  # sleepTimeMultipler * sleepTimeMultipler^11 ~= 10

        logger.info("Awaiting fault on %d segment(s)", len(segments))
        while len(segments) > 0 :
            logger.info("Sleeping %.2f seconds " % sleepTimeSec)
            osInterface.getOsProvider().sleep(sleepTimeSec)

            segmentsForNextPass = []
            for segment in segments:
                logger.info("Checking for fault completion on %s", segment)
                cmd = gp.SendFilerepTransitionMessage.local("Fault Injector Status Check", inputFile.name, \
                                                            segment.getSegmentPort(), segment.getSegmentHostName())
                resultStr = cmd.results.stderr.strip()
                if resultStr == "Success: waitMore":
                    segmentsForNextPass.append(segment)
                elif resultStr != "Success: done":
                    raise Exception("Unexpected result from server %s" % resultStr)

            segments = segmentsForNextPass
            sleepTimeSec = sleepTimeSec if sleepTimeSec > 7 else sleepTimeSec * sleepTimeMultipler
        inputFile.close()

    def isSyncableFaultType(self):
        type = self.options.type
        return type != "reset" and type != "status"

    ######
    def run(self):

        if self.options.masterPort is not None and self.options.masterDataDirectory is not None:
            raise ProgramArgumentValidationException("both master port and master data directory options specified;" \
                    " at most one should be specified, or specify none to use MASTER_DATA_DIRECTORY environment variable")

        isSync = self.getAndValidateIsSyncSetting()
        messageText = self.buildMessage()
        segments = self.loadTargetSegments()

        # inject, maybe wait
        self.injectFaults(segments, messageText)
        if isSync and self.isSyncableFaultType() :
            statusQueryText = self.buildGetStatusMessage()
            self.waitForFaults(segments, statusQueryText)

        logger.info("DONE")
        return 0 # success -- exit code 0!

    def cleanup(self):
        pass

    #-------------------------------------------------------------------------
    @staticmethod
    def createParser():
        description = ("""
        This utility is NOT SUPPORTED and is for internal-use only.

        Used to inject faults into the file replication code.
        """)

        help = ["""

        Return codes:
          0 - Fault injected
          non-zero: Error or invalid options
        """]

        parser = OptParser(option_class=OptChecker,
                    description='  '.join(description.split()),
                    version='%prog version $Revision$')
        parser.setHelp(help)

        addStandardLoggingAndHelpOptions(parser, False)

        # these options are used to determine the target segments
        addTo = OptionGroup(parser, 'Target Segment Options: ')
        parser.add_option_group(addTo)
        addTo.add_option('-r', '--role', dest="targetRole", type='string', metavar="<role>",
                         help="Role of segments to target: primary, mirror, or primary_mirror")
        addTo.add_option("-s", "--seg_dbid", dest="targetDbId", type="string", metavar="<dbid>",
                         help="The segment  dbid on which fault should be set and triggered.")
        addTo.add_option("-H", "--host", dest="targetHost", type="string", metavar="<host>",
                         help="The hostname on which fault should be set and triggered; pass ALL to target all hosts")

        addTo = OptionGroup(parser, 'Master Connection Options')
        parser.add_option_group(addTo)

        addMasterDirectoryOptionForSingleClusterProgram(addTo)
        addTo.add_option("-p", "--master_port", dest="masterPort",  type="int", default=None,
                         metavar="<masterPort>",
                         help="DEPRECATED, use MASTER_DATA_DIRECTORY environment variable or -d option.  " \
                         "The port number of the master database on localhost, " \
                         "used to fetch the segment configuration.")

        addTo = OptionGroup(parser, 'Client Polling Options: ')
        parser.add_option_group(addTo)
        addTo.add_option('-m', '--mode', dest="syncMode", type='string', default="async",
                         metavar="<syncMode>",
                         help="Synchronization mode : sync (client waits for fault to occur)" \
                         " or async (client only sets fault request on server)")

        # these options are used to build the message for the segments
        addTo = OptionGroup(parser, 'Fault Options: ')
        parser.add_option_group(addTo)
        # NB: This list needs to be kept in sync with:
        # - FaultInjectorTypeEnumToString
        # - FaultInjectorType_e
        addTo.add_option('-y','--type', dest="type", type='string', metavar="<type>",
                         help="fault type: sleep (insert sleep), fault (report fault to postmaster and fts prober), " \
			      "fatal (inject FATAL error), panic (inject PANIC error), error (inject ERROR), " \
			      "infinite_loop, data_curruption (corrupt data in memory and persistent media), " \
			      "suspend (suspend execution), resume (resume execution that was suspended), " \
			      "skip (inject skip i.e. skip checkpoint), " \
			      "memory_full (all memory is consumed when injected), " \
			      "reset (remove fault injection), status (report fault injection status), " \
			      "segv (inject a SEGV), " \
			      "interrupt (inject an Interrupt), " \
			      "finish_pending (set QueryFinishPending to true), " \
			      "checkpoint_and_panic (inject a panic following checkpoint) ")
        addTo.add_option("-z", "--sleep_time_s", dest="sleepTimeSeconds", type="int", default="10" ,
                            metavar="<sleepTime>",
                            help="For 'sleep' faults, the amount of time for the sleep.  Defaults to %default." \
				 "Min Max Range is [0, 7200 sec] ")
        addTo.add_option('-f','--fault_name', dest="faultName", type='string', metavar="<name>",
                         help="See src/include/utils/faultinjector_lists.h for list of fault names")
        addTo.add_option("-c", "--ddl_statement", dest="ddlStatement", type="string",
                         metavar="ddlStatement",
                         help="The DDL statement on which fault should be set and triggered " \
                         "(i.e. create_database, drop_database, create_table, drop_table)")
        addTo.add_option("-D", "--database_name", dest="databaseName", type="string",
                         metavar="databaseName",
                         help="The database name on which fault should be set and triggered.")
        addTo.add_option("-t", "--table_name", dest="tableName", type="string",
                         metavar="tableName",
                         help="The table name on which fault should be set and triggered.")
        addTo.add_option("-o", "--occurrence", dest="numOccurrences", type="int", default=1,
                         metavar="numOccurrences",
                         help="The number of occurrence of the DDL statement with the database name " \
                         "and the table name before fault is triggered.  Defaults to %default. Max is 1000. " \
			 "Fault is triggered always if set to '0'. ")
        parser.set_defaults()
        return parser

    @staticmethod
    def createProgram(options, args):
        if len(args) > 0 :
            raise ProgramArgumentValidationException(\
                            "too many arguments: only options may be specified")
        return GpInjectFaultProgram(options)
