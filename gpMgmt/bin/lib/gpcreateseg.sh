#!/usr/bin/env bash
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
CONTENT_ID_TXT="gp_contentid"
DBID_TXT="gp_dbid"
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
    GP_HOSTNAME=`$ECHO $I|$CUT -d$S -f1`
    GP_HOSTADDRESS=`$ECHO $I|$CUT -d$S -f2`
    GP_PORT=`$ECHO $I|$CUT -d$S -f3`
    GP_DIR=`$ECHO $I|$CUT -d$S -f4`
    GP_DBID=`$ECHO $I|$CUT -d$S -f5`
    GP_CONTENT=`$ECHO $I|$CUT -d$S -f6`
}

PARA_EXIT () {
	if [ $1 -ne 0 ];then
		$ECHO "FAILED:$SEGMENT_TO_CREATE" >> $PARALLEL_STATUS_FILE
		LOG_MSG "[FATAL][$INST_COUNT]:-Failed $2"
		exit 2
	else
		LOG_MSG "[INFO][$INST_COUNT]:-Completed $2"
	fi
}

ADD_PG_HBA_ENTRIES() {
    local ADDR
    local CIDR_ADDR

    for ADDR in "$@"; do
        # MPP-15889
        CIDR_ADDR=$(GET_CIDRADDR $ADDR)
        $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host     all          $USER_NAME         $CIDR_ADDR      trust >> ${GP_DIR}/$PG_HBA"
    done
}

CREATE_QES_PRIMARY () {
    LOG_MSG "[INFO][$INST_COUNT]:-Start Function $FUNCNAME"
    LOG_MSG "[INFO][$INST_COUNT]:-Processing segment $GP_HOSTADDRESS"
    # build initdb command
    cmd="$EXPORT_LIB_PATH;$INITDB"
    cmd="$cmd -E $ENCODING"
    cmd="$cmd -D $GP_DIR"
    cmd="$cmd --locale=$LOCALE_SETTING"
    cmd="$cmd $LC_ALL_SETTINGS"
    cmd="$cmd --max_connections=$QE_MAX_CONNECT"
    cmd="$cmd --shared_buffers=$QE_SHARED_BUFFERS"
    if [ x"$HEAP_CHECKSUM" == x"on" ]; then
        cmd="$cmd --data-checksums"
    fi
    
    $TRUSTED_SHELL ${GP_HOSTADDRESS} $cmd >> $LOG_FILE 2>&1
    RETVAL=$?
    
    BACKOUT_COMMAND "$TRUSTED_SHELL ${GP_HOSTADDRESS} \"$RM -rf $GP_DIR > /dev/null 2>&1\""
    BACKOUT_COMMAND "$ECHO \"removing directory $GP_DIR on $GP_HOSTADDRESS\""
    PARA_EXIT $RETVAL "to start segment instance database $GP_HOSTADDRESS $GP_DIR"
    
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
    SED_PG_CONF ${GP_DIR}/$PG_CONF "$CONTENT_ID_TXT" "gp_contentid=${GP_CONTENT}" 0 $GP_HOSTADDRESS
    PARA_EXIT $RETVAL "Update gp_contentid"
    SED_PG_CONF ${GP_DIR}/$PG_INTERNAL_CONF "$DBID_TXT" "gp_dbid=${GP_DBID}" 0 $GP_HOSTADDRESS
    PARA_EXIT $RETVAL "Update gp_dbid"
    
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
    # Configuring PG_HBA
    LOG_MSG "[INFO][$INST_COUNT]:-Configuring segment $PG_HBA"
    if [ $HBA_HOSTNAMES -eq 0 ]; then
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
    
        # Add all local IPV4 addresses
        SEGMENT_IPV4_LOCAL_ADDRESS_ALL=(`$TRUSTED_SHELL $GP_HOSTADDRESS "$IPV4_ADDR_LIST_CMD | $GREP inet | $GREP -v \"127.0.0\" | $AWK '{print \\$2}' | $CUT -d'/' -f1"`)
        ADD_PG_HBA_ENTRIES "${SEGMENT_IPV4_LOCAL_ADDRESS_ALL[@]}"
    
        if [ x"" != x"$MIRROR_HOSTADDRESS" ]; then
          # Add all mirror segments local IPV4 addresses
          MIRROR_SEGMENT_IPV4_LOCAL_ADDRESS_ALL=(`$TRUSTED_SHELL $MIRROR_HOSTADDRESS "$IPV4_ADDR_LIST_CMD | $GREP inet | $GREP -v \"127.0.0\" | $AWK '{print \\$2}' | $CUT -d'/' -f1"`)
          ADD_PG_HBA_ENTRIES "${MIRROR_SEGMENT_IPV4_LOCAL_ADDRESS_ALL[@]}"
        fi
    
        # Add all local IPV6 addresses
        SEGMENT_IPV6_LOCAL_ADDRESS_ALL=(`$TRUSTED_SHELL $GP_HOSTADDRESS "$IPV6_ADDR_LIST_CMD | $GREP inet6 | $AWK '{print \\$2}' | $CUT -d'/' -f1"`)
        ADD_PG_HBA_ENTRIES "${SEGMENT_IPV6_LOCAL_ADDRESS_ALL[@]}"
    
        if [ x"" != x"$MIRROR_HOSTADDRESS" ]; then
          # Add all mirror segments local IPV6 addresses
          MIRROR_SEGMENT_IPV6_LOCAL_ADDRESS_ALL=(`$TRUSTED_SHELL $MIRROR_HOSTADDRESS "$IPV6_ADDR_LIST_CMD | $GREP inet6 | $AWK '{print \\$2}' | $CUT -d'/' -f1"`)
          ADD_PG_HBA_ENTRIES "${MIRROR_SEGMENT_IPV6_LOCAL_ADDRESS_ALL[@]}"
        fi
    else # use hostnames in pg_hba.conf
        # add localhost
        $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host     all          all         localhost      trust >> ${GP_DIR}/$PG_HBA"
        $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host	all	all	${MASTER_HOSTNAME}	trust >> ${GP_DIR}/$PG_HBA"
        if [ x"" != x"$MIRROR_HOSTADDRESS" ]; then
          $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host     all          $USER_NAME         $MIRROR_HOSTADDRESS      trust >> ${GP_DIR}/$PG_HBA"
        fi
        PARA_EXIT $? "Update $PG_HBA for master IP address ${MASTER_HOSTNAME}"
        if [ x"" != x"$STANDBY_HOSTNAME" ];then
            LOG_MSG "[INFO][$INST_COUNT]:-Processing Standby master IP address for segment instances"
            $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host	all	all	${STANDBY_HOSTNAME}	trust >> ${GP_DIR}/$PG_HBA"
            PARA_EXIT $? "Update $PG_HBA for master standby address ${STANDBY_HOSTNAME}"
        fi
        $TRUSTED_SHELL ${GP_HOSTADDRESS} "$ECHO host     all          $USER_NAME         $GP_HOSTADDRESS      trust >> ${GP_DIR}/$PG_HBA"
    fi
    LOG_MSG "[INFO][$INST_COUNT]:-End Function $FUNCNAME"
}

