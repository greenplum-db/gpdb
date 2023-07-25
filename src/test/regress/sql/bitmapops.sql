-- Test bitmap AND and OR

SET default_statistics_target=1000;

-- Encourage index use
SET seq_page_cost=100;


-- Generate enough data that we can test the lossy bitmaps.

-- There's 55 tuples per page in the table. 53 is just
-- below 55, so that an index scan with qual a = constant
-- will return at least one hit per page. 59 is just above
-- 55, so that an index scan with qual b = constant will return
-- hits on most but not all pages. 53 and 59 are prime, so that
-- there's a maximum number of a,b combinations in the table.
-- That allows us to test all the different combinations of
-- lossy and non-lossy pages with the minimum amount of data

CREATE TABLE bmscantest (a int, b int, t text);

INSERT INTO bmscantest
  SELECT (r%53), (r%59), 'foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo'
  FROM generate_series(1,70000) r;

CREATE INDEX i_bmtest_a ON bmscantest(a);
CREATE INDEX i_bmtest_b ON bmscantest(b);

-- We want to use bitmapscans. With default settings, the planner currently
-- chooses a bitmap scan for the queries below anyway, but let's make sure.
set enable_indexscan=false;
set enable_seqscan=false;

-- Lower work_mem to trigger use of lossy bitmaps
set work_mem = 64;


-- Test bitmap-and.
SELECT count(*) FROM bmscantest WHERE a = 1 AND b = 1;

-- Test bitmap-or.
SELECT count(*) FROM bmscantest WHERE a = 1 OR b = 1;

-- Test mix BitmapOp load of on-disk bitmap index scan and in-memory bitmap index scan:

-- 1) heap
CREATE TABLE bmscantest2 (a int, b int, c int, d int, t text);
INSERT INTO bmscantest2
  SELECT (r%53), (r%59), (r%53), (r%59), 'foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo'
  FROM generate_series(1,70000) r;
ANALYZE bmscantest2;
CREATE INDEX i_bmtest2_a ON bmscantest2 USING BITMAP(a);
CREATE INDEX i_bmtest2_b ON bmscantest2 USING BITMAP(b);
CREATE INDEX i_bmtest2_c ON bmscantest2(c);
CREATE INDEX i_bmtest2_d ON bmscantest2(d);

EXPLAIN (COSTS OFF) SELECT count(*) FROM bmscantest2 WHERE a = 1 AND b = 1 AND c = 1;
SELECT count(*) FROM bmscantest2 WHERE a = 1 AND b = 1 AND c = 1;
SELECT count(*) FROM bmscantest2 WHERE a = 1 AND (b = 1 OR c = 1) AND d = 1;

EXPLAIN (COSTS OFF) SELECT count(*) FROM bmscantest2 WHERE a = 1 OR b = 1 OR c = 1;
SELECT count(*) FROM bmscantest2 WHERE a = 1 OR b = 1 OR c = 1;
SELECT count(*) FROM bmscantest2 WHERE a = 1 OR (b = 1 AND c = 1) OR d = 1;

-- 2) ao
CREATE TABLE bmscantest_ao (a int, b int, c int, d int, t text) WITH(appendonly=true, orientation=row);
INSERT INTO bmscantest_ao
  SELECT (r%53), (r%59), (r%53), (r%59), 'foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo'
  FROM generate_series(1,70000) r;
ANALYZE bmscantest_ao;
CREATE INDEX i_bmtest_ao_a ON bmscantest_ao USING BITMAP(a);
CREATE INDEX i_bmtest_ao_b ON bmscantest_ao USING BITMAP(b);
CREATE INDEX i_bmtest_ao_c ON bmscantest_ao(c);
CREATE INDEX i_bmtest_ao_d ON bmscantest_ao(d);

EXPLAIN (COSTS OFF) SELECT count(*) FROM bmscantest_ao WHERE a = 1 AND b = 1 AND c = 1;
SELECT count(*) FROM bmscantest_ao WHERE a = 1 AND b = 1 AND c = 1;
EXPLAIN SELECT count(*) FROM bmscantest_ao WHERE a = 1 AND (b = 1 OR c = 1) AND d = 1;
SELECT count(*) FROM bmscantest_ao WHERE a = 1 AND (b = 1 OR c = 1) AND d = 1;

EXPLAIN (COSTS OFF) SELECT count(*) FROM bmscantest_ao WHERE a = 1 OR b = 1 OR c = 1;
SELECT count(*) FROM bmscantest_ao WHERE a = 1 OR b = 1 OR c = 1;
EXPLAIN SELECT count(*) FROM bmscantest_ao WHERE a = 1 OR (b = 1 AND c = 1) OR d = 1;
SELECT count(*) FROM bmscantest_ao WHERE a = 1 OR (b = 1 AND c = 1) OR d = 1;

-- 3) aocs
CREATE TABLE bmscantest_aocs (a int, b int, c int, d int, t text) WITH(appendonly=true, orientation=column);
INSERT INTO bmscantest_aocs
  SELECT (r%53), (r%59), (r%53), (r%59), 'foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo'
  FROM generate_series(1,70000) r;
ANALYZE bmscantest_aocs;
CREATE INDEX i_bmtest_aocs_a ON bmscantest_aocs USING BITMAP(a);
CREATE INDEX i_bmtest_aocs_b ON bmscantest_aocs USING BITMAP(b);
CREATE INDEX i_bmtest_aocs_c ON bmscantest_aocs(c);
CREATE INDEX i_bmtest_aocs_d ON bmscantest_aocs(d);

EXPLAIN (COSTS OFF) SELECT count(*) FROM bmscantest_aocs WHERE a = 1 AND b = 1 AND c = 1;
SELECT count(*) FROM bmscantest_aocs WHERE a = 1 AND b = 1 AND c = 1;
EXPLAIN SELECT count(*) FROM bmscantest_aocs WHERE a = 1 AND (b = 1 OR c = 1) AND d = 1;
SELECT count(*) FROM bmscantest_aocs WHERE a = 1 AND (b = 1 OR c = 1) AND d = 1;

EXPLAIN (COSTS OFF) SELECT count(*) FROM bmscantest_aocs WHERE a = 1 OR b = 1 OR c = 1;
SELECT count(*) FROM bmscantest_aocs WHERE a = 1 OR b = 1 OR c = 1;
EXPLAIN (COSTS OFF) SELECT count(*) FROM bmscantest_aocs WHERE a = 1 OR (b = 1 AND c = 1) OR d = 1;
SELECT count(*) FROM bmscantest_aocs WHERE a = 1 OR (b = 1 AND c = 1) OR d = 1;


-- clean up
DROP TABLE bmscantest;
DROP TABLE bmscantest_ao;
DROP TABLE bmscantest_aocs;
