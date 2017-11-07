#!/usr/bin/env bash
set -euxo pipefail

source /opt/gpdb/greenplum_path.sh

createdb testdb

psql -U gpadmin testdb -c "CREATE TABLE test_table (test_col1 int, text_col2 text);"
psql -U gpadmin testdb -c "INSERT INTO test_table VALUES (1,'val');"

OUTPUT=`psql -t -U gpadmin testdb -c "SELECT COUNT(*) FROM test_table" | tr -d '[:space:]'`
if [ "$OUTPUT" != "1" ]; then
    exit 1
fi

# open source should not include the proprietary quicklz library. Make sure any attempt to use it will fail.
psql -U gpadmin testdb -c "CREATE TABLE foo (a int, b text) WITH (appendonly=true, compresstype=quicklz, compresslevel=1);"
set +e
quicklz_error=$(psql testdb -c "INSERT INTO foo VALUES (1, 'abc');" 2>&1)
set -e

echo $quicklz_error | grep "quicklz compression not supported"
exit $?