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
import time
import string
import tinctest
from gppylib.commands.base import Command
from tinctest.main import TINCException

'''
    Utility  functions for Gpstop
    gpstop function automation
    @class GpStop
    @exception: GPstopException
'''
class GPstopException( TINCException ): pass
class GpStop():
    def __init__(self):
	self.gphome = os.environ.get('GPHOME')
	self.master_dir = os.environ.get('MASTER_DATA_DIRECTORY')	
	if self.gphome is None:
	   raise GPstopException ("GPHOME environment variable is not set")	
	else:
	   if self.master_dir is None:
		raise GPstopException ("MASTER_DATA_DIRECTORY environment variable is not set")

    def run_gpstop_cmd(self, flag = '-a', mdd = None, logdir = None, masteronly = None, immediate = None ,
                       fast = None, smart = None, quietmode = None, restart = None, timeout = None, parallelproc=None,
                       notstandby = None, verbose = None, version = None, standby = None, reload = None, validate = True):
        '''
        GpStop  function
        @param flag: '-a' is the default option considered .Do not prompt the user for confirmation
        @param mdd: The master host data directory.If not specified, the value set for $MASTER_DATA_DIRECTORY will be used
        @param logdir:The directory to write the log file. Defaults to ~/gpAdminLogs.
        @param masteronly: Shuts down only the master node
        @param immediate: Immediate shut down.
        @param fast: Fast shut down.
        @param smart: Smart shut down.
        @param quietmode: Command output is not displayed on the screen
        @param restart: Restart after shutdown is complete
        @param timeout: Specifies a timeout threshold (in seconds) to wait for a segment instance to shutdown
        @type timeout: Integer
        @param parallelproc: The number of segments to stop in parallel
        @type parallelproc: Integer
        @param verbose:Displays detailed status, progress and error messages output by the utility
        @param notstandby:Do not stop the standby master process
        @param version: Displays the version of this utility.
        @param standby:Do not stop the standby master process
        @param reload: This option reloads the pg_hba.conf files of the master and segments and the runtime parameters of the postgresql.conf                       files but does not shutdown the Greenplum Database array
	'''
	
	make_cmd = ''.join([self.gphome,'/bin/gpstop'])
      	
        # Check the version of gpstop
        if version is not None:
            arg = '--version'
            make_cmd = ' '.join([make_cmd,arg])
	    cmd = Command(name='Run gpstop', cmdStr='source %s/greenplum_path.sh;%s' % (self.gphome, make_cmd)) 
	    tinctest.logger.info("Running gpstop : %s" % cmd)
	    cmd.run(validateAfter=validate)
            result = cmd.get_results()
            if result.rc != 0 or result.stderr:
        	return False
            else:
		tinctest.logger.info((result))
                return True

        # -d The master host data directory
        if mdd is None:
            mdd = ""
        else:
            mdd = " -d %s"  % self.master_dir

	# -q Quietmode
        if quietmode is None:
            quietmode = ""
        else:
            quietmode = "-q"

	# -v Verbose
        if verbose is None:
            verbose = ""
        else:
            verbose = " -v"
	# -y notstandby
        if notstandby is None:
            notstandby = ""
        else:
            notstandby = " -y"

	# -t nnn Timeout
        if timeout is None:
            timeout = ""
        else:
            # Check if timeout is an integer
            try:
                int(timeout)
                timeout=" -t %s" % timeout
            except ValueError, e:
                if e is not None:
	            raise GPstopException ("Gpstop timeout is not set correctly!")

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
	            raise GPstopException ("Gpstop parallelproc is not set correctly!")
        if logdir is None:
		logdir = " "
	else:
		logdir='-l '+ (logdir)
        make_cmd = ' '.join([make_cmd,mdd,quietmode,verbose,notstandby,timeout,logdir,parallelproc])	
        try:
           if immediate is not None:
                make_cmd = ' '.join([make_cmd, " -M immediate"])

           elif masteronly is not None:
                make_cmd = ' '.join([make_cmd, " -m"])

           elif fast is not None:
                make_cmd = ' '.join([make_cmd," -M fast"])

           elif smart is not None:
                make_cmd = ' '.join([make_cmd,"  -M smart"])

           elif restart is not None:
                make_cmd = ' '.join([make_cmd," -r"])

           elif reload is not None:
                make_cmd = ' '.join([make_cmd," -u"])
           else:
                make_cmd = ' '.join([make_cmd,''])
	except Exception, e:
            if e is not None:
                raise

	make_cmd = ' '.join([make_cmd,'-a'])
	cmd = Command(name='Run gpstop', cmdStr='source %s/greenplum_path.sh;%s' % (self.gphome, make_cmd)) 
	tinctest.logger.info("Running gpstop : %s" % cmd)
	cmd.run(validateAfter=validate)
        result = cmd.get_results()
        if result.rc != 0 or result.stderr:
        	return False
        else:
		tinctest.logger.info((result))
        return True


	def get_version(self):
		self.run_gpstop_cmd(version='y')	
