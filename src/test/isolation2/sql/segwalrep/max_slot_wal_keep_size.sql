-- when the WAL replication lag exceeds 'max_slot_wal_keep_size', the extra WAL
-- log will be removed on the primary and the replication slot will be marked as
-- obsoleted. In this case, the mirror will be marked down as well and need full
-- recovery to brought it back.

CREATE OR REPLACE FUNCTION advance_xlog_on_seg0(num int) RETURNS void AS
$$
DECLARE
	i int; /* in func */
BEGIN 
    i := 0; /* in func */
	CREATE TABLE t_dummy_switch(i int) DISTRIBUTED BY (i); /* in func */
	LOOP 
		IF i >= num THEN 
			DROP TABLE t_dummy_switch; /* in func */
			RETURN; /* in func */
		END IF; /* in func */
		PERFORM pg_switch_wal() FROM gp_dist_random('gp_id') WHERE gp_segment_id=0; /* in func */
		INSERT INTO t_dummy_switch SELECT generate_series(1,10); /* in func */
		i := i + 1; /* in func */
	END LOOP; /* in func */
	DROP TABLE t_dummy_switch; /* in func */
END; /* in func */
$$ language plpgsql;

-- On content 0 primary, retain max 128MB (2 WAL files) for
-- replication slots.  That makes it necessary to set
-- max_wal_size to a lower value, that is size of 1 WAL file.  Other
-- GUCs are needed to make the test run faster.
0U: ALTER SYSTEM SET max_slot_wal_keep_size TO 128;
0U: ALTER SYSTEM SET max_wal_size TO 64;
0U: ALTER SYSTEM SET wal_keep_size TO 0;
0U: ALTER SYSTEM SET gp_fts_mark_mirror_down_grace_period TO 0;
0U: select pg_reload_conf();
-- And on coordinator, also to make the test faster.
ALTER SYSTEM SET gp_fts_probe_retries TO 1;
select pg_reload_conf();

CREATE TABLE t_slot_size_limit(a int, fname text);

----------
-- Case 1:
--
--   Verify that max_slot_wal_keep_size GUC is honored and no more WAL is
--   retained when the oldest active PREPARE record falls behind the
--   cutoff specified by the GUC.
----------

-- Suspend QD after preparing a distributed transaction, it will be
-- resumed after checkpoint.
1: SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'suspend', dbid)
   FROM gp_segment_configuration WHERE content=-1 AND role='p';
-- This transaction is prepared on segments but not committed yet.  We
-- advance WAL beyond max_slot_wal_keep_size in the next few steps.
-- In 6X_STABLE Checkpointer should retain WAL up to this prepare LSN, otherwise we
-- will never be able to finish this transaction.  Recording two-phase
-- commit state like this in WAL records is legacy Greenplum specific
-- behavior.  In Greenplum 7+ and PostgreSQL, two-phase
-- state file is used to record this state, and checkpointer does not
-- need to be mindful of prepare WAL records.
3&: INSERT INTO t_slot_size_limit SELECT generate_series(101,120);
1: SELECT gp_wait_until_triggered_fault('transaction_abort_after_distributed_prepared', 1, dbid)
   FROM gp_segment_configuration WHERE content=-1 AND role='p';

