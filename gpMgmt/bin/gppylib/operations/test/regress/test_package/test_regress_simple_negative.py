#!/usr/bin/env python3

import os 
import tarfile

from gppylib.commands.base import ExecutionError
from gppylib.operations.test.regress.test_package import GppkgTestCase, unittest, GppkgSpec, RPMSpec, ARCH, OS, GPDB_VERSION

class SimpleNegativeTestCase(GppkgTestCase):
#    @unittest.expectedFailure
    def test00_wrong_os(self):
        os = "abcde"
        A_spec = self.A_spec 
        alpha_spec = GppkgSpec("alpha", "1.0", GPDB_VERSION, os)
        gppkg_file = self.build(alpha_spec, A_spec) 

        with self.assertRaisesRegex(ExecutionError , "%s OS required. %s OS found" % (os, OS)):
            self.install(gppkg_file)
       
    def test01_wrong_arch(self):
        arch = "abcde"
        A_spec = self.A_spec 
        alpha_spec = GppkgSpec("alpha", "1.0", GPDB_VERSION, OS, arch)
        gppkg_file = self.build(alpha_spec, A_spec) 

        with self.assertRaisesRegex(ExecutionError, "%s Arch required. %s Arch found" % (arch, ARCH)):
            self.install(gppkg_file)

    def test02_wrong_gpdbversion(self):
        gpdb_version = "4.6"
        A_spec = self.A_spec 
        alpha_spec = GppkgSpec("alpha", "1.0", gpdb_version)
        gppkg_file = self.build(alpha_spec, A_spec)

        with self.assertRaisesRegex(ExecutionError, "requires Greenplum Database version %s" % gpdb_version):
            self.install(gppkg_file)

    def test03_install_twice(self):
        gppkg_file = self.build(self.alpha_spec, self.A_spec)

        self.install(self.alpha_spec.get_filename())
 
        with self.assertRaisesRegex(ExecutionError, "%s is already installed" % gppkg_file):
            self.install(gppkg_file)

    @unittest.expectedFailure
    def test04_update_gppkg_lower(self):
        """
        This test tries to update a gppkg which has a lower version, 
        but the main comprising rpm is of higher version than the one
        already installed on the system.
        """
        #Use gppkg from previous test
        self.install(self.alpha_spec.get_filename())

        #Use gppkg which has a lower version, but the rpm is > the one installed
        update_rpm_spec = RPMSpec("A", "1", "2")
        update_gppkg_spec = GppkgSpec("alpha", "0.1")
        update_gppkg_file = self.build(update_gppkg_spec, update_rpm_spec)
       
        with self.assertRaisesRegex(ExecutionError, "Newer version of %s already installed" % update_gppkg_spec.get_package_name()):
            self.update(update_gppkg_file)
        #Check that the original package is still installed and not updated
        assert self.check_install(self.alpha_spec.get_filename())

    def test05_update_rpm_lower(self):
        """
        This test tries to install a gppkg which has a higher version,
        but the main comprising rpm is of lower version than the one
        already installed on the system.
        """
        #Use gppkg from previous test
        self.install(self.alpha_spec.get_filename())

        #Use gppkg with a lower RPM version but gppkg version is > the one installed
        update_rpm_spec = RPMSpec("A", "1", "0")
        update_gppkg_spec = GppkgSpec("alpha", "1.1") 
        update_gppkg_file = self.build(update_gppkg_spec, update_rpm_spec)
        
        with self.assertRaisesRegex(ExecutionError, self.A_spec.get_filename()):
            self.update(update_gppkg_file)
        #Check that the original package is still installed and not updated
        assert self.check_install(self.alpha_spec.get_filename())

    def test06_uninstall_twice(self):
        #Uses the gppkg from previous test
        self.install(self.alpha_spec.get_filename())

        #Uninstall gppkg 
        self.remove(self.alpha_spec.get_filename())

        with self.assertRaisesRegex(ExecutionError, "%s has not been installed" % self.alpha_spec.get_package_name()):
            self.remove(self.alpha_spec.get_filename())

    def test07_invalid_gppkg_name(self):
        invalid_gppkg_name = "abcde-abc"
        with self.assertRaisesRegex(ExecutionError, "Cannot find package %s" % invalid_gppkg_name):
            self.install(invalid_gppkg_name)

    @unittest.expectedFailure
    def test08_wrong_os_update(self):
        os = "windows"
        self.install(self.alpha_spec.get_filename())
        
        invalid_os_gppkg = GppkgSpec("alpha", "1.1", GPDB_VERSION, os)
        gppkg_file = self.build(invalid_os_gppkg, self.A_spec)
        
        with self.assertRaisesRegex(ExecutionError, "%s os required. %s os found" % (os, OS)):
            self.update(gppkg_file)

    def test09_wrong_arch_update(self):
        arch = "abcde"
        self.install(self.alpha_spec.get_filename())

        invalid_os_gppkg = GppkgSpec("alpha", "1.1", GPDB_VERSION, OS, arch)
        gppkg_file = self.build(invalid_os_gppkg, self.A_spec)

        with self.assertRaisesRegex(ExecutionError, "%s Arch required. %s Arch found" % (arch, ARCH)):
            self.update(gppkg_file) 

if __name__ == "__main__":
    unittest.main()
