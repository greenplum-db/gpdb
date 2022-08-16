#!/usr/bin/env python3

import unittest

from gppylib.gparray import GpArray, Segment
from gppylib.system.configurationImplGpdb import GpConfigurationProviderUsingGpdbCatalog
from mock import Mock, patch

class ConfigurationImplGpdbTestCase(unittest.TestCase):

    def setUp(self):
        self.maxDiff = None
        self.configProvider = GpConfigurationProviderUsingGpdbCatalog()
        self.conn = Mock()

        self.coordinator = Segment.initFromString("1|-1|p|p|s|u|cdw|cdw|5432|/data/coordinator")
        self.primary0 = Segment.initFromString("2|0|p|p|s|u|sdw1|sdw1|40000|/data/primary0")
        self.primary1 = Segment.initFromString("3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        self.mirror0 = Segment.initFromString("4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        self.acting_mirror0 = Segment.initFromString("6|0|m|p|d|n|sdw2|sdw2|50002|/data/acting_mirror0")
        self.mirror1 = Segment.initFromString("5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")
        segments = [self.coordinator,self.primary0,self.primary1,self.mirror0,self.mirror1]
        self.gpArray = GpArray(segments)
        self.gpArray.setSegmentsAsLoadedFromDb(segments)


    @patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog.fetchSingleOutputRow', return_value=[6])
    @patch('gppylib.db.dbconn.executeUpdateOrInsert')
    def test_updateSystemConfigRemoveMirror_remove_acting_mirror(self, mockInsert, mockFetch):
        addSQL = self.configProvider.updateSystemConfigRemoveMirror(self.conn, self.gpArray, self.acting_mirror0, "foo")
        self.assertEqual(addSQL, "SELECT gp_add_segment(6::int2, 0::int2, 'm', 'p', 'n', 'd', 50002, 'sdw2', 'sdw2', '/data/acting_mirror0');\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t6,\n\t'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid 6'\n)")
        mockFetch.assert_called_with(self.conn, "SELECT gp_remove_segment_mirror(0::int2)")
        mockInsert.assert_any_call(self.conn, "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\nnow(),\n  6,\n  'foo: removed mirror segment configuration'\n)", 1)

    @patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog.fetchSingleOutputRow', return_value=[4])
    @patch('gppylib.db.dbconn.executeUpdateOrInsert')
    def test_updateSystemConfigRemoveMirror_remove_actual_mirror(self, mockInsert, mockFetch):
        addSQL = self.configProvider.updateSystemConfigRemoveMirror(self.conn, self.gpArray, self.mirror0, "foo")
        self.assertEqual(addSQL, "SELECT gp_add_segment(4::int2, 0::int2, 'm', 'm', 'n', 'd', 50000, 'sdw2', 'sdw2', '/data/mirror0');\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t4,\n\t'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid 4'\n)")
        mockFetch.assert_called_with(self.conn, "SELECT gp_remove_segment_mirror(0::int2)")
        mockInsert.assert_any_call(self.conn, "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\nnow(),\n  4,\n  'foo: removed mirror segment configuration'\n)", 1)

    @patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog.fetchSingleOutputRow', return_value=[2])
    @patch('gppylib.db.dbconn.executeUpdateOrInsert')
    def test_updateSystemConfigRemovePrimary(self, mockInsert, mockFetch):
        addSQL = self.configProvider.updateSystemConfigRemovePrimary(self.conn, self.gpArray, self.primary0, "foo")
        self.assertEqual(addSQL, "SELECT gp_add_segment(2::int2, 0::int2, 'm', 'm', 'n', 'd', 40000, 'sdw1', 'sdw1', '/data/primary0');\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t2,\n\t'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid 2'\n)")
        mockFetch.assert_called_with(self.conn, "SELECT gp_remove_segment(2::int2)")
        mockInsert.assert_any_call(self.conn, "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\nnow(),\n  2,\n  'foo: removed primary segment configuration'\n)", 1)

    @patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog.fetchSingleOutputRow', return_value=[6])
    @patch('gppylib.db.dbconn.executeUpdateOrInsert')
    def test_updateSystemConfigAddMirror_add_acting_mirror(self, mockInsert, mockFetch):
        removeSQL = self.configProvider.updateSystemConfigAddMirror(self.conn, self.gpArray, self.acting_mirror0, "foo")
        self.assertEqual(removeSQL, "SELECT gp_remove_segment(6::int2);\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t6,\n\t'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid 6'\n)")
        mockFetch.assert_called_with(self.conn, "SELECT gp_add_segment(6::int2, 0::int2, 'm', 'm', 'n', 'd', 50002, 'sdw2', 'sdw2', '/data/acting_mirror0')")
        mockInsert.assert_any_call(self.conn, "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\nnow(),\n  6,\n  'foo: inserted mirror segment configuration'\n)", 1)

    @patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog.fetchSingleOutputRow', return_value=[4])
    @patch('gppylib.db.dbconn.executeUpdateOrInsert')
    def test_updateSystemConfigAddMirror_add_actual_mirror(self, mockInsert, mockFetch):
        removeSQL = self.configProvider.updateSystemConfigAddMirror(self.conn, self.gpArray, self.mirror0, "foo")
        self.assertEqual(removeSQL, "SELECT gp_remove_segment(4::int2);\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t4,\n\t'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid 4'\n)")
        mockFetch.assert_called_with(self.conn, "SELECT gp_add_segment(4::int2, 0::int2, 'm', 'm', 'n', 'd', 50000, 'sdw2', 'sdw2', '/data/mirror0')")
        mockInsert.assert_any_call(self.conn, "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\nnow(),\n  4,\n  'foo: inserted mirror segment configuration'\n)", 1)

    @patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog.fetchSingleOutputRow', side_effect=[ [2], [0] ])
    @patch('gppylib.db.dbconn.executeUpdateOrInsert')
    def test_updateSystemConfigAddPrimary(self, mockInsert, mockFetch):
        removeSQL = self.configProvider.updateSystemConfigAddPrimary(self.conn, self.gpArray, self.primary0, "foo", {0: self.mirror0})
        self.assertEqual(removeSQL, "SELECT gp_remove_segment_mirror(0::int2);\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t2,\n\t'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid 2'\n)")
        mockFetch.assert_any_call(self.conn, "SELECT content FROM pg_catalog.gp_segment_configuration WHERE dbId = 2")
        mockFetch.assert_any_call(self.conn, "SELECT gp_add_segment(2::int2, 0::int2, 'p', 'p', 'n', 'u', 40000, 'sdw1', 'sdw1', '/data/primary0')")
        mockInsert.assert_any_call(self.conn, "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\nnow(),\n  2,\n  'foo: inserted primary segment configuration with contentid 0'\n)", 1)

    @patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog.fetchSingleOutputRow', return_value=[6])
    @patch('gppylib.db.dbconn.executeUpdateOrInsert')
    def test_updateSystemConfigRemoveAddMirror_remove_acting_mirror(self, mockInsert, mockFetch):
        addSQL, removeSQL = self.configProvider.updateSystemConfigRemoveAddMirror(self.conn, self.gpArray, self.acting_mirror0, "foo")
        self.assertEqual(addSQL, "SELECT gp_add_segment(6::int2, 0::int2, 'm', 'p', 'n', 'd', 50002, 'sdw2', 'sdw2', '/data/acting_mirror0');\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t6,\n\t'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid 6'\n)")
        self.assertEqual(removeSQL, "SELECT gp_remove_segment(6::int2)")
        mockFetch.assert_any_call(self.conn, "SELECT gp_remove_segment_mirror(0::int2)")
        mockFetch.assert_any_call(self.conn, "SELECT gp_add_segment(6::int2, 0::int2, 'm', 'm', 'n', 'd', 50002, 'sdw2', 'sdw2', '/data/acting_mirror0')")
        mockInsert.assert_any_call(self.conn, "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\nnow(),\n  6,\n  'foo: inserted segment configuration for full recovery or original dbid 6'\n)", 1)

    @patch('gppylib.system.configurationImplGpdb.GpConfigurationProviderUsingGpdbCatalog.fetchSingleOutputRow', return_value=[4])
    @patch('gppylib.db.dbconn.executeUpdateOrInsert')
    def test_updateSystemConfigRemoveAddMirror_remove_actual_mirror(self, mockInsert, mockFetch):
        addSQL, removeSQL = self.configProvider.updateSystemConfigRemoveAddMirror(self.conn, self.gpArray, self.mirror0, "foo")
        self.assertEqual(addSQL, "SELECT gp_add_segment(4::int2, 0::int2, 'm', 'm', 'n', 'd', 50000, 'sdw2', 'sdw2', '/data/mirror0');\nINSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\n\tnow(),\n\t4,\n\t'gprecoverseg: segment config for backout: inserted segment configuration for full recovery or original dbid 4'\n)")
        self.assertEqual(removeSQL, "SELECT gp_remove_segment(4::int2)")
        mockFetch.assert_any_call(self.conn, "SELECT gp_remove_segment_mirror(0::int2)")
        mockFetch.assert_any_call(self.conn, "SELECT gp_add_segment(4::int2, 0::int2, 'm', 'm', 'n', 'd', 50000, 'sdw2', 'sdw2', '/data/mirror0')")
        mockInsert.assert_any_call(self.conn, "INSERT INTO gp_configuration_history (time, dbid, \"desc\") VALUES(\nnow(),\n  4,\n  'foo: inserted segment configuration for full recovery or original dbid 4'\n)", 1)
