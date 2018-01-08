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
import tinctest
from tinctest.lib import local_path
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.gpactivatestandby import GpactivateStandby
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility

gputil = GpUtility()

class BkupRestore(MPPTestCase):
    
    dbname = 'bkdb'
    standby_port = '5433'
    standby_dirname = '_newstandby'

    def __init__(self,methodName):
        self.gpact = GpactivateStandby()
        self.gpinit = GpinitStandby()
        self.runmixin = StandbyRunMixin()
        self.runmixin.createdb(dbname=self.dbname)
        self.bkup_timestamp = ""
        self.gphome = os.environ.get('GPHOME')
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.pgport = os.environ.get('PGPORT')
        self.host = socket.gethostname()
        self.standby_loc = os.path.split(self.mdd)[0]+self.standby_dirname
        super(BkupRestore,self).__init__(methodName)

    def createdb(self):
        PSQL.run_sql_command('Drop database %s;Create database %s;' %(self.dbname, self.dbname), dbname='postgres')

    def run_backup(self):
        #Cleanup db_dumps folder before running backup
        cleanup_cmd = "gpssh -h %s -e 'rm -rf /tmp/db_dumps'" % (self.host)
        cmd = Command('cleanup', cleanup_cmd)
        cmd.run(validateAfter=False)
        gpc_cmd = 'gpcrondump -a -x %s -u /tmp' % self.dbname
        cmd = Command(name='Run gpcrondump', cmdStr=gpc_cmd)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        if result.rc != 0:
            return False
        else:
            self.bkup_timestamp = self.get_timestamp(result.stdout)
            return True

    def validate_timestamp(self, ts):
        try:
            int_ts = int(ts)
        except Exception as e:
            raise Exception('Timestamp is not valid %s' % ts)

        if len(ts) != 14:
            raise Exception('Timestamp is invalid %s' % ts) 

    def get_timestamp(self, result):
        for line in result.splitlines():
            if 'Timestamp key = ' in line:
                log_msg, delim, timestamp = line.partition('=') 
                ts = timestamp.strip()
                self.validate_timestamp(ts)
                return ts
        raise Exception('Timestamp key not found')

    def run_restore(self):
        gpr_cmd = 'gpdbrestore -t %s -a -u /tmp' % self.bkup_timestamp
        (rc, result) = self.run_remote(self.host, gpr_cmd, pgport=self.standby_port, standbydd=self.standby_loc)
        if rc != 0:
            return False
        return True

    def create_standby(self, local=True):
        ''' Create a standby '''
        gputil.create_dir(self.host,self.standby_loc)
        gputil.clean_dir(self.host,self.standby_loc)
        self.gpinit.run(option = '-P %s -s %s -F %s' % (self.standby_port, self.host, self.standby_loc))
        
    def run_remote(self, standbyhost, rmt_cmd, pgport = '', standbydd = ''):
        '''Runs remote command and returns rc, result '''
        export_cmd = "source %s/greenplum_path.sh;export PGPORT=%s;export MASTER_DATA_DIRECTORY=%s" % (self.gphome, pgport, standbydd)
        remote_cmd = "gpssh -h %s -e '%s;%s' " % (standbyhost, export_cmd, rmt_cmd)
        cmd = Command(name='Running Remote command', cmdStr='%s' % remote_cmd)
        tinctest.logger.info(" %s" % cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        return result.rc,result.stdout

    def run_workload(self, dir, verify = False):
        tinctest.logger.info("Running workload ...")
        load_path = local_path(dir) + os.sep
        for file in os.listdir(load_path):
            if file.endswith(".sql"):
                out_file = file.replace(".sql", ".out")
                PSQL.run_sql_file(sql_file = load_path + file, dbname = self.dbname, port = self.pgport, out_file = load_path + out_file)
        if verify == True:
            self.validate_sql_files(load_path)

    def validate_sql_files(self, load_path):
        for file in os.listdir(load_path):
            if file.endswith(".out"):
                out_file = file
                ans_file = file.replace('.out' , '.ans')
                if os.path.exists(load_path + ans_file):
                    assert Gpdiff.are_files_equal(load_path + out_file, load_path + ans_file)
                else:
                    raise Exception("No .ans file exists for %s " % out_file)

    def failback(self):
        gputil.failback_to_original_master(self.mdd, self.host, self.standby_loc, self.standby_port)