CREATE_QES_MIRROR () {
    LOG_MSG "[INFO][$INST_COUNT]:-Start Function $FUNCNAME"
    LOG_MSG "[INFO][$INST_COUNT]:-Processing segment $GP_HOSTADDRESS"
    # on mirror, just copy data from primary as the primary has all the relevant pg_hba.conf content
    # only the entry for replication is added on the primary if mirror hosts are there
    LOG_MSG "[INFO]:-Running pg_basebackup to init mirror on ${GP_HOSTADDRESS} using primary on ${PRIMARY_HOSTADDRESS} ..." 1
    # Add the samehost replication entry to support single-host development
    local PG_HBA_ENTRIES="${PG_HBA_ENTRIES}"$'\n'"host  replication ${GP_USER} samehost trust"
    if [ $HBA_HOSTNAMES -eq 0 ];then
        local MIRROR_ADDRESSES=($($TRUSTED_SHELL ${GP_HOSTADDRESS} "${GPHOME}"/libexec/ifaddrs --no-loopback))
        local PRIMARY_ADDRESSES=($($TRUSTED_SHELL ${PRIMARY_HOSTADDRESS} "${GPHOME}"/libexec/ifaddrs --no-loopback))
        for ADDR in "${MIRROR_ADDRESSES[@]}" "${PRIMARY_ADDRESSES[@]}"
        do
            CIDR_ADDR=$(GET_CIDRADDR $ADDR)
            PG_HBA_ENTRIES="${PG_HBA_ENTRIES}"$'\n'"host  replication ${GP_USER} ${CIDR_ADDR} trust"
        done
    else
        PG_HBA_ENTRIES="${PG_HBA_ENTRIES}"$'\n'"host  replication ${GP_USER} ${GP_HOSTADDRESS} trust"
        if [ "${GP_HOSTADDRESS}" != "${PRIMARY_HOSTADDRESS}" ]; then
            PG_HBA_ENTRIES="${PG_HBA_ENTRIES}"$'\n'"host  replication ${GP_USER} ${PRIMARY_HOSTADDRESS} trust"
        fi
    fi
    RUN_COMMAND_REMOTE ${PRIMARY_HOSTADDRESS} "${EXPORT_GPHOME}; . ${GPHOME}/greenplum_path.sh; cat - >> ${PRIMARY_DIR}/pg_hba.conf; pg_ctl -D ${PRIMARY_DIR} reload" <<< "${PG_HBA_ENTRIES}"
    RUN_COMMAND_REMOTE ${GP_HOSTADDRESS} "${EXPORT_GPHOME}; . ${GPHOME}/greenplum_path.sh; rm -rf ${GP_DIR}; ${GPHOME}/bin/pg_basebackup --xlog-method=stream --slot='internal_wal_replication_slot' -R -c fast -E ./db_dumps -D ${GP_DIR} -h ${PRIMARY_HOSTADDRESS} -p ${PRIMARY_PORT} --target-gp-dbid ${GP_DBID};"
    START_QE "-w"
    RETVAL=$?
    PARA_EXIT $RETVAL "pg_basebackup of segment data directory from ${PRIMARY_HOSTADDRESS} to ${GP_HOSTADDRESS}"
    LOG_MSG "[INFO][$INST_COUNT]:-End Function $FUNCNAME"
}

