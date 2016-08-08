import tinctest

class MockTINCTestCaseForLoaderPass(tinctest.TINCTestCase):
    def test_0(self):
        pass
    def test_1(self):
        pass

class MockTINCTestCaseForLoaderFailure(tinctest.TINCTestCase):
    def test_0(self):
        pass
    def test_1(self):
        """
        @product_version kwhat?
        """
        pass
