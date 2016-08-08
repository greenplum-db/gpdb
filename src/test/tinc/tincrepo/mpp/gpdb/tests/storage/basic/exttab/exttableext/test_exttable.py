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

import os

from mpp.models import SQLTestCase
from tinctest.lib import local_path, run_shell_command

class ExternalTableExtensionTests(SQLTestCase):
    """
    @tags list_mgmt_expand
    @product_version gpdb: [4.3-]
    """

    sql_dir = 'sql/'
    ans_dir = 'expected/'

    def tearDown(self):
        # execute commands to remove output files created in primary segment's data directories
        # This cleans up all the files exported by demoprot on the primary segments.
        # expects removeOutputFile.sql in sql/setup to be run and its corredponfing out file available in
        # out_dir/setup/removeOutputFile.out
        out_file = os.path.join(self.get_out_dir(), 'setup', 'removeOutputFile.out')
        with open(out_file) as f:
            for line in f:
                if (line.find('gpssh ')>=0 and line.find('SELECT')<0):
                    ok = run_shell_command(line)
                    if not ok:
                        raise Exception('Output file remove operation error: %s' %line)
        super(ExternalTableExtensionTests, self).tearDown()

