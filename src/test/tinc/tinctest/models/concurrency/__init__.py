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
import traceback
import gc

from multiprocessing import Pool, Queue

import tinctest
from tinctest import TINCTestCase

import unittest2 as unittest

from unittest import TestResult

def remote_test_invoker(module_name, class_name, method_name, proc_name,
                        dargs=[], dxargs={}, setups = [], cleanups = []):
    """
    A wrapper function that will execute a given test method in an external process.

    @type module_name: string
    @param module_name: Name of the module

    @type class_name: string
    @param class_name: Name of the class

    @type method_name: string
    @param method_name: Name of the test method

    @type proc_name: string
    @type proc_name: Name of the process that will be used in the logs

    @type setups: list
    @type setups: A list of (function, args, kwargs) tuple that will be executed as setups

    @type cleanups: list
    @type cleanups: A list of (function, args, kwargs) tuple that will be executed as cleanups
                    after the test execution

    """
    tinctest.logger.info("Started remote test : %s - %s.%s.%s" % (proc_name, module_name, class_name, method_name) )
    try:

        full_class_path =  module_name + '.' + class_name
        # Import the class
        parts = full_class_path.split('.')
        module = ".".join(parts[:-1])
        klass = __import__( module )
        for comp in parts[1:]:
            klass = getattr(klass, comp)
        test_klass_instance = klass(method_name)

        # Execute all setups
        while setups:
            function, args, kwargs = setups.pop(0)
            try:
                setup_method = getattr(test_klass_instance, function)
                tinctest.logger.debug("Calling setup_method %s" % setup_method)
                setup_method(*args, **kwargs)
            except unittest.case.SkipTest, st:
                return [proc_name, tinctest._SKIP_TEST_MSG_PREFIX + str(st)]
            except Exception, e:
                tinctest.logger.exception("Setup failed: %s - %s.%s.%s" % (proc_name, module_name, class_name, method_name))
                return [proc_name, "Setup failed: %s" %traceback.format_exc()]

        # Execute the test method

        try:
            testMethod = getattr(test_klass_instance, method_name)
            tinctest.logger.debug("Calling testMethod %s" % testMethod)
            testMethod(*dargs, **dxargs)
        except unittest.case.SkipTest, st:
            return [proc_name, tinctest._SKIP_TEST_MSG_PREFIX + str(st)]
        except Exception, e:
            tinctest.logger.exception("Test failed: %s - %s.%s.%s" % (proc_name, module_name, class_name, method_name))
            return [proc_name, "Test failed: %s" %traceback.format_exc()]

        # Execute all cleanups in LIFO
        while cleanups:
            function, args, kwargs = cleanups.pop(-1)
            try:
                cleanup_method = getattr(test_klass_instance, function)
                tinctest.logger.debug("Calling cleanup_method %s" % cleanup_method)
                cleanup_method(*args, **kwargs)
            except Exception, e:
                tinctest.logger.exception("Cleanup failed: %s - %s.%s.%s" % (proc_name, module_name, class_name, method_name))
                return [proc_name, "Cleanup failed: %s" %traceback.formact_exc()]
    except Exception, e:
        tinctest.logger.exception("Error during invocation: %s" %traceback.format_exc())
        return [proc_name, "Error during invocation: %s" %traceback.format_exc()]

    tinctest.logger.info("Finished remote test : %s - %s.%s.%s" % (proc_name, module_name, class_name, method_name))
    return None


