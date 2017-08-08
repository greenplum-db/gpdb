#!/bin/bash
# set -euo pipefail
# IFS=$'\n\t'
# In the future, this script will be provided by the team that owns the test, and they will decide if they should enforce bash strict mode

set -x

SECOND_BINARY_INSTALL_LOCATION=$SECOND_BINARY_INSTALL_LOCATION

extract_gpdb_tarball() {
    local node_hostname=$1
    local tarball_dir=$2
    # Commonly the incoming binary will be called bin_gpdb.tar.gz. Because many other teams/pipelines tend to use
    # that naming convention we are not, deliberately. Once the file crosses into our domain, we will not use
    # the conventional name.  This should make clear that we will install any valid binary, not just those that follow
    # the naming convention.
    gpssh -f cluster_env_files/hostfile_all "bash -c \"\
      mkdir -p $SECOND_BINARY_INSTALL_LOCATION; \
      tar -xf /tmp/gpdb_binary.tar.gz -C $SECOND_BINARY_INSTALL_LOCATION; \
      chown -R gpadmin:gpadmin $SECOND_BINARY_INSTALL_LOCATION; \
    \""
}

update_gphome() {
    local node_hostname=$1
    gpssh -f cluster_env_files/hostfile_all " bash -c \"\
      cd $SECOND_BINARY_INSTALL_LOCATION; \
      sed -i "s,^GPHOME=.*$,GPHOME=$SECOND_BINARY_INSTALL_LOCATION," greenplum_path.sh; \
    \""
}

## Main
source /home/gpadmin/gpinitsystem_config;
source /usr/local/greenplum-db-devel/greenplum_path.sh;
export PGPORT=$MASTER_PORT;
export MASTER_DATA_DIRECTORY=$MASTER_DIRECTORY/$SEG_PREFIX-1;

extract_gpdb_tarball

update_gphome
