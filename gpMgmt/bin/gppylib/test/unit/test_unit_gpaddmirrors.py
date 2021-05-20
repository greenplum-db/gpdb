import os
import sys

import io
from mock import *
from .gp_unittest import *
from gppylib.programs.clsAddMirrors import GpAddMirrorsProgram, ProgramArgumentValidationException
from gparray import Segment, GpArray
from gppylib.system.environment import GpCoordinatorEnvironment
from gppylib.system.configurationInterface import GpConfigurationProvider


class GpAddMirrorsTest(GpTestCase):
    def setUp(self):
        # because gpaddmirrors does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpaddmirrors.py, this is equivalent to:
        #   import gpaddmirrors
        #   self.subject = gpaddmirrors
        self.subject = GpAddMirrorsProgram(None)
        self.gparrayMock = self._createGpArrayWith2Primary2Mirrors()
        self.gparray_get_segments_by_hostname = dict(sdw1=[self.primary0])
        self.apply_patches([
            patch('builtins.input'),
            patch('gppylib.programs.clsAddMirrors.base.WorkerPool'),
            patch('gppylib.programs.clsAddMirrors.logger', return_value=Mock(spec=['log', 'info', 'debug', 'error'])),
            patch('gppylib.programs.clsAddMirrors.log_to_file_only', return_value=Mock()),
            patch('gppylib.programs.clsAddMirrors.GpCoordinatorEnvironment', return_value=Mock(), spec=GpCoordinatorEnvironment),
            patch('gppylib.system.faultProberInterface.getFaultProber'),
            patch('gppylib.programs.clsAddMirrors.configInterface.getConfigurationProvider', return_value=Mock()),
            patch('gppylib.programs.clsAddMirrors.heapchecksum.HeapChecksum'),
            patch('gppylib.gparray.GpArray.getSegmentsByHostName', return_value=self.gparray_get_segments_by_hostname),

        ])
        self.input_mock = self.get_mock_from_apply_patch("input")
        self.mock_logger = self.get_mock_from_apply_patch('logger')
        self.gpCoordinatorEnvironmentMock = self.get_mock_from_apply_patch("GpCoordinatorEnvironment")
        self.gpCoordinatorEnvironmentMock.return_value.getCoordinatorPort.return_value = 123456
        self.gpCoordinatorEnvironmentMock.return_value.getCoordinatorDataDir.return_value = "/data/coordinator/gpseg-1"
        self.getConfigProviderFunctionMock = self.get_mock_from_apply_patch('getConfigurationProvider')
        self.config_provider_mock = Mock(spec=GpConfigurationProvider)
        self.getConfigProviderFunctionMock.return_value = self.config_provider_mock
        self.config_provider_mock.initializeProvider.return_value = self.config_provider_mock
        self.config_provider_mock.loadSystemConfig.return_value = self.gparrayMock
        self.mock_heap_checksum = self.get_mock_from_apply_patch('HeapChecksum')
        self.mock_heap_checksum.return_value.get_coordinator_value.return_value = 1
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [0])
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = False
        self.coordinator.heap_checksum = 1
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = (
            [self.primary0], [], self.coordinator.heap_checksum)
        self.cdd = os.getenv("COORDINATOR_DATA_DIRECTORY")
        if not self.cdd:
            self.cdd = "/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/qddir/demoDataDir-1"
            os.environ["COORDINATOR_DATA_DIRECTORY"] = self.cdd

        self.parser = GpAddMirrorsProgram.createParser()

    def tearDown(self):
        super(GpAddMirrorsTest, self).tearDown()

    def test_validate_heap_checksum_succeeds_if_cluster_consistent(self):
        sys.argv = ['gpaddmirrors', '-a']
        options, _ = self.parser.parse_args()
        self.subject = GpAddMirrorsProgram(options)
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [1])
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = True
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = ([2], [], 1)
        self.subject.validate_heap_checksums(self.gparrayMock)
        self.mock_logger.info.assert_any_call("Heap checksum setting consistent across cluster")

    def test_run_calls_validate_heap_checksum(self):
        self.primary0.heap_checksum = 1
        self.primary1.heap_checksum = 0
        self.coordinator.heap_checksum = 1
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = (
            [self.primary0], [self.primary1], self.coordinator.heap_checksum)
        sys.argv = ['gpaddmirrors', '-a']
        options, args = self.parser.parse_args()
        command_obj = self.subject.createProgram(options, args)
        with self.assertRaisesRegex(Exception, 'Segments have heap_checksum set inconsistently to coordinator'):
            command_obj.run()

    def test_option_batch_of_size_0_will_raise(self):
        sys.argv = ['gpaddmirrors', '-B', '0']
        options, _ = self.parser.parse_args()
        self.subject = GpAddMirrorsProgram(options)
        with self.assertRaises(ProgramArgumentValidationException):
            self.subject.run()

    @patch('sys.stdout', new_callable=io.StringIO)
    def test_option_version(self, mock_stdout):
        sys.argv = ['gpaddmirrors', '--version']
        with self.assertRaises(SystemExit) as cm:
            options, _ = self.parser.parse_args()

        self.assertIn("gpaddmirrors version $Revision$", mock_stdout.getvalue())
        self.assertEqual(cm.exception.code, 0)

    def test_generated_file_contains_default_port_offsets(self):
        datadir_config = _write_datadir_config(self.cdd)
        mirror_config_output_file = "/tmp/test_gpaddmirrors.config"
        sys.argv = ['gpaddmirrors', '-o', mirror_config_output_file, '-m', datadir_config]
        self.config_provider_mock.loadSystemConfig.return_value = GpArray([self.coordinator, self.primary0, self.primary1])
        options, _ = self.parser.parse_args()
        self.subject = GpAddMirrorsProgram(options)
        self.subject.run()

        with open(mirror_config_output_file, 'r') as fp:
            result = fp.readlines()

        self.assertIn("41000", result[0])

    def test_generated_file_contains_port_offsets(self):
        datadir_config = _write_datadir_config(self.cdd)
        mirror_config_output_file = "/tmp/test_gpaddmirrors.config"
        sys.argv = ['gpaddmirrors', '-p', '5000', '-o', mirror_config_output_file, '-m', datadir_config]
        options, _ = self.parser.parse_args()
        self.config_provider_mock.loadSystemConfig.return_value = GpArray([self.coordinator, self.primary0, self.primary1])
        self.subject = GpAddMirrorsProgram(options)
        self.subject.run()

        with open(mirror_config_output_file, 'r') as fp:
            result = fp.readlines()

        self.assertIn("45000", result[0])

    def test_datadir_interview(self):
        self.input_mock.side_effect = ["/tmp/datadirs/mirror1", "/tmp/datadirs/mirror2", "/tmp/datadirs/mirror3"]
        sys.argv = ['gpaddmirrors', '-p', '5000']
        options, _ = self.parser.parse_args()
        self.config_provider_mock.loadSystemConfig.return_value = GpArray([self.coordinator, self.primary0, self.primary1])
        self.subject = GpAddMirrorsProgram(options)
        directories = self.subject._GpAddMirrorsProgram__getDataDirectoriesForMirrors(3, None)
        self.assertEqual(len(directories), 3)

    def _createGpArrayWith2Primary2Mirrors(self):
        self.coordinator = Segment.initFromString(
            "1|-1|p|p|s|u|cdw|cdw|5432|/data/coordinator")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/qddir/demoDataDir-1")
        self.primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|/data/primary1")
        mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|/data/mirror0")
        mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|/data/mirror1")

        return GpArray([self.coordinator, self.primary0, self.primary1, mirror0, mirror1])


def _write_datadir_config(cdd):
    cdd_parent_parent = os.path.realpath(cdd + "../../../")
    mirror_data_dir = os.path.join(cdd_parent_parent, 'mirror')
    if not os.path.exists(mirror_data_dir):
        os.mkdir(mirror_data_dir)
    datadir_config = '/tmp/gpaddmirrors_datadir_config'
    contents = \
"""
{0}
""".format(mirror_data_dir)
    with open(datadir_config, 'w') as fp:
        fp.write(contents)
    return datadir_config


if __name__ == '__main__':
    run_tests()
