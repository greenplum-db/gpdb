#!/bin/bash
set -eox pipefail
if [ -d "testdata/pgdata" ] && [ -d "testdata/pgsql" ] ; then
	pgbin="testdata/pgsql"
	${pgbin}/bin/pg_ctl -D testdata/pgdata  stop || true
	rm -rf testdata/pglog
	rm -rf testdata/pgdata
fi
