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

import tinctest

from tinctest.lib import run_shell_command

from mpp.models import SQLPerformanceTestCase
from mpp.models.sql_tc import _SQLTestCaseResult
from mpp.lib.PSQL import PSQL

import unittest2 as unittest

import fnmatch
import hashlib
import os
import socket
import shutil
import sys

from xml.dom import minidom

@tinctest.skipLoading("Test model. No tests loaded.")
class OptimizerSQLPerformanceTestCase(SQLPerformanceTestCase):
    """
    Inherits from SQLPerformanceTestCase and runs a performance test with additional optimizer gucs
    """
    def __init__(self, methodName, baseline_result = None, sql_file = None, db_name = None):
        super(OptimizerSQLPerformanceTestCase, self).__init__(methodName, baseline_result, sql_file, db_name)
        self.gucs.add('optimizer=on')
        self.gucs.add('optimizer_log=on')

    def _add_gucs_to_sql_file(self, sql_file, gucs_sql_file=None, optimizer=None):
        """
        Form test sql file by adding the defined gucs to the sql file
        @param sql_file Path to  the test sql file
        @returns Path to the modified sql file
        """
        ignore_gucs = False
        if not gucs_sql_file:
            gucs_sql_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file)) 
        if 'setup.sql' in gucs_sql_file or 'teardown.sql' in gucs_sql_file:
            shutil.copyfile(sql_file, gucs_sql_file)
            return gucs_sql_file
        #      gucs_sql_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql', '_gucs.sql'))
        with open(gucs_sql_file, 'w') as o:
            with open(sql_file, 'r') as f:
                # We make a dirty assumption that there is only one valid query block
                # in the sql file as in many other places. We have to re-visit this
                # when we tackle handling of multiple queries in a SQL file
                query_string = ''
                for line in f:
                    if (line.find('--') != 0):
                        query_string += line
                f.seek(0)
                for line in f:
                    if (line.find('--') != 0):
                        break
                    else:
                        o.write(line)
                o.write('\n\n-- start_ignore\n')
                # Add gucs and then add the line
                for guc_string in self.gucs:
                    o.write("SET %s;\n" %guc_string)
                for orca_guc_string in self.orcagucs:
                    o.write("%s;\n"%orca_guc_string)

                # Add explain of query to load optimizer libraries and md cache- QAINF-418
                # Note - Assuming just one valid query block
#                o.write("select disable_xform('CXformInnerJoin2IndexApply');\n")
                o.write('--Add explain of query to load optimizer libraries\n')
                o.write('EXPLAIN \n %s\n\n' %query_string.strip())
                o.write('\\timing on\n')
                o.write('-- end_ignore\n\n')
                for line in f:
                    o.write(line)
        self.test_artifacts.append(gucs_sql_file)
        return gucs_sql_file
        #self.gucs.add('optimizer_damping_factor_join = 1')

class OptStacktrace(object):
    """
    Given a minidump or a text containing stack trace, this parses the stacktrace element from the mini-dump / text and parses the stack trace into an object.
    """

    def __init__(self):
        """
        Initialize parser.
        """
        self.binary = 'postgres'
        self.threads = []
        self.text = ''

    @classmethod
    def parse(cls, type, dxl = None, text = None):
        """
        Parse stack trace from a minidump or a text and return a
        OptStacktrace object
        @param type -  'dxl' or 'text' specifying where to look for a stack trace
        @param dxl - location of the dxl file
        @param text - string containing a stack trace
        """
        if type != 'dxl' and type != 'text':
            tinctest.logger.warning("Unknown source type %s. Returning no stack." %(type))
            return None
        if type == 'dxl':
            return cls._parse_dxl_for_stack_trace(dxl)
        if type == 'text':
            return cls._parse_text_for_stack_trace(text)

    @classmethod
    def _parse_dxl_for_stack_trace(cls, dxl_file):
        if dxl_file == None or dxl_file == '':
            tinctest.logger.warning("No dxl specified. Returning no stack.")
            return None

        # Check if dxl exists
        if not os.path.exists(dxl_file):
            tinctest.logger.warning("Dxl file %s not found. Returning no stack" %(dxl_file))
            return None

        # parse dxl to find string between <dxl:Stacktrace> and </dxl:StackTrace>
        dxl_dom = minidom.parse(dxl_file)
        thread_elements = dxl_dom.getElementsByTagName("dxl:Thread")
        if len(thread_elements) == 0:
            tinctest.logger.warning("No threads found. Returning no stack")
        opt_stack = OptStacktrace()
        # find thread id
        for thread in thread_elements:
            # Look for an attribute "Id"
            thread_number = int(thread.getAttribute("Id"))
            thread_stack = ''
            for stack_node in thread.childNodes:
                if stack_node.nodeName == 'dxl:Stacktrace':
                    thread_stack = stack_node.firstChild.data
                    opt_stack.text += thread_stack
            opt_stack.threads.append(OptStacktraceThread.parse(thread_stack, thread_number))
        return opt_stack

    def get_thread(self, number):
        """
        Given a thread number, returns the corresponding OptSTacktraceThread object
        """
        for thread in self.threads:
            if thread.number == number:
                return thread
        return None

    def __str__(self):
        return self.text

