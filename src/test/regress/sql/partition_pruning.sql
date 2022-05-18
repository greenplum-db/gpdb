--
-- Tests on partition pruning (with ORCA) or constraint exclusion (with the
-- Postgres planner). These tests check that you get an "expected" plan, that
-- only scans the partitions that are needed.
--
-- The "correct" plan for a given query depends a lot on the capabilities of
-- the planner and the rest of the system, so the expected output can need
-- updating, as the system improves.
--

-- Use index scans when possible. That exercises more code, and allows us to
-- spot the cases where the planner cannot use even when it exists.
set enable_seqscan=off;
set enable_bitmapscan=on;
set enable_indexscan=on;

create schema partition_pruning;
set search_path to partition_pruning;

-- Set up common test tables.
CREATE TABLE pt_lt_tab
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
)
distributed by (col1)
partition by list(col2)
(
  partition part1 values(1,2,3,4,5,6,7,8,9,10),
  partition part2 values(11,12,13,14,15,16,17,18,19,20),
  partition part3 values(21,22,23,24,25,26,27,28,29,30),
  partition part4 values(31,32,33,34,35,36,37,38,39,40),
  partition part5 values(41,42,43,44,45,46,47,48,49,50)
);

INSERT INTO pt_lt_tab SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab SELECT i, i,'q',False FROM generate_series(47,50)i;
ANALYZE pt_lt_tab;

-- pt_lt_tab_df is the same as pt_lt_tab, but with a default partition (and some
-- values in the default partition, including NULLs).
CREATE TABLE pt_lt_tab_df
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
)
distributed by (col1)
partition by list(col2)
(
  partition part1 VALUES(1,2,3,4,5,6,7,8,9,10),
  partition part2 VALUES(11,12,13,14,15,16,17,18,19,20),
  partition part3 VALUES(21,22,23,24,25,26,27,28,29,30),
  partition part4 VALUES(31,32,33,34,35,36,37,38,39,40),
  partition part5 VALUES(41,42,43,44,45,46,47,48,49,50),
  default partition def
);

INSERT INTO pt_lt_tab_df SELECT i, i,'a',True FROM generate_series(1,3)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'b',True FROM generate_series(4,6)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'c',True FROM generate_series(7,10)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'e',True FROM generate_series(11,13)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'f',True FROM generate_series(14,16)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'g',True FROM generate_series(17,20)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'i',False FROM generate_series(21,23)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'k',False FROM generate_series(24,26)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'h',False FROM generate_series(27,30)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'m',False FROM generate_series(31,33)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'o',False FROM generate_series(34,36)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'n',False FROM generate_series(37,40)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'p',False FROM generate_series(41,43)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'s',False FROM generate_series(44,46)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'q',False FROM generate_series(47,50)i;

INSERT INTO pt_lt_tab_df SELECT i, i,'u',True FROM generate_series(51,53)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'x',True FROM generate_series(54,56)i;
INSERT INTO pt_lt_tab_df SELECT i, i,'w',True FROM generate_series(57,60)i;

INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);
INSERT INTO pt_lt_tab_df VALUES(NULL,NULL,NULL,NULL);

ANALYZE pt_lt_tab_df;

--
-- Test that stable functions are evaluated when constructing the plan. This
-- differs from PostgreSQL. In PostgreSQL, PREPARE/EXECUTE creates a reusable
-- plan, while in GPDB, we re-plan the query on every execution, so that the
-- stable function is executed during planning, and we can therefore do
-- partition pruning based on its result.
--
create or replace function stabletestfunc() returns integer as $$
begin
  return 10;
end;
$$ language plpgsql stable;

PREPARE prep_prune AS select * from pt_lt_tab WHERE col2 = stabletestfunc();

-- The plan should only scan one partition, where col2 = 10.
EXPLAIN EXECUTE prep_prune;

-- Also test that Params are const-evaluated.
PREPARE prep_prune_param AS select * from pt_lt_tab WHERE col2 = $1;
EXPLAIN EXECUTE prep_prune_param(10);


-- @description B-tree single index key = non-partitioning key
CREATE INDEX idx1 on pt_lt_tab(col1);

SELECT * FROM pt_lt_tab WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col1 = 25 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 = 25 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col1 <> 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 <> 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col1 > 10 AND col1 < 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 > 10 AND col1 < 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col1 > 10 OR col1 = 25 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 > 10 OR col1 = 25 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col1 between 10 AND 25 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 between 10 AND 25 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;

-- @description B-tree single index key = partitioning key
CREATE INDEX idx1 on pt_lt_tab(col2);

SELECT * FROM pt_lt_tab WHERE col2 < 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 < 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 > 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 > 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 > 10 AND col2 < 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 > 10 AND col2 < 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 > 10 OR col2 = 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 > 10 OR col2 = 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;


