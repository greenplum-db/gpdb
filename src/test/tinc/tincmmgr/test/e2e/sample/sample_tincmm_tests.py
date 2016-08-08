import tinctest 

class MockTestCase1 (tinctest .TINCTestCase ):
    """
    @tags functional
    """
    def test_01 (self ):
        print self .name 

    def test_02 (self ):
        print self .name 

    def test_03 (self ):
        print self .name 


class MockTestCase2 (tinctest .TINCTestCase ):
    """
    @tags smoke
    """
    def test_01 (self ):
        print self .name 

    def test_02 (self ):
        print self .name 

    def test_03 (self ):
        print self .name 


class MockTestCase3 (tinctest .TINCTestCase ):

    def test_01 (self ):
        print self .name 

    def test_02 (self ):
        print self .name 

    def test_03 (self ):
        print self .name 


class MockTestCase4 (tinctest .TINCTestCase ):
    def test_01 (self ):
        print self .name 

    def test_02 (self ):
        print self .name 

    def test_03 (self ):
        print self .name 

