#!/usr/bin/env python
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

"""
Copyright (c) 2004-Present Pivotal Software, Inc.

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

Capture information regarding the machine we run test on
"""

import os
import platform
import socket
import re
    
#####
class GpSystem:
    """
    Capture information regarding the machine we run test on
    @class GpSystem
    
    @organization: DCD Partner Engineering
    @contact: Kenneth Wong

    @modified: Johnny Soedomo
    @note: platform.system returns Linux for both Redhat and SuSE, check the release file
    @change: Jacqui Taing - added attributes: os_version, os_major_version to provide OS release version
    """

    ###
    def __init__(self):
        """
        Constructor for GpSystem regards information we care about on the test system
        """
        self.architecture = platform.machine()
        myos = platform.system()
        if myos == "Darwin":
            self.os = 'OSX'
            (version, major_version) = self.__getOsVersion()
            self.os_version = version
            self.os_major_version = major_version
        elif myos == "Linux":
            if os.path.exists("/etc/SuSE-release"):
                self.os = 'SUSE'
                (version, major_version) = self.__getOsVersion('/etc/SuSE-release')
                self.os_version = version
                self.os_major_version = major_version
            elif os.path.exists("/etc/redhat-release"):
                self.os = 'RHEL'
                (version, major_version) = self.__getOsVersion('/etc/redhat-release')
                self.os_version = version
                self.os_major_version = major_version
        elif myos == "SunOS":
            self.os = 'SOL'
            (version, major_version) = self.__getOsVersion('/etc/release')
            self.os_version = version
            self.os_major_version = major_version
        self.host = socket.gethostname()

    ###
    def __str__(self):
        """
        @return: a string consists of host, architecture, os, os version, os major version
        """
        return '\nGpSystem:\n host: %s\n architecture: %s\n os: %s\n os version: %s\n os major version: %s' % (self.host, self.architecture, self.os, self.os_version, self.os_major_version)
    
    def __getOsVersion(self, releasefile=None):
        """
        @summary: Internal function to get the OS full release version (e.g. 5.5.) and major release version (e.g. 5)
        
        @return: list (full_version, major_version), e.g. (5.5, 5)
        """
        if self.os == 'OSX':
            full_version = platform.mac_ver()[0]
            major_version = full_version.split('.')[0]
            return (full_version, major_version) 
        else:
            if os.path.exists(releasefile):
                f = open(releasefile)
                releasetext = f.read()
                f.close()
                full_version = re.search('[\d\.*\d]+', releasetext).group(0)
                major_version = full_version.split('.')[0]
                return (full_version, major_version)
            else:
                return None

    ###
    def GetArchitecture(self):
        """
        @return: architecture
        """
        return self.architecture

    ###
    def GetOS(self):
        """
        @return: os
        """
        return self.os

    def GetOSMajorVersion(self):
        """
        @return: major release version of OS, e.g. 5 for RHEL 5.5
        
        """
        return self.os_major_version
    
    def GetOSVersion(self):
        """
        @return: full release version of OS, e.g. 5.5 for RHEL 5.5.
        
        """
        return self.os_version

    ###
    def GetHost(self):
        """
        @return: host
        """
        return self.host

##########################
if __name__ == '__main__':
    gpSystem = GpSystem()
    print(gpSystem)


