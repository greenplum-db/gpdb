#!/usr/bin/env python

import re, sys

def statusString(msg):
    if success:
        return "PASS"
    else:
        return "FAIL"

for line in sys.stdin.readlines():
    line = line.strip()
    if re.search('passed.*failed.*skipped', line):
        kvps = {}
        fields = line.split(',')
        print ""
        category = None
        number = None
        status = None
        total = 0
        success = True
        for field in range(len(fields)):
            parts = fields[field].split()
            if len(parts) == 3:
                number = int(parts[0])
                category = parts[1]
                status = parts[2]
            elif len(parts) == 2:
                number = int(parts[0])
                status = parts[1]
            else:
                continue
            total += number

            if status == 'failed' or status == 'undefined':
                if number > 0:
                    success = False
        
        print "BEHAVE_RESULTS=%s STATUS=%s MSG='%s'" % (category, statusString(success), line)
