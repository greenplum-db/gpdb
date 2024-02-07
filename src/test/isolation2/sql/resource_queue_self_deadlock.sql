-- This test is used to test if waiting on a resource queue lock will trigger a
-- self deadlock detection and if we will properly clean up (preventing nasty
-- race conditions)

0: CREATE RESOURCE QUEUE rq_self WITH (active_statements = 1);
0: CREATE ROLE r_self RESOURCE QUEUE rq_self;

1: SET ROLE r_self;
1: BEGIN;
1: DECLARE c1 CURSOR FOR SELECT 1;

0: SELECT gp_inject_fault_infinite('res_lock_acquire_self_deadlock_error', 'suspend', dbid) FROM
    gp_segment_configuration WHERE content = -1 AND role = 'p';
0&: SELECT gp_wait_until_triggered_fault('res_lock_acquire_self_deadlock_error', 1, dbid)
    FROM gp_segment_configuration WHERE content = -1 AND role = 'p';
-- Will trip a self-deadlock and the backend will be suspended.
1&: DECLARE c2 CURSOR FOR SELECT 1;

2: SET ROLE r_self;
-- Will wait for a slot in the queue.
2&: SELECT 33176;

-- Now inflate the active statement count for the queue.
0<:
0: ALTER RESOURCE QUEUE rq_self WITH (active_statements = 5);

-- Cancellation of the backend should not lead to us doing an extra grant to the
-- process undergoing self-deadlock. Doing so, leads to an assertion failure:
-- FailedAssertion("!(lock->nGranted <= lock->nRequested)", File: "resqueue.c", Line: 1086)
-- ResGrantLock resqueue.c:1086
-- ResProcLockRemoveSelfAndWakeup resqueue.c:1361
-- ResCleanUpLock resqueue.c:1215
-- ResRemoveFromWaitQueue resqueue.c:1473
-- ResLockWaitCancel proc.c:2102
-- ResLockPortal resscheduler.c:695
-- ...
3: SELECT pg_cancel_backend(pid) FROM pg_stat_activity WHERE query='SELECT 33176;';
2<:

0: SELECT gp_inject_fault('res_lock_acquire_self_deadlock_error', 'reset', dbid) FROM
    gp_segment_configuration WHERE content = -1 AND role = 'p';

1<:

-- Sanity check: Ensure that the resource queue is now empty.
0: SELECT rsqcountlimit, rsqcountvalue from pg_resqueue_status WHERE rsqname = 'rq_self';

-- Clean up the test
0: DROP ROLE r_self;
0: DROP RESOURCE QUEUE rq_self;
