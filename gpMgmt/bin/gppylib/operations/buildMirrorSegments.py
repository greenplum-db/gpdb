import datetime
import os
import pipes
import signal
import time

from gppylib.mainUtils import *

from gppylib.utils import checkNotNone
from gppylib.db import dbconn
from gppylib import gparray, gplog, recoveryinfo
from gppylib.commands import unix
from gppylib.commands import gp
from gppylib.commands import base
from gppylib.gparray import GpArray
from gppylib.operations import startSegments
from gppylib.gp_era import read_era
from gppylib.operations.utils import ParallelOperation, RemoteOperation
from gppylib.system import configurationInterface as configInterface
from gppylib.commands.gp import is_pid_postmaster, get_pid_from_remotehost
from gppylib.commands.unix import check_pid_on_remotehost, Scp
from gppylib.programs.clsRecoverSegment_triples import RecoveryTriplet


logger = gplog.get_default_logger()

gDatabaseDirectories = [
    # this list occur inside initdb.c
    "global",
    "log",
    "pg_wal",
    "pg_xact",
    "pg_changetracking",
    "pg_subtrans",
    "pg_twophase",
    "pg_multixact",
    "pg_distributedxidmap",
    "pg_distributedlog",
    "base",
    "pg_tblspc",
    "pg_stat_tmp"
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
]


