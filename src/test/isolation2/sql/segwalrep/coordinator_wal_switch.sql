-- Test for verifying if wal seg created while basebackup
-- dumps out data does not get cleaned

-- Inject fault after checkpoint creation in basebackup
SELECT gp_inject_fault('base_backup_post_create_checkpoint', 'suspend', dbid)
FROM gp_segment_configuration WHERE content=-1 and role='p';

-- Run pg_basebackup which should trigger and suspend at the fault
1&: SELECT pg_basebackup(hostname, 100, port, false, NULL,
        '/tmp/coordinator_wal_switch_test', true, 'fetch')
    from gp_segment_configuration where content=-1 and role='p';

-- Wait until fault has been triggered
SELECT gp_wait_until_triggered_fault('base_backup_post_create_checkpoint', 1, dbid)
FROM gp_segment_configuration WHERE content=-1 and role='p';

-- See that pg_basebackup is still running
SELECT application_name, state FROM pg_stat_replication;

-- Switch to a new WAL segment file.  Two pg_switch_wal() invocations
-- with a command that generates WAL in between the invocations are
-- suffice to generate new WAL file.
CREATE TEMP TABLE walfile(fname text) DISTRIBUTED BY (fname);
INSERT INTO walfile SELECT pg_walfile_name(pg_switch_wal());
CREATE TABLE coordinator_wal_dummy();
-- This should return false, indicating current WAL segment is
-- different than what was previously recorded walfile table.
SELECT fname = pg_walfile_name(pg_switch_wal()) FROM walfile;

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
!\retcode ls /tmp/coordinator_wal_switch_test/postgresql.auto.conf /tmp/coordinator_wal_switch_test/standby.signal;
