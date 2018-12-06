import unittest

from sql_isolation_testcase import load_helper_file_from_sql_command

class HelpersTest(unittest.TestCase):
    def test_load_helper_returns_expected_lines(self):
        self.assertEqual(
            load_helper_file_from_sql_command("./test/test_file"),
            "foo\nbar"
        )

    def test_it_removes_the_semicolon_that_completes_a_command(self):
        self.assertEqual(
            load_helper_file_from_sql_command("./test/test_file;"),
            "foo\nbar"
        )

    def test_it_removes_empty_spaces_at_the_end_of_a_file(self):
        self.assertEqual(
            load_helper_file_from_sql_command("./test/test_file_with_trailing_spaces;"),
            "this\nthat"
        )
        
    def test_it_handles_empty_spaces_before_the_filename(self):
        self.assertEqual(
            load_helper_file_from_sql_command("   ./test/test_file;"),
            "foo\nbar"
        )


if __name__ == '__main__':
    unittest.main()