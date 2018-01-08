#!/bin/bash
#	Filename:-		gpcreateseg.sh
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
unset PG_CONF_ADD_FILE
# MPP database specific parameters
GP_USER=$USER_NAME
GP_TBL=gp_id
# System table names
GP_CONFIGURATION_TBL=gp_segment_configuration
EXIT_STATUS=0
# SED_PG_CONF search text values
PORT_TXT="#port"
LOG_STATEMENT_TXT="#log_statement ="
LISTEN_ADR_TXT="listen_addresses"
CHKPOINT_SEG_TXT="checkpoint_segments"
TMP_PG_HBA=/tmp/pg_hba_conf_master.$$

#******************************************************************************
# Functions
#******************************************************************************

CHK_CALL () {
	FILE_PREFIX=`$ECHO $PARALLEL_STATUS_FILE|$CUT -d"." -f1`
	if [ ! -f ${FILE_PREFIX}.$PARENT_PID ];then
		$ECHO "[FATAL]:-Not called from correct parent program" 
		exit 2
	fi
}

SET_VAR () {
    # 
    # MPP-13617: If segment contains a ~, we assume ~ is the field delimiter.
    # Otherwise we assume : is the delimiter.  This allows us to easily 
    # handle IPv6 addresses which may contain a : by using a ~ as a delimiter. 
    # 
    I=$1
    case $I in
        *~*)
	    S="~"
            ;;
        *)
	    S=":"
            ;;
    esac
    GP_HOSTADDRESS=`$ECHO $I|$CUT -d$S -f1`
    GP_PORT=`$ECHO $I|$CUT -d$S -f2`
    GP_DIR=`$ECHO $I|$CUT -d$S -f3`
    GP_DBID=`$ECHO $I|$CUT -d$S -f4`
    GP_CONTENT=`$ECHO $I|$CUT -d$S -f5`
}

PARA_EXIT () {
	if [ $1 -ne 0 ];then
		$ECHO "FAILED:$SEGMENT_LINE" >> $PARALLEL_STATUS_FILE
		LOG_MSG "[FATAL][$INST_COUNT]:-Failed $2"
		exit 2
	else
		LOG_MSG "[INFO][$INST_COUNT]:-Completed $2"
	fi
}

