import os
import imp
import sys

from mock import *
from gp_unittest import *
from gppylib.programs.clsAddMirrors import GpAddMirrorsProgram
from gparray import GpDB, GpArray
from gppylib.system.environment import GpMasterEnvironment
from gppylib.system.configurationInterface import GpConfigurationProvider

class GpAddMirrorsTest(GpTestCase):
    def setUp(self):
        self.subject = GpAddMirrorsProgram
        self.gparrayMock = self.createGpArrayWith2Primary2Mirrors()

        self.apply_patches([
            patch('gppylib.programs.clsAddMirrors.base.WorkerPool'),
            patch('gppylib.programs.clsAddMirrors.logger', return_value=Mock(spec=['log', 'info', 'debug', 'error'])),
            patch('gppylib.programs.clsAddMirrors.log_to_file_only', return_value=Mock()),
            patch('gppylib.programs.clsAddMirrors.GpMasterEnvironment', return_value=Mock(), spec=GpMasterEnvironment),
            patch('gppylib.system.faultProberInterface.getFaultProber'),
            patch('gppylib.programs.clsAddMirrors.configInterface.getConfigurationProvider', return_value=Mock()),
            patch('gppylib.programs.clsAddMirrors.heapchecksum.HeapChecksum'),
        ])
        self.mock_logger = self.get_mock_from_apply_patch('logger')
        self.gpMasterEnvironmentMock = self.get_mock_from_apply_patch("GpMasterEnvironment")
        self.gpMasterEnvironmentMock.return_value.getMasterPort.return_value = 123456
        self.getConfigProviderFunctionMock = self.get_mock_from_apply_patch('getConfigurationProvider')
        configProviderMock = Mock(spec=GpConfigurationProvider)
        self.getConfigProviderFunctionMock.return_value = configProviderMock
        configProviderMock.initializeProvider.return_value = configProviderMock
        configProviderMock.loadSystemConfig.return_value = self.gparrayMock
        self.mock_heap_checksum = self.get_mock_from_apply_patch('HeapChecksum')
        self.mock_heap_checksum.return_value.get_master_value.return_value = 1
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [0])
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = False
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = ([1], [1], 1)

    def tearDown(self):
        super(GpAddMirrorsTest, self).tearDown()

    def test_validate_heap_checksum_throws_if_cluster_inconsistent(self):
        with self.assertRaises(Exception):
            self.subject.validate_heap_checksums(self.subject(None), self.gparrayMock)

    def test_validate_heap_checksum_succeds_if_cluster_consistent(self):
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [1])
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = True
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = ([2], [], 1)
        self.subject.validate_heap_checksums(self.subject(None), self.gparrayMock)
        self.mock_logger.info.assert_any_call("Heap checksum setting consistent across cluster")

    def test_run_calls_validate_heap_checksum(self):
        self.primary0.heap_checksum = 1
        self.primary1.heap_checksum = 0
        self.master.heap_checksum = 1
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = (
            [self.primary0], [self.primary1], self.master.heap_checksum)
        sys.argv = ['gpaddmirrors', '-a']
        parser = self.subject.createParser()
        options, args = parser.parse_args()
        command_obj = self.subject.createProgram(options, args)
        with self.assertRaisesRegexp(Exception, 'Segments have heap_checksum set inconsistently to master'):
            command_obj.run()

    def createGpArrayWith2Primary2Mirrors(self):
        self.master = GpDB.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")
        self.primary0 = GpDB.initFromString(
            "2|0|p|p|s|u|aspen|sdw1|40000|41000|/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/qddir/demoDataDir-1||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        self.primary1 = GpDB.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        mirror0 = GpDB.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        mirror1 = GpDB.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")

        return GpArray([self.master, self.primary0, self.primary1, mirror0, mirror1])


if __name__ == '__main__':
    run_tests()
