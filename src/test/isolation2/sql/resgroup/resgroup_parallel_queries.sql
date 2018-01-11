-- start_matchsubs
-- m/ERROR:  tuple concurrently updated \(heapam\.c\:\d+\)/
-- s/\(heapam\.c:\d+\)//
-- end_matchsubs
-- start_ignore
! psql -d isolation2resgrouptest -f ./sql/resgroup/dblink.sql;
-- end_ignore

-- This function execute commands N times. 
-- % in command will be replaced by number specified by range1 sequentially
-- # in command will be replaced by number specified by range2 randomly 
-- range, eg: 1-10 
-- Notice: now it only support SELECT statement return single integer
CREATE or replace FUNCTION exec_commands_n /*in func*/
	(dl_name text, command1 text, /*in func*/
	command2 text, command3 text, /*in func*/
	times integer, range1 text, range2 text, fail_on_error bool) /*in func*/
RETURNS integer AS $$ /*in func*/
DECLARE /*in func*/
	cmd text; /*in func*/
	res int; /*in func*/
	s_r1 int; /*in func*/
	e_r1 int; /*in func*/
	s_r2 int; /*in func*/
	e_r2 int; /*in func*/
BEGIN /*in func*/
	s_r1 = 0; /*in func*/
	e_r1 = 0; /*in func*/
	s_r2 = 0; /*in func*/
	e_r2 = 0; /*in func*/
	IF length(range1) > 0 THEN /*in func*/
		select t[1]::int, t[2]::int into s_r1, e_r1 from regexp_split_to_array(range1, '-') t; /*in func*/
	END IF; /*in func*/
	IF length(range2) > 0 THEN /*in func*/
		select t[1]::int, t[2]::int into s_r2, e_r2 from regexp_split_to_array(range2, '-') t; /*in func*/
	END IF; /*in func*/
	FOR i IN 0..(times - 1) LOOP /*in func*/
		IF length(command1) > 0 THEN /*in func*/
			cmd = regexp_replace(command1, '%', (s_r1 + i % (e_r1 - s_r1 + 1))::text, 'g'); /*in func*/
			cmd = regexp_replace(cmd, '#', (s_r2 + ((random()*100)::int) % (e_r2 - s_r2 + 1))::text, 'g'); /*in func*/
			RAISE NOTICE '%', cmd; /*in func*/
			IF lower(cmd) like 'select %' THEN /*in func*/
				select * into res from dblink(dl_name, cmd, fail_on_error) t(c1 integer); /*in func*/
			ELSE /*in func*/
				perform dblink_exec(dl_name, cmd , fail_on_error); /*in func*/
			END IF; /*in func*/
		END IF; /*in func*/
		IF length(command2) > 0 THEN /*in func*/
			cmd = regexp_replace(command2, '%', (s_r1 + i % (e_r1 - s_r1 + 1))::text, 'g'); /*in func*/
			cmd = regexp_replace(cmd, '#', (s_r2 + ((random()*100)::int) % (e_r2 - s_r2 + 1))::text, 'g'); /*in func*/
			RAISE NOTICE '%', cmd; /*in func*/
			IF lower(cmd) like 'select %' THEN /*in func*/
				select * into res from dblink(dl_name, cmd, fail_on_error) t(c1 integer); /*in func*/
			ELSE /*in func*/
				perform dblink_exec(dl_name, cmd, fail_on_error); /*in func*/
			END IF; /*in func*/
		END IF; /*in func*/
		IF length(command3) > 0 THEN /*in func*/
			cmd = regexp_replace(command3, '%', (s_r1 + i % (e_r1 - s_r1 + 1))::text, 'g'); /*in func*/
			cmd = regexp_replace(cmd, '#', (s_r2 + ((random()*100)::int) % (e_r2 - s_r2 + 1))::text, 'g'); /*in func*/
			RAISE NOTICE '%', cmd; /*in func*/
			IF lower(cmd) like 'select %' THEN /*in func*/
				select * into res from dblink(dl_name, cmd, fail_on_error) t(c1 integer); /*in func*/
			ELSE /*in func*/
				perform dblink_exec(dl_name, cmd, fail_on_error); /*in func*/
			END IF; /*in func*/
		END IF; /*in func*/
	END LOOP; /*in func*/
	return times; /*in func*/
END;$$ /*in func*/
LANGUAGE 'plpgsql';

