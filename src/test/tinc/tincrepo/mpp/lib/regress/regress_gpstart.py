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

Unit tests fro GpStart module
Tests if database is running 
"""

import os
import unittest2 as unittest
import tinctest
from gppylib.commands.base import Command
from mpp.lib.gpstart import GpStart, GPstartException
from mpp.lib.gpstop import GpStop, GPstopException

class gpStartTestCase(unittest.TestCase):
	def setUp(self):
                self.basedir = os.path.dirname(__file__)
		self.gphome = os.environ.get('GPHOME')
                self.gp=GpStart()
                self.gps=GpStop()
                self.MAX_TRY=3
                self.TIMEOUT=90
                self.MAXPARALLELSEG=60

    	def test_gpstart_logDir(self):
                tinctest.logger.info("Running test_gpstart_logDir")
                self.logdir=''.join([self.basedir,'/logs'])
                cmd = Command(name='Remove gpstop<nnnn>.log', cmdStr='rm -f %s/gpstop*' % (self.logdir))
                tinctest.logger.info("Removing gpstop<nnnn>.log : %s" % cmd)
                cmd.run(validateAfter=True)
                result = cmd.get_results()
                if result.rc != 0 or result.stderr:
                   raise gpstopException("Not able to delete existing gpstop<nnnn>.log")
                lcmd=' '.join(['ls',self.logdir, '| wc -l'])
                res=False
                if self.is_not_running_gpdb():
                   res=self.gp.run_gpstart_cmd(logdir=self.logdir)
                if res is not True:
                   raise GPstopError("Error : run_gpstart_cmd(logdir) failed \n")
                cmd = Command(name='count of  gpstart<nnnn>.log', cmdStr=' %s ' % (lcmd))
                tinctest.logger.info("Count gpstart<nnnn>.log : %s" % cmd)
                cmd.run(validateAfter=True)
                result = cmd.get_results()
                if result.rc != 0 or result.stderr:
                   raise gpstopException("Not able to get count of gpstart<nnnn>.log")
                assert int(result.stdout) > 0

    	def test_gpstart_getversion(self):
        	res=self.gp.get_version()
		self.assertTrue(res)
    	def test_gpstart_restrict(self):
                tinctest.logger.info("Running test_gpstart_restrict")
                if self.is_not_running_gpdb():
			res=self.gp.run_gpstart_cmd(restrict ='y')
		self.assertTrue(res)

    	def test_gpstart_timeout(self):
                tinctest.logger.info("Running test_gpstart_timeout")
                if self.is_not_running_gpdb():
			res=self.gp.run_gpstart_cmd(timeout = self.TIMEOUT)
		self.assertTrue(res)

    	def test_gpstart_parallelproc(self):
                tinctest.logger.info("Running test_gpstart_parallelproc")
                if self.is_not_running_gpdb():
			res=self.gp.run_gpstart_cmd(parallelproc=self.MAXPARALLELSEG)
		self.assertTrue(res)

    	def test_gpstart_noprompt(self):
                tinctest.logger.info("Running test_gpstart_noprompt")
                if self.is_not_running_gpdb():
			res=self.gp.run_gpstart_cmd()
		self.assertTrue(res)


        def test_gpstart_cmd_masterOnly(self):
                tinctest.logger.info("Running test_gpstart_cmd_masterOnly")
                if self.is_not_running_gpdb():
			self.gp.run_gpstart_cmd(masteronly='y')
		res=self.gpstartCheck()
		self.assertTrue(res)

        def test_gpstart_cmd_quiet(self):
                tinctest.logger.info("Running test_gpstart_cmd_quiet")
                if self.is_not_running_gpdb():
			res=self.gp.run_gpstart_cmd(quietmode='y')
		self.assertTrue(res)
        def test_gpstart_cmd_startcluster(self):
                tinctest.logger.info("Running test_gpstart_cmd_startcluster")
                if self.is_not_running_gpdb():
			res=self.gp.run_gpstart_cmd()
		self.assertTrue(res)

        def test_gpstart_cmd_verbose(self):
                tinctest.logger.info("Running test_gpstart_cmd_verbose")
		if self.is_not_running_gpdb():
			res=self.gp.run_gpstart_cmd(verbose='y')
		self.assertTrue(res)

    	def test_gpstart_check(self):
        	if not self.gpstartCheck():
			res2=self.gp.gp.run_gpstart_cmd()
			res=self.gpstartCheck()
			self.assertTrue(res)

	def test_func_gpstart_quiet(self):
               if self.is_not_running_gpdb():
                        res=self.gp.gpstart_quiet()
               self.assertTrue(res)

	

       	def is_not_running_gpdb(self):
                res=False
                ctr=0
                while ctr < self.MAX_TRY:
                    ctr=ctr+1
                    res=self.gpstartCheck()
                    if res is True:
			self.gps.run_gpstop_cmd(quietmode='y')
                    else:
                        return True
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

