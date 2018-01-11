#!/bin/bash
DBNAME="stressdb"
MOCKD_PATH="/mount/mockd-linux"

TABLE_1="l1"
EX_RT1="r1"
EX_WT1="w1"
STRUCTURE_1="(ID_INT int,NAME varchar(255),City varchar(255))"
PRI_KEY1="DISTRIBUTED BY (ID_INT)"
FILE_1="1.txt"
ROW1=100

TABLE_2="l2"
EX_RT2="r2"
EX_WT2="w2"
STRUCTURE_2="(ID_INT int,NAME varchar(255),City varchar(255))"
PRI_KEY2="DISTRIBUTED BY (ID_INT)"
FILE_2="2.txt"
ROW2=100000

TABLE_3="l3"
EX_RT3="r3"
EX_WT3="w3"
STRUCTURE_3="(ID_INT int,NAME varchar(255),City varchar(255))"
PRI_KEY3="DISTRIBUTED BY (ID_INT)"
FILE_3="3.txt"
ROW3=1000000

function create_database() {
    if psql -lqt | cut -d\| -f 1 | grep -qw $DBNAME; then
        echo "Has database named as stressdb, drop db first"
        dropdb $DBNAME
    fi

    createdb $DBNAME

    psql -c "create table $TABLE_1 $STRUCTURE_1 $PRI_KEY1;" $DBNAME
    psql -c "create table $TABLE_2 $STRUCTURE_2 $PRI_KEY2;" $DBNAME
    psql -c "create table $TABLE_3 $STRUCTURE_3 $PRI_KEY3;" $DBNAME

    $MOCKD_PATH greenplum -u gpadmin -d $DBNAME -n $ROW1 -t $TABLE_1 -p $PGPORT
    $MOCKD_PATH greenplum -u gpadmin -d $DBNAME -n $ROW2 -t $TABLE_2 -p $PGPORT
    $MOCKD_PATH greenplum -u gpadmin -d $DBNAME -n $ROW3 -t $TABLE_3 -p $PGPORT

    touch ./$FILE_1
    psql -c "create external table $EX_RT1 $STRUCTURE_1 location ('gpfdist://127.0.0.1:8080/$FILE_1') format 'text';" $DBNAME
    psql -c "create writable external table $EX_WT1 $STRUCTURE_1 location ('gpfdist://127.0.0.1:8080/$FILE_1') format 'text';" $DBNAME

    touch ./$FILE_2
    psql -c "create external table $EX_RT2 $STRUCTURE_2 location ('gpfdist://127.0.0.1:8080/$FILE_2') format 'text';" $DBNAME
    psql -c "create writable external table $EX_WT2 $STRUCTURE_2 location ('gpfdist://127.0.0.1:8080/$FILE_2') format 'text';" $DBNAME

    touch ./$FILE_3
    psql -c "create external table $EX_RT3 $STRUCTURE_3 location ('gpfdist://127.0.0.1:8080/$FILE_3') format 'text';" $DBNAME
    psql -c "create writable external table $EX_WT3 $STRUCTURE_3 location ('gpfdist://127.0.0.1:8080/$FILE_3') format 'text';" $DBNAME
}

fdist_Pid=
function data_translation() {
  run_gpfdist

  psql -c "insert into $EX_WT1 select * from $TABLE_1;" $DBNAME
  psql -c "insert into $EX_WT2 select * from $TABLE_2;" $DBNAME
  psql -c "insert into $EX_WT3 select * from $TABLE_3;" $DBNAME

  kill -15 $fdist_Pid
}

ret_value=0
function kill_and_check_gpfdist() {
  local sleep_sec=$(((RANDOM % 6) + 2))
  echo "$fdist_Pid will be killed after $sleep_sec sec"
  sleep $sleep_sec
  kill -15 $fdist_Pid
  sleep 1
  wait > /dev/null

  local result=`ps -a | sed -n /${fdist_Pid}/p`

  if [ "${result:-null}" == null ]; then
      ret_value=0
  else
      echo "ERROR: gpfdist seem to be hang"
      ret_value=1
  fi

}

function run_gpfdist() {
  gpfdist >> /dev/null &
  fdist_Pid=$!
}


function _main() {
  pkill gpfdist
  create_database
  data_translation

  while [ $ret_value -ne 1 ]
  do
      run_gpfdist
      kill_and_check_gpfdist
  done

  echo "gpfdist does not exit after kill -15 send"
  dropdb $DBNAME
}

_main
