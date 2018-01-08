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
import shutil
from time import sleep
import unittest2 as unittest

from tinctest.lib import local_path
from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.lib import Standby
from mpp.gpdb.tests.storage.walrepl import lib as walrepl

pgutil = GpUtility()
pgutil.check_and_start_gpdb()
config = GPDBConfig()

class GpinitStandsbyTestCase(MPPTestCase):
    ''' Testcases for gpinitstandby'''

    db_name = 'walrepl'
    standby_port = '5433'
    standby_dirname = 'newstandby'

    def __init__(self, methodName):
        self.gp = GpinitStandby()
        self.host = socket.gethostname()
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.standby = self.gp.get_standbyhostnode()
        self.standby_loc = os.path.join(os.path.split(self.mdd)[0],
                                        self.standby_dirname)
        self.primary_pid = 0 
        super(GpinitStandsbyTestCase,self).__init__(methodName)

    def setUp(self):
        #Remove standby if present
        self.primary_pid = self.gp.get_primary_pid()
        self.gp.run(option='-r')
       
    def tearDown(self):
        # Cleanup Filespaces
        walrepl.cleanupFilespaces(dbname=self.db_name)
        
      
    def create_directory(self,location):
        pgutil.clean_dir(self.standby,os.path.split(location)[0])
        pgutil.create_dir(self.standby,os.path.split(location)[0])

    def create_unmodifiable_file(self, filename):
        filepath = os.path.join(self.mdd, filename)
        with open(filepath, 'w') as fp:
            pass
        os.chmod(filepath, 000)
   
    def remove_unmodifiable_file(self, filename):
        filepath = os.path.join(self.mdd, filename)
        os.chmod(filepath, 777)
        os.remove(filepath)

    def touch_file(self, filename):
        file_dir = os.path.split(filename)[0]
        if not os.path.exists(file_dir):
            os.makedirs(file_dir)

        with open(filename, 'w') as fp:
            pass

    def test_gpinitstandby_exclude_dirs(self):
        """
        Test pg_basebackup exclusions when copying filespaces from
        the master to the standby during gpinitstandby
        """
        os.makedirs(self.mdd + '/db_dumps')
        self.touch_file(self.mdd + '/db_dumps/testfile')
        self.touch_file(self.mdd + '/gpperfmon/logs/test.log')
        self.touch_file(self.mdd + '/gpperfmon/data/testfile')
        self.touch_file(self.mdd + '/pg_log/testfile')

        self.gp.run(option = '-P %s -s %s -F %s' % (self.standby_port, self.host, self.standby_loc))

        shutil.rmtree(self.mdd + '/db_dumps')
        os.remove(self.mdd + '/gpperfmon/logs/test.log')
        os.remove(self.mdd + '/gpperfmon/data/testfile')
        os.remove(self.mdd + '/pg_log/testfile')

        self.assertFalse(os.path.exists(self.standby_loc + '/db_dumps/testfile'))
        self.assertFalse(os.path.exists(self.standby_loc + '/gpperfmon/logs/test.log'))
        self.assertFalse(os.path.exists(self.standby_loc + '/gpperfmon/data/testfile'))
        self.assertFalse(os.path.exists(self.standby_loc + '/pg_log/testfile'))
        self.assertTrue(self.gp.run(option = '-r'))

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_gpinitstanby_to_new_host(self):
        self.create_directory(self.mdd)
        self.assertTrue(self.gp.run(option = '-s %s' % self.standby))
        self.assertTrue(self.gp.verify_gpinitstandby(self.primary_pid))


    def test_gpinitstandby_to_same_host_new_port_and_new_mdd(self):
        pgutil.clean_dir(self.host,self.standby_loc)
        self.assertTrue(self.gp.run(option = '-P %s -s %s -F %s' % (self.standby_port, self.host, self.standby_loc)))
        self.assertTrue(self.gp.verify_gpinitstandby(self.primary_pid))
        

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_gpinitstandby_new_host_new_port(self):
        self.create_directory(self.mdd)
        self.assertTrue(self.gp.run(option = '-P %s -s %s' % (self.standby_port, self.standby)))
        self.assertTrue(self.gp.verify_gpinitstandby(self.primary_pid))

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_gpinitstandby_new_host_new_mdd(self):
        self.create_directory(self.standby_loc)
        self.assertTrue(self.gp.run(option = '-F %s -s %s' % (self.standby_loc, self.standby)))
        self.assertTrue(self.gp.verify_gpinitstandby(self.primary_pid))
    
	# WALREP_FIXME: Enable this after tablespace with walrep works
    #def test_gpinitstandby_to_same_with_filespaces(self):
    #    from mpp.lib.gpfilespace import Gpfilespace
    #    gpfile = Gpfilespace()
    #    gpfile.create_filespace('fs_walrepl_a')
    #    PSQL.run_sql_file(local_path('filespace.sql'), dbname = self.db_name)
    #    filespace_loc = self.gp.get_filespace_location() 
    #    filespace_loc = os.path.join(os.path.split(filespace_loc)[0], 'newstandby')
    #    filespaces = "pg_system:%s,fs_walrepl_a:%s" %(self.standby_loc , filespace_loc)
    #    self.assertTrue(self.gp.run(option = '-F %s -s %s -P %s' % (filespaces, self.host, self.standby_port)))
    #    self.assertTrue(self.gp.verify_gpinitstandby(self.primary_pid))
    #
    #@unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    #def test_gpinitstandby_new_host_with_filespace(self):
    #    from mpp.lib.gpfilespace import Gpfilespace
    #    gpfile = Gpfilespace()
    #    gpfile.create_filespace('fs_walrepl_a')
    #    PSQL.run_sql_file(local_path('filespace.sql'), dbname = self.db_name)
    #    filespace_loc = self.gp.get_filespace_location()
    #    self.create_directory(filespace_loc)
    #    filespaces = "pg_system:%s,fs_walrepl_a:%s" % (self.mdd, filespace_loc)
    #    self.assertTrue(self.gp.run(option = '-F %s -s %s -P %s' % (filespaces, self.standby, self.standby_port)))
    #    self.assertTrue(self.gp.verify_gpinitstandby(self.primary_pid))
    
    def test_gpinitstandby_remove_from_same_host(self):
        #self.gp.create_dir_on_standby(self.host, self.standby_loc)
        pgutil.clean_dir(self.host,self.standby_loc)
        self.gp.run(option = '-P %s -s %s -F %s' % (self.standby_port, self.host, self.standby_loc))
        self.assertTrue(self.gp.run(option = '-r'))

    @unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    def test_gpinitstandby_remove_from_new_host(self):
        self.create_directory(self.mdd)
        self.gp.run(option = '-s %s' % self.standby)
        self.assertTrue(self.gp.run(option = '-r'))
   
    def test_gpinitstandby_n_with_no_standby_stop(self):
        pgutil.clean_dir(self.host,self.standby_loc)
        self.gp.run(option = '-P %s -s %s -F %s' % (self.standby_port, self.host, self.standby_loc))
        (rc,out) = pgutil.run(command='gpinitstandby -n')
        self.assertTrue('Standy master is already up and running' in out)

    def test_gpinitstandby_n_with_standby_stop(self):
        pgutil.clean_dir(self.host,self.standby_loc)
        self.gp.run(option = '-P %s -s %s -F %s' % (self.standby_port, self.host, self.standby_loc))
        self.standby = Standby(self.standby_loc,
                                        self.standby_port)

        self.standby.stop()
        (rc,out) = pgutil.run(command='gpinitstandby -n')
        self.assertTrue('Successfully started standby master' in out)

    def test_gpinitstandby_with_no_default_path(self):
        self.assertTrue(self.gp.initstand_by_with_default())
    
	# WALREP_FIXME: Enable this after tablespace with walrep works
    #@unittest.skipIf(not config.is_multinode(), "Test applies only to a multinode cluster")
    #def test_gpinitstandby_prompt_for_filespace(self):
    #    from mpp.lib.gpfilespace import Gpfilespace
    #    gpfile = Gpfilespace()
    #    gpfile.create_filespace('fs_walrepl_a')
    #    PSQL.run_sql_file(local_path('filespace.sql'), dbname = self.db_name)
    #    filespace_loc = self.gp.get_filespace_location()
    #    self.create_directory(filespace_loc)
    #    self.assertTrue(self.gp.init_with_prompt(filespace_loc))

    def test_gpinistandby_with_M(self):
        '''
        @product_version gpdb: [4.3.1.0-MAIN]
        '''
        pgutil.clean_dir(self.host,self.standby_loc)
        self.assertTrue(self.gp.run(option = '-P %s -s %s -F %s -M smart' % (self.standby_port, self.host, self.standby_loc)))
        self.assertTrue(self.gp.run(option = '-r'))

    def test_gpinitstandby_to_same_host_new_port_and_new_mdd_with_error(self):
        '''
        @product_version gpdb: [4.3.6.2 - 4.3.99.0)
        '''
        pgutil.clean_dir(self.host,self.standby_loc)
        try:
            self.create_unmodifiable_file('foo')
            (rc, out) = pgutil.run(command='gpinitstandby -a -P %s -s %s -F %s' % (self.standby_port, self.host, self.standby_loc))
            self.assertTrue('ERROR:  could not open file "./foo": Permission denied' in out)
            self.assertFalse('server closed the connection unexpectedly' in out)
        finally:
            self.remove_unmodifiable_file('foo')
            pgutil.clean_dir(self.host,self.standby_loc)
