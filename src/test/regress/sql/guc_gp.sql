SELECT min_val, max_val FROM pg_settings WHERE name = 'gp_resqueue_priority_cpucores_per_segment';

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
