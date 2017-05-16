#!/usr/bin/env python

import os
import unittest

from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.operations.initstandby import get_standby_pg_hba_info, update_pg_hba, update_pg_hba_conf_on_segments
from mock import MagicMock, Mock, mock_open, patch

class InitStandbyTestCase(unittest.TestCase):

    @patch('gppylib.operations.initstandby.unix.InterfaceAddrs.remote', return_value=['192.168.2.1', '192.168.1.1'])
    @patch('gppylib.operations.initstandby.unix.UserId.local', return_value='all')
    def test_get_standby_pg_hba_info(self, m1, m2):
        expected = '# standby master host ip addresses\nhost\tall\tall\t192.168.2.1/32\ttrust\nhost\tall\tall\t192.168.1.1/32\ttrust\n'
        self.assertEqual(expected, get_standby_pg_hba_info('host'))

    def test_update_pg_hba(self):
        file_contents = """some pg hba data here\n"""
        pg_hba_info = '# standby master host ip addresses\nhost\tall\tall\t192.168.2.1/32\ttrust\nhost\tall\tall\t192.168.1.1/32\ttrust\n'
        data_dirs = ['/tmp/d1', '/tmp/d2']
        expected = [file_contents + pg_hba_info, file_contents + pg_hba_info]
        m = MagicMock()
        m.return_value.__enter__.return_value.read.side_effect = [file_contents, file_contents]
        with patch('__builtin__.open', m, create=True):
            self.assertEqual(expected, update_pg_hba(pg_hba_info, data_dirs))

    def test_update_pg_hba_duplicate(self):
        file_contents = """some pg hba data here\n"""
        duplicate_entry = """# standby master host ip addresses\nhost\tall\tall\t192.168.2.1/32\ttrust\nhost\tall\tall\t192.168.1.1/32\ttrust\n"""
        pg_hba_info = '# standby master host ip addresses\nhost\tall\tall\t192.168.2.1/32\ttrust\nhost\tall\tall\t192.168.1.1/32\ttrust\n'
        data_dirs = ['/tmp/d1', '/tmp/d2']
        expected = [file_contents + pg_hba_info]
        m = MagicMock()
        m.return_value.__enter__.return_value.read.side_effect = [file_contents, file_contents + duplicate_entry]
        with patch('__builtin__.open', m, create=True):
            res = update_pg_hba(pg_hba_info, data_dirs)
            self.assertEqual(expected, res) 

    @patch('gppylib.operations.initstandby.WorkerPool')
    @patch('gppylib.operations.initstandby.get_standby_pg_hba_info', return_value='standby ip')
    def test_update_pg_hba_on_segments(self, m1, m2):
        mock_segs = []
        for i in range(6):
            m = Mock()
            m.getSegmentContentId = Mock()
            m.getSegmentContentId.return_value = (i % 3) + 1
            m.getSegmentDataDirectory.return_value = '/tmp/d%d' % i
            mock_segs.append(m)
        gparray = Mock()
        gparray.getDbList = Mock()
        gparray.getDbList.return_value = mock_segs 
        update_pg_hba_conf_on_segments(gparray, 'standby_host') 
