from mock import Mock, patch, call
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests, FakeCursor
from gppylib.operations.get_segments_in_recovery import is_seg_in_backup_mode


class GetSegmentInrecoveryTestCase(GpTestCase):
    def setUp(self):
        mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])
        self.apply_patches([
            patch('gppylib.operations.get_segments_in_recovery.logger', return_value=mock_logger),
        ])

        self.mock_logger = self.get_mock_from_apply_patch('logger')

    def tearDown(self):
        super(GetSegmentInrecoveryTestCase, self).tearDown()

    @patch('gppylib.db.dbconn.connect', side_effect=Exception())
    def test_is_seg_in_backup_mode_conn_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            is_seg_in_backup_mode("sdw1", 6001)
        self.assertEqual('Failed to query pg_is_in_backup() for segment with hostname sdw1, port 6001, error: ',
                         str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.execSQL', side_effect=Exception())
    def test_is_seg_in_bakup_mode_query_exception(self, mock1, mock2):
        with self.assertRaises(Exception) as ex:
            is_seg_in_backup_mode("sdw1", 6001)
        self.assertEqual('Failed to query pg_is_in_backup() for segment with hostname sdw1, port 6001, error: ',
                         str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.execSQL', return_value=FakeCursor(my_list=[[True]]))
    def test_is_seg_in_backup_mode_returns_true(self, mock1, mock2):
        backup_in_progress = is_seg_in_backup_mode("sdw1", 6001)
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call("Checking if backup is already in progress for the source server with host sdw1 and "
                               "port 6001")], self.mock_logger.debug.call_args_list)

        self.assertEqual(backup_in_progress, True)

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.execSQL', return_value=FakeCursor(my_list=[[False]]))
    def test_is_seg_in_backup_mode_returns_false(self, mock1, mock2):
        backup_in_progress = is_seg_in_backup_mode("sdw1", 6001)
        self.assertEqual(1, self.mock_logger.debug.call_count)
        self.assertEqual([call("Checking if backup is already in progress for the source server with host sdw1 and "
                               "port 6001")], self.mock_logger.debug.call_args_list)
        self.assertEqual(backup_in_progress, False)


if __name__ == '__main__':
    run_tests()
