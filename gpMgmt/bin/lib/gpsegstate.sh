#!/bin/bash
#	Filename:-		gpsegstate.sh
#	Version:-		$Revision$
#	Updated:-		$Date$	
#	Status:-		Released	
#	Author:-		G Coombe	
#	Contact:-		gcoombe@greenplum.com
#	Release date:-		Dec 2006
#	Release stat:-		Released
#                               Copyright (c) Metapa 2005. All Rights Reserved.
#                               Copyright (c) 2007 Greenplum Inc
#******************************************************************************
# Update History
#******************************************************************************
# Ver	Date		Who		Update
#******************************************************************************
# Detailed Description
#******************************************************************************
#******************************************************************************
# Prep Code

WORKDIR=`dirname $0`

# Source required functions file, this required for script to run
# exit if cannot locate this file. Change location of FUNCTIONS variable
# as required.
FUNCTIONS=$WORKDIR/gp_bash_functions.sh
if [ -f $FUNCTIONS ]; then
		. $FUNCTIONS
else
		echo "[FATAL]:-Cannot source $FUNCTIONS file Script Exits!"
		exit 2
fi
#******************************************************************************
# Script Specific Variables
#******************************************************************************
# Log file that will record script actions
CUR_DATE=`$DATE +%Y%m%d`
TIME=`$DATE +%H%M%S`
PROG_NAME=`$BASENAME $0`
# Level of script feedback 0=small 1=verbose
unset VERBOSE
GP_USER=$USER_NAME
EXIT_STATUS=0
#******************************************************************************
# Functions
#******************************************************************************
CHK_CALL () {
	FILE_PREFIX=`$ECHO $PARALLEL_STATUS_FILE|$CUT -d"." -f1`
	if [ ! -f ${FILE_PREFIX}.$PARENT_PID ];then
		$ECHO "[FATAL]:-Not called from from correct parent program" 
		exit 2
	fi
}


