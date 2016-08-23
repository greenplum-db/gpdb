#!/bin/bash
#	Filename:-		gp_bash_functions.sh
#	Version:-		$Revision$
#	Updated:-		$Date$
#	Status:-		Released	
#	Author:-		G L Coombe (Greenplum)
#	Contact:-		gcoombe@greenplum.com
#	Release date:-		March 2006
#	Release stat:-		Greenplum Internal
#                               Copyright (c) Metapa 2005. All Rights Reserved.
#                               Copyright (c) Greenplum 2005. All Rights Reserved
#	Brief descn:-		Common functions used by various scripts
#******************************************************************************
# Update History
#******************************************************************************
# Ver	Date		Who		Update
#******************************************************************************
# Detailed Description
#******************************************************************************
#
#******************************************************************************
# Prep Code
#
#***************************************************************
# Location Functions
#******************************************************************************
#Check that SHELL is /bin/bash
	if [ $SHELL != /bin/bash ] && [ `ls -al /bin/sh|grep -c bash` -ne 1 ];then
		echo "[FATAL]:-Scripts must be run by a user account that has SHELL=/bin/bash"
		if [ -f /bin/bash ];then
			echo "[INFO]:-/bin/bash exists, please update user account shell"
		else
			echo "[WARN]:-/bin/bash does not exist, does bash need to be installed?"
		fi
		exit 2
	fi
#CMDPATH is the list of locations to search for commands, in precedence order
declare -a CMDPATH
CMDPATH=(/usr/kerberos/bin /usr/sfw/bin /opt/sfw/bin /usr/local/bin /bin /usr/bin /sbin /usr/sbin /usr/ucb /sw/bin)

#GPPATH is the list of possible locations for the Greenplum Database binaries, in precedence order
declare -a GPPATH
GPPATH=( $GPHOME $MPPHOME $BIZHOME )
if [ ${#GPPATH[@]} -eq 0 ];then
	echo "[FATAL]:-GPHOME environment variable is required to run GPDB but could not be found."
	echo "Please set it by sourcing the  greenplum_path.sh  in your GPDB installation directory."
	echo "Example: ''. /usr/local/gpdb/greenplum_path.sh''"
	exit 2
fi

#GP_UNIQUE_COMMAND is used to identify the binary directory
GP_UNIQUE_COMMAND=gpstart


findCmdInPath() {
		cmdtofind=$1
		
		if [ $cmdtofind = 'awk' ] && [ `uname` = SunOS ]; then
			if [ -f "/usr/xpg4/bin/awk" ]; then
				CMD=/usr/xpg4/bin/awk
				echo $CMD
				return
			else
				echo $cmdtofind
				return "Problem in gp_bash_functions, command '/usr/xpg4/bin/awk' not found. You will need to edit the script named gp_bash_functions.sh to properly locate the needed commands for your platform."
			fi
		fi
		for pathel in ${CMDPATH[@]}
				do
				CMD=$pathel/$cmdtofind
				if [ x"$CMD" != x"" ] && [ -f $CMD ]; then
						echo $CMD
						return
				fi
		done
		echo $cmdtofind
		return "Problem in gp_bash_functions, command '$cmdtofind' not found in COMMAND path. You will need to edit the script named gp_bash_functions.sh to properly locate the needed commands for your platform."
}

findMppPath() {
		cmdtofind=$GP_UNIQUE_COMMAND

		for pathel in ${GPPATH[@]}
				do
				CMD=`find $pathel -follow -name $cmdtofind | tail -1`
				if [ x"$CMD" != x"" ] && [ -f $CMD ]; then
						echo $CMD
						return
				fi
		done
}

#******************************************************************************
# OS Command Variables
#******************************************************************************
AWK=`findCmdInPath awk`
BASENAME=`findCmdInPath basename`
CAT=`findCmdInPath cat`
CLEAR=`findCmdInPath clear`
CKSUM=`findCmdInPath cksum`
CUT=`findCmdInPath cut`
DATE=`findCmdInPath date`
DD=`findCmdInPath dd`
DIRNAME=`findCmdInPath dirname`
DF=`findCmdInPath df`
DU=`findCmdInPath du`
ECHO=`findCmdInPath echo`
#ETHTOOL=`findCmdInPath ethtool`
EXPR=`findCmdInPath expr`
FIND=`findCmdInPath find`
TABECHO=$ECHO
PROMPT="$ECHO"
#TABECHO="$ECHO -e \t"
#PROMPT="$ECHO -n -e \t"
GREP=`findCmdInPath grep`
GZIPCMD=`findCmdInPath gzip`
EGREP=`findCmdInPath egrep`
HEAD=`findCmdInPath head`
HOSTNAME=`findCmdInPath hostname`
IPCS=`findCmdInPath ipcs`
IFCONFIG=`findCmdInPath ifconfig`
KILL=`findCmdInPath kill`
LESSCMD=`findCmdInPath less`
LS=`findCmdInPath ls`
LOCALE=`findCmdInPath locale`
MV=`findCmdInPath mv`
MORECMD=`findCmdInPath more`
MKDIR=`findCmdInPath mkdir`
MKFIFO=`findCmdInPath mkfifo`
NETSTAT=`findCmdInPath netstat`
PING=`findCmdInPath ping`
PS=`findCmdInPath ps`
PYTHON=${GPHOME}/ext/python/bin/python
RM=`findCmdInPath rm`
SCP=`findCmdInPath scp`
SED=`findCmdInPath sed`
SLEEP=`findCmdInPath sleep`
SORT=`findCmdInPath sort`
SPLIT=`findCmdInPath split`
SSH=`findCmdInPath ssh`
TAIL=`findCmdInPath tail`
TAR=`findCmdInPath tar`
TEE=`findCmdInPath tee`
TOUCH=`findCmdInPath touch`
#PTIME=`findCmdInPath ptime`
TR=`findCmdInPath tr`
WC=`findCmdInPath wc`
WHICH=`findCmdInPath which`
WHOAMI=`findCmdInPath whoami`
ZCAT=`findCmdInPath zcat`
#***************#******************************************************************************
# Script Specific Variables
#******************************************************************************
# By default set error logging level to verbose
VERBOSE=1
USER_NAME=`id|$AWK '{print $1}'|$CUT -d"(" -f2|$TR -d ')'`
PROG_NAME=`echo $0 | $TR -d '-'`
PROG_NAME=`$BASENAME $PROG_NAME`
PROG_PIDNAME=`echo $$ $PROG_NAME | awk '{printf "%06d %s\n", $1, $2}'`
CALL_HOST=`$HOSTNAME|$CUT -d. -f1`

#******************************************************************************
# Locate the postgres routines from the Greenplum release
#******************************************************************************
PSQLBIN=`findMppPath`

if [ x"$PSQLBIN" = x"" ];then
		echo "Problem in gp_bash_functions, command '$GP_UNIQUE_COMMAND' not found in Greenplum path."
		echo "Try setting GPHOME to the location of your Greenplum distribution."
		exit 99
fi

PSQLBIN=`$DIRNAME $PSQLBIN`
SCRIPTDIR="`$DIRNAME $PSQLBIN`/bin"
#******************************************************************************
# Greenplum Scripts
#******************************************************************************
GPACTIVATEMASTER=$SCRIPTDIR/gpactivatemaster
GPACTIVATESTANDBY=$SCRIPTDIR/gpactivatestandby
GPADDMIRRORS=$SCRIPTDIR/gpaddmirrors
GPINITSYSTEM=$SCRIPTDIR/gpinitsystem
GPCONFIG=$SCRIPTDIR/gpconfig
GPCRONDUMP=$SCRIPTDIR/gpcrondump
GPDELETESYSTEM=$SCRIPTDIR/gpdeletesystem
GPINITSTANDBY=$SCRIPTDIR/gpinitstandby
GPREBUILDCLUSTER=$SCRIPTDIR/gprebuildcluster
GPRECOVERSEG=$SCRIPTDIR/gprecoverseg
GPSTART=$SCRIPTDIR/gpstart
GPSTATE=$SCRIPTDIR/gpstate
GPSTOP=$SCRIPTDIR/gpstop
MAILFILE=$SCRIPTDIR/mail_contacts
HMAILFILE=$HOME/mail_contacts
GPDOCDIR=${GPHOME}/docs/cli_help/
GPSUBSCRIPTDIR=${SCRIPTDIR}/lib
#******************************************************************************
# Greenplum Command Variables
#******************************************************************************
INITDB=$PSQLBIN/initdb
MPPLOADER=$CMDBIN/loader.sh
PG_CTL=$PSQLBIN/pg_ctl
PG_DUMP=$PSQLBIN/pg_dump
PG_DUMPALL=$PSQLBIN/pg_dumpall
PG_RESTORE=$PSQLBIN/pg_restore
PSQL=$PSQLBIN/psql
GP_DUMP=$PSQLBIN/gp_dump
GP_RESTORE=$PSQLBIN/gp_restore
GPSYNCMASTER=$PSQLBIN/gpsyncmaster


GPLISTDATABASEQTY="SELECT d.datname as \"Name\",
       r.rolname as \"Owner\",
       pg_catalog.pg_encoding_to_char(d.encoding) as \"Encoding\"
FROM pg_catalog.pg_database d
  JOIN pg_catalog.pg_authid r ON d.datdba = r.oid
ORDER BY 1;"
#******************************************************************************
# Greenplum OS Settings 
#******************************************************************************
OS_OPENFILES=65535
#******************************************************************************
# General Variables
#******************************************************************************
HOSTFILE=/etc/hosts
PG_PID=postmaster.pid
PG_OPT=postmaster.opts
PG_CONF=postgresql.conf
PG_HBA=pg_hba.conf
if [ x"$TRUSTED_SHELL" = x"" ]; then TRUSTED_SHELL="$SSH"; fi
if [ x"$TRUSTED_COPY" = x"" ]; then TRUSTED_COPY="$SCP"; fi
PG_CONF_ADD_FILE=$WORKDIR/postgresql_conf_gp_additions
RECOVER_FLAG=/tmp/active_recovery_in_progress
SCHEMA_FILE=cdb_schema.sql
DEFAULTDB=template1

CONFIG_TABLE="(SELECT dbid, content, role, preferred_role, mode, status,
               hostname, address, port, fselocation as datadir,
               replication_port, san_mounts
        FROM gp_segment_configuration
        JOIN pg_filespace_entry ON (dbid = fsedbid)
        JOIN pg_filespace fs ON (fs.oid = fsefsoid)
        WHERE fsname = 'pg_system')"

GP_PG_VIEW="(SELECT dbid, role = 'p' as isprimary, content, status = 'u' as valid,
		preferred_role = 'p' as definedprimary FROM gp_segment_configuration)"

ACTIVITY_TABLE=pg_stat_activity
GP_INITDB_VER_TABLE=gp_version_at_initdb
CLASS_TABLE=pg_class
EXTERNAL_TABLE=pg_exttable
SCHEMA_TABLE=pg_namespace
ATTRIBUTE_TABLE=pg_attribute
INHERIT_TABLE=pg_inherits
INDEX_TABLE=pg_indexes
TYPE_TABLE=pg_type
CONSTRAINT_TABLE=pg_constraint
RULES_TABLE=pg_rules
PG_SIZE_FUNC=pg_relation_size
DISTRIB_TABLE=gp_distribution_policy
CHILD_TABLE_ID="_c_d_"
DEFAULT_CHK_PT_SEG=8
#DEFAULT_IP_ALLOW="0.0.0.0/0"
DEFAULT_QD_MAX_CONNECT=250
QE_CONNECT_FACTOR=3
# DEFAULT_BUFFERS sets the default shared_buffers unless overridden by '-b'.
# It applies to the master db and segment dbs.  Specify either the number of
# buffers (without suffix) or the amount of memory to use for buffers (with
# case-insensitive suffix 'kB', 'MB' or 'GB').
DEFAULT_BUFFERS=128000kB
DEBUG_LEVEL=0
BATCH_DEFAULT=60
WAIT_LIMIT=1800
WARN_MARK="<<<<<"
IDX_TYPE_ARRAY=(btree bitmap hash)
#******************************************************************************
# Functions
#******************************************************************************

IN_ARRAY () {
    for v in $2; do
        if [ x"$1" == x"$v" ]; then
            return 1
        fi
    done
    return 0
}

CHK_DB_QD_SET () {
		if [ ! $MASTER_DATA_DIRECTORY ]; then
				ERROR_EXIT "[FATAL]:-MASTER_DATA_DIRECTORY parameter not set, update user start-up file [i.e. .bashrc]." 2
		fi
}

VERSION_INFO () {
		VERSION=`$HEAD -10 $0|$GREP -i Version: | $CUT -d# -f2|$CUT -d- -f2|$TR -d '\t'`
		STATUS=`$HEAD -10 $0|$GREP -i Status: | $CUT -d# -f2|$CUT -d- -f2|$TR -d '\t'`
		UPDATE=`$HEAD -10 $0|$GREP -i Updated:| $CUT -d# -f2|$CUT -d- -f2|$TR -d '\t'`
		CHK_SUM=`$CKSUM $0|$AWK '{print $1" "$2}'`
		$ECHO "`basename $0` Version   = $VERSION"
		$ECHO "`basename $0` Status    = $STATUS"
		$ECHO "`basename $0` Update    = $UPDATE"
		$ECHO "`basename $0` Check sum = $CHK_SUM"
		exit 0
}

