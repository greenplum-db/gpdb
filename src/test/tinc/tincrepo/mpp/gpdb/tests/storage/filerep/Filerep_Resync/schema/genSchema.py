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
import unittest2 as unittest

from tinctest.lib import local_path

from mpp.lib.PSQL import PSQL

from gppylib.commands.base import Command
from mpp.gpdb.tests.storage.filerep.Filerep_Resync.test_filerep_resync import FilerepResyncException
'''
Schema definitions required for Filerep Resync
'''
class Schema(Command):
    def __init__(self, cmd = None):
            Command.__init__(self, 'running schema related command', cmd)
        
    def create_ao_table(self):
        '''Creating a table in Append-Only mode'''
        tinctest.logger.info('\nCreating the appendonly table')
        out = PSQL.run_sql_file(local_path('create.sql'))
        return out
        
        
    def check_duplicate_entries_in_PT(self):
        '''Checking duplicate entries in the Persistent table'''
        cmd_str = "select count(*) from gp_dist_random('gp_persistent_relation_node') where segment_file_num = 1 and relfilenode_oid in (select relfilenode from gp_dist_random('pg_class') where relname = 'ta') group by relfilenode_oid, gp_segment_id;"
        out = PSQL.run_sql_command(cmd_str).split('\n')[3:-3]
        if len(out) == 0:
            exception_msg = "ERROR!! No values returned for the duplicate entries query. " \
                            "Check if 'query01.sql' got executed or not"
            tinctest.logger.error(exception_msg)
            raise FilerepResyncException(exception_msg)
        count = out[0].strip()
        if int(count) > 1 :
            tinctest.logger.error('Duplicate entries found in Persistent table')
            return True
        else:
            tinctest.logger.info('Duplicate entries not found in Persistent table')
            return False
        

