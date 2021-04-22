-- Test a pg_rewind failure bug. (See the test end for details).
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
include: helpers/server_helpers.sql;

CREATE TABLE tst_missing_tbl (a int);
INSERT INTO tst_missing_tbl values(2),(1),(5);

-- Run a checkpoint so that the below sqls won't cause a checkpoint
-- until an explicit checkpoint command is issued by the test.
-- checkpoint_timeout is by default 300 but below test should be able
-- to finish in 300 seconds.
CHECKPOINT;

-- Based on assumption that wal_keep_segments is 5 and gp_keep_all_xlog is off
-- (All are default values).
SHOW wal_keep_segments;
SHOW gp_keep_all_xlog;
0U: SELECT pg_switch_xlog is not null FROM pg_switch_xlog();
INSERT INTO tst_missing_tbl values(2),(1),(5);
0U: SELECT pg_switch_xlog is not null FROM pg_switch_xlog();
INSERT INTO tst_missing_tbl values(2),(1),(5);
0U: SELECT pg_switch_xlog is not null FROM pg_switch_xlog();
INSERT INTO tst_missing_tbl values(2),(1),(5);
0U: SELECT pg_switch_xlog is not null FROM pg_switch_xlog();
INSERT INTO tst_missing_tbl values(2),(1),(5);
0U: SELECT pg_switch_xlog is not null FROM pg_switch_xlog();
INSERT INTO tst_missing_tbl values(2),(1),(5);
0U: SELECT pg_switch_xlog is not null FROM pg_switch_xlog();
INSERT INTO tst_missing_tbl values(2),(1),(5);
-- Should be not needed mostly but let's 100% ensure since pg_switch_xlog()
-- won't switch if it is on the boundary already (seldom though).
0U: SELECT pg_switch_xlog is not null FROM pg_switch_xlog();
INSERT INTO tst_missing_tbl values(2),(1),(5);

-- Hang at checkpointer before writing checkpoint xlog.
SELECT gp_inject_fault('checkpoint_after_redo_calculated', 'suspend', dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;
0U&: CHECKPOINT;
SELECT gp_wait_until_triggered_fault('checkpoint_after_redo_calculated', 1, dbid) FROM gp_segment_configuration WHERE role='p' AND content = 0;

-- Stop the primary immediately and promote the mirror.
SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE role='p' AND content = 0;
SELECT gp_request_fts_probe_scan();
SELECT role, preferred_role from gp_segment_configuration where content = 0;

-- Write something (promote adds a 'End Of Recovery' xlog that causes the
-- divergence between primary and mirror,but I add a write here so that we know
-- that a wal divergence is explicitly triggered and 100% completed.  Also
-- sanity check the tuple distribution (assumption of the test).
1: INSERT INTO tst_missing_tbl values(2),(1),(5);
1: SELECT gp_segment_id, count(*) from tst_missing_tbl group by gp_segment_id;

-- CHECKPOINT should fail now.
0U<:

-- Ensure that pg_rewind succeeds. For unclean shutdown, there are two
-- checkpoints are introduced in pg_rewind when running single-mode postgres
-- (one is the checkpoint after crash recovery and another is the shutdown
-- checkpoint) and previously the checkpoints clean up the wal files that
-- include the previous checkpoint (before divergence LSN) for pg_rewind and
-- thus makes gprecoverseg (pg_rewind) fail.
!\retcode gprecoverseg -a -v;
SELECT wait_until_all_segments_synchronized();
!\retcode gprecoverseg -ar;
SELECT wait_until_all_segments_synchronized();
1: DROP TABLE tst_missing_tbl;
