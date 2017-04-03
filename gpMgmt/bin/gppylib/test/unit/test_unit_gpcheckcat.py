import imp
import logging
import os
import sys

from mock import *

from gp_unittest import *
from gppylib.gpcatalog import GPCatalogTable

class GpCheckCatTestCase(GpTestCase):
    def setUp(self):
        # because gpcheckcat does not have a .py extension, we have to use imp to import it
        # if we had a gpcheckcat.py, this is equivalent to:
        #   import gpcheckcat
        #   self.subject = gpcheckcat
        gpcheckcat_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpcheckcat")
        self.subject = imp.load_source('gpcheckcat', gpcheckcat_file)

        self.db_connection = Mock(spec=['close', 'query'])
        self.unique_index_violation_check = Mock(spec=['runCheck'])
        self.foreign_key_check = Mock(spec=['runCheck', 'checkTableForeignKey'])
        self.apply_patches([
            patch("gpcheckcat.pg.connect", return_value=self.db_connection),
            patch("gpcheckcat.UniqueIndexViolationCheck", return_value=self.unique_index_violation_check),
            patch("gpcheckcat.ForeignKeyCheck", return_value=self.foreign_key_check),
            patch('os.environ', new={}),
            patch('pygresql.pgdb'),
        ])

        self.subject.logger = Mock(spec=['log', 'info', 'debug', 'error', 'fatal'])
        self.unique_index_violation_check.runCheck.return_value = []
        self.leaked_schema_dropper = Mock(spec=['drop_leaked_schemas'])
        self.leaked_schema_dropper.drop_leaked_schemas.return_value = []

        issues_list = dict()
        issues_list['cat1'] = [('pg_class', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')]),
                              ('arbitrary_catalog_table', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')])]
        issues_list['cat2'] = [('pg_type', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')]),
                               ('arbitrary_catalog_table', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')])]
        self.foreign_key_check.runCheck.return_value = issues_list

        self.subject.GV.cfg = {0:dict(hostname='host0', port=123, id=1, address='123', datadir='dir', content=-1, dbid=0),
                               1:dict(hostname='host1', port=123, id=1, address='123', datadir='dir', content=1, dbid=1)}
        self.subject.GV.checkStatus = True
        self.subject.GV.foreignKeyStatus = True
        self.subject.GV.missingEntryStatus = True
        self.subject.setError = Mock()
        self.subject.print_repair_issues = Mock()


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

    @patch('gpcheckcat_modules.repair.Repair', return_value=Mock())
    @patch('gpcheckcat_modules.repair.Repair.create_repair_for_extra_missing', return_value="/tmp")
    def test_do_repair_for_extra__issues_repair(self, mock1, mock2):
        issues = {("pg_class", "oid"):"extra"}
        self.subject.GV.opt['-E'] = True
        self.subject.do_repair_for_extra(issues)
        self.subject.setError.assert_any_call(self.subject.ERROR_REMOVE)
        self.subject.print_repair_issues.assert_any_call("/tmp")

    @patch('gpcheckcat.removeFastSequence')
    @patch('gpcheckcat.processForeignKeyResult')
    def test_checkForeignKey__with_arg_gp_fastsequence(self, process_foreign_key_mock,fast_seq_mock):
        cat_mock = Mock()
        self.subject.GV.catalog = cat_mock

        gp_fastsequence_issue = dict()
        gp_fastsequence_issue['gp_fastsequence'] = [('pg_class', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')]),
                               ('arbitrary_catalog_table', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')])]
        gp_fastsequence_issue['cat2'] = [('pg_type', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')]),
                               ('arbitrary_catalog_table', ['pkey1', 'pkey2'], [('r1', 'r2'), ('r3', 'r4')])]
        self.foreign_key_check.runCheck.return_value = gp_fastsequence_issue

        cat_tables = ["input1", "input2"]
        self.subject.checkForeignKey(cat_tables)

        self.assertEquals(cat_mock.getCatalogTables.call_count, 0)
        self.assertFalse(self.subject.GV.checkStatus)
        self.assertTrue(self.subject.GV.foreignKeyStatus)
        self.subject.setError.assert_any_call(self.subject.ERROR_REMOVE)
        self.foreign_key_check.runCheck.assert_called_once_with(cat_tables)
        fast_seq_mock.assert_called_once_with(self.db_connection)

    @patch('gpcheckcat.processForeignKeyResult')
    def test_checkForeignKey__with_arg(self, process_foreign_key_mock):
        cat_mock = Mock()
        self.subject.GV.catalog = cat_mock

        cat_tables = ["input1", "input2"]
        self.subject.checkForeignKey(cat_tables)

        self.assertEquals(cat_mock.getCatalogTables.call_count, 0)
        self.assertFalse(self.subject.GV.checkStatus)
        self.assertTrue(self.subject.GV.foreignKeyStatus)
        self.subject.setError.assert_any_call(self.subject.ERROR_NOREPAIR)
        self.foreign_key_check.runCheck.assert_called_once_with(cat_tables)

    @patch('gpcheckcat.processForeignKeyResult')
    def test_checkForeignKey__no_arg(self, process_foreign_key_mock):
        cat_mock = Mock(spec=['getCatalogTables'])
        cat_tables = ["input1", "input2"]
        cat_mock.getCatalogTables.return_value = cat_tables
        self.subject.GV.catalog = cat_mock

        self.subject.checkForeignKey()
        self.assertEquals(cat_mock.getCatalogTables.call_count, 1)
        self.assertFalse(self.subject.GV.checkStatus)
        self.assertTrue(self.subject.GV.foreignKeyStatus)
        self.subject.setError.assert_any_call(self.subject.ERROR_NOREPAIR)
        self.foreign_key_check.runCheck.assert_called_once_with(cat_tables)

    # Test gpcheckat -C option with checkForeignKey
    @patch('gpcheckcat.GPCatalog', return_value=Mock())
    @patch('sys.exit')
    @patch('gpcheckcat.checkTableMissingEntry')
    def test_runCheckCatname__for_checkForeignKey(self, mock1, mock2, mock3):
        self.subject.checkForeignKey = Mock()
        gpcat_class_mock = Mock(spec=['getCatalogTable'])
        cat_obj_mock = Mock()
        self.subject.getCatObj = gpcat_class_mock
        self.subject.getCatObj.return_value = cat_obj_mock
        primaries = [dict(hostname='host0', port=123, id=1, address='123', datadir='dir', content=-1, dbid=0, isprimary='t')]

        for i in range(1, 50):
            primaries.append(dict(hostname='host0', port=123, id=1, address='123', datadir='dir', content=1, dbid=i, isprimary='t'))
        self.db_connection.query.return_value.getresult.return_value = [['4.3']]
        self.db_connection.query.return_value.dictresult.return_value = primaries

        self.subject.GV.opt['-C'] = 'pg_class'

        testargs = ['gpcheckcat', '-port 1', '-C pg_class']
        with patch.object(sys, 'argv', testargs):
            self.subject.main()

        self.subject.getCatObj.assert_called_once_with(' pg_class')
        self.subject.checkForeignKey.assert_called_once_with([cat_obj_mock])

    def test_mirror_matching_success_sets_status_and_error(self):
        with patch('gpcheckcat_modules.mirror_matching_check.MirrorMatchingCheck.run_check') as mirrorMatchingCheckMock:
            mirrorMatchingCheckMock.return_value = []

            self.subject.runOneCheck('mirroring_matching')

            mirrorMatchingCheckMock.assert_called_once_with(self.db_connection, self.subject.logger)
            self.assertTrue(self.subject.GV.checkStatus)
            self.assertEqual(self.subject.setError.call_count, 0)

    def test_mirror_matching_failure_sets_status_and_error(self):
        with patch('gpcheckcat_modules.mirror_matching_check.MirrorMatchingCheck.run_check') as mirrorMatchingCheckMock:
            mirrorMatchingCheckMock.return_value = [1]  # failure, a list of segments that are mismatched

            self.subject.runOneCheck('mirroring_matching')

            mirrorMatchingCheckMock.assert_called_once_with(self.db_connection, self.subject.logger)
            self.assertFalse(self.subject.GV.checkStatus)
            self.subject.setError.assert_called_once_with(self.subject.ERROR_NOREPAIR)

    def test_mirror_matching_exception(self):
        self.subject.logger.info.side_effect = Exception('Boom!')

        self.subject.runOneCheck("mirroring_matching")

        log_messages = [args[0][1] for args in self.subject.logger.log.call_args_list]
        self.assertIn("  Execution error: Boom!", log_messages)
        self.assertFalse(self.subject.GV.checkStatus)
        self.subject.setError.assert_called_once_with(self.subject.ERROR_NOREPAIR)


    @patch('gpcheckcat.checkTableMissingEntry', return_value = None)
    def test_checkMissingEntry__no_issues(self, mock1):
        cat_mock = Mock()
        cat_tables = ["input1", "input2"]
        cat_mock.getCatalogTables.return_value = cat_tables
        self.subject.GV.catalog = cat_mock

        self.subject.runOneCheck("missing_extraneous")

        self.assertTrue(self.subject.GV.missingEntryStatus)
        self.subject.setError.assert_not_called()

    @patch('gpcheckcat.checkTableMissingEntry', return_value= {("pg_clas", "oid"): "extra"})
    @patch('gpcheckcat.getPrimaryKeyColumn', return_value = (None,"oid"))
    def test_checkMissingEntry__uses_oid(self, mock1, mock2):

        self.subject.GV.opt['-E'] = True
        aTable = Mock(spec=GPCatalogTable)

        cat_mock = Mock()
        cat_mock.getCatalogTables.return_value = [aTable]
        self.subject.GV.catalog = cat_mock

        self.subject.runOneCheck("missing_extraneous")

        self.assertEquals(aTable.getPrimaryKey.call_count, 1)
        self.subject.setError.assert_called_once_with(self.subject.ERROR_REMOVE)

    @patch('gpcheckcat.checkTableMissingEntry', return_value= {("pg_operator", "typename, typenamespace"): "extra"})
    @patch('gpcheckcat.getPrimaryKeyColumn', return_value = (None,None))
    def test_checkMissingEntry__uses_pkeys(self, mock1, mock2):

        self.subject.GV.opt['-E'] = True
        aTable = MagicMock(spec=GPCatalogTable)
        aTable.tableHasConsistentOids.return_value = False

        cat_mock = Mock()
        cat_mock.getCatalogTables.return_value = [aTable]
        self.subject.GV.catalog = cat_mock

        self.subject.runOneCheck("missing_extraneous")

        self.assertEquals(aTable.getPrimaryKey.call_count, 1)
        self.subject.setError.assert_called_once_with(self.subject.ERROR_REMOVE)

    def test_getReportConfiguration_uses_contentid(self):
        report_cfg = self.subject.getReportConfiguration()
        self.assertEqual("content -1", report_cfg[-1]['segname'])

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
