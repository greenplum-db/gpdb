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
from mpp.lib.gppkg.gppkg import Gppkg
from mpp.models import SQLTestCase
from tinctest.lib import run_shell_command

class SubtLimitTestCase(SQLTestCase):
    """ 
    @optimizer_mode off
    @tags gppkg
    """

    sql_dir = 'sql/'
    ans_dir = 'expected'
    out_dir = 'output/'

    @classmethod
    def setUpClass(cls):
        """
        Checking if plperl package installed, otherwise install the package
        """
        super(SubtLimitTestCase, cls).setUpClass()
        cmd = 'gpssh --version'
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command (cmd, 'check product version', res)
        gppkg = Gppkg()
        product_version = res['stdout']
        gppkg.gppkg_install(product_version, 'plperl')
