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
set search_path to partition_pruning, public;

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
-- have to drop the indexes on the partitions explicitly.
DROP INDEX idx1_1_prt_part1;
DROP INDEX idx1_1_prt_part2;
DROP INDEX idx1_1_prt_part3;
DROP INDEX idx1_1_prt_part4;
DROP INDEX idx1_1_prt_part5;

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
-- have to drop the indexes on the partitions explicitly.
DROP INDEX idx1_1_prt_part1;
DROP INDEX idx1_1_prt_part2;
DROP INDEX idx1_1_prt_part3;
DROP INDEX idx1_1_prt_part4;
DROP INDEX idx1_1_prt_part5;


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

Create or replace function public.reverse(text) Returns text as $BODY$
DECLARE
   Original alias for $1;
   Reverse_str text;
   I int4;
BEGIN
   Reverse_str :='';
   For I in reverse length(original)..1 LOOP
   Reverse_str := reverse_str || substr(original,I,1);
END LOOP;
RETURN reverse_str;
END;
$BODY$ LANGUAGE plpgsql IMMUTABLE;

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
drop function public.reverse(text) cascade;


Create or replace function public.ZeroFunc(int) Returns int as $BODY$
BEGIN
  RETURN 0;
END;
$BODY$ LANGUAGE plpgsql IMMUTABLE;

drop table if exists mytable cascade;

create table mytable(i int, j int);
insert into mytable select x, x+1 from generate_series(1, 100000) as x;
analyze mytable;

CREATE INDEX mytable_idx1 ON mytable USING bitmap(zerofunc(i));


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

-- Test with very large IN() lists
select get_selected_parts('explain select * from partprune_foo where b in (-1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16,-17,-18,-19,-20,-21,-22,-23,-24,-25,-26,-27,-28,-29,-30,-31,-32,-33,-34,-35,-36,-37,-38,-39,-40,-41,-42,-43,-44,-45,-46,-47,-48,-49,-50,-51,-52,-53,-54,-55,-56,-57,-58,-59,-60,-61,-62,-63,-64,-65,-66,-67,-68,-69,-70,-71,-72,-73,-74,-75,-76,-77,-78,-79,-80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200)');
select * from partprune_foo where b in (-1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16,-17,-18,-19,-20,-21,-22,-23,-24,-25,-26,-27,-28,-29,-30,-31,-32,-33,-34,-35,-36,-37,-38,-39,-40,-41,-42,-43,-44,-45,-46,-47,-48,-49,-50,-51,-52,-53,-54,-55,-56,-57,-58,-59,-60,-61,-62,-63,-64,-65,-66,-67,-68,-69,-70,-71,-72,-73,-74,-75,-76,-77,-78,-79,-80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200);

