from mock import *

from gp_unittest import *
from gpcheckcat_modules.unique_index_violation_check import UniqueIndexViolationCheck


class UniqueIndexViolationCheckTestCase(GpTestCase):
    def setUp(self):
        self.subject = UniqueIndexViolationCheck()

        self.index_query_result = Mock()
        self.index_query_result.getresult.return_value = [
            (9001, 'index1', 'table1', '{index1_column1,index1_column2}'),
            (9001, 'index2', 'table1', '{index2_column1,index2_column2}')
        ]

        self.readindex_checks_query_result = Mock()
        self.readindex_checks_query_result.getresult.return_value = []

        self.gp_toolkit_exists_query_result = Mock()
        self.gp_toolkit_exists_query_result.getresult.return_value = [
            ('1')
        ]

        self.violated_segments_query_result = Mock()

        self.db_connection = Mock(spec=['query'], db='some_db')
        self.db_connection.query.side_effect = self.mock_query_return_value

    def mock_query_return_value(self, query_string):
        if query_string == UniqueIndexViolationCheck.unique_indexes_query:
            return self.index_query_result
        elif query_string == UniqueIndexViolationCheck.check_readindex_function_exists_sql  \
                or \
             query_string == UniqueIndexViolationCheck.check_readindex_type_exists_sql:
            return self.readindex_checks_query_result
        elif query_string == UniqueIndexViolationCheck.check_gp_toolkit_schema_exists_sql:
             return self.gp_toolkit_exists_query_result
        else:
            return self.violated_segments_query_result

    def test_run_check_index_duplication__when_gp_toolkit_schema_does_not_exist(self):
        self.gp_toolkit_exists_query_result.getresult.return_value = []
        with self.assertRaises(Exception) as context:
            self.subject.check_gp_toolkit_schema_exists(self.db_connection)

        self.assertEquals("Schema gp_toolkit does not exist in database 'some_db'. "
                          "Please contact support to create this schema and re-run gpcheckcat.", str(context.exception))

    def test_run_check_index_duplication__when_readindex_function_already_exists(self):
        self.readindex_checks_query_result.getresult.return_value = [('1')]
        with self.assertRaises(Exception) as context:
            self.subject.check_readindex_function_exists(self.db_connection)

        self.assertEquals("Function readindex(oid,bool) already exists in schema gp_toolkit in database 'some_db'. "
                          "Please drop the function and re-run gpcheckcat.", str(context.exception))

    def test_run_check_index_duplication__when_type_name_already_exists(self):
        self.readindex_checks_query_result.getresult.return_value = [('1')]
        with self.assertRaises(Exception) as context:
            self.subject.check_readindex_type_exists(self.db_connection)

        self.assertEquals("Type readindex_type already exists in schema gp_toolkit in database 'some_db'. "
                          "Please drop the type and re-run gpcheckcat.", str(context.exception))

    def test_run_check_index_duplication__when_type_name_does_not_exist(self):
        self.subject.check_readindex_function_exists(self.db_connection)


    def test_run_check_index_duplication__when_readindex_function_does_not_exist(self):
        self.subject.check_readindex_function_exists(self.db_connection)

    def test_run_check_table_duplication__when_there_are_no_issues(self):
        self.violated_segments_query_result.getresult.return_value = []
        violations = self.subject.run_check_table_duplication(self.db_connection)
        self.assertEqual(len(violations), 0)

    def test_run_check_index_duplication__when_there_are_no_issues(self):
        self.violated_segments_query_result.getresult.return_value = []
        violations = self.subject.run_check_index_duplication(self.db_connection)
        self.assertEqual(len(violations), 0)

    def test_run_check_table_duplication__when_index_is_violated(self):
        self.violated_segments_query_result.getresult.side_effect = [
            [(-1,), (0,), (1,)],
            [(-1,)]
        ]
        violations = self.subject.run_check_table_duplication(self.db_connection)
        self.__assert_violations(violations)

    def test_run_check_index_duplication__when_index_is_violated(self):
        self.violated_segments_query_result.getresult.side_effect = [
            [(-1,), (0,), (1,)],
            [(-1,)]
        ]
        violations = self.subject.run_check_index_duplication(self.db_connection)
        self.__assert_violations(violations)

    def __assert_violations(self, violations):
        self.assertEqual(len(violations), 2)
        self.assertEqual(violations[0]['table_oid'], 9001)
        self.assertEqual(violations[0]['table_name'], 'table1')
        self.assertEqual(violations[0]['index_name'], 'index1')
        self.assertEqual(violations[0]['column_names'], 'index1_column1,index1_column2')
        self.assertEqual(violations[0]['violated_segments'], [-1, 0, 1])

        self.assertEqual(violations[1]['table_oid'], 9001)
        self.assertEqual(violations[1]['table_name'], 'table1')
        self.assertEqual(violations[1]['index_name'], 'index2')
        self.assertEqual(violations[1]['column_names'], 'index2_column1,index2_column2')
        self.assertEqual(violations[1]['violated_segments'], [-1])

if __name__ == '__main__':
    run_tests()
