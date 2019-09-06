-- should fail, return type mismatch
create event trigger regress_event_trigger
   on ddl_command_start
   execute procedure pg_backend_pid();

-- OK
create function test_event_trigger() returns event_trigger as $$
BEGIN
    RAISE NOTICE 'test_event_trigger: % %', tg_event, tg_tag;
END
$$ language plpgsql;

-- should fail, event triggers cannot have declared arguments
create function test_event_trigger_arg(name text)
returns event_trigger as $$ BEGIN RETURN 1; END $$ language plpgsql;

-- should fail, SQL functions cannot be event triggers
create function test_event_trigger_sql() returns event_trigger as $$
SELECT 1 $$ language sql;

-- should fail, no elephant_bootstrap entry point
create event trigger regress_event_trigger on elephant_bootstrap
   execute procedure test_event_trigger();

-- OK
create event trigger regress_event_trigger on ddl_command_start
   execute procedure test_event_trigger();

-- OK
create event trigger regress_event_trigger_end on ddl_command_end
   execute procedure test_event_trigger();

-- should fail, food is not a valid filter variable
create event trigger regress_event_trigger2 on ddl_command_start
   when food in ('sandwhich')
   execute procedure test_event_trigger();

-- should fail, sandwhich is not a valid command tag
create event trigger regress_event_trigger2 on ddl_command_start
   when tag in ('sandwhich')
   execute procedure test_event_trigger();

-- should fail, create skunkcabbage is not a valid command tag
create event trigger regress_event_trigger2 on ddl_command_start
   when tag in ('create table', 'create skunkcabbage')
   execute procedure test_event_trigger();

-- should fail, can't have event triggers on event triggers
create event trigger regress_event_trigger2 on ddl_command_start
   when tag in ('DROP EVENT TRIGGER')
   execute procedure test_event_trigger();

-- should fail, can't have event triggers on global objects
create event trigger regress_event_trigger2 on ddl_command_start
   when tag in ('CREATE ROLE')
   execute procedure test_event_trigger();

-- should fail, can't have event triggers on global objects
create event trigger regress_event_trigger2 on ddl_command_start
   when tag in ('CREATE DATABASE')
   execute procedure test_event_trigger();

-- should fail, can't have event triggers on global objects
create event trigger regress_event_trigger2 on ddl_command_start
   when tag in ('CREATE TABLESPACE')
   execute procedure test_event_trigger();

-- should fail, can't have same filter variable twice
create event trigger regress_event_trigger2 on ddl_command_start
   when tag in ('create table') and tag in ('CREATE FUNCTION')
   execute procedure test_event_trigger();

-- should fail, can't have arguments
create event trigger regress_event_trigger2 on ddl_command_start
   execute procedure test_event_trigger('argument not allowed');

-- OK
create event trigger regress_event_trigger2 on ddl_command_start
   when tag in ('create table', 'CREATE FUNCTION')
   execute procedure test_event_trigger();

-- OK
comment on event trigger regress_event_trigger is 'test comment';

-- should fail, event triggers are not schema objects
comment on event trigger wrong.regress_event_trigger is 'test comment';

-- drop as non-superuser should fail
create role regression_bob;
set role regression_bob;
create event trigger regress_event_trigger_noperms on ddl_command_start
   execute procedure test_event_trigger();
reset role;

-- all OK
alter event trigger regress_event_trigger enable replica;
alter event trigger regress_event_trigger enable always;
alter event trigger regress_event_trigger enable;
alter event trigger regress_event_trigger disable;

-- regress_event_trigger2 and regress_event_trigger_end should fire, but not
-- regress_event_trigger
create table event_trigger_fire1 (a int);

-- regress_event_trigger_end should fire on these commands
grant all on table event_trigger_fire1 to public;
comment on table event_trigger_fire1 is 'here is a comment';
revoke all on table event_trigger_fire1 from public;
drop table event_trigger_fire1;
create foreign data wrapper useless;
create server useless_server foreign data wrapper useless;
create user mapping for regression_bob server useless_server;
alter default privileges for role regression_bob
 revoke delete on tables from regression_bob;

-- alter owner to non-superuser should fail
alter event trigger regress_event_trigger owner to regression_bob;

-- alter owner to superuser should work
alter role regression_bob superuser;
alter event trigger regress_event_trigger owner to regression_bob;

-- should fail, name collision
alter event trigger regress_event_trigger rename to regress_event_trigger2;

-- OK
alter event trigger regress_event_trigger rename to regress_event_trigger3;

-- should fail, doesn't exist any more
drop event trigger regress_event_trigger;

-- should fail, regression_bob owns some objects
drop role regression_bob;

-- cleanup before next test
-- these are all OK; the second one should emit a NOTICE
drop event trigger if exists regress_event_trigger2;
drop event trigger if exists regress_event_trigger2;
drop event trigger regress_event_trigger3;
drop event trigger regress_event_trigger_end;

-- test support for dropped objects
CREATE SCHEMA schema_one authorization regression_bob;
CREATE SCHEMA schema_two authorization regression_bob;
CREATE SCHEMA audit_tbls authorization regression_bob;
CREATE TEMP TABLE a_temp_tbl ();
SET SESSION AUTHORIZATION regression_bob;

