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
from tinctest.models.scenario import ScenarioTestCase
from mpp.models.mpp_tc import MPPDUT

@tinctest.skipLoading("Test model. No tests loaded.")
class MPPScenarioTestCase(ScenarioTestCase):
    DUT = MPPDUT()
    tinctest.logger.info(DUT)

    def get_product_version(self):
        """
        This function is used by TINCTestCase to determine the current DUT version.
        It uses this information, along with @product_version, to determine if a test case
        should run in this particular DUT.

        @return: A two-tuple containing name and version of the product where test is executed
        @rtype: (string, string)
        """
        return (self.DUT.product, self.DUT.version_string)
