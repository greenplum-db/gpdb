#!/usr/bin/env python

from mock import Mock, patch, call
from gppylib.operations.segment_tablespace_locations import get_tablespace_locations, get_segment_tablespace_locations
from test.unit.gp_unittest import GpTestCase

class GetTablespaceDirTestCase(GpTestCase):

    def setUp(self):
        self.mock_query = Mock(return_value=[])
        mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.apply_patches([
            patch('gppylib.operations.segment_tablespace_locations.dbconn.connect'),
            patch('gppylib.operations.segment_tablespace_locations.dbconn.execSQL', self.mock_query),
            patch('gppylib.operations.segment_tablespace_locations.logger', return_value=mock_logger),
        ])
        self.mock_logger = self.get_mock_from_apply_patch('logger')

    def tearDown(self):
        super(GetTablespaceDirTestCase, self).tearDown()

    def test_validate_empty_with_all_hosts_get_tablespace_locations(self):
        self.assertEqual([], get_tablespace_locations(True, None))

    def test_validate_empty_with_no_all_hosts_get_tablespace_locations(self):
        mirror_data_directory = '/data/primary/seg1'
        self.assertEqual([], get_tablespace_locations(False, mirror_data_directory))

    def test_validate_data_with_all_hosts_get_tablespace_locations(self):
        self.mock_query.side_effect =[[('sdw1', '/data/tblsp1')]]
        expected = [('sdw1', '/data/tblsp1')]
        self.assertEqual(expected, get_tablespace_locations(True, None))

    def test_validate_data_with_mirror_data_directory_get_tablespace_locations(self):
        self.mock_query.side_effect = [[('sdw1', 1, '/data/tblsp1')]]
        expected = [('sdw1', 1, '/data/tblsp1')]
        mirror_data_directory = '/data/tblsp1'
        self.assertEqual(expected, get_tablespace_locations(False, mirror_data_directory))

    @patch('gppylib.db.catalog.RemoteQueryCommand.run', side_effect=Exception())
    def test_get_segment_tablespace_locations_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            get_segment_tablespace_locations('sdw1', 40000)
        self.assertEqual(0, self.mock_logger.debug.call_count)
        self.assertTrue('Failed to get segment tablespace locations for segment with host sdw1 and port 40000'
                        in str(ex.exception))

    @patch('gppylib.db.catalog.RemoteQueryCommand.__init__', return_value=None)
    @patch('gppylib.db.catalog.RemoteQueryCommand.run')
    @patch('gppylib.db.catalog.RemoteQueryCommand.get_results')
    def test_get_segment_tablespace_locations_success(self, mock1, mock2, mock3):
        get_segment_tablespace_locations('sdw1', 40000)
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call('Successfully got tablespace locations for segment with host sdw1, port 40000')],
                         self.mock_logger.debug.call_args_list)
