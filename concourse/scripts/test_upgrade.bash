#!/bin/bash
#
# Copied from the standard CCP install_gpdb.sh script.
#
set -euo pipefail

# Set the DEBUG_UPGRADE envvar to a nonempty value to get (extremely) verbose
# output.
DEBUG_UPGRADE=${DEBUG_UPGRADE:-}

# The host running the master GPDB segment.
MASTER_HOST=

# The GPHOME directories (containing greenplum_path.sh, the bin/ directory, and
# so on) for the old and new clusters, respectively.
OLD_GPHOME=
NEW_GPHOME=

# The data directory prefix, assumed to be shared by all segments across the
# cluster.
DATADIR_PREFIX=

# The old and new clusters' master data directories.
OLD_MASTER_DATA_DIRECTORY=
NEW_MASTER_DATA_DIRECTORY=

# The locations of the gpinitsystem configuration and hostfile.
GPINITSYSTEM_CONFIG=
GPINITSYSTEM_HOSTFILE=

DIRNAME=$(dirname "$0")

validate_local_envvars() {
    # For local (non-Concourse) runs, make sure we have the environment
    # variables we need for the rest of the script to run.
    #
    # TODO: At the moment, we assume/require a gpdemo setup. That should change.
    local missing=

    if [ -z "${GPHOME:-}" ]; then
        missing+='GPHOME '
    fi
    if [ -z "${MASTER_DATA_DIRECTORY:-}" ]; then
        missing+='MASTER_DATA_DIRECTORY '
    fi
    if [ -z "${PGPORT:-}" ]; then
        missing+='PGPORT '
    fi

    if [ -n "${missing}" ]; then
        echo 'This script requires the following environment variables to be set:'
        echo

        for var in ${missing}; do
            echo "    $var"
        done

        echo
        echo 'Please source greenplum_path.sh and gpdemo-env.sh and try again.'

        exit 1
    fi

    # Quickly check to see if this looks like a demo cluster; for now, it's the
    # only thing we support.
    local demodir=$(dirname $(dirname $(dirname ${MASTER_DATA_DIRECTORY})))
    if [[ $demodir != */gpAux/gpdemo ]]; then
        echo 'At the moment, this script only supports clusters that have been '
        echo 'created using the gpdemo scripts in gpAux. Your master data '
        echo 'directory does not appear to be part of a demo cluster:'
        echo
        echo "    ${MASTER_DATA_DIRECTORY}"
        echo
        echo 'Patches welcome!'

        exit 1
    fi
}

load_old_db_data() {
    # Copy the SQL dump over to the master host and load it into the database.
    local dumpfile=$1
    local psql_env="PGOPTIONS='--client-min-messages=warning'"
    local psql_opts="-q"

    if [ -n "$DEBUG_UPGRADE" ]; then
        # Don't quiet psql when debugging.
        psql_env=
        psql_opts=
    fi

    psql_opts+=" ${PSQL_ADDOPTS}"

    echo 'Loading test database...'

    scp "$dumpfile" ${MASTER_HOST}:/tmp/dump.sql.xz
    ssh -n ${MASTER_HOST} '
        source '"${OLD_GPHOME}"'/greenplum_path.sh
        unxz < /tmp/dump.sql.xz | '"${psql_env}"' psql '"${psql_opts}"' -f - postgres
    '

    # There are some states, important for upgrade, that can't be reached
    # by restoring from a dump file only, so we explicitly setup these cases here.
    scp "${DIRNAME}"/../../src/test/regress/sql/gp_upgrade_cornercases.sql ${MASTER_HOST}:/tmp/
    ssh -n ${MASTER_HOST} '
        source '"${OLD_GPHOME}"'/greenplum_path.sh
        '"${psql_env}"' psql '"${psql_opts}"' -d postgres -f /tmp/gp_upgrade_cornercases.sql
    '
}

dump_cluster() {
    # Dump the entire cluster contents to file, using the new pg_dumpall.
    local dumpfile=$1

    ssh -n ${MASTER_HOST} "
        source ${NEW_GPHOME}/greenplum_path.sh
        pg_dumpall -f '$dumpfile' ${PSQL_ADDOPTS}
    "
}

