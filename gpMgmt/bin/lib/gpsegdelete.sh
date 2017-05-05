#!/bin/bash
#	Filename:-		gpsegdelete.sh
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

CHK_DUMP () {
	LOG_MSG "[INFO][$INST_COUNT]:-Start Function $FUNCNAME"
	LOG_MSG "[INFO][$INST_COUNT]:-Checking host=$DELETE_HOSTNAME Data directory=$DELETE_DATADIR"
	DUMP_COUNT=`$TRUSTED_SHELL $DELETE_HOSTNAME "$FIND $DELETE_DATADIR -name 'gp_dump*' -print|$WC -l"`
	if [ x"" == x"$DUMP_COUNT" ];then
		LOG_MSG "[WARN]:-Issue with count of dump directories on $DELETE_HOSTNAME"
		DUMP_COUNT=0
	fi
	if [ $DUMP_COUNT -ne 0 ];then 
		QE_LINE="${QE_LINE}:DUMP_LOCATED"
	else
		QE_LINE="${QE_LINE}:NO_DUMP"
	fi
	LOG_MSG "[INFO][$INST_COUNT]:-End Function $FUNCNAME"
}

DEL_SEGMENT () {
	LOG_MSG "[INFO][$INST_COUNT]:-Start Function $FUNCNAME"
	WARN=0
	LOG_MSG "[INFO][$INST_COUNT]:-Deleting $DELETE_HOSTNAME $DELETE_DATADIR"
	$TRUSTED_SHELL $DELETE_HOSTNAME "$RM -Rf $DELETE_DATADIR"
	RETVAL=$?
	if [ $RETVAL -ne 0 ];then
		LOG_MSG "[WARN][$INST_COUNT]:-Issue with $RM -Rf $DELETE_DATADIR on $DELETE_HOSTNAME" 1
		WARN=1
	else
		LOG_MSG "[INFO][$INST_COUNT]:-Completed $RM -Rf $DELETE_DATADIR on $DELETE_HOSTNAME"
	fi
	$TRUSTED_SHELL $DELETE_HOSTNAME "$RM -f ${DELETE_DATADIR}.log*"	
	RETVAL=$?
	if [ $RETVAL -ne 0 ];then
		LOG_MSG "[WARN][$INST_COUNT]:-Issue with $RM -Rf ${DELETE_DATADIR}.log n $DELETE_HOSTNAME" 1
		WARN=1
	else
		LOG_MSG "[INFO][$INST_COUNT]:-Completed $RM -Rf ${DELETE_DATADIR}.log n $DELETE_HOSTNAME"
	fi
	if [ $WARN -eq 1 ];then
		QE_LINE="${QE_LINE}:DEL_WARN"
	else
		QE_LINE="${QE_LINE}:DEL_OK:"
	fi
	LOG_MSG "[INFO][$INST_COUNT]:-End Function $FUNCNAME"
}

#******************************************************************************
# Main Section
#******************************************************************************
trap '$ECHO "KILLED|${QE_LINE}" >> $PARALLEL_STATUS_FILE;ERROR_EXIT "[FATAL]:-[$INST_COUNT]-Recieved INT or TERM signal" 2' INT TERM
#Now process supplied call parameters
PARENT_PID=$1;shift		#PID of gpstate process calling this script
CHK_CALL
TYPE=$1;shift
DELETE_HOSTNAME=$1;shift
DELETE_DATADIR=$1;shift
INST_COUNT=$1;shift
LOG_FILE=$1;shift
QE_LINE="${DELETE_HOSTNAME}:${DELETE_DATADIR}"
case $TYPE in
	1 ) CHK_DUMP ;;
	2 ) DEL_SEGMENT ;;
esac
$ECHO "COMPLETED:${QE_LINE}" >> $PARALLEL_STATUS_FILE
LOG_MSG "[INFO][$INST_COUNT]:-End Main"
exit $EXIT_STATUS
