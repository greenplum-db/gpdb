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

import copy
import inspect
import itertools
import new
import os
import re
import sys

import unittest2 as unittest

from datetime import datetime
from collections import defaultdict

import tinctest

from gppylib import gpversion

from tinctest.runner import TINCTextTestResult

def dataProvider(name):
    """
    The decorator that defines a function
    that can be used as a data provider for a test.
    """
    def decorator(f):
        f.__is_data_provider__ = True
        f.__data_provider_name__ = name
        return f
    return decorator

def skipLoading(msg):
    """
    The decorator that allows to define a class
    as a model class so that no tests will be
    generated out of the model class.
    """
    def decorator(c):
        c.__tinc_skip_loading__ = True
        c.__tinc_skip_loading_msg__ = msg
        # Note: We need the following two variables to see if the decorator
        # is declared at a base class level. For eg: consider SQLTestCase which is
        # a model and SampleSQLTests which could be the actual test class and if the decorator
        # is declared for SQLTestCase, while constructing tests for SampleSQLTests,
        # __tinc_skip_loading__ will still be set as there are no class local variables in python.
        # Therefore we save the file and the class name from which the annotation comes from
        # and we skip loading only if the current class name and file is same as the one at
        # which the decorator was declared. 
        c.__tinc_skip_loading_declared_class__ = c.__name__
        c.__tinc_skip_loading_declared_file__ = sys.modules[c.__module__].__file__
        return c
    return decorator

