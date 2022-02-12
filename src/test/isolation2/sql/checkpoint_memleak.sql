include: helpers/server_helpers.sql;

-- setup
-- Set fsync on since we need to test the fsync code logic.
!\retcode gpconfig -c fsync -v on --skipvalidation;
-- Set create_restartpoint_on_ckpt_record_replay to trigger creating 
-- restart point easily.
!\retcode gpconfig -c create_restartpoint_on_ckpt_record_replay -v on --skipvalidation;
!\retcode gpstop -u;
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

CREATE TABLE t_ckpt_memleak_test(id int) WITH (appendonly=true);
INSERT INTO t_ckpt_memleak_test values(1);
CHECKPOINT;

-- Inject fault to trigger the code that will check memory stats around checkpoint
select gp_inject_fault_infinite('ckpt_mem_leak', 'skip', dbid) from gp_segment_configuration where role = 'm' and content = 1;

-- Remember the current timestamp to filter the log later
CREATE TABLE t_ckpt_memleak_test_timestamp AS SELECT CURRENT_TIMESTAMP;

-- Do the checkpointing that we're going to examine
INSERT INTO t_ckpt_memleak_test values(1);
CHECKPOINT;

-- Wait until checkpoint is done (the mirror will hit this fault 2 times, before and after checkpointing), and then clear the fault
select gp_wait_until_triggered_fault('ckpt_mem_leak', 2, dbid) from gp_segment_configuration where role = 'm' and content = 1;
select gp_inject_fault('ckpt_mem_leak', 'reset', dbid) from gp_segment_configuration where role = 'm' and content = 1;

-- We have to stop the primary and promote the mirror in order to run gp_toolkit.__gp_log_segment_ext 
-- on the new primary to check the old mirror's log.
SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE role = 'p' AND content = 1;
SELECT gp_request_fts_probe_scan();

-- If there's a memory leak we'll print a special log message. Search if such log record exists, if so, 
-- we might have a memory leak. Check the log for more information about the memory usage.
-- Note that currently we are only checking MdCxt.
select count(*) from gp_toolkit.__gp_log_segment_ext where logsegment = 'seg1' and logtime > (select * from t_ckpt_memleak_test_timestamp) and logmessage like '[CheckpointMemoryLeakTest] Possible memory leak %';

-- Bring back primary and re-balance.
!\retcode gprecoverseg -a;
SELECT wait_until_all_segments_synchronized();
!\retcode gprecoverseg -ra;
SELECT wait_until_all_segments_synchronized();

-- Cleanup
DROP TABLE t_ckpt_memleak_test;
DROP TABLE t_ckpt_memleak_test_timestamp;
-- Explicitly turn fsync off as it's default to be on
!\retcode gpconfig -c fsync -v off --skipvalidation;
!\retcode gpconfig -r create_restartpoint_on_ckpt_record_replay --skipvalidation;
!\retcode gpstop -u;
