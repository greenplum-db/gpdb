#!/bin/bash

# ======================================================================
# Configuration Variables
# ======================================================================

# Set to zero to force cluster to be created without data checksums
DATACHECKSUMS=1

# ======================================================================
# Data Directories
# ======================================================================

if [ -z "${MASTER_DATADIR}" ]; then
  DATADIRS=${DATADIRS:-`pwd`/datadirs}
else
  DATADIRS="${MASTER_DATADIR}/datadirs"
fi

QDDIR=$DATADIRS/qddir
SEG_PREFIX=demoDataDir

STANDBYDIR=$DATADIRS/standby

# ======================================================================
# Database Ports
# ======================================================================

for (( i=0; i<`expr 2 \* $NUM_PRIMARY_MIRROR_PAIRS`; i++ )); do
  PORT_NUM=`expr $DEMO_PORT_BASE + $i`
  DEMO_SEG_PORTS_LIST="$DEMO_SEG_PORTS_LIST $PORT_NUM"
done
DEMO_SEG_PORTS_LIST=${DEMO_SEG_PORTS_LIST#* }

# ======================================================================
# Functions
# ======================================================================

checkDemoConfig(){
    echo "----------------------------------------------------------------------"
    echo "                   Checking for port availability"
    echo "----------------------------------------------------------------------"
    echo ""
    # Check if Master_DEMO_Port is free
    echo "  Master port check ... : ${MASTER_DEMO_PORT}"
    PORT_FILE="/tmp/.s.PGSQL.${MASTER_DEMO_PORT}"
    if [ -f ${PORT_FILE} -o  -S ${PORT_FILE} ] ; then 
        echo ""
        echo -n " Port ${MASTER_DEMO_PORT} appears to be in use. " 
        echo " This port is needed by the Master Database instance. "
        echo ">>> Edit Makefile to correct the port number (MASTER_PORT). <<<" 
        echo -n " Check to see if the port is free by using : "
        echo " 'netstat -an | grep ${MASTER_DEMO_PORT}"
        echo ""
        return 1
    fi

    # Check if Standby_DEMO_Port is free
    echo "  Standby port check ... : ${STANDBY_DEMO_PORT}"
    PORT_FILE="/tmp/.s.PGSQL.${STANDBY_DEMO_PORT}"
    if [ -f ${PORT_FILE} -o  -S ${PORT_FILE} ] ; then
        echo ""
        echo -n " Port ${STANDBY_DEMO_PORT} appears to be in use. "
        echo " This port is needed by the Standby Database instance. "
        echo ">>> Edit Makefile to correct the port number (STANDBY_PORT). <<<"
        echo -n " Check to see if the port is free by using : "
        echo " 'netstat -an | grep ${STANDBY_DEMO_PORT}"
        echo ""
        return 1
    fi

    for (( i=0; i<`expr 4 \* $NUM_PRIMARY_MIRROR_PAIRS`; i++ )); do
	PORT_NUM=`expr $DEMO_PORT_BASE + $i`

        echo "  Segment port check .. : ${PORT_NUM}"
        PORT_FILE="/tmp/.s.PGSQL.${PORT_NUM}"
        if [ -f ${PORT_FILE} -o -S ${PORT_FILE} ] ; then 
            echo ""
            echo -n "Port ${PORT_NUM} appears to be in use."
            echo " This port is needed for segment database instance."
            echo ">>> Edit Makefile to correct the base ports (PORT_BASE). <<<"
            echo -n " Check to see if the port is free by using : "
            echo " 'netstat -an | grep ${PORT_NUM}"
            echo ""
            return 1
        fi
    done
    return 0
}

USAGE(){
    echo ""
    echo " `basename $0` {-c | -d | -u} <-K>"
    echo " -c : Check if demo is possible."
    echo " -d : Delete the demo."
    echo " -K : Create cluster without data checksums."
    echo " -u : Usage, prints this message."
    echo ""
}

#
# Clean up the demo
#

cleanDemo(){

    ##
    ## Attempt to bring down using GPDB cluster instance using gpstop
    ##

    (export MASTER_DATA_DIRECTORY=$QDDIR/${SEG_PREFIX}-1;
     source ${GPHOME}/greenplum_path.sh;
     gpstop -a)

    ##
    ## Remove the files and directories created; allow test harnesses
    ## to disable this
    ##

    if [ "${GPDEMO_DESTRUCTIVE_CLEAN}" != "false" ]; then
        if [ -f hostfile ];  then
            echo "Deleting hostfile"
            rm -f hostfile
        fi
        if [ -f clusterConfigFile ];  then
            echo "Deleting clusterConfigFile"
            rm -f clusterConfigFile
        fi
        if [ -d ${DATADIRS} ];  then
            echo "Deleting ${DATADIRS}"
            rm -rf ${DATADIRS}
        fi
        if [ -d logs ];  then
            rm -rf logs
        fi
        rm -f optimizer-state.log gpdemo-env.sh
    fi
}

#*****************************************************************************
# Main Section
#*****************************************************************************

while getopts ":cdK'?'" opt
do
	case $opt in 
		'?' ) USAGE ;;
        c) checkDemoConfig
           RETVAL=$?
           if [ $RETVAL -ne 0 ]; then
               echo "Checking failed "
               exit 1
           fi
           exit 0
           ;;
        d) cleanDemo
           exit 0
           ;;
        K) DATACHECKSUMS=0
           shift
           ;;
        *) USAGE
           exit 0
           ;;
	esac
