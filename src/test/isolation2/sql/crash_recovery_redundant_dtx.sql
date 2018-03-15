1:CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
1:CREATE TABLE crash_test_redundant(c1 int);

-- transaction of session 2 suspend after inserted 'COMMIT' record 
1:select gp_inject_fault('dtm_broadcast_commit_prepared', 'suspend', 1);
-- checkpoint suspend before scanning proc array
1:select gp_inject_fault('checkpoint_dtx_info', 'suspend', 1);
1&:CHECKPOINT;

-- the 'COMMIT' record is logically after REDO pointer
2&:insert into crash_test_redundant values (1);

-- resume checkpoint
3:select gp_inject_fault('checkpoint_dtx_info', 'resume', 1);
1<:

-- trigger crash
1:select gp_inject_fault('before_read_command', 'panic', 1);
1:select 1;

2<:

-- transaction of session 2 should be recovered properly
4:select * from crash_test_redundant;
