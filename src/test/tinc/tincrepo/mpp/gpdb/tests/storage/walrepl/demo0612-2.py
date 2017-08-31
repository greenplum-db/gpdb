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
    mpp.gpdb.tests.storage.walrepl.lib.cleanupFilespaces(dbname='haradh1')
    standby = Standby('standby', '5433')
    standby.stop()

    standby.stop()
    shutil.rmtree('standby', True)

    sys.stdout.write('''
***********************************************************
*** Demo for Filespace support
***********************************************************

''')
    pline('Initialize standby info in catalog and take online backup')
    standby.create()
    subprocess.call(['ls', 'standby'])
    subprocess.call(['psql', '-c', 'select * from gp_segment_configuration'])

    pline('Start standby', 'pg_ctl start -D standby')
    standby.create_recovery_conf()
    subprocess.call(['cat', 'standby/recovery.conf'])
    standby.start()

    with open('filespace_setup.sql') as f:
        sql = f.read()
    pline('Run some SQL on primary while standby is running', sql)
    subprocess.call(['psql', '-f', 'filespace_setup.sql'])

    pline('Promote standby', 'pg_ctl promote -D standby\n'
                             'select pg_activate_standby()')
    standby.promote()

    sql = 'SELECT * FROM fspc_table'
    pline('Connect to port 5433', 'psql -p 5433\n{0}'.format(sql))
    subprocess.call(['psql', '-p', '5433', '-c', sql])

    sql = 'SELECT * FROM gp_segment_configuration'
    pline('gp_segment_configuration', sql)
    subprocess.call(['psql', '-p', '5433', '-c', sql])

    sql = 'SELECT * FROM pg_filespace_entry;'
    pline('and pg_filespace_entry', sql)
    subprocess.call(['psql', '-p', '5433', '-c', sql])
