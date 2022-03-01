SELECT min_val, max_val FROM pg_settings WHERE name = 'gp_resqueue_priority_cpucores_per_segment';

-- Test cursor gang should not be reused if SET command happens.
CREATE OR REPLACE FUNCTION test_set_cursor_func() RETURNS text as $$
DECLARE
  result text;
BEGIN
  EXECUTE 'select setting from pg_settings where name=''temp_buffers''' INTO result;
  RETURN result;
END;
$$ LANGUAGE plpgsql;

SET temp_buffers = 2000;
BEGIN;
  DECLARE set_cusor CURSOR FOR SELECT relname FROM gp_dist_random('pg_class');
  -- The GUC setting should not be dispatched to the cursor gang.
  SET temp_buffers = 3000;
END;

-- Verify the cursor gang is not reused. If the gang is reused, the
-- temp_buffers value on that gang should be old one, i.e. 2000 instead of
-- the new committed 3000.
SELECT * from (SELECT test_set_cursor_func() FROM gp_dist_random('pg_class') limit 1) t1
  JOIN (SELECT test_set_cursor_func() FROM gp_dist_random('pg_class') limit 1) t2 ON TRUE;

RESET temp_buffers;

--
-- Test GUC if cursor is opened
--
-- start_ignore
drop table if exists test_cursor_set_table;
drop function if exists test_set_in_loop();
drop function if exists test_call_set_command();
-- end_ignore

create table test_cursor_set_table as select * from generate_series(1, 100);

CREATE FUNCTION test_set_in_loop () RETURNS numeric
    AS $$
DECLARE
    rec record;
    result numeric;
    tmp numeric;
BEGIN
	result = 0;
FOR rec IN select * from test_cursor_set_table
LOOP
        select test_call_set_command() into tmp;
        result = result + 1;
END LOOP;
return result;
END;
$$
    LANGUAGE plpgsql NO SQL;


CREATE FUNCTION test_call_set_command() returns numeric
AS $$
BEGIN
       execute 'SET gp_workfile_limit_per_query=524;';
       return 0;
END;
$$
    LANGUAGE plpgsql NO SQL;

SELECT * from test_set_in_loop();


CREATE FUNCTION test_set_within_initplan () RETURNS numeric
AS $$
DECLARE
	result numeric;
	tmp RECORD;
BEGIN
	result = 1;
	execute 'SET gp_workfile_limit_per_query=524;';
	select into tmp * from test_cursor_set_table limit 100;
	return result;
END;
$$
	LANGUAGE plpgsql;


CREATE TABLE test_initplan_set_table as select * from test_set_within_initplan();


DROP TABLE if exists test_initplan_set_table;
DROP TABLE if exists test_cursor_set_table;
DROP FUNCTION if exists test_set_in_loop();
DROP FUNCTION if exists test_call_set_command();


-- Set work_mem. It emits a WARNING, but it should only emit it once.
--
-- We used to erroneously set the GUC twice in the QD node, whenever you issue
-- a SET command. If this stops emitting a WARNING in the future, we'll need
-- another way to detect that the GUC's assign-hook is called only once.
set work_mem='1MB';
reset work_mem;

--
-- Test if RESET timezone is dispatched to all slices
--
CREATE TABLE timezone_table AS SELECT * FROM (VALUES (123,1513123564),(123,1512140765),(123,1512173164),(123,1512396441)) foo(a, b) DISTRIBUTED RANDOMLY;

SELECT to_timestamp(b)::timestamp WITH TIME ZONE AS b_ts FROM timezone_table ORDER BY b_ts;
SET timezone= 'America/New_York';
-- Check if it is set correctly on QD.
SELECT to_timestamp(1613123565)::timestamp WITH TIME ZONE;
-- Check if it is set correctly on the QEs.
SELECT to_timestamp(b)::timestamp WITH TIME ZONE AS b_ts FROM timezone_table ORDER BY b_ts;
RESET timezone;
-- Check if it is reset correctly on QD.
SELECT to_timestamp(1613123565)::timestamp WITH TIME ZONE;
-- Check if it is reset correctly on the QEs.
SELECT to_timestamp(b)::timestamp WITH TIME ZONE AS b_ts FROM timezone_table ORDER BY b_ts;

--
-- Test if SET TIME ZONE INTERVAL is dispatched correctly to all segments
--
SET TIME ZONE INTERVAL '04:30:06' HOUR TO MINUTE;
-- Check if it is set correctly on QD.
SELECT to_timestamp(1613123565)::timestamp WITH TIME ZONE;
-- Check if it is set correctly on the QEs.
SELECT to_timestamp(b)::timestamp WITH TIME ZONE AS b_ts FROM timezone_table ORDER BY b_ts;

-- Test default_transaction_isolation and transaction_isolation fallback from serializable to repeatable read
CREATE TABLE test_serializable(a int);
insert into test_serializable values(1);
SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL serializable;
show default_transaction_isolation;
SELECT * FROM test_serializable;
SET default_transaction_isolation = 'read committed';
SET default_transaction_isolation = 'serializable';
show default_transaction_isolation;
SELECT * FROM test_serializable;
SET default_transaction_isolation = 'read committed';

