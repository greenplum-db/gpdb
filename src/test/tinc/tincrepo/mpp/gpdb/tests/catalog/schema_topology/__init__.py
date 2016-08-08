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
import shutil
from tinctest.lib import local_path

class ST_Etablefunc_util(object):

    def pre_process(self, product_version):
        """
        Just process all the sql and ans files to replace the path with new environment
        """
        ans_path = local_path('sqls/ddls/enhanced_tables')
        for ans_file in os.listdir(ans_path):
            if ans_file.endswith('4.3.99') and product_version.startswith('4.3.99'):
                shutil.move(os.path.join(ans_path, ans_file), os.path.join(ans_path, ans_file.split('.4.3.99')[0]))