LOG_MSG () {
		TIME=`$DATE +%H":"%M":"%S`
		CUR_DATE=`$DATE +%Y%m%d`
		DISPLAY_TXT=0
		#Check to see if we need to update value of EXIT_STATUS
		if [ `$ECHO $1|$AWK -F"]" '{print $1}'|$TR -d '\133'|$GREP -c "WARN"` -eq 1 ];then
			EXIT_STATUS=1
		fi
		if [ `$ECHO $1|$AWK -F"]" '{print $1}'|$TR -d '\133'|$GREP -c "FATAL"` -eq 1 ];then
			EXIT_STATUS=2
		fi
		WARN=`$ECHO $1|$AWK -F"]" '{print $1}'|$TR -d '\133'|$GREP -c "WARN"`
		if [ x"$WARN" == x"" ]; then
		    WARN=0
		fi
		if [ $WARN -eq 1 ];then
			EXIT_STATUS=1
		fi
		FATAL=`$ECHO $1|$AWK -F"]" '{print $1}'|$TR -d '\133'|$GREP -c "FATAL"`
		if [ x"$FATAL" == x"" ]; then
		    FATAL=0
		fi
		if [ $FATAL -eq 1 ];then
			EXIT_STATUS=2
		fi
		if [ x"" == x"$DEBUG_LEVEL" ];then
			DEBUG_LEVEL=1
		fi
		if [ $# -eq 2 ];then 
			DISPLAY_TXT=1
		fi
		if [ $VERBOSE ]; then
				if [ $DEBUG_LEVEL -eq 1 ] || [ $DISPLAY_TXT -eq 1 ];then
			        $ECHO "${CUR_DATE}:${TIME}:${PROG_PIDNAME}:${CALL_HOST}:${USER_NAME}-$1" | $TEE -a $LOG_FILE	
				else
				$ECHO "${CUR_DATE}:${TIME}:${PROG_PIDNAME}:${CALL_HOST}:${USER_NAME}-$1" >> $LOG_FILE
				fi
		else
				$ECHO "${CUR_DATE}:${TIME}:${PROG_PIDNAME}:${CALL_HOST}:${USER_NAME}-$1" >> $LOG_FILE
		fi
}

POSTGRES_VERSION_CHK() {
    LOG_MSG "[INFO]:-Start Function $FUNCNAME"
    HOST=$1;shift

    CURRENT_VERSION=`$EXPORT_GPHOME; $EXPORT_LIB_PATH; $GPHOME/bin/postgres --gp-version`
    VERSION_MATCH=0

    VER=`$TRUSTED_SHELL $HOST "$EXPORT_GPHOME; $EXPORT_LIB_PATH; $GPHOME/bin/postgres --gp-version"`
    if [ $? -ne 0 ] ; then
	LOG_MSG "[WARN]:- Failed to obtain postgres version on $HOST" 1
	EXIT_STATUS=1
	VERSION_MATCH=0
    fi
    LOG_MSG "[INFO]:- Current postgres version = $CURRENT_VERSION"
    LOG_MSG "[INFO]:- postgres version on $HOST = $VER"

    if [ x"$VER" != x"$CURRENT_VERSION" ] ; then
	LOG_MSG "[WARN]:-Postgres version does not match. [$CURRENT_VERSION != $VER]" 1
	VERSION_MATCH=0
	EXIT_STATUS=1
    else
	VERSION_MATCH=1
    fi
    

    LOG_MSG "[INFO]:-End Function $FUNCNAME"

}

ERROR_EXIT () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		TIME=`$DATE +%H":"%M":"%S`
		CUR_DATE=`$DATE +%Y%m%d`
		$ECHO "${CUR_DATE}:${TIME}:${PROG_NAME}:${CALL_HOST}:${USER_NAME}-$1 Script Exiting!" >> $LOG_FILE
		$ECHO "${CUR_DATE}:${TIME}:${PROG_NAME}:${CALL_HOST}:${USER_NAME}-$1 Script Exiting!"
		DEBUG_LEVEL=1
		if [ $BACKOUT_FILE ]; then
				if [ -s $BACKOUT_FILE ]; then
						LOG_MSG "[WARN]:-Script has left Greenplum Database in an incomplete state"
						LOG_MSG "[WARN]:-Run command /bin/bash $BACKOUT_FILE to remove these changes"
						BACKOUT_COMMAND "if [ x$MASTER_HOSTNAME != x\`$HOSTNAME\` ];then $ECHO \"[FATAL]:-Not on original master host $MASTER_HOSTNAME, backout script exiting!\";exit 2;fi"
						$ECHO "$RM -f $BACKOUT_FILE" >> $BACKOUT_FILE
				fi
		fi
		exit $2
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

READ_CHK () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	NROWS=`env PGOPTIONS="-c gp_session_role=utility" $PSQL -A -t -p $MASTER_PORT -d "$DEFAULTDB" -c" select * from $CONFIG_TABLE a where port<0;"`
	RETVAL=$?
	if [ $RETVAL -ne 0 ];then
		READ_COUNT=0
		#Could not connect to database so set read count to zero
		LOG_MSG "[WARN]:-Could not obtain read count from database"
		EXIT_STATUS=1
	else
		if [ x"" == x"$NROWS" ];then
			READ_COUNT=0
		else
			READ_COUNT=`$ECHO $NROWS|$WC -l`
		fi
		LOG_MSG "[INFO]:-Obtained -ve port read count $READ_COUNT"
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

ERROR_CHK () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	if [ $# -ne 3 ];then
		INITIAL_LEVEL=$DEBUG_LEVEL
		DEBUG_LEVEL=1
		LOG_MSG "[WARN]:-Incorrect # parameters supplied to $FUNCNAME"
		DEBUG_LEVEL=$INITIAL_LEVEL
		return;fi
	RETVAL=$1;shift
	MSG_TXT=$1;shift
	ACTION=$1 #1=issue warn, 2=fatal
	if [ $RETVAL -eq 0 ];then
		LOG_MSG "[INFO]:-Successfully completed $MSG_TXT"
	else
		if [ $ACTION -eq 1 ];then
			INITIAL_LEVEL=$DEBUG_LEVEL
			DEBUG_LEVEL=1
			LOG_MSG "[WARN]:-Issue with $MSG_TXT"
			EXIT_STATUS=1
			DEBUG_LEVEL=$INITIAL_LEVEL
		else
			LOG_MSG "[INFO]:-End Function $FUNCNAME"
			ERROR_EXIT "[FATAL]:-Failed to complete $MSG_TXT " 2
		fi
	fi	
	LOG_MSG "[INFO]:-End Function $FUNCNAME"	
}

SED_PG_CONF () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	SED_TMP_FILE=/tmp/sed_text.$$
	APPEND=0
	FILENAME=$1;shift
	SEARCH_TXT=$1;shift
	SUB_TXT="$1";shift
	KEEP_PREV=$1;shift
	SED_HOST=$1
	if [ x"" == x"$SED_HOST" ]; then
			if [ `$GREP -c "${SEARCH_TXT}[ ]*=" $FILENAME` -gt 1 ]; then
				LOG_MSG "[INFO]:-Found more than 1 instance of $SEARCH_TXT in $FILENAME, will append" 1
				APPEND=1
			fi
			if [ `$GREP -c "${SEARCH_TXT}[ ]*=" $FILENAME` -eq 0 ] || [ $APPEND -eq 1 ]; then
				$ECHO $SUB_TXT >> $FILENAME
				RETVAL=$?
				if [ $RETVAL -ne 0 ]; then
					LOG_MSG "[WARN]:-Failed to append line $SUB_TXT to $FILENAME" 1
				else
					LOG_MSG "[INFO]:-Appended line $SUB_TXT to $FILENAME" 
				fi
			else
				if [ $KEEP_PREV -eq 0 ];then
					$SED -i'.bak1' -e "s/${SEARCH_TXT}/${SUB_TXT} #${SEARCH_TXT}/g" $FILENAME
				else
					$SED -i'.bak1' -e "s/${SEARCH_TXT}.*/${SUB_TXT}/g" $FILENAME
				fi
				RETVAL=$?
				if [ $RETVAL -ne 0 ]; then
					LOG_MSG "[WARN]:-Failed to replace $SEARCH_TXT in $FILENAME"
					ERROR_EXIT=1
				else
					LOG_MSG "[INFO]:-Replaced line in $FILENAME"
					$RM -f ${FILENAME}.bak1
				fi
				$SED -i'.bak2' -e "s/^#${SEARCH_TXT}/${SEARCH_TXT}/g" $FILENAME
				RETVAL=$?
				if [ $RETVAL -ne 0 ]; then
					LOG_MSG "[WARN]:-Failed to replace #$SEARCH_TXT in $FILENAME"
					ERROR_EXIT=1
				else
					LOG_MSG "[INFO]:-Replaced line in $FILENAME"
					$RM -f ${FILENAME}.bak2
				fi
			fi
	else
		if [ `$TRUSTED_SHELL $SED_HOST "$GREP -c \"${SEARCH_TXT}\" $FILENAME"` -gt 1 ]; then
			LOG_MSG "[INFO]:-Found more than 1 instance of $SEARCH_TXT in $FILENAME on $SED_HOST, will append" 1
			APPEND=1
		fi
		if [ `$TRUSTED_SHELL $SED_HOST "$GREP -c \"${SEARCH_TXT}\" $FILENAME"` -eq 0 ] || [ $APPEND -eq 1 ]; then
			$TRUSTED_SHELL $SED_HOST "$ECHO \"$SUB_TXT\" >> $FILENAME"
			RETVAL=$?
			if [ $RETVAL -ne 0 ]; then
				LOG_MSG "[WARN]:-Failed to append line $SUB_TXT to $FILENAME on $SED_HOST"
				ERROR_EXIT=1
			else
				LOG_MSG "[INFO]:-Appended line $SUB_TXT to $FILENAME on $SED_HOST" 1
			fi
		else
			if [ $KEEP_PREV -eq 0 ];then
				$ECHO "s/${SEARCH_TXT}/${SUB_TXT} #${SEARCH_TXT}/g" > $SED_TMP_FILE
			else
				$ECHO "s/${SEARCH_TXT}.*/${SUB_TXT}/g" > $SED_TMP_FILE
			fi
			$CAT $SED_TMP_FILE | $TRUSTED_SHELL ${SED_HOST} $DD of=$SED_TMP_FILE > /dev/null 2>&1
			$TRUSTED_SHELL $SED_HOST "sed -i'.bak1' -f $SED_TMP_FILE $FILENAME" > /dev/null 2>&1
			RETVAL=$?
			if [ $RETVAL -ne 0 ]; then
				LOG_MSG "[WARN]:-Failed to insert $SUB_TXT in $FILENAME on $SED_HOST"
				ERROR_EXIT=1
			else
				LOG_MSG "[INFO]:-Replaced line in $FILENAME on $SED_HOST"
				$TRUSTED_SHELL $SED_HOST "$RM -f ${FILENAME}.bak1" > /dev/null 2>&1
			fi
			$ECHO "s/^#${SEARCH_TXT}/${SEARCH_TXT}/g" > $SED_TMP_FILE
			$CAT $SED_TMP_FILE | $TRUSTED_SHELL ${SED_HOST} $DD of=$SED_TMP_FILE > /dev/null 2>&1
			$TRUSTED_SHELL $SED_HOST "sed -i'.bak2' -f $SED_TMP_FILE $FILENAME" > /dev/null 2>&1
			RETVAL=$?
			if [ $RETVAL -ne 0 ]; then
				LOG_MSG "[WARN]:-Failed to substitute #${SEARCH_TXT} in $FILENAME on $SED_HOST"
				ERROR_EXIT=1
			else
				LOG_MSG "[INFO]:-Replaced line in $FILENAME on $SED_HOST"
				$TRUSTED_SHELL $SED_HOST "$RM -f ${FILENAME}.bak2" > /dev/null 2>&1
			fi
			$TRUSTED_SHELL $SED_HOST "$RM -f $SED_TMP_FILE"

			$RM -f $SED_TMP_FILE
		fi
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

CHK_EXTERNAL () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	EXTERNAL=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"select 1 from $EXTERNAL_TABLE where reloid in (select oid from $CLASS_TABLE where relname='$TABLENAME' and relnamespace in (select oid from $SCHEMA_TABLE where nspname='$SCHEMA_NAME'));"|$WC -l`
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}


# Specifies the regular expressions used to parse a qualified table name.
declare -r TABLE_IDENTIFIER_PATTERN='((")((([^"]{1,})|""){1,})")|([^".]{1,})'
declare -r QUALIFIED_TABLE_IDENTIFIER_PATTERN="^((${TABLE_IDENTIFIER_PATTERN})\\.){0,1}(${TABLE_IDENTIFIER_PATTERN})$"

GET_TABLE_IDENTIFIER () {
    # -----------------------------------------------------------------
    # Parses a properly quoted, fully-qualified SQL table name into
    # the schema and table name parts.  If a part is quoted, the quotes
    # are removed; if a part is not quoted, the part is lower-cased.
    #
    # The schema name, if any, is returned in ${SQL_NAME[0]}, the
    # "quoted" version in ${SQL_NAME[2]}; the table name is returned 
    # in ${SQL_NAME[1]}, the "quoted" version in ${SQL_NAME[3]}.
    # 
    # Quoted identifiers can contain any character, except the 
    # character with code zero. (To include a double quote, write two 
    # double quotes.)
    # -----------------------------------------------------------------
    if PARSE_TABLE_IDENTIFIER "${1}" ; then
        # Get schema name; may be omitted
        if [ "${BASH_REMATCH[2]}" ] ; then
            if [ "${BASH_REMATCH[4]}" ] ; then
                # Quoted name; undouble quotes but otherwise take literally
                SQL_NAME[0]="${BASH_REMATCH[5]//\"\"/\"}"
                SQL_NAME[2]="\"${BASH_REMATCH[5]}\""
            else
                # Unquoted name; make lower-case
                if [ "$OS_TYPE" == "sunos" ] ; then
                	SQL_NAME[0]=$($ECHO "${BASH_REMATCH[2]}" | $MBTR '[:upper:]' '[:lower:]')
                else
                	SQL_NAME[0]=$($ECHO "${BASH_REMATCH[2]}" | $TR '[:upper:]' '[:lower:]')
                fi
                SQL_NAME[2]="${SQL_NAME[0]}"
            fi
        else
            SQL_NAME[0]=
        fi
        # Get table name
        if [ "${BASH_REMATCH[11]}" ] ; then
            # Quoted name; undouble quotes but otherwise take literally
            SQL_NAME[1]="${BASH_REMATCH[12]//\"\"/\"}"
            SQL_NAME[3]="\"${BASH_REMATCH[12]}\""
        else
            # Unquoted name; make lower-case
            if [ "$OS_TYPE" == "sunos" ] ; then
                SQL_NAME[1]=$($ECHO "${BASH_REMATCH[9]}" | $MBTR '[:upper:]' '[:lower:]')
            else
                SQL_NAME[1]=$($ECHO "${BASH_REMATCH[9]}" | $TR '[:upper:]' '[:lower:]')
            fi            
            SQL_NAME[3]="${SQL_NAME[1]}"
        fi
    else
        SQL_NAME=('' '' '' '')
    fi
}

# Determine if the bash regular expression operator '=~' is supported.
# If so, define the version of PARSE_TABLE_IDENTIFER using the operator;
# otherwise, use the sed-based version.
if $BASH -c '[[ "string" =~ string ]]' 2>/dev/null ; then
    eval 'PARSE_TABLE_IDENTIFIER () {
        [[ "${1}" =~ ${QUALIFIED_TABLE_IDENTIFIER_PATTERN} ]]
    }'
else
    # Determine the option, if any, for sed to enable support of the regular
    # expression used to parse the qualified table identifier.
    for reOption in "-r" "-E" ""; do
        reTest=$($ECHO "\"Mixed Case\".\"TableName\"" | $SED ${reOption} -e "s/${QUALIFIED_TABLE_IDENTIFIER_PATTERN}/\\9/" 2>/dev/null)
        if [ $? -eq 0 ] && [ "${reTest}" == "\"TableName\"" ] ; then
            break
        fi
        reOption=
    done
    unset reTest
    if [ ! "${reOption}" ] ; then
        ERROR_EXIT "[FATAL]:-$SED lacks support for the regular expression used to parse qualified table identifiers" 2
    fi

    # The PARSE_TABLE_IDENTIFIER function provides a functional replacement for a 
    # [[ "${1}" =~ ${QUALIFIED_TABLE_IDENTIFIER} ]] statement that could be used
    # in the GET_TABLE_IDENTIFIER function in newer versions of BASH.  Once support
    # for "old" BASH is dropped, the call to this function should be replaced with
    # the BASH regular expression pattern matching expression above.  (Attempts to
    # conditionally use the =~ operator fail BASH syntax checking.)
    PARSE_TABLE_IDENTIFIER () {
        local nl=$'\\\n'
        local nullTag=$'~\t~'
        names="$($ECHO "${1}" | $SED ${reOption} -n -e "
            p
            h
            # Process the schema portion of the name
            s/${QUALIFIED_TABLE_IDENTIFIER_PATTERN}/${nl}\\1${nl}\\2${nl}\\3${nl}\\4${nl}\\5${nl}\\6${nl}\\7${nl}\\8/
            x
            # Reduce to table portion of the name (must have)
            s/${QUALIFIED_TABLE_IDENTIFIER_PATTERN}/\\9/
            t haveTable
            q
            :haveTable
            s/(${TABLE_IDENTIFIER_PATTERN})$/\\1${nl}\\2${nl}\\3${nl}\\4${nl}\\5${nl}\\6${nl}\\7${nl}/
            H
            g
            # Word splitting eliminates adjacent separators; provide a "null" value
            :setNulls
            s/\\n\\n/${nl}${nullTag}${nl}/g
            t setNulls
            p
            ")"
        local oldIFS="${IFS}"
        IFS=$'\n'
        BASH_REMATCH=(${names})
        IFS="${oldIFS}"
        if [ "${#BASH_REMATCH[@]}" -eq 1 ]
        then
            # No table name appeared (only original string is in ${names}
            BASH_REMATCH=()
            return 1
        else
            # Reduce the "null" value to an actual empty string
            local i
            for (( i=0; i <"${#BASH_REMATCH[@]}"; i++ )); do
                [ "${BASH_REMATCH[$i]}" == "${nullTag}" ] && BASH_REMATCH[$i]=""
            done
        fi
        return 0
    }

fi

CHK_EXISTS_TABLE () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	TABLENAME="$1"
	MASTER_PORT=$2
	QD_DBNAME=$3
	if [ $# -ne 3 ];then
		ERROR_EXIT "[FATAL]:-Incorrect number of parameters passed expected 3 got $#" 2;fi
	DB_THERE=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$DEFAULTDB" -c"select 1 from pg_database where datname='$QD_DBNAME';" |$WC -l`
	if [ $DB_THERE -eq 0 ];then
		ERROR_EXIT "[FATAL]:-Database $QD_DBNAME does not exist" 2
	fi
	LOG_MSG "[INFO]:-Checking for table $TABLENAME in database $QD_DBNAME"
	GET_TABLE_IDENTIFIER "$TABLENAME"
	SCHEMA_NAME="${SQL_NAME[0]}"
	QUOTED_SCHEMA_NAME="${SQL_NAME[2]}"
	TABLENAME="${SQL_NAME[1]}"
	QUOTED_TABLENAME="${SQL_NAME[3]}"
	if [ "${SCHEMA_NAME}" ];then
		TMP_OUT=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"select 1 from pg_class where relname='$TABLENAME' and relnamespace in (select oid from pg_namespace where nspname='$SCHEMA_NAME');"`
		TAB_COUNT=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"select 1 from pg_class where relname='$TABLENAME' and relnamespace in (select oid from pg_namespace where nspname='$SCHEMA_NAME');" |$WC -l`
		if [ $TAB_COUNT -eq 1 ];then
			LOG_MSG "[INFO]:-Found table ${QUOTED_SCHEMA_NAME}.${QUOTED_TABLENAME} in database $QD_DBNAME"
			CHK_EXTERNAL
			PARTITIONED=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"select relhassubclass from pg_class where relname='$TABLENAME' and relnamespace in (select oid from pg_namespace where nspname='$SCHEMA_NAME');"`
		fi
	else
	    TMP_OUT=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"select 1 from pg_class where relname='$TABLENAME';"`
		TAB_COUNT=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"select 1 from pg_class where relname='$TABLENAME';" |$WC -l`
		if [ $TAB_COUNT -eq 1 ];then
			PARTITIONED=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"select relhassubclass from pg_class where relname='$TABLENAME';"`
			SCHEMA_NAME=`$EXPORT_LIB_PATH;$PSQL -A -t -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"select nspname from pg_namespace where oid in(select relnamespace from pg_class where relname='$TABLENAME');"`
			CHK_EXTERNAL
		fi
	fi
#		$EXPORT_LIB_PATH;$PSQL -q -p  $MASTER_PORT -d "$QD_DBNAME" -c"\d $TABLENAME;" -o /dev/null
	if [ $TAB_COUNT -eq 0 ] || [ $TAB_COUNT -gt 1 ];then
		EXISTS=1
	else
		EXISTS=0
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

EVENT_MAIL () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	case $OS_TYPE in
		sunos ) MAIL=/bin/mailx ;;
		*) MAIL=`findCmdInPath mail` ;;
	esac
	MAIL_SUBJECT=$1;shift
	MAIL_MESSAGE=$1
	if [ ! -f $MAILFILE ] && [ ! -f $HMAILFILE ] ;then
		LOG_MSG "[WARN]:-No $MAILFILE" 1
		LOG_MSG "[WARN]:-or $HMAILFILE" 1
		LOG_MSG "[WARN]:-Unable to send email notification" 1
		LOG_MSG "[INFO]:-To enable email notification, create $MAILFILE" 1
		LOG_MSG "[INFO]:-or $HMAILFILE containing required email addresses" 1
		EXIT_STATUS=1
	else
		if [ -f $HMAILFILE ];then MAILFILE=$HMAILFILE;fi
		for MAIL_ADDRESS in `$CAT $MAILFILE | $GREP -v "^#"`
		do
			LOG_MSG "[INFO]:-Sending mail to $MAIL_ADDRESS using file $MAILFILE" 1
			$ECHO "$MAIL_MESSAGE"|$MAIL -s "$MAIL_SUBJECT" $MAIL_ADDRESS
			ERROR_CHK $? "send email to $MAIL_ADDRESS" 1
		done
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

