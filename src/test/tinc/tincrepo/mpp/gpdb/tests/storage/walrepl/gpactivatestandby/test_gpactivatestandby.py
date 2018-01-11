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

import os
import socket
from time import sleep
import unittest2 as unittest

from gppylib.gparray import GpArray
from gppylib.db import dbconn

from tinctest.lib import local_path
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl import lib as walrepl

gputil = GpUtility()
gputil.check_and_start_gpdb()
config = GPDBConfig()

class GpactivateStandbyTestCase(MPPTestCase):
    ''' Testcases for gpinitstandby
        Due to STO-355 currently master is not  stopped to activate standby
        This need to be changed later to always stop master before activate standb'''

    db_name = 'walrepl'
    standby_port = '5656'
    origin_mdd = os.environ.get('MASTER_DATA_DIRECTORY')   

    def __init__(self, methodName):
        self.gpact = GpactivateStandby()
        self.host = socket.gethostname()
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.pgport = os.environ.get('PGPORT')
        self.port = self.pgport

        dburl = dbconn.DbURL()
        gparray = GpArray.initFromCatalog(dburl, utility=True)
        self.numcontent = gparray.getNumSegmentContents()
        self.orig_master = gparray.master

        self.standby_pid = ''
        super(GpactivateStandbyTestCase,self).__init__(methodName)

    def setUp(self):
        #Remove standby if present
        gputil.check_and_start_gpdb()
        gputil.remove_standby()
       
    def tearDown(self):
        # Cleanup Filespaces
        walrepl.cleanupFilespaces(dbname=self.db_name)
        #self.gpact.failback_to_original_master()

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_gpactivatestandby_on_new_host(self):
        gputil.install_standby()
        initstdby = GpinitStandby()
        gpact_stdby = GpactivateStandby()
        self.mdd = gpact_stdby.get_standby_dd()
        self.host = initstdby.get_standbyhost()
        self.port = gpact_stdby.get_standby_port()
        self.standby_pid = gpact_stdby.get_standby_pid(self.host, self.port, self.mdd)
        PSQL.run_sql_file(local_path('create_tables.sql'), dbname = self.db_name)
        self.assertTrue(gpact_stdby.activate())
        self.assertTrue(gpact_stdby.verify_gpactivatestandby(self.standby_pid, self.host, self.port, self.mdd)) 
        gputil.failback_to_original_master(self.origin_mdd,self.host,self.mdd,self.port) 

    def test_gpactivatestandby_on_same_host(self):
        ''' Doesn't work due to STO-374'''
        gputil.install_standby(new_stdby_host='localhost')
        initstdby = GpinitStandby()
        gpact_stdby = GpactivateStandby()
        self.mdd = gpact_stdby.get_standby_dd()
        self.port = gpact_stdby.get_standby_port()
        self.standby_pid = gpact_stdby.get_standby_pid('localhost', self.port, self.mdd)
        PSQL.run_sql_file(local_path('create_tables.sql'), dbname = self.db_name)
        self.assertTrue(gpact_stdby.activate())
        self.assertTrue(gpact_stdby.verify_gpactivatestandby(self.standby_pid, 'localhost', self.port, self.mdd))
        gputil.failback_to_original_master(self.origin_mdd,socket.gethostname(),self.mdd,self.port)

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def Dtest_gpactivatestandby_with_gpinitstandby(self):
        '''Doesn't work due to STO-355 '''
        #(self.standby_pid, self.host, self.mdd)  = self.gpact.create_standby(local='no')
        #self.port = self.pgport
        #PSQL.run_sql_file(local_path('create_tables.sql'), dbname = self.db_name)
        #self.assertTrue(self.gpact.activate(self.host, local='no', init = 'yes'))
        #self.assertTrue(self.gpact.verify_gpactivatestandby(self.standby_pid, self.host, self.port, self.mdd)) 
        pass

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_gpactivatestandby_force_on_new_host(self):
        gputil.install_standby()
        initstdby = GpinitStandby()
        gpact_stdby = GpactivateStandby()
        self.mdd = gpact_stdby.get_standby_dd()
        self.host = initstdby.get_standbyhost()
        self.port = gpact_stdby.get_standby_port() 
        self.standby_pid = gpact_stdby.get_standby_pid(self.host, self.port, self.mdd)
        PSQL.run_sql_file(local_path('create_tables.sql'), dbname = self.db_name)
        self.assertTrue(gpact_stdby.activate(option='-f'))
        self.assertTrue(gpact_stdby.verify_gpactivatestandby(self.standby_pid, self.host, self.port, self.mdd)) 
        gputil.failback_to_original_master(self.origin_mdd,self.host,self.mdd,self.port)
   

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_gpactivatestandby_new_host_with_filespace(self):
        from mpp.lib.gpfilespace import Gpfilespace
        gpfile = Gpfilespace()
        gpfile.create_filespace('fs_walrepl_a')
        PSQL.run_sql_file(local_path('filespace.sql'), dbname= self.db_name)
        gputil.install_standby()
        initstdby = GpinitStandby()
        gpact_stdby = GpactivateStandby()
        self.mdd = gpact_stdby.get_standby_dd()
        self.host = initstdby.get_standbyhost()
        self.port = gpact_stdby.get_standby_port()
        self.standby_pid = gpact_stdby.get_standby_pid(self.host, self.port, self.mdd)
        PSQL.run_sql_file(local_path('create_tables.sql'), dbname = self.db_name)
        self.assertTrue(gpact_stdby.activate())
        self.assertTrue(gpact_stdby.verify_gpactivatestandby(self.standby_pid, self.host, self.port, self.mdd)) 
        gputil.failback_to_original_master(self.origin_mdd,self.host,self.mdd,self.port)
