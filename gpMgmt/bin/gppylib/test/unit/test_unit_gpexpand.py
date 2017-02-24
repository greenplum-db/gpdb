import os
import imp

from gp_unittest import *


class GpExpand(GpTestCase):

    def setUp(self):
        # because gpexpand does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpexpand.py, this is equivalent to:
        #   import gpexpand
        #   self.subject = gpexpand
        gpexpand_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpexpand")
        self.subject = imp.load_source('gpexpand', gpexpand_file)

    def tearDown(self):
        pass

    def test_PrepFileSpaces_issues_correct_postgres_command(self):
        prep_file_spaces = self.subject.PrepFileSpaces("name", [""], [""], "foo", 1, 1)

        self.assertIn("--gp_contentid=", prep_file_spaces.cmdStr)
        self.assertIn("--gp_num_contents_in_cluster=", prep_file_spaces.cmdStr)
        self.assertIn("--gp_dbid=", prep_file_spaces.cmdStr)


if __name__ == '__main__':
    run_tests()
