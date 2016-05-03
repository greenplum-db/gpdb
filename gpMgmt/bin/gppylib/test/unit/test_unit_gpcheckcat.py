import imp
import os
import sys

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
        self.db_connection = Mock(spec=['close', 'query'])

        self.unique_index_violation_check = Mock(spec=['run_check_table_duplication','run_check_index_duplication'])
        self.unique_index_violation_check.run_check_table_duplication.return_value = []
        self.unique_index_violation_check.run_check_index_duplication.return_value = []

        self.leaked_schema_dropper = Mock(spec=['drop_leaked_schemas'])
        self.leaked_schema_dropper.drop_leaked_schemas.return_value = []

        # MagicMock: we are choosing to trust the implementation of GV.cfg
        # If we wanted full coverage we would make this a normal Mock()
        # and fully define its behavior
        self.subject.GV.cfg = {0:dict(hostname='host0', port=123, id=1, address='123', datadir='dir', content=-1, dbid=0),
                               1:dict(hostname='host1', port=123, id=1, address='123', datadir='dir', content=1, dbid=1)}
        self.subject.GV.checkStatus = True
        self.subject.setError = Mock()

        self.apply_patches([
            patch("gpcheckcat.pg.connect", return_value=self.db_connection),
            patch("gpcheckcat.UniqueIndexViolationCheck", return_value=self.unique_index_violation_check)
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

    def test_running_unique_index_violation_table_duplication_check__makes_the_check(self):
        self.subject.runOneCheck('table_duplicate')

        self.unique_index_violation_check.run_check_table_duplication.assert_called_with(self.db_connection)

        self.assertTrue(self.subject.GV.checkStatus)
        self.subject.setError.assert_not_called()

    def test_running_unique_index_violation_table_duplication_check__when_violations_are_found__fails_the_check(self):
        self.unique_index_violation_check.run_check_table_duplication.return_value = [
            dict(table_oid=123, table_name='stephen_table', index_name='finger', column_names='c1, c2', violated_segments=[-1,8]),
            dict(table_oid=456, table_name='larry_table', index_name='stock', column_names='c1', violated_segments=[-1]),
        ]

        self.subject.runOneCheck('table_duplicate')

        self.assertFalse(self.subject.GV.checkStatus)
        self.subject.setError.assert_any_call(self.subject.ERROR_NOREPAIR)

    def test_checkcat_report__after_running_unique_index_violation_table_duplication_check__reports_violations(self):
        self.unique_index_violation_check.run_check_table_duplication.return_value = [
            dict(table_oid=123, table_name='stephen_table', index_name='finger', column_names='c1, c2', violated_segments=[-1,8]),
            dict(table_oid=456, table_name='larry_table', index_name='stock', column_names='c1', violated_segments=[-1]),
        ]
        self.subject.runOneCheck('table_duplicate')

        self.subject.checkcatReport()
        self.__assert_unique_index_violations_message()

    def test_running_unique_index_violation_index_duplication_check__makes_the_check(self):
        self.subject.runOneCheck('index_duplicate')

        self.unique_index_violation_check.run_check_index_duplication.assert_called_with(self.db_connection)

        self.assertTrue(self.subject.GV.checkStatus)
        self.subject.setError.assert_not_called()

    def test_running_unique_index_violation_index_duplication_check__when_violations_are_found__fails_the_check(self):
        self.unique_index_violation_check.run_check_index_duplication.return_value = [
            dict(table_oid=123, table_name='stephen_table', index_name='finger', column_names='c1, c2', violated_segments=[-1,8]),
            dict(table_oid=456, table_name='larry_table', index_name='stock', column_names='c1', violated_segments=[-1]),
        ]

        self.subject.runOneCheck('index_duplicate')

        self.assertFalse(self.subject.GV.checkStatus)
        self.subject.setError.assert_any_call(self.subject.ERROR_NOREPAIR)

    def test_checkcat_report__after_running_unique_index_violation_index_duplication_check__reports_violations(self):
        self.unique_index_violation_check.run_check_index_duplication.return_value = [
            dict(table_oid=123, table_name='stephen_table', index_name='finger', column_names='c1, c2', violated_segments=[-1,8]),
            dict(table_oid=456, table_name='larry_table', index_name='stock', column_names='c1', violated_segments=[-1]),
        ]
        self.subject.runOneCheck('index_duplicate')

        self.subject.checkcatReport()
        self.__assert_unique_index_violations_message()

    def __assert_unique_index_violations_message(self):
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

    def test_automatic_thread_count(self):
        self.db_connection.query.return_value.getresult.return_value = [[0]]

        self._run_batch_size_experiment(100)
        self._run_batch_size_experiment(101)

    @patch('gpcheckcat.GPCatalog', return_value=Mock())
    @patch('sys.exit')
    @patch('gpcheckcat.log_literal')
    def test_truncate_batch_size(self, mock_log, mock_gpcheckcat, mock_sys_exit):
        self.subject.GV.opt['-B'] = 300  # override the setting from available memory
        # setup conditions for 50 primaries and plenty of RAM such that max threads > 50
        primaries = [dict(hostname='host0', port=123, id=1, address='123', datadir='dir', content=-1, dbid=0, isprimary='t')]

        for i in range(1, 50):
            primaries.append(dict(hostname='host0', port=123, id=1, address='123', datadir='dir', content=1, dbid=i, isprimary='t'))
        self.db_connection.query.return_value.getresult.return_value = [['4.3']]
        self.db_connection.query.return_value.dictresult.return_value = primaries

        testargs = ['gpcrondump', '-port 1', '-R foo']

        # GOOD_MOCK_EXAMPLE for testing functionality in "__main__": put all code inside a method "main()",
        # which can then be mocked as necessary.
        with patch.object(sys, 'argv', testargs):
            self.subject.main()
            self.assertEquals(self.subject.GV.opt['-B'], len(primaries))

        #mock_log.assert_any_call(50, "Truncated batch size to number of primaries: 50")
        # I am confused that .assert_any_call() did not seem to work as expected --Larry
        last_call = mock_log.call_args_list[0][0][2]
        self.assertEquals(last_call, "Truncated batch size to number of primaries: 50")



    ####################### PRIVATE METHODS #######################
    def _run_batch_size_experiment(self, num_primaries):
        BATCH_SIZE = 4
        self.subject.GV.opt['-B'] = BATCH_SIZE
        self.num_batches = 0
        self.num_joins = 0
        self.num_starts = 0
        self.is_remainder_case = False
        for i in range(2, num_primaries):
            self.subject.GV.cfg[i] = dict(hostname='host1', port=123, id=1, address='123',
                                          datadir='dir', content=1, dbid=i)

        def count_starts():
            self.num_starts += 1

        def count_joins():
            if self.num_starts != BATCH_SIZE:
                self.is_remainder_case = True
            self.num_joins += 1
            if self.num_joins == BATCH_SIZE:
                self.num_batches += 1
                self.num_joins = 0
                self.num_starts = 0

        with patch('gpcheckcat.execThread') as mock_execThread:
            mock_execThread.return_value.cfg = self.subject.GV.cfg[0]
            mock_execThread.return_value.join.side_effect = count_joins
            mock_execThread.return_value.start.side_effect = count_starts
            self.subject.runOneCheck('persistent')

            self.assertTrue(self.num_batches > 0)
            if self.is_remainder_case:
                self.assertTrue(self.num_joins < BATCH_SIZE)

if __name__ == '__main__':
    run_tests()
