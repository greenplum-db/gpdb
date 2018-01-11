import os
import signal
import time

from gppylib.mainUtils import *

from gppylib.utils import checkNotNone, appendNewEntriesToHbaFile
from gppylib.db import dbconn
from gppylib import gparray, gplog
from gppylib.commands import unix
from gppylib.commands import gp
from gppylib.commands import base
from gppylib.gparray import GpArray
from gppylib.testold.testUtils import *
from gppylib.operations import startSegments
from gppylib.gp_era import read_era
from gppylib.operations.utils import ParallelOperation, RemoteOperation
from gppylib.operations.unix import CleanSharedMem
from gppylib.operations.filespace import PG_SYSTEM_FILESPACE, GP_TRANSACTION_FILES_FILESPACE, \
    GP_TEMPORARY_FILES_FILESPACE, GetMoveOperationList, GetFilespaceEntriesDict, GetFilespaceEntries, \
    GetCurrentFilespaceEntries, RollBackFilespaceChanges, UpdateFlatFiles, FileType, MoveFilespaceError
from gppylib.commands.gp import is_pid_postmaster, get_pid_from_remotehost
from gppylib.commands.unix import check_pid_on_remotehost, Scp

logger = gplog.get_default_logger()

gDatabaseDirectories = [
    # this list and the gDatabaseSubDirectories occur combined inside initdb.c
    "global",
    "pg_log",
    "pg_xlog",
    "pg_clog",
    "pg_changetracking",
    "pg_subtrans",
    "pg_twophase",
    "pg_multixact",
    "pg_distributedxidmap",
    "pg_distributedlog",
    "pg_utilitymodedtmredo",
    "base",
    "pg_tblspc",
    "pg_stat_tmp"
]
gDatabaseSubDirectories = [
    "pg_xlog/archive_status",
    "pg_multixact/members",
    "pg_multixact/offsets",
    "base/1"
]

#
# Database files that may exist in the root directory and need deleting 
#
gDatabaseFiles = [
    "PG_VERSION",
    "pg_hba.conf",
    "pg_ident.conf",
    "postgresql.conf",
    "postmaster.log",
    "postmaster.opts",
    "postmaster.pid",
    "gp_dbid"
]


def MPP_12038_fault_injection():
    """This function will check for the environment variable
    GP_MPP_12038 and if it is set will sleep for 2 * gp_fts_probe_interval.
    This is used in this module to check interaction with the FTS prober and
    should only be used for testing.  Note this delay is long enough for a 
    small test installation but would likely not be long enough for a large
    cluster."""
    if os.getenv("GP_MPP_12038_INJECT_DELAY", None):
        faultProber = faultProberInterface.getFaultProber()
        probe_interval_secs = faultProber.getFaultProberInterval()
        logger.info("Sleeping for %d seconds for MPP-12038 test..." % (probe_interval_secs * 2))
        time.sleep(probe_interval_secs * 2)


#
# note: it's a little quirky that caller must set up failed/failover so that failover is in gparray but
#                                 failed is not (if both set)...change that, or at least protect against problems
#

