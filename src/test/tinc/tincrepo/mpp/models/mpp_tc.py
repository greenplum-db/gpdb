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

import inspect
import os
import re
import sys
import time

import tinctest

from tinctest.runner import TINCTextTestResult
from tinctest.lib.system import TINCSystem

from gppylib.commands.base import Command

from mpp.lib.datagen.databases import __databases__, TINCTestDatabase, TINCDatagenException
from mpp.lib.gplog import GpLog
from mpp.lib.gpstop import GpStop
from mpp.lib.PSQL import PSQL

import unittest2 as unittest

class MPPTestCaseException(Exception):
    """
    The exception that will be thrown for any errors or failures in MPPTestCase
    """
    pass

class MPPDUT(object):
    """
    This class is used to find the Device Under Test.
    It provides instance variables for product name and version_string.
    It will only be used by MPPMetaClassType to dynamically change a class's MRO.
    It also provides a product_environment dictionary to store gpopt version if found.
    """
    def __init__(self, product = None, version_string = None):
        
        # Valid products as of 11/25/13: gpdb, hawq
        self.product = product
        
        # version_string has this format: major#.minor#.service_pack#.version_number<hotfix_alphanumeral>
        # It can be incomplete: 4.3 or 4.2.1
        self.version_string = version_string
        
        self.product_environment = {}

        # First, get the product version
        if (self.product is None) or (self.version_string is None):
            self._get_product_version()
        
        # Next, get gpopt (GP Optimizer Mode) version
        gpopt_version = self._get_gpopt_version()
        if gpopt_version:
            self.product_environment['gpopt'] = gpopt_version

    def _get_version_string_output(self):
        # Version string is the output of postgres --gp-version or postgress --version
        # Output in gpdb: "postgres (Greenplum Database) 4.3_PARISTX_ORCA build 43249"
        # Output in hawq: "postgres (HAWQ) 4.2.0 build 1"
        # Output in postgres: "postgres (PostgreSQL) 9.2.4"

        # The following command will fail if the DUT is postgres
        version_command = Command(name = 'get gp-version', cmdStr = 'postgres --gp-version')
        try:
            version_command.run(validateAfter = True)
        except Exception, e:
            tinctest.logger.debug("Failed while running get gp-version: %s" %e)
            version_command = Command(name = 'get version', cmdStr = 'postgres --version')
            version_command.run(validateAfter = True)
            
        return version_command.get_results().stdout

    def _get_product_version(self):

        version_string_information = ''
        try:
            version_string_information = self._get_version_string_output()
        except Exception, e:
            tinctest.logger.exception("Failure while getting version information: %s" %e)
            tinctest.logger.critical("Could not detect one of the supported products (gpdb, hawq or postgres) in your environment. Make sure your environment is set correctly.")
            raise MPPTestCaseException("Could not detect one of the supported products (gpdb, hawq or postgres) in your environment. Make sure your environment is set correctly.")

        match_object = re.search("\((.+)\)", version_string_information)
        database_match = match_object.group(0)

        if "HAWQ" in database_match:
            self.product = 'hawq'
            # Replace version_string_information to point to hawq-version
            version_command = Command(name = 'get hawq-version', cmdStr = 'postgres --hawq-version')
            version_command.run(validateAfter = True)
            version_string_information = version_command.get_results().stdout
            tinctest.logger.info("DUT is detected to be hawq. Version string: %s" %version_string_information)
        elif "Greenplum Database" in database_match:
            tinctest.logger.info("DUT is detected to be gpdb. Version string: %s" %version_string_information)
            self.product = 'gpdb'
        elif "PostgreSQL" in database_match:
            tinctest.logger.info("DUT is detected to be postgres. Version string: %s" %version_string_information)
            self.product = 'postgres'
        else:
            tinctest.logger.critical("Unexpected version string obtained: %s." %version_string_information)
            tinctest.logger.critical("Could not detect one of the supported products (gpdb, hawq or postgres) in your environment. Make sure your environment is set correctly.")
            raise MPPTestCaseException("Unexpected version string obtained: %s" %version_string_information)
                    
        # At this point, version_string_information can be extracted to get the exact version
        # version_string_information for gpdb (--gp_version): "postgres (Greenplum Database) 4.3_PARISTX_ORCA build 43249"
        # version_string_information for hawq (--hawq_version): "postgres (HAWQ) 1.1.4.0 build dev"
        # version_string_information for postgres (--version): "postgres (PostgreSQL) 9.2.4"
        version_string_information_match_list = re.findall("\)\s(.*)", version_string_information)
        if version_string_information_match_list:
            # Remove everything after space and underscore
            version_part = re.sub(r'\s.*$', r'', version_string_information_match_list[0])
            version_part = re.sub(r'_.*$', r'', version_part)
            # At this point, we have a version
            self.version_string = version_part
        else:
            tinctest.logger.critical("Unexpected version string obtained: %s." %version_string_information)
            tinctest.logger.critical("Could not detect one of the supported products (gpdb, hawq or postgres) in your environment. Make sure your environment is set correctly.")
            raise MPPTestCaseException("Unexpected version string obtained: %s" %version_string_information)

    def _get_gpopt_version(self):
        # Return gpopt_version. Return empty, if not found.

        gp_opt_version = ""
        try:
            # The following command will fail if the DUT doesn't have optimizer
            gp_opt_version_cmd_results = {}
            psql_stdout = PSQL.run_sql_command("select gp_opt_version()", flags = "-t -q", results=gp_opt_version_cmd_results).strip()
            if gp_opt_version_cmd_results['rc'] or gp_opt_version_cmd_results['stderr'] != "":
                # received an error
                return gp_opt_version
            # Output is in the format of: GPOPT version: 1.241, GPOS version: 1.90, Xerces version: 3.1.1-p1
            # We want 1.241 from the above
            gp_opt_version = psql_stdout.split()[2].strip(",")
        except Exception, e:
            tinctest.logger.debug("Failed while running select gp_opt_version: %s" %e)
        return gp_opt_version

    def __str__(self):
        return "DUT: product: %s ; version: %s" % (self.product, self.version_string)

