from mock import Mock, patch

from gparray import Segment, GpArray
from gppylib.commands.base import CommandResult
from gppylib.heapchecksum import HeapChecksum, REMOTE
from gppylib.test.unit.gp_unittest import GpTestCase


class GpHeapChecksumTestCase(GpTestCase):
    def setUp(self):
        self.gparray = self.createGpArrayWith2Primary2Mirrors()
        self.subject = HeapChecksum(self.gparray)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.COMMAND_RESULT = """
pg_control version number:            8330500
Catalog version number:               301705051
Database system identifier:           6450200148836071422
Database cluster state:               in production
pg_control last modified:             Thu Aug  3 16:56:40 2017
Latest checkpoint location:           0/4A7CE98
Prior checkpoint location:            0/4A7C908
Latest checkpoint's REDO location:    0/4A7CE98
Latest checkpoint's TimeLineID:       1
Latest checkpoint's NextXID:          0/704
Latest checkpoint's NextOID:          12088
Latest checkpoint's NextRelfilenode:  16384
Latest checkpoint's NextMultiXactId:  1
Latest checkpoint's NextMultiOffset:  0
Time of latest checkpoint:            Thu Aug  3 16:56:39 2017
Minimum recovery ending location:     0/0
Backup start location:                0/0
End-of-backup record required:        no
Maximum data alignment:               8
Database block size:                  32768
Blocks per segment of large relation: 32768
WAL block size:                       32768
Bytes per WAL segment:                67108864
Maximum length of identifiers:        64
Maximum columns in an index:          32
Maximum size of a TOAST chunk:        8140
Date/time type storage:               64-bit integers
Maximum length of locale name:        128
LC_COLLATE:                           en_US.utf-8
LC_CTYPE:                             en_US.utf-8
Data page checksum version:           1
"""

        self.apply_patches([
            patch('gppylib.heapchecksum.WorkerPool'),
            patch('gppylib.heapchecksum.PgControlData.run'),
            patch('gppylib.heapchecksum.PgControlData.get_value'),
        ])
        self.mock_workerpool = self.get_mock_from_apply_patch('WorkerPool')
        self.mock_pg_control_data_get_value = self.get_mock_from_apply_patch('get_value')
        self.mock_pg_control_data_get_value.return_value = '1'  # master checksum value

        self.data_dirs = ['/master', '/seg0', '/seg1', '/seg2']
        # rc,stdout,stderr,completed,halt
        self.results = [CommandResult(0, self.COMMAND_RESULT, '', False, True),
                        CommandResult(0, self.COMMAND_RESULT, '', False, True),
                        CommandResult(0, self.COMMAND_RESULT, '', False, True),
                        CommandResult(0, self.COMMAND_RESULT, '', False, True)]
        self.assertEquals(len(self.data_dirs), len(self.results))

    def tearDown(self):
        super(GpHeapChecksumTestCase, self).tearDown()

    def createGpArrayWith2Primary2Mirrors(self):
        master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|aspen|sdw1|40000|41000|/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/qddir/demoDataDir-1||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")
        standby_master = Segment.initFromString(
            "6|-1|m|m|s|u|mdw_standby|mdw_standby|25432|None|/data/standby_master||/data/standby_master/base/10899,/data/standby_master/base/1,/data/standby_master/base/10898,/data/standby_master/base/25780,/data/standby_master/base/34782")
        return GpArray([master, self.primary0, primary1, mirror0, mirror1, standby_master])

    def test_consistent_heap_checksum_returns_true(self):
        get_values = ['1', '1', '1', '1']
        self.setup_worker_pool(get_values)

        successes, failures = self.subject.get_segments_checksum_settings()
        consistent, inconsistent, master_checksum = self.subject.check_segment_consistency(successes)
        self.assertEquals(self.subject.are_segments_consistent(consistent, inconsistent), True)

    def test_inconsistent_heap_checksum_returns_false(self):
        get_values = ['0', '1', '1', '1']
        self.setup_worker_pool(get_values)

        successes, failures = self.subject.get_segments_checksum_settings()
        consistent, inconsistent, master_checksum = self.subject.check_segment_consistency(successes)
        self.assertEquals(self.subject.are_segments_consistent(consistent, inconsistent), False)

    def test_pg_control_data_raises(self):
        get_values = ['1', '1', '1', Exception("hi")]
        mocks = [Mock() for _ in range(len(self.results))]

        for i, mock in enumerate(mocks):
            mock.get_results.side_effect = [self.results[i]]
            mock.datadir = self.data_dirs[i]
            mock.get_value.side_effect = [get_values[i]]
        self.mock_workerpool.return_value.getCompletedItems.return_value = mocks

        successes, failures = self.subject.get_segments_checksum_settings()
        consistent, inconsistent, master_checksum = self.subject.check_segment_consistency(successes)
        self.assertEquals(self.subject.are_segments_consistent(consistent, inconsistent), True)

    def test_pg_control_data_raises_every_segment(self):
        get_values = [Exception("hi"), Exception("hi"), Exception("hi"), Exception("hi")]
        mocks = [Mock() for _ in range(len(self.results))]

        for i, mock in enumerate(mocks):
            mock.get_results.side_effect = [self.results[i]]
            mock.datadir = self.data_dirs[i]
            mock.get_value.side_effect = [get_values[i]]
        self.mock_workerpool.return_value.getCompletedItems.return_value = mocks

        successes, failures = self.subject.get_segments_checksum_settings()
        consistent, inconsistent, master_checksum = self.subject.check_segment_consistency(successes)
        self.assertEquals(self.subject.are_segments_consistent(consistent, inconsistent), False)

    def test_are_segments_consistent_zero_consistent_zero_inconsistent(self):
        self.assertFalse(self.subject.are_segments_consistent([], []))

    def test_are_segments_consistent_zero_consistent_one_inconsistent(self):
        self.assertFalse(self.subject.are_segments_consistent([], [1]))

    def test_are_segments_consistent_one_consistent_zero_inconsistent(self):
        self.assertTrue(self.subject.are_segments_consistent([1], []))

    def test_are_segments_consistent_one_consistent_one_inconsistent(self):
        self.assertFalse(self.subject.are_segments_consistent([1], [1]))

    def test_get_segments_checksum_settings_accepts_array(self):
        get_values = ['1']
        self.results = [CommandResult(0, self.COMMAND_RESULT, '', False, True)]
        self.setup_worker_pool(get_values)

        successes, failures = self.subject.get_segments_checksum_settings([self.primary0])

        self.assertEquals(len(successes), 1)
        self.assertEquals(len(failures), 0)

    @patch('gppylib.heapchecksum.PgControlData')
    def test_standby_master_context_is_remote(self, mock_pg_control_data):
        #  the standby can either be remote or local, depending on the user setup.
        self.subject.get_standby_value()
        self.assertEquals(len(mock_pg_control_data.call_args_list), 1)
        call_args_dict = mock_pg_control_data.call_args_list[0][1]
        try:
            self.assertEquals(call_args_dict['ctxt'], REMOTE)
            self.assertEquals(call_args_dict['remoteHost'], 'mdw_standby')
        except KeyError as e:
            raise Exception("Argument is missing from the call argument %s " % str(e))

    def test_get_segments_checksum_settings_uses_gparray(self):
        get_values = ['1', '1', '1', '1']
        self.setup_worker_pool(get_values)

        successes, failures = self.subject.get_segments_checksum_settings()

        self.assertEquals(len(successes), 4)
        self.assertEquals(len(failures), 0)

    def test_0_workers_in_WorkerPool_raises(self):
        with self.assertRaises(Exception):
            HeapChecksum(None, 0)

    # ****************** tools ************************
    def setup_worker_pool(self, values):
        mocks = [Mock() for _ in range(len(self.results))]
        for i, mock in enumerate(mocks):
            mock.get_results.return_value = self.results[i]
            mock.datadir = self.data_dirs[i]
            mock.get_value.return_value = values[i]
        self.mock_workerpool.return_value.getCompletedItems.return_value = mocks
