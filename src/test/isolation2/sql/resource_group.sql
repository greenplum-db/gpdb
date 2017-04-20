-- create a resource group when gp_resource_manager is queue
0:CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=.02, memory_limit=.02);
0:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';

-- enable resource group and restart cluster.
-- start_ignore
! gpconfig -c gp_resource_manager -v group;
! gpstop -rai;
-- end_ignore

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

-- reset the GUC and restart cluster.
-- start_ignore
! gpconfig -r gp_resource_manager;
! gpstop -rai;
-- end_ignore
