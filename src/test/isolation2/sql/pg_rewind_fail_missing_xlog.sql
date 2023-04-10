-- Ensure that the wal files up until the one which has the last CHECKPOINT wal
-- record BEFORE the oldest replication slot restart_lsn are not recycled even
-- if more than one CHECKPOINTs are performed after that restart_lsn. Otherwise
-- gprecoverseg (based on pg_rewind) would fail due to missing wal file

CREATE TABLE tst_missing_tbl (a int);
INSERT INTO tst_missing_tbl values(2),(1),(5);

-- Make the test faster by not preserving any extra wal segment files
!\retcode gpconfig -c wal_keep_size -v 0;
!\retcode gpstop -ari;

-- Test 1: Ensure that pg_rewind doesn't fail due to checkpoints inadvertently
-- recycling WAL when a former primary is marked down in configuration, while it
-- actually continues to run. The subsequent CHECKPOINT which is performed after the
-- failover on the segment being marked down should not recycle the wal file
-- which has the last common checkpoint of the target and source segment of
-- pg_rewind.

-- Run a checkpoint so that the below sqls won't cause a checkpoint
-- until an explicit checkpoint command is issued by the test.
-- checkpoint_timeout is by default 300 but the below test should be able to
-- finish in 300 seconds.
1: CHECKPOINT;

0U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
1: INSERT INTO tst_missing_tbl values(2),(1),(5);
-- Should be not needed mostly but let's 100% ensure since pg_switch_wal()
-- won't switch if it has been on the boundary (seldom though).
0U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
1: INSERT INTO tst_missing_tbl values(2),(1),(5);
0Uq:

-- Make sure primary/mirror pair is in sync, otherwise FTS can't promote mirror
1: SELECT wait_until_all_segments_synchronized();
-- Mark down the primary with content 0 via fts fault injection.
1: SELECT gp_inject_fault_infinite('fts_handle_message', 'error', dbid) FROM gp_segment_configuration WHERE content = 0 AND role = 'p';

-- Trigger failover and double check.
1: SELECT gp_request_fts_probe_scan();
1: SELECT role, preferred_role from gp_segment_configuration where content = 0;

-- Run two more checkpoints. Previously this causes the checkpoint.redo wal
-- file before the oldest replication slot LSN is recycled/removed.
0M: CHECKPOINT;
0M: BEGIN;
0M: DROP TABLE tst_missing_tbl;
0M: ABORT;
0M: CHECKPOINT;
0Mq:

