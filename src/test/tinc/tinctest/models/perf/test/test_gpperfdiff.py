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

import unittest2 as unittest
from tinctest.lib import local_path
from tinctest.models.perf import GPPerfDiff

class GPPerfDiffTests(unittest.TestCase):
    def test_perf_diff(self):
        self.assertTrue(GPPerfDiff.check_perf_deviation("test_query_001", local_path("baseline_4.2.2.csv"), 1573, 5))

