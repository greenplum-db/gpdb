-- start_ignore
create language plpythonu;
create language plpgsql;
CREATE EXTENSION gp_inject_fault;
-- end_ignore

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

create table resync_shutdown1(a int, b varchar) distributed by (a);
create table resync_shutdown2(a int, b varchar)
with (appendonly=true) distributed by (a);

-- Stop content 0 mirror so that primary transitions to change tracking
select stop_segment(fselocation) from pg_filespace_entry fe, gp_segment_configuration c, pg_filespace f
where fe.fsedbid = c.dbid and c.content=0 and c.role='m' and f.oid = fe.fsefsoid and f.fsname = 'pg_system';

select wait_for_content0('c');

-- generate enough work for resync workers
insert into resync_shutdown1 select 1, i from generate_series(1,100)i;
insert into resync_shutdown2 select * from resync_shutdown1;
create table resync_shutdown3(a int, b varchar)
with (appendonly=true, orientation=column) distributed by (a);
insert into resync_shutdown3 select * from resync_shutdown1;
create table resync_shutdown4(a int, b varchar)
with (appendonly=true, orientation=column) distributed by (a);
insert into resync_shutdown4 select * from resync_shutdown1;
create table resync_shutdown5(a int, b varchar)
with (appendonly=true) distributed by (a);
insert into resync_shutdown5 select * from resync_shutdown1;
create table resync_shutdown6(a int, b varchar) distributed by (a);
insert into resync_shutdown6 select * from resync_shutdown1;
alter table resync_shutdown6 add column c int default 1;

-- Skip fault in resync-backend specific branch in shutdown handler
select gp_inject_fault_new('filerep_resync_shutdown', 'skip', dbid)
from gp_segment_configuration where content=0 and role='p';

-- Suspend a resync worker after one relation has been completely
-- resynchronized.  That's when resync manager updates the PT entry
-- and performs xlog flush.
select gp_inject_fault_new('filerep_resync_one_rel_complete', 'suspend', dbid)
from gp_segment_configuration where content=0 and role='p';

-- Inject fault to suspend xlog flush in critical section.  Resync
-- manager performs xlog flush during resync, after all workers have
-- been started.
select gp_inject_fault_new('filerep_resync_manager_xlog_flush', 'suspend', dbid)
from gp_segment_configuration where content=0 and role='p';

-- start_ignore
! gprecoverseg -a;
-- end_ignore

select gp_wait_until_triggered_fault('filerep_resync_one_rel_complete', 1, dbid)
from gp_segment_configuration where content=0 and role='p';

-- Resync manager should now be updating the PT entry for the
-- completed relation and hit the suspend fault in critical section of
-- xlog flush.
select gp_wait_until_triggered_fault('filerep_resync_manager_xlog_flush', 1, dbid)
from gp_segment_configuration where content=0 and role='p';

-- Now let one worker error out.
select gp_inject_fault_new('filerep_resync_worker', 'error', dbid)
from gp_segment_configuration where content=0 and role='p';

-- Unleash the suspended worker.
select gp_inject_fault_new('filerep_resync_one_rel_complete', 'reset', dbid)
from gp_segment_configuration where content=0 and role='p';

-- start_ignore
! ps -ef | grep '[r]esync manager';
-- end_ignore

-- Verify that shutdown singal handler was called for resync manager,
-- while it was suspended.  Such a situation would lead to PANIC.
select gp_wait_until_triggered_fault('filerep_resync_shutdown', 1, dbid)
from gp_segment_configuration where content=0 and role='p';

-- Unleash the resync manager.  It should receive the shutdown signal
-- once again after this, and that's when it will actually shutdown.
select gp_inject_fault_new('filerep_resync_manager_xlog_flush', 'reset', dbid)
from gp_segment_configuration where content=0 and role='p';

-- Verify that shutdown signal was received by resync manager the
-- second time.
select gp_wait_until_triggered_fault('filerep_resync_shutdown', 2, dbid)
from gp_segment_configuration where content=0 and role='p';

-- Reset all faults
select gp_inject_fault('all', 'reset', dbid)
from gp_segment_configuration where content=0 and role='p';

-- start_ignore
! gprecoverseg -a;
-- end_ignore

select wait_for_content0('s');
