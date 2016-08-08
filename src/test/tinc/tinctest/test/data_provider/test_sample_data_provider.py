import unittest2 as unittest

import tinctest

from tinctest import TINCTestCase
from test_tinc_sample import test_data_provider

@unittest.skip('mock test case for discovery tests')
class TINCTestCaseWithDataProvider(TINCTestCase):
    """
    
    @maintainer balasr3
    """
    def test_1_sample(self):
        """
        @data_provider test1
        """
        print "Running this test with test data %s" %self.test_data[0]
        print "Test data is %s" %self.test_data[1]

