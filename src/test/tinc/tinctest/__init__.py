"""
@newfield metadata: This class understands the following metadata
"""

from tinctest.case import TINCTestCase
from tinctest.case import dataProvider
from tinctest.case import skipLoading
from tinctest.suite import TINCTestSuite
from tinctest.loader import TINCTestLoader, TINCTestLoaderReverse, TINCTestLoaderRandomized
from tinctest.runner import TINCTestRunner, TINCTextTestResult, TINCTestResultSet
from tinctest.main import TINCTestProgram
from tinctest.lib import get_logger
from tinctest.main import TINCException

import os

#: The default test loader used by tinc that loads tests in the default test order
default_test_loader = TINCTestLoader()

#: A reverse test loader that can be used to load tests in a reverse order
reverse_test_loader = TINCTestLoaderReverse()

#: A randomized test loader that can be used to load tests in a randomized order
randomized_test_loader = TINCTestLoaderRandomized()

#: The default tinc logger that should be used by all the tinc components for logging
logger = get_logger()



##### constants used across all of TINC ####

# for any test for which SkipTest has to be handled separately
# use this message prefix. (Example, tests skipped within
# ConcurrencyTestCase)
_SKIP_TEST_MSG_PREFIX = 'Skipped test within TINC: '
