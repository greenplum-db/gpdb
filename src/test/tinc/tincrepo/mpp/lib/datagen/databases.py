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

from mpp.lib.datagen import TINCTestDatabase
from mpp.lib.datagen.tpch import TPCHDatabase
from mpp.lib.datagen.world.db import WorldDatabase
from mpp.lib.datagen.functionproperty.db import FunctionPropertyTestDatabase
from mpp.lib.datagen.functionproperty.db_builtin import BuiltinFunctionPropertyTestDatabase
from mpp.lib.datagen.groupingfunction.db import GroupingFunctionTestDatabase
from mpp.lib.datagen import TINCDatagenException
from mpp.lib.datagen.dataset import DataSetDatabase
from mpp.lib.datagen.dispatch.db import DispatchSkewDatabase

# Global database dict for supported databases. Each test case will be associated with a
# specific database generator whose setup will be called during the setup of a test case.

TINC_TEST_DATABASE = 'gptest'

__databases__ = {}

__databases__[TINC_TEST_DATABASE] = TINCTestDatabase(database_name=TINC_TEST_DATABASE)

__databases__['analyze_db'] = TINCTestDatabase(database_name='analyze_db')

#Database for CTE tests
__databases__['world_db'] = WorldDatabase()

#Databases for function property tests
__databases__['functionproperty'] = FunctionPropertyTestDatabase(database_name = 'functionproperty')
__databases__['builtin_functionproperty'] = BuiltinFunctionPropertyTestDatabase(database_name = 'builtin_functionproperty')

#Database for grouping functions
__databases__['groupingfunction'] = GroupingFunctionTestDatabase(database_name = 'groupingfunction')

#Database for grouping functions
__databases__['splitdqa'] = TPCHDatabase(database_name = 'splitdqa')

#database for partitioning/partitionoindex
__databases__['ptidx'] = TINCTestDatabase(database_name='ptidx')

# Database for query-finish
__databases__['queryfinish'] = DispatchSkewDatabase()

# Database for Memory Accounting Tests
__databases__['memory_accounting'] = DataSetDatabase(database_name='memory_accounting')

# Database for Runaway query termination
__databases__['runaway_query'] = DataSetDatabase(database_name='runaway_query')
