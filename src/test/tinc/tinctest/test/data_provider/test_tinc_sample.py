import unittest2 as unittest

import tinctest

from tinctest import TINCTestCase

@unittest.skip('mock test case for discovery tests')
class SampleTINCTestCase(TINCTestCase):
    """
    
    @maintainer balasr3
    """
    def test_1_sample(self):
        """
        @created 2013-05-10 12:00:00
        @modified 2013-05-15 12:00:00
        @tags tag1 tag2
        @data_provider test1
        """
        pass

    def test_2_sample(self):
        """
        @created 2013-05-10 12:00:00
        @modified 2013-05-15 12:00:00
        @tags tag3
        """
        self.fail("Failing")

    def test_3_sample(self):
        """
        @created 2013-05-10 12:00:00
        @modified 2013-05-15 12:00:00
        @tags tag1
        @skip Skipping this test
        """
        pass

@unittest.skip('mock test case for discovery tests')
class AnotherSampleTINCTestCase(TINCTestCase):
    """
    
    @maintainer balasr3
    """
    def test_1_sample(self):
        """
        @created 2013-05-10 12:00:00
        @modified 2013-05-15 12:00:00
        @tags tag1 tag2
        """
        pass

    def test_2_sample(self):
        """
        @created 2013-05-10 12:00:00
        @modified 2013-05-15 12:00:00
        @tags tag3
        """
        self.fail("Failing")

    def test_3_sample(self):
        """
        @created 2013-05-10 12:00:00
        @modified 2013-05-15 12:00:00
        @tags tag1
        @skip Skipping this test
        """
        pass

@unittest.skip('mock test case for discovery tests')
class SampleTINCDataProviderTestCase(TINCTestCase):
    """
    
    @maintainer balasr3
    """
    def test_1_sample(self):
        """
        @created 2013-05-10 12:00:00
        @modified 2013-05-15 12:00:00
        @tags tag1 tag2
        @data_provider test1
        """
        self.assertTrue(False)


@tinctest.dataProvider('test1')
def test_data_provider():
    data = {'type1': ['int', 'int2'], 'type2': ['varchar']}
    return data
    