-- Walsender skip sending WAL to the mirror, build replication lag.
-- Note that this fault causes SyncRepWaitForLSN to get stuck.  We try
-- to avoid committing transactions in subsequent steps until this
-- fault is reset.
1: SELECT gp_inject_fault_infinite('walsnd_skip_send', 'skip', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

2: BEGIN;

-- Trigger the fault in walsender.  Also triggers checkpoint.
2: SELECT advance_xlog_on_seg0(1);
1: SELECT gp_wait_until_triggered_fault('walsnd_skip_send', 1, dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Skip checkpoints on seg0.  So that when new WAL is generated in the
-- next step, checkpoints don't get triggered asynchronously.
1: SELECT gp_inject_fault_infinite('checkpoint', 'skip', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';
0U: CHECKPOINT;
1: SELECT gp_wait_until_triggered_fault('checkpoint', 1, dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Generate more WAL on seg0 than max_slot_wal_keep_size.
2: SELECT advance_xlog_on_seg0(3);

-- Resume checkpoints.
1: SELECT gp_inject_fault('checkpoint', 'reset', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';
-- At this point:
--    PREPARE LSN < previous checkpoint <= restart_lsn
-- The checkpoint should not retain WAL even when mirror has lagged behind
-- more than max_slot_wal_keep_size.
0U: CHECKPOINT;

-- Replication slot on content 0 primary should not report valid LSN
-- and checkpoint should not override max_slot_wal_keep_size GUC because it
-- does not need to retain the PREPARE record created by session 3.
0U: select restart_lsn is not null as restart_lsn_is_valid, wal_status from pg_get_replication_slots();
-- WAL accumulated should be greater than max_slot_wal_keep_size
-- (which is set to 128MB above).
0U: select pg_wal_lsn_diff(pg_current_wal_lsn(), restart_lsn) / 1024 /1024 > 128
    as max_slot_size_overridden from pg_get_replication_slots();

-- The mirror should become down in FTS configuration.
SELECT gp_request_fts_probe_scan();
SELECT role, preferred_role, status FROM gp_segment_configuration WHERE content = 0;

-- Unblock walsender, so that the transaction in session 3 can be
-- finished.
1: SELECT gp_inject_fault_infinite('walsnd_skip_send', 'reset', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Unblock the session that was suspected after prepare-transaction
-- step.  It should be able to finish the transaction.
1: SELECT gp_inject_fault_infinite('transaction_abort_after_distributed_prepared', 'reset', dbid)
   FROM gp_segment_configuration WHERE content=-1 AND role='p';
3<:
3: select count(*) from t_slot_size_limit;
3q:

-- do full recovery since replication slot is lost and this is point of no return
!\retcode gprecoverseg -aF;
select wait_until_segment_synchronized(0);

----------
-- Case 2:
--
--   Verify that max_slot_wal_keep_size GUC is honored by invalidating
--   replication slot.
----------

-- Make walsender skip sending WAL to the mirror to build replication
-- lag again.
1: SELECT gp_inject_fault_infinite('walsnd_skip_send', 'skip', dbid) FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Trigger the fault in walsender.  Also triggers checkpoint.
2: SELECT advance_xlog_on_seg0(1);
1: SELECT gp_wait_until_triggered_fault('walsnd_skip_send', 1, dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Replication slot should be valid at this time.
0U: select restart_lsn is not null as restart_lsn_is_valid, wal_status from pg_get_replication_slots();

-- Skip checkpoints on seg0.  So that when new WAL is generated in the
-- next step, checkpoints don't get triggered asynchronously.
1: SELECT gp_inject_fault_infinite('checkpoint', 'skip', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';
0U: CHECKPOINT;
1: SELECT gp_wait_until_triggered_fault('checkpoint', 1, dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Generate more WAL on seg0 than max_slot_wal_keep_size.
2: SELECT advance_xlog_on_seg0(3);

-- Resume checkpoints.
1: SELECT gp_inject_fault('checkpoint', 'reset', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';
-- WAL older than max_slot_wal_keep_size should be removed by this
-- checkpoint.
0U: CHECKPOINT;

-- Replication slot on content 0 primary should report invalid LSN
-- because the WAL files needed by it are removed by previous
-- checkpoint.
0U: select restart_lsn is not null as restart_lsn_is_valid, wal_status from pg_get_replication_slots();

1: SELECT gp_inject_fault_infinite('walsnd_skip_send', 'reset', dbid) FROM gp_segment_configuration WHERE content=0 AND role='p';
1: SELECT gp_request_fts_probe_scan();
2: END;

-- check the mirror is down and the sync_error is set.
1: SELECT role, preferred_role, status FROM gp_segment_configuration WHERE content = 0;
1: SELECT sync_error FROM gp_stat_replication WHERE gp_segment_id = 0;

-- Fault to check if walsender enters catchup state.
1: select gp_inject_fault('is_mirror_up', 'skip', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Wait for the mirror to make the next connection attempt.
1: SELECT gp_inject_fault('initialize_wal_sender', 'skip',  dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';
-- Note that we wait until the fault is triggered twice.  Waiting
-- until the second trigger guarantees that first connection attempt
-- is fully processed and the status check that follows is accurate.
1: SELECT gp_wait_until_triggered_fault('initialize_wal_sender', 2, dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Validate that the mirror is not marked as up after replication slot
-- is obsoleted.  There used to be a bug that caused FTS to be mislead
-- by a walsender that entered catchup state but failed shorty after
-- due to the requested start point not available.  FTS marked the
-- mirror as up and turned synchronous replication on.  The following
-- query should show "num times hit" as 0, implying that the mirror's
-- status was not changed from down to up.
1: select gp_inject_fault('is_mirror_up', 'status', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

1: SELECT gp_inject_fault('all', 'reset', dbid) FROM gp_segment_configuration;

0U: ALTER SYSTEM RESET max_slot_wal_keep_size;
0U: ALTER SYSTEM RESET max_wal_size;
0U: ALTER SYSTEM RESET wal_keep_size;
0U: ALTER SYSTEM RESET gp_fts_mark_mirror_down_grace_period;
0U: select pg_reload_conf();
0Uq:
ALTER SYSTEM RESET gp_fts_probe_retries;
select pg_reload_conf();

-- do full recovery
!\retcode gprecoverseg -aF;
select wait_until_segment_synchronized(0);

-- the mirror is up and the replication is back
1: SELECT role, preferred_role, status FROM gp_segment_configuration WHERE content = 0;
1: SELECT state, sync_error FROM gp_stat_replication WHERE gp_segment_id = 0;

1q:
2q:
