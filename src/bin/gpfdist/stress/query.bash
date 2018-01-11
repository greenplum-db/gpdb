#!/bin/bash

DBNAME="stressdb"
EX_RT1="r1"
EX_RT2="r2"
EX_RT3="r3"
function run_query() {
  while :
  do
      psql -c "select * from $EX_RT1;" $DBNAME &> /dev/null
      psql -c "select * from $EX_RT2;" $DBNAME &> /dev/null
      psql -c "select * from $EX_RT3;" $DBNAME &> /dev/null
      echo "------- query once -------"
  done
}

run_query
