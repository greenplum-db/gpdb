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

from mpp.models import MPPTestCase
from tinctest.lib import local_path

from mpp.lib.PSQL import PSQL

from mpp.gpdb.tests.storage.transaction_management.mpp19973 import table_exists, function_exists

class mpp19973(MPPTestCase):
	"""
	
	@description Automation for MPP-19973
	@tags mpp19973
    @product_version gpdb: [4.2.6.1- main]
	"""

	def test_outof_shmm_exit_slots(self):
		"""
		The issue of MPP-19973 is that a shmem exit callback to reset 
		a temporary namespace is not removed when the temporary namespace is
		reset.

		In situations, where a temporary namespace is multiple times reset
		because of an exception in a subtransaction, the callbacks
		use up all shmem_exit slots.
		"""

		sql_setup_file = local_path('mpp19973_setup.sql')
		PSQL.run_sql_file(sql_file=sql_setup_file)

		# Test case setup verification
		self.assertTrue(table_exists("foo"))
		self.assertTrue(function_exists("testfn"))
	
		sql_file = local_path('mpp19973.sql')
		out_file = local_path('mpp19973.out')
		PSQL.run_sql_file(sql_file=sql_file,
			out_file=out_file, output_to_file=True)

		# There will be different error messages in the output, but
		# we should not run out of shmem exit slots.
		self.assertNotRegexpMatches(open(out_file).read(), 
			"out of on_shmem_exit slots",
			"Database should not run out of shmem_exit slots")

