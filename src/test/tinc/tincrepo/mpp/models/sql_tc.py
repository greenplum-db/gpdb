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

import new
import os
import re
import sys
import shutil
import unittest2 as unittest
from fnmatch import fnmatch

import tinctest

from tinctest import TINCTestLoader
from tinctest.lib import Gpdiff
from tinctest.lib.system import TINCSystem

from gppylib.gparray import GpArray
from gppylib.db.dbconn import DbURL
from gppylib.commands.gp import GpStop, GpStart

from mpp.lib.PSQL import PSQL
from mpp.lib.gpConfig import GpConfig
from mpp.models import MPPTestCase
from mpp.models.mpp_tc import _MPPTestCaseResult

@tinctest.dataProvider('optimizer_handling')
def optimizer_handling_data_provider():
    data = {'planner': 'off', 'orca': 'on'}
    return data

@tinctest.skipLoading("Test model. No tests loaded.")
class SQLTestCase(MPPTestCase):
    """
    SQLTestCase consumes a SQL file and expected output, performs
    the psql, and returns success/failure based on the ensuing gpdiff.

    @metadata: gucs: gucs in the form of key=value pairs separated by ';'
    @metadata: orcagucs: ORCA specific gucs
    @metadata: optimizer_mode: Optimizer mode to run the sql test case with. Valid values are on, off, both, None.
    @metadata: gpdiff: If set to true, the output is matched with expected output and the test is failed if there
                       is a difference. If set to false, the output is not matched with expected output (default: True)
    """

    # Relative path w.r.t test module, where to look for sql files and ans files.
    # Sub-classes can override this to specify a different location.
    #: Relative path w.r.t test module, where to look for sql files
    sql_dir = ''
    #: Relative path w.r.t test module, where to look for sql files
    ans_dir = ''

    #: Class variable to determine if ans file have to be generated.
    #: Valid values: yes, no, force. Default is 'no'.
    generate_ans = 'no'

    # Class variable to keep track of valid values of generate_ans
    _valid_generate_ans_values = ['yes', 'no', 'force']

    #: Class variable to specify the name of the template directory in the sql directory
    template_dir = 'template'

    #: Class variable dictionary to store substitution. Key is the original. Value is the substitution string.
    template_subs = dict()

    # Class variable (hidden) to determine if there are sql template. This is determined automatically.
    _template_exist = False

    # global optimizer mode, as set in postgresql.conf
    _global_optimizer_mode = None

    @classmethod
    def setUpClass(cls):
        """
        SQLTestCase's setUpClass is responsible for the following:
        -> If a 'setup' directory exists in the cls's sql_dir, all the sqls
           within that directory are executed against the database configured for this test class.
           Note that this database should be configured through the metadata 'db_name' in the class' docstring.
        """
        tinctest.logger.trace_in()
        # Call superclass setup first
        super(SQLTestCase, cls).setUpClass()
        cls._global_optimizer_mode = cls.get_global_optimizer_mode()

        setup_dir = os.path.join(cls.get_sql_dir(), 'setup')
        setup_out_dir = os.path.join(cls.get_out_dir(), 'setup')
        if os.path.exists(setup_dir):
            TINCSystem.make_dirs(setup_out_dir, ignore_exists_error = True)
            tinctest.logger.info("Running the setup directory sqls for the test class: %s" %cls.__name__)
            for setup_sql_file in os.listdir(setup_dir):
                if setup_sql_file.endswith('.sql'):
                    out_file = os.path.join(setup_out_dir, setup_sql_file.replace('.sql', '.out'))
                    setup_sql_file = os.path.join(setup_dir, setup_sql_file)
                    tinctest.logger.info("Running setup sql for test - %s" %setup_sql_file)
                    PSQL.run_sql_file(setup_sql_file, dbname=cls.db_name, out_file=out_file)
                    # TODO: Should we support gpdiff for setup sqls ?
        tinctest.logger.trace_out()

    @classmethod
    def get_global_optimizer_mode(cls):
        # TODO: May be , do all this once in MPPTestCase instead of for every test class
        # if we expect tests to not change certain configurations in postgresql.conf
        gpconfig = GpConfig()
        try:
            (master, segment) = gpconfig.getParameter('optimizer')
        except Exception, e:
            tinctest.logger.error("Failed to retrive optimizer mode")
            tinctest.logger.exception(e)
            return None
        return master

    @classmethod
    def get_sql_dir(cls):
        """
        Returns the absolute directory path for the sql directory configured for this class.
        Computed relative to where the test class exists.

        @rtype: string
        @return: Absolute directory path of the class' sql directory, if template are not found. Else, return out_dir/sql_dir
        """
        tinctest.logger.trace_in()

        source_dir = os.path.dirname(sys.modules[cls.__module__].__file__)
        return_sql_dir = os.path.join(source_dir, cls.sql_dir)
        if cls._template_exist:
            out_dir = cls.get_out_dir()
            return_sql_dir = os.path.join(out_dir, cls.__name__, cls.sql_dir)

        tinctest.logger.trace_out("return_sql_dir: %s" % return_sql_dir)
        return return_sql_dir

    @classmethod
    def get_ans_dir(cls):
        """
        Returns the absolute directory path for the ans directory configured for this class.
        Computed relative to where the test class exists.

        @rtype: string
        @return: Absolute directory path of the class' ans directory, if template are not found. Else, return out_dir/ans_dir
        """
        tinctest.logger.trace_in()

        source_dir = os.path.dirname(sys.modules[cls.__module__].__file__)
        return_ans_dir = os.path.join(source_dir, cls.ans_dir)
        if cls._template_exist:
            out_dir = cls.get_out_dir()
            return_ans_dir = os.path.join(out_dir, cls.__name__, cls.ans_dir)

        tinctest.logger.trace_out("return_ans_dir: %s" % return_ans_dir)
        return return_ans_dir

    @classmethod
    def tearDownClass(cls):
        """
        SQLTestCase's tearDownClass is responsible for the following:
        -> If a 'teardown' directory exists in the cls's sql_dir, all the sqls
           within that directory are exectued against the database configured for this test class.
           Note that this database should be configured through the metadata 'db_name' in the class' docstring.
        """
        tinctest.logger.trace_in()
        teardown_dir = os.path.join(cls.get_sql_dir(), 'teardown')
        teardown_out_dir = os.path.join(cls.get_out_dir(), 'teardown')
        if os.path.exists(teardown_dir):
            TINCSystem.make_dirs(teardown_out_dir, ignore_exists_error = True)
            tinctest.logger.info("Running the teardown directory sqls for the test class: %s" %cls.__name__)
            for teardown_sql_file in os.listdir(teardown_dir):
                if teardown_sql_file.endswith('.sql'):
                    out_file = os.path.join(teardown_out_dir, teardown_sql_file.replace('.sql', '.out'))
                    teardown_sql_file = os.path.join(teardown_dir, teardown_sql_file)
                    tinctest.logger.info("Running teardown sql for test - %s" %teardown_sql_file)
                    PSQL.run_sql_file(teardown_sql_file, dbname=cls.db_name, out_file=out_file)
                    # TODO: Should we support gpdiff for teardown sqls ?
        # Call super class teardown
        super(SQLTestCase, cls).tearDownClass()
        tinctest.logger.trace_out()

    @classmethod
    def _find_tests(cls, directory):
        """
        This definition finds the sql files in the directory specified.
        Then, it calls init of the object that creates the implicit test method.

        Following are the sqls that will belong to one test case for which we generate a test
        method dynamically: (from within the SQLTestCase constructor)

        test1.sql - main test sql
        test1_part1.sql, test1_part2.sql etc - supplementary sqls that will be run along with
        the main test sql sequentially.
        test1_setup.sql - Will be run as a part of the setup
        test1_teardown.sql - Will be run as a part of the teardown.
        setup.sql - Will be run as a part of the setup before test1_setup.sql
        teardown.sql - Will be run as a part of the teardown after test1_teardown.sql

        """

        tinctest.logger.trace_in("directory: %s" % directory)

        tests = []

        for filename in os.listdir(directory):
            if not fnmatch(filename, "*.sql"):
                continue

            sql_file = os.path.join(directory, filename)

            # Ignore setup and teardown sqls
            if fnmatch(filename, 'setup.sql') or fnmatch(filename, 'teardown.sql'):
                tinctest.logger.debug("Ignoring setup or teardown sql - %s" %sql_file)
                continue

            if re.match(".*_part\d+.sql", sql_file):
                tinctest.logger.debug("Ignoring part sql %s" %sql_file)
                continue

            if re.search(".*_setup.sql", sql_file):
                tinctest.logger.debug("Ignoring test setup sql %s" %sql_file)
                continue

            if re.match(".*_teardown.sql", sql_file):
                tinctest.logger.debug("Ignoring test teardown sql %s" %sql_file)
                continue

            partial_test_name = filename[:-4]

            # Test name is test_<name of the sql file without the extension>
            # query01.sql will be test_query91
            # template_query01.sql will be test_template_query01
            test_name = TINCTestLoader.testMethodPrefix + partial_test_name

            # The constructor is responsible for generating this test method dynamically if it doesn't exist
            tinctest.logger.debug("Trying to add test %s to dynamic tests list" %test_name)
            try:
                test_instance = cls(test_name)
                tinctest.logger.debug("Added test case %s to dynamic tests list" % test_name)
            except Exception, e:
                tinctest.logger.error("Couldn't add test case %s to dynamic tests list" % test_name)
                tinctest.logger.error(e)
                test_instance = tinctest.loader.make_failed_test(None, cls.__module__ + "." + cls.__name__ + "." + test_name, e)
            finally:
                tests.append(test_instance)

        tinctest.logger.trace_out("tests: (as reported above)")
        return tests

    @classmethod
    def handle_templates(cls):
        """
        This class method checks if any template directory exists.
        If they do, copy/parse all the sql and ans directories to out_dir.

        @rtype: boolean
        @return: Return True, if templates are found. False, otherwise
        """
        # Variables to store non-template directories
        sql_dir = cls.get_sql_dir()
        sql_setup_dir = os.path.join(sql_dir, "setup")
        sql_teardown_dir = os.path.join(sql_dir, "teardown")
        ans_dir = cls.get_ans_dir()
        ans_setup_dir = os.path.join(ans_dir, "setup")
        ans_teardown_dir = os.path.join(ans_dir, "teardown")

        # Variables to store template directories
        sql_template_dir = os.path.join(sql_dir, cls.template_dir)
        sql_setup_template_dir = os.path.join(sql_setup_dir, cls.template_dir)
        sql_teardown_template_dir = os.path.join(sql_teardown_dir, cls.template_dir)
        ans_template_dir = os.path.join(ans_dir, cls.template_dir)
        ans_setup_template_dir = os.path.join(ans_setup_dir, cls.template_dir)
        ans_teardown_template_dir = os.path.join(ans_teardown_dir, cls.template_dir)

        # Define a set for all template dir
        # Check if any exists, while creating a set.
        template_dir_set = set([sql_template_dir, sql_setup_template_dir, sql_teardown_template_dir, ans_template_dir, ans_setup_template_dir, ans_teardown_template_dir])
        for each_dir in template_dir_set:
            # Check if any of the template directory exists.
            tinctest.logger.debug("Checking if directory %s exists..." % each_dir)
            if os.path.exists(each_dir):
                cls._template_exist = True
                break

        if not cls._template_exist:
            # Nothing to do
            tinctest.logger.debug("No template directories found.")
            return False

        ####################################################
        # If here, we have at least one template directory #
        ####################################################

        # Define local variables to store all the out directories
        # SQL files go in out/sql directory, and ans file go in out/ans directory
        out_dir = cls.get_out_dir()
        out_sql_dir = os.path.join(out_dir, cls.__name__, cls.sql_dir)
        out_sql_setup_dir = os.path.join(out_dir, cls.__name__, cls.sql_dir, "setup")
        out_sql_teardown_dir = os.path.join(out_dir, cls.__name__, cls.sql_dir, "teardown")
        out_ans_dir = os.path.join(out_dir, cls.__name__, cls.ans_dir)
        out_ans_setup_dir = os.path.join(out_dir, cls.__name__, cls.ans_dir, "setup")
        out_ans_teardown_dir = os.path.join(out_dir, cls.__name__, cls.ans_dir, "teardown")
        out_dir_set = sorted(set([out_sql_dir, out_sql_setup_dir, out_sql_teardown_dir, out_ans_dir, out_ans_setup_dir, out_ans_teardown_dir]))

        tinctest.logger.debug("Templates are found. Copy or parse sql files/dirs to directory %s. Copy or parse ans files/dirs to directory %s" % (out_sql_dir, out_ans_dir))

        for each_dir in out_dir_set:
            if os.path.exists(each_dir):
                tinctest.logger.warning("Directory %s already exists. Deleting it to start from scratch. Use different output directory to avoid this behavior!" % each_dir)
                shutil.rmtree(each_dir, ignore_errors = True)
            TINCSystem.make_dirs(each_dir, ignore_exists_error = True)

        # Define several sets to store all the sql dirs, ans dirs, setup dirs, etc.

        # Define sets for all sql and ans dirs
        sql_dir_set = set([sql_dir, sql_setup_dir, sql_teardown_dir, sql_template_dir, sql_setup_template_dir, sql_teardown_template_dir])
        ans_dir_set = set([ans_dir, ans_setup_dir, ans_teardown_dir, ans_template_dir, ans_setup_template_dir, ans_teardown_template_dir])

        # Define sets for all setup and teardown dirs
        setup_dir_set = set([sql_setup_dir, ans_setup_dir, sql_setup_template_dir, ans_setup_template_dir])
        teardown_dir_set = set([sql_teardown_dir, ans_teardown_dir, sql_teardown_template_dir, ans_teardown_template_dir])

        # Define a set for all dirs as union of sql and ans dir
        dir_set = sql_dir_set.union(ans_dir_set)

        # Type of files that will be copied or parsed
        file_types = ["*.sql", "*.ans", "*.ans.orca", "*.ans.planner"]

        # Go through each directory to either copy it or parse it
        for each_dir in dir_set:
            if not os.path.exists(each_dir):
                continue

            # SQL dir goes to out_dir/sql; ANS dir goes to out_dir/ans
            destination_dir = out_sql_dir
            if each_dir in ans_dir_set:
                destination_dir = out_ans_dir

            # Setup and Teardown files go in out/sql/setup, out/ans/teardown, etc.
            if each_dir in setup_dir_set:
                destination_dir = os.path.join(destination_dir, 'setup')
            elif each_dir in teardown_dir_set:
                destination_dir = os.path.join(destination_dir, 'teardown')

            # If template directory, parse it. Else, copy it.
            if each_dir in template_dir_set:
                tinctest.logger.debug("Directory %s is a template directory. Parse it and save the results in %s." % (each_dir, destination_dir))
                TINCSystem.substitute_strings_in_directory(each_dir, destination_dir, cls.template_subs, file_types, destination_file_prefix = "template_")
            else:
                tinctest.logger.debug("Directory %s is not a template directory. Copy its sql and ans files as is in %s." % (each_dir, destination_dir))
                TINCSystem.copy_directory(each_dir, destination_dir, file_types)

        return True

    @classmethod
    def _check_generate_ans(cls):
        cls.generate_ans = cls.generate_ans.lower()
        # Check that generate_ans value is valid: 'yes', 'no', or 'force'
        if cls.generate_ans not in cls._valid_generate_ans_values:
            raise SQLTestCaseException("Invalid value for generate_ans specified: %s. It should be one of the following: %s" % (cls.generate_ans, ', '.join(cls._valid_generate_ans_values)))

    @classmethod
    def loadTestsFromTestCase(cls):
        """
        This method customizes the loading of test cases for SQLTestCase. Generates test cases
        for all the test sql files within the sql directory of the test class.
        If templates are found, copy the sql files and the parsed out template files into out_dir.

        @rtype: list
        @return: List of SQLTestCase instances constructed for sql files in the sql directory
        """
        tinctest.logger.trace_in()

        # Check if generate_ans value is valid
        cls._check_generate_ans()

        # If template exists, copy/parse all the directories to out_dir
        cls.handle_templates()

        # Call get_sql_dir only after handling templates. It could point to out_dir, if templates exist.
        sql_dir = cls.get_sql_dir()

        # Time to browse directory and find tests
        tinctest.logger.debug("Finding tests in sql_dir %s..." %(sql_dir))
        tests = cls._find_tests(sql_dir)

        tinctest.logger.trace_out("tests: (as reported above)")
        return tests

    def __init__(self, methodName, baseline_result = None, sql_file=None, db_name = None):
        """
        This is an unconventional constructor. By design, the methodName may be
        implicit and may not yet exist in the class definition of whomever is
        subclassing SQLTestCase.

        So, our approach is to discover the intended test method and dynamically
        generate it; then, we will defer to traditional construction in the parent.
        """
        # Test case metadata
        self.gucs = None
        self.orcagucs = None
        self.optimizer_mode = None
        self.gpdiff = True

        self._optimizer_mode_values = ['on', 'off', 'both']
        # To track the current execution mode
        self._current_optimizer_mode = None

        # Default gucs to be added when optimizer is on
        self._optimizer_gucs = ['optimizer_log=on']
        self.sql_file = sql_file
        self.ans_file = None

        # Paths to original sql file and ans file of the test.
        # When a test is generated from a template, self.sql_file and self.ans_file will point
        # to the generated sql and ans file through out this class. This maintains a ref to
        # the original sql file and ans file of the test case.
        self._original_sql_file = self.sql_file
        self._original_ans_file = self.ans_file

        # if the test method is explicit and already defined, construction is trivial
        if methodName.startswith(tinctest.TINCTestLoader.testMethodPrefix) and \
           hasattr(self.__class__, methodName) and \
           hasattr(getattr(self.__class__, methodName), '__call__'):
            super(SQLTestCase, self).__init__(methodName)
            return

        # otherwise, do dynamic test generation
        self._generate_test_method_dynamically(methodName, sql_file)

        super(SQLTestCase, self).__init__(methodName, baseline_result)

    def _generate_test_method_dynamically(self, methodName, sql_file):
        """
        Given a test method name and a sql file for which the test method is to be constructed,
        this method generates and adds the test method dynamically to the class.

        Assumptions: Assume test method name is the name of the sql file without the extension
        prefixed by 'test_'. For eg: a sql file query01.sql, we will generate a test method 'test_query01'

        TODO: This should also be provided as a service at TINCTestCase level so that sub-classes
        can leverage dynamic test method construction if they are customizing the loading of
        tests.
        """

        assert methodName.startswith(tinctest.TINCTestLoader.testMethodPrefix)
        partial_test_name = methodName[len(tinctest.TINCTestLoader.testMethodPrefix):]

        # implicit sql tests are generated from *.sql/*.ans files
        # found in the current working directory

        # To enable tinc_client to construct a test case for a specific sql file
        # TODO: This piece of code gets executed only through deprecated tincdb. May not work anymore.
        if sql_file is not None:
            self.sql_file = sql_file
            partial_file_name = os.path.basename(sql_file)[:-4]
            # Order in which ans files are located
            # 1. Same as sql file location. 2. sql_file/../expected/
            self.ans_file = os.path.join(os.path.dirname(sql_file), "%s.ans" %partial_file_name)
            if not os.path.exists(self.ans_file):
                ans_dir = os.path.join(self.get_source_dir(), self.__class__.ans_dir)
                self.ans_file = os.path.join(ans_dir, "%s.ans" %partial_file_name)
            if not os.path.exists(self.ans_file):
                self.ans_file = os.path.join(os.path.dirname(sql_file), "../expected/", "%s.ans" %partial_file_name)
        else:
            # Normal execution (sql_file is None)
            if not self.__class__._template_exist and partial_test_name.startswith("template_"):
                # Dealing with templates that are not parsed.
                # Call class method handle_templates to move sql/ans to out_dir
                self.handle_templates()
            if self.__class__._template_exist and partial_test_name.startswith("template_"):
                base_file = partial_test_name[len("template_"):]
                self._original_sql_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), self.__class__.sql_dir, self.__class__.template_dir, "%s.sql" % base_file)
                self._original_ans_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), self.__class__.ans_dir, self.__class__.template_dir, "%s.ans" % base_file)
            else:
                self._original_sql_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), self.__class__.sql_dir, "%s.sql" % partial_test_name)
                self._original_ans_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), self.__class__.ans_dir, "%s.ans" % partial_test_name)

            # At this point, regular non-template sql files would assume that the sql file is the one in sql_dir
            # For template sql file, get_sql_dir will automatically point to the one in out_dir
            self.sql_file = os.path.join(self.get_sql_dir(), "%s.sql" % partial_test_name)
            self.ans_file = os.path.join(self.get_ans_dir(), "%s.ans" % partial_test_name)

        tinctest.logger.debug("sql_file: %s", self.sql_file)
        tinctest.logger.debug("ans_file: %s", self.ans_file)
        intended_docstring = self._read_metadata_as_docstring()

        # this is the dynamic test function we will bind into the
        # generated test intance. (note the use of closures!)
        def implied_test_function(my_self):
            # This used to be an assert on the return value from run_test
            # By default we fail when there are diffs, however some sub-classes
            # have overriden this method and are returning False when there is a
            # failure
            result = my_self.run_test()
            if result is not None:
                my_self.assertTrue(result)

        implied_test_function.__doc__ = intended_docstring

        method = new.instancemethod(implied_test_function,
                                    self,
                                    self.__class__)
        self.__dict__[methodName] = method

    def _read_metadata_as_docstring(self):
        """
        pull out the intended docstring from the implied sql file
        parent instantiation will conveniently look to that docstring to glean metadata.

        Assumptions: Assume that the metadata is given as comments in the sql file at the top.

        TODO: Provide this as an API at TINCTestCase level so that every class can implement
        a custom way of reading metadata.
        """
        intended_docstring = ""

        with open(self.sql_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line.find('--') != 0:
                    break
                intended_docstring += line[2:].strip()
                intended_docstring += "\n"
        return intended_docstring

    def handle_optimizer_mode_metadata(self):

        # metadata optimizer_mode
        if self._metadata.get('optimizer_mode', None):
            if not self._metadata.get('optimizer_mode').lower() in self._optimizer_mode_values:
                self.load_fail(SQLTestCaseException, "Invalid metadata specified for optimizer_mode: %s. Valid values are %s" %(self._metadata.get('optimizer_mode'), self._optimizer_mode_values))
            self.optimizer_mode = self._metadata.get('optimizer_mode').lower()

        if self.optimizer_mode == "both":
            if self.data_provider:
                self.data_provider += " optimizer_handling"
            else:
                self.data_provider = "optimizer_handling"

    def _infer_metadata(self):
        super(SQLTestCase, self)._infer_metadata()

        # metadata gpdiff
        if self._metadata.get('gpdiff') and self._metadata.get('gpdiff').lower() == 'false':
            self.gpdiff = False

        # metadata gucs
        if self._metadata.get('gucs', None) == None:
            self.gucs = set()
        else:
            self.gucs = set(self._metadata['gucs'].split(';'))

        # add metadata orcagucs for orca related gucs 'select disable_xform(XXXX)'
        if self._metadata.get('orcagucs', None) == None:
            self.orcagucs = set()
        else:
            self.orcagucs = set(self._metadata['orcagucs'].split(';'))

        self.handle_optimizer_mode_metadata()

    def setUp(self):
        """
        Runs the setup sql for the test case.
        For a test sql 'query01.sql', this will run setup.sql and query01_setup.sql
        in that order if it exists in the same location as the test sql file.
        """
        tinctest.logger.trace_in()
        super(SQLTestCase, self).setUp()
        if self.sql_file is not None:
            self._run_setup_sql()
        tinctest.logger.trace_out()

    def _run_setup_sql(self):
        """
        Run test case specific setup sql file.If 'setup.sql' exists in the same location
        as the sql file, run that first and then run <test_sql_file>_setup.sql if
        it exists.
        For a test sql 'query01.sql', this will run setup.sql and query01_setup.sql
        in that order if it exists in the same location as the test sql file.

        TODO: Incorporate ans file checks for setup sqls if they exist.
        """
        # Check if a common setup.sql file exists in the same location as the test sql
        setup_sql_file = os.path.join(os.path.dirname(self.sql_file), 'setup.sql')
        if os.path.exists(setup_sql_file):
            tinctest.logger.info("Running setup sql for test - %s" %setup_sql_file)
            self._run_and_verify_sql_file(setup_sql_file)

        test_case_setup_sql_file = self.sql_file.replace('.sql', '_setup.sql')
        if os.path.exists(test_case_setup_sql_file):
            tinctest.logger.info("Running setup sql for test - %s" %test_case_setup_sql_file)
            self._run_and_verify_sql_file(test_case_setup_sql_file)

    def tearDown(self):
        """
        Run teardown sql files for the test case.
        For a test sql 'query01.sql', this will run teardown.sql and query01_teardown.sql
        in that order if it exists in the same location as the test sql file.
        """
        tinctest.logger.trace_in()
        super(SQLTestCase, self).tearDown()
        if self.sql_file is not None:
            self._run_teardown_sql()
        tinctest.logger.trace_out()

    def _run_teardown_sql(self):
        """
        Run test specific teardown sql. If 'teardown.sql' exists in the same location
        as the sql file, run that first and then run <test_sql_file>_teardown.sql if
        it exists.
        """
        # Check if a test case specific teardown sql exists
        teardown_sql_file = self.sql_file.replace('.sql', '_teardown.sql')
        if os.path.exists(teardown_sql_file):
            tinctest.logger.info("Running teardown sql for test - %s" %teardown_sql_file)
            self._run_and_verify_sql_file(teardown_sql_file)

        # Check if a common teardown exists in the same location as the test sql
        teardown_sql_file = os.path.join(os.path.dirname(self.sql_file), 'teardown.sql')
        if os.path.exists(teardown_sql_file):
            tinctest.logger.info("Running teardown sql for test - %s" %teardown_sql_file)
            self._run_and_verify_sql_file(teardown_sql_file)

    def run_test(self):
        """
        The method that subclasses should override to execute a sql test case differently.
        This encapsulates the execution mechanism of SQLTestCase.
        Runs all the sql files that belong to this test case and compare with ans files.

        Note that this also runs the other part sqls that make up the test case. For eg: if the
        base sql is query1.sql, the part sqls are of the form query1_part*.sql in the same location
        as the base sql.

        TODO: Provide this as an api at TINCTestCase level. This can be the default test method that will
        be called for implicitly generated test methods.
        """
        tinctest.logger.trace_in()


        optimizer_mode = self.optimizer_mode
        if optimizer_mode == 'both':
            # test_data will have the real value
            if isinstance(self.test_data, tuple):
                optimizer_type, optimizer_mode = self.test_data
            if isinstance(self.test_data, list):
                for each_tuple in self.test_data:
                    data_provider, optimizer_type, optimizer_mode = each_tuple
                    if data_provider == "optimizer_handling":
                        break
        if optimizer_mode == 'both':
            raise SQLTestCaseException("Data provider for optimizer_handling didn't work as expected")

        if not os.path.exists(self.sql_file):
            raise SQLTestCaseException('sql file for this test case does not exist - %s' %self.sql_file )
        if (self.__class__.generate_ans == 'no' and not os.path.exists(self.ans_file)) and self.gpdiff:
            tinctest.logger.error('ans file for this test case does not exist - %s' %self.ans_file )
            raise SQLTestCaseException('ans file for this test case does not exist - %s' %self.ans_file )

        sql_file_list = self.form_sql_file_list()

        if optimizer_mode == 'on':
            self._current_optimizer_mode = True
            tinctest.logger.info("Running sql files for the test with optimizer on")
        elif optimizer_mode == 'off':
            self._current_optimizer_mode = False
            tinctest.logger.info("Running sql files for the test with optimizer off")
        else:
            tinctest.logger.info("Running sql files for the test without modifying optimizer")

        for sql_file in sql_file_list:
            tinctest.logger.info("Running sql file %s" %sql_file)
            self._run_and_verify_sql_file(sql_file, self._current_optimizer_mode)

        tinctest.logger.trace_out()

    def _optimizer_suffix(self, optimizer):
        return 'orca' if optimizer else 'planner'

    def _which_ans_file(self, sql_file, optimizer):
        '''
        selects the right answer file depending on whether optimizer_mode is on or not

        if optimizer is True, answer file = .ans.orca and if not present, .ans
        if optimizer is False, answer file = .ans.planner and if not present, .ans
        if optimizer is None, answer file = .ans
        '''
        base_sql_file = os.path.basename(sql_file)
        ans_file = None

        if optimizer == True:
            ans_file = os.path.join(self.get_ans_dir(), base_sql_file.replace('.sql', '.ans.orca'))
        elif optimizer == False:
            ans_file = os.path.join(self.get_ans_dir(), base_sql_file.replace('.sql', '.ans.planner'))
	else:
            if self.__class__._global_optimizer_mode == 'on':
                ans_file = os.path.join(self.get_ans_dir(), base_sql_file.replace('.sql', '.ans.orca'))
            else:
                ans_file = os.path.join(self.get_ans_dir(), base_sql_file.replace('.sql', '.ans.planner'))

        if not ans_file:
            ans_file = os.path.join(self.get_ans_dir(), base_sql_file.replace('.sql', '.ans'))
        else:
            if not os.path.exists(ans_file):
                ans_file = os.path.join(self.get_ans_dir(), base_sql_file.replace('.sql', '.ans'))

        return ans_file

    def _run_and_verify_sql_file(self, sql_file, optimizer = None):
        ans_file = self._which_ans_file(sql_file, optimizer)

        if optimizer is None:
            out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql', '.out'))
        else:
            # out file will be *_opt.out or *_planner.out based on optimizer
            out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql', '_%s.out' %self._optimizer_suffix(optimizer)))
        self.run_sql_file(sql_file, out_file=out_file, optimizer=optimizer)

        if self.__class__.generate_ans == 'no':
            if self.gpdiff:
                if not os.path.exists(ans_file):
                    tinctest.logger.warning("ans file %s for sql file %s does not exist" % (ans_file, sql_file))
                elif not self.verify_out_file(out_file, ans_file):
                    tinctest.logger.exception("Failed with diffs during execution of sql: %s, out_file: %s, ans_file: %s" %(sql_file,
                                                                                                                            out_file,
                                                                                                                            ans_file))
                    if optimizer is None:
                        self.fail("Failed: diffs during execution of sql: %s, out_file: %s, ans_file: %s" %(sql_file,
                                                                                                            out_file,
                                                                                                            ans_file))
                    if optimizer is True:
                        self.fail("Failed with optimizer on: diffs during execution of sql: %s, out_file: %s, ans_file: %s" %(sql_file,
                                                                                                                              out_file,
                                                                                                                              ans_file))
                    if optimizer is False:
                        self.fail("Failed with optimizer off: diffs during execution of sql: %s, out_file: %s, ans_file: %s" %(sql_file,
                                                                                                                               out_file,
                                                                                                                               ans_file))
        elif self.__class__.generate_ans == 'yes':
            if os.path.exists(ans_file):
                tinctest.logger.warning("ans_file %s already exists! Since generate_ans is not set to force, ans_file will not be overwritten." % ans_file)
            else:
                tinctest.logger.info("Copying out_file %s to ans_file %s." % (out_file, ans_file))
                shutil.copyfile(out_file, ans_file)
        elif self.__class__.generate_ans == 'force':
            try:
                tinctest.logger.info("Force-copying out_file %s to ans_file %s." % (out_file, ans_file))
                shutil.copyfile(out_file, ans_file)
            except Exception:
                raise SQLTestCaseException("Cannot copy %s to %s. Permission denied. Verify that %s is writeable!" % (out_file, ans_file, ans_file))
        else:
            raise SQLTestCaseException("Invalid value for generate_ans specified: %s. It should be one of the following: %s" % (self.__class__.generate_ans, ', '.join(self.__class__._valid_generate_ans_values)))

    def form_sql_file_list(self, sql_dir = None):
        """
        Forms a list of all sql files belonging to this test case.

        @return: list of sql file names belonging to this test case
        @rtype: list
        """
        tinctest.logger.trace_in("sql_dir: %s" % str(sql_dir))

        if sql_dir is None:
            sql_dir = self.get_sql_dir()
        file_pattern = os.path.splitext(os.path.basename(self.sql_file))[0]

        tinctest.logger.debug("Forming sql file list from sql_dir %s, with file_pattern %s" % (sql_dir, file_pattern))

        # Find out the list of sqls to be run for this test case.
        sql_file_list = []

        # Look for additional parts to run
        for f in os.listdir(sql_dir):
            part_sql_file = os.path.join(sql_dir, f)
            if not fnmatch(f, file_pattern + "_part*.sql") and not fnmatch(f, file_pattern + ".sql"):
                continue
            sql_file_list.append(os.path.join(sql_dir, f))

        tinctest.logger.trace_out("sql_file_list: %s" % str(sql_file_list))
        return sql_file_list

    def _add_gucs_to_sql_file(self, sql_file, gucs_sql_file=None, optimizer=None):
        """
        Form test sql file by adding the defined gucs to the sql file
        @param sql_file Path to  the test sql file
        @param gucs_sql_file Path where the guc sql file should be generated.
        @param optimizer Boolean that specifies whether optimizer is on or off.
        """
        if not gucs_sql_file:
            gucs_sql_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file))

        with open(gucs_sql_file, 'w') as o:
            o.write('\n-- start_ignore\n')
            for guc_string in self.gucs:
                o.write("SET %s;\n" %guc_string)
            for orca_guc_string in self.orcagucs:
                o.write("%s;\n"%orca_guc_string)
            # Write optimizer mode
            optimizer_mode_str = ''
            if optimizer is not None:
                optimizer_mode_str = 'on' if optimizer else 'off'
            if optimizer_mode_str:
                o.write("SET optimizer=%s;\n" %optimizer_mode_str)

            if optimizer is not None and optimizer:
                for guc_string in self._optimizer_gucs:
                    o.write("SET %s;\n" %guc_string)
            o.write('\n-- end_ignore\n')
            with open(sql_file, 'r') as f:
                for line in f:
                    o.write(line)
        self.test_artifacts.append(gucs_sql_file)
        return gucs_sql_file

    def get_substitutions(self):
        """
        The method that subclasses should override to give a substitution directory
        These substitutions will happen to the sql and ans files just before the sql
        file gets executed and just before the verification of the ans file with the out file
        This allows users to have substitutions based on test case metadata and other information
        that might not be available at the time when template directories are handled which
        performs translations based on template_subs at the time a sql test gets loaded.

        @rtype dict
        @returns A dictionary of substitutions that should be applied to the test case sql and ans file
        """
        substitutions = {}
        return substitutions

    def run_sql_file(self, sql_file, out_file = None, out_dir = None, optimizer = None):
        """
        Given a sql file and an out file, runs the sql file against the test database (self.db_name)
        and saves the output to out_file. Before running the sql file, if 'gucs' are set for this test
        instance , runs the sql file by adding gucs to the sql file as session level configurations.

        @type sql_file: string
        @param sql_file: Absolute path to the sql file.

        @type out_file: string
        @param out_file: Absolute path to the out file to which the output of
                         sql file execution is saved.

        @type out_dir: string
        @param out_dir: Absolute path to a directory location where all the generated artifacts
                        will be written to.

        @type optimizer: boolean
        @param optimizer: Flag that determines if optimizer should be on or off while running the sql.
                          Defaults to None which means that the sql will be run as is.
        """
        tinctest.logger.trace_in("sql_file: " + str(sql_file) + "; out_file: " + str(out_file) + "; out_dir: " + str(out_dir) + "; optimizer: " + str(optimizer) + ";")

        if not out_dir:
            out_dir = self.get_out_dir()

        if not os.path.exists(out_dir):
            TINCSystem.make_dirs(out_dir, ignore_exists_error = True)

        # First generate the actual sql file to be run, by adding gucs, doing transformations etc
        generated_sql_file = self._get_sql_file_name(sql_file, optimizer, out_dir)
        self._generate_sql_file(sql_file, optimizer, generated_sql_file)

        if not out_file:
            out_file = self._get_out_file_name(sql_file, optimizer, out_dir)

        self.psql_run(generated_sql_file, self.db_name, out_file)
        self.test_artifacts.append(out_file)
        tinctest.logger.trace_out("out_file: " + str(out_file))
        return out_file

    def _get_out_file_name(self, sql_file, optimizer=None, out_dir=None):
        """
        Get the out file name based on the current sql file and the optimizer mode
        """
        if not out_dir:
            out_dir = self.get_out_dir()

        out_file = None
        if optimizer is None:
            out_file = os.path.join(out_dir, os.path.basename(sql_file).replace('.sql', '.out'))
        else:
            # out file will be *_opt.out or *_planner.out based on optimizer
            out_file = os.path.join(out_dir, os.path.basename(sql_file).replace('.sql', '_%s.out' %self._optimizer_suffix(optimizer)))

        return out_file

    def _get_sql_file_name(self, sql_file, optimizer=None, out_dir=None):
        """
        Get the sql file name that will be generated for a sql test based on the current sql file and optimizer mode
        """
        if not out_dir:
            out_dir = self.get_out_dir()
        generated_sql_file = os.path.join(out_dir, os.path.basename(sql_file))

        if optimizer is not None:
            # sql file will be <basename>_opt.sql or <basename>_planner.sql based on optimizer
            (root, ext) = os.path.splitext(generated_sql_file)
            generated_sql_file = '%s_%s.sql' %(root, self._optimizer_suffix(optimizer))

        return generated_sql_file

    def _generate_sql_file(self, sql_file, optimizer=None, generated_sql_file=None):
        """
        Generate the actual sql file to be run by this test. Adds the gucs metadata and the optimizer
        setting for the test case based on the gucs and optimizer_mode metadata respectively.
        After adding the gucs, perform substitutions in the sql file based on the dict returned by
        self.get_substitutions()
        """
        # Add gucs to the test sql and form the actual sql file to be run
        if not generated_sql_file:
            generated_sql_file = self._get_sql_file_name(sql_file, optimizer)

        self._add_gucs_to_sql_file(sql_file, generated_sql_file, optimizer)

        # Perform substitutions if there are any
        (root, ext) = os.path.splitext(generated_sql_file)
        substitution_sql_file = '%s_transforms%s' %(root, ext)
        if self._substitute_file(generated_sql_file, substitution_sql_file):
            shutil.copyfile(substitution_sql_file, generated_sql_file)

        return generated_sql_file

    def _substitute_file(self, filename, outfile):
        substitutions = self.get_substitutions()
        if not substitutions:
            return False
        return TINCSystem.substitute_strings_in_file(filename, outfile, substitutions)


    def psql_run(self, sql_file, dbname, out_file):
        """
        This is used for run_sql_file method and provide an easier way to override on
        how we want to run the sql file. One use case is running the sql files on a segment in utility mode.
        @param sql_file: Absolute path to the sql file.
        @param dbname: database name
        @param out_file: Absolute path to the output file
        """
        PSQL.run_sql_file(sql_file, dbname = dbname, out_file = out_file)

    def verify_out_file(self, out_file, ans_file, subst_ans_file=None):
        """
        Verify output file with ans file using gpdiff.
        If an 'init_file' exists in the same location as the sql_dir, this will be used
        while doing gpdiff.

        @type out_file: string
        @param out_file: Absolute path to the output file

        @type ans_file: string
        @param ans_file: Absolute path to the answer file.

        @type subst_ans_file: string
        @param subst_ans_file: Absolute path to be used for the answer
        file after substitution

        @rtype: boolean
        @return: True or False depending on whether gpdiff returned diffs

        """
        tinctest.logger.trace_in(
            "out_file: " + str(out_file) + "; ans_file: " +
            str(ans_file) + "; subst_ans_file: " + str(subst_ans_file))

        # Check if an init file exists in the same location as the sql file
        init_files = []
        init_file_path = os.path.join(self.get_sql_dir(), 'init_file')
        if os.path.exists(init_file_path):
            init_files.append(init_file_path)

        # Perform substitutions if there are any.  Writing it to out
        # dir as we do not want any generated files in ans dir which
        # might be checked-in.
        if subst_ans_file is None:
            subst_ans_file = os.path.join(
                self.get_out_dir(), os.path.basename(ans_file))
        if self._substitute_file(ans_file, subst_ans_file):
            ans_file = subst_ans_file

        result = Gpdiff.are_files_equal(out_file, ans_file, match_sub = init_files)
        if result == False and os.path.exists(out_file.replace('.out', '.diff')):
            self.test_artifacts.append(out_file.replace('.out', '.diff'))
        tinctest.logger.trace_out("result: " + str(result))
        return result

    def gather_mini_dump(self, sql_file=None, out_dir=None, minidump_file=None):
        """
        Utility method to gather mini dumps for test queries.
        Set gp_opt_minidump to on, explain test query and gather mini-dump to out_dir
        """
        tinctest.logger.trace_in("sql_file: " + str(sql_file) + "; out_dir: " + str(out_dir) + "; minidump_file: " + str(minidump_file) + ";")
        if not sql_file:
            sql_file = self.sql_file

        if not out_dir:
            out_dir = self.get_out_dir()

        # Handle where self.sql_file might be None
        if not sql_file:
            tinctest.logger.warning("SQL file is not defined.")
            tinctest.logger.trace_out()
            return None

        if not os.path.exists(sql_file):
            tinctest.logger.warning("SQL file %s does not exist." %sql_file)
            tinctest.logger.trace_out()
            return None

        if not os.path.exists(out_dir):
            TINCSystem.make_dirs(out_dir, ignore_exists_error = True)

        opt_md_sql_file = os.path.join(out_dir, os.path.basename(sql_file).replace('.sql', '_opt_md.sql'))
        with open(opt_md_sql_file, 'w') as mini_dump_sql:
            with open(sql_file, 'r') as original_sql:
                opt_write = False
                for line in original_sql:
                    if not line.startswith('--') and not opt_write:
                        # Add optimizer settings and then add the line
                        mini_dump_sql.write('-- start_ignore\n')
                        mini_dump_sql.write('set optimizer = on; \n')
                        mini_dump_sql.write('set optimizer_minidump = always;\n')
                        mini_dump_sql.write('-- end_ignore\n')
                        # TODO - For now we assume only one valid SQL block following metadata
                        # and blindly add explain. This should suffice for the first demo. We
                        # will handle other cases later.
                        mini_dump_sql.write('explain ' + line)
                        opt_write = True
                    else:
                        mini_dump_sql.write(line)


        # Remove existing minidumps in $MASTER_DATA_DIRECTORY. Would be nice to have a GUC
        # to control location , name of the Mini-dump that gets generated.
        mdd = os.environ.get('MASTER_DATA_DIRECTORY', None)
        if not mdd:
            tinctest.logger.warning("Not gathering mini dump because MASTER_DATA_DIRECTORY is not set.")
            tinctest.logger.trace_out()
            return None

        mini_dump_directory = os.path.join(mdd, 'minidumps')

        if os.path.exists(mini_dump_directory):
            for f in os.listdir(mdd):
                if fnmatch(f, 'Minidump_*.mdp'):
                    os.remove(os.path.join(mdd,f))

        # Run opt_md_sql_file and copy the generated mini-dump over to out dir

        out_file = os.path.join(out_dir, os.path.basename(opt_md_sql_file).replace('.sql','.out'))
        tinctest.logger.info("Gathering mini-dump from sql : " + opt_md_sql_file)
        PSQL.run_sql_file(opt_md_sql_file, dbname = self.db_name, out_file = out_file)

        # Copy generated minidump from MASTER_DATA_DIRECTORY to out_dir
        if not os.path.exists(mini_dump_directory):
            tinctest.logger.warning("Minidump does not seem to be generated. Check %s for more information." %out_file)
            tinctest.logger.trace_out()
            return None

        for f in os.listdir(mini_dump_directory):
            if fnmatch(f, 'Minidump_*.mdp'):
                if not minidump_file:
                    test_md_file = os.path.join(out_dir, os.path.basename(self.sql_file).replace('.sql', '_minidump.mdp'))
                else:
                    test_md_file = minidump_file
                shutil.copyfile(os.path.join(mini_dump_directory, f), test_md_file)
                tinctest.logger.info("Minidump gathered at %s." %test_md_file)
                tinctest.logger.trace_out("test_md_file: " + str(test_md_file))
                return test_md_file

        tinctest.logger.warning("Minidump does not seem to be generated. Check %s for more information." %out_file)
        tinctest.logger.trace_out()
        return None

    def _restart_cluster(self, refresh_cache=False):
        """
        restart the cluster possibly refreshing cache
        """
        msg = ''
        if refresh_cache:
            msg = '(refresh cache)'
        msg = 'Restarting cluster ' + msg

        tinctest.logger.info(msg + ': STARTED');

        try:
            if refresh_cache:
                array = GpArray.initFromCatalog(DbURL(), True)
                GpStop.local('gpstop')
                seg_set = set()
                for seg in array.getDbList():
                    if not seg.getSegmentHostName() in seg_set:
                        TINCSystem.drop_caches(seg.getSegmentHostName())
                    seg_set.add(seg.getSegmentHostName())
            else:
                GpStop.local('gpstop')
            GpStart.local('gpstart')
            tinctest.logger.info(msg + ': DONE')
        except Exception, e:
            tinctest.logger.error(msg + ': FAILED')

    def defaultTestResult(self, stream=None, descriptions=None, verbosity=None):
        """
        Return a custom result object for SQLTestCase.
        """
        if stream and descriptions and verbosity:
            # TODO - Eliminate this check. We should not care about this and OptimizerSQLTestCase
            # should always return an SQLTestCaseResult.
            return _SQLTestCaseResult(stream, descriptions, verbosity)
        else:
            return unittest.TestResult()

