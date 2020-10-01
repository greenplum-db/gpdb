1:CREATE TABLE crash_test_table(c1 int);

1:SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
-- transaction of session 2 and session 3 inserted 'COMMIT' record before checkpoint
1:select gp_inject_fault_infinite('dtm_broadcast_commit_prepared', 'suspend', 1);
2&:insert into crash_test_table values (1), (11), (111), (1111);
3&:create table crash_test_ddl(c1 int);

-- wait session 2 and session 3 hit inject point
1:select gp_wait_until_triggered_fault('dtm_broadcast_commit_prepared', 2, 1);
1:CHECKPOINT;

-- transaction of session 4 inserted 'COMMIT' record after checkpoint
4&:insert into crash_test_table values (2), (22), (222), (2222);

-- wait session 4 hit inject point
1:select gp_wait_until_triggered_fault('dtm_broadcast_commit_prepared', 3, 1);

-- transaction of session 5 didn't insert 'COMMIT' record
1:select gp_inject_fault_infinite('transaction_abort_after_distributed_prepared', 'suspend', 1);
5&:INSERT INTO crash_test_table VALUES (3), (33), (333), (3333);

-- wait session 5 hit inject point
1:select gp_wait_until_triggered_fault('transaction_abort_after_distributed_prepared', 1, 1);

-- check injector status
1:select gp_inject_fault('dtm_broadcast_commit_prepared', 'status', 1);
1:select gp_inject_fault('transaction_abort_after_distributed_prepared', 'status', 1);

-- trigger crash
1:select gp_inject_fault('before_read_command', 'panic', 1);
-- verify master panic happens. The PANIC message does not emit sometimes so
-- mask it.
-- start_matchsubs
-- m/PANIC:  fault triggered, fault name:'before_read_command' fault type:'panic'\n/
-- s/PANIC:  fault triggered, fault name:'before_read_command' fault type:'panic'\n//
-- end_matchsubs
1:select 1;

2<:
3<:
4<:
5<:

-- transaction of session 2, session 3 and session 4 will be committed during recovery.
-- SELECT from a heap table, result order is uncertain.
-- Strangely, the test framework does not sort this output.
-- So to make things correct, we add `order by` here.
6:select * from crash_test_table order by 1;
6:select * from crash_test_ddl order by 1;
