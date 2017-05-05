#!/bin/bash
#	Filename:-		gpseginitsb.sh
#	Version:-		$Revision$
#	Updated:-		$Date$	
#	Status:-		Released	
#	Author:-		G Coombe	
#	Contact:-		gcoombe@greenplum.com
#	Release date:-		Oct 2007
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

UPDATE_PGHBA () {
	LOG_MSG "[INFO][$INST_COUNT]:-Start Function $FUNCNAME"
	STANDBY_IP_ADDRESS=(`$ECHO $IP_ADDRESS_LIST|$TR '~' ' '`)
	for STANDBY_IP in "${STANDBY_IP_ADDRESS[@]}"
	do
		# MPP-15889
        	CIDR_STANDBY_IP=$(GET_CIDRADDR $STANDBY_IP)
		CHK_COUNT=`$TRUSTED_SHELL $QE_NAME "$GREP -c \"${CIDR_STANDBY_IP}\" ${QE_BASE_DIR}/$PG_HBA"`
		if [ $CHK_COUNT -eq 0 ];then
			LOG_MSG "[INFO][$INST_COUNT]:-Adding standby IP address $QE_NAME ${QE_BASE_DIR}/$PG_HBA file" 1
			$TRUSTED_SHELL $QE_NAME "$ECHO host  all     all     ${CIDR_STANDBY_IP}       trust >> ${QE_BASE_DIR}/$PG_HBA"
			if [ $? -ne 0 ];then
				$ECHO "FAILED:${QE_LINE}:ADD_IP" >> $PARALLEL_STATUS_FILE
				ERROR_EXIT "Failed to add standby IP address $QE_NAME ${QE_BASE_DIR}/$PG_HBA file" 2
			else
				LOG_MSG "[INFO][$INST_COUNT]:-Added standby IP address $QE_NAME ${QE_BASE_DIR}/$PG_HBA file" 1
			fi
		else
			LOG_MSG "[INFO][$INST_COUNT]:-IP address $QE_NAME ${QE_BASE_DIR}/$PG_HBA already there, no update required" 1
		fi
	done
	LOG_MSG "[INFO][$INST_COUNT]:-End Function $FUNCNAME"
}
#******************************************************************************
# Main Section
#******************************************************************************
trap '$ECHO "KILLED:${QE_NAME}:${QE_BASE_DIR}" >> $PARALLEL_STATUS_FILE;ERROR_EXIT "[FATAL]:-[$INST_COUNT]-Recieved INT or TERM signal" 2' INT TERM
#Now process supplied call parameters
PARENT_PID=$1;shift		#PID of gpstate process calling this script
CHK_CALL
INST_COUNT=$1;shift		#Unique number for this parallel script, starts at 0
LOG_FILE=$1;shift		#Central utility log file
QE_NAME=$1;shift
QE_BASE_DIR=$1;shift
IP_ADDRESS_LIST=$1;shift
PARALLEL_STATUS_FILE=$1;shift
QE_LINE=${QE_NAME}:${QE_BASE_DIR}
LOG_MSG "[INFO][$INST_COUNT]:-Start Main"
UPDATE_PGHBA
$ECHO "COMPLETED:${QE_LINE}" >> $PARALLEL_STATUS_FILE
LOG_MSG "[INFO][$INST_COUNT]:-End Main"
exit $EXIT_STATUS
