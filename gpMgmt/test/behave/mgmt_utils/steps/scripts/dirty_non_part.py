#!/usr/bin/env python
import sys

if len(sys.argv) != 3:
    print "%s FIRST_TABLE LAST_TABLE" % sys.argv[0]
    sys.exit(0)
FIRST_TABLE = int(sys.argv[1])
LAST_TABLE = int(sys.argv[2])

for i in range(FIRST_TABLE, (LAST_TABLE+1)):
    tname = "ao_table_%s" % (i)
    print "insert into %s select * from reference limit 1;" % tname
    tname = "co_table_%s" % (i)
    print "insert into %s select * from reference limit 1;" % tname
