-- Test: SELECT or other read-only operations which set hint bits on pages,
-- and in turn generate Full Page Images (FPI) WAL records should be throttled.
-- We will only throttle when our transaction wal exceeds
-- wait_for_replication_threshold. While the backend is throttled waiting for
-- synchronous replication, it should not block the CHECKPOINT process, and if
-- the mirror is taken down, synchronous replication should be turned off and
-- the backend should stop waiting for synchronous replication and proceed.

-- Setup:
-- 1. Set wait_for_replication_threshold to 1kB for quicker test
-- 2. create two tables (one small and one large)
-- 3. set gp_disable_tuple_hints=off so buffer will be immediately marked dirty on hint bit change

include: helpers/server_helpers.sql;

-- set wait_for_replication_threshold to 100kB for quicker test.
-- this cannot be too low otherwise is_query_waiting_for_syncrep() might hit it too.
!\retcode gpconfig -c wait_for_replication_threshold -v 100;
!\retcode gpstop -u;

CREATE TABLE select_no_throttle(a int) DISTRIBUTED BY (a);
INSERT INTO select_no_throttle SELECT generate_series (1, 10);
CREATE TABLE select_throttle(a int) DISTRIBUTED BY (a);
INSERT INTO select_throttle SELECT generate_series (1, 100000);

-- Enable tuple hints so that buffer will be marked dirty upon a hint bit change
-- (so that we don't have to wait for the tuple to age. See logic in markDirty)
1U: SET gp_disable_tuple_hints=off;

-- Test 1:
-- 1. Suspend walsender
-- 2. Perform a read-only operation (SELECT) which would now set the hint bits
--  For the small table this operation should finish,
--  but for large table the SELECT should be throttled
--  since it would generate a lot of WAL greater than wait_for_replication_threshold
-- 3. Confirm that the query is waiting on Syncrep
-- 4. Reset the walsender and the transaction should complete

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

-- wait until fault is triggered
select gp_wait_until_triggered_fault('wal_sender_loop', 1, dbid) from gp_segment_configuration where role='p' and content = 1;

-- check if the above query is waiting on SyncRep in pg_stat_activity
SELECT is_query_waiting_for_syncrep(50, 'SELECT count(*) FROM select_throttle;');

-- reset walsender
SELECT gp_inject_fault_infinite('wal_sender_loop', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;
-- after this, system continue to proceed

1U<:

SELECT wait_until_all_segments_synchronized();

-- Test 2:
-- 1. Suspend walsender
-- 2. Perform a read-only operation (SELECT) which would now set the hint bits
--  For the large table the SELECT should be throttled
--  since it would generate a lot of WAL greater than wait_for_replication_threshold
-- 3. Confirm that the query is waiting on Syncrep
-- 4. Perform CHECKPOINT and confirm that it does not block
-- 5. Stop the mirror
-- 6. Reset the walsender and the transaction should complete without waiting for syncrep

-- Setup:
-- set mirror down grace period to zero to instantly mark mirror down.
-- the 1Uq and 1U pair will force a wait on the config reload.
!\retcode gpconfig -c gp_fts_mark_mirror_down_grace_period -v 2;
!\retcode gpstop -u;
1Uq:
1U: show gp_fts_mark_mirror_down_grace_period;
-- Enable tuple hints so that buffer will be marked dirty upon a hint bit change
-- (so that we don't have to wait for the tuple to age. See logic in markDirty)
1U: SET gp_disable_tuple_hints=off;
Truncate select_throttle;
INSERT INTO select_throttle SELECT generate_series (1, 100000);
-- flush the data to disk
checkpoint;
-- Suspend walsender
SELECT gp_inject_fault_infinite('wal_sender_loop', 'suspend', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;

checkpoint;
-- SELECT will set the hint bit on (the buffer will be marked dirty)
-- This query should wait for Syncrep since its WAL size for hint bits is greater than wait_for_replication_threshold
1U&: SELECT count(*) FROM select_throttle;

-- wait until fault is triggered
select gp_wait_until_triggered_fault('wal_sender_loop', 1, dbid) from gp_segment_configuration where role='p' and content = 1;

-- check if the above query is waiting on SyncRep in pg_stat_activity
SELECT is_query_waiting_for_syncrep(50, 'SELECT count(*) FROM select_throttle;');

-- while SELECT is waiting for syncrep, it should not block a subsequent checkpoint
CHECKPOINT;

-- stop the mirror should turn off syncrep
SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE content=1 AND role = 'm';

-- reset walsender and let it exit so that mirror stop can be detected
SELECT gp_inject_fault_infinite('wal_sender_loop', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;

-- perform fts probe scan and verify that mirror is down
select wait_for_mirror_down(1::smallint, 30);
select content, role, preferred_role, mode, status from gp_segment_configuration where content = 1;

-- after mirror is stopped, the SELECT query should proceed without waiting for syncrep
1U<:

!\retcode gprecoverseg -av;
SELECT wait_until_all_segments_synchronized();

-- Test 3:
-- Just like Test 2, but with VACUUM instead of SELECT, so exclusive buffer lock
-- will be acquired instead of shared lock.

-- Setup:
-- set mirror down grace period to zero to instantly mark mirror down.
-- the 1Uq and 1U pair will force a wait on the config reload.
!\retcode gpconfig -c gp_fts_mark_mirror_down_grace_period -v 2;
!\retcode gpstop -u;
1Uq:
1U: show gp_fts_mark_mirror_down_grace_period;
set gp_disable_tuple_hints = off;

create table vacuum_throttle(a int);
insert into vacuum_throttle select * from generate_series(1,1000);
delete from vacuum_throttle;
checkpoint;
select gp_inject_fault_infinite('wal_sender_loop', 'suspend', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;
checkpoint;
1&: vacuum vacuum_throttle;

-- wait until fault is triggered
select gp_wait_until_triggered_fault('wal_sender_loop', 1, dbid) from gp_segment_configuration where role='p' and content = 1;

-- check if the above query is waiting on SyncRep in pg_stat_activity
select is_query_waiting_for_syncrep(50, 'vacuum vacuum_throttle;');

-- this shouldn't stuck
2: checkpoint;

-- stop the mirror should turn off syncrep
SELECT pg_ctl(datadir, 'stop', 'immediate') FROM gp_segment_configuration WHERE content=1 AND role = 'm';

-- reset walsender and let it exit so that mirror stop can be detected
select gp_inject_fault('wal_sender_loop', 'reset', dbid) FROM gp_segment_configuration WHERE role = 'p' and content = 1;

-- perform fts probe scan and verify that mirror is down
select wait_for_mirror_down(1::smallint, 30);
select content, role, preferred_role, mode, status from gp_segment_configuration where content = 1;

-- after mirror is stopped, the VACUUM query should proceed without waiting for syncrep
1<:

!\retcode gprecoverseg -av;
SELECT wait_until_all_segments_synchronized();

-- Cleanup
reset gp_disable_tuple_hints;
-- reset the mirror down grace period back to its default value.
-- the 1Uq and 1U pair will force a wait on the config reload.
!\retcode gpconfig -r gp_fts_mark_mirror_down_grace_period;
!\retcode gpstop -u;
1Uq:
1U: show gp_fts_mark_mirror_down_grace_period;

!\retcode gpconfig -r wait_for_replication_threshold;
!\retcode gpstop -u;

