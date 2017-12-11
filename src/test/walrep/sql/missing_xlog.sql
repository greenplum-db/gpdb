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

create or replace function move_xlog(source text, dest text)
returns text as $$
	import subprocess

	cmd = 'mkdir -p %s; ' % dest
	cmd = cmd + 'mv %s/0* %s' % (source, dest)
	
	return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;

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

-- stop a mirror 
select pg_ctl((select fselocation from gp_segment_configuration c, pg_filespace_entry f where c.role='m' and c.content=0 and c.dbid = f.fsedbid), 'stop', NULL, NULL);

-- checkpoint and switch the xlog to avoid corrupting the xlog due to background processes
checkpoint;
-- substring() function is used to ignore the output, but not the error
select substring(pg_switch_xlog(), 0, 0) from gp_dist_random('gp_id') where gp_segment_id = 0;

-- hide old xlog on segment 0
select move_xlog((select fselocation || '/pg_xlog' from gp_segment_configuration c, pg_filespace_entry f where c.role='p' and c.content=0 and c.dbid=f.fsedbid), '/tmp/missing_xlog');

-- bring the mirror back up
select pg_ctl((select fselocation from gp_segment_configuration c, pg_filespace_entry f where c.role='m' and c.content=0 and c.dbid = f.fsedbid), 'start', (select port from gp_segment_configuration where content = 0 and preferred_role = 'm'), 0);

-- check the view, we expect to see error
select wait_for_replication_error('walread', 0, 200);
select sync_error from gp_stat_replication where gp_segment_id = 0;

-- bring the missing xlog back on segment 0
select move_xlog('/tmp/missing_xlog', (select fselocation || '/pg_xlog' from gp_segment_configuration c, pg_filespace_entry f where c.role='p' and c.content=0 and c.dbid=f.fsedbid));

-- the error should go away
select wait_for_replication_error('none', 0, 100);
select sync_error from gp_stat_replication where gp_segment_id = 0;
