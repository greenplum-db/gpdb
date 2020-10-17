#!/usr/bin/env python3

import os

from gppylib.operations.test.regress.test_package import GppkgTestCase, unittest, skipIfNoStandby, get_host_list, ARCHIVE_PATH, run_command, skipIfSingleNode
from gppylib.operations.unix import RemoveRemoteFile, CheckRemoteFile

class CleanGppkgTestCase(GppkgTestCase):
    def setUp(self):
        super(CleanGppkgTestCase, self).setUp()
        self.clean_command = "gppkg --clean"

        standby, segment_host_list = get_host_list()
        cluster_host_list = [standby] + [host for host in segment_host_list]        

    def test00_build(self):
        self.build(self.alpha_spec, self.A_spec)

    def test01_install(self):
        self.install(self.alpha_spec.get_filename())

    def test02_no_packages_on_cluster(self):
        """
        This test covers the case when we run clean 
        with no packages installed on the cluster
        """
        run_command(self.clean_command) 
        standby, segment_host_list = get_host_list()
        cluster_host_list = [standby] + [host for host in segment_host_list]        

        for host in cluster_host_list:
            if host is not None:
                self.check_remote_rpm_uninstall(self.A_spec.get_package_name(), host)
                self.assertFalse(CheckRemoteFile(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()), host).run())

    def test03_no_package_on_master(self):
        """
        This test covers the case when there is no
        package installed on the master, but some
        package is installed on the rest of the cluster
        """
        self.install(self.alpha_spec.get_filename())
        
        os.remove(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()))
     
        self.uninstall_rpm(self.A_spec.get_filename())

        run_command(self.clean_command) 
        standby, segment_host_list = get_host_list()
        cluster_host_list = [standby] + [host for host in segment_host_list]
       
        for host in cluster_host_list:
            if host is not None:
                self.check_remote_rpm_uninstall(self.A_spec.get_package_name(), host)
                self.assertFalse(CheckRemoteFile(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()), host).run())

    @skipIfSingleNode()
    def test04_no_package_on_segment(self):
        """
        This test covers the case when there is no
        package installed on one of the segment, but
        it is installed on the master and everywhere else 
        """
        self.install(self.alpha_spec.get_filename())
       
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0)
        host = segment_host_list[0]       
 
        RemoveRemoteFile(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()), host).run()

        self.uninstall_rpm_remotely(self.A_spec.get_filename(), host)

        run_command(self.clean_command)

        self.check_remote_rpm_install(self.A_spec.get_package_name(), host)
        self.assertTrue(CheckRemoteFile(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()), host).run())

    @unittest.expectedFailure
    def test05_no_rpm_on_master(self):
        """
        This test covers the case when the rpm has been 
        uninstalled on the master and the gppkg is in the archive, 
        and the rpms/gppkgs are intstalled on the segments.   
        JIRA - MPP-15968
        """
        self.install(self.alpha_spec.get_filename())
        
        self.uninstall_rpm(self.A_spec.get_filename())
        run_command(self.clean_command)

        self.check_rpm_install(self.A_spec.get_package_name())
        self.assertTrue(os.path.exists(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename())))

    @skipIfSingleNode()
    @unittest.expectedFailure
    def test06_no_rpm_on_segment(self):
        """
        This test covers the case when the gppkg has been installed
        on the cluster and the rpm has not been installed properly on
        one of the segments. The gppkg however exists in the archive on
        the segment. 
        """
        self.install(self.alpha_spec.get_filename())
        
        segment_host_list = get_host_list()[1]
        self.assertTrue(len(segment_host_list) > 0)
        host = segment_host_list[0]

        self.uninstall_rpm_remotely(self.A_spec.get_filename(), host)
        run_command(self.clean_command)

        self.check_remote_rpm_install(self.A_spec.get_package_name(), host)
        self.assertTrue(CheckRemoteFile(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()), host).run())

    @skipIfNoStandby()
    def test07_no_package_on_standby(self):
        """
        This test covers the case when there is no package
        installed on the standby, but the package is installed
        across the other hosts in the cluster.
        JIRA - MPP-15969
        """ 
        self.install(self.alpha_spec.get_filename())

        standby = get_host_list()[0]
        
        RemoveRemoteFile(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()), standby).run()
        self.uninstall_rpm_remotely(self.A_spec.get_filename(), standby)
        run_command(self.clean_command)
    
        self.check_remote_rpm_install(self.A_spec.get_package_name(), standby)
        self.assertTrue(CheckRemoteFile(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()), standby).run())
        
    @skipIfNoStandby()
    @unittest.expectedFailure
    def test08_no_rpm_on_standby(self):
        """
        Covers the case when there is no rpm installed on the standby
        but the gppkg is present in the archive and the package has 
        been installed across other hosts in the cluster.
        JIRA - MPP-15968, MPP-15969
        """ 
        self.install(self.alpha_spec.get_filename())
        
        standby = get_host_list()[0]
    
        self.uninstall_rpm_remotely(self.A_spec.get_filename(), standby)
        run_command(self.clean_command)

        self.check_remote_rpm_install(self.A_spec.get_package_name(), standby)
        self.assertTrue(CheckRemoteFile(os.path.join(ARCHIVE_PATH, self.alpha_spec.get_filename()), standby).run())

if __name__ == "__main__":
    unittest.main()
