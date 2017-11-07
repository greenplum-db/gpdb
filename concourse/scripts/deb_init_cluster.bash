#!/bin/bash
set -euxo pipefail

cp /opt/gpdb/docs/cli_help/gpconfigs/gpinitsystem_singlenode /tmp/

echo `hostname` > ./hostlist_singlenode

source /opt/gpdb/greenplum_path.sh

sed -i "s/MASTER_HOSTNAME=hostname_of_machine/MASTER_HOSTNAME=$HOSTNAME/g" /tmp/gpinitsystem_singlenode
sed -i "s#MASTER_DIRECTORY=/gpmaster#MASTER_DIRECTORY=/tmp/gpmaster#g" /tmp/gpinitsystem_singlenode
sed -i "s#declare -a DATA_DIRECTORY=(/gpdata1 /gpdata2)#declare -a DATA_DIRECTORY=(/tmp/gpdata1 /tmp/gpdata2)#g" /tmp/gpinitsystem_singlenode

mkdir -p /tmp/gpmaster /tmp/gpdata1 /tmp/gpdata2
export TRUSTED_SHELL=ssh

set +e
    gpinitsystem -a -c /tmp/gpinitsystem_singlenode -l /home/gpadmin/gpAdminLogs
    RETURN=$?
set -e

if [ "${RETURN}" -gt 1 ];
then
    # gpinitsystem will return warnings as exit code 1
    exit ${RETURN}
else
    exit 0
fi
