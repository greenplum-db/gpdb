#!/usr/bin/env python
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

Network Utility Class, IPV4 and IPV6
For multi-nic environment, you will need to pass in that hostname
Normally, in GPDB setup, the hostname should have only IPV4 or IPV6
"""

import socket
import types

class network:
    """
    Network Class
    """

    def __init__(self, hostname=None):
        """
        Initialize Network Object
        @param hostname: hostname, optional, use the default running machine
        """
        if hostname is None:
            self.hostname = socket.gethostname()
        else:
            self.hostname = hostname
        # compatible for IPV4 and IPV6
        hostinfo = socket.getaddrinfo(self.hostname, None)
        self.ipv4 = ""
        self.ipv6 = ""
        self._set_ipaddress(hostinfo)

    def _set_ipaddress(self, hostinfo):
        """
        Set the IPV4 or IPV6 for the hostname
        @param hostinfo: host information
        """
        ipaddrlist = list(set([(ai[4][0]) for ai in hostinfo]))
        for myip in ipaddrlist:
            if myip.find(":")>0:
                self.ipv6 = myip
            elif myip.find(".")>0:
                self.ipv4 = myip

    def get_hostname(self):
        """
        Get hostname
        @return hostname
        """
        return self.hostname

    def get_ipv4(self):
        """
        Get IPV4 for the hostname
        @return ipv4
        """
        return self.ipv4

    def get_ipv6(self):
        """
        Get IPV6 for the hostname
        @return ipv4
        """
        return self.ipv6

    def get_ip(self):
        """
        Get the available IP address, returns IPV6, else IPV4
        For GPDB setup, it's either IPV4 or IPV6 only
        @return ip address
        """
        if self.ipv6 is "":
            return self.ipv4
        return self.ipv6
    
    def islocal(self, hostname):
        """
        Check whether hostname is localhost
        @param hostname: String or List of hostnames
        network.islocal("localhost")
        network.islocal(["sdw1", "sdw2", "sdw3"])
        @return True or False
        """
        if type(hostname) == types.ListType:
            hostname = ", ".join(hostname)

        if hostname.find("localhost")!=-1 or hostname.find("127.0.0.1")!=-1 or hostname.find("::1")!=-1:
            return True
        return False

myhost = network()

if __name__ == '__main__':
    MYHOST = network()
    print MYHOST.get_hostname()
    print "IP: %s " % MYHOST.get_ip()
    print "IPV4: %s " % MYHOST.get_ipv4()
    print "IPV6: %s " % MYHOST.get_ipv6()

