#!/bin/sh

SOURCE_ROOT="/tmp/pgsql/source"
VERSIONS="9.1.0 9.0.4 8.4.8 8.3.15 8.2.21"

for v in $VERSIONS
  do 
     PGHOME=/tmp/pgsql/${v}
     PGDATA=${PGHOME}/data

     ${PGHOME}/bin/pg_ctl -w -D ${PGDATA} stop
     rm -rf ${PGDATA}
     ${PGHOME}/bin/initdb --no-locale -D ${PGDATA} -E utf-8

     ${PGHOME}/bin/pg_ctl -o '-p 15432' -w -D ${PGDATA} start
     ${PGHOME}/bin/psql -p 15432 -c 'SELECT pg_switch_xlog()' postgres
     ${PGHOME}/bin/psql -p 15432 -f test/sql/test.sql postgres
     ${PGHOME}/bin/pg_ctl -w -D ${PGDATA} stop

     mkdir -p test/xlog/${v}
     cp ${PGDATA}/pg_xlog/???????????????????????? test/xlog/${v}
done;