extract_gpdb_tarball() {
    local node_hostname=$1
    local tarball_dir=$2
    # Commonly the incoming binary will be called bin_gpdb.tar.gz. Because many other teams/pipelines tend to use 
    # that naming convention we are not, deliberately. Once the file crosses into our domain, we will not use
    # the conventional name.  This should make clear that we will install any valid binary, not just those that follow
    # the naming convention.
    scp ${tarball_dir}/*.tar.gz $node_hostname:/tmp/gpdb_binary.tar.gz
    ssh -ttn $node_hostname "sudo bash -c \"\
      mkdir -p ${NEW_GPHOME}; \
      tar -xf /tmp/gpdb_binary.tar.gz -C ${NEW_GPHOME}; \
      chown -R gpadmin:gpadmin ${NEW_GPHOME}; \
      sed -ie 's|^GPHOME=.*|GPHOME=${NEW_GPHOME}|' ${NEW_GPHOME}/greenplum_path.sh ; \
    \""
}

create_new_datadir() {
    local node_hostname=$1

    # We only need superuser access to create the new data directory when
    # running in Concourse.
    if (( "${CONCOURSE_MODE}" )); then
        SUDO=sudo
    else
        SUDO=
    fi

    # Create a -new directory for every data directory that already exists.
    # This is what we'll be init'ing the new database into.
    ssh -ttn "$node_hostname" "$SUDO"' bash -c '\''
        for dir in $(find '"${DATADIR_PREFIX}"'/* -maxdepth 0 -type d); do
            newdir="${dir}-new"

            mkdir -p "$newdir"

            if (( "'"${CONCOURSE_MODE}"'" )); then
                chown gpadmin:gpadmin "$newdir"
            fi
        done
    '\'''
}

prep_new_cluster() {
    # Install the new GPDB version on all nodes, if running in Concourse mode,
    # and create the new data directories.
    if (( $CONCOURSE_MODE )); then
        local cluster_name=$(cat ./terraform*/name)

        if [ -z "${NUMBER_OF_NODES}" ]; then
            echo "Number of nodes must be supplied to this script"
            exit 1
        fi

        cat << EOF
  ############################
  #                          #
  #  New GPDB Installation   #
  #                          #
  ############################
EOF

        if [ -z "${GPDB_TARBALL_DIR}" ]; then
            GPDB_TARBALL_DIR=gpdb_binary
            echo "Using default tarball directory: $GPDB_TARBALL_DIR"
        fi

        for ((i=0; i<${NUMBER_OF_NODES}; ++i)); do
            extract_gpdb_tarball ccp-${cluster_name}-$i ${GPDB_TARBALL_DIR}
            create_new_datadir ccp-${cluster_name}-$i
        done
    else
        create_new_datadir localhost
    fi
}

gpinitsystem_for_upgrade() {
    # Stop the old cluster and init a new one.
    #
    # FIXME: the checksum/string settings below need to be pulled from the
    # previous database, not hardcoded. And long-term, we need to decide how
    # Greenplum clusters should be upgraded when GUC settings' defaults change.
    ssh -ttn ${MASTER_HOST} '
        source '"${OLD_GPHOME}"'/greenplum_path.sh
        gpstop -a -d '"${OLD_MASTER_DATA_DIRECTORY}"'

        source '"${NEW_GPHOME}"'/greenplum_path.sh
        sed -E -e '\''s|('"${DATADIR_PREFIX}"'/[[:alnum:]_-]+)|\1-new|g'\'' '"${GPINITSYSTEM_CONFIG}"' > gpinitsystem_config_new

        # XXX Disable mirrors for now.
        echo "unset MIRROR_DATA_DIRECTORY" >> gpinitsystem_config_new
        echo "unset MIRROR_PORT_BASE" >> gpinitsystem_config_new

        # echo "HEAP_CHECKSUM=off" >> gpinitsystem_config_new
        # echo "standard_conforming_strings = off" >> upgrade_addopts
        # echo "escape_string_warning = off" >> upgrade_addopts
        gpinitsystem -a -c gpinitsystem_config_new -h '"${GPINITSYSTEM_HOSTFILE}"' # -p ~gpadmin/upgrade_addopts
        gpstop -a -d '"${NEW_MASTER_DATA_DIRECTORY}"'
    '
}

