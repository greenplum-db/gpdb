import sys
import signal
from gppylib.gparray import GpArray
from gppylib.db import dbconn
from gppylib.commands.gp import GpSegStopCmd
from gppylib.commands import base
from gppylib import gplog

from gppylib.operations.segment_reconfigurer import SegmentReconfigurer

MIRROR_PROMOTION_TIMEOUT=600


class ReconfigDetectionSQLQueryCommand(base.SQLCommand):
    """A distributed query that will cause the system to detect
    the reconfiguration of the system"""

    query = "SELECT * FROM gp_dist_random('gp_id')"

    def __init__(self, conn):
        base.SQLCommand.__init__(self, "Reconfig detection sql query")
        self.cancel_conn = conn

    def run(self):
        dbconn.execSQL(self.cancel_conn, self.query)


class GpSegmentRebalanceOperation:
    def __init__(self, gpEnv, gpArray, batch_size, segment_batch_size):
        self.gpEnv = gpEnv
        self.gpArray = gpArray
        self.batch_size = batch_size
        self.segment_batch_size = segment_batch_size
        self.logger = gplog.get_default_logger()

    def rebalance(self):
        self.logger.info("Determining primary and mirror segment pairs to rebalance")

        # The current implementation of rebalance calls "gprecoverseg -a" below.
        # Thus, if another balanced pair is not synchronized, or has a down mirror
        # that pair will be recovered as a side-effect of rebalancing.
        unbalanced_primary_segs = []
        for segmentPair in self.gpArray.segmentPairs:
            if segmentPair.balanced():
                continue

            if segmentPair.up() and segmentPair.reachable() and segmentPair.synchronized():
                unbalanced_primary_segs.append(segmentPair.primaryDB)
            else:
                self.logger.warning(
                    "Not rebalancing primary segment dbid %d with its mirror dbid %d because one is either down, unreachable, or not synchronized" \
                    % (segmentPair.primaryDB.dbid, segmentPair.mirrorDB.dbid))

        if not len(unbalanced_primary_segs):
            self.logger.info("No segments to rebalance")
            return True

        unbalanced_primary_segs = GpArray.getSegmentsByHostName(unbalanced_primary_segs)

        pool = base.WorkerPool(min(len(unbalanced_primary_segs), self.batch_size))
        try:
            # Disable ctrl-c
            signal.signal(signal.SIGINT, signal.SIG_IGN)

            self.logger.info("Stopping unbalanced primary segments...")
            for hostname in list(unbalanced_primary_segs.keys()):
                cmd = GpSegStopCmd("stop unbalanced primary segs",
                                   self.gpEnv.getGpHome(),
                                   self.gpEnv.getGpVersion(),
                                   'fast',
                                   unbalanced_primary_segs[hostname],
                                   ctxt=base.REMOTE,
                                   remoteHost=hostname,
                                   timeout=600,
                                   segment_batch_size=self.segment_batch_size)
                pool.addCommand(cmd)

            base.join_and_indicate_progress(pool)
            
            failed_count = 0
            completed = pool.getCompletedItems()
            for res in completed:
                if not res.get_results().wasSuccessful():
                    failed_count += 1

            allSegmentsStopped = (failed_count == 0)

            if not allSegmentsStopped:
                self.logger.warn("%d segments failed to stop.  A full rebalance of the" % failed_count)
                self.logger.warn("system is not possible at this time.  Please check the")
                self.logger.warn("log files, correct the problem, and run gprecoverseg -r")
                self.logger.warn("again.")
                self.logger.info("gprecoverseg will continue with a partial rebalance.")

            pool.empty_completed_items()
            segment_reconfigurer = SegmentReconfigurer(logger=self.logger,
                    worker_pool=pool, timeout=MIRROR_PROMOTION_TIMEOUT)
            segment_reconfigurer.reconfigure()

            # Final step is to issue a recoverseg operation to resync segments
            self.logger.info("Starting segment synchronization")
            original_sys_args = sys.argv[:]
            self.logger.info("=============================START ANOTHER RECOVER=========================================")
            # import here because GpRecoverSegmentProgram and GpSegmentRebalanceOperation have a circular dependency
            from gppylib.programs.clsRecoverSegment import GpRecoverSegmentProgram
            cmd_args = ['gprecoverseg', '-a', '-B', str(self.batch_size), '-b', str(self.segment_batch_size)]
            sys.argv = cmd_args[:]
            local_parser = GpRecoverSegmentProgram.createParser()
            local_options, args = local_parser.parse_args()
            recover_cmd = GpRecoverSegmentProgram.createProgram(local_options, args)
            try:
                recover_cmd.run()
            except SystemExit as e:
                if e.code != 0:
                    self.logger.error("Failed to start the synchronization step of the segment rebalance.")
                    self.logger.error("Check the gprecoverseg log file, correct any problems, and re-run")
                    self.logger.error(' '.join(cmd_args))
                    raise Exception("Error synchronizing.\nError: %s" % str(e))
            finally:
                if recover_cmd:
                    recover_cmd.cleanup()
                sys.argv = original_sys_args
                self.logger.info("==============================END ANOTHER RECOVER==========================================")

        except Exception as ex:
            raise ex
        finally:
            pool.join()
            pool.haltWork()
            pool.joinWorkers()
            signal.signal(signal.SIGINT, signal.default_int_handler)

        return allSegmentsStopped # if all segments stopped, then a full rebalance was done

