-- start_matchsubs
--
-- m/ERROR:  process \d+ is in IDLE state/
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
DROP ROLE IF EXISTS role_move_query_small;
-- start_ignore
DROP RESOURCE GROUP rg_move_query;
DROP RESOURCE GROUP rg_move_query_small;
-- end_ignore

CREATE RESOURCE GROUP rg_move_query WITH (concurrency=1, cpu_hard_quota_limit=20);
CREATE RESOURCE GROUP rg_move_query_small WITH (concurrency=1, cpu_hard_quota_limit=10);
CREATE ROLE role_move_query RESOURCE GROUP rg_move_query;
CREATE ROLE role_move_query_small RESOURCE GROUP rg_move_query_small;

CREATE EXTENSION IF NOT EXISTS gp_inject_fault;



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

-- test3: the destination group will wake up 'pg_resgroup_move_query' when a new slot become available
1: SET ROLE role_move_query;
1&: SELECT pg_sleep(5);
2: SET ROLE role_move_query_small;
2&: SELECT pg_sleep(10);
3&: SELECT pg_resgroup_move_query(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep(10)%' AND rsgname='rg_move_query_small';
1<:
-- connection 1 finished, it will wake up connection 3
3<:
3: SELECT rsgname, query FROM pg_stat_activity WHERE state = 'active' and query like 'SELECT%';
2<:
1q:
2q:
3q:

-- test4: check destination group has no slot leaking if move signal processed at the time target process became idle
-- start transaction at first process
-- start to move it at second process, but suspend before sending signal to it
-- end transaction at first process
-- resume at second process, it should throw an error
1: SET ROLE role_move_query_small;
1: BEGIN;
1: SELECT 1 a FROM pg_class LIMIT 1;
2: SELECT gp_inject_fault('resource_group_give_away_begin', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2: SELECT gp_inject_fault('resource_group_give_away_begin', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2&: SELECT pg_resgroup_move_query(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_class%' AND rsgname='rg_move_query_small';
1: END;
1: SELECT gp_wait_until_triggered_fault('resource_group_give_away_begin', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: SELECT gp_inject_fault('resource_group_give_away_begin', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2<:
2: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='rg_move_query';

-- test5: check destination group has no slot leaking if move signal processed at the time target process became dead
-- start transaction at first process
-- start to move it at second process, but suspend before sending signal to it
-- end transaction at first process and quit
-- resume at second process, it should throw an error
1: SET ROLE role_move_query_small;
1: BEGIN;
1: SELECT 1 a FROM pg_class LIMIT 1;
2: SELECT gp_inject_fault('resource_group_give_away_begin', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2: SELECT gp_inject_fault('resource_group_give_away_begin', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2&: SELECT pg_resgroup_move_query(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_class%' AND rsgname='rg_move_query_small';
1: END;
1q:
3: SELECT gp_wait_until_triggered_fault('resource_group_give_away_begin', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
3: SELECT gp_inject_fault('resource_group_give_away_begin', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2<:
2: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='rg_move_query';

-- test6: check destination group has no slot leaking if we got an error on latch waiting
-- sleep at first process
-- start to move it at second process, send moving signal to first process
-- suspend at first process after handling moveto* params
-- interrupt in WaitLatch block at second process, this will force pg_resgroup_move_query to continue
-- second process should throw an error, but consider moveto* params handled by target
-- resume at first process
-- first process should continue with moving as all slot control is on it's side
-- segments will not be moved to new group until the next command begins
1: SET ROLE role_move_query_small;
1: SELECT gp_inject_fault('resource_group_move_handler_after_qd_control', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: SELECT gp_inject_fault('resource_group_move_handler_after_qd_control', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: BEGIN;
1&: SELECT * FROM gp_dist_random('gp_id'), pg_sleep(5) LIMIT 1;
2: SELECT gp_inject_fault('resource_group_give_away_wait_latch', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2: SELECT gp_inject_fault('resource_group_give_away_wait_latch', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2&: SELECT pg_resgroup_move_query(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND rsgname='rg_move_query_small';
3: SELECT gp_wait_until_triggered_fault('resource_group_give_away_wait_latch', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
3: SELECT pg_cancel_backend(pid) FROM pg_stat_activity WHERE query LIKE '%pg_resgroup_move_query%' AND pid != pg_backend_pid();
3: SELECT gp_inject_fault('resource_group_give_away_wait_latch', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2<:
2: SELECT gp_inject_fault('resource_group_move_handler_after_qd_control', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1<:
2: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='rg_move_query';
--if there any next command called in the same transaction, segments will try to fix the situation and move out of inconsistent state
1: SELECT * FROM gp_dist_random('gp_id'), pg_sleep(1) LIMIT 1;
2: SELECT is_session_in_group(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND state = 'idle in transaction';
2: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='rg_move_query';
1: END;
2: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='rg_move_query';
1q:

-- test7: check destination group has no slot leaking if target process set latch at the last moment
-- sleep at first process
-- start to move it at second process, send moving signal to first process
-- suspend at first process just before setting latch
-- wait for timeout on WaitLatch on second process and suspend
-- resume at first process, it should set latch (which is late) and clean moveto* values
-- resume at second process, as moveto* was cleaned, we know first process handled signal
-- moving command at second process should finish successfully
1: SET ROLE role_move_query_small;
1: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: SELECT gp_inject_fault('resource_group_move_handler_after_qd_control', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: SELECT gp_inject_fault('resource_group_move_handler_after_qd_control', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: BEGIN;
1&: SELECT * FROM gp_dist_random('gp_id'), pg_sleep(3) LIMIT 1;
2: SELECT gp_inject_fault('resource_group_give_away_after_latch', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2: SELECT gp_inject_fault('resource_group_give_away_after_latch', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2: SET gp_resource_group_move_timeout = 1000;
2&: SELECT pg_resgroup_move_query(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND rsgname='rg_move_query_small';
3: SELECT gp_wait_until_triggered_fault('resource_group_give_away_after_latch', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
3: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
3: SELECT gp_wait_until_triggered_fault('resource_group_move_handler_after_qd_control', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
3: SELECT gp_inject_fault('resource_group_move_handler_after_qd_control', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
3: SELECT gp_inject_fault('resource_group_give_away_after_latch', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1<:
2<:
2: RESET gp_resource_group_move_timeout;
3: SELECT is_session_in_group(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND state = 'idle in transaction';
1: END;
3: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='rg_move_query';
1q:

-- test8: check destination group has no slot leaking if taget process recieved one move command at the time of processing another
-- sleep at first process
-- start to move it at second process, send moving signal to first process
-- suspend at first process just before setting latch and moving
-- run another moving command at third process, it should throw an error as target process is alredy moving
-- resume at first process, it should continue with moving
-- moving command at second process should finish successfully
1: SET ROLE role_move_query_small;
1: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: BEGIN;
1&: SELECT * FROM gp_dist_random('gp_id'), pg_sleep(5) LIMIT 1;
2&: SELECT pg_resgroup_move_query(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND rsgname='rg_move_query_small';
3: SELECT gp_wait_until_triggered_fault('resource_group_move_handler_before_qd_control', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
3: SELECT pg_resgroup_move_query(pid, 'default_group') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND rsgname='rg_move_query_small';
3: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1<:
2<:
3: SELECT is_session_in_group(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND state = 'idle in transaction';
1: END;
3: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='rg_move_query';
3: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='default_group';
1q:

-- Test9: check we'll wait and quit by gp_resource_group_move_timeout if target process stuck on signal handling
1: SET ROLE role_move_query_small;
1: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
1: BEGIN;
1&: SELECT pg_sleep(3);
2: SET gp_resource_group_move_timeout = 3000;
2: SELECT pg_resgroup_move_query(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND rsgname='rg_move_query_small';
2: SELECT gp_inject_fault('resource_group_move_handler_before_qd_control', 'resume', dbid) FROM gp_segment_configuration where role = 'p' and content = -1;
2: RESET gp_resource_group_move_timeout;
1<:
2: SELECT num_running FROM gp_toolkit.gp_resgroup_status WHERE rsgname='rg_move_query';
1: END;

-- Test10: check entrydb queries working
-- Previously, we sent a signal to only one process - dispatcher or entrydb.
-- This led to various errors - triggered assertions or only entrydb process moving.
-- But it never led to the only one correct result - ALL processes should be moved.
-- Here we use is_session_in_group() to precisely check ALL processes were moved.
1: SET ROLE role_move_query_small;
1: BEGIN;
--spawn all backends at first short call to guarantee correct pg_resgroup_move_query() execution
1: SELECT * FROM gp_dist_random('gp_id'), pg_sleep(1) LIMIT 1;
1&: SELECT * FROM gp_dist_random('gp_id'), pg_sleep(3) LIMIT 1;
2: SELECT pg_resgroup_move_query(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND rsgname='rg_move_query_small';
1<:
2: SELECT is_session_in_group(pid, 'rg_move_query') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND state = 'idle in transaction';
-- and check we can move it back right in the same transaction
1&: SELECT * FROM gp_dist_random('gp_id'), pg_sleep(3) LIMIT 1;
2: SELECT pg_resgroup_move_query(pid, 'rg_move_query_small') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND rsgname='rg_move_query';
1<:
2: SELECT is_session_in_group(pid, 'rg_move_query_small') FROM pg_stat_activity WHERE query LIKE '%pg_sleep%' AND state = 'idle in transaction';
1: END;

DROP ROLE role_move_query;
DROP RESOURCE GROUP rg_move_query;
DROP ROLE role_move_query_small;
DROP RESOURCE GROUP rg_move_query_small;
