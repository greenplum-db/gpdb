-- Create some tables and load some data
-- We do 1 row for gpdb_one_phase_commit to bypass autostats later
CREATE TABLE gpdb_two_phase_commit_before_acquire_share_lock(num int);
CREATE TABLE gpdb_two_phase_commit_after_acquire_share_lock(num int);
CREATE TABLE gpdb_one_phase_commit(num int);
CREATE TABLE gpdb_two_phase_commit_after_restore_point(num int);
INSERT INTO gpdb_two_phase_commit_before_acquire_share_lock SELECT generate_series(1, 10);
INSERT INTO gpdb_two_phase_commit_after_acquire_share_lock SELECT generate_series(1, 10);
INSERT INTO gpdb_one_phase_commit VALUES (1);

-- Inject suspend faults that will be used later to test different
-- distributed commit scenarios, and to also test the commit blocking
-- requirement which should only block twophase commits during
-- distributed commit broadcast when a restore point is being created.
1: CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
1: SELECT gp_inject_fault('dtm_broadcast_prepare', 'suspend', 1);
1: SELECT gp_inject_fault('gp_create_restore_point_acquired_lock', 'suspend', 1);

-- Delete from both tables. Only one will succeed during recovery
-- rebroadcast later during PITR.
2: BEGIN;
2: DELETE FROM gpdb_two_phase_commit_before_acquire_share_lock;
3: BEGIN;
3: DELETE FROM gpdb_two_phase_commit_after_acquire_share_lock;

-- Call the restore point creation function. This will merely grab the
-- TwophaseCommit lwlock in EXCLUSIVE mode until the fault is
-- released. The inserted row will be recorded after the restore point
-- so it will not show up later during PITR.
4: BEGIN;
4: INSERT INTO gpdb_two_phase_commit_after_restore_point SELECT generate_series(1, 10);
4&: SELECT segment_id, count(*) FROM gp_create_restore_point('test_restore_point') AS r(segment_id smallint, restore_lsn pg_lsn) GROUP BY segment_id ORDER BY segment_id;
1: SELECT gp_wait_until_triggered_fault('gp_create_restore_point_acquired_lock', 1, 1);

-- Distributed commit record will not be written; commit blocked by
-- fault injected suspension.
2&: COMMIT;
1: SELECT gp_wait_until_triggered_fault('dtm_broadcast_prepare', 1, 1);
-- Distributed commit record will be written; commit blocked by
-- attempt to acquire TwophaseCommit lwlock in SHARED mode but the
-- restore point session has the lwlock in EXCLUSIVE mode already.
3&: COMMIT;
-- One-phase commit query should not block.
1: INSERT INTO gpdb_one_phase_commit VALUES (2);
-- Read-only query should not block.
1: SELECT * FROM gpdb_two_phase_commit_before_acquire_share_lock;

-- Unblock SQL session 2, 3, and 4 by resetting the fault to create
-- the restore points which will release the TwophaseCommit lwlock.
1: SELECT gp_inject_fault('gp_create_restore_point_acquired_lock', 'reset', 1);
4<:
4: COMMIT;
3<:
1: SELECT gp_inject_fault('dtm_broadcast_prepare', 'reset', 1);
2<:

-- Show what we have currently before going back in time
SELECT * FROM gpdb_two_phase_commit_before_acquire_share_lock;
SELECT * FROM gpdb_two_phase_commit_after_acquire_share_lock;
SELECT * FROM gpdb_one_phase_commit;
SELECT * FROM gpdb_two_phase_commit_after_restore_point ORDER BY num;

-- Run pg_switch_wal() so that the WAL segment files with the restore
-- points are archived to the WAL Archive directories.
SELECT true FROM pg_switch_wal();
SELECT (SELECT true FROM pg_switch_wal()) FROM gp_dist_random('gp_id');

-- Call a checkpoint to flush buffers (including the switch xlog record)
CHECKPOINT;
