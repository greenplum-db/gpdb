1:CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
1:CREATE TABLE crash_test_table(c1 int);

-- transaction of session 2 and session 3 inserted 'COMMIT' record before checkpoint
1:select gp_inject_fault('dtm_broadcast_commit_prepared', 'suspend', 1);
2&:insert into crash_test_table values (1);
3&:create table crash_test_ddl(c1 int);

-- wait session 2 and session 3 hit inject point
1:select gp_inject_fault('dtm_broadcast_commit_prepared', 'wait_until_triggered', '', '', '', 2, 0, 1);
1:CHECKPOINT;

-- transaction of session 4 inserted 'COMMIT' record after checkpoint
4&:insert into crash_test_table values (2);

-- wait session 4 hit inject point
1:select gp_inject_fault('dtm_broadcast_commit_prepared', 'wait_until_triggered', '', '', '', 3, 0, 1);

-- transaction of session 5 didn't insert 'COMMIT' record
1:select gp_inject_fault('transaction_abort_after_distributed_prepared', 'suspend', 1);
5&:INSERT INTO crash_test_table VALUES (3);

-- wait session 5 hit inject point
1:select gp_inject_fault('transaction_abort_after_distributed_prepared', 'wait_until_triggered', '', '', '', 1, 0, 1);

-- check injector status
1:select gp_inject_fault('dtm_broadcast_commit_prepared', 'status', 1);
1:select gp_inject_fault('transaction_abort_after_distributed_prepared', 'status', 1);

-- trigger crash
1:select gp_inject_fault('before_read_command', 'panic', 1);
1:select 1;

2<:
3<:
4<:
5<:

-- transaction of session 2, session 3 and session 4 will be committed during recovery.
6:select * from crash_test_table;
6:select * from crash_test_ddl;
