--
-- Concurrent scan on bitmap index when there's insert running in the backend
-- may cause the bitmap scan read wrong tid.
-- If a LOV has multiple bitmap pages, and the index insert tries to insert a tid
-- into a compressed word on a full bitmap page(Let's call the page `PAGE_FULL`).
-- Then it'll try to find free space on next bitmap page(Let's call the page `PAGE_NEXT`)
-- and rearrange the words and copy extra words into the next bitmap page.
-- So when the above insertion happens, imagine below case:
-- 1. Query on bitmap: A query starts and reads all bitmap pages to `PAGE_FULL`, increase
-- next tid to fetch, release lock after reading each page.
-- 2. Concurrent insert: insert a tid into `PAGE_FULL` cause expand compressed words to
-- new words, and rearrange words into `PAGE_NEXT`.
-- 3. Query on bitmap: fetch `PAGE_NEXT` and expect the first tid in it should equal the
-- saved next tid. But actually `PAGE_NEXT` now contains words used to belong in `PAGE_FULL`.
-- This causes the real next tid less than the expected next tid. But our scan keeps increasing
-- the wrong tid. And then this leads to a wrong result.
-- This related to issue: https://github.com/greenplum-db/gpdb/issues/11308
--

-- Setup fault injectors
CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- Here we use append optimized table to speed up create full bitmap pages
-- since each transaction use different seg file number. And ao table's AOTupleId
-- is composed of (seg file number, row number). So this will lead to lots of
-- compressed words in the first bitmap page.
-- With the below transacions in each session, on seg0, the bitmap for id=97
-- will generate two bitmap pages, and the first page is a full page.
-- Use heap table, delete tuples and then vacuum should be the same. But it needs huge tuples.
CREATE TABLE bmupdate (id int) with(appendonly = true) DISTRIBUTED BY (id);

1: begin;
2: begin;
3: begin;
4: begin;
5: begin;
6: begin;
7: begin;
8: begin;
9: begin;
10: begin;
11: begin;
12: begin;
13: begin;
14: begin;
15: begin;
16: begin;
17: begin;
18: begin;
19: begin;
20: begin;
21: begin;
22: begin;

1: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
2: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
3: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
4: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
5: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
6: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
7: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
8: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
9: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
10: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
11: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
12: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
13: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
14: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
15: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
16: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
17: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
18: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
19: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
20: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
21: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;
22: INSERT INTO bmupdate SELECT i%10000 FROM generate_series(1, 1000000) AS i;

1: commit;
2: commit;
3: commit;
4: commit;
5: commit;
6: commit;
7: commit;
8: commit;
9: commit;
10: commit;
11: commit;
12: commit;
13: commit;
14: commit;
15: commit;
16: commit;
17: commit;
18: commit;
19: commit;
20: commit;
21: commit;
22: commit;

-- Let's check the total tuple count with id=97 without bitmap index.
SELECT count(*) FROM bmupdate WHERE id = 97;

CREATE INDEX idx_bmupdate__id ON bmupdate USING bitmap (id);

