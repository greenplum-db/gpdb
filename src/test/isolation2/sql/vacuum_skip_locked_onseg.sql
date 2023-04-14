-- Test VACUUM with SKIP_LOCKED
-- The test focuses on the vacuum behavior when the table is locked
-- on segments. There is another test vacuum-skip-locked in
-- isolation dir which focuses on regular test cases (table is locked
-- on master).

1: CREATE TABLE vacuum_tbl (c1 int) DISTRIBUTED BY (c1);

-- Connect to seg #0 in utility mode, lock the table in share mode
0U: BEGIN;
0U: LOCK vacuum_tbl IN SHARE MODE;

-- Issue vacuum with SKIP_LOCKED option
-- Note that some ANALYZE options are disabled here because the ANALYZE
-- try to acquire sample on segments and block. It is acceptable on
-- GPDB. See the comments in isolation/specs/vacuum-skip-locked.spec.

2: VACUUM (SKIP_LOCKED) vacuum_tbl;
--2: ANALYZE (SKIP_LOCKED) vacuum_tbl;
--2: VACUUM (ANALYZE, SKIP_LOCKED) vacuum_tbl;
2: VACUUM (SKIP_LOCKED, FULL) vacuum_tbl;

0U: COMMIT;

-- Connect to seg #0 in utility mode, lock the table in exclusive mode
0U: BEGIN;
0U: LOCK vacuum_tbl IN ACCESS EXCLUSIVE MODE;

-- Issue vacuum with SKIP_LOCKED option
-- Note that some ANALYZE options are disabled here because the ANALYZE
-- try to acquire sample on segments and block. It is acceptable on
-- GPDB. See the comments in isolation/specs/vacuum-skip-locked.spec.

2: VACUUM (SKIP_LOCKED) vacuum_tbl;
--2: ANALYZE (SKIP_LOCKED) vacuum_tbl;
--2: VACUUM (ANALYZE, SKIP_LOCKED) vacuum_tbl;
2: VACUUM (SKIP_LOCKED, FULL) vacuum_tbl;

0U: COMMIT;

1: DROP TABLE IF EXISTS vacuum_tbl;