class _MPPMetaClassType(type):
    """
    MPPMetaClassType class overrides new and init methods of metaclass type.
    It is used to dynamically change a class's MRO for a DUT.
    It does this by iterating through the base classes and checking
    if there are any product-specific hidden models of those base classes.
    MPPTestCase and all of its derived classes are of type MPPMetaClassType.
    Product-specific hidden models have to follow these rules:
    - They have to reside in the same module as the base class.
    - They have to be prefixed and suffixed with two underscores (__)
    - They have to have the lower-case product name in the class name, following the prefix of __
    - The product name has to be same as the one provided by DUT class.
    An example of product-specific hidden model: __gpdbSQLTestCase__ in the same module as SQLTestCase for gpdb DUT.
    """

    # Class variable to keep track of DUT
    DUT = MPPDUT()
    tinctest.logger.info(DUT)
    def __new__(metaclass, clsname, bases, dct):
        # Add DUT to class's built-in dictionary
        dct['__product__'] = _MPPMetaClassType.DUT.product
        dct['__version_string__'] = _MPPMetaClassType.DUT.version_string
        dct['__product_environment__'] = _MPPMetaClassType.DUT.product_environment
        dct['change_mro'] = False
        dct['make_me_product_agnostic'] = classmethod(metaclass.make_me_product_agnostic)
        
        new_bases = ()
        if (clsname.startswith('__') and clsname.endswith('__')) or (clsname is 'MPPTestCase'):
            # If here, our clsname is one of the product-specific hidden models or MPPTestCase
            # No need to check bases
            new_bases += bases
        else:
            # If here, we need to check each of our clsname's bases
            # and see if each has product-specific class
            for base in bases:
                new_base_name = '__' + _MPPMetaClassType.DUT.product + base.__name__ + '__'
                # Variable to track whether we found a match for the base
                try:
                    """ Product-specific hidden models should always reside in the same module as the base class """
                    exec ('from ' + base.__module__ + ' import ' + new_base_name)
                    new_bases += (eval(new_base_name),)
                except:
                    new_bases += (base,)
        return super(_MPPMetaClassType, metaclass).__new__(metaclass, clsname, new_bases, dct)
    
    def __init__(cls, clsname, bases, dct):
        super(_MPPMetaClassType, cls).__init__(clsname, bases, dct)
    
    @staticmethod
    def make_me_product_agnostic(cls):
        # Change the class variable change_mro to let mro() method know that this class needs to prepend product specific model
        cls.change_mro = True
        # The line below (fakingly changing the cls' bases) retriggers mro() method
        cls.__bases__ = cls.__bases__ + tuple()
    
    def mro(cls):
        default_mro = super(_MPPMetaClassType, cls).mro()
        if hasattr(cls, "change_mro") and cls.change_mro:
            new_class_name = '__' + _MPPMetaClassType.DUT.product + cls.__name__ + '__'
            try:
                exec ('from ' + cls.__module__ + ' import ' + new_class_name)
                new_class_object = eval(new_class_name)
                default_mro.insert(0, new_class_object)
                return default_mro
            except:
                # No hidden class defined. Nothing to do
                pass
        return default_mro

