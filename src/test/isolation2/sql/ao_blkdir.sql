-- White-box tests asserting composition of AO/CO block directory entries.
-- All tuples are directed to seg0 and each INSERT has an increasing row count
-- to make their identification easy.

--------------------------------------------------------------------------------
-- AO tables
--------------------------------------------------------------------------------

CREATE TABLE ao_blkdir_test(i int, j int) USING ao_row DISTRIBUTED BY (j);
CREATE INDEX ao_blkdir_test_idx ON ao_blkdir_test(i);

1: INSERT INTO ao_blkdir_test SELECT i, 2 FROM generate_series(1, 10) i;
-- There should be 1 block directory row with a single entry covering 10 rows
SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
    WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

1: INSERT INTO ao_blkdir_test SELECT i, 2 FROM generate_series(11, 30) i;
-- There should be 2 block directory entries in a new block directory row, and
-- the row from the previous INSERT should not be visible. The entry from the
-- first INSERT should remain unchanged.
SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

1: BEGIN;
1: INSERT INTO ao_blkdir_test SELECT i, 2 FROM generate_series(31, 60) i;
2: BEGIN;
2: INSERT INTO ao_blkdir_test SELECT i, 2 FROM generate_series(71, 110) i;
1: COMMIT;
2: COMMIT;
-- The second INSERT of 40 rows above would have landed in segfile 1 (unlike
-- segfile 0, like the first INSERT of 30 rows above). This should be reflected
-- in the block directory entries for these rows.
SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

TRUNCATE ao_blkdir_test;
-- Insert enough rows to overflow the first block directory minipage by 2.
INSERT INTO ao_blkdir_test SELECT i, 2 FROM generate_series(1, 292700) i;
-- There should be 2 block directory rows, one with 161 entries covering 292698
-- rows and the other with 1 entry covering the 2 overflow rows.
SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

-- Unique index white box tests
DROP TABLE ao_blkdir_test;
CREATE TABLE ao_blkdir_test(i int UNIQUE, j int) USING ao_row DISTRIBUTED BY (i);

