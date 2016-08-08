from tinctest.test.skip_loading import SampleTINCTestModelWithoutSkipDecorator

class SampleTINCTestsWithoutSkipDecorator(SampleTINCTestModelWithoutSkipDecorator):
    """
    Running this through discover should load test_01 twice once through the base
    class and once through the subclass
    """
    
    def test_02(self):
        pass
