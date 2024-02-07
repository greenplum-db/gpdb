-- Test various scenarios with respect to backend termination.

0:CREATE RESOURCE QUEUE rq_terminate WITH (active_statements = 1);
0:CREATE ROLE role_terminate RESOURCE QUEUE rq_terminate;

--
-- Scenario 1: Terminate a backend with a regular open cursor
--
1:SET ROLE role_terminate;
1:BEGIN;
1:DECLARE cs1 CURSOR FOR SELECT 0;

0:SELECT pg_terminate_backend(pid) FROM pg_stat_activity
  WHERE query='DECLARE cs1 CURSOR FOR SELECT 0;';

1<:
-- Sanity check: Ensure that the resource queue is now empty.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_terminate';

--
-- Scenario 2: Terminate a backend with a holdable open cursor that has been
-- persisted.
--
2:SET ROLE role_terminate;
2:DECLARE cs2 CURSOR WITH HOLD FOR SELECT 0;

0:SELECT pg_terminate_backend(pid) FROM pg_stat_activity
  WHERE query='DECLARE cs2 CURSOR WITH HOLD FOR SELECT 0;';

2<:
-- Sanity check: Ensure that the resource queue is now empty.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_terminate';

--
-- Scenario 3: Terminate a backend with a waiting statement
--
3:SET ROLE role_terminate;
3:BEGIN;
3:DECLARE cs3 CURSOR FOR SELECT 0;
4:SET ROLE role_terminate;
4&:SELECT 331763;

0:SELECT pg_terminate_backend(pid) FROM pg_stat_activity
  WHERE query='SELECT 331763;';

4<:
3:END;
-- Sanity check: Ensure that the resource queue is now empty.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_terminate';

--
-- Scenario 4: Terminate a backend with a waiting holdable cursor
--
5:SET ROLE role_terminate;
5:BEGIN;
5:DECLARE cs4 CURSOR FOR SELECT 0;
6:SET ROLE role_terminate;
6&:DECLARE cs5 CURSOR WITH HOLD FOR SELECT 0;

0:SELECT pg_terminate_backend(pid) FROM pg_stat_activity
  WHERE query='DECLARE cs5 CURSOR WITH HOLD FOR SELECT 0;';

6<:
5:END;
-- Sanity check: Ensure that the resource queue is now empty.
0:SELECT rsqcountlimit, rsqcountvalue FROM pg_resqueue_status WHERE rsqname = 'rq_terminate';


-- Cleanup
0:DROP ROLE role_terminate;
0:DROP RESOURCE QUEUE rq_terminate;