CREATE TABLE schema_one.table_one(a int);
CREATE TABLE schema_one."table two"(a int);
CREATE TABLE schema_one.table_three(a int);
CREATE TABLE audit_tbls.schema_one_table_two(the_value text);

CREATE TABLE schema_two.table_two(a int);
CREATE TABLE schema_two.table_three(a int, b text);
CREATE TABLE audit_tbls.schema_two_table_three(the_value text);

CREATE OR REPLACE FUNCTION schema_two.add(int, int) RETURNS int LANGUAGE plpgsql
  CALLED ON NULL INPUT
  AS $$ BEGIN RETURN coalesce($1,0) + coalesce($2,0); END; $$;
CREATE AGGREGATE schema_two.newton
  (BASETYPE = int, SFUNC = schema_two.add, STYPE = int);

RESET SESSION AUTHORIZATION;

CREATE TABLE undroppable_objs (
	object_type text,
	object_identity text
);
INSERT INTO undroppable_objs VALUES
('table', 'schema_one.table_three'),
('table', 'audit_tbls.schema_two_table_three');

CREATE TABLE dropped_objects (
	type text,
	schema text,
	object text
);

-- This tests errors raised within event triggers; the one in audit_tbls
-- uses 2nd-level recursive invocation via test_evtrig_dropped_objects().
CREATE OR REPLACE FUNCTION undroppable() RETURNS event_trigger
LANGUAGE plpgsql AS $$
DECLARE
	obj record;
	undroppable_obj record;
BEGIN
	PERFORM 1 FROM pg_tables WHERE tablename = 'undroppable_objs';
	IF NOT FOUND THEN
		RAISE NOTICE 'table undroppable_objs not found, skipping';
		RETURN;
	END IF;

	-- This query has been modified from upstream's,
	-- to not do a join, because with the original query, the planner would
	-- execute the pg_event_trigger_dropped_objects() function in an entry DB
	-- worker process, not the QD process. That doesn't work, the function
	-- relies on information stored in backend-local memory, and it throws
	-- an error if it's executed in a different process. Work around that
	-- by looping through the rows in PL/pgSQL instead.
	FOR obj IN SELECT * FROM pg_event_trigger_dropped_objects()
	LOOP
	  FOR undroppable_obj IN
		SELECT * FROM undroppable_objs WHERE object_type = obj.object_type AND object_identity = obj.object_identity
	  LOOP
		RAISE EXCEPTION 'object % of type % cannot be dropped',
			obj.object_identity, obj.object_type;
	  END LOOP;
	END LOOP;
END;
$$;

CREATE EVENT TRIGGER undroppable ON sql_drop
	EXECUTE PROCEDURE undroppable();

CREATE OR REPLACE FUNCTION test_evtrig_dropped_objects() RETURNS event_trigger
LANGUAGE plpgsql AS $$
DECLARE
    obj record;
BEGIN
    FOR obj IN SELECT * FROM pg_event_trigger_dropped_objects()
    LOOP
        IF obj.object_type = 'table' THEN
                EXECUTE format('DROP TABLE IF EXISTS audit_tbls.%I',
					format('%s_%s', obj.schema_name, obj.object_name));
        END IF;

	INSERT INTO dropped_objects
		(type, schema, object) VALUES
		(obj.object_type, obj.schema_name, obj.object_identity);
    END LOOP;
END
$$;

CREATE EVENT TRIGGER regress_event_trigger_drop_objects ON sql_drop
	WHEN TAG IN ('drop table', 'drop function', 'drop view',
		'drop owned', 'drop schema', 'alter table')
	EXECUTE PROCEDURE test_evtrig_dropped_objects();

ALTER TABLE schema_one.table_one DROP COLUMN a;
DROP SCHEMA schema_one, schema_two CASCADE;
DELETE FROM undroppable_objs WHERE object_identity = 'audit_tbls.schema_two_table_three';
DROP SCHEMA schema_one, schema_two CASCADE;
DELETE FROM undroppable_objs WHERE object_identity = 'schema_one.table_three';
DROP SCHEMA schema_one, schema_two CASCADE;

SELECT * FROM dropped_objects WHERE schema IS NULL OR schema <> 'pg_toast';

DROP OWNED BY regression_bob;
SELECT * FROM dropped_objects WHERE type = 'schema';

DROP ROLE regression_bob;

DROP EVENT TRIGGER regress_event_trigger_drop_objects;
DROP EVENT TRIGGER undroppable;

CREATE OR REPLACE FUNCTION event_trigger_report_dropped()
 RETURNS event_trigger
 LANGUAGE plpgsql
AS $$
DECLARE r record;
BEGIN
    FOR r IN SELECT * from pg_event_trigger_dropped_objects()
    LOOP
    IF NOT r.normal AND NOT r.original THEN
        CONTINUE;
    END IF;
    RAISE NOTICE 'NORMAL: orig=% normal=% istemp=% type=% identity=% name=% args=%',
        r.original, r.normal, r.is_temporary, r.object_type,
        r.object_identity, r.address_names, r.address_args;
    END LOOP;
