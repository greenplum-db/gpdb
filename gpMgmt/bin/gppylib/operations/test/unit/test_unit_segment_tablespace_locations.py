#!/usr/bin/env python3


from mock import Mock, patch
from gppylib.operations.segment_tablespace_locations import get_tablespace_locations
from test.unit.gp_unittest import GpTestCase

class GetTablespaceDirTestCase(GpTestCase):

    def setUp(self):
        self.mock_query = Mock(return_value=[])
        self.apply_patches([
            patch('gppylib.operations.segment_tablespace_locations.dbconn.connect'),
            patch('gppylib.operations.segment_tablespace_locations.dbconn.query', self.mock_query)
        ])

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
