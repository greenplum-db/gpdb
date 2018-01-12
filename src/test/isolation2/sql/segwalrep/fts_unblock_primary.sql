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

create or replace function pg_ctl(datadir text, command text, port int, contentid int)
returns text as $$
    import subprocess

    cmd = 'pg_ctl -D %s ' % datadir
    if command in ('stop', 'restart'):
        cmd = cmd + '-w -m immediate %s' % command
    elif command == 'start':
        opts = '-p %d -\-gp_dbid=0 -\-silent-mode=true -i -M mirrorless -\-gp_contentid=%d -\-gp_num_contents_in_cluster=3' % (port, contentid)
        cmd = cmd + '-o "%s" start' % opts
    elif command == 'reload':
        cmd = cmd + 'reload'
    else:
        return 'Invalid command input'

    return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;

-- make sure we are in-sync for the primary we will be testing with
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- synchronous_standby_names should be set to '*' by default on primary 2, since
-- we have a working/sync'd mirror
-- XXX We assume primary 2 is DBID 4, which may not be the case.
4U: show synchronous_standby_names;

-- create table and show commits are not blocked
create table fts_unblock_primary (a int) distributed by (a);
insert into fts_unblock_primary values (1);

-- turn off fts
! gpconfig -c gp_fts_probe_pause -v true --masteronly --skipvalidation;
1U: select pg_ctl((select fselocation from gp_segment_configuration c, pg_filespace_entry f where c.role='p' and c.content=-1 and c.dbid = f.fsedbid), 'reload', NULL, NULL);

-- stop a mirror
1U: select pg_ctl((select fselocation from gp_segment_configuration c, pg_filespace_entry f where c.role='m' and c.content=2 and c.dbid = f.fsedbid), 'stop', NULL, NULL);

-- this should block since mirror is not up and sync replication is on
2: begin;
2: insert into fts_unblock_primary values (1);
2&: commit;

-- this should not block due to direct dispatch to primary with active synced mirror
insert into fts_unblock_primary values (3);

-- turn on fts
! gpconfig -c gp_fts_probe_pause -v false --masteronly --skipvalidation;
1U: select pg_ctl((select fselocation from gp_segment_configuration c, pg_filespace_entry f where c.role='p' and c.content=-1 and c.dbid = f.fsedbid), 'reload', NULL, NULL);

--trigger fts probe and check to see primary marked n/u and mirror n/d
select gp_request_fts_probe_scan();
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- should unblock and commit after FTS sent primary a SyncRepOff libpq message
2<:

-- synchronous_standby_names should now be empty on the primary
4U: show synchronous_standby_names;

-- bring the mirror back up and see primary s/u and mirror s/u
1U: select pg_ctl((select fselocation from gp_segment_configuration c, pg_filespace_entry f where c.role='m' and c.content=2 and c.dbid = f.fsedbid), 'start', (select port from gp_segment_configuration where content = 2 and preferred_role = 'm'), 2);
select wait_for_streaming(2::smallint);
select content, role, preferred_role, mode, status from gp_segment_configuration where content=2;

-- everything is back to normal
insert into fts_unblock_primary select i from generate_series(1,10)i;

-- synchronous_standby_names should be back to its original value on the primary
4U: show synchronous_standby_names;
