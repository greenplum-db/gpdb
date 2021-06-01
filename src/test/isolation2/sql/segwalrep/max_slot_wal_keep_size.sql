-- when the WAL replication lag exceeds 'max_slot_wal_keep_size', the extra WAL
-- log will be removed on the primary and the replication slot will be marked as
-- obsoleted. In this case, the mirror will be marked down as well and need full
-- recovery to brought it back. 

include: helpers/server_helpers.sql;

CREATE OR REPLACE FUNCTION advance_xlog_on_seg0(num int) RETURNS void AS
$$
DECLARE
	i int; 
BEGIN 
    i := 0; 
	CREATE TABLE t_dummy_switch(i int) DISTRIBUTED BY (i); 
	LOOP 
		IF i >= num THEN 
			DROP TABLE t_dummy_switch; 
			RETURN; 
		END IF; 
		PERFORM pg_switch_xlog() FROM gp_dist_random('gp_id') WHERE gp_segment_id=0; 
		INSERT INTO t_dummy_switch SELECT generate_series(1,10); 
		i := i + 1; 
	END LOOP; 
	DROP TABLE t_dummy_switch; 
END; 
$$ language plpgsql;

-- On content 0 primary, retain max 128MB (2 WAL files) for
-- replication slots.  That makes it necessary to set
-- checkpoint_segments to a lower value, that is 1 WAL file.  Other
-- GUCs are needed to make the test run faster.
0U: ALTER SYSTEM SET max_slot_wal_keep_size TO 128;
0U: ALTER SYSTEM SET checkpoint_segments TO 1;
0U: ALTER SYSTEM SET wal_keep_segments TO 0;
0U: ALTER SYSTEM SET gp_fts_mark_mirror_down_grace_period TO 0;
0U: select pg_reload_conf();
-- And on coordinator, also to make the test faster.
ALTER SYSTEM SET gp_fts_probe_retries TO 1;
select pg_reload_conf();

CREATE TABLE t_slot_size_limit(a int, fname text);

----------
-- Case 1:
--
--   Verify that max_slot_wal_keep_size GUC is ignored and more WAL is
--   retained when the oldest active PREPARE record falls behind the
--   cutoff specified by the GUC.
----------

-- Suspend QD after preparing a distributed transaction, it will be
-- resumed after checkpoint.
1: SELECT gp_inject_fault('transaction_abort_after_distributed_prepared', 'suspend', dbid)
   FROM gp_segment_configuration WHERE content=-1 AND role='p';
-- This transaction is prepared on segments but not committed yet.  We
-- advance WAL beyond max_slot_wal_keep_size in the next few steps.
-- Checkpointer should retain WAL upto this prepare LSN, otherwise we
-- will never be able to finish this transaction.  Recording two-phase
-- commit state like this in WAL records in Greenplum specific
-- behavior.  In newer Greenplum versions and PostgreSQL, two-phase
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
--    PREPARE LSN < previous checkpoint < restart_lsn
-- The checkpoint should retain WAL even when mirror has lagged behind
-- more than max_slot_wal_keep_size.
0U: CHECKPOINT;

-- Replication slot on content 0 primary should report valid LSN
-- because checkpoint must override max_slot_wal_keep_size GUC in
-- order to retain the PREPARE record created by session 3.
0U: select restart_lsn is not null as restart_lsn_is_valid from pg_get_replication_slots();
-- WAL accumulated should be greater than max_slot_wal_keep_size
-- (which is set to 128MB above).
0U: select pg_xlog_location_diff(pg_current_xlog_location(), restart_lsn) / 1024 /1024 > 128
    as max_slot_size_overridden from pg_get_replication_slots();

-- The mirror should remain up in FTS configuration.
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
0U: select restart_lsn is not null as restart_lsn_is_valid from pg_get_replication_slots();

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
0U: select restart_lsn is not null as restart_lsn_is_valid from pg_get_replication_slots();

1: SELECT gp_inject_fault_infinite('walsnd_skip_send', 'reset', dbid) FROM gp_segment_configuration WHERE content=0 AND role='p';
1: SELECT gp_request_fts_probe_scan();
2: END;

-- check the mirror is down and the sync_error is set.
1: SELECT role, preferred_role, status FROM gp_segment_configuration WHERE content = 0;
1: SELECT sync_error FROM gp_stat_replication WHERE gp_segment_id = 0;

-- Fault to check if walsender enters catchup state.
1: select gp_inject_fault('enter_catchup_or_streaming', 'skip', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Wait for the mirror to make the next connection attempt.
1: SELECT gp_inject_fault('initialize_wal_sender', 'skip',  dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';
-- Note that we wait until the fault is triggered twice.  Waiting
-- until the second trigger guarantees that first connection attempt
-- is fully processed and the status check that follows is accurate.
1: SELECT gp_wait_until_triggered_fault('initialize_wal_sender', 2, dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

-- Validate that walsender does not enter catchup state, once the
-- replication slot is obsoleted.  There used to be a bug that caused
-- FTS to be mislead by a walsender that entered catchup state but
-- failed shorty after due to the requested start point not available.
-- FTS marked the mirror as up and turn synchronous replication on and
-- later found it down.  If walsender fails before entering catchup
-- state, FTS cannot be misled into thinking the mirror is up.  The
-- following query should show "num times hit" as 0.
1: select gp_inject_fault('enter_catchup_or_streaming', 'status', dbid)
   FROM gp_segment_configuration WHERE content=0 AND role='p';

1: SELECT gp_inject_fault('all', 'reset', dbid) FROM gp_segment_configuration;

0U: ALTER SYSTEM RESET max_slot_wal_keep_size;
0U: ALTER SYSTEM RESET checkpoint_segments;
0U: ALTER SYSTEM RESET wal_keep_segments;
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
