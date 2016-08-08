#!/usr/bin/env python
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

@summary: Test class for installing plperl language in a given GPDB system
"""
from mpp.gpdb.tests.package.procedural_language import ProceduralLanguage
from mpp.lib.gppkg.gppkg import Gppkg
from mpp.models import MPPTestCase
from tinctest.lib import run_shell_command

cmd = 'gpssh --version'
res = {'rc':0, 'stderr':'', 'stdout':''}
run_shell_command (cmd, 'check product version', res)
product_version = res['stdout'].split('gpssh version ')[1].split(' build ')[0]

class PlperlMPPTestCase(MPPTestCase):
    
    def __init__(self, methodName):
        self.pl = ProceduralLanguage()
        self.language = 'plperl'
        super(PlperlMPPTestCase, self).__init__(methodName)

    @classmethod
    def setUpClass(self):
        super(PlperlMPPTestCase, self).setUpClass()
        gppkg = Gppkg()
        gppkg.gppkg_install(product_version, 'plperl')

    def setUp(self):
        """
        @summary: Overrides setUp for gptest to check if current OS is supported for gppkg.  If not, test is skipped.
        
        """
        if self.pl.gppkg_os.find('rhel') < 0 and self.pl.gppkg_os.find('suse') < 0:
            self.skipTest('TEST SKIPPED: plperl is only supported on RHEL and SuSE. Skipping test.')

    def tearDown(self):
        pass
        
    def test_install_Plperl(self):
        """Install plperl"""
        if self.pl.language_in_db(self.language) == True:
            self.pl.drop_lanaguage_from_db(self.language)
        self.assertTrue(self.pl.create_language_in_db(self.language))


    def test_uninstall_Plperl(self):
        """uninstall plperl language"""
        if self.pl.language_in_db(self.language) == False:
            self.pl.create_language_in_db(self.language)
        self.assertTrue(self.pl.drop_lanaguage_from_db(self.language))