--
-- DDLs vs DDLs
--
1:select dblink_connect('dblink_rg_test1', 'dbname=isolation2resgrouptest');
2:select dblink_connect('dblink_rg_test2', 'dbname=isolation2resgrouptest');
3:select dblink_connect('dblink_rg_test3', 'dbname=isolation2resgrouptest');
4:select dblink_connect('dblink_rg_test4', 'dbname=isolation2resgrouptest');
5:select dblink_connect('dblink_rg_test5', 'dbname=isolation2resgrouptest');
6:select dblink_connect('dblink_rg_test6', 'dbname=isolation2resgrouptest');

1>:select exec_commands_n('dblink_rg_test1','CREATE RESOURCE GROUP rg_test_g# WITH (concurrency=#, cpu_rate_limit=#, memory_limit=#)', 'DROP RESOURCE GROUP rg_test_g#', 'ALTER RESOURCE GROUP rg_test_g# set concurrency #', 60, '', '1-6', false);
2>:select exec_commands_n('dblink_rg_test2','CREATE RESOURCE GROUP rg_test_g# WITH (concurrency=#, cpu_rate_limit=#, memory_limit=#)', 'DROP RESOURCE GROUP rg_test_g#', 'ALTER RESOURCE GROUP rg_test_g# set concurrency#', 60, '', '1-6', false);
3>:select exec_commands_n('dblink_rg_test3','CREATE RESOURCE GROUP rg_test_g# WITH (concurrency=#, cpu_rate_limit=#, memory_limit=#)', 'DROP RESOURCE GROUP rg_test_g#', 'ALTER RESOURCE GROUP rg_test_g# set cpu_rate_limit #', 60, '', '1-6', false);
4>:select exec_commands_n('dblink_rg_test4','CREATE RESOURCE GROUP rg_test_g# WITH (concurrency=#, cpu_rate_limit=#, memory_limit=#)', 'DROP RESOURCE GROUP rg_test_g#', 'ALTER RESOURCE GROUP rg_test_g# set memory_limit #', 60, '', '1-6', false);
5>:select exec_commands_n('dblink_rg_test5','CREATE RESOURCE GROUP rg_test_g# WITH (concurrency=#, cpu_rate_limit=#, memory_limit=#)', 'DROP RESOURCE GROUP rg_test_g#', 'ALTER RESOURCE GROUP rg_test_g# set memory_shared_quota #', 60, '', '1-6', false);
6>:select exec_commands_n('dblink_rg_test6','CREATE RESOURCE GROUP rg_test_g# WITH (concurrency=#, cpu_rate_limit=#, memory_limit=#)', 'DROP RESOURCE GROUP rg_test_g#', 'ALTER RESOURCE GROUP rg_test_g# set memory_limit #', 60, '', '1-6', false);

1<:
2<:
3<:
4<:
5<:
6<:

1: select dblink_disconnect('dblink_rg_test1');
2: select dblink_disconnect('dblink_rg_test2');
3: select dblink_disconnect('dblink_rg_test3');
4: select dblink_disconnect('dblink_rg_test4');
5: select dblink_disconnect('dblink_rg_test5');
6: select dblink_disconnect('dblink_rg_test6');


1q:
2q:
3q:
4q:
5q:
6q:
--
-- DDLs vs DMLs
--
-- Prepare resource groups and roles and tables
create table rg_test_foo as select i as c1, i as c2 from generate_series(1,1000) i;
create table rg_test_bar as select i as c1, i as c2 from generate_series(1,1000) i;
grant all on rg_test_foo to public;
grant all on rg_test_bar to public;

-- start_ignore
select dblink_connect('dblink_rg_test', 'dbname=isolation2resgrouptest');
select exec_commands_n('dblink_rg_test','DROP ROLE rg_test_r%', '', '', 7, '1-7', '', false);
select exec_commands_n('dblink_rg_test','DROP RESOURCE GROUP rg_test_g%', '', '', 7, '1-7', '', false);
-- end_ignore

-- create 6 roles and 6 resource groups
select exec_commands_n('dblink_rg_test','CREATE RESOURCE GROUP rg_test_g% WITH (concurrency=9, cpu_rate_limit=1, memory_limit=7)', '', '', 6, '1-6', '', true);
select exec_commands_n('dblink_rg_test','CREATE ROLE rg_test_r% login resource group rg_test_g%;', '', '', 6, '1-6', '', true);
select exec_commands_n('dblink_rg_test','GRANT ALL ON rg_test_foo to rg_test_r%;', '', '', 6, '1-6', '',  true);
select exec_commands_n('dblink_rg_test','GRANT ALL ON rg_test_bar to rg_test_r%;', '', '', 6, '1-6', '', true);

