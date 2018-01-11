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

import os
import shlex
import sys
import ConfigParser
import StringIO
import unittest2 as unittest

from discovery import TINCDiscoveryQueryHandler

try:
    from unittest2.signals import installHandler
except ImportError:
    installHandler = None

from tinctest.loader import TINCTestLoader
from tinctest.case import TINCTestCase

class TINCException(Exception):
    """
    Base class for all tinc exceptions.
    """
    pass

class TINCSchedule(object):
    """
    Encapsulates the parameters given in a schedule file
    Schedule files are searched in the following folders:
      - if not, fully qualified name search for the file in 
          - TINCREPOHOME/schedules
          - PWD
    """
    def __init__(self, schedule_file):
        self.schedule_file = schedule_file
        self.schedule_name = None
        self.time_limit = None
        self.options = None
        self._parse_schedule_file()

    def _parse_schedule_file(self):
        if not self.schedule_file:
            raise TINCException('schedule file name not given')

        file_to_parse = None
        if os.path.isabs(self.schedule_file) and os.path.exists(self.schedule_file):
            file_to_parse = self.schedule_file
        else:
            if os.environ.has_key("TINC_SCHEDULE_DIR"):
                file_env_folder = os.path.join(os.environ["TINC_SCHEDULE_DIR"], self.schedule_file)
                if os.path.exists(file_env_folder):
                    file_to_parse = file_env_folder

            if not file_to_parse:
                if os.environ.has_key("PWD"):
                    file_sched_folder = os.path.join(os.environ["PWD"], 'schedules', self.schedule_file)
                    if os.path.exists(file_sched_folder):
                        file_to_parse = file_sched_folder
                    if not file_to_parse:
                        file_curdir = os.path.join(os.environ["PWD"], self.schedule_file)
                        if os.path.exists(file_curdir):
                            file_to_parse = file_curdir

        if not file_to_parse:
            raise TINCException("schedule file [%s] doesn't exist" %self.schedule_file)

        cfg_string = '[Schedule]\n'
        cfg_string = cfg_string + open(file_to_parse).read()

        fd = StringIO.StringIO(cfg_string)
        cfg = ConfigParser.RawConfigParser()
        cfg.readfp(fd)

        for key, val in cfg.items('Schedule'):
            low_key = key.lower()
            if low_key == 'name':
                self.schedule_name = val
            elif low_key == 'timelimit':
                self.time_limit = int(val)
            elif low_key == 'options':
                self.options = os.path.expandvars(val)
            else:
                #logger not configured; 
                raise TINCException("unknown key [%s] in schedule file [%s]" %(key, self.schedule_file))

