-- Try to verify that a session fatal due to OOM should have no effect on other sessions.
-- Report on https://github.com/greenplum-db/gpdb/issues/12399

create extension if not exists gp_inject_fault;

1: select gp_inject_fault('make_dispatch_result_error', 'skip', dbid) from gp_segment_configuration where role = 'p' and content = -1;
2: begin;

-- session1 will be fatal.
1: select count(*) > 0 from gp_dist_random('pg_class');

-- session2 should be ok.
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

2: select pg_ctl((select datadir from gp_segment_configuration c where c.role='p' and c.content=1), 'stop');
-- next sql will trigger FTS to mark seg1 as down
2: select gp_request_fts_probe_scan();

-- this will go to cdbgang_createGang_async's code path
-- for some segments are DOWN. It should not PANIC even
-- with a named portal existing.
1: select * from t_12703;
1: abort;

1q:
2q:

-- Case for cdbCopyEndInternal
-- Provide some data to copy in
insert into t_12703 select * from generate_series(1, 10)i;
copy t_12703 to '/tmp/t_12703';
-- make copy in statement hang at the entry point of cdbCopyEndInternal
select gp_inject_fault('cdb_copy_end_internal_start', 'suspend', dbid) from gp_segment_configuration where role = 'p' and content = -1;
1&: copy t_12703 from '/tmp/t_12703';
select gp_wait_until_triggered_fault('cdb_copy_end_internal_start', 1, dbid) from gp_segment_configuration where role = 'p' and content = -1;
-- make Gang connection is BAD
select pg_ctl((select datadir from gp_segment_configuration c where c.role='p' and c.content=2), 'stop');
2: select gp_request_fts_probe_scan();
2: begin;
select gp_inject_fault('cdb_copy_end_internal_start', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;
-- continue copy it should not PANIC
1<:
1q:
-- session 2 still alive (means not PANIC happens)
2: select 1;
2: end;
2q:

!\retcode gprecoverseg -aF --no-progress;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

!\retcode gprecoverseg -ar;

-- loop while segments come in sync
select wait_until_all_segments_synchronized();

-- verify no segment is down after recovery
select count(*) from gp_segment_configuration where status = 'd';