class GpMirrorToBuild:
    def __init__(self, failedSegment, liveSegment, failoverSegment, forceFullSynchronization, logger=logger):
        checkNotNone("liveSegment", liveSegment)
        checkNotNone("forceFullSynchronization", forceFullSynchronization)

        if failedSegment is None and failoverSegment is None:
            raise Exception("No mirror passed to GpMirrorToBuild")

        if not liveSegment.isSegmentQE():
            raise ExceptionNoStackTraceNeeded("Segment to recover from for content %s is not a correct segment "
                                              "(it is a master or standby master)" % liveSegment.getSegmentContentId())
        if not liveSegment.isSegmentPrimary(True):
            raise ExceptionNoStackTraceNeeded(
                "Segment to recover from for content %s is not a primary" % liveSegment.getSegmentContentId())
        if not liveSegment.isSegmentUp():
            raise ExceptionNoStackTraceNeeded(
                "Primary segment is not up for content %s" % liveSegment.getSegmentContentId())

        if failedSegment is not None:
            if failedSegment.getSegmentContentId() != liveSegment.getSegmentContentId():
                raise ExceptionNoStackTraceNeeded(
                    "The primary is not of the same content as the failed mirror.  Primary content %d, "
                    "mirror content %d" % (liveSegment.getSegmentContentId(), failedSegment.getSegmentContentId()))
            if failedSegment.getSegmentDbId() == liveSegment.getSegmentDbId():
                raise ExceptionNoStackTraceNeeded("For content %d, the dbid values are the same.  "
                                                  "A segment may not be recovered from itself" %
                                                  liveSegment.getSegmentDbId())

        if failoverSegment is not None:
            if failoverSegment.getSegmentContentId() != liveSegment.getSegmentContentId():
                raise ExceptionNoStackTraceNeeded(
                    "The primary is not of the same content as the mirror.  Primary content %d, "
                    "mirror content %d" % (liveSegment.getSegmentContentId(), failoverSegment.getSegmentContentId()))
            if failoverSegment.getSegmentDbId() == liveSegment.getSegmentDbId():
                raise ExceptionNoStackTraceNeeded("For content %d, the dbid values are the same.  "
                                                  "A segment may not be built from itself"
                                                  % liveSegment.getSegmentDbId())

        if failedSegment is not None and failoverSegment is not None:
            # for now, we require the code to have produced this -- even when moving the segment to another
            #  location, we preserve the directory
            assert failedSegment.getSegmentDbId() == failoverSegment.getSegmentDbId()

        self.__failedSegment = failedSegment
        self.__liveSegment = liveSegment
        self.__failoverSegment = failoverSegment

        """
        __forceFullSynchronization is true if full resynchronization should be FORCED -- that is, the
           existing segment will be cleared and all objects will be transferred by the file resynchronization
           process on the server
        """
        self.__forceFullSynchronization = forceFullSynchronization

    def getFailedSegment(self):
        """
        returns the segment that failed. This can be None, for example when adding mirrors
        """
        return self.__failedSegment

    def getLiveSegment(self):
        """
        returns the primary segment from which the recovery will take place.  Will always be non-None
        """
        return self.__liveSegment

    def getFailoverSegment(self):
        """
        returns the target segment to which we will copy the data, or None
            if we will recover in place.  Note that __failoverSegment should refer to the same dbid
            as __failedSegment, but should have updated path + file information.
        """
        return self.__failoverSegment

    def isFullSynchronization(self):
        """
        Returns whether or not this segment to recover needs to recover using full resynchronization
        """

        if self.__forceFullSynchronization:
            return True

        # if we are failing over to a new segment location then we must fully resync
        if self.__failoverSegment is not None:
            return True

        return False