select get_selected_parts('explain select * from partprune_foo where b in (-1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16,-17,-18,-19,-20,-21,-22,-23,-24,-25,-26,-27,-28,-29,-30,-31,-32,-33,-34,-35,-36,-37,-38,-39,-40,-41,-42,-43,-44,-45,-46,-47,-48,-49,-50,-51,-52,-53,-54,-55,-56,-57,-58,-59,-60,-61,-62,-63,-64,-65,-66,-67,-68,-69,-70,-71,-72,-73,-74,-75,-76,-77,-78,-79,-80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,313,314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,330,331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,357,358,359,360,361,362,363,364,365,366,367,368,369,370,371,372,373,374,375,376,377,378,379,380,381,382,383,384,385,386,387,388,389,390,391,392,393,394,395,396,397,398,399,400,401,402,403,404,405,406,407,408,409,410,411,412,413,414,415,416,417,418,419,420,421,422,423,424,425,426,427,428,429,430,431,432,433,434,435,436,437,438,439,440,441,442,443,444,445,446,447,448,449,450,451,452,453,454,455,456,457,458,459,460,461,462,463,464,465,466,467,468,469,470,471,472,473,474,475,476,477,478,479,480,481,482,483,484,485,486,487,488,489,490,491,492,493,494,495,496,497,498,499,500,501,502,503,504,505,506,507,508,509,510,511,512,513,514,515,516,517,518,519,520,521,522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,538,539,540,541,542,543,544,545,546,547,548,549,550,551,552,553,554,555,556,557,558,559,560,561,562,563,564,565,566,567,568,569,570,571,572,573,574,575,576,577,578,579,580,581,582,583,584,585,586,587,588,589,590,591,592,593,594,595,596,597,598,599,600,601,602,603,604,605,606,607,608,609,610,611,612,613,614,615,616,617,618,619,620,621,622,623,624,625,626,627,628,629,630,631,632,633,634,635,636,637,638,639,640,641,642,643,644,645,646,647,648,649,650,651,652,653,654,655,656,657,658,659,660,661,662,663,664,665,666,667,668,669,670,671,672,673,674,675,676,677,678,679,680,681,682,683,684,685,686,687,688,689,690,691,692,693,694,695,696,697,698,699,700,701,702,703,704,705,706,707,708,709,710,711,712,713,714,715,716,717,718,719,720,721,722,723,724,725,726,727,728,729,730,731,732,733,734,735,736,737,738,739,740,741,742,743,744,745,746,747,748,749,750,751,752,753,754,755,756,757,758,759,760,761,762,763,764,765,766,767,768,769,770,771,772,773,774,775,776,777,778,779,780,781,782,783,784,785,786,787,788,789,790,791,792,793,794,795,796,797,798,799,800,801,802,803,804,805,806,807,808,809,810,811,812,813,814,815,816,817,818,819,820,821,822,823,824,825,826,827,828,829,830,831,832,833,834,835,836,837,838,839,840,841,842,843,844,845,846,847,848,849,850,851,852,853,854,855,856,857,858,859,860,861,862,863,864,865,866,867,868,869,870,871,872,873,874,875,876,877,878,879,880,881,882,883,884,885,886,887,888,889,890,891,892,893,894,895,896,897,898,899,900,901,902,903,904,905,906,907,908,909,910,911,912,913,914,915,916,917,918,919,920,921,922,923,924,925,926,927,928,929,930,931,932,933,934,935,936,937,938,939,940,941,942,943,944,945,946,947,948,949,950,951,952,953,954,955,956,957,958,959,960,961,962,963,964,965,966,967,968,969,970,971,972,973,974,975,976,977,978,979,980,981,982,983,984,985,986,987,988,989,990,991,992,993,994,995,996,997,998,999,1000)');
select * from partprune_foo where b in (-1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16,-17,-18,-19,-20,-21,-22,-23,-24,-25,-26,-27,-28,-29,-30,-31,-32,-33,-34,-35,-36,-37,-38,-39,-40,-41,-42,-43,-44,-45,-46,-47,-48,-49,-50,-51,-52,-53,-54,-55,-56,-57,-58,-59,-60,-61,-62,-63,-64,-65,-66,-67,-68,-69,-70,-71,-72,-73,-74,-75,-76,-77,-78,-79,-80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,313,314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,330,331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,357,358,359,360,361,362,363,364,365,366,367,368,369,370,371,372,373,374,375,376,377,378,379,380,381,382,383,384,385,386,387,388,389,390,391,392,393,394,395,396,397,398,399,400,401,402,403,404,405,406,407,408,409,410,411,412,413,414,415,416,417,418,419,420,421,422,423,424,425,426,427,428,429,430,431,432,433,434,435,436,437,438,439,440,441,442,443,444,445,446,447,448,449,450,451,452,453,454,455,456,457,458,459,460,461,462,463,464,465,466,467,468,469,470,471,472,473,474,475,476,477,478,479,480,481,482,483,484,485,486,487,488,489,490,491,492,493,494,495,496,497,498,499,500,501,502,503,504,505,506,507,508,509,510,511,512,513,514,515,516,517,518,519,520,521,522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,538,539,540,541,542,543,544,545,546,547,548,549,550,551,552,553,554,555,556,557,558,559,560,561,562,563,564,565,566,567,568,569,570,571,572,573,574,575,576,577,578,579,580,581,582,583,584,585,586,587,588,589,590,591,592,593,594,595,596,597,598,599,600,601,602,603,604,605,606,607,608,609,610,611,612,613,614,615,616,617,618,619,620,621,622,623,624,625,626,627,628,629,630,631,632,633,634,635,636,637,638,639,640,641,642,643,644,645,646,647,648,649,650,651,652,653,654,655,656,657,658,659,660,661,662,663,664,665,666,667,668,669,670,671,672,673,674,675,676,677,678,679,680,681,682,683,684,685,686,687,688,689,690,691,692,693,694,695,696,697,698,699,700,701,702,703,704,705,706,707,708,709,710,711,712,713,714,715,716,717,718,719,720,721,722,723,724,725,726,727,728,729,730,731,732,733,734,735,736,737,738,739,740,741,742,743,744,745,746,747,748,749,750,751,752,753,754,755,756,757,758,759,760,761,762,763,764,765,766,767,768,769,770,771,772,773,774,775,776,777,778,779,780,781,782,783,784,785,786,787,788,789,790,791,792,793,794,795,796,797,798,799,800,801,802,803,804,805,806,807,808,809,810,811,812,813,814,815,816,817,818,819,820,821,822,823,824,825,826,827,828,829,830,831,832,833,834,835,836,837,838,839,840,841,842,843,844,845,846,847,848,849,850,851,852,853,854,855,856,857,858,859,860,861,862,863,864,865,866,867,868,869,870,871,872,873,874,875,876,877,878,879,880,881,882,883,884,885,886,887,888,889,890,891,892,893,894,895,896,897,898,899,900,901,902,903,904,905,906,907,908,909,910,911,912,913,914,915,916,917,918,919,920,921,922,923,924,925,926,927,928,929,930,931,932,933,934,935,936,937,938,939,940,941,942,943,944,945,946,947,948,949,950,951,952,953,954,955,956,957,958,959,960,961,962,963,964,965,966,967,968,969,970,971,972,973,974,975,976,977,978,979,980,981,982,983,984,985,986,987,988,989,990,991,992,993,994,995,996,997,998,999,1000);

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
-- TODO #141973839: we selected extra parts because of disjunction: 24 parts: 2 * 3 * 4
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
drop table if exists foo;
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

