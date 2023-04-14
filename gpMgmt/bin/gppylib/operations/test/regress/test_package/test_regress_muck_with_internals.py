#!/usr/bin/env python3

import os
import tarfile
import shutil

from contextlib import closing
from gppylib.commands.unix import Rsync
from gppylib.commands.base import ExecutionError
from gppylib.operations.package import GpRsync
from gppylib.operations.unix import RemoveRemoteFile
from gppylib.operations.test.regress.test_package import GppkgTestCase, unittest, get_host_list, ARCHIVE_PATH, RPM_DATABASE, run_command, skipIfSingleNode

class MuckWithInternalsTestCase(GppkgTestCase):
    """
        Each test method in this suite should clean up
        whatever it "mucks". Otherwise it will leave the 
        system in an inconsistent state and the generic
        cleanup method will not be able to cleanup 
        the packages. 
    """
    def test00_delete_package_from_archive_and_install(self):
        """
        Delete a package from the archive on the coordinator
        and try to install the gppkg again.
        """
        gppkg_file = self.build(self.alpha_spec, self.A_spec)
        self.install(gppkg_file) 

        #Remove the package from archive
        os.remove(os.path.join(ARCHIVE_PATH, gppkg_file))
       
        self.install(gppkg_file)

    @unittest.expectedFailure
    def test01_delete_package_from_archive_and_uninstall(self):
        """
        Delete a package from the archive on the coordinator 
        and try to uninstall the gppkg.  
        Known issue: MPP-15737
        """
        #Building off of last test case's state
        #Use gppkg from previous test
        gppkg_file = self.alpha_spec.get_filename()
        self.install(gppkg_file)

        #Remove the package from archive
        os.remove(os.path.join(ARCHIVE_PATH, gppkg_file))

        try:
            self.remove(self.alpha_spec.get_filename())
        except ExecutionError as e:
            shutil.copy(gppkg_file, os.path.join(ARCHIVE_PATH, gppkg_file))
            self.fail("Execution Error %s" % e)

    def test02_delete_rpm_and_install(self):
        """
        Delete the main comprising rpm from the coordinator
        and try to install the gppkg again.
        """
        #Use gppkg from previous test
        gppkg_file = self.alpha_spec.get_filename()
        self.install(gppkg_file)
    
        #Uninstall the RPM
        self.uninstall_rpm(self.A_spec.get_filename()) 
        self.install(gppkg_file) 

    def test03_delete_rpm_and_uninstall(self):
        """
        Delete the main comprising rpm from the coordinator
        and try to install the gppkg again.
        """
        #Use gppkg from previous test
        gppkg_file = self.alpha_spec.get_filename()
        self.install(gppkg_file)
    
        #Uninstall the RPM
        self.uninstall_rpm(self.A_spec.get_filename())
        self.remove(gppkg_file) 

    def test04_install_rpm_and_install(self):
        """
        Install the main comprising rpm on the coordinator
        and try to install a gppkg.
        """
        with closing(tarfile.open(self.alpha_spec.get_filename())) as tf:
            tf.extract(self.A_spec.get_filename())
          
        self.install_rpm(self.A_spec.get_filename()) 
        #Use gppkg from previous test
        gppkg_file = self.alpha_spec.get_filename()
      
        self.install(gppkg_file)

    @unittest.expectedFailure
    def test05_install_rpm_and_uninstall(self):
        """
        Install the main comprising rpm on the coordinator and 
        try to uninstall a gppkg.
        """
        with closing(tarfile.open(self.alpha_spec.get_filename())) as tf:
            tf.extract(self.A_spec.get_filename())
          
        self.install_rpm(self.A_spec.get_filename())
        gppkg_file = self.alpha_spec.get_filename()
      
        try: 
            self.remove(gppkg_file)
        except ExecutionError as e:
            run_command("rpm -e %s --dbpath %s" % (self.A_spec.get_package_name(), RPM_DATABASE))
            os.remove(self.A_spec.get_filename())
            self.fail("ExecutionError %s" % e)

    @skipIfSingleNode()
    @unittest.expectedFailure
    def test06_delete_package_from_archive_on_segment_and_install(self):
        """
        Delete a package from the archive on the segment 
        and try to install the gppkg again.
        """
        gppkg_file = self.alpha_spec.get_filename()
        archive_file = os.path.join(ARCHIVE_PATH, gppkg_file)
        self.install(gppkg_file)

        #Remove package from archive
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0)
        host = segment_host_list[0]
        RemoveRemoteFile(archive_file ,host).run()

        try:
            self.install(gppkg_file)
        except ExecutionError as e:
            Rsync(name = "copy gppkg to segment",
                srcFile = gppkg_file, 
                dstFile = archive_file, 
                srcHost = None,
                dstHost = host).run(validateAfter = True)
            self.fail("ExecutionError %s" % e)
           
    @skipIfSingleNode()
    @unittest.expectedFailure 
    def test07_delete_package_from_archive_on_segment_and_uninstall(self):
        """
        Delete a package from the archive on the segment
        and try to uninstall the gppkg.
        """
        gppkg_file = self.alpha_spec.get_filename()
        archive_file = os.path.join(ARCHIVE_PATH, gppkg_file)
        self.install(gppkg_file)

        #Remove package from archive
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0)
        host = segment_host_list[0]
        RemoveRemoteFile(archive_file ,host).run()

        try:
            self.remove(gppkg_file)
        except ExecutionError as e:
            GpRsync(source_path = gppkg_file,
                  target_path = archive_file,
                  host_list = segment_host_list).run()
            self.fail("ExecutionError %s" % str(e))

    @skipIfSingleNode()
    @unittest.expectedFailure
    def test08_uninstall_rpm_on_segments_and_install(self):
        """
        Try to install a gppkg again, when rpm has not been 
        installed on one of the segments.
        """
        #Use gppkg from previous test
        gppkg_file = self.alpha_spec.get_filename()
        self.install(gppkg_file)
   
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0)
        host = segment_host_list[0]
 
        self.uninstall_rpm_remotely(self.A_spec.get_filename(), host)

        try:
            self.install(gppkg_file) 
        except ExecutionError as e:
            #Install the rpm 
            with closing(tarfile.open(self.alpha_spec.get_filename())) as tf:
                tf.extract(self.A_spec.get_filename())
            Rsync(name = "copy rpm to segment",
                srcFile = self.A_spec.get_filename(), 
                dstFile = self.A_spec.get_filename(), 
                srcHost = None,
                dstHost = host).run(validateAfter = True)
            self.install_rpm_remotely(self.A_spec.get_filename(), host)
            os.remove(self.A_spec.get_filename())
            RemoveRemoteFile(self.A_spec.get_filename(), host).run()
            self.fail("ExecutionError %s" % e)

    @skipIfSingleNode()
    def test09_uninstall_rpm_on_segments_and_uninstall(self):
        """
        Try to uninstall a gppkg when the rpm has already been 
        uninstalled on one of the segments.
        """
        #Use gppkg from previous test
        gppkg_file = self.alpha_spec.get_filename()
        self.install(gppkg_file)
   
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0) 
        host = segment_host_list[0]
 
        self.uninstall_rpm_remotely(self.A_spec.get_filename(), host)
        self.remove(gppkg_file) 

    @skipIfSingleNode()
    def test10_install_rpm_on_segments_and_install(self):
        """
        Try to install a gppkg when an rpm has already been 
        installed on one of the segments.
        """
        gppkg_file = self.alpha_spec.get_filename()
 
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0)
        host = segment_host_list[0]
 
        #Install the rpm 
        with closing(tarfile.open(self.alpha_spec.get_filename())) as tf:
            tf.extract(self.A_spec.get_filename())
        Rsync(name = "copy rpm to segment",
            srcFile = self.A_spec.get_filename(), 
            dstFile = self.A_spec.get_filename(), 
            srcHost = None, 
            dstHost = host).run(validateAfter = True)

        self.install_rpm_remotely(self.A_spec.get_filename(), host)
        self.install(gppkg_file) 

    @skipIfSingleNode()
    @unittest.expectedFailure
    def test11_install_rpm_on_segments_and_uninstall(self):
        """
        Try to uninstall a gppkg, when the gppkg has not been installed,
        but the main comprising rpm has been installed on one of the segments.
        The desired result is that the rpm should be uninstalled.
        """
        gppkg_file = self.alpha_spec.get_filename()
        
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0)
        host = segment_host_list[0]

        #Install the rpm
        with closing(tarfile.open(self.alpha_spec.get_filename())) as tf:
            tf.extract(self.A_spec.get_filename())
        Rsync(name = "copy rpm to segment",
              srcFile = self.A_spec.get_filename(),
              dstFile = self.A_spec.get_filename(), 
              srcHost = None,
              dstHost = host).run(validateAfter = True)

        self.install_rpm_remotely(self.A_spec.get_filename(), host)

        try:
            self.remove(gppkg_file)
        except ExecutionError as e:
            self.uninstall_rpm_remotely(self.A_spec.get_filename(), host)
            os.remove(self.A_spec.get_filename())
            RemoveRemoteFile(self.A_spec.get_filename(), host).run()
            self.fail("ExecutionError %s" % e)

if __name__ == "__main__":
    unittest.main()
