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
set optimizer_enable_hashjoin = off;
select find_operator('analyze select * from mpp21834_t2,mpp21834_t1 where mpp21834_t2.i < mpp21834_t1.i;','Dynamic Index Scan');
select find_operator('analyze select * from mpp21834_t2,mpp21834_t1 where mpp21834_t2.i < mpp21834_t1.i;','Nested Loop');

-- CLEANUP
-- start_ignore
drop index index_2;
drop index index_1;
drop table if exists mpp21834_t2;
drop table if exists mpp21834_t1;
reset optimizer_enable_hashjoin;
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
set optimizer_enable_dynamictablescan = off;

select count_operator('select * from mpp24151_t, mpp24151_pt where tid = ptid and pt1 = E''hello0'';','Result');
select * from mpp24151_t, mpp24151_pt where tid = ptid and pt1 = 'hello0';

-- CLEANUP
-- start_ignore
drop index ptid_idx;
drop index pt1_idx;
drop table if exists mpp24151_t;
drop table if exists mpp24151_pt;
reset optimizer_enable_dynamictablescan;
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

set optimizer_enable_dynamictablescan = off;
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
reset optimizer_enable_dynamictablescan;
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
set optimizer_enable_dynamictablescan = off;
select * from pp where b=2 and c=2;
select count_operator('select * from pp where b=2 and c=2;','Partition Selector');

-- CLEANUP
-- start_ignore
drop index if exists pp_rest_2_idx;
drop index if exists pp_rest_1_idx;
drop index if exists pp_1_prt_1_idx;
drop table if exists pp;
reset optimizer_enable_dynamictablescan;
reset optimizer_segments;
set optimizer_partition_selection_log=off;
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

-- Partitioned tables with default partitions and indexes on all parts,
-- queries on them with a predicate on index column must not consider the scan
-- as partial and should not fallback.
CREATE TABLE part_tbl
(
	time_client_key numeric(16,0) NOT NULL,
	ngin_service_key numeric NOT NULL,
	profile_key numeric NOT NULL
)
DISTRIBUTED BY (time_client_key)
PARTITION BY RANGE(time_client_key)
SUBPARTITION BY LIST (ngin_service_key)
SUBPARTITION TEMPLATE
(
	SUBPARTITION Package5 VALUES (479534741),
	DEFAULT SUBPARTITION other_services
)
(
	PARTITION p20151110 START (2015111000::numeric) 
END (2015111100::numeric) WITH (appendonly=false)
);
INSERT INTO part_tbl VALUES (2015111000, 479534741, 99999999);
INSERT INTO part_tbl VALUES (2015111000, 479534742, 99999999);
CREATE INDEX part_tbl_idx 
ON part_tbl(profile_key);
EXPLAIN SELECT * FROM part_tbl WHERE profile_key = 99999999;
SELECT * FROM part_tbl WHERE profile_key = 99999999;
DROP TABLE part_tbl;

CREATE TABLE mpp3263 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
( partition aa start (0) end (1000) every (500), default partition default_part );

-- These are OK
alter table mpp3263 add column AAA int;
alter table mpp3263 add column BBB int;
alter table mpp3263 drop column BBB;

alter table mpp3263 drop partition;

alter table mpp3263 add column CCC int;

insert into mpp3263 (unique1) values (1111);

drop table mpp3263;

CREATE TABLE mpp3541 (f1 time(2) with time zone, f2 char(4), f3 varchar(10))
partition by list (f2)
subpartition by range (f3)
subpartition template (
  subpartition male values ('Male','M'),
  subpartition female values ('Female','F')
)
( partition pst values ('PST'),
  partition est values ('EST')
);

CREATE TABLE mpp3541 (f1 time(2) with time zone, f2 char(4), f3 varchar(10))
partition by range (f2)
subpartition by list (f3)
subpartition template (
  subpartition male values ('Male','M'),
  subpartition female values ('Female','F')
)
( partition pst values ('PST'),
  partition est values ('EST')
);

CREATE TABLE mpp3541 (f1 time(2) with time zone, f2 char(4), f3 varchar(10))
partition by range (f2)
subpartition by range (f3)
subpartition template (
  subpartition male values ('Male','M'),
  subpartition female values ('Female','F')
)
( partition pst values ('PST'),
  partition est values ('EST')
);

CREATE TABLE mpp3542_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 (f1 time(2) with time zone)
partition by range (f1)
(
  partition "Los Angeles" start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition "New York" start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);

-- Truncates the table name to mpp3542_0000000000111111111122222222223333333333444444444455555, but partition name is too long, so ERROR
alter table mpp3542_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 rename partition "Los Angeles_1" to "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
-- Truncates the table name to mpp3542_0000000000111111111122222222223333333333444444444455555, and partition name is safe, so renamed
alter table mpp3542_000000000011111111112222222222333333333344444444445555555555556666666666777777777788888888889999999999 rename partition "Los Angeles_1" to "LA1";
-- Use the actual table name
alter table mpp3542_0000000000111111111122222222223333333333444444444455555 rename partition "Los Angeles_2" to "LA2";
-- MPP-3542
alter table  mpp3542_0000000000111111111122222222223333333333444444444455555 rename to m; 


CREATE TABLE MULTI_PART2(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
distributed by (a)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (2) every (1)),
subpartition by range (c) subpartition template ( start (1) end (2) every (1)),
subpartition by range (d) subpartition template ( start (1) end (2) every (1)),
subpartition by range (e) subpartition template ( start (1) end (2) every (1)),
subpartition by range (f) subpartition template ( start (1) end (2) every (1)),
subpartition by range (g) subpartition template ( start (1) end (2) every (1)),
subpartition by range (h) subpartition template ( start (1) end (2) every (1)),
subpartition by range (i) subpartition template ( start (1) end (2) every (1)),
subpartition by range (j) subpartition template ( start (1) end (2) every (1)),
subpartition by range (k) subpartition template ( start (1) end (2) every (1)),
subpartition by range (l) subpartition template ( start (1) end (2) every (1)),
subpartition by range (m) subpartition template ( start (1) end (2) every (1)),
subpartition by range (n) subpartition template ( start (1) end (2) every (1)),
subpartition by range (o) subpartition template ( start (1) end (2) every (1)),
subpartition by range (p) subpartition template ( start (1) end (2) every (1)),
subpartition by range (q) subpartition template ( start (1) end (2) every (1)),
subpartition by range (r) subpartition template ( start (1) end (2) every (1)),
subpartition by range (s) subpartition template ( start (1) end (2) every (1)),
subpartition by range (t) subpartition template ( start (1) end (2) every (1)),
subpartition by range (u) subpartition template ( start (1) end (2) every (1)),
subpartition by range (v) subpartition template ( start (1) end (2) every (1)),
subpartition by range (w) subpartition template ( start (1) end (2) every (1)),
subpartition by range (x) subpartition template ( start (1) end (2) every (1)),
subpartition by range (y) subpartition template ( start (1) end (2) every (1)),
subpartition by range (z) subpartition template ( start (1) end (2) every (1))
( start (1) end (2) every (1));

