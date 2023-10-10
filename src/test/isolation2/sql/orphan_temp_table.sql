-- Test orphan temp table on coordinator. 
-- Before the fix, when backend process panic on the segment, the temp table will be left on the coordinator.

-- create a temp table
1: CREATE TEMP TABLE test_temp_table_cleanup(a int);

-- panic on segment 0
1: SELECT gp_inject_fault('before_exec_scan', 'panic', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;

-- trigger 'before_exec_scan' panic in ExecScan
1: SELECT * FROM test_temp_table_cleanup;

-- we should not see the temp table on the coordinator
1: SELECT oid, relname, relnamespace FROM pg_class where relname = 'test_temp_table_cleanup';
-- we should not see the temp namespace on the coordinator
1: SELECT count(*) FROM pg_namespace where (nspname like '%pg_temp_%' or nspname like '%pg_toast_temp_%') and oid > 16386;


-- the temp table is left on segment 0, it should be dropped by autovacuum later
0U: SELECT relname FROM pg_class where relname = 'test_temp_table_cleanup';

-- no temp table left on other segments
1U: SELECT oid, relname, relnamespace FROM pg_class where relname = 'test_temp_table_cleanup';

1: SELECT gp_inject_fault('before_exec_scan', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;
