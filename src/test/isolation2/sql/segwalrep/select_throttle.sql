-- Test: SELECT or other read-only operations which set hint bits on pages,
-- and in turn generate Full Page Images (FPI) WAL records should be throttled.
-- We will only throttle when our transaction wal exceeds
-- wait_for_replication_threshold
-- For this test we will:
-- 1. Set wait_for_replication_threshold to 1kB for quicker test
-- 2. create two tables (one small and one large)
-- 3. set gp_disable_tuple_hints=off so buffer will be immediately marked dirty on hint bit change
-- 4. Suspend walsender
-- 5. Perform a read-only operation (SELECT) which would now set the hint bits
--  For the small table this operation should finish,
--  but for large table the SELECT should be throttled
--  since it would generate a lot of WAL greater than wait_for_replication_threshold
-- 6. Confirm that the query is waiting on Syncrep
-- 7. Reset the walsender and the transaction should complete
--

include: helpers/server_helpers.sql;

-- set wait_for_replication_threshold to 1kB for quicker test
ALTER SYSTEM SET wait_for_replication_threshold = 1;
SELECT pg_reload_conf();

CREATE TABLE select_no_throttle(a int) DISTRIBUTED BY (a);
INSERT INTO select_no_throttle SELECT generate_series (1, 10);
CREATE TABLE select_throttle(a int) DISTRIBUTED BY (a);
INSERT INTO select_throttle SELECT generate_series (1, 100000);

-- Enable tuple hints so that buffer will be marked dirty upon a hint bit change
-- (so that we don't have to wait for the tuple to age. See logic in markDirty)
1U: SET gp_disable_tuple_hints=off;

-- flush the data to disk
checkpoint;

-- Suspend walsender
SELECT gp_inject_fault_infinite('wal_sender_loop', 'suspend', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;

-- the following SELECTS will set the hint bit on (the buffer will be marked dirty)
-- This query should not wait
1U: SELECT count(*) FROM select_no_throttle;
checkpoint;
-- This query should wait for Syncrep since its WAL size for hint bits is greater than wait_for_replication_threshold
1U&: SELECT count(*) FROM select_throttle;

-- check if the above query is waiting on SyncRep in pg_stat_activity
SELECT is_query_waiting_for_syncrep(50, 'SELECT count(*) FROM select_throttle;');

-- reset walsender
SELECT gp_inject_fault_infinite('wal_sender_loop', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;
-- after this, system continue to proceed

1U<:

SELECT wait_until_all_segments_synchronized();
1U: RESET gp_disable_tuple_hints;
1Uq:

ALTER SYSTEM RESET wait_for_replication_threshold;
SELECT pg_reload_conf();
