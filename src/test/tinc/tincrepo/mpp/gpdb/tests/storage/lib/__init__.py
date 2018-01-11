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
import sys

import tinctest
from tinctest.lib import local_path

from mpp.lib.PSQL import PSQL
from tinctest.lib import run_shell_command

from gppylib.commands.base import Command
from gppylib.db import dbconn

class Disk():
    '''Methods relating to disk usage, dca disk policy etc'''

    def __init__(self):
        pass

    def get_disk_usage(self, hostname, partition='/data'):
        '''Returns the disk usage of individual hosts'''
        cmd_str = "ssh %s df %s | grep -v Filesystem |awk \'{print $4}\'" % (hostname, partition)
        results={'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command(cmd_str, results=results)
        return results1['rc'], results1['stdout']

class Database():
    
    def setupDatabase(self, dbname = None):
        #Create a database , if none same name as user
        if dbname is None:
            dbname = os.environ["USER"]
        cmd = Command(name='Drop if exists and Create the database', cmdStr='dropdb %s;createdb %s' % (dbname, dbname))
        cmd.run()
        if cmd.get_results().rc != 0:
            raise Exception("Create database %s failed" % dbname)
        tinctest.logger.info("Database %s is created" % dbname)

    def dropDatabase(self, dbname = None):
        if dbname is None:
            raise Exception(' No database name provided')
        cmd = Command(name='Drop database', cmdStr='dropdb %s' % (dbname))
        cmd.run()
        if cmd.get_results().rc != 0:
            raise Exception("Drop database %s failed" % dbname)
        tinctest.logger.info("Database %s is droped" % dbname)
                
    def is_debug(self):
        version_str = PSQL.run_sql_command('select version();', flags = '-q -t', dbname= 'postgres')
        print version_str
        if 'assert' in version_str :
            print ' coming in if'
            return True

class Config():

    def __init__(self):
        pass

    def getMasterMirrorHostname(self):
        sql_cmd = "select hostname from gp_segment_configuration where content = -1 and role = 'm'"
        out = PSQL.run_sql_command(sql_cmd, dbname = 'template1')
        return out.strip()

        return False
       