-- @description multiple column b-tree index
CREATE INDEX idx1 on pt_lt_tab(col1,col2);

SELECT * FROM pt_lt_tab WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 > 10 AND col1 = 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 > 10 AND col1 = 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 > 10.00 OR col1 = 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 > 10.00 OR col1 = 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;

-- @description multi-column unique constraint (= b-tree index). Essentially the
-- same as the previous case, but the columns are the other way 'round, and we
-- do this on the table with default partition.
ALTER TABLE pt_lt_tab_df ADD CONSTRAINT col2_col1_unique unique(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col1 < 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col1 > 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 = 25 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 > 10 AND col1 = 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 > 10 AND col1 = 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 > 10.00 OR col1 = 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 > 10.00 OR col1 = 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 between 10 AND 50 ORDER BY col2,col3 LIMIT 5;

ALTER TABLE pt_lt_tab_df DROP CONSTRAINT col2_col1_unique;


-- @description Heterogeneous index, index on partition key, b-tree index on all partitions
CREATE INDEX idx1 on pt_lt_tab_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_1_prt_part5(col2);

SELECT * FROM pt_lt_tab WHERE col2 between 1 AND 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 between 1 AND 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 > 5 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 > 5 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 = 5 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 = 5 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;
DROP INDEX idx2;
DROP INDEX idx3;
DROP INDEX idx4;
DROP INDEX idx5;


-- @description Heterogeneous index,b-tree index on all parts,index, index on non-partition col
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col1);

SELECT * FROM pt_lt_tab_df WHERE col1 between 1 AND 100 ORDER BY col1 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col1 between 1 AND 100 ORDER BY col1 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col1 > 50 ORDER BY col1 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col1 > 50 ORDER BY col1 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col1 < 50 ORDER BY col1 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col1 < 50 ORDER BY col1 LIMIT 5;

DROP INDEX idx1;
DROP INDEX idx2;
DROP INDEX idx3;
DROP INDEX idx4;
DROP INDEX idx5;
DROP INDEX idx6;


-- @description Heterogeneous index,b-tree index on all parts including default, index on partition col
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2);

SELECT * FROM pt_lt_tab_df WHERE col2 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 > 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 > 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 <> 10 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 < 50 AND col1 > 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 < 50 AND col1 > 10 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;
DROP INDEX idx2;
DROP INDEX idx3;
DROP INDEX idx4;
DROP INDEX idx5;
DROP INDEX idx6;


-- @description Negative tests Combination tests, no index on default partition
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2);

SELECT * FROM pt_lt_tab_df WHERE col2 > 51 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 > 51 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;
DROP INDEX idx2;
DROP INDEX idx3;
DROP INDEX idx4;
DROP INDEX idx5;

-- @description Negative tests Combination tests ,index exists on some regular partitions and not on the default partition
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2);

SELECT * FROM pt_lt_tab_df WHERE col2 is NULL ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;
DROP INDEX idx5;


-- @description Heterogeneous index,b-tree index on all parts,index , multiple index 
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2,col1);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2,col1);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2,col1);
CREATE INDEX idx4 on pt_lt_tab_df_1_prt_part4(col2,col1);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2,col1);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2,col1);

SELECT * FROM pt_lt_tab_df WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 = 50 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col2 > 10 AND col1 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 > 10 AND col1 between 1 AND 100 ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab_df WHERE col1 = 10 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col1 = 10 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;
DROP INDEX idx2;
DROP INDEX idx3;
DROP INDEX idx4;
DROP INDEX idx5;
DROP INDEX idx6;


-- @description Index exists on some continuous set of partitions, e.g. p1,p2,p3
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx2 on pt_lt_tab_df_1_prt_part2(col2);
CREATE INDEX idx3 on pt_lt_tab_df_1_prt_part3(col2);

SELECT * FROM pt_lt_tab_df WHERE col2 = 35 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 = 35 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;
DROP INDEX idx2;
DROP INDEX idx3;


-- @description Index exists on some regular partitions and on the default partition [INDEX exists on non-consecutive partitions, e.g. p1,p3,p5]
CREATE INDEX idx1 on pt_lt_tab_df_1_prt_part1(col2);
CREATE INDEX idx5 on pt_lt_tab_df_1_prt_part5(col2);
CREATE INDEX idx6 on pt_lt_tab_df_1_prt_def(col2);

SELECT * FROM pt_lt_tab_df WHERE col2 > 15 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab_df WHERE col2 > 15 ORDER BY col2,col3 LIMIT 5;

DROP INDEX idx1;
DROP INDEX idx5;
DROP INDEX idx6;