POSTGRES_PORT_CHK () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	GET_PG_PID_ACTIVE $1 $2
	if [ $PID -ne 0 ];then
		ERROR_EXIT "[FATAL]:-Host $2 has an active database process on port = $1" 2
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

CREATE_SPREAD_MIRROR_ARRAY () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	((MAX_ARRAY=${#QE_PRIMARY_ARRAY[@]}-1))

	# Current host and subnet we are working on
	CURRENT_HOST=0
	CURRENT_SUBNET=0
	
	# Destination host and subnet
	DEST_HOST=0
	DEST_SUBNET=0
	
	if [ x"$NUM_MHOST_NODE" != x"" ] && [ $NUM_MHOST_NODE -gt 0 ] ; then
		((DIRS_PER_SUBNET=$NUM_DATADIR/$NUM_MHOST_NODE))
	else
		DIRS_PER_SUBNET=$NUM_DATADIR
	fi
	
	((MAX_SUBNET=$NUM_DATADIR/$DIRS_PER_SUBNET))
	((MAX_HOST=${#QE_PRIMARY_ARRAY[@]}/$NUM_DATADIR))
	
	SEGS_PROCESSED=0
	SEGS_PROCESSED_HOST=0


	# The following is heavily dependent on sort order of primary array.  This sort
	# order will be affected by hostnames so something non-standard will cause 
	# strange behaviour.  This isn't new (just recording this fact for future generations) 
	# and can be worked around with a mapping file to gpinitsystem (-I option).
	# The right way to do this would require us to connect to remote hosts, determine 
	# what subnet we are on for that hostname and then build the array that way.  We *will*
	# do this once this is in python (or anything other than BASH)
	LOG_MSG "[INFO]:-Building spread mirror array type $MULTI_TXT, please wait..." 1
	for QE_LINE in ${QE_PRIMARY_ARRAY[@]}
	do
		if [ $DEBUG_LEVEL -eq 0 ] && [ x"" != x"$VERBOSE" ];then $NOLINE_ECHO ".\c";fi

		if [ $(($SEGS_PROCESSED%$NUM_DATADIR)) -eq 0 ] ; then
			# A new host group is starting
			if [ $SEGS_PROCESSED -ne 0 ] ; then ((CURRENT_HOST=$CURRENT_HOST+1)); fi
			# Start the mirroring on the next host
			((DEST_HOST=$CURRENT_HOST+1))
			# Always subnet "0" to start
			CURRENT_SUBNET=0
			DEST_SUBNET=1
			# Make sure we loop back when needed
			if [ $DEST_HOST -ge $MAX_HOST ] ; then DEST_HOST=0; fi
			SEGS_PROCESSED_HOST=0
		else
			# Continue with current host
			# move dest host to the next one (This is spread mirroring)
			((DEST_HOST=$DEST_HOST+1))
			# Make sure we look back when needed
			if [ $DEST_HOST -ge $MAX_HOST ] ; then DEST_HOST=0; fi
			# Get what subnet we are on, we may have moved to next
			((CURRENT_SUBNET=($SEGS_PROCESSED_HOST+1)/$DIRS_PER_SUBNET))
			((DEST_SUBNET=$CURRENT_SUBNET+1))
			# Handle looping over
			if [ $DEST_SUBNET -ge $MAX_SUBNET ] ; then DEST_SUBNET=0; fi
			# Increment the number of segments we've processed for this host
			((SEGS_PROCESSED_HOST=$SEGS_PROCESSED_HOST+1))
		fi			
        
        # Handle the case where it's a single hostname (thus a single subnet)
		# This case will mainly be for QA testing
		if [ $NUM_DATADIR -eq $DIRS_PER_SUBNET ] ; then DEST_SUBNET=0; fi
		
		# Handle possible loop
		if [ $DEST_SUBNET -ge $MAX_SUBNET ] ; then DEST_SUBNET=0; fi

		# Calculate the index based on host and subnet number
		((PRIM_SEG_INDEX=($DEST_HOST*$NUM_DATADIR)+($DEST_SUBNET*$DIRS_PER_SUBNET)))

		QE_M_NAME=`$ECHO ${QE_PRIMARY_ARRAY[$PRIM_SEG_INDEX]}|$AWK -F"~" '{print $1}'`
		GP_M_DIR=${MIRROR_DATA_DIRECTORY[$SEGS_PROCESSED%$NUM_DATADIR]}
		P_PORT=`$ECHO $QE_LINE|$AWK -F"~" '{print $2}'`
		P_REPL_PORT=`$ECHO $QE_LINE|$AWK -F"~" '{print $6}'`
		((GP_M_PORT=$P_PORT+$MIRROR_OFFSET))
		((M_REPL_PORT=$P_REPL_PORT+$MIRROR_REPLICATION_PORT_OFFSET))
		M_CONTENT=`$ECHO $QE_LINE|$AWK -F"~" '{print $5}'`
		M_SEG=`$ECHO $QE_LINE|$AWK -F"~" '{print $3}'|$AWK -F"/" '{print $NF}'`
		QE_MIRROR_ARRAY=(${QE_MIRROR_ARRAY[@]} ${QE_M_NAME}~${GP_M_PORT}~${GP_M_DIR}/${M_SEG}~${DBID_COUNT}~${M_CONTENT}~${M_REPL_PORT})
		POSTGRES_PORT_CHK $GP_M_PORT $QE_M_NAME
		((DBID_COUNT=$DBID_COUNT+1))
		((SEGS_PROCESSED=$SEGS_PROCESSED+1))
	done
	if [ $DEBUG_LEVEL -eq 0 ] && [ x"" != x"$VERBOSE" ];then $ECHO;fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_MAX_LEN () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	LOG_MSG "[INFO]:-Formatting output, please wait..." 1
	GET_MASTER_PORT $MASTER_DATA_DIRECTORY
	LEN_ARRAY=(`$EXPORT_LIB_PATH;env PGOPTIONS="-c gp_session_role=utility" $PSQL -F" " -A -t -q -p  $MASTER_PORT -d "$DEFAULTDB" -c"select max(length(hostname))+1, max(length(port))+1, max(length(datadir))+1, max(length(content))+1, max(length(dbid)) from $CONFIG_TABLE a where content<>-1;"`)
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

CREATE_GROUP_MIRROR_ARRAY () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	LOG_MSG "[INFO]:-Building group mirror array type $MULTI_TXT, please wait..." 1
	PRI_HOST_COUNT=`$ECHO ${QE_PRIMARY_ARRAY[@]}|$TR ' ' '\n'|$AWK -F"~" '{print $1}'|$SORT -u|$WC -l`
	if [ $MULTI_HOME -eq 1 ] && [ $REMOTE_HOST_COUNT -eq 1 ];then
		PRI_HOST_COUNT=1
	fi

	if [ x"$NUM_MHOST_NODE" != x"" ] && [ $NUM_MHOST_NODE -gt 0 ] ; then
		((DIRS_PER_SUBNET=$NUM_DATADIR/$NUM_MHOST_NODE))
	else
		DIRS_PER_SUBNET=$NUM_DATADIR
	fi
	((MAX_SUBNET=$NUM_DATADIR/$DIRS_PER_SUBNET))
	((MAX_HOST=${#QE_PRIMARY_ARRAY[@]}/$NUM_DATADIR))

	# Current host we are working on
	CURRENT_HOST=0
	
	# Destination host and subnet
	DEST_HOST=0
	DEST_SUBNET=0

	PRIMARY_ARRAY_LENGTH=${#QE_PRIMARY_ARRAY[@]}
	PRIMARY_INDEX=0

	for QE_LINE in ${QE_PRIMARY_ARRAY[@]}
	do
		if [ $(($PRIMARY_INDEX%$NUM_DATADIR)) -eq 0 ] ; then
			if [ $PRIMARY_INDEX -ne 0 ] ; then ((CURRENT_HOST=$CURRENT_HOST+1)); fi
			((DEST_HOST=$CURRENT_HOST+1))
			if [ $DEST_HOST -ge $MAX_HOST ] ; then DEST_HOST=0; fi
			DEST_SUBNET=1
		else
			if [ $(($PRIMARY_INDEX%$DIRS_PER_SUBNET)) -eq 0 ] ; then
				((DEST_SUBNET=$DEST_SUBNET+1))
			fi	
		fi

		# Handle possible loop
		if [ $DEST_SUBNET -ge $MAX_SUBNET ] ; then DEST_SUBNET=0; fi		

		((MIRROR_INDEX=($DEST_HOST*$NUM_DATADIR)+($DEST_SUBNET*$DIRS_PER_SUBNET)))

		if [ $DEBUG_LEVEL -eq 0 ] && [ x"" != x"$VERBOSE" ];then $NOLINE_ECHO ".\c";fi

		QE_M_NAME=`$ECHO ${QE_PRIMARY_ARRAY[$MIRROR_INDEX]}|$AWK -F"~" '{print $1}'`
		GP_M_DIR=${MIRROR_DATA_DIRECTORY[$PRIMARY_INDEX%$NUM_DATADIR]}/`$ECHO $QE_LINE|$AWK -F"~" '{print $3}'|$AWK -F"/" '{print $NF}'`

		M_CONTENT=`$ECHO $QE_LINE|$AWK -F"~" '{print $5}'`
		P_PORT=`$ECHO $QE_LINE|$AWK -F"~" '{print $2}'`
		P_REPL_PORT=`$ECHO $QE_LINE|$AWK -F"~" '{print $6}'`
		GP_M_PORT=$(($P_PORT+$MIRROR_OFFSET))
		M_REPL_PORT=$(($P_REPL_PORT+$MIRROR_REPLICATION_PORT_OFFSET))
		
		QE_MIRROR_ARRAY=(${QE_MIRROR_ARRAY[@]} ${QE_M_NAME}~${GP_M_PORT}~${GP_M_DIR}~${DBID_COUNT}~${M_CONTENT}~${M_REPL_PORT})
		POSTGRES_PORT_CHK $GP_M_PORT $QE_M_NAME

		DBID_COUNT=$(($DBID_COUNT+1))
	    PRIMARY_INDEX=$((PRIMARY_INDEX+1))
	done
	if [ $DEBUG_LEVEL -eq 0 ] && [ x"" != x"$VERBOSE" ];then $ECHO;fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_REPLY () {
	$ECHO "$1 Yy/Nn>"
	read REPLY
	if [ -z $REPLY ]; then
		LOG_MSG "[WARN]:-User abort requested, Script Exits!" 1
		exit 1
	fi
	if [ $REPLY != Y ] && [ $REPLY != y ]; then
		LOG_MSG "[WARN]:-User abort requested, Script Exits!" 1
		exit 1
	fi
} 
	
CHK_MULTI_HOME () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	GET_QE_DETAILS
	MULTI_ARRAY=()
	J=0
	if [ x"" == x"$1" ];then
		#Select two hosts to test as we do not want to do the whole array
		LOG_MSG "[INFO]:-Obtaining GPDB array type, [Brief], please wait..." 1
		while [ $J -lt 2 ]
		do
			QE_HOST=`$ECHO ${QE_ARRAY[$J]}|$AWK -F"|" '{print $1}'`
			REMOTE_HOSTNAME=`$TRUSTED_SHELL $QE_HOST "$HOSTNAME"`
			MULTI_ARRAY=(${MULTI_ARRAY[@]} ${QE_HOST}:$REMOTE_HOSTNAME)
			((J=$J+1))
		done
	else
		LOG_MSG "[INFO]:-Obtaining GPDB array type, [Full], please wait..." 1
		for QE_LINE in ${QE_ARRAY[@]}
		do
			QE_HOST=`$ECHO $QE_LINE|$AWK -F"|" '{print $1}'`
			REMOTE_HOSTNAME=`$TRUSTED_SHELL $QE_HOST "$HOSTNAME"`
			MULTI_ARRAY=(${MULTI_ARRAY[@]} ${QE_HOST}:$REMOTE_HOSTNAME)
		done
	fi
	SEG_HOST_COUNT=`$ECHO ${MULTI_ARRAY[@]}|$TR ' ' '\n'|$AWK -F"~" '{print $1}'|$SORT -u|wc -l`
	REMOTE_HOST_COUNT=`$ECHO ${MULTI_ARRAY[@]}|$TR ' ' '\n'|$AWK -F"~" '{print $2}'|$SORT -u|wc -l`
	if [ $SEG_HOST_COUNT -eq $REMOTE_HOST_COUNT ];then
		LOG_MSG "[INFO]:-Non multi-home configuration"
		MULTI_HOME=0
		MULTI_TXT="Standard"
	else
		LOG_MSG "[INFO]:-Multi-home configuration"
		MULTI_HOME=1
		MULTI_TXT="Multi-home"
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_DISTRIB_COLS () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	if [ $# -ne 3 ];then
		ERROR_EXIT "[FATAL]:-Incorrect # parameters supplied, got $# expected 3" 2;fi
	T_NAME=$1
	if [ `$ECHO $1|$GREP -c "\."` -eq 1 ];then
		T_NAME=`$ECHO $1|$CUT -d"." -f2`
		S_NAME=`$ECHO $1|$CUT -d"." -f1`
		DISTRIB_COLS=`${EXPORT_LIB_PATH};$PSQL -p $2 -d "$3" -A -t -c"select attrnums from  $DISTRIB_TABLE  where localoid in (select oid from pg_class where relname='${T_NAME}' and relnamespace in (select oid from pg_namespace where nspname='${S_NAME}'));"`
		if [ x"" == x"$DISTRIB_COLS" ];then
			DISTRIB_COLS='';fi
	else
		LOG_MSG "[WARN]:-Incorrect table name sent to function, should be schema.tablename" 1
		ERROR_EXIT "[FATAL]:-Error in $FUNCNAME parameters" 2
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_TABLE_TYPE () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	if [ $# -ne 3 ];then
		ERROR_EXIT "[FATAL]:-Incorrect # parameters supplied, got $# expected 3" 2;fi
	T_NAME=$1
	if [ `$ECHO $1|$GREP -c "\."` -eq 1 ];then
		T_NAME=`$ECHO $1|$CUT -d"." -f2`
		S_NAME=`$ECHO $1|$CUT -d"." -f1`
		TAB_TYPE=`${EXPORT_LIB_PATH};$PSQL -p $2 -d "$3" -A -t -c"select 1 from pg_depend where objid in (select oid from pg_class where relname='${T_NAME}' and relnamespace in (select oid from pg_namespace where nspname='${S_NAME}')) and refobjid not in (select relnamespace from pg_class where relname='${T_NAME}' and relnamespace in (select oid from pg_namespace where nspname='${S_NAME}'));"|$WC -l`
		if [ $TAB_TYPE -eq 1 ];then
			TAB_TYPE_TXT="Child table"
			TAB_PARENT_NAME=(`${EXPORT_LIB_PATH};$PSQL -p $2 -d "$3" -R" " -A -t -c"select relname from pg_class where oid in (select refobjid from pg_depend  where objid in (select oid from pg_class where relname='${T_NAME}' and relnamespace in (select oid from pg_namespace where nspname='${S_NAME}')) and refobjid not in (select relnamespace from pg_class where relname='${T_NAME}' and relnamespace in (select oid from pg_namespace where nspname='${S_NAME}')));"`)
		else
			TAB_TYPE_TXT="Not child table"
		fi
	else
		LOG_MSG "[WARN]:-Incorrect table name sent to function, should be schema.tablename" 1
		ERROR_EXIT "[FATAL]:-Error in $FUNCNAME parameters" 2
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}
	
CHK_FILE () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		FILENAME=$1
		FILE_HOST=$2
		if [ x"" == x"$FILE_HOST" ];then
			LOG_MSG "[INFO]:-Checking file $FILENAME"
			if [ ! -s $FILENAME ] || [ ! -r $FILENAME ]
					then
					EXISTS=1
			else
					EXISTS=0
			fi
		else
			EXISTS=`$TRUSTED_SHELL $FILE_HOST "if [ ! -s $FILENAME ] || [ ! -r $FILENAME ];then $ECHO 1;else $ECHO 0;fi"`
			RETVAL=$?
			if [ $RETVAL -ne 0 ];then
				LOG_MSG "[WARN]:-Failed to obtain details of $FILENAME on $FILE_HOST"
				EXIT_STATUS=1
				EXISTS=1
			fi
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}
CHK_DIR () {
		# this function might be called very early, before logfiles are initialized
		if [ x"" == x"$3" ];then
			LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		fi
		DIR_NAME=$1
		DIR_HOST=$2
		if [ x"" == x"$DIR_HOST" ];then
			EXISTS=`if [ -d $DIR_NAME ];then $ECHO 0;else $ECHO 1;fi`
		else
			EXISTS=`$TRUSTED_SHELL $DIR_HOST "if [ -d $DIR_NAME ];then $ECHO 0;else $ECHO 1;fi"`
			RETVAL=$?
			if [ $RETVAL -ne 0 ];then
			LOG_MSG "[WARN]:-Failed to obtain details of $DIR_NAME on $DIR_HOST" 1
			EXIT_STATUS=1
			EXISTS=1
			fi
		fi
		if [ x"" == x"$3" ];then
			LOG_MSG "[INFO]:-End Function $FUNCNAME"
		fi
}

CHK_WRITE_DIR () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	DIR_NAME=$1;shift
	DIR_HOST=$1;shift
	NOEXIT=$1
	LOG_MSG "[INFO]:-Checking write access to $DIR_NAME on $DIR_HOST"
	$TRUSTED_SHELL $DIR_HOST "$TOUCH ${DIR_NAME}/tmp_file_test"
	RETVAL=$?
	if [ $RETVAL -ne 0 ];then
		if [ x"" == x"$NOEXIT" ];then
			ERROR_EXIT "[FATAL]:-Cannot write to $DIR_NAME on $DIR_HOST" 2
		else
			LOG_MSG "[FATAL]:-Cannot write to $DIR_NAME on $DIR_HOST" 1
		fi
	else
		$TRUSTED_SHELL $DIR_HOST "$RM -f ${DIR_NAME}/tmp_file_test"
	LOG_MSG "[INFO]:-Write test passed on $DIR_HOST $DIR_NAME directory"
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}
GET_MASTER_PORT () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		MASTER_DATA_DIRECTORY=$1
		if [ x"" == x"$MASTER_DATA_DIRECTORY" ];then
			ERROR_EXIT "[FATAL]:-MASTER_DATA_DIRECTORY variable not set" 2;fi
		if [ ! -d $MASTER_DATA_DIRECTORY ]; then
				ERROR_EXIT "[FATAL]:-No $MASTER_DATA_DIRECTORY directory" 2
		fi
		if [ -r $MASTER_DATA_DIRECTORY/$PG_CONF ];then
			MASTER_PORT=`$AWK 'split($0,a,"#")>0 && split(a[1],b,"=")>1 {print b[1] " " b[2]}' $MASTER_DATA_DIRECTORY/$PG_CONF | $AWK '$1=="port" {print $2}' | $TAIL -1`
			if [ x"" == x"$MASTER_PORT" ] ; then
                #look for include files
                for INC_FILE in `$AWK '/^[ ]*include /{print $2}' $MASTER_DATA_DIRECTORY/$PG_CONF | $TR -d "'\""` ; do
                    if [[ $INC_FILE == /* ]] ; then
                        GET_MASTER_PORT_RECUR "$INC_FILE" 1
                    else
                        GET_MASTER_PORT_RECUR "$MASTER_DATA_DIRECTORY/$INC_FILE" 1
                    fi
                done
                if [ x"" == x"$MASTER_PORT" ] ; then
			        ERROR_EXIT "[FATAL]:-Failed to obtain master port number from $MASTER_DATA_DIRECTORY/$PG_CONF" 2
                fi
			fi
		else
			ERROR_EXIT "[FATAL]:-Do not have read access to $MASTER_DATA_DIRECTORY/$PG_CONF" 2
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_MASTER_PORT_RECUR () {
    INCLUDED_FILE=$1
    RECUR=$2
    if [ $RECUR -le 10 ] ; then
        MASTER_PORT=`$AWK 'split($0,a,"#")>0 && split(a[1],b,"=")>1 {print b[1] " " b[2]}' $INCLUDED_FILE | $AWK '$1=="port" {print $2}' | $TAIL -1`
        if [ x"" == x"$MASTER_PORT" ] ; then
            #look for include files
            let CURR_DEPTH=$RECUR+1
            for INC_FILE in `$AWK '/^[ ]*include /{print $2}' $INC_FILE | $TR -d "'\""` ; do
                if [[ $INC_FILE == /* ]] ; then
                    GET_MASTER_PORT_RECUR "$INC_FILE" $CURR_DEPTH
                else
                    GET_MASTER_PORT_RECUR "$MASTER_DATA_DIRECTORY/$INC_FILE" $CURR_DEPTH
                fi
                if [ x"" != x"$MASTER_PORT" ] ; then
                    break
                fi
            done
        fi
    else
        ERROR_EXIT "[FATAL]:-Could not open configuration file \"$INCLUDED_FILE\": maximum nesting depth exceeded"
    fi
}

CHK_ON_PASSIVE_STANDBY () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	GPSYNC_COUNT=0
	CHK_DB_QD_SET
	GET_MASTER_PORT $MASTER_DATA_DIRECTORY
	GET_PG_PID_ACTIVE $MASTER_PORT
	if [ -f $MASTER_DATA_DIRECTORY/postmaster.opts ];then
		GPSYNC_COUNT=`$GREP -c gpsync $MASTER_DATA_DIRECTORY/postmaster.opts`
	fi
	if [ $PID -ne 0 ] && [ $GPSYNC_COUNT -ne 0 ];then
		LOG_MSG "[FATAL]:-Cannot run this script on a passive standby instance" 1
		LOG_MSG "[FATAL]:-where there is a conflict with the current value of" 1
		LOG_MSG "[FATAL]:-the MASTER_DATA_DIRECTORY environment variable setting." 1
		ERROR_EXIT "[FATAL]:-Unable to process requested command" 2
	fi
	if [ $GPSYNC_COUNT -ne 0 ];then
		LOG_MSG "[FATAL]:-Cannot run this script on the standby instance" 1
		LOG_MSG "[FATAL]:-Status indicates that standby instance processs not running" 1
		LOG_MSG "[FATAL]:-Check standby process status via gpstate -f on Master instance" 1
		ERROR_EXIT "[FATAL]:-Unable to process requested command" 2
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_CIDRADDR () {
    # MPP-15889
    # assuming argument is an ip address, return the address
    # with a /32 or /128 cidr suffix based on whether or not the
    # address contains a :

    if [ `echo $1 | grep -c :` -gt 0 ]; then
	echo $1/128
    else
	echo $1/32
    fi
}

BUILD_MASTER_PG_HBA_FILE () {
        LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	if [ $# -eq 0 ];then ERROR_EXIT "[FATAL]:-Passed zero parameters, expected at least 1" 2;fi
	GP_DIR=$1
        LOG_MSG "[INFO]:-Clearing values in Master $PG_HBA"
        $GREP "^#" ${GP_DIR}/$PG_HBA > $TMP_PG_HBA
        $MV $TMP_PG_HBA ${GP_DIR}/$PG_HBA
        LOG_MSG "[INFO]:-Setting local access"
        $ECHO "local    all         $USER_NAME         $PG_METHOD" >> ${GP_DIR}/$PG_HBA
        #$ECHO "local    all         all                $PG_METHOD" >> ${GP_DIR}/$PG_HBA
        LOG_MSG "[INFO]:-Setting local host access"
        $ECHO "host     all         $USER_NAME         127.0.0.1/28    trust" >> ${GP_DIR}/$PG_HBA
        for ADDR in "${MASTER_IP_ADDRESS_ALL[@]}"
        do
        	# MPP-15889
        	CIDRADDR=$(GET_CIDRADDR $ADDR)
        	$ECHO "host     all         $USER_NAME         $CIDRADDR       trust" >> ${GP_DIR}/$PG_HBA
        done
        for ADDR in "${STANDBY_IP_ADDRESS_ALL[@]}"
        do
        	# MPP-15889
        	CIDRADDR=$(GET_CIDRADDR $ADDR)
        	$ECHO "host     all         $USER_NAME         $CIDRADDR       trust" >> ${GP_DIR}/$PG_HBA
        done

        # Add all local IPV6 addresses
        for ADDR in "${MASTER_IPV6_LOCAL_ADDRESS_ALL[@]}"
        do
        	# MPP-15889
        	CIDRADDR=$(GET_CIDRADDR $ADDR)
        	$ECHO "host     all         $USER_NAME         $CIDRADDR       trust" >> ${GP_DIR}/$PG_HBA
        done

        # Add replication config
        $ECHO "local    replication $USER_NAME         $PG_METHOD" >> ${GP_DIR}/$PG_HBA
        $ECHO "host     replication $USER_NAME         samenet       trust" >> ${GP_DIR}/$PG_HBA
        LOG_MSG "[INFO]:-Complete Master $PG_HBA configuration"
        LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

BUILD_GPSSH_CONF () {
        LOG_MSG "[INFO]:-Start Function $FUNCNAME"
        if [ $# -eq 0 ];then ERROR_EXIT "[FATAL]:-Passed zero parameters, expected at least 1" 2;fi
        GP_DIR=$1
        $CAT <<_EOF_ >> $GP_DIR/gpssh.conf
[gpssh]
# delaybeforesend specifies the time in seconds to wait at the
# beginning of an ssh interaction before doing anything.
# Increasing this value can have a big runtime impact at the
# beginning of gpssh.
delaybeforesend = 0.05

# prompt_validation_timeout specifies a timeout multiplier that
# will be used in validating the ssh prompt. Increasing this
# value will have a small runtime impact at the beginning of
# gpssh.
prompt_validation_timeout = 1.0
_EOF_
        LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

BUILD_PERFMON() {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	GP_DIR=$1
	$MKDIR -p $GP_DIR/gpperfmon/conf $GP_DIR/gpperfmon/logs $GP_DIR/gpperfmon/data
	$CAT <<_EOF_ >> $GP_DIR/gpperfmon/conf/gpperfmon.conf
[GPMMON]
# quantum specifies the time in seconds between updates from 
# performance monitor agents on all segments. Valid values 
# are 10, 15, 20, 30, or 60
quantum = 15

# min_query_time specifies the minimum query run time 
# in seconds for statistics collection. The monitor logs all 
# queries that run longer than this value in the queries_history 
# table. For queries with shorter run times, no historical 
# data is collected.
min_query_time = 20

# Specifies the minimum iterator run time in seconds for 
# statistics collection. The monitor logs all iterators that 
# run longer than this value in the iterators_history table. 
# For iterators with shorter run times, no data is collected. 
# Minimum value is 10 seconds.
min_detailed_query_time = 60

# This should be a percentage between 0 and 100 and should be
# less than the error_disk_space_percentage.  If a filesystem’s 
# disk space used percentage equals or exceeds this value a 
# warning will be logged and a warning email/snmp trap may be 
# sent.  If this configuration is set to 0 or not specified, no 
# warnings are sent.
#warning_disk_space_percentage = 80

# This should be a percentage between 0 and 100 and should be 
# greater than the warning_disk_space_percentage. If a 
# filesystem’s disk space used percentage equals or exceeds 
# this value an error will be logged and a error email/snmp 
# trap may be sent.  If this configuration is set to 0 or not 
# specified, no errors are sent.
#error_disk_space_percentage = 90

#This is the interval in minutes that limits the number of 
#error/warning messages that are sent. The minimum value for 
#this configuration is 1.  Setting this to 0 or not specifying 
#this configuration results in it getting set to the minimum.  
disk_space_interval = 60

#This is the maximum number of error/warning messages that 
#will be sent in the disk_space_interval.  The maximum value
#for this configuration is 50.  The minimum value for this 
#configuration is 1.  Setting this configuration to greater 
#than 50 or not specifying this configuration results in it 
#getting set to the maximum.
max_disk_space_messages_per_interval = 10

# The number of partitions for statistics data in month 
# will be retained. Older partitions will be dropped.
#partition_age = 6


log_location = $GP_DIR/gpperfmon/logs
_EOF_
}

CHK_DB_RUNNING () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		if [ $# -eq 1 ];then
			CHK_DISPATCH_ACCESS=1
		else
			CHK_DISPATCH_ACCESS=0
		fi
		if [ ! -d $MASTER_DATA_DIRECTORY ]; then
				ERROR_EXIT "[FATAL]:-No Master $MASTER_DATA_DIRECTORY directory" 2
		fi
		if [ ! -f $MASTER_DATA_DIRECTORY/$PG_PID ]; then
			LOG_MSG "[FATAL]:-No $MASTER_DATA_DIRECTORY/$PG_PID file" 1
			ERROR_EXIT "[FATAL]:-Run gpstart to start the Greenplum database." 2
		fi		
		GET_MASTER_PORT $MASTER_DATA_DIRECTORY
		export $EXPORT_LIB_PATH;env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"SELECT d.datname as \"Name\",
       r.rolname as \"Owner\",
       pg_catalog.pg_encoding_to_char(d.encoding) as \"Encoding\"
FROM pg_catalog.pg_database d
  JOIN pg_catalog.pg_authid r ON d.datdba = r.oid
ORDER BY 1;" >> $LOG_FILE 2>&1
		if [ $? -ne 0 ];then
			LOG_MSG "[FATAL]:-Have a postmaster.pid file for master instance on port $MASTER_PORT" 1
			LOG_MSG "[FATAL]:-However, error reported on test psql access to master instance" 1
			LOG_MSG "[INFO]:-Check ps output for a postmaster process on the above port" 1
			LOG_MSG "[INFO]:-Check the master postgres logfile for errors and also the utility log file" 1
			ERROR_EXIT "[FATAL]:-Unable to continue" 2
		fi
		if [ $CHK_DISPATCH_ACCESS -eq 1 ];then
			#Check if in admin mode
			export $EXPORT_LIB_PATH;$PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"\l" >> $LOG_FILE 2>&1
			if [ $? -ne 0 ];then
				LOG_MSG "[WARN]:-Can access the Master instance in admin mode, but dispatch access failed" 1
				LOG_MSG "[INFO]:-This could mean that the Master instance is in admin mode only" 1
				LOG_MSG "[INFO]:-Run gpstop -m to shutdown Master instance from admin mode, and restart" 1
				LOG_MSG "[INFO]:-the Greenplum database using gpstart" 1
				EXIT_STATUS=1
			else
				EXIT_STATUS=0
			fi
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_QD_DB_NAME () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		GET_MASTER_PORT $MASTER_DATA_DIRECTORY
		CHK_DB_RUNNING	
		#Check if we have PGDATABASE environment variable set, if so see if that database exists
		if [ x"" != x"$PGDATABASE" ];then
			LOG_MSG "[INFO]:-PGDATABASE set, checking for this database"
			QD_DBNAME_THERE=`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"select 1 from pg_database where datname='${PGDATABASE}';"|$WC -l`
			ERROR_CHK $? "check for $PGDATABASE" 2
			if [ $QD_DBNAME_THERE -eq 1 ];then
				QD_DBNAME=$PGDATABASE
			else
				QD_DBNAME_THERE=""
			fi
		fi
		if [ x"" = x"$QD_DBNAME_THERE" ];then
			LOG_MSG "[INFO]:-Checking for a non-system database"
			QD_DBNAME=`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"$GPLISTDATABASEQTY " 2>/dev/null|$GREP -v postgres|$GREP -v template0|$HEAD -1|$CUT -d"|" -f1`
			ERROR_CHK $? "obtain database name" 2
		fi
		MASTER_DIR_CHK=`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"select 1 from ${CONFIG_TABLE} a;"|$WC -l`
		if [ $MASTER_DIR_CHK -eq 0 ];then
			ERROR_EXIT "[FATAL]:-MASTER_DATA_DIRECTORY value of $MASTER_DATA_DIRECTORY is incorrect" 2;fi
		if [ x"" == x"$QD_DBNAME" ]; then
				LOG_MSG "[INFO]:-Unable to obtain a non-system database name, setting value to "$DEFAULTDB""
				QD_DBNAME="$DEFAULTDB"
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_STANDBY_COUNT () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	GET_QD_DB_NAME
	STANDBY_COUNT=`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -q -p $MASTER_PORT -d "$QD_DBNAME" -A -t -c"select 1 from $CONFIG_TABLE a where content=-1 and dbid<>1 ;"|$WC -l`
	ERROR_CHK $? "obtain standby master host count" 1
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_STANDBY_DETAILS () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	GET_QD_DB_NAME
	STANDBY_ARRAY=(`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -q -p $MASTER_PORT -d "$QD_DBNAME" -A -t -c"select hostname, datadir, port, mode, status, preferred_role, role from $CONFIG_TABLE a where content=-1 and dbid<>1"`) > /dev/null 2>&1
	RETVAL=$?
	if [ $RETVAL -ne 0 ]; then
		LOG_MSG "[INFO]:-Unable to obtain standby master details, error code $RETVAL returned" 1
		EXIT_STATUS=1
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_TRANS_READ () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	TRANS_STATE=`${EXPORT_LIB_PATH};$PSQL -q -p $MASTER_PORT -d "$QD_DBNAME" -A -t -c"show transaction_read_only;"|$AWK '{print $NF}'`
	ERROR_CHK $? "obtain transaction state" 1
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_QE_DETAILS () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		GET_QD_DB_NAME
		if [ x"" == x"$PGUSER" ];then
		    DBUSER=$USER
		else
		    DBUSER=$PGUSER
		fi
		
		if [ $# -eq 0 ];then
			QE_ARRAY=(`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -q -p $MASTER_PORT -U $DBUSER -d "$QD_DBNAME" -A -t -c"select a.hostname, a.datadir, a.port, b.valid, b.definedprimary from $CONFIG_TABLE a, $GP_PG_VIEW b where a.dbid=b.dbid and a.content<>-1 order by b.dbid;"`) > /dev/null 2>&1
		else
			QE_ARRAY=(`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -q -p $MASTER_PORT -U $DBUSER -d "$QD_DBNAME" -A -t -c"select a.hostname, a.datadir, a.port, b.valid, b.definedprimary from $CONFIG_TABLE a, $GP_PG_VIEW b where a.dbid=b.dbid and a.content<>-1 order by a.port;"`) > /dev/null 2>&1
		fi
		RETVAL=$?
		if [ $RETVAL -ne 0 ]; then
				LOG_MSG "[WARN]:-Unable to obtain segment instance host details from Master db, error code $RETVAL returned" 1
				EXIT_STATUS=1
		fi
		QE_ARRAY_COUNT=${#QE_ARRAY[@]}
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_DB_LIST () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	GET_MASTER_PORT $MASTER_DATA_DIRECTORY
	DB_ARRAY=(`env PGOPTIONS="-c gp_session_role=utility" $PSQL -q -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"select datname from pg_database where datname not in ('template1', 'template0', 'postgres');"`)
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

CHK_MIRRORS_CONFIGURED () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	MIRROR_COUNT=`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"select count(dbid)/count(distinct(content)) from $CONFIG_TABLE a where content<>-1;"`
	ERROR_CHK $? "obtain mirror count from master instance" 2
	LOG_MSG "[INFO]:-Obtained $MIRROR_COUNT as check value"
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_MIRROR_TYPE () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	CHK_MIRRORS_CONFIGURED
	if [ $MIRROR_COUNT -eq 1 ];then
		MIR_TYPE="No Mirror";MIR_TYPE_NUM=0
	else
		SEP_COUNT=`${EXPORT_LIB_PATH};$PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"select count(distinct hostname)/(select count(distinct hostname) from $CONFIG_TABLE a where preferred_role = 'p' and content<>-1) from $CONFIG_TABLE a where content<>-1;"`
		if [ $SEP_COUNT -eq 2 ];then
			SEP_TEXT="[Separate array]"
		else
			SEP_TEXT="[Shared array]"
		fi
		#Get number of primary hosts
		NUM_SEG_HOSTS=`${EXPORT_LIB_PATH};$PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"select count(distinct(hostname)) from $CONFIG_TABLE a where content<>-1;"`
		#Get the primary and mirror hostnames for the first segment instance host
		FIRST_PRI_MIR_ARRAY=(`${EXPORT_LIB_PATH};$PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"select hostname, content from $CONFIG_TABLE a where content>-1 and content<(select max(port)-min(port)+1 from $CONFIG_TABLE a where content<>-1 and preferred_role='p') order by content,dbid;"|$TR '\n' ' '`)
		((SEG_PER_HOST=${#FIRST_PRI_MIR_ARRAY[@]}/2))
		CHK_MULTI_HOME
		if [ $MULTI_HOME -eq 0 ];then
			if [ `$ECHO ${FIRST_PRI_MIR_ARRAY[@]}|$TR ' ' '\n'|$AWK -F"|" '{print $1}'|$SORT -u|$WC -l` -eq 2 ] || [ $NUM_SEG_HOSTS -eq 1 ];then
				MIR_TYPE="Group [Single-home] $SEP_TEXT";MIR_TYPE_NUM=1
			else
				MIR_TYPE="Spread [Single-home] $SEP_TEXT";MIR_TYPE_NUM=2
			fi
		else
			J=0
			MIR_H_NAME_ARRAY=()
			while [ $J -lt $SEG_PER_HOST ]
			do
				MIR_HOST=`$ECHO ${FIRST_PRI_MIR_ARRAY[@]}|$TR ' ' '\n'|$GREP "|${J}$"|$AWK -F"|" '{print $1}'|$TAIL -1`
				PRI_HOST=`$ECHO ${FIRST_PRI_MIR_ARRAY[@]}|$TR ' ' '\n'|$GREP "|${J}$"|$AWK -F"|" '{print $1}'|$HEAD -1`
				#MIR_H_HOST=`$ECHO ${MULTI_ARRAY[@]}|$TR ' ' '\n'|$GREP "${MIR_HOST}~"|$SORT -u|$AWK -F"~" '{print $2}'`
				MIR_H_CONFIG_HOST=`${EXPORT_LIB_PATH};$PSQL -p $MASTER_PORT -d "$DEFAULTDB" -A -t -c"select hostname from $CONFIG_TABLE a where hostname<>'${PRI_HOST}' and content=${J};"`
				MIR_H_HOST=`$TRUSTED_SHELL $MIR_H_CONFIG_HOST "$HOSTNAME"|$AWK '{print $1}'`
				#MIR_HOST_VAL=`$TRUSTED_SHELL $MIR_HOST "$HOSTNAME"`
				MIR_H_NAME_ARRAY=(${MIR_H_NAME_ARRAY[@]} $MIR_H_HOST)
				#MIR_H_NAME_ARRAY=(${MIR_H_NAME_ARRAY[@]} $MIR_HOST_VAL)
				((J=$J+1))
			done
			UNIQ_COUNT=`$ECHO ${MIR_H_NAME_ARRAY[@]}|$TR ' ' '\n'|$SORT -u|$WC -l`
			if [ $UNIQ_COUNT -eq 1 ];then
				MIR_TYPE="Group [Multi-home] $SEP_TEXT";MIR_TYPE_NUM=3
			else
				MIR_TYPE="Spread [Multi-home] $SEP_TEXT";MIR_TYPE_NUM=4;fi
		
		fi	
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_QE_PRIMARY_DETAILS () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		GET_QD_DB_NAME
		QE_PRIMARY_ARRAY=(`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -q -p $MASTER_PORT -d "$QD_DBNAME" -A -t -c"select a.hostname, a.datadir, a.port, b.valid, b.isprimary from $CONFIG_TABLE a, $GP_PG_VIEW b where a.dbid=b.dbid and a.content<>-1 and  a.preferred_role = 'p' order by a.content;" 2>/dev/null`) > /dev/null 2>&1

		RETVAL=$?
		if [ $RETVAL -ne 0 ]; then
				LOG_MSG "[WARN]:-Unable to obtain segment instance primary host details from Master db, error code $RETVAL returned" 1
				EXIT_STATUS=1
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_QE_MIRROR_DETAILS () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		GET_QD_DB_NAME
		QE_MIRROR_ARRAY=(`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -q -p $MASTER_PORT -d "$QD_DBNAME" -A -t -c"select a.hostname, a.datadir, a.port, b.valid, b.isprimary from $CONFIG_TABLE a, $GP_PG_VIEW b where a.dbid=b.dbid and a.content<>-1 and a.preferred_role='m' order by a.content;" 2>/dev/null`) > /dev/null 2>&1

		RETVAL=$?
		if [ $RETVAL -ne 0 ]; then
				LOG_MSG "[WARN]:-Unable to obtain segment instance mirror host details from Master db, error code $RETVAL returned" 1
				EXIT_STATUS=1
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

GET_PG_PID_ACTIVE () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		# Expects port number and hostname for remote checking
		PORT=$1;shift
		HOST=$1
		PG_LOCK_FILE="/tmp/.s.PGSQL.${PORT}.lock"
		PG_LOCK_NETSTAT=""
		if [ x"" == x"$HOST" ];then
			#See if we have a netstat entry for this local host
			PORT_ARRAY=(`$NETSTAT -an 2>/dev/null |$GREP ".s.PGSQL.${PORT}"|$AWK '{print $NF}'|$AWK -F"." '{print $NF}'|$SORT -u`)
			for P_CHK in ${PORT_ARRAY[@]}
			do
				if [ $P_CHK -eq $PORT ];then  PG_LOCK_NETSTAT=$PORT;fi
			done
			#PG_LOCK_NETSTAT=`$NETSTAT -an 2>/dev/null |$GREP ".s.PGSQL.${PORT}"|$AWK '{print $NF}'|$HEAD -1`
			#See if we have a lock file in /tmp
			if [ -f ${PG_LOCK_FILE} ];then
				PG_LOCK_TMP=1
			else
				PG_LOCK_TMP=0
			fi
			if [ x"" == x"$PG_LOCK_NETSTAT" ] && [ $PG_LOCK_TMP -eq 0 ];then
				PID=0
				LOG_MSG "[INFO]:-No socket connection or lock file in /tmp found for port=${PORT}"
			else
				#Now check the failure combinations
				if [ $PG_LOCK_TMP -eq 0 ] && [ x"" != x"$PG_LOCK_NETSTAT" ];then
				#Have a process but no lock file
					LOG_MSG "[WARN]:-No lock file $PG_LOCK_FILE but process running on port $PORT" 1
					PID=1
					EXIT_STATUS=1
				fi
				if [ $PG_LOCK_TMP -eq 1 ] && [ x"" == x"$PG_LOCK_NETSTAT" ];then
				#Have a lock file but no process
					if [ -r ${PG_LOCK_FILE} ];then
						PID=`$CAT ${PG_LOCK_FILE}|$HEAD -1|$AWK '{print $1}'`
					else
						LOG_MSG "[WARN]:-Unable to access ${PG_LOCK_FILE}" 1
						PID=1
					fi
					LOG_MSG "[WARN]:-Have lock file $PG_LOCK_FILE but no process running on port $PORT" 1
					EXIT_STATUS=1
				fi
				if [ $PG_LOCK_TMP -eq 1 ] && [ x"" != x"$PG_LOCK_NETSTAT" ];then
				#Have both a lock file and a netstat process
					if [ -r ${PG_LOCK_FILE} ];then
						PID=`$CAT ${PG_LOCK_FILE}|$HEAD -1|$AWK '{print $1}'`
					else
						LOG_MSG "[WARN]:-Unable to access ${PG_LOCK_FILE}" 1
						PID=1
						EXIT_STATUS=1
					fi
					LOG_MSG "[INFO]:-Have lock file $PG_LOCK_FILE and a process running on port $PORT"
				fi
			fi
		else
			PING_HOST $HOST 1
			if [ $RETVAL -ne 0 ];then
				PID=0
				EXIT_STATUS=1
			else	
				PORT_ARRAY=(`$TRUSTED_SHELL $HOST "$NETSTAT -an 2>/dev/null |$GREP ".s.PGSQL.${PORT}" 2>/dev/null"|$AWK '{print $NF}'|$AWK -F"." '{print $NF}'|$SORT -u`)
				for P_CHK in ${PORT_ARRAY[@]}
				do
					if [ $P_CHK -eq $PORT ];then  PG_LOCK_NETSTAT=$PORT;fi
				done
				#PG_LOCK_NETSTAT=`$TRUSTED_SHELL $HOST "$NETSTAT -an 2>/dev/null |$GREP ".s.PGSQL.${PORT}" 2>/dev/null"|$AWK '{print $NF}'|$HEAD -1`
				PG_LOCK_TMP=`$TRUSTED_SHELL $HOST "$LS ${PG_LOCK_FILE} 2>/dev/null"|$WC -l`
				if [ x"" == x"$PG_LOCK_NETSTAT" ] && [ $PG_LOCK_TMP -eq 0 ];then
					PID=0
					LOG_MSG "[INFO]:-No socket connection or lock file $PG_LOCK_FILE found for port=${PORT}"
				else
				#Now check the failure combinations
					if [ $PG_LOCK_TMP -eq 0 ] && [ x"" != x"$PG_LOCK_NETSTAT" ];then
					#Have a process but no lock file
						LOG_MSG "[WARN]:-No lock file $PG_LOCK_FILE but process running on port $PORT on $HOST" 1
						PID=1
						EXIT_STATUS=1
					fi
					if [ $PG_LOCK_TMP -eq 1 ] && [ x"" == x"$PG_LOCK_NETSTAT" ];then
					#Have a lock file but no process
						CAN_READ=`$TRUSTED_SHELL $HOST "if [ -r ${PG_LOCK_FILE} ];then echo 1;else echo 0;fi"`
						if [ $CAN_READ -eq 1 ];then
							PID=`$TRUSTED_SHELL $HOST "$CAT ${PG_LOCK_FILE}|$HEAD -1 2>/dev/null"|$AWK '{print $1}'`
						else
							LOG_MSG "[WARN]:-Unable to access ${PG_LOCK_FILE} on $HOST" 1
						fi
						LOG_MSG "[WARN]:-Have lock file $PG_LOCK_FILE but no process running on port $PORT on $HOST" 1
						PID=1
						EXIT_STATUS=1
					fi
					if [ $PG_LOCK_TMP -eq 1 ] && [ x"" != x"$PG_LOCK_NETSTAT" ];then
					#Have both a lock file and a netstat process
						CAN_READ=`$TRUSTED_SHELL $HOST "if [ -r ${PG_LOCK_FILE} ];then echo 1;else echo 0;fi"`
						if [ $CAN_READ -eq 1 ];then
							PID=`$TRUSTED_SHELL $HOST "$CAT ${PG_LOCK_FILE}|$HEAD -1 2>/dev/null"|$AWK '{print $1}'`
						else
							LOG_MSG "[WARN]:-Unable to access ${PG_LOCK_FILE} on $HOST" 1	
							EXIT_STATUS=1
						fi
						LOG_MSG "[INFO]:-Have lock file $PG_LOCK_FILE and a process running on port $PORT on $HOST"
					fi				
				fi
			fi
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

RUN_COMMAND () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		COMMAND="$1"
		LOG_MSG "[INFO]:-Commencing local $COMMAND"
		$COMMAND >> $LOG_FILE 2>&1
		RETVAL=$?
		if [ $RETVAL -ne 0 ]; then
				ERROR_EXIT "[FATAL]:- Command $COMMAND failed with error status $RETVAL, see log file $LOG_FILE for more detail" 2
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

RUN_COMMAND_REMOTE () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		HOST=$1
		COMMAND=$2
		LOG_MSG "[INFO]:-Commencing remote $TRUSTED_SHELL $HOST $COMMAND"
		$TRUSTED_SHELL $HOST $COMMAND >> $LOG_FILE 2>&1
		RETVAL=$?
		if [ $RETVAL -ne 0 ]; then
			LOG_MSG "[FATAL]:- Command $COMMAND on $HOST failed with error status $RETVAL" 2
		else
			LOG_MSG "[INFO]:-Completed $TRUSTED_SHELL $HOST $COMMAND"
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
		return $RETVAL
}

BACKOUT_COMMAND () {
		LOG_MSG "[INFO]:-Start Function $FUNCNAME"
		COMMAND=$1
		if [ ! -f $BACKOUT_FILE ]; then
				$ECHO $COMMAND > $BACKOUT_FILE
		else
				$CAT $BACKOUT_FILE > /tmp/backout_file.tmp.$$
				$ECHO $COMMAND > $BACKOUT_FILE
				$CAT /tmp/backout_file.tmp.$$ >> $BACKOUT_FILE
				$RM -f /tmp/backout_file.tmp.$$
		fi
		LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

PING_HOST () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	TARGET_HOST=$1;shift
	PING_EXIT=$1
	if [ x"" == x"$PING_EXIT" ];then PING_EXIT=0;fi
	case $OS_TYPE in
		darwin )
			$PING $PING_TIME $TARGET_HOST > /dev/null 2>&1 || $PING6 $PING_TIME $TARGET_HOST > /dev/null 2>&1 
                        ;;
		linux )
			$PING $TARGET_HOST $PING_TIME > /dev/null 2>&1 || $PING6 $TARGET_HOST $PING_TIME > /dev/null 2>&1 
                        ;;
		* )
			$PING $TARGET_HOST $PING_TIME > /dev/null 2>&1
	esac
	RETVAL=$?
	case $RETVAL in
		0) LOG_MSG "[INFO]:-$TARGET_HOST contact established" 
                   ;;
		1) if [ $PING_EXIT -eq 0 ];then
			ERROR_EXIT "[FATAL]:-Unable to contact $TARGET_HOST" 2
		   else
		        LOG_MSG "[WARN]:-Unable to contact $TARGET_HOST" 1
		   fi 
                   ;;
		2) if [ $PING_EXIT -eq 0 ];then
		 	ERROR_EXIT "[FATAL]:-Unknown host $TARGET_HOST" 2
		   else
			LOG_MSG "[WARN]:-Unknown host $TARGET_HOST" 1
		   fi 
                   ;;
	esac
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
	return $RETVAL
}

STANDBY_CATALOG_UPDATE () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	MASTER_STANDBY_HOSTNAME=$1;shift
	MASTER_STANDBY_DATA_DIRECTORY=$1;shift
	MASTER_STANDBY_PORT=$1
	STANDBY_HOST_COUNT=`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -A -t -d "$QD_DBNAME" -c"select distinct hostname from $CONFIG_TABLE a where hostname='${MASTER_STANDBY_HOSTNAME}';"|$WC -l`
	ERROR_CHK $? "obtain standby host count" 2
    LOG_MSG "[INFO]:-Creating new gp_segment_configuration record for ${MASTER_STANDBY_HOSTNAME}"
    MAX_DB_ID=`${EXPORT_LIB_PATH};env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -A -t -d "$QD_DBNAME" -c"select max(dbid)+1 from $CONFIG_TABLE a;"`
	ERROR_CHK $? "obtain max dbid from gp_configuration" 2
    $EXPORT_LIB_PATH;env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -A -t -d "$QD_DBNAME" -c"insert into gp_segment_configuration (dbid, content, role, preferred_role, mode, status, hostname, address, port) values (${MAX_DB_ID},-1,'m', 'm', 'u', 's', '${MASTER_STANDBY_HOSTNAME}','${MASTER_STANDBY_HOSTNAME}',${MASTER_STANDBY_PORT});"
    $EXPORT_LIB_PATH;env PGOPTIONS="-c gp_session_role=utility" $PSQL -p $MASTER_PORT -A -t -d "$QD_DBNAME" -c"insert into pg_filespace_entry (fsefsoid, fsedbid, fselocation) values (3052, ${MAX_DB_ID},'${MASTER_STANDBY_DATA_DIRECTORY}');"
    ERROR_CHK $? "add standby host gp_configuration record" 2
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

PARALLEL_SETUP () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	PARALLEL_STATUS_FILE=$1
	$TOUCH $PARALLEL_STATUS_FILE
	export PARALLEL_STATUS_FILE=$PARALLEL_STATUS_FILE
	LOG_MSG "[INFO]:-Spawning parallel processes    batch [1], please wait..." 1
	BATCH_COUNT=0
	INST_COUNT=0
	BATCH_DONE=1
	BATCH_TOTAL=0
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

PARALLEL_COUNT () {
        LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	if [ $# -ne 2 ];then ERROR_EXIT "[FATAL]:-Incorrect number of parameters passed to $FUNCNAME" 2;fi
	BATCH_LIMIT=$1
	BATCH_DEFAULT=$2
	((INST_COUNT=$INST_COUNT+1))
	((BATCH_COUNT=$BATCH_COUNT+1))
	((BATCH_TOTAL=$BATCH_TOTAL+1))
	if [ $BATCH_COUNT -eq $BATCH_DEFAULT ] || [ $BATCH_LIMIT -eq $BATCH_TOTAL ];then
		if [ $DEBUG_LEVEL -eq 0 ] && [ x"" != x"$VERBOSE" ];then $ECHO;fi
		PARALLEL_WAIT
		((BATCH_DONE=$BATCH_DONE+1))
		BATCH_COUNT=0
		if [ $BATCH_LIMIT -ne $BATCH_TOTAL ];then
			LOG_MSG "[INFO]:-Spawning parallel processes    batch [$BATCH_DONE], please wait..." 1
		fi
	fi
        LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

PARALLEL_WAIT () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	LOG_MSG "[INFO]:-Waiting for parallel processes batch [$BATCH_DONE], please wait..." 1
	SLEEP_COUNT=0
	while [ `$WC -l $PARALLEL_STATUS_FILE|$AWK '{print $1}'` -ne $INST_COUNT ]
	do
		if [ $DEBUG_LEVEL -eq 0 ] && [ x"" != x"$VERBOSE" ];then $NOLINE_ECHO ".\c";fi
		$SLEEP 1
		((SLEEP_COUNT=$SLEEP_COUNT+1))
		if [ $WAIT_LIMIT -lt $SLEEP_COUNT ];then
			if [ $DEBUG_LEVEL -eq 0 ] && [ x"" != x"$VERBOSE" ];then $NOLINE_ECHO ".\c";fi
			LOG_MSG "[FATAL]:-Failed to process this batch of segments within $WAIT_LIMIT seconds" 1
			LOG_MSG "[INFO]:-Review contents of $LOG_FILE" 1
			ERROR_EXIT "[FATAL]:-Process timeout failure" 2
		fi
	done
	if [ $DEBUG_LEVEL -eq 0 ] && [ x"" != x"$VERBOSE" ];then $ECHO;fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

PARALLEL_SUMMARY_STATUS_REPORT () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"	
	REPORT_FAIL=0
	if [ -f $1 ];then
	        KILLED_COUNT=`$GREP -c "KILLED:" $PARALLEL_STATUS_FILE`
                COMPLETED_COUNT=`$GREP -c "COMPLETED:" $PARALLEL_STATUS_FILE`
                FAILED_COUNT=`$GREP -c "FAILED:" $PARALLEL_STATUS_FILE`
		((TOTAL_FAILED_COUNT=$KILLED_COUNT+$FAILED_COUNT))
                LOG_MSG "[INFO]:------------------------------------------------" 1
                LOG_MSG "[INFO]:-Parallel process exit status" 1
                LOG_MSG "[INFO]:------------------------------------------------" 1
                LOG_MSG "[INFO]:-Total processes marked as completed           = $COMPLETED_COUNT" 1
                if [ $KILLED_COUNT -ne 0 ];then
                LOG_MSG "[WARN]:-Total processes marked as killed              = $KILLED_COUNT $WARN_MARK" 1
		REPORT_FAIL=1
                else
                LOG_MSG "[INFO]:-Total processes marked as killed              = 0" 1
                fi
                if [ $FAILED_COUNT -ne 0 ];then
                LOG_MSG "[WARN]:-Total processes marked as failed              = $FAILED_COUNT $WARN_MARK" 1
		REPORT_FAIL=1
                else
                LOG_MSG "[INFO]:-Total processes marked as failed              = 0" 1
                fi
                LOG_MSG "[INFO]:------------------------------------------------" 1
	else
		 LOG_MSG "[WARN]:-Could not locate status file $1" 1
		REPORT_FAIL=1
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

RESET_BATCH_VALUE () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	LOG_MSG "[INFO]:-Batch value being re-configured from $BATCH_DEFAULT to $BATCH_LIMIT" 1
	SAVE_BATCH=$BATCH_DEFAULT
	BATCH_DEFAULT=$BATCH_LIMIT
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

RESTORE_BATCH_VALUE () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	if [ x"" != x"$SAVE_BATCH" ];then
		BATCH_DEFAULT=$SAVE_BATCH
		SAVE_BATCH=""
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

CHK_GPDB_ID () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"	
	if [ -f ${INITDB} ];then
	        PERMISSION=`ls -al ${INITDB}|$AWK '{print $1}'`
		MASTER_INITDB_ID=`ls -al ${INITDB}|$AWK '{print $3}'`
		INIT_CHAR=`$ECHO $MASTER_INITDB_ID|$TR -d '\n'|$WC -c|$TR -d ' '`
		MASTER_INITDB_GROUPID=`ls -al ${INITDB}|$AWK '{print $4}'`
		GROUP_INIT_CHAR=`$ECHO $MASTER_INITDB_ID|$TR -d '\n'|$WC -c|$TR -d ' '`
		GPDB_ID=`id|$TR '(' ' '|$TR ')' ' '|$AWK '{print $2}'`
		GPDB_GROUPID=`id|$TR '(' ' '|$TR ')' ' '|$AWK '{print $4}'`

		USER_EXECUTE=`$ECHO $PERMISSION | $SED -e 's/...\(.\).*/\1/g'`
		GROUP_EXECUTE=`$ECHO $PERMISSION | $SED -e 's/......\(.\).*/\1/g'`

		if [ `$ECHO $GPDB_ID|$TR -d '\n'|$WC -c` -gt $INIT_CHAR ];then
			GPDB_ID_CHK=`$ECHO $GPDB_ID|$CUT -c1-$INIT_CHAR`
		else
			GPDB_ID_CHK=$GPDB_ID
		fi

		if [ `$ECHO $GPDB_GROUPID|$TR -d '\n'|$WC -c` -gt $GROUP_INIT_CHAR ];then
			GPDB_GROUPID_CHK=`$ECHO $GPDB_GROUPID|$CUT -c1-$GROUP_INIT_CHAR`
		else
			GPDB_GROUPID_CHK=$GPDB_GROUPID
		fi		

		if [ x$GPDB_ID_CHK == x$MASTER_INITDB_ID ] && [ x"x" == x"$USER_EXECUTE" ];then		
		    LOG_MSG "[INFO]:-Current user id of $GPDB_ID, matches initdb id of $MASTER_INITDB_ID"
		elif [ x$GPDB_GROUPID_CHK == x$MASTER_INITDB_GROUPID ] && [ x"x" == x"$GROUP_EXECUTE" ] ; then
		    LOG_MSG "[INFO]:-Current group id of $GPDB_GROUPID, matches initdb group id of $MASTER_INITDB_GROUPID"
		else
			LOG_MSG "[WARN]:-File permission mismatch.  The $GPDB_ID_CHK owns the Greenplum Database installation directory."
			LOG_MSG "[WARN]:-You are currently logged in as $MASTER_INITDB_ID and may not have sufficient"
			LOG_MSG "[WARN]:-permissions to run the Greenplum binaries and management utilities."
		fi

		if [ x"" != x"$USER" ];then
			if [ `$ECHO $USER|$TR -d '\n'|$WC -c` -gt $INIT_CHAR ];then
				USER_CHK=`$ECHO $USER|$CUT -c1-$INIT_CHAR`
			else
				USER_CHK=$USER
			fi
			if [ x$GPDB_ID_CHK != x$USER_CHK ];then
				LOG_MSG "[WARN]:-\$USER mismatch, id returns $GPDB_ID, \$USER returns $USER" 1
				LOG_MSG "[WARN]:-The GPDB super user account that owns the initdb binary should run these utilities" 1
				LOG_MSG "[WARN]:-This may cause problems when these utilities are run as $USER" 1
			fi
		else
			LOG_MSG "[INFO]:-Environment variable \$USER unset, will set to $GPDB_ID" 1
			export USER=$GPDB_ID
		fi
		if [ x"" != x"$LOGNAME" ];then
			if [ `$ECHO $LOGNAME|$TR -d '\n'|$WC -c` -gt $INIT_CHAR ];then
				LOGNAME_CHK=`$ECHO $LOGNAME|$CUT -c1-$INIT_CHAR`
			else
				LOGNAME_CHK=$LOGNAME
			fi
			if [ x$GPDB_ID_CHK != x$LOGNAME_CHK ];then
				LOG_MSG "[WARN]:-\$LOGNAME mismatch, id returns $GPDB_ID_CHK, \$LOGNAME returns $LOGNAME_CHK" 1
				LOG_MSG "[WARN]:-The GPDB super user account that owns the initdb binary should run these utilities" 1
				LOG_MSG "[WARN]:-This may cause problems when these utilities are run as $LOGNAME" 1
			fi
		else
			LOG_MSG "[INFO]:-Environment variable \$LOGNAME unset, will set to $GPDB_ID" 1
			export LOGNAME=$GPDB_ID
		fi
	else
		LOG_MSG "[WARN]:-No initdb file, unable to verify id" 1
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

PARSE_PERMISSIONS () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	#Expect 3 parameters schema.tablename port database
	if [ $# -ne 3 ];then
		ERROR_EXIT "[FATAL]:-Got $# parameters, expected 3" 2
	fi
	T_NAME=$1;shift
	PORT=$1;shift
	DB=$1
	#Check to ensure that we have schema_name.tablename
	if [ `$ECHO $T_NAME|grep -c "."` -ne 1 ];then
		ERROR_EXIT "[FATAL]:-Schema name not supplied" 2
	else
		SCHEMA_NAME=`$ECHO $T_NAME|$AWK -F"." '{print $1}'`
		TABLENAME=`$ECHO $T_NAME|$AWK -F"." '{print $2}'`
	fi
	#Get relacl information
	PERM_COUNT=0
	for PERM_LINE in `$PSQL -p $PORT -d "$DB" -A -t -c "select relacl from pg_class where relname='${TABLENAME}' and relnamespace in (select oid from pg_namespace where nspname='$SCHEMA_NAME');"|$TR -d '{'|$TR -d '}'|$TR ',' '\n'|$GREP -v "${USER}="|$TR '/' '\n'|$GREP -v "${USER}" `
	do
		DB_ACCOUNTNAME=`$ECHO $PERM_LINE|$AWK -F"=" '{print $1}'`
		if [ x"" != x"$DB_ACCOUNTNAME" ];then
		    PERM_ALLOW=`$ECHO $PERM_LINE|$AWK -F"=" '{print $2}'`
		    if [ `$ECHO $PERM_ALLOW|$GREP -c "r"` -eq 1 ];then PERM_TEXT="$PERM_TEXT select";fi
		    if [ `$ECHO $PERM_ALLOW|$GREP -c "w"` -eq 1 ];then PERM_TEXT="$PERM_TEXT update";fi
		    if [ `$ECHO $PERM_ALLOW|$GREP -c "d"` -eq 1 ];then PERM_TEXT="$PERM_TEXT delete";fi
		    if [ `$ECHO $PERM_ALLOW|$GREP -c "a"` -eq 1 ];then PERM_TEXT="$PERM_TEXT insert";fi
		    if [ `$ECHO $PERM_ALLOW|$GREP -c "R"` -eq 1 ];then PERM_TEXT="$PERM_TEXT rule";fi
		    if [ `$ECHO $PERM_ALLOW|$GREP -c "x"` -eq 1 ];then PERM_TEXT="$PERM_TEXT references";fi
		    if [ `$ECHO $PERM_ALLOW|$GREP -c "t"` -eq 1 ];then PERM_TEXT="$PERM_TEXT trigger";fi
		    PERM_TEXT=`$ECHO $PERM_TEXT|$TR ' ' ','`
		    PERM_ARRAY[$PERM_COUNT]="grant $PERM_TEXT on ~ to $DB_ACCOUNTNAME;"
		    PERM_TEXT=''
		    ((PERM_COUNT=$PERM_COUNT+1))
		fi
	done
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}


CHK_START_ERROR_TXT () {
	LOG_MSG "[INFO]:-Start Function $FUNCNAME"
	if [ $DISPLAY_ERROR -eq 1 ];then
		LOG_MSG "[INFO]:-*********************************************************************************" 1
		LOG_MSG "[WARN]:-There are $INVALID_COUNT segment(s) marked as invalid" 1
		LOG_MSG "[INFO]:-The GPDB gp_fault_action parameter is set to continue in the event of failure" 1
		LOG_MSG "[INFO]:-To recover from this current invalid state, review usage of gprecoverseg" 1
		LOG_MSG "[INFO]:-management utility, which will recover failed segment instance databases" 1
		LOG_MSG "[WARN]:-As gp_fault_action is set to continue, recovery will need to shutdown the" 1
		LOG_MSG "[WARN]:-GPDB database to ensure a consistent recovery can be completed" 1	
		LOG_MSG "[INFO]:-*********************************************************************************" 1
		EXIT_STATUS=1 
	fi
	LOG_MSG "[INFO]:-End Function $FUNCNAME"
}

# Make a dbid file at a particular host. The dbid file is used by gpstart
# to tell the process in question which segment/master it is.
# Arguments:
#   1 - DBID
#   2 - host name
#   3 - path to data directory
MAKE_DBID_FILE() {
	DBID=$1; shift
	HOST=$1; shift
	DATADIR=$1; shift

	FILEPATH=$DATADIR/gp_dbid

	if [ "$FILEPATH" = "/gp_dbid" ]; then # DATADIR is empty
		ERROR_EXIT "[FATAL]:-Internal error -- expected non-empty data directory" 2
	fi

	$TRUSTED_SHELL $HOST \
	  "$ECHO \"# Greenplum Database identifier for this master/segment.
# Do not change the contents of this file.
dbid = $DBID\" > $FILEPATH &&  chmod 400 $FILEPATH"
}

UPDATE_MPP () {
	LOG_MSG "[INFO][$INST_COUNT]:-Start Function $FUNCNAME"
	U_DB=$DEFAULTDB
	U_PT=$1
	U_MPPNAME="$2"
	U_NUMSEG=$3
	U_DBID=$4
	U_CONTENT=$5
	TYPE=$6
	U_HOST=$7
	U_DIR=$8
	LOG_MSG "[INFO][$INST_COUNT]:-Making dbid file @ $U_HOST:$U_DIR = $U_DBID"
	MAKE_DBID_FILE $U_DBID $U_HOST $U_DIR
	LOG_MSG "[INFO][$INST_COUNT]:-Successfully updated GPDB system table"
	LOG_MSG "[INFO][$INST_COUNT]:-End Function $FUNCNAME"
}

#******************************************************************************
# Main Section
#******************************************************************************
#******************************************************************************
# Setup logging directory
#******************************************************************************
CUR_DATE=`$DATE +%Y%m%d`
DEFLOGDIR=$HOME/gpAdminLogs
if [ ! -d $DEFLOGDIR ]; then
		mkdir $DEFLOGDIR
fi
LOG_FILE=$DEFLOGDIR/${PROG_NAME}_${CUR_DATE}.log

if [ $# -ne 0 ]; then
		if [ "$1" == "-v" ]; then
				VERSION_INFO
		fi
fi
#Set up OS type for scripts to change command lines
OS_TYPE=`uname -s|tr '[A-Z]' '[a-z]'`
case $OS_TYPE in
	sunos ) IPV4_ADDR_LIST_CMD="$IFCONFIG -a4"
		IPV6_ADDR_LIST_CMD="$IFCONFIG -a6"
		PS_TXT="-ef"
		LIB_TYPE="LD_LIBRARY_PATH"
		ZCAT=gzcat
		# MPP-15890
		PG_METHOD=ident
		HOST_ARCH_TYPE="uname -i"
		NOLINE_ECHO=/usr/bin/echo
		DEFAULT_LOCALE_SETTING=en_US.UTF-8
		MAIL=/bin/mailx
		PING_TIME="1"
		DF=`findCmdInPath df`
		# Multi-byte tr needed on Solaris to handle [:upper:], [:lower:], etc.
		MBTR=/usr/xpg4/bin/tr
		DU_TXT="-s" ;;
	linux ) IPV4_ADDR_LIST_CMD="`findCmdInPath ip` -4 address show"
		IPV6_ADDR_LIST_CMD="`findCmdInPath ip` -6 address show"
		PS_TXT="ax"
		LIB_TYPE="LD_LIBRARY_PATH"
		PG_METHOD="ident"
		HOST_ARCH_TYPE="uname -i"
		NOLINE_ECHO="$ECHO -e"
		DEFAULT_LOCALE_SETTING=en_US.utf8
		PING6=`findCmdInPath ping6`
		PING_TIME="-c 1"
		DF="`findCmdInPath df` -P"
		ID=`whoami`
		DU_TXT="-c" ;;
	darwin ) IPV4_ADDR_LIST_CMD="$IFCONFIG -a inet"
		IPV6_ADDR_LIST_CMD="$IFCONFIG -a inet6"
		PS_TXT="ax"
		LIB_TYPE="DYLD_LIBRARY_PATH"
		# Darwin zcat wants to append ".Z" to the end of the file name; use "gunzip -c" instead
		ZCAT="`findCmdInPath gunzip` -c"
		PG_METHOD="ident"
		HOST_ARCH_TYPE="uname -m"
		NOLINE_ECHO=$ECHO
		DEFAULT_LOCALE_SETTING=en_US.utf-8
        	PING6=`findCmdInPath ping6`
		PING_TIME="-c 1"
		DF="`findCmdInPath df` -P"
		DU_TXT="-c" ;;	
	freebsd ) IPV4_ADDR_LIST_CMD="$IFCONFIG -a inet"
		IPV6_ADDR_LIST_CMD="$IFCONFIG -a inet6"
		LIB_TYPE="LD_LIBRARY_PATH"
		PG_METHOD="ident"
		HOST_ARCH_TYPE="uname -m"
		NOLINE_ECHO="$ECHO -e"
		DEFAULT_LOCALE_SETTING=en_US.utf8
		PING_TIME="-c 1"
		DF="`findCmdInPath df` -P"
		DU_TXT="-c" ;;	
	* ) echo unknown ;;
esac


GP_LIBRARY_PATH=`$DIRNAME \`$DIRNAME $INITDB\``/lib
SHARE_PATH=`$DIRNAME \`$DIRNAME $INITDB\``/share
SCRIPTS_DIR=`$DIRNAME \` $DIRNAME $INITDB\``/bin


##
# we setup some EXPORT foo='blah' commands for when we dispatch to segments and standby master
##
EXPORT_GPHOME='export GPHOME='$GPHOME
if [ x"$LIB_TYPE" == x"LD_LIBRARY_PATH" ]; then
    EXPORT_LIB_PATH="export LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
else
    EXPORT_LIB_PATH="export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH"
fi
