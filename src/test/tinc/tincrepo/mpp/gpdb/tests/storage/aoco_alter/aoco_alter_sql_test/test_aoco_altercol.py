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

from mpp.models import SQLTestCase
'''
Test AOCO alter table add/drop column 
The suit tests the alter table add/drop column for Append Only, Column oriented tables.
Starting 4.3.2 aocs alter table add column has gone through performance optimization
The test suit makes sure no functionality is broken due to this optimization
'''

class AOCOAlterColumnTestCase(SQLTestCase):
    """
    @optimizer_mode off
    @tags ORCA
    @product_version gpdb: [4.3.2.0-]
    """

    """
    Collection of various combinations
    """
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'