# run_upgrade hostname data-directory [options]
run_upgrade() {
    # Runs pg_upgrade on a host for the given data directory. The new data
    # directory is assumed to follow the *-new convention established by
    # gpinitsystem_for_upgrade(), above.

    local hostname=$1
    local datadir=$2
    shift 2

    local upgrade_opts=

    if [ -n "$DEBUG_UPGRADE" ]; then
        upgrade_opts="--verbose"
    fi

    ssh -ttn "$hostname" '
        source '"${NEW_GPHOME}"'/greenplum_path.sh
        export TIMEFORMAT=$'\'''"$hostname"'::'"$datadir"'\telapsed\t%3lR\tuser\t%3lU\tsys\t%3lS'\''
        time pg_upgrade '"${upgrade_opts}"' '"$*"' \
            -b '"${OLD_GPHOME}"'/bin/ -B '"${NEW_GPHOME}"'/bin/ \
            -d '"$datadir"' \
            -D '"$(get_new_datadir "$datadir")"
}

dump_old_master_query() {
    # Prints the rows generated by the given SQL query to stdout. The query is
    # run on the old master, pre-upgrade.
    ssh -n ${MASTER_HOST} '
        source '"${OLD_GPHOME}"'/greenplum_path.sh
        psql postgres '"${PSQL_ADDOPTS}"' --quiet --no-align --tuples-only -F"'$'\t''" -c "'$1'"
    '
}

get_segment_datadirs() {
    # Prints the hostnames and data directories of each primary: one instance
    # per line, with the hostname and data directory separated by a tab.
    #
    # XXX For now we ignore mirrors; they don't upgrade correctly with GPDB 6.

    # First try dumping the 6.0 version...
    local q="SELECT hostname, datadir FROM gp_segment_configuration WHERE content <> -1 AND role = 'p'"
    if ! dump_old_master_query "$q" 2>/dev/null; then
        # ...and then fall back to pre-6.0.
        q="SELECT hostname, fselocation FROM gp_segment_configuration JOIN pg_catalog.pg_filespace_entry ON (dbid = fsedbid) WHERE content <> -1 AND role = 'p'"
        dump_old_master_query "$q"
    fi
}

get_new_datadir() {
    # Given a data directory for the old cluster, generates a new data directory
    # based on that path and prints it to stdout.
    local datadir=$1
    sed -E -e 's|('"${DATADIR_PREFIX}"'/[[:alnum:]_-]+)|\1-new|g' <<< "$datadir"
}

start_upgraded_cluster() {
    ssh -n ${MASTER_HOST} "
        source ${NEW_GPHOME}/greenplum_path.sh
        MASTER_DATA_DIRECTORY='${NEW_MASTER_DATA_DIRECTORY}' gpstart -a -v
    "
}

apply_sql_fixups() {
    local psql_env=
    local psql_opts="-v ON_ERROR_STOP=1"

    if [ -n "$DEBUG_UPGRADE" ]; then
        # Don't quiet psql when debugging.
        psql_opts+=" -e"
    else
        psql_env="PGOPTIONS='--client-min-messages=warning'"
        psql_opts+=" -q"
    fi

    psql_opts+=" ${PSQL_ADDOPTS}"

    echo 'Finalizing upgrade...'

    # FIXME: we need a generic way for gp_upgrade to figure out which SQL fixup
    # files need to be applied to the cluster before it is used.
    ssh -n ${MASTER_HOST} '
        source '"${NEW_GPHOME}"'/greenplum_path.sh
        if [ -f reindex_all.sql ]; then
            '"${psql_env}"' psql '"${psql_opts}"' -f reindex_all.sql template1
        fi
    '
}

# Compares the old and new pg_dumpall output (after running them both through
# dumpsort).
compare_dumps() {
    local old_dump=$1
    local new_dump=$2

    scp "$DIRNAME/dumpsort.gawk" ${MASTER_HOST}:~

    ssh -n ${MASTER_HOST} "
        diff -w -U3 --speed-large-files --ignore-space-change \
            <(gawk -f ~/dumpsort.gawk < '$old_dump') \
            <(gawk -f ~/dumpsort.gawk < '$new_dump')
    "
}

exit_with_usage() {
    echo "Usage: $0 [options]"
    echo
    echo ' -c           Operate in "Concourse mode", which makes assumptions '
    echo '              about cluster locations and settings'
    echo ' -n           Concourse-mode only: number of separate hosts to install '
    echo '              GPDB to'
    echo ' -s           Location of an xz-zipped SQL dump file to restore before '
    echo '              upgrade (optional) '
    echo ' -t           Concourse-mode only: directory containing the GPDB '
    echo '              tarball to install'
    echo
    echo ' -h           display this message'

    exit 1
}

