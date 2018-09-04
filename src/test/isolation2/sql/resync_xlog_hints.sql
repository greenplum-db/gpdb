-- start_ignore
create language plpythonu;
create language plpgsql;
CREATE EXTENSION gp_inject_fault;
-- end_ignore

3: create table resync_xlog_hints(a int, b int) distributed by (a);
3: insert into resync_xlog_hints values (1, 0);

create or replace function stop_segment(datadir text)
returns text as $$
    import subprocess
    cmd = 'pg_ctl -l postmaster.log -D %s -w -m immediate stop' % datadir
    return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).replace('.', '')
$$ language plpythonu;

-- Wait for content 0 to assume specified mode
create or replace function wait_for_content0(target_mode char) /*in func*/
returns void as $$ /*in func*/
declare /*in func*/
    iterations int := 0; /*in func*/ 
begin /*in func*/
    while iterations < 120 loop /*in func*/
        perform pg_sleep(1); /*in func*/
        if exists (select * from gp_segment_configuration where content = 0 and mode = target_mode) then /*in func*/
                return; /*in func*/
        end if; /*in func*/
        iterations := iterations + 1; /*in func*/
    end loop; /*in func*/
end $$ /*in func*/
language plpgsql;

-- Stop content 0 primary and let the mirror take over
select stop_segment(fselocation) from pg_filespace_entry fe, gp_segment_configuration c, pg_filespace f
where fe.fsedbid = c.dbid and c.content=0 and c.role='p' and f.oid = fe.fsefsoid and f.fsname = 'pg_system';

select wait_for_content0('c');

-- the following tuple hashes to seg0
1: insert into resync_xlog_hints values (1, 1);
1: insert into resync_xlog_hints values (1, 2);

-- fsync the buffer dirtied by previous inserts, so that it's clean again
1: checkpoint;

-- disable bgwriter and skip checkpoints so that a buffer dirtied by the following transactions is not written out asynchronously
1: select gp_inject_fault('fault_in_background_writer_main', 'suspend', dbid) from gp_segment_configuration where content = 0 and role = 'p';
1: select gp_inject_fault('checkpoint', 'skip', dbid) from gp_segment_configuration where content = 0 and role = 'p';


1: begin;
1: set gp_disable_tuple_hints = off;
1: select gp_inject_fault('changetracking_add_buffer', 'suspend', dbid) from gp_segment_configuration where content = 0 and role = 'p';
1&: select * from resync_xlog_hints;

2: begin;
2: set gp_disable_tuple_hints = off;
-- Session 2 should also be suspended due to WALInsert lock held by session 1.
2: select gp_inject_fault('changetracking_add_buffer', 'status', dbid) from gp_segment_configuration where content = 0 and role = 'p';
2&: select * from resync_xlog_hints;

-- Resume session 1.  That will result in XLOG_HINT record created in XLOG as well as CT log.
3: select gp_inject_fault('changetracking_add_buffer', 'reset', dbid) from gp_segment_configuration where content = 0 and role = 'p';

1<:
1: commit;
2<:
2: commit;

-- That should also resume session 2 as it can now acquire WALInsert lock.
-- Note that session 2's XLOG_HINT record follows that of session 1.
-- However, session 2 should have skipped setting page LSN because session 1 already marked the page dirty.


-- Ensure that subsequent checkpoints triggered by gprecoverseg will go through.
3: select gp_inject_fault('all', 'reset', dbid) from gp_segment_configuration where content = 0 and role = 'p';

3: checkpoint;

-- start_ignore
! gprecoverseg -a;
select wait_for_content0('s');
! gprecoverseg -ra;
select wait_for_content0('s');
-- end_ignore

select * from resync_xlog_hints;