class SQLTestCaseException(Exception):
    """
    Exception object thrown by SQLTestCase for failures / errors
    """
    pass

class _SQLTestCaseResult(_MPPTestCaseResult):
    """
    A custom listener class for SQLTestCase. This is responsible for
    reacting appropriately to failures and errors of type SQLTestCase.

    Following is what this class does on failure:
    -> If the test failed with optimizer turned on, gather mini dumps
       for the test sqls.
    """

    def addFailure(self, test, err):
        if test._current_optimizer_mode:
            test.gather_mini_dump()
        super(_SQLTestCaseResult, self).addFailure(test, err)

@tinctest.skipLoading("Test model. No tests loaded.")
class __gpdbSQLTestCase__(SQLTestCase):
    """
    Overwrite handle_optimizer_mode_metadata to check if GPDB version is less than 4.3. If it is, force it off
    """

    def handle_optimizer_mode_metadata(self):
        overwrite_optimizer_mode = False

        # metadata optimizer_mode
        if self._metadata.get('optimizer_mode', None):
            if not self._metadata.get('optimizer_mode').lower() in self._optimizer_mode_values:
                self.load_fail(SQLTestCaseException, "Invalid metadata specified for optimizer_mode: %s. Valid values are %s" %(self._metadata.get('optimizer_mode'), self._optimizer_mode_values))
            self.optimizer_mode = self._metadata.get('optimizer_mode').lower()
            if overwrite_optimizer_mode:
                self.optimizer_mode = None

        if self.optimizer_mode == "both":
            if self.data_provider:
                self.data_provider += " optimizer_handling"
            else:
                self.data_provider = "optimizer_handling"

@tinctest.skipLoading("Test model. No tests loaded.")
class __hawqSQLTestCase__(SQLTestCase):
    """
    Since HAWQ doesn't support planner, we should always set it to on
    """
    def handle_optimizer_mode_metadata(self):
        if self._metadata.get('optimizer_mode') and self._metadata.get('optimizer_mode').lower() == 'both':
            self.optimizer_mode = None
        elif self._metadata.get('optimizer_mode'):
            if not self._metadata.get('optimizer_mode').lower() in self._optimizer_mode_values:
                self.load_fail(SQLTestCaseException, "Invalid metadata specified for optimizer_mode: %s. Valid values are %s" %(self._metadata.get('optimizer_mode'), self._optimizer_mode_values))
            self.optimizer_mode = self._metadata.get('optimizer_mode').lower()
        else:
            self.optimizer_mode = None
