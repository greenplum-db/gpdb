create schema orca_skew;
-- start_ignore
GRANT ALL ON SCHEMA orca_skew TO PUBLIC;
SET search_path to orca_skew, public;
-- end_ignore

-- start_ignore
SET optimizer_trace_fallback to on;
-- end_ignore

set optimizer_skew_factor = 1;

-- Verify ORCA chooses broadcast over redistribution for skewed join 
-- t1 skewed, randomly distributed
-- t2 uniform, randomly distributed
-- t1, t2 have ~25 rows

-- start_ignore
DROP TABLE t1;
DROP TABLE t2;
-- end_ignore

CREATE TABLE t1 (
    c11 integer,
    c12 integer
)
 DISTRIBUTED RANDOMLY;

CREATE TABLE t2 (
    c21 integer,
    c22 integer
)
 DISTRIBUTED RANDOMLY;

-- t1: 6 distinct values
-- (0,1) 20 rows (skewed)
-- (1,2) 1 row
-- (2,3) 1 row
-- (3,4) 1 row
-- (5,6) 1 row
-- (6,7) 1 row

insert into t1 select 0,1 from generate_series(1,20);
insert into t1 select 1,2;
insert into t1 select 2,3;
insert into t1 select 3,4;
insert into t1 select 5,6;
insert into t1 select 6,7;

-- t2: 9 distinct values
-- (7,8) 3 rows
-- (8,9) 3 rows
-- (9,10) 3 rows
-- (10,11) 3 rows
-- (12,13) 3 rows
-- (15,16) 3 rows
-- (16,1) 3 rows (match)
-- (17,2) 3 rows (match)
-- (20,3) 3 rows (match)

insert into t2 select 7,8 from generate_series(1,3);
insert into t2 select 8,9 from generate_series(1,3);
insert into t2 select 9,10 from generate_series(1,3);
insert into t2 select 10,11 from generate_series(1,3);
insert into t2 select 12,13 from generate_series(1,3);
insert into t2 select 15,16 from generate_series(1,3);
insert into t2 select 16,1 from generate_series(1,3);
insert into t2 select 17,2 from generate_series(1,3);
insert into t2 select 20,3 from generate_series(1,3);

ANALYZE t1;
ANALYZE t2;

EXPLAIN SELECT
  c12, c22
  FROM
  t1 INNER JOIN t2
    ON c12 = c22;

-- Verify ORCA chooses broadcast over redistribution for skewed multiple joins 
-- t1 has a skewed value (0,1) matching tuples (16,1) in t2
-- the result of t1 ⋈ t2 also tends to skew

-- start_ignore
DROP TABLE t3;
-- end_ignore

CREATE TABLE t3 (
    c31 integer,
    c32 integer
)
 DISTRIBUTED RANDOMLY;

-- t3: 3 distinct values
-- (0,1) 7 rows
-- (1,2) 7 rows
-- (2,3) 7 rows

insert into t3 select 0,1 from generate_series(1,7);
insert into t3 select 1,2 from generate_series(1,7);
insert into t3 select 2,3 from generate_series(1,7);

ANALYZE t3;

-- Force ORCA to execute t1 ⋈ t2 first
set optimizer_join_order = query;

EXPLAIN SELECT
  c12, c22, c32
  FROM
  t1 INNER JOIN t2
    ON c12 = c22
  INNER JOIN t3
    ON c22 = c32;

-- Reset join order
set optimizer_join_order = exhaustive2;

-- Verify ORCA chooses redistribution over broadcast
-- when the data skew isn't pronuounced

-- start_ignore
DROP TABLE t4;
-- end_ignore

CREATE TABLE t4 (
    c41 integer,
    c42 integer
)
 DISTRIBUTED RANDOMLY;

-- t4: 3 distinct values
-- (3,2) 8 rows
-- (4,3) 6 rows
-- (5,4) 4 rows

insert into t4 select 3,2 from generate_series(1,8);
insert into t4 select 4,3 from generate_series(1,6);
insert into t4 select 5,4 from generate_series(1,4);

ANALYZE t4;

EXPLAIN SELECT
  c32, c42
  FROM
  t3 INNER JOIN t4
    ON c32 = c42;

-- user option to emphasize data skew by multiplying
-- the skew ratio with a larger-than 1 skew factor
set optimizer_skew_factor = 25;

EXPLAIN SELECT
  c32, c42
  FROM
  t3 INNER JOIN t4
    ON c32 = c42;

-- reset skew factor
set optimizer_skew_factor = 1;

-- Currently ORCA doesn't introduce a shuffle motion
-- before joining a table distributed by a skewed column
-- To demonstrate that, we recreate t1 and t2 with the same
-- data but distribute them by the c*1 column
-- t1 skewed, distributed by c11
-- t2 uniform, distributed by c21
-- t1, t2 have ~25 rows

-- start_ignore
DROP TABLE t1;
DROP TABLE t2;
-- end_ignore

CREATE TABLE t1 (
    c11 integer,
    c12 integer
)
 DISTRIBUTED BY (c11);

CREATE TABLE t2 (
    c21 integer,
    c22 integer
)
 DISTRIBUTED BY (c21);

-- t1: 6 distinct values
-- segment 1:
-- (0,1) 20 rows
-- (1,2) 1 rows
-- segment 2:
-- (2,3) 1 rows
-- (3,4) 1 rows
-- segment 3:
-- (5,6) 1 rows
-- (6,7) 1 rows

insert into t1 select 0,1 from generate_series(1,20);
insert into t1 select 1,2;
insert into t1 select 2,3;
insert into t1 select 3,4;
insert into t1 select 5,6;
insert into t1 select 6,7;

-- t2: 6 distinct values
-- segment 1:
-- (7,8) 3 rows
-- (8,9) 3 rows
-- (16,1) 3 rows
-- segment 2:
-- (9,10) 3 rows
-- (10,11) 3 rows
-- (17,2) 3 rows
-- segment 3:
-- (12,13) 3 rows
-- (15,16) 3 rows
-- (20,3) 3 rows

insert into t2 select 7,8 from generate_series(1,3);
insert into t2 select 8,9 from generate_series(1,3);
insert into t2 select 9,10 from generate_series(1,3);
insert into t2 select 10,11 from generate_series(1,3);
insert into t2 select 12,13 from generate_series(1,3);
insert into t2 select 15,16 from generate_series(1,3);
insert into t2 select 16,1 from generate_series(1,3);
insert into t2 select 17,2 from generate_series(1,3);
insert into t2 select 20,3 from generate_series(1,3);


ANALYZE t1;
ANALYZE t2;

EXPLAIN SELECT
  c12, c22
  FROM
  t1 INNER JOIN t2
    ON c12 = c22;

reset optimizer_skew_factor;

-- start_ignore
DROP SCHEMA orca_skew CASCADE;
-- end_ignore
