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

from mpp.models import SQLTestCase
'''
Trigger sqls for create_tests
'''
class TestTriggerSQLClass(SQLTestCase):
    '''
    This class contains all the sqls that are part of the trigger phase
    The sqls in here will get suspended by one of the faults that are triggered in the main run
    @gpdiff False
    '''
    sql_dir = 'sql/'

    @classmethod
    def setUpClass(cls):
        """
        Since some operation in this scenario is blocked, we want to run
        only aimed SQLs without unexpected setup.  Just make this no-op.
        """
        pass