--
-- Finally, after running all the other tests on pg_lt_tab, test that
-- partition pruning still works after dropping a column
--
CREATE INDEX idx1 on pt_lt_tab(col4);

ALTER TABLE pt_lt_tab DROP column col1;

SELECT * FROM pt_lt_tab WHERE col4 is False ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col4 is False ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col4 = False ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col4 = False ORDER BY col2,col3 LIMIT 5;
SELECT * FROM pt_lt_tab WHERE col2 > 41 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 > 41 ORDER BY col2,col3 LIMIT 5;

ALTER TABLE pt_lt_tab DROP column col4;

SELECT * FROM pt_lt_tab WHERE col2 > 41 ORDER BY col2,col3 LIMIT 5;
EXPLAIN SELECT * FROM pt_lt_tab WHERE col2 > 41 ORDER BY col2,col3 LIMIT 5;


--
-- Test a more complicated partitioning scheme, with subpartitions.
--
CREATE TABLE pt_complex (i int, j int, k int, l int, m int) DISTRIBUTED BY (i)
PARTITION BY list(k)
  SUBPARTITION BY list(j) SUBPARTITION TEMPLATE (subpartition p11 values (1), subpartition p12 values(2))
  SUBPARTITION BY list(l, m) SUBPARTITION TEMPLATE (subpartition p11 values ((1,1)), subpartition p12 values((2,2)))
( partition p1 values(1), partition p2 values(2));

INSERT INTO pt_complex VALUES (1, 1, 1, 1, 1), (2, 2, 2, 2, 2);

CREATE INDEX i_pt_complex ON pt_complex (i);

SELECT * FROM pt_complex WHERE i = 1 AND j = 1;
EXPLAIN SELECT * FROM pt_complex WHERE i = 1 AND j = 1;

--
-- See MPP-6861
--
CREATE TABLE ds_4
(
  month_id character varying(6),
  cust_group_acc numeric(10),
  mobile_no character varying(10),
  source character varying(12),
  vas_group numeric(10),
  vas_type numeric(10),
  count_vas integer,
  amt_vas numeric(10,2),
  network_type character varying(3),
  execution_id integer
)
WITH (
  OIDS=FALSE
)
DISTRIBUTED BY (cust_group_acc, mobile_no)
PARTITION BY LIST(month_id)
          (
          PARTITION p200800 VALUES('200800'),
          PARTITION p200801 VALUES('200801'),
          PARTITION p200802 VALUES('200802'),
          PARTITION p200803 VALUES('200803')
);

-- this is the case that worked before MPP-6861
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id = '200800';


-- now we can evaluate this function at planning/prune time
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id::int = 200800;

-- this will be satisfied by 200800
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id::int - 801 < 200000;

-- test OR case -- should NOT get pruning
explain select * from ds_4 where month_id::int - 801 < 200000 OR count_vas > 10;

-- test AND case -- should still get pruning
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id::int - 801 < 200000 AND count_vas > 10;

-- test expression case : should get pruning
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where case when month_id = '200800' then 100 else 2 end = 100;

-- test expression case : should get pruning
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where case when month_id = '200800' then NULL else 2 end IS NULL;

-- should still get pruning here -- count_vas is only used in the path for month id = 200800
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where case when month_id::int = 200800 then count_vas else 2 end IS NULL;

-- do one that matches a couple partitions
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain select * from ds_4 where month_id::int in (200801, 1,55,6,6,6,6,66,565,65,65,200803);

-- cleanup
drop table ds_4;


--
-- See MPP-18979
--

CREATE TABLE ds_2
(
  month_id character varying(6),
  cust_group_acc numeric(10),
  mobile_no character varying(10),
  source character varying(12),
  vas_group numeric(10),
  vas_type numeric(10),
  count_vas integer,
  amt_vas numeric(10,2),
  network_type character varying(3),
  execution_id integer
)
WITH (
  OIDS=FALSE
)
DISTRIBUTED BY (cust_group_acc, mobile_no)
PARTITION BY LIST(month_id)
          (
          PARTITION p200800 VALUES('200800'),
          PARTITION p200801 VALUES('200801'),
          PARTITION p200802 VALUES('200802'),
          PARTITION p200803 VALUES('200803'),
          PARTITION p200804 VALUES('200804'),
          PARTITION p200805 VALUES('200805'),
          PARTITION p200806 VALUES('200806'),
          PARTITION p200807 VALUES('200807'),
          PARTITION p200808 VALUES('200808'),
          PARTITION p200809 VALUES('200809')
);

insert into ds_2(month_id) values('200800');
insert into ds_2(month_id) values('200801');
insert into ds_2(month_id) values('200802');
insert into ds_2(month_id) values('200803');
insert into ds_2(month_id) values('200804');
insert into ds_2(month_id) values('200805');
insert into ds_2(month_id) values('200806');
insert into ds_2(month_id) values('200807');
insert into ds_2(month_id) values('200808');
insert into ds_2(month_id) values('200809');

