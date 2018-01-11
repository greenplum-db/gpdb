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
import tinctest

from mpp.gpdb.lib.datagen import TINCTestDatabase
from mpp.common.lib.PSQL import PSQL

from tinctest.lib import local_path
from tinctest.lib import run_shell_command

from gppylib.commands.base import Command, CommandResult

class Mpp16291Database(TINCTestDatabase):

    def __init__(self, database_name = "fm01"):
        self.db_name = database_name
        super(Mpp16291Database, self).__init__(database_name)

    def setUp(self):
        # Assume setup is done if db exists
        output = PSQL.run_sql_command("select 'command_found_' || datname from pg_database where datname like '" + self.db_name + "'")
        if 'command_found_' + self.db_name in output:
            return
        cmd = Command('dropdb', "dropdb " + self.db_name)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        cmd = Command('createdb', "createdb " + self.db_name)
        cmd.run(validateAfter=True)
        result = cmd.get_results()

        MYD = os.path.abspath( os.path.dirname( __file__ ) )

        # Copy and unzip data files
        if os.path.exists( MYD + '/' + '/compressed_data/' + 'mpp16291.tar.gz' ):
            run_shell_command( 'cp ' + MYD + '/compressed_data/' + 'mpp16291.tar.gz ' + MYD + '/' + 'mpp16291.tar.gz ', 'Copy compressed data' )
            run_shell_command( 'gunzip ' + MYD + '/' +  'mpp16291.tar.gz', 'Unzip compressed data' )
            run_shell_command( 'tar -xvf ' + MYD + '/' +  'mpp16291.tar -C ' + MYD, 'Untar archive' )

        filelist = [ 'dim_workflows.dat', 'dim_temporarl_expressions.dat', 'dim_subject_areas.dat', 'dim_dates.dat', 'xref_dim_dates_te.dat', 'fact_workflow_events.dat', 'fact_task_events.dat', 'dim_tasks.dat']

        for i in range(len(filelist)):
            runfile = MYD + '/adp/' + filelist[i]
            PSQL.run_sql_file( runfile, out_file = runfile.replace('.dat', '') + '.out', dbname = self.db_name)

        return True

