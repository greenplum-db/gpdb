#!/bin/sh

SOURCE_ROOT="/tmp/pgsql/source"
VERSIONS="9.1.0 9.0.4 8.4.8 8.3.15 8.2.21"

for v in $VERSIONS
  do 
     PGHOME=/tmp/pgsql/${v}
     PGDATA=${PGHOME}/data

     ${PGHOME}/bin/pg_ctl -o '-p 15432' -w -D ${PGDATA} start
     ./xlogdump-${v} -p 15432 -g
     mv oid2name.out oid2name-${v}.txt
     ${PGHOME}/bin/pg_ctl -w -D ${PGDATA} stop
done;
