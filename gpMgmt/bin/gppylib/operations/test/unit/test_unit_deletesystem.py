#!/usr/bin/env python

import unittest
from gppylib.operations.deletesystem import validate_pgport
from mock import patch, MagicMock, Mock

class GpDeletesystemTestCase(unittest.TestCase):
    @patch('os.getenv', return_value=None)
    @patch('gppylib.operations.deletesystem.get_masterport', return_value=12345)
    def test_validate_pgport_with_no_pgport_env(self, mock1, mock2):
        self.assertEqual(12345, validate_pgport('/foo'))        

    @patch('os.getenv', return_value='2345')
    @patch('gppylib.operations.deletesystem.get_masterport', return_value=12345)
    def test_validate_pgport_with_non_matching_pgport(self, mock1, mock2):
        master_data_dir = '/foo'
        with self.assertRaisesRegexp(Exception, 'PGPORT value in %s/postgresql.conf does not match PGPORT environment variable' % master_data_dir):
            validate_pgport(master_data_dir)        
    
    @patch('os.getenv', return_value='12345')
    @patch('gppylib.operations.deletesystem.get_masterport', return_value=12345)
    def test_validate_pgport_with_matching_pgport(self, mock1, mock2):
        self.assertEqual(12345, validate_pgport('/foo'))        
