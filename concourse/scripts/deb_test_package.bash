#!/bin/bash
set -euxo pipefail

CURRENT_DIR=`pwd`
apt-get update
apt-get install -y vim less  # convenience tools for developers to examine logs
apt-get install -y lsb-release
apt-get install -y ./${DEBIAN_PACKAGE:-deb_package_ubuntu16/greenplum-db.deb}

locale-gen en_US.UTF-8
${CURRENT_DIR}/gpdb_src/concourse/scripts/setup_gpadmin_user.bash
su - gpadmin ${CURRENT_DIR}/gpdb_src/concourse/scripts/deb_init_cluster.bash
su - gpadmin ${CURRENT_DIR}/gpdb_src/concourse/scripts/deb_test_cluster.bash

