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
import tinctest
from gppylib.commands.base import Command, REMOTE, WorkerPool
from tinctest.main import TINCException
from mpp.lib.config import GPDBConfig

class GpRecoversegException(TINCException): pass

class GpRecoverseg():
    '''Class for gprecoverseg utility methods '''

    def __init__(self):
        self.gphome = os.environ.get('GPHOME')
        self.rc = None
        self.stdout = None
        self.stderr = None

    def run_using_workerpool(self, option=''):
        if not (set(option.split()) <= set(['-F' , '-r', '--persistent-check', ' '])):
            raise GpRecoversegException('Not a valid option with gprecoverseg')

        rcvr_cmd = 'gprecoverseg -a  %s' % option
        cmd = Command(name='Run gprecoverseg', cmdStr='source %s/greenplum_path.sh;%s' % (self.gphome, rcvr_cmd))
        tinctest.logger.info("Running gprecoverseg : %s" % cmd)

        pool = WorkerPool(numWorkers=1, daemonize=True)
        pool.addCommand(cmd)

    def run(self,option=' ', validate=True, results=True):
        '''
        @type option: string
        @param option: gprecoverseg option (-F or -r)
        ''' 
        if not (set(option.split()) <= set(['-F' , '-r', '--persistent-check', ' '])):
            raise GpRecoversegException('Not a valid option with gprecoverseg')
        rcvr_cmd = 'gprecoverseg -a  %s' % option
        cmd = Command(name='Run gprecoverseg', cmdStr='source %s/greenplum_path.sh;%s' % (self.gphome, rcvr_cmd))
        tinctest.logger.info("Running gprecoverseg : %s" % cmd)
        cmd.run(validateAfter=validate)

        if results:
            result = cmd.get_results()
            self.rc, self.stdout, self.stderr = result.rc, result.stdout, result.stderr
            if result.rc != 0 or result.stderr:
                return False
            return True

    def wait_till_insync_transition(self):
        pass

class GpRecover(GpRecoverseg):
    '''Class for gprecoverseg utility methods '''

    MAX_COUNTER=400

    def __init__(self, config=None):
        if config is not None:
            self.config = config
        else:
            self.config = GPDBConfig()
        self.gphome = os.environ.get('GPHOME')

    def incremental(self, workerPool=False):
        '''Incremental Recoverseg '''
        tinctest.logger.info('Running Incremental gprecoverseg...')
        if workerPool:
            return self.run_using_workerpool()
        else:
            return self.run()

    def full(self):
        '''Full Recoverseg '''
        tinctest.logger.info('Running Full gprecoverseg...')
        return self.run(option = '-F')

    def rebalance(self):
        '''Run gprecoverseg to rebalance the cluster '''
        tinctest.logger.info('Running gprecoverseg rebalance...')
        return self.run(option = '-r')

    def wait_till_insync_transition(self):
        '''
            Poll till all the segments transition to insync state. 
            Number of trials set to MAX_COUNTER
        '''
        counter= 1
        while(not self.config.is_not_insync_segments()):
            if counter > self.MAX_COUNTER:
                raise Exception('Segments did not come insync after 20 minutes')
            else:
                counter = counter + 1
                time.sleep(3) #Wait 3 secs before polling again
        tinctest.logger.info('Segments are synchronized ...')
        return True
        
    def recover_rebalance_segs(self):
        if not self.config.is_balanced_segments():
            # recover
            if not self.incremental():
                raise Exception('Gprecvoerseg failed')
            if not self.wait_till_insync_transition():
                raise Exception('Segments not in sync')
            tinctest.logger.info('Segments recovered and back in sync')

            # rebalance
            if not self.rebalance():
                raise Exception('Gprecvoerseg -r failed')
            if not self.wait_till_insync_transition():
                raise Exception('Segments not in sync')
            tinctest.logger.info('Segments rebalanced and back in sync')
