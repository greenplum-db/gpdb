#!/usr/bin/env bash

SEGMENTS=`psql -XtA -c "select content+1 from gp_segment_configuration where content >= 0"`
BASEPORT=1999

echo "Launching SAS Writer Processes"
for i in ${SEGMENTS}
do
  let PORT=$BASEPORT+$i
  echo "  $i: Launching SAS Writer backend on port $PORT, logging to results/write_$i.log"
  $GPHOME/bin/sas_write $PORT < results/data_$i.dat 2> results/write_$i.log &
done

