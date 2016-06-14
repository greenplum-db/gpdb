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

        self.violated_segments_query_result = Mock()

        self.db_connection = Mock(spec=['query'])
        self.db_connection.query.side_effect = self.mock_query_return_value

    def mock_query_return_value(self, query_string):
        if query_string == UniqueIndexViolationCheck.unique_indexes_query:
            return self.index_query_result
        else:
            return self.violated_segments_query_result

    def test_run_check__when_there_are_no_issues(self):
        self.violated_segments_query_result.getresult.return_value = []

        violations = self.subject.runCheck(self.db_connection)

        self.assertEqual(len(violations), 0)

    def test_run_check__when_index_is_violated(self):
        self.violated_segments_query_result.getresult.side_effect = [
            [(-1,), (0,), (1,)],
            [(-1,)]
        ]

        violations = self.subject.runCheck(self.db_connection)

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
