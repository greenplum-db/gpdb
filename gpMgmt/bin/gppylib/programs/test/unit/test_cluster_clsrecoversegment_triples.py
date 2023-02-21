#!/usr/bin/env python3

import copy
import io
import itertools

from mock import Mock, patch, MagicMock, call
import tempfile

import gppylib
from gparray import Segment, GpArray
from gppylib.programs.clsRecoverSegment_triples import RecoveryTripletsUserConfigFile, RecoveryTripletsFactory, \
    RecoveryTriplet, get_segments_with_running_basebackup, is_pg_rewind_running
from test.unit.gp_unittest import GpTestCase, FakeCursor


class RecoveryTripletsFactoryTestCase(GpTestCase):
    def setUp(self):
        # Set maxDiff to None to see the entire diff on the console in case of failure
        self.maxDiff = None
        mock_logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        self.apply_patches([
            patch('gppylib.programs.clsRecoverSegment_triples.logger', return_value=mock_logger),
        ])

        self.mock_logger = self.get_mock_from_apply_patch('logger')

    def run_single_ConfigFile_test(self, test):
        with tempfile.NamedTemporaryFile() as f:
            f.write(test["config"].encode("utf-8"))
            f.flush()
            return self._run_single_FromGpArray_test(test["gparray"], f.name, None, test.get("unreachable_hosts"),
                                                     test.get("is_pgrewind_running", itertools.repeat(False)),
                                                     test.get("segments_with_running_basebackup", set()),
                                                     test.get("unreachable_existing_hosts"))

    def run_single_GpArray_test(self, test):
        return self._run_single_FromGpArray_test(test["gparray"], None, test["new_hosts"],
                                                 test.get("unreachable_hosts"),
                                                 test.get("is_pgrewind_running", itertools.repeat(False)),
                                                 test.get("segments_with_running_basebackup", set()),
                                                 test.get("unreachable_existing_hosts"))

    # TODO: do we want new hosts here?  We do not officially support new hosts with "-i"
    def test_RecoveryTripletsUserConfigFile_getMirrorTriples_should_pass(self):
        tests = [
            {
                "name": "blank_config_file",
                "gparray": self.all_up_gparray_str,
                "config": "",
                "expected": [],
            },
            {
                "name": "single_old_and_new",
                "gparray": self.all_up_gparray_str,
                "config": "sdw2|21000|/mirror/gpseg0 sdw3|41000|/mirror/gpseg5_new",
                "expected": [self._triplet('10|0|m|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           '2|0|p|p|s|u|sdw1|sdw1|20000|/primary/gpseg0',
                                           '10|0|m|m|s|u|sdw3|sdw3|41000|/mirror/gpseg5_new')]
            },
            {
                "name": "multiple_old_and_new",
                "gparray": self.all_up_gparray_str,
                "config": """sdw2|21000|/mirror/gpseg0 sdw3|41001|/mirror/gpseg4
                             sdw2|21001|/mirror/gpseg1 sdw2|41001|/mirror/gpseg_new
                             sdw4|21000|/mirror/gpseg4
                             sdw1|21000|/mirror/gpseg6 sdw4|41000|/mirror/gpseg7""",
                "expected": [self._triplet('10|0|m|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           '2|0|p|p|s|u|sdw1|sdw1|20000|/primary/gpseg0',
                                           '10|0|m|m|s|u|sdw3|sdw3|41001|/mirror/gpseg4'),
                             self._triplet('11|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           '3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1',
                                           '11|1|m|m|s|u|sdw2|sdw2|41001|/mirror/gpseg_new'),
                             self._triplet('14|4|m|m|s|u|sdw4|sdw4|21000|/mirror/gpseg4',
                                           '6|4|p|p|s|u|sdw3|sdw3|20000|/primary/gpseg4',
                                           None),
                             self._triplet('16|6|m|m|s|u|sdw1|sdw1|21000|/mirror/gpseg6',
                                           '8|6|p|p|s|u|sdw4|sdw4|20000|/primary/gpseg6',
                                           '16|6|m|m|s|u|sdw4|sdw4|41000|/mirror/gpseg7')]
            },
            # TODO: this should work but the failedSegment in the gparray is mutated but should not be...
            # {
            #     "name": "multiple_old_with_unreachable_existing_hosts",
            #     "gparray": self.all_up_gparray_str,
            #     "config": """sdw2|21000|/mirror/gpseg0 sdw3|41001|/mirror/gpseg4
            #                  sdw4|21000|/mirror/gpseg4
            #                  sdw1|21000|/mirror/gpseg6 sdw4|41000|/mirror/gpseg7""",
            #     "unreachable_existing_hosts": ['sdw2'],
            #     "expected": [self._triplet('14|4|m|m|s|u|sdw4|sdw4|21000|/mirror/gpseg4',
            #                                '6|4|p|p|s|u|sdw3|sdw3|20000|/primary/gpseg4',
            #                                None),
            #                  self._triplet('16|6|m|m|s|u|sdw1|sdw1|21000|/mirror/gpseg6',
            #                                '8|6|p|p|s|u|sdw4|sdw4|20000|/primary/gpseg6',
            #                                '16|6|m|m|s|u|sdw4|sdw4|41000|/mirror/gpseg7')]
            # },
            {
                "name": "in_place_1_part",
                "gparray": self.all_up_gparray_str,
                "config": "sdw2|21000|/mirror/gpseg0",
                "expected": [self._triplet('10|0|m|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           '2|0|p|p|s|u|sdw1|sdw1|20000|/primary/gpseg0',
                                           None)]
            },
            {
                "name": "in_place_2_parts",
                "gparray": self.all_up_gparray_str,
                "config": "sdw2|21000|/mirror/gpseg0 sdw2|21000|/mirror/gpseg0",
                "expected": [self._triplet('10|0|m|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           '2|0|p|p|s|u|sdw1|sdw1|20000|/primary/gpseg0',
                                           '10|0|m|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0')]
            },
            {
                "name": "in_place_1_part_unreachable_exising_host",
                "gparray": self.all_up_gparray_str,
                "config": "sdw2|21000|/mirror/gpseg0",
                "unreachable_existing_hosts": ['sdw2'],
                "expected": []
            },
            {
                "name": "one_mirror_inconfig_has_running_basebackup",
                "gparray": self.all_up_gparray_str,
                "config": """sdw2|21000|/mirror/gpseg0 sdw2|21000|/mirror/gpseg0_new
                             sdw1|21000|/mirror/gpseg6 sdw1|21000|/mirror/gpseg6_new""",
                "segments_with_running_basebackup": {0},
                "expected": [self._triplet('16|6|m|m|s|u|sdw1|sdw1|21000|/mirror/gpseg6',
                                           '8|6|p|p|s|u|sdw4|sdw4|20000|/primary/gpseg6',
                                           '16|6|m|m|s|u|sdw1|sdw1|21000|/mirror/gpseg6_new')]
            },
            {
                "name": "some_mirrors_inconfig_have_running_basebackup",
                "gparray": self.all_up_gparray_str,
                "config": """sdw2|21000|/mirror/gpseg0 sdw2|21000|/mirror/gpseg0_new
                             sdw2|21001|/mirror/gpseg1 sdw2|21000|/mirror/gpseg1_new
                             sdw1|21000|/mirror/gpseg6 sdw1|21000|/mirror/gpseg6_new""",
                "segments_with_running_basebackup": {0, 1},
                "expected": [self._triplet('16|6|m|m|s|u|sdw1|sdw1|21000|/mirror/gpseg6',
                                           '8|6|p|p|s|u|sdw4|sdw4|20000|/primary/gpseg6',
                                           '16|6|m|m|s|u|sdw1|sdw1|21000|/mirror/gpseg6_new')]
            },
            {
                "name": "all_mirrors_inconfig_have_running_basebackup",
                "gparray": self.all_up_gparray_str,
                "config": """sdw2|21000|/mirror/gpseg0 sdw2|21000|/mirror/gpseg0_new
                             sdw2|21001|/mirror/gpseg1 sdw2|21000|/mirror/gpseg1_new
                             sdw1|21000|/mirror/gpseg6 sdw1|21000|/mirror/gpseg6_new""",
                "segments_with_running_basebackup": {0, 1, 6},
                "expected": []
            },
            {
                "name": "one_mirror_inconfig_has_running_pgrewind",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "config": """sdw1|20000|/primary/gpseg0
                             sdw1|20001|/primary/gpseg1
                             sdw2|20000|/primary/gpseg2""",
                "is_pgrewind_running": [True, False, False],
                "expected": [self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           None),
                             self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           None)]
            },
            {
                "name": "some_mirrors_inconfig_have_running_pgrewind",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "config": """sdw1|20000|/primary/gpseg0
                             sdw1|20001|/primary/gpseg1
                             sdw2|20000|/primary/gpseg2""",
                "is_pgrewind_running": [True, False, True],
                "expected": [self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           None)]
           },
           {
                "name": "all_mirrors_inconfig_have_running_pgrewind",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "config": """sdw1|20000|/primary/gpseg0
                             sdw1|20001|/primary/gpseg1
                             sdw2|20000|/primary/gpseg2""",
                "is_pgrewind_running": [True, True, True],
                "expected": []
          },
          {
                "name": "no_mirror_inconfig_has_running_pgrewind",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "config": """sdw1|20000|/primary/gpseg0
                             sdw1|20001|/primary/gpseg1
                             sdw2|20000|/primary/gpseg2""",
                "is_pgrewind_running": [False, False, False],
                "expected": [self._triplet('2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0',
                                           '6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           None),
                             self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           None),
                             self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           None)]
          },

        ]
        self.run_pass_tests(tests, self.run_single_ConfigFile_test)

    def test_RecoveryTripletsUserConfigFile_getMirrorTriples_should_fail(self):
        tests = [
            {
                "name": "invalid_failed_address",
                "gparray": self.three_failedover_segs_gparray_str,
                "config": "seg_does_not_exist|20000|/primary/gpseg0 sdw3|20001|/primary/gpseg5",
                "expected": "segment to recover was not found in configuration.*described by.*seg_does_not_exist"
            },
            {
                "name": "invalid_failed_port1",
                "gparray": self.three_failedover_segs_gparray_str,
                "config": "sdw1|99999|/primary/gpseg0 sdw3|20001|/primary/gpseg5",
                "expected": "segment to recover was not found in configuration.*described by.*99999"
            },
            {
                "name": "invalid_failed_port2",
                "gparray": self.three_failedover_segs_gparray_str,
                "config": "sdw1|port_does_not_exist|/primary/gpseg0 sdw3|20001|/primary/gpseg5",
                "expected": "Invalid port"
            },
            {
                "name": "invalid_failed_data_dir",
                "gparray": self.three_failedover_segs_gparray_str,
                "config": "sdw1|20000|/does/not/exist sdw3|20001|/primary/gpseg5",
                "expected": "segment to recover was not found in configuration.*described by.*exist"
            },
            {
                "name": "invalid_failed_and_new_address",
                "gparray": self.three_failedover_segs_gparray_str,
                "config": "seg_does_not_exist_old|20000|/primary/gpseg0 seg_does_not_exist_new|20001|/primary/gpseg5",
                "expected": "segment to recover was not found in configuration.*described by.*seg_does_not_exist_old"
            },
            {
                "name": "invalid_recovery_port1",
                "gparray": self.three_failedover_segs_gparray_str,
                "config": "sdw1|20000|/primary/gpseg0 sdw3|port_does_not_exist|/primary/gpseg5",
                "expected": "Invalid port"
            },
            {
                "name": "no_peer_for_failed_seg",
                "gparray": self.content0_no_peer_gparray_str,
                "config": "sdw1|20000|/primary/gpseg0 sdw3|20000|/primary/gpseg5",
                "expected": "No peer found for dbid 2. liveSegment is None"
            },
            {
                "name": "both_peers_down",
                "gparray": self.content0_mirror_and_its_peer_down_gparray_str,
                "config": "sdw1|20000|/primary/gpseg0 sdw3|20000|/primary/gpseg5",
                "expected": "Primary segment is not up for content 0"
            },
            {
                "name": "failed_and_live_same_dbid",
                "gparray": """1|-1|p|p|n|u|cdw|cdw|5432|/coordinator/gpseg-1
                               2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0
                               3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1
                               8|2|m|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                               9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                               2|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0
                               7|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                               4|2|p|p|s|u|sdw2|sdw2|20000|/primary/gpseg2
                               5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3""",
                "config": "sdw1|20000|/primary/gpseg0 sdw3|20000|/primary/gpseg5",
                "expected": "For content 2, the dbid values are the same.  A segment may not be recovered from itself"
            },
            {
                "name": "failover_unreachable",
                "gparray": """1|-1|p|p|n|u|cdw|cdw|5432|/coordinator/gpseg-1
                                        2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0
                                        3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1
                                        8|2|m|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                                        9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                                        6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0
                                        7|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                                        4|2|p|p|s|u|sdw2|sdw2|20000|/primary/gpseg2
                                        5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3""",
                "config": "sdw1|20000|/primary/gpseg0 new_1|20000|/primary/gpseg0",
                "unreachable_hosts": ['new_1'],
                "expected": "The recovery target segment new_1 \(content 0\) is unreachable."
            },
            #
            #
            # TODO: these should fail, but right now do not.  For recovery port and host, we should detect them here.
            #   For recovery data directory, it is not clear where the check should go.
            # {
            #     "name": "invalid_recovery_host",
            #     "config": "sdw1|20000|/primary/gpseg0 host_does_not_exist|20001|/primary/gpseg5",
            # },
            # {
            #     "name": "invalid_recovery_port",
            #     "config": "sdw1|20000|/primary/gpseg0 sdw3|99999|/primary/gpseg5",
            # },
            # {
            #     "name": "invalid_recovery_data_dir",
            #     "config": "sdw1|20000|/primary/gpseg0 sdw3|20001|/does/not/exist",
            # },
        ]
        self.run_fail_tests(tests, self.run_single_ConfigFile_test)

    def test_RecoveryTripletsInPlaceAndNewHosts_getMirrorTriples_should_pass(self):
        tests = [{
                "name": "no_new_hosts",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "expected": [self._triplet('2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0',
                                           '6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           None),
                             self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           None),
                             self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           None)]
            },
            {
                "name": "one_existing_host_down",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "unreachable_existing_hosts": ['sdw1'],
                "expected": [self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           None)]
            },
            {
                "name": "all_relevant_existing_hosts_down",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "unreachable_existing_hosts": ['sdw1', 'sdw2'],
                "expected": []
            },
            {
                "name": "no_segs_to_recover",
                "gparray": self.all_up_gparray_str,
                "new_hosts": ['new_1'],
                "expected": []
            },
            {
                "name": "enough_hosts",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": ['new_1', 'new_2'],
                "expected": [self._triplet('2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0',
                                           '6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           '2|0|m|p|s|d|new_1|new_1|20000|/primary/gpseg0'),
                             self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           '3|1|m|p|s|d|new_1|new_1|20001|/primary/gpseg1'),
                             self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           '4|2|m|p|s|d|new_2|new_2|20000|/primary/gpseg2')]
            },
            {
                "name": "more_hosts",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": ['new_1', 'new_2', 'new_3'], # This test assumes there is only one extra host
                "expected": [self._triplet('2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0',
                                           '6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           '2|0|m|p|s|d|new_1|new_1|20000|/primary/gpseg0'),
                             self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           '3|1|m|p|s|d|new_1|new_1|20001|/primary/gpseg1'),
                             self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           '4|2|m|p|s|d|new_2|new_2|20000|/primary/gpseg2')]
            },
            {
                "name": "failed_unreachable",
                "gparray": """1|-1|p|p|n|u|mdw|mdw|5432|/coordinator/gpseg-1
                               2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0
                               3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1
                               8|2|m|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                               9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                               6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0
                               7|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                               4|2|p|p|s|u|sdw2|sdw2|20000|/primary/gpseg2
                               5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3""",
                "new_hosts": ['new_1'],
                "unreachable_existing_hosts": ['sdw1'],
                "expected": [self._triplet('2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0',
                                           '6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           '2|0|m|p|s|d|new_1|new_1|20000|/primary/gpseg0', failed_unreachable=True)]
            },
            {
                "name": "one_failed_segments_has_running_basebackup",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "segments_with_running_basebackup": {0},
                "expected": [self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           None),
                             self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           None)]
            },
            {
                "name": "some_failed_segments_have_running_basebackup",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "segments_with_running_basebackup": {0, 1},
                "expected": [self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           None)]
            },
            {
                "name": "all_failed_segments_have_running_basebackup",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "segments_with_running_basebackup": {0, 1, 2},
                "expected": []
            },
            {
                "name": "one_failed_segments_have_running_pgrewind",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "is_pgrewind_running": [True, False, False],
                "expected": [self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           None),
                             self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           None)]
            },
            {
                "name": "some_failed_segments_have_running_pgrewind",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "is_pgrewind_running": [True, False, True],
                "expected": [self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           None)]
            },
            {
                "name": "all_failed_segments_have_running_pgrewind",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "is_pgrewind_running": [True, True, True],
                "expected": []
            },
            {
                "name": "no_failed_segment_has_running_pgrewind",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": [],
                "is_pgrewind_running": [False, False, False],
                "expected": [self._triplet('2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0',
                                           '6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0',
                                           None),
                             self._triplet('3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1',
                                           '7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1',
                                           None),
                             self._triplet('4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2',
                                           '8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2',
                                           None)]
            },
        ]

        self.run_pass_tests(tests, self.run_single_GpArray_test)

    def test_RecoveryTripletsInPlaceAndNewHosts_getMirrorTriples_should_fail(self):
        tests = [
            {
                "name": "not_enough_hosts",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": ['new_1'],
                "unreachable_hosts": [],
                "expected": "Not enough new recovery hosts given for recovery."
            },
            {
                "name": "all_hosts_unreachable1",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": ['new_1'],
                "unreachable_hosts": ['new_1'],
                "expected": "Not enough new recovery hosts given for recovery."
            },
            {
                "name": "all_hosts_unreachable2",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": ['new_1', 'new_2'],
                "unreachable_hosts": ['new_1', 'new_2'],
                "expected": "Cannot recover. The following recovery target hosts are unreachable: \['new_1', 'new_2'\]"
            },
            {
                "name": "some_hosts_unreachable",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": ['new_1', 'new_2'],
                "unreachable_hosts": ['new_2'],
                "expected": "Cannot recover. The following recovery target hosts are unreachable: \['new_2'\]"
            },
            {
                "name": "no_peer_for_failed_seg",
                "gparray": self.content0_no_peer_gparray_str,
                "new_hosts": ['new_1', 'new_2'],
                "unreachable_hosts": [],
                "expected": "No peer found for dbid 2. liveSegment is None"
            },
            {
                "name": "both_peers_down",
                "gparray": self.content0_mirror_and_its_peer_down_gparray_str,
                "new_hosts": ['new_1', 'new_2'],
                "unreachable_hosts": [],
                "expected": "Primary segment is not up for content 0"
            },
            {
                "name": "both_peers_down2",
                "gparray": self.content0_mirror_and_its_peer_down_gparray_str2,
                "new_hosts": ['new_1', 'new_2'],
                "unreachable_hosts": [],
                "expected": "Segment to recover from for content 0 is not a primary"
            },
            {
                "name": "live_unreachable",
                "gparray": self.three_failedover_segs_gparray_str,
                "new_hosts": ['new_1','new_2'],
                "unreachable_existing_hosts": ['sdw2'],
                "expected": "The recovery source segment sdw2 \(content 0\) is unreachable"
            },
            {
            "name": "failed_and_live_same_dbid",
            "gparray": """1|-1|p|p|n|u|mdw|mdw|5432|/coordinator/gpseg-1
                       2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0
                       3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1
                       8|2|m|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                       9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                       2|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0
                       7|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                       4|2|p|p|s|u|sdw2|sdw2|20000|/primary/gpseg2
                       5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3""",
            "new_hosts": ['new_1'],
            "expected": "For content 2, the dbid values are the same.  A segment may not be recovered from itself"
            },
            {
                 "name": "failover_unreachable",
                 "gparray": """1|-1|p|p|n|u|mdw|mdw|5432|/coordinator/gpseg-1
                                2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0
                                3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1
                                8|2|m|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                                9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                                6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0
                                7|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                                4|2|p|p|s|u|sdw2|sdw2|20000|/primary/gpseg2
                                5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3""",
                 "new_hosts": ['new_1'],
                 "unreachable_hosts": ['new_1'],
                 "expected": "Cannot recover. The following recovery target hosts are unreachable: \['new_1'\]"
            },
        ]
        self.run_fail_tests(tests, self.run_single_GpArray_test)

    def run_pass_tests(self, tests, fn_to_test):
        for test in tests:
            with self.subTest(msg=test["name"]):

                initial_gparray, actual_gparray, actual = fn_to_test(test)
                # expected = self._update_triplets(test["expected"], test.get("set_unreachable_to_false_for_failed_segments_on_these_hosts"))
                self.assertEqual(test["expected"], actual,
                                 msg="\n\nTest {} failed.\n\nexpected:\n{}\n\ngot:\n{}".format(
                                     test["name"], test["expected"], actual))

                expected_gparray = self.get_expected_gparray(initial_gparray, test["expected"])
                self.assertEqual(self.get_gparray_for_cmp(expected_gparray), self.get_gparray_for_cmp(actual_gparray),
                                 msg="\n\nTest {} failed.\n\ninitial:\n{}\n\nexpected:\n{}\n\ngot:\n{}".format(
                                     test["name"],
                                     self.get_gparray_for_cmp(initial_gparray),
                                     self.get_gparray_for_cmp(expected_gparray),
                                     self.get_gparray_for_cmp(actual_gparray)))

    def run_fail_tests(self, tests, fn_to_test):
        for test in tests:
            with self.subTest(msg=test["name"]):
                # make sure test does not pass trivially("" will pass assertRaisesRegex)
                self.assertTrue(test["expected"].strip() != "")
                # TODO it is possible to match partial strings with regex that might be a typo. Should we instead not
                #  use Regex and type out the exact error message ?
                with self.assertRaisesRegex(Exception, test["expected"]):
                    fn_to_test(test)

    @patch('gppylib.db.dbconn.connect', side_effect=Exception())
    def test_get_segments_with_running_basebackup_conn_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            get_segments_with_running_basebackup()

        self.assertTrue('Failed to query gp_stat_replication' in str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.query', side_effect=Exception())
    def test_get_segments_with_running_basebackup_query_exception(self, mock1, mock2):
        with self.assertRaises(Exception) as ex:
            get_segments_with_running_basebackup()

        self.assertTrue('Failed to query gp_stat_replication:' in str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.query', return_value=FakeCursor(my_list=[]))
    def test_get_segments_with_running_basebackup_no_basebackup(self, mock1, mock2):
        segments_with_running_basebackup = get_segments_with_running_basebackup()
        self.assertEqual(list(segments_with_running_basebackup), [])
        self.assertEqual([call("No basebackup running")], self.mock_logger.debug.call_args_list)

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.query', return_value=FakeCursor(my_list=[[1]]))
    def test_get_segments_with_running_basebackup_has_basebackup(self, mock1, mock2):
        segments_with_running_basebackup = get_segments_with_running_basebackup()
        self.assertEqual(list(segments_with_running_basebackup), [1])
        self.assertEqual(0, self.mock_logger.debug.call_count)

    @patch('gppylib.db.dbconn.connect', side_effect=Exception())
    def test_is_pg_rewind_running_conn_exception(self, mock1):
        with self.assertRaises(Exception) as ex:
            is_pg_rewind_running("sdw1", 6001)
        self.assertEqual('Failed to query pg_stat_activity for segment hostname: sdw1, port: 6001, error: ', str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.querySingleton', side_effect=Exception())
    def test_is_pg_rewind_running_query_exception(self, mock1, mock2):
        with self.assertRaises(Exception) as ex:
            is_pg_rewind_running("sdw1", 6001)
        self.assertEqual('Failed to query pg_stat_activity for segment hostname: sdw1, port: 6001, error: ', str(ex.exception))

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.querySingleton', return_value=2)
    def test_is_pg_rewind_running_returns_true_for_query_result_greater_than_one(self, mock1, mock2):
        is_pgrewind_running = is_pg_rewind_running("sdw1", 6001)
        self.assertEqual([call("Checking for running instances of pg_rewind with host sdw1 and port 6001 as source "
                               "server")], self.mock_logger.debug.call_args_list)
        self.assertEqual(is_pgrewind_running, True)

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.querySingleton', return_value=1)
    def test_is_pg_rewind_running_returns_true(self, mock1, mock2):
        is_pgrewind_running = is_pg_rewind_running("sdw1", 6001)
        self.assertEqual([call("Checking for running instances of pg_rewind with host sdw1 and port 6001 as source "
                               "server")], self.mock_logger.debug.call_args_list)
        self.assertEqual(is_pgrewind_running, True)

    @patch('gppylib.db.dbconn.connect', autospec=True)
    @patch('gppylib.db.dbconn.querySingleton', return_value=0)
    def test_is_pg_rewind_running_returns_false(self, mock1, mock2):
        is_pgrewind_running = is_pg_rewind_running("sdw1", 6001)
        self.assertEqual([call("Checking for running instances of pg_rewind with host sdw1 and port 6001 as source "
                               "server")], self.mock_logger.debug.call_args_list)
        self.assertEqual(is_pgrewind_running, False)

    def __init__(self, arg):
        super().__init__(arg)

        # It's possible to have no down segments, as gpmovemirrors also calls gprecoverseg.
        self.all_up_gparray_str = '''1|-1|p|p|n|u|mdw|mdw|5432|/coordinator/gpseg-1
                                  2|0|p|p|s|u|sdw1|sdw1|20000|/primary/gpseg0
                                  3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1
                                  16|6|m|m|s|u|sdw1|sdw1|21000|/mirror/gpseg6
                                  17|7|m|m|s|u|sdw1|sdw1|21001|/mirror/gpseg7
                                  10|0|m|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0
                                  11|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                                  4|2|p|p|s|u|sdw2|sdw2|20000|/primary/gpseg2
                                  5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3
                                  12|2|m|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                                  13|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                                  6|4|p|p|s|u|sdw3|sdw3|20000|/primary/gpseg4
                                  7|5|p|p|s|u|sdw3|sdw3|20001|/primary/gpseg5
                                  14|4|m|m|s|u|sdw4|sdw4|21000|/mirror/gpseg4
                                  15|5|m|m|s|u|sdw4|sdw4|21001|/mirror/gpseg5
                                  8|6|p|p|s|u|sdw4|sdw4|20000|/primary/gpseg6
                                  9|7|p|p|s|u|sdw4|sdw4|20001|/primary/gpseg7'''

        # We include down segments, so that gprecoverseg can find them automatically
        self.three_failedover_segs_gparray_str = '''1|-1|p|p|n|u|mdw|mdw|5432|/coordinator/gpseg-1
                                  2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0
                                  3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1
                                  8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                                  9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                                  6|0|p|m|s|u|sdw2|sdw2|21000|/mirror/gpseg0
                                  7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                                  4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2
                                  5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3'''

        self.content0_no_peer_gparray_str = '''1|-1|p|p|n|u|mdw|mdw|5432|/coordinator/gpseg-1
                                  2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0
                                  3|1|m|p|s|d|sdw1|sdw1|20001|/primary/gpseg1
                                  8|2|p|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                                  9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                                  7|1|p|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                                  4|2|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg2
                                  5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3'''

        self.content0_mirror_and_its_peer_down_gparray_str = '''1|-1|p|p|n|u|mdw|mdw|5432|/coordinator/gpseg-1
                                  2|0|m|p|s|d|sdw1|sdw1|20000|/primary/gpseg0
                                  3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1
                                  8|2|m|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                                  9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                                  6|0|p|m|s|d|sdw2|sdw2|21000|/mirror/gpseg0
                                  7|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                                  4|2|p|p|s|u|sdw2|sdw2|20000|/primary/gpseg2
                                  5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3'''

        self.content0_mirror_and_its_peer_down_gparray_str2 = '''1|-1|p|p|n|u|mdw|mdw|5432|/coordinator/gpseg-1
                                  2|0|m|p|s|d|sdw2|sdw2|20000|/primary/gpseg0
                                  3|1|p|p|s|u|sdw1|sdw1|20001|/primary/gpseg1
                                  8|2|m|m|s|u|sdw3|sdw3|21000|/mirror/gpseg2
                                  9|3|m|m|s|u|sdw3|sdw3|21001|/mirror/gpseg3
                                  6|0|p|m|s|d|sdw1|sdw1|21000|/mirror/gpseg0
                                  7|1|m|m|s|u|sdw2|sdw2|21001|/mirror/gpseg1
                                  4|2|p|p|s|u|sdw2|sdw2|20000|/primary/gpseg2
                                  5|3|p|p|s|u|sdw2|sdw2|20001|/primary/gpseg3'''

    def _run_single_FromGpArray_test(self, gparray_str, config_file, new_hosts, unreachable_hosts, is_pgrewind_running,
                                     segments_with_running_basebackup, unreachable_existing_hosts=None):
        unreachable_hosts = unreachable_hosts if unreachable_hosts else []
        gppylib.programs.clsRecoverSegment_triples.get_unreachable_segment_hosts = Mock(return_value=unreachable_hosts)
        gppylib.programs.clsRecoverSegment_triples.get_segments_with_running_basebackup = Mock(
            return_value=segments_with_running_basebackup)
        gppylib.programs.clsRecoverSegment_triples.is_pg_rewind_running = Mock(side_effect=is_pgrewind_running)

        initial_gparray = self.get_gp_array(gparray_str, unreachable_existing_hosts)
        mutated_gparray = self.get_gp_array(gparray_str, unreachable_existing_hosts)
        i = RecoveryTripletsFactory.instance(mutated_gparray, config_file=config_file, new_hosts=new_hosts)
        triples = i.getTriplets()

        warnings = i.getInterfaceHostnameWarnings()
        if warnings:
            # TODO Currently we do not assert if warnings should have been populated.
            expected = ["The following recovery hosts were not needed:", "\t{}".format(new_hosts[-1])]
            self.assertEqual(expected, warnings)

        return initial_gparray, mutated_gparray, triples

    @staticmethod
    def _triplet(failed, live, failover, failed_unreachable=False):
        failedSeg = Segment.initFromString(failed)
        failedSeg.unreachable = failed_unreachable
        return RecoveryTriplet(failedSeg,
                              Segment.initFromString(live),
                              Segment.initFromString(failover) if failover else None)

    @staticmethod
    def get_gp_array(gparray_str, unreachable_existing_hosts=None):
        with tempfile.NamedTemporaryFile() as f:
            f.write(gparray_str.encode('utf-8'))
            f.flush()

            gparray = GpArray.initFromFile(f.name)
            if not unreachable_existing_hosts:
                return gparray

            # the caller of the function under test sets to True the "unreachable"
            # member on any segment on an unreachable host; we emulate that here.
            for seg in gparray.getSegDbList():
                if seg.getSegmentHostName() in unreachable_existing_hosts:
                    seg.unreachable = True
            return gparray

    # apply the "recover_triplets" to the "gparray" such that the "result"ing gparray
    # will be the result of a successful gprecoverseg.
    @staticmethod
    def get_expected_gparray(gparray, recover_triplets):
        result = copy.copy(gparray)
        segMap = result.getSegDbMap()

        for t in recover_triplets:
            failed, failover = t.failed, t.failover

            if not failover:
                continue

            if not failed.getSegmentDbId() in segMap:
                continue

            # these are the only fields we can change in the failed segment
            segMap[failed.getSegmentDbId()].address = failover.address
            segMap[failed.getSegmentDbId()].hostname = failover.hostname
            segMap[failed.getSegmentDbId()].port = failover.port
            segMap[failed.getSegmentDbId()].datadir = failover.datadir
            segMap[failed.getSegmentDbId()].unreachable = failover.unreachable

        return result

    def get_gparray_for_cmp(self, gparray):
        """
        TODO This should ideally be in the __repr__ function in GpArray class.

        Get a string representation of gparray for comparison in our tests.
        We cannot rely on str(gparray) for comparison since the __str__ function for the Segment class
        does not include the port and address.
        """
        segMap = gparray.getSegDbMap()
        gparray_str = io.StringIO()
        for dbid in sorted(segMap.keys()):
            gparray_str.write(repr(segMap[dbid]))
            #TODO gparray's repr function does not include the unreachable property, so we have to add it explicitly heres
            gparray_str.write(":unreachable=%s" % segMap[dbid].unreachable)
            gparray_str.write('\n')

        return gparray_str.getvalue()


class RecoveryTripletsUserConfigFileParserTestCase(GpTestCase):

    @staticmethod
    def run_single_parser_test(test):
        with tempfile.NamedTemporaryFile() as f:
            f.write(test["config"].encode("utf-8"))
            f.flush()
            return RecoveryTripletsUserConfigFile._parseConfigFile(f.name)

    passing_tests = [
        {
            "name": "simple_pass",
            "config": """sdw1|20000|/primary/gpseg0 sdw3|20001|/primary/gpseg5
                      sdw1|20001|/primary/gpseg1 sdw1|40001|/primary/gpseg_new
                      sdw3|20000|/primary/gpseg4
                      sdw4|20000|/primary/gpseg6 sdw4|20000|/primary/gpseg6"""
        },
        {
            "name": "6X_web_doc",
            "config": """sdw2|50000|/data2/mirror/gpseg0 sdw3|50000|/data/mirror/gpseg0
                     sdw2|50001|/data2/mirror/gpseg1 sdw4|50001|/data/mirror/gpseg1
                     sdw3|50002|/data2/mirror/gpseg2 sdw1|50002|/data/mirror/gpseg2"""},
        {
            "name": "old_to_new_new_to_other",
            "config": """sdw1|20000|/primary/gpseg0 sdw3|20001|/primary/gpseg5
                      sdw3|20001|/primary/gpseg5 sdw4|20000|/primary/gpseg6"""
        },
        {
            "name": "old_to_new_new_to_old",
            "config": """sdw1|20000|/primary/gpseg0 sdw3|20001|/primary/gpseg5
                      sdw3|20001|/primary/gpseg5 sdw1|20000|/primary/gpseg0"""
        }
        ]

    def test_parsing_should_pass(self):
        for test in self.passing_tests:
            with self.subTest(msg=test["name"]):

                rows = self.run_single_parser_test(test)
                self.assertEqual(self._get_expected_config(test["config"]), rows)

    failing_tests = [
        {
            "name":
                "too_many_groups",
            "config":
                """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5 sdw3|20001|/mirror/gpseg5""",
            "expected":
                "line 1 of file .*: expected 1 or 2 groups but found 3"
        },
        {
            "name":
                "too_few_parts_in_group_1",
            "config":
                """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                   sdw1|20000 sdw3|20001|/mirror/gpseg5""",
            "expected":
                "line 2 of file .*: expected 3 parts on failed segment group, obtained 2"
        },
        {
            "name":
                "too_few_parts_in_group_2",
            "config": """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                         sdw2|50001|/data2/mirror/gpseg1 sdw4|50001""",
            "expected":
                "line 2 of file .*: expected 3 parts on new segment group, obtained 2"
        },
        {
            "name":
                "old_inplace_and_old_to_old",
            "config":
                """sdw1|20000|/mirror/gpseg0
                   sdw1|20000|/mirror/gpseg0 sdw1|20000|/mirror/gpseg0""",
            "expected":
                "config file lines 1 and 2 conflict: Cannot recover the same failed segment sdw1 and data "
                "directory /mirror/gpseg0 twice"
        },
        {
            "name": "old_inplace_and_old_to_old",
            "config": """sdw1|20000|/mirror/gpseg0
                         sdw1|20000|/mirror/gpseg0 sdw1|20000|/mirror/gpseg0""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover the same failed segment sdw1 and data "
                        "directory /mirror/gpseg0 twice"

        },
        {
            "name": "old_inplace_and_old_inplace",
            "config": """sdw1|20000|/mirror/gpseg0
                         sdw1|20000|/mirror/gpseg0""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover the same failed segment sdw1 and data "
                        "directory /mirror/gpseg0 twice"

        },
        {
            "name": "old1_to_new_and_old2_to_new",
            "config": """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                         sdw2|20001|/mirror/gpseg3 sdw3|20001|/mirror/gpseg5""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover to the same segment sdw3 and data directory "
                        "/mirror/gpseg5 twice"
        },
        {
            "name": "old_to_old_and_old_to_old",
            "config": """sdw1|20000|/mirror/gpseg0 sdw1|20000|/mirror/gpseg0
                         sdw1|20000|/mirror/gpseg0 sdw1|20000|/mirror/gpseg0""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover the same failed segment sdw1 and data "
                        "directory /mirror/gpseg0 twice"
        },
        {
            "name": "old_to_new_and_old_inplace",
            "config": """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                         sdw1|20000|/mirror/gpseg0""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover the same failed segment sdw1 and data "
                        "directory /mirror/gpseg0 twice"

        },
        {
            "name": "old_to_new_and_old_to_old",
            "config": """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                         sdw1|20000|/mirror/gpseg0 sdw1|20000|/mirror/gpseg0""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover the same failed segment sdw1 and data "
                        "directory /mirror/gpseg0 twice"

        },
        {
            "name": "old_to_new1_and_old_to_new2",
            "config": """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                         sdw1|20000|/mirror/gpseg0 sdw2|20001|/mirror/gpseg3""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover the same failed segment sdw1 and data "
                        "directory /mirror/gpseg0 twice"
        },
        {
            "name": "old_to_new_and_new_to_new",
            "config": """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                         sdw3|20001|/mirror/gpseg5 sdw3|20001|/mirror/gpseg5""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover to the same segment sdw3 and data directory "
                        "/mirror/gpseg5 twice"
        },
        {
            "name": "old_to_new_and_old_to_new",
            "config": """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                         sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover the same failed segment sdw1 and data "
                        "directory /mirror/gpseg0 twice"
        },
        {
            "name": "old_to_new_and_new_inplace",
            "config": """sdw1|20000|/mirror/gpseg0 sdw3|20001|/mirror/gpseg5
                         sdw3|20001|/mirror/gpseg5""",
            "expected": "config file lines 1 and 2 conflict: Cannot recover segment sdw3 with data directory "
                        "/mirror/gpseg5 in place if it is used as a recovery segment"
        },
        {
            "name": "old_data_dir_not_absolute",
            "config": """sdw2|50000|/data2/mirror/gpseg0 sdw3|50000|/data/mirror/gpseg0
                         sdw2|50001|relative/old/mirror/gpseg1 sdw4|50001|/data/mirror/gpseg1""",
            "expected": "Path entered.*is invalid; it must be a full path.  Path: 'relative/old/mirror/gpseg1' from: 2"
        },
        {
            "name": "new_data_dir_not_absolute",
            "config": """sdw2|50001|/data2/mirror/gpseg1 sdw4|50001|relative/new/mirror/gpseg1
                         sdw2|50000|/data2/mirror/gpseg0 sdw3|50000|/data/mirror/gpseg0""",
            "expected": "Path entered.*is invalid; it must be a full path.  Path: 'relative/new/mirror/gpseg1' from: 1"
        },
        {
            "name": "old_port_invalid",
            "config": """sdw2|old_invalid_port|/data2/mirror/gpseg1 sdw4|50001|relative/new/mirror/gpseg1""",
            "expected": "Invalid port on line 1"
        },
        {
            "name": "new_port_invalid",
            "config": """sdw2|50001|/data2/mirror/gpseg1 sdw4|new_invalid_port|relative/new/mirror/gpseg1""",
            "expected": "Invalid port on line 1"
        }
    ]

    def test_parsing_should_fail(self):
        for test in self.failing_tests:
            with self.subTest(msg=test["name"]):
                # make sure test does not pass trivially("" will pass assertRaisesRegex)
                self.assertTrue(test["expected"].strip() != "")

                with self.assertRaisesRegex(Exception, test["expected"]):
                    self.run_single_parser_test(test)

    @staticmethod
    def _get_expected_config(config_str):
        rows = []
        lineno = 0

        for line in config_str.splitlines():
            lineno += 1
            groups = line.split()

            address, port, datadir = groups[0].split('|')
            row = {
                'failedAddress': address,
                'failedPort': port,
                'failedDataDirectory': datadir,
                'lineno': lineno
            }

            if len(groups) > 1:
                address, port, datadir = groups[1].split('|')
                row.update({
                    'newAddress': address,
                    'newPort': port,
                    'newDataDirectory': datadir
                })

            rows.append(row)

        return rows