-- queries without bitmap scan
-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
set optimizer_segments=2;
explain select * from ds_2 where month_id::int in (200808, 1315) order by month_id;

-- start_ignore
-- Known_opt_diff: MPP-21316
-- end_ignore
explain  select * from ds_2 where month_id::int in (200808, 200801, 2008010) order by month_id;
reset optimizer_segments;
select * from ds_2 where month_id::int in (200907, 1315) order by month_id;

select * from ds_2 where month_id::int in (200808, 1315) order by month_id;

select * from ds_2 where month_id::int in (200808, 200801) order by month_id;

select * from ds_2 where month_id::int in (200808, 200801, 2008010) order by month_id;

-- cleanup
drop table ds_2;

drop table if exists dnsdata cascade;

CREATE TABLE dnsdata(dnsname text) DISTRIBUTED RANDOMLY;

CREATE INDEX dnsdata_d1_idx ON dnsdata USING bitmap (split_part(reverse(dnsname),'.'::text,1));


CREATE INDEX dnsdata_d2_idx ON dnsdata USING bitmap (split_part(reverse(dnsname),'.'::text,2));

insert into dnsdata values('www.google.com');
insert into dnsdata values('www.google1.com');
insert into dnsdata values('1.google.com');
insert into dnsdata values('2.google.com');
insert into dnsdata select 'www.b.com' from generate_series(1, 100000) as x(a);

analyze dnsdata;

-- queries with bitmap scan enabled
set enable_bitmapscan=on;
set enable_indexscan=on;
set enable_seqscan=off;

Select dnsname from dnsdata
where (split_part(reverse('cache.google.com'),'.',1))=(split_part(reverse(dnsname),'.',1))
and (split_part(reverse('cache.google.com'),'.',2))=(split_part(reverse(dnsname),'.',2)) 
order by dnsname;

Select dnsname from dnsdata
where (split_part(reverse('cache.google.com'),'.',1))=(split_part(reverse(dnsname),'.',1))
and (split_part(reverse('cache.google.com'),'.',2))=(split_part(reverse(dnsname),'.',2))
and dnsname = 'cache.google.com'
order by dnsname;

-- cleanup
drop table dnsdata cascade;


Create or replace function ZeroFunc(int) Returns int as $BODY$
BEGIN
  RETURN 0;
END;
$BODY$ LANGUAGE plpgsql IMMUTABLE;

drop table if exists mytable cascade;

create table mytable(i int, j int);
insert into mytable select x, x+1 from generate_series(1, 100000) as x;
analyze mytable;

CREATE INDEX mytable_idx1 ON mytable USING bitmap(ZeroFunc(i));


select * from mytable where ZeroFunc(i)=0 and i=100 order by i;

select * from mytable where ZeroFunc(i)=0 and i=-1 order by i;

-- cleanup
drop function ZeroFunc(int) cascade;
drop table mytable cascade;


-- start_ignore
create language plpythonu;
-- end_ignore


-- @description Tests for static partition selection (MPP-24709, GPSQL-2879)

create or replace function get_selected_parts(explain_query text) returns text as
$$
rv = plpy.execute(explain_query)
search_text = 'Partition Selector'
result = []
result.append(0)
result.append(0)
selected = 0
out_of = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        j = i+1
        temp_line = rv[j]['QUERY PLAN']
        while temp_line.find('Partitions selected:') == -1:
            j += 1
            if j == len(rv) - 1:
                break
            temp_line = rv[j]['QUERY PLAN']

        if temp_line.find('Partitions selected:') != -1:
            selected += int(temp_line[temp_line.index('selected: ')+10:temp_line.index(' (out')])
            out_of += int(temp_line[temp_line.index('out of')+6:temp_line.index(')')])
result[0] = selected
result[1] = out_of
return result
$$
language plpythonu;

drop table if exists partprune_foo;
create table partprune_foo(a int, b int, c int) partition by range (b) (start (1) end (101) every (10));
insert into partprune_foo select generate_series(1,5), generate_series(1,100), generate_series(1,10);
analyze partprune_foo;

select get_selected_parts('explain select * from partprune_foo;');
select * from partprune_foo;

select get_selected_parts('explain select * from partprune_foo where b = 35;');
select * from partprune_foo where b = 35;

select get_selected_parts('explain select * from partprune_foo where b < 35;');
select * from partprune_foo where b < 35;

select get_selected_parts('explain select * from partprune_foo where b in (5, 6, 14, 23);');
select * from partprune_foo where b in (5, 6, 14, 23);

