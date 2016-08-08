"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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
    @metadata: product_version: product version desc (tbd)
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
        #: Instance of TINCProductVersionMetadata encapsulating the product version metadata specified for this test case
        self.product_version = None
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
        pass or fail for the current tun. Currently set only for tests run through deprecated tincdb,
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

        if 'product_version' in self._metadata:
            try:
                self.product_version = _TINCProductVersionMetadata(self._metadata.get('product_version').strip())
            except TINCInvalidProductVersionException, e:
                self.load_fail(tinctest.TINCException, "Invalid product version specified - %s" %self._metadata.get('product_version').strip())
            
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

    def get_product_version(self):
        """
        This function should be implemented by the subclasses to return the currently
        deployed version of the product as a (<product_name>, <version>) tuple.
        For eg: ('gpdb' , '4.2.0.0').

        This will be used to verify the test case compatiblity against the currently deployed
        version of the product. Test case version will be provided as a part of the test case
        metadata 'product_version'

        @rtype: tuple
        @return: A tuple containing the name of the product and the version of the product.
        """
        return None

    def do_version_check(self):
        """
        Performs test case version compatibility against the deployed product version.
        If no product_version metadata is specified, the test case is considered valid.
        If get_product_version_check is not implemented or if it does not return anything,
        version checking is skipped and the test case will be run.

        If valid product_version metadata is specified, the test case is checked for compatibility
        against the deployed product version and is skipped if the current version does not
        fall within the range of the product versions specified through 'product_version' metadata
        """
        # None or empty string means all versions
        if not self.product_version:
            return True

        # Call out to get_product_version that the model authors will implement
        deployed_version = self.get_product_version()

        if not deployed_version:
            tinctest.logger.warning('Skipping version check as deployed version is None')
            return True
        if not self.product_version.match_product_version(deployed_version[0], deployed_version[1]):
            test_name = '%s.%s.%s' %(self.__class__.__module__, self.__class__.__name__, self._testMethodName)
            tinctest.logger.info('Skipping test %s as it does not apply to the deployed product version. ' %test_name + \
                                     'Test case version - %s , product version - %s' %(self.product_version, deployed_version))
            self.skip = 'Test does not apply to the deployed product version. Test case version - %s , Deployed version - %s' %(self.product_version, deployed_version)
            return False
        return True

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

class TINCInvalidProductVersionException(Exception):
    """
    Exception that is thrown when product version metadata is validated
    """
    pass

