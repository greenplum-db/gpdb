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

import optparse
import os
import shlex
import sys

from collections import namedtuple
from itertools import groupby

import tinctest

from tinctest.main import TINCTestProgram
from tinctest.suite import TINCTestSuite
from optparse import OptionParser
from tincmmgr import TINCMMException
from tincmmgr.grammar import TINCMMQueryHandler
from tincmmgr.tincmm import TINCTestSuiteMM
from tincmmgr.parser import TINCLogFileParser
from tincmmgr.config import tincmm_log_config
from tincmmgr.grammar import TINCMMQueryHandler

def split_into_buckets(lst, distribution):
    """
    Given a lst or a sequence or anything slice-able,
    this splits the sequence into smaller lists based on the distribution

    @param lst: A list or a sequence that needs to be split
    @type lst: list / sequence

    @param distribution: A list of percentage distribution that will be used for splitting the sequence into
                         smaller lists. Should add up to 100
    @type distribution: list 
    """
    if sum(distribution) != 100:
        raise Exception("Invalid distribution list. Should add up to 100")

    start_idx = 0

    sum_distribution = 0
    for d in distribution:
        sum_distribution += d
        stop_idx = int((sum_distribution/100.0) * len(lst))
        yield lst[start_idx:stop_idx]
        start_idx = stop_idx


