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

from fnmatch import fnmatch
import os
import re
import tempfile
import time
import sys

from gppylib.commands.base import Command

import tinctest
from tinctest.lib import Gpdiff
from tinctest import logger

from mpp.lib.PSQL import PSQL
from mpp.models import MPPTestCase

class SimpleCommand(object):
    """A simple wrapper for Command."""

    def __init__(self, cmd, args=''):
        self.cmd = cmd
        self.args = args
        cmd_array = [cmd] + args
        self.proc = Command('run ' + cmd, ' '.join(cmd_array))

    def run_or_fail(self):
        """Run this command, and fails if the return code is non-zero.
        Returns stdout and stderr of the command.
        """

        self.proc.run()
        results = self.proc.get_results()
        assert results.rc == 0, (""
            "{cmd} failed: args='{args}'\n{stderr}".format(
                cmd=self.cmd,
                args=' '.join(self.args),
                stderr=results.stderr))
        self.results = results
        return results.stdout, results.stderr

class RetryPSQL(object):
    def __init__(self, retry=5, interval=1, **kwargs):
        self.retry = retry
        self.interval = interval
        self.psql_args = kwargs

    def run(self):
        """Run psql and see if stderr contains "ERROR" string.
        If it was an error, re-try with given interval (1 sec by default)
        and repeat as many as retry parameter (5 by default).  If retry
        count exceeds, return False.  Otherwise, True.
        """

        retry = self.retry
        for i in range(self.retry):
            cmd = PSQL(**self.psql_args)
            logger.debug("Running command: %s" % cmd)
            cmd.run(validateAfter=False)
            result = cmd.get_results()
            if "ERROR" not in result.stderr:
                return True
            time.sleep(self.interval)
        return False

class BackupTestCase(MPPTestCase):
    """This test case is for general dump-restore validation.
    The basic flow of _run_test(), which is the main test process, is
    as follows.
    - drop and create a database
    - populate the database
    - run the test queries and compare the result with the answer
    - dump the database into file
    - drop the database and re-create it
    - restore the data
    - run the test queries and compare again

    You'll need 'sql' directory in your test script directory and place
    these files
    - {test_name}_init.sql
    - {test_name}.ans
    - {test_name}.sql
    and out/diff files will be output in this directory after the test run.
    We still need the answer file although the result might be able to be
    verified by cross check before/after dump, but if the initial population
    of data went unexpectedly, there is no way to catch that case without
    the answer file.
    """

    dbname = 'backupdb'
    sql_dir = 'sql'

    @classmethod
    def locate(cls):
        """Returns a pair of the path to the file of the class
        and its directory.
        """

        myself = sys.modules[cls.__module__].__file__
        myhome = os.path.dirname(myself)
        return myself, myhome

    @classmethod
    def setUpClass(cls):
        myself, myhome = cls.locate()
        sql_dir = os.path.join(myhome, cls.sql_dir)
        for filename in os.listdir(sql_dir):
            if (fnmatch(filename, '*.out*') or
                fnmatch(filename, '*.dump') or
                fnmatch(filename, '*.diff*')):
                os.unlink(os.path.join(sql_dir, filename))

    def _pg_dump(self):
        """Run pg_dump."""
        args = ['-f', self._dump_file, '-d', self.dbname]
        SimpleCommand('pg_dump', args).run_or_fail()

    def _pg_restore(self):
        """This is actually psql, not pg_restore, assuming
        the dump is plain type.
        """

        args = ['-d', self.dbname, '-f', self._dump_file]
        SimpleCommand('psql', args).run_or_fail()

    def _gp_dump(self):
        """Run gp_dump.  This call will obtain the timestamp key."""

        args = ['-d', self.dbname]
        stdout, stderr = SimpleCommand('gp_dump', args).run_or_fail()
        gp_k = None
        for line in stdout.splitlines():
            m = re.match(r"Timestamp Key: (\d{14})", line)
            if m:
                gp_k = m.group(1)

        assert gp_k is not None, "Timestamp key not found"
        self.gp_dump_timestamp = gp_k

    def _gp_restore(self):
        """Run gp_restore.  gp_dump should be run beforehand where
        we obtain timestamp key fro the dump directory.
        """

        args = ['--gp-k', self.gp_dump_timestamp, '-d', self.dbname]
        SimpleCommand('gp_restore', args).run_or_fail()

    def _refresh_database(self):
        "Drop (if exists) and create a database."
        self._drop_database()
        psql = RetryPSQL(sql_cmd="""
                CREATE DATABASE {dbname};
            """.format(dbname=self.dbname), dbname="postgres")
        assert psql.run(), "CREATE DATABASE failed"
        logger.debug("Database created")

    def _drop_database(self):
        """Drop if exists database.
        We need retry-psql here, as gp_dump keeps connection for a while,
        which prevents an immediate DROP DATABASE.
        """

        psql = RetryPSQL(sql_cmd="""
                DROP DATABASE IF EXISTS {dbname}
            """.format(dbname=self.dbname), dbname="postgres")
        assert psql.run(), "DROP DATABASE failed"

    def _run_sql(self, sql_file, out_file=None):
        output_to_file = False
        if out_file:
            output_to_file = True
        assert PSQL.run_sql_file(sql_file,
            dbname = self.dbname,
            output_to_file=output_to_file,
            out_file=out_file)

    def _validate(self, file1, file2):
        result = Gpdiff.are_files_equal(file1, file2)
        assert result, "{file1} and {file2} differ".format(
                file1=file1, file2=file2)

    def _run_test(self, test_name, test_type):
        """Run the actual test.
        @param test_type pg_dump or gp_dump
        """
        base = self._base_file_path()
        base = os.path.join(base, test_name)

        self._sql_file = base + '.sql'
        self._init_file = base + '_init.sql'
        self._init_out_file = base + '.' + test_type + '_init.out'
        self._ans_file = base + '.ans'
        self._dump_file = base + '.dump' # for pg_dump only
        self._pre_out_file = base + '.' + test_type + '.out1'
        self._post_out_file = base + '.' + test_type + '.out2'

        logger.debug("Refreshing database")
        self._refresh_database()
        self._run_sql(self._init_file, self._init_out_file)
        self._run_sql(self._sql_file, self._pre_out_file)
        self._validate(self._pre_out_file, self._ans_file)
        if test_type == 'pg_dump':
            self._pg_dump()
        elif test_type == 'gp_dump':
            self._gp_dump()
        else:
            assert False, "unknown test_type " + test_type
        self._refresh_database()
        if test_type == 'pg_dump':
            self._pg_restore()
        elif test_type == 'gp_dump':
            self._gp_restore()

        self._run_sql(self._sql_file, self._post_out_file)
        self._validate(self._post_out_file, self._ans_file)

    def _base_file_path(self):
        myself, myhome = self.__class__.locate()
        return os.path.join(myhome, self.__class__.sql_dir)
