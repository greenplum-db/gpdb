#!/usr/bin/env python3

import sys, os, getpass
import unittest
from gppylib.userinput import ask_create_password


class GppylibUserInputTestCase(unittest.TestCase):

    @staticmethod
    def mock_get_pass_start(values):
        def mock_get_pass(prompt):
            mock_get_pass.counter += 1
            return values[mock_get_pass.counter]
        mock_get_pass.counter = -1
        GppylibUserInputTestCase.old_std_method = getpass.getpass
        getpass.getpass = mock_get_pass

    @staticmethod
    def mock_get_pass_stop():
        getpass.getpass = GppylibUserInputTestCase.old_std_method

    def test00_verify_too_short_password(self):
        """Verify too short password is rejected"""

        stdinvals = ['abc', 'abc', 'foobar', 'foobar']
        GppylibUserInputTestCase.mock_get_pass_start(stdinvals)

        result = ask_create_password()

        GppylibUserInputTestCase.mock_get_pass_stop()

        self.assertEqual(result, 'foobar')

    def test01_verify_too_short_password(self):
        """Verify non-matching password is rejected"""

        stdinvals = ['abcdef', 'ABCDEF', 'coolio', 'coolio']
        GppylibUserInputTestCase.mock_get_pass_start(stdinvals)

        result = ask_create_password()

        GppylibUserInputTestCase.mock_get_pass_stop()

        self.assertEqual(result, 'coolio')
 
    def test02_verify_max_attempts(self):
        """Verify max number of attempts to get password results in failure"""

        stdinvals = ['a', 'a', 'b', 'b', 'c', 'c', 'd', 'd']
        GppylibUserInputTestCase.mock_get_pass_start(stdinvals)

        result = ask_create_password(max_attempts=3)

        GppylibUserInputTestCase.mock_get_pass_stop()

        self.assertIsNone(result)

    def test03_verify_min_length(self):
        """Verify minimum password length"""

        stdinvals = ['a', 'a', 'bb', 'bb', 'ccc', 'ccc', 'dddd', 'dddd']
        GppylibUserInputTestCase.mock_get_pass_start(stdinvals)

        result = ask_create_password(max_attempts=10, min_length=3)

        GppylibUserInputTestCase.mock_get_pass_stop()

        self.assertEqual(result, 'ccc')
 
 
if __name__ == "__main__":
    unittest.main()