#
# note: it's a little quirky that caller must set up failed/failover so that failover is in gparray but
#                                 failed is not (if both set)...change that, or at least protect against problems
#
# Note the following uses:
#   failedSegment = segment that actually failed
#   liveSegment = segment to recover "from" (in order to restore the failed segment)
#   failoverSegment = segment to recover "to"
# In other words, we are recovering the failedSegment to the failoverSegment using the liveSegment.
class GpMirrorToBuild:
    def __init__(self, failedSegment, liveSegment, failoverSegment, forceFullSynchronization):
        checkNotNone("forceFullSynchronization", forceFullSynchronization)

        # We need to call this validate function here because addmirrors directly calls GpMirrorToBuild.
        RecoveryTriplet.validate(failedSegment, liveSegment, failoverSegment)

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
    class Progress:
        NONE = 0
        INPLACE = 1
        SEQUENTIAL = 2

    def __init__(self, toBuild, pool, quiet, parallelDegree, additionalWarnings=None, logger=logger, forceoverwrite=False, progressMode=Progress.INPLACE, parallelPerHost=gp.DEFAULT_SEGHOST_NUM_WORKERS):
        self.__mirrorsToBuild = toBuild
        self.__pool = pool
        self.__quiet = quiet
        self.__progressMode = progressMode
        self.__parallelDegree = parallelDegree
        # true for gprecoverseg and false for gpexpand and gpaddmirrors
        self.__forceoverwrite = forceoverwrite
        self.__parallelPerHost = parallelPerHost
        self.__additionalWarnings = additionalWarnings or []
        self.segments_to_mark_down = []
        self.mirrorsToStart = []
        self.full_recovery_dbids = {}

        if not logger:
            raise Exception('logger argument cannot be None')

        self.__logger = logger

    class ProgressCommand(gp.Command):
        """
        A Command, but with an associated DBID and log file path for use by
        _join_and_show_segment_progress(). This class is tightly coupled to that
        implementation.
        """
        def __init__(self, name, cmdStr, dbid, filePath, ctxt, remoteHost):
            super(GpMirrorListToBuild.ProgressCommand, self).__init__(name, cmdStr, ctxt, remoteHost)
            self.dbid = dbid
            self.filePath = filePath

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

    class RewindSegmentInfo:
        """
        Which segments to run pg_rewind during incremental recovery.  The
        targetSegment is of type gparray.Segment.  All progressFiles should have
        the same timeStamp.
        """
        def __init__(self, targetSegment, sourceHostname, sourcePort, timeStamp):
            self.targetSegment = targetSegment
            self.sourceHostname = sourceHostname
            self.sourcePort = sourcePort
            self.progressFile = '%s/pg_rewind.%s.dbid%s.out' % (gplog.get_logger_dir(),
                                                                timeStamp,
                                                                targetSegment.getSegmentDbId())

    def _cleanup_before_recovery(self, gpEnv):
        self._stop_failed_segments(gpEnv)

        self._wait_fts_to_mark_down_segments(gpEnv, self._get_segments_to_mark_down())


        if not self.__forceoverwrite:
            self._clean_up_failed_segments()
        self._set_seg_status_in_gparray()


    def _get_segments_to_mark_down(self):
        segments_to_mark_down = []
        for toRecover in self.__mirrorsToBuild:
            if toRecover.getFailedSegment() is not None:
                if toRecover.getFailedSegment().getSegmentStatus() == gparray.STATUS_UP:
                    segments_to_mark_down.append(toRecover.getFailedSegment())
        return segments_to_mark_down

    def _validate_gparray(self, gpArray):
        for toRecover in self.__mirrorsToBuild:
            if toRecover.getFailoverSegment() is not None:
                # no need to update the failed segment's information -- it is
                #   being overwritten in the configuration with the failover segment
                for gpArraySegment in gpArray.getDbList():
                    if gpArraySegment is toRecover.getFailedSegment():
                        raise Exception(
                            "failed segment should not be in the new configuration if failing over to new segment")

    def _set_seg_status_in_gparray(self):
        for toRecover in self.__mirrorsToBuild:
            target_seg = toRecover.getFailoverSegment() or toRecover.getFailedSegment()
            # down initially, we haven't started it yet
            target_seg.setSegmentStatus(gparray.STATUS_DOWN)
            target_seg.setSegmentMode(gparray.MODE_NOT_SYNC)
            # The change in configuration to of the mirror to down requires that
            # the primary also be marked as unsynchronized.
            live_seg = toRecover.getLiveSegment()
            live_seg.setSegmentMode(gparray.MODE_NOT_SYNC)


    def buildMirrors(self, actionName, gpEnv, gpArray):
        """
        Build the mirrors.

        gpArray must have already been altered to have updated directories -- that is, the failoverSegments
            from the mirrorsToBuild must be present in gpArray.

        """
        if len(self.__mirrorsToBuild) == 0:
            self.__logger.info("No segments to {}".format(actionName))
            return True
        self.__logger.info("%s segment(s) to %s" % (len(self.__mirrorsToBuild), actionName))

        self.checkForPortAndDirectoryConflicts(gpArray)

        for toRecover in self.__mirrorsToBuild:
            # TODO: remove this after adding pg_ctl start inside wrapper
            target_seg = toRecover.getFailoverSegment() or toRecover.getFailedSegment()
            self.mirrorsToStart.append(target_seg)

            if toRecover.isFullSynchronization():
                if target_seg.getSegmentDbId() > 0:
                    self.full_recovery_dbids[target_seg.getSegmentDbId()] = True

        self._cleanup_before_recovery(gpEnv)

        self._validate_gparray(gpArray)

        self._run_recovery()

        # should use mainUtils.getProgramName but I can't make it work!
        programName = os.path.split(sys.argv[0])[-1]

        # Disable Ctrl-C, going to save metadata in database and transition segments
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            self.__logger.info("Updating configuration with new mirrors")
            configInterface.getConfigurationProvider().updateSystemConfig(
                gpArray,
                "%s: segment config for resync" % programName,
                dbIdToForceMirrorRemoveAdd=self.full_recovery_dbids,
                useUtilityMode=False,
                allowPrimary=False
            )

            self.__logger.info("Starting mirrors")
            start_all_successful = self.__startAll(gpEnv, gpArray, self.mirrorsToStart)
        finally:
            # Re-enable Ctrl-C
            signal.signal(signal.SIGINT, signal.default_int_handler)

        return start_all_successful

    def checkForPortAndDirectoryConflicts(self, gpArray):
        """
        Check gpArray for internal consistency -- no duplicate ports or directories on the same host, for example

        A detected problem causes an Exception to be raised
        """

        for hostName, segmentArr in GpArray.getSegmentsByHostName(gpArray.getDbList()).items():
            usedPorts = {}
            usedDataDirectories = {}
            for segment in segmentArr:

                # check for port conflict
                port = segment.getSegmentPort()
                dbid = segment.getSegmentDbId()
                if port in usedPorts:
                    raise Exception(
                        "Segment dbid's %s and %s on host %s cannot have the same port %s." %
                        (dbid, usedPorts.get(port), hostName, port))

                usedPorts[port] = dbid

                # check for directory conflict; could improve this by reporting nicer the conflicts
                path = segment.getSegmentDataDirectory()

                if path in usedDataDirectories:
                    raise Exception(
                        "Segment dbid's %s and %s on host %s cannot have the same data directory '%s'." %
                        (dbid, usedDataDirectories.get(path), hostName, path))
                usedDataDirectories[path] = dbid

    def _join_and_show_segment_progress(self, cmds, inplace=False,
                                        outfile=sys.stdout, interval=1):
        written = False

        def print_progress():
            if written and inplace:
                outfile.write("\x1B[%dA" % len(cmds))

            output = []
            for cmd in cmds:
                try:
                    # since print_progress is called multiple times,
                    # cache cmdStr to reset it after being mutated by cmd.run()
                    cmd_str = cmd.cmdStr
                    cmd.run(validateAfter=True)
                    cmd.cmdStr = cmd_str
                    results = cmd.get_results().stdout.rstrip()
                    if not results:
                        results = "skipping pg_rewind on mirror as standby.signal is present"
                except ExecutionError:
                    lines = cmd.get_results().stderr.splitlines()
                    if lines:
                        results = lines[0]
                    else:
                        results = ''

                output.append("%s (dbid %d): %s" % (cmd.remoteHost, cmd.dbid, results))
                if inplace:
                    output.append("\x1B[K")
                output.append("\n")

            outfile.write("".join(output))
            outfile.flush()

        while not self.__pool.join(interval):
            print_progress()
            written = True

        # Make sure every line is updated with the final status.
        print_progress()

    # There is race between when the recovery process creates the progressFile
    # when this progressCmd is run. Thus, the progress command touches
    # the file to ensure its presence before tailing.
    def __getProgressAndRemoveCmds(self, progressFile, targetSegmentDbId, targetHostname):
        progressCmd = None
        if self.__progressMode != GpMirrorListToBuild.Progress.NONE:
            progressCmd = GpMirrorListToBuild.ProgressCommand("tail the last line of the file",
                                                              "set -o pipefail; touch -a {0}; tail -1 {0} | tr '\\r' '\\n' | tail -1".format(
                                                                  pipes.quote(progressFile)),
                                                              targetSegmentDbId,
                                                              progressFile,
                                                              ctxt=base.REMOTE,
                                                              remoteHost=targetHostname)

        removeCmd = base.Command("remove file",
                                 "rm -f %s" % pipes.quote(progressFile),
                                 ctxt=base.REMOTE,
                                 remoteHost=targetHostname)

        return progressCmd, removeCmd

    def __runWaitAndCheckWorkerPoolForErrorsAndClear(self, cmds, suppressErrorCheck=False, progressCmds=[]):
        for cmd in cmds:
            self.__pool.addCommand(cmd)

        if self.__quiet:
            self.__pool.join()
        elif progressCmds:
            self._join_and_show_segment_progress(progressCmds,
                                                 inplace=self.__progressMode == GpMirrorListToBuild.Progress.INPLACE)
        else:
            base.join_and_indicate_progress(self.__pool)

        if not suppressErrorCheck:
            self.__pool.check_results()

        completedRecoveryCmds = list(set(self.__pool.getCompletedItems()) & set(cmds))

        self.__pool.empty_completed_items()

        return completedRecoveryCmds

    def __copyFiles(self, srcDir, destDir, fileNames):
        for name in fileNames:
            cmd = gp.LocalCopy("copy file for segment", srcDir + "/" + name, destDir + "/" + name)
            cmd.run(validateAfter=True)

    def __createEmptyDirectories(self, dir, newDirectoryNames):
        for name in newDirectoryNames:
            subDir = os.path.join(dir, name)
            unix.MakeDirectory("create blank directory for segment", subDir).run(validateAfter=True)
            unix.Chmod.local('set permissions on blank dir', subDir, '0700')

    def _run_recovery(self): #TODO add tests ?
        recovery_info_by_host = recoveryinfo.build_recovery_info(self.__mirrorsToBuild)

        def createSegRecoveryCommand(host_name, cmd_label, validate_only):
            recovery_info_list = recovery_info_by_host[host_name]
            checkNotNone("segmentInfo for %s" % host_name, recovery_info_list)
            if validate_only:

                return gp.GpSegSetupRecovery(cmd_label,
                                       recoveryinfo.serialize_recovery_info_list(recovery_info_list),
                                       gplog.get_logger_dir(),
                                       verbose=gplog.logging_is_verbose(),
                                       batchSize=self.__parallelPerHost,
                                       remoteHost=host_name,
                                       forceoverwrite=self.__forceoverwrite)
            else:
                return gp.GpSegRecovery(cmd_label,
                                       recoveryinfo.serialize_recovery_info_list(recovery_info_list),
                                       gplog.get_logger_dir(),
                                       verbose=gplog.logging_is_verbose(),
                                       batchSize=self.__parallelPerHost,
                                       remoteHost=host_name,
                                       forceoverwrite=self.__forceoverwrite)
        #
        # validate directories for target segments

        self.__logger.info('Setting up the required segments for recovery')
        cmds = []
        for hostName in list(recovery_info_by_host.keys()):
            cmds.append(createSegRecoveryCommand(hostName,
                                                 'Validate data directories for '
                                                 'full recovery', True))
        for cmd in cmds:
            self.__pool.addCommand(cmd)

        if self.__quiet:
            self.__pool.join()
        else:
            base.join_and_indicate_progress(self.__pool)

        validationErrors = []
        for item in self.__pool.getCompletedItems():
            results = item.get_results()
            if not results.wasSuccessful():
                if results.rc == 1:
                    # stdoutFromFailure = results.stdout.replace("\n", " ").strip()
                    lines = results.stderr.split("\n")
                    for line in lines:
                        if len(line.strip()) > 0:
                            #TODO add a behave test for this error
                            validationErrors.append("Validation failure on host %s %s" % (item.remoteHost, line))
                else:
                    validationErrors.append(str(item))
        self.__pool.empty_completed_items()
        if validationErrors:
            raise ExceptionNoStackTraceNeeded("\n" + ("\n".join(validationErrors)))

        # Configure a new segment
        #
        # Recover segments using gpsegrecovery, which will internally call either
        # pg_basebackup or pg_rewind. gprecoverseg generates a log filename which is
        # passed to gpsegrecovery using the confinfo parameter. gprecoverseg
        # tails this file to show recovery progress to the user, and removes the
        # file when done. A new file is generated for each run of gprecoverseg
        # based on a timestamp.

        self.__logger.info('Running recovery for the required segments')
        cmds = []
        progressCmds = []
        removeCmds= []
        for hostName in list(recovery_info_by_host.keys()):
            for ri in recovery_info_by_host[hostName]:
                progressCmd, removeCmd = self.__getProgressAndRemoveCmds(ri.progress_file,
                                                                         ri.target_segment_dbid,
                                                                         hostName)
                removeCmds.append(removeCmd)
                if progressCmd:
                    progressCmds.append(progressCmd)

            cmds.append(
                createSegRecoveryCommand(hostName, 'Recover segments', False))

        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, suppressErrorCheck=False, progressCmds=progressCmds)

        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(removeCmds, suppressErrorCheck=False)

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
        cmdStr = """python -c 'import os; print(os.path.realpath("%s"))'""" % datadir
        cmd = base.Command('dereference a symlink on a remote host', cmdStr=cmdStr, ctxt=base.REMOTE, remoteHost=host)
        cmd.run()
        results = cmd.get_results()
        if results.rc != 0:
            self.__logger.warning('Unable to determine if %s is symlink. Assuming it is not symlink' % (datadir))
            return datadir
        return results.stdout.strip()

    def _get_failed_segments(self):
        # will stop the failed segment.  Note that we do this even if we are recovering to a different location!
        return [ toRecover.getFailedSegment() for toRecover in self.__mirrorsToBuild if toRecover.getFailedSegment() is not None ]

    #TODO add tests for this function ??
    def _stop_failed_segments(self, gpEnv):
        failed_segments = self._get_failed_segments()
        if len(failed_segments) == 0:
            return

        self.__logger.info("Ensuring %d failed segment(s) are stopped" % (len(failed_segments)))
        segments = self._get_running_postgres_segments(failed_segments)
        segmentByHost = GpArray.getSegmentsByHostName(segments)


        cmds = []
        for hostName, segments in segmentByHost.items():
            cmd = gp.GpSegStopCmd("remote segment stop on host '%s'" % hostName,
                                  gpEnv.getGpHome(), gpEnv.getGpVersion(),
                                  mode='fast', dbs=segments, verbose=gplog.logging_is_verbose(),
                                  ctxt=base.REMOTE, remoteHost=hostName, segment_batch_size=self.__parallelPerHost)

            cmds.append(cmd)

        # we suppress checking for the error.  This is because gpsegstop will actually error
        #  in many cases where the stop is actually done (that is, for example, the segment is
        #  running but slow to shutdown so gpsegstop errors after whacking it with a kill)
        #
        # Perhaps we should make it so that it so that is checks if the seg is running and only attempt stop
        #  if it's running?  In that case, we could propagate the error
        #
        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, suppressErrorCheck=True)

    def _wait_fts_to_mark_down_segments(self, gpEnv, segments_to_mark_down):
        """Waits for FTS prober to mark segments as down"""

        wait_time = 60 * 30  # Wait up to 30 minutes to handle very large, busy
        # clusters that may have faults.  In most cases the
        # actual time to wait will be small and this operation
        # is only needed when moving mirrors that are up and
        # needed to be stopped, an uncommon operation.

        dburl = dbconn.DbURL(port=gpEnv.getCoordinatorPort(), dbname='template1')

        time_elapsed = 0
        seg_up_count = 0
        initial_seg_up_count = len(segments_to_mark_down)
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
            for segdb in segments_to_mark_down:
                if segdb.getSegmentDbId() in seg_db_map and seg_db_map[segdb.getSegmentDbId()].isSegmentUp():
                    seg_up_count += 1
            if seg_up_count == 0:
                break
            else:
                if last_seg_up_count != seg_up_count:
                    print("\n", end=' ')
                    #TODO fix - this message prints negative values
                    self.__logger.info("%d of %d segments have been marked down." %
                                (initial_seg_up_count - seg_up_count, initial_seg_up_count))
                    last_seg_up_count = seg_up_count

                for _i in range(1, 5):
                    time.sleep(1)
                    sys.stdout.write(".")
                    sys.stdout.flush()

                time_elapsed += 5

        if seg_up_count == 0:
            print("\n", end=' ')
            self.__logger.info("%d of %d segments have been marked down." %
                        (initial_seg_up_count, initial_seg_up_count))
        else:
            raise Exception("%d segments were not marked down by FTS" % seg_up_count)

    def _clean_up_failed_segments(self):
        segments_to_clean_up = []
        for toRecover in self.__mirrorsToBuild:
            is_in_place = toRecover.getFailedSegment() is not None and toRecover.getFailoverSegment() is None
            if is_in_place and toRecover.isFullSynchronization():
                segments_to_clean_up.append(toRecover.getFailedSegment())

        if len(segments_to_clean_up) == 0:
            return

        self.__logger.info("Cleaning files from %d segment(s)" % (len(segments_to_clean_up)))
        segments_to_clean_up_by_host = GpArray.getSegmentsByHostName(segments_to_clean_up)

        cmds = []
        for hostName, segments_to_clean_up in segments_to_clean_up_by_host.items():
            cmds.append(gp.GpCleanSegmentDirectories("clean segment directories on %s" % hostName,
                                                     segments_to_clean_up, gp.REMOTE, hostName))

        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds)

    def __createStartSegmentsOp(self, gpEnv):
        return startSegments.StartSegmentsOperation(self.__pool, self.__quiet,
                                                    gpEnv.getGpVersion(),
                                                    gpEnv.getGpHome(), gpEnv.getCoordinatorDataDir(),
                                                    parallel=self.__parallelPerHost)

    # FIXME: This function seems to be unused. Remove if not required.
    def __updateGpIdFile(self, gpEnv, gpArray, segments):
        segmentByHost = GpArray.getSegmentsByHostName(segments)
        newSegmentInfo = gp.ConfigureNewSegment.buildSegmentInfoForNewSegment(segments)

        cmds = []
        for hostName in list(segmentByHost.keys()):
            segmentInfo = newSegmentInfo[hostName]
            checkNotNone("segmentInfo for %s" % hostName, segmentInfo)
            cmd = gp.ConfigureNewSegment("update gpid file",
                                         segmentInfo,
                                         gplog.get_logger_dir(),
                                         newSegments=False,
                                         verbose=gplog.logging_is_verbose(),
                                         batchSize=self.__parallelPerHost,
                                         ctxt=gp.REMOTE,
                                         remoteHost=hostName,
                                         validationOnly=False,
                                         writeGpIdFileOnly=True)

            cmds.append(cmd)
        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds)

    def __startAll(self, gpEnv, gpArray, segments):

        # the newly started segments should belong to the current era
        era = read_era(gpEnv.getCoordinatorDataDir(), logger=self.__logger)

        segmentStartResult = self.__createStartSegmentsOp(gpEnv).startSegments(gpArray, segments,
                                                                               startSegments.START_AS_MIRRORLESS,
                                                                               era)
        start_all_successfull = len(segmentStartResult.getFailedSegmentObjs()) == 0
        for failure in segmentStartResult.getFailedSegmentObjs():
            failedSeg = failure.getSegment()
            failureReason = failure.getReason()
            self.__logger.warn(
                "Failed to start segment.  The fault prober will shortly mark it as down. Segment: %s: REASON: %s" % (
                failedSeg, failureReason))

        return start_all_successfull

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
