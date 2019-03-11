CREATE FUNCTION test_encoding_name() RETURNS text AS '$libdir/gppc_test' LANGUAGE c IMMUTABLE;
CREATE FUNCTION test_reset_current_memory() RETURNS bool AS '$libdir/gppc_test' LANGUAGE c;
CREATE FUNCTION test_reset_child_memory() RETURNS bool AS '$libdir/gppc_test' LANGUAGE c;
CREATE FUNCTION test_interrupt(bool) RETURNS void AS '$libdir/gppc_test' LANGUAGE c;

SELECT test_reset_current_memory();
SELECT test_reset_child_memory();
SELECT test_interrupt(NULL);
SELECT test_interrupt(pg_cancel_backend(pg_backend_pid()));
