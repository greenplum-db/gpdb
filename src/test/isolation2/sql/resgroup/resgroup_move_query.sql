-- start_matchsubs
--
-- m/ERROR:  process \d+ is in IDLE state/
-- s/\d+/XXX/g
--
-- m/ERROR:  group \d+ doesn't have enough memory on master, expect:\d+, available:\d+/
-- s/\d+/XXX/g
--
-- m/ERROR:  group \d+ doesn't have enough memory on segment, expect:\d+, available:\d+/
-- s/\d+/XXX/g
--
-- end_matchsubs

-- check whether a query running in the specific group
-- @param pid: the pid of QD
-- @param groupname: resource group id
-- @return bool: true/false indicating whether it the session is in the resource group
-- start_ignore
CREATE LANGUAGE plpython3u;
-- end_ignore

DROP ROLE IF EXISTS role_move_query;
-- start_ignore
DROP RESOURCE GROUP rg_move_query;
-- end_ignore
CREATE RESOURCE GROUP rg_move_query WITH (concurrency=1, cpu_hard_quota_limit=20);
CREATE ROLE role_move_query RESOURCE GROUP rg_move_query;

-- test1: cannot move IDLE sessions
1: SET ROLE role_move_query;
1: SET gp_vmem_idle_resource_timeout = 0;
SELECT pg_resgroup_move_query(pid, 'admin_group') FROM pg_stat_activity WHERE query LIKE '%gp_vmem_idle_resource_timeout%' AND state = 'idle';
SELECT is_session_in_group(pid, 'admin_group') FROM pg_stat_activity WHERE query LIKE '%gp_vmem_idle_resource_timeout%' AND state = 'idle';

-- test2: cannot move sessions that are waiting for slot
1: SET ROLE role_move_query;
1: BEGIN;
2: SET ROLE role_move_query;
2&: BEGIN;
SELECT pg_resgroup_move_query(pid, 'default_group') FROM pg_stat_activity WHERE wait_event_type='ResourceGroup';
SELECT is_session_in_group(pid, 'default_group') FROM pg_stat_activity WHERE wait_event_type='ResourceGroup';
1: END;
2<:
2: END;

DROP ROLE role_move_query;
DROP RESOURCE GROUP rg_move_query;
