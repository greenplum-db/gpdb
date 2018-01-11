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

from tinctest.lib import local_path, run_shell_command

class GPDBUtil:

    def getGPDBVersion(self):
        """
        Returns the version of gpdb
        """
        cmd = 'gpssh --version'
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command (cmd, 'check product version', res)
        product_version = res['stdout'].split('gpssh version ')[1].split(' build ')[0]
        return product_version

    def isVersionOfMajor(self, major_version=None):
        """
        Check if the version of current gpdb matches any major gpdb version, eg: 4.2*, 4.3*
        """
        return self.getGPDBVersion().startswith(major_version)
