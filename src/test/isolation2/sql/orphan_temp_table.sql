-- Test orphan temp table on coordinator. 
-- Before the fix, when backend process panic on the segment, the temp table will be left on the coordinator.

-- create a temp table
1: CREATE TEMP TABLE test_temp_table_cleanup(a int);

-- panic on segment 0
2: SELECT gp_inject_fault('before_read_command', 'panic', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;
-- wait 1 seconds, the fault inject handler process will call ReadCommand in
-- PostgresMain loop, but the 'gp_inject_fault' query will return before that it
-- gets there.
1: select pg_sleep(1);

-- the backend process has exit due to segment reset.
1: SELECT * FROM test_temp_table_cleanup;

-- we should not see the temp table on the coordinator
1: SELECT oid, relname, relnamespace FROM pg_class where relname = 'test_temp_table_cleanup';

-- the temp table is left on segment 0, it should be dropped by autovacuum later
0U: SELECT relname FROM pg_class where relname = 'test_temp_table_cleanup';

-- no temp table left on other segments
1U: SELECT oid, relname, relnamespace FROM pg_class where relname = 'test_temp_table_cleanup';