class OptStacktraceThread(object):
    """
    Class representing one thread of a stack trace. Contains a list of OptStackFrame objects.
    """

    def __init__(self):
        self.frames = []
        self.number = 0
        self.text = ''
        self.first_opt_stack_frame = None

    @classmethod
    def parse(cls, text, number):
        """
        Parse a single thread's stack trace text and returns an OptStacktraceThread object
        """
        thread = OptStacktraceThread()
        thread.number = number
        thread.text = text
        # Get every line in the text and form a frame object
        for line in text.splitlines():
            thread.frames.append(OptStackFrame.parse(line))
        return thread

    def get_first_relevant_frame(self):
        """
        Returns the first relevant frame in the stack trace. Relevance
        here means the first non-gpos stack frame. We should refine this
        when we encounter special cases. For the following stack:
        1    0x000000000132f465 gpos::CException::Raise + 165
        2    0x0000000001b9f148 gpdxl::CDXLUtils::PphdxlParseDXLFile + 888
        3    0x000000000035450d COptTasks::PdrgPssLoad + 61,
        this should return the second frame
        """
        ret = False
        for frame in self.frames:
            if ret == True:
                return frame
            if 'gpos' in frame.function:
                ret = True

        # This means there was no frame with gpos functions and we return None
        return None

    def hash(self, number_of_frames):
        """
        Return a hash of the top 'number_of_frames' of the stack trace.
        """
        if len(self.frames) < number_of_frames:
            number_of_frames = len(self.frames)

        m = hashlib.md5()
        for i in xrange(number_of_frames):
            m.update(self.frames[i].text)
        return m.hexdigest()

    def __str__(self):
        return self.text

class OptStackFrame(object):
    """
    Single stack frame element representing a single function call in a stack.
    Each frame is assumed to be of the following format:
    1    0x000000000132f465 gpos::CException::Raise + 165
    """

    def __init__(self):
        self.function = None
        self.number = -1
        self.address = None
        self.file = None
        self.line = -1
        self.text = None

    @classmethod
    def parse(cls, text):
        """
        Given a single line of stack trace, parses the string and returns an OptStackFrame object
        """
        frame = OptStackFrame()
        frame.text = text
        frame_elements = text.split()
        # Assuming the following format
        # "1    0x000000000132f465 gpos::CException::Raise + 165""
        # TODO - Check if we will have other formats
        frame.function = frame_elements[2]
        frame.number = int(frame_elements[0])
        frame.address = frame_elements[1]
        frame.line = int(frame_elements[4])
        return frame

    def __str__(self):
        return self.text

class OptimizerTestResult(_SQLTestCaseResult):
    """
    A listener for OptimizerSQLTestCase that will collect mini dumps when a test case fails
    """

    def addFailure(self, test, err):
        """
        Collect mini dumps for test queries during a failure
        """
        dxl_file = test._collect_mini_dump()
        super(OptimizerTestResult, self).addFailure(test, err)

    def _get_stack_info(self, dxl_file, stack_frames):
        stack = OptStacktrace.parse(type = 'dxl', dxl = dxl_file)
        stack_hash = ''
        stack_trace = ''
        stack_owner = ''
        if stack is not None:
            stack_hash = stack.get_thread(0).hash(stack_frames)
            stack_trace = stack.get_thread(0).text
        return (stack_trace, stack_hash)
