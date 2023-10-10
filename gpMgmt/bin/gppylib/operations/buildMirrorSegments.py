from contextlib import closing
import os
import pipes
import signal
import time
import re

from gppylib.recoveryinfo import RecoveryResult
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
from gppylib.commands.unix import check_pid_on_remotehost
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


def get_recovery_progress_file(gplog):
    # recovery progress file on the coordinator, used by gpstate to read and show progress
    return "{}/recovery_progress.file".format(gplog.get_logger_dir())


def get_recovery_progress_pattern(recovery_type='incremental'):
    """
    Returns the pattern of progress result for various type of recoveries
    For example:
        progress of pg_basebackup/pg_rewind look like: "164848/1371715 kB (84%)"
        progress of rsync looks like: "1,036,923,510  99%   39.90MB/s    0:00:24"
    """
    if recovery_type == 'differential':
        return r" +\d+%\ +\d+.\d+(kB|mB)\/s"
    return r"\d+\/\d+ (kB|mB) \(\d+\%\)"


def get_recovery_type(file_basename):
    """
    To get the recovery type based on progress file name
    params: file_basename: path base of progress file created
    returns: string which represents recovery type
    """
    recovery_type = 'incremental'
    if file_basename == 'pg_basebackup':
        recovery_type = 'full'
    elif file_basename == 'rsync':
        recovery_type = 'differential'
    return recovery_type


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
    def __init__(self, failedSegment, liveSegment, failoverSegment, forceFullSynchronization, differentialSynchronization):
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

        """
        __differentialSynchronization is true if differential resynchronization should be done -- that is only 
        the delta between the source and target datadir will be copied over to the target server
        """

        self.__differentialSynchronization = differentialSynchronization

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

    def isDifferentialSynchronization(self):
        """
        Returns whether or not this segment to recover needs to be recovered using differential synchronization method
        """
        return self.__differentialSynchronization


