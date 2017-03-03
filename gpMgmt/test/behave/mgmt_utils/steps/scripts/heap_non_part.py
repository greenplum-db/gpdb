#!/usr/bin/env python
import sys

if len(sys.argv) != 2:
    print "%s TABLECOUNT" % sys.argv[0]
    sys.exit(0)
NON_PART_TABLE_COUNT = int(sys.argv[1])

for i in range(NON_PART_TABLE_COUNT):
    tname = "heap_table_%s" % (i+1)
    print "create table %s (id int, description varchar(256)) with (appendonly=false) distributed by (id);" % tname
    print "insert into %s select * from reference;" % tname


