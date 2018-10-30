#!/bin/bash

source gpdb_src/concourse/scripts/common.bash
install_gpdb

source /usr/local/greenplum-db-devel/greenplum_path.sh

pushd tz_src && git checkout $tz_tag
make DESTDIR=/tmp/tzdir install
popd
python gpdb_src/concourse/scripts/runtzcheck.py /usr/local/greenplum-db-devel /tmp/tzdir
