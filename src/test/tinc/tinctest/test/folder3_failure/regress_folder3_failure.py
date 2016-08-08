import tinctest

class MockTINCRegressCaseForLoaderPass(tinctest.TINCTestCase):
    def test_0(self):
        pass
    def test_1(self):
        pass

class MockTINCRegressCaseForLoaderFailure(tinctest.TINCTestCase):
    def test_0(self):
        pass
    def test_1(self):
        """
        @product_version kwhat?
        """
        pass
