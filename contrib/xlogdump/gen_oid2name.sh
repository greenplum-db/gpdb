#!/bin/sh

VERSIONS="92 91 90 84 83"

PORT_OPTS='-p 5434'

for v in $VERSIONS
  do if [ -d /usr/local/pgsql${v} ]; then
	 export PGDATA=/tmp/pgdata${v}
         export PATH=/usr/local/pgsql${v}/bin:$PATH

	 rm -rf $PGDATA
	 mkdir $PGDATA
         initdb --no-locale -D ${PGDATA}

         pg_ctl -w -D ${PGDATA} start -o "$PORT_OPTS"

	 cat /dev/null > oid2name-${v}.txt
	 psql -A -t -F' ' $PORT_OPTS postgres -c 'SELECT oid,spcname FROM pg_tablespace ORDER BY oid' >> oid2name-${v}.txt
	 psql -A -t -F' ' $PORT_OPTS postgres -c 'SELECT oid,datname FROM pg_database ORDER BY oid' >> oid2name-${v}.txt
	 psql -A -t -F' ' $PORT_OPTS postgres -c 'SELECT oid,relname FROM pg_class ORDER BY oid' >> oid2name-${v}.txt

         pg_ctl -w -D ${PGDATA} stop

	 rm -rf ${PGDATA}
     fi;
done;