--
-- Test 1, run Bitmap Heap Scan on the bitmap index when there's
-- backend insert running.
--
-- Inject fault after read the first bitmap page when query the table.
SELECT gp_inject_fault_infinite('after_read_one_bitmap_idx_page', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = 0;

-- Inject fault when insert new tid cause rearrange words from current
-- bitmap page to next bitmap page.
SELECT gp_inject_fault_infinite('rearrange_word_to_next_bitmap_page', 'skip', dbid) FROM gp_segment_configuration where role = 'p' and content = 0;

1: set optimizer = off;
1: set enable_seqscan=off;
-- Should generate Bitmap Heap Scan on the bitmap index.
1: EXPLAIN (COSTS OFF) SELECT * FROM bmupdate WHERE id = 97;
-- Query should suspend on the first fault injection which finish read the first bitmap page.
1&: SELECT count(*) FROM bmupdate WHERE id = 97;

-- Insert will insert new tid in the first bitmap page and cause the word expand
-- and rearrange exceed words to next bitmap page.
-- The reason it not insert at the end of bitmap LOV is because right now only one
-- transaction doing the insert, and it'll insert to small seg file number.
2: INSERT INTO bmupdate VALUES (97);

-- Query should read the first page(buffer lock released), and then INSERT insert to
-- the first page which will trigger rearrange words.
SELECT gp_wait_until_triggered_fault('rearrange_word_to_next_bitmap_page', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = 0;
SELECT gp_inject_fault('rearrange_word_to_next_bitmap_page', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = 0;

-- Insert triggered rearrange
SELECT gp_wait_until_triggered_fault('after_read_one_bitmap_idx_page', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = 0;
SELECT gp_inject_fault('after_read_one_bitmap_idx_page', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = 0;

-- Should return the correct tuple count with id=97. It used to raise assertion failure for
-- AO tables. This is because the wrong tid transform to an invalud AOTupleId.
1<:

-- Let's check the total tuple count after the test.
SELECT count(*) FROM bmupdate WHERE id = 97;

--
-- Test 2, run Bitmap Heap Scan on the bitmap index that match multiple keys when there's backend
-- insert running.
--

-- Inject fault after read the first bitmap page when query the table.
SELECT gp_inject_fault_infinite('after_read_one_bitmap_idx_page', 'suspend', dbid) FROM gp_segment_configuration where role = 'p' and content = 0;

-- Inject fault when insert new tid cause rearrange words from current
-- bitmap page to next bitmap page.
SELECT gp_inject_fault_infinite('rearrange_word_to_next_bitmap_page', 'skip', dbid) FROM gp_segment_configuration where role = 'p' and content = 0;

-- Should generate Bitmap HEAP Scan on the bitmap index that match multiple keys.
1: EXPLAIN (COSTS OFF) SELECT * FROM bmupdate WHERE id >= 97 and id <= 99 and gp_segment_id = 0;
-- Query should suspend on the first fault injection which finish read the first bitmap page.
1&: SELECT count(*) FROM bmupdate WHERE id >= 97 and id <= 99 and gp_segment_id = 0;

-- Insert will insert new tid in the first bitmap page and cause the word expand
-- and rearrange exceed words to next bitmap page.
-- The reason it not insert at the end of bitmap LOV is because right now only one
-- transaction doing the insert, and it'll insert to small seg file number.
-- Here insert both values to make sure update on full bitmap happens for one LOV.
2: INSERT INTO bmupdate SELECT 97 FROM generate_series(1, 1000);
2: INSERT INTO bmupdate SELECT 99 FROM generate_series(1, 1000);

-- Query should read the first page(buffer lock released), and then INSERT insert to
-- the first page which will trigger rearrange words.
SELECT gp_wait_until_triggered_fault('rearrange_word_to_next_bitmap_page', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = 0;
SELECT gp_inject_fault('rearrange_word_to_next_bitmap_page', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = 0;

-- Insert triggered rearrange
SELECT gp_wait_until_triggered_fault('after_read_one_bitmap_idx_page', 1, dbid) FROM gp_segment_configuration where role = 'p' and content = 0;
SELECT gp_inject_fault('after_read_one_bitmap_idx_page', 'reset', dbid) FROM gp_segment_configuration where role = 'p' and content = 0;

-- Should return the correct tuple count with id=97. It used to raise assertion failure for
-- AO tables. This is because the wrong tid transform to an invalud AOTupleId.
1<:

-- Let's check the total tuple count after the test.
SELECT count(*) FROM bmupdate WHERE id >= 97 and id <= 99 and gp_segment_id = 0;

-- Regression test, when large amount of inserts concurrent inserts happen,
-- querying the table shouldn't take along time.
-- This test is from https://github.com/greenplum-db/gpdb/issues/15389
DROP TABLE IF EXISTS bug.let_me_out;
DROP SCHEMA IF EXISTS bug;
CREATE SCHEMA bug;
CREATE TABLE bug.let_me_out
(
  date_column date NULL,
  int_column  int4 NULL
)
WITH (appendonly = true, orientation = column)
distributed randomly;

1&: INSERT INTO bug.let_me_out(date_column, int_column)
   SELECT ('2017-01-01'::timestamp + random() * ('2023-08-10'::timestamp - '2017-01-01'::timestamp))::date AS date_column,
       id / 50000 AS int_column
       -- id % 700 as int_column
   FROM generate_series(1, 30000000) s(id);

2&: INSERT INTO bug.let_me_out(date_column, int_column)
   SELECT ('2017-01-01'::timestamp + random() * ('2023-08-10'::timestamp - '2017-01-01'::timestamp))::date AS date_column,
       id / 50000 AS int_column
       -- id % 700 as int_column
   FROM generate_series(30000000, 50000000) s(id);

1<:
2<:

CREATE INDEX idx_let_me_out__date_column ON bug.let_me_out USING bitmap (date_column);
CREATE INDEX idx_let_me_out__int_column ON bug.let_me_out USING bitmap (int_column);
VACUUM FULL ANALYZE bug.let_me_out;

SET random_page_cost = 1;
-- expected to finish under 250ms, but if we go over 60000, then something really bad happened
SET statement_timeout=60000;
EXPLAIN ANALYZE
SELECT date_column,
       int_column
FROM bug.let_me_out
WHERE date_column in ('2023-03-19', '2023-03-08', '2023-03-13', '2023-03-29', '2023-03-20', '2023-03-28', '2023-03-23', '2023-03-04', '2023-03-05', '2023-03-18', '2023-03-14', '2023-03-06', '2023-03-15', '2023-03-31', '2023-03-11', '2023-03-21', '2023-03-24', '2023-03-30', '2023-03-26', '2023-03-03', '2023-03-22', '2023-03-01', '2023-03-12', '2023-03-17', '2023-03-27', '2023-03-07', '2023-03-16', '2023-03-10', '2023-03-25', '2023-03-09', '2023-03-02')
AND
int_column IN (1003,1025,1026,1033,1034,1216,1221,160,161,1780,3049,305,3051,3052,3069,3077,3083,3084,3092,3121,3122,3123,3124,3180,3182,3183,3184,3193,3225,3226,3227,3228,3234,3267,3269,3270,3271,3272,3277,3301,3302,3303,3305,3307,3308,3310,3314,3317,3318,3319,3320,3321,3343,3344,3345,3347,3348,3388,339,341,345,346,347,349,3522,3565,3606,3607,3610,3612,3613,3637,3695,3738,3739,3740,3741,3742,3764,3829,3859,3861,3864,3865,3866,3867,3870,3871,3948,3967,3969,3971,3974,3975,3976,4043,4059,4061,4062,4064,4065,4069,4070,4145,42,423,4269,43,4300,4303,4308,4311,4312,4313,4361,4449,445,446,4475,4476,4479,4480,4483,4485,4486,450,4581,4609,4610,4611,4613,4614,4685,4707,4708,4709,4710,4799,4800,4825,4831,4832,4905,4940,4941,4942,4945,4947,4948,4953,4954,4957,540,572,627,743,762,763,77,787,80,81,84,871,899,901,902,905,906);
