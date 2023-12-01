-- Test system faults scenarios

-- start_matchsubs
--
-- m/Is the server running on host.*/
-- s/Is the server running on host "\d+.\d+.\d+.\d+" and accepting/Is the server running on host <IP> and accepting/
-- m/(seg\d+ \d+.\d+.\d+.\d+:\d+)/
-- s/(.*)/(seg<ID> IP:PORT)/
-- m/ERROR:  connection to dbid 1 .*:7000 failed .*/
-- s/ERROR:  connection to dbid 1 .*:7000 failed .*/ERROR:  connection to dbid 1 <host>:7000 failed/
--
-- end_matchsubs

-- Let FTS detect/declare failure sooner
!\retcode gpconfig -c gp_fts_probe_interval -v 10 --coordinatoronly;
!\retcode gpstop -u;

create table hs_failover(a int);
insert into hs_failover select * from generate_series(1,10);
-1S: select * from hs_failover;

----------------------------------------------------------------
-- Mirror segment fails
----------------------------------------------------------------
select pg_ctl(datadir, 'stop', 'immediate') from gp_segment_configuration where content=1 and role = 'm';

-- make sure mirror is detected down
create temp table hs_tt(a int);
select gp_request_fts_probe_scan();

-- will not succeed
-1S: select * from hs_failover;
-1Sq:

-- recovery
!\retcode gprecoverseg -aF;

-- sync-up
select wait_until_all_segments_synchronized();

-- works now
-1S: select * from hs_failover;

----------------------------------------------------------------
-- Primary segment fails
----------------------------------------------------------------
-- inject a fault where the mirror gets out of recovery
select gp_inject_fault('out_of_recovery_in_startupxlog', 'skip', dbid) from gp_segment_configuration where content = 1 and role = 'm';

select pg_ctl(datadir, 'stop', 'immediate') from gp_segment_configuration where content=1 and role = 'p';
select gp_request_fts_probe_scan();

-- make sure failover happens
select dbid, content, role, preferred_role, mode, status from gp_segment_configuration where content = 1;
select gp_wait_until_triggered_fault('out_of_recovery_in_startupxlog', 1, dbid) from gp_segment_configuration where content = 1 and role = 'p';
select gp_inject_fault('out_of_recovery_in_startupxlog', 'reset', dbid) from gp_segment_configuration where content = 1 and role = 'p';

-- On an existing standby connection, query will run but it is dispatched to the previous mirror
-- in an existing gang. That mirror is now a primary, so it will complain and the query fails.
-1S: select * from hs_failover;
-1Sq:

-- will fail due to downed mirror (previous primary)
-1S: select * from hs_failover;
-1Sq:

-- bring the downed mirror up
!\retcode gprecoverseg -aF;
select wait_until_all_segments_synchronized();

-- mirror is up
-1S: select dbid, content, role, preferred_role, mode, status from gp_segment_configuration where content = 1;

-- now the query will succeed
-1S: select * from hs_failover;
-1Sq:

-- re-balance, bring the segments to their preferred roles
!\retcode gprecoverseg -ar;
select wait_until_all_segments_synchronized();
-1S: select dbid, content, role, preferred_role, mode, status from gp_segment_configuration where content = 1;

-- query runs fine still
-1S: select * from hs_failover;

----------------------------------------------------------------
-- DTX recovery
----------------------------------------------------------------
-- skip FTS probe to prevent unexpected mirror promotion
1: select gp_inject_fault_infinite('fts_probe', 'skip', dbid) from gp_segment_configuration where role='p' and content=-1;

-- inject fault to cripple QE right after a QE finished the prepare phase of 2PC
select gp_inject_fault_infinite('finish_commit_prepared', 'error', dbid) from gp_segment_configuration where content=1 and role='p';

-- session 1 on primary QD tries to commit a DTX, but cannot finish due to the fault on a QE
1: begin;
1: create table tt_hs_dtx(a int);
1&: end;

-- inject a panic on primary QD, essentially restarts the primary QD
2: select gp_inject_fault('before_read_command', 'panic', dbid) from gp_segment_configuration where content=-1 and role='p';
2: select 1;

1<:
1q:
2q:

-- standby QD can still run query
-1S: select * from hs_failover;
-- XXX: currently it sees the in-doubt DTX but it shouldnt' when we supported DTX isolation.
-1S: select * from tt_hs_dtx;

-- resets the fault
-1S: select gp_inject_fault_infinite('finish_commit_prepared', 'reset', dbid) from gp_segment_configuration where content=1 and role='p';

-1S: select wait_until_all_segments_synchronized();
1: select gp_inject_fault('before_read_command', 'reset', dbid) from gp_segment_configuration where content=-1 and role='p';
1: select gp_inject_fault('fts_probe', 'reset', dbid) from gp_segment_configuration where role='p' and content=-1;
