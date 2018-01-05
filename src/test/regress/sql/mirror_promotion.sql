-- start_ignore
create language plpythonu;
create language plpgsql;
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

create or replace function wait_for_replication_replay (retries int) returns bool as
$$
declare
    i int;
begin
    i := 0;
    loop
        if (select count(*) from gp_stat_replication) =
            (select count(*) from gp_stat_replication
                where replay_location = sent_location) then
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

-- stop a primary in order to trigger a mirror promotion
select pg_ctl((select fselocation from gp_segment_configuration c,
pg_filespace_entry f where c.role='p' and c.content=0 and c.dbid = f.fsedbid), 'stop', NULL, NULL);

-- force fts probe
select gp_request_fts_probe_scan();

-- expect: to see the content 0, preferred primary is mirror and it's down
-- the preferred mirror is primary and it's up and not-in-sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- rebuild mirror for content 0
\! ../../../gpAux/gpdemo/gpsegwalrep.py rebuild --content 0

-- force fts probe
select gp_request_fts_probe_scan();

-- wait for the replication to finish, timeout in 20 secs
select wait_for_replication_replay(200);

-- expect: to see the new rebuilt mirror up and in sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- now, let's stop the new primary, so that we can restore original role
select pg_ctl((select fselocation from gp_segment_configuration c,
pg_filespace_entry f where c.role='p' and c.content=0 and c.dbid = f.fsedbid), 'stop', NULL, NULL);

-- force fts probe
select gp_request_fts_probe_scan();

-- expect segments restored back to its preferred role, but mirror is down
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- now, let's rebuild the mirror
\! ../../../gpAux/gpdemo/gpsegwalrep.py rebuild --content 0

-- force fts probe
select gp_request_fts_probe_scan();

-- wait for replication sync again.
select wait_for_replication_replay(200);

-- now, the content 0 primary and mirror should be at their preferred role
-- and up and in-sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