@tinctest.skipLoading("Test model. No tests loaded.")
class MPPTestCase(tinctest.TINCTestCase):
    """
    MPPTestCase model is a top-level executor for all MPP test cases. All MPP test cases (HAWQ, GPDB, etc.)
    should either directly or indirectly inherit from MPPTestCase. It inherits from TINCTestCase,
    and is a parent of SQLTestCase.

    When a test of this type fails, we do the following:
    -> if restart_on_fatal_failure is set to True, inspect logs for errors and restart the cluster.
    -> if gather_logs_on_failure is set to True, gather master and segment logs for the duration of the test case when this test case fails.

    @metadata: host: Host where the MPP database resides. Defaults to localhost.
    @metadata: db_name: Database where the test case will be executed. Defaults to system environment variable DBNAME.
    @metadata: username: Username to use to login to the database. Defaults to system environment variable USER.
    @metadata: password: Password to use to login to the database. If not given, it assumes that user has trust authentication.
    @metadata: gather_logs_on_fatal_failure: Gather master and segment logs in case of a fatal failure.
    @metadata: restart_on_fatal_failure: Boolean to determine if the cluster should be restarted on failure. If the metadata doesn't exist, it won't be restarted.

    @undocumented: defaultTestResult
    @undocumented: __metaclass__
    """

    # MPPTestCase class is of type MPPMetaClassType
    # MPPMetaClassType will take of reconfiguring the bases of all the derived classes that have product-specific hidden models
    __metaclass__ = _MPPMetaClassType

    #: Directory relative to the test module where all the output artifacts will be collected. Defaults to 'output/'
    out_dir = 'output/'
    
    #: Database name to be used for any connection to the test cluster. Defaults to None. This database will also be configured in setUpClass on MPPTestCase
    db_name = None

    def __init__(self, methodName, baseline_result = None):
        #: boolean that determines whether or not to restart the cluster on a fatal failure. Defaults to False.
        self.restart_on_fatal_failure = False
        
        #: boolean that determines whether or not to gather logs on failure. Defaults to False
        self.gather_logs_on_failure = False
        
        super(MPPTestCase, self).__init__(methodName, baseline_result)

    @classmethod
    def setUpClass(cls):
        """
        setUpClass of MPPTestCase does the following:
        
        -> Create out directory for the class if it does not exist.
           This is thread safe in case an MPPTestCase is used concurrently
           within a ScenarioTestCase or ConcurrencyTestCase

        -> Configures the database specified at the class level variable 'db_name'
        """
        tinctest.logger.trace_in()

        #QAINF-760 - we need to treat db_name in the class level doc string as a class level variable
        #rather than an instance level variable
        ds = cls.__doc__
        if ds:
            lines = ds.splitlines()
            for line in lines:
                line = line.strip()
                if line.find('@db_name') != 0:
                    continue
                line = line[1:]
                if len(line.split()) <= 1:
                    break
                (key, cls.db_name) = line.split(' ', 1)
                break

        super(MPPTestCase, cls).setUpClass()
        if not os.path.exists(cls.get_out_dir()):
            TINCSystem.make_dirs(cls.get_out_dir(), ignore_exists_error = True)

        if cls.db_name:
            tinctest.logger.debug("Configure database %s from MPPTestCase setUpClass." % cls.db_name)
            cls.configure_database(cls.db_name)
        tinctest.logger.trace_out()

    @classmethod
    def get_out_dir(cls):
        """
        Returns the absolute output directory for this test class.
        Joins cls.out_dir with the location where the test module exists.
        """
        source_file = sys.modules[cls.__module__].__file__
        source_dir = os.path.dirname(source_file)
        abs_out_dir = os.path.join(source_dir, cls.out_dir)
        return abs_out_dir

    @classmethod
    def get_source_dir(cls):
        """
        Returns the directory at which this test class exists.
        """
        source_file = sys.modules[cls.__module__].__file__
        source_dir = os.path.dirname(source_file)
        return source_dir

    @classmethod
    def configure_database(cls,db_name):
        """
        Configures the given database using datagen libraries.

        @param db_name: Name of the database to be configured. If there is no specific datagen available for this database,
                        this will just create an empty database with the given name.
        @type db_name: string
        """
        tinctest.logger.trace_in(db_name)
        if not __databases__.has_key(db_name):
            tinctest.logger.info("db_name %s is not defined in __databases__ dictionary." % db_name)
            __databases__[db_name] = TINCTestDatabase(database_name=db_name)
            py_mod = sys.modules[cls.__module__]
            TINCTestCustomDatabase = None
            for obj in inspect.getmembers(py_mod, lambda member: inspect.isclass(member)
                                          and issubclass(member, TINCTestDatabase)):
                if obj[1]._infer_metadata().get('db_name', None) == db_name:
                    TINCTestCustomDatabase = obj[1]
                    break

            if TINCTestCustomDatabase:
                __databases__[db_name] = TINCTestCustomDatabase(database_name=db_name)
            else:
                tinctest.logger.warning("No CustomDatabase class provided for %s." %db_name)

        if __databases__[db_name]:
            tinctest.logger.info("Running setup of database %s." % db_name)
            try: 
                __databases__[db_name].setUp()
            except Exception, exp:
                # if we are here, setup failed. handle errors 
                # accordingly.
                __databases__[db_name].tearDown()
                raise TINCDatagenException(exp)

        tinctest.logger.trace_out()
    
    def setUp(self):
        """
        setUp method in MPPTestCase does the following:

        -> Configures the database specified through the metadat 'db_name'.
           This will configure the database only if it was not already done in setUpClass.
        """
        tinctest.logger.trace_in()
        super(MPPTestCase, self).setUp()
        # Create the database if db_name metadata is specified and if it doesn't exists
        # TODO: Change TINCTestDatabase to take-in PSQL options (part of story QAINF-191)
        if self.db_name and self.__class__.db_name and self.db_name == self.__class__.db_name:
            tinctest.logger.debug("No need to configure database %s in setUp, since it would have already been configured via setUpClass." % self.db_name)
        elif self.db_name:
            tinctest.logger.debug("Configure database %s from MPPTestCase setUp." % self.db_name)
            self.configure_database(self.db_name)
        tinctest.logger.trace_out()

    def defaultTestResult(self, stream=None, descriptions=None, verbosity=None):
        """
        TODO: This method should not be exposed as a public method. All result objects
        will be internal.

        Return a custom result object for MPPTestCase. We need a handle on
        whether the test errored out / failed to honor metadata like 'restart'
        """
        if stream and descriptions and verbosity:
            return _MPPTestCaseResult(stream, descriptions, verbosity)
        else:
            return unittest.TestResult()

    def get_product_version(self):
        """
        This function is used by TINCTestCase to determine the current DUT version.
        It uses this information, along with @product_version, to determine if a test case
        should run in this particular DUT.

        @return: A two-tuple containing name and version of the product where test is executed
        @rtype: (string, string)
        """
        return (self.__class__.__product__, self.__class__.__version_string__)

    def _infer_metadata(self):
        """
        Read all the metadata and store them as instance variables.
        """
        super(MPPTestCase, self)._infer_metadata()
        self.host = self._metadata.get('host', 'localhost')
        self.db_name = self._metadata.get('db_name', self.__class__.db_name)
        self.username = self._metadata.get('username', None)
        self.password = self._metadata.get('password', None)
        if self._metadata.get('gather_logs_on_failure') and self._metadata.get('gather_logs_on_failure').lower() == 'true':
            self.gather_logs_on_failure = True
        if self._metadata.get('restart_on_fatal_failure') and self._metadata.get('restart_on_fatal_failure').lower() == 'true':
            self.restart_on_fatal_failure = True
        self.gpopt = self._metadata.get('gpopt', None)

        if self.gpopt:
            if 'gpopt' not in self.__class__.__product_environment__:
                self.skip = 'Test does not apply to the deployed system. Test Case GPOPT version - %s , Deployed system has no GPOPT' % self.gpopt
            elif tuple(self.gpopt.split('.')) > tuple(self.__class__.__product_environment__['gpopt'].split('.')):
                self.skip = 'Test does not apply to the deployed GPOPT version. Test Case GPOPT version - %s , Deployed version - %s' % (self.gpopt, self.__class__.__product_environment__['gpopt'])
    
    def install_cluster(self):
        """
        This function will install the cluster
        """
        pass

    def initialize_cluster(self):
        """
        This function will initialize the cluster
        """
        pass

    def configure_cluster(self):
        """
        This function will configure the cluster
        """
        pass

    def inspect_cluster(self):
        """
        This function will inspect the cluster from the start time of this test till now.
        Returns true if there are no errors in logs, False if there are errors in logs.

        @return: Returns True / False depending on whether errors were found in the log
        @rtype: boolean
        """
        tinctest.logger.trace_in()
        start_time = self.start_time
        if start_time == 0 or not start_time:
            return True
        end_time = self.end_time
        if end_time == 0 or not end_time:
            end_time = time.time()

        return_status = not GpLog.check_log_for_errors(start_time, end_time)
        tinctest.logger.trace_out(str(return_status))
        return return_status

    def gather_log(self):
        """
        This method will gather logs from all segments between start_time and end_time
        of the test and write it to an out file in the output directory. The default name
        of the log file will be <testmethodname>.logs
        """
        tinctest.logger.trace_in()
        start_time = self.start_time
        if start_time == 0 or not start_time:
            return
        end_time = self.end_time
        if end_time == 0 or not end_time:
            end_time = time.time()
        out_file = os.path.join(self.get_out_dir(), self._testMethodName + '.logs')
        GpLog.gather_log(start_time, end_time, out_file)
        tinctest.logger.trace_out()

    def delete_cluster(self):
        """
        This function will delete the cluster
        """
        pass

    def start_cluster(self):
        """
        This function will start the cluster
        """
        pass

    def stop_cluster(self):
        """
        This function will stop the cluster
        """
        pass

    def restart_cluster(self):
        """
        This function will restart the cluster
        """
        pass

