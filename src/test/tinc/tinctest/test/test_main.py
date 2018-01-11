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
import sys

import unittest2 as unittest

from tinctest.main import TINCSchedule, TINCException

class TINCScheduleTests(unittest.TestCase):
    def test_schedule_parsing(self):
        sched_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'test_schedule')
        sched_obj = TINCSchedule(sched_file)
        self.assertEquals(sched_obj.schedule_name, "Test Schedule")
        self.assertEquals(sched_obj.time_limit, 10)
        self.assertTrue(sched_obj.options.startswith("discover -s folderx"))
        envvar = sched_obj.options.find("$HOME")
        self.assertEquals(envvar, -1)

    def test_no_schedule_file(self):
        sched_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'non_existant_schedule')
        self.assertRaises(TINCException, TINCSchedule, sched_file)

    def test_invalid_schedule_file(self):
        sched_file = os.path.join(os.path.dirname(sys.modules[self.__class__.__module__].__file__), 'invalid_schedule_file')
        self.assertRaises(TINCException, TINCSchedule, sched_file)
