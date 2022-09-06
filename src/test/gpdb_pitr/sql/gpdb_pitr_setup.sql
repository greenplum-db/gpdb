-- Prepare PITR extension
CREATE EXTENSION IF NOT EXISTS gp_pitr;

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

-- Run gp_switch_wal() so that the WAL segment files with the restore
-- points are eligible for archival to the WAL Archive directories. While
-- we're at it, store the WAL segment filenames that were just archived
-- so that we can check that WAL archival was successful or not later. We
-- must do this in a plpgsql cursor because of a known limitation with
-- CTAS on an EXECUTE ON COORDINATOR function.
CREATE TEMP TABLE switch_walfile_names(content_id smallint, walfilename text);
CREATE OR REPLACE FUNCTION populate_switch_walfile_names() RETURNS void AS $$
DECLARE curs CURSOR FOR SELECT * FROM gp_switch_wal(); /*in func*/
DECLARE rec record; /*in func*/
BEGIN /*in func*/
    OPEN curs; /*in func*/
    LOOP
        FETCH curs INTO rec; /*in func*/
        EXIT WHEN NOT FOUND; /*in func*/

        INSERT INTO switch_walfile_names VALUES (rec.gp_segment_id, rec.pg_walfile_name); /*in func*/
    END LOOP; /*in func*/
END $$
LANGUAGE plpgsql; /*in func*/
SELECT populate_switch_walfile_names();

-- Ensure that the last WAL segment file for each GP segment was archived.
-- This function loops until the archival is complete. It times out after
-- approximately 10mins.
CREATE OR REPLACE FUNCTION check_archival() RETURNS BOOLEAN AS $$
DECLARE archived BOOLEAN; /*in func*/
DECLARE archived_count INTEGER; /*in func*/
BEGIN /*in func*/
    FOR i in 1..3000 LOOP
        SELECT bool_and(seg_archived), count(*)
        FROM
            (SELECT last_archived_wal =
            l.walfilename AS seg_archived
            FROM switch_walfile_names l
            INNER JOIN gp_stat_archiver a
            ON l.content_id = a.gp_segment_id) s
        INTO archived, archived_count; /*in func*/
        IF archived AND archived_count = 4 THEN
            RETURN archived; /*in func*/
END IF; /*in func*/
        PERFORM pg_sleep(0.2); /*in func*/
END LOOP; /*in func*/
END $$
LANGUAGE plpgsql;

SELECT check_archival();