PROCESS_QE () {

    LOG_MSG "[INFO][$INST_COUNT]:-Start Function $FUNCNAME"
    LOG_MSG "[INFO][$INST_COUNT]:-Processing segment $GP_HOSTADDRESS"

    MIRROR_ONLY_INITDB_OPTION=
    if [ x"" != x"$COPY_FROM_PRIMARY_HOSTADDRESS" ]; then
	MIRROR_ONLY_INITDB_OPTION=-m
    fi

    # build initdb command, capturing output in ${GP_DIR}.initdb
    cmd="$EXPORT_LIB_PATH;$INITDB"
    cmd="$cmd $MIRROR_ONLY_INITDB_OPTION"
    cmd="$cmd -E $ENCODING"
    cmd="$cmd -D $GP_DIR"
    cmd="$cmd --locale=$LOCALE_SETTING"
    cmd="$cmd $LC_ALL_SETTINGS"
    cmd="$cmd --max_connections=$QE_MAX_CONNECT"
    cmd="$cmd --shared_buffers=$QE_SHARED_BUFFERS"
    if [ x"$HEAP_CHECKSUM" == x"on" ]; then
        cmd="$cmd --data-checksums"
    fi
    cmd="$cmd --backend_output=$GP_DIR.initdb"

    $TRUSTED_SHELL ${GP_HOSTADDRESS} $cmd >> $LOG_FILE 2>&1
    RETVAL=$?

    # if there was an error, copy ${GP_DIR}.initdb to the log before cleaning it up
    if [ $RETVAL -ne 0 ]; then
	$TRUSTED_SHELL ${GP_HOSTADDRESS} "cat $GP_DIR.initdb" >> $LOG_FILE 2>&1
    fi
    $TRUSTED_SHELL ${GP_HOSTADDRESS} "rm -f $GP_DIR.initdb" >> $LOG_FILE 2>&1
    BACKOUT_COMMAND "$TRUSTED_SHELL ${GP_HOSTADDRESS} \"$RM -rf $GP_DIR > /dev/null 2>&1\""
    BACKOUT_COMMAND "$ECHO \"removing directory $GP_DIR on $GP_HOSTADDRESS\""
    PARA_EXIT $RETVAL "to start segment instance database $GP_HOSTADDRESS $GP_DIR"

    # on mirror, copy data from primary
    if [ x"" != x"$COPY_FROM_PRIMARY_HOSTADDRESS" ]; then
        LOG_MSG "[INFO]:-Running pg_basebackup to init mirror on ${GP_HOSTADDRESS} using primary on ${COPY_FROM_PRIMARY_HOSTADDRESS} ..." 1
        RUN_COMMAND_REMOTE ${COPY_FROM_PRIMARY_HOSTADDRESS} "${EXPORT_GPHOME}; . ${GPHOME}/greenplum_path.sh; echo 'host  replication ${GP_USER} samenet trust' >> ${COPY_FROM_PRIMARY_DIR}/pg_hba.conf; pg_ctl -D ${COPY_FROM_PRIMARY_DIR} reload"
        RUN_COMMAND_REMOTE ${GP_HOSTADDRESS} "${EXPORT_GPHOME}; . ${GPHOME}/greenplum_path.sh; rm -rf ${GP_DIR}; ${GPHOME}/bin/pg_basebackup -x -R -c fast -E ./pg_log -E ./db_dumps -E ./gpperfmon/data -E ./gpperfmon/logs -D ${GP_DIR} -h ${COPY_FROM_PRIMARY_HOSTADDRESS} -p ${COPY_FROM_PRIMARY_PORT}; mkdir ${GP_DIR}/pg_log; mkdir ${GP_DIR}/pg_xlog/archive_status"
        REGISTER_MIRROR
        START_QE ""
        RETVAL=$?
        PARA_EXIT $RETVAL "pg_basebackup of segment data directory from ${COPY_FROM_PRIMARY_HOSTADDRESS} to ${GP_HOSTADDRESS}"
    fi

    # do primary stuff
    if [ x"" = x"$COPY_FROM_PRIMARY_HOSTADDRESS" ]; then
        # Configure postgresql.conf
        LOG_MSG "[INFO][$INST_COUNT]:-Configuring segment $PG_CONF"
        $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO \"#MPP Specific parameters\" >> ${GP_DIR}/$PG_CONF"
        RETVAL=$?
        PARA_EXIT $RETVAL "Update ${GP_DIR}/$PG_CONF file"
        $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO \"#----------------------\" >> ${GP_DIR}/$PG_CONF"
        RETVAL=$?
        PARA_EXIT $RETVAL "Update ${GP_DIR}/$PG_CONF file"
        SED_PG_CONF ${GP_DIR}/$PG_CONF "$PORT_TXT" port=$GP_PORT 0 $GP_HOSTADDRESS
        PARA_EXIT $RETVAL "Update port number to $GP_PORT"
        SED_PG_CONF ${GP_DIR}/$PG_CONF "$LISTEN_ADR_TXT" listen_addresses=\'*\' 0 $GP_HOSTADDRESS
        PARA_EXIT $RETVAL "Update listen address"
        SED_PG_CONF ${GP_DIR}/$PG_CONF "$CHKPOINT_SEG_TXT" checkpoint_segments=$CHECK_POINT_SEGMENTS 0 $GP_HOSTADDRESS
        PARA_EXIT $RETVAL "Update checkpoint segments"

        if [ x"" != x"$PG_CONF_ADD_FILE" ]; then
	        LOG_MSG "[INFO][$INST_COUNT]:-Processing additional configuration parameters"
	        for NEW_PARAM in `$CAT $PG_CONF_ADD_FILE|$TR -s ' '|$TR -d ' '|$GREP -v "^#"`
	        do
	            LOG_MSG "[INFO][$INST_COUNT]:-Adding config $NEW_PARAM to segment"
	            SEARCH_TXT=`$ECHO $NEW_PARAM |$CUT -d"=" -f1`
	            SED_PG_CONF ${GP_DIR}/$PG_CONF $SEARCH_TXT $NEW_PARAM 0 $GP_HOSTADDRESS
	            PARA_EXIT $RETVAL "Update $PG_CONF $SEARCH_TXT $NEW_PARAM"
	        done
        fi

        # Configuring PG_HBA  -- on mirror, only need to add local addresses (skip the other addresses)
        LOG_MSG "[INFO][$INST_COUNT]:-Configuring segment $PG_HBA"
        if [ x"" = x"$COPY_FROM_PRIMARY_HOSTADDRESS" ]; then
	        for MASTER_IP in "${MASTER_IP_ADDRESS[@]}"
	        do
	            # MPP-15889
	            CIDR_MASTER_IP=$(GET_CIDRADDR $MASTER_IP)
	            $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host	all	all	${CIDR_MASTER_IP}	trust >> ${GP_DIR}/$PG_HBA"
	            PARA_EXIT $? "Update $PG_HBA for master IP address ${CIDR_MASTER_IP}"
	        done
	        if [ x"" != x"$STANDBY_HOSTNAME" ];then
	            LOG_MSG "[INFO][$INST_COUNT]:-Processing Standby master IP address for segment instances"
	            for STANDBY_IP in "${STANDBY_IP_ADDRESS[@]}"
	            do
	                # MPP-15889
	                CIDR_STANDBY_IP=$(GET_CIDRADDR $STANDBY_IP)
		            $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host	all	all	${CIDR_STANDBY_IP}	trust >> ${GP_DIR}/$PG_HBA"
		            PARA_EXIT $? "Update $PG_HBA for master standby address ${CIDR_STANDBY_IP}"
	            done
	        fi
        fi

        # Add all local IPV4 addresses
        SEGMENT_IPV4_LOCAL_ADDRESS_ALL=(`$TRUSTED_SHELL $GP_HOSTADDRESS "$IPV4_ADDR_LIST_CMD | $GREP inet | $GREP -v \"127.0.0\" | $AWK '{print \\$2}' | $CUT -d'/' -f1"`)
        for ADDR in "${SEGMENT_IPV4_LOCAL_ADDRESS_ALL[@]}"
        do
	        # MPP-15889
	        CIDR_ADDR=$(GET_CIDRADDR $ADDR)
            $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host     all          $USER_NAME         $CIDR_ADDR      trust >> ${GP_DIR}/$PG_HBA"
        done

        # Add all local IPV6 addresses
        SEGMENT_IPV6_LOCAL_ADDRESS_ALL=(`$TRUSTED_SHELL $GP_HOSTADDRESS "$IPV6_ADDR_LIST_CMD | $GREP inet6 | $AWK '{print \\$2}' | $CUT -d'/' -f1"`)
        for ADDR in "${SEGMENT_IPV6_LOCAL_ADDRESS_ALL[@]}"
        do
	        # MPP-15889
	        CIDR_ADDR=$(GET_CIDRADDR $ADDR)
	        $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host     all          $USER_NAME         $CIDR_ADDR      trust >> ${GP_DIR}/$PG_HBA"
        done

        # start primary
        START_QE "-w"
    fi

    LOG_MSG "[INFO][$INST_COUNT]:-End Function $FUNCNAME"
}

