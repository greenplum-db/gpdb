from mock import call, Mock, patch

from .gp_unittest import Contains, GpTestCase
from gppylib.commands.base import CommandResult
from gppylib.commands import gp
from gppylib.gparray import Segment
from gppylib.operations.buildMirrorSegments import GpMirrorToBuild
from gppylib.recoveryinfo import  build_recovery_info, RecoveryInfo, RecoveryResult


class BuildRecoveryInfoTestCase(GpTestCase):
    def setUp(self):
        self.maxDiff = None
        self.p1 = Segment.initFromString("1|0|p|p|s|u|sdw1|sdw1|1000|/data/primary1")
        self.p2 = Segment.initFromString("2|1|p|p|s|u|sdw2|sdw2|2000|/data/primary2")
        self.p3 = Segment.initFromString("3|2|p|p|s|u|sdw2|sdw2|3000|/data/primary3")
        self.p4 = Segment.initFromString("4|3|p|p|s|u|sdw3|sdw3|4000|/data/primary4")

        self.m1 = Segment.initFromString("5|0|m|m|s|d|sdw2|sdw2|5000|/data/mirror1")
        self.m2 = Segment.initFromString("6|1|m|m|s|d|sdw1|sdw1|6000|/data/mirror2")
        self.m3 = Segment.initFromString("7|2|m|m|s|d|sdw3|sdw3|7000|/data/mirror3")
        self.m4 = Segment.initFromString("8|3|m|m|s|d|sdw3|sdw3|8000|/data/mirror4")

        self.m5 = Segment.initFromString("5|0|m|m|s|d|sdw4|sdw4|9000|/data/mirror5")
        self.m6 = Segment.initFromString("6|1|m|m|s|d|sdw4|sdw4|10000|/data/mirror6")
        self.m7 = Segment.initFromString("7|2|m|m|s|d|sdw1|sdw1|11000|/data/mirror7")
        self.m8 = Segment.initFromString("8|3|m|m|s|d|sdw1|sdw1|12000|/data/mirror8")


        self.apply_patches([
            patch('recoveryinfo.gplog.get_logger_dir', return_value='/tmp/logdir'),
            patch('recoveryinfo.datetime.datetime')
        ])
        self.mock_logdir = self.get_mock_from_apply_patch('get_logger_dir')


    def tearDown(self):
        super(BuildRecoveryInfoTestCase, self).tearDown()

    def test_build_recovery_info_passes(self):
        # The expected dictionary within each test has target_host as the key.
        # Each recoveryInfo object holds source_host (live segment), but not the target_host.
        tests = [
            {
                "name": "single_target_host_suggest_full_and_incr",
                "mirrors_to_build": [GpMirrorToBuild(self.m3, self.p3, None, True),
                                     GpMirrorToBuild(self.m4, self.p4, None, False)],
                "expected": {'sdw3': [RecoveryInfo('/data/mirror3', 7000, 7, 'sdw2', 3000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid7.out'),
                                      RecoveryInfo('/data/mirror4', 8000, 8, 'sdw3', 4000,
                                                   False, '/tmp/logdir/pg_rewind.111.dbid8.out')]}
            },
            {
                "name": "single_target_hosts_suggest_full_and_incr_with_failover",
                "mirrors_to_build": [GpMirrorToBuild(self.m1, self.p1, self.m5, True),
                                     GpMirrorToBuild(self.m2, self.p2, self.m6, False)],
                "expected": {'sdw4': [RecoveryInfo('/data/mirror5', 9000, 5, 'sdw1', 1000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid5.out'),
                                      RecoveryInfo('/data/mirror6', 10000, 6, 'sdw2', 2000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid6.out')]}
            },
            {
                "name": "multiple_target_hosts_suggest_full",
                "mirrors_to_build": [GpMirrorToBuild(self.m1, self.p1, None, True),
                                     GpMirrorToBuild(self.m2, self.p2, None, True)],
                "expected": {'sdw2': [RecoveryInfo('/data/mirror1', 5000, 5, 'sdw1', 1000,
                                                  True, '/tmp/logdir/pg_basebackup.111.dbid5.out')],
                             'sdw1': [RecoveryInfo('/data/mirror2', 6000, 6, 'sdw2', 2000,
                                                  True, '/tmp/logdir/pg_basebackup.111.dbid6.out')]}
            },
            {
                "name": "multiple_target_hosts_suggest_full_and_incr",
                "mirrors_to_build": [GpMirrorToBuild(self.m1, self.p1, None, True),
                                     GpMirrorToBuild(self.m3, self.p3, None, False),
                                     GpMirrorToBuild(self.m4, self.p4, None, True)],
                "expected": {'sdw2': [RecoveryInfo('/data/mirror1', 5000, 5, 'sdw1', 1000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid5.out')],
                             'sdw3': [RecoveryInfo('/data/mirror3', 7000, 7, 'sdw2', 3000,
                                                   False, '/tmp/logdir/pg_rewind.111.dbid7.out'),
                                      RecoveryInfo('/data/mirror4', 8000, 8, 'sdw3', 4000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid8.out')]}
            },
            {
                "name": "multiple_target_hosts_suggest_incr_failover_same_as_failed",
                "mirrors_to_build": [GpMirrorToBuild(self.m1, self.p1, self.m1, False),
                                     GpMirrorToBuild(self.m2, self.p2, self.m2, False)],
                "expected": {'sdw2': [RecoveryInfo('/data/mirror1', 5000, 5, 'sdw1', 1000,
                                                  True, '/tmp/logdir/pg_basebackup.111.dbid5.out')],
                             'sdw1': [RecoveryInfo('/data/mirror2', 6000, 6, 'sdw2', 2000,
                                                  True, '/tmp/logdir/pg_basebackup.111.dbid6.out')]}
            },
            {
                "name": "multiple_target_hosts_suggest_full_failover_same_as_failed",
                "mirrors_to_build": [GpMirrorToBuild(self.m1, self.p1, self.m1, True),
                                     GpMirrorToBuild(self.m3, self.p3, self.m3, True),
                                     GpMirrorToBuild(self.m4, self.p4, None, True)],
                "expected": {'sdw2': [RecoveryInfo('/data/mirror1', 5000, 5, 'sdw1', 1000,
                                                  True, '/tmp/logdir/pg_basebackup.111.dbid5.out')],
                             'sdw3': [RecoveryInfo('/data/mirror3', 7000, 7, 'sdw2', 3000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid7.out'),
                                      RecoveryInfo('/data/mirror4', 8000, 8, 'sdw3', 4000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid8.out')]}
            },
            {
                "name": "multiple_target_hosts_suggest_full_and_incr",
                "mirrors_to_build": [GpMirrorToBuild(self.m1, self.p1, self.m5, True),
                                     GpMirrorToBuild(self.m2, self.p2, None, False),
                                     GpMirrorToBuild(self.m3, self.p3, self.m3, False),
                                     GpMirrorToBuild(self.m4, self.p4, self.m8, True)],
                "expected": {'sdw4': [RecoveryInfo('/data/mirror5', 9000, 5, 'sdw1', 1000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid5.out'),
                                      ],
                             'sdw1': [RecoveryInfo('/data/mirror2', 6000, 6,
                                                   'sdw2', 2000, False,
                                                   '/tmp/logdir/pg_rewind.111.dbid6.out'),
                                      RecoveryInfo('/data/mirror8', 12000, 8,
                                                   'sdw3', 4000, True,
                                                   '/tmp/logdir/pg_basebackup.111.dbid8.out')],
                             'sdw3': [RecoveryInfo('/data/mirror3', 7000, 7, 'sdw2', 3000,
                                                   True, '/tmp/logdir/pg_basebackup.111.dbid7.out')]
                             }
            },

        ]
        self.run_tests(tests)

    def run_tests(self, tests):
        for test in tests:
            with self.subTest(msg=test["name"]):
                self.mock_datetime = self.get_mock_from_apply_patch('datetime')
                self.mock_datetime.today.return_value.strftime = Mock(side_effect=['111', '222'])
                actual_ri_by_host = build_recovery_info(test['mirrors_to_build'])
                self.assertEqual(test['expected'], actual_ri_by_host)
                self.mock_datetime.today.return_value.strftime.assert_called_once()


class RecoveryResultTestCase(GpTestCase):
    def setUp(self):
        self.maxDiff = None
        self.all_dbids = set([2, 3, 4])

    def tearDown(self):
        super(RecoveryResultTestCase, self).tearDown()

    #FIXME better name ?
    def _msg(self, host, port, logfile=None, datadir=None, error=None, type=None):
        if logfile:
            return Contains('hostname: {}; port: {}; logfile: {}; recoverytype: {}'.format(host, port, logfile, type))
        elif datadir:
            return Contains('hostname: {}; port: {}; datadir: {}'.format(host, port, datadir))
        elif error:
            return Contains('hostname: {}; port: {}; error: {}'.format(host, port, error))

    def test_run_setup_recovery(self):
        tests = [
            {
                "name": "run_setup_recovery_no_errors",
                "host1_error": None,
                "host2_error": None,
                "expected_info_msgs": [],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": True,
                "recovery_successful": True,
            },
            {
                "name": "setup_recovery_errors",
                "host1_error": '[{"error_type": "validation", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                              '{"error_type": "validation", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "validation", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Failed to setup recovery for the following segments'))],
                "expected_error_msgs": [call(self._msg('host1', 7001, error='some error for dbid 2')),
                                        call(self._msg('host1', 7003, error='some error for dbid 3')),
                                        call(self._msg('host2', 7005, error='some error for dbid 4'))],
                "setup_successful": False,
                "full_recovery_successful": False,
                "recovery_successful": False,
            },
            {
                "name": "setup_recovery_invalid_errors",
                "host1_error": 'invalid value before error1 [{"error_type": "validation", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                              'invalid value before error2 {"error_type": "validation", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "validation", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call('Failed to setup recovery for the following segments')],
                "expected_error_msgs": [call(self._msg('host2', 7005, error='some error for dbid 4')),
                                        call(Contains('Unable to parse recovery error. hostname: host1, error: invalid value before error1'))],
                "setup_successful": False,
                "full_recovery_successful": False,
                "recovery_successful": False,
            },
            {
                "name": "run_setup_recovery_empty_errors",
                "host1_error": '',
                "host2_error": '',
                "expected_info_msgs": [],
                "expected_error_msgs": [call(Contains('Unable to parse recovery error. hostname: host1, error: ')),
                                        call(Contains('Unable to parse recovery error. hostname: host2, error: '))],
                "setup_successful": False,
                "full_recovery_successful": False,
                "recovery_successful": False,
            },
            {
                "name": "run_setup_recovery_errors_on_one_host",
                "host1_error": '[{"error_type": "validation", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                               '{"error_type": "validation", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": None,
                "expected_info_msgs": [call(Contains('-----')),
                                       call('Failed to setup recovery for the following segments')],
                "expected_error_msgs": [call(self._msg('host1', 7001, error='some error for dbid 2')),
                                        call(self._msg('host1', 7003, error='some error for dbid 3'))],
                "setup_successful": False,
                "full_recovery_successful": False,
                "recovery_successful": False,
            },
            {
                "name": "run_setup_recovery_does_not_print_other_errors",
                "host1_error": '[{"error_type": "full", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}]',
                "host2_error": '[{"error_type": "start", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [2]
            },
        ]
        self.run_tests(tests)

    def test_run_recovery(self):
        tests = [
            {
                "name": "run__recovery_no_errors",
                "host1_error": None,
                "host2_error": None,
                "expected_info_msgs": [],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": True,
                "recovery_successful": True,
                "dbids_that_failed_bb_rewind": []
            },
            {
                "name": "run_recovery_invalid_errors",
                "host1_error": 'invalid error1',
                "host2_error": '[{"error_type": "full", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call('Failed to action_recover the following segments'),
                                       call(self._msg('host2', 7005, logfile='/tmp/progress4', type='full'))],
                "expected_error_msgs": [call(Contains('Unable to parse recovery error. hostname: host1, error: invalid error1'))],
                "setup_successful": False,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [4]
            },
            {
                "name": "run_recovery_empty_errors",
                "host1_error": '',
                "host2_error": '',
                "expected_info_msgs": [],
                "expected_error_msgs": [call(Contains('Unable to parse recovery error. hostname: host1, error: ')),
                                        call(Contains('Unable to parse recovery error. hostname: host2, error: '))],
                "setup_successful": False,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": []
            },
            {
                "name": "run_recovery_all_dbids_fail_only_bb_errors",
                "host1_error": '[{"error_type": "full", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                               '{"error_type": "full", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "full", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call('Failed to action_recover the following segments'),
                                       call(self._msg('host1', 7001, logfile='/tmp/progress2', type='full')),
                                       call(self._msg('host1', 7003, logfile='/tmp/progress3', type='full')),
                                       call(self._msg('host2', 7005, logfile='/tmp/progress4', type='full'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [2, 3, 4]
            },
            {
                "name": "run_recovery_all_dbids_fail_only_rewind_errors",
                "host1_error": '[{"error_type": "incremental", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                               '{"error_type": "incremental", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "incremental", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call('Failed to action_recover the following segments. You must run gprecoverseg -F for all incremental failures'),
                                       call(self._msg('host1', 7001, logfile='/tmp/progress2', type='incremental')),
                                       call(self._msg('host1', 7003, logfile='/tmp/progress3', type='incremental')),
                                       call(self._msg('host2', 7005, logfile='/tmp/progress4', type='incremental'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": True,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [2, 3, 4]
            },
            {
                "name": "run_recovery_all_dbids_fail_only_bb_rewind_errors",
                "host1_error": '[{"error_type": "full", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                              '{"error_type": "incremental", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "full", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Failed to action_recover the following segments. You must run gprecoverseg -F for all incremental failures')),
                                       call(self._msg('host1', 7003, logfile='/tmp/progress3', type='incremental')),
                                       call(self._msg('host1', 7001, logfile='/tmp/progress2', type='full')),
                                       call(self._msg('host2', 7005, logfile='/tmp/progress4', type='full'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [2, 3, 4]
            },
            {
                "name": "run_recovery_some_dbids_fail_only_bb_rewind_errors",
                "host1_error": '[{"error_type": "incremental", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}]',
                "host2_error": '[{"error_type": "full", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Failed to action_recover the following segments. You must run gprecoverseg -F for all incremental failures')),
                                       call(self._msg('host1', 7001, logfile='/tmp/progress2', type='incremental')),
                                       call(self._msg('host2', 7005, logfile='/tmp/progress4', type='full'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [2, 4]
            },
            {
                "name": "run_recovery_all_dbids_fail_only_start_errors",
                "host1_error": '[{"error_type": "start", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                              '{"error_type": "start", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "start", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Failed to start the following segments')),
                                       call(self._msg('host1', 7001, datadir='/datadir2')),
                                       call(self._msg('host1', 7003, datadir='/datadir3')),
                                       call(self._msg('host2', 7005, datadir='/datadir4'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": True,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": []
            },
            {
                "name": "run_recovery_some_dbids_fail_only_start_errors",
                "host1_error": '[{"error_type": "start", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}]',
                "host2_error": '[{"error_type": "start", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Failed to start the following segments')),
                                       call(self._msg('host1', 7001, datadir='/datadir2')),
                                       call(self._msg('host2', 7005, datadir='/datadir4'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": True,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": []
            },
            {
                "name": "run_recovery_all_dbids_fail_both_recovery_and_start_errors",
                "host1_error": '[{"error_type": "full",  "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                              '{"error_type": "start",  "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "full",  "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Failed to action_recover the following segments')),
                                       call(self._msg('host1', 7001, logfile='/tmp/progress2', type='full')),
                                       call(self._msg('host2', 7005, logfile='/tmp/progress4', type='full')),
                                       call(Contains('-----')),
                                       call(Contains('Failed to start the following segments. Please check the latest logs')),
                                       call(self._msg('host1', 7003, datadir='/datadir3'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [2, 4]
            },
            {
                "name": "run_recovery_all_dbids_fail_both_recovery_and_default_errors",
                "host1_error": '[{"error_type": "full",  "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                              '{"error_type": "default",  "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "start",  "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Failed to action_recover the following segments')),
                                       call(self._msg('host1', 7001, logfile='/tmp/progress2', type='full')),
                                       call(Contains('-----')),
                                       call(Contains('Failed to start the following segments. Please check the latest logs')),
                                       call(self._msg('host2', 7005, datadir='/datadir4'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [2]
            },
            {
                "name": "run_recovery_all_dbids_fail_only_update_errors",
                "host1_error": '[{"error_type": "update", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                               '{"error_type": "update", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "update", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Did not start the following segments due to failure while updating the port.')),
                                       call(self._msg('host1', 7001, datadir='/datadir2')),
                                       call(self._msg('host1', 7003, datadir='/datadir3')),
                                       call(self._msg('host2', 7005, datadir='/datadir4'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": True,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": []
            },
            {
                "name": "run_recovery_some_dbids_fail_only_update_errors",
                "host1_error": '[{"error_type": "update", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}]',
                "host2_error": '[{"error_type": "update", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Did not start the following segments due to failure while updating the port.')),
                                       call(self._msg('host1', 7001, datadir='/datadir2')),
                                       call(self._msg('host2', 7005, datadir='/datadir4'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": True,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": []
            },
            {
                "name": "run_recovery_some_dbids_fail_recovery_and_update_errors",
                "host1_error": '[{"error_type": "full",  "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                               '{"error_type": "default",  "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]',
                "host2_error": '[{"error_type": "update", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]',
                "expected_info_msgs": [call(Contains('-----')),
                                       call(Contains('Failed to action_recover the following segments')),
                                       call(self._msg('host1', 7001, logfile='/tmp/progress2', type='full')),
                                       call(Contains('-----')),
                                       call(Contains('Did not start the following segments due to failure while updating the port.')),
                                       call(self._msg('host2', 7005, datadir='/datadir4'))],
                "expected_error_msgs": [],
                "setup_successful": True,
                "full_recovery_successful": False,
                "recovery_successful": False,
                "dbids_that_failed_bb_rewind": [2]
            },
        ]
        self.run_tests(tests, run_recovery=True)

    def run_tests(self, tests, run_recovery=False):
        for test in tests:
            with self.subTest(msg=test["name"]):
                if test["host1_error"] is None:
                    host1_result = CommandResult(0, b"", b"", True, False)
                else:
                    host1_result = CommandResult(1, 'failed 1'.encode(), test["host1_error"].encode(), True, False)
                if test["host2_error"] is None:
                    host2_result = CommandResult(0, b"", b"", True, False)
                else:
                    host2_result = CommandResult(1, 'failed 2'.encode(), test["host2_error"].encode(), True, False)

                host1_recovery_output = gp.GpSegRecovery(None, None, None, False, 1, 'host1', None, True)
                host2_recovery_output = gp.GpSegRecovery(None, None, None, False, 1, 'host2', None, True)
                host1_recovery_output.get_results = Mock(return_value=host1_result)
                host2_recovery_output.get_results = Mock(return_value=host2_result)

                mock_logger = Mock(spec=['log', 'info', 'debug', 'error', 'warn', 'exception'])
                r = RecoveryResult('action_recover', [host1_recovery_output, host2_recovery_output], mock_logger)
                self.assertEqual(r.setup_successful(), test["setup_successful"])
                self.assertEqual(r.full_recovery_successful(), test["full_recovery_successful"])
                self.assertEqual(r.recovery_successful(), test["recovery_successful"])

                if run_recovery:
                    r.print_bb_rewind_update_and_start_errors()
                else:
                    r.print_setup_recovery_errors()

                self.assertEqual(test['expected_info_msgs'], mock_logger.info.call_args_list)
                self.assertEqual(test['expected_error_msgs'], mock_logger.error.call_args_list)

                dbids_that_failed_bb_rewind = test.get("dbids_that_failed_bb_rewind", [])
                for failed_dbid in dbids_that_failed_bb_rewind:
                    self.assertFalse(r.was_bb_rewind_successful(failed_dbid))
                dbids_that_passed_bb_rewind = self.all_dbids - set(dbids_that_failed_bb_rewind)
                for pass_dbid in dbids_that_passed_bb_rewind:
                    self.assertTrue(r.was_bb_rewind_successful(pass_dbid))
