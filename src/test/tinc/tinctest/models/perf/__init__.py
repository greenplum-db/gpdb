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

from datetime import datetime
from math import sqrt
import inspect
import new
import os

import tinctest
from tinctest import TINCTestCase
from tinctest import logger

@tinctest.skipLoading("Test model. No tests loaded.")
class PerformanceTestCase(TINCTestCase):
    """
    This is an abstract class and cannot be instantiated directly. 
    PerformanceTestCase provides metadata and functionality that
    is common to all performance tests

    @metadata: repetitions: repeat the execution of the test this many times (default: 1)
    @metadata: baseline: a file containing the baseline results (default: None)
    @metadata: threshold: if a test performs worse than baseline by threshold percentage,
                          the test is considered failed (default: 5)
    """
    
    def __init__(self, methodName="runTest"):
        self.repetitions = None
        self.threshold = None
        self.baseline = None
        self._orig_testMethodName = methodName
        super(PerformanceTestCase, self).__init__(methodName)
         
    def _infer_metadata(self):
        super(PerformanceTestCase, self)._infer_metadata()
        self.repetitions = int(self._metadata.get('repetitions', '1'))
        self.threshold = int(self._metadata.get('threshold', '5'))
        self.baseline = self._metadata.get('baseline', None)

    def run(self, result=None):
        """
        This implementation of run method will generate a new test method that will
        run the actual test method 'self.repetitions' number of times and gather
        performance stats for the test method such as avg , maxruntime etc

        @type result: TINCTextTestResult
        @param result: The result object to be used for this particular test instance. 
        """
        """
        # XXX - Should revisit this approach later. Directly cloning the
        # method definition using lambda or new.instancemethod does not
        # seem to work well which is what we are trying to do here. 
        # Currently, we are going with the approach of creating a new test method
        # that calls the original test method repetitively and changing
        # self._testMethodName to point to the newly created method. 
        # The bug here will be that the test names used in the result object 
        # will be the name of the newly created method. 

        # For PerformanceTestCase, we should run the test method for
        # 'self.repetitions' number of times. So, we redefine the original
        # test method to actually do it's work multiple times.
        self._orig_testMethodName = 'orig_' + self._testMethodName
        methd = getattr(self, self._testMethodName)
        
        # The following crazy line of code creates a new instance method named
        # 'self._orig_testMethodName' which has the same definition as the 
        # instance method 'self._testMethodName'.
        
        orig_test_method  = lambda self : self.__class__.__dict__[self._testMethodName](self)
        orig_test_method.__name__ = 'orig_' + self._testMethodName
        setattr(self.__class__, self._orig_testMethodName, orig_test_method)
        """

        # For PerformanceTestCase, we should run the test method for
        # 'self.repetitions' number of times. So, we create a new instance
        # method that runs self._testMethodName the desired number of times
        # and set self._testMethodName to the new method before calling super.run().
        # Note - The test will be reported using the new method instead of the original
        # test method. We will re-visit this later. 


        self._orig_testMethodName = self._testMethodName

        def test_function(my_self):
            orig_test_method = getattr(my_self, my_self._orig_testMethodName)
            runtime_list = []
            for i in range(my_self.repetitions):
                # Get time before and after this function to time the test
                start = datetime.now()
                orig_test_method()
                end = datetime.now()

                delta = end - start
                milli = delta.seconds * 1000 + (float(delta.microseconds) / 1000)
                runtime_list.append(milli)
            total_runtime = sum(runtime_list)
            min_runtime = min(runtime_list)
            max_runtime = max(runtime_list)
            avg_runtime = total_runtime / my_self.repetitions
            std_dev = sqrt(sum((runtime - avg_runtime)**2 for runtime in runtime_list) / my_self.repetitions)
            std_dev_pct = std_dev * 100 / float(avg_runtime)
            logger.info("%s - %s" % (my_self, runtime_list))

            # Find the baseline file. For now, we assume that there is only
            # one baseline version specified
            current_dir = os.path.dirname(inspect.getfile(my_self.__class__))
            baseline_file = 'baseline_' + my_self.baseline + '.csv'
            baseline_file_path =  os.path.join(current_dir, baseline_file)
           
            (baseline_runtime, delta) =  GPPerfDiff.check_perf_deviation(my_self._orig_testMethodName, \
                                                   baseline_file_path, avg_runtime, \
                                                   my_self.threshold)
            # compose statistics
            stats = [ 
                    ('Test Name', "%s.%s" % (self.__class__.__name__, self._orig_testMethodName)),
                    ('Average Runtime', "%0.2f" % avg_runtime),
                    ('Baseline Runtime', "%0.2f" % baseline_runtime),
                    ('% Difference', "%0.2f" % delta),
                    ('Allowable Threshold', "%0.2f" % my_self.threshold),
                    ('Repetitions Performed', "%d" % my_self.repetitions),
                    ('Total Runtime', "%0.2f" % total_runtime),
                    ('Min Runtime', "%0.2f" % min_runtime),
                    ('Max Runtime', "%0.2f" % max_runtime),
                    ('Std Dev', "%0.2f" % std_dev),
                    ('% Std Dev', "%0.2f" % std_dev_pct)    
                    ]
            header = [x[0] for x in stats]
            data = [x[1] for x in stats]

            # dump statistics to a runtime_stats.csv file
            output_file_path = os.path.join(current_dir, 'runtime_stats.csv')
            existing = os.path.exists(output_file_path)
            mode = 'a' if existing else 'w'
            with open(output_file_path, mode) as f:
                if not existing:
                    f.write("%s\n" % ",".join(header))
                f.write("%s\n" % ",".join(data))
            self.assertGreater(my_self.threshold, delta, "assert delta < my_self.threshold")


        test_method = new.instancemethod(test_function,
                                         self, self.__class__)
        self.__dict__[ self._testMethodName + "*"] = test_method
        self._testMethodName = self._testMethodName + "*"
        super(PerformanceTestCase, self).run(result)

        
class GPPerfDiff(object):
    '''
    Utility class for checking performance deviation for performance test cases.
    '''
            
    @staticmethod
    def check_perf_deviation(test_name, baseline_file, current_runtime, threshold):
        runtime = -1
        with open(baseline_file, 'r') as f:
            for line in f:
                tokens = line.strip().split(',')
                if len(tokens) != 2:
                    continue
                if tokens[0] == test_name:
                    runtime = float(tokens[1])
                    break
        if runtime == -1:
            return (-1, 100)
        delta = int(((current_runtime - runtime)/runtime) * 100)
        return (runtime, delta)

