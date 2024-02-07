-- Here we ensure that we clean up resource queue in-memory state gracefully
-- in the face of deadlocks and statement cancellations, when there is more than
-- one active portal in the session.
0:CREATE RESOURCE QUEUE rq_multi_portal WITH (active_statements = 2);
0:CREATE ROLE role_multi_portal RESOURCE QUEUE rq_multi_portal;

1:SET ROLE role_multi_portal;
2:SET ROLE role_multi_portal;

--
-- Scenario 1:
-- Multiple explicit cursors active in the same session with a deadlock.
--

1:BEGIN;
1:DECLARE c1 CURSOR FOR SELECT 1;

2:BEGIN;
2:DECLARE c2 CURSOR FOR SELECT 1;

-- There should be 2 active statements.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';

-- This should block as it will exceed the active statements limit.
1&:DECLARE c3 CURSOR FOR SELECT 1;

-- There should be 2 active statements.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';

-- This should cause a deadlock.
2:DECLARE c4 CURSOR FOR SELECT 1;

-- After the deadlock report, one session should have ERRORed out with the
-- deadlock report and aborted, while the other session should remain active
-- and idle in transaction. The active statement count should be 2, contributed
-- to by the session that is idle in transaction.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';
0:SELECT count(*) from pg_stat_activity WHERE query LIKE 'DECLARE c% CURSOR FOR SELECT 1;'
    AND state = 'idle in transaction';
0:SELECT count(*) from pg_stat_activity WHERE query LIKE 'DECLARE c% CURSOR FOR SELECT 1;'
    AND state = 'idle in transaction (aborted)';

-- After ending the transactions, there should be 0 active statements.
1<:
1:END;
2:END;
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';

--
-- Scenario 2:
-- Multiple explicit cursors active in the same session with a self deadlock.
--
1:BEGIN;
1:DECLARE c1 CURSOR FOR SELECT 1;
1:DECLARE c2 CURSOR FOR SELECT 1;

-- There should be 2 active statements.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';

-- This should cause a self-deadlock and session 1 should error out, aborting
-- its transaction.
1:DECLARE c3 CURSOR FOR SELECT 1;

-- There should be 0 active statements following the transaction abort.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';

1:END;

--
-- Scenario 3:
-- Multiple explicit cursors active in the same session with cancellation.
--
1:BEGIN;
1:DECLARE c1 CURSOR FOR SELECT 1;

2:BEGIN;
2:DECLARE c2 CURSOR FOR SELECT 1;

-- This should block as it will exceed the active statements limit.
1&:DECLARE c3 CURSOR FOR SELECT 1;

-- There should be 2 active statements.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';

-- Cancel session 1's transaction.
0:SELECT pg_cancel_backend(pid) FROM pg_stat_activity
    WHERE query = 'DECLARE c3 CURSOR FOR SELECT 1;';

-- There should now only be one active statement, following the abort of session
-- 1's transaction. The active statement is contributed by session 2.
1<:
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';
0:SELECT query, state from pg_stat_activity
  WHERE query = 'DECLARE c2 CURSOR FOR SELECT 1;';

-- After ending the transactions, there should be 0 active statements.
1:END;
2:END;
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_multi_portal';

-- Cleanup
0:DROP ROLE role_multi_portal;
0:DROP RESOURCE QUEUE rq_multi_portal;
