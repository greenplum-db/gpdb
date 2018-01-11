import os
import imp

from gp_unittest import *
from mock import *
from gparray import Segment, GpArray
from gppylib.db.dbconn import DbURL
from gppylib.db import catalog
from gppylib.gplog import *
from gppylib.system.configurationInterface import GpConfigurationProvider
from gppylib.system.environment import GpMasterEnvironment

class GpExpand(GpTestCase):
    def setUp(self):
        # because gpexpand does not have a .py extension,
        # we have to use imp to import it
        # if we had a gpexpand.py, this is equivalent to:
        #   import gpexpand
        #   self.subject = gpexpand
        gpexpand_file = os.path.abspath(os.path.dirname(__file__) + "/../../../gpexpand")
        self.subject = imp.load_source('gpexpand', gpexpand_file)
        self.old_sys_argv = sys.argv
        sys.argv = []  # We need to do this otherwise, the parser will read the command line as the default arguments.
        self.options, self.args, self.parser = self.subject.parseargs()

        self.subject.logger = Mock(spec=['log', 'warn', 'info', 'debug', 'error', 'warning', 'fatal', 'exception'])

        # All we're doing here is checking files using open, which doesn't seem worth the effort to fully mock out
        self.subject.is_gpexpand_running = Mock(return_value=False)

        self.gparray = self.createGpArrayWith2Primary2Mirrors()
        self.apply_patches([
            patch('gpexpand.GpArray.initFromCatalog', return_value=self.gparray),
            patch('__builtin__.open', mock_open(), create=True),
            patch('gpexpand.PgControlData', return_value=Mock()),
            patch('gpexpand.copy.deepcopy', return_value=Mock()),
            patch('gpexpand.dbconn.execSQL', return_value=FakeCursor()),
            patch('gpexpand.GpExpandStatus', return_value=Mock()),
            patch('gpexpand.GpExpandStatus.return_value.get_current_status', return_value=(None, None)),
            patch('gpexpand.GpStart.local', return_value=Mock()),
            patch('gpexpand.GpStop.local', return_value=Mock()),
            patch('gpexpand.chk_local_db_running', return_value=(True, True, True, True, True)),
            patch('gpexpand.get_local_db_mode', return_value='NORMAL'),
            patch('gpexpand.dbconn.DbURL', return_value=Mock(), spec=DbURL),
            patch('gpexpand.dbconn.connect', return_value=Mock()),
            patch('gpexpand.GpMasterEnvironment', return_value=Mock(), spec=GpMasterEnvironment),
            patch('gpexpand.configurationInterface.getConfigurationProvider'),
            patch('os.path.exists', return_value=Mock()),
            patch('gpexpand.get_default_logger', return_value=self.subject.logger),
            patch('gpexpand.HeapChecksum'),
        ])
        self.getConfigProviderFunctionMock = self.get_mock_from_apply_patch("getConfigurationProvider")
        self.gpMasterEnvironmentMock = self.get_mock_from_apply_patch("GpMasterEnvironment")
        self.previous_master_data_directory = os.getenv('MASTER_DATA_DIRECTORY', '')
        os.environ["MASTER_DATA_DIRECTORY"] = '/tmp/dirdoesnotexist'
        configProviderMock = Mock(spec=GpConfigurationProvider)
        self.getConfigProviderFunctionMock.return_value = configProviderMock
        configProviderMock.initializeProvider.return_value = configProviderMock
        self.gpMasterEnvironmentMock.return_value.getMasterPort.return_value = 123456
        self.mock_heap_checksum = self.get_mock_from_apply_patch('HeapChecksum')
        self.mock_heap_checksum.return_value.get_master_value.return_value = 1
        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [1])
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = True
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = ([2], [], 1)

    def tearDown(self):
        os.environ['MASTER_DATA_DIRECTORY'] = self.previous_master_data_directory
        sys.argv = self.old_sys_argv
        super(GpExpand, self).tearDown()

    def test_PrepFileSpaces_issues_correct_postgres_command(self):
        prep_file_spaces = self.subject.PrepFileSpaces("name", [""], [""], "foo", 1, 1)

        self.assertIn("--gp_contentid=", prep_file_spaces.cmdStr)
        self.assertIn("--gp_num_contents_in_cluster=", prep_file_spaces.cmdStr)
        self.assertIn("--gp_dbid=", prep_file_spaces.cmdStr)


    # @patch('gpexpand.PgControlData.return_value.get_value', side_effect=[1, 1, 0])
    def test_validate_heap_checksums_aborts_when_cluster_inconsistent(self):
        self.options.filename = '/tmp/doesnotexist' # Replacement of the sys.argv

        self.mock_heap_checksum.return_value.get_segments_checksum_settings.return_value = ([1], [0])
        self.mock_heap_checksum.return_value.are_segments_consistent.return_value = False
        self.primary0.heap_checksum = 1
        self.primary1.heap_checksum = 0
        self.master.heap_checksum = '1'
        self.mock_heap_checksum.return_value.check_segment_consistency.return_value = (
            [self.primary0], [self.primary1], self.master.heap_checksum)

        with self.assertRaises(SystemExit):
            self.subject.main(self.options, self.args, self.parser)

        self.subject.logger.fatal.assert_any_call("Cluster heap checksum setting differences reported")
        self.subject.logger.fatal.assert_any_call("Heap checksum settings on 1 of 2 segment instances "
                                                          "do not match master <<<<<<<<")
        self.subject.logger.error.assert_called_with("gpexpand failed: Segments have heap_checksum set "
                                                             "inconsistently to master \n\nExiting...")

    @patch('gpexpand.FileDirExists.return_value.filedir_exists', return_value=True)
    @patch('gpexpand.FileDirExists', return_value=Mock())
    # @patch('gpexpand.HeapChecksum.PgControlData.return_value.get_value', side_effect=[1, 1, 1])
    def test_validate_heap_checksums_completes_when_cluster_consistent(self, mock1, mock2):
        """
        If all the segment checksums match the checksum at the master, then the cluster is consistent.
        This is essentially making sure that the validate_heap_checksums() internal method has not detected any
        inconisistency.  However, since we are mocking all the dependencies, the actual message from gpexpand would be a
        failure from the method after validate_heap_checksums().
        """
        self.options.filename = '/tmp/doesnotexist' # Replacement of the sys.argv
        with self.assertRaises(SystemExit):
            self.subject.main(self.options, self.args, self.parser)
        self.subject.logger.info.assert_any_call("Heap checksum setting consistent across cluster")
        self.subject.logger.error.assert_called_with("gpexpand failed: Invalid input file: No expansion "
                                                                  "segments defined \n\nExiting...")

    def createGpArrayWith2Primary2Mirrors(self):
        self.master = Segment.initFromString(
            "1|-1|p|p|s|u|mdw|mdw|5432|None|/data/master||/data/master/base/10899,/data/master/base/1,/data/master/base/10898,/data/master/base/25780,/data/master/base/34782")
        self.primary0 = Segment.initFromString(
            "2|0|p|p|s|u|aspen|sdw1|40000|41000|/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/qddir/demoDataDir-1||/data/primary0/base/10899,/data/primary0/base/1,/data/primary0/base/10898,/data/primary0/base/25780,/data/primary0/base/34782")
        self.primary1 = Segment.initFromString(
            "3|1|p|p|s|u|sdw2|sdw2|40001|41001|/data/primary1||/data/primary1/base/10899,/data/primary1/base/1,/data/primary1/base/10898,/data/primary1/base/25780,/data/primary1/base/34782")
        self.mirror0 = Segment.initFromString(
            "4|0|m|m|s|u|sdw2|sdw2|50000|51000|/data/mirror0||/data/mirror0/base/10899,/data/mirror0/base/1,/data/mirror0/base/10898,/data/mirror0/base/25780,/data/mirror0/base/34782")
        self.mirror1 = Segment.initFromString(
            "5|1|m|m|s|u|sdw1|sdw1|50001|51001|/data/mirror1||/data/mirror1/base/10899,/data/mirror1/base/1,/data/mirror1/base/10898,/data/mirror1/base/25780,/data/mirror1/base/34782")
        return GpArray([self.master, self.primary0, self.primary1, self.mirror0, self.mirror1])

if __name__ == '__main__':
    run_tests()
