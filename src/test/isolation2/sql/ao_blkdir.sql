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
