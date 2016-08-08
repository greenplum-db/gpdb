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
"""

import re,sys

inputfile = sys.argv[1]
outputfile = sys.argv[2]

file = open(inputfile)
outfile = open(outputfile, 'w')
record = ""

for line in file:
    if (line.strip() == ""):
        record = re.sub('	$','',record)
        outfile.write(record+'\n')
        record = ""
    else:
        match = re.search('(Column \d+):(.+)', line)
        if (match != None):
            record += match.group(2)+'\t'
        match = re.search('(Column \d+:)\B', line)
        if (match != None):
            record += '\\N\t'

file.close()
outfile.close()