alter table multi_part2 rename to multi_part2_0000000;
alter table multi_part2 rename to m_0000000;

-- We want to check both m and m_00000, thus I didn't put where clause
-- Assumes that there are no other partitions
-- order 2
select tablename, partitionlevel, partitiontablename, partitionname, partitionrank, partitionboundary from pg_partitions where tablename like 'm_000%' order by tablename;

drop table m;
drop table m_0000000;
create table mpp3061 (i int) partition by range(i) (start(1) end(10) every(1));
insert into mpp3061 values(1);
-- Not allowed. MPP-3061
update mpp3061 set i = 2 where i = 1;
drop table mpp3061;
create table mpp3466 (i int) partition by range(i) (start(1) end(10) every(2), default partition f);
alter table mpp3466 split partition f at (3) into (partition f, partition new);
drop table mpp3466;
create table mpp3058 (a char(1), b date, d char(3))       
 distributed by (a)         
partition by range (b)                                                                                            
 (              
 partition aa start (date '2008-01-01') end (date '2009-01-01') 
 every (interval '10 days'));
drop table mpp3058;

create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('2008-01-01') end ('2010-01-01')           
  );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('2008-01-01') end ('2006-01-01')           
  );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('2008-01-01') end ('-2009-01-01')           
  );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('-2008-01-01') end ('2009-01-01')           
  );
drop table mpp3058;

create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)     
partition by range (b)    
(        
     partition aa start ('2008-01-01') end ('2010-01-01')           
  );
drop table mpp3058;

create table mpp3058 (a char(1), b date, d char(3))   
distributed by (a)        
partition by range (b)      
 (                  
           partition aa start ('2008-01-01') end ('2008-04-01') every(interval '1 day')   
     );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))  
  distributed by (a)    
   partition by range (b)             
  (           
    partition aa start ('2008-01-01') end ('2009-01-01') every( '1 day')  
     );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))  
  distributed by (a)    
   partition by range (b)             
  (           
    partition aa end ('2009-01-01') every( interval '1 day')  
     );
drop table mpp3058;

-- Expected Error
create table mpp3058 (a char(1), b date, d char(3))  
  distributed by (a)    
   partition by range (b)             
  (           
    partition aa start ('2006-01-01') every( interval '1 day')  
     );
drop table mpp3058;
create table mpp3607 (aa int, bb int) partition by range (bb)
(partition foo start(2));
alter table mpp3607 add partition baz end (3); -- Expected Overlaps
alter table mpp3607 add partition baz end (4); -- Expected Overlaps
alter table mpp3607 add partition aa end (2); -- OK as of 4.2.0.0 (RIO)
alter table mpp3607 add partition bb end (1); -- Expected Overlaps
alter table mpp3607 add partition baz end (3); -- Expected Overlaps
alter table mpp3607 add partition baz end (4); -- Expected Overlaps
alter table mpp3607 add partition baz end (2); -- Expected Overlaps
alter table mpp3607 add partition bb end (0); -- Expected Overlaps

alter table mpp3607 drop partition aa;

alter table mpp3607 add partition aa end (-4); -- partition rule aa < -4, foo >=2
alter table mpp3607 add partition bb end (-3); -- Overlaps

alter table mpp3607 drop partition aa;

alter table mpp3607 add partition aa end (0); -- partition rule aa < 0, foo >=2
alter table mpp3607 drop partition aa;

alter table mpp3607 add partition aa start (4); -- Overlaps
alter table mpp3607 add partition aa start (3) end (4); -- Overlaps
alter table mpp3607 add partition aa start (0) end (1); -- partition rule aa bb>=0 and bb<1, foo bb>=2
alter table mpp3607 add partition bb start (-1) end (0); -- partition rule bb bb>=-1 and bb<0
alter table mpp3607 add partition cc start (-4); -- partition rule cc bb>=-4 and bb <-1
alter table mpp3607 add partition dd start (-5) end (-4);
alter table mpp3607 add partition ee start (-10);
alter table mpp3607 add partition ff start (-9) end (-8); -- Overlaps

drop table mpp3607;
CREATE TABLE mpp3632(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );
alter table mpp3632 add partition a1 start (30);
alter table mpp3632 add partition a2 start (25) end (30);
alter table mpp3632 add partition a3 start (10) end (20);
alter table mpp3632 add partition a4 start (20) end (21);
alter table mpp3632 add partition a5 start (22) end (24);
alter table mpp3632 add partition a6 start (21) end (22);
-- alter table mpp3632 add partition a7 start (23) end (24); -- MPP-3667

drop table mpp3632;
create table mpp3671 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
insert into mpp3671 select i from generate_series(1, 10) i;
alter table mpp3671 split partition for(1) at (1,2) into (partition f1a, partition f1b);
drop table mpp3671;
create table mpp3639 (i int) partition by range(i) (start(0) exclusive end(100) inclusive every(10));
insert into mpp3639 select i from generate_series(1, 100) i;
insert into mpp3639 select i from generate_series(1, 100) i;
insert into mpp3639 select i from generate_series(1, 100) i;
select * from mpp3639 order by i;
drop table mpp3639;
create table mpp3588 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8));
insert into mpp3588 select i from generate_series(1, 8) i;

alter table mpp3588 split partition for(1) at (1,2) into (partition fa, partition fb);
alter table mpp3588 split partition for(1) at (1,2) into (partition f1a, partition f1b); -- This has partition rules that overlaps
alter table mpp3588 split partition for(1) at (1,2) into (partition f2a, partition f2b); -- 5,6 are not within the boundary of first partition
alter table mpp3588 split partition for(1) at (1,2) into (partition f3a, partition f3b); 
alter table mpp3588 split partition for(1) at (1,2) into (partition f4a, partition f4b);
alter table mpp3588 split partition for(1) at (1,2) into (partition f5a, partition f5b); -- Out of the original boundary specification
alter table mpp3588 split partition for(1) at (1,2) into (partition f6a, partition f6b); -- I can keep going and going and going....
alter table mpp3588 split partition for(1) at (1,2) into (partition f7a, partition f7b);

drop table mpp3588;
--  MPP-3692, MPP-3679
create table mpp3679 (a text, b text) partition by list (a) (partition foo values ('foo'), partition bar values ('bar'), default partition baz); 
insert into mpp3679 values ('foo', 'blah');
insert into mpp3679 values ('bar', 'blah');
insert into mpp3679 values ('baz', 'blah');

