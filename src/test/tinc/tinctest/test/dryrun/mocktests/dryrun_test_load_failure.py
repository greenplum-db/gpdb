from tinctest.case import TINCTestCase

class Sample2TINCTests(TINCTestCase):

    @classmethod
    def setUpClass(cls):
        print "SetupClass"

    def setUp(self):
        print "setup"
        
    def test_021(self):
        pass

    def test_022(self):
        """
        @product_version blahblahblah.blah.invalid.product.version
        """
        pass

    def tearDown(self):
        print "teardown"

    @classmethod
    def tearDownClass(cls):
        print "teardown class"
