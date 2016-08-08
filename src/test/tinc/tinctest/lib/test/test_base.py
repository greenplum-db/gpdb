import os
from tinctest.lib import local_path
import unittest2 as unittest

class PathTestCase(unittest.TestCase):
    def test_local_path(self):
        filename = local_path('some_file_test_that_lives_in_my_current_working_directory')
        assert filename == os.path.join(os.path.dirname(__file__), 'some_file_test_that_lives_in_my_current_working_directory')
