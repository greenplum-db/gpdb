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
import tinctest
from mpp.models import MPPTestCase
from mpp.lib.gppkg.gppkg import Gppkg

gppkg = Gppkg()
pkgname = 'plperl'
class CompatiblityMPPTestCase(MPPTestCase):

    def __init__(self, methodName):
        super(CompatiblityMPPTestCase, self).__init__(methodName)

    @classmethod
    def setUpClass(self):
        super(CompatiblityMPPTestCase, self).setUpClass()
        gppkg.run_gppkg_uninstall(pkgname)

    def test_install_should_fail(self):
        """@product_version gpdb: [4.3.5.0 -]"""   	
        "Old package on the new database which is above the version of 4.3.5.0 should fail"
        gppkg = Gppkg()
        build_type = None
        if os.environ.get("BUILD_TYPE"):
            build_type = os.environ["BUILD_TYPE"]
        os.environ["BUILD_TYPE"] = 'rc'
        with self.assertRaisesRegexp(Exception, 'Failed to install'):
            gppkg.gppkg_install(product_version='4.3.4.0', gppkg=pkgname)
        if build_type is not None:
            os.environ["BUILD_TYPE"] = build_type
        existed, _ = gppkg.check_pkg_exists(pkgname)
        self.assertFalse(existed)