@tinctest.skipLoading("Test model. No tests loaded.")
class ConcurrencyTestCase(TINCTestCase):
    """
    This model class should not be instantiated directly and should
    be extended for adding test methods.
    ConcurrencyTestCase provides an implementation where the test method
    will be run concurrently based on the metadata 'concurrency'


    @metadata: concurrency: number of concurrent executions of the test case (default: 1)
    @metadata: iterations: number of times the concurrent executions are run (default: 1)
    """

    def __init__(self, methodName="runTest", baseline_result = None):
        self.iterations = None
        self.concurrency = None
        super(ConcurrencyTestCase, self).__init__(methodName)

    def _infer_metadata(self):
        super(ConcurrencyTestCase, self)._infer_metadata()
        self.iterations = int(self._metadata.get('iterations', '1'))
        self.concurrency = int(self._metadata.get('concurrency', '1'))

    def run(self, result=None, pool = None):
        """
        This method is overriden to implement concurrency for a test method. The default
        implementation of unittest's run method will just run the test method directly.

        In the implementation, we construct a supplementary test method that will run the
        actual test method concurrently based on self.concurrency.

        In addition, this accepts an optional 'pool' argument which is passed when a ConcurrencyTestCAse
        is used within a ScenarioTestCase.

        @type result: TINCTextTestResult
        @param result: The result object to be used for this test

        @type pool: TINCWorkerPool
        @param pool: The worker pool to be used to submit external tests. If not provided, a new worker pool will be created.
                     This is to enable ScenarioTestCase and ConcurrencyTestCase share the same worker pool. 
        """

        # RB: For ConcurrencyTestCase, we should run the test method for
        # 'self.iterations' number of times. So, we create a new instance
        # method that runs self._testMethodName the desired number of times
        # concurrently using a worker pool of size self.concurrency
        # and set self._testMethodName to the new method before calling super.run().
        # Note - The test will be reported using the new method instead of the original
        # test method. We will re-visit this later.
        self._orig_testMethodName = self._testMethodName

        worker_pool = pool

        def test_function(my_self):
            my_class = my_self.__class__.__name__
            my_module = my_self.__module__
            my_method_name = my_self._orig_testMethodName
            for iter in xrange(my_self.iterations):
                tinctest.logger.info("Starting iteration# %s of total %s..." % (str(iter + 1), str(my_self.iterations)))
                should_wait = True
                # TODO - Parameterize maximum pool size
                if worker_pool is None:
                    pool = TINCTestWorkerPool(100)
                else:
                    # This means that this test is being executed through a ScenarioTestCase
                    # and we should defer inspecting the results to the scenario test case.
                    pool = worker_pool
                    should_wait = False
                for i in xrange(my_self.concurrency):
                    proc_prefix = "%s_proc_" %my_self._testMethodName
                    proc_postfix = "_iter_%s_proc_%s" %(str(iter + 1), str(i + 1))
                    proc_name = proc_prefix + proc_postfix
                    # We use 'run_worker' here that will simply call out to the
                    # super class' run method. ConcurrencyTestCase.run method has
                    # the logic to create a new test method and we would not want this to be done twice.
                    pool.submit_test(my_module, my_class, my_method_name, proc_name)

                # Wait and inspect only when the concurrency test case is executed by itself.
                # Defer result inspection when concurrency test case is executed through
                # a scenario test case.
                if should_wait:
                    pool.join()

                    # Inspect the result_queue for failures or errors
                    try:
                        if pool.has_failures():
                            failure_string = pool.get_failure_string()
                            failure_index = failure_string.find(" failed execution")
                            if failure_index != -1:
                                failure_string = failure_string[:failure_index]
                            self.fail("Workers encountered errors or failures: %s" % failure_string)
                    finally:
                        pool.terminate()


        test_method = new.instancemethod(test_function,
                                         self, self.__class__)
        self.__dict__[ self._testMethodName + "*"] = test_method
        self._testMethodName = self._testMethodName + "*"
        super(ConcurrencyTestCase, self).run(result)


