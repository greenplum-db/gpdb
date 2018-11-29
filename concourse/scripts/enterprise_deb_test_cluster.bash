#!/usr/bin/env bash
set -euxo pipefail

source /opt/gpdb/greenplum_path.sh
psql -t -U gpadmin -v ON_ERROR_STOP=ON template1 << EOF
   -- check libxml
   SELECT '<?xml version="1.0" ?><response><status>foobar</status></response>'::xml;
   -- check quicklz
   DROP TABLE IF EXISTS T1;
   CREATE TABLE T1 (c1 varchar ENCODING (compresstype=quicklz, blocksize=65536)) WITH (appendonly=true, orientation=column);
   INSERT INTO T1 SELECT i FROM generate_series(1, 1000) i;
   \d+ T1;
EOF