select get_selected_parts('explain select * from partprune_foo where b < 15 or b > 60;');
select * from partprune_foo where b < 15 or b > 60;

select get_selected_parts('explain select * from partprune_foo where b = 150;');
select * from partprune_foo where b = 150;

select get_selected_parts('explain select * from partprune_foo where b = a*5;');
select * from partprune_foo where b = a*5;

-- Test with IN() lists
-- Number of elements > threshold, partition elimination is not performed
set optimizer_array_expansion_threshold = 3;
select get_selected_parts('explain select * from partprune_foo where b in (5, 6, 14, 23);');
select * from partprune_foo where b in (5, 6, 14, 23);

reset optimizer_array_expansion_threshold;

-- Test "ANY (<array>)" syntax.
select get_selected_parts($$ explain select * from partprune_foo where b = ANY ('{5, 6, 14}') $$);
select * from partprune_foo where b = ANY ('{5, 6, 14}');

select get_selected_parts($$ explain select * from partprune_foo where b < ANY ('{12, 14, 11}') $$);
select * from partprune_foo where b < ANY ('{12, 14, 11}');


-- Check for all the different number of partition selections
DROP TABLE IF EXISTS DATE_PARTS;
CREATE TABLE DATE_PARTS (id int, year int, month int, day int, region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
    SUBPARTITION BY LIST (month)
       SUBPARTITION TEMPLATE (
        SUBPARTITION Q1 VALUES (1, 2, 3), 
        SUBPARTITION Q2 VALUES (4 ,5 ,6),
        SUBPARTITION Q3 VALUES (7, 8, 9),
        SUBPARTITION Q4 VALUES (10, 11, 12),
        DEFAULT SUBPARTITION other_months )
        	SUBPARTITION BY RANGE(day)
        		SUBPARTITION TEMPLATE (
        		START (1) END (31) EVERY (10), 
		        DEFAULT SUBPARTITION other_days)
( START (2002) END (2012) EVERY (4), 
  DEFAULT PARTITION outlying_years );

insert into DATE_PARTS select i, extract(year from dt), extract(month from dt), extract(day from dt), NULL from (select i, '2002-01-01'::date + i * interval '1 day' day as dt from generate_series(1, 3650) as i) as t;

-- Expected total parts => 4 * 1 * 4 => 16: 
-- TODO #141973839: we selected extra parts because of disjunction: 32 parts: 4 * 2 * 4
select get_selected_parts('explain analyze select * from DATE_PARTS where month between 1 and 3;');

-- Expected total parts => 4 * 2 * 4 => 32: 
-- TODO #141973839: we selected extra parts because of disjunction: 48 parts: 4 * 3 * 4
select get_selected_parts('explain analyze select * from DATE_PARTS where month between 1 and 4;');

-- Expected total parts => 1 * 2 * 4 => 8: 
-- TODO #141973839: we selected extra parts because of disjunction: 12 parts: 1 * 3 * 4
select get_selected_parts('explain analyze select * from DATE_PARTS where year = 2003 and month between 1 and 4;');

-- 1 :: 5 :: 4 => 20 // Only default for year
select get_selected_parts('explain analyze select * from DATE_PARTS where year = 1999;');

-- 4 :: 1 :: 4 => 16 // Only default for month
select get_selected_parts('explain analyze select * from DATE_PARTS where month = 13;');

-- 1 :: 1 :: 4 => 4 // Default for both year and month
select get_selected_parts('explain analyze select * from DATE_PARTS where year = 1999 and month = 13;');

-- 4 :: 5 :: 1 => 20 // Only default part for day
select get_selected_parts('explain analyze select * from DATE_PARTS where day = 40;');

-- General predicate
-- TODO #141973839. We expected 112 parts: (month = 1) =>   4 * 1 * 4 => 16, month > 3 => 4 * 4 * 4 => 64, month in (0, 1, 2) => 4 * 1 * 4 => 16, month is NULL => 4 * 1 * 4 => 16.
-- However, we selected 128 parts: (month = 1) =>   4 * 1 * 4 => 16, month > 3 => 4 * 4 * 4 => 64, month in (0, 1, 2) => 4 * 2 * 4 => 32, month is NULL => 4 * 1 * 4 => 16.
select get_selected_parts('explain analyze select * from DATE_PARTS where month = 1 union all select * from DATE_PARTS where month > 3 union all select * from DATE_PARTS where month in (0,1,2) union all select * from DATE_PARTS where month is null;');

-- Equality predicate
-- 16 partitions => 4 from year x 1 from month x 4 from days.
select get_selected_parts('explain analyze select * from DATE_PARTS where month = 3;');  -- Not working (it only produces general)

-- More Equality and General Predicates ---
create table foo(a int, b int)
partition by list (b)
(partition p1 values(1,3), partition p2 values(4,2), default partition other);

-- General predicate
-- Total 6 parts. b = 1: 1 part, b > 3: 2 parts, b in (0, 1): 2 parts. b is null: 1 part
select get_selected_parts('explain analyze select * from foo where b = 1 union all select * from foo where b > 3 union all select * from foo where b in (0,1) union all select * from foo where b is null;');

drop table if exists pt;
CREATE TABLE pt (id int, gender varchar(2)) 
DISTRIBUTED BY (id)
PARTITION BY LIST (gender)
( PARTITION girls VALUES ('F', NULL), 
  PARTITION boys VALUES ('M'), 
  DEFAULT PARTITION other );

-- General filter
-- TODO #141916623. Expecting 6 parts, but optimizer plan selects 7 parts. The 6 parts breakdown is: gender = 'F': 1 part, gender < 'M': 2 parts (including default), gender in ('F', F'M'): 2 parts, gender is null => 1 part
select get_selected_parts('explain analyze select * from pt where gender = ''F'' union all select * from pt where gender < ''M'' union all select * from pt where gender in (''F'', ''FM'') union all select * from pt where gender is null;');

-- DML
-- Non-default part
insert into DATE_PARTS values (-1, 2004, 11, 30, NULL);
select * from date_parts_1_prt_2_2_prt_q4_3_prt_4 where id < 0;

-- Default year
insert into DATE_PARTS values (-2, 1999, 11, 30, NULL);
select * from date_parts_1_prt_outlying_years_2_prt_q4_3_prt_4 where id < 0;

-- Default month
insert into DATE_PARTS values (-3, 2004, 20, 30, NULL);
select * from date_parts_1_prt_2_2_prt_other_months where id < 0;

-- Default day
insert into DATE_PARTS values (-4, 2004, 10, 50, NULL);
select * from date_parts_1_prt_2_2_prt_q4_3_prt_other_days where id < 0;

-- Default everything
insert into DATE_PARTS values (-5, 1999, 20, 50, NULL);
select * from date_parts_1_prt_outlying_years_2_prt_other_mo_3_prt_other_days where id < 0;

-- Default month + day but not year
insert into DATE_PARTS values (-6, 2002, 20, 50, NULL);
select * from date_parts_1_prt_2_2_prt_other_months_3_prt_other_days where id < 0;

-- Dropped columns with exchange
drop table if exists sales;
CREATE TABLE sales (trans_id int, to_be_dropped1 int, date date, amount 
decimal(9,2), to_be_dropped2 int, region text) 
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (date)
SUBPARTITION BY LIST (region)
SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), 
  SUBPARTITION asia VALUES ('asia'), 
  SUBPARTITION europe VALUES ('europe'), 
  DEFAULT SUBPARTITION other_regions)
  (START (date '2011-01-01') INCLUSIVE
   END (date '2012-01-01') EXCLUSIVE
   EVERY (INTERVAL '3 month'), 
   DEFAULT PARTITION outlying_dates );

