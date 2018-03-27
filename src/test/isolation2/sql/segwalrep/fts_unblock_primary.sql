-- This test assumes 3 primaries and 3 mirrors from a gpdemo segwalrep cluster

-- function to wait for mirror to come up in sync (1 minute timeout)
create or replace function wait_for_streaming(contentid smallint)
returns void as $$
declare
  updated bool; /* in func */
begin /* in func */
  for i in 1 .. 120 loop /* in func */
    perform gp_request_fts_probe_scan(); /* in func */
    select (mode = 's' and status = 'u') into updated /* in func */
    from gp_segment_configuration /* in func */
    where content = contentid and role = 'm'; /* in func */
    exit when updated; /* in func */
    perform pg_sleep(0.5); /* in func */
  end loop; /* in func */
end; /* in func */
$$ language plpgsql;

-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function pg_ctl(datadir text, command text, port int, contentid int, dbid int)
returns text as $$
    import subprocess
    cmd = 'pg_ctl -D %s ' % datadir
    if command in ('stop', 'restart'):
        cmd = cmd + '-w -m immediate %s' % command
    elif command == 'start':
        opts = '-p %d -\-gp_dbid=%d -\-silent-mode=true -i -\-gp_contentid=%d -\-gp_num_contents_in_cluster=3' % (port, dbid, contentid)
        cmd = cmd + '-o "%s" start' % opts
    else:
        return 'Invalid command input'
    return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;

-- make sure we are in-sync for the primary we will be testing with
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- synchronous_standby_names should be set to '*' by default on primary 2, since
-- we have a working/sync'd mirror
2U: show synchronous_standby_names;
2U: show gp_fts_mark_mirror_down_grace_period;

-- create table and show commits are not blocked
create table fts_unblock_primary (a int) distributed by (a);
insert into fts_unblock_primary values (1);

-- skip FTS probes always
create extension if not exists gp_inject_fault;
select gp_inject_fault('fts_probe', 'reset', 1);
select gp_inject_fault('fts_probe', 'skip', '', '', '', -1, 0, 1);
-- force scan to trigger the fault
select gp_request_fts_probe_scan();
-- verify the failure should be triggered once
select gp_inject_fault('fts_probe', 'status', 1);

-- stop a mirror
-1U: select pg_ctl((select datadir from gp_segment_configuration c where c.role='m' and c.content=2), 'stop', NULL, NULL, NULL);

-- this should block since mirror is not up and sync replication is on
2: begin;
2: insert into fts_unblock_primary values (1);
2&: commit;

-- this should not block due to direct dispatch to primary with active synced mirror
insert into fts_unblock_primary values (3);

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

-- bring the mirror back up and see primary s/u and mirror s/u
-1U: select pg_ctl((select datadir from gp_segment_configuration c where c.role='m' and c.content=2), 'start', (select port from gp_segment_configuration where content = 2 and preferred_role = 'm'), 2, (select dbid from gp_segment_configuration c where c.role='m' and c.content=2));
select wait_for_streaming(2::smallint);
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- everything is back to normal
insert into fts_unblock_primary select i from generate_series(1,10)i;

-- synchronous_standby_names should be back to its original value on the primary
2U: show synchronous_standby_names;

!\retcode gpconfig -r gp_fts_mark_mirror_down_grace_period;
!\retcode gpstop -u;
2U: show gp_fts_mark_mirror_down_grace_period;
