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
import subprocess
import socket
from time import sleep
import tinctest
from tinctest.lib import local_path
from gppylib.commands.base import Command
from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.lib.pg_util import GpUtility
from mpp.gpdb.tests.storage.walrepl.gpinitstandby import GpinitStandby
from mpp.gpdb.tests.storage.walrepl.lib.verify import StandbyVerify
from mpp.gpdb.tests.storage.walrepl.lib.standby import Standby

config = GPDBConfig()

class OOMClass(object):
    '''Class for methods required for OOM testcase'''

    standby_port = '5433'
    standby_dirname = 'newstandby'

    def __init__(self):
        self.gpinit = GpinitStandby()
        self.mdd = os.environ.get('MASTER_DATA_DIRECTORY')
        self.config = GPDBConfig()
        self.pgutil = GpUtility()
        self.verify = StandbyVerify()
        self.host = socket.gethostname()
        self.standby_loc = os.path.join(os.path.split(self.mdd)[0],
                                        self.standby_dirname)
        self.standby = Standby(self.standby_loc, self.standby_port)

    def create_standby(self):
        self.pgutil.clean_dir(self.host,self.standby_loc)
        self.gpinit.run(option = '-P %s -s %s -F pg_system:%s' % (self.standby_port, self.host, self.standby_loc))

    def setup_oom(self):
        # Build it before testing.
        thisdir = os.path.dirname(__file__)
        builddir = os.path.join(thisdir, 'lib')
        subprocess.check_call(['make', '-C', builddir, 'install'])

        #Copy oom_malloc.so and wrapper.sh to all the segment nodes
        for host in config.get_hosts() :
            if host.strip() == self.host :
                continue
            cmd = "gpssh -h %s -e 'mkdir -p %s'; scp %s/oom_malloc.so %s:%s/; scp %s/wrapper.sh %s:%s/" % (host.strip(), builddir, builddir, host.strip(), builddir, builddir, host.strip(), builddir)
            self.pgutil.run(cmd)

    def touch_malloc(self):
        # Touch file oom_malloc in standby directory
        cmd = 'touch %s/oom_malloc' % self.standby_loc
        self.pgutil.run(cmd)

    def startdb(self):
        (rc, result) = self.pgutil.run('gpstart -a --wrapper %s' % (local_path('lib/wrapper.sh')))
        if rc != 0 and 'Could not start standby master' in result :
            return False
        return True

    def restartdb(self):
        # Remove file oom_malloc from standby
        cmd = 'rm %s/oom_malloc' % self.standby_loc
        self.pgutil.run(cmd)
        (rc, result) = self.pgutil.run('gpstop -ar')
        if rc == 0 and (self.verify.check_pg_stat_replication()):
            return True
        return False

    def psql_and_oom(self):
        #Touch oom_malloc in standby_dir and issue PSQL : Check if processes are gone
        self.touch_malloc()
        PSQL.run_sql_command('Drop table if exists wal_oomt1;Create table wal_oomt1(a1 int, a2 text) with(appendonly=true);')
        sleep(2)
        if not (self.verify.check_standby_processes()):
            return True
        return False 

    def start_standby(self):
        # Remove oom_malloc and start standby : Check if all processes are back
        cmd = 'rm %s/oom_malloc' % self.standby_loc
        self.pgutil.run(cmd)
        res = self.standby.start()
        sleep(2)
        if (self.verify.check_standby_processes()) :
            return True
        return False
