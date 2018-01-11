#!/usr/bin/env python
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

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

String matching for Array of text. Uses regular expression
"""

import string
import re

class String():
    def __init__(self):
        pass
    
    def match(self, text, pattern):
        """
        Uses regular expression to match
        """
        return re.search(pattern, text)
    
    def match_array(self, arr, patterns):
        """
        Match an array of string
        """
        for pattern in patterns:
            if self.match(' '.join(arr), pattern) is None:
                return False
        return True # Matches all pattern

    def replace_array(self, content, search, replace):
        """
        Replace content of an array of string
        @param content: content
        @param search: search key
        @param replace: replace value
        """
        newline = []
        for line in content:
            newline.append(line.replace(search, replace))
        return newline
    
mstring = String()

