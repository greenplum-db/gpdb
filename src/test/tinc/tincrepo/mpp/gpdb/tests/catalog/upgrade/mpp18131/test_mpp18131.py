"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

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

from gppylib.commands.base import Command
from mpp.models import MPPTestCase

import os
import subprocess

class test_mpp18131(MPPTestCase):

    def test_mpp18131(self):
        """
        
        @description Test MPP-18131, GUC parameter handling issue during startup
        @created 2013-07-03 00:00:00
        @modified 2013-07-03 00:00:00
        @tags configuration GUC
        @product_version gpdb: [4.2.6.1- main]
        """

	# The core issue is that psql will segfault when invoked with specific PGOPTIONS.
	# This test detects whether the server crashes when invoking psql with
	# the offending PGOPTIONS. Note that psql exit status does not communicate whether
	# the server crashed or not, so we cannot inspect the return code. Instead, we
	# scan the stderr to see if the server died unexpectedly.
	# Steps:
        # 1. gpstart
        # 2. PGOPTIONS='-c allow_system_table_mods=ddl' psql
	# 3. inspect stderr for string identifying server crash

	# string identifying server crash
	crash_str = "server closed the connection unexpectedly"

	# invoke psql
        psql = Command('psql GUC configuration',
			 'PGOPTIONS="-c allow_system_table_mods=ddl" '
                         'psql -c "SELECT 1"')
        psql.run()

        results = psql.get_results()
	self.assertNotIn(crash_str, results.stderr);
