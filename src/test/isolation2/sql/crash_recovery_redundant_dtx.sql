1:CREATE TABLE crash_test_redundant(c1 int);

1:SELECT role, preferred_role, content, mode, status FROM gp_segment_configuration;
-- transaction of session 2 suspend after inserted 'COMMIT' record 
1:select gp_inject_fault_infinite('dtm_broadcast_commit_prepared', 'suspend', 1);
-- checkpoint suspend before scanning proc array
1:select gp_inject_fault_infinite('checkpoint_dtx_info', 'suspend', 1);
1&:CHECKPOINT;

-- wait till checkpoint reaches intended point
2:select gp_wait_until_triggered_fault('checkpoint_dtx_info', 1, 1);
-- the 'COMMIT' record is logically after REDO pointer
2&:insert into crash_test_redundant values (1);

-- resume checkpoint
3:select gp_inject_fault('checkpoint_dtx_info', 'reset', 1);
1<:

-- wait till insert reaches intended point
1:select gp_wait_until_triggered_fault('dtm_broadcast_commit_prepared', 1, 1);
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

-- transaction of session 2 should be recovered properly
4:select * from crash_test_redundant;
