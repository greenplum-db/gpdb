from mpp.models import MPPTestCase, SQLTestCase

class SampleMPPTests(MPPTestCase):
    """
    @tags class1
    """
    def test_1(self):
        pass
    def test_2(self):
        
        """

        This is a comment
        
        @tags test2

        """

        pass
    def test_3(self):
        '''
        @product_version prod1: 
        '''
        self.assertTrue(True)
        """
        Some large comment...
        That happens to have dead code
        @tags NOOO
        """
        pass
        
class SampleSQLTests(SQLTestCase):
    
    """
    
    Comment here
    
    
    
    Some space above... Another comment...
    
    @tags sql smoke
    
    """
    
    sql_dir = 'sql'
    ans_dir = 'expected'

    def test_explicit_1(self):
        pass
    def test_explicit_2(self):
        """
        @tags explicit
        """
        pass

class SampleTestsNoDocstring(MPPTestCase):
    def test_no_1(self):
        pass
    def test_no_2(self):
        pass
    def test_no_3():
        """
        @tags NOOO
        """
        pass

def stray_def(var1, var2):
    """
    @tags stray_def
    """
    pass

class StrayClass(object):
    """
    Not a test case class
    @tags stray_class
    """
    def test_stray_1(self):
        """
        @tags tag_in_stray_class_method_1
        """
        pass
    def test_stray_2(self):
        """
        @tags tag_in_stray_class_method_1
        """
        pass

class SampleTestsAfterStray(MPPTestCase):
    """
    @tags class_after_stray
    """
    def test_last_1(self):
        """
        @tags test_last_1
        """
        pass
    def test_last_2(self):
        """
        @tags test_last_2
        """
        pass
