import sys
import signal
from contextlib import closing
from gppylib.gparray import GpArray
from gppylib.db import dbconn
from gppylib.commands.gp import GpSegStopCmd
from gppylib.commands import base
from gppylib import gplog

from gppylib.operations.segment_reconfigurer import SegmentReconfigurer

MIRROR_PROMOTION_TIMEOUT = 600

logger = gplog.get_default_logger()


def replay_lag(primary_db):
    """
    This function returns replay lag (diff of flush_lsn and replay_lsn) on mirror segment. Goal being if there is a
    lot to catchup on mirror the user should be warned about that and rebalance opertion should be aborted.
    params: primary segment info
    return value: replay lag in bytes
    replay lag in bytes: diff of flush_lsn and replay_lsn on mirror
    """
    port = primary_db.getSegmentPort()
    host = primary_db.getSegmentHostName()
    logger.debug('Get replay lag on mirror of primary segment with host:{}, port:{}'.format(host, port))
    sql = "select pg_xlog_location_diff(flush_location, replay_location) from pg_stat_replication;"

    try:
        dburl = dbconn.DbURL(hostname=host, port=port)
        with closing(dbconn.connect(dburl, utility=True, encoding='UTF8')) as conn:
            replay_lag = dbconn.execSQLForSingleton(conn, sql)
    except Exception as ex:
        raise Exception("Failed to query pg_stat_replication for host:{}, port:{}, error: {}".
                        format(host, port, str(ex)))
    return replay_lag


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
    def __init__(self, gpEnv, gpArray, batch_size, segment_batch_size, replay_lag):
        self.gpEnv = gpEnv
        self.gpArray = gpArray
        self.batch_size = batch_size
        self.segment_batch_size = segment_batch_size
        self.replay_lag = replay_lag
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
                if self.replay_lag is not None:
                    self.logger.info("Allowed replay lag during rebalance is {} GB".format(self.replay_lag))
                    replay_lag_in_bytes = replay_lag(segmentPair.primaryDB)
                    if float(replay_lag_in_bytes) >= (self.replay_lag * 1024 * 1024 * 1024):
                        raise Exception("{} bytes of xlog is still to be replayed on mirror with dbid {}, let "
                                        "mirror catchup on replay then trigger rebalance. Use --replay-lag to "
                                        "configure the allowed replay lag limit."
                                        .format(replay_lag_in_bytes, segmentPair.primaryDB.getSegmentDbId()))
                unbalanced_primary_segs.append(segmentPair.primaryDB)
            else:
                self.logger.warning(
                    "Not rebalancing primary segment dbid %d with its mirror dbid %d because one is either down, "
                    "unreachable, or not synchronized" \
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
            for hostname in unbalanced_primary_segs.keys():
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
                self.logger.warn("%d segments failed to stop.  A full rebalance of the")
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

        except Exception, ex:
            raise ex
        finally:
            pool.join()
            pool.haltWork()
            pool.joinWorkers()
            signal.signal(signal.SIGINT, signal.default_int_handler)

        return allSegmentsStopped # if all segments stopped, then a full rebalance was done

