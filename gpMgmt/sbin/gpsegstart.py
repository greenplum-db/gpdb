#!/usr/bin/env python
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103
#
# Copyright (c) EMC/Greenplum Inc 2011. All Rights Reserved.
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#
"""
Internal Use Function.
"""

# THIS IMPORT MUST COME FIRST
from gppylib.mainUtils import simple_main, addStandardLoggingAndHelpOptions

import os, pickle, base64, time

from gppylib.gpparseopts import OptParser, OptChecker
from gppylib import gparray, gplog
from gppylib.commands import base, gp
from gppylib.utils import parseKeyColonValueLines
from gppylib.commands.pg import PgControlData

logger = gplog.get_default_logger()


DESCRIPTION = """
This utility is NOT SUPPORTED and is for internal-use only.
Starts a set of one or more segment databases.
"""

HELP = ["""
Utility should only be used by other GP utilities.  

Return codes:
  0 - All segments started successfully
  1 - At least one segment didn't start successfully

"""]




class StartResult:
    """
    Recorded result information from an attempt to start one segment.
    """

    def __init__(self, datadir, started, reason, reasoncode):
        """
        @param datadir
        @param started
        @param reason
        @param reasoncode one of the gp.SEGSTART_* values
        """
        self.datadir    = datadir
        self.started    = started
        self.reason     = reason
        self.reasoncode = reasoncode
    
    def __str__(self):
        return "".join([
                "STATUS", 
                "--DIR:", str(self.datadir),
                "--STARTED:", str(self.started),
                "--REASONCODE:", str(self.reasoncode),
                "--REASON:", str(self.reason)
                ])


class OverallStatus:
    """
    Mapping and segment status information for all segments on this host.
    """

    def __init__(self, dblist):
        """
        Build the datadir->segment mapping and remember the original size.
        Since segments which fail to start will be removed from the mapping, 
        we later test the size of the map against the original size when
        returning the appropriate status code to the caller.
        """
        self.dirmap          = dict([(seg.getSegmentDataDirectory(), seg) for seg in dblist])
        self.original_length = len(self.dirmap)
        self.results         = []
        self.logger          = logger


    def mark_failed(self, datadir, msg, reasoncode):
        """
        Mark a segment as failed during some startup process.
        Remove the entry for the segment from dirmap.

        @param datadir
        @param msg
        @param reasoncode one of the gp.SEGSTART_* constant values
        """
        self.logger.info("Marking failed %s, %s, %s" % (datadir, msg, reasoncode))
        self.results.append( StartResult(datadir=datadir, started=False, reason=msg, reasoncode=reasoncode) )
        del self.dirmap[datadir]


    def remaining_items_succeeded(self):
        """
        Add results for all remaining items in our datadir->segment map.
        """
        for datadir in self.dirmap.keys():
            self.results.append( StartResult(datadir=datadir, started=True, reason="Start Succeeded", reasoncode=gp.SEGSTART_SUCCESS ) )


    def log_results(self):
        """
        Log info messages with our results
        """
        status = '\nCOMMAND RESULTS\n' + "\n".join([str(result) for result in self.results])
        self.logger.info(status)


    def exit_code(self):
        """
        Return an appropriate exit code: 0 if no failures, 1 if some segments failed to start.
        """
        if len(self.dirmap) != self.original_length:
            return 1
        return 0