class _TINCProductVersionMetadata(object):
    """
    This encapsulates the information given as a part of 'product_version' metadata.
    """

    def __init__(self, product_version_str=None):
        """
        Parse the given metadata information and form the corresponding product version object.
        """
        # A dictionary of product and a list of _TINCProductVersionRange for inclusive checks
        self.product_version_included = defaultdict(list)
        # A dictionary of product and a list of _TINCProductVersionRange for exclusive checks
        self.product_version_excluded = defaultdict(list)
        self.product_version_str = None
        if product_version_str:
            self.product_version_str = product_version_str.strip()
            self._parse_product_version_metadata()

    def __add__(self, other_product_version_metadata):
        if isinstance(other_product_version_metadata, basestring):
            other = _TINCProductVersionMetadata(other_product_version_metadata)
        else:
            other = other_product_version_metadata
        result = _TINCProductVersionMetadata()
        result.product_version_included = copy.deepcopy(self.product_version_included)
        result.product_version_excluded = copy.deepcopy(self.product_version_excluded)

        for product in other.product_version_included:
            for version in other.product_version_included[product]:
                if result._contains_version_included(product, version) >= 0:
                    continue
                result.product_version_included[product].append(version)

        for product in other.product_version_excluded:
            for version in other.product_version_excluded[product]:
                if result._contains_version_excluded(product, version) >= 0:
                    continue
                result.product_version_excluded[product].append(version)
        return result


    def __sub__(self, other_product_version_metadata):
        if isinstance(other_product_version_metadata, basestring):
            other = _TINCProductVersionMetadata(other_product_version_metadata)
        else:
            other = other_product_version_metadata
        result = _TINCProductVersionMetadata()
        result.product_version_included = copy.deepcopy(self.product_version_included)
        result.product_version_excluded = copy.deepcopy(self.product_version_excluded)

        for product in other.product_version_included:
            for version in other.product_version_included[product]:
                idx = result._contains_version_included(product, version)
                if idx >= 0:
                    del result.product_version_included[product][idx]
            if not result.product_version_included[product]:
                del result.product_version_included[product]
                

        for product in other.product_version_excluded:
            for version in other.product_version_excluded[product]:
                idx = result._contains_version_excluded(product, version) 
                if idx >= 0:
                    del result.product_version_excluded[product][idx]
            if not result.product_version_excluded[product]:
                del result.product_version_excluded[product]
        return result

    def __eq__(self, other_product_version_metadata):
        """
        Two product version metadata objects are considered equal if all product version included
        and prodcut version excluded objects are the same in both
        Uses literal comparisons when comparing version /version range objects
        """
        if isinstance(other_product_version_metadata, basestring):
            other = _TINCProductVersionMetadata(other_product_version_metadata)
        else:
            other = other_product_version_metadata

        temp_self = _TINCProductVersionMetadata()
        temp_self.product_version_included = copy.deepcopy(self.product_version_included)
        temp_self.product_version_excluded = copy.deepcopy(self.product_version_excluded)

        temp_other = _TINCProductVersionMetadata()
        temp_other.product_version_included = copy.deepcopy(other.product_version_included)
        temp_other.product_version_excluded = copy.deepcopy(other.product_version_excluded)

        for product in self.product_version_included:
            for version in self.product_version_included[product]:
                if other._contains_version_included(product, version) >= 0:
                    del temp_self.product_version_included[product][0]
                    if not temp_self.product_version_included[product]:
                        del temp_self.product_version_included[product]
                    del temp_other.product_version_included[product][0]
                    if not temp_other.product_version_included[product]:
                        del temp_other.product_version_included[product]
                else:
                    return False

        if temp_self.product_version_included or temp_other.product_version_included:
            # This means there are unmatched elements in one of the lists
            return False

        for product in self.product_version_excluded:
            for version in self.product_version_excluded[product]:
                if other._contains_version_excluded(product, version) >= 0:
                    del temp_self.product_version_excluded[product][0]
                    if not temp_self.product_version_excluded[product]:
                        del temp_self.product_version_excluded[product]
                    del temp_other.product_version_excluded[product][0]
                    if not temp_other.product_version_excluded[product]:
                        del temp_other.product_version_excluded[product]
                else:
                    return False
            if temp_self.product_version_included[product] or temp_other.product_version_included[product]:
                # This means there are unmatched elements in one of the lists
                return False

        if temp_self.product_version_excluded or temp_other.product_version_excluded:
            # This means there are unmatched elements in one of the lists
            return False
        
        return True

    def _contains_version_included(self, product, version):
        """
        Verifies if a given product , version / version range is present in the inclusive ranges
        Uses literal comparisons.
        """
        if not product in self.product_version_included:
            return -1
        version_included_list = self.product_version_included[product]
        for item, idx in zip(version_included_list, range(len(version_included_list))):
            if type(version) == type(item) and version.is_literal_match(item):
                return idx
        return -1

    def _contains_version_excluded(self, product, version):
        """
        Verifies if a given product , version / version range is present in the exclusive ranges
        Uses literal comparisons
        """
        if not product in self.product_version_excluded:
            return -1
        version_excluded_list = self.product_version_excluded[product]
        for item, idx in zip(version_excluded_list, range(len(version_excluded_list))):
            if type(version) == type(item) and version.is_literal_match(item):
                return idx
        return -1
        
    def __str__(self):
        if self.product_version_str:
            return self.product_version_str
        else:
            if not self.product_version_included and not self.product_version_excluded:
                return str(None)
            return self._get_product_version_metadata_string()

    def _get_product_version_metadata_string(self):
        """
        Return a string a representation of product version metadata
        """
        version_str_list = []
        for item in self.product_version_included:
            version_str_list.append("%s : %s" %(item,
                                                ", ".join(str(element) for element in self.product_version_included[item])))

        for item in self.product_version_excluded:
            version_str_list.append("%s : %s" %(item,
                                                ", ".join('-%s' %str(element) for element in self.product_version_excluded[item])))

        return ", ".join(version_str_list)
            
        
    def _parse_product_version_metadata(self):
        # product_version_str will be in the format of:
        # gpdb: 4.2.6.1, [4.3 - 4.4], -4.3.1.1, (4.5-4.6), -(4.5.1.1-4.5.1.3), hawk:

        multiple_versions = []
        if ',' in self.product_version_str:
            multiple_versions = self.product_version_str.split(',')
        else:
            multiple_versions.append(self.product_version_str)
        
        product_attribute = None
        for product_version in multiple_versions:
            product_exists = re.search('(.*):(.*)', product_version)
            if product_exists:
                # product is defined; Store it to use it for subsequent versions
                product_attribute = product_exists.group(1)
                product_version = product_exists.group(2)
            if product_attribute:
                product_attribute = product_attribute.strip()
            else:
                raise TINCInvalidProductVersionException("Given product version %s is invalid. " %self.product_version_str)

            product_version = product_version.strip()
            if product_version.startswith('-'):
                # add it to exclusive
                product_version = product_version.strip('-')
                if product_version.startswith('[') or product_version.startswith('('):
                    self.product_version_excluded[product_attribute].append(_TINCProductVersionRange(product_version))
                else:
                    self.product_version_excluded[product_attribute].append(_TINCProductVersion(product_version))
            else:
                # add it to inclusive
                if product_version.startswith('[') or product_version.startswith('('):
                    self.product_version_included[product_attribute].append(_TINCProductVersionRange(product_version))
                else:
                    self.product_version_included[product_attribute].append(_TINCProductVersion(product_version))
    
    def match_product_version(self, product, version_str):
        """
        Given a product and a version string, verify if there is a match
        in the product version metadata
        """
        # Create a local version object
        product = product.strip()
        version_str = version_str.strip()
        dut_version_object = _TINCProductVersion(version_str, filler=str(_TINCProductVersion._upper_bound))
        
        # Exclusives always take precedence! First, check if dut version is there
        for iter_version_object in self.product_version_excluded[product]:
            if iter_version_object.match_version(dut_version_object):
                tinctest.logger.info("Product %s version %s does not fall in product_version metadata - %s" %(product, version_str, self.product_version_str))
                return False
        
        # Check inclusives
        for iter_version_object in self.product_version_included[product]:
            if iter_version_object.match_version(dut_version_object):
                return True

        # By default we return False, if the product is not specified in product version
        tinctest.logger.info("Product %s version %s does not fall in product_version metadata - %s" %(product, version_str, self.product_version_str))
        return False