REGISTER_MIRROR() {
    LOG_MSG "[INFO]:-Start Function $FUNCNAME"
    env PGOPTIONS="-c gp_session_role=utility" $PSQL "${DEFAULTDB}" -c "select pg_catalog.gp_add_segment_mirror(${GP_CONTENT}::int2, '${GP_HOSTADDRESS}', '${GP_HOSTADDRESS}', ${GP_PORT}, '${GP_DIR}')" >> $LOG_FILE 2>&1
    LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

STOP_QE() {
    # we don't add backout commands here.  We could get double-stop calls since the same QE is sometimes started, stopped, and restarted.  But hopefully that's okay

    LOG_MSG "[INFO]:-Start Function $FUNCNAME" 1
    LOG_MSG "[INFO]:-Stopping instance on segment ${GP_HOSTADDRESS}:${GP_PORT}" 1
    $TRUSTED_SHELL ${GP_HOSTADDRESS} "$EXPORT_LIB_PATH;export PGPORT=${GP_PORT}; $PG_CTL -w -l $GP_DIR/pg_log/startup.log -D $GP_DIR -o \"-i -p ${GP_PORT}\" stop" >> $LOG_FILE 2>&1
    LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

START_QE() {
	LOG_MSG "[INFO][$INST_COUNT]:-Starting Functioning instance on segment ${GP_HOSTADDRESS}"
	PG_CTL_WAIT=$1
	$TRUSTED_SHELL ${GP_HOSTADDRESS} "$EXPORT_LIB_PATH;export PGPORT=${GP_PORT}; $PG_CTL $PG_CTL_WAIT -l $GP_DIR/pg_log/startup.log -D $GP_DIR -o \"-i -p ${GP_PORT} --gp_dbid=${GP_DBID} --gp_contentid=${GP_CONTENT} --gp_num_contents_in_cluster=0\" start" >> $LOG_FILE 2>&1
	RETVAL=$?
	if [ $RETVAL -ne 0 ]; then
		BACKOUT_COMMAND "$TRUSTED_SHELL $GP_HOSTADDRESS \"${EXPORT_LIB_PATH};export PGPORT=${GP_PORT}; $PG_CTL -w -D $GP_DIR -o \"-i -p ${GP_PORT}\" -m immediate  stop\""
		BACKOUT_COMMAND "$ECHO \"Stopping segment instance on $GP_HOSTADDRESS\""
		$TRUSTED_SHELL ${GP_HOSTADDRESS} "$CAT ${GP_DIR}/pg_log/startup.log "|$TEE -a $LOG_FILE
		PARA_EXIT $RETVAL "Start segment instance database"
	fi	
	BACKOUT_COMMAND "$TRUSTED_SHELL $GP_HOSTADDRESS \"${EXPORT_LIB_PATH};export PGPORT=${GP_PORT}; $PG_CTL -w -D $GP_DIR -o \"-i -p ${GP_PORT}\" -m immediate  stop\""
	BACKOUT_COMMAND "$ECHO \"Stopping segment instance on $GP_HOSTADDRESS\""
	LOG_MSG "[INFO][$INST_COUNT]:-Successfully started segment instance on $GP_HOSTADDRESS"
}

#******************************************************************************
# Main Section
#******************************************************************************
trap '$ECHO "KILLED:$SEGMENT_LINE" >> $PARALLEL_STATUS_FILE;ERROR_EXIT "[FATAL]:-[$INST_COUNT]-Recieved INT or TERM signal" 2' INT TERM
while getopts ":qp:" opt
do
	case $opt in
		q ) unset VERBOSE ;;
		p ) PG_CONF_ADD_FILE=$OPTARG
		    shift
		    shift ;;
		\? ) echo "Invalid option: -$OPTARG"
			 exit 1 ;;
	esac
