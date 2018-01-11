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

import ConfigParser
import os
import re
import sys

from contextlib import closing
from datetime import datetime
from StringIO import StringIO
from unittest2.runner import _WritelnDecorator


from mpp.models import OptStacktrace, OptStackFrame

import tinctest

import unittest2 as unittest

class StackTraceHandlerTests(unittest.TestCase):
    """
    This will encompass tests for the stack trace handler utility.
    """
    def test_sanity_construction(self):
        stack = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test.dxl'))
        self.assertIsNotNone(stack)
        self.assertTrue(len(stack.text) > 0)
        self.assertEqual(stack.binary, 'postgres')
        self.assertEqual(len(stack.threads), 1)

    def test_opt_stacktrace_thread(self):
       stack = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test.dxl'))
       thread = stack.get_thread(0)
       self.assertIsNotNone(thread)
       self.assertEqual(thread.number, 0)
       self.assertEqual(len(thread.frames), 8)
       self.assertTrue(len(thread.text) > 0)

    def test_opt_stackt_frame(self):
       stack = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test.dxl'))
       thread = stack.get_thread(0)
       self.assertIsNotNone(thread)
       frame = thread.frames[0]
       self.assertEqual(frame.function, 'gpos::CException::Raise')
       self.assertEqual(frame.number, 1)
       self.assertEqual(frame.address, '0x00000000013313a5')
       self.assertEqual(frame.line, 165)
       self.assertEqual(frame.file, None)
       self.assertEqual(frame.text, '1    0x00000000013313a5 gpos::CException::Raise + 165')

    def test_first_relevant_stack_frame(self):
        stack = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test.dxl'))
        self.assertIsNotNone(stack)
        thread = stack.get_thread(0)
        frame = thread.get_first_relevant_frame()
        self.assertEqual(frame.function, 'gpdxl::CDXLUtils::PphdxlParseDXLFile')
        self.assertEqual(frame.number, 2)
        self.assertEqual(frame.address, '0x0000000001ba33b8')
        self.assertEqual(frame.line, 888)
        self.assertEqual(frame.file, None)
        self.assertEqual(frame.text, '2    0x0000000001ba33b8 gpdxl::CDXLUtils::PphdxlParseDXLFile + 888')    

    def test_hash_same(self):
        stack = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test.dxl'))
        self.assertIsNotNone(stack)
        thread = stack.get_thread(0)
        hash1 = thread.hash(5)
        hash2 = thread.hash(5)
        self.assertEqual(hash1, hash2)

    def test_hash_different(self):
        stack1 = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test.dxl'))
        self.assertIsNotNone(stack1)
        stack2 = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test2.dxl'))
        self.assertIsNotNone(stack2)
        thread1 = stack1.get_thread(0)
        thread2 = stack2.get_thread(0)
        self.assertNotEqual(thread1.hash(5), thread2.hash(5))

    def test_hash_number_of_frames(self):
        stack = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test.dxl'))
        self.assertIsNotNone(stack)
        thread = stack.get_thread(0)
        hash1 = thread.hash(4)
        hash2 = thread.hash(3)
        self.assertNotEqual(hash1, hash2)

    def test_hash_with_fewer_frames(self):
        stack = OptStacktrace.parse('dxl', dxl=os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test3.dxl'))
        self.assertIsNotNone(stack)
        thread = stack.get_thread(0)
        hash1 = thread.hash(10)
        hash2 = thread.hash(3)
        self.assertEqual(hash1, hash2)