START_QE() {
	LOG_MSG "[INFO][$INST_COUNT]:-Starting Functioning instance on segment ${GP_HOSTADDRESS}"
	PG_CTL_WAIT=$1
	$TRUSTED_SHELL ${GP_HOSTADDRESS} "$EXPORT_LIB_PATH;export PGPORT=${GP_PORT}; $PG_CTL $PG_CTL_WAIT -l $GP_DIR/pg_log/startup.log -D $GP_DIR -o \"-p ${GP_PORT} -c gp_role=execute\" start" >> $LOG_FILE 2>&1
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
trap '$ECHO "KILLED:$SEGMENT_TO_CREATE" >> $PARALLEL_STATUS_FILE;ERROR_EXIT "[FATAL]:-[$INST_COUNT]-Recieved INT or TERM signal"' INT TERM
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

# gpcreateseg.sh is called for creating primary and mirror segments.
# Below is an example for invocation to create a primary
# MASTER_HOSTNAME=bhuvi.local HBA_HOSTNAMES=0 /usr/local/gpdb/bin/lib/gpcreateseg.sh -p clusterConfigPostgresAddonsFile 65324 1
# IS_PRIMARY host1.local~45432~/tmp/demoDataDir0~2~0 host2.local~45433~/tmp/demoDataDir0~3~0 0
# /tmp/gpAdminLogs/gpinitsystem_20190903.log on  10.64.249.254~192.168.1.72~::1~fe80::1%lo0 10.64.249.252~192.168.1.73~::1~fe81::1%lo1 &
# PARALLEL_COUNT 1 4
# Now process supplied call parameters
PARENT_PID=$1;shift		# PID of calling gpinitsystem program ex: 31578
CHK_CALL

# for primary: it's IS_PRIMARY
# for mirror: it's IS_MIRROR
PRIMARY_OR_MIRROR_IDENTIFIER=$1;shift

# string containing details of segment to create, format: hostname~port~datadir~dbid~content
SEGMENT_TO_CREATE=$1;shift

# string containing details of the corresponding pair (if mirroring enabled else empty)
CORRESPONDING_PAIR_INFO=$1;shift

# SET_VAR populates, GP_HOSTADDRESS, GP_DIR, GP_PORT, GP_CONTENT
SET_VAR $CORRESPONDING_PAIR_INFO
if [ x"IS_MIRROR" == x"$PRIMARY_OR_MIRROR_IDENTIFIER" ]; then
   # store the corresponding primary info
   PRIMARY_HOSTADDRESS=$GP_HOSTADDRESS
   PRIMARY_DIR=$GP_DIR
   PRIMARY_PORT=$GP_PORT
   PRIMARY_CONTENT=$GP_CONTENT
else
   # store the corresponding mirror hostname
   MIRROR_HOSTADDRESS=$GP_HOSTADDRESS
fi

# update GP_HOSTADDRESS, GP_DIR, GP_PORT, GP_CONTENT with the segment to create
SET_VAR $SEGMENT_TO_CREATE

if [ x"IS_MIRROR" == x"$PRIMARY_OR_MIRROR_IDENTIFIER" ]; then
    if [ x"$GP_CONTENT" != x"$PRIMARY_CONTENT" ]; then
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
if [ x"IS_PRIMARY" == x"$PRIMARY_OR_MIRROR_IDENTIFIER" ]; then
    CREATE_QES_PRIMARY
else
    CREATE_QES_MIRROR
fi
$ECHO "COMPLETED:$SEGMENT_TO_CREATE" >> $PARALLEL_STATUS_FILE

LOG_MSG "[INFO][$INST_COUNT]:-End Main"
exit $EXIT_STATUS
