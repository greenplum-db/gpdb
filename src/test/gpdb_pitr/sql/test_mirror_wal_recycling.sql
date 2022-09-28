-- Test that archive_mode=on does not affect WAL recycling on a GPDB
-- High Availability mirror segment... mainly that it shouldn't be
-- marking WAL segments as ready for archiving. This test assumes that
-- archive_mode=on has already been enabled for the content 0 mirror.

-- start_matchignore
--
-- # ignore NOTICE outputs from the test plpython function
-- m/NOTICE\:.*archive_status.*/
-- m/CONTEXT\:.*PL\/Python function.*/
--
-- end_matchignore

-- start_ignore
-- Enable GUC to do faster restartpoints
\! gpconfig -c create_restartpoint_on_ckpt_record_replay -v on --skipvalidation;
\! gpstop -u;
-- end_ignore

-- Add fault injector skip to where deletion/recycling of WAL segments finishes
SELECT gp_inject_fault_infinite('finished_removing_old_xlog_files', 'skip', dbid)
	FROM gp_segment_configuration WHERE role = 'm' AND content = 0;

-- Force a restartpoint
CHECKPOINT;

-- Wait until restartpoint finishes recycling WAL segments
SELECT gp_wait_until_triggered_fault('finished_removing_old_xlog_files', 1, dbid)
	FROM gp_segment_configuration WHERE content=0 AND role='m';

-- Check if a .ready file exists in the mirror's
-- $datadir/pg_xlog/archive_status/ directory after recycling WAL
-- segments. It should not.
-- start_ignore
CREATE EXTENSION IF NOT EXISTS plpython2u;
-- end_ignore
CREATE OR REPLACE FUNCTION ready_file_exists(datadir text)
RETURNS bool AS $$
    import os

    cmd = 'find %s/pg_xlog/archive_status -name "*.ready" | grep .' % (datadir)
    plpy.notice('Running: %s' % cmd) # useful debug info that is match ignored
    rc = os.system(cmd)

    return (rc == 0)
$$ LANGUAGE plpython2u VOLATILE;
SELECT ready_file_exists(datadir) FROM gp_segment_configuration WHERE content=0 AND role='m';

-- start_ignore
-- Do clean up
\! gpconfig -c create_restartpoint_on_ckpt_record_replay -v off --skipvalidation;
\! gpstop -u;
-- end_ignore