alter table mpp3679 split default partition at ('baz') into (partition bing, default partition);
drop table mpp3679;
-- MPP-3691, MPP-3681
create table mpp3681 (id int, date date, amt decimal(10,2)) distributed by (id) partition by range(date) (start (date '2008-01-01') inclusive end ('2008-04-01') exclusive every (interval '1 month')); 

alter table mpp3681 add default partition def;

alter table mpp3681 split default partition start('2008-04-01') inclusive end('2008-05-01') exclusive into (partition apr08, default partition);

drop table mpp3681;
-- MPP-3593
create table mpp3593 (i int) partition by range(i) (start(1) end(100) every(10));
insert into mpp3593 select i from generate_series(1, 99) i;
alter table mpp3593 split partition for(1) at (5) into (partition aa, partition bb);
alter table mpp3593 split partition for(15) at (20) into (partition a1, partition b1);
alter table mpp3593 split partition for(25) at (30) into (partition a2, partition b2);
alter table mpp3593 split partition for(35) at (40) into (partition a3, partition b3);
alter table mpp3593 split partition for(55) at (60) into (partition a4, partition b4);
alter table mpp3593 split partition for(45) at (50) into (partition a5, partition b5);

drop table mpp3593;
CREATE TABLE mpp3742 (
c_id varchar(36),
ss_id varchar(36),
c_ts timestamp,
name varchar(36),
PRIMARY KEY (c_id,ss_id,c_ts)) partition by range (c_ts)
(
  start (date '2007-01-01')
  end (date '2008-01-01') every (interval '1 month'),
  default partition default_part

);
alter table mpp3742 split default partition start ('2009-01-01') end ('2009-02-01') into (partition a3, default partition);

drop table mpp3742;
create table mpp3597 (i int) partition by list(i) (partition a values(1, 2, 3, 4),
partition b values(5, 6, 7, 8), default partition default_part);
insert into mpp3597 select i from generate_series(1, 100) i;
insert into mpp3597 values (NULL);
alter table mpp3597 split default partition at (NULL);
insert into mpp3597 values (NULL);

select * from mpp3597_1_prt_default_part where i=NULL; -- No NULL values

drop table mpp3597;
create table mpp3594 (i date) partition by range(i) (start('2008-01-01') end('2009-01-01') every(interval '1 month'), default partition default_part);
alter table mpp3594 split default partition start ('2009-01-01') end ('2009-02-01') into (partition aa, partition nodate);
drop table mpp3594;
CREATE TABLE mpp3512 (id int, rank int, year int, gender char(1), count int)
DISTRIBUTED BY (id);

create table mpp3512_part (like mpp3512) partition by range (year) ( start (2001) end (2006) every ('1'));

create table mpp3512a (like mpp3512_part);

\d mpp3512
\d mpp3512a
select * from pg_partitions where tablename='mpp3512_part';

drop table mpp3512;
drop table mpp3512_part;
drop table mpp3512a;
CREATE TABLE mpp3988 ( ps_partkey integer,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_supplycost)
subpartition by range (ps_supplycost)
(default partition foo (default subpartition bar));

drop table mpp3988;
CREATE TABLE mpp3816 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name,
	startDate       date		
) partition by range (startDate)
( start ('2007-01-01') end ('2008-01-01') every (interval '1 month'), default partition default_part );

alter table mpp3816 add column AAA int;
alter table mpp3816 add column BBB int;
alter table mpp3816 drop column BBB;
alter table mpp3816 drop column startDate;

drop table mpp3816;

CREATE TABLE mpp3762_cities (
        city     varchar(80) primary key,
        location point
);

CREATE TABLE mpp3762_weather (
        city      varchar(80) references mpp3762_cities(city),
        temp_lo   int,
        temp_hi   int,
        prcp      real,
        date      date
);

CREATE TABLE mpp3762_cities_partition (
        city     varchar(80) primary key,
        location point
)  partition by list (city) ( partition a values ('Los Angeles'), partition b values ('San Mateo') );

CREATE TABLE mpp3762_weather_partition (
        city      varchar(80) references mpp3762_cities_partition(city),
        temp_lo   int,
        temp_hi   int,
        prcp      real,
        date      date
) partition by range(date) ( start('2008-01-01') end ('2009-01-01') every (interval '1 month'));

drop table mpp3762_cities, mpp3762_weather cascade;
drop table mpp3762_cities_partition, mpp3762_weather_partition cascade;

create table mpp3754a ( i int, d date, primary key (d)) partition by range(d) ( start ('2008-01-01')  inclusive end ('2008-12-01')  exclusive every (interval '1 month'));
create table mpp3754b ( i int, d date, constraint prim_tr primary key (d)) partition by range(d) ( start ('2008-01-01')  inclusive end ('2008-12-01')  exclusive every (interval '1 month'));

drop table mpp3754a;
drop table mpp3754b;

create table mpp4172 (a char(1), b int)
distributed by (b)
partition by range(a)
(
partition aa start ('2006') end ('2009'), partition bb start ('2007') end ('2008')
);

CREATE TABLE mpp4582 (id int,
mpp4582 int, year date, gender char(1))
DISTRIBUTED BY (id, gender, year)
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01')
end (date '2006-01-01') every (interval '1 year')) (
partition boys values ('M'),
partition girls values ('F'),
default partition neuter
);

ALTER table mpp4582 drop partition for ('-1');
ALTER table mpp4582 drop partition for ('--');
alter table mpp4582 drop partition for (';');
alter table mpp4582 drop partition for ();
alter table mpp4582 drop partition for (NULL);
alter table mpp4582 drop partition for ('NULL');

drop table mpp4582;

-- Use a particular username in the tests below, so that the output of \di
-- commands don't vary depending on current user.
CREATE USER mpp3641_user;
GRANT ALL ON SCHEMA bfv_partition TO mpp3641_user;
SET ROLE mpp3641_user;

