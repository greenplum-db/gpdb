from tinctest.case import TINCTestCase
import unittest2 as unittest

@unittest.skip('mock')
class MockTINCTestCase(TINCTestCase):

    def test_01(self):
        self.assertTrue(True)

    def test_02(self):
        self.assertTrue(True)
