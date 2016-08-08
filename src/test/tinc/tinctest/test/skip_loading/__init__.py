import tinctest

class SampleTINCTestModelWithoutSkipDecorator(tinctest.TINCTestCase):

    def test_01(self):
        pass


@tinctest.skipLoading("skipping model loading")
class SampleTINCTestModelWithSkipDecorator(tinctest.TINCTestCase):

    def test_01(self):
        pass
