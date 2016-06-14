#!/usr/bin/env python

import os, sys
import unittest2 as unittest
from gppylib import gplog
from gppylib.commands.base import CommandResult
from gpsegstop import SegStop, SegStopStatus
from mock import patch

logger = gplog.get_unittest_logger()

class SegStopTestCase(unittest.TestCase):

    def setUp(self):
        self.segstop = SegStop(name='Segment Stop',
                               db=None,
                               mode=None,
                               timeout=None)

    def test_get_datadir_and_port(self):
        self.segstop.db = '/tmp/gpseg0:1234' 
        self.assertEqual(['/tmp/gpseg0', '1234'], self.segstop.get_datadir_and_port())

    def test_get_datadir_and_port_empty_port(self):
        self.segstop.db = '/tmp/gpseg0'
        self.assertEqual(['/tmp/gpseg0'], self.segstop.get_datadir_and_port())

    def test_get_datadir_and_port_empty_datadir(self):
        self.segstop.db = '1234'
        self.assertEqual(['1234'], self.segstop.get_datadir_and_port())

    def test_get_datadir_and_port_empty_string(self):
        self.segstop.db = ''
        self.assertEqual([''], self.segstop.get_datadir_and_port())

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, '', '',True, False))
    @patch('gppylib.commands.gp.SegmentIsShutDown.is_shutdown', return_value=True)
    @patch('gpsegstop.unix.kill_9_segment_processes')
    @patch('gpsegstop.pg.ReadPostmasterTempFile.getResults', return_value=(True, 1234, '/tmp/gpseg0'))
    def test_run(self, mock1, mock2, mock3, mock4, mock5):
        self.segstop.db = '/tmp/gpseg0:1234'
        self.segstop.mode = 'smart'
        self.segstop.timeout = '10'
        expected = SegStopStatus('/tmp/gpseg0', True, 'Shutdown Succeeded')
        result = self.segstop.run()
        self.assertEqual(str(expected), str(result))

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(1, '', '',True, False))
    @patch('gpsegstop.pg.ReadPostmasterTempFile.getResults', return_value=(True, 1234, '/tmp/gpseg0'))
    @patch('gpsegstop.unix.kill_sequence')
    @patch('gpsegstop.unix.kill_9_segment_processes')
    def test_run_with_error(self, mock1, mock2, mock3, mock4, mock5):
        self.segstop.db = '/tmp/gpseg0:1234'
        self.segstop.mode = 'smart'
        self.segstop.timeout = '10'
        expected = SegStopStatus('/tmp/gpseg0', True, 'Forceful termination success')
        result = self.segstop.run()
        self.assertIn(str(expected), str(result))
 
    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, '', '',True, False))
    @patch('gppylib.commands.gp.SegmentIsShutDown.is_shutdown', return_value=False)
    @patch('gpsegstop.pg.ReadPostmasterTempFile.getResults', return_value=(True, 1234, '/tmp/gpseg0'))
    @patch('gpsegstop.unix.kill_sequence')
    @patch('gpsegstop.unix.kill_9_segment_processes')
    def test_run_with_pg_controldata_error(self, mock1, mock2, mock3, mock4, mock5, mock6):
        self.segstop.db = '/tmp/gpseg0:1234'
        self.segstop.mode = 'smart'
        self.segstop.timeout = '10'
        expected = SegStopStatus('/tmp/gpseg0', True, 'Forceful termination success')
        result = self.segstop.run()
        self.assertIn(str(expected), str(result))

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, '', '',True, False))
    @patch('gppylib.commands.gp.SegmentIsShutDown.is_shutdown', return_value=False)
    @patch('gpsegstop.pg.ReadPostmasterTempFile.getResults', return_value=(True, 1234, '/tmp/gpseg0'))
    @patch('gpsegstop.unix.kill_9_segment_processes')
    def test_run_with_pg_controldata_error_in_immediate_mode(self, mock1, mock2, mock3, mock4, mock5):
        self.segstop.db = '/tmp/gpseg0:1234'
        self.segstop.mode = 'immediate'
        self.segstop.timeout = '10'
        expected = SegStopStatus('/tmp/gpseg0', True, 'Shutdown Immediate')
        result = self.segstop.run()
        self.assertEqual(str(expected), str(result))

    @patch('gppylib.commands.base.Command.run')
    @patch('gppylib.commands.base.Command.get_results', return_value=CommandResult(0, '', '',True, False))
    @patch('gppylib.commands.gp.SegmentIsShutDown.is_shutdown', return_value=False)
    @patch('gpsegstop.pg.ReadPostmasterTempFile.getResults', return_value=(True, 1234, '/tmp/gpseg0'))
    @patch('gpsegstop.unix.kill_sequence')
    @patch('gpsegstop.unix.check_pid', return_value=False)
    @patch('gpsegstop.unix.kill_9_segment_processes')
    def test_run_with_kill_error(self, mock1, mock2, mock3, mock4, mock5, mock6, mock7):
        self.segstop.db = '/tmp/gpseg0:1234'
        self.segstop.mode = 'immediate'
        self.segstop.timeout = '10'
        expected = SegStopStatus('/tmp/gpseg0', True, 'Shutdown Immediate')
        result = self.segstop.run()
        self.assertEqual(str(expected), str(result))

#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    unittest.main()
