create or replace language plpythonu;


--
-- pg_ctl:
--   datadir: data directory of process to target with `pg_ctl`
--   command: commands valid for `pg_ctl`
--   command_mode: modes valid for `pg_ctl -m`  
--
create or replace function pg_ctl(datadir text, command text, command_mode text default 'immediate')
returns text as $$
    import subprocess
    if command == 'promote':
        cmd = 'pg_ctl promote -D %s' % datadir
    elif command in ('stop', 'restart'):
        cmd = 'pg_ctl -l postmaster.log -D %s ' % datadir
        cmd = cmd + '-w -m %s %s' % (command_mode, command)
    else:
        return 'Invalid command input'

    return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;


--
-- pg_ctl_start:
--
-- Start a specific greenplum segment
--
-- intentionally separate from pg_ctl() because it needs more information
--
--   datadir: data directory of process to target with `pg_ctl`
--   port: which port the server should start on
--
create or replace function pg_ctl_start(datadir text, port int)
returns text as $$
    import subprocess
    cmd = 'pg_ctl -l postmaster.log -D %s ' % datadir
    opts = '-p %d' % (port)
    cmd = cmd + '-o "%s" start' % opts
    return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;


--
-- restart_primary_segments_containing_data_for(table_name text):
--     table_name: the table containing data whose segment that needs a restart
--
-- Note: this does an immediate restart, which forces recovery
--
create or replace function restart_primary_segments_containing_data_for(table_name text) returns setof integer as $$
declare
	segment_id integer;
begin
	for segment_id in select * from primary_segments_containing_data_for(table_name)
	loop
		perform pg_ctl(
      (select get_data_directory_for(segment_id)),
      'restart',
      'immediate'
    );
	end loop;
end;
$$ language plpgsql;


--
-- clean_restart_primary_segments_containing_data_for(table_name text):
--     table_name: the table containing data whose segment that needs a restart
--
-- Note: this does a fast restart, which does not require recovery
--
create or replace function clean_restart_primary_segments_containing_data_for(table_name text) returns setof integer as $$
declare
	segment_id integer;
begin
	for segment_id in select * from primary_segments_containing_data_for(table_name)
	loop
		perform pg_ctl(
      (select get_data_directory_for(segment_id)),
      'restart',
      'fast'
    );
	end loop;
end;
$$ language plpgsql;


create or replace function primary_segments_containing_data_for(table_name text) returns setof integer as $$
begin
	return query execute 'select distinct gp_segment_id from ' || table_name;
end;
$$ language plpgsql;


create or replace function get_data_directory_for(segment_number int, segment_role text default 'p') returns text as $$
BEGIN
	return (
		select datadir 
		from gp_segment_configuration 
		where role=segment_role and 
		content=segment_number
	);
END;
$$ language plpgsql;

create or replace function master() returns setof gp_segment_configuration as $$
	select * from gp_segment_configuration where role='p' and content=-1;
$$ language sql;
