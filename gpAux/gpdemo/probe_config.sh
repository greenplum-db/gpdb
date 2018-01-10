#!/bin/bash

#***********************************************************
# Look for Greenplum executables to find path
#***********************************************************
if [ x"$GPHOME" = x ]; then
  if [ x"$BIZHOME" = x ]; then
    echo "Neither GPHOME or BIZHOME are set.  Set one of these variables to point to the location"
	echo "of the Greenplum installation directory."
    echo ""
	exit 1
  else
    GPSEARCH=$BIZHOME
  fi
else
  GPSEARCH=$GPHOME
fi

GPPATH=`find $GPSEARCH -name gpstart | tail -1`
RETVAL=$?

if [ "$RETVAL" -ne 0 ]; then
  echo "Error attempting to find Greenplum executables in $GPSEARCH"
  exit 1
fi

if [ ! -x "$GPPATH" ]; then
  echo "No executables found for Greenplum installation in $GPSEARCH"
  exit 1
fi
GPPATH=`dirname $GPPATH`
#***********************************************************

#***********************************************************
# Create a list of catalog tables to printout
#***********************************************************
declare -a TABLES=(version_at_initdb)
declare -a TABLESQD=(segment_configuration pgdatabase version_at_initdb)

#***********************************************************
# Declare the max and min port numbers to probe
#***********************************************************
declare -a PORTS=(5432 10001 10002 10003)
((PORT_MIN=0))
((PORT_MAX=$NUM_PRIMARY_MIRROR_PAIRS))

if [ -z $MASTER_DEMO_PORT ] ; then
    echo "set MASTER_DEMO_PORT"
    exit 1
fi

if [ -z $DEMO_PORT_BASE ] ; then
    echo "set DEMO_PORT_BASE"
    exit 1
fi

declare -a PORTS=($MASTER_DEMO_PORT `expr $DEMO_PORT_BASE` \
   `expr $DEMO_PORT_BASE + 1` `expr $DEMO_PORT_BASE + 2 `)

#
# Check tables on Master
#

#***********************************************************
# Loop over all ports and all tables, printing out their
# contents
#***********************************************************

for ((i=PORT_MIN; i<PORT_MAX+1; i++)); do
    echo "======================================================================"
    echo "Probing segment instance at port number ${PORTS[$i]}"
    echo "======================================================================"
    if [ ${i} -eq 0 ]; then 
        for table in ${TABLESQD[@]}; do
            echo ""
            echo "----------------------------------------"
            echo "Table: gp_$table"
            echo "----------------------------------------"
            PGOPTIONS="-c gp_session_role=utility" $GPPATH/psql --pset pager=off -p ${PORTS[$i]} -d template1 -c  "select * from gp_"$table
            RETVAL=$?
            if [ $RETVAL -ne 0 ]; then
                echo "$0 failed."
                exit 1
            fi
        done
    else
        for table in ${TABLES[@]}; do
            echo ""
            echo "----------------------------------------"
            echo "Table: gp_$table"
            echo "----------------------------------------"
            PGOPTIONS="-c gp_session_role=utility" $GPPATH/psql --pset pager=off -p ${PORTS[$i]} -d template1 -c \
                "select * from gp_"$table
            RETVAL=$?
            if [ $RETVAL -ne 0 ]; then
                echo "$0 failed."
                exit 1
            fi
        done
    fi
done

echo "**********************************************************************"