CREATE TABLE mpp3641a (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
( partition aa start (0) end (1000) every (100), default partition default_part );

CREATE TABLE mpp3641b (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
subpartition by range (unique2) subpartition template ( start (0) end (1000) every (100) )
( start (0) end (1000) every (100));
alter table mpp3641b add default partition default_part;

CREATE INDEX mpp3641a_unique1 ON mpp3641a USING btree(unique1 int4_ops);
CREATE INDEX mpp3641a_unique2 ON mpp3641a USING btree(unique2 int4_ops);
CREATE INDEX mpp3641a_hundred ON mpp3641a USING btree(hundred int4_ops);
CREATE INDEX mpp3641a_stringu1 ON mpp3641a USING btree(stringu1 name_ops);

CREATE INDEX mpp3641b_unique1 ON mpp3641b USING btree(unique1 int4_ops);
CREATE INDEX mpp3641b_unique2 ON mpp3641b USING btree(unique2 int4_ops);
CREATE INDEX mpp3641b_hundred ON mpp3641b USING btree(hundred int4_ops);
CREATE INDEX mpp3641b_stringu1 ON mpp3641b USING btree(stringu1 name_ops);

\t

\di mpp3641*

drop table mpp3641a;
drop table mpp3641b;

RESET ROLE;

\di mpp3641*
\t


create schema rgs;
show search_path;

create table rgs.mpp4604(
	id int,
	time timestamp
) partition by range( time ) (
	partition p1 start (date '2008-10-14') end (date '2008-10-15'),
	partition p2 end (date '2008-11-01')
);

ALTER TABLE rgs.mpp4604 SPLIT PARTITION p2 AT( '2008-10-20' ) INTO( PARTITION p2_tmp, PARTITION p3 );
alter table rgs.mpp4604 drop partition p3;

drop schema rgs cascade;
CREATE TABLE mpp3817 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name,
	startDate       date		
) partition by range (startDate)
( start ('2007-01-01') end ('2008-01-01') every (interval '1 month'), default partition default_part );

alter table mpp3817 drop column unique1; -- Set distribution key to randomly
alter table mpp3817 drop column unique2;

\d mpp3817
\d mpp3817_1_prt_10
\d mpp3817_1_prt_default_part

drop table mpp3817;
-- All these should error out because they have overlapping range partitions

CREATE TABLE NATION (
            N_NATIONKEY INTEGER,
            N_NAME CHAR(25),
            N_REGIONKEY INTEGER,
            N_COMMENT VARCHAR(152)
            )

partition by range (n_regionkey) 
(
partition p1 start('0') end('1') inclusive, partition p2 start('1') end('5') inclusive
);

CREATE TABLE ORDERS (
                O_ORDERKEY INT8,
                O_CUSTKEY INTEGER,
                O_ORDERSTATUS CHAR(1),
                O_TOTALPRICE decimal,
                O_ORDERDATE date,
                O_ORDERPRIORITY CHAR(15),
                O_CLERK CHAR(15),
                O_SHIPPRIORITY integer,
                O_COMMENT VARCHAR(79)
                )
partition by range (o_custkey) 
subpartition by range (o_orderkey) 
(
partition p1 start('1') end('150001') every 9 (150000)
(subpartition sp1 start('1') end('1500000'),subpartition sp2 start('1351816') end('6000001'))
);

CREATE TABLE LINEITEM (
                L_ORDERKEY INT8,
                L_PARTKEY INTEGER,
                L_SUPPKEY INTEGER,
                L_LINENUMBER integer,
                L_QUANTITY decimal,
                L_EXTENDEDPRICE decimal,
                L_DISCOUNT decimal,
                L_TAX decimal,
                L_RETURNFLAG CHAR(1),
                L_LINESTATUS CHAR(1),
                L_SHIPDATE date,
                L_COMMITDATE date,
                L_RECEIPTDATE date,
                L_SHIPINSTRUCT CHAR(25),
                L_SHIPMODE CHAR(10),
                L_COMMENT VARCHAR(44)
                )
partition by range (l_discount) 
subpartition by range (l_quantity) 
,subpartition by range (l_tax) subpartition template (start('0') end('1.08') every 6 (1))
,subpartition by range (l_receiptdate) subpartition template (subpartition sp1 start('1992-01-03') end('1999-01-01'), subpartition sp2 start('1993-01-03') end ('1997-01-01'))
(
partition p1 start('0') end('1.1') 
(subpartition sp1 start('1') end('51') every(10)
) , partition p2 start('1.1') end ('2.2') (subpartition sp1 start('1') end('51') every (5))
);

CREATE TABLE ORDERS (
                O_ORDERKEY INT8,
                O_CUSTKEY INTEGER,
                O_ORDERSTATUS CHAR(1),
                O_TOTALPRICE decimal,
                O_ORDERDATE date,
                O_ORDERPRIORITY CHAR(15),
                O_CLERK CHAR(15),
                O_SHIPPRIORITY integer,
                O_COMMENT VARCHAR(79)
                )
partition by range (o_custkey) 
subpartition by range (o_orderdate) subpartition template (start('1992-01-01') end('1998-08-03') every 3 (interval '12 months')

)
,subpartition by range (o_orderkey) subpartition template (start('1') end('6000001') every 3 (3000000)

)
(
partition p1 start('1') , partition p2 start('55170') end('114873'), partition p3 start('44717') end('56000'), partition p4 start('114873') end('150001')
);
CREATE TABLE mpp3114 (f1 time(2) with time zone)
partition by range (f1)
(
  partition pst start (time with time zone '00:00 PST') end (time with time zone '23:00 PST') EVERY (INTERVAL '1 hour'),
  partition est start (time with time zone '00:00 EST') end (time with time zone '23:00 EST') EVERY (INTERVAL '1 hour')
);
DROP TABLE mpp3114;
CREATE TABLE sg_cal_detail_r1 (
     datacenter character varying(32),
     poolname character varying(128),
     machinename character varying(128),
     transactionid character varying(32),
     threadid integer,
     transactionorder integer,
     eventclass character(1),
     eventtime timestamp(2) without time zone,
     eventtype character varying(128),
     eventname character varying(128),
     status character varying(128),
     duration numeric(18,2),
     data character varying(4096)
)
WITH (appendonly=true, compresslevel=5, blocksize=2097152)
DISTRIBUTED BY (transactionid)
PARTITION BY RANGE(eventtime) 
SUBPARTITION BY LIST(datacenter)
SUBPARTITION TEMPLATE
(
SUBPARTITION SMF VALUES ('smf01','smf02'),
SUBPARTITION SJC VALUES ('sjc01','sjc02'),
SUBPARTITION DEN VALUES ('den01','den02'),
SUBPARTITION PHX VALUES ('phx01','phx02'),
DEFAULT SUBPARTITION xdc
)
SUBPARTITION BY LIST(eventtype)
SUBPARTITION TEMPLATE
(
SUBPARTITION ET1 VALUES ('EXEC'),
SUBPARTITION ET2 VALUES ('URL','EXECP','ufb'),
SUBPARTITION ET3 VALUES
('EXECT','V3Rules','SOJ','MEQ','RTM','TL','ActiveRules','RTMe','API',
'Info','BizProcess','APIRequest','_ui','Warning','Consume','XML','DSAPar
serTransform'),
SUBPARTITION ET4 VALUES('InflowHandler',
'TaskType',
'LOG',
'FETCH',
'TD',
'AxisInflowPipeline',
'AxisOutflowPipeline',
'API Security',
'SD_DSBE',
'SD_ExpressSale',
'V4Header',
'V4Footer',
'SOAP_Handler',
'MLR',
'EvictedStmtRemove',
'CT',
'DSATransform',
'APIClient',
'DSAQueryExec',
'processDSA',
'FilterEngine',
'Prefetch',
'AsyncCb',
'MC',
'SQL',
'SD_UInfo',
'TnSPayload',
'Serialization',
'CxtSetup',
'LazyInit',
'Deserialization',
'CleanUp',
'RESTDeserialize',
'RESTSerialize',
'SD_StoreNames',
'Serialize',
'Deserialize',
'SVC_INVOKE',
'SD_TitleAggr',
'eLVIS',
'SD_Promo',
'ServerCalLogId',
'SD_DSA',
'ClientCalLogId',
'NCF Async processor',
'V3Rules_OLAP',
'RTAM',
'SOAP_Handlers',
'SOAP_Ser',
'SOAP_Exec',
'RtmAB',
'RTPromotionOptimizer',
'crypt',
'Error',
'DBGetDataHlp',
'NoEncoding',
'Default',
'PromoAppCenter',
'BES_CONSUMER',
'TitleKeywordsModel',
'SOA_CLIENT',
'SD_UserContent',
'NCF',
'BEGenericPortlet',
'PortletExecution',
'SoaPortlet',
'ICEP',
'LOGIC',
'SYI_Eval_Detail',
'SD_Catalog',
'SignIn_Eval_Detail',
'Elvis Client',
'BES',
'TIMESTAMP',
'TLH',
'TLH-PRE-SYI',
'RFC',
'Offer_Eval_Detail',
'SFE_RunQuery',
'DBGetData',
'TKOItem2',
'Notification',
'XSHModel',
'APIDefinition',
'captcha',
'SD_HalfItem',
'Mail_Transport',
'MODPUT',
'60DAY_OLD_ITEM_FETCHED',
'List',
'RemotePortlet',
'MakeOffer_Eval_Detail',
'60_TO_90_DAY_OLD_ITEM_FETCHED',
'Logic',
'RtmGetContentName',
'BEPortletService',
'SYI_EUP_Rbo',
'SYI_Rbo',
'EOA',
'SEC',
'CCHP',
'TKOItem3',
'TnsFindingModelBucket',
'Mail_Send',
'SignIn_Rbo',
'SignIn=23_elvisEvl',
'TnsFindingModelXSH',
'RtmSvc',
'SWEET_TOOTH_LOCATOR_EXPIRED',
'COOKIE_INFO',
'Database',
'RYI_Eval_Detail',
'TnsFindingModelSNP',
'TitleRiskScoringModel_2_0',
'ClientIPin10',
'TnsFindingModelFraud',
'SignIn_BaseRbo2',
'Offer_EUP_Rbo',
'Offer_Rbo',
'FSA',
'Processing_elvis_events',
'NSS_API',
'MyebayBetaRedirect',
'MOTORS_PARTNER_RECIPIENT_HANDLER',
'ElvisEngine',
'PreSyi_Eval_Detail',
'RADAR',
'Latency',
'SD_TAggrCache',
'MEA',
'SD_TitleAggregatorShopping',
'KEM',
'SD_Batch',
'KG',
'ITEM_VISIBILITY',
'APPLOGIC',
'OOPexecute',
'ERRPAGE',
'FQ_RECIPIENT_HANDLER',
'RADAR_POST_Eval_Detail',
'Captcha',
'V3Rules_Detail',
'FilterEngDetail_AAQBuyerSentPre',
'Task',
'SYI_EUP_Report',
'WRITE_MOVE_FILE',
'KG_SYI',
'BatchRecord',
'SD_TitleDesc',
'B001_RTAM',
'SignIn_Report',
'SD_StoreUrl',
'CACHE_REFRESH',
'TKOItem',
'KG_EXTERNAL_CALL',
'WatchDelSkipped',
'SD_Completed',
'RequestCounts',
'FilterEngDetail_RTQEmail',
'FilterEngDetail_AAQBuyerSentPost',
'RYI_EUP_Rbo',
'RYI_Rbo',
'MF_RECIPIENT_HANDLER',
'SYI_Report',
'LCBT',
'HalfRyiSingle_Eval_Detail',
'FilterEngDetail_AAQBuyerSentEmail',
'ViewAdAbTests',
'MakeOffer_EUP_Rbo',
'MakeOffer_Rbo',
'ShipCalc Url',
'Offer_Report',
'TKOUser',
'RADAR_POST_EUP_Rbo',
'SiteStat_LeftNav',
'SiteStat_UserIsSeller',
'FilterEngDetail_RTQPost',
'INFO',
'Offer_EUP_Report',
'RADAR_POST_Rbo',
'SignIn_EUP_Rbo',
'Mail_XML',
'Processing_item_events',
'GEM',
'Mail',
'ELVIS',
'FilterEngDetail_SYIRYI',
'SD_TitleCach',
'Processing_itemend_events',
'HalfRyiSingle_EUP_Rbo',
'AlertNotify',
'AVSRedirectLog',
'BillerService',
'MENMSG',
'UserSegSvc',
'PRICE_CHANGE_ALERT_RECIPIENT_HANDLER',
'NSSOptP',
'PreSyi_Rbo',
'PreSyi_EUP_Rbo',
'NOTIFICATION.BES.BID_NEW',
'Mail_Connect',
'Mail_Close',
'GEMRECORD',
'McapCommunicatorTx',
'IMGPROC',
'KnownGood',
'FilterEngDetail_RTQPre',
'AUTH',
'BULKAPI',
'AAQBuyerSentPre_Eval_Detail',
'RYI_EUP_Report',
'HalfRyiSingle_Rbo',
'MakeOffer_Report',
'ItemClosureLOGIC',
'MakeOffer_EUP_Report',
'RADAR_POST_Report',
'BidBinStats',
'Iterator',
'RADAR_POST_EUP_Report',
'SessionStats',
'RYI_Report',
'SIBE',
'EOT',
'UsageProcessingTx',
'Processing_itemrevised_events',
'HalfSyiSingle_Eval_Detail',
'SignIn_EUP_Report',
'Referer',
'RTQEmail_Eval_Detail',
'AAQBuyerSentPost_Eval_Detail',
'AAQBuyerSentEmail_Eval_Detail',
'NCFEvent',
'CHKOUT',
'SocketWriter',
'RTQPost_Eval_Detail',
'HalfRyiSingle_Report',
'HalfRyiSingle_EUP_Report',
'DcpConnectRequest',
'SD_CatalogCache',
'PreSyi_Report',
'BotSignIn',
'Total Listing : BE_MAIN',
'Z',
'ItemPopularityScore',
'SD_TitleCacheOverflow',
'UserSegmentationCommand',
'FilterEngDetail_AAQSellerSentPre',
'PreSyi_EUP_Report',
'FilterEngDetail_AAQSellerSentPost',
'FilterEngDetail_BestOffer',
'RS',
'FilterEngDetail_AAQSellerSentEmail',
'HalfSyiSingle_EUP_Rbo',
'Service',
'Total Listing : BE_DE',
'BULK.API.HALF.PUT'),
DEFAULT SUBPARTITION etx
)
(
START ('2008-09-30')
END ('2008-10-01')
EVERY (INTERVAL '1 day')
);

ALTER TABLE sg_cal_detail_r1 ADD PARTITION START ('2008-10-01') INCLUSIVE END ('2008-10-02') EXCLUSIVE
WITH (appendonly=true, compresslevel=5, blocksize=2097152);

select count(*) from pg_partitions where tablename = 'sg_cal_detail_r1';

drop table sg_cal_detail_r1;
create table j (i int, a date) partition by range(i)
subpartition by list(a) subpartition template
(subpartition a values(1, 2, 3, 4),
subpartition b values(5, 6, 7, 8))
(
start (date '2001-01-01'),
start (date '2002-01-01'),
start (date '2003-01-01'),
start (date '2004-01-01'),
start (date '2005-01-01')
);
set optimizer_analyze_root_partition=on;
create table mpp3487 (i int) partition by range (i) (start(1) end(10) every(1));
insert into mpp3487 select i from generate_series(1, 9) i;
vacuum analyze mpp3487;
select  schemaname, tablename, attname, null_frac, avg_width, n_distinct, most_common_freqs, histogram_bounds from pg_stats where tablename like 'mpp3487%' order by 2;
drop table mpp3487;
-- Negative Test for Alter subpartition template
CREATE TABLE qa147sales (trans_id int, date date, amount 
decimal(9,2), region text)  
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
SUBPARTITION BY LIST (region) 
SUBPARTITION TEMPLATE 
( SUBPARTITION usa VALUES ('usa'), 
  SUBPARTITION asia VALUES ('asia'), 
  SUBPARTITION europe VALUES ('europe') ) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') ); 

-- Invalid TEMPLATE
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (NULL);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (-1);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (10000);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ('');
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ("");
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (*);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (1*);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ("1*");
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (ABC);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ($);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (%%);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (#);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (!);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (&);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (^);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (@);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (<);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (>);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (.);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (?);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (/);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (|);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (~);
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE (`);

select * from pg_partition_templates where tablename='qa147sales';

drop table qa147sales;

select * from pg_partition_templates where tablename='qa147sales';;

-- Now with Schema
-- Negative Test for alter subpartition template with Schema
create schema qa147;
CREATE TABLE qa147.sales (trans_id int, date date, amount
decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (date)
SUBPARTITION BY LIST (region)
SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'),
  SUBPARTITION asia VALUES ('asia'),
  SUBPARTITION europe VALUES ('europe') )
( START (date '2008-01-01') INCLUSIVE
   END (date '2009-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month') );

-- Invalid TEMPLATE
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (NULL);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (-1);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (10000);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ('');
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ("");
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (*);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (1*);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ("1*");
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (ABC);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ($);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (%%);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (#);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (!);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (&);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (^);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (@);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (<);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (>);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (.);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (?);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (/);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (|);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (~);
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE (`);

select * from pg_partition_templates where schemaname='qa147' and tablename='sales';

set client_min_messages='warning';
drop schema qa147 cascade;
reset client_min_messages;

select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix-Match for Alter subpartition template
CREATE TABLE qa147sales (trans_id int, date date, amount 
decimal(9,2), region text)  
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
SUBPARTITION BY LIST (region) 
SUBPARTITION TEMPLATE 
( SUBPARTITION usa VALUES ('usa'), 
  SUBPARTITION asia VALUES ('asia'), 
  SUBPARTITION europe VALUES ('europe') ) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '1 month') ); 

-- Clear TEMPLATE
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ();
select * from pg_partition_templates where tablename='qa147sales';
-- This will overwrite previous subpartition template
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), SUBPARTITION asia VALUES ('asia') );
select * from pg_partition_templates where tablename='qa147sales';
-- Invalid subpartition
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2008-02-01') EXCLUSIVE );
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1 month') );
select * from pg_partition_templates where tablename='qa147sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( 
SUBPARTITION usa1 VALUES('usa'),
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE);
select * from pg_partition_templates where tablename='qa147sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( 
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE,
SUBPARTITION usa1 VALUES('usa'));
select * from pg_partition_templates where tablename='qa147sales';

