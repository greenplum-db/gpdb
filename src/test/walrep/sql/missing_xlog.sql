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
        opts = '-p %d -\-gp_dbid=0 -\-silent-mode=true -i -\-gp_contentid=%d -\-gp_num_contents_in_cluster=3' % (port, contentid)
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
	checkpoint_locs text[];
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
	create temp table dummy (id int4) distributed randomly;

	-- Wait until all mirrors have replayed up to the location we
	-- memorized above.
	loop
		all_caught_up = true;
		for r in select gp_segment_id, replay_location as loc from gp_stat_replication loop
			-- XXX: Using text comparison to compare XLOG positions
			-- is not quite right. Comparing 10/12345678 with
			-- 9/12345678 would yield incorrect result, for
			-- example. Ignore that for now, because this test is
			-- executed in a fresh test cluster, which surely
			-- hasn't written enough WAL yet to hit that problem.
			-- With WAL positions smaller than 10/00000000, this
			-- should work. PostgreSQL 9.4 got a pg_lsn datatype
			-- that we could use here, once we merge up to 9.4.
			if r.loc < checkpoint_locs[r.gp_segment_id] then
				all_caught_up = false;
			end if;
		end loop;

		if all_caught_up then
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

-- checkpoint to ensure clean xlog replication before bring down mirror
select checkpoint_and_wait_for_replication_replay(200);

-- stop a mirror 
select pg_ctl((select datadir from gp_segment_configuration c where c.role='m' and c.content=0), 'stop', NULL, NULL);

-- checkpoint and switch the xlog to avoid corrupting the xlog due to background processes
checkpoint;
-- substring() function is used to ignore the output, but not the error
select substring(pg_switch_xlog(), 0, 0) from gp_dist_random('gp_id') where gp_segment_id = 0;

-- hide old xlog on segment 0
select move_xlog((select datadir || '/pg_xlog' from gp_segment_configuration c where c.role='p' and c.content=0), '/tmp/missing_xlog');

-- bring the mirror back up
select pg_ctl((select datadir from gp_segment_configuration c where c.role='m' and c.content=0), 'start', (select port from gp_segment_configuration where content = 0 and preferred_role = 'm'), 0);

-- check the view, we expect to see error
select wait_for_replication_error('walread', 0, 200);
select sync_error from gp_stat_replication where gp_segment_id = 0;

-- bring the missing xlog back on segment 0
select move_xlog('/tmp/missing_xlog', (select datadir || '/pg_xlog' from gp_segment_configuration c where c.role='p' and c.content=0));

-- the error should go away
select wait_for_replication_error('none', 0, 100);
select sync_error from gp_stat_replication where gp_segment_id = 0;
