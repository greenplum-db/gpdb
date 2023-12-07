-- test that autovacuum cleans up orphaned temp table correctly

-- speed up test
1: alter system set autovacuum_naptime = 5;
1: alter system set autovacuum_vacuum_threshold = 50;
1: !\retcode gpstop -u;

-- session 1 is going to panic on primary segment 0, creating orphaned temp table on all QEs;
-- session 2 will remain, so the temp table on non-PANIC segment will remain too (until the session resets or exits)
1: create temp table tt_av1(a int);
2: create temp table tt_av2(a int);

-- temp tables created in utility mode: the one on the PANIC segment will be gone, but not other segments.
0U: create temp table ttu_av0(a int);
1U: create temp table ttu_av1(a int);

-- Inject a PANIC on one of the segment. Use something that's not going to be hit by background worker (including autovacuum).
1: select gp_inject_fault('create_function_fail', 'panic', dbid) from gp_segment_configuration where content=0 and role='p';
1: create function my_function() returns void as $$ begin end; $$ language plpgsql;

0Uq:
1q:

-- make sure the segment restarted,
-- also served as a third test case.
1: create temp table tt_av3(a int);

-- clear the fault
1: select gp_inject_fault('create_function_fail', 'reset', dbid) from gp_segment_configuration where content=0 and role='p';

-- make sure the autovacuum is run at least once,
-- it might've been run already, which is fine.
1: select gp_inject_fault('auto_vac_worker_after_report_activity', 'skip', '', '', 'pg_class', 1, 1, 0, dbid) from gp_segment_configuration where content=0 and role='p';
1: begin;
1: create table bloat_tbl(i int, j int, k int, l int, m int, n int, o int, p int) distributed by (i) partition by range (j) (start (0) end (1000) every (1));
1: abort;
-- wait for autovacuum to hit pg_class, triggering a fault
1: select gp_wait_until_triggered_fault('auto_vac_worker_after_report_activity', 1, dbid) from gp_segment_configuration where content=0 and role='p';
-- clean up fault
1: select gp_inject_fault('auto_vac_worker_after_report_activity', 'reset', dbid) from gp_segment_configuration where content=0 and role='p';

-- the orphaned temp table on all QEs should be cleaned up
0U: select count(*) from pg_class where relname = 'tt_av1';
1U: select count(*) from pg_class where relname = 'tt_av1';

-- the temp table that is associated with an existing session should be gone on the PANIC'ed
-- segment (because the QE is killed), but not other segments where QE is still there.
0U: select count(*) from pg_class where relname = 'tt_av2';
1U: select count(*) from pg_class where relname = 'tt_av2';

-- new temp table created is not affected
0U: select count(*) from pg_class where relname = 'tt_av3';
1U: select count(*) from pg_class where relname = 'tt_av3';

-- the utility-mode temp table on the PANIC'ed segment will be gone (because the utility connection
-- was killed), but not other segments where utility connection is still there.
0U: select count(*) from pg_class where relname = 'ttu_av0';
1U: select count(*) from pg_class where relname = 'ttu_av1';

-- restore settings
1: alter system reset autovacuum_naptime;
1: alter system reset autovacuum_vacuum_threshold;
1: !\retcode gpstop -u;