class _TINCProductVersionRange(object):
    """
    Encapsulates a specific range of a product version.
    """

    def __init__(self, version_range_str):
        self.version_range_str = version_range_str.strip()
        self.upper_bound_version = None
        self.lower_bound_version = None
        self.upper_bound_inclusive = None
        self.lower_bound_inclusive = None
        self._parse_product_version_range()

    def _parse_product_version_range(self):
        """
        Ranges should be in the format [range1-range2], (range1-range2),
        (range1-), (-range2) and any combination of paranthesis
        '[]' means inclusive
        '()' means exclusive
        """
        version_range_pattern = r"""^(?P<lb_inc>\(|\[) # Matches the first bracket
                                    (?P<lb>.*?) # Matches the lower bound version string
                                    - #Matches the range separator
                                    (?P<ub>.*?) # Matches the upper bound version string
                                    (?P<ub_inc>\)|\])$ # Matches the last bracket
                                """

        matches = re.match(version_range_pattern, self.version_range_str, flags = re.I | re.X)

        if not matches:
            raise TINCInvalidProductVersionException("Given version range string %s is invalid." %self.version_range_str)

        lb = matches.group('lb')
        lb_inc = matches.group('lb_inc')
        ub_inc = matches.group('ub_inc')
        ub = matches.group('ub')

        self.lower_bound_inclusive = True if lb_inc == '[' else False
        self.upper_bound_inclusive = True if ub_inc == ']' else False
        

        self.lower_bound_version = _TINCProductVersion(lb, filler=str(_TINCProductVersion._lower_bound)) if lb else _TINCProductVersion('0.0.0.0')
        self.upper_bound_version = _TINCProductVersion(ub, filler=str(_TINCProductVersion._upper_bound)) if ub else _TINCProductVersion('main')

        # Incr / decr upper bound and lower bound versions
        if not self.lower_bound_inclusive:
            self.lower_bound_version = self.lower_bound_version.incr()
        if not self.upper_bound_inclusive:
            self.upper_bound_version = self.upper_bound_version.decr()

        # Assert that upper bound version is greater than lower bound version
        if not self.upper_bound_version > self.lower_bound_version:
            raise TINCInvalidProductVersionException("Upper bound version %s should be greater than lower bound version %s" %(self.upper_bound_version,
                                                                                                                              self.lower_bound_version))

    def is_literal_match(self, other):
        if not isinstance(other, _TINCProductVersionRange):
            other = _TINCProductVersionRange(other)
        if not self.upper_bound_version.is_literal_match(other.upper_bound_version) or \
               not self.lower_bound_version.is_literal_match(other.lower_bound_version):
            return False
        return True
            
    def match_version(self, version):
        """
        Given a __TINCProductVersion, verify if it falls within
        this range.
        """
        if not isinstance(version, _TINCProductVersion):
            version = _TINCProductVersion(version)
        
        if version >= self.lower_bound_version and version <= self.upper_bound_version:
            return True
        return False

    def __cmp__(self, other):
        if not isinstance(other, _TINCProductVersionRange):
            other = _TINCProductVersionRange(other)
        if (self.lower_bound_version == other.lower_bound_version) and \
           (self.upper_bound_version == other.upper_bound_version):
            return 0

        if self.lower_bound_version != other.lower_bound_version:
            return cmp(self.lower_bound_version, other.lower_bound_version)

        return cmp(self.upper_bound_version, other.upper_bound_version)

    def __str__(self):
        return self.version_range_str
        

