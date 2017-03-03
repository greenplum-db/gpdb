#!/usr/bin/env python
import sys
import datetime

if len(sys.argv) != 4:
    print "%s YEAR COPYYEAR NUMDAYS" % sys.argv[0]
    sys.exit(0)

YEAR = int(sys.argv[1])
COPYYEAR = int(sys.argv[2])
DAY_COUNT = int(sys.argv[3])

description = """abcdefghijklmnopqrstuvwqyz.?{}-abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}
abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}"""

date_iterator = datetime.date(YEAR, 01, 02)
delta = datetime.timedelta(days=1)

for i in range(DAY_COUNT):

    print "INSERT INTO ao_part_table select '%s', data, description from ao_part_table where eventdate = '%s-01-01';"  \
        % (str(date_iterator), COPYYEAR)

    print "INSERT INTO co_part_table select '%s', data, description from ao_part_table where eventdate = '%s-01-01';"  \
        % (str(date_iterator), COPYYEAR)

    print "INSERT INTO part_table select '%s', data, description from part_table where eventdate = '%s-01-01';"  \
        % (str(date_iterator), COPYYEAR)

    date_iterator = date_iterator + delta



