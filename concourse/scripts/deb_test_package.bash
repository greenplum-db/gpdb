#!/bin/bash
set -euxo pipefail

CURRENT_DIR=`pwd`
apt-get install -y ./${DEBIAN_PACKAGE:-deb_package_ubuntu16/greenplum-db.deb}

locale-gen en_US.UTF-8
${CURRENT_DIR}/gpdb_src/concourse/scripts/setup_gpadmin_user.bash
su - gpadmin ${CURRENT_DIR}/gpdb_src/concourse/scripts/deb_init_cluster.bash

if ${IS_OPEN_SOURCE_DEB} ; then
    su - gpadmin ${CURRENT_DIR}/gpdb_src/concourse/scripts/open_source_deb_test_cluster.bash
else
   su - gpadmin -c "source /opt/gpdb/greenplum_path.sh && psql -t -U gpadmin -v ON_ERROR_STOP=ON template1" << EOF
   -- check libxml
   SELECT '<?xml version="1.0" ?><response><status>foobar</status></response>'::xml;
   -- check quicklz
   DROP TABLE IF EXISTS T1;
   CREATE TABLE T1 (c1 varchar ENCODING (compresstype=quicklz, blocksize=65536)) WITH (appendonly=true, orientation=column);
   INSERT INTO T1 SELECT i FROM generate_series(1, 1000) i;
   \d+ T1;
EOF

fi
