#!/usr/bin/env python3

import os
import unittest

from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.operations.initstandby import create_standby_pg_hba_entries, update_pg_hba
from gppylib.operations.update_pg_hba_on_segments import update_pg_hba_on_segments_for_standby
from mock import MagicMock, Mock, mock_open, patch

class InitStandbyTestCase(unittest.TestCase):

    @patch('gppylib.operations.initstandby.gp.IfAddrs.list_addrs', return_value=['192.168.2.1', '192.168.1.1'])
    @patch('gppylib.operations.initstandby.unix.UserId.local', return_value='all')
    def test_create_standby_pg_hba_entries(self, m1, m2):
        expected = '# standby coordinator host ip addresses\nhost\tall\tall\t192.168.2.1/32\ttrust\nhost\tall\tall\t192.168.1.1/32\ttrust\n'
        entries = create_standby_pg_hba_entries('host')
        standby_pg_hba_entries = ''.join(entries)
        self.assertEqual(expected, standby_pg_hba_entries)

    def test_update_pg_hba(self):
        file_contents = """some pg hba data here\n"""
        pg_hba_info = '# standby coordinator host ip addresses\nhost\tall\tall\t192.168.2.1/32\ttrust\nhost\tall\tall\t192.168.1.1/32\ttrust\n'
        data_dirs = ['/tmp/d1', '/tmp/d2']
        expected = [file_contents + pg_hba_info, file_contents + pg_hba_info]
        m = MagicMock()
        m.return_value.__enter__.return_value.read.side_effect = [file_contents, file_contents]
        with patch('builtins.open', m, create=True):
            self.assertEqual(expected, update_pg_hba(pg_hba_info, data_dirs))

    def test_update_pg_hba_duplicate(self):
        file_contents = """some pg hba data here\n"""
        duplicate_entry = """# standby coordinator host ip addresses\nhost\tall\tall\t192.168.2.1/32\ttrust\nhost\tall\tall\t192.168.1.1/32\ttrust\n"""
        pg_hba_info = '# standby coordinator host ip addresses\nhost\tall\tall\t192.168.2.1/32\ttrust\nhost\tall\tall\t192.168.1.1/32\ttrust\n'
        data_dirs = ['/tmp/d1', '/tmp/d2']
        expected = [file_contents + pg_hba_info]
        m = MagicMock()
        m.return_value.__enter__.return_value.read.side_effect = [file_contents, file_contents + duplicate_entry]
        with patch('builtins.open', m, create=True):
            res = update_pg_hba(pg_hba_info, data_dirs)
            self.assertEqual(expected, res) 

    @patch('gppylib.operations.initstandby.WorkerPool')
    @patch('gppylib.operations.update_pg_hba_on_segments.update_on_segments')
    @patch('gppylib.operations.update_pg_hba_on_segments.SegUpdateHba')
    @patch('gppylib.operations.update_pg_hba_on_segments.create_standby_pg_hba_entries', return_value=['standby ip'])
    def test_update_pg_hba_on_segments(self, m1, m2, m4, m5):
        mock_segs = []
        batch_size = 1
        for i in range(6):
            m = Mock()
            m.getSegmentContentId = Mock()
            m.getSegmentContentId.return_value = (i % 3) + 1
            m.getSegmentDataDirectory.return_value = '/tmp/d%d' % i
            m.primaryDB.unreachable = False
            mock_segs.append(m)
        gparray = Mock()
        gparray.getSegmentList = Mock()
        gparray.getSegmentList.return_value = mock_segs
        update_pg_hba_on_segments_for_standby(gparray, 'standby_host', False, batch_size)
