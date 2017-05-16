#!/usr/bin/env python

import unittest

from gppylib.operations.test.regress.test_package import GppkgTestCase, GppkgSpec, BuildGppkg, RPMSpec, BuildRPM, run_command, run_remote_command

class SimpleGppkgTestCase(GppkgTestCase):
    """Covers simple build/install/remove/update test cases"""
    def test00_simple_build(self):
        self.build(self.alpha_spec, self.A_spec)

    def test01_simple_install(self):
        gppkg_file = self.alpha_spec.get_filename() 
        self.install(gppkg_file)

        #Check RPM database
        self.check_rpm_install(self.A_spec.get_package_name())

    def test02_simple_update(self):
        gppkg_file = self.alpha_spec.get_filename() 
        self.install(gppkg_file)

        update_rpm_spec = RPMSpec("A", "1", "2")
        update_gppkg_spec = GppkgSpec("alpha", "1.1")
        update_gppkg_file = self.build(update_gppkg_spec, update_rpm_spec)
   
        self.update(update_gppkg_file)

        #Check for the packages
        self.check_rpm_install(update_rpm_spec.get_package_name())

    def test03_simple_uninstall(self):
        gppkg_file = self.alpha_spec.get_filename()

        self.install(gppkg_file)
        self.remove(gppkg_file)

        results = run_command("gppkg -q --all")
        results = results.split('\n')[self.start_output:self.end_output]
        
        self.assertEqual(results, [])
       
    def test04_help(self):
        help_options = ["--help", "-h", "-?"] 

        for opt in help_options:
            results = run_command("gppkg " + opt)
            self.assertNotEqual(results, "")

    def test05_version(self):
        results = run_command("gppkg --version")
        self.assertNotEqual(results, "")

if __name__ == "__main__":
    unittest.main()
