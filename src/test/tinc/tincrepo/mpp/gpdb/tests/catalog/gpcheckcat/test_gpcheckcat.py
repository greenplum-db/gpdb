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

import fnmatch
import os

from mpp.lib.config import GPDBConfig
from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase
from tinctest.lib import local_path, run_shell_command
from tinctest.lib import Gpdiff

class GpCheckcatTests(MPPTestCase):
    """
    @description gpcheckcat test suite
    @tags gpcheckcat
    @product_version gpdb: [4.3.5.1 -]
    """

    def __init__(self, methodName):
        self.master_port = os.environ.get('PGPORT', '5432')
        super(GpCheckcatTests, self).__init__(methodName)

    def setUp(self):
        super(GpCheckcatTests, self).setUp()
        self.config = GPDBConfig()
        self.gpcheckcat_test_dir = local_path('gpcheckcat_dir')
        if not os.path.exists(self.gpcheckcat_test_dir):
            os.makedirs(self.gpcheckcat_test_dir, 0777)
        else:
            os.chmod(self.gpcheckcat_test_dir, 0777)

    def tearDown(self):
        super(GpCheckcatTests, self).tearDown()

    def test_error(self):
        """
        Test for errors during the generation of verify file
        """
        dbname = 'test_error'
        PSQL.run_sql_command('DROP DATABASE IF EXISTS %s' % dbname)
        stdout = PSQL.run_sql_command('CREATE DATABASE %s' % dbname)
        if not stdout.endswith('CREATE DATABASE\n'):
            self.fail('failed to create database: %s' % stdout)

        # Remove old verify files before runing the test.
        if not run_shell_command('rm -f %s/gpcheckcat.verify.%s.*' %
                                 (self.gpcheckcat_test_dir, dbname)):
            self.fail('failed to remove old verify files')

        sql_file = local_path('sql/create_tables.sql')
        if not PSQL.run_sql_file(sql_file, dbname=dbname,
                                 output_to_file=False):
            self.fail('failed to create tables')

        host, port = self.config.get_hostandport_of_segment()
        sql_file = local_path('sql/catalog_corruption.sql')
        if not PSQL.run_sql_file_utility_mode(
                sql_file, dbname=dbname, host=host, port=port,
                output_to_file=False):
            self.fail('failed to introduce catalog corruption')

        os.chmod(self.gpcheckcat_test_dir, 0555)
        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(
            "cd %s && $GPHOME/bin/lib/gpcheckcat -p %s %s" %
            (self.gpcheckcat_test_dir, self.master_port, dbname),
            results=res)
        self.assertEqual(3, res['rc'])
        for f in os.listdir(self.gpcheckcat_test_dir):
            if fnmatch.fnmatch(f, 'gpcheckcat.verify.%s.*' % dbname):
                self.fail('found verify file when not expecting it')

    def test_no_corruption(self):
        """
        Test that gpcheckcat does not report any errors and it does
        not generate the verify file if the gpcheckcat test succeeds.
        We choose missing_extraneous test for this purpose.

        """
        dbname = 'test_no_corruption'
        PSQL.run_sql_command('DROP DATABASE IF EXISTS %s' % dbname)
        stdout = PSQL.run_sql_command('CREATE DATABASE %s' % dbname)
        if not stdout.endswith('CREATE DATABASE\n'):
            self.fail('failed to create database: %s' % stdout)

        sql_file = local_path('sql/create_tables.sql')
        if not PSQL.run_sql_file(sql_file, dbname=dbname,
                                 output_to_file=False):
            self.fail('failed to create tables')

        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(
            "cd %s && $GPHOME/bin/lib/gpcheckcat -p %s -R missing_extraneous %s" %
            (self.gpcheckcat_test_dir, self.master_port, dbname),
            results=res)
        self.assertEqual(0, res['rc'])
        for f in os.listdir(self.gpcheckcat_test_dir):
            if fnmatch.fnmatch(f, 'gpcheckcat.verify.%s.*' % dbname):
                self.fail('found verify file when not expecting it')

    def test_singledb_corruption(self):
        """
        Test that gpcheckcat reports errors and it generates
        the verify file
        """
        dbname = 'test_singledb_corruption'
        PSQL.run_sql_command('DROP DATABASE IF EXISTS %s' % dbname)
        stdout = PSQL.run_sql_command('CREATE DATABASE %s' % dbname)
        if not stdout.endswith('CREATE DATABASE\n'):
            self.fail('failed to create database: %s' % stdout)

        sql_file = local_path('sql/create_tables.sql')
        if not PSQL.run_sql_file(sql_file, dbname=dbname,
                                 output_to_file=False):
            self.fail('failed to create tables')

        host, port = self.config.get_hostandport_of_segment()
        sql_file = local_path('sql/catalog_corruption.sql')
        if not PSQL.run_sql_file_utility_mode(
                sql_file, dbname=dbname, host=host, port=port,
                output_to_file=False):
            self.fail('failed to introduce catalog corruption')

        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command(
            "cd %s && $GPHOME/bin/lib/gpcheckcat -p %s %s" %
            (self.gpcheckcat_test_dir, self.master_port, dbname),
            results=res)

        self.assertEqual(3, res['rc'])
        found = False
        for f in os.listdir(self.gpcheckcat_test_dir):
            if fnmatch.fnmatch(f, 'gpcheckcat.verify.%s.*' % dbname):
                found = True
        self.assertTrue(found)

        verify_file_pat = 'gpcheckcat.verify.%s.*' % dbname
        mtime = lambda f: os.stat(
            os.path.join(self.gpcheckcat_test_dir, f)).st_mtime
        fname = list(sorted(
                        fnmatch.filter(
                            os.listdir(self.gpcheckcat_test_dir),
                            verify_file_pat),
                        key=mtime))[-1]
        if not PSQL.run_sql_file(os.path.join(self.gpcheckcat_test_dir, fname), output_to_file=False):
            self.fail('failed to run verify file for database %s' % dbname)

    def test_multidb_corruption(self):
        """
        Test that gpcheckcat reports errors and it generates
        the verify file
        """
        dbname1 = 'test_multidb_corruption1'
        dbname2 = 'test_multidb_corruption2'
        PSQL.run_sql_command('DROP DATABASE IF EXISTS %s' % dbname1)
        stdout = PSQL.run_sql_command('CREATE DATABASE %s' % dbname1)
        if not stdout.endswith('CREATE DATABASE\n'):
            self.fail('failed to create database: %s' % stdout)
        PSQL.run_sql_command('DROP DATABASE IF EXISTS %s' % dbname2)
        stdout = PSQL.run_sql_command('CREATE DATABASE %s' % dbname2)
        if not stdout.endswith('CREATE DATABASE\n'):
            self.fail('failed to create database: %s' % stdout)

        sql_file = local_path('sql/create_tables.sql')
        if not PSQL.run_sql_file(sql_file, dbname=dbname1,
                                 output_to_file=False):
            self.fail('failed to create tables in database %s' % dbname1)
        if not PSQL.run_sql_file(sql_file, dbname=dbname2,
                                output_to_file=False):
            self.fail('failed to create tables in database %s' % dbname2)

        host, port = self.config.get_hostandport_of_segment()
        sql_file = local_path('sql/catalog_corruption.sql')
        if not PSQL.run_sql_file_utility_mode(
                sql_file, dbname=dbname1, host=host, port=port,
                output_to_file=False):
            self.fail('failed to introduce corruption in database %s' % dbname1)
        if not PSQL.run_sql_file_utility_mode(
                sql_file, dbname=dbname2, host=host, port=port,
                output_to_file=False):
            self.fail('failed to introduce corruption in database %s' % dbname2)

        res = {'rc': 0, 'stdout' : '', 'stderr': ''}
        run_shell_command("cd %s && $GPHOME/bin/lib/gpcheckcat -p %s %s" %
                          (self.gpcheckcat_test_dir, self.master_port, dbname1),
                          results=res)
        self.assertTrue(res['rc'] > 0)
        run_shell_command("cd %s && $GPHOME/bin/lib/gpcheckcat -p %s %s" %
                          (self.gpcheckcat_test_dir, self.master_port, dbname2),
                          results=res)
        self.assertTrue(res['rc'] > 0)

        found = False
        for f in os.listdir(self.gpcheckcat_test_dir):
            if fnmatch.fnmatch(f, 'gpcheckcat.verify.%s.*' % dbname1):
                found = True
        self.assertTrue(found)

        found = False
        for f in os.listdir(self.gpcheckcat_test_dir):
            if fnmatch.fnmatch(f, 'gpcheckcat.verify.%s.*' % dbname2):
                found = True
        self.assertTrue(found)

        mtime = lambda f: os.stat(
            os.path.join(self.gpcheckcat_test_dir, f)).st_mtime
        # Choose the most recent verify file with dbname1 in its name.
        verify_file_pat = 'gpcheckcat.verify.%s.*' % dbname1
        fname = list(
            sorted(
                fnmatch.filter(
                    os.listdir(self.gpcheckcat_test_dir),
                    verify_file_pat),
                key=mtime))[-1]

        # Ensure that the verify file can be run.  It is difficult to
        # assert the SQL output against an expected answer file
        # because the output mostly has OIDs.  We are therefore
        # skipping this level of assertion for now.
        if not PSQL.run_sql_file(os.path.join(self.gpcheckcat_test_dir, fname), output_to_file=False):
            self.fail('failed to run verify file for database %s' % dbname1)

        # Similarly for dbname2.
        verify_file_pat = 'gpcheckcat.verify.%s.*' % dbname2
        mtime = lambda f: os.stat(
            os.path.join(self.gpcheckcat_test_dir, f)).st_mtime
        fname = list(sorted(
                        fnmatch.filter(
                            os.listdir(self.gpcheckcat_test_dir),
                            verify_file_pat),
                        key=mtime))[-1]
        if not PSQL.run_sql_file(os.path.join(self.gpcheckcat_test_dir, fname), output_to_file=False):
            self.fail('failed to run verify file for database %s' % dbname2)
