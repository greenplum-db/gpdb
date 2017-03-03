#!/usr/bin/env python
import sys
import datetime

if len(sys.argv) != 5:
    print "%s START_YEAR START_MONTH START_DAY NUMDAYS" % sys.argv[0]
    sys.exit(0)

START_YEAR = int(sys.argv[1])
START_MONTH = int(sys.argv[2])
START_DAY = int(sys.argv[3])
DAY_COUNT = int(sys.argv[4])

date_iterator = datetime.date(START_YEAR, START_MONTH, START_DAY)
delta = datetime.timedelta(days=1)

for i in range(DAY_COUNT):

    print "INSERT INTO ao_part_table select * from ao_part_table where eventdate = '%s' limit 1;"  % (str(date_iterator))
    print "INSERT INTO co_part_table select * from co_part_table where eventdate = '%s' limit 1;"  % (str(date_iterator))
    print "INSERT INTO part_table select * from part_table where eventdate = '%s' limit 1;"  % (str(date_iterator))

    date_iterator = date_iterator + delta