select dblink_disconnect('dblink_rg_test');

select groupname, concurrency, cpu_rate_limit from gp_toolkit.gp_resgroup_config where groupname like 'rg_test_g%' order by groupname;

--
-- 2* : DMLs
--
-- start 6 session to concurrently change resource group and run simple queries randomly
-- BEGIN/END
21: select dblink_connect('dblink_rg_test21', 'dbname=isolation2resgrouptest');
21>: select exec_commands_n('dblink_rg_test21', 'set role rg_test_r#', 'BEGIN', 'END', 24000, '', '1-6', true);
-- BEGIN/ABORT
22: select dblink_connect('dblink_rg_test22', 'dbname=isolation2resgrouptest');
22>: select exec_commands_n('dblink_rg_test22', 'set role rg_test_r#', 'BEGIN', 'ABORT', 24000, '', '1-6', true);
-- query with memory sensitive node
23: select dblink_connect('dblink_rg_test23', 'dbname=isolation2resgrouptest');
23>: select exec_commands_n('dblink_rg_test23', 'set role rg_test_r#', 'insert into rg_test_foo values (#, #)', 'select count(*) from rg_test_bar t1, rg_test_foo t2 where t1.c2=t2.c2 group by t1.c2', 3000, '', '1-6', true);
-- high cpu
24: select dblink_connect('dblink_rg_test24', 'dbname=isolation2resgrouptest');
24>: select exec_commands_n('dblink_rg_test24', 'set role rg_test_r#', 'insert into rg_test_bar values (#, #)', 'select count(*) from rg_test_bar where c2! = 1000', 60, '', '1-6', true);
-- simple select
25: select dblink_connect('dblink_rg_test25', 'dbname=isolation2resgrouptest');
25>: select exec_commands_n('dblink_rg_test25', 'set role rg_test_r#', 'select count(*) from rg_test_foo', 'select count(*) from rg_test_bar', 6000, '', '1-6', true);
-- vacuum
26: select dblink_connect('dblink_rg_test26', 'dbname=isolation2resgrouptest');
26>: select exec_commands_n('dblink_rg_test26', 'set role rg_test_r#', 'vacuum rg_test_bar', 'vacuum rg_test_foo', 6000, '', '1-6', true);

--
-- 3* : Alter groups
--
-- start a new session to alter concurrency randomly
31: select dblink_connect('dblink_rg_test31', 'dbname=isolation2resgrouptest');
31>: select exec_commands_n('dblink_rg_test31', 'alter resource group rg_test_g% set concurrency #', 'select 1 from pg_sleep(0.1)', '', 1000, '1-6', '0-5', true);

-- start a new session to alter cpu_rate_limit randomly
32: select dblink_connect('dblink_rg_test32', 'dbname=isolation2resgrouptest');
32>: select exec_commands_n('dblink_rg_test32', 'alter resource group rg_test_g% set cpu_rate_limit #', 'select 1 from pg_sleep(0.1)', '', 1000, '1-6', '1-6', true);

-- start a new session to alter memory_limit randomly
33: select dblink_connect('dblink_rg_test33', 'dbname=isolation2resgrouptest');
33>: select exec_commands_n('dblink_rg_test33', 'alter resource group rg_test_g% set memory_limit #', 'select 1 from pg_sleep(0.1)', '', 1000, '1-6', '1-7', true);

-- start a new session to alter memory_shared_quota randomly
34: select dblink_connect('dblink_rg_test34', 'dbname=isolation2resgrouptest');
34>: select exec_commands_n('dblink_rg_test34', 'alter resource group rg_test_g% set memory_shared_quota #', 'select 1 from pg_sleep(0.1)', '', 1000, '1-6', '1-80', true);

--
-- 4* : CREATE/DROP tables & groups
--
-- start a new session to create and drop table, it will cause massive catchup interrupt.
41: select dblink_connect('dblink_rg_test41', 'dbname=isolation2resgrouptest');
41>: select exec_commands_n('dblink_rg_test41', 'drop table if exists rg_test_t%', 'create table rg_test_t% (c1 int, c2 int)' ,'', 3000, '1-6', '', true);

-- start a new session to create & drop resource group 
42: select dblink_connect('dblink_rg_test42', 'dbname=isolation2resgrouptest');
42>: select exec_commands_n('dblink_rg_test42', 'create resource group rg_test_g7 with (cpu_rate_limit=1, memory_limit=1)', 'drop resource group rg_test_g7', '', 1000, '', '', true);

