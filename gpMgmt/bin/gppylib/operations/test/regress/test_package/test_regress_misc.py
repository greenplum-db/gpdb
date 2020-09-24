#!/usr/bin/env python3

import os
import shutil

from gppylib.operations.test.regress.test_package import GppkgTestCase, unittest, run_command, GPHOME

class MiscTestCase(GppkgTestCase):
    """Covers all test cases which do not fall under any specific category"""
    @unittest.expectedFailure
    def test00_MPP_14671(self):
        """
        This test ensures that gppkg does not use the package name
        to discern information about the package. 
        """
        gppkg_file = self.build(self.alpha_spec, self.A_spec)

        dummy_file = "dummy-x.y.z-abcd-efgh.gppkg"
        shutil.copy(gppkg_file, dummy_file)
        self.extra_clean.add(dummy_file)

        self.install(dummy_file)
        self.assertTrue(os.path.exists(os.path.join(GPHOME, 'share', 'packages', 'archive', dummy_file)))

        self.check_rpm_install(self.A_spec.get_package_name())

        self.assertTrue(self.check_install(gppkg_file))

        results = run_command("gppkg -q --all")
        self.assertTrue(''.join(gppkg_file.split('-')[:1]) in results)

if __name__ == "__main__":
    unittest.main()