class GpMirrorListToBuild:
    def __init__(self, toBuild, pool, quiet, parallelDegree, additionalWarnings=None, logger=logger):
        self.__mirrorsToBuild = toBuild
        self.__pool = pool
        self.__quiet = quiet
        self.__parallelDegree = parallelDegree
        self.__additionalWarnings = additionalWarnings or []
        if not logger:
            raise Exception('logger argument cannot be None')

        self.__logger = logger

    def getMirrorsToBuild(self):
        """
        Returns a newly allocated list
        """
        return [m for m in self.__mirrorsToBuild]

    def getAdditionalWarnings(self):
        """
        Returns any additional warnings generated during building of list
        """
        return self.__additionalWarnings

    def __moveFilespaces(self, gparray, target_segment):
        """
            Moves filespaces for temporary and transaction files to a particular location.
        """
        master_seg = gparray.master
        default_filespace_dir = master_seg.getSegmentDataDirectory()

        cur_filespace_entries = GetFilespaceEntriesDict(GetFilespaceEntries(gparray,
                                                                            PG_SYSTEM_FILESPACE).run()).run()
        pg_system_filespace_entries = GetFilespaceEntriesDict(GetFilespaceEntries(gparray,
                                                                                  PG_SYSTEM_FILESPACE).run()).run()
        cur_filespace_name = gparray.getFileSpaceName(int(cur_filespace_entries[1][0]))
        segments = [target_segment] + [seg for seg in gparray.getDbList() if
                                       seg.getSegmentContentId() == target_segment.getSegmentContentId() and
                                       seg.getSegmentDbId() != target_segment.getSegmentDbId()]

        self.__logger.info('Starting file move procedure for %s' % target_segment)

        if os.path.exists(os.path.join(default_filespace_dir, GP_TRANSACTION_FILES_FILESPACE)):
            # On the expansion segments, the current filespace used by existing nodes will be the
            # new filespace to which we want to move the transaction and temp files.
            # The filespace directories which have to be moved will be the default pg_system directories.
            new_filespace_entries = GetFilespaceEntriesDict(
                GetCurrentFilespaceEntries(gparray, FileType.TRANSACTION_FILES).run()).run()
            self.__logger.info('getting filespace information')
            new_filespace_name = gparray.getFileSpaceName(int(new_filespace_entries[1][0]))
            self.__logger.info('getting move operations list for filespace %s' % new_filespace_name)
            operation_list = GetMoveOperationList(segments,
                                                  FileType.TRANSACTION_FILES,
                                                  new_filespace_name,
                                                  new_filespace_entries,
                                                  cur_filespace_entries,
                                                  pg_system_filespace_entries).run()
            self.__logger.info('Starting transaction files move')
            ParallelOperation(operation_list).run()

            self.__logger.debug('Checking transaction files move')
            try:
                for operation in operation_list:
                    operation.get_ret()
                    pass
            except Exception, e:
                self.__logger.info('Failed to move transaction filespace. Rolling back changes ...')
                RollBackFilespaceChanges(gparray.getExpansionSegDbList(),
                                         FileType.TRANSACTION_FILES,
                                         cur_filespace_name,
                                         cur_filespace_entries,
                                         new_filespace_entries,
                                         pg_system_filespace_entries).run()
                raise

        if os.path.exists(os.path.join(default_filespace_dir, GP_TEMPORARY_FILES_FILESPACE)):
            new_filespace_entries = GetFilespaceEntriesDict(GetCurrentFilespaceEntries(gparray,
                                                                                       FileType.TEMPORARY_FILES).run()).run()
            new_filespace_name = gparray.getFileSpaceName(int(new_filespace_entries[1][0]))
            operation_list = GetMoveOperationList(segments,
                                                  FileType.TEMPORARY_FILES,
                                                  new_filespace_name,
                                                  new_filespace_entries,
                                                  cur_filespace_entries,
                                                  pg_system_filespace_entries).run()
            self.__logger.info('Starting temporary files move')
            ParallelOperation(operation_list).run()

            self.__logger.debug('Checking temporary files move')
            try:
                for operation in operation_list:
                    operation.get_ret()
                    pass
            except Exception, e:
                self.__logger.info('Failed to move temporary filespace. Rolling back changes ...')
                RollBackFilespaceChanges(gparray.getExpansionDbList(),
                                         FileType.TRANSACTION_FILES,
                                         cur_filespace_name,
                                         cur_filespace_entries,
                                         new_filespace_entries,
                                         pg_system_filespace_entries).run()
                raise

    def buildMirrors(self, actionName, gpEnv, gpArray):
        """
        Build the mirrors.

        gpArray must have already been altered to have updated directories -- that is, the failoverSegments
            from the mirrorsToBuild must be present in gpArray.

        """
        testOutput("building %s segment(s)" % len(self.__mirrorsToBuild))

        if len(self.__mirrorsToBuild) == 0:
            self.__logger.info("No segments to " + actionName)
            return True

        self.checkForPortAndDirectoryConflicts(gpArray)

        self.__logger.info("%s segment(s) to %s" % (len(self.__mirrorsToBuild), actionName))

        self.__verifyGpArrayContents(gpArray)

        # make sure the target directories are up-to-date
        #  by cleaning them, if needed, and then copying a basic directory there
        #  the postgresql.conf in that basic directory will need updating (to change the port)
        toStopDirectives = []
        toEnsureMarkedDown = []
        cleanupDirectives = []
        copyDirectives = []
        for toRecover in self.__mirrorsToBuild:

            if toRecover.getFailedSegment() is not None:
                # will stop the failed segment.  Note that we do this even if we are recovering to a different location!
                toStopDirectives.append(GpStopSegmentDirectoryDirective(toRecover.getFailedSegment()))
                if toRecover.getFailedSegment().getSegmentStatus() == gparray.STATUS_UP:
                    toEnsureMarkedDown.append(toRecover.getFailedSegment())

            if toRecover.isFullSynchronization():

                isTargetReusedLocation = False
                if toRecover.getFailedSegment() is not None and \
                                toRecover.getFailoverSegment() is None:
                    #
                    # We are recovering a failed segment in-place
                    #
                    cleanupDirectives.append(GpCleanupSegmentDirectoryDirective(toRecover.getFailedSegment()))
                    isTargetReusedLocation = True

                if toRecover.getFailoverSegment() is not None:
                    targetSegment = toRecover.getFailoverSegment()
                else:
                    targetSegment = toRecover.getFailedSegment()

                d = GpCopySegmentDirectoryDirective(toRecover.getLiveSegment(), targetSegment, isTargetReusedLocation)
                copyDirectives.append(d)

        self.__ensureStopped(gpEnv, toStopDirectives)
        self.__ensureSharedMemCleaned(gpEnv, toStopDirectives)
        self.__ensureMarkedDown(gpEnv, toEnsureMarkedDown)
        self.__cleanUpSegmentDirectories(cleanupDirectives)
        self.__copySegmentDirectories(gpEnv, gpArray, copyDirectives)

        # Move the filespace for transaction and temporary files
        for toRecover in self.__mirrorsToBuild:
            target_segment = None

            if toRecover.getFailoverSegment() is not None:
                target_segment = toRecover.getFailoverSegment()
            elif toRecover.isFullSynchronization():
                target_segment = toRecover.getFailedSegment()

            if target_segment is not None:
                self.__moveFilespaces(gpArray, target_segment)

        # If we are adding mirrors, we need to update the flat files on the primaries as well
        if actionName == "add":
            try:
                UpdateFlatFiles(gpArray, primaries=True).run()
            except MoveFilespaceError, e:
                self.__logger.error(str(e))
                raise
        else:
            try:
                print 'updating flat files'
                UpdateFlatFiles(gpArray, primaries=False).run()
            except MoveFilespaceError, e:
                self.__logger.error(str(e))
                raise

        # update and save metadata in memory
        for toRecover in self.__mirrorsToBuild:

            if toRecover.getFailoverSegment() is None:
                # we are recovering the lost segment in place
                seg = toRecover.getFailedSegment()
            else:
                seg = toRecover.getFailedSegment()
                # no need to update the failed segment's information -- it is
                #   being overwritten in the configuration with the failover segment
                for gpArraySegment in gpArray.getDbList():
                    if gpArraySegment is seg:
                        raise Exception(
                            "failed segment should not be in the new configuration if failing over to new segment")

                seg = toRecover.getFailoverSegment()
            seg.setSegmentStatus(gparray.STATUS_DOWN)  # down initially, we haven't started it yet
            seg.setSegmentMode(gparray.MODE_RESYNCHRONIZATION)

        # figure out what needs to be started or transitioned
        mirrorsToStart = []
        primariesToConvert = []
        convertPrimaryUsingFullResync = []
        fullResyncMirrorDbIds = {}
        for toRecover in self.__mirrorsToBuild:
            seg = toRecover.getFailoverSegment()
            if seg is None:
                seg = toRecover.getFailedSegment()  # we are recovering in place
            mirrorsToStart.append(seg)
            primarySeg = toRecover.getLiveSegment()
            # The change in configuration to of the mirror to down requires
            # that the primary also be change to change tracking if required.
            if primarySeg.getSegmentMode() != gparray.MODE_CHANGELOGGING:
                primarySeg.setSegmentMode(gparray.MODE_CHANGELOGGING)
            primariesToConvert.append(primarySeg)
            convertPrimaryUsingFullResync.append(toRecover.isFullSynchronization())

            if toRecover.isFullSynchronization() and seg.getSegmentDbId() > 0:
                fullResyncMirrorDbIds[seg.getSegmentDbId()] = True

        # should use mainUtils.getProgramName but I can't make it work!
        programName = os.path.split(sys.argv[0])[-1]

        # Disable Ctrl-C, going to save metadata in database and transition segments
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            self.__logger.info("Updating configuration with new mirrors")
            configInterface.getConfigurationProvider().updateSystemConfig(
                gpArray,
                "%s: segment config for resync" % programName,
                dbIdToForceMirrorRemoveAdd=fullResyncMirrorDbIds,
                useUtilityMode=False,
                allowPrimary=False
            )

            MPP_12038_fault_injection()

            self.__logger.info("Updating mirrors")
            self.__updateGpIdFile(gpEnv, gpArray, mirrorsToStart)

            self.__logger.info("Starting mirrors")
            start_all_successful = self.__startAll(gpEnv, gpArray, mirrorsToStart)

            self.__logger.info("Updating configuration to mark mirrors up")
            for seg in mirrorsToStart:
                seg.setSegmentStatus(gparray.STATUS_UP)
            for seg in primariesToConvert:
                seg.setSegmentMode(gparray.MODE_RESYNCHRONIZATION)
            configInterface.getConfigurationProvider().updateSystemConfig(
                gpArray,
                "%s: segment resync marking mirrors up and primaries resync" % programName,
                dbIdToForceMirrorRemoveAdd={},
                useUtilityMode=True,
                allowPrimary=False
            )

            MPP_12038_fault_injection()

            #
            # note: converting the primaries may take a really long time to complete because of initializing
            #       resynchronization
            #
            self.__logger.info("Updating primaries")
            self.__convertAllPrimaries(gpEnv, gpArray, primariesToConvert, convertPrimaryUsingFullResync)

            self.__logger.info("Done updating primaries")
        finally:
            # Reenable Ctrl-C
            signal.signal(signal.SIGINT, signal.default_int_handler)

        return start_all_successful

    def __verifyGpArrayContents(self, gpArray):
        """
        Run some simple assertions against gpArray contents
        """
        for seg in gpArray.getDbList():
            if seg.getSegmentDataDirectory() != seg.getSegmentFilespaces()[gparray.SYSTEM_FILESPACE]:
                raise Exception("Mismatch between segment data directory and filespace entry for segment %s" %
                                seg.getSegmentDbId())

    def checkForPortAndDirectoryConflicts(self, gpArray):
        """
        Check gpArray for internal consistency -- no duplicate ports or directories on the same host, for example

        A detected problem causes an Exception to be raised
        """

        for hostName, segmentArr in GpArray.getSegmentsByHostName(gpArray.getDbList()).iteritems():
            usedPorts = {}
            usedDataDirectories = {}
            for segment in segmentArr:

                # check for port conflict
                replicationPort = segment.getSegmentReplicationPort()
                port = segment.getSegmentPort()
                dbid = segment.getSegmentDbId()
                if port in usedPorts:
                    raise Exception(
                        "On host %s, port %s for segment with dbid %s conflicts with port for segment dbid %s" %
                        (hostName, port, dbid, usedPorts.get(port)))

                if segment.isSegmentQE():
                    if replicationPort is None:
                        raise Exception("On host %s, the replication port is not set for segment with dbid %s" %
                                        (hostName, dbid))

                    if replicationPort in usedPorts:
                        raise Exception(
                            "On host %s, replication port %s for segment with dbid %s conflicts "
                            "with a port for segment dbid %s" %
                            (hostName, dbid, replicationPort, usedPorts.get(replicationPort)))

                    if port == replicationPort:
                        raise Exception("On host %s, segment with dbid %s has equal port and replication port" %
                                        (hostName, dbid))

                usedPorts[port] = dbid
                usedPorts[replicationPort] = dbid

                # check for directory conflict; could improve this by reporting nicer the conflicts
                paths = [path for oid, path in segment.getSegmentFilespaces().items() if
                         oid != gparray.SYSTEM_FILESPACE]
                paths.append(segment.getSegmentDataDirectory())

                for path in paths:
                    if path in usedDataDirectories:
                        raise Exception(
                            "On host %s, directory (base or filespace) for segment with dbid %s conflicts with a "
                            "directory (base or filespace) for segment dbid %s; directory: %s" %
                            (hostName, dbid, usedDataDirectories.get(path), path))
                    usedDataDirectories[path] = dbid

    def __runWaitAndCheckWorkerPoolForErrorsAndClear(self, cmds, actionVerb, suppressErrorCheck=False):
        for cmd in cmds:
            self.__pool.addCommand(cmd)

        self.__pool.wait_and_printdots(len(cmds), self.__quiet)
        if not suppressErrorCheck:
            self.__pool.check_results()
        self.__pool.empty_completed_items()

    def __copyFiles(self, srcDir, destDir, fileNames):
        for name in fileNames:
            cmd = gp.LocalCopy("copy file for segment", srcDir + "/" + name, destDir + "/" + name)
            cmd.run(validateAfter=True)

    def __createEmptyDirectories(self, dir, newDirectoryNames):
        for name in newDirectoryNames:
            subDir = os.path.join(dir, name)
            unix.MakeDirectory("create blank directory for segment", subDir).run(validateAfter=True)
            unix.Chmod.local('set permissions on blank dir', subDir, '0700')

    def __buildTarFileForTransfer(self, gpEnv, masterSegment, sampleSegment, newSegments):
        """
        Returns the file for the tarfile that should be transferred and used
         for building the blank segment

        """
        masterDir = gpEnv.getMasterDataDir()

        # note that this tempdir will be left around on the system (this is what other scripts do currently)
        tempDir = gp.createTempDirectoryName(gpEnv.getMasterDataDir(), "gpbuildingsegment")
        unix.MakeDirectory("create temp directory for segment", tempDir).run(validateAfter=True)

        schemaDir = tempDir + "/schema"
        unix.MakeDirectory("create temp schema directory for segment", schemaDir).run(validateAfter=True)
        unix.Chmod.local('set permissions on schema dir', schemaDir, '0700')  # set perms so postgres can start

        #
        # Copy remote files from the sample segment to the master
        #
        for toCopyFromRemote in ["postgresql.conf", "pg_hba.conf"]:
            cmd = gp.RemoteCopy('copying %s from a segment' % toCopyFromRemote,
                                os.path.join(sampleSegment.getSegmentDataDirectory(), toCopyFromRemote),
                                masterSegment.getSegmentHostName(), schemaDir, ctxt=base.REMOTE,
                                remoteHost=sampleSegment.getSegmentAddress())
            cmd.run(validateAfter=True)

        appendNewEntriesToHbaFile(schemaDir + "/pg_hba.conf", newSegments)

        #
        # Use the master's version of other files, and build
        #
        self.__createEmptyDirectories(schemaDir, gDatabaseDirectories)
        self.__createEmptyDirectories(schemaDir, gDatabaseSubDirectories)
        self.__copyFiles(masterDir, schemaDir, ["PG_VERSION", "pg_ident.conf"])

        #
        # Build final tar
        #
        tarFileName = "gp_emptySegmentSchema.tar"
        tarFile = tempDir + "/" + tarFileName
        cmd = gp.CreateTar('gpbuildingmirrorsegment tar segment template', schemaDir, tarFile)
        cmd.run(validateAfter=True)

        return (tempDir, tarFile, tarFileName)

    def __copySegmentDirectories(self, gpEnv, gpArray, directives):
        """
        directives should be composed of GpCopySegmentDirectoryDirective values
        """
        if len(directives) == 0:
            return

        srcSegments = [d.getSrcSegment() for d in directives]
        destSegments = [d.getDestSegment() for d in directives]
        isTargetReusedLocation = [d.isTargetReusedLocation() for d in directives]
        destSegmentByHost = GpArray.getSegmentsByHostName(destSegments)
        newSegmentInfo = gp.ConfigureNewSegment.buildSegmentInfoForNewSegment(destSegments, isTargetReusedLocation)

        self.__logger.info('Building template directory')
        (tempDir, blankTarFile, tarFileName) = self.__buildTarFileForTransfer(gpEnv, gpArray.master, srcSegments[0],
                                                                              destSegments)

        def createConfigureNewSegmentCommand(hostName, cmdLabel, validationOnly):
            segmentInfo = newSegmentInfo[hostName]
            checkNotNone("segmentInfo for %s" % hostName, segmentInfo)
            return gp.ConfigureNewSegment(cmdLabel,
                                          segmentInfo,
                                          tarFile=tarFileName,
                                          newSegments=True,
                                          verbose=gplog.logging_is_verbose(),
                                          batchSize=self.__parallelDegree,
                                          ctxt=gp.REMOTE,
                                          remoteHost=hostName,
                                          validationOnly=validationOnly)

        #
        # validate directories for target segments
        #
        self.__logger.info('Validating remote directories')
        cmds = []
        for hostName in destSegmentByHost.keys():
            cmds.append(createConfigureNewSegmentCommand(hostName, 'validate blank segments', True))
        for cmd in cmds:
            self.__pool.addCommand(cmd)
        self.__pool.wait_and_printdots(len(cmds), self.__quiet)
        validationErrors = []
        for item in self.__pool.getCompletedItems():
            results = item.get_results()
            if not results.wasSuccessful():
                if results.rc == 1:
                    # stdoutFromFailure = results.stdout.replace("\n", " ").strip()
                    lines = results.stderr.split("\n")
                    for line in lines:
                        if len(line.strip()) > 0:
                            validationErrors.append("Validation failure on host %s %s" % (item.remoteHost, line))
                else:
                    validationErrors.append(str(item))
        self.__pool.empty_completed_items()
        if validationErrors:
            raise ExceptionNoStackTraceNeeded("\n" + ("\n".join(validationErrors)))

        #
        # copy tar from master to target hosts
        #
        self.__logger.info('Copying template directory file')
        cmds = []
        for hostName in destSegmentByHost.keys():
            cmds.append(gp.RemoteCopy("copy segment tar", blankTarFile, hostName, tarFileName))

        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, "building and transferring basic segment directory")

        #
        # unpack and configure new segments
        #
        self.__logger.info('Configuring new segments')
        cmds = []
        for hostName in destSegmentByHost.keys():
            cmds.append(createConfigureNewSegmentCommand(hostName, 'configure blank segments', False))
        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, "unpacking basic segment directory")

        #
        # copy dump files from old segment to new segment
        #
        for srcSeg in srcSegments:
            for destSeg in destSegments:
                if srcSeg.content == destSeg.content:
                    src_dump_dir = os.path.join(srcSeg.getSegmentDataDirectory(), 'db_dumps')
                    cmd = base.Command('check existence of db_dumps directory', 'ls %s' % (src_dump_dir),
                                       ctxt=base.REMOTE, remoteHost=destSeg.getSegmentAddress())
                    cmd.run()
                    if cmd.results.rc == 0:  # Only try to copy directory if it exists
                        cmd = Scp('copy db_dumps from old segment to new segment',
                                  os.path.join(srcSeg.getSegmentDataDirectory(), 'db_dumps*', '*'),
                                  os.path.join(destSeg.getSegmentDataDirectory(), 'db_dumps'),
                                  srcSeg.getSegmentAddress(),
                                  destSeg.getSegmentAddress(),
                                  recursive=True)
                        cmd.run(validateAfter=True)
                        break

        #
        # Clean up copied tar from each remote host
        #
        self.__logger.info('Cleaning files')
        cmds = []
        for hostName, segments in destSegmentByHost.iteritems():
            cmds.append(unix.RemoveFile('remove tar file', tarFileName, ctxt=gp.REMOTE, remoteHost=hostName))
        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, "cleaning up tar file on segment hosts")

        #
        # clean up the local temp directory
        #
        unix.RemoveDirectory.local('remove temp directory', tempDir)

    def _get_running_postgres_segments(self, segments):
        running_segments = []
        for seg in segments:
            datadir = self.dereference_remote_symlink(seg.getSegmentDataDirectory(), seg.getSegmentHostName())
            pid = get_pid_from_remotehost(seg.getSegmentHostName(), datadir)
            if pid is not None:
                if check_pid_on_remotehost(pid, seg.getSegmentHostName()):
                    if is_pid_postmaster(datadir, pid, seg.getSegmentHostName()):
                        running_segments.append(seg)
                    else:
                        self.__logger.info("Skipping to stop segment %s on host %s since it is not a postgres process" % (
                        seg.getSegmentDataDirectory(), seg.getSegmentHostName()))
                else:
                    self.__logger.debug("Skipping to stop segment %s on host %s since process with pid %s is not running" % (
                    seg.getSegmentDataDirectory(), seg.getSegmentHostName(), pid))
            else:
                self.__logger.debug("Skipping to stop segment %s on host %s since pid could not be found" % (
                seg.getSegmentDataDirectory(), seg.getSegmentHostName()))

        return running_segments

    def dereference_remote_symlink(self, datadir, host):
        cmdStr = """python -c 'import os; print os.path.realpath("%s")'""" % datadir
        cmd = base.Command('dereference a symlink on a remote host', cmdStr=cmdStr, ctxt=base.REMOTE, remoteHost=host)
        cmd.run()
        results = cmd.get_results()
        if results.rc != 0:
            self.__logger.warning('Unable to determine if %s is symlink. Assuming it is not symlink' % (datadir))
            return datadir
        return results.stdout.strip()

    def __ensureSharedMemCleaned(self, gpEnv, directives):
        """

        @param directives a list of the GpStopSegmentDirectoryDirective values indicating which segments to cleanup 

        """

        if len(directives) == 0:
            return

        self.__logger.info('Ensuring that shared memory is cleaned up for stopped segments')
        segments = [d.getSegment() for d in directives]
        segmentsByHost = GpArray.getSegmentsByHostName(segments)
        operation_list = [RemoteOperation(CleanSharedMem(segments), host=hostName) for hostName, segments in
                          segmentsByHost.items()]
        ParallelOperation(operation_list).run()

        for operation in operation_list:
            try:
                operation.get_ret()
            except Exception as e:
                self.__logger.warning('Unable to clean up shared memory for stopped segments on host (%s)' % operation.host)

    def __ensureStopped(self, gpEnv, directives):
        """

        @param directives a list of the GpStopSegmentDirectoryDirective values indicating which segments to stop

        """
        if len(directives) == 0:
            return

        self.__logger.info("Ensuring %d failed segment(s) are stopped" % (len(directives)))
        segments = [d.getSegment() for d in directives]
        segments = self._get_running_postgres_segments(segments)
        segmentByHost = GpArray.getSegmentsByHostName(segments)

        cmds = []
        for hostName, segments in segmentByHost.iteritems():
            cmd = gp.GpSegStopCmd("remote segment stop on host '%s'" % hostName,
                                  gpEnv.getGpHome(), gpEnv.getGpVersion(),
                                  mode='fast', dbs=segments, verbose=gplog.logging_is_verbose(),
                                  ctxt=base.REMOTE, remoteHost=hostName)

            cmds.append(cmd)

        # we suppress checking for the error.  This is because gpsegstop will actually error
        #  in many cases where the stop is actually done (that is, for example, the segment is
        #  running but slow to shutdown so gpsegstop errors after whacking it with a kill)
        #
        # Perhaps we should make it so that it so that is checks if the seg is running and only attempt stop
        #  if it's running?  In that case, we could propagate the error
        #
        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, "stopping segments", suppressErrorCheck=True)

    def __ensureMarkedDown(self, gpEnv, toEnsureMarkedDown):
        """Waits for FTS prober to mark segments as down"""

        wait_time = 60 * 30  # Wait up to 30 minutes to handle very large, busy
        # clusters that may have faults.  In most cases the
        # actual time to wait will be small and this operation
        # is only needed when moving mirrors that are up and
        # needed to be stopped, an uncommon operation.

        dburl = dbconn.DbURL(port=gpEnv.getMasterPort(), dbname='template1')

        time_elapsed = 0
        seg_up_count = 0
        initial_seg_up_count = len(toEnsureMarkedDown)
        last_seg_up_count = initial_seg_up_count

        if initial_seg_up_count == 0:
            # Nothing to wait on
            return

        self.__logger.info("Waiting for segments to be marked down.")
        self.__logger.info("This may take up to %d seconds on large clusters." % wait_time)

        # wait for all needed segments to be marked down by the prober.  We'll wait
        # a max time of double the interval 
        while wait_time > time_elapsed:
            seg_up_count = 0
            current_gparray = GpArray.initFromCatalog(dburl, True)
            seg_db_map = current_gparray.getSegDbMap()

            # go through and get the status of each segment we need to be marked down
            for segdb in toEnsureMarkedDown:
                if segdb.getSegmentDbId() in seg_db_map and seg_db_map[segdb.getSegmentDbId()].isSegmentUp():
                    seg_up_count += 1
            if seg_up_count == 0:
                break
            else:
                if last_seg_up_count != seg_up_count:
                    print "\n",
                    self.__logger.info("%d of %d segments have been marked down." %
                                (initial_seg_up_count - seg_up_count, initial_seg_up_count))
                    last_seg_up_count = seg_up_count

                for _i in range(1, 5):
                    time.sleep(1)
                    sys.stdout.write(".")
                    sys.stdout.flush()

                time_elapsed += 5

        if seg_up_count == 0:
            print "\n",
            self.__logger.info("%d of %d segments have been marked down." %
                        (initial_seg_up_count, initial_seg_up_count))
        else:
            raise Exception("%d segments were not marked down by FTS" % seg_up_count)

    def __cleanUpSegmentDirectories(self, directives):
        if len(directives) == 0:
            return

        self.__logger.info("Cleaning files from %d segment(s)" % (len(directives)))
        segments = [d.getSegment() for d in directives]
        segmentByHost = GpArray.getSegmentsByHostName(segments)

        cmds = []
        for hostName, segments in segmentByHost.iteritems():
            cmds.append(gp.GpCleanSegmentDirectories("clean segment directories on %s" % hostName,
                                                     segments, gp.REMOTE, hostName))

        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, "cleaning existing directories")

    def __createStartSegmentsOp(self, gpEnv):
        return startSegments.StartSegmentsOperation(self.__pool, self.__quiet,
                                                    gpEnv.getGpVersion(),
                                                    gpEnv.getGpHome(), gpEnv.getMasterDataDir()
                                                    )

    def __updateGpIdFile(self, gpEnv, gpArray, segments):
        segmentByHost = GpArray.getSegmentsByHostName(segments)
        newSegmentInfo = gp.ConfigureNewSegment.buildSegmentInfoForNewSegment(segments)

        cmds = []
        for hostName in segmentByHost.keys():
            segmentInfo = newSegmentInfo[hostName]
            checkNotNone("segmentInfo for %s" % hostName, segmentInfo)
            cmd = gp.ConfigureNewSegment("update gpid file",
                                         segmentInfo,
                                         newSegments=False,
                                         verbose=gplog.logging_is_verbose(),
                                         batchSize=self.__parallelDegree,
                                         ctxt=gp.REMOTE,
                                         remoteHost=hostName,
                                         validationOnly=False,
                                         writeGpIdFileOnly=True)

            cmds.append(cmd)
        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, "writing updated gpid files")

    def __startAll(self, gpEnv, gpArray, segments):

        # the newly started segments should belong to the current era
        era = read_era(gpEnv.getMasterDataDir(), logger=self.__logger)

        segmentStartResult = self.__createStartSegmentsOp(gpEnv).startSegments(gpArray, segments,
                                                                               startSegments.START_AS_PRIMARY_OR_MIRROR,
                                                                               era)
        start_all_successfull = len(segmentStartResult.getFailedSegmentObjs()) == 0
        for failure in segmentStartResult.getFailedSegmentObjs():
            failedSeg = failure.getSegment()
            failureReason = failure.getReason()
            self.__logger.warn(
                "Failed to start segment.  The fault prober will shortly mark it as down. Segment: %s: REASON: %s" % (
                failedSeg, failureReason))

        return start_all_successfull

    def __convertAllPrimaries(self, gpEnv, gpArray, segments, convertUsingFullResync):
        segmentStartResult = self.__createStartSegmentsOp(gpEnv).transitionSegments(gpArray, segments,
                                                                                    convertUsingFullResync,
                                                                                    startSegments.MIRROR_MODE_PRIMARY)
        for failure in segmentStartResult.getFailedSegmentObjs():
            failedSeg = failure.getSegment()
            failureReason = failure.getReason()
            self.__logger.warn("Failed to inform primary segment of updated mirroring state.  Segment: %s: REASON: %s" % (
            failedSeg, failureReason))


class GpCleanupSegmentDirectoryDirective:
    def __init__(self, segment):
        checkNotNone("segment", segment)
        self.__segment = segment

    def getSegment(self):
        return self.__segment


class GpStopSegmentDirectoryDirective:
    def __init__(self, segment):
        checkNotNone("segment", segment)
        self.__segment = segment

    def getSegment(self):
        return self.__segment


class GpCopySegmentDirectoryDirective:
    def __init__(self, source, dest, isTargetReusedLocation):
        """
        @param isTargetReusedLocation if True then the dest location is a cleaned-up location
        """
        checkNotNone("source", source)
        checkNotNone("dest", dest)

        self.__source = source
        self.__dest = dest
        self.__isTargetReusedLocation = isTargetReusedLocation

    def getSrcSegment(self):
        return self.__source

    def getDestSegment(self):
        return self.__dest

    def isTargetReusedLocation(self):
        return self.__isTargetReusedLocation
