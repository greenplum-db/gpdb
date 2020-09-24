#!/usr/bin/env python3

import os
import tarfile

from contextlib import closing
from gppylib.commands.base import ExecutionError
from gppylib.operations.test.regress.test_package import GppkgTestCase, unittest, RPMSpec, GppkgSpec, run_command, OS, GPDB_VERSION

class QueryTestCase(GppkgTestCase):
    """Test the query option of gppkg"""
    def setUp(self):
        self.A_spec = RPMSpec("A", "1", "1")    
        self.alpha_spec = GppkgSpec("alpha", "1.0")
 
        self.B_spec = RPMSpec("B", "1", "1")
        self.beta_spec = GppkgSpec("beta", "1.0")

        self.alpha_info = ['Name alpha', 'Version 1.0', 'Architecture x86_64', 'OS %s' % OS, 'GPDBVersion %s' % GPDB_VERSION, 'Description Temporary Test Package']

    def tearDown(self):
        """
            Overriding the teardown here as successive tests
            in the suite make use of gppkg installed in the
            previous test.
        """
        pass

    def check_info_results(self, output, expected_info_result):
        self.assertTrue(len(output.split('\n')) > 1)
        output = self.remove_timestamp(output)
        self.assertTrue(output == expected_info_result)

    def check_list_results(self, output, expected_list_result):
        self.assertTrue(len(output.split('\n')) > 1)
        output = output.split('\n')[self.start_output:self.end_output]
        self.assertTrue(output == expected_list_result)
  
    def get_file_list(self, gppkg_filename):
        file_list = []
        with closing(tarfile.open(gppkg_filename, 'r:gz')) as tarinfo:
            file_list = tarinfo.getnames()
        return file_list

    def test00_install(self):
        gppkg_file1 = self.build(self.alpha_spec, self.A_spec)
        gppkg_file2 = self.build(self.beta_spec, self.B_spec)

        self.install(gppkg_file1) 
        self.install(gppkg_file2)       
  
    def test01_query_all(self):
        
        results = run_command("gppkg -q --all")
        self.assertTrue(results.split('\n')[self.start_output:self.end_output].sort() == [self.alpha_spec.get_package_name(), self.beta_spec.get_package_name()].sort())

        results = run_command("gppkg --all -q")
        self.assertTrue(results.split('\n')[self.start_output:self.end_output].sort() == [self.alpha_spec.get_package_name(), self.beta_spec.get_package_name()].sort())

        self.assertRaises(ExecutionError, run_command, "gppkg -qall")

    def test02_query_info(self):
        #Normal order of the options
        results = run_command("gppkg -q --info %s" % self.alpha_spec.get_filename())
        self.check_info_results(results, self.alpha_info)

        #Reverse order of the options
        results = run_command("gppkg --info -q %s" % self.alpha_spec.get_filename())
        self.check_info_results(results, self.alpha_info)

    def test03_query_list(self):
        expected_list_result = self.get_file_list(self.alpha_spec.get_filename()) 

        results = run_command("gppkg -q --list %s" % self.alpha_spec.get_filename())
        self.check_list_results(results, expected_list_result)
        
        results = run_command("gppkg --list -q %s" % self.alpha_spec.get_filename())
        self.check_list_results(results, expected_list_result)

    def test04_query_info_expanded(self):
        results = run_command("gppkg --query --info %s" % self.alpha_spec.get_filename())
        self.check_info_results(results, self.alpha_info)

        results = run_command("gppkg --info --query %s" % self.alpha_spec.get_filename())
        self.check_info_results(results, self.alpha_info)

        with self.assertRaises(ExecutionError):
            results = run_command("gppkg --query --info abcde")

    def test05_query_list_expanded(self):
        expected_list_result = self.get_file_list(self.alpha_spec.get_filename()) 
    
        results = run_command("gppkg --query --list %s" % self.alpha_spec.get_filename())
        self.check_list_results(results, expected_list_result)
        
        results = run_command("gppkg --list --query %s" % self.alpha_spec.get_filename())
        self.check_list_results(results, expected_list_result)

        with self.assertRaises(ExecutionError):
            run_command("gppkg --query --list abcde")
   
    def test06_query_package(self):
        results = run_command("gppkg -q %s" % self.alpha_spec.get_filename())
        self.assertTrue("%s is installed" % self.alpha_spec.get_filename().split('-')[0] in results)

        results = run_command("gppkg --query %s" % self.alpha_spec.get_filename())
        self.assertTrue("%s is installed" % self.alpha_spec.get_filename().split('-')[0] in results)

        self.remove(self.alpha_spec.get_filename())
        results = run_command("gppkg --query %s" % self.alpha_spec.get_filename())
        self.assertTrue("%s is not installed" % self.alpha_spec.get_filename().split('-')[0] in results)

        with self.assertRaises(ExecutionError):
            run_command("gppkg --query abcde")
            
    def test07_clean_up(self):
        self.cleanup()

if __name__ == "__main__":
    unittest.main()
