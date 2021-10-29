include: helpers/server_helpers.sql;

-- setup
-- Set fsync on since we need to test the fsync code logic.
!\retcode gpconfig -c fsync -v on --skipvalidation;
-- Set create_restartpoint_on_ckpt_record_replay to trigger creating 
-- restart point easily.
!\retcode gpconfig -c create_restartpoint_on_ckpt_record_replay -v on --skipvalidation;
!\retcode gpstop -u;
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- test scenario

-- The test is to validate crash recovery can be completed after AO table
-- relfilenode file and its segments were removed by pg_rewind.
-- The scenario is when a checkpoint is executing on the primary segment,
-- a bunch of insert requests are received so generate a lot of insertion
-- XLOG records between the REO point and the LSN of the current checkpoint.
-- The primary segment then crashed after the checkpoint completed.
-- After the mirror segment is promoted to new primary, a truncate request
-- is received to truncate this AO table. Then both old relfilenode and its
-- segment files are removed when executing gprecoverseg. 
-- On the old primary (new mirror), those files are also removed but only
-- the old refilenode segment files will be re-generated due to the insert 
-- records during WAL replay. This will queue fsync request to checkpointer
-- and will not be forgotten in later flush. Later restart point creation will
-- flush the buffer to disk and expect to access the AO old base relfilenode
-- file (.0), while it was removed by pg_rewind previously, therefore PANIC
-- at mdsync during creating restart point. 
-- The fix is intended to remove the logic of accessing the base relfilenode
-- file when the target is a segment file in the above fsync scenario.

1: CREATE TABLE ao_fsync_panic_tbl(id int) WITH (appendonly=true);
1: CHECKPOINT;

-- Pause when creating checkpoint to make insert records between the
-- REDO point and the current checkpoint LSN.
2: SELECT gp_inject_fault_infinite('before_wait_VirtualXIDsDelayingChkpt', 'suspend', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
3&: CHECKPOINT;
2: SELECT gp_wait_until_triggered_fault('before_wait_VirtualXIDsDelayingChkpt', 1, dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
1: INSERT INTO ao_fsync_panic_tbl SELECT generate_series(1, 100);
2: SELECT gp_inject_fault_infinite('before_wait_VirtualXIDsDelayingChkpt', 'reset', dbid)
    FROM gp_segment_configuration WHERE ROLE = 'p' AND content = 0;
3<:

-- Create more insert records.
1: INSERT INTO ao_fsync_panic_tbl SELECT generate_series(200, 299);

-- Stop the primary immediately and promote the mirror.
2: SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
2: SELECT gp_request_fts_probe_scan();

-- Wait for the end of recovery CHECKPOINT completed after the mirror was promoted,
-- to ensure the CHECKPOINT record to be appended right after the END_OF_RECOVERY record.
-- Note, if the TRUNCATE operation interleaves between the END_OF_RECOVERY and the
-- CHECKPOINT record, PANIC might happen due to a fsync request being queued for an unlinked AO file,
-- is not forgotten in this case.
2: SELECT gp_inject_fault('checkpoint_after_redo_calculated', 'skip', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
2: SELECT gp_wait_until_triggered_fault('checkpoint_after_redo_calculated', 1, dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
2: SELECT gp_inject_fault('checkpoint_after_redo_calculated', 'reset', dbid)
    FROM gp_segment_configuration WHERE role = 'p' AND content = 0;

-- Expect to see the content 0, preferred primary is mirror and it's down;
-- the preferred mirror is primary and it's up and not-in-sync.
2: SELECT content, preferred_role, role, status, mode FROM gp_segment_configuration WHERE content = 0;

-- Truncate the AO table.
4: TRUNCATE ao_fsync_panic_tbl;
4: INSERT INTO ao_fsync_panic_tbl SELECT generate_series(300,399);

-- Do pg_rewind and crash recovery, should be no PANIC and completed successfully.
!\retcode gprecoverseg -a;
SELECT wait_until_all_segments_synchronized();

-- Re-balance the cluster.
!\retcode gprecoverseg -ra;
SELECT wait_until_all_segments_synchronized();

-- cleanup
DROP TABLE ao_fsync_panic_tbl;
!\retcode gpconfig -c fsync -v off --skipvalidation;
!\retcode gpconfig -r create_restartpoint_on_ckpt_record_replay --skipvalidation;
!\retcode gpstop -u;