BEGIN TRANSACTION ISOLATION LEVEL serializable;
	show transaction_isolation;
	SELECT * FROM test_serializable;
COMMIT;
DROP TABLE test_serializable;


-- Test single query guc rollback
set allow_segment_DML to on;

set datestyle='german';
select gp_inject_fault('set_variable_fault', 'error', dbid)
from gp_segment_configuration where content=0 and role='p';
set datestyle='sql, mdy';
-- after guc set failed, before next query handle, qd will sync guc
-- to qe. using `select 1` trigger guc reset.
select 1;
select current_setting('datestyle') from gp_dist_random('gp_id');

select gp_inject_fault('all', 'reset', dbid) from gp_segment_configuration;
set allow_segment_DML to off;
--
-- Test DISCARD TEMP.
--
-- There's a test like this in upstream 'guc' test, but this expanded version
-- verifies that temp tables are dropped on segments, too.
--
CREATE TEMP TABLE reset_test ( data text ) ON COMMIT DELETE ROWS;
DISCARD TEMP;
-- Try to create a new temp table with same. Should work.
CREATE TEMP TABLE reset_test ( data text ) ON COMMIT PRESERVE ROWS;

-- Now test that the effects of DISCARD TEMP can be rolled back
BEGIN;
DISCARD TEMP;
ROLLBACK;
-- the table should still exist.
INSERT INTO reset_test VALUES (1);

-- Unlike DISCARD TEMP, DISCARD ALL cannot be run in a transaction.
BEGIN;
DISCARD ALL;
COMMIT;
-- the table should still exist.
INSERT INTO reset_test VALUES (2);
SELECT * FROM reset_test;

-- Also DISCARD ALL does not have cluster wide effects. CREATE will fail as the
-- table will not be dropped in the segments.
DISCARD ALL;
CREATE TEMP TABLE reset_test ( data text ) ON COMMIT PRESERVE ROWS;

CREATE TABLE guc_gp_t1(i int);
INSERT INTO guc_gp_t1 VALUES(1),(2);

-- generate an idle redaer gang by the following query
SELECT count(*) FROM guc_gp_t1, guc_gp_t1 t;

-- test create role and set role in the same transaction
BEGIN;
DROP ROLE IF EXISTS guc_gp_test_role1;
CREATE ROLE guc_gp_test_role1;
SET ROLE guc_gp_test_role1;
RESET ROLE;
END;

-- generate an idle redaer gang by the following query
SELECT count(*) FROM guc_gp_t1, guc_gp_t1 t;

BEGIN ISOLATION LEVEL REPEATABLE READ;
DROP ROLE IF EXISTS guc_gp_test_role2;
CREATE ROLE guc_gp_test_role2;
SET ROLE guc_gp_test_role2;
RESET ROLE;
END;

-- test cursor case
-- cursors are also reader gangs, but they are not idle, thus will not be
-- destroyed by utility statement.
BEGIN;
DECLARE c1 CURSOR FOR SELECT * FROM guc_gp_t1 a, guc_gp_t1 b order by a.i, b.i;
DECLARE c2 CURSOR FOR SELECT * FROM guc_gp_t1 a, guc_gp_t1 b order by a.i, b.i;
FETCH c1;
DROP ROLE IF EXISTS guc_gp_test_role1;
CREATE ROLE guc_gp_test_role1;
SET ROLE guc_gp_test_role1;
RESET ROLE;
FETCH c2;
FETCH c1;
FETCH c2;
END;

DROP TABLE guc_gp_t1;

-- test for string guc is quoted correctly
SET search_path = "'";
SHOW search_path;
SET search_path = '"';
SHOW search_path;
SET search_path = '''';
SHOW search_path;
SET search_path = '''abc''';
SHOW search_path;
SET search_path = '\path';
SHOW search_path;
RESET search_path;

-- when the original string guc is empty, we change the guc to new value during executing a command.
-- this guc will be added to gp_guc_restore_list, and they will be restored
-- to original value to qe when the next command is executed.
-- however, the dispatch command is "set xxx to ;" that is wrong.
create extension if not exists gp_inject_fault;
create table public.restore_guc_test(tc1 int);

-- inject fault to change the value of search_path during creating materialized view
SELECT gp_inject_fault('change_string_guc', 'skip', 1);
-- inject fault when dispatch guc restore command occur errors, we throw an error.
SELECT gp_inject_fault('restore_string_guc', 'error', 1);

-- set search_path to '';
SELECT pg_catalog.set_config('search_path', '', false);
-- trigger inject fault of change_string_guc, and add this guc to gp_guc_restore_list
create MATERIALIZED VIEW public.view_restore_guc_test as select * from public.restore_guc_test;

--we should restore gucs in gp_guc_restore_list to qe, no error occurs.
drop MATERIALIZED VIEW public.view_restore_guc_test;
drop table public.restore_guc_test;

--cleanup
reset search_path;
SELECT gp_inject_fault('change_string_guc', 'reset', 1);
SELECT gp_inject_fault('restore_string_guc', 'reset', 1);
