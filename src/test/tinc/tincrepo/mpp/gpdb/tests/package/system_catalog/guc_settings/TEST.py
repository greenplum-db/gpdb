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

Test GPDB gucs
"""

############################################################################
# Set up some globals, and import gptest 
#    [YOU DO NOT NEED TO CHANGE THESE]
#
import os
import sys

MYD = os.path.abspath(os.path.dirname(__file__))
mkpath = lambda *x: os.path.join(MYD, *x)
UPD = os.path.abspath(mkpath('../..'))
if UPD not in sys.path:
    sys.path.append(UPD)

import gptest
from lib.Shell import shell
from lib.PSQL import psql

import gpConfig

USER = os.environ.get('LOGNAME')
HOST = "localhost"

###########################################################################
#  A Test class must inherit from gptest.GPTestCase
#    [CREATE A CLASS FOR YOUR TESTS]
#
class guc_settings(gptest.GPTestCase):

    gpdbConfig = gpConfig.GpConfig(HOST, USER)

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_01_guc_minimumvalue(self):
        """GUCS: MPP-8307: minimum max_connections is 10"""
        
        self.gpdbConfig.setParameterMasterOnly("max_connections","10")
        shell.run("gpstop -ar") # Restarts, TODO: need a utilty to restart GPDB
        max_connections = self.gpdbConfig.getParameterMasterOnly("max_connections")
        self.failUnless(max_connections=="10")
       
    def test_02_guc_minimumvalue(self):
        """GUCS: MPP-8307: invalid max_connections if set less than 10, default to 200"""

        # cur_maxconnections = self.gpdbConfig.getParameterMasterOnly("max_connections")
        self.gpdbConfig.setParameterMasterOnly("max_connections","4")
        shell.run("gpstop -ar") # Restarts, TODO: need a utilty to restart GPDB
        max_connections = self.gpdbConfig.getParameterMasterOnly("max_connections")
        self.failUnless(max_connections=="200")
        

###########################################################################
#  Try to run if user launched this script directly
#     [YOU SHOULD NOT CHANGE THIS]
if __name__ == '__main__':
    gptest.main()
