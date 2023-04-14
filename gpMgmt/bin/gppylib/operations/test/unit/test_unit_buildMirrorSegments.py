#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
import os
from collections import OrderedDict
import io
import logging
import shutil
import tempfile


from mock import ANY, call, patch, Mock, mock_open
from gppylib import gplog, recoveryinfo
from gppylib.commands import base, gp
from gppylib.commands.base import CommandResult, LocalExecutionContext

from gppylib.gparray import Segment, GpArray
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.operations.buildMirrorSegments import GpMirrorToBuild, GpMirrorListToBuild, GpStopSegmentDirectoryDirective
from gppylib.system import configurationInterface
from test.unit.gp_unittest import Contains, GpTestCase

from gppylib.recoveryinfo import RecoveryInfo, RecoveryResult


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
        self.default_action_name = 'recover'
        self.actions_to_test = [
            {
                "action_name": "recover",
            },
            {
                "action_name": "add",
            }
        ]

        self.mock_gp_era = Mock(return_value='dummy_era')
        self.apply_patches([
            patch('gppylib.operations.buildMirrorSegments.GpArray.getSegmentsByHostName'),
            patch('gppylib.operations.buildMirrorSegments.gplog.get_logger_dir', return_value='/tmp/logdir'),
            patch('gppylib.operations.buildMirrorSegments.gplog.logging_is_verbose', return_value=False),
            patch('gppylib.operations.buildMirrorSegments.read_era', self.mock_gp_era),
            patch('gppylib.recoveryinfo.RecoveryResult.print_bb_rewind_update_and_start_errors'),
            patch('gppylib.recoveryinfo.RecoveryResult.print_setup_recovery_errors'),
            patch('gppylib.operations.buildMirrorSegments.dbconn')
        ])
        self.mock_dbconn = self.get_mock_from_apply_patch('dbconn')
        self.test_backout_map = {2: ['select gp_remove_segment_mirror(2)', 'select gp_add_segment_mirror(2)'],
                       3: ['select gp_remove_segment_mirror(3)', 'select gp_add_segment_mirror(3)'],
                       4: ['select gp_remove_segment_mirror(4)', 'select gp_add_segment_mirror(4)']}

        self.mock_get_segments_by_hostname = self.get_mock_from_apply_patch('getSegmentsByHostName')
        self.mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])
        self.default_build_mirrors_obj = GpMirrorListToBuild(
            toBuild=Mock(),
            pool=Mock(),
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger
        )
        self.progress_file_dbid2='/tmp/progress_file2'
        self.progress_file_dbid3='/tmp/progress_file3'
        self.progress_file_dbid4='/tmp/progress_file4'

        self.gpEnv = Mock()
        self.gpArray = GpArray([self.coordinator, self.primary, self.mirror])
        self.recovery_info1 = RecoveryInfo('/datadir2', 7001, 2, 'source_host_for_dbid2', 7002, True, self.progress_file_dbid2)
        self.recovery_info2 = RecoveryInfo('/datadir3', 7003, 3, 'source_host_for_dbid3', 7004, True, self.progress_file_dbid3)
        self.recovery_info3 = RecoveryInfo('/datadir4', 7005, 4, 'source_host_for_dbid4', 7006, True, self.progress_file_dbid4)

    def tearDown(self):
        super(BuildMirrorsTestCase, self).tearDown()

    #TODO too many setup functions. clean up to make it obvious
    def _setup_build_mirrors_mocks(self, build_mirrors_obj):
        markdown_mock = Mock()
        build_mirrors_obj._wait_fts_to_mark_down_segments = markdown_mock
        build_mirrors_obj._run_recovery = Mock(return_value=RecoveryResult('action', [], None))
        build_mirrors_obj._run_setup_recovery = Mock(return_value=RecoveryResult('action', [], None))
        build_mirrors_obj._clean_up_failed_segments = Mock()
        build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear = Mock()
        build_mirrors_obj._get_running_postgres_segments = Mock()
        build_mirrors_obj._revert_config_update = Mock()
        # build_mirrors_obj._run_b = Mock()
        build_mirrors_obj._trigger_fts_probe = Mock()
        configurationInterface.getConfigurationProvider = Mock()
        #TODO add test for backout_map
        configurationInterface.getConfigurationProvider.return_value.updateSystemConfig.return_value = {} #mock backout_map

    def _setup_recovery_mocks(self, host_errors=[]):
        host1_recovery_output = gp.GpSegRecovery('test recover 1', 'dummy_info1', '/tmp/log1/', False, 1,
                                                 'host1', 'dummy_era1', True)
        host2_recovery_output = gp.GpSegRecovery('test recover 2', 'dummy_info2', '/tmp/log2/', True, 2,
                                                 'host2', 'dummy_era2', False)

        host1_result = CommandResult(0, ''.encode(), ''.encode(), True, False)
        host2_result = CommandResult(0, ''.encode(), ''.encode(), True, False)
        if host_errors:
            host1_result = CommandResult(1, 'failed 1'.encode(), host_errors[0].encode(), True, False)
            host2_result = CommandResult(1, 'failed 2'.encode(), host_errors[1].encode(), True, False)
        host1_recovery_output.get_results = Mock(return_value=host1_result)
        host2_recovery_output.get_results = Mock(return_value=host2_result)

        completed_items = [host1_recovery_output, host2_recovery_output]
        self.default_build_mirrors_obj._do_setup_for_recovery = Mock(return_value=completed_items)
        self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear = Mock(
            return_value=completed_items)

    def _common_asserts_with_stop_and_logger(self, build_mirrors_obj, expected_logger_msg, expected_segs_to_stop,
                                             expected_segs_to_markdown, expected_segs_to_update, cleanup_count,
                                             action_name, info_call_count=3):
        self.mock_logger.info.assert_any_call(expected_logger_msg)
        #TODO assert all logger info msgs
        self.assertEqual(info_call_count, self.mock_logger.info.call_count)

        self.assertEqual([call(expected_segs_to_stop)], build_mirrors_obj._get_running_postgres_segments.call_args_list)
        self._common_asserts(build_mirrors_obj,  expected_segs_to_markdown, expected_segs_to_update, cleanup_count,
                             action_name)

    def _common_asserts(self, build_mirrors_obj,  expected_segs_to_markdown, expected_segs_to_update, cleanup_count,
                        action_name):
        self.assertEqual([call(self.gpEnv, expected_segs_to_markdown)],
                         build_mirrors_obj._wait_fts_to_mark_down_segments.call_args_list)
        self.assertEqual(cleanup_count, build_mirrors_obj._clean_up_failed_segments.call_count)
        self.assertEqual([call(self.default_action_name, ANY)], build_mirrors_obj._run_setup_recovery.call_args_list)
        self.assertEqual([call(self.default_action_name, ANY, self.gpEnv)], build_mirrors_obj._run_recovery.call_args_list)
        self.assertEqual([call(self.gpArray, ANY, dbIdToForceMirrorRemoveAdd=expected_segs_to_update,
                               useUtilityMode=False, allowPrimary=False)],
                         configurationInterface.getConfigurationProvider.return_value.updateSystemConfig.call_args_list)
        self.assertEqual(1, build_mirrors_obj._trigger_fts_probe.call_count)

        if action_name == 'add':
            self.assertEqual(0, build_mirrors_obj._revert_config_update.call_count)
        elif action_name == 'recover':
            self.assertEqual(1, build_mirrors_obj._revert_config_update.call_count)

    def _assert_setup_recovery(self):
        self.assertEqual(1, RecoveryResult.print_setup_recovery_errors.call_count)
        self.assertEqual(0, RecoveryResult.print_bb_rewind_update_and_start_errors.call_count)
        self.assertEqual([], self.mock_logger.info.call_args_list)
        self.assertEqual([], self.mock_logger.error.call_args_list)

    def _assert_run_recovery(self,):
        expected_info_msgs = [call(Contains('Initiating segment recovery'))]
        self.assertEqual(expected_info_msgs, self.mock_logger.info.call_args_list)
        self.assertEqual([], self.mock_logger.error.call_args_list)
        self.assertEqual(0, RecoveryResult.print_setup_recovery_errors.call_count)
        self.assertEqual(1, RecoveryResult.print_bb_rewind_update_and_start_errors.call_count)

    def _run_buildMirrors(self, mirrors_to_build, action):
        self.mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        build_mirrors_obj = GpMirrorListToBuild(
            toBuild=mirrors_to_build,
            pool=Mock(),
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger
        )
        self._setup_build_mirrors_mocks(build_mirrors_obj)
        action_name = action['action_name']
        self.default_action_name = action_name
        #TODO add test for when buildMirrors returns False
        if action_name == 'add':
            self.assertTrue(build_mirrors_obj.add_mirrors(self.gpEnv, self.gpArray))
        elif action_name == 'recover':
            self.assertTrue(build_mirrors_obj.recover_mirrors(self.gpEnv, self.gpArray))
        return build_mirrors_obj

    def _create_primary(self, dbid='1', contentid='0', state='n', status='u', host='sdw1', unreachable=False):
        seg = Segment.initFromString('{}|{}|p|p|{}|{}|{}|{}|21000|/primary/gpseg0'
                                     .format(dbid, contentid, state, status, host, host))
        seg.unreachable = unreachable
        return seg

    def _create_mirror(self, dbid='2', contentid='0', state='n', status='u', host='sdw2'):
        return Segment.initFromString('{}|{}|p|p|{}|{}|{}|{}|22000|/mirror/gpseg0'
                                      .format(dbid, contentid, state, status, host, host))

    def test_recover_mirrors_noMirrors(self):
        build_mirrors_obj = GpMirrorListToBuild(
            toBuild=[],
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger
        )
        self.assertTrue(build_mirrors_obj.recover_mirrors(None, None))

        self.assertEqual([call('No segments to recover')], self.mock_logger.info.call_args_list)

    def test_add_mirrors_noMirrors(self):
        build_mirrors_obj = GpMirrorListToBuild(
            toBuild=[],
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger
        )
        self.assertTrue(build_mirrors_obj.add_mirrors(None, None))

        self.assertEqual([call('No segments to add')], self.mock_logger.info.call_args_list)

    def test_add_recover_mirrors_no_failed_pass(self):
        for action in self.actions_to_test:
            with self.subTest(msg=action["action_name"]):
                tests = [
                    {
                        "name": "no_failed_full",
                        "live": self._create_primary(),
                        "failover": self._create_mirror(host='sdw2'),
                        "forceFull": False,
                    },
                    {
                        "name": "no_failed_full2",
                        "live": self._create_primary(dbid='3'),
                        "failover": self._create_mirror(dbid='4'),
                        "forceFull": True,
                    }
                ]

                mirrors_to_build = []
                for test in tests:
                    mirrors_to_build.append(GpMirrorToBuild(None, test["live"], test["failover"], test["forceFull"]))
                build_mirrors_obj = self._run_buildMirrors(mirrors_to_build, action)

                self.assertEqual(2, self.mock_logger.info.call_count)
                self.assertEqual(0, build_mirrors_obj._get_running_postgres_segments.call_count)
                self._common_asserts(build_mirrors_obj, [], {2: True, 4: True}, cleanup_count=1, action_name=action["action_name"])

                for test in tests:
                    self.assertEqual('n', test['live'].getSegmentMode())
                    self.assertEqual('d', test['failover'].getSegmentStatus())
                    self.assertEqual('n', test['failover'].getSegmentMode())

    def test_add_recover_mirrors_no_failover_pass(self):
        for action in self.actions_to_test:
            with self.subTest(msg=action["action_name"]):
                tests = [
                    {
                        "name": "no_failover",
                        "failed": self._create_mirror(status='d'),
                        "live": self._create_primary(),
                        "forceFull": False,
                    },
                    {
                        "name": "no_failover_full",
                        "failed": self._create_mirror(status='d', dbid='4', host='sdw2'),
                        "live": self._create_primary(),
                        "forceFull": True,
                    },
                    {
                        "name": "no_failover_failed_seg_exists_in_gparray",
                        "failed": self.mirror,
                        "live": self._create_primary(),
                        "forceFull": True,
                        "forceoverwrite": True
                    },
                    {
                        "name": "no_failover_failed_segment_is_up",
                        "failed": self._create_mirror(dbid='5'),
                        "live": self._create_primary(dbid='6', host='sdw2'),
                        "is_failed_segment_up": True,
                        "forceFull": False,
                    }
                ]
                mirrors_to_build = []
                expected_segs_to_stop = []
                expected_segs_to_markdown = []
                for test in tests:
                    mirrors_to_build.append(GpMirrorToBuild(test["failed"], test["live"], None,
                                                            test["forceFull"]))
                    expected_segs_to_stop.append(test["failed"])
                    if 'is_failed_segment_up' in test and test["is_failed_segment_up"]:
                        expected_segs_to_markdown.append(test['failed'])

                build_mirrors_obj = self._run_buildMirrors(mirrors_to_build, action)

                self._common_asserts_with_stop_and_logger(build_mirrors_obj, "Ensuring 4 failed segment(s) are stopped",
                                                          expected_segs_to_stop,
                                                          expected_segs_to_markdown, {4: True, 30: True}, 1,
                                                          action["action_name"])
                for test in tests:
                    self.assertEqual('n', test['live'].getSegmentMode())
                    self.assertEqual('d', test['failed'].getSegmentStatus())
                    self.assertEqual('n', test['failed'].getSegmentMode())

    def test_add_recover_mirrors_both_failed_failover_pass(self):
        for action in self.actions_to_test:
            with self.subTest(msg=action["action_name"]):
                tests = [
                    {
                        "name": "both_failed_failover_full",
                        "failed": self._create_primary(status='d'),
                        "live": self._create_mirror(),
                        "failover": self._create_primary(status='d', host='sdw3'),
                        "forceFull": True
                    },
                    {
                        "name": "both_failed_failover_failed_segment_is_up",
                        "failed": self._create_primary(dbid='5'),
                        "live": self._create_mirror(dbid='6'),
                        "failover": self._create_primary(dbid='5', host='sdw3'),
                        "is_failed_segment_up": True,
                        "forceFull": True
                    },
                    {
                        "name": "both_failed_failover_failover_is_down_live_is_marked_as_sync",
                        "failed": self._create_primary(dbid='9', status='d'),
                        "live": self._create_mirror(dbid='10', state='s'),
                        "failover": self._create_primary(dbid='9', status='d'),
                        "forceFull": False,
                    },
                    {
                        "name": "both_failed_failover_failed_is_unreachable",
                        "failed": self._create_primary(dbid='9', status='d', unreachable=True),
                        "live": self._create_mirror(dbid='10', state='s'),
                        "failover": self._create_primary(dbid='9', status='d'),
                        "forceFull": False,
                    },

                ]
                mirrors_to_build = []
                expected_segs_to_stop = []
                expected_segs_to_markdown = []

                for test in tests:
                    mirrors_to_build.append(GpMirrorToBuild(test["failed"], test["live"], test["failover"],
                                                            test["forceFull"]))
                    #TODO better way to check for this condition
                    if not test["failed"].unreachable:
                        expected_segs_to_stop.append(test["failed"])
                    if 'is_failed_segment_up' in test and test["is_failed_segment_up"]:
                        expected_segs_to_markdown.append(test['failed'])

                build_mirrors_obj = self._run_buildMirrors(mirrors_to_build, action)

                # TODO improve the logic that passes info_call_count
                self._common_asserts_with_stop_and_logger(build_mirrors_obj, "Ensuring 3 failed segment(s) are stopped",
                                                          expected_segs_to_stop,
                                                          expected_segs_to_markdown, {1: True, 5: True, 9: True}, 1,
                                                          action["action_name"], info_call_count=4)

                for test in tests:
                    self.assertEqual('n', test['live'].getSegmentMode())
                    self.assertEqual('d', test['failover'].getSegmentStatus())
                    self.assertEqual('n', test['failover'].getSegmentMode())

    def test_add_recover_mirrors_forceoverwrite_true(self):
        for action in self.actions_to_test:
            with self.subTest(msg=action["action_name"]):
                self.mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])
                action_name = action["action_name"]
                self.default_action_name = action_name
                failed = self._create_primary(status='d')
                live = self._create_mirror()
                failover = self._create_primary(host='sdw3')

                build_mirrors_obj = GpMirrorListToBuild(
                    toBuild=[GpMirrorToBuild(failed, live, failover, False)],
                    pool=None,
                    quiet=True,
                    parallelDegree=0,
                    logger=self.mock_logger,
                    forceoverwrite=True
                )
                self._setup_build_mirrors_mocks(build_mirrors_obj)

                if action_name == 'add':
                    self.assertTrue(build_mirrors_obj.add_mirrors(self.gpEnv, self.gpArray))
                elif action_name == 'recover':
                    self.assertTrue(build_mirrors_obj.recover_mirrors(self.gpEnv, self.gpArray))

                self._common_asserts_with_stop_and_logger(build_mirrors_obj, "Ensuring 1 failed segment(s) are stopped",
                                                          [failed], [], {1: True}, 0, action["action_name"])
                self.assertEqual('n', live.getSegmentMode())
                self.assertEqual('d', failover.getSegmentStatus())
                self.assertEqual('n', failover.getSegmentMode())

    def test_recover_mirrors_failed_seg_in_gparray_fail(self):
        tests = [
            {
                "name": "failed_seg_exists_in_gparray1",
                "failed": self._create_primary(status='d'),
                "failover": self._create_primary(status='d'),
                "live": self._create_mirror(),
                "forceFull": True,
                "forceoverwrite": False
            },
            {
                "name": "failed_seg_exists_in_gparray2",
                "failed": self._create_primary(dbid='3', status='d'),
                "failover": self._create_primary(dbid='3', status='d'),
                "live": self._create_mirror(dbid='4'),
                "forceFull": False,
                "forceoverwrite": False
            },
            {
                "name": "failed_seg_exists_in_gparray2",
                "failed": self._create_primary(dbid='3', status='d'),
                "failover": self._create_primary(dbid='3', status='d'),
                "live": self._create_mirror(dbid='4'),
                "forceFull": False,
                "forceoverwrite": True
            }
        ]
        for test in tests:
            mirror_to_build = GpMirrorToBuild(test["failed"], test["live"], test["failover"], test["forceFull"])
            build_mirrors_obj = GpMirrorListToBuild(
                toBuild=[mirror_to_build,],
                pool=None,
                quiet=True,
                parallelDegree=0,
                logger=self.mock_logger,
                forceoverwrite=test['forceoverwrite']
            )
            self._setup_build_mirrors_mocks(build_mirrors_obj)
            local_gp_array = GpArray([self.coordinator, test["failed"]])
            expected_error = "failed segment should not be in the new configuration if failing over to"
            with self.subTest(msg=test["name"]):
                with self.assertRaisesRegex(Exception, expected_error):
                    build_mirrors_obj.recover_mirrors(self.gpEnv, local_gp_array)

    def test_clean_up_failed_segments(self):
        failed1 = self._create_primary(status='d')
        live1 = self._create_mirror()

        failed2 = self._create_primary(dbid='3', status='d')
        failover2 = self._create_primary(dbid='3', status='d')
        live2 = self._create_mirror(dbid='4')

        failed3 = self._create_primary(dbid='5')
        live3 = self._create_mirror(dbid='6')

        failed4 = self._create_primary(dbid='5')
        live4 = self._create_mirror(dbid='7')

        inplace_full1 = GpMirrorToBuild(failed1, live1, None, True)
        not_inplace_full = GpMirrorToBuild(failed2, live2, failover2, True)
        inplace_full2 = GpMirrorToBuild(failed3, live3, None, True)
        inplace_not_full = GpMirrorToBuild(failed4, live4, None, False)

        build_mirrors_obj = GpMirrorListToBuild(
            toBuild=[inplace_full1, not_inplace_full, inplace_full2, inplace_not_full],
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger,
            forceoverwrite=True
        )
        build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear = Mock()

        build_mirrors_obj._clean_up_failed_segments()

        self.mock_get_segments_by_hostname.assert_called_once_with([failed1, failed3])
        self.mock_logger.info.called_once_with('"Cleaning files from 2 segment(s)')

    def test_clean_up_failed_segments_no_segs_to_cleanup(self):
        failed2 = self._create_primary(dbid='3', status='d')
        failover2 = self._create_primary(dbid='3', status='d')
        live2 = self._create_mirror(dbid='4')

        failed4 = self._create_primary(dbid='5')
        live4 = self._create_mirror(dbid='7')

        not_inplace_full = GpMirrorToBuild(failed2, live2, failover2, True)
        inplace_not_full = GpMirrorToBuild(failed4, live4, None, False)

        build_mirrors_obj = GpMirrorListToBuild(
            toBuild=[not_inplace_full, inplace_not_full],
            pool=None,
            quiet=True,
            parallelDegree=0,
            logger=self.mock_logger,
            forceoverwrite=True
        )
        build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear = Mock()

        build_mirrors_obj._clean_up_failed_segments()
        self.assertEqual(0, self.mock_get_segments_by_hostname.call_count)
        self.assertEqual(0, self.mock_logger.info.call_count)

    def test_run_setup_recovery_errors(self):
        host1_error = '[{"error_type": "validation", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                      '{"error_type": "validation", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]'
        host2_error = '[{"error_type": "validation", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'
        self._setup_recovery_mocks([host1_error, host2_error])
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            self.default_build_mirrors_obj._run_setup_recovery(self.default_action_name, None)

        self._assert_setup_recovery()

    def test_run_setup_recovery_invalid_errors(self):
        host1_error = 'invalid error'
        host2_error = '[{"error_type": "validation", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'
        self._setup_recovery_mocks([host1_error, host2_error])
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            self.default_build_mirrors_obj._run_setup_recovery(self.default_action_name, None)

        self._assert_setup_recovery()

    def test_run_setup_recovery_empty_errors(self):
        self._setup_recovery_mocks(['', ''])
        with self.assertRaises(ExceptionNoStackTraceNeeded):
            self.default_build_mirrors_obj._run_setup_recovery(self.default_action_name, None)

        self._assert_setup_recovery()

    def test_run_recovery_no_errors(self):
        self._setup_recovery_mocks()

        recovery_info = OrderedDict()  # We use ordered dict to deterministically assert the side effects of _run_recovery
        recovery_info['host1'] = [self.recovery_info1]
        recovery_info['host2'] = [self.recovery_info2, self.recovery_info3]

        self.default_build_mirrors_obj._run_recovery(self.default_action_name, recovery_info, self.gpEnv)

        self.assertEqual([call(ANY, progressCmds=ANY, suppressErrorCheck=True), call(ANY, suppressErrorCheck=False)],
                         self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list)

        seg_recovery_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[0][0][0]
        progress_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[0][1]['progressCmds']
        rm_progress_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[1][0][0]

        #TODO fix formatting
        expected_recovery_cmd_strs = ["""$GPHOME/sbin/gpsegrecovery.py -c '[{"target_datadir": "/datadir2", "target_port": 7001, "target_segment_dbid": 2, "source_hostname": "source_host_for_dbid2", "source_port": 7002, "is_full_recovery": true, "progress_file": "/tmp/progress_file2"}]' -l /tmp/logdir -b 64 --era=dummy_era""", """$GPHOME/sbin/gpsegrecovery.py -c '[{"target_datadir": "/datadir3", "target_port": 7003, "target_segment_dbid": 3, "source_hostname": "source_host_for_dbid3", "source_port": 7004, "is_full_recovery": true, "progress_file": "/tmp/progress_file3"}, {"target_datadir": "/datadir4", "target_port": 7005, "target_segment_dbid": 4, "source_hostname": "source_host_for_dbid4", "source_port": 7006, "is_full_recovery": true, "progress_file": "/tmp/progress_file4"}]' -l /tmp/logdir -b 64 --era=dummy_era"""]
        self.assertEqual(2, len(seg_recovery_cmds))
        self.assertEqual('host1', seg_recovery_cmds[0].remoteHost)
        self.assertEqual(sorted(expected_recovery_cmd_strs[0]), sorted(seg_recovery_cmds[0].cmdStr))
        self.assertEqual('host2', seg_recovery_cmds[1].remoteHost)
        self.assertEqual(sorted(expected_recovery_cmd_strs[1]), sorted(seg_recovery_cmds[1].cmdStr))

        self.assertEqual(3, len(progress_cmds))
        self.assertEqual('host1', progress_cmds[0].remoteHost)
        self.assertEqual("set -o pipefail; touch -a /tmp/progress_file2; tail -1 /tmp/progress_file2 | tr '\\r' '\\n' | tail -1",
                         progress_cmds[0].cmdStr)
        self.assertEqual('host2', progress_cmds[1].remoteHost)
        self.assertEqual("set -o pipefail; touch -a /tmp/progress_file3; tail -1 /tmp/progress_file3 | tr '\\r' '\\n' | tail -1",
                         progress_cmds[1].cmdStr)
        self.assertEqual('host2', progress_cmds[2].remoteHost)
        self.assertEqual("set -o pipefail; touch -a /tmp/progress_file4; tail -1 /tmp/progress_file4 | tr '\\r' '\\n' | tail -1",
                         progress_cmds[2].cmdStr)
        self.assertEqual(1, self.mock_gp_era.call_count)

        self.assertEqual(3, len(rm_progress_cmds))
        self.assertEqual("host1", rm_progress_cmds[0].remoteHost)
        self.assertEqual("rm -f /tmp/progress_file2", rm_progress_cmds[0].cmdStr)
        self.assertEqual("host2", rm_progress_cmds[1].remoteHost)
        self.assertEqual("rm -f /tmp/progress_file3", rm_progress_cmds[1].cmdStr)
        self.assertEqual("host2", rm_progress_cmds[2].remoteHost)
        self.assertEqual("rm -f /tmp/progress_file4", rm_progress_cmds[2].cmdStr)

        self._assert_run_recovery()

    def test_run_recovery_invalid_errors(self):
        recovery_info = {'host1': [self.recovery_info1, self.recovery_info2], 'host2': [self.recovery_info3]}
        host1_error = 'invalid error1'
        host2_error = ''
        self._setup_recovery_mocks([host1_error, host2_error])
        self.default_build_mirrors_obj._run_recovery(self.default_action_name, recovery_info, self.gpEnv)
        self._assert_run_recovery()

    def test_run_recovery_all_dbids_fail_only_bb_rewind_errors(self):
        recovery_info = {'host1': [self.recovery_info1, self.recovery_info2], 'host2': [self.recovery_info3]}
        host1_error = '[{"error_type": "full", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                      '{"error_type": "incremental", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]'
        host2_error = '[{"error_type": "full", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'
        self._setup_recovery_mocks([host1_error, host2_error])

        self.default_build_mirrors_obj._run_recovery(self.default_action_name, recovery_info, self.gpEnv)
        self.assertEqual([call(ANY, progressCmds=ANY, suppressErrorCheck=True), call(ANY, suppressErrorCheck=False)],
                         self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list)
        rm_progress_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[1][0][0]

        self.assertEqual(0, len(rm_progress_cmds))

        self._assert_run_recovery()

    def test_run_recovery_some_dbids_fail_all_bb_rewind_errors(self):
        recovery_info = {'host1': [self.recovery_info1], 'host2': [self.recovery_info2, self.recovery_info3]}
        host1_error = '[{"error_type": "full", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}]'
        host2_error = '[{"error_type": "full", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'
        self._setup_recovery_mocks([host1_error, host2_error])

        self.default_build_mirrors_obj._run_recovery(self.default_action_name, recovery_info, self.gpEnv)
        self.assertEqual([call(ANY, progressCmds=ANY, suppressErrorCheck=True), call(ANY, suppressErrorCheck=False)],
                         self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list)
        rm_progress_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[1][0][0]

        self.assertEqual(1, len(rm_progress_cmds))
        self.assertEqual("host2", rm_progress_cmds[0].remoteHost)
        self.assertEqual("rm -f /tmp/progress_file3", rm_progress_cmds[0].cmdStr)

        self._assert_run_recovery()

    def test_run_recovery_all_dbids_fail_all_start_errors(self):
        recovery_info = {'host1': [self.recovery_info1], 'host2': [self.recovery_info2, self.recovery_info3]}
        host1_error = '[{"error_type": "start", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                      '{"error_type": "start", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]'
        host2_error = '[{"error_type": "start", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'
        self._setup_recovery_mocks([host1_error, host2_error])
        self.default_build_mirrors_obj._run_recovery(self.default_action_name, recovery_info, self.gpEnv)
        self.assertEqual([call(ANY, progressCmds=ANY, suppressErrorCheck=True), call(ANY, suppressErrorCheck=False)],
                         self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list)
        rm_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[1][0][0]

        self.assertEqual(3, len(rm_cmds))
        self._assert_run_recovery()

    def test_run_recovery_some_dbids_fail_all_start_errors(self):
        recovery_info = {'host1': [self.recovery_info1], 'host2': [self.recovery_info2, self.recovery_info3]}
        host1_error = '[{"error_type": "start", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}]'
        host2_error = '[{"error_type": "start", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'

        self._setup_recovery_mocks([host1_error, host2_error])

        self.default_build_mirrors_obj._run_recovery(self.default_action_name, recovery_info, self.gpEnv)
        self.assertEqual([call(ANY, progressCmds=ANY, suppressErrorCheck=True), call(ANY, suppressErrorCheck=False)],
                         self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list)
        rm_progress_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[1][0][0]
        self.assertEqual(3, len(rm_progress_cmds))

        self._assert_run_recovery()

    def test_run_recovery_all_dbids_fail_bb_rewind_and_start_errors(self):
        recovery_info = {'host1': [self.recovery_info1, self.recovery_info2], 'host2': [self.recovery_info3]}
        host1_error = '[{"error_type": "full",  "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                      '{"error_type": "start",  "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]'
        host2_error = '[{"error_type": "full",  "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'
        self._setup_recovery_mocks([host1_error, host2_error])

        self.default_build_mirrors_obj._run_recovery(self.default_action_name, recovery_info, self.gpEnv)

        self.assertEqual([call(ANY, progressCmds=ANY, suppressErrorCheck=True), call(ANY, suppressErrorCheck=False)],
                         self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list)
        rm_progress_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[1][0][0]

        # Since start passed for dbid:3, we should have deleted it's progress file
        self.assertEqual(1, len(rm_progress_cmds))
        self.assertEqual("host1", rm_progress_cmds[0].remoteHost)
        self.assertEqual("rm -f /tmp/progress_file3", rm_progress_cmds[0].cmdStr)

        self._assert_run_recovery()

    def test_run_recovery_all_dbids_fail_bb_rewind_and_default_errors(self):
        recovery_info = OrderedDict()  # We use ordered dict to deterministically assert the side effects of _run_recovery
        recovery_info['host1'] = [self.recovery_info1]
        recovery_info['host2'] = [self.recovery_info2, self.recovery_info3]

        # recovery_info = OrderedDict({'host1': [self.recovery_info1], 'host2': [self.recovery_info2, self.recovery_info3]})
        host1_error = '[{"error_type": "start",  "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                      '{"error_type": "default",  "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]'
        host2_error = '[{"error_type": "incremental",  "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'
        self._setup_recovery_mocks([host1_error, host2_error])

        self.default_build_mirrors_obj._run_recovery(self.default_action_name, recovery_info, self.gpEnv)

        self.assertEqual([call(ANY, progressCmds=ANY, suppressErrorCheck=True), call(ANY, suppressErrorCheck=False)],
                         self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list)
        rm_progress_cmds = self.default_build_mirrors_obj._GpMirrorListToBuild__runWaitAndCheckWorkerPoolForErrorsAndClear.call_args_list[1][0][0]

        self.assertEqual(2, len(rm_progress_cmds))
        self.assertEqual("host1", rm_progress_cmds[0].remoteHost)
        self.assertEqual("host2", rm_progress_cmds[1].remoteHost)
        self.assertEqual("rm -f /tmp/progress_file2", rm_progress_cmds[0].cmdStr)
        self.assertEqual("rm -f /tmp/progress_file3", rm_progress_cmds[1].cmdStr)

        self._assert_run_recovery()

    def test_run_backout_some_dbids_have_errors(self):
        host1_error = '[{"error_type": "full", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                      '{"error_type": "incremental", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]'
        host1_result = CommandResult(1, 'failed 1'.encode(), host1_error.encode(), True, False)
        host2_result = CommandResult(0, b"", b"", True, False)
        host1_recovery_output = Mock()
        host2_recovery_output = Mock()
        host1_recovery_output.get_results = Mock(return_value=host1_result)
        host2_recovery_output.get_results = Mock(return_value=host2_result)

        recovery_results = RecoveryResult(self.default_action_name, [host1_recovery_output, host2_recovery_output],
                                          self.mock_logger)

        self.default_build_mirrors_obj._revert_config_update(recovery_results, self.test_backout_map)

        self.assertEqual([call(Contains('Some mirrors failed during basebackup')),
                          call(Contains('Successfully reverted the gp_segment_configuration updates'))],
                         self.mock_logger.debug.call_args_list)
        self.assertEqual([call(ANY, "BEGIN"), call(ANY, "COMMIT")], self.mock_dbconn.execSQL.call_args_list)
        expected_sql = 'SET allow_system_table_mods=true;\n' \
                       'select gp_remove_segment_mirror(2);\n' \
                       'select gp_add_segment_mirror(2);\n' \
                       'select gp_remove_segment_mirror(3);\n' \
                       'select gp_add_segment_mirror(3);\n'
        self.assertEqual([call(ANY, expected_sql, 1)], self.mock_dbconn.executeUpdateOrInsert.call_args_list)

    def test_run_backout_all_dbids_have_errors(self):
        host1_error = '[{"error_type": "full", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                      '{"error_type": "incremental", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]'
        host2_error = '[{"error_type": "full", "error_msg":"some error for dbid 4", "dbid": 4, "datadir": "/datadir4", "port": 7005, "progress_file": "/tmp/progress4"}]'

        host1_result = CommandResult(1, 'failed 1'.encode(), host1_error.encode(), True, False)
        host2_result = CommandResult(1, 'failed 2'.encode(), host2_error.encode(), True, False)
        host1_recovery_output = Mock()
        host2_recovery_output = Mock()
        host1_recovery_output.get_results = Mock(return_value=host1_result)
        host2_recovery_output.get_results = Mock(return_value=host2_result)

        recovery_results = RecoveryResult(self.default_action_name, [host1_recovery_output, host2_recovery_output],
                                          self.mock_logger)

        self.default_build_mirrors_obj._revert_config_update(recovery_results, self.test_backout_map)

        self.assertEqual([call(Contains('Some mirrors failed during basebackup')),
                          call(Contains('Successfully reverted the gp_segment_configuration updates'))],
                         self.mock_logger.debug.call_args_list)
        self.assertEqual([call(ANY, "BEGIN"), call(ANY, "COMMIT")], self.mock_dbconn.execSQL.call_args_list)
        expected_sql = 'SET allow_system_table_mods=true;\n' \
                       'select gp_remove_segment_mirror(2);\n' \
                       'select gp_add_segment_mirror(2);\n' \
                       'select gp_remove_segment_mirror(3);\n' \
                       'select gp_add_segment_mirror(3);\n' \
                       'select gp_remove_segment_mirror(4);\n' \
                       'select gp_add_segment_mirror(4);\n'
        self.assertEqual([call(ANY, expected_sql, 1)], self.mock_dbconn.executeUpdateOrInsert.call_args_list)

    def test_run_backout_all_dbids_have_incremental_errors(self):
        host1_error = '[{"error_type": "incremental", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2", "port": 7001, "progress_file": "/tmp/progress2"}, ' \
                      '{"error_type": "incremental", "error_msg":"some error for dbid 3", "dbid": 3, "datadir": "/datadir3", "port": 7003, "progress_file": "/tmp/progress3"}]'

        host1_result = CommandResult(1, 'failed 1'.encode(), host1_error.encode(), True, False)
        host2_result = CommandResult(0, b"", b"", True, False)
        host1_recovery_output = Mock()
        host2_recovery_output = Mock()
        host1_recovery_output.get_results = Mock(return_value=host1_result)
        host2_recovery_output.get_results = Mock(return_value=host2_result)

        recovery_results = RecoveryResult(self.default_action_name, [host1_recovery_output, host2_recovery_output],
                                          self.mock_logger)
        self.default_build_mirrors_obj._revert_config_update(recovery_results, self.test_backout_map)

        self.assertEqual(0, self.mock_logger.debug.call_count)
        self.assertEqual(0, self.mock_dbconn.execSQL.call_count)
        self.assertEqual(0, self.mock_dbconn.executeUpdateOrInsert.call_count)

    def test_run_backout_no_errors(self):
        host1_recovery_output = Mock()
        host2_recovery_output = Mock()
        host1_recovery_output.get_results = Mock(return_value=CommandResult(0, b"", b"", True, False))
        host2_recovery_output.get_results = Mock(return_value=CommandResult(0, b"", b"", True, False))

        recovery_results = RecoveryResult(self.default_action_name, [host1_recovery_output, host2_recovery_output],
                                          self.mock_logger)
        self.default_build_mirrors_obj._revert_config_update(recovery_results, self.test_backout_map)

        self.assertEqual(0, self.mock_logger.debug.call_count)
        self.assertEqual(0, self.mock_dbconn.execSQL.call_count)
        self.assertEqual(0, self.mock_dbconn.executeUpdateOrInsert.call_count)

    def test_run_backout_exception(self):
        self.mock_dbconn.executeUpdateOrInsert.side_effect = Exception('running backout query failed')
        host1_error = """[{"error_type": "full", "error_msg":"some error for dbid 2", "dbid": 2, "datadir": "/datadir2",
                            "port": 7001, "progress_file": "/tmp/progress2"}]"""

        host1_recovery_output = Mock()
        host1_recovery_output.get_results = Mock(return_value=CommandResult(1, 'failed 1'.encode(), host1_error.encode(),
                                                                            True, False))

        recovery_results = RecoveryResult(self.default_action_name, [host1_recovery_output],
                                          self.mock_logger)

        with self.assertRaisesRegex(Exception, "running backout query failed"):
            self.default_build_mirrors_obj._revert_config_update(recovery_results, self.test_backout_map)


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

    # def test_run_recovery(self):
    #     self.buildMirrorSegs = GpMirrorListToBuild(
    #         toBuild = [],
    #         pool = None,
    #         quiet = True,
    #         parallelDegree = 0,
    #         logger=self.mock_logger
    #     )
    #     self.buildMirrorSegs._run_recovery(Mock())
    #     pass

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
        self.tmp_log_dir = tempfile.mkdtemp()
        self.apply_patches([
            patch('recoveryinfo.gplog.get_logger_dir', return_value=self.tmp_log_dir),
            patch('gppylib.operations.buildMirrorSegments.os.remove')
        ])
        self.mock_os_remove = self.get_mock_from_apply_patch("remove")
        self.combined_progress_file = "{}/recovery_progress.file".format(self.tmp_log_dir)

    def tearDown(self):
        super(SegmentProgressTestCase, self).tearDown()
        shutil.rmtree(self.tmp_log_dir)

    def create_command(self, remoteHost, dbid, filepath, stdout=None, stderr=None):
            cmd = Mock(spec=GpMirrorListToBuild.ProgressCommand)
            cmd.remoteHost = remoteHost
            cmd.dbid = dbid
            cmd.filePath = filepath
            cmd.get_results.return_value.stdout = stdout
            cmd.get_results.return_value.stderr = stderr
            return cmd

    def test_command_output_is_displayed_once_after_worker_pool_completes(self):
        cmd1 = self.create_command('localhost', 2, './pg_basebackup.23432', "string 1\n")
        cmd2 = self.create_command('host2', 4, './pg_basebackup.234324', "string 2\n")

        outfile = io.StringIO()
        self.pool.join.return_value = True
        self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): string 1\n'
            'host2 (dbid 4): string 2\n'
        ))

    def test_command_output_is_displayed_once_for_every_blocked_join(self):
        cmd = self.create_command('localhost', 2, './pg_basebackup.23432', "string 1\n")

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
        cmd1 = self.create_command('localhost', 2, './pg_basebackup.234324', "string 1\n")
        cmd2 = self.create_command('host2', 4, './pg_basebackup.234324', "string 2\n")

        cmd1.get_results.side_effect = [Mock(stdout="string 1"), Mock(stdout="string 2")]

        cmd2.get_results.side_effect = [Mock(stdout="string 3"), Mock(stdout="string 4")]

        outfile = io.StringIO()
        self.pool.join.side_effect = [False, True]
        self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], inplace=True, outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): string 1\x1b[K\n'
            'host2 (dbid 4): string 3\x1b[K\n'
            '\x1b[2A'
            'localhost (dbid 2): string 2\x1b[K\n'
            'host2 (dbid 4): string 4\x1b[K\n'
        ))

    def test_errors_during_command_execution_are_displayed(self):
        cmd1 = self.create_command('localhost', 2, './pg_basebackup.234324', stderr="some error\n")
        cmd2 = self.create_command('host2', 4, './pg_basebackup.234324', stderr='')

        cmd1.run.side_effect = base.ExecutionError("Some exception", cmd1)
        cmd2.run.side_effect = base.ExecutionError("Some exception", cmd2)

        outfile = io.StringIO()
        self.pool.join.return_value = True
        self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], outfile=outfile)

        results = outfile.getvalue()
        self.assertEqual(results, (
            'localhost (dbid 2): some error\n'
            'host2 (dbid 4): \n'
        ))

    def test_successful_command_execution_should_delete_the_recovery_progress_file(self):
        cmd1 = self.create_command('host1', 1, './pg_basebackup.23432', "1164848/1371715 kB (84%)\n")
        cmd2 = self.create_command('host2', 2, './pg_rewind.23432', "1164858/1371715 kB (90%)\n")


        outfile = io.StringIO()
        self.pool.join.return_value = True
        self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], outfile=outfile)
        self.mock_os_remove.assert_called_once_with(self.combined_progress_file)

    def test_error_during_command_execution_should_delete_the_recovery_progress_file(self):
        cmd1 = self.create_command('host1', 1, './pg_basebackup.23432', stderr="some error\n")
        cmd2 = self.create_command('host2', 2, './pg_rewind.23432', stderr='')

        cmd1.run.side_effect = Exception("Some exception1")
        cmd2.run.side_effect = Exception("Some exception2")


        outfile = io.StringIO()
        self.pool.join.return_value = True
        with self.assertRaisesRegex(Exception, "Some exception1"):
            self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], outfile=outfile)
        self.mock_os_remove.assert_called_once_with(self.combined_progress_file)

    def test_join_and_show_segment_progress_writes_progress_file(self):

        cmd1 = self.create_command('host1', 1, './pg_basebackup.23432', "1164848/1371715 kB (84%)\n")
        cmd2 = self.create_command('host2', 2, './pg_rewind.23432', "1164858/1371715 kB (90%)\n")

        outfile = io.StringIO()
        self.pool.join.return_value = True

        self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], outfile=outfile)

        self.mock_os_remove.assert_called_once_with(self.combined_progress_file)

        with open(self.combined_progress_file, 'r') as f:
            results = f.readlines()
        self.assertEqual(results, [
            'full:1:1164848/1371715 kB (84%)\n',
            'incremental:2:1164858/1371715 kB (90%)\n'
        ])

    def test_join_and_show_segment_progress_overwrites(self):
        cmd1 = self.create_command('host1', 1, './pg_basebackup.23432', "1164848/1371715 kB (84%)\n")
        cmd2 = self.create_command('host2', 2, './pg_rewind.23432', "1164858/1371715 kB (90%)\n")

        outfile = io.StringIO()
        self.pool.join.return_value = True

        self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], outfile=outfile)

        self.mock_os_remove.assert_called_once_with(self.combined_progress_file)

        with open(self.combined_progress_file, 'r') as f:
            results = f.readlines()
        self.assertEqual(results, [
            'full:1:1164848/1371715 kB (84%)\n',
            'incremental:2:1164858/1371715 kB (90%)\n'
        ])

        cmd1 = self.create_command('host11', 11, './pg_rewind.23432', "9964858/9971715 kB (1%)\n")
        cmd2 = self.create_command('host22', 22, './pg_basebackup.23432', "9964848/9971715 kB (45%)\n")
        self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], outfile=outfile)

        with open(self.combined_progress_file, 'r') as f:
            results = f.readlines()
            self.assertEqual(results, [
                'incremental:11:9964858/9971715 kB (1%)\n',
                'full:22:9964848/9971715 kB (45%)\n'
            ])

    def test_verify_recovery_progress_file_when_command_results_are_not_in_expected_format(self):
            cmd1 = self.create_command('host1', 1, './pg_basebackup.23432', "string 1\n")
            cmd2 = self.create_command('host2', 2, './pg_basebackup.23432', "string 2\n")


            outfile = io.StringIO()
            self.pool.join.return_value = True
            self.buildMirrorSegs._join_and_show_segment_progress([cmd1, cmd2], outfile=outfile)

            self.mock_os_remove.assert_called_once_with(self.combined_progress_file)

            with open(self.combined_progress_file, 'r') as f:
                results = f.readlines()
            self.assertEqual(results, [])

if __name__ == '__main__':
    run_tests()
