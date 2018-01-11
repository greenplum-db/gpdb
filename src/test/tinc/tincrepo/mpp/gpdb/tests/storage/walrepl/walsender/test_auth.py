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

from mpp.models import MPPTestCase
from gppylib.commands.base import Command

from gppylib.commands import pg
from gppylib.db import dbconn

from mpp.gpdb.tests.storage.walrepl.lib import DbConn
from mpp.gpdb.tests.storage.walrepl.lib.walcomm import WalClient
from mpp.gpdb.tests.storage.walrepl.lib import pqwrap

import os
import re

class auth(MPPTestCase):

    class DummyDb(object):
        def getSegmentDataDirectory(self):
            return os.environ['MASTER_DATA_DIRECTORY']

    def reload_master(self):
        # cheat gppylib
        pg.ReloadDbConf.local('pg_ctl reload', auth.DummyDb())

    def disable_replication(self):
        """
        Remove 'replication' lines from pg_hba.
        """

        def callback(lines):
            pat = re.compile(r'\sreplication\s')
            return [l for l in lines if not pat.search(l)]

        self.modify_pg_hba(callback)
        self.reload_master()

    def enable_replication(self, rolname=None):
        """
        Add 'replication' lines to pg_hba.
        """

        if rolname is None:
            with DbConn(utility=True) as conn:
                # oid = 10 is the bootstrap user.
                sql = 'select rolname from pg_authid where oid = 10'
                rolname = conn.execute(sql)[0].rolname

        def callback(lines):
            values = ['local', 'replication', rolname, 'ident']
            newline = '\t'.join(values) + '\n'
            if newline not in lines:
                lines.append(newline)

            values = ['host', 'replication', rolname, '0.0.0.0/0', 'trust']
            newline = '\t'.join(values) + '\n'
            if newline not in lines:
                lines.append(newline)

            return lines

        self.modify_pg_hba(callback)
        self.reload_master()

    def modify_pg_hba(self, callback):
        """
        Modify pg_hba.conf by taking callback, which is func(lines) where
        lines is array of lines in pg_hba.  The array returned by this
        callback will be written in pg_hba.conf.  gpstop -u or pg_ctl reload
        is required to activate the new settings.  The original file is saved
        as .orig file, so after the work ends you can restore the original
        file.
        """

        datadir = os.environ['MASTER_DATA_DIRECTORY']
        pg_hba = os.path.join(datadir, 'pg_hba.conf')
        with open(pg_hba) as f:
            lines = f.readlines()

        lines = callback(lines)

        tmpfile = pg_hba + '.tmp'
        with open(tmpfile, 'w') as f:
            f.writelines(lines)
        os.rename(pg_hba, pg_hba + '.orig')
        os.rename(tmpfile, pg_hba)

    def tearDown(self):
        datadir = os.environ['MASTER_DATA_DIRECTORY']
        pg_hba = os.path.join(datadir, 'pg_hba.conf')

        if os.path.exists(pg_hba + '.orig'):
            os.rename(pg_hba + '.orig', pg_hba)
            self.reload_master()

        with DbConn() as conn:
            conn.execute('drop role if exists waluser')

    def test_reject(self):
        """
        Replication connection should be rejected if pg_hba disallows it.

        @tags sanity
        """

        self.disable_replication()
        with WalClient("replication=true") as client:
            self.assertEqual(client.status(), pqwrap.CONNECTION_BAD)

        with WalClient("replication=true host=localhost") as client:
            self.assertEqual(client.status(), pqwrap.CONNECTION_BAD)

    def test_accept(self):
        """
        Replication connection should be accepted when pg_hba says so.

        @tags sanity
        """
        Command('gpinitstandby -ra','gpinitstandby -ra').run()
        self.enable_replication()
        with WalClient('replication=true') as client:
            self.assertEqual(client.status(), pqwrap.CONNECTION_OK)

        with WalClient('replication=true host=localhost') as client:
            self.assertEqual(client.status(), pqwrap.CONNECTION_OK)

    def test_nonsuper(self):
        """
        Replication connection for non-super user shuold be disallowed
        even if pg_hba says ok.

        @tags sanity
        """
        with DbConn() as conn:
            conn.execute('create role waluser login')
        self.enable_replication(rolname='waluser')
        with WalClient('replication=true user=waluser') as client:
            self.assertEqual(client.status(), pqwrap.CONNECTION_BAD)
