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

-- test for guc dev_opt_unsafe_truncate_in_subtransaction
-- start_ignore
CREATE LANGUAGE plpythonu;
-- end_ignore
CREATE OR REPLACE FUNCTION run_all_in_one() RETURNS VOID AS
$$
     plpy.execute('CREATE TABLE unsafe_truncate(a int, b int) DISTRIBUTED BY (a)')
     plpy.execute('INSERT INTO unsafe_truncate SELECT * FROM generate_series(1, 10)')
     for i in range(1,4):
         plpy.execute('UPDATE unsafe_truncate SET b = b + 1')
         plpy.execute('CREATE TABLE foobar AS SELECT * FROM unsafe_truncate DISTRIBUTED BY (a)')

         before_truncate = plpy.execute('SELECT relfilenode FROM gp_dist_random(\'pg_class\') WHERE relname=\'unsafe_truncate\' ORDER BY gp_segment_id')
         plpy.execute('truncate unsafe_truncate')
         after_truncate = plpy.execute('SELECT relfilenode FROM gp_dist_random(\'pg_class\') WHERE relname=\'unsafe_truncate\' ORDER BY gp_segment_id')

         plpy.execute('DROP TABLE unsafe_truncate')
         plpy.execute('ALTER TABLE foobar RENAME TO unsafe_truncate')

         if before_truncate[0]['relfilenode'] == after_truncate[0]['relfilenode']:
	     plpy.info('iteration:%d unsafe truncate performed' % (i))
         else:
	     plpy.info('iteration:%d safe truncate performed' % (i))

	 plpy.execute('SET dev_opt_unsafe_truncate_in_subtransaction TO ON')
     plpy.execute('DROP TABLE unsafe_truncate')
     plpy.execute('RESET dev_opt_unsafe_truncate_in_subtransaction')
$$ language plpythonu;

select run_all_in_one();

-- Test single query default_tablespace GUC rollback
-- Function just to save default_tablespace GUC to gp_guc_restore_list
CREATE OR REPLACE FUNCTION set_conf_param() RETURNS VOID
AS $$
BEGIN
    EXECUTE 'SELECT 1;';
END;
$$ LANGUAGE plpgsql
SET default_tablespace TO '';
-- Create temp table to create temp schema
CREATE TEMP TABLE just_a_temp_table (a int);
-- Temp schema should be created for each segment
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Save default_tablespace GUC to gp_guc_restore_list
SELECT set_conf_param();
-- Trigger default_tablespace GUC restore from gp_guc_restore_list
SELECT 1;
-- When default_tablespace GUC is restored from gp_guc_restore_list
-- successfully no RemoveTempRelationsCallback is called.
-- So check that segments still have temp schemas
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Cleanup
DROP TABLE just_a_temp_table;

-- Test single query gp_default_storage_options GUC rollback
-- Function just to save gp_default_storage_options to gp_guc_restore_list
CREATE OR REPLACE FUNCTION set_conf_param() RETURNS VOID
AS $$
BEGIN
    EXECUTE 'SELECT 1;';
END;
$$ LANGUAGE plpgsql
SET gp_default_storage_options TO 'blocksize=32768,compresstype=none,checksum=false';
-- Create temp table to create temp schema
CREATE TEMP TABLE just_a_temp_table (a int);
-- Temp schema should be created for each segment
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Save gp_default_storage_options GUC to gp_guc_restore_list
SELECT set_conf_param();
-- Trigger gp_default_storage_options GUC restore from gp_guc_restore_list
SELECT 1;
-- When gp_default_storage_options GUC is restored from gp_guc_restore_list
-- successfully no RemoveTempRelationsCallback is called.
-- So check that segments still have temp schemas
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Cleanup
DROP TABLE just_a_temp_table;

-- Test single query lc_numeric GUC rollback
-- Set lc_numeric to OS-friendly value
SET lc_numeric TO 'C';
-- Function just to save lc_numeric GUC to gp_guc_restore_list
CREATE OR REPLACE FUNCTION set_conf_param() RETURNS VOID
AS $$
BEGIN
    EXECUTE 'SELECT 1;';
END;
$$ LANGUAGE plpgsql
SET lc_numeric TO 'C';
-- Create temp table to create temp schema
CREATE TEMP TABLE just_a_temp_table (a int);
-- Temp schema should be created for each segment
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Save lc_numeric GUC to gp_guc_restore_list
SELECT set_conf_param();
-- Trigger lc_numeric GUC restore from gp_guc_restore_list
SELECT 1;
-- When lc_numeric GUC is restored from gp_guc_restore_list
-- successfully no RemoveTempRelationsCallback is called.
-- So check that segments still have temp schemas
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Cleanup
DROP TABLE just_a_temp_table;

