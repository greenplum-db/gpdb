-- create a resource group when gp_resource_manager is queue
0:CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=.20, memory_limit=.20);
0:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';

-- test1: test gp_toolkit.gp_resgroup_status and pg_stat_activity
-- no query has been assigned to the this group
1:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
1:CREATE role role_concurrency_test RESOURCE GROUP rg_concurrency_test;
2:SET role role_concurrency_test;
2:BEGIN;
3:SET role role_concurrency_test;
3:BEGIN;
4:SET role role_concurrency_test;
4&:BEGIN;
-- new transaction will be blocked when the concurrency limit of the resource group is reached.
1:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
1:SELECT waiting_reason, rsgqueueduration > '0'::interval as time from pg_stat_activity where current_query = 'BEGIN;' and rsgname = 'rg_concurrency_test';
2:END;
3:END;
4<:
4:END;
1:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
1:DROP role role_concurrency_test;
1:DROP RESOURCE GROUP rg_concurrency_test;

-- test2: test alter concurrency
-- Create a resource group with concurrency=2. Prepare 2 running transactions and 1 queueing transactions.
-- Alter concurrency 2->3, the queueing transaction will be woken up, the 'value' and 'proposed' of pg_resgroupcapability will be set to 3.
11:CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=.20, memory_limit=.20);
11:CREATE role role_concurrency_test RESOURCE GROUP rg_concurrency_test;
12:SET role role_concurrency_test;
12:BEGIN;
13:SET role role_concurrency_test;
13:BEGIN;
14:SET role role_concurrency_test;
14&:BEGIN;
11:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
11:SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
11:ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 3;
11:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
11:SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
12:END;
13:END;
14<:
14:END;
11:DROP role role_concurrency_test;
11:DROP RESOURCE GROUP rg_concurrency_test;

-- test3: test alter concurrency
-- Create a resource group with concurrency=3. Prepare 3 running transactions, and 1 queueing transaction.
21:CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=3, cpu_rate_limit=.20, memory_limit=.20);
21:CREATE role role_concurrency_test RESOURCE GROUP rg_concurrency_test;
22:SET role role_concurrency_test;
22:BEGIN;
23:SET role role_concurrency_test;
23:BEGIN;
24:SET role role_concurrency_test;
24:BEGIN;
25:SET role role_concurrency_test;
25&:BEGIN;
21:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
21:SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- Alter concurrency 3->2, the 'proposed' of pg_resgroupcapability will be set to 2.
21:ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
21:SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- When one transaction is finished, queueing transaction won't be woken up. There're 2 running transactions and 1 queueing transaction.
24:END;
21:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- New transaction will be queued, there're 2 running and 2 queueing transactions.
24&:BEGIN;
21:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- Finish another transaction, one queueing transaction will be woken up, there're 2 running transactions and 1 queueing transaction.
22:END;
21:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- Alter concurrency 2->2, the 'value' and 'proposed' of pg_resgroupcapability will be set to 2.
21:ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
21:SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- Finish another transaction, one queueing transaction will be woken up, there're 2 running transactions and 0 queueing transaction.
23:END;
21:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
24<:
25<:
25:END;
24:END;
21:DROP role role_concurrency_test;
21:DROP RESOURCE GROUP rg_concurrency_test;

-- test4: concurrently drop resource group

31:CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=.20, memory_limit=.20);
31:CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

-- DROP should fail if there're running transactions
32:SET ROLE role_concurrency_test;
32:BEGIN;
31:BEGIN;
31:DROP ROLE role_concurrency_test;
31:DROP RESOURCE GROUP rg_concurrency_test;
31:END;
32:END;
32:RESET ROLE;

-- DROP is abortted
31:BEGIN;
31:DROP ROLE role_concurrency_test;
31:DROP RESOURCE GROUP rg_concurrency_test;
31:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
32:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
32:SET ROLE role_concurrency_test;
32&:BEGIN;
31:ABORT;
32<:
32:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
32:END;
32:RESET ROLE;

-- DROP is committed
31:BEGIN;
31:DROP ROLE role_concurrency_test;
31:DROP RESOURCE GROUP rg_concurrency_test;
31:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
32:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
32:SET ROLE role_concurrency_test;
32&:BEGIN;
31:END;
32<:
33:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';

-- test5: concurrently alter resource group cpu rate limit

-- start_ignore
DROP RESOURCE GROUP rg1_concurrency_test;
DROP RESOURCE GROUP rg2_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg1_concurrency_test WITH (concurrency=2, cpu_rate_limit=0.1, memory_limit=0.2);
CREATE RESOURCE GROUP rg2_concurrency_test WITH (concurrency=2, cpu_rate_limit=0.2, memory_limit=0.2);

41:BEGIN;
41:ALTER RESOURCE GROUP rg1_concurrency_test SET CPU_RATE_LIMIT 0.35;
42:BEGIN;
42&:ALTER RESOURCE GROUP rg2_concurrency_test SET CPU_RATE_LIMIT 0.35;
41:ABORT;
42<:
42:COMMIT;
SELECT g.rsgname, c.cpu_rate_limit FROM gp_toolkit.gp_resgroup_config c, pg_resgroup g WHERE c.groupid=g.oid ORDER BY g.oid;

DROP RESOURCE GROUP rg1_concurrency_test;
DROP RESOURCE GROUP rg2_concurrency_test;

CREATE RESOURCE GROUP rg1_concurrency_test WITH (concurrency=2, cpu_rate_limit=0.1, memory_limit=0.2);
CREATE RESOURCE GROUP rg2_concurrency_test WITH (concurrency=2, cpu_rate_limit=0.2, memory_limit=0.2);

41:BEGIN;
41:ALTER RESOURCE GROUP rg1_concurrency_test SET CPU_RATE_LIMIT 0.35;
42:BEGIN;
42&:ALTER RESOURCE GROUP rg2_concurrency_test SET CPU_RATE_LIMIT 0.35;
41:COMMIT;
42<:
SELECT g.rsgname, c.cpu_rate_limit FROM gp_toolkit.gp_resgroup_config c, pg_resgroup g WHERE c.groupid=g.oid ORDER BY g.oid;

DROP RESOURCE GROUP rg1_concurrency_test;
DROP RESOURCE GROUP rg2_concurrency_test;
