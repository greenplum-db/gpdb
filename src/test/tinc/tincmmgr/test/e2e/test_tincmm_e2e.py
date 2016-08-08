import fnmatch
import inspect
import os
import shutil
import stat

import unittest2 as unittest

import tinctest
from tinctest.lib import Gpdiff

from gppylib.commands.base import Command, REMOTE

class TINCMME2ETests(unittest.TestCase):
    """
    Test cases for tincmm through tincmm.py command line.
    Runs a tincmm.py command and verifies the changed files with expected baseline files
    through gpdiff.
    Also verifies the tincmm.py output
    """
    # Directory where the output of tincmm cmd will be generated
    out_dir = os.path.join(os.path.dirname(__file__), 'cmd_output')

    # Directory where the expected output of tincmm cmd will be stored
    expected_out_dir = os.path.join(os.path.dirname(__file__), 'expected_cmd_output')

    # Directory where all the expected modified test files are put for each test case
    expected_test_scripts_dir = os.path.join(os.path.dirname(__file__), 'expected_test_scripts')

    # Directory where the tests are located
    test_dir = os.path.join(os.path.dirname(__file__), 'sample')

    #Directory where tests for a specific test case will be generated. All files from test_dir will be copied over
    # to test_scripts_dir/<testname> upon which mm queries will be executed
    test_scripts_out_dir = os.path.join(os.path.dirname(__file__), 'tests')

    @classmethod
    def setUpClass(cls):
        # Remove and generate all required directories
        if os.path.exists(cls.out_dir):
            shutil.rmtree(cls.out_dir)
        os.makedirs(cls.out_dir)

        # Remove and generate all required directories
        if os.path.exists(cls.test_scripts_out_dir):
            shutil.rmtree(cls.test_scripts_out_dir)
        os.makedirs(cls.test_scripts_out_dir)
        

    def _create_tests(self):
        # First create a test scripts directory for this test case
        # Copy all the tests from the tests directory to the test case specific tests directory
        # upon which all mm queries will be executed against
        test_case_scripts_dir = os.path.join(self.__class__.test_scripts_out_dir, self._testMethodName)
        if os.path.exists(test_case_scripts_dir):
            shutil.rmtree(test_case_scripts_dir)
        os.makedirs(test_case_scripts_dir)

        for folder, subs, files in os.walk(self.__class__.test_dir):
            for filename in files:
                if fnmatch.fnmatch(filename, '*.pyc'):
                    continue
                file_path = os.path.abspath(os.path.join(folder, filename))
                output_file_path = os.path.join(test_case_scripts_dir,
                                                os.path.relpath(file_path, self.__class__.test_dir))
                if not os.path.exists(os.path.dirname(output_file_path)):
                    os.makedirs(os.path.dirname(output_file_path))
                # Copy the file to test scripts directory
                shutil.copyfile(file_path, output_file_path)
                os.chmod(output_file_path, stat.S_IRWXU)
        return test_case_scripts_dir
        
    def _run_test(self, tincmm_cmd):
        """
        Executes a tincmm cmd and compares the output with an expected file
        """
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_dir = os.path.join(pwd, 'sample')

        cmd = Command('tincmm', tincmm_cmd)
        cmd.run()
        result = cmd.get_results()

        self.assertEquals(result.rc, 0)

        cmd_out_file = os.path.join(self.__class__.out_dir, "%s_cmd.out" %self._testMethodName)
        cmd_expected_out_file = os.path.join(self.__class__.expected_out_dir, "%s_cmd.out" %self._testMethodName)
        with open(cmd_out_file, 'w') as f:
            f.write(result.stdout)

        basename = os.path.basename(cmd_out_file)
        sorted_out_file = '/tmp/%s_output' %basename
        sorted_expected_out_file = '/tmp/%s_expected' %basename
        cmd = Command('sort', "sort %s > %s; sort %s > %s" %(cmd_out_file, sorted_out_file, cmd_expected_out_file, sorted_expected_out_file))
        cmd.run()
        result = cmd.get_results()
        self.assertEqual(result.rc, 0, result.stdout)
        cmd = Command('diff', "diff %s %s" %(sorted_out_file, sorted_expected_out_file))
        cmd.run()
        result = cmd.get_results()
        self.assertEqual(result.rc, 0, result.stdout)

    def _compare_test_files(self):
        """
        Compares all .sql and .py files in the expected test scripts directory with the generated test scripts directory
        """
        expected_dir = os.path.join(self.__class__.expected_test_scripts_dir, self._testMethodName)
        output_dir = os.path.join(self.__class__.test_scripts_out_dir, self._testMethodName)
        # Just compare the two directories
        # gpdiff ignores comments in sql files by default and we do not want that here
        cmd = Command('diff', 'diff --exclude=*.pyc -r %s %s' %(output_dir, expected_dir))
        cmd.run()
        result = cmd.get_results()
        self.assertEqual(result.rc, 0, result.stdout)
                
    def test_simple_select(self):
        """
        Runs a select query against python tests 
        """
        tincmm_cmd = "tincmm.py -d '-s %s -p sample_tincmm_tests.py' -q 'select tags'" %(self.__class__.test_dir)
        self._run_test(tincmm_cmd)


    def test_simple_select_sql_tests(self):
        """
        Runs a select query against sql tests
        """
        tincmm_cmd = "tincmm.py -d '-s %s -p sample_tincmm_sql_tests.py' -q 'select tags'" %(self.__class__.test_dir)
        self._run_test(tincmm_cmd)

    def test_insert_new_class_metadata(self):
        """
        Insert a new metadata at the class level
        """
        test_case_scripts_dir = self._create_tests()
        tincmm_cmd = "tincmm.py -d '-s %s -p sample_tincmm*.py' -q 'insert newmetadata=newvalue'" %(test_case_scripts_dir)
        self._run_test(tincmm_cmd)
        self._compare_test_files()

    def test_insert_metadata_method_level(self):
        """
        Insert a new metadata at test method level / sql file level
        """
        test_case_scripts_dir = self._create_tests()
        tincmm_cmd = "tincmm.py -d '-s %s -p sample_tincmm*.py -q \"method=test_01 or method=test_query01 or method=test_query_nodocstring\"' -q 'insert newmetadata=newvalue' -q 'insert anothernewmd=newvalue'" %(test_case_scripts_dir)
        self._run_test(tincmm_cmd)
        self._compare_test_files()

    def test_update(self):
        """
        Update tags at both class level and sql file level
        """
        test_case_scripts_dir = self._create_tests()
        tincmm_cmd = "tincmm.py -d '-s %s -p sample_tincmm*.py' -q 'update tags=newvalue'" %(test_case_scripts_dir)
        self._run_test(tincmm_cmd)
        self._compare_test_files()

    @unittest.skip("Some bugs in delete")
    def test_delete(self):
        """
        Delete tags at both class level and sql file level
        """
        test_case_scripts_dir = self._create_tests()
        tincmm_cmd = "tincmm.py -d '-s %s -p sample_tincmm*.py' -q 'delete tags=newvalue'" %(test_case_scripts_dir)
        self._run_test(tincmm_cmd)
        self._compare_test_files()
        
