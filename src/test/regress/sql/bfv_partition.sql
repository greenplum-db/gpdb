create schema bfv_partition;
set search_path=bfv_partition;

--
-- Initial setup for all the partitioning test for this suite
--
-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function count_operator(query text, operator text) returns int as
$$
rv = plpy.execute('EXPLAIN ' + query)
search_text = operator
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpythonu;

create or replace function find_operator(query text, operator_name text) returns text as
$$
rv = plpy.execute('EXPLAIN ' + query)
search_text = operator_name
result = ['false']
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = ['true']
        break
return result
$$
language plpythonu;

--
-- Tests if it produces SIGSEGV from "select from partition_table group by rollup or cube function"
--

-- SETUP
-- start_ignore
drop table if exists mpp7980;
-- end_ignore
create table mpp7980
(
 month_id date,
 bill_stmt_id  character varying(30),
 cust_type     character varying(10),
 subscription_status      character varying(30),
 voice_call_min           numeric(15,2),
 minute_per_call          numeric(15,2),
 subscription_id          character varying(15)
)
distributed by (subscription_id, bill_stmt_id)
  PARTITION BY RANGE(month_id)
    (
    start ('2009-02-01'::date) end ('2009-08-01'::date)  exclusive EVERY (INTERVAL '1 month')
    );
    
-- TEST
select count_operator('select cust_type, subscription_status,count(distinct subscription_id),sum(voice_call_min),sum(minute_per_call) from mpp7980 where month_id =E''2009-04-01'' group by rollup(1,2);','SIGSEGV');

insert into mpp7980 values('2009-04-01','xyz','zyz','1',1,1,'1');
insert into mpp7980 values('2009-04-01','zxyz','zyz','2',2,1,'1');
insert into mpp7980 values('2009-03-03','xyz','zyz','4',1,3,'1');
select cust_type, subscription_status,count(distinct subscription_id),sum(voice_call_min),sum(minute_per_call) from mpp7980 where month_id ='2009-04-01' group by rollup(1,2);

-- CLEANUP
-- start_ignore
drop table mpp7980;
-- end_ignore



--
-- Tests if it is using casting comparator for partition selector with compatible types
--

-- SETUP
CREATE TABLE TIMESTAMP_MONTH_rangep_STARTINCL (i1 int, f2 timestamp)
partition by range (f2)
(
  start ('2000-01-01'::timestamp) INCLUSIVE
  end (date '2001-01-01'::timestamp) EXCLUSIVE
  every ('1 month'::interval)
);

CREATE TABLE TIMESTAMP_MONTH_rangep_STARTEXCL (i1 int, f2 timestamp)
partition by range (f2)
(
  start ('2000-01-01'::timestamp) EXCLUSIVE
  end (date '2001-01-01'::timestamp) INCLUSIVE
  every ('1 month'::interval)
);

CREATE TABLE TIMESTAMP_MONTH_listp (i1 int, f2 timestamp)
partition by list (f2)
(
  partition jan1 values ('2000-01-01'::timestamp),
  partition jan2 values ('2000-01-02'::timestamp),
  partition jan3 values ('2000-01-03'::timestamp),
  partition jan4 values ('2000-01-04'::timestamp),
  partition jan5 values ('2000-01-05'::timestamp)
);