drop table qa147sales;

CREATE TABLE qa147sales (trans_id int, date date, amount
decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY LIST (region)
SUBPARTITION BY RANGE (date)
SUBPARTITION TEMPLATE
( START (date '2008-01-01') INCLUSIVE
   END (date '2009-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month') )
(
  PARTITION usa VALUES ('usa'),
  PARTITION asia VALUES ('asia'),
  PARTITION europe VALUES ('europe') );

-- Clear TEMPLATE
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE ();
select * from pg_partition_templates where tablename='qa147sales';
-- This will overwrite previous subpartition template
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2008-02-01') EXCLUSIVE );
select * from pg_partition_templates where tablename='qa147sales';
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1 month') );
select * from pg_partition_templates where tablename='qa147sales';
-- Invalid subpartition template
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), SUBPARTITION asia VALUES ('asia') );
select * from pg_partition_templates where tablename='qa147sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usa1 VALUES('usa'),
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE);
select * from pg_partition_templates where tablename='qa147sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147sales SET SUBPARTITION TEMPLATE
( 
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE,
SUBPARTITION usa1 VALUES('usa'));
select * from pg_partition_templates where tablename='qa147sales';

drop table qa147sales;
select * from pg_partition_templates where tablename='qa147sales';

-- Now with Schema
-- Mix-Match for Alter subpartition template in a schema
create schema qa147;
CREATE TABLE qa147.sales (trans_id int, date date, amount
decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY RANGE (date)
SUBPARTITION BY LIST (region)
SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'),
  SUBPARTITION asia VALUES ('asia'),
  SUBPARTITION europe VALUES ('europe') )
