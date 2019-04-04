-- start_ignore
create language plpythonu;
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

    return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;

create or replace function move_xlog(source text, dest text)
returns text as $$
	import subprocess

	cmd = 'mkdir -p %s; ' % dest
	cmd = cmd + 'mv %s/0* %s' % (source, dest)

	return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;

-- Issue a checkpoint, and wait for it to be replayed on all segments.
create or replace function checkpoint_and_wait_for_replication_replay (retries int) returns bool as
$$
declare
	i int;
	checkpoint_locs pg_lsn[];
	replay_locs pg_lsn[];
	failed_for_segment text[];
	r record;
	all_caught_up bool;
begin
	i := 0;

	-- Issue a checkpoint.
	checkpoint;

	-- Get the WAL positions after the checkpoint records on every segment.
	for r in select gp_segment_id, pg_current_xlog_location() as loc from gp_dist_random('gp_id') loop
		checkpoint_locs[r.gp_segment_id] = r.loc;
	end loop;
	-- and the QD, too.
	checkpoint_locs[-1] = pg_current_xlog_location();

	-- Force some WAL activity, to nudge the mirrors to replay past the
	-- checkpoint location. There are some cases where a non-transactional
	-- WAL record is created right after the checkpoint record, which
	-- doesn't get replayed on the mirror until something else forces it
	-- out.
	drop table if exists dummy;
	create temp table dummy (id int4) distributed randomly;

	-- Wait until all mirrors have replayed up to the location we
	-- memorized above.
	loop
		all_caught_up = true;
		for r in select gp_segment_id, replay_location as loc from gp_stat_replication loop
			replay_locs[r.gp_segment_id] = r.loc;
			if r.loc < checkpoint_locs[r.gp_segment_id] then
				all_caught_up = false;
				failed_for_segment[r.gp_segment_id] = 1;
			else
				failed_for_segment[r.gp_segment_id] = 0;
			end if;
		end loop;

		if all_caught_up then
			return true;
		end if;

		if i >= retries then
			RAISE INFO 'checkpoint_locs:    %', checkpoint_locs;
			RAISE INFO 'replay_locs:        %', replay_locs;
			RAISE INFO 'failed_for_segment: %', failed_for_segment;
			return false;
		end if;
		perform pg_sleep(0.1);
		i := i + 1;
	end loop;
end;
$$ language plpgsql;

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

-- checkpoint to ensure clean xlog replication before bring down mirror
select checkpoint_and_wait_for_replication_replay(500);

create extension if not exists gp_inject_fault;
-- Prevent FTS from probing segments as we don't want a change in
-- cluster configuration to be triggered after the mirror is stoped
-- temporarily in the test.  Request a scan so that the skip fault is
-- triggered immediately, rather that waiting until the next probe
-- interval.
select gp_inject_fault_infinite('fts_probe', 'skip', 1);
select gp_request_fts_probe_scan();
select gp_wait_until_triggered_fault('fts_probe', 1, 1);

-- stop a mirror
select pg_ctl((select datadir from gp_segment_configuration c where c.role='m' and c.content=0), 'stop', NULL);

-- checkpoint and switch the xlog to avoid corrupting the xlog due to background processes
checkpoint;
-- substring() function is used to ignore the output, but not the error
select substring(pg_switch_xlog()::text, 0, 0) from gp_dist_random('gp_id') where gp_segment_id = 0;

-- hide old xlog on segment 0
select move_xlog((select datadir || '/pg_xlog' from gp_segment_configuration c where c.role='p' and c.content=0), '/tmp/missing_xlog');

-- bring the mirror back up
select pg_ctl((select datadir from gp_segment_configuration c where c.role='m' and c.content=0), 'start', (select port from gp_segment_configuration where content = 0 and preferred_role = 'm'));

-- check the view, we expect to see error
select wait_for_replication_error('walread', 0, 500);
select sync_error from gp_stat_replication where gp_segment_id = 0;

-- bring the missing xlog back on segment 0
select move_xlog('/tmp/missing_xlog', (select datadir || '/pg_xlog' from gp_segment_configuration c where c.role='p' and c.content=0));

-- the error should go away
select wait_for_replication_error('none', 0, 500);
select sync_error from gp_stat_replication where gp_segment_id = 0;

-- Resume FTS probes and perform a probe scan.
select gp_inject_fault('fts_probe', 'reset', 1);
select gp_request_fts_probe_scan();
-- Validate that the mirror for content=0 is marked up.
select count(*) = 2 as mirror_up from gp_segment_configuration
 where content=0 and status='u';
-- make sure leave the test only after mirror is in sync to avoid
-- affecting other tests. Thumb rule: leave cluster in same state as
-- test started.
select wait_for_mirror_sync(0::smallint);
select role, preferred_role, content, mode, status from gp_segment_configuration;
