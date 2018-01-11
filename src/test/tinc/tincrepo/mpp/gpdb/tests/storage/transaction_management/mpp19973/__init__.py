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

from mpp.lib.PSQL import PSQL

def table_exists(table_name, *args, **kwargs):
	""" 
	Checks if a table exists in the catalog.
	"""

	# TODO (dmeister): This creates an SQL injection, but it should not
	# be a problem for this purpose.
	table_exists_text_count = PSQL.run_sql_command(
		"SELECT 'table exists' FROM pg_class WHERE relname='%s'" % (table_name),
		*args, **kwargs).count("table exists")
	print table_exists_text_count
	return table_exists_text_count == 2

def function_exists(function_name, *args, **kwargs):
	"""
	Checks if a function exists in the catalog
	"""

	# TODO (dmeister): This creates an SQL injection, but it should not
	# be a problem for this purpose.
	function_exists_text_count = PSQL.run_sql_command(
		"SELECT 'function exists' FROM pg_proc WHERE proname='%s'" % (function_name),
		*args, **kwargs).count("function exists")
	return function_exists_text_count == 2

