from contextlib import closing
from datetime import datetime
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator

import inspect
import os

import unittest2 as unittest

import tinctest

from tincmmgr.parser import TINCLogFileParser
from tincmmgr.config import TINCMMLogConfig
from tincmmgr.main import TINCMMProgram
from tinctest.loader import TINCTestLoader

class TINCLogFileParserTests(unittest.TestCase):

    def test_log_file_parser(self):
        """
        Test sanity construction of log file parser
        """
        pwd = os.path.dirname(inspect.getfile(self.__class__))
        test_directory = os.path.join(pwd, 'logtests')

        #TODO: Get the latest cew log file in the log directory
        # to test log file parser against changes in logformat
        # Using a checked-in log file as it is difficult to get hold of
        # where the logs will be generated when the tests are run
        log_file = max(sorted([f for f in os.listdir(test_directory) if f.endswith('_cew.log')]))
        log_file_path = os.path.join(test_directory, log_file)
        log_parser = TINCLogFileParser(log_file_path)
        # log_parser should have 12 tests in it's dict. 6 tests each from test_sample1 and test_sample2 in logtests
        self.assertEquals(len(log_parser.dict_of_test_tuples), 12)

        # Test bucket construction
        log_config = TINCMMLogConfig()
        log_config.bucket_distribution = [30, 30, 40]

        with open(os.path.join(os.getcwd(), 'tincmm.log'), 'w') as f:
            test_buckets = TINCMMProgram._construct_test_suite_buckets(log_parser, log_config, f)
            self.assertEquals(len(test_buckets), 3)
            self.assertEquals(len(test_buckets[0]), 0)
            self.assertEquals(len(test_buckets[1]), 4)
            self.assertEquals(len(test_buckets[2]), 8)
        
        




        
        

        
