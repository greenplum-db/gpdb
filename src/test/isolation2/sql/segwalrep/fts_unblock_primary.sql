-- This test assumes 3 primaries and 3 mirrors from a gpdemo segwalrep cluster
include: helpers/server_helpers.sql;


-- make sure we are in-sync for the primary we will be testing with
select content, role, preferred_role, mode, status from gp_segment_configuration;

-- synchronous_standby_names should be set to '*' by default on primary 2, since
-- we have a working/sync'd mirror
2U: show synchronous_standby_names;
2U: show gp_fts_mark_mirror_down_grace_period;

-- create table and show commits are not blocked
create table fts_unblock_primary (a int) distributed by (a);
-- This case aims to insert a tuple to seg0.
-- Under jump consistent hash, int value 4 should
-- be on seg0.
insert into fts_unblock_primary values (4);

-- skip FTS probes always
select gp_inject_fault('fts_probe', 'reset', 1);
select gp_inject_fault_infinite('fts_probe', 'skip', 1);
-- force scan to trigger the fault
select gp_request_fts_probe_scan();
-- verify the failure should be triggered once
select gp_wait_until_triggered_fault('fts_probe', 1, 1);

-- stop a mirror
-1U: select pg_ctl((select get_data_directory_for(2, 'm')), 'stop');

-- this should block since mirror is not up and sync replication is on
2: begin;
2: insert into fts_unblock_primary values (5);
2&: commit;

-- this should not block due to direct dispatch to primary with active synced mirror
insert into fts_unblock_primary values (2);

-- resume FTS probes
select gp_inject_fault('fts_probe', 'reset', 1);

-- trigger fts probe and check to see primary marked n/u and mirror still n/u as
-- still should be in mirror down grace period.
select gp_request_fts_probe_scan();
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- set mirror down grace period to zero to instantly mark mirror down
!\retcode gpconfig -c gp_fts_mark_mirror_down_grace_period -v 0;
!\retcode gpstop -u;

2U: show gp_fts_mark_mirror_down_grace_period;

-- trigger fts probe and check to see primary marked n/u and mirror n/d
select gp_request_fts_probe_scan();
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- should unblock and commit after FTS sent primary a SyncRepOff libpq message
2<:

-- synchronous_standby_names should now be empty on the primary
2U: show synchronous_standby_names;

--hold walsender in startup
select gp_inject_fault_infinite('initialize_wal_sender', 'suspend', dbid)
from gp_segment_configuration where role='p' and content=2;

-- bring the mirror back up and see primary s/u and mirror s/u
-1U: select pg_ctl_start((select datadir from gp_segment_configuration c where c.role='m' and c.content=2), (select port from gp_segment_configuration where content = 2 and preferred_role = 'm'));
select gp_wait_until_triggered_fault('initialize_wal_sender', 1, dbid)
from gp_segment_configuration where role='p' and content=2;
-- make sure the walsender on primary is in startup
select state from gp_stat_replication where gp_segment_id=2;
select gp_request_fts_probe_scan();
-- mirror should continue to be reported as down since walsender is in startup
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- let the walsender proceed
select gp_inject_fault('initialize_wal_sender', 'reset', dbid)
from gp_segment_configuration where role='p' and content=2;
select wait_until_segment_synchronized(2);
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- everything is back to normal
insert into fts_unblock_primary select i from generate_series(1,10)i;

-- synchronous_standby_names should be back to its original value on the primary
2U: show synchronous_standby_names;

!\retcode gpconfig -r gp_fts_mark_mirror_down_grace_period;
!\retcode gpstop -u;
2U: show gp_fts_mark_mirror_down_grace_period;
