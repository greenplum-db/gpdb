import os

import unittest2 as unittest

from tinctest.lib import Gpdiff

class GpdiffTestCase(unittest.TestCase):

    def test_without_ignore_plans(self):
        file1 = os.path.join(os.path.dirname(__file__), 'test1.out')
        file2 = os.path.join(os.path.dirname(__file__), 'test2.out')

        self.assertFalse(Gpdiff.are_files_equal(file1, file2))

    def test_with_ignore_plans(self):
        file1 = os.path.join(os.path.dirname(__file__), 'test1.out')
        file2 = os.path.join(os.path.dirname(__file__), 'test2.out')

        self.assertTrue(Gpdiff.are_files_equal(file1, file2, ignore_plans = True))

    def test_ignore_comments(self):
        file1 = os.path.join(os.path.dirname(__file__), 'test3.out')
        file2 = os.path.join(os.path.dirname(__file__), 'test3.ans')

        self.assertTrue(Gpdiff.are_files_equal(file1, file2))

    def test_ignore_settings_and_rows_in_plans(self):
        """
        Test whether gpdiff automatically ignores Settings: and number of rows in plan diffs.
        Two plans differing only in 'Settings:' and 'number of rows' should be consdiered the same.
        """
        file1 = os.path.join(os.path.dirname(__file__), 'explain_with_settings.out')
        file2 = os.path.join(os.path.dirname(__file__), 'explain_without_settings.out')

        self.assertTrue(Gpdiff.are_files_equal(file1, file2))

        # However other diffs should be considered
        file3 = os.path.join(os.path.dirname(__file__), 'explain_with_optimizer.out')
        self.assertFalse(Gpdiff.are_files_equal(file1, file3))
        self.assertFalse(Gpdiff.are_files_equal(file2, file3))
        
        
        
        
