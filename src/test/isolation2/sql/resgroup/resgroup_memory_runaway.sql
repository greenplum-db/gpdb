-- start_ignore
DROP ROLE IF EXISTS role1_memory_test;
DROP RESOURCE GROUP rg1_memory_test;
DROP RESOURCE GROUP rg2_memory_test;
-- end_ignore

CREATE OR REPLACE FUNCTION resGroupPalloc(float) RETURNS int AS
'/home/huanzhang/workspace/gpdb/src/test/isolation2/../regress/regress.so', 'resGroupPalloc'
LANGUAGE C READS SQL DATA;

CREATE OR REPLACE FUNCTION hold_memory_by_percent(float) RETURNS int AS $$
	SELECT * FROM resGroupPalloc($1)
$$ LANGUAGE sql;

CREATE OR REPLACE VIEW rg_mem_status AS
	SELECT groupname, memory_limit, memory_shared_quota
	FROM gp_toolkit.gp_resgroup_config
	WHERE groupname='rg1_memory_test' OR groupname='rg2_memory_test'
	ORDER BY groupid;

CREATE OR REPLACE VIEW memory_result AS SELECT rsgname, memory_usage from gp_toolkit.gp_resgroup_status;

-- start_ignore
! gpconfig -c runaway_detector_activation_percent -v 50;
! gpstop -ari;
-- end_ignore

-- after the restart we need a new connection to run the queries
--	1) single allocation
--	Group Share Quota = 0
--	Global Share Quota > 0
--	Slot Quota > 0
--	-----------------------

--	we assume system total chunks is 100%
--	rg1's expected: 100% * 20% => 20%
--	rg1's slot quota: 20% / 2 * 2 => 20%
--	rg1's single slot quota: 20% / 2 => 10%
--	rg1's shared quota: 20% - 20% => %0
--	system free chunks: 100% - 10% - 30% - 20% => 40%
--  global area safe threshold: 40% / 2 = 20%
1: CREATE RESOURCE GROUP rg1_memory_test
    WITH (concurrency=2, cpu_rate_limit=10,
          memory_limit=20, memory_shared_quota=0);
1: CREATE ROLE role1_memory_test RESOURCE GROUP rg1_memory_test;
--	1a) on QD
1: SET ROLE TO role1_memory_test;
1: SELECT hold_memory_by_percent(1.0);
1: SELECT hold_memory_by_percent(0.3);
1: SELECT hold_memory_by_percent(0.3);
1q:

--	1b) on QEs
2: SELECT pg_sleep(1);
2: SET ROLE TO role1_memory_test;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(1.0)=0;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(0.3)=0;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(0.3)=0;
2q:

0: DROP ROLE role1_memory_test;
0: DROP RESOURCE GROUP rg1_memory_test;
0q:


--	we assume system total chunks is 100%
--	rg1's expected: 100% * 20% => 20%
--	rg1's slot quota: 20% / 2  => 10%
--	rg1's single slot quota: 10% / 2 => 5%
--	rg1's shared quota: %20 - %10 => %10
--	system free chunks: 100% - 10% - 30% - 20% => 40%
--  safe threshold: 40% / 2 = 20%
1: CREATE RESOURCE GROUP rg1_memory_test
    WITH (concurrency=2, cpu_rate_limit=10,
          memory_limit=20, memory_shared_quota=50);
1: CREATE ROLE role1_memory_test RESOURCE GROUP rg1_memory_test;
--	1a) on QD
1: SET ROLE TO role1_memory_test;
1: SELECT hold_memory_by_percent(1.0);
1: SELECT hold_memory_by_percent(0.3);
1: SELECT hold_memory_by_percent(0.3);
1: SELECT hold_memory_by_percent(0.3);
1q:

--	1b) on QEs
2: SELECT pg_sleep(1);
2: SET ROLE TO role1_memory_test;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(1.0)=0;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(0.3)=0;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(0.3)=0;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(0.3)=0;
2q:

0: DROP ROLE role1_memory_test;
0: DROP RESOURCE GROUP rg1_memory_test;
0q:



--	we assume system total chunks is 100%
--	rg1's expected: 100% * 20% => 20%
--	rg1's slot quota: 20% / 2  => 10%
--	rg1's single slot quota: 10% / 2 => 5%
--	rg1's shared quota: %20 - %10 => %10
--	rg2's expected: 100% * 20% => 20%
--	system free chunks: 100% - 10% - 30% - 20% - 20%=> 20%
--  safe threshold: 20% / 2 = 10%
1: CREATE RESOURCE GROUP rg1_memory_test
    WITH (concurrency=2, cpu_rate_limit=10,
          memory_limit=20, memory_shared_quota=50);
1: CREATE RESOURCE GROUP rg2_memory_test
    WITH (concurrency=2, cpu_rate_limit=10,
          memory_limit=20, memory_shared_quota=0);
1: CREATE ROLE role1_memory_test RESOURCE GROUP rg1_memory_test;
--	1a) on QD
1: SET ROLE TO role1_memory_test;
1: SELECT hold_memory_by_percent(1.0);
1: SELECT hold_memory_by_percent(0.15);
1: SELECT hold_memory_by_percent(0.15);
1q:

--	1b) on QEs
2: SELECT pg_sleep(1);
2: SET ROLE TO role1_memory_test;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(1.0)=0;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(0.15)=0;
2: SELECT count(null) FROM gp_dist_random('gp_id') t1 WHERE hold_memory_by_percent(0.15)=0;
2q:

0: DROP ROLE role1_memory_test;
0: DROP RESOURCE GROUP rg1_memory_test;
0: DROP RESOURCE GROUP rg2_memory_test;
0q:


-- start_ignore
! gpconfig -c runaway_detector_activation_percent -v 100;
! gpstop -ari;
-- end_ignore
