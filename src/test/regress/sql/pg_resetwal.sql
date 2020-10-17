-- setup
\! mkdir -p /tmp/test_pg_resetwal/global /tmp/test_pg_resetwal/pg_wal
\! touch /tmp/test_pg_resetwal/global/pg_control
\! echo "12" > /tmp/test_pg_resetwal/PG_VERSION

-- negative test:
-- missing argument
\! pg_resetwal -k -n /tmp/test_pg_resetwal
-- wrong argument
\! pg_resetwal -k wrong_version -n /tmp/test_pg_resetwal
-- lower than 0
\! pg_resetwal -k -1 -n /tmp/test_pg_resetwal
-- greater than PG_DATA_CHECKSUM_VERSION
\! pg_resetwal -k 2 -n /tmp/test_pg_resetwal

-- positive test:
-- disable the data checksum
\! pg_resetwal -k 0 -n /tmp/test_pg_resetwal | grep "Data page checksum version"
-- enable the data checksum
\! pg_resetwal -k 1 -n /tmp/test_pg_resetwal | grep "Data page checksum version"
-- default the checksum should be on
\! pg_resetwal -n /tmp/test_pg_resetwal | grep "Data page checksum version"

-- cleanup
\! rm -fr /tmp/test_pg_resetwal
