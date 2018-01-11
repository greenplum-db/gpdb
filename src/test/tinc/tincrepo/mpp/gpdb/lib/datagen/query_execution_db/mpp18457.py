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

class Mpp18457Database(TINCTestDatabase):

    def __init__(self, database_name = "mpp18457_db"):
        self.db_name = database_name
        super(Mpp18457Database, self).__init__(database_name)

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

        #First create the schemas, before loading the data
        runfile = MYD + '/' + 'mpp18457_repro_setup.sql'
        PSQL.run_sql_file( runfile, out_file = runfile.replace('.sql', '') + '.out', dbname = self.db_name)

        # Copy and unzip data files
        if os.path.exists( MYD + '/' + '/compressed_data/' + 'mpp18457.tar.gz' ):
            run_shell_command( 'cp ' + MYD + '/compressed_data/' + 'mpp18457.tar.gz ' + MYD + '/' + 'mpp18457.tar.gz ', 'Copy compressed data' )
            run_shell_command( 'gunzip ' + MYD + '/' +  'mpp18457.tar.gz', 'Unzip compressed data' )
            run_shell_command( 'tar -xvf ' + MYD + '/' +  'mpp18457.tar -C ' + MYD, 'Untar archive' )

        mypath = MYD + '/mpp18457/'

        filelist = [ f for f in os.listdir(mypath) if os.path.isfile(mypath + f) and f.endswith('dmp') ]

        # Set-up schema, data
        for i in range(len(filelist)):
            runfile = str(MYD) + str('/mpp18457/') + str(filelist[i])
            PSQL.run_sql_file( runfile, out_file = runfile.replace('.dmp', '') + '.out', dbname = self.db_name)

        return True
			
