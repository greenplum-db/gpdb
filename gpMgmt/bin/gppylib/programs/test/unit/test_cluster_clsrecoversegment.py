#!/usr/bin/env python

from gparray import Segment, GpArray
from gppylib import gparray
from gppylib.commands.base import CommandResult, WorkerPool, Command
from gppylib.programs.clsRecoverSegment import GpRecoverSegmentProgram
from mock import Mock, patch, MagicMock

from system.configurationInterface import GpConfigurationProvider
from test.unit.gp_unittest import GpTestCase


class GpRecoverSegmentProgramTestCase(GpTestCase):
    def setUp(self):
        raw_options = GpRecoverSegmentProgram.createParser()
        (options, _) = raw_options.parse_args(args=[])
        options.spareDataDirectoryFile = None
        options.newRecoverHosts = None
        self.subject = GpRecoverSegmentProgram(options)

        self.execSqlResult = Mock(spec=['fetchall'])

        self.gp_env = Mock()
        GpMasterEnvironmentMock = Mock(return_value=self.gp_env)

        self.gparray = Mock(spec=GpArray)
        self.gparray.getDbList.return_value = self._segments_mock()

        configProviderMock = Mock(spec=GpConfigurationProvider)
        configProviderMock.initializeProvider.return_value = configProviderMock
        configProviderMock.loadSystemConfig.return_value = self.gparray

        self.getConfigProviderFunctionMock = Mock(GpConfigurationProvider)
        self.getConfigProviderFunctionMock.return_value = configProviderMock

        self.subject.logger = Mock()

        self.worker_pool = Mock(spec=WorkerPool, return_value=None)
        self.worker_pool.getCompletedItems.return_value = []
        self.worker_pool.logger = self.subject.logger
        self.worker_pool.addCommand.return_value = None
        self.pool_completed = []


        self.apply_patches([
            patch("gppylib.db.dbconn.connect"),
            patch("gppylib.db.dbconn.DbURL"),
            patch("gppylib.db.dbconn.execSQL", return_value=self.execSqlResult),
            patch('time.sleep'),

            patch('gppylib.programs.clsRecoverSegment.GpMasterEnvironment', GpMasterEnvironmentMock),
            # patch('gppylib.system.environment.GpMasterEnvironment.__init__', self.gp_env),
            # patch('gppylib.system.environment.GpMasterEnvironment.getMasterPort'),
            patch('gppylib.system.faultProberInterface.getFaultProber'),
            patch('gppylib.system.configurationInterface.getConfigurationProvider', self.getConfigProviderFunctionMock),

            patch('gppylib.commands.base.WorkerPool.__init__', self.worker_pool),
            patch('gppylib.commands.base.WorkerPool.getCompletedItems', return_value=self.pool_completed),
            patch('gppylib.commands.base.WorkerPool.addCommand'),
            patch('gppylib.commands.base.WorkerPool.join'),
        ])

        # tests make use of a workaround to access a python attribute that is normally
        # name mangled when specified with a "__" prefix. That workaround is to use _<class>__<attribute>
        # such as  self.subject._GpRecoverSegmentProgram__pool = mock_pool
        self.subject._GpRecoverSegmentProgram__pool = self.worker_pool

    def test_output_segments_in_change_tracking_disabled_should_print_failed_segments(self):
        segs_in_change_tracking_disabled = {2: 'ChangeTrackingDisabled', 4: 'ChangeTrackingDisabled'}
        self.subject._output_segments_in_change_tracking_disabled(segs_in_change_tracking_disabled)
        self.subject.logger.warn.assert_called_once_with(
            'Segments with dbid 2 ,4 in change tracking disabled state, need to run recoverseg with -F option.')

    def test_check_segment_change_tracking_disabled_state_return_true(self):
        res = self.subject.check_segment_change_tracking_disabled_state(
            gparray.SEGMENT_STATE_CHANGE_TRACKING_DISABLED)
        self.assertEquals(res, True)

    def test_check_segment_change_tracking_disabled_state_return_false(self):
        res = self.subject.check_segment_change_tracking_disabled_state(gparray.SEGMENT_STATE_READY)
        self.assertEquals(res, False)

    def test_output_segments_with_persistent_mirroring_disabled_should_print_failed_segments(self):
        segs_with_persistent_mirroring_disabled = [0, 1]
        self.subject._output_segments_with_persistent_mirroring_disabled(segs_with_persistent_mirroring_disabled)
        self.subject.logger.warn.assert_called_once_with(
            'Segments with dbid 0, 1 not recovered; persistent mirroring state is disabled.')

    def test_output_segments_with_persistent_mirroring_disabled_should_not_print_if_no_segments(self):
        segs_with_persistent_mirroring_disabled = []
        self.subject._output_segments_with_persistent_mirroring_disabled(segs_with_persistent_mirroring_disabled)
        assert not self.subject.logger.warn.called

    def test_is_segment_mirror_state_mismatched_cluster_mirroring_enabled_segment_mirroring_disabled(self):
        self.execSqlResult.fetchall.return_value = [[3], [1]]
        gparray_mock = Mock(spec=GpArray)
        gparray_mock.hasMirrors = True

        segment_mock = Mock(spec=Segment)
        segment_mock.getSegmentContentId.return_value = 0
        mismatched = self.subject.is_segment_mirror_state_mismatched(gparray_mock, segment_mock)
        self.assertTrue(mismatched)

    def test_is_segment_mirror_state_mismatched_cluster_and_segments_mirroring_enabled(self):
        self.execSqlResult.fetchall.return_value = [[3]]
        gparray_mock = Mock(spec=GpArray)
        gparray_mock.hasMirrors = True

        segment_mock = Mock(spec=Segment)
        segment_mock.getSegmentContentId.return_value = 0
        mismatched = self.subject.is_segment_mirror_state_mismatched(gparray_mock, segment_mock)
        self.assertFalse(mismatched)

    def test_is_segment_mirror_state_mismatched_cluster_and_segments_mirroring_disabled(self):
        gparray_mock = Mock(spec=GpArray)
        gparray_mock.hasMirrors = False

        segment_mock = Mock(spec=Segment)
        segment_mock.getSegmentDbId.return_value = 0
        mismatched = self.subject.is_segment_mirror_state_mismatched(gparray_mock, segment_mock)
        self.assertFalse(mismatched)

    ############################################################
    # Private
    def _get_mock_segment(self, name, port, address, datadir):
        segment = Mock(spec=Segment)
        segment.getSegmentHostName.return_value = name
        segment.getSegmentAddress.return_value = address
        segment.getSegmentPort.return_value = port
        segment.getSegmentDataDirectory.return_value = datadir
        return segment

    def _get_mock_conf_provider(self, gparray_result=None):
        conf_provider = Mock(spec=GpConfigurationProvider)
        conf_provider.loadSystemConfig.return_value = gparray_result
        return conf_provider

    def _segments_mock(self):
        segment1 = Mock(spec=Segment)
        segment1.getSegmentHostName.return_value = 'foo1'
        segment1.isSegmentUp.return_value = True
        segment1.isSegmentMaster.return_value = False
        segment1.isSegmentStandby.return_value = False
        segment2 = Mock(spec=Segment)
        segment2.getSegmentHostName.return_value = 'foo2'
        segment2.isSegmentUp.return_value = True
        segment2.isSegmentMaster.return_value = False
        segment2.isSegmentStandby.return_value = False
        return [segment1, segment2]

