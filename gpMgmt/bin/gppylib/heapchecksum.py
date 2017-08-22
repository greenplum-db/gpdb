
from gppylib.commands.base import REMOTE, WorkerPool
from gppylib.commands.pg import PgControlData


class HeapChecksum:
    """
    check whether heap checksum is the same between master and all segments
    """

    def __init__(self, gparray, num_workers=8, logger=None):
        self.gparray = gparray
        self.workers = num_workers
        self.logger = logger

    def get_master_value(self):
        """
        can raise
        :return: the heap checksum setting (1 or 0) for the master
        """
        master_gpdb = self.gparray.master
        cmd = PgControlData(name='run pg_controldata', datadir=master_gpdb.getSegmentDataDirectory())
        cmd.run(validateAfter=True)
        value = cmd.get_value('Data page checksum version')
        return value

    def get_segments_checksum_settings(self):
        """
        :return: two lists of GpDB objects (as defined in gparray) for successes and failures,
        where successes have successfully reported their checksum values
        """
        failures = []
        successes = []
        completed = self._get_pgcontrol_data_from_segments()
        for pg_control_data in completed:
            result = pg_control_data.get_results()
            gparray_gpdb = pg_control_data.gparray_gpdb
            if result.rc == 0:
                try:
                    checksum = pg_control_data.get_value('Data page checksum version')
                    gparray_gpdb.heap_checksum = checksum
                    successes.append(gparray_gpdb)
                except Exception as e:
                    failures.append(gparray_gpdb)
                    self._logger_warn("cannot access heap checksum from pg_controldata for dbid %s "
                                      "on host %s; got pgcontrol_data: %s and exception %s" % (
                                          gparray_gpdb.dbid, gparray_gpdb.getSegmentHostName(),
                                          pg_control_data, str(e)))
            else:
                failures.append(gparray_gpdb)
                self._logger_warn("cannot access pg_controldata for dbid %s on host %s" % (
                    gparray_gpdb.dbid, gparray_gpdb.getSegmentHostName()))

        return successes, failures

    def check_segment_consistency(self, successes):
        """
        :param successes: list of segments (GpDB objects) for which we successfully determined checksum setting
        :return: lists of segments that are consistent with master and inconsistent with master, respectively,
        along with master GpDb itself
        """
        master = self.get_master_value()
        consistent = []
        inconsistent = []

        for gparray_gpdb in successes:
            checksum = gparray_gpdb.heap_checksum
            if checksum == master:
                consistent.append(gparray_gpdb)
            else:
                inconsistent.append(gparray_gpdb)

        return consistent, inconsistent, master

    def are_segments_consistent(self, consistent, inconsistent):
        """
        :return: True if there is at least 1 segment that agrees with master and no segments that disagree
        """
        return len(consistent) >= 1 and len(inconsistent) == 0

    # private methods #######################################
    def _get_pgcontrol_data_from_segments(self):
        pool = WorkerPool(numWorkers=self.workers)
        try:
            for seg in self.gparray.getSegmentList():  # iterate for all segments
                for gparray_gpdb in seg.get_dbs():  # iterate for both primary and mirror
                    cmd = PgControlData(name='run pg_controldata', datadir=gparray_gpdb.getSegmentDataDirectory(),
                                        ctxt=REMOTE, remoteHost=gparray_gpdb.getSegmentHostName())
                    cmd.gparray_gpdb = gparray_gpdb
                    pool.addCommand(cmd)
            pool.join()
        finally:
            # Make sure that we halt the workers or else we'll hang
            pool.haltWork()
            pool.joinWorkers()
        return pool.getCompletedItems()

    def _logger_warn(self, message):
        """
        logger: log only if a logger is provided
        """
        if self.logger:
            self.logger.warn(message)