class GpMirrorListToBuild:
    class Progress:
        NONE = 0
        INPLACE = 1
        SEQUENTIAL = 2

    class Action:
        ADDMIRRORS='add'
        RECOVERMIRRORS='recover'

    def __init__(self, toBuild, pool, quiet, parallelDegree, additionalWarnings=None, logger=logger, forceoverwrite=False, progressMode=Progress.INPLACE, parallelPerHost=gp.DEFAULT_SEGHOST_NUM_WORKERS):
        self.__mirrorsToBuild = toBuild
        self.__pool = pool
        self.__quiet = quiet
        self.__progressMode = progressMode
        self.__parallelDegree = parallelDegree
        # true for gprecoverseg and gpmovemirrors; false for gpexpand and gpaddmirrors
        self.__forceoverwrite = forceoverwrite
        self.__parallelPerHost = parallelPerHost
        self.__additionalWarnings = additionalWarnings or []
        self.segments_to_mark_down = []
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

    def _cleanup_before_recovery(self, gpArray, gpEnv):
        self.checkForPortAndDirectoryConflicts(gpArray)
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

    def add_mirrors(self, gpEnv, gpArray):
        return self.__build_mirrors(GpMirrorListToBuild.Action.ADDMIRRORS, gpEnv, gpArray)

    def recover_mirrors(self, gpEnv, gpArray):
        return self.__build_mirrors(GpMirrorListToBuild.Action.RECOVERMIRRORS, gpEnv, gpArray)

    def __build_mirrors(self, actionName, gpEnv, gpArray):
        """
        Build the mirrors.

        gpArray must have already been altered to have updated directories -- that is, the failoverSegments
            from the mirrorsToBuild must be present in gpArray.

        """
        if len(self.__mirrorsToBuild) == 0:
            self.__logger.info("No segments to {}".format(actionName))
            return True

        if actionName not in [GpMirrorListToBuild.Action.ADDMIRRORS, GpMirrorListToBuild.Action.RECOVERMIRRORS]:
            raise Exception('Invalid action. Valid values are {} and {}'.format(GpMirrorListToBuild.Action.RECOVERMIRRORS,
                                                                                GpMirrorListToBuild.Action.ADDMIRRORS))

        self.__logger.info("%s segment(s) to %s" % (len(self.__mirrorsToBuild), actionName))

        self._cleanup_before_recovery(gpArray, gpEnv)
        self._validate_gparray(gpArray)

        recovery_info_by_host = recoveryinfo.build_recovery_info(self.__mirrorsToBuild)

        self._run_setup_recovery(actionName, recovery_info_by_host)

        backout_map = self._update_config(recovery_info_by_host, gpArray)

        recovery_results = self._run_recovery(actionName, recovery_info_by_host, gpEnv)
        if actionName == GpMirrorListToBuild.Action.RECOVERMIRRORS:
            self._revert_config_update(recovery_results, backout_map)

        self._trigger_fts_probe(port=gpEnv.getCoordinatorPort())

        return recovery_results.recovery_successful()

    def _trigger_fts_probe(self, port=0):
        self.__logger.info('Triggering FTS probe')
        conn = dbconn.connect(dbconn.DbURL(port=port))

        # XXX Perform two probe scans in a row, to work around a known
        # race where gp_request_fts_probe_scan() can return early during the
        # first call. Remove this duplication once that race is fixed.
        for _ in range(2):
            dbconn.execSQL(conn,"SELECT gp_request_fts_probe_scan()")
        conn.close()

    def _update_config(self, recovery_info_by_host, gpArray):
        # should use mainUtils.getProgramName but I can't make it work!
        programName = os.path.split(sys.argv[0])[-1]

        full_recovery_dbids = {}
        for host_name, recovery_info_list in recovery_info_by_host.items():
            for ri in recovery_info_list:
                if ri.is_full_recovery:
                    full_recovery_dbids[ri.target_segment_dbid] = True

        # Disable Ctrl-C, going to save metadata in database and transition segments
        old_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        backout_map = None
        try:
            self.__logger.info("Updating configuration for mirrors")
            backout_map = configInterface.getConfigurationProvider().updateSystemConfig(
                gpArray,
                "%s: segment config for resync" % programName,
                dbIdToForceMirrorRemoveAdd=full_recovery_dbids,
                useUtilityMode=False,
                allowPrimary=False
            )

            self.__logger.debug("Generating configuration backout scripts")
        finally:
            # Re-enable Ctrl-C
            signal.signal(signal.SIGINT, old_handler)
            return backout_map

    def _remove_progress_files(self, recovery_info_by_host, recovery_results):
        remove_progress_file_cmds = []
        for hostName, recovery_info_list in recovery_info_by_host.items():
            for ri in recovery_info_list:
                if recovery_results.was_bb_rewind_rsync_successful(ri.target_segment_dbid):
                    remove_progress_file_cmds.append(self._get_remove_cmd(ri.progress_file, hostName))
        self.__runWaitAndCheckWorkerPoolForErrorsAndClear(remove_progress_file_cmds, suppressErrorCheck=False)

    def _revert_config_update(self, recovery_results, backout_map):
        if len(backout_map) == 0:
            return
        if recovery_results.full_recovery_successful():
            return

        final_sql = "SET allow_system_table_mods=true;\n"
        for dbid in backout_map:
            #TODO 1. we don't need to check for both bb and rewind
            #TODO 2. we can ignore incremental dbids. Ideally incremental dbids won't have a backout script
            # but being explicit in the code will make the intent clear
            if recovery_results.was_bb_rewind_rsync_successful(dbid):
                continue
            for statement in backout_map[dbid]:
                final_sql += "{};\n".format(statement)

        self.__logger.debug("Some mirrors failed during basebackup. Reverting the gp_segment_configuration updates for"
                            " these mirrors")
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            with closing(dbconn.connect(dbconn.DbURL(dbname='template1'), unsetSearchPath=False, utility=True)) as conn:
                dbconn.execSQL(conn, "BEGIN")
                dbconn.executeUpdateOrInsert(conn, final_sql, 1)
                dbconn.execSQL(conn, "COMMIT")
        finally:
            signal.signal(signal.SIGINT, signal.default_int_handler)
        self.__logger.debug("Successfully reverted the gp_segment_configuration updates for the failed mirrors")


    def remove_postmaster_pid_from_remotehost(self, host, datadir):
        cmd = base.Command(name = 'remove the postmaster.pid file',
                           cmdStr = 'rm -f %s/postmaster.pid' % datadir,
                           ctxt=gp.REMOTE, remoteHost = host)
        cmd.run()

        return_code = cmd.get_return_code()
        if return_code != 0:
            raise ExecutionError("Failed while trying to remove postmaster.pid.", cmd)

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

    def _join_and_show_segment_progress(self, cmds, inplace=False, outfile=sys.stdout, interval=1):

        def print_progress():
            if written and inplace:
                outfile.write("\x1B[%dA" % len(cmds))

            complete_progress_output = []

            output = []
            for cmd in cmds:
                try:
                    # since print_progress is called multiple times,
                    # cache cmdStr to reset it after being mutated by cmd.run()
                    cmd_str = cmd.cmdStr
                    cmd.run(validateAfter=True)
                    cmd.cmdStr = cmd_str
                    results = cmd.get_results().stdout.rstrip()
                    recovery_type = get_recovery_type(os.path.basename(cmd.filePath).split('.')[0])
                    if not results and recovery_type == 'incremental':
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

                if re.search(diff_pattern, results) or re.search(rewind_bb_pattern, results):
                    complete_progress_output.extend("%s:%d:%s\n" % (recovery_type, cmd.dbid, results))

            combined_progress_file.write("".join(complete_progress_output))
            combined_progress_file.flush()

            try:
                outfile.write("".join(output))
                outfile.flush()

            # During SSH disconnections, writing to stdout might not be possible. So ignore the
            # error, but continue writing to the recovery_progress file.
            except IOError:
                pass

        written = False
        combined_progress_filepath = get_recovery_progress_file(gplog)
        rewind_bb_pattern = re.compile(get_recovery_progress_pattern())
        diff_pattern = re.compile(get_recovery_progress_pattern('differential'))
        try:
            with open(combined_progress_filepath, 'w') as combined_progress_file:
                while not self.__pool.join(interval):
                    print_progress()
                    written = True
                # Make sure every line is updated with the final status.
                print_progress()
        finally:
            if os.path.exists(combined_progress_filepath):
                os.remove(combined_progress_filepath)


    def _get_progress_cmd(self, progressFile, targetSegmentDbId, targetHostname):
        """
        # There is race between when the recovery process creates the progressFile
        # when this progress cmd is run. Thus, the progress command touches
        # the file to ensure its presence before tailing.
        """
        if self.__progressMode != GpMirrorListToBuild.Progress.NONE:
            return GpMirrorListToBuild.ProgressCommand("tail the last line of the file",
                                                       "set -o pipefail; touch -a {0}; tail -1 {0} | tr '\\r' '\\n' |"
                                                       " tail -1".format(pipes.quote(progressFile)),
                                                       targetSegmentDbId, progressFile, ctxt=base.REMOTE,
                                                       remoteHost=targetHostname)
        return None

    def _get_remove_cmd(self, remove_file, target_host):
        return base.Command("remove file", "rm -f {}".format(pipes.quote(remove_file)), ctxt=base.REMOTE, remoteHost=target_host)

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

        completed_cmds = list(set(self.__pool.getCompletedItems()) & set(cmds))

        self.__pool.empty_completed_items()

        return completed_cmds

    def _run_setup_recovery(self, action_name, recovery_info_by_host):
        completed_setup_results = self._do_setup_for_recovery(recovery_info_by_host)
        setup_recovery_results = RecoveryResult(action_name, completed_setup_results, self.__logger)
        setup_recovery_results.print_setup_recovery_errors()
        #FIXME we should raise this exception outside the function
        if not setup_recovery_results.setup_successful():
            raise ExceptionNoStackTraceNeeded()

        return setup_recovery_results

    def _run_recovery(self, action_name, recovery_info_by_host, gpEnv):
        completed_recovery_results = self._do_recovery(recovery_info_by_host, gpEnv)
        recovery_results = RecoveryResult(action_name, completed_recovery_results, self.__logger)
        recovery_results.print_bb_rewind_differential_update_and_start_errors()

        self._remove_progress_files(recovery_info_by_host, recovery_results)
        return recovery_results

    def _do_recovery(self, recovery_info_by_host, gpEnv):
        """
        # Recover and start segments using gpsegrecovery, which will internally call either
        # pg_basebackup or pg_rewind. gprecoverseg generates a log filename which is
        # passed to gpsegrecovery using the confinfo parameter. gprecoverseg
        # tails this file to show recovery progress to the user, and removes the
        # file when done. A new file is generated for each run of gprecoverseg
        # based on a timestamp.
        :param gpEnv:
        :param recovery_info_by_host:
        :return:
        """
        self.__logger.info('Initiating segment recovery. Upon completion, will start the successfully recovered segments')
        cmds = []
        progress_cmds = []
        era = read_era(gpEnv.getCoordinatorDataDir(), logger=self.__logger)
        for hostName, recovery_info_list in recovery_info_by_host.items():
            for ri in recovery_info_list:
                progressCmd = self._get_progress_cmd(ri.progress_file, ri.target_segment_dbid, hostName)
                if progressCmd:
                    progress_cmds.append(progressCmd)

            cmds.append(gp.GpSegRecovery('Recover segments',
                                         recoveryinfo.serialize_list(recovery_info_list),
                                         gplog.get_logger_dir(),
                                         verbose=gplog.logging_is_verbose(),
                                         batchSize=self.__parallelPerHost,
                                         remoteHost=hostName,
                                         era=era,
                                         forceoverwrite=self.__forceoverwrite))
        completed_recovery_results = self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds, suppressErrorCheck=True,
                                                                                       progressCmds=progress_cmds)
        return completed_recovery_results

    def _do_setup_for_recovery(self, recovery_info_by_host):
        self.__logger.info('Setting up the required segments for recovery')
        cmds = []
        for host_name, recovery_info_list in recovery_info_by_host.items():
            cmds.append(gp.GpSegSetupRecovery('Run validation checks and setup data directories for recovery',
                                              recoveryinfo.serialize_list(recovery_info_list),
                                              gplog.get_logger_dir(),
                                              verbose=gplog.logging_is_verbose(),
                                              batchSize=self.__parallelPerHost,
                                              remoteHost=host_name,
                                              forceoverwrite=self.__forceoverwrite))
        for cmd in cmds:
            self.__pool.addCommand(cmd)
        if self.__quiet:
            self.__pool.join()
        else:
            base.join_and_indicate_progress(self.__pool)
        completed_results = self.__pool.getCompletedItems()
        self.__pool.empty_completed_items()
        return completed_results

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
        cmdStr = """python3 -c 'import os; print(os.path.realpath("%s"))'""" % datadir
        cmd = base.Command('dereference a symlink on a remote host', cmdStr=cmdStr, ctxt=base.REMOTE, remoteHost=host)
        cmd.run()
        results = cmd.get_results()
        if results.rc != 0:
            self.__logger.warning('Unable to determine if %s is symlink. Assuming it is not symlink' % (datadir))
            return datadir
        return results.stdout.strip()

    def _get_failed_reachable_segments(self):
        # will stop the failed segment.  Note that we do this even if we are recovering to a different location!
        failed_reachable_segments = []
        for toRecover in self.__mirrorsToBuild:
            failed = toRecover.getFailedSegment()
            if failed is not None:
                if failed.unreachable:
                    self.__logger.info('Skipping gpsegstop on unreachable host: %s segment: %s'
                                       % (failed.getSegmentHostName(), failed.getSegmentContentId()))
                else:
                    failed_reachable_segments.append(failed)
        return failed_reachable_segments

    def _stop_failed_segments(self, gpEnv):
        failed_reachable_segments = self._get_failed_reachable_segments()
        if len(failed_reachable_segments) == 0:
            return

        self.__logger.info("Ensuring %d failed segment(s) are stopped" % (len(failed_reachable_segments)))
        segments = self._get_running_postgres_segments(failed_reachable_segments)
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
                    #FIXME - this message prints negative values
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
    # def __updateGpIdFile(self, gpEnv, gpArray, segments):
    #     segmentByHost = GpArray.getSegmentsByHostName(segments)
    #     newSegmentInfo = gp.ConfigureNewSegment.buildSegmentInfoForNewSegment(segments)
    #
    #     cmds = []
    #     for hostName in list(segmentByHost.keys()):
    #         segmentInfo = newSegmentInfo[hostName]
    #         checkNotNone("segmentInfo for %s" % hostName, segmentInfo)
    #         cmd = gp.ConfigureNewSegment("update gpid file",
    #                                      segmentInfo,
    #                                      gplog.get_logger_dir(),
    #                                      newSegments=False,
    #                                      verbose=gplog.logging_is_verbose(),
    #                                      batchSize=self.__parallelPerHost,
    #                                      ctxt=gp.REMOTE,
    #                                      remoteHost=hostName,
    #                                      validationOnly=False,
    #                                      writeGpIdFileOnly=True)
    #
    #         cmds.append(cmd)
    #     self.__runWaitAndCheckWorkerPoolForErrorsAndClear(cmds)


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