( START (date '2008-01-01') INCLUSIVE
   END (date '2009-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month') );

-- Clear TEMPLATE
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ();
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- This will overwrite previous subpartition template
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), SUBPARTITION asia VALUES ('asia') );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Invalid subpartition
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2008-02-01') EXCLUSIVE );
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1 month') );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usa1 VALUES('usa'),
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE);
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE,
SUBPARTITION usa1 VALUES('usa'));
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';

DROP SCHEMA qa147 cascade;
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';

CREATE SCHEMA qa147;
CREATE TABLE qa147.sales (trans_id int, date date, amount
decimal(9,2), region text)
DISTRIBUTED BY (trans_id)
PARTITION BY LIST (region)
SUBPARTITION BY RANGE (date)
SUBPARTITION TEMPLATE
( START (date '2008-01-01') INCLUSIVE
   END (date '2009-01-01') EXCLUSIVE
   EVERY (INTERVAL '1 month') )
(
  PARTITION usa VALUES ('usa'),
  PARTITION asia VALUES ('asia'),
  PARTITION europe VALUES ('europe') );

-- Clear TEMPLATE
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE ();
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- This will overwrite previous subpartition template
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2008-02-01') EXCLUSIVE );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usam1 start (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1 month') );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Invalid subpartition template
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
( SUBPARTITION usa VALUES ('usa'), SUBPARTITION asia VALUES ('asia') );
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usa1 VALUES('usa'),
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE);
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
-- Mix and Match RANGE/LIST . Expect to Error
ALTER TABLE qa147.sales SET SUBPARTITION TEMPLATE
(
SUBPARTITION usadate start (date '2008-01-01') INCLUSIVE END(date '2009-01-01') EXCLUSIVE,
SUBPARTITION usa1 VALUES('usa'));
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';

