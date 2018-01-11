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

import datetime
import os
from threading import Thread

import tinctest
from tinctest.models.concurrency import ConcurrencyTestCase

from mpp.models import SQLTestCase

@tinctest.skipLoading("Test model. No tests loaded.")
class SQLConcurrencyTestCase(SQLTestCase, ConcurrencyTestCase):
    """
	SQLConcurrencyTestCase consumes a SQL file, performs the psql on the same sql file using concurrent connections
        specified by the 'concurrency' metadata.
        Note the order of base classes in the above inheritance. With multiple inheritance, the order in which the base
        class constructors will be called is from left to right. We first have to call SQLTestCase constructor which
        will dynamically generate our test method which is a pre-requisite to call ConcurrencyTestCase.__init__ because
        it works out of the test method that SQLTestCase generates. If you swap the order, good luck with debugging.

        @metadata: gpdiff: If set to true, for every reptition of the query for every iteration, the output is matched
                           with expected output and the test is failed if there is a difference. If set to false, the 
                           output is not matched with expected output for any of the repetition/iteration (default: False)
                           Note: the default is different from the parent SQLTestCase class.
    
    """
    def _infer_metadata(self):
        super(SQLConcurrencyTestCase, self)._infer_metadata()
        # Default value of gpdiff is False
        if self._metadata.get('gpdiff') and self._metadata.get('gpdiff') == 'True':
            self.gpdiff = True
        else:
            self.gpdiff = False

    def setUp(self):
        """
        Generate the sql files to be run for the concurrency test in setUp.
        Note: SQLConcurrencyTestCase works in a way where we execute the run_test method simultaneously in
        external test processes based on the concurrency metadata. This run_test method cannot generate sql files
        for the test because it causes synchronization issues when all the test proceses try to generate the same file with the same
        name. Hence, this generation has to happen in the setUp method which will be invoked before the test processes
        begin execution.
        """
        super(SQLConcurrencyTestCase, self).setUp()
        # Do the following only for sql based tests. If there is an explicit test method defined in the test class,
        # skip sql file generation for that test. For now, explicit test methods are identified by self.sql_file.
        # Normally, self.sql_file will be None for explicit test methods and will be set to the path of the sql file
        # for implicitly generated test methods.
        if self.sql_file:
            sql_file_list = self.form_sql_file_list()
            for sql_file in sql_file_list:
                generated_sql_file = self._generate_sql_file(sql_file, self._current_optimizer_mode)
    
    def run_test(self):
        """
        This implementation of run test takes care of running sql files concurrenctly based
        on the metadata concurrency and iterations.

        A sql file query01.sql with concurrency '5' will be run in 5 different external processes
        concurrently.

        If there are part sqls along with a sql file - for eg: query01.sql, query01_part1.sql,
        query01_part2.sql etc with each invocation will run all the part sqls concurrently in
        different threads in each of the external process started above.

        Note: This method will be run concurrently and hence do not make changes here that will cause synchronization issues.
        """
        tinctest.logger.trace_in()
        # Construct an out_file name based on the currentime
        # run the test and the parts of the test if it has
        # concurrently.
        # TODO - generate a more readable file name based
        # on the current execution context (using iteration and
        # thread id from ConcurrencyTestCase
        sql_file_list = self.form_sql_file_list()
        compare_files_list = []
        thread_list = []
        for sql_file in sql_file_list:
            now = datetime.datetime.now()
            timestamp_pid = '%s%s%s%s%s%s%s_%s'%(now.year,now.month,now.day,now.hour,now.minute,now.second,now.microsecond,os.getpid())
            out_file = sql_file.replace('.sql', '_' + timestamp_pid + '.out')
            out_file = os.path.join(self.get_out_dir(), os.path.basename(out_file))
            subst_ans_file = out_file.replace('.out', '.ans')
            ans_file = sql_file.replace('.sql', '.ans')
            ans_file = os.path.join(self.get_ans_dir(), os.path.basename(ans_file))
            compare_files_list.append((out_file, ans_file, subst_ans_file))
            thread_name = "Thread_" + os.path.splitext(os.path.basename(sql_file))[0]
            generated_sql_file = self._get_sql_file_name(sql_file, self._current_optimizer_mode)
            thread = _SQLConcurrencyTestCaseThread(target=self.psql_run, name=thread_name, args=(generated_sql_file, self.db_name, out_file), kwargs={})
            thread.start()
            thread_list.append(thread)

        # Wait for all the runs to finish
        for thread in thread_list:
            thread.join()

        # Time to compare the results
        result = True
        if self.gpdiff:
            for compare_objects in compare_files_list:
                if self.verify_out_file(
                        compare_objects[0],
                        compare_objects[1],
                        compare_objects[2]):
                    tinctest.logger.info(
                        "Found no differences between out_file"
                        " %s and ans_file %s." %
                        (compare_objects[0], compare_objects[2]))
                else:
                    tinctest.logger.error(
                        "Found differences between out_file "
                        "%s and ans_file %s." %
                        (compare_objects[0], compare_objects[2]))
                    result = False
        tinctest.logger.trace_out()
        return result

class _SQLConcurrencyTestCaseThread(Thread):
    """
    A subclass of Thread. We need this because we want to
    get the return value of the target function.
    """
    def __init__(self, group=None, target=None, name=None,
                 args=(), kwargs={}, Verbose=None):
        """
        initialize a thread
        """
        Thread.__init__(self, group, target, name, args, kwargs, Verbose)
        self._return = None
        self._name = name

    def run(self):
        """
        run the thread and save the return value of the target function
        into selt._return
        """
        tinctest.logger.info("Started thread: %s" %self._name)
        if self._Thread__target is not None:
            self._return = self._Thread__target(*self._Thread__args,
                                                **self._Thread__kwargs)

    def join(self):
        """
        return the return value of target function after join
        """
        Thread.join(self)
        tinctest.logger.info("Finished thread: %s" %self._name)
        return self._return

