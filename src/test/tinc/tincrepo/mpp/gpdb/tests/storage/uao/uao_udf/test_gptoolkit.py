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

from mpp.models import MPPTestCase
from mpp.lib.PSQL import PSQL
import os
import sys

class UAOGPToolkitTestCase(MPPTestCase):
    """
        Test suite to test the ao diagnostics function in there
        gp_toolkit version.
    """

    def get_oid(self, table_name):
        s = PSQL.run_sql_command("SELECT oid from pg_class WHERE relname = '%s'" % table_name);
        return int(s.splitlines()[3])

    def get_utility_mode_port():
        """
            Gets the port number/hostname combination of the
            dbid with the id = name
        """
        o = PSQL.run_sql_command("SELECT port FROM gp_segment_configuration WHERE dbid = 2").splitlines()[3]
        return o

    utility_port = get_utility_mode_port()

    def has_zero_rows(self, sql, utility=False):
        if not utility:
            o = PSQL.run_sql_command(sql)
        else:
            o = PSQL.run_sql_command_utility_mode(sql, port=self.utility_port)
        return o.find("(0 rows)") >= 0

    def has_rows(self, sql, utility=False):
        if not utility:
            o = PSQL.run_sql_command(sql)
        else:
            o = PSQL.run_sql_command_utility_mode(sql, port=self.utility_port)
        return (o.find("rows)") >= 0 and o.find("(0 rows)") < 0) or o.find("(1 row)") >= 0

    def has_zero_rows(self, sql, utility=False):
        if not utility:
            o = PSQL.run_sql_command(sql)
        else:
            o = PSQL.run_sql_command_utility_mode(sql, port=self.utility_port)
        return o.find("(0 rows)") >= 0

    def test_ao_co_diagnostics(self):
        base_dir = os.path.dirname(sys.modules[self.__class__.__module__].__file__)
        setup_file = os.path.join(
            base_dir, "gptoolkit_sql", "gptoolkit_setup.sql");    
        PSQL.run_sql_file(setup_file)

        oid = self.get_oid('foo');
        oidcs = self.get_oid('foocs');

        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aoseg_history(%s)' % oid))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aocsseg(%s)' % oidcs))
        self.assertTrue(self.has_rows("SELECT * FROM gp_toolkit.__gp_aocsseg_name('foocs')"))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aocsseg_history(%s)' % oidcs))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aoseg_history(%s)' % oid))
        self.assertTrue(self.has_zero_rows('SELECT * FROM gp_toolkit.__gp_aovisimap(%s)' % oid))
        self.assertTrue(self.has_zero_rows("SELECT * FROM gp_toolkit.__gp_aovisimap_name('foo')"))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aovisimap_hidden_info(%s)' % oid))
        self.assertTrue(self.has_rows("SELECT * FROM gp_toolkit.__gp_aovisimap_hidden_info_name('foo')"))
        self.assertTrue(self.has_zero_rows('SELECT * FROM gp_toolkit.__gp_aovisimap_entry(%s)' % oid))
        self.assertTrue(self.has_zero_rows("SELECT * FROM gp_toolkit.__gp_aovisimap_entry_name('foo')"))
        self.assertTrue(self.has_rows("SELECT * FROM gp_toolkit.__gp_aoseg_name('foo')"))

        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aoseg_history(%s)' % oid, True))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aocsseg(%s)' % oidcs, True))
        self.assertTrue(self.has_rows("SELECT * FROM gp_toolkit.__gp_aocsseg_name('foocs')", True))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aocsseg_history(%s)' % oidcs, True))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aoseg_history(%s)' % oid, True))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aovisimap(%s)' % oid, True))
        self.assertTrue(self.has_rows("SELECT * FROM gp_toolkit.__gp_aovisimap_name('foo')", True))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aovisimap_hidden_info(%s)' % oid, True))
        self.assertTrue(self.has_rows("SELECT * FROM gp_toolkit.__gp_aovisimap_hidden_info_name('foo')", True))
        self.assertTrue(self.has_rows('SELECT * FROM gp_toolkit.__gp_aovisimap_entry(%s)' % oid, True))
        self.assertTrue(self.has_rows("SELECT * FROM gp_toolkit.__gp_aovisimap_entry_name('foo')", True))
        self.assertTrue(self.has_rows("SELECT * FROM gp_toolkit.__gp_aoseg_name('foo')", True))
