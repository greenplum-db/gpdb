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

import tinctest

import unittest2 as unittest

from mpp.gpdb.tests.storage.filerep.Filerep_Resync.schema.genSchema import Schema
from tinctest import TINCTestCase

class AOTable(TINCTestCase):
    '''
    Create the required schema for the scenario
    '''
    @classmethod
    def setUpClass(cls):
        super(AOTable,cls).setUpClass()
        tinctest.logger.info('Setting up required schema for the test')

    def test_create_ao_table(self):
        newschema = Schema()
        self.assertTrue(newschema.create_ao_table(),"AO table not created")

class DuplicateEntries(TINCTestCase):
    '''
    Check for duplicate entries in the Persistent Tables
    '''
    @classmethod
    def setUpClass(cls):
        super(DuplicateEntries,cls).setUpClass()
        tinctest.logger.info('Checking for duplicate entries in Persistent Tables')

    def test_duplicate_entries_after_hitting_fault(self):
        newschema = Schema()
        self.assertTrue(newschema.check_duplicate_entries_in_PT(),"Failed to create the duplicate entries scenario")

    def test_duplicate_entries_after_recovery(self):
        newschema = Schema()
        self.assertFalse(newschema.check_duplicate_entries_in_PT(),"Found Duplicate entries in Persistent Table")
