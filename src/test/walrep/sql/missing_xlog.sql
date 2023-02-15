-- start_ignore
create language plpython3u;
-- end_ignore

create or replace function pg_ctl(datadir text, command text, port int)
returns text as $$
    import subprocess

    cmd = 'pg_ctl -l postmaster.log -D %s ' % datadir
    if command in ('stop', 'restart'):
        cmd = cmd + '-w -m immediate %s' % command
    elif command == 'start':
        opts = '-p %d -i ' % (port)
        cmd = cmd + '-o "%s" start' % opts
    elif command == 'reload':
        cmd = cmd + 'reload'
    else:
        return 'Invalid command input'

    return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).decode().replace('.', '')
$$ language plpython3u;

create or replace function wait_for_replication_error (expected_error text, segment_id int, retries int) returns bool as
$$
declare
	i int;
begin
	i := 0;
	loop
		if exists (select * from (select sync_error from gp_stat_replication where gp_segment_id = segment_id) t where sync_error = expected_error) then
			return true;
		end if;
		if i >= retries then
			return false;
		end if;
		perform pg_sleep(0.1);
		i := i + 1;
	end loop;
end;
$$ language plpgsql;

-- function to wait for mirror to come up in sync (10 minute timeout)
create or replace function wait_for_mirror_sync(contentid smallint)
	returns void as $$
declare
	updated bool;
begin
for i in 1 .. 1200 loop
perform gp_request_fts_probe_scan();
select (mode = 's' and status = 'u') into updated
from gp_segment_configuration
where content = contentid and role = 'm';
exit when updated;
perform pg_sleep(0.5);
end loop;
end;
$$ language plpgsql;

create or replace function wait_for_mirror_down(contentid smallint, timeout_sec integer) returns bool as
$$
declare i int;
begin
	i := 0;
	loop
		perform gp_request_fts_probe_scan();
		if (select count(1) from gp_segment_configuration where role='m' and content=$1 and status='d') = 1 then
			return true;
		end if;
		if i >= 2 * $2 then
			return false;
		end if;
		perform pg_sleep(0.5);
		i = i + 1;
	end loop;
end;
$$ language plpgsql;

-- start_ignore
-- let the mirror be marked as 'down' quickly
\! gpconfig -c gp_fts_mark_mirror_down_grace_period -v 2
-- since setting wal_keep_size to 0 is not safe for master/standby,
-- we are not going to set wal_keep_size to 0 now, to avoid flaky tests
\! gpconfig -c wal_keep_size -v 64MB
\! gpstop -u
-- end_ignore
-- stop a mirror
select pg_ctl((select datadir from gp_segment_configuration c where c.role='m' and c.content=0), 'stop', NULL);

-- The WAL segment containing the restart_lsn of internal_wal_replication_slot
-- should be removed from the seg0 primary. That will cause subsequent
-- incremental recovery of its mirror to fail. The seg0 primary should remember
-- this failure in the sync_error flag.
select gp_inject_fault_infinite('keep_log_seg', 'skip', dbid)
  from gp_segment_configuration where role='p' and content = 0;
checkpoint;
select gp_wait_until_triggered_fault('keep_log_seg', 1, dbid)
  from gp_segment_configuration where role='p' and content = 0;
-- wait for the mirror down, so the following SQLs needs no replication on seg0
select wait_for_mirror_down(0::smallint, 30);
-- running pg_switch_wal() several times and `checkpoint` to remove old WAL files
do $$
    declare i int;
    begin
        i := 0;
        create table t_dummy_switch(i int) distributed by (i);
        loop
            if i >= 2 then
                return ;
            end if;
            perform pg_switch_wal() from gp_dist_random('gp_id') where gp_segment_id=0;
            insert into t_dummy_switch select generate_series(1,10);
            i := i + 1;
        end loop;
        drop table t_dummy_switch;
    end
$$;
-- This is when the old WAL segments created by the switch operation will be removed.
checkpoint;

-- start_ignore
\! gprecoverseg -av
-- end_ignore
-- check the view, we expect to see error, since the WAL files required
-- by mirror are removed on the corresponding primary
select wait_for_replication_error('walread', 0, 500);
select sync_error from gp_stat_replication where gp_segment_id = 0;

-- do full recovery for the first mirror
select pg_ctl((select datadir from gp_segment_configuration c where c.role='m' and c.content=0), 'stop', NULL);
-- wait for mirror is marked down
-- the status of the mirror is not marked as 'd' immediately
-- even if we run gp_request_fts_probe_scan() or pg_sleep()
select wait_for_mirror_down(0::smallint, 30);

-- let the primary be normal before do full recovery for mirror
select gp_inject_fault('keep_log_seg', 'reset', dbid)
  from gp_segment_configuration where role='p' and content = 0;
-- start_ignore
\! gprecoverseg -avF
-- end_ignore
-- force the WAL segment to switch over from after previous pg_switch_wal().
create temp table dummy2 (id int4) distributed randomly;

-- the error should go away
select wait_for_replication_error('none', 0, 500);
select sync_error from gp_stat_replication where gp_segment_id = 0;

select gp_request_fts_probe_scan();
-- Validate that the mirror for content=0 is marked up.
select count(*) = 2 as mirror_up from gp_segment_configuration
 where content=0 and status='u';
-- make sure leave the test only after mirror is in sync to avoid
-- affecting other tests. Thumb rule: leave cluster in same state as
-- test started.
select wait_for_mirror_sync(0::smallint);
select role, preferred_role, content, mode, status from gp_segment_configuration;
-- start_ignore
\! gpconfig -r gp_fts_mark_mirror_down_grace_period
\! gpconfig -r wal_keep_size
\! gpstop -u
-- end_ignore
