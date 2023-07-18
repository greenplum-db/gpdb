#!/bin/bash
if [ -d "testdata/pgdata" ] && [ -d "testdata/pgsql" ] ; then
	pgbin="testdata/pgsql"
	${pgbin}/bin/pg_ctl -D testdata/pgdata  stop || true
	rm -rf testdata/pgdata
fi
rm -rf testdata/pglog