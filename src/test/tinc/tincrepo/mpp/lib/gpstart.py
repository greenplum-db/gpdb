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

GpStart.py - utility for executing gpstart with various parameters
"""

import os
import sys
import pexpect
import string
import tinctest
from gppylib.commands.base import Command
from tinctest.main import TINCException
'''
    Utility  functions for Gpstart
    gpstart function automation
    @class GpStart
    @exception: GPstartException
'''
class GPstartException( Exception ): pass
class GpStart():
    def __init__(self):
	self.MYD = os.path.abspath(os.path.dirname(__file__))
	self.MAX_TRY=5
        self.gphome = os.environ.get('GPHOME')
        self.master_dir = os.environ.get('MASTER_DATA_DIRECTORY')
        if self.gphome is None:
           raise GPstartException ("GPHOME environment variable is not set")
        else:
           if self.master_dir is None:
                raise GPstartException ("MASTER_DATA_DIRECTORY environment variable is not set")


    def run_gpstart_cmd(self, flag ='-a', logdir = None, mdd = None, masteronly = None, 
                        quietmode = None, restrict = None, parallelproc = None, verbose = None, 
                        notstandby = None, version = None, timeout = None ):
        '''
	run_gpstart_cmd function

        @param flag: '-a' is the default option considered
        @param logdir: The directory to write the log file in. Default: None, The logfile is created at ~/gpAdminLogs.
        @param masteronly: Start the Greenplum master instance only and connect in utility mode
        @param mdd: The master host data directory. If not specified, the value set for $MASTER_DATA_DIRECTORY will be used.
        @param quietmode: Run in quiet mode.
        @param restrict: Start a Greenplum Database system in restricted mode (only allow superuser connections)
        @param timeout: Specifies a timeout in seconds to wait for a segment instance to start up.
        @type timeout: Integer
        @param parallelproc: The number of segments to start in parallel
        @type parallelproc: Integer
        @param verbose: Displays detailed status, progress and error messages output by the utility.
        @param version: Displays the version of this utility
        @param standby: To be added. Do not start the standby master host.
        @param parallel: To be added. The number of segments to start in parallel.
        @param notstandby: Do not start the standby master process
        '''

	make_cmd = ''.join([self.gphome,'/bin/gpstart'])

        # Check the version of gpstart
        if version is not None:
            arg = '--version'
            make_cmd = ' '.join([make_cmd,arg])
            cmd = Command(name='Run gpstart', cmdStr='source %s/greenplum_path.sh;%s' % (self.gphome, make_cmd))
            tinctest.logger.info("Running gpstart : %s" % cmd)
            cmd.run(validateAfter=True)
            result = cmd.get_results()
            if result.rc != 0 or result.stderr:
                return False
            else:
                tinctest.logger.info((result))
                return True


        #check rest of the flags
        if logdir is None:
            logdir = ""
        else:
            logdir = "-l %s" % logdir

        if mdd is None:
            mdd = ""
        else:
            mdd = "-d %s" % self.master_dir

        if quietmode is None:
            quietmode = ""
        else:
            quietmode = "-q"

        if restrict is None:
            restrict = ""
        else:
            restrict = "-R"

        if verbose is None:
            verbose = ""
        else:
            verbose = "-v"

        # -y notstandby
        if notstandby is None:
            notstandby = ""
        else:
            notstandby = " -y"


        if timeout is None:
            timeout = ""
        else:
            # Check if timeout is an integer
            try:
                int(timeout)
                timeout = "-t %s" % timeout
            except ValueError, e:
                if e is not None:
                    raise GPstartException ("Gpstart timeout is not set correctly!")

       # -B nnn Parallel Process
        if parallelproc is None:
            parallelproc = ""
        else:
            # Check if parallelprocs is an integer
            try:
                int(parallelproc)
                parallelproc=" -B %s" % parallelproc
            except ValueError, e:
                if e is not None:
                    raise GPstartException ("Gpstart parallelproc is not set correctly!")

	make_cmd = ' '.join([make_cmd,logdir, mdd, quietmode, restrict, verbose, timeout,parallelproc])	
	make_cmd = ' '.join([make_cmd,'-a'])
        tinctest.logger.info("Intrim gpstart command is : %s" % make_cmd)

        if masteronly is None:
		cmd = Command(name='Run gpstart', cmdStr='source %s/greenplum_path.sh;%s' % (self.gphome, make_cmd))
        	tinctest.logger.info("Running gpstart : %s" % cmd)
        	cmd.run(validateAfter=True)
        	result = cmd.get_results()
        	if result.rc != 0 :
                        tinctest.logger.info((result))
                	return False
        	else:
                	tinctest.logger.info((result))
        		return True
        else:
                self.startMaster()



    def get_version(self):
	'''
	--version Displays gpstart utility version
        '''
        return( self.run_gpstart_cmd(version='y'))


    def startMaster(self):
                cmd="/bin/bash -c 'source %s/greenplum_path.sh;'" % (self.gphome)
                cmd=cmd+"gpstart -m -a"
                tinctest.logger.info("Running gpstart : %s" % cmd)
                file_path = self.MYD+'/logfileGpstart'
                filelog = open((file_path),'w+')
                child = pexpect.spawn(cmd,timeout=40000)
                child.logfile = filelog
		check=10
		ctr=0
		while ctr < self.MAX_TRY:
			ctr=ctr+1
			check=child.expect ('.*.')
			if check == 0:
				break
		if check != 0:
			raise GPstartException('gpstart not asking for confirmation!')
                child.sendline ('y')
		try:
			check=10
			ctr=0
			while ctr < self.MAX_TRY:
				ctr=ctr+1;
                		check=child.expect('.*Database successfully started*')
				if check == 0:
					break
	        	if check != 0:
			   raise GPstartException('Gpstart -m did not work')		
                      	child.close()
                except pexpect.ExceptionPexpect, e:
                   if e is not None:
			raise GPstartException('Failure in pexpect child process')
		      	return False			
 		   else:
		      	return True

	

    def gpstart_quiet(self):
		res=self.run_gpstart_cmd(quietmode = 'y')
		return res