done

#Now process supplied call parameters
PARENT_PID=$1;shift		#PID of calling gpinitsystem program
CHK_CALL

TYPE=$1;shift
case $TYPE in
	1 )
		SEGMENT_LINE=$1;shift		#String used to build segment instance
        COPY_FROM_PRIMARY_SEGMENT_LINE=$1;shift # String used to build primary segment instance from which to copy data
		if [ x"IS_PRIMARY" != x"$COPY_FROM_PRIMARY_SEGMENT_LINE" ]; then
			SET_VAR $COPY_FROM_PRIMARY_SEGMENT_LINE
			COPY_FROM_PRIMARY_HOSTADDRESS=$GP_HOSTADDRESS
			COPY_FROM_PRIMARY_DIR=$GP_DIR
			COPY_FROM_PRIMARY_PORT=$GP_PORT
            COPY_FROM_PRIMARY_CONTENT=$GP_CONTENT
		fi
		SET_VAR $SEGMENT_LINE

        if [ x"IS_PRIMARY" != x"$COPY_FROM_PRIMARY_SEGMENT_LINE" ]; then
            if [ x"$GP_CONTENT" != x"$COPY_FROM_PRIMARY_CONTENT" ]; then
                $ECHO "[FATAL]:-mismatch between content id and primary content id" 
		        exit 2
            fi
        fi
		INST_COUNT=$1;shift		#Unique number for this parallel script, starts at 0
		BACKOUT_FILE=/tmp/gpsegcreate.sh_backout.$$
		LOG_FILE=$1;shift		#Central logging file
		LOG_MSG "[INFO][$INST_COUNT]:-Start Main"
		LOG_MSG "[INFO][$INST_COUNT]:-Command line options passed to utility = $*"
		HEAP_CHECKSUM=$1;shift
		TMP_MASTER_IP_ADDRESS=$1;shift	#List of IP addresses for the master instance
		MASTER_IP_ADDRESS=(`$ECHO $TMP_MASTER_IP_ADDRESS|$TR '~' ' '`)
		TMP_STANDBY_IP_ADDRESS=$1;shift #List of IP addresses for standby master
		STANDBY_IP_ADDRESS=(`$ECHO $TMP_STANDBY_IP_ADDRESS|$TR '~' ' '`)
		PROCESS_QE
		$ECHO "COMPLETED:$SEGMENT_LINE" >> $PARALLEL_STATUS_FILE
		;;
esac

LOG_MSG "[INFO][$INST_COUNT]:-End Main"
exit $EXIT_STATUS
