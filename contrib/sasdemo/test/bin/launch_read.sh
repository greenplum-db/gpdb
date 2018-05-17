#!/usr/bin/env bash

/bin/rm -rf ./read_*.log

SEGMENTS=`psql -XtA -c "select content+1 from gp_segment_configuration where content >= 0"`
BASEPORT=1999

echo "Launching SAS Reader Processes"
for i in ${SEGMENTS}
do
  let PORT=$BASEPORT+$i
  echo "  $i: Launching SAS Reader backend on port $PORT, logging to results/read_$i.log"
  $GPHOME/bin/sas_read $PORT 1> results/data_$i.dat 2> results/read_$i.log &
done