class _TINCProductVersion(object):
    """
    This can just be a wrapper around gpversion.GpVersion. However since gpversion does
    not take care of hotfixes, we should include that here.
    """

    _main_version = '99.99.99.99'
    _lower_bound = 0
    _upper_bound = 99

    def __init__(self, version_str, filler='x'):
        self.version_str = version_str.strip() if version_str else None
        self.filler = filler.strip()
        # The four part version. Use filler for empty part numbers
        # Should either be a digit or 'x' which is the allowed wildcard. 
        self.version = []
        # The hotfix string
        self.hotfix = None
        self._parse_version_str(filler)

    def _parse_version_str(self, filler):
        """
        Possible regex:
        4.2.x, 4.2.x.x, 4.2, 4.x, x, 4.2.1.3, 4.2.1.3A, 4.2.1.3B, etc
        It is the user's responsibility to pass a string like this to
        __TINCProductVersion__.
        """
        # filler should just be 'x' or 'digits'
        filler_pattern = r"""^(x|\d+)$"""
        if not re.match(filler_pattern, filler):
            raise TINCInvalidProductVersionException("Invalid filler specified. Should be 'x' or 'digits'")
        # '' or 'x' or None means a complete wild card match. 

        if not self.version_str or self.version_str == 'x' or self.version_str == 'X':
            self.version.extend(['x'] * 4)
            return

        if self.version_str.lower() == 'main':
            self.version.extend(self._main_version.split('.'))
            return

        _version_str_pattern = r"""^(?P<majors>((x|\d+)\.){1,3}) # Matches upto first three parts of a four part version x. , x.x. , x.x.x.
                                   (?P<minor>x|\d+) # Matches the final part of a four part version
                                   (?P<hotfix>([a-z]+\d*)*)$ # Matches the hotfix part of the version string
                                """

        matches = re.match(_version_str_pattern, self.version_str, flags = re.I | re.X)
        
        if not matches:
            raise TINCInvalidProductVersionException("Given version string %s is invalid." %self.version_str)

        majors = matches.group('majors')
        minor = matches.group('minor')
        hotfix = matches.group('hotfix')
       
        #majors now have to be of the form x. , x.x. or x.x.x.
        #filter it to remove the last part which will be None
        major_parts = [x.strip() for x in filter(None, majors.split('.'))]

        if len(major_parts) > 3 or len(major_parts) < 1:
            raise TINCInvalidProductVersionException("Given version string %s is invalid." %self.version_str)

        # minor should not be none
        if not minor:
            raise TINCInvalidProductVersionException("Given version string %s is invalid." %self.version_str)

        self.version.extend([x.lower() for x in major_parts])
        self.version.append(minor.lower())

        #hotfix should be given only if the whole version is given
        # For eg: 4.2MS1 will be invalid, 4.2.3.5MS1 , 4.2.2.4MS1 is valid,
        # 4.2.xMS1 will also be invalid, 4.2.x.xMS1 will also be invalid
        if (len(self.version) < 4 and hotfix) or ('x' in self.version and hotfix):
            raise TINCInvalidProductVersionException("Given version string %s is invalid." %self.version_str + \
                                                     "Hotfix can be provided only with four part versions.")
        
        if hotfix:
            self.hotfix = hotfix.strip()
        
        # If version does not have four parts , fill the list with 'filler' to make it a four part version
        if len(self.version) < 4:
            self.version.extend([filler.lower()] * (4 - len(self.version)))

        # Strip all version components
        self.version = [x.strip() for x in self.version]
        

    def incr(self):
        """
        Utility method to increment the version. 4.2.0.0 -> 4.2.0.1
        4.2.99.99 -> 4.3.0.0
        Returns a new version object after the increment operation
        """

        if 'x' in self.version:
            return _TINCProductVersion(self.version_str)
        
        """
        Version with hotfix increment supported.
            4.3.1.0LA1 -> 4.3.1.0LA2
        Assumptions: the hotfix digits will cross double digit
        """
        if self.hotfix:
            hotfix_filter = re.compile("([a-zA-Z]+)([0-9]*)")
            hotfix_split = hotfix_filter.match(self.hotfix)
            if len(hotfix_split.groups()) > 1:
               if hotfix_split.group(2) == "":
                  raise TINCInvalidProductVersionException("Given version string %s is invalid." %self.version_str + \
                                                      "\nIncrement Operation not supported on hotfix versions without digits at the end \
                                                      Hotfix should end with a number \"LA1\", \"MS1\"")
               else:
                  num = int(hotfix_split.group(2)) + 1
                  new_version = str(".".join(str(x) for x in self.version))
                  new_version = new_version + hotfix_split.group(1) + str(num)
               return _TINCProductVersion(new_version)
            else:
               raise TINCInvalidProductVersionException("Given version string %s is invalid." %self.version_str + \
                                                      "\nIncrement Operation not supported on hotfix versions without digits at the end \
                                                      Hotfix should end with a number \"LA1\", \"MS1\"") 

        # If it is main version , return
        if self == _TINCProductVersion('main'):
            return _TINCProductVersion('main')
        
        new_version = []
        do_incr = True
        
        for i, v in enumerate(reversed(self.version)):
            if do_incr:
                new_version.append(str((int(v) + 1) % (self._upper_bound + 1)))
            else:
                new_version.append(v)
            # Stop incr if we do not wrap around
            if not new_version[i] == '0':
                do_incr = False

        return _TINCProductVersion('.'.join(list(reversed(new_version))))

    def decr(self):
        """
        Reverse of incr
        Utility method to decrement the version. 4.2.0.0 -> 4.1.99.99
        4.2.99.99 -> 4.2.99.98, 4.2.99.0 -> 4.2.98.99
        Returns a new version object after the decrement operation
        """

        if 'x' in self.version:
            return _TINCProductVersion(self.version_str)
        
        """
        Version with hotfix decrement supported.
            4.3.1.0LA5 -> 4.3.1.0LA4
            4.3.1.0LA1 -> 4.3.1.0     hotfix section removed
        """ 
        if self.hotfix:
            hotfix_filter = re.compile("([a-zA-Z]+)([0-9]*)")
            hotfix_split = hotfix_filter.match(self.hotfix)
            if len(hotfix_split.groups()) > 1:
               if hotfix_split.group(2) == "":
                  raise TINCInvalidProductVersionException("Given version string %s is invalid." %self.version_str + \
                                                      "\nDecrement Operation not supported on hotfix versions without digits at the end \
                                                      Hotfix should end with a number Ex.: \"LA1\", \"MS1\"")
               if int(hotfix_split.group(2)) ==1:
                  new_version =  str(".".join(str(x) for x in self.version))
                  return _TINCProductVersion(new_version)
               else:
                  num = int(hotfix_split.group(2)) - 1
                  new_version = str(".".join(str(x) for x in self.version))
                  new_version = new_version + hotfix_split.group(1) + str(num)
                  return _TINCProductVersion(new_version)
            else:
               raise TINCInvalidProductVersionException("Given version string %s is invalid." %self.version_str + \
                                                      "\nDecrement Operation not supported on hotfix versions without digits at the end \
                                                      Hotfix should end with a number Ex.: \"LA1\", \"MS1\"")
        # If it is main version , return
        if self == _TINCProductVersion('0.0.0.0'):
            return _TINCProductVersion('0.0.0.0')
        
        new_version = []
        do_decr = True
        
        for i, v in enumerate(reversed(self.version)):
            if do_decr:
                new_version.append(str((int(v) - 1) % (self._upper_bound + 1)))
            else:
                new_version.append(v)
            # Stop decr if we do not wrap around
            if not new_version[i] == '99':
                do_decr = False
        return _TINCProductVersion('.'.join(list(reversed(new_version))))

    def match_version(self, other):
        """
        Find if the other version object is equal to this version object
        """
        if not isinstance(other, _TINCProductVersion):
            raise TINCInvalidProductVersionException("Comparison supported only between two version instances.")
        if self == other:
            return True
        return False

    def is_literal_match(self, other):
        """
        Find if the other version object is literally equal to this version object.
        This means an exact match for the version_str
        """
        version1 = str(self)
        version2 = str(other)
        return version1.lower() == version2.lower()
        
    def __cmp__(self, other):
        """
        Implement comparison operations. Take into account hotfix versions.
        """
        if not isinstance(other, _TINCProductVersion):
            raise TINCInvalidProductVersionException("Comparison supported only between two version instances.")

        
        # Compare the list. Take into account the wildcard 'x'
        for x, y in zip(self.version, other.version):
            if x.lower() == 'x' or y.lower() == 'x':
                continue

            if int(x) < int(y):
                return -1
                
            if int(x) > int(y):
                return 1

        # If it gets here , versions are equal so far and we have to do hotfix match
        # Do hotfix match only if there are no wildcards in the version. We want to consider
        # 4.2.x to be equal to 4.2.1.0A 4.2.2.1B etc
        # If the given version does not have a wild card such as 4.2.1.0 then we dont consider
        # to be equal to 4.2.1.0A 

        # Also if the user specifies 4.2.x.1 (which is a corner case), then we dont do hotfix match
        # For eg: 4.2.x.1 will be considered equal to 4.2.9.1A 4.2.10.1 4.2.10.1B etc

        if 'x' in self.version or 'x' in other.version:
            return 0
        
        if not self.hotfix or not other.hotfix:
            return cmp(self.hotfix, other.hotfix)

        # If both are not None, do case insensitive comparison
        return cmp(self.hotfix.lower(), other.hotfix.lower())
            
    def __str__(self):
        if not self.version_str:
            return ".".join(self.version)
        return self.version_str                                           