class TINCTestWorkerPool(object):
    """
    A wrapper around multiprocessing.pool for handling concurrency in TINC. Used from within
    ConcurrencyTestCase and ScenarioTestCase
    """

    def __init__(self, worker_pool_size = 100):
        """
        Initialize a multiprocessing.pool

        @param worker_pool_size: Size of the worker pool
        @type worker_pool_size: integer
        """
        tinctest.logger.info("Initializing worker pool with %d workers" % worker_pool_size)
        # The Queue object that will be shared between the current process and the process
        # that will run the test method. The external process will update the queue with
        # failure information which will be inspected from the runner for failures.
        self.result_queue = Queue()
        self.skipped_queue = Queue()
        self.total_tests = 0

        # A list of two-tuples containing the name of the worker tha failed and the traceback
        # as a string object from the remote process
        self.failed_workers = []

        # String containing worker name that failed and complete traceback string for each failed worker
        self._failure_info = ''
        # String containing worker name that failed and just the failure message from the traceback string for each failed worker
        self._brief_failure_info = ''
        gc.disable()
        self.pool = Pool(worker_pool_size)
        gc.enable()

    # callback function for each spawned process in the pool.
    # the ret parameter is the return value from the process's executor funtion
    def remote_test_cb(self, ret):
        # keep track of the total number of tests in the future we may need 
        # to find out if all tests in the concurrency/scenario suite were skipped
        # this variable will be helpful to decide that
        self.total_tests += 1
        if ret:
            if ret[1].find(tinctest._SKIP_TEST_MSG_PREFIX) != -1:
                self.skipped_queue.put(ret)
            else:
                self.result_queue.put(ret)


    def submit_test(self, module_name, class_name, method_name, proc_name = 'remote_test_process', dargs=[], dxargs={}, setups = [], cleanups = []):
        """
        Submit a test case asynchronously for remote execution

        @param module_name: Name of the module where the test resides
        @type module_name: string

        @param class_name: Name of the class where the test resides
        @type class_name: string

        @param method_name: Name of the test method to be executed remotely through this worker pool
        @type method_name: string

        @param proc_name: Name to be used for the process that is started for this test submitted
        @type proc_name: string

        @param dargs: A list of non-keyword arguments to be passed to the submitted test
        @type dargs: list

        @param dxargs: A dict of keyworkd arguments to be passed to the test while invoking
        @type dxargs: dict

        @param setups: A list of method names that should be run before the actual test is executed
        @type setups: list

        @param cleanups: A list of method names that should be run after the actual test is executed
        @type cleanups: list
        """
        self.pool.apply_async(remote_test_invoker, [module_name, class_name, method_name, proc_name, dargs, dxargs, setups, cleanups], callback=self.remote_test_cb)

    def join(self):
        """
        Join the worker pool. Will wait till all the tasks in the pool finishes execution
        """
        self.pool.close()
        self.pool.join()
        # Find failed workers
        self._find_failed_workers()

    def _find_failed_workers(self):
        """
        Inspect the result queue that will contain the failed workers and populate self.failed_workers
        """
        while not self.result_queue.empty():
            tinctest.logger.error("Failures encountered in at least one of the test workers.")
            worker_info = self.result_queue.get()
            self.failed_workers.append((worker_info[0], worker_info[1]))
        

    def has_failures(self):
        """
        Returns True / False depending on whether there are failures in the tasks submitted through this instance
        of the pool

        @rtype: boolean
        @return: True if there are failures in the submitted tasks, False otherwise
        """
        return len(self.failed_workers) > 0

    def inspect(self):
        """
        Inspect the result queue and returns list of workers that failed or errored in a
        tuple containing the worker name and the traceback string

        @rtype: list of two-tuples
        @return: 
        """
        if self.has_failures():
            tinctest.logger.error("Failures encountered in at least one of the test workers.")

    def get_failure_string(self):
        """
        Return an aggregated failure string for all the tasks submitted through this instance of the worker pool
        """
        for failed_worker in self.failed_workers:
            self._failure_info += "Worker %s failed execution : \n %s\n" %(failed_worker[0], failed_worker[1])
        return self._failure_info

    def get_brief_failure_string(self):
        """
        Similar to get_failure_string(), however, returns worker names and just the error message from the exception
        instead of the whole stack trace
        """
        for failed_worker in self.failed_workers:
            error_msg = ''
            if failed_worker[1] and len(failed_worker[1].split('\n')) >=2:
                error_msg = failed_worker[1].split('\n')[-2]
            self._brief_failure_info += "Worker %s failed execution: %s\n" %(failed_worker[0], error_msg)
        return self._brief_failure_info

    def terminate(self):
        """
        Termiates the worker pool. Disable gc to avoid hangs 
        """
        gc.disable()
        self.pool.terminate()
        gc.enable()

