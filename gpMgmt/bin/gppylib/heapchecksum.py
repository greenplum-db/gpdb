
from gppylib.commands.base import REMOTE, WorkerPool
from gppylib.commands.pg import PgControlData


class HeapChecksum:
    """
    check whether heap checksum is the same between master and all segments
    """

    def __init__(self, gparray, num_workers=8, logger=None):
        if num_workers <= 0:
            raise Exception("number of workers must be greater than 0")
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

    def get_standby_value(self):
        """
        can raise
        :return: the heap checksum setting (1 or 0) for the standby master
        """
        standbyMaster_gpdb = self.gparray.standbyMaster
        cmd = PgControlData(name='run pg_controldata', datadir=standbyMaster_gpdb.getSegmentDataDirectory(),
                            ctxt=REMOTE, remoteHost=standbyMaster_gpdb.getSegmentHostName())
        cmd.run(validateAfter=True)
        value = cmd.get_value('Data page checksum version')
        return value

    def get_segments_checksum_settings(self, gpdb_list=None):
        """
        :return: two lists of Segment objects (as defined in gparray) for successes and failures,
        where successes have successfully reported their checksum values
        """
        failures = []
        successes = []
        if not gpdb_list:
            gpdb_list = []
            for segment in self.gparray.getSegmentList():
                gpdb_list.extend(segment.get_dbs())

        completed = self._get_pgcontrol_data_from_segments(gpdb_list)
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
        :param successes: list of segments (Segment objects) for which we successfully determined checksum setting
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

    @staticmethod
    def are_segments_consistent(consistent, inconsistent):
        """
        :return: True if there is at least 1 segment that agrees with master and no segments that disagree
        """
        return len(consistent) >= 1 and len(inconsistent) == 0

    # private methods #######################################
    def _get_pgcontrol_data_from_segments(self, gpdb_list):
        pool = WorkerPool(numWorkers=self.workers)
        try:
            for gpdb in gpdb_list:  # iterate for all segments
                cmd = PgControlData(name='run pg_controldata', datadir=gpdb.getSegmentDataDirectory(),
                                    ctxt=REMOTE, remoteHost=gpdb.getSegmentHostName())
                cmd.gparray_gpdb = gpdb
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
