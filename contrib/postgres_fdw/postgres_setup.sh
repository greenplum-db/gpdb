#!/bin/bash
set -eox pipefail
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
	wget https://get.enterprisedb.com/postgresql/postgresql-10.14-1-linux-x64-binaries.tar.gz
	tar xf postgresql-10.14-1-linux-x64-binaries.tar.gz
	rm postgresql-10.14-1-linux-x64-binaries.tar.gz
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
