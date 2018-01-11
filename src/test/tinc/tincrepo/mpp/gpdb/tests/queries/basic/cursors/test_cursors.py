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

import time

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL

import pygresql.pg
from struct import *
import os

class CursorTests(MPPTestCase):

    def test_mpp24119(self):
        """
        @description postgres process crashed when running cursor with hold
        @product_version gpdb: [4.2.8.1-4.2.99.99], [4.3.3.0-]
        """
        start_time = time.time()
        conn = pygresql.pg.connect()
        conn.query("drop table if exists mpp24119")
        conn.query("create table mpp24119(i int, j int)")
        conn.query("insert into mpp24119 select i , i from generate_series(1, 10) i")
        conn.query("begin; declare c cursor with hold for select * from mpp24119;")
        conn.query("commit")
        # The crash happens when exec_execute_message is triggered after a tx with cursor
        # with hold is committed. Since there was no way to trigger this, we send a protocol
        # message directly to the socket on which the connection is established
        sockno = conn.fileno()
        msg = pack('!sbi', 'c', 0, 0)
        l = len(msg) + 4

        res = os.write(sockno, pack('!c', 'E'))
        res = os.write(sockno, pack('!i', l))
        res = os.write(sockno, msg)
        format_start_time = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(start_time))
        output = PSQL.run_sql_command("SELECT logmessage FROM gp_toolkit.gp_log_system WHERE " + \
                                      "logtime >= '%s' and logdatabase is null and logseverity = 'PANIC'" %(format_start_time), flags= '-q -t')
        self.assertFalse(output.strip())
        conn.close()
        
        

    
