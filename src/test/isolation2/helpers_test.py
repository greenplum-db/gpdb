import unittest

from sql_isolation_testcase import (
    load_helper_file,
    parse_include_statement
)

class HelpersTest(unittest.TestCase):
    def test_load_helper_returns_expected_lines(self):
        self.assertEqual(
            load_helper_file("./test/test_file"),
            "foo\nbar"
        )

    def test_it_removes_empty_spaces_at_the_end_of_a_file(self):
        self.assertEqual(
            load_helper_file("./test/test_file_with_trailing_spaces"),
            "this\nthat"
        )
        
    def test_it_parses_an_include_line(self):
        self.assertEqual(
            parse_include_statement("include: some text;"),
            "some text"
        )

    def test_it_removes_the_semicolon_that_completes_a_command(self):
        self.assertEqual(
            parse_include_statement("include: ./test/test_file;"),
            "./test/test_file"
        )

    def test_it_handles_empty_spaces_before_the_filename(self):
        self.assertEqual(
            parse_include_statement("include:      ./test/test_file;"),
            "./test/test_file"
        )

    def test_it_strips_trailing_whitespace(self):
        self.assertEqual(
            parse_include_statement("include: ./test/test_file;  "),
            "./test/test_file"
        )

    def test_it_requires_a_semicolon_for_include_statements(self):
        def test_function():
            parse_include_statement("include: ./test/test_file")

        self.assertRaises(SyntaxError, test_function)


if __name__ == '__main__':
    unittest.main()
