-- This test is used to test if waiting on a resource queue lock will
-- trigger a local deadlock detection and properly clean up.

0: CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- We need gp_autostats_mode to be ON_NO_STATS in this test.
0: SHOW gp_autostats_mode;

-- Create a resource queue where only 1 query can run at a time and
-- new queries must wait. The role attached to the resource queue must
-- be a nonsuperuser.
0: CREATE RESOURCE QUEUE rq_deadlock_test WITH (active_statements = 1);
0: CREATE ROLE role_deadlock_test RESOURCE QUEUE rq_deadlock_test;

-- Inject suspend fault right before auto_stats() starts doing its thing
0: SELECT gp_inject_fault_new('before_auto_stats', 'suspend', dbid) FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
0&: SELECT gp_wait_until_triggered_fault('before_auto_stats', 1, dbid) FROM gp_segment_configuration WHERE content = -1 AND role = 'p';

-- The INSERT will block when it hits the injected suspend fault. The
-- INSERT will also hold the ExclusiveLock on the resource queue while
-- blocking.
1: SET ROLE role_deadlock_test;
1: CREATE TABLE t_deadlock_test(c1 int);
1&: INSERT INTO t_deadlock_test VALUES (1);

-- The ANALYZE will acquire ShareUpdateExclusiveLock on
-- the t_deadlock_test table.
2: SET ROLE role_deadlock_test;
2: BEGIN;
2: ANALYZE t_deadlock_test;

-- Unblock session 1's INSERT which will move forward with the
-- autostats logic. Session 1 will try to acquire
-- ShareUpdateExclusiveLock on the t_deadlock_test table but will
-- block waiting for session 2 to release the lock.
0<:
0: SELECT gp_inject_fault_new('before_auto_stats', 'reset', dbid) FROM gp_segment_configuration WHERE content = -1 AND role = 'p';

-- The SELECT will try to acquire ExclusiveLock on the resource
-- queue. This will cause a deadlock because session 1 is waiting for
-- session 2's ShareUpdateExclusiveLock while session 2 will now be
-- waiting for session 1's ExclusiveLock. Deadlock detection will
-- catch the issue and error out session 2.
2: SELECT * FROM t_deadlock_test;

-- Finish up the two sessions.
2: ROLLBACK;
1<:

-- Clean up the test
0: DROP TABLE t_deadlock_test;
0: DROP ROLE role_deadlock_test;
0: DROP RESOURCE QUEUE rq_deadlock_test;