class TINCMMProgram(object):

    log_file = os.path.join(os.getcwd(), 'tincmm.log')

    def __init__(self, argv=None):
        self.test_suite = None
        self.queries = []
        self.logfiles = []
        self.discover_options = None

        if not argv:
            argv = sys.argv

        try:
            self.parse_args(argv)
            self.run()
        except Exception,e:
            tinctest.logger.exception(e)
            raise TINCMMException("tincmm encountered an error: %s" %e)

    def parse_args(self, argv):
        parser = OptionParser()
        parser.add_option("-d", "--tinc-discover-options", action="store", type="string",
                          dest="discover_options",
                          help="tinc.py discover options to find tests against which tincmm queries will be applied.")
        parser.add_option("-q", "--tincmm-query", action="append", dest="queries",
                          help="TINCMM query that should be applied to the tests. Can be specified multiple times")
        parser.add_option("-l", "--logfile", action="append", type="string",
                          dest="logfile",
                          help=optparse.SUPPRESS_HELP)

        (options, args) = parser.parse_args(argv)

        if not options.discover_options and not options.logfile:
            parser.print_help()
            parser.error("Option -d is required.")

        self.discover_options = options.discover_options
        
        if options.discover_options and not options.queries:
            parser.print_help()
            parser.error("Atleast one tincmm query should be provided.")

        if options.logfile and options.queries:
            parser.print_help()
            parser.error("-q should not be provided with -l option. -l uses queries from config.py")

        if options.logfile and options.discover_options:
            parser.print_help()
            parser.error("Option -d and -l are mutually exclusive")

        if options.queries:
            self.queries.extend(options.queries)

        if options.logfile:
            self.logfiles.extend(options.logfile)

        for f in self.logfiles:
            if not os.path.exists(f) or not os.path.isfile(f):
                parser.error("Logfile %s does not exist" %f)

    def run(self):
        if self.logfiles:
            for logfile in self.logfiles:
                TINCMMProgram._handle_logfile(logfile, tincmm_log_config)
            return

        if self.discover_options:
            # construct an argument list out of discover_options
            argv = shlex.split(self.discover_options)
            self.test_suite = TINCTestProgram.construct_discover_test_suite(argv)[0]
        
        # Construct mm objects for the test suite
        # Construct a query handler
        query_handler = TINCMMQueryHandler(self.queries)
        test_suite_mm = TINCTestSuiteMM(self.test_suite)
        test_suite_mm.apply_queries(query_handler)

    @staticmethod
    def _handle_logfile(logfile, tincmm_log_config):
        with open(TINCMMProgram.log_file, 'a') as f:
            f.write("*" * 80)
            f.write("\nTINCMM Log Config\n")
            f.write("Bucket distribution: %s \n" %(tincmm_log_config.bucket_distribution))
            f.write("Bucket queries: %s \n" %(tincmm_log_config.bucket_queries))
            f.write("*" * 80 + "\n")

            f.write("*" * 80 + "\n")
            f.write("Parsing log file: %s \n\n" %logfile)
            log_parser = TINCLogFileParser(logfile)
            test_buckets = TINCMMProgram._construct_test_suite_buckets(log_parser, tincmm_log_config, f)
            f.write("Finished parsing log file: %s \n\n" %logfile)
            f.write("*" * 80 + "\n")
            TINCMMProgram._apply_mm_queries(log_parser, test_buckets, tincmm_log_config)

    @staticmethod
    def _construct_test_suite_buckets(log_parser, tincmm_log_config, log_handler):
        """
        Takes a log file parser object , distributes the test suite into
        different buckets as configured in tincmmgr/config.py

        The distribution algorithm is simple:
        Group by test class, sort by runtime, and distribute tests into
        different buckets based on bucket_distribution in config.py

        Takes as input a TINCLogFileParser object and a tincmm_log_config that
        has information about how to distribute the tests
        """
        test_buckets = []
        for x in xrange(len(tincmm_log_config.bucket_distribution)):
            test_buckets.append([])
        
        test_tuple = namedtuple('test_tuple', 'name result duration')
        # Group by test class name. For a test a.b.c.d.test_method, test class is a.b.c.d
        for key, group in groupby(sorted(log_parser.dict_of_test_tuples), lambda x: x.rpartition('.')[0]):
            # For each test class, sort by runtime and distribute them into different buckets
            test_class_tuples = [test_tuple(name=item, result=log_parser.dict_of_test_tuples[item].result,
                                            duration=log_parser.dict_of_test_tuples[item].duration) for item in group]
            # Sort each test class by duration
            sorted_test_class_tuples = sorted(test_class_tuples, key = lambda x: float(x.duration))

            log_handler.write("Distributing test class: %s \n\n" %key)
            idx = 0
            buckets = list(split_into_buckets(sorted_test_class_tuples, tincmm_log_config.bucket_distribution))
            for bucket in buckets:
                log_handler.write("=" * 80 + "\n") 
                log_handler.write("Bucket: %s\n" %idx)
                for tuple in bucket:
                    # Ignore failed tests
                    if tuple.result.lower() != 'pass':
                        log_handler.write("Test case: %s Duration: %s Result: %s --> !! IGNORED !!\n" %(tuple.name, tuple.duration, tuple.result))
                        continue
                    log_handler.write("Test case: %s Duration: %s Result: %s \n" %(tuple.name, tuple.duration, tuple.result))
                    test_case = log_parser.dict_of_test_tuples[tuple.name].test_case_object
                    test_buckets[idx].append(tuple)
                    # Limitation: tincmm log handler will always work on the method level docstrings
                    # It is difficult to find here if all tests 
                idx = idx + 1
                log_handler.write("=" * 80 + "\n")

        return test_buckets

    @staticmethod
    def _apply_mm_queries(logparser, test_buckets, tincmm_log_config):
        """
        test_buckets contains individual bucket of test tuples based on the above distribution algorithm.

        Construct a test suite for each of the bucket and apply mm queries from tincmm_log_config
        """
        for bucket, idx in zip(test_buckets, range(len(test_buckets))):
            suite = TINCTestSuite()
            for test_tuple in bucket:
                test_case = logparser.dict_of_test_tuples[test_tuple.name].test_case_object
                # LIMITATION: We always set _all_tests_loaded to False for tincmm log parser.
                # It is difficult to find out at this point whether all tests from a test class
                # belongs to the same bucket. Most common use cases will split a test class
                # into multiple buckets and hence we assume that tincmm can operate on the
                # method docstrings
                test_case.__class__._all_tests_loaded = False
                suite.addTest(test_case)

            query_handler = TINCMMQueryHandler(tincmm_log_config.bucket_queries[idx])
            tinc_test_suite_mm = TINCTestSuiteMM(suite)
            tinc_test_suite_mm.apply_queries(query_handler)
            
                
            
        
