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
import socket
import tinctest
from mpp.lib.PSQL import PSQL
from mpp.lib.config import GPDBConfig
from gppylib.commands.base import Command, REMOTE

class GpfileTestCase():


    def get_host_and_db_path(self, dbname, contentid=0):
        ''' Get the host and database path for the content'''
        config = GPDBConfig()
        db_oid = PSQL.run_sql_command("select oid from pg_database where datname='%s'" % dbname, flags='-q -t', dbname='postgres')
        dbid = PSQL.run_sql_command("select dbid from gp_segment_configuration where content=%s and role='p'" % contentid, flags='-q -t', dbname='postgres')
        (host, address) = config.get_host_and_datadir_of_segment(dbid= dbid.strip())

        db_path = os.path.join(address, 'base', db_oid.strip())
        return (host.strip(), db_path)

    def get_relfile_list(self,dbname, tablename, db_path, host):
        ''' Get the list of relfilenodes for the relation '''
        relfilenode = PSQL.run_sql_command("select relfilenode from pg_class where relname='%s'" % tablename, flags='-q -t', dbname=dbname)
        if host.strip() in ('localhost', socket.gethostname()):
            cmd = Command("Get list of files", "cd %s;ls %s.*" % (db_path, relfilenode.strip()))
        else:
            cmd = Command("Get list of files", "cd %s;ls %s.*" % (db_path,relfilenode.strip()), ctxt=REMOTE, remoteHost=host.strip())
        cmd.run(validateAfter=False)

        result = cmd.get_results()
        file_list = result.stdout.split()
        return file_list

    def check_in_filedump(self, db_path, host, relid, search_str, flag=None):
        option='-z '
        if flag != None:
            option = option + flag
        if host.strip() in ('localhost', socket.gethostname()):
            cmd = Command('Run gp_filedump ... ', cmdStr ='gp_filedump %s  %s' % (option,os.path.join(db_path, relid)))
        else:
            cmd = Command('Run gp_filedump ... ', cmdStr ='gp_filedump %s  %s' % (option,os.path.join(db_path, relid)), ctxt=REMOTE, remoteHost=host.strip())
        tinctest.logger.info(cmd.cmdStr)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        for line in result.stdout.splitlines() :
            if line.find(search_str) > -1:
                return True
        return False