# Whether we're running as part of a Concourse build.
CONCOURSE_MODE=0
SQLDUMP_FILE=
GPDB_TARBALL_DIR=

# Use the -n argument for the number of hosts to connect to; if that's not
# given, fall back to the NUMBER_OF_NODES environment variable.
NUMBER_OF_NODES=${NUMBER_OF_NODES:-}

while getopts "cn:s:t:h" option; do
    case "$option" in
    c)
        CONCOURSE_MODE=1
        ;;
    n)
        NUMBER_OF_NODES=$OPTARG
        ;;
    s)
        SQLDUMP_FILE=$OPTARG
        ;;
    t)
        GPDB_TARBALL_DIR=$OPTARG
        ;;
    *)
        exit_with_usage
        ;;
    esac
done

# Set up the globals according to whether we're running in local or Concourse
# mode.
if (( $CONCOURSE_MODE )); then
    MASTER_HOST=mdw
    OLD_GPHOME=/usr/local/greenplum-db-devel
    NEW_GPHOME=/usr/local/gpdb_master
    DATADIR_PREFIX=/data/gpdata
    OLD_MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
    NEW_MASTER_DATA_DIRECTORY=/data/gpdata/master-new/gpseg-1
    PSQL_ADDOPTS=
    GPINITSYSTEM_CONFIG=gpinitsystem_config
    GPINITSYSTEM_HOSTFILE=segment_host_list
else
    validate_local_envvars

    # XXX Several of these variable assignments assume a standard gpdemo layout.
    # If you'd like to change that, make sure you change the
    # validate_local_envvars() implementation as well.
    MASTER_HOST=localhost
    OLD_GPHOME=${GPHOME}
    NEW_GPHOME=${GPHOME}
    DATADIR_PREFIX=$(dirname $(dirname ${MASTER_DATA_DIRECTORY}))
    OLD_MASTER_DATA_DIRECTORY=${MASTER_DATA_DIRECTORY}
    NEW_MASTER_DATA_DIRECTORY=$(get_new_datadir "${MASTER_DATA_DIRECTORY}")
    PSQL_ADDOPTS="-p ${PGPORT}"
    GPINITSYSTEM_CONFIG=$(dirname ${DATADIR_PREFIX})/clusterConfigFile
    GPINITSYSTEM_HOSTFILE=$(dirname ${DATADIR_PREFIX})/hostfile
fi

old_dump=/tmp/pre_upgrade.sql
new_dump=/tmp/post_upgrade.sql

set -v

if (( ${CONCOURSE_MODE} )); then
    # Make sure we can contact every host in the cluster.
    ./ccp_src/scripts/setup_ssh_to_cluster.sh
fi

# Load a SQL dump if given with the -s option.
if [ -n "${SQLDUMP_FILE}" ]; then
    time load_old_db_data "${SQLDUMP_FILE}"
fi

prep_new_cluster

time dump_cluster "$old_dump"
get_segment_datadirs > /tmp/segment_datadirs.txt
gpinitsystem_for_upgrade

# TODO: we need to switch the mode argument according to GPDB version
echo "Upgrading master at ${MASTER_HOST}..."
run_upgrade ${MASTER_HOST} "${OLD_MASTER_DATA_DIRECTORY}" --mode=dispatcher

while read -u30 hostname datadir; do
    echo "Upgrading segment at '$hostname' ($datadir)..."

    newdatadir=$(get_new_datadir "$datadir")

    # NOTE: the trailing slash on the rsync source directory is important! It
    # means to transfer the directory's contents and not the directory itself.
    ssh -n ${MASTER_HOST} rsync -r --delete "${NEW_MASTER_DATA_DIRECTORY}/" "$hostname:$newdatadir" \
        --exclude /postgresql.conf \
        --exclude /pg_hba.conf \
        --exclude /postmaster.opts \
        --exclude /gp_replication.conf \
        --exclude /gp_dbid \
        --exclude /gpssh.conf \
        --exclude /gpperfmon/

    run_upgrade "$hostname" "$datadir" --mode=segment
done 30< /tmp/segment_datadirs.txt

start_upgraded_cluster
time apply_sql_fixups
time dump_cluster "$new_dump"

if ! compare_dumps "$old_dump" "$new_dump"; then
    echo 'error: before and after dumps differ'
    exit 1
fi

echo Complete
