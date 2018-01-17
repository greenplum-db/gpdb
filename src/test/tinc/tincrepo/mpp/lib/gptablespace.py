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

############################################################################
import tinctest
from gppylib.commands.base import Command
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from tinctest.lib import local_path, run_shell_command
from tinctest.main import TINCException

class Gptablespace(object):

    def __init__(self, config=None):
        if config is not None:
            self.config = config
        else:
            self.config = GPDBConfig()

    def get_standby_tablespace_directory(self, tablespace):
        '''
        Get the standby tablespace directory location
        @param tablespace: tablespace name
        @return dir: standby tablespace directory
        '''
        sql_cmd = "SELECT dbid from gp_segment_configuration where content = -1 and role = 'm'";
        dbid = int(PSQL.run_sql_command(sql_cmd, flags = '-t -q -c', dbname='postgres'))
        sql_cmd = ("SELECT gp_tablespace_path(spclocation, %d) FROM pg_tablespace WHERE spcname <> %s") % (dbid, tablespace)
        dir = PSQL.run_sql_command(sql_cmd, flags = '-t -q', dbname='postgres')
        return dir

    def exists(self, tablespace):
        '''
        Check whether tablespace exist in catalog
        @param tablespace:tablespace name
        @return True or False
        '''
        fs_out = PSQL.run_sql_command("select count(*) from pg_tablespace where spcname='%s'" % tablespace, flags = '-t -q', dbname='postgres')
        if int(fs_out.strip()) > 0:
            return True
        return False

    def create_tablespace(self, tablespace, loc):
        '''
        @param tablespace: Tablespace Name
        @param loc: Tablespace location
        '''
        if self.exists(tablespace):
            tinctest.logger.info('tablespace %s exists' % tablespace)
            return

        for record in self.config.record:
            cmd = "gpssh -h %s -e 'rm -rf %s; mkdir -p %s'"  % (record.hostname, loc, loc)
            run_shell_command(cmd)
        tinctest.logger.info('create tablespace %s' % tablespace)
        sql_cmd = 'create tablespace %s location \'%s\'' % (tablespace, loc)
        PSQL.run_sql_command(sql_cmd, flags = '-t -q -c', dbname='postgres')

