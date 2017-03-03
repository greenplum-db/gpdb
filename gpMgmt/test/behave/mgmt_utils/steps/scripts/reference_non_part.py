#!/usr/bin/env python
import sys

NON_PART_ROW_COUNT = 10000

description = """abcdefghijklmnopqrstuvwqyz.?{}-abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}
abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}abcdefghijklmnopqrstuvwqyz.?{}"""

print "create table reference (id int, description varchar(256)) distributed by (id);" 
print "insert into reference select generate_series(1,%s), '%s';" % (NON_PART_ROW_COUNT, description)

