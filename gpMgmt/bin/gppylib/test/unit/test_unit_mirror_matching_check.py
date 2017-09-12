import os

from mock import *

from gp_unittest import *
from gpcheckcat_modules.mirror_matching_check import MirrorMatchingCheck

class MirrorMatchingTestCase(GpTestCase):
    def setUp(self):
        self.subject = MirrorMatchingCheck()

        self.logger = Mock(spec=['log', 'info', 'debug', 'error'])
        self.db_connection = Mock(spec=['close', 'query'])

        self.gpConfigMock = Mock()
        self.apply_patches([
           patch('gpcheckcat_modules.mirror_matching_check.get_gparray_from_config', return_value=self.gpConfigMock),
        ])

    def test_mirror_matching_permutations(self):
        tests = self._get_list_of_tests_for_permutations_of_matching()
        for test in tests:
            segment_states = self._create_mirror_states(mirror_id_and_state_collection=test["segment_ids_and_states"])
            config_has_mirrors = True if test["config"] else False
            self.gpConfigMock.hasMirrors = config_has_mirrors

            self.db_connection.reset_mock()
            self.logger.reset_mock()
            self._setup_mirroring_matching(
                database_mirror_states=segment_states
            )

            self.subject.run_check(self.db_connection, self.logger)
            self._assert_mirroring_common()
            if test["is_matched"]:
                info_messages = self.logger.info.call_args_list
                self.assertIn("[OK] mirroring_matching", info_messages[2][0])
            else:
                self._assert_mirroring_mismatch(segment_states, config_has_mirrors)

    ####################### PRIVATE METHODS #######################

    def _setup_mirroring_matching(self, database_mirror_states=None):
        self.db_connection.query.return_value.getresult.return_value = database_mirror_states

    def _assert_mirroring_mismatch(self, segment_states, config_has_mirrors):
        info_messages = self.logger.info.call_args_list
        self.assertIn('[FAIL] Mirroring mismatch detected', info_messages[2][0])

        error_messages = self.logger.error.call_args_list
        config_enabled = config_has_mirrors == True
        self.assertIn("The GP configuration reports mirror enabling is: %s" % config_enabled, info_messages[3][0])
        self.assertIn("The following segments are mismatched in PT:", error_messages[0][0])
        self.assertIn("Segment ID:\tmirror_existence_state:", error_messages[2][0])

        info_index = 3
        for segment_state in segment_states:
            segment_id = segment_state[0]
            segment_mirroring_state = segment_state[1]
            segment_enabled = segment_mirroring_state > 1
            # 0 is considered a match in either situation
            if segment_mirroring_state == 0:
                segment_enabled = config_enabled
            if segment_enabled != config_enabled:
                label = "Enabled" if segment_enabled else "Disabled"
                self.assertIn("%s\t\t%s (%s)" % (segment_id, segment_mirroring_state, label), error_messages[info_index][0])
                info_index += 1

    def _assert_mirroring_common(self):
        expected_message1 = '-----------------------------------'
        expected_message2 = 'Checking mirroring_matching'
        info_messages = self.logger.info.call_args_list
        error_messages = self.logger.error.call_args_list
        self.assertIn(expected_message1, info_messages[0][0])
        self.assertIn(expected_message2, info_messages[1][0])
        self.db_connection.query.assert_called_once_with(
            "SELECT gp_segment_id, mirror_existence_state FROM gp_dist_random('gp_persistent_relation_node') GROUP BY 1,2")
        return error_messages, info_messages

    def _get_list_of_tests_for_permutations_of_matching(self):
        result = []
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 3)], is_matched=True))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 3)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1)], is_matched=True))

        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 3), (2, 3)], is_matched=True))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 3), (2, 3)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 1), (2, 3)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1), (2, 4)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 3), (2, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 3), (2, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 1), (2, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1), (2, 1)], is_matched=True))

        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 4), (2, 3), (3, 3)], is_matched=True))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 3), (2, 3), (3, 5)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 1), (2, 3), (3, 3)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1), (2, 4), (3, 3)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 3), (2, 1), (3, 3)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 3), (2, 1), (3, 7)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 3), (2, 6), (3, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 3), (2, 4), (3, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 1), (2, 1), (3, 3)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1), (2, 1), (3, 3)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 1), (2, 6), (3, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1), (2, 3), (3, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 3), (2, 1), (3, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 5), (2, 1), (3, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 1), (2, 1), (3, 1)], is_matched=False))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1), (2, 1), (3, 1)], is_matched=True))

        # A case to test for when one segment returns multiple mirror states
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 1), (2, 1), (2, 3), (3, 3)], is_matched=False))

        # 0 matches enabled
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(2, 0), (2, 3), (3, 3)], is_matched=True))
        result.append(self._create_test_permutation(config=True, segment_ids_and_states=[(1, 0), (1, 1), (2, 3), (2, 0)], is_matched=False))

        # 0 matches disabled
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1), (1, 0), (2, 1)], is_matched=True))
        result.append(self._create_test_permutation(config=False, segment_ids_and_states=[(1, 1), (1, 0), (2, 0), (2, 3)], is_matched=False))

        return result

    # Convert the arguments to a dictionary
    def _create_test_permutation(self, **permutation_args):
        return permutation_args

    # Convert a list of tuples to a list of lists
    def _create_mirror_states(self, mirror_id_and_state_collection):
        result = []
        for (seg_id, mirror_state) in mirror_id_and_state_collection:
            result.append([seg_id, mirror_state])
        return result

if __name__ == '__main__':
    run_tests()
