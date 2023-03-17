#!/usr/bin/env python3

import os
import tarfile

from contextlib import closing
from gppylib.commands.base import ExecutionError
from gppylib.commands.unix import Rsync
from gppylib.operations.package import GpRsync
from gppylib.operations.test.regress.test_package import GppkgTestCase, unittest, skipIfNoStandby, get_host_list, ARCHIVE_PATH, run_command
from gppylib.operations.unix import RemoveRemoteFile

@skipIfNoStandby()
class MuckWithInternalsOnStandby(GppkgTestCase):            
    @unittest.expectedFailure
    def test00_delete_package_from_archive_on_standby_and_install(self):
        gppkg_file = self.build(self.alpha_spec, self.A_spec)
        archive_file = os.path.join(ARCHIVE_PATH, gppkg_file)
        self.install(gppkg_file)

        #Remove package from standby
        standby = get_host_list()[0] 
        RemoveRemoteFile(os.path.join(ARCHIVE_PATH, gppkg_file), standby).run()

        try:
            self.install(gppkg_file)
        except ExecutionError as e:
            Rsync(name = "copy gppkg to standby",
                srcFile = gppkg_file,
                dstFile = archive_file,
                srcHost = None,
                dstHost = standby).run(validateAfter = True)
            self.fail("ExecutionError %s" % e)

    @unittest.expectedFailure
    def test01_delete_package_from_archive_on_standby_and_uninstall(self):
        """
            Known issue: MPP-15737
        """
        gppkg_file = self.alpha_spec.get_filename()
        archive_file = os.path.join(ARCHIVE_PATH, gppkg_file)
        self.install(gppkg_file)

        #Remove package from standby
        standby = get_host_list()[0] 
        RemoveRemoteFile(os.path.join(ARCHIVE_PATH, gppkg_file), standby).run()

        try:
            self.remove(gppkg_file)
        except ExecutionError as e:
            GpRsync(source_path = gppkg_file,
                  target_path = archive_file,
                  host_list = get_host_list()[1]).run()
            Rsync(name = "copy gppkg to standby",
                srcFile = gppkg_file,
                dstFile = archive_file, 
                srcHost = None, 
                dstHost = standby).run(validateAfter = True)
            self.fail("ExecutionError %s" % e)

    @unittest.expectedFailure
    def test02_uninstall_rpm_on_standby_and_install(self):
        #Use gppkg from previous test
        gppkg_file = self.alpha_spec.get_filename()
        self.install(gppkg_file)
   
        standby = get_host_list()[0]
 
        self.uninstall_rpm_remotely(self.A_spec.get_filename(), standby)
        try:
            self.install(gppkg_file) 
        except ExecutionError as e:
            #Install the rpm 
            with closing(tarfile.open(self.alpha_spec.get_filename())) as tf:
                tf.extract(self.A_spec.get_filename())
            Rsync(name = "copy rpm to standby",
                srcFile = self.A_spec.get_filename(), 
                dstFile = self.A_spec.get_filename(), 
                srcHost = None,
                dstHost = standby).run(validateAfter = True)
            self.install_rpm_remotely(self.A_spec.get_filename(), standby)
            os.remove(self.A_spec.get_filename())
            RemoveRemoteFile(self.A_spec.get_filename(), standby).run()
            self.fail("ExecutionError %s" % e)

    def test03_uninstall_rpm_on_standby_and_uninstall(self):
        #Use gppkg from previous test
        gppkg_file = self.alpha_spec.get_filename()
        self.install(gppkg_file)
   
        standby = get_host_list()[0]
 
        self.uninstall_rpm_remotely(self.A_spec.get_filename(), standby)
        self.remove(gppkg_file) 

    def test04_install_rpm_on_standby_and_install(self):
        gppkg_file = self.alpha_spec.get_filename()
 
        standby = get_host_list()[0]
 
        #Install the rpm 
        with closing(tarfile.open(self.alpha_spec.get_filename())) as tf:
            tf.extract(self.A_spec.get_filename())
        Rsync(name = "copy the rpm to standby",
            srcFile = self.A_spec.get_filename(),
            dstFile = self.A_spec.get_filename(), 
            srcHost = None, 
            dstHost = standby).run(validateAfter = True)

        self.install_rpm_remotely(self.A_spec.get_filename(), standby)
        self.install(gppkg_file) 

    @unittest.expectedFailure
    def test05_install_rpm_on_standby_and_uninstall(self):
        gppkg_file = self.alpha_spec.get_filename()
        
        standby = get_host_list()[0]

        #Install the rpm
        with closing(tarfile.open(self.alpha_spec.get_filename())) as tf:
            tf.extract(self.A_spec.get_filename())
        Rsync(name = "copy rpm to standby",
            srcFile = self.A_spec.get_filename(),
            dstFile = self.A_spec.get_filename(), 
            srcHost = None,
            dstHost = standby).run(validateAfter = True)

        self.install_rpm_remotely(self.A_spec.get_filename(), standby)

        try:
            self.remove(gppkg_file)
        except ExecutionError as e:
            self.uninstall_rpm_remotely(self.A_spec.get_filename(), standby)
            os.remove(self.A_spec.get_filename())
            RemoveRemoteFile(self.A_spec.get_filename(), standby).run()
            self.fail("ExecutionError %s" % e)

if __name__ == "__main__":
    unittest.main()
