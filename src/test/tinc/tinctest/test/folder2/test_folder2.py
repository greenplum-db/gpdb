import unittest2 as unittest

import tinctest

from tinctest import TINCTestCase

@unittest.skip('mock test case for discovery tests')
class TINCTestCase2(TINCTestCase):
    """
    
    @maintainer balasr3
    """
    def test_folder2_sample(self):
        pass
