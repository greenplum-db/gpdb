-- Testing when we terminate a query involves entrydb, UnassignResGroup() would
-- be called on both QD and entrydb process. If that function finishes calling
-- on QD before entrydb calling, a bug could previously be triggered that leads
-- to data corruption. This test verifies this.

-- start_ignore
DROP ROLE IF EXISTS role_test;
DROP RESOURCE GROUP rg_test;
-- end_ignore

CREATE RESOURCE GROUP rg_test WITH (concurrency=2, cpu_rate_limit=10, memory_limit=10);
CREATE ROLE role_test RESOURCE GROUP rg_test;

-- By pass this session, else this affects the testing session, i.e. 1:
SET gp_resource_group_bypass = true;

1: SET ROLE role_test;

SELECT gp_inject_fault('unassign_resgroup_end_qd', 'reset', 1);
SELECT gp_inject_fault('unassign_resgroup_start_entrydb', 'reset', 1);

SELECT gp_inject_fault('unassign_resgroup_end_qd', 'suspend', 1);
SELECT gp_inject_fault('unassign_resgroup_start_entrydb', 'suspend', 1);

-- A query invovles entrydb.
1&: SELECT * from gp_dist_random('gp_id'), pg_sleep(15);

SELECT pg_terminate_backend(pid)
  FROM pg_stat_activity WHERE query LIKE
  'SELECT * from gp_dist_random%'
  ORDER BY xact_start LIMIT 1;

-- Make sure entry db starts calling UnassignResGroup() after QD
-- finishes calling UnassignResGroup().
SELECT gp_wait_until_triggered_fault('unassign_resgroup_end_qd', 1, 1);
SELECT gp_inject_fault('unassign_resgroup_start_entrydb', 'resume', 1);

SELECT gp_inject_fault('unassign_resgroup_end_qd', 'reset', 1);
SELECT gp_inject_fault('unassign_resgroup_start_entrydb', 'reset', 1);

1<:

-- Verify the system is ok with entrydb.
2: SET ROLE role_test;
2: SELECT * from gp_dist_random('gp_id'), pg_sleep(0.1) LIMIT 1;
2q:

-- Clean up
DROP ROLE role_test;
DROP RESOURCE GROUP rg_test;
