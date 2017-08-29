"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import tinctest
from tinctest.lib import local_path, run_shell_command

class RPMUtil(object):
    """Utility module for dealing with RPM packages"""
    
    def __init__(self):
        self.rpm_cmd = "sudo rpm"

    def query_package(self, pkg_name):
        """
        Queries for rpm package.
        
        @param pkg_name: RPM package name
        @return: tuple containing status of installed package and list of 
                 matching packages
        """
        cmd_str = "%s -qa | egrep \"%s\"" %(self.rpm_cmd, pkg_name)
        res = {}
        result = run_shell_command(cmd_str,"Query packages",res)
        list_pkgs = res['stdout'].replace('\n',',').split(',')[:-1]
        return (result, list_pkgs)
            
    def is_pkg_installed(self, pkg_name):
        """
        Checks if the package is present or not.
        
        @return: True or False based on the status
        """
        return self.query_package(pkg_name)[0]
        
    def install_package_using_yum(self, pkg_name, is_regex_pkg_name = False):
        """
        Installs a given package using yum installer.
        If complete package name not known, can give regex pattern of package name
        and pass is_regex_pkg_name as True
        
        @param pkg_name: name of the package to be installed.
        @param is_regex_pkg_name: True if passing regex pattern for package name else False
        @return: Boolean value based on installation status
        """
        if is_regex_pkg_name:
            pkg_name = pkg_name + "*"
        cmd_str = "sudo yum -y install %s"%pkg_name
        res = {}
        result = run_shell_command(cmd_str, "Install package using yum", res)
        return result
        
    def install_rpms_from(self, rpm_pkgs_loc):
        """
        Installs all the rpms from a give location
        
        @param rpm_pkgs_loc: location where all packages reside
        """
        cmd_str = "%s -ivh %s/*.rpm" %(self.rpm_cmd, rpm_pkgs_loc)
        res = {}
        packages_installed = run_shell_command(cmd_str, "Install RPMs from loc - %s" %rpm_pkgs_loc, res)
        if not packages_installed:
            tinctest.logger.error("Failed to install rpms from %s - Error: %s" %(rpm_pkgs_loc, res['stderr']))
            raise Exception("Failed to install rpms from %s - Error: %s" %(rpm_pkgs_loc, res['stderr']))
        
    def erase_package(self, pkg_name):
        """
        Erases a given rpm package
        
        @param pkg_name: name of the package to be deleted
        @return: deletion status
        """
        cmd_str = "%s -e %s" %(self.rpm_cmd, pkg_name)
        result = run_shell_command(cmd_str,"Erase packages")
        return result
        
    def erase_all_packages(self, pkg_name_regex):
        """
        Erases more than 1 package based on the package name regex
        
        @param pkg_name_regex: regex pattern of the packages to be deleted
        @return: deletion status
        """
        cmd_str = "%s -qa | egrep \"%s\" | xargs %s -e" %(self.rpm_cmd, pkg_name_regex, self.rpm_cmd)
        result = run_shell_command(cmd_str,"Erase All packages matching regex pattern")
        return result
        
