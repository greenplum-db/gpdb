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
import re

import tinctest
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.walrepl.run import StandbyRunMixin
from mpp.gpdb.tests.storage.walrepl import lib as walrepl
from mpp.gpdb.tests.storage.walrepl.lib import DbConn
from mpp.gpdb.tests.storage.walrepl.lib import SmartCmd

from mpp.gpdb.tests.storage.walrepl.lib import WalReplException

class StandbyVerify(object):
    '''Class for standby verification 
       Disclaimer: Some of these may repeat with the mpp/lib version'''
    def __init__(self):
        self.stdby = StandbyRunMixin()
        

    def _run_remote_command(self, host, command):
        rmt_cmd = "gpssh -h %s -e '%s' " % (host, command)
        cmd = Command(name='Running a remote command', cmdStr = rmt_cmd)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        tinctest.logger.info('%s\n%s' %(rmt_cmd, result.stdout))
        return result.stdout

    def get_pg_stat_replication(self):
        '''Returns the pg_stat_replication result as a list'''
        if self.stdby.wait_for_walsender() == 0:
            raise WalReplException('Standby Replication has not started')

        with DbConn(utility=True, dbname='template1') as conn:
            return conn.execute("""
                SELECT
                    procpid,
                    usesysid,
                    usename,
                    application_name,
                    client_addr,
                    client_port,
                    backend_start,
                    state,
                    sent_location,
                    write_location,
                    flush_location,
                    replay_location,
                    sync_priority,
                    sync_state
                FROM
                    pg_stat_replication
             """)

    def check_gp_segment_config(self):
        ''' Check for the new entry in gp_segment_configuration'''
        sm_count = PSQL.run_sql_command("select count(*) from gp_segment_configuration where content='-1' and role='m';", flags='-q -t', dbname='postgres')
        if int(sm_count.strip()) != 1:
            return False
        tinctest.logger.info('A new entry is added for standby in gp_segment_configuration')
        return True

    def check_pg_stat_replication(self):
        '''Check the state and sync_state from pg_stat_replication '''
        for i in walrepl.polling(max_try=20, interval=0.5):
            res = self.get_pg_stat_replication()
            if len(res) == 0:
                continue
            elif res[0].state == 'streaming' and res[0].sync_state == 'sync':
                tinctest.logger.info('pg_stat_replication is updated with the information')
                return True
            else:
                continue 
        return False

    def check_standby_processes(self):
        '''Check if all the standby processes are present '''

        # Get hostname and data directory of standby, if any.
        # We could use gparray, but for now let's stay away from gppylib
        with DbConn(dbname='postgres') as conn:
            results = conn.execute("""
                SELECT hostname, fselocation
                FROM gp_segment_configuration
                INNER JOIN pg_filespace_entry ON dbid = fsedbid
                WHERE fsefsoid = 3052 AND content = -1 AND role = 'm'
                """)
            # If standby is not configured, there must not be any standby processes.
            if len(results) == 0:
                return False
            host = results[0].hostname
            datadir = results[0].fselocation

        # We look for these processes that are spawned from standby postmaster.
        # They should have postmaster pid as ppid.  We minimize remote operation
        # cost by getting ps output once, and search for these strings from the
        # output lines using regexp.
        process_list  = ['master logger process', 'startup process', 'wal receiver process']
        target_process = '(' + '|'.join(process_list) + ')'
        postmaster_pid = walrepl.get_postmaster_pid(datadir, host)
        # If postmaster does not exit, child processes are not present.
        if postmaster_pid == -1:
            return False

        # Make it string for the later comparison
        postmaster_pid = str(postmaster_pid)
        cmd = SmartCmd('ps -e -o ppid,command | grep [p]ostgres', host=host)
        cmd.run()
        standby_processes = []
        for line in cmd.get_results().stdout.splitlines(True):
            line = line.strip()
            (ppid, command) = re.split(r'\s+', line, 1)
            if ppid == postmaster_pid and re.search(target_process, command):
                standby_processes.append(line)

        # If we found more or less than expected, we don't know.
        if len(standby_processes) != len(process_list):
            return False
        tinctest.logger.info('All the standby processes are present at standby host''')
        return True
