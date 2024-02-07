-- Behavioral tests to showcase that we always use the current_user to determine
-- which resource queue that the statement should belong to. Also, even though
-- we may use the current_user to route the statement, we may still use the
-- session_user in resource queue views.

0:CREATE RESOURCE QUEUE rq_role_test1 WITH (active_statements = 2);
0:CREATE RESOURCE QUEUE rq_role_test2 WITH (active_statements = 2);
0:CREATE ROLE rq_role_test_role1 RESOURCE QUEUE rq_role_test1;
0:CREATE ROLE rq_role_test_role2 RESOURCE QUEUE rq_role_test2;

--
-- SET ROLE
--

1:SET ROLE rq_role_test_role1;
1:BEGIN;
1:DECLARE c1 CURSOR FOR SELECT 1;
1:SET ROLE rq_role_test_role2;
1:DECLARE c2 CURSOR FOR SELECT 1;

-- We should see 1 lock each on each queue, with 1 holder, with 1 active
-- statement in queue. The lorusename in gp_locks_on_resqueue will be the session_user
-- as opposed to the current_user 'rq_role_test_role1' or 'rq_role_test_role2' (which we
-- use to find the destination queue)
0:SELECT lorusename=session_user, lorrsqname, lorlocktype, lormode, lorgranted
  FROM gp_toolkit.gp_locks_on_resqueue WHERE lorrsqname IN ('rq_role_test1', 'rq_role_test2');
0:SELECT rsqname, rsqcountlimit, rsqcountvalue, rsqwaiters, rsqholders
  FROM gp_toolkit.gp_resqueue_status WHERE rsqname IN ('rq_role_test1', 'rq_role_test2');

1:END;
1q:

--
-- SET SESSION AUTHORIZATION
--

1:SET SESSION AUTHORIZATION rq_role_test_role1;
1:BEGIN;
1:DECLARE c1 CURSOR FOR SELECT 1;
1:SET SESSION AUTHORIZATION  rq_role_test_role2;
1:DECLARE c2 CURSOR FOR SELECT 1;

-- We should see 1 lock each on each queue, with 1 holder, with 1 active
-- statement in queue. The lorusename in gp_locks_on_resqueue will be the session_user
-- as opposed to the current_user 'rq_role_test_role1' or 'rq_role_test_role2' (which we
-- use to find the destination queue)
0:SELECT lorusename=session_user, lorrsqname, lorlocktype, lormode, lorgranted
  FROM gp_toolkit.gp_locks_on_resqueue WHERE lorrsqname IN ('rq_role_test1', 'rq_role_test2');
0:SELECT rsqname, rsqcountlimit, rsqcountvalue, rsqwaiters, rsqholders
  FROM gp_toolkit.gp_resqueue_status WHERE rsqname IN ('rq_role_test1', 'rq_role_test2');

1:END;
1q:

--
-- Executing a SECURITY DEFINER function
--

1:SET ROLE rq_role_test_role2;
1:CREATE FUNCTION rq_run_as_secd() RETURNS VOID AS 'SELECT pg_sleep(10000000);'
    LANGUAGE SQL SECURITY DEFINER;
1:SET ROLE rq_role_test_role1;
1&:SELECT rq_run_as_secd();

-- It may seem that because rq_run_as_secd() is to be executed as
-- rq_role_test_role2, the owner of rq_run_as_secd(), that it will be allocated
-- to rq_role_test1. However, since the portal is created prior to the
-- current_user switch for SECURITY DEFINER in the function manager, we end up
-- allocating the statement to rq_role_test1, according to the "current" current_user.
0:SELECT lorusename=session_user, lorrsqname, lorlocktype, lormode, lorgranted
  FROM gp_toolkit.gp_locks_on_resqueue WHERE lorrsqname IN ('rq_role_test1', 'rq_role_test2');
0:SELECT rsqname, rsqcountlimit, rsqcountvalue, rsqwaiters, rsqholders
  FROM gp_toolkit.gp_resqueue_status WHERE rsqname IN ('rq_role_test1', 'rq_role_test2');

-- Terminate the function
0:SELECT pg_cancel_backend(pid) FROM pg_stat_activity WHERE query = 'SELECT rq_run_as_secd();';
1<:

-- Cleanup
0:DROP FUNCTION rq_run_as_secd();
0:DROP ROLE rq_role_test_role1;
0:DROP RESOURCE QUEUE rq_role_test1;
0:DROP ROLE rq_role_test_role2;
0:DROP RESOURCE QUEUE rq_role_test2;