done

if [ -z "${GPHOME}" ]; then
    echo "FATAL: The GPHOME enviroment variable is not set."
    echo ""
    echo "  You can set it by sourcing the greenplum_path.sh"
    echo "  file in your Greenplum installation directory."
    echo ""
    exit 1
else
    GPSEARCH=$GPHOME
fi

cat <<-EOF
	======================================================================
	            ______  _____  ______  _______ _______  _____
	           |  ____ |_____] |     \ |______ |  |  | |     |
	           |_____| |       |_____/ |______ |  |  | |_____|

	----------------------------------------------------------------------

	  This is a demo of the Greenplum Database system.  We will create
	  a cluster installation with master and `expr 2 \* ${NUM_PRIMARY_MIRROR_PAIRS}` segment instances
	  (${NUM_PRIMARY_MIRROR_PAIRS} primary & ${NUM_PRIMARY_MIRROR_PAIRS} mirror).

	    GPHOME ................. : ${GPHOME}
	    MASTER_DATA_DIRECTORY .. : ${QDDIR}/${SEG_PREFIX}-1

	    MASTER PORT (PGPORT) ... : ${MASTER_DEMO_PORT}
	    STANDBY PORT ........... : ${STANDBY_DEMO_PORT}
	    SEGMENT PORTS .......... : ${DEMO_SEG_PORTS_LIST}

	  NOTE(s):

	    * The DB ports identified above must be available for use.
	    * An environment file gpdemo-env.sh has been created for your use.

	======================================================================

EOF

GPPATH=`find $GPSEARCH -name gpstart| tail -1`
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
if [ ! -x $GPPATH/gpinitsystem ]; then
    echo "No mgmt executables found for Greenplum installation in $GPPATH"
    exit 1
fi

if [ -d $DATADIRS ]; then
  rm -rf $DATADIRS
fi
mkdir $DATADIRS
mkdir $QDDIR
mkdir $DATADIRS/gpAdminLogs

for (( i=1; i<=$NUM_PRIMARY_MIRROR_PAIRS; i++ ))
do
  PRIMARY_DIR=$DATADIRS/dbfast$i
  mkdir -p $PRIMARY_DIR
  PRIMARY_DIRS_LIST="$PRIMARY_DIRS_LIST $PRIMARY_DIR"

  MIRROR_DIR=$DATADIRS/dbfast_mirror$i
  mkdir -p $MIRROR_DIR
  MIRROR_DIRS_LIST="$MIRROR_DIRS_LIST $MIRROR_DIR"
