import imp
import os

from mock import *

from gp_unittest import *


class GpCheckCatTestCase(GpTestCase):
    def setUp(self):
        # because gpcheckcat does not have a .py extension, we have to use imp to import it
        # if we had a gpcheckcat.py, this is equivalent to:
        #   import gpcheckcat
        #   self.subject = gpcheckcat
        gpcheckcat_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpcheckcat")
        self.subject = imp.load_source('gpcheckcat', gpcheckcat_file)

        self.subject.logger = Mock(spec=['log', 'info', 'debug', 'error'])
        self.db_connection = Mock(spec=['close'])

        self.unique_index_violation_check = Mock(spec=['runCheck'])
        self.unique_index_violation_check.runCheck.return_value = []

        self.leaked_schema_dropper = Mock(spec=['drop_leaked_schemas'])
        self.leaked_schema_dropper.drop_leaked_schemas.return_value = []

        # MagicMock: we are choosing to trust the implementation of GV.cfg
        # If we wanted full coverage we would make this a normal Mock()
        # and fully define its behavior
        self.subject.GV.cfg = MagicMock()
        self.subject.GV.checkStatus = True
        self.subject.setError = Mock()

        self.apply_patches([
            patch("gpcheckcat.pg.connect", return_value=self.db_connection),
            patch("gpcheckcat.UniqueIndexViolationCheck", return_value=self.unique_index_violation_check),
        ])

    def test_running_unknown_check__raises_exception(self):
        with self.assertRaises(LookupError):
            self.subject.runOneCheck('some_unknown_check')

    # @skip("order of checks")
    # def test_run_all_checks__runs_all_checks_in_correct_order(self):
    #     self.subject.runAllChecks()
    #
    #     self.unique_index_violation_check.runCheck.assert_any_call(self.db_connection)
    #     # add other checks here
    #     # figure out how to enforce the order of calls;
    #     # at a minimum, check the order number of the static list gpcheckcat.all_checks

    def test_running_unique_index_violation_check__makes_the_check(self):
        self.subject.runOneCheck('unique_index_violation')

        self.unique_index_violation_check.runCheck.assert_called_with(self.db_connection)

    def test_running_unique_index_violation_check__when_no_violations_are_found__passes_the_check(self):
        self.subject.runOneCheck('unique_index_violation')

        self.assertTrue(self.subject.GV.checkStatus)
        self.subject.setError.assert_not_called()

    def test_running_unique_index_violation_check__when_violations_are_found__fails_the_check(self):
        self.unique_index_violation_check.runCheck.return_value = [
            dict(table_oid=123, table_name='stephen_table', index_name='finger', column_names='c1, c2', violated_segments=[-1,8]),
            dict(table_oid=456, table_name='larry_table', index_name='stock', column_names='c1', violated_segments=[-1]),
        ]

        self.subject.runOneCheck('unique_index_violation')

        self.assertFalse(self.subject.GV.checkStatus)
        self.subject.setError.assert_any_call(self.subject.ERROR_NOREPAIR)

    def test_checkcat_report__after_running_unique_index_violations_check__reports_violations(self):
        self.unique_index_violation_check.runCheck.return_value = [
            dict(table_oid=123, table_name='stephen_table', index_name='finger', column_names='c1, c2', violated_segments=[-1,8]),
            dict(table_oid=456, table_name='larry_table', index_name='stock', column_names='c1', violated_segments=[-1]),
        ]
        self.subject.runOneCheck('unique_index_violation')

        self.subject.checkcatReport()

        expected_message1 = '    Table stephen_table has a violated unique index: finger'
        expected_message2 = '    Table larry_table has a violated unique index: stock'
        log_messages = [args[0][1] for args in self.subject.logger.log.call_args_list]
        self.assertIn(expected_message1, log_messages)
        self.assertIn(expected_message2, log_messages)

    def test_drop_leaked_schemas__when_no_leaked_schemas_exist__passes_gpcheckcat(self):
        self.subject.drop_leaked_schemas(self.leaked_schema_dropper, self.db_connection)

        self.subject.setError.assert_not_called()

    def test_drop_leaked_schemas____when_leaked_schemas_exist__finds_and_drops_leaked_schemas(self):
        self.leaked_schema_dropper.drop_leaked_schemas.return_value = ['schema1', 'schema2']

        self.subject.drop_leaked_schemas(self.leaked_schema_dropper, self.db_connection)

        self.leaked_schema_dropper.drop_leaked_schemas.assert_called_once_with(self.db_connection)

    def test_drop_leaked_schemas__when_leaked_schemas_exist__passes_gpcheckcat(self):
        self.leaked_schema_dropper.drop_leaked_schemas.return_value = ['schema1', 'schema2']

        self.subject.drop_leaked_schemas(self.leaked_schema_dropper, self.db_connection)

        self.subject.setError.assert_not_called()

    def test_drop_leaked_schemas__when_leaked_schemas_exist__reports_which_schemas_are_dropped(self):
        self.leaked_schema_dropper.drop_leaked_schemas.return_value = ['schema1', 'schema2']

        self.subject.drop_leaked_schemas(self.leaked_schema_dropper, "some_db_name")

        expected_message = "Found and dropped 2 unbound temporary schemas"
        log_messages = [args[0][1] for args in self.subject.logger.log.call_args_list]
        self.assertIn(expected_message, log_messages)

if __name__ == '__main__':
    run_tests()
