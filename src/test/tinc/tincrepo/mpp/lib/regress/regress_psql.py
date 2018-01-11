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

import fnmatch
import getpass
import inspect
import os
import re
import time

from datetime import datetime

import unittest2 as unittest

from mpp.lib.PSQL import PSQL, PSQLException

class PSQLCommandLineRegressionTests(unittest.TestCase):
    def test_run_sql_file(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        self.assertTrue(PSQL.run_sql_file(sql_file))
        # assert that we generate an out file by default
        out_file = sql_file.replace('.sql', '.out')
        self.assertTrue(os.path.exists(out_file))

    def test_run_sql_file_invalid_file(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_not_exists.sql')
        with self.assertRaises(PSQLException):
            PSQL.run_sql_file(sql_file)

    def test_run_sql_file_with_out_file(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.out')
        self.assertTrue(PSQL.run_sql_file(sql_file = sql_file, out_file = out_file))
        self.assertTrue(os.path.exists(out_file))
        os.remove(out_file)
        self.assertFalse(os.path.exists(out_file))

    def test_run_sql_file_with_out_file_disabled(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.out')
        self.assertTrue(PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, output_to_file = False))
        self.assertTrue(not(os.path.exists(out_file)))
       
    def test_run_sql_file_with_port(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        if not os.environ['PGPORT']:
            port = 5432
        else:
            port = int(os.environ['PGPORT'])
        self.assertTrue(PSQL.run_sql_file(sql_file = sql_file, port = port))

        #Invalid port
        self.assertFalse(PSQL.run_sql_file(sql_file = sql_file, port = 12345))

    def test_run_sql_file_wth_username(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.sql')
        username = getpass.getuser()
        self.assertTrue(PSQL.run_sql_file(sql_file = sql_file, username = username))

        #Invalid username
        self.assertFalse(PSQL.run_sql_file(sql_file = sql_file, username = 'invalidusername'))

    def test_run_sql_file_with_pgoptions(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test3.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test3.out')
        self.assertTrue(PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, PGOPTIONS = "-c client_min_messages=log"))
        self.assertTrue(os.path.exists(out_file))
        with open(out_file, 'r') as f:
            output = f.read()
            self.assertIsNotNone(re.search('log', output))

        os.remove(out_file)
        self.assertFalse(os.path.exists(out_file))
        #Invalid pgoptions
        self.assertFalse(PSQL.run_sql_file(sql_file = sql_file, PGOPTIONS="test"))

    def test_run_sql_file_default_flags_check(self):
        # Check if -a is  honored by default
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test3.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test3.out')
        self.assertTrue(PSQL.run_sql_file(sql_file = sql_file, out_file = out_file))
        self.assertTrue(os.path.exists(out_file))
        with open(out_file, 'r') as f:
            output = f.read()
            self.assertIsNotNone(re.search('show client_min_messages', output))

        os.remove(out_file)
        self.assertFalse(os.path.exists(out_file))

    def test_run_sql_file_with_flags(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test3.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test3.out')
        self.assertTrue(PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, flags='-q'))
        self.assertTrue(os.path.exists(out_file))
        with open(out_file, 'r') as f:
            output = f.read()
            self.assertIsNone(re.search('show client_min_messages', output))
        os.remove(out_file)
        self.assertFalse(os.path.exists(out_file))

    def test_run_sql_file_with_background(self):
        # Note command.run is not returning even with & that puts it in the background.
        # Have to investigate this later.
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_background.sql')
        self.assertTrue(PSQL.run_sql_file(sql_file = sql_file, background = True))


    def test_run_sql_command(self):
        sql_cmd = 'SELECT 1'
        self.assertTrue(PSQL.run_sql_command(sql_cmd = sql_cmd))

    def test_run_sql_command_with_out_file(self):
        sql_cmd = 'SELECT 1'
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test.out')
        self.assertFalse(os.path.exists(out_file))
        PSQL.run_sql_command(sql_cmd = sql_cmd, out_file = out_file)
        self.assertTrue(os.path.exists(out_file))
        os.remove(out_file)
        self.assertFalse(os.path.exists(out_file))

    def test_run_sql_command_with_port(self):
        sql_cmd = 'SELECT 1'
        if not os.environ['PGPORT']:
            port = 5432
        else:
            port = int(os.environ['PGPORT'])
        self.assertTrue(PSQL.run_sql_command(sql_cmd = sql_cmd, port = port))

        #Invalid port
        self.assertFalse(PSQL.run_sql_command(sql_cmd = sql_cmd, port = 12345))

    def test_run_sql_command_wth_username(self):
        sql_cmd = 'SELECT 1'
        username = getpass.getuser()
        self.assertTrue(PSQL.run_sql_command(sql_cmd = sql_cmd, username = username))

        #Invalid username
        self.assertFalse(PSQL.run_sql_command(sql_cmd = sql_cmd, username = 'invalidusername'))

    def test_run_sql_command_with_pgoptions(self):
        sql_cmd = 'show client_min_messages'
        output = PSQL.run_sql_command(sql_cmd = sql_cmd, PGOPTIONS = "-c client_min_messages=log")
        self.assertIsNotNone(output)
        self.assertIsNotNone(re.search('log', output))

        #Invalid pgoptions
        self.assertFalse(PSQL.run_sql_command(sql_cmd = sql_cmd, PGOPTIONS="test"))

    def test_run_sql_command_default_flags_check(self):
        # Check if -a is  honored by default
        sql_cmd = 'show client_min_messages'
        output = PSQL.run_sql_command(sql_cmd = sql_cmd)
        self.assertIsNotNone(output)
        self.assertIsNotNone(re.search('show client_min_messages', output))

    def test_run_sql_command_with_flags(self):
        sql_cmd = 'show client_min_messages'
        output = PSQL.run_sql_command(sql_cmd = sql_cmd, flags='-q')
        self.assertIsNotNone(output)
        self.assertIsNone(re.search('show client_min_messages', output))

    def test_run_sql_file_utility_mode(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_utility_mode.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_utility_mode.out')
        self.assertFalse(os.path.exists(out_file))
        try:
            self.assertTrue(PSQL.run_sql_file_utility_mode(sql_file = sql_file, out_file = out_file))
            self.assertTrue(os.path.exists(out_file))
            with open(out_file, 'r') as f:
                output = f.read()
                self.assertIsNotNone(re.search('utility', output))
        finally:
            os.remove(out_file)
            self.assertFalse(os.path.exists(out_file))

    def test_run_sql_command_utility_mode(self):
        sql_cmd = 'show gp_session_role'
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_utility_mode.out')
        self.assertFalse(os.path.exists(out_file))
        try:
            PSQL.run_sql_command_utility_mode(sql_cmd = sql_cmd, out_file = out_file)
            self.assertTrue(os.path.exists(out_file))
            with open(out_file, 'r') as f:
                output = f.read()
                self.assertIsNotNone(re.search('utility', output))
        finally:
            os.remove(out_file)
            self.assertFalse(os.path.exists(out_file))

    def test_run_sql_file_catalog_update(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_catalog_update.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_catalog_update.out')
        self.assertFalse(os.path.exists(out_file))
        try:
            self.assertTrue(PSQL.run_sql_file_catalog_update(sql_file = sql_file, out_file = out_file))
            self.assertTrue(os.path.exists(out_file))
            with open(out_file, 'r') as f:
                output = f.read()
                self.assertIsNotNone(re.search('utility', output))
                self.assertIsNotNone(re.search('DML', output))
        finally:
            os.remove(out_file)
            self.assertFalse(os.path.exists(out_file))

    def test_run_sql_file_catalog_update_with_additional_pgoptions(self):
        sql_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_catalog_update.sql')
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_catalog_update.out')
        self.assertFalse(os.path.exists(out_file))
        try:
            self.assertTrue(PSQL.run_sql_file_catalog_update(sql_file = sql_file,
                                                             out_file = out_file,
                                                             PGOPTIONS = "-c client_min_messages=log"))
            self.assertTrue(os.path.exists(out_file))
            with open(out_file, 'r') as f:
                output = f.read()
                self.assertIsNotNone(re.search('utility', output))
                self.assertIsNotNone(re.search('DML', output))
                self.assertIsNotNone(re.search('log', output))
        finally:
            os.remove(out_file)
            self.assertFalse(os.path.exists(out_file))

    def test_run_sql_command_catalog_update(self):
        sql_cmd = 'show gp_session_role;'
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_catalog_update.out')
        self.assertFalse(os.path.exists(out_file))
        try:
            PSQL.run_sql_command_catalog_update(sql_cmd = sql_cmd, out_file = out_file)
            self.assertTrue(os.path.exists(out_file))
            with open(out_file, 'r') as f:
                output = f.read()
                self.assertIsNotNone(re.search('utility', output))
        finally:
            os.remove(out_file)
            self.assertFalse(os.path.exists(out_file))

        sql_cmd = 'show allow_system_table_mods;'
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_catalog_update.out')
        self.assertFalse(os.path.exists(out_file))
        try:
            PSQL.run_sql_command_catalog_update(sql_cmd = sql_cmd, out_file = out_file)
            self.assertTrue(os.path.exists(out_file))
            with open(out_file, 'r') as f:
                output = f.read()
                self.assertIsNotNone(re.search('DML', output))
        finally:
            os.remove(out_file)
            self.assertFalse(os.path.exists(out_file))

    def test_run_sql_command_catalog_update_with_additional_pgoptions(self):
        sql_cmd = 'show gp_session_role;'
        output = PSQL.run_sql_command_catalog_update(sql_cmd = sql_cmd)
        self.assertIsNotNone(re.search('utility', output))

        sql_cmd = 'show allow_system_table_mods;'
        output = PSQL.run_sql_command_catalog_update(sql_cmd = sql_cmd)
        self.assertIsNotNone(re.search('DML', output))

        sql_cmd = 'show client_min_messages;'
        output = PSQL.run_sql_command_catalog_update(sql_cmd = sql_cmd,
                                                     PGOPTIONS = "-c client_min_messages=log")
        self.assertIsNotNone(re.search('log', output))

    def test_create_database(self):
        dbname = 'testdb'
        self.assertFalse(PSQL.database_exists(dbname))
        PSQL.create_database(dbname)
        time.sleep(4)
        self.assertTrue(PSQL.database_exists(dbname))
        PSQL.drop_database(dbname)
        time.sleep(4)
        self.assertFalse(PSQL.database_exists(dbname))

    def test_create_database_already_exists(self):
        dbname = 'testdupdb'
        self.assertFalse(PSQL.database_exists(dbname))
        PSQL.create_database(dbname)
        time.sleep(4)
        self.assertTrue(PSQL.database_exists(dbname))
        with self.assertRaises(PSQLException) as cm:
            PSQL.create_database(dbname)
        PSQL.drop_database(dbname)
        time.sleep(4)
        self.assertFalse(PSQL.database_exists(dbname))

    def test_drop_database(self):
        dbname = 'testdropdb'
        self.assertFalse(PSQL.database_exists(dbname))
        PSQL.create_database(dbname)
        time.sleep(4)
        self.assertTrue(PSQL.database_exists(dbname))
        #dropdb
        PSQL.drop_database(dbname)
        time.sleep(4)
        self.assertFalse(PSQL.database_exists(dbname))

    def test_drop_database_does_not_exist(self):
        with self.assertRaises(PSQLException) as cm:
            PSQL.drop_database('testnotexistdatabase')

    def test_reset_database_not_exists(self):
        dbname = 'testresetdb'
        PSQL.reset_database(dbname)
        time.sleep(4)
        self.assertTrue(PSQL.database_exists(dbname))
        PSQL.drop_database(dbname)
        time.sleep(4)
        self.assertFalse(PSQL.database_exists(dbname))

    def test_reset_database_exists(self):
        dbname = 'testresetdb2'
        PSQL.create_database(dbname)
        time.sleep(4)
        PSQL.run_sql_command(sql_cmd = "CREATE TABLE testresettable(i int)", dbname = dbname)
        output = PSQL.run_sql_command(sql_cmd = "SELECT * FROM testresettable", dbname = dbname)
        self.assertIsNotNone(re.search('0 rows', output))

        PSQL.reset_database(dbname)
        out_file = os.path.join(os.path.dirname(inspect.getfile(self.__class__)),'test_reset_database_exists.out')
        self.assertFalse(os.path.exists(out_file))
        PSQL.run_sql_command(sql_cmd = "SELECT * FROM testresettable", dbname = dbname, out_file = out_file)
        self.assertTrue(os.path.exists(out_file))
        # stdout should be none
        with open(out_file, 'r') as f:
            output = f.read()
            self.assertIsNotNone(re.search('relation.*does not exist', output))
        os.remove(out_file)
        self.assertFalse(os.path.exists(out_file))
        PSQL.drop_database(dbname)
        time.sleep(4)
        self.assertFalse(PSQL.database_exists(dbname))

    @classmethod
    def setUpClass(cls):
        dbs = ['testdb', 'testdupdb', 'testdropdb', 'testresetdb', 'testresetdb2']
        for db in dbs:
            if PSQL.database_exists(db):
                PSQL.drop_database(db)

        curr_dir = os.path.dirname(inspect.getfile(cls))
        for filename in os.listdir(curr_dir):
            if fnmatch.fnmatch(filename, "*.out"):
                os.remove(os.path.join(curr_dir, filename))