@skipLoading("Test model. No tests loaded.")
class TINCTestCase(unittest.TestCase):
    """
    This is an abstract class and cannot be instantiated directly. Proper implementations
    of TINCTestCase require at least one test implementation function *and* proper instantatiations
    of such a test case must pass in the name of said function.

    Bear in mind, it's uncommon to define new implementations of TINCTestCase, as these would
    correspond to new types/models of testing. It's even more uncommon to directy instantiate such
    test cases, as test discovery mechanisms should provide that functionality with more robustness.

    @metadata: author: Author of the test case
    @metadata: maintainer: Current maintainer of the test case
    @metadata: description: Testcase description
    @metadata: skip: if set to True, skip running the test case
    @metadata: created: date/time when the test case was created
    @metadata: modified: date/time when the test case was last modified
    @metadata: data_provider: tdb
    @metadata: tags: tags desc (tbd)
    """

    #: Deprecated: Additional test configuration. For eg, global GUCs
    test_config = []

    # Private dict of package names for a module
    _module_package_dict = defaultdict(str)

    # Flag that determines if all tests from this test class are included in the test suite by
    # the loader
    _all_tests_loaded = True

    def __init__(self, methodName, baseline_result = None):
        super(TINCTestCase, self).__init__(methodName)

        #: Name of the test case. Usually <classname>.<testmethodname>
        self.name = None
        #: Completely qualified name of the test case. <package>.<module>.<classname>.<methodname>
        self.full_name = None
        #: Class name to which this test belongs
        self.class_name = None
        #: Name of the package to which this test belongs
        self.package_name = None
        #: Name of the module to which this test belongs
        self.module_name = None
        #: Name of the test method that will be run with this instance
        self.method_name = None
        #: Author of the test case
        self.author = None
        #: Maintainer of the test case
        self.maintainer = None
        #: Description string of the test case
        self.description = None
        #: Date on which this test case was created
        self.created_datetime = None
        #: Last modified date of this test case
        self.modified_datetime = None
        #: All the tags specified for thsi test case
        self.tags = None
        #: Skip message set through metadata for test cases that should be skipped
        self.skip = None
        #: Name of the data provider to be used for this test case
        self.data_provider = None
        self._infer_metadata()
        #: Start time of the test case. Set when the test case is started
        self.start_time = None
        #: End time of the test case. Set when the test case finishes execution irrespective of the result
        self.end_time = None
        #: Duration in ms of the test case execution. Set when the test case finishes execution irrespective of the result
        self.duration = None

        self.baseline_result = baseline_result
        """
        A baseline result object that can be passed on to trending tests. For eg: performance tests
        would require a baseline result object from which it can infer the previous runtime and assert
        pass or fail for the current run. Currently set only for tests run through deprecated tincdb,
        so this may not work anymore and may need to be removed on closer inspection.
        """
        
        #: list of file names that define the artifacts for the test case
        self.test_artifacts = []

        #: The dictionary that will be set for tests with data providers
        self.test_data_dict = {}

        #: 
        self.test_data = None

    def _infer_metadata(self):
        """
        This function sets all the first-class metadata that belongs to TINCTestCase. Every
        subclass is responsible for inferring additional first-class metadata that is required
        by its implementation.
        """
        self._metadata = {}
        for docstring in [self.__doc__, getattr(self, self._testMethodName).__doc__]:
            self._parse_metadata_from_doc(docstring)

        self.name = "%s.%s" % (self.__class__.__name__, self._testMethodName)
        self.full_name = self._get_full_name()
        self.author = self._metadata.get('author', None)
        self.maintainer = self._metadata.get('maintainer', None)
        self.description = self._metadata.get('description', '')
        self.skip = self._metadata.get('skip', None)
        self.created_datetime = datetime.strptime(self._metadata.get('created', '2000-01-01 00:00:00'), '%Y-%m-%d %H:%M:%S')
        self.modified_datetime = datetime.strptime(self._metadata.get('modified', '2000-01-01 00:00:00'), '%Y-%m-%d %H:%M:%S')
        self.data_provider = self._metadata.get('data_provider', None)
        if self._metadata.get('tags', None) == None:
            self.tags = set()
        else:
            self.tags = set(self._metadata['tags'].split())

    def _append_docstrings(self, doc):
        self.__doc__ = doc + self.__doc__

    def _get_full_name(self):
        """
        Find the full name of the test. <package>.<module>.<class>.<method>
        """
        # TODO: May be we have to do this only once for each test class.
        method = self.method_name = self._testMethodName
        clazz = self.class_name = self.__class__.__name__
        # To find the package keep going up from the module path
        # until we don't find an __init__.py
        package = ''
        file_name = sys.modules[self.__class__.__module__].__file__
        module = self.module_name = self.__class__.__module__.split('.')[-1]
        if self._module_package_dict[file_name]:
            package = self._module_package_dict[file_name]
        else:
            current_directory = os.path.dirname(file_name)
            found = True
            while True:
                if os.path.exists(os.path.join(current_directory, '__init__.py')):
                    package = "%s.%s" %(os.path.split(current_directory)[1], package)
                else:
                    break
                current_directory = os.path.abspath(os.path.join(current_directory, os.path.pardir))
            self._module_package_dict[file_name] = package = package.strip('.')

        self.package_name = package
        full_name = "%s.%s.%s.%s" %(package,
                                    module, clazz, method)
        return full_name
        
    def _parse_metadata_from_doc(self, docstring):
        """
        parse the meta from the docsting
        """
        if not docstring:
            return
        lines = docstring.splitlines()
        for line in lines:
            line = line.strip()
            if line.find('@') != 0:
                continue
            line = line[1:]
            if len(line.split()) <= 1:
                continue
            (key, value) = line.split(' ', 1)
            if self._metadata.has_key(key) and key == 'tags':
                self._metadata[key] += ' ' + value
            elif self._metadata.has_key(key) and key== 'gucs':
                self._metadata[key] = self._metadata[key].strip(';') + '; ' + value.strip(';')
            else:
                self._metadata[key] = value

    def load_fail(self, exception_class = None, exception_message = ""):
        
        if exception_class is None:
            exception_class = tinctest.TINCException
        
        generic_message = "Loading of test %s failed. " % self.full_name
        exception_message = generic_message + exception_message
        tinctest.logger.error(exception_message)
        raise exception_class(exception_message)

    def setUp(self):
        """
        TINCTestCase's setUp method is responsible for the following:
        -> Skip the test if skip metadata is specified.
        """
        if self.skip:
            self.skipTest(self.skip)
        super(TINCTestCase, self).setUp()
    
    def cleanup(self):
        '''
        implement this function if there is any special generic cleanup that 
        needs to be done by the subclass... if we had a good exception handling
        model we could use this function as a cleanup for each test by adding 
        addCleanup
        '''
        pass

    def defaultTestResult(self, stream=None, descriptions=None, verbosity=None):
        if stream and descriptions and verbosity:
            return TINCTextTestResult(stream, descriptions, verbosity)
        else:
            return unittest.TestResult()

    def match_metadata(self, key, value):
        """
        This function checks if the value of metadata 'key' matches
        'value'.  Default will be a straight case-sensitive string compare.
        Sub-classes should override this method to implement 'metadata' specific match logic.

        For eg: this method takes care of 'tags' by checking if the given value is in the
        instance variable self.tags which is a set instead of doing a string compare.

        @rtype: boolean
        @return: True if the 'value' matches metadata 'key', False otherwise. Note that if the
                 test case instance does not have the metadata 'key', this will return False
        """
        is_match = False
        try:
            meta_attr = getattr(self, key)
        except:
            return False

        if key == 'tags':
            is_match = value in meta_attr
        else:
            is_match = meta_attr == value

        return is_match

    def collect_files(self):
        """
        Collects files related to the test.  This may be log files, core files, etc.,
        and is likely specific to the test (or test type) being run. Should be overriden by
        sub-classes. 
        """
        pass

    def _get_absolute_filename(self, filename):
        """
            Returns filename prefixed with the path where
            the TestCase Object resides.

            e.g If the ExpansionTestCase object resides in tincrepo/eating/test_expansion.py
            and if get_absolute_filename('filename') is invoked from anywhere e.g
            tinctest/models/gpdb/expansion/__init__.py, the output will be
            tincrepo/eating/filename.
        """
        source_file = sys.modules[self.__class__.__module__].__file__
        source_dir = os.path.dirname(source_file)
        return os.path.join(source_dir, filename)

    def add_data_provider_test_methods(self):
        """
        If the test has a data provider, this will generate a list of  additional tests one for each
        set of data returned by the data provider configured for this test.

        For eg: if a data provider configured for a test 'test_method'
        returns {'key1': 'data1', 'key2': 'data2'}, this method will return a list of tests
        ['test_method_key1', 'test_method_key2'] with self.test_data set to 'data1' and 'data2'
        respectively.

        @rtype: list
        @return: Returns a list of test cases that has the same logic as the given test case with
                 self.test_data set for each generated test case to an item returned by the
                 data provider configured for this test. 
        """
        test_name = '%s.%s.%s' %(self.__class__.__module__, self.__class__.__name__, self._testMethodName)
        
        data_provider_tests = []

        dict_of_test_data_dicts = {}
        for each_data_provider in self.data_provider.strip().split():
            each_data_provider_func = self._find_data_provider(each_data_provider)
            each_test_data_dict = each_data_provider_func()
            if not each_test_data_dict:
                raise tinctest.TINCException("Data provider %s for test %s should return some data" %(each_data_provider, test_name))
            if not type(each_test_data_dict) is dict:
                raise tinctest.TINCException("Data provider %s for test %s should return a dict" %(each_data_provider, test_name))
            dict_of_test_data_dicts[each_data_provider] = each_test_data_dict

        if len(dict_of_test_data_dicts) == 1:
            # Just one data provider. Handle it so that none of the existing usage breaks.
            # test_data will be a simple tuple of data key & data value
            test_data_dict = dict_of_test_data_dicts.values()[0]
            for key, value in test_data_dict.items():
                new_test_method_name = self._testMethodName + '_' + key
                test_tuple = (key, value)
                self._add_new_method(test_tuple, new_test_method_name)
                data_provider_tests.append(new_test_method_name)

        else:
            # Multiple data providers. Need to mix-n-match
            # test_data will be a list of tuples of (data_provider, data_key, data_value)
            data_providers, test_data_dicts = zip(*dict_of_test_data_dicts.items())
            product_list = [dict(zip(data_providers, test_data_dict)) for test_data_dict in itertools.product(*test_data_dicts)]

            for each_product in sorted(product_list):
                new_test_method_name = self._testMethodName
                test_tuple = []
                for data_provider,test_data_key in sorted(each_product.items()):
                    test_data_value = dict_of_test_data_dicts[data_provider][test_data_key]
                    new_test_method_name = new_test_method_name + '_' + test_data_key
                    test_tuple.append((data_provider, test_data_key, test_data_value))
                self._add_new_method(test_tuple, new_test_method_name)
                data_provider_tests.append(new_test_method_name)
                
        return data_provider_tests

    def _add_new_method(self, test_tuple, new_method_name):
        self._orig_testMethodName = self._testMethodName
        def test_function(my_self):
            my_self.test_data = test_tuple
            orig_test_method = getattr(my_self,my_self._orig_testMethodName)
            orig_test_method()

        new_test_method = new.instancemethod(test_function,
                                             self, self.__class__)
        self.__dict__[new_method_name] = new_test_method

    def _find_data_provider(self, each_data_provider):
        data_provider_function = None
        if not data_provider_function:
            # Check if the definition is found somewhere besides the module file...
            for each_class in inspect.getmro(self.__class__):
                if data_provider_function:
                    break
                functions = inspect.getmembers(inspect.getmodule(each_class), predicate=inspect.isfunction)
                for (name, function) in functions:
                    if hasattr(function, '__is_data_provider__'):
                        if function.__data_provider_name__ == each_data_provider:
                            data_provider_function = function
                            break
        
        if not data_provider_function:            
            test_name = '%s.%s.%s' %(self.__class__.__module__, self.__class__.__name__, self._testMethodName)
            raise tinctest.TINCException("Invalid data provider specified for test - %s" %test_name)
        return data_provider_function
