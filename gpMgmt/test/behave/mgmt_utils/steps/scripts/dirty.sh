#!/bin/bash

./dirty_non_part.py 1 25 | psql -e perftest
./dirty_part.py 2005 01 01 4 | psql -e perftest
