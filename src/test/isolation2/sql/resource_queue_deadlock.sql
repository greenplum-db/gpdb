-- This test is used to test if waiting on a resource queue lock will
-- trigger a local deadlock detection.

0: CREATE RESOURCE QUEUE rq_deadlock_test WITH (active_statements = 1);
0: CREATE role role_deadlock_test RESOURCE QUEUE rq_deadlock_test;

0: SELECT gp_inject_fault_infinite('before_auto_stats', 'suspend', dbid) FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
0&: SELECT gp_wait_until_triggered_fault('before_auto_stats', 1, dbid) FROM gp_segment_configuration WHERE content = -1 AND role = 'p';

1: SET role role_deadlock_test;
-- We need gp_autostats_mode to be ON_NO_STATS in this test.
1: SET gp_autostats_mode = ON_NO_STATS;
1: SHOW gp_autostats_mode;
1: CREATE TABLE t_deadlock_test(c1 int);
1&: INSERT INTO t_deadlock_test VALUES (1);
2: SET role role_deadlock_test;
2: BEGIN;
2: analyze t_deadlock_test;
0<:
0: select gp_inject_fault_infinite('before_auto_stats', 'reset', dbid) from gp_segment_configuration where content = -1 and role = 'p';
2: SELECT * FROM t_deadlock_test;
2: ROLLBACK;
1<:

-- Sanity check: Ensure that the resource queue is now empty.
0: SELECT rsqcountlimit, rsqcountvalue from pg_resqueue_status WHERE rsqname = 'rq_deadlock_test';

-- Clean up the test
0: DROP TABLE t_deadlock_test;
0: DROP ROLE role_deadlock_test;
0: DROP RESOURCE QUEUE rq_deadlock_test;
