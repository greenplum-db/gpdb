#!/usr/bin/env python

import os, sys
import unittest2 as unittest
from gppylib import gplog
from gpsegstart import GpSegStart
from mock import patch

logger = gplog.get_unittest_logger()

class GpSegStartTestCase(unittest.TestCase):

    @patch('gpsegstart.GpSegStart.getOverallStatusKeys', return_value=[])
    @patch('gpsegstart.gp.GpVersion.local', return_value=None)
    @patch('gpsegstart.base.WorkerPool')
    def test_check_postmasters_01(self, mk1, mk2, mk3):
        db = '1|1|p|p|s|u|mdw|mdw-1|2000|2001|/data/gpseg-1s||'
        gpseg = GpSegStart([db], None, 'col1:col2:col3', 'quiescent', None, None, None, None, None, None, None)
        result = gpseg.checkPostmasters(False)
        self.assertTrue(result)

    @patch('gpsegstart.GpSegStart.getOverallStatusKeys', return_value=['foo1', 'foo2'])
    @patch('gpsegstart.gp.check_pid', return_value=False)
    @patch('gpsegstart.gp.GpVersion.local', return_value=None)
    @patch('gpsegstart.base.WorkerPool')
    def test_check_postmasters_02(self, mk1, mk2, mk3, mk4):
        db = '1|1|p|p|s|u|mdw|mdw-1|2000|2001|/data/gpseg-1s||'
        gpseg = GpSegStart([db], None, 'col1:col2:col3', 'quiescent', None, None, None, None, None, None, None)
        result = gpseg.checkPostmasters(False)
        self.assertFalse(result)

    @patch('gpsegstart.GpSegStart.getOverallStatusKeys', return_value=['foo1', 'foo2'])
    @patch('gpsegstart.gp.check_pid', side_effect=[False, True])
    @patch('gpsegstart.gp.GpVersion.local', return_value=None)
    @patch('gpsegstart.base.WorkerPool')
    def test_check_postmasters_03(self, mk1, mk2, mk3, mk4):
        db = '1|1|p|p|s|u|mdw|mdw-1|2000|2001|/data/gpseg-1s||'
        gpseg = GpSegStart([db], None, 'col1:col2:col3', 'quiescent', None, None, None, None, None, None, None)
        result = gpseg.checkPostmasters(False)
        self.assertFalse(result)

#------------------------------- Mainline --------------------------------
if __name__ == '__main__':
    unittest.main()
