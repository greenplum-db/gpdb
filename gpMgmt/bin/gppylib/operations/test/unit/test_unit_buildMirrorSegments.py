#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
import io
import logging
from mock import ANY, call, patch, Mock

from gppylib import gplog
from gppylib.commands import base
from gppylib.gparray import Segment, GpArray
from gppylib.operations.buildMirrorSegments import GpMirrorToBuild, GpMirrorListToBuild, GpStopSegmentDirectoryDirective
from gppylib.operations.startSegments import StartSegmentsResult
from gppylib.system import configurationInterface
from test.unit.gp_unittest import GpTestCase


class BuildMirrorsTestCase(GpTestCase):
    """
    This class only tests for the buildMirrors function and also test_clean_up_failed_segments
    """
    def setUp(self):
        self.maxDiff = None
        self.coordinator = Segment(content=-1, preferred_role='p', dbid=1, role='p', mode='s',
                                   status='u', hostname='coordinatorhost', address='coordinatorhost-1',
                                   port=1111, datadir='/coordinatordir')

        self.primary = Segment(content=0, preferred_role='p', dbid=20, role='p', mode='s',
                               status='u', hostname='primaryhost', address='primaryhost-1',
                               port=3333, datadir='/primary')
        self.mirror = Segment(content=0, preferred_role='m', dbid=30, role='m', mode='s',
                              status='d', hostname='primaryhost', address='primaryhost-1',
                              port=3333, datadir='/primary')

        gplog.get_unittest_logger()
        self.apply_patches([
            patch('gppylib.operations.buildMirrorSegments.GpArray.getSegmentsByHostName')
        ])
        self.mock_get_segments_by_hostname = self.get_mock_from_apply_patch('getSegmentsByHostName')

        self.action = 'recover'
        self.gpEnv = Mock()
        self.gpArray = GpArray([self.coordinator, self.primary, self.mirror])
        self.mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

    def tearDown(self):
        super(BuildMirrorsTestCase, self).tearDown()

    def _setup_mocks(self, buildMirrorSegs_obj):
        buildMirrorSegs_obj._GpMirrorListToBuild__startAll = Mock(return_value=True)
        markdown_mock = Mock()
        buildMirrorSegs_obj._wait_fts_to_mark_down_segments = markdown_mock
        buildMirrorSegs_obj._run_recovery = Mock()
        buildMirrorSegs_obj._clean_up_failed_segments = Mock()
        buildMirrorSegs_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear = Mock()
        buildMirrorSegs_obj._get_running_postgres_segments = Mock()
        configurationInterface.getConfigurationProvider = Mock()
        configurationInterface.getConfigurationProvider.return_value.updateSystemConfig.return_value = {} # mock backout_map

    def _common_asserts_with_stop_and_logger(self, buildMirrorSegs_obj, expected_logger_msg, expected_segs_to_stop,
                                             expected_segs_to_start, expected_segs_to_markdown, expected_segs_to_update,
                                             cleanup_count):
        self.mock_logger.info.assert_any_call(expected_logger_msg)
        #TODO assert all logger info msgs
        self.assertEqual(4, self.mock_logger.info.call_count)

        self.assertEqual([call(expected_segs_to_stop)],
                         buildMirrorSegs_obj._get_running_postgres_segments.call_args_list)
        self._common_asserts(buildMirrorSegs_obj, expected_segs_to_start, expected_segs_to_markdown,
                             expected_segs_to_update, cleanup_count)

    def _common_asserts(self, buildMirrorSegs_obj, expected_segs_to_start, expected_segs_to_markdown,
                        expected_segs_to_update, cleanup_count):
        self.assertEqual([call(self.gpEnv, expected_segs_to_markdown)],
                         buildMirrorSegs_obj._wait_fts_to_mark_down_segments.call_args_list)
        self.assertEqual(cleanup_count, buildMirrorSegs_obj._clean_up_failed_segments.call_count)

        self.assertEqual(1, buildMirrorSegs_obj._run_recovery.call_count)

        self.assertEqual([call(self.gpArray, ANY, dbIdToForceMirrorRemoveAdd=expected_segs_to_update,
                               useUtilityMode=False, allowPrimary=False)],
                         configurationInterface.getConfigurationProvider.return_value.updateSystemConfig.call_args_list)
        self.assertEqual([call(self.gpEnv, self.gpArray, expected_segs_to_start)],
                         buildMirrorSegs_obj._GpMirrorListToBuild__startAll.call_args_list)

    def _run_no_failed_tests(self, tests):
        mirrors_to_build = []
        expected_segs_to_start = []

        for test in tests:
            with self.subTest(msg=test["name"]):
                mirrors_to_build.append(GpMirrorToBuild(None, test["live"], test["failover"],
                                                        test["forceFull"]))
                expected_segs_to_start.append(test["failover"])

        buildMirrorSegs_obj = self._run_buildMirrors(mirrors_to_build)

        self.assertEqual(3, self.mock_logger.info.call_count)
        self.assertEqual(0, buildMirrorSegs_obj._get_running_postgres_segments.call_count)
        self._common_asserts(buildMirrorSegs_obj, expected_segs_to_start, [], {2: True, 4: True}, 1)

        for test in tests:
            self.assertEqual('n', test['live'].getSegmentMode())
            self.assertEqual('d', test['failover'].getSegmentStatus())
            self.assertEqual('n', test['failover'].getSegmentMode())

    def _run_no_failover_tests(self, tests):
        mirrors_to_build = []
        expected_segs_to_start = []
        expected_segs_to_stop = []
        expected_segs_to_markdown = []

        for test in tests:
            with self.subTest(msg=test["name"]):
                mirrors_to_build.append(GpMirrorToBuild(test["failed"], test["live"], None,
                                                        test["forceFull"]))
                expected_segs_to_stop.append(test["failed"])
                expected_segs_to_start.append(test["failed"])
                if 'is_failed_segment_up' in test and test["is_failed_segment_up"]:
                    expected_segs_to_markdown.append(test['failed'])

        buildMirrorSegs_obj = self._run_buildMirrors(mirrors_to_build)

        self._common_asserts_with_stop_and_logger(buildMirrorSegs_obj, "Ensuring 4 failed segment(s) are stopped",
                                                  expected_segs_to_stop, expected_segs_to_start,
                                                  expected_segs_to_markdown, {4: True, 30: True}, 1)
        for test in tests:
            self.assertEqual('n', test['live'].getSegmentMode())
            self.assertEqual('d', test['failed'].getSegmentStatus())
            self.assertEqual('n', test['failed'].getSegmentMode())

    def _run_both_failed_failover_tests(self, tests):
        mirrors_to_build = []
        expected_segs_to_start = []
        expected_segs_to_stop = []
        expected_segs_to_markdown = []

        for test in tests:
            with self.subTest(msg=test["name"]):
                mirrors_to_build.append(GpMirrorToBuild(test["failed"], test["live"], test["failover"],
                                                        test["forceFull"]))
                expected_segs_to_stop.append(test["failed"])
                expected_segs_to_start.append(test["failover"])
                if 'is_failed_segment_up' in test and test["is_failed_segment_up"]:
                    expected_segs_to_markdown.append(test['failed'])

        buildMirrorSegs_obj = self._run_buildMirrors(mirrors_to_build)

        self._common_asserts_with_stop_and_logger(buildMirrorSegs_obj, "Ensuring 3 failed segment(s) are stopped",
                                                  expected_segs_to_stop, expected_segs_to_start,
                                                  expected_segs_to_markdown, {1: True, 5: True, 9: True}, 1)

        for test in tests:
            self.assertEqual('n', test['live'].getSegmentMode())
            self.assertEqual('d', test['failover'].getSegmentStatus())
            self.assertEqual('n', test['failover'].getSegmentMode())

    def _run_buildMirrors(self, mirrors_to_build):
        buildMirrorSegs_obj = GpMirrorListToBuild(
            toBuild=mirrors_to_build,
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger
        )
        self._setup_mocks(buildMirrorSegs_obj)
        self.assertTrue(buildMirrorSegs_obj.buildMirrors(self.action, self.gpEnv, self.gpArray))
        return buildMirrorSegs_obj

    def create_primary(self, dbid='1', contentid='0', state='n', status='u', host='sdw1'):
        return Segment.initFromString('{}|{}|p|p|{}|{}|{}|{}|21000|/primary/gpseg0'
                                      .format(dbid, contentid, state, status, host, host))

    def create_mirror(self, dbid='2', contentid='0', state='n', status='u', host='sdw2'):
        return Segment.initFromString('{}|{}|p|p|{}|{}|{}|{}|22000|/mirror/gpseg0'
                                      .format(dbid, contentid, state, status, host, host))

    def test_buildMirrors_failed_null_pass(self):
        failed_null_tests = [
            {
                "name": "no_failed_full",
                "live": self.create_primary(),
                "failover": self.create_mirror(),
                "forceFull": False,
            },
            {
                "name": "no_failed_full2",
                "live": self.create_primary(dbid='3'),
                "failover": self.create_mirror(dbid='4'),
                "forceFull": True,
            }
        ]
        self._run_no_failed_tests(failed_null_tests)

    def test_buildMirrors_no_failover_pass(self):
        tests = [
            {
                "name": "no_failover",
                "failed": self.create_mirror(status='d'),
                "live": self.create_primary(),
                "forceFull": False,
            },
            {
                "name": "no_failover_full",
                "failed": self.create_mirror(status='d', dbid='4'),
                "live": self.create_primary(),
                "forceFull": True,
            },
            {
                "name": "no_failover_failed_seg_exists_in_gparray",
                "failed": self.mirror,
                "live": self.create_primary(),
                "forceFull": True,
                "forceoverwrite": True
            },
            {
                "name": "no_failover_failed_segment_is_up",
                "failed": self.create_mirror(dbid='5'),
                "live": self.create_primary(dbid='6'),
                "is_failed_segment_up": True,
                "forceFull": False,
            }
        ]
        self._run_no_failover_tests(tests)

    def test_buildMirrors_both_failed_failover_pass(self):
        tests = [
            {
                "name": "both_failed_failover_full",
                "failed": self.create_primary(status='d'),
                "live": self.create_mirror(),
                "failover": self.create_primary(status='d', host='sdw3'),
                "forceFull": True
            },
            {
                "name": "both_failed_failover_failed_segment_is_up",
                "failed": self.create_primary(dbid='5'),
                "live": self.create_mirror(dbid='6'),
                "failover": self.create_primary(dbid='5', host='sdw3'),
                "is_failed_segment_up": True,
                "forceFull": True
            },
            {
                "name": "both_failed_failover_failover_is_down_live_is_marked_as_sync",
                "failed": self.create_primary(dbid='9', status='d'),
                "live": self.create_mirror(dbid='10', state='s'),
                "failover": self.create_primary(dbid='9', status='d'),
                "forceFull": False,
            },
        ]
        self._run_both_failed_failover_tests(tests)

    def test_buildMirrors_forceoverwrite_true(self):
        failed = self.create_primary(status='d')
        live = self.create_mirror()
        failover = self.create_primary(host='sdw3')

        buildMirrorSegs_obj = GpMirrorListToBuild(
            toBuild=[GpMirrorToBuild(failed, live, failover, False)],
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger,
            forceoverwrite=True
        )
        self._setup_mocks(buildMirrorSegs_obj)
        self.assertTrue(buildMirrorSegs_obj.buildMirrors(self.action, self.gpEnv, self.gpArray))

        self._common_asserts_with_stop_and_logger(buildMirrorSegs_obj, "Ensuring 1 failed segment(s) are stopped",
                                                  [failed], [failover], [], {1: True}, 0)
        self.assertEqual('n', live.getSegmentMode())
        self.assertEqual('d', failover.getSegmentStatus())
        self.assertEqual('n', failover.getSegmentMode())

    def test_buildMirrors_failed_seg_in_gparray_fail(self):
        tests = [
            {
                "name": "failed_seg_exists_in_gparray1",
                "failed": self.create_primary(status='d'),
                "failover": self.create_primary(status='d'),
                "live": self.create_mirror(),
                "forceFull": True,
                "forceoverwrite": False
            },
            {
                "name": "failed_seg_exists_in_gparray2",
                "failed": self.create_primary(dbid='3', status='d'),
                "failover": self.create_primary(dbid='3', status='d'),
                "live": self.create_mirror(dbid='4'),
                "forceFull": False,
                "forceoverwrite": False
            },
            {
                "name": "failed_seg_exists_in_gparray2",
                "failed": self.create_primary(dbid='3', status='d'),
                "failover": self.create_primary(dbid='3', status='d'),
                "live": self.create_mirror(dbid='4'),
                "forceFull": False,
                "forceoverwrite": True
            }
        ]
        for test in tests:
            mirror_to_build = GpMirrorToBuild(test["failed"], test["live"], test["failover"], test["forceFull"])
            buildMirrorSegs_obj = GpMirrorListToBuild(
                toBuild=[mirror_to_build,],
                pool=None,
                quiet=True,
                parallelDegree=0,
                logger=self.mock_logger,
                forceoverwrite=test['forceoverwrite']
            )
            self._setup_mocks(buildMirrorSegs_obj)
            local_gp_array = GpArray([self.coordinator, test["failed"]])
            expected_error = "failed segment should not be in the new configuration if failing over to"
            with self.subTest(msg=test["name"]):
                with self.assertRaisesRegex(Exception, expected_error):
                    buildMirrorSegs_obj.buildMirrors(self.action, self.gpEnv, local_gp_array)

    def test_clean_up_failed_segments(self):
        failed1 = self.create_primary(status='d')
        live1 = self.create_mirror()

        failed2 = self.create_primary(dbid='3', status='d')
        failover2 = self.create_primary(dbid='3', status='d')
        live2 = self.create_mirror(dbid='4')

        failed3 = self.create_primary(dbid='5')
        live3 = self.create_mirror(dbid='6')

        failed4 = self.create_primary(dbid='5')
        live4 = self.create_mirror(dbid='7')

        inplace_full1 = GpMirrorToBuild(failed1, live1, None, True)
        not_inplace_full = GpMirrorToBuild(failed2, live2, failover2, True)
        inplace_full2 = GpMirrorToBuild(failed3, live3, None, True)
        inplace_not_full = GpMirrorToBuild(failed4, live4, None, False)

        buildMirrorSegs_obj = GpMirrorListToBuild(
            toBuild=[inplace_full1, not_inplace_full, inplace_full2, inplace_not_full],
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger,
            forceoverwrite=True
        )
        buildMirrorSegs_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear = Mock()

        buildMirrorSegs_obj._clean_up_failed_segments()

        self.mock_get_segments_by_hostname.assert_called_once_with([failed1, failed3])
        self.mock_logger.info.called_once_with('"Cleaning files from 2 segment(s)')

    def test_clean_up_failed_segments_no_segs_to_cleanup(self):
        failed2 = self.create_primary(dbid='3', status='d')
        failover2 = self.create_primary(dbid='3', status='d')
        live2 = self.create_mirror(dbid='4')

        failed4 = self.create_primary(dbid='5')
        live4 = self.create_mirror(dbid='7')

        not_inplace_full = GpMirrorToBuild(failed2, live2, failover2, True)
        inplace_not_full = GpMirrorToBuild(failed4, live4, None, False)

        buildMirrorSegs_obj = GpMirrorListToBuild(
            toBuild=[not_inplace_full, inplace_not_full],
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger,
            forceoverwrite=True
        )
        buildMirrorSegs_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear = Mock()

        buildMirrorSegs_obj._clean_up_failed_segments()
        self.assertEqual(0, self.mock_get_segments_by_hostname.call_count)
        self.assertEqual(0, self.mock_logger.info.call_count)

    def test_buildMirrors_noMirrors(self):
        buildMirrorSegs_obj = GpMirrorListToBuild(
            toBuild=[],
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger
        )
        self.assertTrue(buildMirrorSegs_obj.buildMirrors(None, None, None))
        self.assertTrue(buildMirrorSegs_obj.buildMirrors(self.action, None, None))

        self.assertEqual([call('No segments to None'), call('No segments to recover')],
                         self.mock_logger.info.call_args_list)