class _MPPTestCaseResult(TINCTextTestResult):
    """
    A custom listener class for MPPTestCase. This is responsible for
    reacting appropriately to failures and errors of type MPPTestCase.
    Following is what this class does on failure:
    -> If restart_on_fatal_failure is set for the test , inspects the logs for
       fatal failure and restarts the cluster if there are any errors found.
    -> If gather_logs_on_failure is set for the test, gathers segment and master
       logs to the output directory.
    """

    def addFailure(self, test, err):
        try:
            # restart the cluster if restart_on_failure is set to True and inspect cluster returns False
            if test.gather_logs_on_failure:
                test.gather_log()
            if test.restart_on_fatal_failure:
                if not test.inspect_cluster():
                    tinctest.logger.warning("Errors found in the logs for this test case. Restarting the cluster")
                    test.restart_cluster()
        except Exception, e:
            tinctest.logger.exception("Re-starting cluster failed - %s" %e)
        super(_MPPTestCaseResult, self).addFailure(test, err)

class __gpdbMPPTestCase__(MPPTestCase):
    """
    __gpdbMPPTestCase__ is a hidden class that overrides GPDB specific methods of MPPTestCase.
    This class should never be used as a parent or as an executor for any test cases.
    Presently, this class doesn't override any methods. It is here only for reference.
    """
    pass

class __hawqMPPTestCase__(MPPTestCase):
    """
    __hawqMPPTestCase__ is a hidden class that overrides HAWQ specific methods of MPPTestCase.
    This class should never be used as a parent or as an executor for any test cases.
    Presently, this class doesn't override any methods. It is here only for reference.
    """
    pass

class __postgresMPPTestCase__(MPPTestCase):
    """
    __postgresMPPTestCase__ is a hidden class that overrides postgres specific methods of MPPTestCase.
    This class should never be used as a parent or as an executor for any test cases.
    Presently, this class doesn't override any methods. It is here only for reference.
    """
    pass
    
