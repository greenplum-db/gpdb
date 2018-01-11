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
from tinctest.lib import Gpdiff
from tinctest.lib.system import TINCSystem
from gppylib.commands.base import *

from mpp.models import MPPTestCase
from mpp.models.sql_tc import SQLTestCase
from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from mpp.gpdb.tests.storage.lib.sql_isolation_testcase import SQLIsolationTestCase
from mpp.lib.filerep_util import Filerepe2e_Util

import pygresql.pg
import threading
import time

def installExtension(extdir):
    '''
    Install C UDF to all hosts
    '''

    query_result = PSQL.run_sql_command('select distinct hostname from gp_segment_configuration', flags='-t -A')
    hosts = []
    for host in query_result.strip().split('\n'):
        assert(len(host.strip()) > 0)
        hosts.append('-h ' + host)

    assert(len(hosts) > 0)
    hosts = ' '.join(hosts)

    make = Command('make', 'make -C {extdir}'.format(extdir=extdir))
    make.run(validateAfter=True)

    gpscp = Command('gpscp', 'gpscp {hosts} {extdir}/*.so =:$GPHOME/lib/postgresql/'.format(
                              extdir=extdir, hosts=hosts))
    gpscp.run(validateAfter=True)

    install_file = os.path.join(extdir, 'install.sql')
    res = PSQL.run_sql_file(install_file, out_file=install_file + '.out')
    assert(res == True)

class test_crossexec(SQLTestCase):
    '''
    @product_version gpdb: [4.3.3.0-]
    '''

    sql_dir = 'crossexec'
    ans_dir = 'crossexec'

    @classmethod
    def setUpClass(cls):
        extdir = os.path.join(cls.get_source_dir(), 'lockrelease')
        installExtension(extdir)

    def tearDown(self):
        Filerepe2e_Util().inject_fault(f='all', y='reset', H='ALL', r='primary')
        Filerepe2e_Util().inject_fault(f='all', y='reset', seg_id='1')
        super(test_crossexec, self).tearDown()

    def check_fault_triggered(self, fault_name, status='triggered'):
        # TODO Filerepe2e_Util.check_status should take seg_id parameter to work with master
        poll = 0
        triggered = False
        filereputil = Filerepe2e_Util()
        while poll < 10 and not triggered:
            status = 'triggered'
            (ok, out) = filereputil.inject_fault(f=fault_name, y='status', seg_id='1')
            for line in out.splitlines():
                if line.find(fault_name) > 0 and line.find(status) > 0:
                    poll = 0
                    triggered = True
                    break
            time.sleep(1)
        self.assertTrue(triggered, 'fault should be ' + status)

    def get_file_names(self, suffix):
        stem = self.method_name.replace('test_', '')
        sql_file_root = os.path.join(self.get_sql_dir(), stem  + '.sql')
        sql_file = sql_file_root + '.' + suffix
        ans_file = sql_file.replace('.sql.', '.ans.')
        out_file_name = os.path.basename(sql_file).replace('.sql.', '.out.')
        out_file = os.path.join(self.get_out_dir(), out_file_name)
        TINCSystem.make_dirs(self.get_out_dir(), ignore_exists_error=True)
        return (sql_file, ans_file, out_file)

    def run_psql(self, psql):
        return psql.run()

    def test_insert_commit_before_truncate(self):
        '''
        @description We suspend the vacuum on master after the first
                     transaction, and connect to segment.  Modify the
                     relation in vacuum and commit the segment local
                     transaction before the truncate transaction starts.
        '''
        fault_name = 'vacuum_relation_end_of_first_round'

        gpdbconfig = GPDBConfig()
        seghost, segport = gpdbconfig.get_hostandport_of_segment(0, 'p')
        filereputil = Filerepe2e_Util()
        filereputil.inject_fault(f=fault_name, y='suspend', seg_id='1')

        # run vacuum in background, it'll be blocked.
        sql_file1, ans_file1, out_file1 = self.get_file_names('conn1')
        psql1 = PSQL(sql_file=sql_file1, out_file=out_file1)
        thread1 = threading.Thread(target=self.run_psql, args=(psql1,))
        thread1.start()

        self.check_fault_triggered(fault_name)

        sql_file2, ans_file2, out_file2 = self.get_file_names('conn2')
        # utility to seg0
        psql2 = PSQL(sql_file=sql_file2, out_file=out_file2,
                     host=seghost, port=segport,
                     PGOPTIONS='-c gp_session_role=utility')
        self.run_psql(psql2)

        # resume vacuum
        filereputil.inject_fault(f=fault_name, y='reset', seg_id='1')
        thread1.join()
        self.assertTrue(Gpdiff.are_files_equal(out_file1, ans_file1))
        self.assertTrue(Gpdiff.are_files_equal(out_file2, ans_file2))

    def test_insert_unlock_before_truncate(self):
        '''
        @description This is rather complicated.  We suspend the vacuum on
                     master after the first transaction, and connect to
                     segment, modify the relation in question, and release the
                     lock, keep the transaction.  To release the lock, we need
                     a special UDF.  Vacuum is supposed to skip truncate if it
                     sees such in-progress transaction.  Usually this should
                     not happen, but it rather simulates catalog DDL.
        '''
        fault_name = 'vacuum_relation_end_of_first_round'

        gpdbconfig = GPDBConfig()
        seghost, segport = gpdbconfig.get_hostandport_of_segment(0, 'p')
        filereputil = Filerepe2e_Util()
        filereputil.inject_fault(f=fault_name, y='suspend', seg_id='1')

        PSQL.run_sql_command(sql_cmd='drop table if exists sync_table; create table sync_table(a int)')
        # Use pygresql to keep the connection and issue commands seprately.
        # thread2 will wait on sync_table before finish its work, so we
        # can keep the transaction open until the vacuum completes its work.
        conn = pygresql.pg.connect(host=seghost, port=int(segport), opt='-c gp_session_role=utility')
        conn.query('begin')
        conn.query('lock sync_table in access exclusive mode')

        # run vacuum background, it'll be blocked.
        sql_file1, ans_file1, out_file1 = self.get_file_names('conn1')
        psql1 = PSQL(sql_file=sql_file1, out_file=out_file1)
        thread1 = threading.Thread(target=self.run_psql, args=(psql1,))
        thread1.start()

        self.check_fault_triggered(fault_name)

        sql_file2, ans_file2, out_file2 = self.get_file_names('conn2')
        # utility to seg0
        psql2 = PSQL(sql_file=sql_file2, out_file=out_file2,
                     host=seghost, port=segport,
                     PGOPTIONS='-c gp_session_role=utility')
        thread2 = threading.Thread(target=self.run_psql, args=(psql2,))
        thread2.start()

        # resume vacuum
        filereputil.inject_fault(f=fault_name, y='reset', seg_id='1')

        # Once thread1 finishes, we can now release the lock on sync_table,
        # so that thread2 can proceed.
        thread1.join()
        conn.query('commit')
        thread2.join()

        self.assertTrue(Gpdiff.are_files_equal(out_file1, ans_file1))
        self.assertTrue(Gpdiff.are_files_equal(out_file2, ans_file2))
