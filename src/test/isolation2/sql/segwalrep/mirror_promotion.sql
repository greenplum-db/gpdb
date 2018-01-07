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

-- stop a primary in order to trigger a mirror promotion
select pg_ctl((select datadir from gp_segment_configuration
	where role='p' and content=0), 'stop', NULL, NULL);

-- trigger failover
select gp_request_fts_probe_scan();

-- expect: to see the content 0, preferred primary is mirror and it's down
-- the preferred mirror is primary and it's up and not-in-sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- wait for dbid 5 (mirror for content 0) to finish the promotion
5U: select 1;

-- fully recover the failed primary as new mirror
!\retcode ../../../gpAux/gpdemo/gpsegwalrep.py recoverfull;

-- expect: to see the new rebuilt mirror up and in sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- now, let's stop the new primary, so that we can restore original role
select pg_ctl((select datadir from gp_segment_configuration
	where role='p' and content=0), 'stop', NULL, NULL);

-- trigger failover
select gp_request_fts_probe_scan();

-- expect segments restored back to its preferred role, but mirror is down
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

-- wait for dbid 2 (primary for content 0) finish promotion
2U: select 1;

-- now, let's fully recover the mirror
!\retcode ../../../gpAux/gpdemo/gpsegwalrep.py recoverfull;

-- now, the content 0 primary and mirror should be at their preferred role
-- and up and in-sync
select content, preferred_role, role, status, mode
from gp_segment_configuration
where content = 0;