-- TEST
-- Middle of a middle range
INSERT INTO TIMESTAMP_MONTH_rangep_STARTINCL values (1, '2000-07-16');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = '2000-07-16';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_TIMESTAMP('2000-07-16', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_DATE('2000-07-16', 'YYYY-MM-DD');

-- Beginning of the first range
INSERT INTO TIMESTAMP_MONTH_rangep_STARTINCL values (2, '2000-01-01');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = '2000-01-01';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_TIMESTAMP('2000-01-01', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_DATE('2000-01-01', 'YYYY-MM-DD');

INSERT INTO TIMESTAMP_MONTH_rangep_STARTINCL values (3, '2000-01-02');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = '2000-01-02';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_TIMESTAMP('2000-01-02', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_DATE('2000-01-02', 'YYYY-MM-DD');

-- End of the last range
INSERT INTO TIMESTAMP_MONTH_rangep_STARTINCL values (4, '2000-12-31');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = '2000-12-31';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_TIMESTAMP('2000-12-31', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_DATE('2000-12-31', 'YYYY-MM-DD');

INSERT INTO TIMESTAMP_MONTH_rangep_STARTINCL values (5, '2001-01-01'); -- should fail, no such partition
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = '2001-01-01';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_TIMESTAMP('2001-01-01', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTINCL WHERE f2 = TO_DATE('2001-01-01', 'YYYY-MM-DD');

-- Range partitioning: START EXCLUSIVE, END INCLUSIVE
-- Middle of a middle range
INSERT INTO TIMESTAMP_MONTH_rangep_STARTEXCL values (1, '2000-07-16');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = '2000-07-16';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_TIMESTAMP('2000-07-16', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_DATE('2000-07-16', 'YYYY-MM-DD');

-- Beginning of the first range
INSERT INTO TIMESTAMP_MONTH_rangep_STARTEXCL values (2, '2000-01-01'); -- should fail, no such partition
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = '2000-01-01';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_TIMESTAMP('2000-01-01', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_DATE('2000-01-01', 'YYYY-MM-DD');

INSERT INTO TIMESTAMP_MONTH_rangep_STARTEXCL values (3, '2000-01-02');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = '2000-01-02';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_TIMESTAMP('2000-01-02', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_DATE('2000-01-02', 'YYYY-MM-DD');

-- End of the last range
INSERT INTO TIMESTAMP_MONTH_rangep_STARTEXCL values (4, '2000-12-31');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = '2000-12-31';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_TIMESTAMP('2000-12-31', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_DATE('2000-12-31', 'YYYY-MM-DD');

INSERT INTO TIMESTAMP_MONTH_rangep_STARTEXCL values (5, '2001-01-01');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = '2001-01-01';
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_TIMESTAMP('2001-01-01', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_rangep_STARTEXCL WHERE f2 = TO_DATE('2001-01-01', 'YYYY-MM-DD');


-- List partitioning
INSERT INTO TIMESTAMP_MONTH_listp values (1, '2000-01-03');
SELECT * FROM TIMESTAMP_MONTH_listp WHERE f2 = '2000-01-03';
SELECT * FROM TIMESTAMP_MONTH_listp WHERE f2 = TO_TIMESTAMP('2000-01-03', 'YYYY-MM-DD');
SELECT * FROM TIMESTAMP_MONTH_listp WHERE f2 = TO_DATE('2000-01-03', 'YYYY-MM-DD');

-- CLEANUP
-- start_ignore
DROP TABLE TIMESTAMP_MONTH_listp;
DROP TABLE TIMESTAMP_MONTH_rangep_STARTEXCL;
DROP TABLE TIMESTAMP_MONTH_rangep_STARTINCL;
-- end_ignore


--
-- Data Engineer can see partition key in psql
--

-- SETUP
-- start_ignore
DROP TABLE IF EXISTS T26002_T1;
DROP TABLE IF EXISTS T26002_T2;


CREATE TABLE T26002_T1 (empid int, departmentid int, year int, region varchar(20))
DISTRIBUTED BY (empid)
  PARTITION BY RANGE (year)
  SUBPARTITION BY LIST (region, departmentid)
    SUBPARTITION TEMPLATE (
       SUBPARTITION usa VALUES (('usa', 1)),
       SUBPARTITION europe VALUES (('europe', 2)),
       SUBPARTITION asia VALUES (('asia', 3)),
       DEFAULT SUBPARTITION other_regions)
( START (2012) END (2015) EVERY (3),
  DEFAULT PARTITION outlying_years);
-- end_ignore

-- TEST
-- expected to see the partition key
\d T26002_T1;
\d t26002_t1_1_prt_2;
\d t26002_t1_1_prt_2_2_prt_asia;

\d+ T26002_T1;
\d+ t26002_t1_1_prt_2;
\d+ t26002_t1_1_prt_2_2_prt_asia;

/*
-- test 2: Data Engineer won't see partition key for non-partitioned table
GIVEN I am a Data Engineer
WHEN I run \d on a non-partitioned table
THEN I should NOT see the partition key in the output
*/
CREATE TABLE T26002_T2 (empid int, departmentid int, year int, region varchar(20))
DISTRIBUTED BY (empid);

\d T26002_T2;

\d+ T26002_T2;

-- CLEANUP
-- start_ignore
DROP TABLE IF EXISTS T26002_T1;
DROP TABLE IF EXISTS T26002_T2;
-- end_ignore







-- ************ORCA ENABLED**********


--
-- MPP-23195
--

-- SETUP
-- start_ignore
set optimizer_enable_bitmapscan=on;
set optimizer_enable_indexjoin=on;
drop table if exists mpp23195_t1;
drop table if exists mpp23195_t2;
-- end_ignore

create table mpp23195_t1 (i int) partition by range(i) (partition pt1 start(1) end(10), partition pt2 start(10) end(20));
create index index_mpp23195_t1_i on mpp23195_t1(i);
create table mpp23195_t2(i int);

insert into mpp23195_t1 values (generate_series(1,19));
insert into mpp23195_t2 values (1);

-- TEST
select find_operator('select * from mpp23195_t1,mpp23195_t2 where mpp23195_t1.i < mpp23195_t2.i;', 'Dynamic Index Scan');
select * from mpp23195_t1,mpp23195_t2 where mpp23195_t1.i < mpp23195_t2.i;

-- CLEANUP
-- start_ignore
drop table if exists mpp23195_t1;
drop table if exists mpp23195_t2;
set optimizer_enable_bitmapscan=off;
set optimizer_enable_indexjoin=off;
-- end_ignore


--
-- Check we have Dynamic Index Scan operator and check we have Nest loop operator
--

-- SETUP
-- start_ignore
drop table if exists mpp21834_t1;
drop table if exists mpp21834_t2;
-- end_ignore

create table mpp21834_t1 (i int, j int) partition by range(i) (partition pp1 start(1) end(10), partition pp2 start(10) end(20));

create index index_1 on mpp21834_t1(i);

create index index_2 on mpp21834_t1(j);

create table mpp21834_t2(i int, j int);

-- TEST
-- start_ignore
select disable_xform('CXformInnerJoin2HashJoin');
-- end_ignore
select find_operator('analyze select * from mpp21834_t2,mpp21834_t1 where mpp21834_t2.i < mpp21834_t1.i;','Dynamic Index Scan');
select find_operator('analyze select * from mpp21834_t2,mpp21834_t1 where mpp21834_t2.i < mpp21834_t1.i;','Nested Loop');

-- CLEANUP
-- start_ignore
drop index index_2;
drop index index_1;
drop table if exists mpp21834_t2;
drop table if exists mpp21834_t1;
select enable_xform('CXformInnerJoin2HashJoin');
-- end_ignore


--
-- A rescanning of DTS with its own partition selector (under sequence node)
--

-- SETUP
-- start_ignore
set optimizer_enable_broadcast_nestloop_outer_child=on;
drop table if exists mpp23288;
-- end_ignore

create table mpp23288(a int, b int) 
  partition by range (a)
  (
      PARTITION pfirst  END(5) INCLUSIVE,
      PARTITION pinter  START(5) EXCLUSIVE END (10) INCLUSIVE,
      PARTITION plast   START (10) EXCLUSIVE
  );

insert into mpp23288(a) select generate_series(1,20);

analyze mpp23288;

-- TEST
select count_operator('select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on (t1.a < t2.a and t2.a =10) order by t2.a, t1.a;','Dynamic Table Scan');
select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on (t1.a < t2.a and t2.a =10) order by t2.a, t1.a;

select count_operator('select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on (t1.a < t2.a and (t2.a = 10 or t2.a = 5 or t2.a = 12)) order by t2.a, t1.a;','Dynamic Table Scan');
select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on (t1.a < t2.a and (t2.a = 10 or t2.a = 5 or t2.a = 12)) order by t2.a, t1.a;

select count_operator('select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on t1.a < t2.a and t2.a = 1 or t2.a < 10 order by t2.a, t1.a;','Dynamic Table Scan');
select t2.a, t1.a from mpp23288 as t1 join mpp23288 as t2 on t1.a < t2.a and t2.a = 1 or t2.a < 10 order by t2.a, t1.a;

-- CLEANUP
-- start_ignore
drop table if exists mpp23288;
set optimizer_enable_broadcast_nestloop_outer_child=off;
-- end_ignore


--
-- Testing whether test gives wrong results with partition tables when sub-partitions are distributed differently than the parent partition.
--

-- SETUP
-- start_ignore
drop table if exists pt;
drop table if exists t;
-- end_ignore

create table pt(a int, b int, c int) distributed by (a) partition by range(b) (start(0) end(10) every (2));
alter table pt_1_prt_1 set distributed randomly;

create table t(a int, b int);
insert into pt select g%10, g%2 + 1, g*2 from generate_series(1,20) g;
insert into pt values(1,1,3);
insert into t select g%10, g%2 + 1 from generate_series(1,20) g;

create index pt_c on pt(c);

analyze t;
analyze pt;

-- TEST
SELECT COUNT(*) FROM pt, t WHERE pt.a = t.a;
SELECT COUNT(*) FROM pt, t WHERE pt.a = t.a and pt.c=4;

select a, count(*) from pt group by a; 
select b, count(*) from pt group by b;
select a, count(*) from pt where a<2 group by a;

-- CLEANUP
-- start_ignore
drop index pt_c;
drop table if exists pt;
drop table if exists t;
-- end_ignore


--
-- Tests if DynamicIndexScan sets tuple descriptor of the planstate->ps_ResultTupleSlot
--

-- SETUP
-- start_ignore
drop table if exists mpp24151_t;
drop table if exists mpp24151_pt;
-- end_ignore

create table mpp24151_t(dist int, tid int, t1 text, t2 text);
create table mpp24151_pt(dist int, pt1 text, pt2 text, pt3 text, ptid int) 
DISTRIBUTED BY (dist)
PARTITION BY RANGE(ptid) 
          (
          START (0) END (5) EVERY (1),
          DEFAULT PARTITION junk_data
          )
;
create index pt1_idx on mpp24151_pt using btree (pt1);
create index ptid_idx on mpp24151_pt using btree (ptid);
insert into mpp24151_pt select i, 'hello' || 0, 'world', 'drop this', i % 6 from generate_series(0,100)i;
insert into mpp24151_pt select i, 'hello' || i, 'world', 'drop this', i % 6 from generate_series(0,200000)i;

insert into mpp24151_t select i, i % 6, 'hello' || i, 'bar' from generate_series(0,10)i;
analyze mpp24151_pt;
analyze mpp24151_t;

-- TEST
-- start_ignore
select disable_xform('CXformDynamicGet2DynamicTableScan');
-- end_ignore

select count_operator('select * from mpp24151_t, mpp24151_pt where tid = ptid and pt1 = E''hello0'';','Result');
select * from mpp24151_t, mpp24151_pt where tid = ptid and pt1 = 'hello0';

-- CLEANUP
-- start_ignore
drop index ptid_idx;
drop index pt1_idx;
drop table if exists mpp24151_t;
drop table if exists mpp24151_pt;
select enable_xform('CXformDynamicGet2DynamicTableScan');
-- end_ignore


--
-- No DPE (Dynamic Partition Elimination) on second child of a union under a join
--

-- SETUP
-- start_ignore
drop table if exists t;
drop table if exists p1;
drop table if exists p2;
drop table if exists p3;
drop table if exists p;
-- end_ignore

create table p1 (a int, b int) partition by range(b) (start (1) end(100) every (20));
create table p2 (a int, b int) partition by range(b) (start (1) end(100) every (20));
create table p3 (a int, b int) partition by range(b) (start (1) end(100) every (20));
create table p (a int, b int);
create table t(a int, b int);

insert into t select g, g*10 from generate_series(1,100) g;

insert into p1 select g, g%99 +1 from generate_series(1,10000) g;

insert into p2 select g, g%99 +1 from generate_series(1,10000) g;

insert into p3 select g, g%99 +1 from generate_series(1,10000) g;

insert into p select g, g%99 +1 from generate_series(1,10000) g;

analyze t;
analyze p1;
analyze p2;
analyze p3;
analyze p;

-- TEST
select count_operator('select * from (select * from p1 union all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 except all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 except select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 intersect all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p2 union all select * from p3) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p2 union all select * from p) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p union all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p2 intersect all select * from p3) as p_all, t where p_all.b=t.b;','Partition Selector');

select count_operator('select * from (select * from p1 union select * from p intersect all select * from p2) as p_all, t where p_all.b=t.b;','Partition Selector');

-- CLEANUP
-- start_ignore
drop table t;
drop table p1;
drop table p2;
drop table p3;
drop table p;
-- end_ignore


--
-- Gracefully handle NULL partition set from BitmapTableScan, DynamicTableScan and DynamicIndexScan
--

-- SETUP
-- start_ignore
drop table if exists dts;
drop table if exists dis;
drop table if exists dbs;
-- end_ignore

create table dts(c1 int, c2 int) partition by range(c2) (start(1) end(11) every(1));
create table dis(c1 int, c2 int, c3 int) partition by range(c2) (start(1) end(11) every(1));
create index dis_index on dis(c3);
CREATE TABLE dbs(c1 int, c2 int, c3 int) partition by range(c2) (start(1) end(11) every(1));
create index dbs_index on dbs using bitmap(c3);


-- TEST
select find_operator('(select * from dts where c2 = 1) union (select * from dts where c2 = 2) union (select * from dts where c2 = 3) union (select * from dts where c2 = 4) union (select * from dts where c2 = 5) union (select * from dts where c2 = 6) union (select * from dts where c2 = 7) union (select * from dts where c2 = 8) union (select * from dts where c2 = 9) union (select * from dts where c2 = 10);', 'Dynamic Table Scan');

(select * from dts where c2 = 1) union
(select * from dts where c2 = 2) union
(select * from dts where c2 = 3) union
(select * from dts where c2 = 4) union
(select * from dts where c2 = 5) union
(select * from dts where c2 = 6) union
(select * from dts where c2 = 7) union
(select * from dts where c2 = 8) union
(select * from dts where c2 = 9) union
(select * from dts where c2 = 10);

-- start_ignore
select disable_xform('CXformDynamicGet2DynamicTableScan');
-- end_ignore

select find_operator('(select * from dis where c3 = 1) union (select * from dis where c3 = 2) union (select * from dis where c3 = 3) union (select * from dis where c3 = 4) union (select * from dis where c3 = 5) union (select * from dis where c3 = 6) union (select * from dis where c3 = 7) union (select * from dis where c3 = 8) union (select * from dis where c3 = 9) union (select * from dis where c3 = 10);', 'Dynamic Index Scan');

(select * from dis where c3 = 1) union
(select * from dis where c3 = 2) union
(select * from dis where c3 = 3) union
(select * from dis where c3 = 4) union
(select * from dis where c3 = 5) union
(select * from dis where c3 = 6) union
(select * from dis where c3 = 7) union
(select * from dis where c3 = 8) union
(select * from dis where c3 = 9) union
(select * from dis where c3 = 10);

select find_operator('select * from dbs where c2= 15 and c3 = 5;', 'Bitmap Table Scan');

select * from dbs where c2= 15 and c3 = 5;

-- CLEANUP
-- start_ignore
drop index dbs_index;
drop table if exists dbs;
drop index dis_index;
drop table if exists dis;
drop table if exists dts;
select enable_xform('CXformDynamicGet2DynamicTableScan');
-- end_ignore

--
-- Partition elimination for heterogenous DynamicIndexScans
--

-- SETUP
-- start_ignore
drop table if exists pp;
drop index if exists pp_1_prt_1_idx;
drop index if exists pp_rest_1_idx;
drop index if exists pp_rest_2_idx;
set optimizer_segments=2;
set optimizer_partition_selection_log=on;
-- end_ignore
create table pp(a int, b int, c int) partition by range(b) (start(1) end(15) every(5));
insert into pp values (1,1,2),(2,6,2), (3,11,2);
-- Heterogeneous Index on the partition table
create index pp_1_prt_1_idx on pp_1_prt_1(c);
-- Create other indexes so that we can automate the repro for MPP-21069 by disabling tablescan
create index pp_rest_1_idx on pp_1_prt_2(c,a);
create index pp_rest_2_idx on pp_1_prt_3(c,a);
-- TEST
-- start_ignore
select disable_xform('CXformDynamicGet2DynamicTableScan') ;
-- end_ignore

select * from pp where b=2 and c=2;
select count_operator('select * from pp where b=2 and c=2;','Partition Selector');

-- CLEANUP
-- start_ignore
drop index if exists pp_rest_2_idx;
drop index if exists pp_rest_1_idx;
drop index if exists pp_1_prt_1_idx;
drop table if exists pp;
select enable_xform('CXformDynamicGet2DynamicTableScan') ;
reset optimizer_segments;
set optimizer_partition_selection_log=off;
select enable_xform('CXformDynamicGet2DynamicTableScan') ;
-- end_ignore


--
-- Partition elimination with implicit CAST on the partitioning key
--

-- SETUP
-- start_ignore
set optimizer_segments=2;
set optimizer_partition_selection_log=on;
DROP TABLE IF EXISTS ds_4;
-- end_ignore

CREATE TABLE ds_4
(
  month_id character varying(6),
  cust_group_acc numeric(10),
  mobile_no character varying(10)
)
DISTRIBUTED BY (cust_group_acc, mobile_no)
PARTITION BY LIST(month_id)
          (
          PARTITION p200800 VALUES('200800'),
          PARTITION p200801 VALUES('200801'),
          PARTITION p200802 VALUES('200802'),
          PARTITION p200803 VALUES('200803')
);

-- TEST
select * from ds_4 where month_id = '200800';
select count_operator('select * from ds_4 where month_id = E''200800'';','Partition Selector');

select * from ds_4 where month_id > '200800';
select count_operator('select * from ds_4 where month_id > E''200800'';','Partition Selector');

select * from ds_4 where month_id <= '200800';
select count_operator('select * from ds_4 where month_id <= E''200800'';','Partition Selector');

select * from ds_4 a1,ds_4 a2 where a1.month_id = a2.month_id and a1.month_id > '200800';
select count_operator('select * from ds_4 a1,ds_4 a2 where a1.month_id = a2.month_id and a1.month_id > E''200800'';','Partition Selector');

-- CLEANUP
-- start_ignore
DROP TABLE IF EXISTS ds_4;
set optimizer_partition_selection_log=off;
reset optimizer_segments;

drop function if exists find_operator(query text, operator_name text);
drop function if exists count_operator(query text, operator_name text);
-- end_ignore

--
-- Partition table with appendonly leaf, full join
--

-- SETUP
-- start_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;

CREATE TABLE foo (a int);

CREATE TABLE bar (b int, c int)
PARTITION BY RANGE (b)
  SUBPARTITION BY RANGE (c) SUBPARTITION TEMPLATE 
  (
    START (1) END (10) WITH (appendonly=true),
    START (10) END (20)
  ) 
( 
  START (1) END (10) ,
  START (10) END (20)
); 
-- end_ignore
INSERT INTO foo VALUES (1);
INSERT INTO bar VALUES (2,3);

SELECT * FROM foo FULL JOIN bar ON foo.a = bar.b;

-- CLEANUP
-- start_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;
-- end_ignore

--
-- Partition table with appendonly set at middlevel partition, full join
--

-- SETUP
-- start_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;

CREATE TABLE foo (a int);

CREATE TABLE bar (b int, c int)
PARTITION BY RANGE (b)
  SUBPARTITION BY RANGE (c) SUBPARTITION TEMPLATE 
  (
    START (1) END (10),
    START (10) END (20)
  ) 
( 
  START (1) END (10) WITH (appendonly=true),
  START (10) END (20)
); 
-- end_ignore
INSERT INTO foo VALUES (1);
INSERT INTO bar VALUES (2,3);

SELECT * FROM foo FULL JOIN bar ON foo.a = bar.b;

-- CLEANUP
-- start_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;
-- end_ignore

--
-- Partition table with appendonly set at root partition, full join
--

-- SETUP
-- start_ignore
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;

CREATE TABLE foo (a int);

CREATE TABLE bar (b int, c int) WITH (appendonly=true)
PARTITION BY RANGE (b)
  SUBPARTITION BY RANGE (c) SUBPARTITION TEMPLATE 
  (
    START (1) END (10),
    START (10) END (20)
  ) 
( 
  START (1) END (10),
  START (10) END (20)
); 
-- end_ignore
INSERT INTO foo VALUES (1);
INSERT INTO bar VALUES (2,3);

SELECT * FROM foo FULL JOIN bar ON foo.a = bar.b;


--
-- Test a hash agg that has a Sequence + Partition Selector below it.
--

-- SETUP
-- start_ignore
DROP TABLE IF EXISTS bar;
-- end_ignore
CREATE TABLE bar (b int, c int)
PARTITION BY RANGE (b)
(
  START (0) END (10),
  START (10) END (20)
);

INSERT INTO bar SELECT g % 20, g % 20 from generate_series(1, 1000) g;
ANALYZE bar;

SELECT b FROM bar GROUP BY b;

EXPLAIN SELECT b FROM bar GROUP BY b;


-- CLEANUP
DROP TABLE IF EXISTS foo;
DROP TABLE IF EXISTS bar;


-- Test EXPLAIN ANALYZE on a partitioned table. There used to be a bug, where
-- you got an internal error with this, because the EXPLAIN ANALYZE sends the
-- stats from QEs to the QD at the end of query, but because the subnodes are
-- terminated earlier, their stats were already gone.
create table mpp8031 (oid integer,
odate timestamp without time zone,
cid integer)
PARTITION BY RANGE(odate)
(
PARTITION foo START ('2005-05-01 00:00:00'::timestamp
without time zone) END ('2005-07-01 00:00:00'::timestamp
without time zone) EVERY ('2 mons'::interval),

START ('2005-07-01 00:00:00'::timestamp without time zone)
END ('2006-01-01 00:00:00'::timestamp without time zone)
EVERY ('2 mons'::interval)
);
explain analyze select a.* from mpp8031 a, mpp8031 b where a.oid = b.oid;
drop table mpp8031;

-- CLEANUP
-- start_ignore
drop schema if exists bfv_partition;
-- end_ignore