drop schema qa147 cascade;
select * from pg_partition_templates where schemaname='qa147' and tablename='sales';
set gp_autostats_mode=on_change;
set gp_autostats_on_change_threshold=100;

create table mpp5427 (i int) partition by range (i) (start(1) end(10000000) every(100000));
insert into mpp5427 select i from generate_series(1, 100) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(1, 100000) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(1, 1000000) i;
select * from pg_stats where tablename like 'mpp5427%';

truncate table mpp5427;

alter table mpp5427 add default partition default_part;
insert into mpp5427 select i from generate_series(1, 100) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(1, 100000) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(1, 1000000) i;
select * from pg_stats where tablename like 'mpp5427%';
insert into mpp5427 select i from generate_series(10000000, 15000000) i;
select * from pg_stats where tablename like 'mpp5427%';

drop table mpp5427;

-- MPP-5524
create table mpp5524 (a int, b int, c int, d int) partition by range(d) (start(1) end(20) every(1));
-- Not allowed
alter table mpp5524 alter partition for(rank(1)) set distributed by (b);
-- Not allowed
alter table mpp5524 alter partition for(rank(2)) set distributed by (c);
insert into mpp5524 select i, i+1, i+2, i+3 from generate_series(1, 10) i;
drop table mpp5524;

CREATE TABLE fff_main (id int, rank int, year int, gender char(1), count int)
 partition by range (year) ( start (2001) end (2006) every ('1'));

alter table fff_main_1_prt_1 drop oids;
alter table fff_main_1_prt_1 no inherit fff_main;
alter table fff_main_1_prt_1 drop column rank;
alter table fff_main_1_prt_1 add partition;
alter table fff_main_1_prt_1 drop partition;

alter table fff_main_1_prt_1 add column c int;

create table fff_main2 (like fff_main);
alter table fff_main_1_prt_1 inherit fff_main2;
alter table fff_main_1_prt_1 alter column i type bigint;
alter table fff_main_1_prt_1 set without oids;
alter table fff_main_1_prt_1 drop constraint fff_main_1_prt_1_check;

-- Add default partition
alter table fff_main_1_prt_1 split partition def at ('2009');
alter table fff_main add default partition def;
alter table fff_main_1_prt_1 split partition def at ('2009');

-- Unable to coalesce or merge, not supported
alter table fff_main_1_prt_1 exchange partition aa  with table fff_main_1_prt_2 without validation;

alter table fff_main add partition aa start ('2008') end ('2009');
alter table fff_main add partition bb start ('2009') end ('2010');
alter table fff_main_1_prt_1 add partition cc start ('2010') end ('2011');

drop table fff_main, fff_main2;