-- This will introduce different column numbers in subsequent part tables
alter table sales drop column to_be_dropped1;
alter table sales drop column to_be_dropped2;

-- Create the exchange candidate without dropped columns
drop table if exists sales_exchange_part;
create table sales_exchange_part (trans_id int, date date, amount 
decimal(9,2), region text);

-- Insert some data
insert into sales_exchange_part values(1, '2011-01-01', 10.1, 'usa');

-- Exchange
ALTER TABLE sales 
ALTER PARTITION FOR (RANK(1))
EXCHANGE PARTITION FOR ('usa') WITH TABLE sales_exchange_part ;
ANALYZE sales;

-- Expect 10 parts. First level: 4 parts + 1 default. Second level 2 parts.
select get_selected_parts('explain analyze select * from sales where region = ''usa'' or region = ''asia'';');
select * from sales where region = 'usa' or region = 'asia';

-- Test DynamicIndexScan with extra filter
create index idx_sales_date on sales(date);
explain select * from sales where date = '2011-01-01' and region = 'usa';
select * from sales where date = '2011-01-01' and region = 'usa';

-- Updating partition key

select * from sales_1_prt_2_2_prt_usa;
select * from sales_1_prt_2_2_prt_europe;
update sales set region = 'europe' where trans_id = 1;
select * from sales_1_prt_2_2_prt_europe;
select * from sales_1_prt_2_2_prt_usa;
select * from sales;

-- Distinct From
drop table if exists bar;
CREATE TABLE bar (i INTEGER, j decimal)
partition by list (j)
subpartition by range (i) subpartition template (start(1) end(4) every(2))
(partition p1 values(0.2,2.8, NULL), partition p2 values(1.7,3.1),
partition p3 values(5.6), default partition other);