-- TODO: #141973839. Expected 10 parts, currently selecting 15 parts. First level: 4 parts + 1 default. Second level 2 parts. Total 10 parts.
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
-- TODO: #141722947 6 parts: Everything except 1 part that contains 5.6.
select get_selected_parts('explain analyze select * from bar where j is distinct from 5.6;');
-- 8 parts: NULL is shared with others on p1. So, all 8 parts.
select get_selected_parts('explain analyze select * from bar where j is distinct from NULL;');

-- Two partition selectors selecting for one dynamic table scan
CREATE TABLE partition_table (dk int, pk int) DISTRIBUTED BY (dk) PARTITION BY range(pk) (START (0) END (6) EVERY (2));
INSERT INTO partition_table SELECT j, i FROM generate_series(0, 31) AS t(j) CROSS JOIN generate_series(0, 5) AS i;
ANALYZE partition_table;

DROP TABLE IF EXISTS bar;
DROP TABLE IF EXISTS jazz;
CREATE TABLE bar (c, d) AS SELECT j, i FROM generate_series(0, 3) AS j CROSS JOIN (VALUES (0), (2), (8)) AS t(i) DISTRIBUTED BY (c);
CREATE TABLE jazz (e, f) AS SELECT j, i FROM generate_series(0, 3) AS j CROSS JOIN (VALUES (0), (4), (8)) AS t(i) DISTRIBUTED BY (e);
ANALYZE bar;
ANALYZE jazz;

CREATE OR REPLACE FUNCTION check_partition_selection (pk int) RETURNS INT AS $$
BEGIN
	IF pk NOT IN (0, 1) THEN
		raise exception 'pk: %', pk;
	END IF;
	return pk;
END;
$$ STABLE LANGUAGE plpgsql;

SELECT dk, pk, debug, c, d, e, f FROM (SELECT *, check_partition_selection(pk) as debug FROM partition_table) AS pt JOIN bar ON dk = c AND pk = d JOIN jazz on dk = e AND pk = f;

RESET ALL;