done
PRIMARY_DIRS_LIST=${PRIMARY_DIRS_LIST#* }
MIRROR_DIRS_LIST=${MIRROR_DIRS_LIST#* }

#*****************************************************************************************
# Host configuration
#*****************************************************************************************

LOCALHOST=`hostname`
echo $LOCALHOST > hostfile

#*****************************************************************************************
# Name of the system configuration file.
#*****************************************************************************************

CLUSTER_CONFIG=clusterConfigFile
CLUSTER_CONFIG_POSTGRES_ADDONS=clusterConfigPostgresAddonsFile

rm -f ${CLUSTER_CONFIG}
rm -f ${CLUSTER_CONFIG_POSTGRES_ADDONS}

#*****************************************************************************************
# Create the system configuration file
#*****************************************************************************************

cat >> $CLUSTER_CONFIG <<-EOF
	# Set this to anything you like
	ARRAY_NAME="Demo $HOSTNAME Cluster"
	CLUSTER_NAME="Demo $HOSTNAME Cluster"
	
	# This file must exist in the same directory that you execute gpinitsystem in
	MACHINE_LIST_FILE=`pwd`/hostfile
	
	# This names the data directories for the Segment Instances and the Entry Postmaster
	SEG_PREFIX=$SEG_PREFIX
	
	# This is the port at which to contact the resulting Greenplum database, e.g.
	#   psql -p \$PORT_BASE -d template1
	PORT_BASE=${DEMO_PORT_BASE}
	
	# Prefix for script created database
	DATABASE_PREFIX=demoDatabase
	
	# Array of data locations for each hosts Segment Instances, the number of directories in this array will
	# set the number of segment instances per host
	declare -a DATA_DIRECTORY=(${PRIMARY_DIRS_LIST})
	
	# Name of host on which to setup the QD
	MASTER_HOSTNAME=$LOCALHOST
	
	# Name of directory on that host in which to setup the QD
	MASTER_DIRECTORY=$QDDIR
	
	MASTER_PORT=${MASTER_DEMO_PORT}
	
	# Hosts to allow to connect to the QD (and Segment Instances)
	# By default, allow everyone to connect (0.0.0.0/0)
	IP_ALLOW=0.0.0.0/0
	
	# Shell to use to execute commands on all hosts
	TRUSTED_SHELL="`pwd`/lalshell"
	
	CHECK_POINT_SEGMENTS=8
	
	ENCODING=UNICODE
EOF

if [ "${DATACHECKSUMS}" == "0" ]; then
    cat >> $CLUSTER_CONFIG <<-EOF
	# Turn off data checksums
	HEAP_CHECKSUM=off
EOF
fi

if [ "${WITH_MIRRORS}" == "true" ]; then
    cat >> $CLUSTER_CONFIG <<-EOF

		# Array of mirror data locations for each hosts Segment Instances, the number of directories in this array will
		# set the number of segment instances per host
		declare -a MIRROR_DATA_DIRECTORY=(${MIRROR_DIRS_LIST})

		MIRROR_PORT_BASE=`expr $DEMO_PORT_BASE + $NUM_PRIMARY_MIRROR_PAIRS`

		REPLICATION_PORT_BASE=`expr $DEMO_PORT_BASE + 2 \* $NUM_PRIMARY_MIRROR_PAIRS`
		MIRROR_REPLICATION_PORT_BASE=`expr $DEMO_PORT_BASE + 3 \* $NUM_PRIMARY_MIRROR_PAIRS`
	EOF
fi


STANDBY_INIT_OPTS=""
if [ "${WITH_STANDBY}" == "true" ]; then
	STANDBY_INIT_OPTS="-s ${LOCALHOST} -P ${STANDBY_DEMO_PORT} -F ${STANDBYDIR}"
fi

if [ ! -z "${EXTRA_CONFIG}" ]; then
  echo ${EXTRA_CONFIG} >> $CLUSTER_CONFIG
fi

cat >> $CLUSTER_CONFIG <<-EOF

	# Path for Greenplum mgmt utils and Greenplum binaries
	PATH=$GPHOME/bin:$PATH
	LD_LIBRARY_PATH=$GPHOME/lib:$LD_LIBRARY_PATH
	export PATH
	export LD_LIBRARY_PATH
	export MASTER_DATA_DIRECTORY
	export TRUSTED_SHELL
EOF

if [ -z "${DEFAULT_QD_MAX_CONNECT}" ]; then
   DEFAULT_QD_MAX_CONNECT=25
fi

cat >> $CLUSTER_CONFIG <<-EOF

	# Keep max_connection settings to reasonable values for
	# installcheck good execution.

	DEFAULT_QD_MAX_CONNECT=$DEFAULT_QD_MAX_CONNECT
	QE_CONNECT_FACTOR=5

EOF

if [ -n "${STATEMENT_MEM}" ]; then
	cat >> $CLUSTER_CONFIG_POSTGRES_ADDONS<<-EOF
		statement_mem = ${STATEMENT_MEM}
	EOF
fi

if [ "${ONLY_PREPARE_CLUSTER_ENV}" == "true" ]; then
    echo "ONLY_PREPARE_CLUSTER_ENV set, generated clusterConf file: $CLUSTER_CONFIG, exiting"
    exit 0
fi

## ======================================================================
## Create cluster
## ======================================================================

##
## Provide support to pass dynamic values to ${CLUSTER_CONFIG_POSTGRES_ADDONS}
## which is used during gpinitsystems.
##

if [ "${BLDWRAP_POSTGRES_CONF_ADDONS}" != "__none__" ]  && \
   [ "${BLDWRAP_POSTGRES_CONF_ADDONS}" != "__unset__" ] && \
   [ -n "${BLDWRAP_POSTGRES_CONF_ADDONS}" ]; then

    [ -f ${CLUSTER_CONFIG_POSTGRES_ADDONS} ] && chmod a+w ${CLUSTER_CONFIG_POSTGRES_ADDONS}

    for addon in $( echo ${BLDWRAP_POSTGRES_CONF_ADDONS} | sed -e "s/|/ /g" ); do
        echo "" >> ${CLUSTER_CONFIG_POSTGRES_ADDONS}
        echo $addon >> ${CLUSTER_CONFIG_POSTGRES_ADDONS}
	if [ "$addon" == "fsync=off" ]; then
		echo "WARNING: fsync is off, database consistency is not guaranteed."
	fi
        echo "" >> ${CLUSTER_CONFIG_POSTGRES_ADDONS}
    done

    echo ""
    echo "======================================================================"
    echo "CLUSTER_CONFIG_POSTGRES_ADDONS: ${CLUSTER_CONFIG_POSTGRES_ADDONS}"
    echo "----------------------------------------------------------------------"
    cat ${CLUSTER_CONFIG_POSTGRES_ADDONS}
    echo "======================================================================"
    echo ""
fi

if [ -f "${CLUSTER_CONFIG_POSTGRES_ADDONS}" ]; then
    echo "=========================================================================================="
    echo "executing:"
    echo "  $GPPATH/gpinitsystem -a -c $CLUSTER_CONFIG -l $DATADIRS/gpAdminLogs -p ${CLUSTER_CONFIG_POSTGRES_ADDONS} ${STANDBY_INIT_OPTS} \"$@\""
    echo "=========================================================================================="
    echo ""
    $GPPATH/gpinitsystem -a -c $CLUSTER_CONFIG -l $DATADIRS/gpAdminLogs -p ${CLUSTER_CONFIG_POSTGRES_ADDONS} ${STANDBY_INIT_OPTS} "$@"
else
    echo "=========================================================================================="
    echo "executing:"
    echo "  $GPPATH/gpinitsystem -a -c $CLUSTER_CONFIG -l $DATADIRS/gpAdminLogs ${STANDBY_INIT_OPTS} \"$@\""
    echo "=========================================================================================="
    echo ""
    $GPPATH/gpinitsystem -a -c $CLUSTER_CONFIG -l $DATADIRS/gpAdminLogs ${STANDBY_INIT_OPTS} "$@"
fi
RETURN=$?

echo "========================================"
echo "gpinitsystem returned: ${RETURN}"
echo "========================================"
echo ""

if [ "$enable_gpfdist" = "yes" ] && [ "$with_openssl" = "yes" ]; then
	echo "======================================================================"
	echo "Generating SSL certificates for gpfdists:"
	echo "======================================================================"
	echo ""

	./generate_certs.sh >> generate_certs.log

	cp -r certificate/gpfdists $QDDIR/$SEG_PREFIX-1/

	for (( i=1; i<=$NUM_PRIMARY_MIRROR_PAIRS; i++ ))
	do
		cp -r certificate/gpfdists $DATADIRS/dbfast$i/${SEG_PREFIX}$((i-1))/
		cp -r certificate/gpfdists $DATADIRS/dbfast_mirror$i/${SEG_PREFIX}$((i-1))/
	done
	echo ""
fi

OPTIMIZER=$(psql -t -p ${MASTER_DEMO_PORT} -d template1 -c "show optimizer"   2>&1)

echo "======================================================================" 2>&1 | tee -a optimizer-state.log
echo "                           OPTIMIZER STATE"                             2>&1 | tee -a optimizer-state.log
echo "----------------------------------------------------------------------" 2>&1 | tee -a optimizer-state.log
echo "  Optimizer state .. : ${OPTIMIZER}"                                    2>&1 | tee -a optimizer-state.log
echo "======================================================================" 2>&1 | tee -a optimizer-state.log
echo ""                                                                       2>&1 | tee -a optimizer-state.log

psql -p ${MASTER_DEMO_PORT} -d template1 -c "select version();"               2>&1 | tee -a optimizer-state.log

psql -p ${MASTER_DEMO_PORT} -d template1 -c "show optimizer;" > /dev/null     2>&1
if [ $? = 0 ]; then
    psql -p ${MASTER_DEMO_PORT} -d template1 -c "show optimizer;"             2>&1 | tee -a optimizer-state.log
fi

psql -p ${MASTER_DEMO_PORT} -d template1 -c "select gp_opt_version();" > /dev/null 2>&1
if [ $? = 0 ]; then
    psql -p ${MASTER_DEMO_PORT} -d template1 -c "select gp_opt_version();"    2>&1 | tee -a optimizer-state.log
fi

echo "======================================================================" 2>&1 | tee -a optimizer-state.log
echo ""                                                                       2>&1 | tee -a optimizer-state.log

cat > gpdemo-env.sh <<-EOF
	## ======================================================================
	##                                gpdemo
	## ----------------------------------------------------------------------
	## timestamp: $( date )
	## ======================================================================

	export PGPORT=${MASTER_DEMO_PORT}
	export MASTER_DATA_DIRECTORY=$QDDIR/${SEG_PREFIX}-1
EOF

if [ "${RETURN}" -gt 1 ];
then
    # gpinitsystem will return warnings as exit code 1
    exit ${RETURN}
else
    exit 0
fi