class BuildMirrorSegmentsTestCase(GpTestCase):
    def setUp(self):
        self.coordinator = Segment(content=-1, preferred_role='p', dbid=1, role='p', mode='s',
                              status='u', hostname='coordinatorhost', address='coordinatorhost-1',
                              port=1111, datadir='/coordinatordir')

        self.primary = Segment(content=0, preferred_role='p', dbid=2, role='p', mode='s',
                               status='u', hostname='primaryhost', address='primaryhost-1',
                               port=3333, datadir='/primary')
        self.mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])
        gplog.get_unittest_logger()
        self.apply_patches([
        ])

        self.buildMirrorSegs = GpMirrorListToBuild(
            toBuild = [],
            pool = None,
            quiet = True,
            parallelDegree = 0,
            logger=self.mock_logger
            )

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster')
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost')
    def test_get_running_postgres_segments_empty_segs(self, mock1, mock2, mock3):
        toBuild = []
        expected_output = []
        segs = self.buildMirrorSegs._get_running_postgres_segments(toBuild)
        self.assertEqual(segs, expected_output)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', return_value=True)
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', return_value=True)
    def test_get_running_postgres_segments_all_pid_postmaster(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEqual(segs, mock_segs)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', side_effect=[True, False])
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', return_value=True)
    def test_get_running_postgres_segments_some_pid_postmaster(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        expected_output = []
        expected_output.append(mock_segs[0])
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEqual(segs, expected_output)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', side_effect=[True, False])
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', side_effect=[True, False])
    def test_get_running_postgres_segments_one_pid_postmaster(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        expected_output = []
        expected_output.append(mock_segs[0])
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEqual(segs, expected_output)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', side_effect=[False, False])
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', side_effect=[True, False])
    def test_get_running_postgres_segments_no_pid_postmaster(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        expected_output = []
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEqual(segs, expected_output)

    @patch('gppylib.operations.buildMirrorSegments.get_pid_from_remotehost')
    @patch('gppylib.operations.buildMirrorSegments.is_pid_postmaster', side_effect=[False, False])
    @patch('gppylib.operations.buildMirrorSegments.check_pid_on_remotehost', side_effect=[False, False])
    def test_get_running_postgres_segments_no_pid_running(self, mock1, mock2, mock3):
        mock_segs = [Mock(), Mock()]
        expected_output = []
        segs = self.buildMirrorSegs._get_running_postgres_segments(mock_segs)
        self.assertEqual(segs, expected_output)

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=base.CommandResult(rc=0, stdout=b'/tmp/seg0', stderr=b'', completed=True, halt=False))
    def test_dereference_remote_symlink_valid_symlink(self, mock1, mock2):
        datadir = '/tmp/link/seg0'
        host = 'h1'
        self.assertEqual(self.buildMirrorSegs.dereference_remote_symlink(datadir, host), '/tmp/seg0')

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=base.CommandResult(rc=1, stdout=b'', stderr=b'', completed=True, halt=False))
    def test_dereference_remote_symlink_unable_to_determine_symlink(self, mock1, mock2):
        datadir = '/tmp/seg0'
        host = 'h1'
        self.assertEqual(self.buildMirrorSegs.dereference_remote_symlink(datadir, host), '/tmp/seg0')
        self.mock_logger.warning.assert_any_call('Unable to determine if /tmp/seg0 is symlink. Assuming it is not symlink')

    @patch('gppylib.operations.buildMirrorSegments.read_era')
    @patch('gppylib.operations.startSegments.StartSegmentsOperation')
    def test_startAll_succeeds(self, mock1, mock2):
        result = StartSegmentsResult()
        result.getFailedSegmentObjs()
        mock1.return_value.startSegments.return_value = result
        result = self.buildMirrorSegs._GpMirrorListToBuild__startAll(Mock(), [Mock(), Mock()], [])
        self.assertTrue(result)

    @patch('gppylib.operations.buildMirrorSegments.read_era')
    @patch('gppylib.operations.startSegments.StartSegmentsOperation')
    def test_startAll_fails(self, mock1, mock2):
        result = StartSegmentsResult()
        failed_segment = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        result.addFailure(failed_segment, 'reason', 'reasoncode')
        mock1.return_value.startSegments.return_value = result
        result = self.buildMirrorSegs._GpMirrorListToBuild__startAll(Mock(), [Mock(), Mock()], [])
        self.assertFalse(result)
        self.mock_logger.warn.assert_any_call('Failed to start segment.  The fault prober will shortly mark it as down. '
                                         'Segment: sdw1:/data/primary0:content=0:dbid=2:role=p:preferred_role=p:mode=s:status=u: REASON: reason')

    def _createGpArrayWith2Primary2Mirrors(self):
        self.coordinator = Segment.initFromString(
                "1|-1|p|p|s|u|cdw|cdw|5432|/data/coordinator")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")

        return GpArray([self.coordinator, self.primary0, self.primary1, mirror0, mirror1])

    def test_checkForPortAndDirectoryConflicts__given_the_same_host_checks_ports_differ(self):
        self.coordinator.hostname = "samehost"
        self.primary.hostname = "samehost"

        self.coordinator.port = 1111
        self.primary.port = 1111

        gpArray = GpArray([self.coordinator, self.primary])

        with self.assertRaisesRegex(Exception, r"Segment dbid's 2 and 1 on host samehost cannot have the same port 1111"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)

    def test_checkForPortAndDirectoryConflicts__given_the_same_host_checks_data_directories_differ(self):
        self.coordinator.hostname = "samehost"
        self.primary.hostname = "samehost"

        self.coordinator.datadir = "/data"
        self.primary.datadir = "/data"

        gpArray = GpArray([self.coordinator, self.primary])

        with self.assertRaisesRegex(Exception, r"Segment dbid's 2 and 1 on host samehost cannot have the same data directory '/data'"):
            self.buildMirrorSegs.checkForPortAndDirectoryConflicts(gpArray)


class SegmentProgressTestCase(GpTestCase):
    """
    Test case for GpMirrorListToBuild._join_and_show_segment_progress().
    """
    def setUp(self):
        self.pool = Mock(spec=base.WorkerPool)
        self.buildMirrorSegs = GpMirrorListToBuild(
            toBuild=[],
            pool=self.pool,
            quiet=True,
            parallelDegree=0,
            logger=Mock(spec=logging.Logger)
        )

    def test_command_output_is_displayed_once_after_worker_pool_completes(self):
        cmd = Mock(spec=base.Command)
        cmd.remoteHost = 'localhost'
        cmd.dbid = 2
        cmd.get_results.return_value.stdout = "string 1\n"

        cmd2 = Mock(spec=base.Command)
        cmd2.remoteHost = 'host2'
        cmd2.dbid = 4
        cmd2.get_results.return_value.stdout = "string 2\n"

        outfile = io.StringIO()
        self.pool.join.return_value = True
        self.buildMirrorSegs._join_and_show_segment_progress([cmd, cmd2], outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): string 1\n'
            'host2 (dbid 4): string 2\n'
        ))

    def test_command_output_is_displayed_once_for_every_blocked_join(self):
        cmd = Mock(spec=base.Command)
        cmd.remoteHost = 'localhost'
        cmd.dbid = 2

        cmd.get_results.side_effect = [Mock(stdout="string 1"), Mock(stdout="string 2")]

        outfile = io.StringIO()
        self.pool.join.side_effect = [False, True]
        self.buildMirrorSegs._join_and_show_segment_progress([cmd], outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): string 1\n'
            'localhost (dbid 2): string 2\n'
        ))

    def test_inplace_display_uses_ansi_escapes_to_overwrite_previous_output(self):
        cmd = Mock(spec=base.Command)
        cmd.remoteHost = 'localhost'
        cmd.dbid = 2
        cmd.get_results.side_effect = [Mock(stdout="string 1"), Mock(stdout="string 2")]

        cmd2 = Mock(spec=base.Command)
        cmd2.remoteHost = 'host2'
        cmd2.dbid = 4
        cmd2.get_results.side_effect = [Mock(stdout="string 3"), Mock(stdout="string 4")]

        outfile = io.StringIO()
        self.pool.join.side_effect = [False, True]
        self.buildMirrorSegs._join_and_show_segment_progress([cmd, cmd2], inplace=True, outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): string 1\x1b[K\n'
            'host2 (dbid 4): string 3\x1b[K\n'
            '\x1b[2A'
            'localhost (dbid 2): string 2\x1b[K\n'
            'host2 (dbid 4): string 4\x1b[K\n'
        ))

    def test_errors_during_command_execution_are_displayed(self):
        cmd = Mock(spec=base.Command)
        cmd.remoteHost = 'localhost'
        cmd.dbid = 2
        cmd.get_results.return_value.stderr = "some error\n"
        cmd.run.side_effect = base.ExecutionError("Some exception", cmd)

        cmd2 = Mock(spec=base.Command)
        cmd2.remoteHost = 'host2'
        cmd2.dbid = 4
        cmd2.get_results.return_value.stderr = ''
        cmd2.run.side_effect = base.ExecutionError("Some exception", cmd2)

        outfile = io.StringIO()
        self.pool.join.return_value = True
        self.buildMirrorSegs._join_and_show_segment_progress([cmd, cmd2], outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): some error\n'
            'host2 (dbid 4): \n'
        ))


if __name__ == '__main__':
    run_tests()
