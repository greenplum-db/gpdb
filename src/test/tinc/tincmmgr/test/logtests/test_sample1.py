import time 

from tinctest .case import TINCTestCase 

class SampleTINCTestCase1 (TINCTestCase ):
    def test_sleep1 (self ):
        time .sleep (0.5 )

    def test_sleep2 (self ):
        time .sleep (1.0 )

    def test_sleep3 (self ):
        time .sleep (1.5 )

class SampleTINCTestCase2 (TINCTestCase ):
    def test_sleep1 (self ):
        time .sleep (0.5 )

    def test_sleep2 (self ):
        time .sleep (1.0 )

    def test_sleep3 (self ):
        time .sleep (1.5 )

