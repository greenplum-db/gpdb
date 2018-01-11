import imp
import logging
import os
import sys

from mock import Mock, patch

from gparray import Segment, GpArray
from gppylib.operations.startSegments import StartSegmentsResult
from gppylib.test.unit.gp_unittest import GpTestCase, run_tests


class GpStart(GpTestCase):
    def setUp(self):
        # because gpstart does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpstart.py, this is equivalent to:
        #   import gpstart
        #   self.subject = gpstart
        gpstart_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpstart")
        self.subject = imp.load_source('gpstart', gpstart_file)
        self.subject.logger = Mock(
            spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal', 'warning_to_file_only'])

        self.os_environ = dict(MASTER_DATA_DIRECTORY='/tmp/mdd', GPHOME='/tmp/gphome', GP_MGMT_PROCESS_COUNT=1,
                               LANGUAGE=None)
        self.gparray = self._createGpArrayWith2Primary2Mirrors()
        self.segments_by_content_id = GpArray.getSegmentsByContentId(self.gparray.getSegDbList())

        start_result = StartSegmentsResult()
        start_result.addSuccess(self.primary0)

        self.apply_patches([
            patch('os.getenv', side_effect=self._get_env),
            patch('gpstart.os.path.exists'),
            patch('gpstart.gp'),
            patch('gpstart.pgconf'),
            patch('gpstart.unix'),
            patch('gpstart.dbconn.DbURL'),
            patch('gpstart.dbconn.connect'),
            patch('gpstart.GpArray.initFromCatalog', return_value=self.gparray),
            patch('gpstart.GpArray.getSegmentsByContentId', return_value=self.segments_by_content_id),
            patch('gpstart.GpArray.getSegmentsGroupedByValue',
                  side_effect=[{2: self.primary0, 3: self.primary1}, [], []]),
            patch('gpstart.GpDbidFile'),
            patch('gpstart.GpEraFile'),
            patch('gpstart.userinput'),
            patch('gpstart.HeapChecksum'),
            patch('gpstart.log_to_file_only'),
            patch("gpstart.is_filespace_configured", return_value=True),
            patch("gpstart.CheckFilespaceConsistency"),
            patch("gpstart.StartSegmentsOperation"),
            patch("gpstart.base.WorkerPool"),
            patch("gpstart.gp.MasterStart.local"),
            patch("gpstart.pg.DbStatus.local"),
            patch("gpstart.TableLogger"),
            patch('gpstart.PgControlData'),
        ])

        self.mockFilespaceConsistency = self.get_mock_from_apply_patch("CheckFilespaceConsistency")
        self.mockFilespaceConsistency.return_value.run.return_value = True

        self.mock_start_result = self.get_mock_from_apply_patch('StartSegmentsOperation')
        self.mock_start_result.return_value.startSegments.return_value.getSuccessfulSegments.return_value = start_result.getSuccessfulSegments()

        self.mock_os_path_exists = self.get_mock_from_apply_patch('exists')
        self.mock_gp = self.get_mock_from_apply_patch('gp')
        self.mock_pgconf = self.get_mock_from_apply_patch('pgconf')
        self.mock_userinput = self.get_mock_from_apply_patch('userinput')
        self.mock_heap_checksum = self.get_mock_from_apply_patch('HeapChecksum')
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [1])
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = True
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = ([], [], None)
        self.mock_pgconf.readfile.return_value = Mock()
        self.mock_gplog_log_to_file_only = self.get_mock_from_apply_patch("log_to_file_only")

        self.mock_gp.get_masterdatadir.return_value = 'masterdatadir'
        self.mock_gp.GpCatVersion.local.return_value = 1
        self.mock_gp.GpCatVersionDirectory.local.return_value = 1
        sys.argv = ["gpstart"]  # reset to relatively empty args list

    def tearDown(self):
        super(GpStart, self).tearDown()

    def test_option_master_success_without_auto_accept(self):
        sys.argv = ["gpstart", "-m"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()

        gpstart = self.subject.GpStart.createProgram(options, args)
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with master-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Master instance in admin mode')
        self.subject.logger.info.assert_any_call('Master Started...')
        self.assertEqual(return_code, 0)

    def test_option_master_success_with_auto_accept(self):
        sys.argv = ["gpstart", "-m", "-a"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False

        self.mock_os_path_exists.side_effect = os_exists_check

        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()

        gpstart = self.subject.GpStart.createProgram(options, args)
        return_code = gpstart.run()

        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 0)
        self.subject.logger.info.assert_any_call('Starting Master instance in admin mode')
        self.subject.logger.info.assert_any_call('Master Started...')
        self.assertEqual(return_code, 0)

    def test_output_to_stdout_and_log_for_master_only_happens_before_heap_checksum(self):
        sys.argv = ["gpstart", "-m"]
        self.mock_userinput.ask_yesno.return_value = True
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()
        gpstart = self.subject.GpStart.createProgram(options, args)

        return_code = gpstart.run()

        self.assertEqual(return_code, 0)
        self.assertEqual(self.mock_userinput.ask_yesno.call_count, 1)
        self.mock_userinput.ask_yesno.assert_called_once_with(None, '\nContinue with master-only startup', 'N')
        self.subject.logger.info.assert_any_call('Starting Master instance in admin mode')
        self.subject.logger.info.assert_any_call('Master Started...')

        self.assertEquals(self.mock_gplog_log_to_file_only.call_count, 0)

    def test_output_to_stdout_and_log_differs_for_heap_checksum(self):
        sys.argv = ["gpstart", "-a"]
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = False
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        self.primary1.heap_checksum = 0
        self.master.heap_checksum = '1'
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = (
            [self.primary0], [self.primary1], self.master.heap_checksum)
        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()
        gpstart = self.subject.GpStart.createProgram(options, args)

        return_code = gpstart.run()

        self.assertEqual(return_code, 1)

        self.subject.logger.fatal.assert_any_call('Cluster heap checksum setting differences reported.')
        self.mock_gplog_log_to_file_only.assert_any_call('Failed checksum consistency validation:', logging.WARN)
        self.mock_gplog_log_to_file_only.assert_any_call('dbid: %s '
                                                         'checksum set to %s differs from '
                                                         'master checksum set to %s' %
                                                         (self.primary1.getSegmentDbId(), 0, 1), logging.WARN)
        self.subject.logger.fatal.assert_any_call("Shutting down master")
        self.assertEquals(self.mock_gp.GpStop.call_count, 1)

    def test_failed_to_contact_segments_causes_logging_and_failure(self):
        sys.argv = ["gpstart", "-a"]
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([], [1])
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()
        gpstart = self.subject.GpStart.createProgram(options, args)

        return_code = gpstart.run()

        self.assertEqual(return_code, 1)
        self.subject.logger.fatal.assert_any_call(
            'No segments responded to ssh query for heap checksum. Not starting the array.')

    def test_checksum_consistent(self):
        sys.argv = ["gpstart", "-a"]
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [1])
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()
        gpstart = self.subject.GpStart.createProgram(options, args)

        return_code = gpstart.run()

        self.assertEqual(return_code, 0)

        self.subject.logger.info.assert_any_call('Heap checksum setting is consistent across the cluster')

    def test_skip_checksum_validation_succeeds(self):
        sys.argv = ["gpstart", "-a", "--skip-heap-checksum-validation"]
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [1])
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_os_path_exists.side_effect = os_exists_check
        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()
        gpstart = self.subject.GpStart.createProgram(options, args)

        return_code = gpstart.run()

        self.assertEqual(return_code, 0)
        messages = [msg[0][0] for msg in self.subject.logger.info.call_args_list]
        self.assertNotIn('Heap checksum setting is consistent across the cluster', messages)
        self.subject.logger.warning.assert_any_call('Because of --skip-heap-checksum-validation, '
                                                    'the GUC for data_checksums '
                                                    'will not be checked between master and segments')

    def test_gpstart_fails_if_standby_heap_checksum_doesnt_match_master(self):
        sys.argv = ["gpstart", "-a"]
        self.gparray = GpArray([self.master, self.primary0, self.primary1, self.mirror0, self.mirror1, self.standby])
        self.segments_by_content_id = GpArray.getSegmentsByContentId(self.gparray.getSegDbList())
        self.mock_os_path_exists.side_effect = os_exists_check
        self.subject.unix.PgPortIsActive.local.return_value = False
        self.mock_heap_checksum.return_value.get_master_value.return_value = 1
        self.mock_heap_checksum.return_value.get_standby_value.return_value = 0

        parser = self.subject.GpStart.createParser()
        options, args = parser.parse_args()
        gpstart = self.subject.GpStart.createProgram(options, args)

        with patch("gpstart.GpArray.initFromCatalog", return_value=self.gparray):
            return_code = gpstart.run()
        self.assertEqual(return_code, 1)
        self.subject.logger.warning.assert_any_call("Heap checksum settings on standby master do not match master <<<<<<<<")
        self.subject.logger.error.assert_any_call("gpstart error: Heap checksum settings are not consistent across the cluster.")

    def _createGpArrayWith2Primary2Mirrors(self):
        self.master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|5532|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        self.primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        self.mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        self.mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")
        self.standby = Segment.initFromString(
            "6|-1|m|m|s|u|sdw3|sdw3|5433|5533|/data/standby||/data/standby/base/10899,/data/standby/base/1,/data/standby/base/10898,/data/standby/base/25780,/data/standby/base/34782")
        return GpArray([self.master, self.primary0, self.primary1, self.mirror0, self.mirror1])

    def _get_env(self, arg):
        if arg not in self.os_environ:
            return None
        return self.os_environ[arg]


def os_exists_check(arg):
    # Skip file related checks
    if 'pg_log' in arg:
        return True
    elif 'postmaster.pid' in arg or '.s.PGSQL' in arg:
        return False
    return False


if __name__ == '__main__':
    run_tests()
