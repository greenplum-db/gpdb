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
