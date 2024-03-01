-- Test orphan temp table on coordinator. 

-- case 1: Before the fix, when backend process panic on the segment, the temp table will be left on the coordinator.Before the fix, when backend process panic on the segment, the temp table will be left on the coordinator.
-- create a temp table
1: CREATE TEMP TABLE test_temp_table_cleanup(a int);

-- panic on segment 0
1: SELECT gp_inject_fault('before_exec_scan', 'panic', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;

-- trigger 'before_exec_scan' panic in ExecScan
1: SELECT * FROM test_temp_table_cleanup;

-- make sure seg0 is recovered from panic
1: SELECT count(*) FROM gp_dist_random('gp_id');

-- we should not see the temp table on the coordinator
1: SELECT oid, relname, relnamespace FROM pg_class where relname = 'test_temp_table_cleanup';

1: SELECT gp_inject_fault('before_exec_scan', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;
1q:

-- case 2: Test if temp table will be left on the coordinator, when session exits in coordinator within a transaction block.
2: CREATE TEMP TABLE test_temp_table_cleanup(a int);

-- make sure that RemoveTempRelationsCallback is called before validation
3: SELECT gp_inject_fault_infinite('remove_temp_relation_callback', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content = -1;
3&: SELECT gp_wait_until_triggered_fault('remove_temp_relation_callback', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = -1;

2: begin;
2: select * from test_temp_table_cleanup;
2q:

3<:
3: SELECT gp_inject_fault('remove_temp_relation_callback', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content = -1;
3: select count(*) from pg_class where relname = 'test_temp_table_cleanup';
3q:

