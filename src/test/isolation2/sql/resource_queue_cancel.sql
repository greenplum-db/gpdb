-- Simple test for cancellation of a query stuck on a resource queue when the
-- active statements limit is reached.

0:CREATE RESOURCE QUEUE rq_cancel WITH (active_statements = 1);
0:CREATE ROLE role_cancel RESOURCE QUEUE rq_cancel;

-- Consume an active statement in session 1.
1:SET ROLE role_cancel;
1:BEGIN;
1:DECLARE c CURSOR FOR SELECT 0;

-- Make session 2 wait on the resource queue lock.
2:SET ROLE role_cancel;
2&:SELECT 100;

-- Cancel SELECT from session 2.
0:SELECT pg_cancel_backend(pid) FROM pg_stat_activity
  WHERE query='SELECT 100;';

-- Now once we end session 1's transaction, we should be able to consume the
-- vacated active statement slot in session 2.
1:END;

2<:
2:END;
2:BEGIN;
2:DECLARE c CURSOR FOR SELECT 0;

2:END;

-- Sanity check: Ensure that the resource queue is now empty.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_cancel';

-- Cleanup
0:DROP ROLE role_cancel;
0:DROP RESOURCE QUEUE rq_cancel;
