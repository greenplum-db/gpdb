0:CREATE RESOURCE QUEUE rq_concurrency_test WITH (active_statements = 1);
0:CREATE role role_concurrency_test RESOURCE QUEUE rq_concurrency_test;

1:SET role role_concurrency_test;
1:BEGIN;
1:DECLARE c1 CURSOR FOR SELECT 1;

2:SET role role_concurrency_test;
2:PREPARE fooplan AS SELECT 1;
2&:EXECUTE fooplan;

-- EXECUTE statement(cached plan) will be blocked when the concurrency limit of the resource queue is reached.
0:SELECT rsqcountvalue FROM gp_toolkit.gp_resqueue_status WHERE rsqname='rq_concurrency_test';
0:SELECT waiting_reason from pg_stat_activity where query = 'EXECUTE fooplan;';
0:SELECT granted, locktype, mode FROM pg_locks where locktype = 'resource queue' and pid != pg_backend_pid();

1:END;

2<:
2:END;

-- Sanity check: Ensure that all locks were released.
0:SELECT granted, locktype, mode FROM pg_locks where locktype = 'resource queue' and pid != pg_backend_pid();

-- Sanity check: Ensure that the resource queue is now empty.
0: SELECT rsqcountlimit, rsqcountvalue from pg_resqueue_status WHERE rsqname = 'rq_concurrency_test';

-- Introduce a holdable cursor.
3:SET role role_concurrency_test;
3:DECLARE c_hold CURSOR WITH HOLD FOR SELECT 1;

-- Sanity check: The holdable cursor should be accounted for in pg_locks.
0:SELECT granted, locktype, mode FROM pg_locks where locktype = 'resource queue' and pid != pg_backend_pid();

3:CLOSE c_hold;

-- Sanity check: Ensure that all locks were released.
0:SELECT granted, locktype, mode FROM pg_locks where locktype = 'resource queue' and pid != pg_backend_pid();

-- Sanity check: Ensure that the resource queue is now empty.
0: SELECT rsqcountlimit, rsqcountvalue from pg_resqueue_status WHERE rsqname = 'rq_concurrency_test';

0:DROP role role_concurrency_test;
0:DROP RESOURCE QUEUE rq_concurrency_test;
