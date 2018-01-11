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

from persistent_rebuild_utility import PTRebuildUtil
from mpp.gpdb.tests.storage.persistent_tables.fault.genFault import Fault
from tinctest import TINCTestCase

''' Global Persistent table Rebuild -Test Enchantment ParisTX-PT '''

class RebuildPersistentObjectsTest(TINCTestCase):

    def test_rebuild_persistent_objects_segment(self):
        ptutil = PTRebuildUtil() 
        if ptutil.check_dbconnections():
            tinctest.logger.info('As there are active Database connections, cant rebuild PT')
            return
        (hostname, port) = ptutil.get_hostname_port_of_segment()
        tinctest.logger.info('Rebuilding Global Persistent Object on segment %s : %s'%(hostname, port))
        ptutil.persistent_Rebuild(hostname, port, 'Segment')
        
    def test_rebuild_persistent_objects_master(self):
        ptutil = PTRebuildUtil()
        if ptutil.check_dbconnections():
            tinctest.logger.info('As there are active Database connections, cant rebuild PT')
            return
        tinctest.logger.info('Rebuilding Global Persistent Object on Master')
        ptutil.persistent_Rebuild()

class AbortRebuildPersistentObjectsTest(TINCTestCase):

    @classmethod
    def setUpClass(cls):
        tinctest.logger.info('Abort Rebuilding Global Persistent Object process...')

    def test_rebuild_persistent_objects(self):
        ptutil = PTRebuildUtil()
        if ptutil.check_dbconnections():
            tinctest.logger.info('As there are active Database connections, cant rebuild PT')
            return
        (hostname, port) = ptutil.get_hostname_port_of_segment()
        tinctest.logger.info('Rebuilding Global Persistent Object on segment %s : %s'%(hostname, port))
        ptutil.persistent_Rebuild(hostname, port, 'Segment')	

    def test_stop_db(self):
        newfault = Fault()
        newfault.stop_db('i')