31<:
31: select exec_commands_n('dblink_rg_test31', 'alter resource group rg_test_g% set concurrency #', 'select 1 from pg_sleep(0.1)', '', 6, '1-6', '1-5', true);

-- start a new session to acquire the status of resource groups
44: select dblink_connect('dblink_rg_test44', 'dbname=isolation2resgrouptest');
44>: select exec_commands_n('dblink_rg_test44', 'select count(*) from gp_toolkit.gp_resgroup_status;', '', '', 100, '', '', true);

-- wait all sessions to finish
21<:
22<:
23<:
24<:
25<:
26<:
32<:
33<:
34<:
41<:
42<:
44<:

21: select dblink_disconnect('dblink_rg_test21');
22: select dblink_disconnect('dblink_rg_test22');
23: select dblink_disconnect('dblink_rg_test23');
24: select dblink_disconnect('dblink_rg_test24');
25: select dblink_disconnect('dblink_rg_test25');
26: select dblink_disconnect('dblink_rg_test26');
31: select dblink_disconnect('dblink_rg_test31');
32: select dblink_disconnect('dblink_rg_test32');
33: select dblink_disconnect('dblink_rg_test33');
34: select dblink_disconnect('dblink_rg_test34');
41: select dblink_disconnect('dblink_rg_test41');
42: select dblink_disconnect('dblink_rg_test42');
44: select dblink_disconnect('dblink_rg_test44');

21q:
22q:
23q:
24q:
25q:
26q:
31q:
32q:
33q:
34q:
41q:
42q:

select groupname, concurrency::int < 7, cpu_rate_limit::int < 7 from gp_toolkit.gp_resgroup_config where groupname like 'rg_test_g%' order by groupname;

-- Beacuse concurrency of each resource group is changed between 1..6, so the num_queued must be larger than 0
select num_queued > 0 from gp_toolkit.gp_resgroup_status where rsgname like 'rg_test_g%' order by rsgname;

-- After all queries finished in each resource group, the memory_usage should be zero, no memory leak
with t_1 as
(
	select rsgname, row_to_json(json_each(memory_usage::json)) as j from gp_toolkit.gp_resgroup_status where rsgname like 'rg_test_g%' order by rsgname
)
select rsgname, sum(((j->'value')->>'used')::int) from t_1 group by rsgname ;

-- start_ignore
drop table rg_test_foo;
drop table rg_test_bar;
select dblink_connect('dblink_rg_test', 'dbname=isolation2resgrouptest');
select exec_commands_n('dblink_rg_test','DROP ROLE rg_test_r%', '', '', 6, '1-6', '', true);
select exec_commands_n('dblink_rg_test','DROP RESOURCE GROUP rg_test_g%', '', '', 6, '1-6', '', true);
select dblink_disconnect('dblink_rg_test');
-- end_ignore

--
-- 5*: Test connections in utility mode are not governed by resource group
--
create resource group rg_test_g8 with (concurrency= 1, cpu_rate_limit=1, memory_limit=1);
create role rg_test_r8 login resource group rg_test_g8;
51:select dblink_connect('dblink_rg_test51', 'dbname=isolation2resgrouptest user=rg_test_r8 options=''-c gp_session_role=utility''');
52:select dblink_connect('dblink_rg_test52', 'dbname=isolation2resgrouptest user=rg_test_r8 options=''-c gp_session_role=utility''');
53:select dblink_connect('dblink_rg_test53', 'dbname=isolation2resgrouptest user=rg_test_r8 options=''-c gp_session_role=utility''');

51>:select exec_commands_n('dblink_rg_test51', 'select 1', 'begin', 'end', 100, '', '', true);
51<:
52>:select exec_commands_n('dblink_rg_test52', 'select 1', 'select 1', 'select 1', 100, '', '', true);
52<:
53>:select exec_commands_n('dblink_rg_test53', 'select 1', 'begin', 'abort', 100, '', '', true);
53<:

51: select dblink_disconnect('dblink_rg_test51');
52: select dblink_disconnect('dblink_rg_test52');
53: select dblink_disconnect('dblink_rg_test53');

51q:
52q:
53q:

-- num_executed and num_queued must be zero
select num_queued, num_executed from gp_toolkit.gp_resgroup_status where rsgname = 'rg_test_g8';
drop role rg_test_r8;
drop resource group rg_test_g8;

-- clean up
select * from gp_toolkit.gp_resgroup_config;
