\echo use "create extension" to load this file. \quit

CREATE FUNCTION extended_protocol_commit_test_fdw_handler()
RETURNS fdw_handler
AS '$libdir/extended_protocol_commit_test_fdw', 'extended_protocol_commit_test_fdw_handler'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER extended_protocol_commit_test_fdw
  HANDLER extended_protocol_commit_test_fdw_handler;
