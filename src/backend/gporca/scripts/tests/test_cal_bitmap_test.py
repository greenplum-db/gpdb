import unittest
from unittest.mock import patch
from unittest.mock import Mock
from unittest.mock import MagicMock

from itertools import count

import cal_bitmap_test
from cal_bitmap_test import connect
from cal_bitmap_test import find_crossover
from cal_bitmap_test import explain_index_scan
from cal_bitmap_test import explain_join_scan

class TestStringMethods(unittest.TestCase):

    @patch('gppylib.db.dbconn.query')
    def test_explain_index_scan(self, mock_query):
        mock_query.return_value = Mock()
        mock_query.return_value.fetchall.return_value = [
            [" Gather Motion 3:1  (slice1; segments: 3)  (cost=0.00..8.04 rows=1 width=4)"],
            ["   ->  Index Scan using cal_txtest_i_bitmap_10 on cal_txtest  (cost=0.00..8.02 rows=1 width=4)"],
            ["         Index Cond: (bitmap10 > 42)"]
        ]

        (scan, cost) = explain_index_scan(Mock(), "mock sql query string")
        self.assertEqual(scan, cal_bitmap_test.INDEX_SCAN)
        self.assertEqual(cost, 8.02)

    @patch('gppylib.db.dbconn.query')
    def test_explain_index_only_scan(self, mock_query):
        mock_query.return_value = Mock()
        mock_query.return_value.fetchall.return_value = [
            [" Gather Motion 3:1  (slice1; segments: 3)  (cost=0.00..8.04 rows=1 width=4)"],
            ["   ->  Index Only Scan using cal_txtest_i_bitmap_10 on cal_txtest  (cost=0.00..8.02 rows=1 width=4)"],
            ["         Index Cond: (bitmap10 > 42)"]
        ]

        (scan, cost) = explain_index_scan(Mock(), "mock sql query string")
        self.assertEqual(scan, cal_bitmap_test.INDEX_ONLY_SCAN)
        self.assertEqual(cost, 8.02)

    @patch('gppylib.db.dbconn.query')
    def test_explain_join_index_scan(self, mock_query):
        mock_query.return_value = Mock()
        mock_query.return_value.fetchall.return_value = [
            [" Gather Motion 3:1  (slice1; segments: 3)  (cost=0.00..437.00 rows=1 width=4)"],
            ["   ->  Nested Loop  (cost=0.00..437.00 rows=1 width=4)"],
            ["         Join Filter: true"],
            ["         ->  Seq Scan on cal_txtest  (cost=0.00..431.00 rows=1 width=4)"],
            ["         ->  Index Scan using cal_txtest_a_idx on cal_txtest cal_txtest_1  (cost=0.00..6.00 rows=1 width=1)"],
            ["               Index Cond: (a = cal_txtest.a)"]
        ]

        (scan, cost) = explain_join_scan(Mock(), "mock sql query string")
        self.assertEqual(scan, cal_bitmap_test.INDEX_SCAN)
        self.assertEqual(cost, 437.00)

    @patch('gppylib.db.dbconn.query')
    def test_explain_join_index_only_scan(self, mock_query):
        mock_query.return_value = Mock()
        mock_query.return_value.fetchall.return_value = [
            [" Gather Motion 3:1  (slice1; segments: 3)  (cost=0.00..437.00 rows=1 width=4)"],
            ["   ->  Nested Loop  (cost=0.00..437.00 rows=1 width=4)"],
            ["         Join Filter: true"],
            ["         ->  Broadcast Motion 3:3  (slice2; segments: 3)  (cost=0.00..431.00 rows=1 width=4)"],
            ["               ->  Seq Scan on cal_txtest  (cost=0.00..431.00 rows=1 width=4)"],
            ["         ->  Index Only Scan using cal_txtest_a_idx on cal_txtest cal_txtest_1  (cost=0.00..6.00 rows=1 width=1)"],
            ["               Index Cond: (a = cal_txtest.a)"]
        ]

        (scan, cost) = explain_join_scan(Mock(), "mock sql query string")
        self.assertEqual(scan, cal_bitmap_test.INDEX_ONLY_SCAN)
        self.assertEqual(cost, 437.00)

    @patch('cal_bitmap_test.timed_execute_and_check_timeout')
    @patch('cal_bitmap_test.execute_sql')
    def test_x(self, mock_execute, mock_execute_and_check_timeout):
        mock_conn = Mock()
        mock_setup = Mock()
        mock_parameterizeMethod = Mock()
        mock_explain_method = Mock()
        mock_explain_method.side_effect = [(cal_bitmap_test.INDEX_ONLY_SCAN, 1.1),
                                           (cal_bitmap_test.INDEX_SCAN, 2.1),
                                           (cal_bitmap_test.INDEX_ONLY_SCAN, 1.2),
                                           (cal_bitmap_test.INDEX_SCAN, 2.2),
                                           (cal_bitmap_test.INDEX_ONLY_SCAN, 1.3),
                                           (cal_bitmap_test.INDEX_SCAN, 2.3)
        ]
        mock_reset_method = Mock()
        plan_ids = [cal_bitmap_test.INDEX_ONLY_SCAN, cal_bitmap_test.INDEX_SCAN]
        mock_force_methods = MagicMock()

        explainDict, execDict, errMessages = find_crossover(mock_conn, 0, 2, mock_setup, mock_parameterizeMethod, mock_explain_method,
                                                            mock_reset_method, plan_ids, mock_force_methods, 1)
        self.assertEqual(explainDict, {0: ('indexonly_scan', 1.1, 1.2, 1.3), 1: ('index_scan', 2.1, 2.2, 2.3)})

if __name__ == '__main__':
    unittest.main()