SELECT gp_inject_fault('appendonly_insert', 'suspend', '', '', 'ao_blkdir_test', 1, 1, 0, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
1: BEGIN;
1&: INSERT INTO ao_blkdir_test VALUES (2, 2);

-- There should be a placeholder row inserted to cover the rows for each INSERT
-- session, before we insert the 1st row in that session, that is only visible
-- to SNAPSHOT_DIRTY.
SELECT gp_wait_until_triggered_fault('appendonly_insert', 1, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;
SET gp_select_invisible TO ON;
SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;
RESET gp_select_invisible;

-- The placeholder row is invisible to other transactions (that don't perform a
-- uniqueness check) while the INSERT is in progress.
2: SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
   WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

SELECT gp_inject_fault('appendonly_insert', 'reset', dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
1<:

-- The placeholder row is invisible to the INSERTing transaction. Since the
-- INSERT finished, there should be 1 visible blkdir row representing the INSERT.
1: SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

-- The placeholder row is invisible to other transactions (that don't perform a
-- uniqueness check) even after the INSERT finishes. The blkdir row representing
-- the INSERT should not be visible as the INSERTing transaction hasn't
-- committed yet.
2: SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

1: COMMIT;

-- The placeholder row is invisible to other transactions (that don't perform a
-- uniqueness check) even after the INSERTing transaction commits. Since the
-- INSERTing transaction has committed, the blkdir row representing the INSERT
-- should be visible now.
2: SELECT (gp_toolkit.__gp_aoblkdir('ao_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

DROP TABLE ao_blkdir_test;

--------------------------------------------------------------------------------
-- AOCO tables
--------------------------------------------------------------------------------

CREATE TABLE aoco_blkdir_test(i int, j int) USING ao_column DISTRIBUTED BY (j);
CREATE INDEX aoco_blkdir_test_idx ON aoco_blkdir_test(i);

1: INSERT INTO aoco_blkdir_test SELECT i, 2 FROM generate_series(1, 10) i;
-- There should be 2 block directory rows with a single entry covering 10 rows,
-- (1 for each column).
SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

1: INSERT INTO aoco_blkdir_test SELECT i, 2 FROM generate_series(11, 30) i;
-- There should be 2 block directory rows, carrying 2 entries each. The rows
-- from the previous INSERT should not be visible. The entries from the first
-- INSERT should remain unchanged.
SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

1: BEGIN;
1: INSERT INTO aoco_blkdir_test SELECT i, 2 FROM generate_series(31, 60) i;
2: BEGIN;
2: INSERT INTO aoco_blkdir_test SELECT i, 2 FROM generate_series(71, 110) i;
1: COMMIT;
2: COMMIT;
-- The second INSERT of 40 rows above would have landed in segfile 1 (unlike
-- segfile 0, like the first INSERT of 30 rows above). This should be reflected
-- in the block directory entries for these rows.
SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

TRUNCATE aoco_blkdir_test;
-- Insert enough rows to overflow the first block directory minipage by 2.
INSERT INTO aoco_blkdir_test SELECT i, 2 FROM generate_series(1, 1317143) i;
-- There should be 2 block directory rows, 2 for each column, one with 161
-- entries covering 1317141 rows and the other with 1 entry covering the 2
-- overflow rows.
SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

-- Unique index white box tests
DROP TABLE aoco_blkdir_test;
CREATE TABLE aoco_blkdir_test(h int, i int UNIQUE, j int) USING ao_column DISTRIBUTED BY (i);

SELECT gp_inject_fault('appendonly_insert', 'suspend', '', '', 'aoco_blkdir_test', 1, 1, 0, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
1: BEGIN;
1&: INSERT INTO aoco_blkdir_test VALUES (2, 2, 2);

-- There should be a placeholder row inserted to cover the rows for each INSERT
-- session (for the first non-dropped column), before we insert the 1st row in
-- that session, that is only visible to SNAPSHOT_DIRTY.
SELECT gp_wait_until_triggered_fault('appendonly_insert', 1, dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;
SET gp_select_invisible TO ON;
SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;
RESET gp_select_invisible;

-- The placeholder row is invisible to other transactions (that don't perform a
-- uniqueness check) while the INSERT is in progress.
2: SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
   WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

-- Before the INSERT commits, if we try to drop column 'h', for which the
-- placeholder row was created, the session will block (locking). So it is
-- perfectly safe to use 1 placeholder row (and not have 1 placeholder/column)
3&: ALTER TABLE aoco_blkdir_test DROP COLUMN h;

SELECT gp_inject_fault('appendonly_insert', 'reset', dbid)
FROM gp_segment_configuration WHERE role = 'p' AND content = 0;
1<:

-- The placeholder row is invisible to the INSERTing transaction. Since the
-- INSERT finished, there should be 3 visible blkdir rows representing the
-- INSERT, 1 for each column.
1: SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
   WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

-- The placeholder row is invisible to other transactions (that don't perform a
-- uniqueness check) even after the INSERT finishes. The blkdir rows representing
-- the INSERT should not be visible as the INSERTing transaction hasn't
-- committed yet.
2: SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
   WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

1: COMMIT;

-- The placeholder row is invisible to other transactions (that don't perform a
-- uniqueness check) even after the INSERTing transaction commits. Since the
-- INSERTing transaction has committed, the blkdir rows representing the INSERT
-- should be visible now.
2: SELECT (gp_toolkit.__gp_aoblkdir('aoco_blkdir_test')).* FROM gp_dist_random('gp_id')
   WHERE gp_segment_id = 0 ORDER BY 1,2,3,4,5;

-- Now even though the DROP COLUMN has finished, we would still be able to
-- properly resolve uniqueness checks (by consulting the first non-dropped
-- column's block directory row).
3<:
4: INSERT INTO aoco_blkdir_test VALUES (2, 2);

DROP TABLE aoco_blkdir_test;