class GpSegStart:
    """
    Logic to start segment servers on this host.
    """

    def __init__(self, dblist, gpversion, mirroringMode, num_cids, era,
                 timeout, pickledTransitionData, specialMode, wrapper, wrapper_args,
                 master_checksum_version, logfileDirectory=False):

        # validate/store arguments
        #
        self.dblist                = map(gparray.Segment.initFromString, dblist)

        expected_gpversion         = gpversion
        actual_gpversion           = gp.GpVersion.local('local GP software version check', os.path.abspath(os.pardir))
        if actual_gpversion != expected_gpversion:
            raise Exception("Local Software Version does not match what is expected.\n"
                            "The local software version is: '%s'\n"
                            "But we were expecting it to be: '%s'\n"
                            "Please review and correct" % (actual_gpversion, expected_gpversion))

        self.mirroringMode         = mirroringMode
        self.num_cids              = num_cids
        self.era                   = era
        self.timeout               = timeout
        self.pickledTransitionData = pickledTransitionData

        assert(specialMode in [None, 'upgrade', 'maintenance'])
        self.specialMode           = specialMode

        self.wrapper               = wrapper
        self.wrapper_args          = wrapper_args

        # initialize state
        #
        self.pool                  = base.WorkerPool(numWorkers=len(dblist))
        self.logger                = logger
        self.overall_status        = None

        self.logfileDirectory      = logfileDirectory
        self.master_checksum_version = master_checksum_version

    def getOverallStatusKeys(self):
        return self.overall_status.dirmap.keys()

    # return True if all running
    # return False if not all running
    def checkPostmasters(self, must_be_running):
        """
        Check that segment postmasters have been started.
        @param must_be_running True if postmasters must be running by now.
        """
        self.logger.info("Checking segment postmasters... (must_be_running %s)" % must_be_running)

        all_running = True
        for datadir in self.getOverallStatusKeys():
            pid     = gp.read_postmaster_pidfile(datadir)
            running = gp.check_pid(pid)
            msg     = "Postmaster %s %srunning (pid %d)" % (datadir, "is " if running else "NOT ", pid)
            self.logger.info(msg)

            if not running:
                all_running = False

            if must_be_running and not running:
                reasoncode = gp.SEGSTART_ERROR_PG_CTL_FAILED
                self.overall_status.mark_failed(datadir, msg, reasoncode)

        return all_running


    def __validateDirectoriesAndSetupRecoveryStartup(self):
        """
        validate that the directories all exist and run recovery startup if needed
        """
        self.logger.info("Validating directories...")

        for datadir in self.overall_status.dirmap.keys():
            self.logger.info("Validating directory: %s" % datadir)

            if os.path.isdir(datadir):
                #
                # segment datadir exists
                #
                pg_log = os.path.join(datadir, 'pg_log')
                if not os.path.exists(pg_log):
                    os.mkdir(pg_log)
                    
                postmaster_pid = os.path.join(datadir, 'postmaster.pid')
                if os.path.exists(postmaster_pid):
                    self.logger.warning("postmaster.pid file exists, checking if recovery startup required")

                    msg = gp.recovery_startup(datadir)
                    if msg:
                        reasoncode = gp.SEGSTART_ERROR_STOP_RUNNING_SEGMENT_FAILED
                        self.overall_status.mark_failed(datadir, msg, reasoncode)

            else:
                #
                # segment datadir does not exist
                #
                msg = "Segment data directory does not exist for: '%s'" % datadir
                self.logger.warning(msg)

                reasoncode = gp.SEGSTART_ERROR_DATA_DIRECTORY_DOES_NOT_EXIST
                self.overall_status.mark_failed(datadir, msg, reasoncode)


    def __startSegments(self):
        """
        Start the segments themselves
        """
        self.logger.info("Starting segments... (mirroringMode %s)" % self.mirroringMode)

        for datadir, seg in self.overall_status.dirmap.items():

            if self.master_checksum_version != None:
                cmd = PgControlData(name='run pg_controldata', datadir=datadir)
                cmd.run(validateAfter=True)
                res = cmd.get_results()

                if res.rc != 0:
                    msg = "pg_controldata failed.\nstdout:%s\nstderr:%s\n" % (res.stdout, res.stderr)
                    reasoncode = gp.SEGSTART_ERROR_PG_CONTROLDATA_FAILED
                    self.overall_status.mark_failed(datadir, msg, reasoncode)
                    continue

                segment_heap_checksum_version = cmd.get_value('Data page checksum version')
                if segment_heap_checksum_version != self.master_checksum_version:
                    msg = "Segment checksum %s does not match master checksum %s.\n" % (segment_heap_checksum_version,
                                                                                        self.master_checksum_version)
                    reasoncode = gp.SEGSTART_ERROR_CHECKSUM_MISMATCH
                    self.overall_status.mark_failed(datadir, msg, reasoncode)
                    continue

            cmd = gp.SegmentStart("Starting seg at dir %s" % datadir, 
                                  seg,
                                  self.num_cids,
                                  self.era,
                                  self.mirroringMode,
                                  self.master_checksum_version,
                                  timeout=self.timeout,
                                  specialMode=self.specialMode,
                                  wrapper=self.wrapper,
                                  wrapper_args=self.wrapper_args)
            self.pool.addCommand(cmd)

        self.pool.join()

        for cmd in self.pool.getCompletedItems():
            res = cmd.get_results()
            if res.rc != 0:

                # we should also read in last entries in startup.log here
                
                datadir    = cmd.segment.getSegmentDataDirectory()
                msg        = "PG_CTL failed.\nstdout:%s\nstderr:%s\n" % (res.stdout, res.stderr)
                reasoncode = gp.SEGSTART_ERROR_PG_CTL_FAILED
                self.overall_status.mark_failed(datadir, msg, reasoncode)

        self.pool.empty_completed_items()



    def __convertSegments(self):
        """
        Inform segments of their role
        """
        if self.mirroringMode != 'quiescent':
            self.logger.info("Not transitioning segments, mirroringMode is %s..." % self.mirroringMode)
            return

        self.logger.info("Transitioning segments, mirroringMode is %s..."  % self.mirroringMode)

        transitionData = None
        if self.pickledTransitionData is not None:
            transitionData = pickle.loads(base64.urlsafe_b64decode(self.pickledTransitionData))

        # send transition messages to the segments
        #
        for datadir, seg in self.overall_status.dirmap.items():
            #
            # This cmd will deliver a message to the postmaster using gp_primarymirror
            # (look for the protocol message type PRIMARY_MIRROR_TRANSITION_REQUEST_CODE )
            #
            port = seg.getSegmentPort()
            cmd  = gp.SendFilerepTransitionMessage.buildTransitionMessageCommand(transitionData, datadir, port)

            self.pool.addCommand(cmd)
        self.pool.join()


        # examine the results from the segments
        #
        segments     = self.overall_status.dirmap.values()
        dataDirToSeg = gparray.GpArray.getSegmentsGroupedByValue(segments, gparray.Segment.getSegmentDataDirectory)
        toStop       = []
        cmds         = self.pool.getCompletedItems()

        for cmd in cmds:
            res = cmd.get_results()
            if res.rc == 0:
                continue

            # some form of failure
            #
            stdoutFromFailure = res.stdout.replace("\n", " ").strip()
            stderrFromFailure = res.stderr.replace("\n", " ").strip()
            shouldStop = False

            if res.rc == gp.SendFilerepTransitionMessage.TRANSITION_ERRCODE_ERROR_SERVER_DID_NOT_RETURN_DATA:
                msg        = "Segment did not respond to startup request; check segment logfile"
                reasoncode = gp.SEGSTART_ERROR_SERVER_DID_NOT_RESPOND

                # server crashed when sending response, should ensure it's stopped completely!
                shouldStop = True

            elif stderrFromFailure.endswith("failure: Error: MirroringFailure"):
                msg        = "Failure in segment mirroring; check segment logfile"
                reasoncode = gp.SEGSTART_ERROR_MIRRORING_FAILURE

            elif stderrFromFailure.endswith("failure: Error: PostmasterDied"):
                msg        = "Segment postmaster has exited; check segment logfile"
                reasoncode = gp.SEGSTART_ERROR_POSTMASTER_DIED

            elif stderrFromFailure.endswith("failure: Error: InvalidStateTransition"):
                msg        = "Not a valid operation at this time; check segment logfile"
                reasoncode = gp.SEGSTART_ERROR_INVALID_STATE_TRANSITION

                # This should never happen, but if it does then we will ensure process is gone
                shouldStop = True

            elif stderrFromFailure.endswith("failure: Error: ServerIsInShutdown"):
                msg        = "System is shutting down"
                reasoncode = gp.SEGSTART_ERROR_SERVER_IS_IN_SHUTDOWN

            else:
                if res.rc == gp.SendFilerepTransitionMessage.TRANSITION_ERRCODE_ERROR_SOCKET:

                    # Couldn't connect to server to do transition or got another problem
                    # communicating, must make sure it's halted!
                    shouldStop = True

                msg        = "Start failed; check segment logfile.  \"%s%s\"" % (stdoutFromFailure, stderrFromFailure)
                reasoncode = gp.SEGSTART_ERROR_OTHER

            self.overall_status.mark_failed(cmd.dataDir, msg, reasoncode)

            if shouldStop:
                assert len(dataDirToSeg[cmd.dataDir]) == 1, "Multiple segments with dir %s" % cmd.dataDir
                toStop.append( dataDirToSeg[cmd.dataDir][0] )


        # ensure segments in a bad state are stopped
        # 
        for seg in toStop:
            datadir, port = (seg.getSegmentDataDirectory(), seg.getSegmentPort())
            
            msg = "Stopping segment %s, %s because of failure sending transition" % (datadir, port)
            self.logger.info(msg)

            cmd = gp.SegmentStop('stop segment', datadir, mode="immediate")
            cmd.run(validateAfter=False)
            res = cmd.get_results()

            if res.rc == 0:
                self.logger.info("Stop of segment succeeded")
            else:
                stdoutFromFailure = res.stdout.replace("\n", " ").strip()
                stderrFromFailure = res.stderr.replace("\n", " ").strip()
                self.logger.info("Stop of segment failed: rc: %s\nstdout:%s\nstderr:%s" % \
                                (res.rc, stdoutFromFailure, stderrFromFailure))

    def run(self):
        """
        Logic to start the segments.
        """

        # we initialize an overall status object which maintains a mapping 
        # from each segment's data directory to the segment object as well 
        # as a list of specific success/failure results.
        #
        self.overall_status = OverallStatus(self.dblist)

        # Each of the next four steps executes operations which may cause segment
        # details to be removed from the mapping and recorded as failures.
        #
        self.__validateDirectoriesAndSetupRecoveryStartup()
        self.__startSegments()

        # Being paranoid, we frequently check for postmaster failures.
        # The postmasters should be running by now.
        #
        self.checkPostmasters(must_be_running=True)

        self.__convertSegments()
        self.checkPostmasters(must_be_running=True)

        # At this point any segments remaining in the mapping are assumed to
        # have successfully started.
        #
        self.overall_status.remaining_items_succeeded()
        self.overall_status.log_results()
        return self.overall_status.exit_code()

    

    def cleanup(self):
        """
        Cleanup worker pool resources
        """
        if self.pool:
            self.pool.haltWork()
    

    @staticmethod
    def createParser():
        """
        Create parser expected by simple_main
        """

        parser = OptParser(option_class=OptChecker,
                           description=' '.join(DESCRIPTION.split()),
                           version='%prog version main build dev')
        parser.setHelp(HELP)

        #
        # Note that this mirroringmode parameter should only be either mirrorless or quiescent.
        #   If quiescent then it is implied that there is pickled transition data that will be
        #   provided (using -p) to immediately convert to a primary or a mirror.
        #
        addStandardLoggingAndHelpOptions(parser, includeNonInteractiveOption=False)

        parser.add_option("-D", "--dblist", dest="dblist", action="append", type="string")
        parser.add_option("-M", "--mirroringmode", dest="mirroringMode", type="string")
        parser.add_option("-p", "--pickledTransitionData", dest="pickledTransitionData", type="string")
        parser.add_option("-V", "--gp-version", dest="gpversion", metavar="GP_VERSION", help="expected software version")
        parser.add_option("-n", "--numsegments", dest="num_cids", help="number of distinct content ids in cluster")
        parser.add_option("", "--era", dest="era", help="master era")
        parser.add_option("-t", "--timeout", dest="timeout", type="int", default=gp.SEGMENT_TIMEOUT_DEFAULT,
                          help="seconds to wait")
        parser.add_option('-U', '--specialMode', type='choice', choices=['upgrade', 'maintenance'],
                           metavar='upgrade|maintenance', action='store', default=None,
                           help='start the instance in upgrade or maintenance mode')
        parser.add_option('', '--wrapper', dest="wrapper", default=None, type='string')
        parser.add_option('', '--wrapper-args', dest="wrapper_args", default=None, type='string')
        parser.add_option('', '--master-checksum-version', dest="master_checksum_version", default=None, type='string', action="store")
        
        return parser

    @staticmethod
    def createProgram(options, args):
        """
        Create program expected by simple_main
        """
        logfileDirectory = options.ensure_value("logfileDirectory", False)
        return GpSegStart(options.dblist,
                          options.gpversion,
                          options.mirroringMode,
                          options.num_cids,
                          options.era,
                          options.timeout,
                          options.pickledTransitionData,
                          options.specialMode,
                          options.wrapper,
                          options.wrapper_args,
                          options.master_checksum_version,
                          logfileDirectory=logfileDirectory)

#------------------------------------------------------------------------- 
if __name__ == '__main__':
    mainOptions = { 'setNonuserOnToolLogger':True}
    simple_main( GpSegStart.createParser, GpSegStart.createProgram, mainOptions )