insert into bar values(1, 0.2); --p1
insert into bar values(1, 1.7); --p2
insert into bar values(1, 2.1); --default
insert into bar values(1, 5.6); --default
insert into bar values(1, NULL); --p1

-- In-equality
-- 8 parts: All 4 parts on first level and each will have 2 range parts 
select get_selected_parts('explain analyze select * from bar where j>0.02;');
-- 6 parts: Excluding 1 list parts at first level. So, 3 at first level and each has 2 at second level.
select get_selected_parts('explain analyze select * from bar where j>2.8;');

-- Distinct From
-- 6 parts: Everything except 1 part that contains 5.6.
select get_selected_parts('explain analyze select * from bar where j is distinct from 5.6;');
-- 8 parts: NULL is shared with others on p1. So, all 8 parts.
select get_selected_parts('explain analyze select * from bar where j is distinct from NULL;');

-- Validate that a planner bug found in production is fixed.  The bug
-- caused a SIGSEGV during execution due to incorrect manipulation of
-- target list when adding a junk attribute for a sort key when
-- generating a Merge Join plan with Parition Selector.
--
-- Prerequisites to trigger the bug include dynamic partition
-- elimination node (Partition Selector) in the plan, one or more junk
-- attributes in sort key and presense of a Motion node between the
-- Partition Selector and Append nodes.

create table part_left (id int, pkey timestamp, d int)
distributed by (pkey)
partition by range (pkey)
(start ('2020-12-01 00:00:00'::timestamp)
 end   ('2020-12-04 23:59:59'::timestamp)
 every ('1 day'::interval));

insert into part_left values (1, '2020-12-01 00:00:00'::timestamp, 1);
insert into part_left values (1, '2020-12-02 13:00:00'::timestamp, 2);
insert into part_left values (1, '2020-12-03 14:00:00'::timestamp, 3);

create table part_right (id int, pkey timestamp, d int)
distributed by (id)
partition by range (pkey)
(start ('2020-12-01 00:00:00'::timestamp)
 end   ('2020-12-31 23:59:59'::timestamp)
 every ('1 day'::interval));

insert into part_right values (1, '2020-12-01 12:00:00'::timestamp, 1);
insert into part_right values (1, '2020-12-10 13:00:00'::timestamp, 2);
insert into part_right values (1, '2020-12-20 14:00:00'::timestamp, 3);

analyze part_left;
analyze part_right;

-- Enforce merge join because the plan should have a Sort node.
set enable_hashjoin=off;
set enable_mergejoin=on;

-- The date_trunc() function causes a junk attribute to be generated
-- when computing the sort key for the merge join.  The target list of
-- Sort node is appended with one target entry corresponding to the
-- junk attribute.  Presence of a Result node as a child of the Sort
-- node indicates that the generated plan is correct.  The Result node
-- has its own target list, separate from its child node's target
-- list.  Without a Result node, the target list of Partition Selector
-- node ends up being modified and Partition Node shares the target
-- list with its child, which happens to be Motion node in this case.
-- Motion node does not share targe list with its children.  However,
-- if Motion node's target list does not match its child, there will
-- be trouble during execution because the expected number of
-- attributes received is more than the number of attributes actually
-- sent by the motion sender (aka SIGSEGV).
explain (costs off) select r.id, l.pkey from part_left l inner join part_right r
on (date_trunc('day', r.pkey) = l.pkey
    and r.pkey between '2020-12-01 00:00:00'::timestamp and
                        '2020-12-03 00:00:59'::timestamp
                        );

select r.id, l.pkey from part_left l inner join part_right r
on (date_trunc('day', r.pkey) = l.pkey
    and r.pkey between '2020-12-01 00:00:00'::timestamp and
                        '2020-12-03 00:00:59'::timestamp
                        );

-- Test memory consumption for DML partition pruning for Postgres optimzier
-- Before this patch inheritance_planner build plan for each child relation
-- in spite of provided predicates and as a result, easily overcame protected
-- memory limit

