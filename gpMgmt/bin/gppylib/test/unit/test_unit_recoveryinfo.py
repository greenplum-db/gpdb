from mock import call, Mock, patch

from .gp_unittest import GpTestCase, SubTest
from gppylib.gparray import Segment
from gppylib.operations.buildMirrorSegments import GpMirrorToBuild
from gppylib.recoveryinfo import  build_recovery_info, RecoveryInfo


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
            with SubTest.subTest(test["name"]):
                self.mock_datetime = self.get_mock_from_apply_patch('datetime')
                self.mock_datetime.today.return_value.strftime = Mock(side_effect=['111', '222'])
                actual_ri_by_host = build_recovery_info(test['mirrors_to_build'])
                self.assertEqual(test['expected'], actual_ri_by_host)
                self.mock_datetime.today.return_value.strftime.assert_called_once()
