#!/bin/bash

# NON-PARTITION DATA
./reference_non_part.py | psql -e perftest 
./ao_non_part.py 2500| psql -e perftest &
./co_non_part.py  2500 | psql -e perftest &
./heap_non_part.py 10000 | psql -e perftest &
wait

# CREATE PARTITIONED TABLES AND DON'T LOAD DATA UNTIL DONE
./part_ddl.py 2005 8 | psql -e perftest

# LOAD PARTITIONED DATA
for i in `seq 2005 2012`; do
    ./part_data.py $i 2005 50 | psql -e perftest  &
done
wait
