-- This test verifies that FTS shouldn't issue failover to mirror when
-- the primary is taking long in the RESET state.

-- start_matchsubs
-- m/seg0 [0-9.]+:\d+/
-- s/seg0 [0-9.]+:\d+/seg0 IP:PORT/
-- end_matchsubs

-- Let FTS detect/declare failure sooner 
!\retcode gpconfig -c gp_fts_probe_interval -v 10 --masteronly;
-- Because after RESET, it still takes a little while for the primary
-- to restart, and potentially makes FTS think it's in "recovery not
-- in progress" stage and promote the mirror, we would need the FTS
-- to make that decision a bit less frequently.
!\retcode gpconfig -c gp_fts_probe_retries -v 15 --masteronly;
!\retcode gpstop -u;

-- Let the background writer sleep 17 seconds to delay the resetting.
-- This number is selected to be larger than the 15-second retry window
-- which makes a meaningful test, meanwhile reduce the chance that FTS sees
-- a "recovery not in progress" primary as much as possible.
select gp_inject_fault('fault_in_background_writer_quickdie', 'sleep', '', '', '', 1, 1, 17, dbid) 
from gp_segment_configuration where role = 'p' and content = 0;

-- Do not let the postmaster send SIGKILL to the bgwriter
select gp_inject_fault_infinite('postmaster_server_loop_no_sigkill', 'skip', dbid) 
from gp_segment_configuration where role = 'p' and content = 0;

-- Now bring down primary of seg0. There're a lot of ways to do that, in order
-- to better emulate a real-world scnarios we're injecting a PANIC to do that.
1:select gp_inject_fault('start_prepare', 'panic', dbid) 
from gp_segment_configuration where role = 'p' AND content = 0;
1&:create table fts_reset_t(a int);

-- This should fail due to the seg0 in reset mode
2&:create table fts_reset_t2(a int);

-- Try another one but let the gang creation retry for longer.
-- Default is 2000ms (gp_gang_creation_retry_timer) * 5 (gp_gang_creation_retry_count) = 10s.
-- Now make it 50s which is well longer than the delay we inserted before, so it can succeed.
3:set gp_gang_creation_retry_timer = 10000;
3:create table fts_reset_t3(a int);

1<:
2<:

-- We shouldn't see failover to mirror
select gp_request_fts_probe_scan();
select dbid, role, preferred_role, status from gp_segment_configuration where content = 0;

select gp_inject_fault('postmaster_server_loop_no_sigkill', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;
select gp_inject_fault('fault_in_background_writer_quickdie', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = 0;

-- The only table that should have been created successfully
drop table fts_reset_t3;

-- In case anything goes wrong, we don't want to affect other tests. So rebalance the cluster anyway.
!\retcode gprecoverseg -aF
!\retcode gprecoverseg -ar

-- restore parameters
!\retcode gpconfig -r gp_fts_probe_interval --masteronly;
!\retcode gpconfig -r gp_fts_probe_retries --masteronly;
!\retcode gpstop -u;
