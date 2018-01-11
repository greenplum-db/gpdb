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

from os import path, listdir

from gppylib.commands.base import Command

from mpp.lib.PSQL import PSQL

class TINCDatagenException(Exception):
    pass

class TINCTestDatabase(object):
    """
    Base class for the test database generator. Metadata is provided as doc strings
    similar to TINCTestCase
    """
    def __init__(self, database_name):
        self.db_name = database_name

    @classmethod
    def _infer_metadata(cls):
        """
        Populate a _metadata dictionary by reading the class' docstring
        """
        cls._metadata = {}
        docstring = cls.__doc__
        if not docstring:
            return
        lines = docstring.splitlines()
        for line in lines:
            line = line.strip()
            if line.find('@') != 0:
                continue
            line = line[1:]
            (key, value) = line.split(' ', 1)
            cls._metadata[key] = value
        return cls._metadata

    def setUp(self):
        """
        The method that sub-classes must implement to setup a particular database.
        
        @rtype: boolean
        @return: True if db is already present; False if it is not present a new db was created
                 Raises TINCDatagenException if db creation failed
        """
        # Assume setup is done if db exists
        output = PSQL.run_sql_command("select 'command_found_' || datname from pg_database where datname like '" + self.db_name + "'")
        if 'command_found_' + self.db_name in output:
            return True
        cmd = Command('createdb', "createdb " + self.db_name)
        cmd.run(validateAfter=True)
        result = cmd.get_results()
        if result.rc != 0:
            raise TINCDatagenException('createdb failed')
        return False

    def tearDown(self):
        """
        The method that sub-classes must implement as a cleanup for the database
        Typically called when setup fails midway
        
        @return: Nothing if drop db succeeded.  Raises TINCDatagenException if dropdb failed
        """
        output = PSQL.run_sql_command("select 'command_found_' || datname from pg_database where datname like '" + self.db_name + "'")
        if 'command_found_' + self.db_name in output:
            cmd = Command('dropdb', "dropdb " + self.db_name)
            cmd.run(validateAfter=True)
            result = cmd.get_results()
            if result.rc != 0:
                raise TINCDatagenException('dropdb failed')