CREATE TABLE partsupp_def ( ps_partkey int,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
subpartition by range (ps_partkey)
subpartition template
( subpartition sp1 start(0) end (300) every(100),
  default subpartition subdef
)
( partition aa start (0) end (300) every(100),
  default partition def
);

alter table partsupp_def set subpartition template();
alter table partsupp_def set subpartition template( subpartition aaa start(400) end (600) every(100) );
-- Note 1: We cannot add subpartition template since we have a default partition
-- If we want to use the new subpartition template, we have to drop the default partition first, and then readd the default partition
-- Note 2: We do not support this function yet, but if we are able to split default partition with default subpartition, would we
-- be using the subpartition template to definte the "new" partition or the existing one.

select * from pg_partition_templates where tablename='partsupp_def';

alter table partsup_def add partition f1 start(0) end (300) every(100);

-- This works adding subpartition template with parent default partition
CREATE TABLE partsupp_def2 ( ps_partkey int,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
subpartition by range (ps_partkey)
subpartition template
( subpartition sp1 start(0) end (300) every(100),
  default subpartition subdef
)
( partition aa start (0) end (300) every(100)
);

alter table partsupp_def2 set subpartition template();
alter table partsupp_def2 set subpartition template( subpartition aaa start(400) end (600) every(100) );

select * from pg_partition_templates where tablename='partsupp_def2';

drop table partsupp_def;
drop table partsupp_def2;
create table mpp5431 (c1 date, c2 int) distributed by (c2) partition by range (c1) (partition p20090401 start('2009-04-01 00:00:00'::timestamp) inclusive end ('2009-04-02 00:00:00'::timestamp) exclusive);
alter table mpp5431 add partition p20090402 start('2009-04-02 00:00:00'::timestamp) inclusive end ('2009-04-03 00:00:00'::timestamp) exclusive;

create or replace function mpp5431_f1 () returns void as $$
begin
alter table mpp5431 add partition p20090403 start('2009-04-03 00:00:00'::timestamp) inclusive end ('2009-04-04 00:00:00'::timestamp) exclusive;
end;
$$ LANGUAGE 'plpgsql';

select mpp5431_f1();

drop function mpp5431_f1();
drop table mpp5431;

CREATE TABLE mpp6612 (
        unique1         int4,
        unique2         int4,
        two                     int4,
        four            int4,
        ten                     int4,
        twenty          int4,
        hundred         int4,
        thousand        int4,
        twothousand     int4,
        fivethous       int4,
        tenthous        int4,
        odd                     int4,
        even            int4,
        stringu1        name,
        stringu2        name,
        string4         name
) partition by range (unique1)
( partition aa start (0) end (1000) every (500), default partition default_part );

-- Modify any other column
alter table mpp6612 alter column unique2 type char(15);
alter table mpp6612 alter column unique2 type char(10);
-- Show the dsecription
\d mpp6612*

-- Modify the partion definition. MPP-3724
-- alter table mpp6612 alter column unique1 type char(10); -- This should fail
-- alter table mpp6612 alter column unique1 type int8;
-- Show the dsecription
-- \d mpp6612*


drop table mpp6612;
create table mpp4048 (aaa int, bbb date)
partition by range (bbb)
subpartition by range (bbb)
subpartition by range (bbb)
(
partition y2008 start (date '2008-01-01') end (date '2009-01-01')
(
  subpartition dec start (date '2008-12-01') end (date '2009-01-01') (start (date '2008-12-01') end (date '2009-01-01') every (interval '1 day'))
));

drop table mpp4048;

-- This is only for ADD primary key for partition table
-- DROP primary key is still in progress
-- MPP-6573
CREATE TABLE mpp6573 (id int, date date, amt decimal(10,2)) 
DISTRIBUTED BY (id) 
PARTITION BY RANGE (date) 
( START (date '2008-01-01') INCLUSIVE 
   END (date '2008-01-03') EXCLUSIVE 
   EVERY (INTERVAL '1 day') );
alter table mpp6573 add primary key (id, date) ;
\d mpp6573*

drop table mpp6573;


-- MPP-6724

-- Dummy select to give atmsort a cluebat that it's not processing a result
-- set right now. It gets confused by some of the errors from above.
select 1 as foobar;
-- start_matchsubs
-- m/mpp6724_1_prt_r\d+/
-- s/(mpp6724_1_prt_r)\d+/$1xxxxxx/g
-- end_matchsubs
create table mpp6724 ( c1 int, dt DATE, c2 varchar, PRIMARY KEY ( c1,dt ) ) distributed by (c1) partition by range ( dt ) ( start ( date '2009-01-01' ) inclusive end ( date '2009-01-03' ) EXCLUSIVE EVERY ( INTERVAL '1 day' ) );
insert into mpp6724 values ( 1,'2009-01-01','One'), (2,'2009-01-02','Two'),(3,'2009-01-01','three'), (3,'2009-01-02', 'three`');
insert into mpp6724 values ( 1,'2009-01-01','One'); -- This violate the primary key, expected to fail

alter table mpp6724 add partition start ( date '2009-01-03' ) inclusive end ( date '2009-01-04' ) ;
insert into mpp6724 values ( 4,'2009-01-03','Four');

-- Should fail because violates the primary key
insert into mpp6724 values ( 4,'2009-01-03','Four');
insert into mpp6724 values ( 4,'2009-01-03','Four');
insert into mpp6724 values ( 4,'2009-01-03','Four');
insert into mpp6724 values ( 4,'2009-01-03','Four');

select c1, dt, count(*) from mpp6724 group by 1,2 having count(*) > 1;

drop table mpp6724;

-- MPP-6247
-- Delete Using on partitioned table causes repetitive scans on using table	
create table mpp6247_foo ( c1 int, dt date ) distributed by ( c1 ) partition by range (dt) ( start ( date '2009-05-01' ) end ( date '2009-05-11' ) every ( interval '1 day' ) );
create table mpp6247_bar (like mpp6247_foo);

-- EXPECT: Single HJ after partition elimination instead of sequence of HJ under Append
select count_operator('delete from mpp6247_foo using mpp6247_bar where mpp6247_foo.c1 = mpp6247_bar.c1 and mpp6247_foo.dt = ''2009-05-03''', 'Hash Join');

drop table mpp6247_bar;
drop table mpp6247_foo;

--
-- Test partition elimination, MPP-7891
--

-- cleanup
-- start_ignore
drop table if exists r_part;
drop table if exists r_co;

deallocate f1;
deallocate f2;
deallocate f3;
-- end_ignore

create table r_part(a int, b int) partition by range(a) (start (1) end(10) every(1));
create table r_co(a int, b int) with (orientation=column, appendonly=true) partition by range(a) (start (1) end(10) every(1)) ;

insert into r_part values (1,1), (2,2), (3,3);

select * from r_part order by a,b;

analyze r_part;

explain select * from r_part r1, r_part r2 where r1.a=1; -- should eliminate partitions in the r1 copy of r_part

explain select * from r_part where a in (1,2); -- should eliminate partitions

-- Test partition elimination in prepared statements
prepare f1(int) as select * from r_part where a = 1 order by a,b; 
prepare f2(int) as select * from r_part where a = $1 order by a,b;

execute f1(1); 
execute f2(1); 
execute f2(2); 


explain select * from r_part where a = 1 order by a,b; -- should eliminate partitions
--force_explain
explain execute f1(1); -- should eliminate partitions 
--force_explain
explain execute f2(2); -- should eliminate partitions


-- Test partition elimination on CO tables
insert into r_co values (1,1), (2,2), (3,3);
analyze r_co; 
explain select * from r_co where a=2; -- should eliminate partitions

-- test partition elimination in prepared statements on CO tables
prepare f3(int) as select * from r_co where a = $1 order by a,b; 
--force_explain
explain execute f3(2); -- should eliminate partitions

-- start_ignore
drop table r_part;
drop table r_co;
deallocate f1;
deallocate f2;
deallocate f3;
-- end_ignore

--
-- Test partition elimination, MPP-7891
--

-- start_ignore
drop table if exists fact;
deallocate f1;

create table fact(x int, dd date, dt text) distributed by (x) partition by range (dd) ( start('2008-01-01') end ('2320-01-01') every(interval '100 years'));
-- end_ignore

analyze fact;

select '2009-01-02'::date = to_date('2009-01-02','YYYY-MM-DD'); -- ensure that both are in fact equal

explain select * from fact where dd < '2009-01-02'::date; -- partitions eliminated

explain select * from fact where dd < to_date('2009-01-02','YYYY-MM-DD'); -- partitions eliminated

explain select * from fact where dd < current_date; --partitions eliminated

-- Test partition elimination in prepared statements

prepare f1(date) as select * from fact where dd < $1;

-- force_explain
explain execute f1('2009-01-02'::date); -- should eliminate partitions
-- force_explain
explain execute f1(to_date('2009-01-02', 'YYYY-MM-DD')); -- should eliminate partitions

-- start_ignore
drop table fact;
deallocate f1;
-- end_ignore
-- Test for partition cleanup

-- start_ignore
drop schema partition_999 cascade;
create schema partition_999;

set search_path=bfv_partition,partition_999;
-- end_ignore

create table partition_cleanup1 (a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
( partition aa start (1) end (10) every (1) );

CREATE TABLE partition_999.partition_cleanup2(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (10) every (1))
( partition aa start (1) end (10) every (1) );

drop table partition_cleanup1;
drop schema partition_999 cascade;

-- These should be empty
select 'pg_partition_columns', count(*) from pg_partition_columns where tablename='partition_cleanup%';
select 'pg_partitions', count(*) from pg_partitions where tablename='partition_cleanup%';
select 'pg_partition_templates', count(*) from pg_partition_templates where tablename='partition_cleanup%';


-- CLEANUP
-- start_ignore
drop schema if exists bfv_partition cascade;
DROP USER mpp3641_user;
-- end_ignore
