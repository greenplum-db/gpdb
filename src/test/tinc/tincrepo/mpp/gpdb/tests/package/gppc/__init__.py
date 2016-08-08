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
from tinctest.lib import local_path

class Gppc_util(object):

    def pre_process(self):
        """
        Just process all the sql and ans files to replace the path with new environment
        """
        for sql_file in os.listdir(local_path('sql')):
            if sql_file.endswith('.t'):
                f_input = open(sql_file, 'r')
                f_output = open(sql_file.split('.t')[0], 'w')
                for line in f_input:
                    if line.find('%MYD%') >= 0:
                        f_output.write(line.replace('%MYD%', local_path('')))
                f_input.close()
                f_output.close()

        for ans_file in os.listdir(local_path('expected')):
            if ans_file.endswith('.t'):
                f_input = open(ans_file, 'r')
                f_output = open(ans_file.split('.t')[0], 'w')
                for line in f_input:
                    if line.find('%MYD%') >= 0:
                        f_output.write(line.replace('%MYD%', local_path('')))
                f_input.close()
                f_output.close()

