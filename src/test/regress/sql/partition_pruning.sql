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
rv = plpy.execute('explain ' + explain_query)
search_text = 'Partition Selector'
result = []
result.append(0)
result.append(0)
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
            result[0] = int(temp_line[temp_line.index('selected: ')+10:temp_line.index(' (out')])
            result[1] = int(temp_line[temp_line.index('out of')+6:temp_line.index(')')])
return result
$$
language plpythonu;

drop table if exists partprune_foo;
create table partprune_foo(a int, b int, c int) partition by range (b) (start (1) end (101) every (10));
insert into partprune_foo select generate_series(1,5), generate_series(1,100), generate_series(1,10);
analyze partprune_foo;

select get_selected_parts(' select * from partprune_foo;');
select * from partprune_foo;

select get_selected_parts(' select * from partprune_foo where b = 35;');
select * from partprune_foo where b = 35;

select get_selected_parts(' select * from partprune_foo where b < 35;');
select * from partprune_foo where b < 35;

select get_selected_parts(' select * from partprune_foo where b in (5, 6, 14, 23);');
select * from partprune_foo where b in (5, 6, 14, 23);

select get_selected_parts(' select * from partprune_foo where b < 15 or b > 60;');
select * from partprune_foo where b < 15 or b > 60;

select get_selected_parts(' select * from partprune_foo where b = 150;');
select * from partprune_foo where b = 150;

select get_selected_parts(' select * from partprune_foo where b = a*5;');
select * from partprune_foo where b = a*5;
