-- Test for verifying if xlog seg created while basebackup
-- dumps out data does not get cleaned

include: helpers/gp_management_utils_helpers.sql;

-- Inject fault after checkpoint creation in basebackup
SELECT gp_inject_fault('base_backup_post_create_checkpoint', 'suspend', dbid)
FROM gp_segment_configuration WHERE content=-1 and role='p';

-- Run pg_basebackup which should trigger and suspend at the fault
1&: SELECT pg_basebackup(hostname, 100, port, NULL,
        '/tmp/master_xlog_switch_test', true, 'fetch')
    from gp_segment_configuration where content=-1 and role='p';

-- Wait until fault has been triggered
SELECT gp_wait_until_triggered_fault('base_backup_post_create_checkpoint', 1, dbid)
FROM gp_segment_configuration WHERE content=-1 and role='p';

-- See that pg_basebackup is still running
SELECT application_name, state FROM pg_stat_replication;

-- Switch to a new WAL segment file.  Two pg_switch_xlog() invocations
-- with a command that generates WAL in between the invocations are
-- suffice to generate new WAL file.
CREATE TEMP TABLE xlogfile(fname text) DISTRIBUTED BY (fname);
INSERT INTO xlogfile SELECT pg_xlogfile_name(pg_switch_xlog());
CREATE TABLE master_xlog_dummy();
-- This should return false, indicating current WAL segment is
-- different than what was previously recorded xlogfile table.
SELECT fname = pg_xlogfile_name(pg_switch_xlog()) FROM xlogfile;

-- Checkpoint should retain WAL that is still needed by basebackup.
CHECKPOINT;

-- Resume basebackup
SELECT gp_inject_fault('base_backup_post_create_checkpoint', 'reset', dbid)
FROM gp_segment_configuration WHERE content=-1 and role='p';

-- Wait until basebackup finishes
1<:

-- Verify if basebackup completed successfully
-- See if recovery.conf exists (Yes - Pass)
SELECT application_name, state FROM pg_stat_replication;
!\retcode ls /tmp/master_xlog_switch_test/recovery.conf;
