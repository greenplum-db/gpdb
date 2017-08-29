import sys
import signal
from gppylib.gparray import GpArray
from gppylib.db import dbconn
from gppylib.commands.gp import GpSegStopCmd, GpRecoverseg
from gppylib.commands import base
from gppylib import gplog


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
    def __init__(self, gpEnv, gpArray):
        self.gpEnv = gpEnv
        self.gpArray = gpArray
        self.logger = gplog.get_default_logger()

    def rebalance(self):
        # Get the unbalanced primary segments grouped by hostname
        # These segments are what we will shutdown.
        self.logger.info("Getting unbalanced segments")
        unbalanced_primary_segs = GpArray.getSegmentsByHostName(self.gpArray.get_unbalanced_primary_segdbs())
        pool = base.WorkerPool()
        count = 0

        try:
            # Disable ctrl-c
            signal.signal(signal.SIGINT, signal.SIG_IGN)

            self.logger.info("Stopping unbalanced primary segments...")
            for hostname in unbalanced_primary_segs.keys():
                cmd = GpSegStopCmd("stop unbalanced primary segs",
                                   self.gpEnv.getGpHome(),
                                   self.gpEnv.getGpVersion(),
                                   'fast',
                                   unbalanced_primary_segs[hostname],
                                   ctxt=base.REMOTE,
                                   remoteHost=hostname,
                                   timeout=600)
                pool.addCommand(cmd)
                count += 1

            pool.wait_and_printdots(count, False)
            
            failed_count = 0
            completed = pool.getCompletedItems()
            for res in completed:
                if not res.get_results().wasSuccessful():
                    failed_count += 1

            allSegmentsStopped = (failed_count == 0)

            if not allSegmentsStopped:
                self.logger.warn("%d segments failed to stop.  A full rebalance of the")
                self.logger.warn("system is not possible at this time.  Please check the")
                self.logger.warn("log files, correct the problem, and run gprecoverseg -r")
                self.logger.warn("again.")
                self.logger.info("gprecoverseg will continue with a partial rebalance.")

            pool.empty_completed_items()
            # issue a distributed query to make sure we pick up the fault
            # that we just caused by shutting down segments
            conn = None
            try:
                self.logger.info("Triggering segment reconfiguration")
                dburl = dbconn.DbURL()
                conn = dbconn.connect(dburl)
                cmd = ReconfigDetectionSQLQueryCommand(conn)
                pool.addCommand(cmd)
                pool.wait_and_printdots(1, False)
            except Exception:
                # This exception is expected
                pass
            finally:
                if conn:
                    conn.close()

            # Final step is to issue a recoverseg operation to resync segments
            self.logger.info("Starting segment synchronization")
            original_sys_args = sys.argv[:]
            try:
                self.logger.info("=============================START ANOTHER RECOVER=========================================")
                # import here because GpRecoverSegmentProgram and GpSegmentRebalanceOperation have a circular dependency
                from gppylib.programs.clsRecoverSegment import GpRecoverSegmentProgram
                sys.argv = ['gprecoverseg', '-a']
                local_parser = GpRecoverSegmentProgram.createParser()
                local_options, args = local_parser.parse_args()
                cmd = GpRecoverSegmentProgram.createProgram(local_options, args)
                cmd.run()

            except SystemExit as e:
                if e.code != 0:
                    self.logger.error("Failed to start the synchronization step of the segment rebalance.")
                    self.logger.error("Check the gprecoverseg log file, correct any problems, and re-run")
                    self.logger.error("'gprecoverseg -a'.")
                    raise Exception("Error synchronizing.\nError: %s" % str(e))
            finally:
                if cmd:
                    cmd.cleanup()
                sys.argv = original_sys_args
                self.logger.info("==============================END ANOTHER RECOVER==========================================")

        except Exception, ex:
            raise ex
        finally:
            pool.join()
            pool.haltWork()
            pool.joinWorkers()
            signal.signal(signal.SIGINT, signal.default_int_handler)

        return allSegmentsStopped # if all segments stopped, then a full rebalance was done

