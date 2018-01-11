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

import random
import sys

def main(total_rows, number_of_error_rows):
    error_count = 0
    for i in xrange(number_of_error_rows):
        print "error_%s" %str(error_count)
        
    for i in xrange(total_rows - number_of_error_rows):
            print "%s|%s_number" %(i,i)
        
if __name__ == '__main__':
    total_rows = 20
    error_rows = 0
    if len(sys.argv) > 1:
        total_rows = int(sys.argv[1])
        error_rows = int(sys.argv[2])
    main(total_rows, error_rows)