-- Write something (promote adds a 'End Of Recovery' xlog that causes the
-- divergence between primary and mirror, but I add a write here so that we
-- know that a wal divergence is explicitly triggered and 100% completed.  Also
-- sanity check the tuple distribution (assumption of the test).
2: INSERT INTO tst_missing_tbl values(2),(1),(5);
2: SELECT gp_segment_id, count(*) from tst_missing_tbl group by gp_segment_id;

-- Ensure that pg_rewind succeeds. Previously it could fail since the divergence
-- LSN wal file is missing.
!\retcode gprecoverseg -av;
-- In case it fails it should not affect subsequent testing.
!\retcode gprecoverseg -aF;
2: SELECT wait_until_all_segments_synchronized();

-- Test 2: Ensure that pg_rewind doesn't fail due to checkpoints inadvertently
-- recycling WAL when a former primary was abnormally shutdown. In the case the
-- target segment was abnormally shutdown, pg_rewind starts and then stops
-- the target segment as single-user mode postgres to ensure clean shutdown which
-- causes two checkpoints. The two newer checkpoints introduced by pg_rewind
-- should not recycle the wal file that has the last common checkpoint of the
-- target and source segment of pg_rewind.

-- See previous comment for why.
3: CHECKPOINT;

1U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
3: INSERT INTO tst_missing_tbl values(2),(1),(5);
1U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
3: INSERT INTO tst_missing_tbl values(2),(1),(5);
1U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
3: INSERT INTO tst_missing_tbl values(2),(1),(5);
-- Should be not needed mostly but let's 100% ensure since pg_switch_wal()
-- won't switch if it is on the boundary already (seldom though).
1U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
3: INSERT INTO tst_missing_tbl values(2),(1),(5);

-- Hang at checkpointer before writing checkpoint xlog.
3: SELECT gp_inject_fault('checkpoint_after_redo_calculated', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;
1U&: CHECKPOINT;
3: SELECT gp_wait_until_triggered_fault('checkpoint_after_redo_calculated', 1, dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;

-- Stop the primary immediately and promote the mirror.
3: SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE role='p' AND content = 1;
3: SELECT gp_request_fts_probe_scan();
-- Wait for the end of recovery CHECKPOINT completed after the mirror was promoted
3: SELECT gp_inject_fault('checkpoint_after_redo_calculated', 'skip', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;
3: SELECT gp_wait_until_triggered_fault('checkpoint_after_redo_calculated', 1, dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
3: SELECT gp_inject_fault('checkpoint_after_redo_calculated', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
3: SELECT role, preferred_role from gp_segment_configuration where content = 1;

4: INSERT INTO tst_missing_tbl values(2),(1),(5);
4: SELECT gp_segment_id, count(*) from tst_missing_tbl group by gp_segment_id;

-- CHECKPOINT should fail now.
1U<:
1Uq:

-- Ensure that pg_rewind succeeds. For unclean shutdown, there are two
-- checkpoints are introduced in pg_rewind when running single-user mode postgres
-- (one is the checkpoint after crash recovery and another is the shutdown
-- checkpoint) and previously the checkpoints clean up the wal files that
-- include the previous checkpoint (before divergence LSN) for pg_rewind and
-- thus makes gprecoverseg (pg_rewind) fail.
!\retcode gprecoverseg -av;
-- In case it fails it should not affect subsequent testing.
!\retcode gprecoverseg -aF;
4: SELECT wait_until_all_segments_synchronized();

-- Test 3: Ensure that pg_rewind doesn't fail due to checkpoints inadvertently
-- recycling WAL when a former primary is marked down in configuration, while it
-- actually continued running for a while before it was cleanly shutdown. The wal
-- file on the target segment which has the last common checkpoint of the target
-- and source segment of pg_rewind should survive the subsequent CHECKPOINTs
-- performed on the target segment even if the segment was cleanly shutdown
-- and started again after the failover.

-- Run a checkpoint so that the below sqls won't cause a checkpoint
-- until an explicit checkpoint command is issued by the test.
-- checkpoint_timeout is by default 300 but the below test should be able to
-- finish in 300 seconds.
1: CHECKPOINT;

0U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
1: INSERT INTO tst_missing_tbl values(2),(1),(5);
-- Should be not needed mostly but let's 100% ensure since pg_switch_wal()
-- won't switch if it has been on the boundary (seldom though).
0U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
1: INSERT INTO tst_missing_tbl values(2),(1),(5);
0Uq:

-- Make sure primary/mirror pair is in sync, otherwise FTS can't promote mirror
1: SELECT wait_until_all_segments_synchronized();
-- Mark down the primary with content 0 via fts fault injection.
1: SELECT gp_inject_fault_infinite('fts_handle_message', 'error', dbid) FROM gp_segment_configuration WHERE content = 0 AND role = 'p';

-- Trigger failover and double check.
1: SELECT gp_request_fts_probe_scan();
1: SELECT role, preferred_role from gp_segment_configuration where content = 0;

-- Run two more checkpoints. Previously this causes the checkpoint.redo wal
-- file before the oldest replication slot LSN is recycled/removed.
0M: BEGIN;
0M: DROP TABLE tst_missing_tbl;
0M: ABORT;
0M: CHECKPOINT;
0M: BEGIN;
0M: DROP TABLE tst_missing_tbl;
0M: ABORT;
0M: CHECKPOINT;

-- Clean shutdown. Clean shutdown performs CHECKPOINT
2: SELECT pg_ctl(datadir, 'stop', 'fast') from gp_segment_configuration where role = 'm' and content = 0;
0Mq:

-- Start again. Start from a clean shutdown does not perform CHECKPOINT
2: SELECT pg_ctl_start(datadir, port) from gp_segment_configuration where role = 'm' and content = 0;

-- Perform CHECKPOINT. Previously this causes the checkpoint.redo wal
-- file before the oldest replication slot LSN is recycled/removed.
0M: CHECKPOINT;

-- Write something (promote adds a 'End Of Recovery' xlog that causes the
-- divergence between primary and mirror, but I add a write here so that we
-- know that a wal divergence is explicitly triggered and 100% completed.  Also
-- sanity check the tuple distribution (assumption of the test).
2: INSERT INTO tst_missing_tbl values(2),(1),(5);
2: SELECT gp_segment_id, count(*) from tst_missing_tbl group by gp_segment_id;

-- Ensure that pg_rewind succeeds. Previously it could fail since the divergence
-- LSN wal file is missing.
!\retcode gprecoverseg -av;
-- In case it fails it should not affect subsequent testing.
!\retcode gprecoverseg -aF;
2: SELECT wait_until_all_segments_synchronized();

-- Test 4: Ensure that pg_rewind doesn't fail due to checkpoints inadvertently
-- recycling WAL when a former primary was abnormally shutdown and it wrote
-- a CHECKPOINT record locally but the record did not make to the wal receiver.
-- In the case the target segment was abnormally shutdown, pg_rewind starts and
-- then stops the target segment as single-user mode postgres to ensure clean shutdown
-- which causes two checkpoints. The two newer checkpoints introduced by pg_rewind
-- plus the checkpoint during the unclean shutdown should not recycle the wal
-- file that has the last common checkpoint of the target and source segment of
-- pg_rewind.

-- See previous comment for why.
3: CHECKPOINT;
1U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
3: INSERT INTO tst_missing_tbl values(2),(1),(5);
-- Should be not needed mostly but let's 100% ensure since pg_switch_wal()
-- won't switch if it is on the boundary already (seldom though).
1U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
3: INSERT INTO tst_missing_tbl values(2),(1),(5);

-- Have primary/mirror pair in sync before suspending the wal sender.
3: SELECT wait_until_all_segments_synchronized();

-- Hang the wal sender before writing checkpoint wal record.
3: SELECT gp_inject_fault('wal_sender_loop', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;
3: SELECT gp_wait_until_triggered_fault('wal_sender_loop', 1, dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;

-- Checkpoint and make sure the CHECKPOINT record is written on disk
3: SELECT gp_inject_fault('checkpoint_control_file_updated', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;
1U&: CHECKPOINT;
3: SELECT gp_wait_until_triggered_fault('checkpoint_control_file_updated', 1, dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;

-- Stop the primary immediately and promote the mirror.
3: SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE role='p' AND content = 1;
3: SELECT gp_request_fts_probe_scan();

-- Reset faults and confirm FTS configuration
3: SELECT gp_inject_fault('wal_sender_loop', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;
3: SELECT gp_inject_fault('checkpoint_control_file_updated', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
3: SELECT role, preferred_role from gp_segment_configuration where content = 1;

-- Write something on the current primary
4: INSERT INTO tst_missing_tbl values(2),(1),(5);
4: SELECT gp_segment_id, count(*) from tst_missing_tbl group by gp_segment_id;

-- CHECKPOINT should fail now.
1U<:
1Uq:

-- Ensure that pg_rewind succeeds. For unclean shutdown, there are two
-- checkpoints are introduced in pg_rewind when running single-user mode postgres
-- (one is the checkpoint after crash recovery and another is the shutdown
-- checkpoint) and previously the checkpoints clean up the wal files that
-- include the previous checkpoint (before divergence LSN) for pg_rewind and
-- thus makes gprecoverseg (pg_rewind) fail.
!\retcode gprecoverseg -av;
-- In case it fails it should not affect subsequent testing.
!\retcode gprecoverseg -aF;
4: SELECT wait_until_all_segments_synchronized();

-- Test 5: Ensure that replication slot's restart_lsn advances and WAL files
-- that are older than the minimum restart_lsn get removed/recycled.

-- Turn off wal_recycle to make this test easier to write.
!\retcode gpconfig -c wal_recycle -v false;
!\retcode gpstop -ari;

-- Create an unlogged table on the primary that remembers replication slot's last restart_lsn and number of WAL files.
1U: CREATE UNLOGGED TABLE unlogged_wal_retention_test(restart_lsn_before pg_lsn, wal_count_before int);
1U: INSERT INTO unlogged_wal_retention_test SELECT (select restart_lsn FROM pg_replication_slots WHERE slot_name = 'internal_wal_replication_slot') as restart_lsn_before, (select count(*) from pg_ls_waldir()) as wal_count_before;
5: CHECKPOINT;
-- Replication slot's restart_lsn should advance to the checkpoint's redo location.
1U: SELECT pg_wal_lsn_diff(restart_lsn, restart_lsn_before) > 0 from pg_replication_slots, unlogged_wal_retention_test WHERE slot_name = 'internal_wal_replication_slot';
1U: SELECT pg_wal_lsn_diff(restart_lsn, redo_lsn) = 0 from pg_replication_slots, pg_control_checkpoint() WHERE slot_name = 'internal_wal_replication_slot';
-- Some old WALs should be removed
1U: select ((select count(*)::int from pg_ls_waldir()) - wal_count_before) < 0 FROM unlogged_wal_retention_test;
-- Record the restart_lsn and the WAL file count.
1U: UPDATE unlogged_wal_retention_test SET restart_lsn_before = (SELECT restart_lsn from pg_replication_slots WHERE slot_name = 'internal_wal_replication_slot'), wal_count_before = (select count(*) from pg_ls_waldir());
-- Write some records to a newer WAL file.
1U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
5: INSERT INTO tst_missing_tbl values(2),(1),(5);
-- Replication slot's restart_lsn should NOT change regardless mirror has received more wals.
1U: select pg_wal_lsn_diff(restart_lsn, restart_lsn_before) = 0 FROM pg_replication_slots, unlogged_wal_retention_test WHERE slot_name = 'internal_wal_replication_slot';
-- Record the WAL file count.
1U: UPDATE unlogged_wal_retention_test SET wal_count_before = (select count(*) from pg_ls_waldir());

-- Hang the wal sender before writing more wals.
5: SELECT gp_inject_fault('wal_sender_loop', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;
-- Perform checkpoint. This is to mimic walreciver's confirmed received lsn is behind the redo lsn.
1U: CHECKPOINT;

-- Replication slot's restart_lsn should NOT change regardless a new checkpoints was performed.
1U: select pg_wal_lsn_diff(restart_lsn, restart_lsn_before) = 0 FROM pg_replication_slots, unlogged_wal_retention_test WHERE slot_name = 'internal_wal_replication_slot';
-- Replication slot's restart_lsn should be smaller than the checkpoint's redo location.
1U: SELECT pg_wal_lsn_diff(restart_lsn, redo_lsn) < 0 from pg_replication_slots, pg_control_checkpoint() WHERE slot_name = 'internal_wal_replication_slot';

-- Resume walsender.
5: SELECT gp_inject_fault('wal_sender_loop', 'reset', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 1;
-- Perform transaction to make sure wals are in sync.
5: INSERT INTO tst_missing_tbl values(2),(1),(5);
-- Replication slot's restart_lsn should now advance to the checkpoint's redo location.
1U: SELECT pg_wal_lsn_diff(restart_lsn, restart_lsn_before) > 0 from pg_replication_slots, unlogged_wal_retention_test WHERE slot_name = 'internal_wal_replication_slot';
1U: SELECT pg_wal_lsn_diff(restart_lsn, redo_lsn) = 0 from pg_replication_slots, pg_control_checkpoint() WHERE slot_name = 'internal_wal_replication_slot';
-- Trigger old WAL removal.
1U: CHECKPOINT;
-- And the WAL file(s) older than the redo lsn that we previously kept should now be removed.
1U: select ((select count(*)::int from pg_ls_waldir()) - wal_count_before) < 0 FROM unlogged_wal_retention_test;

-- Cleanup
1U: DROP TABLE unlogged_wal_retention_test;
1Uq:

5: DROP TABLE tst_missing_tbl;
!\retcode gpconfig -r wal_keep_size;
!\retcode gpconfig -r wal_recycle;
!\retcode gpstop -ari;
