-- Create an append only table, popluated with data
CREATE TABLE index_deadlocking_test_table (value int) WITH (appendonly=true);

-- Setup a fault to ensure that the first session pauses while creating an index,
-- ensuring a concurrent index creation.
SELECT gp_inject_fault('before_acquire_lock_during_create_ao_blkdir_table', 'suspend', 1);

-- Attempt to concurrently create an index
1>: CREATE INDEX index_deadlocking_test_table_idx1 ON index_deadlocking_test_table (value);
SELECT gp_wait_until_triggered_fault('before_acquire_lock_during_create_ao_blkdir_table', 1, 1);
2>: CREATE INDEX index_deadlocking_test_table_idx2 ON index_deadlocking_test_table (value);
SELECT gp_inject_fault('before_acquire_lock_during_create_ao_blkdir_table', 'reset', 1);

-- Both index creation attempts should succeed
1<:
2<:
