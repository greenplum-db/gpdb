#!/usr/bin/env python

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

import shutil
import subprocess
import sys

from mpp.gpdb.tests.storage.walrepl.lib.standby import Standby

def pline(msg, cmd=None):
    sys.stdout.write('----' + msg + '\n')
    if cmd:
        sys.stdout.write(cmd + '\n')
    sys.stdin.readline()

if __name__ == '__main__':
    standby = Standby('standby', '5433')
    standby.stop()

    standby.stop()
    shutil.rmtree('standby', True)
    subprocess.call(['psql', '-c', 'DROP TABLE IF EXISTS foo1'])

    sys.stdout.write('''
***********************************************************
*** Demo for Master Standby using Streaming Replication ***
***********************************************************

''')
    pline('Take online backup', 'pg_basebackup -x -D standby')
    standby.create()
    subprocess.call(['ls', 'standby'])

    pline('Put recovery.conf')
    standby.create_recovery_conf()
    subprocess.call(['cat', 'standby/recovery.conf'])

    pline('Start standby', 'pg_ctl start -D standby')
    standby.start()

    sql = ('CREATE TABLE foo1 AS\n'
           'SELECT i a, i b FROM generate_series(1, 100)i;')
    pline('Run some SQL...', sql)
    subprocess.call(['psql', '-c', sql])

    pline('Promote standby', 'pg_ctl promote -D standby')
    standby.promote()

    pline('Connect to port 5433', 'psql -p 5433')
    subprocess.call(['psql', '-p', '5433', '-c', 'SELECT * FROM foo1'])
