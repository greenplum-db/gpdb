-- test memory limit
-- start_ignore
DROP ROLE IF EXISTS role_memory_test;
DROP RESOURCE GROUP rg_memory_test;
DROP TABLE t_memory_limit;
-- end_ignore

-- create a pl function to show the memory used by a process
CREATE OR REPLACE FUNCTION func_memory_test (text) RETURNS text as /*in func*/
$$ /*in func*/
DECLARE /*in func*/
        ln text; /*in func*/
        tmp text[]; /*in func*/
	match bool := false; /*in func*/
BEGIN /*in func*/
        FOR ln IN execute format('explain analyze %s', $1) LOOP /*in func*/
                IF NOT match THEN      /*in func*/
			tmp := regexp_match(ln, 'Memory used:  (.*)'); /*in func*/
			IF tmp IS NOT null THEN /*in func*/
				match := true; /*in func*/
			END IF; /*in func*/
		END IF; /*in func*/
        END LOOP; /*in func*/
	RETURN tmp[1]; /*in func*/
END; /*in func*/
$$ /*in func*/
LANGUAGE plpgsql;

-- create a resource group with memory limit 100 Mb
CREATE RESOURCE GROUP rg_memory_test WITH(memory_limit=100, cpu_hard_quota_limit=20, concurrency=2);
CREATE ROLE role_memory_test RESOURCE GROUP rg_memory_test;

-- session1: explain memory used by query
1: SET ROLE TO role_memory_test;
1: CREATE TABLE t_memory_limit(a int);
1: BEGIN;
1: SELECT func_memory_test('SELECT * FROM t_memory_limit');

-- session2: test alter resource group's memory limit
2:ALTER RESOURCE GROUP rg_memory_test SET memory_limit 200;

-- memory used will grow up to 100 Mb
1: SELECT func_memory_test('SELECT * FROM t_memory_limit');
1: END;
-- set gp_resgroup_memory_query_fixed_mem to 200MB
1: SET gp_resgroup_memory_query_fixed_mem to 204800;
1: SELECT func_memory_test('SELECT * FROM t_memory_limit');
1: RESET ROLE;
-- clean
DROP FUNCTION func_memory_test(text);
DROP TABLE t_memory_limit;
DROP ROLE IF EXISTS role_memory_test;
DROP RESOURCE GROUP rg_memory_test;
