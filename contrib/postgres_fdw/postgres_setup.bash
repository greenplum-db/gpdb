#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ ! -d testdata ]; then
	mkdir testdata
fi
pushd ${DIR}/testdata
GPPORT=${PGPORT}
GPOPTIONS=${PGOPTIONS}
export PGPORT=${PG_PORT}
# set PGOPTIONS to be empty and restart the GP.
# Becuase PGOPTIONS='-c optimizer=off' is sometimes set on gp cluster
# and it will be sent to pg through postgres_fdw, but pg can not
# recognize the 'optimizer' config. PGOPTIONS is not useful for gp
# cluster, it is used by psql.
export PGOPTIONS=''
pgbin="pgsql"

# install postgres
if [ ! -d "${pgbin}" ] ; then
	mkdir ${pgbin}
	if [ ! -d postgresql-10.4 ]; then
		wget --no-check-certificate https://ftp.postgresql.org/pub/source/v10.4/postgresql-10.4.tar.gz
		tar -xvf postgresql-10.4.tar.gz
	fi
	pushd postgresql-10.4
	./configure --prefix=${DIR}/testdata/${pgbin}
	make -j4 install
	rm -rf postgresql-10.4.tar.gz
	popd
fi

# start postgres
# there may be already a postgres postgres running, anyway, stop it
if [ -d "pgdata" ] ; then
	${pgbin}/bin/pg_ctl -D pgdata  stop || true
	rm -r pgdata
fi
${pgbin}/bin/initdb -D pgdata
${pgbin}/bin/pg_ctl -D pgdata -l pglog start

# init postgres
dropdb --if-exists contrib_regression
createdb contrib_regression
export PGPORT=${GPPORT}
# export PGOPTIONS=${GPOPTIONS}
popd
gpstop -ar
