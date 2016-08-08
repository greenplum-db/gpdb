"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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
import inspect

import tinctest

from mpp.gpdb.lib.datagen import TINCTestDatabase
from mpp.common.lib.PSQL import PSQL

from tinctest.lib import local_path

from gppylib.commands.base import Command, CommandResult

class RegExpDatabase(TINCTestDatabase):

    def __init__(self, database_name = "regexpdb"):
        self.db_name = database_name
        super(RegExpDatabase, self).__init__(database_name)

    def perform_transformation(self,old_filename, new_filename):
        PATH = os.path.dirname(inspect.getfile(self.__class__))
        transforms= {"%MYD%":PATH}
        with open(old_filename, 'r') as input:
            with open(new_filename, 'w') as output:
                for line in input.readlines():
                    for key,value in transforms.iteritems():
                        if key in line:
                            line = line.replace(key, value)
                    output.write(line)
        return new_filename

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

        self.perform_transformation(local_path('setup.sql'), local_path('setup.sql.t'))
        PSQL.run_sql_file(local_path('setup.sql.t'), out_file = local_path('setup.out'), dbname = self.db_name)
	return True
			
