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
import os

def pline(msg, cmd=None):
    sys.stdout.write('\n' + '_')
    sys.stdin.readline()
    sys.stdout.write('===>> ' + msg + '\n')
    if cmd:
        sys.stdout.write(cmd + '\n')
    sys.stdin.readline()

if __name__ == '__main__':
    subprocess.call(['psql', '-c', 'DROP TABLE IF EXISTS DEMO1,DEMO2'])
    subprocess.call(['rm', '-rf', '/gpdata/agrawa2/gpdata/gpdata_WAL-REPLICATION/master_standby'])

    sys.stdout.write('''


 =================================================
|                                                 |
|    Demo for WAL based Streaming Replication     |
|                                                 |
 =================================================

''')
    pline('Check Standby Master configured...', 'gpstate -f')
    subprocess.call(['gpstate', '-f'])

#    pline('Added flexibility, new options to gpinitstandby')
#    subprocess.call(['gpinitstandby', '--help'])

    cmd = ('gpinitstandby -s support002.esc.dh.greenplum.com -P 5433 -F pg_system:/gpdata/agrawa2/gpdata/gpdata_WAL-REPLICATION/master_standby')
    pline('Create Standby Master...', cmd);
    subprocess.call(['gpinitstandby', '-s', 'support002.esc.dh.greenplum.com', '-P', '5433', '-F', 'pg_system:/gpdata/agrawa2/gpdata/gpdata_WAL-REPLICATION/master_standby'])

    pline('Check Standby Master configured...', 'gpstate -f')
    subprocess.call(['gpstate', '-f'])

    cmd = ('SELECT * FROM pg_stat_replication;')
    pline('Different way to fetch the same...', cmd)
    subprocess.call(['psql', '-c', cmd, '-x'])

    sql = ('CREATE TABLE demo1 AS SELECT i a FROM generate_series(1, 1000)i;')
    pline('Create a Table with Data...', sql)
    subprocess.call(['psql', '-c', sql])

    sql = ('CREATE TABLE demo2 AS SELECT i b FROM generate_series(1, 500)i;')
    pline('One more Table with Data...', sql)
    subprocess.call(['psql', '-c', sql])

    pline('Lets again check Standby Master state...', 'gpstate -f')
    subprocess.call(['gpstate', '-f'])

    cmd = ('kill -9 `ps -ef | grep 2200 | grep -v grep | awk \'{print $2}\'`')
    pline('Lets create *** DISASTER *** hammer down Master', cmd)
    subprocess.call(['/gpdata/agrawa2/code/TINC/private/mpp.gpdb.tests.storage.walrepl/kill_master.sh'])

    os.environ["PGPORT"] = "5433"
    os.environ["MASTER_DATA_DIRECTORY"] = "/gpdata/agrawa2/gpdata/gpdata_WAL-REPLICATION/master_standby"
    pline('Just Activate Standby Master...', 'gpactivatestandby')
    subprocess.call(['gpactivatestandby', '-f'])

    pline('Access data from NEW Master...IMP point connect to new master PORT', 'SELECT count(*) FROM demo1')
    subprocess.call(['psql', '-p', '5433', '-c', 'SELECT count(*) FROM demo1'])
    pline('SELECT count(*) FROM demo2')
    subprocess.call(['psql', '-p', '5433', '-c', 'SELECT count(*) FROM demo2'])

    sys.stdout.write('''


 ==========================================================
|                                                          |
|    DEMO COMPLETE for WAL based Streaming Replication     |
|                                                          |
 ==========================================================

''')
       
