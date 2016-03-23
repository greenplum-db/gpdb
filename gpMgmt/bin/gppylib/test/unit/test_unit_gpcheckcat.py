from pygresql import pg
import os
import imp
import glob
gpcheckcat_path = os.path.abspath('gpcheckcat')
gpcheckcat = imp.load_source('gpcheckcat', gpcheckcat_path)
import mock
import gpcheckcat
import unittest2 as unittest
from mock import call
from mock import patch

class MockGlobal(object):
    
    def __init__(self):
        pass


def start_patches(patchers):
    for p in patchers:
        p.start()

def stop_patches(patchers):
    for p in patchers:
        p.stop()


class GpCheckCatTestCase(unittest.TestCase):

    def setUp(self):
        self.patches = []
        self.mock_connect = mock.Mock()
        self.patches.append(mock.patch("gpcheckcat.connect", new = mock.Mock(return_value=self.mock_connect)))
        self.patches.append(mock.patch("gpcheckcat.GV", MockGlobal()))
        start_patches(self.patches)

    def tearDown(self):
        stop_patches(self.patches)

    @patch('gpcheckcat.checkPGNamespace')
    @patch('gpcheckcat.getLeakedSchemas', new = mock.Mock(return_value=["fake_leak_1" ,"fake_leak_2"]))
    def test_dropLeakedSchemas(self, mock1):
        '''testing method dropLeakedSchemas which is supposed to drop any leaked/orphan schemas'''

        gpcheckcat.dropLeakedSchemas(dbname="fake_db") 
        drop_query_expected_list = [call('DROP SCHEMA IF EXISTS \"fake_leak_1\" CASCADE;\n'),
                                    call('DROP SCHEMA IF EXISTS \"fake_leak_2\" CASCADE;\n')]
        self.assertEquals(self.mock_connect.query.call_args_list , drop_query_expected_list)


if __name__ == '__main__':
    (unittest.main(verbosity=2, buffer=True))
