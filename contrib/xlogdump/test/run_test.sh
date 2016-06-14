#!/bin/sh

XLOGDUMP_BIN=$1
PGHOME=$2
TEST_PATH=$3

if [ -z ${TEST_PATH} ]; then
    echo "Usage: $0 <XLOGDUMP_BIN> <PGHOME> <TEST_PATH>";
    exit;
fi;

PGDATA=${PGHOME}/data
PATH=${PGHOME}/bin:$PATH
PGPORT=5436

export PGHOME PGDATA PATH PGPORT

function setUp()
{
    rm -rf ${PGDATA}
    initdb -D ${PGDATA} --no-locale -E UTF-8
    pg_ctl -w -D ${PGDATA} start -o "-p $PGPORT"
    psql -p $PGPORT -c 'select pg_switch_xlog()' postgres

    rm -rf ${TEST_PATH}/output ${TEST_PATH}/result
}

function tearDown()
{
    pg_ctl -w -D ${PGDATA} stop

    mkdir -p ${TEST_PATH}/output
    cp ${PGDATA}/pg_xlog/000000010000000000000002 ${TEST_PATH}/output

    ${XLOGDUMP_BIN} ${TEST_PATH}/output/000000010000000000000002 > ${TEST_PATH}/output/xlogdump.out

    # Rewrite timestamp strings in the output, and then
    # compare it with the expected one.
    perl -e 's/\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d [^ \n]+/0000-00-00 00:00:00 GMT/g;' -p < ${TEST_PATH}/output/xlogdump.out > ${TEST_PATH}/output/xlogdump.out.ts

    mkdir -p ${TEST_PATH}/result
    diff -rc ${TEST_PATH}/expected/xlogdump.out ${TEST_PATH}/output/xlogdump.out.ts > ${TEST_PATH}/result/xlogdump.diff
}

setUp
psql -p $PGPORT -f ${TEST_PATH}/sql/test.sql postgres
tearDown
