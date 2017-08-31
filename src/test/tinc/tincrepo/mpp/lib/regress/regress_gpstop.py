#!python
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

Unit tests fro gpStop module
"""

import os
import unittest2 as unittest
import tinctest
from gppylib.commands.base import Command
from mpp.lib.gpstart import GpStart, GPstartException
from mpp.lib.gpstop import GpStop, GPstopException

class gpStopTestCase(unittest.TestCase):
	def setUp(self):
		self.basedir = os.path.dirname(__file__)
		self.gphome = os.environ.get('GPHOME')
		self.gp=GpStart()
		self.gps=GpStop()
		self.MAX_TRY=3
                self.TIMEOUT=605
                self.MAXPARALLELSEG=60

    	def test_gpstop_immediate(self):
		tinctest.logger.info("Running test_gpstop_immediate")
		if self.is_gpdb_running():
        	   res=self.gps.run_gpstop_cmd(immediate='y')
		self.assertTrue(res)


    	def test_gpstop_getversion(self):
		tinctest.logger.info("Running test_gpstop_getversion")
        	res=self.gps.run_gpstop_cmd(version ='y')
		self.assertTrue(res)

    	def test_gpstop_quiet(self):
		tinctest.logger.info("Running test_gpstop_quiet")
		if self.is_gpdb_running():
		   res=self.gps.run_gpstop_cmd(quietmode='y')
	     	self.assertTrue(res)

    	def test_gpstop_verbose(self):
		tinctest.logger.info("Running test_gpstop_verbose")
		if self.is_gpdb_running():
		   res=self.gps.run_gpstop_cmd(verbose='y')
	     	self.assertTrue(res)

    	def test_gpstop_fast(self):
		tinctest.logger.info("Running test_gpstop_fast")
		if self.is_gpdb_running():
		   res=self.gps.run_gpstop_cmd(fast='y')
	     	self.assertTrue(res)

    	def test_gpstop_smart(self):
		tinctest.logger.info("Running test_gpstop_smart")
		if self.is_gpdb_running():
		   res=self.gps.run_gpstop_cmd(smart='y')
	     	self.assertTrue(res)

        def test_gpStop_masterOnly(self):
		tinctest.logger.info("Running test_gpstop_masteronly")
		if self.is_gpdb_running():
        	   res=self.gps.run_gpstop_cmd(masteronly='y')
		self.assertTrue(res)

    	def test_gpstop_restart(self):
		tinctest.logger.info("Running test_gpstop_restart")
		if self.is_gpdb_running():
        	   res=self.gps.run_gpstop_cmd(restart='y')
		self.assertTrue(res)

        def test_gpstop_reload(self):
		tinctest.logger.info("Running test_gpstop_reload")
		if self.is_gpdb_running():
        	   res=self.gps.run_gpstop_cmd(reload='y')
		self.assertTrue(res)

        def test_gpstop_timeout(self):
		tinctest.logger.info("Running test_gpstop_timeout")
		if self.is_gpdb_running():
        	   res=self.gps.run_gpstop_cmd(timeout=self.TIMEOUT)
		self.assertTrue(res)

        def test_gpstop_parallelproc(self):
		tinctest.logger.info("Running test_gpstop_parallelproc")
		if self.is_gpdb_running():
        	   res=self.gps.run_gpstop_cmd(parallelproc=self.MAXPARALLELSEG)
		self.assertTrue(res)

        def test_gpstop_notstandby(self):
		tinctest.logger.info("Running test_gpstop_notstandby")
		if self.is_gpdb_running():
        	   res=self.gps.run_gpstop_cmd(notstandby='y')
		self.assertTrue(res)

    	def test_gpstop_logDir(self):
		tinctest.logger.info("Running test_gpstop_logDir")
                self.logdir=''.join([self.basedir,'/logs'])
	        cmd = Command(name='Remove gpstop<nnnn>.log', cmdStr='rm -f %s/gpstop*' % (self.logdir)) 	
		tinctest.logger.info("Removing gpstop<nnnn>.log : %s" % cmd)
		cmd.run(validateAfter=True)
        	result = cmd.get_results()
        	if result.rc != 0 or result.stderr:
                   raise gpstopException("Not able to delete existing gpstop<nnnn>.log")
		lcmd=' '.join(['ls',self.logdir, '| wc -l'])
		res=False
		if self.is_gpdb_running():
        	   res=self.gps.run_gpstop_cmd(logdir=self.logdir)
		if res is not True:
                   raise GPstopError("Error : gpstop_logDir() failed \n")		
		cmd = Command(name='count of  gpstop<nnnn>.log', cmdStr=' %s ' % (lcmd))
		tinctest.logger.info("Count gpstop<nnnn>.log : %s" % cmd)
		cmd.run(validateAfter=True)
        	result = cmd.get_results()
        	if result.rc != 0 or result.stderr:
                   raise gpstopException("Not able to get count of gpstop<nnnn>.log")
		assert int(result.stdout) > 0 

        def is_gpdb_running(self):
		res=False
		ctr=0
		while ctr < self.MAX_TRY: 
		    ctr=ctr+1
	            res=self.gpstartCheck()
                    if res is False:
                        res=self.gp.run_gpstart_cmd(quietmode='y')
		    else:
			break
		if (res is True and ctr < self.MAX_TRY):
			return True
		else:
			return False

        def gpstartCheck(self):
                """
                Checks if the cluster is brought up correctly and all segments are in sync
                """
                bashCmd = 'source ' + (self.gphome)+'/greenplum_path.sh;'+(self.gphome)+'/bin/pg_ctl status -D $MASTER_DATA_DIRECTORY | grep \'pg_ctl: server is running\''
                dbStart = Command(name='gpstartCheck ',cmdStr=bashCmd)
                dbStart.run()
                rc = dbStart.get_results().rc
                if rc != 0:
                        return False
                return True

