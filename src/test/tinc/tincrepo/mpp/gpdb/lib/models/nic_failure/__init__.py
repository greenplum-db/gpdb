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
"""

import unittest2 as unittest

import tinctest
from gppylib.commands.base import Command, REMOTE, WorkerPool
from gppylib.commands.unix import Ping

from mpp.models import MPPTestCase

class NICFailureTestCase(MPPTestCase):
    """ 
        Base class for an NIC failure test case
    """ 

    def __init__(self, methodName):
        self.TESTDB = 'testdb'
        self.nic_to_address_map = dict() #(interface, hostname) -> ip address map
        super(NICFailureTestCase, self).__init__(methodName)

    def bring_down_nic(self, nics, hostname):
        """
            Bring down nics based on the input nic names
        """ 
        if nics is None:
            return False

        pool = WorkerPool()

        try:    
            #get the ip address of the interface
            for nic in nics:
                cmd = Command(name='get the ip of the interface', cmdStr="/sbin/ifconfig %s | grep \'inet addr:\' | cut -d: -f2 | awk \'{ print $1}\'" % nic, ctxt=REMOTE, remoteHost=hostname)
                cmd.run(validateAfter=True)
                results = cmd.get_results()
                if results.rc != 0:
                    raise Exception('Unable to map interface to ipaddress') 

                self.nic_to_address_map[(nic, hostname)] = results.stdout.split()[0].strip()

            for nic in nics:
                tinctest.logger.info("Bringing down %s:%s ..." % (hostname, nic))   
                cmd = Command(name='bring NIC down', cmdStr='sudo /sbin/ifdown %s' % nic, ctxt=REMOTE, remoteHost=hostname)
                pool.addCommand(cmd)

            pool.join()
            for cmd in pool.getCompletedItems():
                results = cmd.get_results()
                if results.rc != 0:
                    return False
        finally:
            pool.haltWork()
            pool.joinWorkers()
            pool.join()

        return True


    def validate_nic_down(self):
        """     
            Ping validation on the nics.
        """     

        pool = WorkerPool()

        try:    
            for nic, hostname in self.nic_to_address_map:
                address = self.nic_to_address_map[(nic, hostname)]
                cmd = Ping('ping validation', address, ctxt=REMOTE, remoteHost='localhost')
                pool.addCommand(cmd)
            pool.join()

            for cmd in pool.getCompletedItems():
                results = cmd.get_results()
                if results.rc == 0:
                    return False
        finally:
            pool.haltWork()
            pool.joinWorkers()
            pool.join()

        tinctest.logger.info("Successfully brought down nics ...")   
        return True

