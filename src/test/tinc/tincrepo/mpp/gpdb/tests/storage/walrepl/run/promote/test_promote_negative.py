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

import os
import unittest2 as unittest

import tinctest
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

class PromoteTestCase(MPPTestCase):
   '''testcase for gpstart''' 
 
   def __init__(self,methodName):
       self.pgutil = GpUtility()
       super(PromoteTestCase,self).__init__(methodName)
  
   def setUp(self):
       self.pgutil.check_and_start_gpdb()
       # We should forcibly recreate standby, as it might has been promoted.
       self.pgutil.remove_standby()
       self.pgutil.install_standby()


   def tearDown(self):
       self.pgutil.remove_standby()

   def test_promote_incomplete_stdby(self):
       ''' 
       remove the standby base dir, try promote and check if fail       
       '''
       gpactivate_stdby = GpactivateStandby()
       gpinit_stdby = GpinitStandby()
       stdby_mdd = gpactivate_stdby.get_standby_dd()
       stdby_host = gpinit_stdby.get_standbyhost()
       stdby_port = gpactivate_stdby.get_standby_port()
       destDir = os.path.join(stdby_mdd, 'base')
       self.pgutil.clean_dir(stdby_host,destDir)
       promote_cmd = "pg_ctl promote -D %s"%stdby_mdd       
       (rc, output) = gpactivate_stdby.run_remote(stdby_host,promote_cmd ,stdby_port,stdby_mdd)
       self.assertEqual(rc, 0)
       pid = self.pgutil.get_pid_by_keyword(host=stdby_host, pgport=stdby_port, keyword='master', option='bin')
       self.assertTrue(int(pid) == -1, 'incomplete standby data directory promote succeeds.')
