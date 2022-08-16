include: helpers/server_helpers.sql;

-- Try to verify that a session fatal due to OOM should have no effect on other sessions.
-- Report on https://github.com/greenplum-db/gpdb/issues/12399

-- Because the number of errors reported to master can depend on ic types (i.e. ic-tcp and ic-proxy have one 
-- additional error from the backend on seg0 which is trying to tear down TCP connection), we have to ignore
-- all of them.
-- start_matchignore
-- m/ERROR:  Error on receive from seg0.*\n/
-- m/\tbefore or while.*\n/
-- m/\tThis probably means.*\n/
-- end_matchignore

create extension if not exists gp_inject_fault;

1: set gp_debug_linger = '1s';
1: select gp_inject_fault('make_dispatch_result_error', 'skip', dbid) from gp_segment_configuration where role = 'p' and content = -1;
2: begin;

-- session1 will fatal
1: select count(*) > 0 from gp_dist_random('pg_class');

-- session 2 should be ok
2: select count(*) > 0 from gp_dist_random('pg_class');
2: commit;
1q:
2q:

select gp_inject_fault('make_dispatch_result_error', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;

--
-- Test for issue https://github.com/greenplum-db/gpdb/issues/12703
--

-- Case for cdbgang_createGang_async
1: create table t_12703(a int);

1:begin;
-- make a cursor so that we have a named portal
1: declare cur12703 cursor for select * from t_12703;

-- next, trigger a segment down so the existing session will be reset
2: select gp_inject_fault('start_prepare', 'panic', dbid) from gp_segment_configuration where role = 'p' AND content = 0;
2: create table t_12703_2(a int);

-- this will go to cdbgang_createGang_async's code path
-- for some segments are DOWN. It should not PANIC even
-- with a named portal existing.
1: select * from t_12703;
1: abort;

1q:
2q:

-- Case for cdbCopyEndInternal
-- Provide some data to copy in
3:insert into t_12703 select * from generate_series(1, 10)i;
3:copy t_12703 to '/tmp/t_12703';
-- make copy in statement hang at the entry point of cdbCopyEndInternal
3:select gp_inject_fault('cdb_copy_end_internal_start', 'suspend', dbid) from gp_segment_configuration where role = 'p' and content = -1;
1&: copy t_12703 from '/tmp/t_12703';
select gp_wait_until_triggered_fault('cdb_copy_end_internal_start', 1, dbid) from gp_segment_configuration where role = 'p' and content = -1;
-- make Gang connection is BAD
4: select gp_inject_fault('start_prepare', 'panic', dbid) from gp_segment_configuration where role = 'p' AND content = 1;
4: create table t_12703_2(a int);
2: begin;
select gp_inject_fault('cdb_copy_end_internal_start', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;
-- continue copy it should not PANIC
1<:
1q:
-- session 2 still alive (means not PANIC happens)
2: select 1;
2: end;
2q:

-- start_ignore
-- For a mirrorless cluster this will return a non-zero code, so ignore all the output, we'll verify segments' status anyway.
!\ gprecoverseg -aF --no-progress;
!\ gprecoverseg -ar;
-- end_ignore

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

-- verify no segment is down after recovery
select count(*) from gp_segment_configuration where status = 'd';
