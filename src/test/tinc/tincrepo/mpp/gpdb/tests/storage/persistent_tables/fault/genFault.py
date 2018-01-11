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

import tinctest
import os
import random

from time import sleep
from random import randint
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command
from subprocess import Popen, PIPE
from mpp.lib.config import GPDBConfig

GPDBNAME = str(os.environ["PGDATABASE"])

'''
Creates Faults for the scenario
'''
class Fault(Command):
    def __init__(self, cmd = None):
        Command.__init__(self, 'Running fault command', cmd)

    def _run_sys_cmd(self, cmd_str, validate = False):
        '''helper function to run a sys cmd'''
        tinctest.logger.info("execute:" +cmd_str)
        cmd = Fault(cmd_str)
        cmd.run(validateAfter = validate)
        return cmd.get_results()

    def stop_db(self,options = None):
        ''' Stops the greenplum DB based on the options provided '''
        cmd_str = "source $GPHOME/greenplum_path.sh; gpstop -a"
        if options is None:
            options = ''
            
        cmd_str = cmd_str + options
        tinctest.logger.info("Starting the db operation: %s "%cmd_str)
        result = self._run_sys_cmd(cmd_str)
        return result
    
    def start_db(self,options = None):
        ''' Start the greenplum DB based on the options provided '''
        cmd_str = "source $GPHOME/greenplum_path.sh; gpstart -a"
        if options is None:
            options = ''

        cmd_str = cmd_str + options
        tinctest.logger.info("Starting the db operation: %s "%cmd_str)
        result = self._run_sys_cmd(cmd_str)
        return result

    def restart_db(self,options = 'ir'):
        ''' Restarts the greenplum DB '''
        return self.stop_db(options)

    def run_recovery(self,options = None):
        '''Runs the incremental recovery'''
        tinctest.logger.info('Invoking gprecoverseg to bring the segments up')
        if options is None:
            options = ''
        cmd_str = "source $GPHOME/greenplum_path.sh; gprecoverseg -a" + options

        result = self._run_sys_cmd(cmd_str, False)
        return result.stdout

    def drop_db(self, dbname = GPDBNAME):
        ''' Drop database '''
        tinctest.logger.info('Drop database ' + dbname)
        cmd = 'drop_db '+ dbname
        result = self._run_sys_cmd(cmd)
        tinctest.logger.info(result.stderr) 

    def create_db(self, dbname = GPDBNAME):
        ''' Create Database '''
        tinctest.logger.info('Create database ' + dbname)
        cmd = 'createdb '+ dbname
        result = self._run_sys_cmd(cmd)
        tinctest.logger.info(result.stderr)

    def rebalance_cluster(self):
        config = GPDBConfig()
        self.run_recovery('r')
        rtrycnt = 0
        while ((config.is_not_insync_segments()) == False and rtrycnt <= 5):
            tinctest.logger.info("Waiting [%s] for DB to recover" %rtrycnt)
            sleep(10)
            rtrycnt = rtrycnt + 1
        #Many time it has been observed that gprecoverseg -ar marks segment down
        if config.is_not_insync_segments():
            return True
        else:
            self.run_recovery()
            rtrycnt = 0
            max_rtrycnt = 10
            while ((config.is_not_insync_segments()) == False and rtrycnt < max_rtrycnt):
                tinctest.logger.info("waiting [%s] for DB to recover" %rtrycnt)
                sleep(10)
                rtrycnt = rtrycnt + 1
            if rtrycnt < max_rtrycnt:
                return True
            else:
                tinctest.logger.error("Segments not up after incremental recovery!!")
                return False
    
