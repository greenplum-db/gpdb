-- Behavioral tests to showcase what happens when resource queue limits are
-- altered concurrently with active and waiting statements.

0:CREATE RESOURCE QUEUE rq_alter WITH (active_statements = 1);
0:CREATE ROLE rq_alter_role RESOURCE QUEUE rq_alter;

--
-- Scenario 1: Bumping the resource queue limit
--
1:SET ROLE rq_alter_role;
1:BEGIN;
1:DECLARE c1 CURSOR FOR SELECT 1;

2:SET ROLE rq_alter_role;
2:BEGIN;
-- The following will block waiting for the resource queue slot.
2&:DECLARE c2 CURSOR FOR SELECT 1;

-- Inflate the limit.
0:ALTER RESOURCE QUEUE rq_alter WITH (active_statements = 2);
-- The limit has been bumped. However we still see that Session 2 is waiting.
0:SELECT lorusename=session_user, lorrsqname, lorlocktype, lormode, lorgranted
  FROM gp_toolkit.gp_locks_on_resqueue WHERE lorrsqname ='rq_alter';
0:SELECT rsqname, rsqcountlimit, rsqcountvalue, rsqwaiters, rsqholders
  FROM gp_toolkit.gp_resqueue_status WHERE rsqname = 'rq_alter';

-- New statements actually can get into the queue ahead of Session 2!
1:DECLARE c3 CURSOR FOR SELECT 1;

-- Only once a statement completes (or ERRORs out) is Session 2 woken up, and
-- can complete acquiring the lock for portal c2.
1:CLOSE c1;
2<:
1q:
2q:

--
-- Scenario 2: Decreasing the resource queue limit
--
1:SET ROLE rq_alter_role;
1:BEGIN;
1:DECLARE c1 CURSOR FOR SELECT 1;
1:DECLARE c2 CURSOR FOR SELECT 1;

-- The following will block waiting for the resource queue slot.
2:SET ROLE rq_alter_role;
2:BEGIN;
2&:DECLARE c3 CURSOR FOR SELECT 1;

-- The current situation:
0:SELECT lorusename=session_user, lorrsqname, lorlocktype, lormode, lorgranted
  FROM gp_toolkit.gp_locks_on_resqueue WHERE lorrsqname ='rq_alter';
0:SELECT rsqname, rsqcountlimit, rsqcountvalue, rsqwaiters, rsqholders
  FROM gp_toolkit.gp_resqueue_status WHERE rsqname = 'rq_alter';

-- Decrease the limit
0:ALTER RESOURCE QUEUE rq_alter WITH (active_statements = 1);

-- Nothing changes, current holders and waiters continue as they were. However,
-- do note that now we have a situation where rsqcountvalue > rsqcountlimit.
0:SELECT lorusename=session_user, lorrsqname, lorlocktype, lormode, lorgranted
  FROM gp_toolkit.gp_locks_on_resqueue WHERE lorrsqname ='rq_alter';
0:SELECT rsqname, rsqcountlimit, rsqcountvalue, rsqwaiters, rsqholders
  FROM gp_toolkit.gp_resqueue_status WHERE rsqname = 'rq_alter';

-- The following will NOT wake up Session 2, as we would exceed the limit if we
-- were to do so.
1:CLOSE c1;
0:SELECT lorusename=session_user, lorrsqname, lorlocktype, lormode, lorgranted
  FROM gp_toolkit.gp_locks_on_resqueue WHERE lorrsqname ='rq_alter';
0:SELECT rsqname, rsqcountlimit, rsqcountvalue, rsqwaiters, rsqholders
  FROM gp_toolkit.gp_resqueue_status WHERE rsqname = 'rq_alter';

-- Now this will wake up Session 2
1:CLOSE c2;
2<:

1q:
2q:

0:DROP ROLE rq_alter_role;
0:DROP RESOURCE QUEUE rq_alter;
