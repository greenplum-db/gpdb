#!/usr/bin/env bash

NUMHOSTS=`psql -XtA -c "select count(distinct hostname) from gp_segment_configuration"`
SEGMENTS=`psql -XtA -c "select count(distinct content) from gp_segment_configuration where content >= 0"`

echo "Detected configuration with $NUMHOSTS hosts and $SEGMENTS segments"
if [ ${NUMHOSTS} -gt 1 ]; then
	echo "Test requires single host configuration, detected $NUMHOSTS hosts"
	exit 1
fi

