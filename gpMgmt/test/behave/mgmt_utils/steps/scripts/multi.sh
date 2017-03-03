#!/bin/bash

#./dirty_non_part.py 1 25 | psql -e perftest
#./dirty_part.py 2005 01 01 4 | psql -e perftest
#time gpcrondump -x perftest --incremental -a

#./dirty_non_part.py 26 50 | psql -e perftest
#./dirty_part.py 2005 01 05 4 | psql -e perftest
#time gpcrondump -x perftest --incremental -a

#./dirty_non_part.py 51 75 | psql -e perftest
#./dirty_part.py 2005 01 09 4 | psql -e perftest
#time gpcrondump -x perftest --incremental -a

#./dirty_non_part.py 76 100 | psql -e perftest
#./dirty_part.py 2005 01 13 4 | psql -e perftest
#time gpcrondump -x perftest --incremental -a

#./dirty_non_part.py 101 125 | psql -e perftest
#./dirty_part.py 2005 01 17 4 | psql -e perftest
#time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 126 150 | psql -e perftest
./dirty_part.py 2005 01 21 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 151 175 | psql -e perftest
./dirty_part.py 2005 01 25 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 176 200 | psql -e perftest
./dirty_part.py 2005 01 29 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 201 225 | psql -e perftest
./dirty_part.py 2005 02 04 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 226 250 | psql -e perftest
./dirty_part.py 2005 02 08 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 251 275 | psql -e perftest
./dirty_part.py 2006 01 01 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 276 300 | psql -e perftest
./dirty_part.py 2006 01 05 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 301 325 | psql -e perftest
./dirty_part.py 2006 01 09 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 326 350 | psql -e perftest
./dirty_part.py 2006 01 13 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

./dirty_non_part.py 351 375 | psql -e perftest
./dirty_part.py 2006 01 17 4 | psql -e perftest
time gpcrondump -x perftest --incremental -a

