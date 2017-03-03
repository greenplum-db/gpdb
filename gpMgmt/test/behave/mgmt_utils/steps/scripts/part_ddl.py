#!/usr/bin/env python
import sys
import datetime

if len(sys.argv) != 3:
    print "%s YEAR NUMYEARS" % sys.argv[0]
    sys.exit(0)

PART_ROW_COUNT = 3000000
YEAR = int(sys.argv[1])
YEAR_COUNT = int(sys.argv[2])

description = """abcdefghijklmnopqrstuvwqyz.?{}-abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}
abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}"""

print "create table ao_part_table (eventdate date, data int, description varchar(256)) \
        with (compresstype=zlib, appendonly=true, orientation=row) \
       distributed by (eventdate) partition by range (eventdate) \
       (start (date '%s-01-01') end (date '%s-01-02') every (interval '1 day'));" % (YEAR, YEAR+YEAR_COUNT)
print "insert into ao_part_table select '%s-01-01', generate_series(1,%d), '%s';" % (YEAR, PART_ROW_COUNT, description)

print "create table co_part_table (eventdate date, data int, description varchar(256)) \
       with (compresstype=zlib, appendonly=true, orientation=column) \
       distributed by (eventdate) partition by range (eventdate) \
       (start (date '%s-01-01') end (date '%s-01-02') every (interval '1 day'));" % (YEAR, YEAR+YEAR_COUNT)
print "insert into co_part_table select '%s-01-01', generate_series(1,%d), '%s';" % (YEAR, PART_ROW_COUNT, description)

print "create table part_table (eventdate date, data int, description varchar(256)) \
        with (appendonly=true, orientation=row) \
       distributed by (eventdate) partition by range (eventdate) \
       (start (date '%s-01-01') end (date '%s-01-02') every (interval '1 day'));" % (YEAR, YEAR+YEAR_COUNT)
print "insert into part_table select '%s-01-01', generate_series(1,%d), '%s';" % (YEAR, PART_ROW_COUNT, description)


