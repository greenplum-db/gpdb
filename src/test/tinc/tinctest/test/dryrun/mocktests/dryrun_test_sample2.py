import tinctest

from tinctest.case import TINCTestCase

class SampleTINCTests(TINCTestCase):

    @classmethod
    def setUpClass(cls):
        super(SampleTINCTests, cls).setUpClass()
        tinctest.test.dryrun.tests_run_count += 1

    def setUp(self):
        super(SampleTINCTests, self).setUp()
        tinctest.test.dryrun.tests_run_count += 1
        
    def test_01(self):
        """
        @tags hawq
        """
        tinctest.test.dryrun.tests_run_count += 1

    def test_02(self):
        tinctest.test.dryrun.tests_run_count += 1

    def test_03(self):
        """
        @skip just skipping
        """
        tinctest.test.dryrun.tests_run_count += 1

    def tearDown(self):
        tinctest.test.dryrun.tests_run_count += 1
        super(SampleTINCTests, self).tearDown()

    @classmethod
    def tearDownClass(cls):
        tinctest.test.dryrun.tests_run_count += 1
        super(SampleTINCTests, cls).tearDownClass()
        