-- Test single query pljava_classpath GUC rollback
-- Function just to save pljava_classpath GUC to gp_guc_restore_list
CREATE OR REPLACE FUNCTION set_conf_param() RETURNS VOID
AS $$
BEGIN
    EXECUTE 'SELECT 1;';
END;
$$ LANGUAGE plpgsql
SET pljava_classpath TO '';
-- Create temp table to create temp schema
CREATE TEMP TABLE just_a_temp_table (a int);
-- Temp schema should be created for each segment
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Save pljava_classpath GUC to gp_guc_restore_list
SELECT set_conf_param();
-- Trigger pljava_classpath GUC restore from gp_guc_restore_list
SELECT 1;
-- When pljava_classpath GUC is restored from gp_guc_restore_list
-- successfully no RemoveTempRelationsCallback is called.
-- So check that segments still have temp schemas
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Cleanup
DROP TABLE just_a_temp_table;

-- Test single query pljava_vmoptions GUC rollback
-- Function just to save pljava_vmoptions GUC to gp_guc_restore_list
CREATE OR REPLACE FUNCTION set_conf_param() RETURNS VOID
AS $$
BEGIN
    EXECUTE 'SELECT 1;';
END;
$$ LANGUAGE plpgsql
SET pljava_vmoptions TO '';
-- Create temp table to create temp schema
CREATE TEMP TABLE just_a_temp_table (a int);
-- Temp schema should be created for each segment
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Save pljava_vmoptions GUC to gp_guc_restore_list
SELECT set_conf_param();
-- Trigger pljava_vmoptions GUC restore from gp_guc_restore_list
SELECT 1;
-- When pljava_vmoptions GUC is restored from gp_guc_restore_list
-- successfully no RemoveTempRelationsCallback is called.
-- So check that segments still have temp schemas
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Cleanup
DROP TABLE just_a_temp_table;

-- Test single query GUC TimeZone rollback
-- Set TimeZone to value that has to be quoted due to slash
SET TimeZone TO 'Africa/Mbabane';
-- Function just to save TimeZone to gp_guc_restore_list
CREATE OR REPLACE FUNCTION set_conf_param() RETURNS VOID
AS $$
BEGIN
    EXECUTE 'SELECT 1;';
END;
$$ LANGUAGE plpgsql
SET TimeZone TO 'UTC';
-- Create temp table to create temp schema
CREATE TEMP TABLE just_a_temp_table (a int);
-- Temp schema should be created for each segment
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Save TimeZone GUC to gp_guc_restore_list
SELECT set_conf_param();
-- Trigger TimeZone GUC restore from gp_guc_restore_list
SELECT 1;
-- When TimeZone GUC is restored from gp_guc_restore_list
-- successfully no RemoveTempRelationsCallback is called.
-- So check that segments still have temp schemas
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Cleanup
DROP TABLE just_a_temp_table;

-- Test single query search_path GUC rollback
-- Add empty value to search_path that caused issues before
-- to verify that rollback it it will be successful.
SET search_path TO public, '';
-- Function just to save default_tablespace GUC to gp_guc_restore_list
CREATE OR REPLACE FUNCTION set_conf_param() RETURNS VOID
AS $$
BEGIN
    EXECUTE 'SELECT 1;';
END;
$$ LANGUAGE plpgsql
SET search_path TO "public";

-- Create temp table to create temp schema
CREATE TEMP TABLE just_a_temp_table (a int);
-- Temp schema should be created for each segment
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Save default_tablespace GUC to gp_guc_restore_list
SELECT set_conf_param();
-- Trigger default_tablespace GUC restore from gp_guc_restore_list
SELECT 1;

-- When search_path GUC is restored from gp_guc_restore_list
-- successfully no RemoveTempRelationsCallback is called.
-- So check that segments still have temp schemas
SELECT count(nspname) FROM gp_dist_random('pg_namespace') WHERE nspname LIKE 'pg_temp%';
-- Cleanup
DROP TABLE just_a_temp_table;
RESET search_path;

-- Try to set statement_mem > max_statement_mem
SET statement_mem = '4000MB';
RESET statement_mem;
