#!/usr/bin/env python

import os
import unittest2 as unittest

from gppylib import gparray
from gppylib.commands.base import CommandResult
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.programs.clsRecoverSegment import GpRecoverSegmentProgram
from mock import MagicMock, Mock, mock_open, patch

class GpRecoverSegmentProgramTestCase(unittest.TestCase):

    def _get_mock_segment(self, name, port, address, datadir):
        m = Mock()
        m.getSegmentHostName.return_value = name
        m.getSegmentAddress.return_value = address 
        m.getSegmentPort.return_value = port
        m.getSegmentDatadirectory.return_value = datadir
        return m

    @patch('gppylib.programs.clsRecoverSegment.GpRecoverSegmentProgram._get_dblist', return_value=[['template1']])
    def test_check_persistent_tables(self, mock1):
        options = Mock()
        segments = [self._get_mock_segment('seg1', '1234', 'seg1', '/tmp/seg1'), self._get_mock_segment('seg2', '2345', 'seg2', '/tmp/seg2')]
        mock_pool = Mock()
        m1 = Mock()
        m1.get_results.return_value = ''
        m2 = Mock()
        m2.get_results.return_value = ''
        mock_pool.getCompletedItems.return_value = [m1, m2]
        gprecover_prog = GpRecoverSegmentProgram(options)
        gprecover_prog._GpRecoverSegmentProgram__pool = mock_pool 
        gprecover_prog._check_persistent_tables(segments)

    @patch('gppylib.programs.clsRecoverSegment.GpRecoverSegmentProgram._get_dblist', return_value=[['template1']])
    def test_check_persistent_tables_no_segments(self, mock1):
        options = Mock()
        segments = []
        mock_pool = Mock() 
        mock_pool.getCompletedItems.return_value = []
        gprecover_prog = GpRecoverSegmentProgram(options)
        gprecover_prog._GpRecoverSegmentProgram__pool = mock_pool 
        gprecover_prog._check_persistent_tables(segments)

    @patch('gppylib.programs.clsRecoverSegment.GpRecoverSegmentProgram._get_dblist', return_value=[['template1']])
    def test_check_persistent_tables_error(self, mock1):
        options = Mock()
        segments = [self._get_mock_segment('seg1', '1234', 'seg1', '/tmp/seg1'), self._get_mock_segment('seg2', '2345', 'seg2', '/tmp/seg2')]
        mock_pool = Mock()
        m1 = Mock()
        m1.get_results.return_value = ['sdfsdf']
        m2 = Mock()
        m2.get_results.return_value = ['asdfas']
        mock_pool.getCompletedItems.return_value = [m1, m2]
        gprecover_prog = GpRecoverSegmentProgram(options)
        gprecover_prog._GpRecoverSegmentProgram__pool = mock_pool 
        with self.assertRaisesRegexp(Exception, 'Please fix the persistent tables issue'):
            gprecover_prog._check_persistent_tables(segments)

    @patch('gppylib.programs.clsRecoverSegment.GpRecoverSegmentProgram._check_segment_state_for_connection')
    @patch('time.sleep')
    def test_check_database_connection(self, mock1, mock2):
        options = Mock()
        confProvider = Mock()
        gprecover_prog = GpRecoverSegmentProgram(options)
        self.assertTrue(gprecover_prog._check_database_connection(confProvider))

    @patch('gppylib.programs.clsRecoverSegment.GpRecoverSegmentProgram._check_segment_state_for_connection', side_effect=[Exception('Error')] * 5)
    @patch('time.sleep')
    def test_check_database_connection_exceed_max_retries(self, mock1, mock2):
        options = Mock()
        confProvider = Mock()
        gprecover_prog = GpRecoverSegmentProgram(options)
        self.assertFalse(gprecover_prog._check_database_connection(confProvider))

    def test_check_segment_state(self):
        options = Mock()
        mock_pool = Mock()
        m1 = Mock()
        m1.get_results.return_value = CommandResult(1, 'Failed to connect', '', False, True)
        m2 = Mock()
        m2.get_results.return_value = CommandResult(0, 'segmentState: Ready', '', False, True)
        mock_pool.getCompletedItems.return_value = []
        confProvider = Mock()
        m1 = Mock()
        m1.getSegmentHostName.return_value = 'foo1'
        m1.isSegmentUp.return_value = True
        m1.isSegmentMaster.return_value = False
        m2 = Mock()
        m2.getSegmentHostName.return_value = 'foo2'
        m2.isSegmentUp.return_value = True
        m2.isSegmentMaster.return_value = False
        gparray = Mock()
        gparray.getDbList.return_value = [m1, m2]
        confProvider.loadSystemConfig.return_value = gparray 
        gprecover_prog = GpRecoverSegmentProgram(options)
        gprecover_prog._GpRecoverSegmentProgram__pool = mock_pool 
        gprecover_prog._check_segment_state_for_connection(confProvider)

    def test_check_segment_state_with_segment_not_ready(self):
        options = Mock()
        m1 = Mock()
        m1.get_results.return_value = CommandResult(0, 'Failed to connect', '', False, True)
        m2 = Mock()
        m2.get_results.return_value = CommandResult(0, 'segmentState: Not Ready', '', False, True)
        mock_pool = Mock()
        mock_pool.getCompletedItems.return_value = [m1, m2]
        m1 = Mock()
        m1.getSegmentHostName.return_value = 'foo1'
        m1.isSegmentUp.return_value = True
        m1.isSegmentMaster.return_value = False
        m2 = Mock()
        m2.getSegmentHostName.return_value = 'foo2'
        m2.isSegmentUp.return_value = True
        m2.isSegmentMaster.return_value = False
        gparray = Mock()
        gparray.getDbList.return_value = [m1, m2]
        confProvider = Mock()
        confProvider.loadSystemConfig.return_value = gparray 
        gprecover_prog = GpRecoverSegmentProgram(options)
        gprecover_prog._GpRecoverSegmentProgram__pool = mock_pool 
        with self.assertRaisesRegexp(Exception, 'Not ready to connect to database'):
            gprecover_prog._check_segment_state_for_connection(confProvider)

    @patch('gppylib.commands.base.Command.get_results')
    def test_check_segment_state_ready_for_recovery_ignores_initial_stdout_warnings(self, mock_results):
        options = Mock()
        gprecover_prog = GpRecoverSegmentProgram(options)
        gprecover_prog.logger.info = Mock()
        mock_results.return_value = CommandResult(0, '', 'Warning: Permanently added "a4eb06fc188f,172.17.0.2" (RSA) to the list of \nmode: PrimarySegment\nsegmentState: ChangeTrackingDisabled\ndataState: InChangeTracking\n', False, True)
        segment_mock = Mock()
        segment_mock.isSegmentQD.return_value = False
        segment_mock.isSegmentModeInChangeLogging.return_value = True
        segment_mock.getSegmentHostName.return_value = 'foo1'
        segment_mock.getSegmentDataDirectory.return_value = 'bar'
        segment_mock.getSegmentPort.return_value = 5555
        segment_mock.getSegmentDbId.return_value = 2
        segment_mock.getSegmentRole.return_value = 'p'
        segment_mock.getSegmentMode.return_value = 'c'

        segmentList = [segment_mock]
        dbsMap = {2:segment_mock}

        mock_pool= Mock()
        mock_pool.addCommand = Mock()
        mock_pool.join = Mock()
        gprecover_prog._GpRecoverSegmentProgram__pool = mock_pool

        segmentStates = gprecover_prog.check_segment_state_ready_for_recovery(segmentList, dbsMap)
        self.assertEquals(segmentStates, {2: 'ChangeTrackingDisabled'})
        gprecover_prog.logger.info.assert_called_once_with('Warning: Permanently added "a4eb06fc188f,172.17.0.2" (RSA) to the list of ')

    @patch('gppylib.commands.base.Command.get_results')
    def test_check_segment_state_ready_for_recovery_with_segment_in_change_tracking_disabled(self, mock_results):
        options = Mock()
        mock_results.return_value = CommandResult(0, '', 'mode: PrimarySegment\nsegmentState: ChangeTrackingDisabled\ndataState: InChangeTracking\n', False, True)
        m2 = Mock()
        m2.isSegmentQD.return_value = False
        m2.isSegmentModeInChangeLogging.return_value = True
        m2.getSegmentHostName.return_value = 'foo1'
        m2.getSegmentDataDirectory.return_value = 'bar'
        m2.getSegmentPort.return_value = 5555
        m2.getSegmentDbId.return_value = 2
        m2.getSegmentRole.return_value = 'p'
        m2.getSegmentMode.return_value = 'c'

        segmentList = [m2]
        dbsMap = {2:m2}

        gprecover_prog = GpRecoverSegmentProgram(options)
        mock_pool= Mock()
        mock_pool.addCommand = Mock()
        mock_pool.join = Mock()
        gprecover_prog._GpRecoverSegmentProgram__pool = mock_pool

        segmentStates = gprecover_prog.check_segment_state_ready_for_recovery(segmentList, dbsMap)
        self.assertEquals(segmentStates, {2: 'ChangeTrackingDisabled'})

    def test_output_segments_in_change_tracking_disabled_should_print_failed_segments(self):
        segs_in_change_tracking_disabled = {2:'ChangeTrackingDisabled', 4:'ChangeTrackingDisabled'}
        options = Mock()
        gprecover_prog = GpRecoverSegmentProgram(options)
        gprecover_prog.logger.warn = Mock()
        gprecover_prog._output_segments_in_change_tracking_disabled(segs_in_change_tracking_disabled)
        gprecover_prog.logger.warn.assert_called_once_with('Segments with dbid 2 ,4 in change tracking disabled state, need to run recoverseg with -F option.')

    def test_check_segment_change_tracking_disabled_state_return_true(self):
        options = Mock()
        gprecover_prog = GpRecoverSegmentProgram(options)
        res = gprecover_prog.check_segment_change_tracking_disabled_state(gparray.SEGMENT_STATE_CHANGE_TRACKING_DISABLED)
        self.assertEquals(res, True)

    def test_check_segment_change_tracking_disabled_state_return_false(self):
        options = Mock()
        gprecover_prog = GpRecoverSegmentProgram(options)
        res = gprecover_prog.check_segment_change_tracking_disabled_state(gparray.SEGMENT_STATE_READY)
        self.assertEquals(res, False)
