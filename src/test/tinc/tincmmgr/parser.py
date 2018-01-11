"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from collections import namedtuple
import sys
import re

import tinctest
from tinctest import TINCTestSuite, TINCTestLoader

class TINCLogFileParser(object):

    test_tuple = namedtuple('test_tuple', 'result duration test_case_object')
    def __init__(self, logfile):
        """
        The constructor requires a TINC log file to parse and the query handler to apply.
        """
        self.logfile = logfile

        # Key is test name, Value is test_tuple
        self.dict_of_test_tuples = {}
        self._populate_dict_of_test_tuples()
    
    def _populate_dict_of_test_tuples(self):
        """
        This hidden method will parse through the log file and find all the tests.
        All the information will be populated in self.dict_of_test_tuples
        """
        # All completed test cases will have the following line:
        # 20140417:08:34:13::[STATUS]::__init__.py:115::pid27994::MainThread:: Finished test: tinctest.test.sample.TINCTestCaseWithDataProviderComplicated.test_with_data_provider_complicated_type1_type6_type3 Result: PASS Duration: 0.39 ms Message: NONE
        re_compiled_object = re.compile(r"Finished test: (.*) Result: (.*) Duration: (.*) ms ")
        
        # We want to gather all of the tests in dict_of_test_tuples
        
        try:
            file_object = open(self.logfile, 'r')
            file_object.close()
        except:
            raise TINCLogFileParserException("Cannot open file %s" % self.logfile)

        tinctest.logger.debug("Parsing file %s" %self.logfile)
        with open(self.logfile, 'r') as file_object:
            for line in file_object:
                line = line.strip()
                if line.find('[STATUS]') == -1:
                    continue
                re_result_object = re_compiled_object.search(line)
                if re_result_object:
                    test_name = re_result_object.group(1)
                    test_result = re_result_object.group(2)
                    test_duration = re_result_object.group(3)
                    test_case_object = TINCTestLoader.findTINCTestFromName(test_name)
                    self.dict_of_test_tuples[test_name] = TINCLogFileParser.test_tuple(result=test_result, duration=test_duration, test_case_object=test_case_object)