END; $$;
CREATE EVENT TRIGGER regress_event_trigger_report_dropped ON sql_drop
    EXECUTE PROCEDURE event_trigger_report_dropped();
CREATE SCHEMA evttrig
	CREATE TABLE one (col_a SERIAL PRIMARY KEY, col_b text DEFAULT 'forty two')
	CREATE INDEX one_idx ON one (col_b)
	CREATE TABLE two (col_c INTEGER CHECK (col_c > 0) REFERENCES one DEFAULT 42);

ALTER TABLE evttrig.two DROP COLUMN col_c;
ALTER TABLE evttrig.one ALTER COLUMN col_b DROP DEFAULT;
ALTER TABLE evttrig.one DROP CONSTRAINT one_pkey;
DROP INDEX evttrig.one_idx;
DROP SCHEMA evttrig CASCADE;
DROP TABLE a_temp_tbl;

DROP EVENT TRIGGER regress_event_trigger_report_dropped;

-- only allowed from within an event trigger function, should fail
select pg_event_trigger_table_rewrite_oid();

-- test Table Rewrite Event Trigger
CREATE OR REPLACE FUNCTION test_evtrig_no_rewrite() RETURNS event_trigger
LANGUAGE plpgsql AS $$
BEGIN
  RAISE EXCEPTION 'I''m sorry Sir, No Rewrite Allowed.';
END;
$$;

create event trigger no_rewrite_allowed on table_rewrite
  execute procedure test_evtrig_no_rewrite();

create table rewriteme (id serial primary key, foo float);
insert into rewriteme
     select x * 1.001 from generate_series(1, 500) as t(x);
alter table rewriteme alter column foo type numeric;
alter table rewriteme add column baz int default 0;

-- test with more than one reason to rewrite a single table
CREATE OR REPLACE FUNCTION test_evtrig_no_rewrite() RETURNS event_trigger
LANGUAGE plpgsql AS $$
BEGIN
  RAISE NOTICE 'Table ''%'' is being rewritten (reason = %)',
               pg_event_trigger_table_rewrite_oid()::regclass,
               pg_event_trigger_table_rewrite_reason();
END;
$$;

alter table rewriteme
 add column onemore int default 0,
 add column another int default -1,
 alter column foo type numeric(10,4);

-- shouldn't trigger a table_rewrite event
alter table rewriteme alter column foo type numeric(12,4);

-- typed tables are rewritten when their type changes.  Don't emit table
-- name, because firing order is not stable.
CREATE OR REPLACE FUNCTION test_evtrig_no_rewrite() RETURNS event_trigger
LANGUAGE plpgsql AS $$
BEGIN
  RAISE NOTICE 'Table is being rewritten (reason = %)',
               pg_event_trigger_table_rewrite_reason();
END;
$$;

create type rewritetype as (a int);
create table rewritemetoo1 of rewritetype;
create table rewritemetoo2 of rewritetype;
alter type rewritetype alter attribute a type text cascade;

-- but this doesn't work
create table rewritemetoo3 (a rewritetype);
alter type rewritetype alter attribute a type varchar cascade;

drop table rewriteme;
drop event trigger no_rewrite_allowed;
drop function test_evtrig_no_rewrite();

-- test Row Security Event Trigger
RESET SESSION AUTHORIZATION;
CREATE TABLE event_trigger_test (a integer, b text);

CREATE OR REPLACE FUNCTION start_command()
RETURNS event_trigger AS $$
BEGIN
RAISE NOTICE '% - ddl_command_start', tg_tag;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION end_command()
RETURNS event_trigger AS $$
BEGIN
RAISE NOTICE '% - ddl_command_end', tg_tag;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION drop_sql_command()
RETURNS event_trigger AS $$
BEGIN
RAISE NOTICE '% - sql_drop', tg_tag;
END;
$$ LANGUAGE plpgsql;

CREATE EVENT TRIGGER start_rls_command ON ddl_command_start
    WHEN TAG IN ('CREATE POLICY', 'ALTER POLICY', 'DROP POLICY') EXECUTE PROCEDURE start_command();

CREATE EVENT TRIGGER end_rls_command ON ddl_command_end
    WHEN TAG IN ('CREATE POLICY', 'ALTER POLICY', 'DROP POLICY') EXECUTE PROCEDURE end_command();

CREATE EVENT TRIGGER sql_drop_command ON sql_drop
    WHEN TAG IN ('DROP POLICY') EXECUTE PROCEDURE drop_sql_command();

CREATE POLICY p1 ON event_trigger_test USING (FALSE);
ALTER POLICY p1 ON event_trigger_test USING (TRUE);
ALTER POLICY p1 ON event_trigger_test RENAME TO p2;
DROP POLICY p2 ON event_trigger_test;

DROP EVENT TRIGGER start_rls_command;
DROP EVENT TRIGGER end_rls_command;
DROP EVENT TRIGGER sql_drop_command;
