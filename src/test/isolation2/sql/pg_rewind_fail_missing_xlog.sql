-- Test the bug that if checkpoint.redo before the oldest replication slot LSN
-- is removed/recylced in checkpointer, gprecoverseg (based on pg_rewind) would
-- would fail.

CREATE TABLE tst_missing_tbl (a int);
INSERT INTO tst_missing_tbl values(2),(1),(5);

-- make the test faster.
!\retcode gpconfig -c wal_keep_segments -v 2;
!\retcode gpstop -ari;

-- Test 1: primary was marked down by the master but acetually it keeps running
-- and previously, checkpoints could recycle/remove the checkpoint.redo wal
-- file before the oldest replication slot LSN and thus make pg_rewind fail due
-- to missing xlog file.

-- Run a checkpoint so that the below sqls won't cause a checkpoint
-- until an explicit checkpoint command is issued by the test.
-- checkpoint_timeout is by default 300 but the below test should be able to
-- finish in 300 seconds.
1: CHECKPOINT;

0U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
1: INSERT INTO tst_missing_tbl values(2),(1),(5);
0U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
1: INSERT INTO tst_missing_tbl values(2),(1),(5);
0U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
1: INSERT INTO tst_missing_tbl values(2),(1),(5);
-- Should be not needed mostly but let's 100% ensure since pg_switch_wal()
-- won't switch if it has been on the boundary (seldom though).
0U: SELECT pg_switch_wal is not null FROM pg_switch_wal();
1: INSERT INTO tst_missing_tbl values(2),(1),(5);

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

-- Test 2
-- primary is abnormally shutdown, but pg_rewind would call single mode
-- postgres to ensure it clean shutdown and that causes two checkpoints.

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
-- checkpoints are introduced in pg_rewind when running single-mode postgres
-- (one is the checkpoint after crash recovery and another is the shutdown
-- checkpoint) and previously the checkpoints clean up the wal files that
-- include the previous checkpoint (before divergence LSN) for pg_rewind and
-- thus makes gprecoverseg (pg_rewind) fail.
!\retcode gprecoverseg -av;
-- In case it fails it should not affect subsequent testing.
!\retcode gprecoverseg -aF;
4: SELECT wait_until_all_segments_synchronized();

-- Cleanup
5: DROP TABLE tst_missing_tbl;
!\retcode gprecoverseg -ar;
5: SELECT wait_until_all_segments_synchronized();
!\retcode gpconfig -r wal_keep_segments;
!\retcode gpstop -ari;
