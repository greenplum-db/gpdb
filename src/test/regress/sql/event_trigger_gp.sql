create or replace function test_event_trigger() returns event_trigger as $$
BEGIN
    RAISE NOTICE 'test_event_trigger: % %', tg_event, tg_tag;
END
$$ language plpgsql;

create event trigger regress_event_trigger on ddl_command_start
   execute procedure test_event_trigger();

-- Test event triggers on GPDB specific objects
CREATE EXTERNAL WEB TABLE echotest (x text) EXECUTE 'echo foo;' FORMAT 'text';
DROP EXTERNAL TABLE echotest;

CREATE OR REPLACE FUNCTION write_to_file() RETURNS integer as '$libdir/gpextprotocol.so', 'demoprot_export' LANGUAGE C STABLE NO SQL;
CREATE OR REPLACE FUNCTION read_from_file() RETURNS integer as '$libdir/gpextprotocol.so', 'demoprot_import' LANGUAGE C STABLE NO SQL;

CREATE PROTOCOL demoprot_event_trig_test (readfunc = 'read_from_file', writefunc = 'write_to_file');

CREATE WRITABLE EXTERNAL TABLE demoprot_w(a int) location('demoprot_event_trig_test://demoprotfile.txt') format 'text';

DROP EXTERNAL TABLE demoprot_w CASCADE;

DROP PROTOCOL demoprot_event_trig_test;

drop event trigger regress_event_trigger;
