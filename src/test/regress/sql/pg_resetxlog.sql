-- setup
\! mkdir -p /tmp/test_pg_resetxlog/global /tmp/test_pg_resetxlog/pg_xlog
\! touch /tmp/test_pg_resetxlog/global/pg_control
\! echo "9.4" > /tmp/test_pg_resetxlog/PG_VERSION

-- negative test:
-- missing argument
\! pg_resetxlog -k -n /tmp/test_pg_resetxlog
-- wrong argument
\! pg_resetxlog -k wrong_version -n /tmp/test_pg_resetxlog
-- lower than 0
\! pg_resetxlog -k -1 -n /tmp/test_pg_resetxlog
-- greater than PG_DATA_CHECKSUM_VERSION
\! pg_resetxlog -k 2 -n /tmp/test_pg_resetxlog

-- positive test:
-- disable the data checksum
\! pg_resetxlog -k 0 -n /tmp/test_pg_resetxlog | grep "Data page checksum version"
-- enable the data checksum
\! pg_resetxlog -k 1 -n /tmp/test_pg_resetxlog | grep "Data page checksum version"
-- default the checksum should be on
\! pg_resetxlog -n /tmp/test_pg_resetxlog | grep "Data page checksum version"

-- cleanup
\! rm -fr /tmp/test_pg_resetxlog