GET_SEGMENT_STATUS () {
	LOG_MSG "[INFO][$INST_COUNT]:-Start Function $FUNCNAME"
	QE_PING_HOST=1
	QE_PID_FILE=1
	QE_PID_FILE_PID_ID=1
	QE_LOCK_FILE=1
	QE_ACTIVE_PID=0
	QE_MIRROR_ACTIVE=1
	QE_MIRROR_FAILED=1
	QE_PRIMARY_FAILED=1
	QE_FAILED=0
	#*************************************
	#First see if we can contact this host
	#*************************************
	PING_HOST $QE_NAME 1
	if [ $RETVAL -eq 0 ];then
		QE_PING_HOST=0
	else
		EXIT_STATUS=$RETVAL
		LOG_MSG "[WARN][$INST_COUNT]:-Failed to establish contact with $QE_NAME"
		((QE_FAILED=$QE_FAILED+1))
		QE_LINE="${QE_LINE}:${QE_FAILED}:${QE_PID_FILE}:${QE_PID_FILE_PID_ID}:${QE_LOCK_FILE}:${QE_ACTIVE_PID}:${QE_MIRROR_ACTIVE}:${QE_MIRROR_FAILED}:${QE_PRIMARY_FAILED}"
		$ECHO "PING_FAIL:$QE_LINE" >> $PARALLEL_STATUS_FILE
		LOG_MSG "[INFO][$INST_COUNT]:-End Main"
		exit $EXIT_STATUS
	fi
	#*************************************
	#Check postmaster.pid file
	#*************************************
	QE_PID_FILE=`$TRUSTED_SHELL $QE_NAME "if [ -f $QE_DIR/$PG_PID ]; then echo 0;else echo 1;fi"`
	RETVAL=$?
	if [ $RETVAL -eq 0 ] && [ $QE_PID_FILE -eq 0 ];then
		#Have located file, now process contents
		QE_PID_FILE_PID_ID=`$TRUSTED_SHELL $QE_NAME "$HEAD -1 $QE_DIR/$PG_PID"` 2>> /dev/null
		if [ x"" == x"$QE_PID_FILE_PID_ID" ];then
			QE_PID_FILE_PID_ID=1
			((QE_FAILED=$QE_FAILED+1))
		fi
	else
		QE_PID_FILE_PID_ID=1
	fi
	#*************************************
	#Now check to see if we have lock file
	#*************************************
	QE_LOCK_FILE=`$TRUSTED_SHELL $QE_NAME "if [ -f /tmp/.s.PGSQL.${QE_PORT}.lock ];then echo 0;else echo 1;fi"`
	RETVAL=$?
	if [ $RETVAL -ne 0 ] || [ $QE_LOCK_FILE -eq 1 ];then
		QE_LOCK_FILE=1
		LOG_MSG "[WARN][$INST_COUNT]:-No /tmp/.s.PGSQL.${QE_PORT}.lock file located on $QE_NAME"
		((QE_FAILED=$QE_FAILED+1))
	fi
	#*************************************
	#See if we have an active pid
	#*************************************
	GET_PG_PID_ACTIVE $QE_PORT $QE_NAME
	if [ x"" == x"$PG_LOCK_NETSTAT" ];then
		LOG_MSG "[WARN][$INST_COUNT]:-Have no running postmaster process on $QE_NAME $QE_PORT" 1
		QE_ACTIVE_PID=1
		((QE_FAILED=$QE_FAILED+1))
	else
		QE_ACTIVE_PID=$PID
	fi
	#*************************************
	#Process gp_pgdatabase info
	#*************************************
    QE_CURRENTLY_PRIMARY=`env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -d $DEFAULTDB  -A -t -c"select isprimary from $GP_PG_VIEW as _gp_pg_view where dbid in (select dbid from $CONFIG_TABLE a where port=$QE_PORT and datadir='${QE_DIR}');"`

	if [ "$QE_PRIMARY" == 'f' ];then
		#*************************************
		#Process status mirror segment
		#*************************************
		if [ "$QE_CURRENTLY_PRIMARY" == "t" ]; then
			LOG_MSG "[WARN][$INST_COUNT]:-Mirror $QE_DIR $QE_PORT on $QE_NAME is active"
			((QE_FAILED=$QE_FAILED+1))
		fi
	else
        if [ "$QE_CURRENTLY_PRIMARY" == "f" ]; then
            LOG_MSG "[WARN][$INST_COUNT]:-Primary $QE_DIR $QE_PORT on $QE_NAME is not active"
            ((QE_FAILED=$QE_FAILED+1))
        fi
	fi
	QE_LINE="${QE_LINE}:${QE_FAILED}:${QE_PID_FILE}:${QE_PID_FILE_PID_ID}:${QE_LOCK_FILE}:${QE_ACTIVE_PID}:${QE_CURRENTLY_PRIMARY}"
	LOG_MSG "[INFO][$INST_COUNT]:-End Function $FUNCNAME"
}

#******************************************************************************
# Main Section
#******************************************************************************
trap '$ECHO "KILLED:${QE_LINE}" >> $PARALLEL_STATUS_FILE;ERROR_EXIT "[FATAL]:-[$INST_COUNT]-Recieved INT or TERM signal" 2' INT TERM
#Now process supplied call parameters
PARENT_PID=$1;shift		#PID of gpstate process calling this script
CHK_CALL
QE_NAME=$1;shift		#Hostname holding database segment
QE_PORT=$1;shift		#Segment port
QE_DIR=$1;shift			#Segment directory
QE_VALID=$1;shift		#Status of segment either t or f
QE_PRIMARY=$1;shift		#t primary f mirror
INST_COUNT=$1;shift		#Unique number for this parallel script, starts at 0
LOG_FILE=$1;shift
MASTER_PORT=$1;shift
TYPE=$1;shift
LOG_MSG "[INFO][$INST_COUNT]:-Start Main"
QE_LINE="${QE_NAME}:${QE_PORT}:${QE_DIR}:${QE_PRIMARY}:${QE_VALID}"
case $TYPE in
	1 ) GET_SEGMENT_STATUS ;;
esac
$ECHO "COMPLETED:$QE_LINE" >> $PARALLEL_STATUS_FILE
LOG_MSG "[INFO][$INST_COUNT]:-End Main"
exit $EXIT_STATUS
