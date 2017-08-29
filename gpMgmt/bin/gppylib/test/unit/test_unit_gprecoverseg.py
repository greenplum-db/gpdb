import os
import shutil
import sys
import tempfile

from mock import *

from gp_unittest import *
from gparray import GpArray, GpDB, FAULT_STRATEGY_FILE_REPLICATION
from gppylib.heapchecksum import HeapChecksum
from gppylib.operations.buildMirrorSegments import GpMirrorToBuild, GpMirrorListToBuild
from pgconf import gucdict, setting
from system import faultProberInterface
from system.configurationInterface import GpConfigurationProvider


class Options:
    def __init__(self):
        self.newRecoverHosts = None
        self.spareDataDirectoryFile = ""
        self.recoveryConfigFile = None
        self.outputSpareDataDirectoryFile = None
        self.rebalanceSegments = None

        self.outputSampleConfigFile = None
        self.parallelDegree = 1
        self.forceFullResynchronization = None
        self.persistent_check = None
        self.quiet = None
        self.interactive = False


class GpRecoversegTestCase(GpTestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.config_file_path = os.path.join(self.temp_dir, "foo")
        with open(self.config_file_path, "w") as config_file:
            config_file.write("")

        self.conn = Mock()
        self.cursor = FakeCursor()
        self.db_singleton = Mock()

        self.os_env = dict(USER="my_user")
        self.os_env["MASTER_DATA_DIRECTORY"] = self.temp_dir
        self.os_env["GPHOME"] = self.temp_dir
        self.gparray = self._create_gparray_with_2_primary_2_mirrors()

        self.pool = Mock()
        self.pool.getCompletedItems.return_value = []

        self.pgconf_dict = gucdict()
        self.pgconf_dict["port"] = setting("port", "123", None, None, None)
        self.pgconf_dict["max_connection"] = setting("max_connections", "1", None, None, None)

        configProviderMock = MagicMock(spec=GpConfigurationProvider)
        configProviderMock.initializeProvider.return_value = configProviderMock

        self.gpArrayMock = MagicMock(spec=GpArray)
        self.gpArrayMock.getDbList.side_effect = [[], [self.primary0], [self.primary0]]
        self.gpArrayMock.getFaultStrategy.return_value = FAULT_STRATEGY_FILE_REPLICATION
        self.gpArrayMock.isStandardArray.return_value = (True, None)
        self.gpArrayMock.master = self.gparray.master

        configProviderMock.loadSystemConfig.return_value = self.gpArrayMock

        self.mirror_to_build = GpMirrorToBuild(self.mirror0, self.primary0, None, False)
        self.apply_patches([
            patch('os.environ', new=self.os_env),
            patch('gppylib.db.dbconn.connect', return_value=self.conn),
            patch('gppylib.db.dbconn.execSQL', return_value=self.cursor),
            patch('gppylib.db.dbconn.execSQLForSingletonRow', return_value=["foo"]),
            patch('gppylib.commands.base.WorkerPool'),
            patch('gppylib.pgconf.readfile', return_value=self.pgconf_dict),
            patch('gppylib.commands.gp.GpVersion'),
            patch('gppylib.db.catalog.getCollationSettings',
                  return_value=("en_US.utf-8", "en_US.utf-8", "en_US.utf-8")),
            patch('gppylib.system.faultProberInterface.getFaultProber'),
            patch('gppylib.system.configurationInterface.getConfigurationProvider', return_value=configProviderMock),
            patch('gppylib.commands.base.WorkerPool', return_value=self.pool),
            patch('gppylib.gparray.GpArray.getSegmentsByHostName', return_value={}),
            patch('gppylib.gplog.get_default_logger'),
            patch.object(GpMirrorListToBuild, "__init__", return_value=None),
            patch.object(GpMirrorListToBuild, "buildMirrors"),
            patch.object(GpMirrorListToBuild, "getAdditionalWarnings"),
            patch.object(GpMirrorListToBuild, "getMirrorsToBuild"),
            patch.object(HeapChecksum, "check_segment_consistency"),
            patch.object(HeapChecksum, "get_segments_checksum_settings"),
        ])

        self.call_count = 0
        self.return_one = True

        self.mock_get_mirrors_to_build = self.get_mock_from_apply_patch('getMirrorsToBuild')
        self.mock_heap_checksum_init = self.get_mock_from_apply_patch("__init__")
        self.mock_check_segment_consistency = self.get_mock_from_apply_patch('check_segment_consistency')
        self.mock_get_segments_checksum_settings = self.get_mock_from_apply_patch('get_segments_checksum_settings')

        sys.argv = ["gprecoverseg"]  # reset to relatively empty args list

        # import HERE so that patches are already in place!
        from programs.clsRecoverSegment import GpRecoverSegmentProgram

        options = Options()
        options.masterDataDirectory = self.temp_dir
        options.spareDataDirectoryFile = self.config_file_path

        self.subject = GpRecoverSegmentProgram(options)
        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal'])

        faultProberInterface.gFaultProber = Mock()

    def _get_test_mirrors(self):
        if self.return_one:
            return [self.mirror_to_build]

        if self.call_count == 0:
            self.call_count += 1
            return [self.mirror_to_build]
        else:
            self.call_count += 1
            return []

    def tearDown(self):
        shutil.rmtree(self.temp_dir)
        super(GpRecoversegTestCase, self).tearDown()

    def test_when_checksums_mismatch_it_raises(self):
        self.primary0.heap_checksum = 0
        self.mock_check_segment_consistency.return_value = ([], [self.primary0], 1)
        self.mock_get_segments_checksum_settings.return_value = ([self.mirror0], [])
        self.return_one = True
        self.mock_get_mirrors_to_build.side_effect = self._get_test_mirrors
        self.assertTrue(self.gparray.master.isSegmentMaster(True))

        with self.assertRaisesRegexp(Exception, "Heap checksum setting differences reported on segments"):
            self.subject.run()

        self.mock_get_segments_checksum_settings.assert_called_with([self.primary0])
        self.subject.logger.fatal.assert_any_call('Heap checksum setting differences reported on segments')
        self.subject.logger.fatal.assert_any_call('Failed checksum consistency validation:')
        self.subject.logger.fatal.assert_any_call('sdw1 checksum set to 0 differs from master checksum set to 1')

    @patch.object(HeapChecksum, "__init__", return_value=None)
    def test_when_cannot_determine_checksums_it_raises(self, mock_heap_checksum_init):
        self.mock_check_segment_consistency.return_value = ([], [1], True)
        self.mock_get_segments_checksum_settings.return_value = ([], [])
        self.mock_get_mirrors_to_build.side_effect = self._get_test_mirrors
        self.return_one = True
        self.assertTrue(self.gparray.master.isSegmentMaster(True))
        with self.assertRaisesRegexp(Exception, "No segments responded to ssh query for heap checksum validation."):
            self.subject.run()

        self.mock_get_segments_checksum_settings.assert_called_with([self.primary0])
        mock_heap_checksum_init.assert_called_with(self.gpArrayMock, logger=self.subject.logger, num_workers=1)

    @patch("os._exit")
    def test_when_no_segments_to_recover_validation_succeeds(self, _):
        self.mock_get_mirrors_to_build.side_effect = self._get_test_mirrors
        self.return_one = False

        self.subject.run()

        self.subject.logger.info.assert_any_call('No checksum validation necessary when '
                                                 'there are no segments to recover.')


    def _create_gparray_with_2_primary_2_mirrors(self):
        master = GpDB.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")
        self.primary0 = GpDB.initFromString(
            "2|0|p|p|s|u|sdw1|sdw1|40000|41000|/data/primary0||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        primary1 = GpDB.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        self.mirror0 = GpDB.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        mirror1 = GpDB.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")
        return GpArray([master, self.primary0, primary1, self.mirror0, mirror1])



if __name__ == '__main__':
    run_tests()
