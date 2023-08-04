#!/usr/bin/env python3

from gppylib.operations.validate_disk_space import RelocateSegmentPair, RelocateDiskUsage, FileSystem
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests
from gppylib.mainUtils import UserAbortedException
from mock import Mock, patch, call, MagicMock


class RelocateSegmentPairTestCase(GpTestCase):
    def setUp(self):

        class Options:
            def __init__(self):
                self.interactive = False

        self.pairs = [RelocateSegmentPair("sdw1", "/data/seg1", "tdw1","/tmp/seg1"),
                      RelocateSegmentPair("sdw2", "/data/seg2", "tdw2", "/tmp/seg2")]

        self.rel_disk_usage = RelocateDiskUsage(self.pairs, 16, Options())
        mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.apply_patches([
            patch('gppylib.operations.validate_disk_space.logger', return_value=mock_logger),
        ])
        self.mock_logger = self.get_mock_from_apply_patch('logger')

    @patch('gppylib.operations.validate_disk_space.RelocateDiskUsage._determine_source_disk_usage', return_value=None)
    def test_validate_disk_space(self, mock1):
        RelocateDiskUsage._target_host_filesystems = MagicMock()
        fs = {}
        fs['tdw1'] = [FileSystem('/tmp/disk1', disk_free=int(1124), disk_required=int(1024), directories=('seg1', 'tbl1'))]
        fs['tdw2'] = [FileSystem('/tmp/disk2', disk_free=int(1124), disk_required=int(1024), directories=('seg2', 'tbl2'))]
        mock1._target_host_filesystems.return_value = fs

        result = self.rel_disk_usage.validate_disk_space()
        self.assertEqual(True, result)

    @patch('gppylib.operations.validate_disk_space.RelocateDiskUsage._determine_source_disk_usage', return_value=None)
    def test_validate_disk_space_fail(self, mock1):
        RelocateDiskUsage._target_host_filesystems = MagicMock()
        fs = {}
        fs['tdw1'] = [FileSystem('/tmp/disk1', disk_free=int(1024), disk_required=int(1024), directories=('seg1', 'tbl1'))]
        fs['tdw2'] = [FileSystem('/tmp/disk2', disk_free=int(1024), disk_required=int(1024), directories=('seg2', 'tbl2'))]
        RelocateDiskUsage._target_host_filesystems.return_value = fs

        self.rel_disk_usage.validate_disk_space()
        self.mock_logger.error.assert_has_calls([call('Not enough space on host tdw1 for directories seg1, tbl1.'),
                                                 call('Filesystem /tmp/disk1 has 1024 kB available, but requires 1024 kB.')],
                                                 self.mock_logger.error.call_args_list)


    @patch('gppylib.operations.validate_disk_space.ask_yesno', return_value=True)
    @patch('gppylib.operations.validate_disk_space.RelocateDiskUsage._determine_source_disk_usage', return_value=None)
    def test_validate_disk_space_less_than_10per(self, mock1, mock2):
        RelocateDiskUsage._target_host_filesystems = MagicMock()
        fs = {}
        fs['tdw1'] = [FileSystem('/tmp/disk1', disk_free=int(1025), disk_required=int(1024), directories=('seg1', 'tbl1'))]
        fs['tdw2'] = [FileSystem('/tmp/disk2', disk_free=int(1025), disk_required=int(1024), directories=('seg2', 'tbl2'))]
        RelocateDiskUsage._target_host_filesystems.return_value = fs

        self.rel_disk_usage.options.interactive = True
        result = self.rel_disk_usage.validate_disk_space()

        self.mock_logger.info.assert_has_calls([call(
            'Target Host:tdw1 Filesystem:/tmp/disk1 (directory seg1, tbl1) has 1025 kB free disk space and required '
            'is 1024 kB.')], self.mock_logger.info.call_args_list)
        self.assertEqual(True, result)

    @patch('gppylib.operations.validate_disk_space.RelocateDiskUsage._target_host_filesystems')
    @patch('gppylib.operations.validate_disk_space.ask_yesno', return_value=False)
    @patch('gppylib.operations.validate_disk_space.RelocateDiskUsage._determine_source_disk_usage', return_value=None)
    def test_validate_disk_space_less_than_10per_prompt(self, mock1, mock2, mock3):
        fs = {}
        fs['tdw1'] = [FileSystem('/tmp/disk1', disk_free=int(1025), disk_required=int(1024), directories=('seg1', 'tbl1'))]
        fs['tdw2'] = [FileSystem('/tmp/disk2', disk_free=int(1025), disk_required=int(1024), directories=('seg2', 'tbl2'))]
        mock3.return_value = fs

        self.rel_disk_usage.options.interactive = True
        with self.assertRaises(UserAbortedException):
            self.rel_disk_usage.validate_disk_space()

        self.mock_logger.info.assert_has_calls([call('Target Host:tdw1 Filesystem:/tmp/disk1 (directory seg1, tbl1) '
                                                     'has 1025 kB free disk space and required is 1024 kB.'),
                                                call('User Aborted. Exiting...')], self.mock_logger.info.call_args_list)



    @patch('gppylib.operations.validate_disk_space.RelocateDiskUsage._disk_usage', return_value={'/data/seg1':1024, '/data/seg2':186})
    @patch('gppylib.operations.validate_disk_space.get_tablespace_locations', return_value=())
    def test_determine_source_disk_usage_without_tblspace(self, mock1, mock2):
        self.rel_disk_usage._determine_source_disk_usage()
        self.assertEqual(self.pairs[0].source_data_dir_usage, 1024)
        self.assertEqual(self.pairs[1].source_data_dir_usage, 186)

    @patch('gppylib.operations.validate_disk_space.RelocateDiskUsage._disk_usage', side_effect=[ {'/data/seg1': 1024},
                                                                                                {'/data/tbl1': 480},
                                                                                                {'/data/seg2': 186},
                                                                                                {'/data/tbl2' : 1024}])
    @patch('gppylib.operations.validate_disk_space.get_tablespace_locations', side_effect=[[('sdw1', 1, '/data/tbl1')],
                                                                                           [('sdw2', 1, '/data/tbl2')]])
    def test_determine_source_disk_usage_with_tblspace(self, mock1, mock2):
        self.rel_disk_usage._determine_source_disk_usage()
        self.assertEqual(self.pairs[0].source_data_dir_usage, 1024)
        self.assertEqual(self.pairs[0].source_tablespace_usage, {'/data/tbl1': 480})
        self.assertEqual(self.pairs[1].source_data_dir_usage, 186)
        self.assertEqual(self.pairs[1].source_tablespace_usage, {'/data/tbl2': 1024})

    @patch('gppylib.operations.validate_disk_space.DiskUsage')
    @patch('gppylib.operations.validate_disk_space.WorkerPool')
    def test_disk_usage(self, mock1, mock2):
        cmd = Mock()
        cmd.was_successful.return_value = True
        cmd.directory = '/data/seg1'
        cmd.kbytes_used.return_value = 1024

        pool = Mock()
        pool.getCompletedItems.return_value = [ cmd ]
        mock1.return_value = pool

        result = self.rel_disk_usage._disk_usage('sdw1', ['/data/seg1'])
        self.assertEqual(result, {'/data/seg1': 1024})

    @patch('gppylib.operations.validate_disk_space.DiskUsage')
    @patch('gppylib.operations.validate_disk_space.WorkerPool')
    def test_disk_usage_fail(self, mock1, mock2):
        cmd = Mock()
        cmd.was_successful.return_value = False
        cmd.get_results().stderr = 'Generic Error'

        pool = Mock()
        pool.getCompletedItems.return_value = [cmd]
        mock1.return_value = pool

        with self.assertRaises(Exception) as ex:
            self.rel_disk_usage._disk_usage('sdw1', ['/data/seg1'])

        self.assertTrue('Unable to check disk usage on source segment: Generic Error' in str(ex.exception))

    def test_target_host_filesystems(self):
        RelocateDiskUsage._target_filesystems = MagicMock()
        fs1 = FileSystem('/tmp/disk1', disk_free=int(1025), disk_required=int(1024), directories=('/data/seg1', '/data/tbl1'))
        fs2 = FileSystem('/tmp/disk2', disk_free=int(1025), disk_required=int(1024), directories=('/data/seg2', '/data/tbl2'))
        RelocateDiskUsage._target_filesystems.side_effect = [[fs1], [fs2]]

        res = self.rel_disk_usage._target_host_filesystems()

        res1 = res[self.pairs[0].target_hostaddr]
        self.assertIsInstance(res1[0], FileSystem)
        res2 = res[self.pairs[1].target_hostaddr]
        self.assertIsInstance(res2[0], FileSystem)

    @patch('gppylib.operations.validate_disk_space.base64')
    @patch('gppylib.operations.validate_disk_space.pickle')
    @patch('gppylib.operations.validate_disk_space.DiskFree')
    @patch('gppylib.operations.validate_disk_space.WorkerPool')
    def test_target_filesystem(self, mock1, mock2, mock3, mock4):
        cmd = Mock()
        cmd.was_successful.return_value = True
        cmd.directory = '/data/seg1'
        cmd.kbytes_used.return_value = 1024

        pool = Mock()
        pool.getCompletedItems.return_value = [cmd]
        mock1.return_value = pool

        fs1 = FileSystem('/tmp/disk1', disk_free=int(1025), directories=['/tmp/seg1'])
        fs2 = FileSystem('/tmp/disk1', disk_free=int(1025), directories=['/data/tbl1'])
        fs3 = FileSystem('/tmp/disk2', disk_free=int(1025), directories=['/tmp/seg2'])
        fs4 = FileSystem('/tmp/disk2', disk_free=int(1025), directories=['/data/tbl2'])
        mock3.loads.side_effect =[  [fs1, fs2] , [fs3, fs4] ]

        self.pairs[0].source_data_dir_usage = 1024
        self.pairs[1].source_data_dir_usage = 184
        self.pairs[0].source_tablespace_usage = {'/data/tbl1': 425}
        self.pairs[1].source_tablespace_usage = {'/data/tbl2': 125}

        fs_list1 = self.rel_disk_usage._target_filesystems(self.pairs[0])
        self.assertEqual(fs_list1[0].name, fs1.name)
        self.assertEqual(fs_list1[0].disk_free, fs1.disk_free)
        self.assertEqual(fs_list1[0].disk_required, self.pairs[0].source_data_dir_usage)
        self.assertEqual(fs_list1[0].directories, fs1.directories)

        self.assertEqual(fs_list1[1].name, fs2.name)
        self.assertEqual(fs_list1[1].disk_free, fs2.disk_free)
        self.assertEqual(fs_list1[1].disk_required, self.pairs[0].source_tablespace_usage['/data/tbl1'])
        self.assertEqual(fs_list1[1].directories, fs2.directories)

        fs_list2 = self.rel_disk_usage._target_filesystems(self.pairs[1])
        self.assertEqual(fs_list2[0].name, fs3.name)
        self.assertEqual(fs_list2[0].disk_free, fs3.disk_free)
        self.assertEqual(fs_list2[0].disk_required, self.pairs[1].source_data_dir_usage)
        self.assertEqual(fs_list2[0].directories, fs3.directories)

        self.assertEqual(fs_list2[1].name, fs4.name)
        self.assertEqual(fs_list2[1].disk_free, fs4.disk_free)
        self.assertEqual(fs_list2[1].disk_required, self.pairs[1].source_tablespace_usage['/data/tbl2'])
        self.assertEqual(fs_list2[1].directories, fs4.directories)

    def tearDown(self):
        super(RelocateSegmentPairTestCase, self).tearDown()