class TINCTestProgram(unittest.TestProgram):

    tinc_config = None

    def parseArgs(self, argv):
        """
        TINCTestProgram accepts some additional arguments that should be propogated to tests
        before they are run. This parses the additional arguments after calling out to
        the super class.
        """
        if len(argv) > 1 and argv[1].lower() == 'discover':
            self._do_discovery(argv[2:])
            return

        import getopt
        long_opts = ['help', 'verbose', 'quiet', 'failfast', 'catch', 'buffer', 'schedule']
        # Add custom options
        long_opts[len(long_opts):] = ['codeline=','testconfig=']
        try:
            options, args = getopt.getopt(argv[1:], 'hHvqfcbs:', long_opts)
            for opt, value in options:
                if opt in ('-h','-H','--help'):
                    self.usageExit()
                if opt in ('-q','--quiet'):
                    self.verbosity = 0
                if opt in ('-v','--verbose'):
                    self.verbosity = 2
                if opt in ('-f','--failfast'):
                    if self.failfast is None:
                        self.failfast = True
                    # Should this raise an exception if -f is not valid?
                if opt in ('-c','--catch'):
                    if self.catchbreak is None and installHandler is not None:
                        self.catchbreak = True
                    # Should this raise an exception if -c is not valid?
                if opt in ('-b','--buffer'):
                    if self.buffer is None:
                        self.buffer = True
                    # Should this raise an exception if -b is not valid?
                if opt in ('-s','--schedule'):
                    self.schedule = value
                    schedule_obj = TINCSchedule(self.schedule)
                    if not schedule_obj:
                        raise TINCExcpetion("Schedule file %s doesn't list any options to tinc.py" %self.schedule)
                    self.parseArgs([argv[0]] + shlex.split(schedule_obj.options))
                    return

                if opt in ('--testconfig'):
                    TINCTestCase.test_config.extend(value.split(','))

            if len(args) == 0 and self.defaultTest is None:
                # createTests will load tests from self.module
                self.testNames = None
            elif len(args) > 0:
                self.testNames = args
                if __name__ == '__main__':
                    # to support python -m unittest ...
                    self.module = None
            else:
                self.testNames = (self.defaultTest,)
            self.createTests()
        except getopt.error, msg:
            self.usageExit(msg)

    def _do_discovery(self, argv, Loader=TINCTestLoader):
        """
        TINCTestProgram provides a minimialistic change to _do_discovery, in which
        the Loader is made to be a TINCTestLoader. Without this change, TestProgram
        would use the base TestLoader, incapable of discovering and loading SQLTestCases.

        This code is copy/paste-d from unittest.TestProgram, simply because deferring
        to the parent in this instance was found to be cumbersome.
        """
        import optparse
        # handle command line args for test discovery
        self.progName = '%s discover' % self.progName
        
        # Following options are related to the running of tests through discovery
        options_list = []
        options_list.append(optparse.make_option('-v', '--verbose', dest='verbose', default=False,
                                                 help='Verbose output', action='store_true'))

        if self.failfast != False:
            options_list.append(optparse.make_option('-f', '--failfast', dest='failfast', default=False,
                                                     help='Stop on first fail or error',
                                                     action='store_true'))
        if self.catchbreak != False and installHandler is not None:
            options_list.append(optparse.make_option('-c', '--catch', dest='catchbreak', default=False,
                                                     help='Catch ctrl-C and display results so far',
                                                     action='store_true'))
        if self.buffer != False:
            options_list.append(optparse.make_option('-b', '--buffer', dest='buffer', default=False,
                                                     help='Buffer stdout and stderr during tests',
                                                     action='store_true'))
        
        test_suite, options = TINCTestProgram.construct_discover_test_suite(argv, Loader=TINCTestLoader,
                                                                            additional_discover_options = options_list,
                                                                            progName = self.progName
                                                                            )


        # only set options from the parsing here
        # if they weren't set explicitly in the constructor
        if self.failfast is None:
            self.failfast = options.failfast
        if self.catchbreak is None and installHandler is not None:
            self.catchbreak = options.catchbreak
        if self.buffer is None:
            self.buffer = options.buffer

        if options.verbose:
            self.verbosity = 2
        self.test = test_suite

    @staticmethod
    def construct_discover_test_suite(argv,Loader=TINCTestLoader, additional_discover_options = [], progName=''):
        import optparse
        parser = optparse.OptionParser()
        parser.prog = progName
        
        parser.add_option('-s', '--start-directory', dest='start', default=[], action='append',
                          help="Directory to start discovery ('.' default), can be specified multiple times to run discovery from different folders.")
        parser.add_option('-t', '--top-level-directory', dest='top', default=None,
                          help='Top level directory of project (defaults to start directory)')
        parser.add_option('-p', '--pattern', dest='pattern', default=[], action='append',
                          help="Module patterns to discover tests from('test*.py' default), can be specified multiple " + \
                          "times to discover tests from multiple patterns.")
                          
        parser.add_option('-q', '--query', dest='queries', action='append', default=None,
                          help='TINC test queries to filter discovered tests. Can be specified multiple times to provide multiple queries. Refer to ' + \
                          'documentation for more information on the queries supported.')

        parser.add_option('-d', '--dryrun', dest='dryrun', default=False,
                          help='No tests will be executed. Only test discovery will happen.',
                          action='store_true')

        
        for option in additional_discover_options:
            parser.add_option(option)

        options, args = parser.parse_args(argv)
        
        if len(args) > 2 :
            parser.print_help()
            sys.exit(1)

        for name, value in zip(('start', 'pattern'), args):
            setattr(options, name, [value])

        if not options.start:
            options.start = ['.']
        if not options.pattern:
            options.pattern = ['test*']

        start_dirs = options.start
        patterns = [x.strip('\'"') for x in options.pattern]
        top_level_dir = options.top

        loader = Loader()

        if options.queries:
            tinc_queries = [x.strip('\'"') for x in options.queries]
            query_handler = TINCDiscoveryQueryHandler(tinc_queries)
        else:
            query_handler = None
        test_suite = loader.discover(start_dirs, patterns, top_level_dir, query_handler, options.dryrun)
        return test_suite, options
