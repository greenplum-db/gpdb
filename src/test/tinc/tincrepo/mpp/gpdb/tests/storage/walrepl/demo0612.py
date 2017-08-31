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
import signal

import mpp.gpdb.tests.storage.walrepl.lib

def pline(msg, cmd=None):
    sys.stdout.write('----' + msg + '\n')
    if cmd:
        sys.stdout.write(cmd + '\n')
    sys.stdin.readline()

if __name__ == '__main__':
    standby = mpp.gpdb.tests.storage.walrepl.lib.Standby('standby', '5433')
    standby.stop()

    standby.stop()
    shutil.rmtree('standby', True)
    subprocess.call(['psql', '-c', 'DROP TABLE IF EXISTS foo'])

    sys.stdout.write('''
***********************************************************
*** Demo for Master Standby using Synchronous Streaming Replication ***
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

    pline('Perform Checkpoint', 'psql -c CHECKPOINT')
    subprocess.call(['psql', '-c', 'CHECKPOINT'])

    pline('Suspend WAL Receiver...')
    proc = subprocess.Popen(['ps', '-ef'],stdout=subprocess.PIPE)
    stdout = proc.communicate()[0]
    for line in stdout.split('\n'):
        search = "wal receiver process"
        if (line.find(search) > 0):
            #print (line)
            split_line = line.split(' ');
            #print split_line[1]
            break

    print ("WAL Receiver PID = " + split_line[1] + " will suspend\n")
    walRcvPID = int(split_line[1])

    os.kill(walRcvPID, signal.SIGUSR2)

    pline('Create table foo...','psql -c create table foo (a int)')
    subprocess.Popen(['psql','-c','create table foo (a int)'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)

    pline('Check if the backened is waiting...')
    proc = subprocess.Popen(['ps', '-ef'],stdout=subprocess.PIPE)
    stdout = proc.communicate()[0]
    for line in stdout.split('\n'):
        search = "fsync"
        if (line.find(search) > 0):
            print (line + "\n")

    pline ('Resume the WAL Receiver...')
    os.kill(walRcvPID, signal.SIGUSR2)

    pline('Again check if the backened is waiting...')
    flag = True
    proc = subprocess.Popen(['ps', '-ef'],stdout=subprocess.PIPE)
    stdout = proc.communicate()[0]
    for line in stdout.split('\n'):
        search = "fsync"
        if (line.find(search) > 0):
            print (line)
            flag = False

    assert flag
    print ("No Blocked backend found!")

    pline('Verify if table exists ? ...','select * from foo')
    subprocess.call(['psql', '-c', 'select * from foo'])

    print("\nPass")
