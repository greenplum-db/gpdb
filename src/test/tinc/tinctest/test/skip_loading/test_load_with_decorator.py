from tinctest.test.skip_loading import SampleTINCTestModelWithSkipDecorator

class SampleTINCTestsWithSkipDecorator(SampleTINCTestModelWithSkipDecorator):
    """
    Running this through discover should load test_01 once through the subclass
    and skip load through the base class
    """
    
    def test_02(self):
        pass
