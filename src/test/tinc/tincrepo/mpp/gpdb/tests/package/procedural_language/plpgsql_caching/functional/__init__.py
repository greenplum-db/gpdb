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

import fileinput
import os, re
from tinctest.lib import local_path
class Plpg:

    def pre_process(self, sql_files=[], ans_files=[]):
        for sql_file in sql_files:
            file = local_path(sql_file)
            if os.path.isfile(file):
                self.replace(file, ' modifies sql data', '')
        for ans_file in ans_files:
            file = local_path(ans_file)
            if os.path.isfile(file):
                self.replace(file, ' modifies sql data', '')

    def replace(self, file = None, template=None, newContent=None):    
        for line in fileinput.FileInput(file,inplace=1):
            line = re.sub(template, newContent, line)
            print str(re.sub('\n','',line))
