#!/usr/bin/env bash
if [ -d "testdata/pgdata" ] && [ -d "testdata/pgsql" ] ; then
	pgbin="testdata/pgsql"
	${pgbin}/bin/pg_ctl -D testdata/pgdata  stop || true
	${pgbin}/bin/pg_ctl -D testdata/pgdata2 -o "-p 5555"  stop || true
	rm -rf testdata/pgdata
	rm -rf testdata/pgdata2
fi
rm -rf testdata/pglog