CREATE TABLE t_part1 (
    key1 int, update_me int, dummy1 int, dummy2 int, dummy3 int, dummy4 int, dummy5 int, dummy6 int, dummy7 int,
    dummy8 int, dummy9 int, dummy10 int, dummy11 int, dummy12 int, dummy13 int, dummy14 int, dummy15 int,
    dummy16 int, dummy17 int, dummy18 int, dummy19 int, dummy20 int, dummy21 int, dummy22 int, dummy23 int,
    dummy24 int, dummy25 int, dummy26 int, dummy27 int, dummy28 int, dummy29 int, dummy30 int, dummy31 int,
    dummy32 int, dummy33 int, dummy34 int, dummy35 int, dummy36 int, dummy37 int, dummy38 int, dummy39 int,
    dummy40 int, dummy41 int, dummy42 int, dummy43 int, dummy44 int, dummy45 int, dummy46 int, dummy47 int,
    dummy48 int, dummy49 int, dummy50 int, dummy51 int, dummy52 int, dummy53 int, dummy54 int, dummy55 int,
    dummy56 int, dummy57 int, dummy58 int, dummy59 int, dummy60 int, dummy61 int, dummy62 int, dummy63 int,
    dummy64 int, dummy65 int, dummy67 int, dummy68 int, dummy69 int, dummy70 int, dummy71 int, dummy72 int,
    dummy73 int, dummy74 int, dummy75 int, dummy76 int, dummy77 int, dummy78 int, dummy79 int, dummy80 int,
    dummy81 int, dummy82 int, dummy83 int, dummy84 int
)
DISTRIBUTED BY (key1)
PARTITION BY RANGE (key1) (start (1) end (400) every (1));

explain (costs off)
update t_part1 trg
set update_me = src.update_me
from (
      select
      r.key1,
      r.update_me as update_me,
      row_number() over() rn
      from t_part1 r
      where r.key1 = 2
) src
where  trg.key1 = src.key1
        and trg.key1 = 2;

DROP TABLE t_part1;

-- Test ORCA avoiding default partition scan for all levels with equality predicates
-- Level 0: year 2 parts [2009, 2010), [2010, 2011)
-- Level 1: quarter 5 parts
-- Level 2: region 3 parts
-- Level 3: sales 4 parts
-- Total partitions: 2*5*3*4 = 120
DROP TABLE sales;
CREATE TABLE sales (id bigint, year int, quarter int, region text, sales int) DISTRIBUTED BY (id)
PARTITION BY RANGE(year)
 SUBPARTITION BY LIST (quarter)
 SUBPARTITION TEMPLATE (DEFAULT subpartition other_quarter,
				subpartition q1 VALUES (1),
				subpartition q2 VALUES (2),
				subpartition q3 VALUES (3),
				subpartition q4 VALUES (4))
 SUBPARTITION BY LIST (region)
 SUBPARTITION TEMPLATE (DEFAULT subpartition other_region,
                                subpartition eu VALUES ('eu'),
                                subpartition usa VALUES ('usa'))
 SUBPARTITION BY RANGE (sales)
 SUBPARTITION TEMPLATE (DEFAULT SUBPARTITION other_sales,
				SUBPARTITION below START (100) INCLUSIVE,
       				SUBPARTITION meet START (5000) INCLUSIVE,
       				SUBPARTITION beyond START (10000) INCLUSIVE END (50000) EXCLUSIVE)
(START(2009) END(2011) EVERY(1));

INSERT INTO sales
SELECT generate_series(1,5), 2010, 0, 'asia', generate_series(50,36050,9000)
UNION
SELECT generate_series(6,15), 2009, 1, 'eu', generate_series(1000,4600,400)
UNION
SELECT generate_series(16,20), 2009, 2, 'usa', generate_series(8000,12000,1000)
UNION
SELECT generate_series(21,30), 2010, 3, 'usa', generate_series(4000,22000,2000)
UNION
SELECT generate_series(31,35), 2010, 4, 'usa', generate_series(7000,9000,500);

SELECT * FROM sales ORDER BY id;

-- year 1, quarter 2, region 1, sales 1
-- Expect to scan partitions 1*2*1*1 = 2
EXPLAIN (COSTS OFF) SELECT * FROM sales WHERE year = 2009 AND
quarter IN (2, 4) AND
region = 'usa' AND
sales = 8000;

-- year 2, quarter 2, region 1, sales 1
-- Expect to scan partitions 2*2*1*1 = 4
EXPLAIN (COSTS OFF) SELECT * FROM sales WHERE year > 2009 AND
quarter IN (2, 4) AND
region = 'usa' AND
sales = 8000;

-- year 2, quarter 3 (including default), region 1, sales 3 (including default)
-- Expect to scan partitions 2*3*1*3 = 18
EXPLAIN (COSTS OFF) SELECT * FROM sales WHERE year > 2009 AND
quarter >= 3 AND
region = 'usa' AND
sales > 8000;

SELECT * FROM sales WHERE year = 2009 AND
quarter IN (2, 4) AND
region = 'usa' AND
sales = 8000;

SELECT * FROM sales WHERE year > 2009 AND
quarter IN (2, 4) AND
region = 'usa' AND
sales = 8000;

SELECT * FROM sales WHERE year > 2009 AND
quarter >= 3 AND
region = 'usa' AND
sales > 8000;

RESET ALL;
