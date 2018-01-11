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
import os
import datetime
from time import sleep
from random import randint
from tinctest.lib import local_path
from mpp.lib.PSQL import PSQL
from gppylib.commands.base import Command

GPDBNAME = str(os.environ["PGDATABASE"])

''' Utility's for Global Persistent table Rebuild test '''

class PTRebuildUtil(Command):
    def __init__(self, cmd = None):
        Command.__init__(self, 'Running fault command', cmd)

    def _run_sys_cmd(self, cmd_str, validate = False):
        '''helper function to run a sys cmd'''
        tinctest.logger.info("execute:" +cmd_str)
        cmd = PTRebuildUtil(cmd_str)
        cmd.run(validateAfter = validate)
        return cmd.get_results()

    def check_dbconnections(self):
        ''' Check if database has any active connections '''
        sql_cmd = 'select count(*) FROM pg_stat_activity;'
        conCount = int(PSQL.run_sql_command(sql_cmd).split('\n')[3]) - 1
        if conCount > 0:
            print "There are %s Active connection on Database" %conCount
            return True
        else :
            return False

    def get_hostname_port_of_segment(self):
        ''' Get hostname and Port no of Primary segments '''
        # Get primary segments 
        cmd_str = "select hostname, port from gp_segment_configuration where role = 'p' and content != '-1';" 
        seglist = PSQL.run_sql_command(cmd_str).split('\n')

        #select any segment randomly
        segNo = 2 + 1 #randint( 1, 2) : Commented so that it will rebuild same segment in 2nd re-try
        (hostname, port) = seglist[segNo].split('|')
        return (hostname, port)

    def persistent_Rebuild(self, hostname = None, port = None, type = 'Master'):
        ''' Rebuild Persistent Object by connecting in Utility mode '''
        sql_file = local_path('persistent_Rebuild_%s.sql'%type)
        now = datetime.datetime.now()
        timestamp = '%s%s%s%s%s%s%s'%(now.year,now.month,now.day,now.hour,now.minute,now.second,now.microsecond)
        out_file = sql_file.replace('.sql', timestamp + '.out')
        PSQL.run_sql_file(sql_file = sql_file,  PGOPTIONS = '-c gp_session_role=utility', host = hostname, port = port, out_file = out_file) 

