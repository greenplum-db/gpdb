#!python
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

Unit tests fro PgHba module
Tests adding, removing, searching entries. (There is no
database running when these tests are executed)
"""

# TYPE  DATABASE    USER        CIDR-ADDRESS          METHOD
#local   all         all                               trust
#host    postgres    all         192.168.12.10/32      md5

import os
import unittest2 as unittest
import mpp.lib.PgHba as PgHba

_BASEDIR = os.path.dirname(__file__)

class PgHbaTestCase(unittest.TestCase):
    def setUp(self):
        self.pghba_file = PgHba.PgHba(os.path.join(_BASEDIR, 'input', 'pg_hba.conf'))

    def test_initial_values(self):
        res1 = self.pghba_file.search(type='local', 
                                     database='all',
                                     user='all',
                                     authmethod='trust')
        res2 = self.pghba_file.search(type='host', 
                                     database='postgres',
                                     user='all',
                                     address='192.168.12.10/32',
                                     authmethod='md5')
        self.assertTrue(res1)
        self.assertTrue(res2)
        self.assertEqual(2, len(self.pghba_file.get_contents_without_comments()))


    def test_add_new_value(self):
        new_ent = PgHba.Entry(entry_type='local', 
                              database = 'test_database',
                              user = 'test_user', 
                              authmethod = 'sspi')
        self.pghba_file.add_entry(new_ent)
        self.assertEqual(3, len(self.pghba_file.get_contents_without_comments()))

        res = self.pghba_file.search(type='local', 
                                     database='test_database',
                                     user='test_user',
                                     authmethod='sspi')
        self.assertTrue(res)
