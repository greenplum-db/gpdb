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

Running workload parallely through ScenarioTestCase framework can
reduce the total amount of time, however, when vacuum full is also
being executed at the same time, it can cause deadlock problem.

In addition, concurrently alterring same relation is not allowed.

Therefore, this part is to run such workload in sequential way.
"""

from mpp.models import SQLTestCase

class VacuumTestCase(SQLTestCase):
    """
    @gucs gp_create_table_random_default_distribution=off
    """
    
    sql_dir = 'sql/'
    ans_dir = 'expected/'
    out_dir = 'output/'
